/* -*-c++-*- */
/* pollxx:  Simple C++ wrapper of poll(2)
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
#ifndef POLLXX_H__
#define POLLXX_H__

#include <iterator>
#include <cstdlib>
#include <errno.h>
#include <poll.h>

#ifndef BEGIN_NAMESPACE
#define BEGIN_NAMESPACE(x)      namespace x {
#define END_NAMESPACE(x)        }
#endif

BEGIN_NAMESPACE(xbridge)

//template <typename PollHandler>

#define POLL_GROW_SIZE  32

class PollHandlerBase {
public:
  bool readable(int fd, short revent, void *arg) { return true; }
  bool writable(int fd, short revent, void *arg) { return true; }
  bool hanged(int fd, short revent, void *arg)   { return true; }
  bool errored(int fd, short revent, void *arg)  { return true; }
};


template<class PollHandler = PollHandlerBase>
class Poll {
public:
  const int grow_size;

  Poll(PollHandler *handlers = NULL)
    : grow_size(POLL_GROW_SIZE), handlers_(handlers),
      pfds_(0), pfds_end_(0), pfds_capacity_(0), handler_args_(0)
  {
  }
  ~Poll() { free(pfds_); }

  typedef pollfd value_type;
  typedef pollfd *pointer;
  typedef const pollfd *const_pointer;
  typedef pollfd *iterator;
  typedef const pollfd *const_iterator;
  typedef pollfd &reference;
  typedef const pollfd &const_reference;
  typedef size_t size_type;
  typedef ptrdiff_t difference_type;

  iterator begin() { return pfds_; }
  iterator end() { return pfds_ + pfds_end_; }

  const_iterator cbegin() const { return pfds_; }
  const_iterator cend() const { return pfds_ + pfds_end_; }

  void push_back(const pollfd &pfd) { return push_back(pfd.fd, pfd.events); }
  void push_back(int fd, short events) {
    if (pfds_end_ >= pfds_capacity_)
      grow();

    pfds_[pfds_end_].fd = fd;
    pfds_[pfds_end_].events = events;
    pfds_[pfds_end_].revents = 0;
    //pfds_[pfds_end_].revents = pfd.revents;

    ++pfds_end_;
  }

  bool poll(int milleseconds = 0, void *args = 0) {
    handler_args_ = args;
    int ret = ::poll(pfds_, pfds_end_, milleseconds);
    if (ret == -1)
      if (errno != EINTR)
        return false;
    return true;
  }

  bool dispatch(void) {
    if (!handlers_)
      return false;

    for (const_iterator i = begin(); i != end(); i++) {
      if ((i->revents & POLLIN || i->revents & POLLPRI)
          && !handlers_->readable(i->fd, i->revents, handler_args_))
        return false;
      if ((i->revents & POLLOUT)
          && !handlers_->writable(i->fd, i->revents, handler_args_))
        return false;

      // Testing for hangup conditions;
      if ((i->revents & POLLHUP
#ifdef POLLRDHUP
           || i->revents & POLLRDHUP
#endif
           ) && !handlers_->hanged(i->fd, i->revents, handler_args_))
        return false;

      // Testing for errors
      if ((i->revents & POLLERR || i->revents & POLLNVAL)
          && !handlers_->errored(i->fd, i->revents, handler_args_))
        return false;
    }
    return true;
  }
  void clear() { pfds_end_ = 0; }

private:
  void grow() {
    pollfd *p;
    size_t newsize = pfds_capacity_ + grow_size;

    p = static_cast<pollfd *>(std::realloc(pfds_, newsize));
    if (!p)
      throw std::bad_alloc();

    pfds_ = p;
    pfds_capacity_ = newsize;
  }

  PollHandler *handlers_;
  pollfd *pfds_;

  size_t pfds_end_;
  size_t pfds_capacity_;

  void *handler_args_;
};


END_NAMESPACE(xbridge)

#endif  /* POLLXX_H__ */
