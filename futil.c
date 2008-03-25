/* $Id$ */
/* Filename relative utility
 * Copyright (C) 2003, 2004, 2005  Seong-Kook Shin <cinsky@gmail.com>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public
 * License along with this library; if not, write to the Free
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */
#include <futil.h>
#include <obsutil.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <dirent.h>
#include <utime.h>
#include <stdio.h>

static struct obstack cwdpool_;
static struct obstack offpool_;

static struct obstack *cwdpool = &cwdpool_;
static struct obstack *offpool = &offpool_;

char *cwdpool_head;
char *offpool_head;

#define OFFSET_ISEMPTY()        (OBSTACK_OBJECT_SIZE(offpool) == 0)
#define OFFSET_PUSH(x)          (OBSTACK_INT_GROW(offpool, (int)(x)))
static __inline__ off_t
OFFSET_POP()
{
  off_t off;
  int *offsets = (int *)OBSTACK_BASE(offpool);

  off = (off_t)offsets[OBSTACK_OBJECT_SIZE(offpool) / sizeof(int) - 1];
  OBSTACK_BLANK(offpool, -sizeof(int));

  return off;
}


int
fu_init(int use_atexit)
{
  if (OBSTACK_INIT(cwdpool) < 0)
    return -1;

  if (OBSTACK_INIT(offpool) < 0) {
    OBSTACK_FREE(cwdpool, 0);
    return -1;
  }

  offpool_head = OBSTACK_ALLOC(offpool, sizeof(int));
  cwdpool_head = OBSTACK_ALLOC(cwdpool, sizeof(int));

  if (use_atexit)
    atexit(fu_end);
  return 0;
}


void
fu_end(void)
{
  OBSTACK_FREE(cwdpool, 0);
  OBSTACK_FREE(offpool, 0);
}


int
fu_isdir(const char *pathname)
{
  struct stat sbuf;

  assert(pathname != 0);

  if (stat(pathname, &sbuf) < 0)
    return 0;

  return fu_isdir_with_stat(&sbuf);
}


int
fu_islink(const char *pathname)
{
  struct stat sbuf;

  assert(pathname != 0);

  if (lstat(pathname, &sbuf) < 0)
    return 0;

  return fu_islink_with_stat(&sbuf);
}


long
fu_blksize(const char *pathname)
{
  struct stat sbuf;

  if (stat(pathname, &sbuf) < 0)
    return 0;

  return (long)sbuf.st_blksize;
}


int
fu_datasync(const char *pathname)
{
  int fd;
  int ret, saved_errno;
  assert(pathname != 0);

  fd = open(pathname, O_RDONLY);
  if (fd < 0)
    return -1;

  ret = fdatasync(fd);
  saved_errno = errno;
  close(fd);

  if (ret < 0)
    errno = saved_errno;

  return ret;
}


char *
fu_url2pathname(const char *url)
{
  assert(url != 0);

  if (strncmp(url, "file://", 7) == 0)
    return (char *)(url + 7);

  if (url[0] == '/' || url[0] == '.') {
    /* If URL is in relative URI syntax, */
    return (char *)url;
  }

  return 0;
}


