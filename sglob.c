/* sglob -- another implementation of glob(3)
 * Copyright (C) 2010   Seong-Kook Shin <cinsky@gmail.com>
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

/* $Id$ */

/*
 * TODO -- to collect filename recursively onto its subdirectories
 *         (with no change corrent working directory)
 */
#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif
#include <stdarg.h>
#include <sys/types.h>
#include <dirent.h>
#include <fnmatch.h>
#include <stdlib.h>
#include <unistd.h>

#include "sglob.h"

#include <string.h>

#define DIR_SEPARATOR '/'
#define DIR_SEPARATORS "/"

static void sglob_init(sglob_t *glob, int flags);
static int sglob_incr_cap(sglob_t *glob);
static int sglob_push(sglob_t *glob, const char *dir, const char *filename);


int
sglob(const char *pattern, int flags, sglob_t *glob)
{
  char *p;
  char *dir, *filename;
  int ret;
  char *pat = NULL;
  int len = 0;

  dir = NULL;
  filename = NULL;

  if (pattern) {
    pat = strdup(pattern);
    len = strlen(pattern);
    dir = NULL;
    filename = NULL;
  }

  while ((p = memrchr(pat, DIR_SEPARATOR, len)) != NULL) {
    if (p > pat) {
      if (*(p - 1) == '\\') {
        len = p - pat;
      }
      else {
        *p = '\0';
        dir = pat;
        filename = p + 1;
        break;
      }
    }
    else if (p == pat) {
      dir = "/";
      filename = p + 1;
      break;
    }
  }
  if (p == NULL) {
    filename = pat;
  }

  ret = sglob_(dir, filename, flags, glob);
  free(pat);
  return ret;
}


int
sglob_(const char *directory, const char *pattern, int flags, sglob_t *glob)
{
  DIR *d;
  const char *dname = ".";
  struct dirent *ent;

  sglob_init(glob, flags);

  if (directory)
    dname = directory;

  d = opendir(dname);
  if (!d) {
    return -1;
  }

  while ((ent = readdir(d)) != NULL) {
    if (strcmp(ent->d_name, ".") == 0 ||
        strcmp(ent->d_name, "..") == 0)
      continue;

    if (pattern == NULL || pattern[0] == '\0')
      sglob_push(glob, dname, ent->d_name);
    else if (fnmatch(pattern, ent->d_name, FNM_PERIOD) == 0)
      sglob_push(glob, dname, ent->d_name);
  }
  closedir(d);

  return 0;
}


void
sglobfree(sglob_t *glob)
{
  int i;

  if (!glob)
    return;

  for (i = 0; i < glob->pathc; i++)
    free(glob->pathv[i]);
  free(glob->pathv);
}


static void
sglob_init(sglob_t *glob, int flags)
{
  glob->pathc = 0;
  glob->pathv = NULL;
  glob->capacity_ = 0;

  if (!(flags & SGLOB_MASK))
    glob->mask = S_IFREG;
}


/*
 * Increase the memory buffer of glob->pathv if not sufficient.
 */
static int
sglob_incr_cap(sglob_t *glob)
{
  char **p;
  size_t cap;

  if (glob->pathv != NULL && glob->pathc < glob->capacity_ - 1)
    return 0;

  cap = (glob->capacity_ == 0) ? 32 : glob->capacity_;

  p = realloc(glob->pathv, cap * 2 * sizeof(char *));
  if (!p)
    return -1;

  glob->capacity_ = cap * 2;
  glob->pathv = p;

  return 0;
}


/*
 * Add the joined dir+filename into glob if matched.
 */
static int
sglob_push(sglob_t *glob, const char *dir, const char *filename)
{
  size_t dlen, flen, tlen;
  char *p, *pathname;
  struct stat sbuf;

  if (sglob_incr_cap(glob) < 0)
    return -1;

  dlen = strlen(dir);
  flen = strlen(filename);
  tlen = dlen + flen + 1;

  if (dir[dlen - 1] != DIR_SEPARATOR)
    tlen++;

  pathname = malloc(tlen);
  if (!pathname)
    return -1;

  p = pathname;
  strcpy(p, dir);
  p = p + dlen;
  if (dir[dlen - 1] != DIR_SEPARATOR) {
    strcpy(p, DIR_SEPARATORS);
    p = p + sizeof(DIR_SEPARATORS) - 1;
  }
  strcpy(p, filename);

  if (stat(pathname, &sbuf) < 0) {
    free(pathname);
    return -1;
  }
  if ((sbuf.st_mode & S_IFMT) != glob->mask) {
    free(pathname);
    return 0;
  }

  glob->pathv[glob->pathc] = pathname;
  glob->pathc++;

  return 0;
}


#ifdef TEST_SGLOB

#include <stdio.h>

int
main(int argc, char *argv[])
{
  sglob_t g;
  int ret;
  int i;
  const char *pattern = NULL;

  if (argc != 3 && argc != 2) {
    fprintf(stderr, "usage: %s BASE-DIR [PATTERN]", argv[0]);
    return 1;
  }
  if (argc == 3)
    pattern = argv[2];

  //g.mask = S_IFSOCK;
  //ret = sglob(argv[1], SGLOB_MASK, &g);
  ret = sglob(argv[1], 0, &g);
  printf("sglob() returns %d\n", ret);
  if (ret < 0)
    return -1;

  for (i = 0; i < g.pathc; i++) {
    printf("%s\n", g.pathv[i]);
  }
  sglobfree(&g);

  return 0;
}
#endif  /* TEST_SGLOB */
