/* $Id$ */
/*
 * Skeleton source for a filter program
 * Copyright (C) 2005  Seong-Kook Shin <cinsky at gmail dot com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
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
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <getopt.h>

#include <errno.h>
#include <error.h>


#ifndef PROGRAM_NAME
#error Define PROGRAM_NAME before compiling the source
#endif

#ifndef VERSION_STRING
#error Define VERSION_STRING before compiling the source
#endif

const char *program_name = PROGRAM_NAME;
const char *version_string = VERSION_STRING;

int verbose_mode = 1;
const char *output_filename;

static void help_and_exit(void);
static void version_and_exit(void);

static struct option options[] = {
  { "help", no_argument, 0, 1 },
  { "version", no_argument, NULL, 2 },
  { "quiet", no_argument, NULL, 'q' },
  { "output", required_argument, NULL, 'o' },
  { NULL, 0, NULL, 0 },
};


int
main(int argc, char *argv[])
{
  int c;

  while (1) {
    c = getopt_long(argc, argv, "o:q", options, NULL);
    if (c < 0)
      break;

    switch (c) {
    case 1:
      help_and_exit();
    case 2:
      version_and_exit();
    case 'q':
      verbose_mode = 0;
      break;
    case 'o':
      output_filename = strdup(optarg);
      break;
    case '?':
      error(0, 0, "Try `%s -h' for more information.\n", program_name);
      break;
    default:
      abort();
    }
  }

  if (optind == argc) {         /* There is no more argument in the
                                   command-line */
    /* TODO: insert code for no argument */
  }
  else {
    int i;
    for (i = optind; i < argc; i++) {
      /* Process each argv[i] here */
      printf("processing %s\n", argv[i]);
    }
  }

  return 0;
}


static void
help_and_exit(void)
{
  static const char *msgs[] = {
    "usage: " PROGRAM_NAME " [OPTION...] [FILES...]",
    "",
    "  -q, --quiet              quite mode",
    "  -o, --output=FILE        FILE send output to file FILE. If FILE is `-',",
    "                           send output to stdout.",
    "",
    "      --help       display this help and exit",
    "      --version    output version information and exit",
    "",
    "Report bugs to <cinsky at gmail dot com>",
  };
  int i;
  for (i = 0; i < sizeof(msgs) / sizeof(const char *); i++)
    puts(msgs[i]);
  exit(EXIT_SUCCESS);
}


static void
version_and_exit(void)
{
  printf(PROGRAM_NAME " version %s\n", version_string);
  exit(EXIT_SUCCESS);
}

