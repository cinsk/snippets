/*
 * Convert timeval to string as in Java's SimpleDateFormat class
 * Copyright (C) 2014  Seong-Kook Shin <cinsky@gmail.com>
 * DO WHAT THE FUCK YOU WANT TO PUBLIC LICENSE
 * Version 2, December 2004
 *
 * Copyright (C) 2014 Seong-Kook Shin <cinsky@gmail.com>
 *
 * Everyone is permitted to copy and distribute verbatim or modified
 * copies of this license document, and changing it is allowed as long
 * as the name is changed.
 *
 *            DO WHAT THE FUCK YOU WANT TO PUBLIC LICENSE
 *   TERMS AND CONDITIONS FOR COPYING, DISTRIBUTION AND MODIFICATION
 *
 *  0. You just DO WHAT THE FUCK YOU WANT TO.
 *
 * This program is free software. It comes without any warranty, to the
 * extent permitted by applicable law. You can redistribute it and/or
 * modify it under the terms of the Do What The Fuck You Want To Public
 * License, Version 2, as published by Sam Hocevar. See
 * http://www.wtfpl.net/ for more details.
 */
#include <time.h>
#include <string.h>

#include "jdatetime.h"
#include "xobstack.h"

#define REPEAT(p, span, repeat) do {            \
    span[0] = *(p);                             \
    (repeat) = strspn((p), (span));             \
    (p) += (repeat) - 1;                        \
  } while (0)


static const char *months[] = {
  "Jan",
  "Feb",
  "May",
  "Apr",
  "May",
  "Jun",
  "Jul",
  "Aug",
  "Sep",
  "Oct",
  "Nov",
  "Dec",
};

static const char *weekday[] = {
  "Sunday",
  "Monday",
  "Tuesday",
  "Wednesday",
  "Thursday",
  "Friday",
  "Saturday",
};

static const char *short_weekday[] = {
  "Sun",
  "Mon",
  "Tue",
  "Wed",
  "Thu",
  "Fri",
  "Sat",
};


