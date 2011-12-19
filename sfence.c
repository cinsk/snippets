
/*
 * sfence: corrupt stack detector
 * Copyright (C) 2009  Seong-Kook Shin <cinsky at gmail dot com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */
#define _GNU_SOURCE
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include <pthread.h>

#include <errno.h>

#include <unistd.h>
#include <sys/mman.h>
#include <dlfcn.h>

#define FRAME_MAX 512

#define THREAD_TABLE_PAGES      1

struct frame {
  void *func_ptr;
  void *base_ptr;
  void *ret_addr;
};

struct frame_info {
  int id;
  int depth;
  struct frame stack[FRAME_MAX];
};

struct thread_entry {
  pthread_t tid;
  struct frame_info *frame;
};

struct thread_info {
  int size;
  int current;
  struct thread_entry entries[0];
};

static struct thread_info *thread_table;
static pthread_mutex_t frame_mutex = PTHREAD_MUTEX_INITIALIZER;
static const char *sfence_version = "0.1";


__attribute__((no_instrument_function)) static void
banner(void)
{
  fprintf(stderr, "sfence %s\n", sfence_version);
  fprintf(stderr, "Copyright (c) 2008 Seong-Kook Shin <cinsky@gmail.com>\n\n");
}


__attribute__((no_instrument_function)) static void
dump_frame(struct frame_info *finfo, int i)
{
  Dl_info info;
  info.dli_sname = NULL;

  if (dladdr(finfo->stack[i].func_ptr, &info) == 0) {
    info.dli_sname = NULL;
  }
  else {
    if (info.dli_sname == NULL)
      info.dli_sname = "*CORRUPT*";
  }

  if (i == finfo->depth) {
    fprintf(stderr, "sfence:   #%d %s (%#08x)\n",
            finfo->depth - i,
            info.dli_sname,
            (unsigned)(finfo->stack[i].func_ptr));
  }
  else {
    fprintf(stderr, "sfence:   #%d %#08x in %s (%#08x)\n",
            finfo->depth - i,
            (unsigned)(finfo->stack[i + 1].ret_addr),
            info.dli_sname,
            (unsigned)(finfo->stack[i].func_ptr));
  }
}


__attribute__((no_instrument_function)) static void
dump_frame_info(struct frame_info *finfo)
{
  int i;
  int show_hint = 0;

  fprintf(stderr, "sfence: backtrace (thread #%d)\n", finfo->id);

  dump_frame(finfo, finfo->depth);
  for (i = finfo->depth - 1; i >= 0; i--) {
    dump_frame(finfo, i);
  }
#if 0
  for (i = 0; i < finfo->depth - 1; i++) {
    if (dladdr(finfo->stack[i].func_ptr, &info)) {
      if (info.dli_sname == 0) {
        show_hint = 1;
        fprintf(stderr, "sfence: \t%08x %08x\n",
                (unsigned)(finfo->stack[i].func_ptr),
                (unsigned)(finfo->stack[i].ret_addr));
      }
      else {
        fprintf(stderr, "\t%08x %s %08x\n",
                (unsigned)(finfo->stack[i].func_ptr), info.dli_sname,
                (unsigned)(finfo->stack[i].ret_addr));

      }
    }
    else
      fprintf(stderr, "\t%08x %08x\n",
              (unsigned)(finfo->stack[i].func_ptr),
              (unsigned)(finfo->stack[i].ret_addr));
  }
#endif  /* 0 */

  if (show_hint) {
    fprintf(stderr,
            "You may need to pass `-rdynamic' to get the function name");
    fprintf(stderr, "on call stack information.");
  }
}


__attribute__((no_instrument_function)) void
sfence_dump_stack(void)
{
  int i;

  for (i = 0; i < thread_table->current; i++) {
    if (thread_table->entries[i].frame != 0) {
      dump_frame_info(thread_table->entries[i].frame);
    }
  }
}


__attribute__((no_instrument_function,constructor)) static void
sfence_init(void)
{
  int i;
  int pagesize = sysconf(_SC_PAGE_SIZE);

  banner();

  thread_table = mmap(NULL, pagesize * THREAD_TABLE_PAGES,
                      PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS,
                      -1, 0);
  if (thread_table == MAP_FAILED) {
    fprintf(stderr, "sfence: mmap(2) failed: %s\n", strerror(errno));
    exit(1);
  }

  thread_table->size = (pagesize * THREAD_TABLE_PAGES - sizeof(size_t)) \
    / sizeof(*thread_table->entries) - 2;

  for (i = 0; i < thread_table->size; i++) {
    //thread_table->entries[i].tid = -1;
    thread_table->entries[i].frame = NULL;
  };
  thread_table->current = 0;

  if (mprotect(thread_table, pagesize * THREAD_TABLE_PAGES, PROT_READ) == -1) {
    fprintf(stderr, "sfence: mprotect(2) failed: %s\n", strerror(errno));
    exit(1);
  }
}