fu_findfiles_t *
fu_find_files(const char *basedir, struct obstack *pool,
              size_t nbulk, int flags,
              int (*predicate)(const char *, void *), void *data)
{
  static const char *saved_cwd = 0;
  static const char *dummy_off = 0;
  fu_findfiles_t *list = 0;
  char *cwd;
  char *pathname;
  DIR *dir, *auxdir;
  struct dirent *ent;
  off_t off;
  int saved_errno = 0;

  //assert(OBSTACK_OBJECT_SIZE(offpool) == 0);
  //assert(OBSTACK_OBJECT_SIZE(cwdpool) == 0);
  //assert(OBSTACK_OBJECT_SIZE(pool) == 0);

  if (pool == 0) {
    if (saved_cwd)
      OBSTACK_FREE(cwdpool, (void *)saved_cwd);
    if (dummy_off)
      OBSTACK_FREE(offpool, (void *)dummy_off);
    saved_cwd = dummy_off = 0;
    return 0;
  }

  if (basedir) {
    if (saved_cwd)
      OBSTACK_FREE(cwdpool, (void *)saved_cwd);
    if (dummy_off)
      OBSTACK_FREE(offpool, (void *)dummy_off);

    saved_cwd = OBSTACK_GETCWD(cwdpool);
    dummy_off = OBSTACK_ALLOC(offpool, 1);

    if (chdir(basedir) < 0)
      goto err;
  }

  cwd = OBSTACK_GETCWD(cwdpool);
  if (!cwd)
    goto err;
  //basename = OBSTACK_BASE(cwdpool) + strlen(cwd);

  list = OBSTACK_ALLOC(pool,
                       sizeof(*list) + (nbulk - 1) * sizeof(const char *));
  if (!list)
    goto err;
  list->nelem = 0;


  dir = opendir(cwd);

  if (!basedir) {
    if (OFFSET_ISEMPTY()) {
      closedir(dir);
      goto end;
    }
    seekdir(dir, OFFSET_POP());
  }

 read_entry:
  while ((ent = readdir(dir)) != NULL) {
    if (strcmp(ent->d_name, ".") == 0 || strcmp(ent->d_name, "..") == 0)
      continue;

    /* conceptually, this block does
     *   pathname = pathname + '/' + ent->dname */
    pathname = OBSTACK_STR_GROW3(cwdpool, cwd, "/", ent->d_name);
    //OBSTACK_BLANK(cwdpool, -1);
    //pathname = OBSTACK_STR_GROW2(cwdpool, "/", ent->d_name);

    if ((flags & FU_NOFOLLOW) && fu_islink(pathname)) {
      OBSTACK_FREE(cwdpool, pathname);
      continue;
    }

    if (fu_isdir(pathname)) {   /* If ENT is a direcotry, */
      if ((flags & FU_DIRPRED) &&
          predicate && predicate(pathname, data) == 0) {
        OBSTACK_FREE(cwdpool, pathname);
        continue;
      }

      auxdir = opendir(pathname);
      if (auxdir && chdir(pathname) == 0) { /* Entering new directory, */
        OFFSET_PUSH(telldir(dir));
        closedir(dir);
        dir = auxdir;

        OBSTACK_FREE(cwdpool, cwd);
        cwd = OBSTACK_GETCWD(cwdpool);
      }
      else {       /* If something goes wrong, skip this directory. */
        if (auxdir)
          closedir(auxdir);
        OBSTACK_FREE(cwdpool, pathname);
      }

    }
    else {                      /* If ENT is a regular file, */
      if (predicate && predicate(pathname, data) == 0)
        continue;

      if (flags & FU_URLFORM)
        list->names[list->nelem] = OBSTACK_STR_COPY2(pool, "file://", pathname);
      else
        list->names[list->nelem] = OBSTACK_STR_COPY(pool, pathname);
      list->nelem++;

      if (list->nelem >= nbulk) {
        OFFSET_PUSH(telldir(dir));
        closedir(dir);
        OBSTACK_FREE(cwdpool, cwd);
        return list;
      }
      OBSTACK_FREE(cwdpool, pathname);
    }
  }
  closedir(dir);

  if (!OFFSET_ISEMPTY()) {
    off = OFFSET_POP();
    if (chdir("..") < 0) {
      /* Unable to traverse back to the parent directory. */
      goto err;
    }
    OBSTACK_FREE(cwdpool, cwd);
    cwd = OBSTACK_GETCWD(cwdpool);
    dir = opendir(cwd);
    if (!dir)
      return -1;
    seekdir(dir, off);
    goto read_entry;
  }

 end:
  if (dummy_off) {
    OBSTACK_FREE(offpool, (void *)dummy_off);
    dummy_off = 0;
  }
  if (saved_cwd) {
    if (chdir(saved_cwd) < 0)
      goto err;
    OBSTACK_FREE(cwdpool, (void *)saved_cwd);
    saved_cwd = 0;
  }
  if (list->nelem == 0) {
    OBSTACK_FREE(pool, list);
    list = 0;
  }
  return list;

 err:
  saved_errno = errno;
  if (dummy_off) {
    OBSTACK_FREE(offpool, (void *)dummy_off);
    dummy_off = 0;
  }
  if (saved_cwd) {
    chdir(saved_cwd);
    OBSTACK_FREE(cwdpool, (void *)saved_cwd);
    saved_cwd = 0;
  }
  if (list) {
    OBSTACK_FREE(pool, list);
  }
  return 0;
}


int
touch(const char *file, time_t tm)
{
  struct utimbuf times;
  times.actime = times.modtime = tm;

  return utime(file, &times);
}


char *
fu_path_concat(const char *lhs, const char *rhs)
{
  char *s;
  int len;
  int last_char_is_sep = 0;

  len = strlen(lhs);

  if (lhs[len - 1] == DIR_SEPARATOR[0])
    last_char_is_sep = 1;

  len = len + 1 + strlen(rhs) + 1 - last_char_is_sep;
  s = malloc(len);
  if (!s)
    return NULL;

  sprintf(s, "%s%s%s", lhs, (last_char_is_sep) ? "" : DIR_SEPARATOR, rhs);
  return s;
}
