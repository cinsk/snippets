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
char *output_filename;
FILE *output_fp;

static void help_and_exit(void);
static void version_and_exit(void);
static void cleanup(void);
static int do_work(const char *pathname);

static struct option options[] = {
  { "help", no_argument, 0, 1 },
  { "version", no_argument, NULL, 2 },
  { "quiet", no_argument, NULL, 'q' },
  { "output", required_argument, NULL, 'o' },
  /* TODO: Insert your choice of options here. */
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
      if (!output_filename)
        error(1, errno, "out of memory");
      break;
    case '?':
      error(0, 0, "Try `%s -h' for more information.\n", program_name);
      break;
    default:
      abort();
    }
  }

  if (output_filename) {
    output_fp = fopen(output_filename, "w");
    if (!output_fp)
      error(1, errno, "cannot open %s for the output", output_filename);
  }
  else
    output_fp = stdout;

  if (optind == argc) {         /* There is no more argument in the
                                   command-line */
    /* TODO: insert code for no argument */
    do_work(NULL);
  }
  else {
    int i;
    for (i = optind; i < argc; i++) {
      int ret;

      /* Process each argv[i] here */
      if (strcmp(argv[i], "-") != 0)
        ret = do_work(argv[i]);
      else
        ret = do_work(NULL);

      if (ret < 0)
        break;
    }
  }

  cleanup();
  return 0;
}


static void
cleanup(void)
{
  if (output_filename)
    free(output_filename);
  if (output_fp != stdout)
    fclose(output_fp);
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


static int
do_work(const char *pathname)
{
  /* 1. If PATHNAME is NULL, use STDIN for the input
   * 2. If OUTPUT_FILENAME is not NULL, use that for the output. In the
   *    below skeleton code, I prepared OUTPUT_FP for your convinience.
   *    Since OUTPUT_FP is used for the entire session, do not close it.
   *    It will be closed automatically.
   * 3. If a critical error occurrs, call exit(EXIT_FAILURE) here after
   *    calling cleanup().
   * 4. If an error occurrs for the PATHNAME only, and if the processing
   *    can continue, return -1. */


  if (pathname)
    fprintf(output_fp, "processing %s...\n", pathname);
  else
    fprintf(output_fp, "processing STDIN...\n");

  return 0;
}

