/* $Id$ */
/* dlog: message logger for debugging
 * Copyright (C) 2004  Seong-Kook Shin <cinsky@gmail.com>
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
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "dlog.h"

#ifndef NO_THREAD
# include <pthread.h>
#else
# define flockfile(fp)          ((void)0)
# define funlockfile(fp)        ((void)0)
#endif  /* NO_THREAD */

static FILE *dlog_fp;
static unsigned dlog_mask;
static const char *dlog_prefix = "dlog";


FILE *
dlog_set_output(FILE *fp)
{
  FILE *old = dlog_fp;

  if (dlog_fp == NULL)
    dlog_fp = stderr;

  dlog_fp = fp;

  return old;
}


unsigned
dlog_set_filter(unsigned mask)
{
  unsigned old;

  old = dlog_mask;
  dlog_mask = mask;

  return old;
}


const char *
dlog_set_prefix(const char *prefix)
{
  const char *old = dlog_prefix;
  dlog_prefix = prefix;
  return old;
}


void
dlog_(int ecode, int status, unsigned category, const char *format, ...)
{
  va_list ap;

  if (dlog_mask & category)
    return;

  if (!dlog_fp)
    dlog_set_output(0);

  flockfile(stdout);
  flockfile(dlog_fp);
  fflush(stdout);
  fprintf(dlog_fp, "%s: ", dlog_prefix);

  if (status)
    fprintf(dlog_fp, "%s: ", strerror(status));

  va_start(ap, format);
  vfprintf(dlog_fp, format, ap);
  va_end(ap);
  fputc('\n', dlog_fp);

  fflush(stderr);
  funlockfile(dlog_fp);
  funlockfile(stdout);

  if (ecode)
    exit(ecode);
}


void
derror(int ecode, int status, const char *format, ...)
{
  va_list ap;

  if (!dlog_fp)
    dlog_set_output_(0);

  flockfile(stdout);
  flockfile(dlog_fp);
  fflush(stdout);
  fprintf(dlog_fp, "%s: ", dlog_prefix);

  if (status)
    fprintf(dlog_fp, "%s: ", strerror(status));

  va_start(ap, format);
  vfprintf(dlog_fp, format, ap);
  va_end(ap);
  fputc('\n', dlog_fp);

  fflush(stderr);
  funlockfile(dlog_fp);
  funlockfile(stdout);

  if (ecode)
    exit(ecode);
}
