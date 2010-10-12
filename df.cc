/* df: a class that implements df(1) in function level.
 * Copyright (C) 2010  Seong-Kook Shin <cinsky@gmail.com>
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

// $Id$

#include <cassert>
#include <mntent.h>
#include <cstdio>
#include <cstdarg>
#include <cstdlib>
#include "df.hpp"

DiskFree::DiskFree(bool call_update)
  : mtab_path("/etc/mtab"), failbit_(false)
{
  if (call_update)
    update();
}


bool
DiskFree::push_back(const char *path, const char *fs)
{
  value_type val;

  assert(path != 0 && fs != 0);

  if (statvfs(path, &val.vfs) != 0) {
    failbit_ = true;
    return false;
  }
  val.mounted = path;
  val.device = fs;
  container_.push_back(val);
  return true;
}


//
// (Hopefully,) Useful wrapper of getmntent_r(3)
//
// xgetmntent saves the mount entry in MNTBUF, and string information into
// BUF that has size, SIZE.
//
// If BUF is NULL, xgetmntent() will create it automatically.  In that
// case, if SIZE is non-zero, SIZE will be filled with the size of
// BUF.
//
// If BUF is not large enough, it will reallocate memory using
// realloc(3), and set BUF and SIZE accordingly.
//
// xgetmntent() returns true on success, otherwise returns false.
//
// Note that on xgetmntent() will DEALLOCATE BUF in EOF or in case of
// error.  This means that you do not need to call free(3) on normal
// occasion:
//
//   char *buf = 0;
//   size_t size;
//   struct mntent mbuf;
//   fp = setmntent(...);
//   while (xgetmntent(fp, &mbuf, &buf, &size)) {
//     ...
//   }

static bool
xgetmntent(FILE *fp, struct mntent *mntbuf, char **buf, size_t *size)
{
  /*
   * I don't know why, but getmntent_r() seems not to fail even if the
   * buffer size is really small on my Gentoo linux 2.6.34 with glibc 2.11.2
   *
   * It seems that it truncated every string in MNTBUF rather than
   * returns -1.  Thus, I need to allocate enough buffer so that
   * getmntent_r() never complain for the small buffer.
   */
  size_t sz = 4096;
  const int repeat_count = 4;
  int i;
  char *p, *q;

  assert(fp != NULL && mntbuf != NULL);
  assert(buf == NULL || ((buf != NULL) && (size != NULL)));

  if (!*buf || *size < 4096) {
    p = static_cast<char *>(realloc(*buf, sz));
    if (!p)
      return false;
    if (size)
      *size = sz;
  }
  else {
    p = *buf;
    sz = *size;
  }

  for (i = 0; i < repeat_count; ++i) {
    // Repeat for the reasonable times
    //
    // TODO: If my claim above is true, there's no reason to repeat, right??
    if (getmntent_r(fp, mntbuf, p, sz) != 0)
      break;
    else if (feof(fp))
      break;

    sz += 512;
    q = static_cast<char *>(realloc(p, sz));
    if (!q) {
      free(p);
      return false;
    }
    else
      p = q;
  }
  if (feof(fp) || i == repeat_count) {
    free(p);
    return false;
  }
  *buf = p;
  if (size)
    *size = sz;
  return true;
}


bool
DiskFree::update()
{
  FILE *mtab = setmntent(mtab_path, "r");
  mntent ent;
  char *buf = 0;
  size_t bufsize = 0;

  reset();

  if (!mtab) {
    failbit_ = true;
    return false;
  }

  try {
    while (xgetmntent(mtab, &ent, &buf, &bufsize) != 0) {
      push_back(ent.mnt_dir, ent.mnt_fsname);
      //free(buf);
      //buf = 0;
    }
  }
  catch (...) {
    free(buf);
    failbit_ = true;
    endmntent(mtab);
    throw;
  }
  endmntent(mtab);

  return true;
}


bool
DiskFree::update(int argc, const char *argv[])
{
  reset();
  try {
    for (int i = 0; i < argc; i++) {
      if (argv[i])
        push_back(argv[i]);
    }
  }
  catch (...) {
    failbit_ = true;
    throw;
  }
  return true;
}


bool
DiskFree::update(const char *fs_path, ...)
{
  std::va_list ap;

  reset();

  try {
    if (!fs_path)
      return true;

    push_back(fs_path);

    va_start(ap, fs_path);
    const char *arg;
    while ((arg = static_cast<const char *>(va_arg(ap, const char *))) != 0)
      push_back(arg);
    va_end(ap);
  }
  catch (...) {
    failbit_ = true;
    throw;
  }

  return true;
}


#ifdef TEST_DF

#include <iostream>
#include <iomanip>

int
main(int argc, char *argv[])
{
  DiskFree df;

  if (argc == 1)
    df.update();

  if (df) {
    for (DiskFree::iterator i = df.begin(); i != df.end(); ++i) {
      if (i->vfs.f_blocks <= 0)
        continue;
      std::cout << std::left << std::setw(20) << i->device << " ";
      std::cout << std::right << std::setw(9) << df.blocks(i) << " ";
      std::cout << std::setw(9) << df.used_blocks(i) << " ";
      std::cout << std::setw(9) << df.avail_blocks(i) << " ";
      std::cout << std::setw(3) << df.percent_blocks(i) << "% ";
      std::cout << i->mounted << "\n";
    }
  }
  return 0;
}
#endif // TEST_DF