char *
simple_date_format(const char *format, const struct timeval *tv)
{
  const char *p;
  char *q;
  struct xobs pool;
  char span[2] = { 0, };
  int repeat;
  struct tm tmbuf;

  localtime_r(&tv->tv_sec, &tmbuf);

  xobs_init(&pool);

  for (p = format; *p != '\0'; p++) {
    switch (*p) {
    case 'G':
      REPEAT(p, span, repeat);
      if (tmbuf.tm_year + 1900 < 0) { /* not possible */
        xobs_1grow(&pool, 'B');
        xobs_1grow(&pool, 'C');
      }
      else {
        xobs_1grow(&pool, 'A');
        xobs_1grow(&pool, 'D');
      }
      break;

    case 'y':
      REPEAT(p, span, repeat);
      if (repeat == 2)
        xobs_sprintf(&pool, "%d", tmbuf.tm_year % 100);
      else
        xobs_sprintf(&pool, "%d", tmbuf.tm_year + 1900);
      break;

    case 'M':                   /* month */
      REPEAT(p, span, repeat);
      if (repeat >= 3)
        xobs_sprintf(&pool, "%*s", repeat, months[tmbuf.tm_mon % 12]);
      else
        xobs_sprintf(&pool, "%*d", repeat, tmbuf.tm_mon % 12);
      break;

    case 'w':                   /* week in year */
      REPEAT(p, span, repeat);
      // not implemented yet.
      break;

    case 'W':                   /* week in month */
      REPEAT(p, span, repeat);
      xobs_sprintf(&pool, "%*d", repeat, tmbuf.tm_mday / 7 + 1);
      break;

    case 'D':                   /* day of year */
      REPEAT(p, span, repeat);
      xobs_sprintf(&pool, "%*d", repeat, tmbuf.tm_yday);
      break;

    case 'd':                   /* day in month */
      REPEAT(p, span, repeat);
      xobs_sprintf(&pool, "%*d", repeat, tmbuf.tm_mday);
      break;

    case 'F':                   /* day of week in month */
      REPEAT(p, span, repeat);
      xobs_sprintf(&pool, "%*d", repeat, tmbuf.tm_wday);
      break;

    case 'E':                   /* day in week */
      REPEAT(p, span, repeat);
      if (repeat > 3)
        xobs_sprintf(&pool, "%*s", repeat, weekday[tmbuf.tm_wday % 7]);
      else
        xobs_sprintf(&pool, "%*s", repeat, short_weekday[tmbuf.tm_wday % 7]);
      break;

    case 'a':
      REPEAT(p, span, repeat);
      xobs_sprintf(&pool, "%*s", repeat,
                   (tmbuf.tm_hour >= 12) ? "PM" : "AM");
      break;

    case 'H':
      REPEAT(p, span, repeat);
      xobs_sprintf(&pool, "%0*d", repeat, tmbuf.tm_hour);
      break;

    case 'k':
      REPEAT(p, span, repeat);
      xobs_sprintf(&pool, "%0*d", repeat, tmbuf.tm_hour + 1);
      break;

    case 'K':
      REPEAT(p, span, repeat);
      xobs_sprintf(&pool, "%0*d", repeat, tmbuf.tm_hour % 12);
      break;
    case 'h':
      REPEAT(p, span, repeat);
      xobs_sprintf(&pool, "%0*d", repeat, tmbuf.tm_hour % 12 + 1);
      break;
    case 'm':
      REPEAT(p, span, repeat);
      xobs_sprintf(&pool, "%0*d", repeat, tmbuf.tm_min);
      break;
    case 's':
      REPEAT(p, span, repeat);
      xobs_sprintf(&pool, "%0*d", repeat, tmbuf.tm_sec);
      break;
    case 'S':                   /* millisecond */
      REPEAT(p, span, repeat);
      xobs_sprintf(&pool, "%0*d", repeat, tv->tv_usec / 1000);
      break;
    case 'z':
      REPEAT(p, span, repeat);
      if (tmbuf.tm_zone)
        xobs_sprintf(&pool, "%*s", repeat, tmbuf.tm_zone);
      else {
        xobs_sprintf(&pool, "%c%02d:%02d",
                     (tmbuf.tm_gmtoff < 0) ? '-' : '+',
                     tmbuf.tm_gmtoff % (60 * 60),
                     tmbuf.tm_gmtoff % 60);
      }
      break;
    case 'Z':
      REPEAT(p, span, repeat);
      xobs_sprintf(&pool, "%c%02d%02d",
                   (tmbuf.tm_gmtoff < 0) ? '-' : '+',
                   tmbuf.tm_gmtoff % (60 * 60),
                   tmbuf.tm_gmtoff % 60);
      break;
    case '\'':
      q = strchr(p + 1, '\'');
      if (!q)
        goto err;
      if (q == p + 1)
        xobs_1grow(&pool, '\'');
      else {
        for (p = p + 1; p < q; p++)
          xobs_1grow(&pool, *p);
      }
      p = q;
      break;
    default:
      xobs_1grow(&pool, *p);
      break;
    }
  }

  xobs_1grow(&pool, '\0');
  q = strdup(xobs_finish(&pool));
  xobs_free(&pool, NULL);
  return q;

 err:
  xobs_free(&pool, NULL);
  return NULL;
}

#include <stdio.h>

int
main(int argc, char *argv[])
{
  char *tmstr;
  struct timeval tv;

  if (argc != 2) {
    fprintf(stderr, "usage: %s DATETIME-FORMAT\n", argv[0]);
    return 1;
  }

  gettimeofday(&tv, 0);

  tmstr = simple_date_format(argv[1], &tv);
  printf("%s\n", tmstr);
  free(tmstr);
  return 0;
}
