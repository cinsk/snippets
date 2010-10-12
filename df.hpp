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
#ifndef DISKFREE_HPP__
#define DISKFREE_HPP__

// $Id$

#ifndef __cplusplus
#error This is a C++ header file.
#endif

#include <vector>
#include <string>
#include <sys/statvfs.h>

// TODO: failbit_ operation is not fully tested.
//       currently, failbit_ is cleared iff reset() has been called.

class DiskFree {
public:
  typedef struct {
    std::string mounted;
    std::string device;
    struct statvfs vfs;
  } value_type;

  typedef std::vector<value_type> container_type;
  typedef container_type::size_type size_type;
  typedef container_type::iterator iterator;
  typedef container_type::reverse_iterator reverse_iterator;
  typedef container_type::const_reverse_iterator const_reverse_iterator;
  typedef container_type::const_iterator const_iterator;

  // TODO: I'm not sure whether it is good idea to mimic a STL container.
  // TODO: Perhaps exposing container_ is better?
  iterator begin()                      { return container_.begin(); }
  const_iterator begin() const          { return container_.begin(); }
  iterator end()                        { return container_.end(); }
  const_iterator end() const            { return container_.end(); }
  const_reverse_iterator rbegin() const { return container_.rbegin(); }
  reverse_iterator rbegin()             { return container_.rbegin(); }
  reverse_iterator rend()               { return container_.rend(); }
  const_reverse_iterator rend() const   { return container_.rend(); }

  const char * const mtab_path;

  explicit DiskFree(bool call_update = false);

  bool update();
  bool update(const char *fs_path, ...);
  bool update(int argc, const char *argv[]);

  operator void *() const {
    return failbit_ ? 0 : const_cast<DiskFree *>(this);
  }

  bool fail() const { return failbit_; }
  void reset() { failbit_ = false; container_.empty(); }

  size_type size() const { return container_.size(); }

  // Return the total number of blocks in K.
  unsigned long blocks(iterator i) const;
  unsigned long blocks(const_iterator i) const;

  // Return the used number of blocks in K.
  unsigned long used_blocks(iterator i) const;
  unsigned long used_blocks(const_iterator i) const;

  // Return the available(free) number of blocks in K.
  unsigned long avail_blocks(iterator i) const;
  unsigned long avail_blocks(const_iterator i) const;

  // Return the used percentage of disk (%)
  unsigned percent_blocks(iterator i) const;
  unsigned percent_blocks(const_iterator i) const;

private:
  bool push_back(const char *path, const char *fs = 0);
  inline unsigned long scale(unsigned long blocks,
                             unsigned long blocksize) const;

  bool failbit_;
  container_type container_;
};


inline unsigned long
DiskFree::scale(unsigned long b, unsigned long bs) const
{
  return (static_cast<unsigned long long>(bs) * b + 1024 / 2) / 1024;
}


inline unsigned long
DiskFree::blocks(iterator i) const {
  // TODO: why const_cast<> failed??
  return blocks(static_cast<const_iterator>(i));
}

inline unsigned long
DiskFree::blocks(const_iterator i) const {
  return scale(i->vfs.f_blocks, i->vfs.f_bsize);
}

inline unsigned long
DiskFree::used_blocks(iterator i) const {
  return used_blocks(static_cast<const_iterator>(i));
}

inline unsigned long
DiskFree::used_blocks(const_iterator i) const {
  return scale(i->vfs.f_blocks - i->vfs.f_bfree, i->vfs.f_bsize);
}

inline unsigned long
DiskFree::avail_blocks(iterator i) const {
  return avail_blocks(static_cast<const_iterator>(i));
}

inline unsigned long
DiskFree::avail_blocks(const_iterator i) const {
  return scale(i->vfs.f_bavail, i->vfs.f_bsize);
}

inline unsigned
DiskFree::percent_blocks(iterator i) const {
  return percent_blocks(static_cast<const_iterator>(i));
}

inline unsigned
DiskFree::percent_blocks(const_iterator i) const {
  unsigned long b_used = i->vfs.f_blocks - i->vfs.f_bfree;

  if (b_used + i->vfs.f_bavail)
    return (b_used * 100ULL + (b_used + i->vfs.f_bavail) / 2) /
      (b_used + i->vfs.f_bavail);
  else
    return 0;
}


#endif  // DISKFREE_HPP__