__attribute__((no_instrument_function)) static struct frame_info *
create_frame_info(int id)
{
  //int pagesize = sysconf(_SC_PAGE_SIZE);
  struct frame_info *finfo;
  int i;

  finfo = mmap(NULL, sizeof(*finfo),
               PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);

  if (finfo == MAP_FAILED) {
    fprintf(stderr, "init_frames: mmap() failed: %s\n", strerror(errno));
    exit(1);
  }

  finfo->id = id;
  finfo->depth = -1;
  for (i = 0; i < FRAME_MAX; i++) {
    finfo->stack[i].func_ptr = NULL;
    finfo->stack[i].base_ptr = NULL;
    finfo->stack[i].ret_addr = NULL;
  }

  if (mprotect(finfo, sizeof(*finfo), PROT_READ) == -1) {
    fprintf(stderr, "init_frames: mprotect(2) failed: %s\n", strerror(errno));
    exit(1);
  }

  return finfo;
}


__attribute__((no_instrument_function)) static struct frame_info *
sfence_frame(pthread_t thread)
{
  int i;
  int pagesize = sysconf(_SC_PAGE_SIZE);

  pthread_mutex_lock(&frame_mutex);

  for (i = 0; i < thread_table->current; i++) {
    if (pthread_equal(thread_table->entries[i].tid, thread) &&
        thread_table->entries[i].frame != 0)
      return thread_table->entries[i].frame;
  }
  if (i >= thread_table->size) {
    for (i = 0; i < thread_table->size; i++) {
      if (thread_table->entries[i].frame == 0)
        break;
    }
    if (i >= thread_table->size) {
      fprintf(stderr, "sfence: too many threads\n");
      exit(1);
    }
  }

  if (mprotect(thread_table, pagesize * THREAD_TABLE_PAGES,
               PROT_READ | PROT_WRITE) == -1) {
    fprintf(stderr, "sfence: mprotect(2) failed: %s\n", strerror(errno));
    exit(1);
  }

  thread_table->entries[i].tid = thread;
  thread_table->entries[i].frame = create_frame_info(i);
  thread_table->current++;

  if (mprotect(thread_table, pagesize * THREAD_TABLE_PAGES, PROT_READ) == -1) {
    fprintf(stderr, "sfence: mprotect(2) failed: %s\n", strerror(errno));
    exit(1);
  }

  pthread_mutex_unlock(&frame_mutex);

  return thread_table->entries[i].frame;
}


__attribute__((no_instrument_function)) void
__cyg_profile_func_enter(void *this_fn, void *call_site)

{
  struct frame_info *finfo = sfence_frame(pthread_self());

  if (mprotect(finfo, sizeof(*finfo), PROT_READ | PROT_WRITE) == -1) {
    fprintf(stderr, "sfence: mprotect(2) failed: %s\n", strerror(errno));
    exit(1);
  }

  finfo->depth++;

  if (finfo->depth >= FRAME_MAX) {
    fprintf(stderr, "sfence: stack overflow");
    exit(1);
  }

  finfo->stack[finfo->depth].func_ptr = this_fn;
  finfo->stack[finfo->depth].base_ptr = 0;
  finfo->stack[finfo->depth].ret_addr = call_site;

  if (mprotect(finfo, sizeof(*finfo), PROT_READ) == -1) {
    fprintf(stderr, "sfence: mprotect(2) failed: %s\n", strerror(errno));
    exit(1);
  }

#if 0
  struct profile *prof;

  profile_get(&prof);
  prof->depth++;
  prof->stack[prof->top++] = this_fn;
  fprintf(prof->fp, "%*sCALL(0x%08x) from 0x%08x\n", prof->depth * 2, "",
          (unsigned)this_fn, (unsigned)call_site);

  fprintf(prof->fp, "%d,%08x,%08x\n", prof->depth,
          (unsigned)this_fn, (unsigned)call_site);

#endif  /* 0 */
}


__attribute__((no_instrument_function)) void
__cyg_profile_func_exit(void *this_fn, void *call_site)
{
  struct frame_info *finfo = sfence_frame(pthread_self());
  void *base = 0;

  if (finfo->stack[finfo->depth].func_ptr != this_fn ||
      finfo->stack[finfo->depth].base_ptr != base ||
      finfo->stack[finfo->depth].ret_addr != call_site) {
    fprintf(stderr, "sfence: STACK CORRUPTION DETECTED!\n");
    dump_frame_info(finfo);
    abort();
  }

  if (mprotect(finfo, sizeof(*finfo), PROT_READ | PROT_WRITE) == -1) {
    fprintf(stderr, "sfence: mprotect(2) failed: %s\n", strerror(errno));
    exit(1);
  }

  finfo->depth--;

  if (mprotect(finfo, sizeof(*finfo), PROT_READ) == -1) {
    fprintf(stderr, "sfence: mprotect(2) failed: %s\n", strerror(errno));
    exit(1);
  }

#if 0
  struct profile *prof;

  profile_get(&prof);
  prof->depth--;
  prof->top--;
  //printf("LEAVE: 0x%08x\n", (unsigned)this_fn);
  //printf("\tCS: 0x%08x\n", (unsigned)call_site);
  fprintf(prof->fp, "%d,%08x,%08x\n", prof->depth,
          (unsigned)this_fn, (unsigned)call_site);
#endif  /* 0 */
}


