/* GNU fmt -- simple text formatter.
   Copyright (C) 1994-2006 Free Software Foundation, Inc.

   This program is free software: you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation, either version 3 of the License, or
   (at your option) any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program.  If not, see <http://www.gnu.org/licenses/>.  */

/* Originally written by Ross Paterson <rap@doc.ic.ac.uk>.
 * C module by Seong-Kook Shin <cinsky@gmail.com>. */

/* $Id$ */

//#include <config.h>
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <limits.h>

#ifndef DEBUG_OBSTACK
# include <obstack.h>
#else
# include "fakeobs.h"
#endif  /* DEBUG_OBSTACK */

#include <error.h>
#include <ctype.h>
#include <locale.h>
#include <sys/types.h>
#include <getopt.h>

#include <fcntl.h>
#include <sys/types.h>
#include <unistd.h>

#include "fmt.h"

/* Redefine.  Otherwise, systems (Unicos for one) with headers that define
   it to be a type get syntax errors for the variable declaration below.  */
#define word unused_word_type

#define _(x)            x
#define quote(x)        x
#define STREQ(a, b)     (strcmp(a, b))
#ifndef TRUE
#define FALSE   0
#define TRUE    (!FALSE)
#endif

#if 0
#include "system.h"
#include "error.h"
#include "quote.h"
#include "xstrtol.h"
#endif /* 0 */


#define obstack_chunk_alloc     malloc
#define obstack_chunk_free      free

/* The official name of this program (e.g., no `g' prefix).  */
#define PROGRAM_NAME "fmt"

#define AUTHORS "Ross Paterson"

/* The following parameters represent the program's idea of what is
   "best".  Adjust to taste, subject to the caveats given.  */

/* Default longest permitted line length (max_width).  */
#define WIDTH   75

/* Prefer lines to be LEEWAY % shorter than the maximum width, giving
   room for optimization.  */
#define LEEWAY  7

/* The default secondary indent of tagged paragraph used for unindented
   one-line paragraphs not preceded by any multi-line paragraphs.  */
#define DEF_INDENT 3

/* Costs and bonuses are expressed as the equivalent departure from the
   optimal line length, multiplied by 10.  e.g. assigning something a
   cost of 50 means that it is as bad as a line 5 characters too short
   or too long.  The definition of SHORT_COST(n) should not be changed.
   However, EQUIV(n) may need tuning.  */

/* FIXME: "fmt" misbehaves given large inputs or options.  One
   possible workaround for part of the problem is to change COST to be
   a floating-point type.  There are other problems besides COST,
   though; see MAXWORDS below.  */

typedef long int COST;

#define MAXCOST TYPE_MAXIMUM (COST)
/* True if the arithmetic type T is signed.  */
#define TYPE_SIGNED(t) (! ((t) 0 < (t) -1))
#define TYPE_MAXIMUM(t) \
  ((t) (! TYPE_SIGNED (t) \
        ? (t) -1 \
        : ~ (~ (t) 0 << (sizeof (t) * CHAR_BIT - 1))))

#define SQR(n)          ((n) * (n))
#define EQUIV(n)        SQR ((COST) (n))

/* Cost of a filled line n chars longer or shorter than best_width.  */
#define SHORT_COST(n)   EQUIV ((n) * 10)

/* Cost of the difference between adjacent filled lines.  */
#define RAGGED_COST(n)  (SHORT_COST (n) / 2)

/* Basic cost per line.  */
#define LINE_COST       EQUIV (70)

/* Cost of breaking a line after the first word of a sentence, where
   the length of the word is N.  */
#define WIDOW_COST(n)   (EQUIV (200) / ((n) + 2))

/* Cost of breaking a line before the last word of a sentence, where
   the length of the word is N.  */
#define ORPHAN_COST(n)  (EQUIV (150) / ((n) + 2))

/* Bonus for breaking a line at the end of a sentence.  */
#define SENTENCE_BONUS  EQUIV (50)

/* Cost of breaking a line after a period not marking end of a sentence.
   With the definition of sentence we are using (borrowed from emacs, see
   get_line()) such a break would then look like a sentence break.  Hence
   we assign a very high cost -- it should be avoided unless things are
   really bad.  */
#define NOBREAK_COST    EQUIV (600)

/* Bonus for breaking a line before open parenthesis.  */
#define PAREN_BONUS     EQUIV (40)

/* Bonus for breaking a line after other punctuation.  */
#define PUNCT_BONUS     EQUIV(40)

/* Credit for breaking a long paragraph one line later.  */
#define LINE_CREDIT     EQUIV(3)

/* Size of paragraph buffer, in words and characters.  Longer paragraphs
   are handled neatly (cf. flush_paragraph()), so long as these values
   are considerably greater than required by the width.  These values
   cannot be extended indefinitely: doing so would run into size limits
   and/or cause more overflows in cost calculations.  FIXME: Remove these
   arbitrary limits.  */

#define MAXWORDS        1000
#define MAXCHARS        5000

/* Extra ctype(3)-style macros.  */

#define isopen(c)       (strchr ("([`'\"", c) != NULL)
#define isclose(c)      (strchr (")]'\"", c) != NULL)
#define isperiod(c)     (strchr (".?!", c) != NULL)

/* Size of a tab stop, for expansion on input and re-introduction on
   output.  */
#define TABWIDTH        8

/* Word descriptor structure.  */


typedef struct Word WORD;

struct Word
  {

    /* Static attributes determined during input.  */

    const char *text;           /* the text of the word */
    int length;                 /* length of this word */
    int space;                  /* the size of the following space */
    unsigned int paren:1;       /* starts with open paren */
    unsigned int period:1;      /* ends in [.?!])* */
    unsigned int punct:1;       /* ends in punctuation */
    unsigned int final:1;       /* end of sentence */

    /* The remaining fields are computed during the optimization.  */

    int line_length;            /* length of the best line starting here */
    COST best_cost;             /* cost of best paragraph starting here */
    WORD *next_break;           /* break which achieves best_cost */
  };


struct fmt_ {
  int crown;
  int tagged;
  int split;
  int uniform;

  const char *prefix;
  int max_width;

  int prefix_full_length;
  int prefix_lead_space;
  int prefix_length;

  int best_width;

  int in_column;
  int out_column;

  char *parabuf;                /* [MAXCHARS] */
  char *wptr;
  WORD *word;                   /* [MAXWORDS] */
  WORD *word_limit;

  int tabs;
  int prefix_indent;
  int first_indent;
  int other_indent;

  int next_char;
  int next_prefix_indent;
  int last_line_length;

  unsigned flags;
  char *dummy;
  char *input;
  char *output;
#if 0
  crown = tagged = split = uniform = FALSE;
  max_width = WIDTH;
  prefix = "";
  prefix_length = prefix_lead_space = prefix_full_length = 0;
  best_width = max_width * (2 * (100 - LEEWAY) + 1) / 200;
#endif  /* 0 */

  struct obstack *pool;
  struct obstack pool_;
};

/* Forward declarations.  */

static void set_prefix (fmt_t *fmt, char *p);
static int get_paragraph (fmt_t *f);
static int get_line (fmt_t *f, int c);
static int get_prefix (fmt_t *fmt);
static int get_space (fmt_t *fmt, int c);
static int copy_rest (fmt_t *fmt, int c);
static int same_para (fmt_t *f, int c);
static void flush_paragraph (fmt_t *fmt);
static void fmt_paragraph (fmt_t *f);
static void check_punctuation (WORD *w);
static COST base_cost (fmt_t *f, WORD *this);
static COST line_cost (fmt_t *f, WORD *next, int len);
static void put_paragraph (fmt_t *f, WORD *finish);
static void put_line (fmt_t *f, WORD *w, int indent);
static void put_word (fmt_t *f, WORD *w);
static void put_space (fmt_t *f, int space);
static void set_other_indent (fmt_t *fmt, int same_paragraph);
static void fmt_reset(fmt_t *f);

#if 0
/* The name this program was run with.  */
const char *program_name;

/* Option values.  */

/* If TRUE, first 2 lines may have different indent (default FALSE).  */
static int crown;

/* If TRUE, first 2 lines _must_ have different indent (default FALSE).  */
static int tagged;

/* If TRUE, each line is a paragraph on its own (default FALSE).  */
static int split;

/* If TRUE, don't preserve inter-word spacing (default FALSE).  */
static int uniform;

/* Prefix minus leading and trailing spaces (default "").  */
static const char *prefix;

/* User-supplied maximum line width (default WIDTH).  The only output
   lines longer than this will each comprise a single word.  */
static int max_width;

/* Values derived from the option values.  */

/* The length of prefix minus leading space.  */
static int prefix_full_length;

/* The length of the leading space trimmed from the prefix.  */
static int prefix_lead_space;

/* The length of prefix minus leading and trailing space.  */
static int prefix_length;

/* The preferred width of text lines, set to LEEWAY % less than max_width.  */
static int best_width;

/* Dynamic variables.  */

/* Start column of the character most recently read from the input file.  */
static int in_column;

/* Start column of the next character to be written to stdout.  */
static int out_column;

/* Space for the paragraph text -- longer paragraphs are handled neatly
   (cf. flush_paragraph()).  */
static char parabuf[MAXCHARS];

/* A pointer into parabuf, indicating the first unused character position.  */
static char *wptr;

/* The words of a paragraph -- longer paragraphs are handled neatly
   (cf. flush_paragraph()).  */
static WORD word[MAXWORDS];

/* A pointer into the above word array, indicating the first position
   after the last complete word.  Sometimes it will point at an incomplete
   word.  */
static WORD *word_limit;

/* If TRUE, current input file contains tab characters, and so tabs can be
   used for white space on output.  */
static int tabs;

/* Space before trimmed prefix on each line of the current paragraph.  */
static int prefix_indent;

/* Indentation of the first line of the current paragraph.  */
static int first_indent;

/* Indentation of other lines of the current paragraph */
static int other_indent;

/* To detect the end of a paragraph, we need to look ahead to the first
   non-blank character after the prefix on the next line, or the first
   character on the following line that failed to match the prefix.
   We can reconstruct the lookahead from that character (next_char), its
   position on the line (in_column) and the amount of space before the
   prefix (next_prefix_indent).  See get_paragraph() and copy_rest().  */

/* The last character read from the input file.  */
static int next_char;

/* The space before the trimmed prefix (or part of it) on the next line
   after the current paragraph.  */
static int next_prefix_indent;

/* If nonzero, the length of the last line output in the current
   paragraph, used to charge for raggedness at the split point for long
   paragraphs chosen by fmt_paragraph().  */
static int last_line_length;
#endif  /* 0 */

static inline int
fmt_getc(fmt_t *fmt)
{
  int c;

  if (fmt->input == NULL)
    return '\0';

  c = *fmt->input;
  if (c == '\0')
    return '\0';
  fmt->input++;
  return c;
}


fmt_t *
fmt_new(unsigned flags)
{
  fmt_t *p;
  int i;

  p = malloc(sizeof(*p));
  if (!p)
    return NULL;

  p->pool = &p->pool_;
  obstack_init(p->pool);

  p->parabuf = obstack_alloc(p->pool, MAXCHARS);
  p->word = obstack_alloc(p->pool, sizeof(WORD) * MAXWORDS);
  for (i = 0; i < MAXWORDS; i++) {
    p->word[i].text = NULL;
    p->word[i].length = 0;
    p->word[i].space = 0;
    p->word[i].paren = 0;
    p->word[i].period = 0;
    p->word[i].punct = 0;
    p->word[i].final = 0;
    p->word[i].line_length = 0;
    p->word[i].best_cost = 0;
    p->word[i].next_break = NULL;
  }

  p->in_column = 0;
  p->out_column = 0;
  p->wptr = NULL;
  p->word_limit = NULL;
  p->tabs = TRUE;               /* ?? */
  p->prefix_indent = 0;
  p->first_indent = 0;
  p->other_indent = 0;
  p->next_char = 0;
  p->next_prefix_indent = 0;
  p->last_line_length = 0;

  p->dummy = p->input = p->output = NULL;

  p->crown = p->tagged = p->split = p->uniform = FALSE;
  p->max_width = WIDTH;
  p->prefix = "";
  p->prefix_length = p->prefix_lead_space = p->prefix_full_length = 0;
  p->best_width = p->max_width * (2 * (100 - LEEWAY) + 1) / 200;

  p->flags = flags;
  return p;
}


void
fmt_set_width(fmt_t *p, int width)
{
  p->max_width = width;
  p->best_width = p->max_width * (2 * (100 - LEEWAY) + 1) / 200;
}


void
fmt_delete(fmt_t *fmt)
{
  obstack_free(fmt->pool, NULL);
  free(fmt);
}


#if 0
void
usage (int status)
{
  if (status != EXIT_SUCCESS)
    fprintf (stderr, _("Try `%s --help' for more information.\n"),
             program_name);
  else
    {
      printf (_("Usage: %s [-DIGITS] [OPTION]... [FILE]...\n"), program_name);
      fputs (_("\
Reformat each paragraph in the FILE(s), writing to standard output.\n\
If no FILE or if FILE is `-', read standard input.\n\
\n\
"), stdout);
      fputs (_("\
Mandatory arguments to long options are mandatory for short options too.\n\
"), stdout);
      fputs (_("\
  -c, --crown-margin        preserve indentation of first two lines\n\
  -p, --prefix=STRING       reformat only lines beginning with STRING,\n\
                              reattaching the prefix to reformatted lines\n\
  -s, --split-only          split long lines, but do not refill\n\
"),
             stdout);
      fputs (_("\
  -t, --tagged-paragraph    indentation of first line different from second\n\
  -u, --uniform-spacing     one space between words, two after sentences\n\
  -w, --width=WIDTH         maximum line width (default of 75 columns)\n\
"), stdout);
      fputs (HELP_OPTION_DESCRIPTION, stdout);
      fputs (VERSION_OPTION_DESCRIPTION, stdout);
      fputs (_("\
\n\
With no FILE, or when FILE is -, read standard input.\n"),
             stdout);
      emit_bug_reporting_address ();
    }
  exit (status);
}

/* Decode options and launch execution.  */

static const struct option long_options[] =
  {
    {"crown-margin", no_argument, NULL, 'c'},
    {"prefix", required_argument, NULL, 'p'},
    {"split-only", no_argument, NULL, 's'},
    {"tagged-paragraph", no_argument, NULL, 't'},
    {"uniform-spacing", no_argument, NULL, 'u'},
    {"width", required_argument, NULL, 'w'},
    //{GETOPT_HELP_OPTION_DECL},
    //{GETOPT_VERSION_OPTION_DECL},
    {NULL, 0, NULL, 0},
  };
#endif  /* 0 */


/* Trim space from the front and back of the string P, yielding the prefix,
   and record the lengths of the prefix and the space trimmed.  */

static void
set_prefix (fmt_t *fmt, char *p)
{
  char *s;

  fmt->prefix_lead_space = 0;
  while (*p == ' ')
    {
      fmt->prefix_lead_space++;
      p++;
    }
  fmt->prefix = p;
  fmt->prefix_full_length = strlen (p);
  s = p + fmt->prefix_full_length;
  while (s > p && s[-1] == ' ')
    s--;
  *s = '\0';
  fmt->prefix_length = s - p;
}

/* read file F and send formatted output to stdout.  */


char **
fmt_vectorize(fmt_t *f)
{
  register char *p;
  char *q, *r;
  void *vp;
  size_t size;

  assert(obstack_object_size(f->pool) == 0);

  if (!f->output)
    return NULL;

  for (p = q = f->output; *p != '\0'; p++) {
    if (*p == '\n') {
      *p = '\0';

      if (f->flags & FF_MALLOC_STR) {
        r = strdup(q);
        if (!r) {
          /* TODO: deallocate all PTR in OBSTACK */
          return NULL;
        }
      }
      else
        r = q;

      obstack_ptr_grow(f->pool, r);
      q = p + 1;

#if 0
      if (f->flags & FF_MALLOC)
        obstack_ptr_grow(f->pool, (void *)(q - f->output));
      else
        obstack_ptr_grow(f->pool, q);
      q = p + 1;
#endif  /* 0 */
    }
  }
  if (p != q) {
    if (f->flags & FF_MALLOC_STR) {
      q = strdup(q);
      if (!q) {
        /* TODO: deallocate all PTR in OBSTACK */
        return NULL;
      }
    }
    obstack_ptr_grow(f->pool, q);
  }
  obstack_ptr_grow(f->pool, NULL);

  if (f->flags & FF_MALLOC_VEC) {
    size = obstack_object_size(f->pool);
    vp = malloc(size);
    if (!vp) {
      /* TODO: deallocate all PTR in OBSTACK */
    }
    memcpy(vp, obstack_base(f->pool), size);
    obstack_free(f->pool, obstack_finish(f->pool));
  }
  else
    vp = obstack_finish(f->pool);

#if 0
  if (f->flags & FF_MALLOC) {
    char **v;
    int i;

    size = obstack_object_size(f->pool);
    vp = malloc(size + p - f->output + 1);
    memcpy(vp, obstack_finish(f->pool), size);
    memcpy(vp + size, f->output, p - f->output + 1);

    v = vp;
    for (i = 0; i < size / sizeof(void *) - 1; i++) {
      v[i] = (int)v[i] + (char *)vp + size;
    }
  }
  else
    vp = obstack_finish(f->pool);
#endif  /* 0 */

  return vp;
}


static void
fmt_reset(fmt_t *f)
{
  f->tabs = FALSE;
  f->other_indent = 0;

  if (f->dummy) {
    obstack_free(f->pool, f->dummy);

    f->crown = f->tagged = f->split = f->uniform = FALSE;
    f->max_width = WIDTH;
    f->prefix = "";
    f->prefix_length = f->prefix_lead_space = f->prefix_full_length = 0;
    f->best_width = f->max_width * (2 * (100 - LEEWAY) + 1) / 200;

    f->in_column = 0;
    f->out_column = 0;
    f->wptr = NULL;
    f->word_limit = NULL;
    f->tabs = TRUE;               /* ?? */
    f->prefix_indent = 0;
    f->first_indent = 0;
    f->other_indent = 0;
    f->next_char = 0;
    f->next_prefix_indent = 0;
    f->last_line_length = 0;
  }
  f->dummy = f->input = NULL;
}


char *
fmt_format (fmt_t *f, const char *s)
{
  fmt_reset(f);

  if (!s)
    return NULL;

  f->dummy = obstack_alloc(f->pool, 1);
  f->input = obstack_copy0(f->pool, s, strlen(s));

  f->next_char = get_prefix (f);
  while (get_paragraph (f))
    {
      fmt_paragraph (f);
      put_paragraph (f, f->word_limit);
    }
  obstack_1grow(f->pool, '\0');
  f->output = obstack_finish(f->pool);

  return f->output;
}

/* Set the global variable `other_indent' according to SAME_PARAGRAPH
   and other global variables.  */

static void
set_other_indent (fmt_t *fmt, int same_paragraph)
{
  if (fmt->split)
    fmt->other_indent = fmt->first_indent;
  else if (fmt->crown)
    {
      fmt->other_indent = (same_paragraph ? fmt->in_column : fmt->first_indent);
    }
  else if (fmt->tagged)
    {
      if (same_paragraph && fmt->in_column != fmt->first_indent)
        {
          fmt->other_indent = fmt->in_column;
        }

      /* Only one line: use the secondary indent from last time if it
         splits, or 0 if there have been no multi-line paragraphs in the
         input so far.  But if these rules make the two indents the same,
         pick a new secondary indent.  */

      else if (fmt->other_indent == fmt->first_indent)
        fmt->other_indent = fmt->first_indent == 0 ? DEF_INDENT : 0;
    }
  else
    {
      fmt->other_indent = fmt->first_indent;
    }
}

/* Read a paragraph from input file F.  A paragraph consists of a
   maximal number of non-blank (excluding any prefix) lines subject to:
   * In split mode, a paragraph is a single non-blank line.
   * In crown mode, the second and subsequent lines must have the
   same indentation, but possibly different from the indent of the
   first line.
   * Tagged mode is similar, but the first and second lines must have
   different indentations.
   * Otherwise, all lines of a paragraph must have the same indent.
   If a prefix is in effect, it must be present at the same indent for
   each line in the paragraph.

   Return FALSE if end-of-file was encountered before the start of a
   paragraph, else TRUE.  */

static int
get_paragraph (fmt_t *f)
{
  int c;

  f->last_line_length = 0;
  c = f->next_char;

  /* Scan (and copy) blank lines, and lines not introduced by the prefix.  */

  while (c == '\n' || c == '\0'
         || f->next_prefix_indent < f->prefix_lead_space
         || f->in_column < f->next_prefix_indent + f->prefix_full_length)
    {
      c = copy_rest (f, c);
      if (c == '\0')
        {
          f->next_char = '\0';
          return FALSE;
        }
      obstack_1grow(f->pool, '\n');
      c = get_prefix (f);
    }

  /* Got a suitable first line for a paragraph.  */

  f->prefix_indent = f->next_prefix_indent;
  f->first_indent = f->in_column;
  f->wptr = f->parabuf;
  f->word_limit = f->word;
  c = get_line (f, c);
  set_other_indent (f, same_para (f, c));

  /* Read rest of paragraph (unless split is specified).  */

  if (f->split)
    {
      /* empty */
    }
  else if (f->crown)
    {
      if (same_para (f, c))
        {
          do
            {                   /* for each line till the end of the para */
              c = get_line (f, c);
            }
          while (same_para (f, c) && f->in_column == f->other_indent);
        }
    }
  else if (f->tagged)
    {
      if (same_para (f, c) && f->in_column != f->first_indent)
        {
          do
            {                   /* for each line till the end of the para */
              c = get_line (f, c);
            }
          while (same_para (f, c) && f->in_column == f->other_indent);
        }
    }
  else
    {
      while (same_para (f, c) && f->in_column == f->other_indent)
        c = get_line (f, c);
    }
  (f->word_limit - 1)->period = (f->word_limit - 1)->final = TRUE;
  f->next_char = c;
  return TRUE;
}

/* Copy to the output a line that failed to match the prefix, or that
   was blank after the prefix.  In the former case, C is the character
   that failed to match the prefix.  In the latter, C is \n or EOF.
   Return the character (\n or EOF) ending the line.  */

static int
copy_rest (fmt_t *fmt, int c)
{
  const char *s;

  fmt->out_column = 0;
  if (fmt->in_column > fmt->next_prefix_indent || (c != '\n' && c != '\0'))
    {
      put_space (fmt, fmt->next_prefix_indent);
      for (s = fmt->prefix;
           fmt->out_column != fmt->in_column && *s; fmt->out_column++) {
        //putchar(*s);
        obstack_1grow(fmt->pool, *s++);
      }
      if (c != '\0' && c != '\n')
        put_space (fmt, fmt->in_column - fmt->out_column);
      if (c == '\0' &&
          fmt->in_column >= fmt->next_prefix_indent + fmt->prefix_length)
        obstack_1grow(fmt->pool, '\n');
    }
  while (c != '\n' && c != '\0')
    {
      //putchar(c);
      obstack_1grow(fmt->pool, c);
      c = fmt_getc(fmt);
    }
  return c;
}

/* Return TRUE if a line whose first non-blank character after the
   prefix (if any) is C could belong to the current paragraph,
   otherwise FALSE.  */

static int
same_para (fmt_t *f, int c)
{
  return (f->next_prefix_indent == f->prefix_indent
          && f->in_column >= f->next_prefix_indent + f->prefix_full_length
          && c != '\n' && c != '\0');
}

/* Read a line from input file F, given first non-blank character C
   after the prefix, and the following indent, and break it into words.
   A word is a maximal non-empty string of non-white characters.  A word
   ending in [.?!]["')\]]* and followed by end-of-line or at least two
   spaces ends a sentence, as in emacs.

   Return the first non-blank character of the next line.  */

static int
get_line (fmt_t *f, int c)
{
  int start;
  char *end_of_parabuf;
  WORD *end_of_word;

  end_of_parabuf = &f->parabuf[MAXCHARS];
  end_of_word = &f->word[MAXWORDS - 2];

  do
    {                           /* for each word in a line */

      /* Scan word.  */

      f->word_limit->text = f->wptr;
      do
        {
          if (f->wptr == end_of_parabuf)
            {
              set_other_indent (f, TRUE);
              flush_paragraph (f);
            }
          *f->wptr++ = c;
          c = fmt_getc(f);
        }
      while (c != '\0' && !isspace (c));
      f->in_column += f->word_limit->length = f->wptr - f->word_limit->text;
      check_punctuation (f->word_limit);

      /* Scan inter-word space.  */

      start = f->in_column;
      c = get_space (f, c);
      f->word_limit->space = f->in_column - start;
      f->word_limit->final = (c == '\0'
                              || (f->word_limit->period
                                  && (c == '\n' || f->word_limit->space > 1)));
      if (c == '\n' || c == '\0' || f->uniform)
        f->word_limit->space = f->word_limit->final ? 2 : 1;
      if (f->word_limit == end_of_word)
        {
          set_other_indent (f, TRUE);
          flush_paragraph (f);
        }
      f->word_limit++;
    }
  while (c != '\n' && c != '\0');
  return get_prefix (f);
}

/* Read a prefix from input file F.  Return either first non-matching
   character, or first non-blank character after the prefix.  */

static int
get_prefix (fmt_t *fmt)
{
  int c;

  fmt->in_column = 0;

  c = get_space (fmt, fmt_getc(fmt));

  if (fmt->prefix_length == 0)
    fmt->next_prefix_indent = fmt->prefix_lead_space < fmt->in_column ?
      fmt->prefix_lead_space : fmt->in_column;
  else
    {
      const char *p;
      fmt->next_prefix_indent = fmt->in_column;
      for (p = fmt->prefix; *p != '\0'; p++)
        {
          unsigned char pc = *p;
          if (c != pc)
            return c;
          fmt->in_column++;
          c = fmt_getc(fmt);
        }
      c = get_space (fmt, c);
    }
  return c;
}

/* Read blank characters from input file F, starting with C, and keeping
   in_column up-to-date.  Return first non-blank character.  */

static int
get_space (fmt_t *fmt, int c)
{
  for (;;)
    {
      if (c == ' ')
        fmt->in_column++;
      else if (c == '\t')
        {
          fmt->tabs = TRUE;
          fmt->in_column = (fmt->in_column / TABWIDTH + 1) * TABWIDTH;
        }
      else
        return c;
      c = fmt_getc(fmt);
    }
}

/* Set extra fields in word W describing any attached punctuation.  */

static void
check_punctuation (WORD *w)
{
  char const *start = w->text;
  char const *finish = start + (w->length - 1);
  unsigned char fin = *finish;

  w->paren = isopen (*start);
  w->punct = !! ispunct (fin);
  while (start < finish && isclose (*finish))
    finish--;
  w->period = isperiod (*finish);
}

/* Flush part of the paragraph to make room.  This function is called on
   hitting the limit on the number of words or characters.  */

static void
flush_paragraph (fmt_t *fmt)
{
  WORD *split_point;
  WORD *w;
  int shift;
  COST best_break;

  /* In the special case where it's all one word, just flush it.  */

  if (fmt->word_limit == fmt->word)
    {
#if 0
      fwrite (fmt->parabuf, sizeof *fmt->parabuf,
              fmt->wptr - fmt->parabuf, stdout);
#endif  /* 0 */
      obstack_grow(fmt->pool, fmt->parabuf, fmt->wptr - fmt->parabuf);
      fmt->wptr = fmt->parabuf;
      return;
    }

  /* Otherwise:
     - format what you have so far as a paragraph,
     - find a low-cost line break near the end,
     - output to there,
     - make that the start of the paragraph.  */

  fmt_paragraph (fmt);

  /* Choose a good split point.  */

  split_point = fmt->word_limit;
  best_break = MAXCOST;
  for (w = fmt->word->next_break; w != fmt->word_limit; w = w->next_break)
    {
      if (w->best_cost - w->next_break->best_cost < best_break)
        {
          split_point = w;
          best_break = w->best_cost - w->next_break->best_cost;
        }
      if (best_break <= MAXCOST - LINE_CREDIT)
        best_break += LINE_CREDIT;
    }
  put_paragraph (fmt, split_point);

  /* Copy text of words down to start of parabuf -- we use memmove because
     the source and target may overlap.  */

  memmove (fmt->parabuf, split_point->text, fmt->wptr - split_point->text);
  shift = split_point->text - fmt->parabuf;
  fmt->wptr -= shift;

  /* Adjust text pointers.  */

  for (w = split_point; w <= fmt->word_limit; w++)
    w->text -= shift;

  /* Copy words from split_point down to word -- we use memmove because
     the source and target may overlap.  */

  memmove (fmt->word, split_point,
           (fmt->word_limit - split_point + 1) * sizeof *fmt->word);
  fmt->word_limit -= split_point - fmt->word;
}

/* Compute the optimal formatting for the whole paragraph by computing
   and remembering the optimal formatting for each suffix from the empty
   one to the whole paragraph.  */

static void
fmt_paragraph (fmt_t *f)
{
  WORD *start, *w;
  int len;
  COST wcost, best;
  int saved_length;

  f->word_limit->best_cost = 0;
  saved_length = f->word_limit->length;
  f->word_limit->length = f->max_width;       /* sentinel */

  for (start = f->word_limit - 1; start >= f->word; start--)
    {
      best = MAXCOST;
      len = start == f->word ? f->first_indent : f->other_indent;

      /* At least one word, however long, in the line.  */

      w = start;
      len += w->length;
      do
        {
          w++;

          /* Consider breaking before w.  */

          wcost = line_cost (f, w, len) + w->best_cost;
          if (start == f->word && f->last_line_length > 0)
            wcost += RAGGED_COST (len - f->last_line_length);
          if (wcost < best)
            {
              best = wcost;
              start->next_break = w;
              start->line_length = len;
            }

          /* This is a kludge to keep us from computing `len' as the
             sum of the sentinel length and some non-zero number.
             Since the sentinel w->length may be INT_MAX, adding
             to that would give a negative result.  */
          if (w == f->word_limit)
            break;

          len += (w - 1)->space + w->length;    /* w > start >= word */
        }
      while (len < f->max_width);
      start->best_cost = best + base_cost (f, start);
    }

  f->word_limit->length = saved_length;
}

/* Return the constant component of the cost of breaking before the
   word THIS.  */

static COST
base_cost (fmt_t *f, WORD *this)
{
  COST cost;

  cost = LINE_COST;

  if (this > f->word)
    {
      if ((this - 1)->period)
        {
          if ((this - 1)->final)
            cost -= SENTENCE_BONUS;
          else
            cost += NOBREAK_COST;
        }
      else if ((this - 1)->punct)
        cost -= PUNCT_BONUS;
      else if (this > f->word + 1 && (this - 2)->final)
        cost += WIDOW_COST ((this - 1)->length);
    }

  if (this->paren)
    cost -= PAREN_BONUS;
  else if (this->final)
    cost += ORPHAN_COST (this->length);

  return cost;
}

/* Return the component of the cost of breaking before word NEXT that
   depends on LEN, the length of the line beginning there.  */

static COST
line_cost (fmt_t *f, WORD *next, int len)
{
  int n;
  COST cost;

  if (next == f->word_limit)
    return 0;
  n = f->best_width - len;
  cost = SHORT_COST (n);
  if (next->next_break != f->word_limit)
    {
      n = len - next->line_length;
      cost += RAGGED_COST (n);
    }
  return cost;
}

/* Output to stdout a paragraph from word up to (but not including)
   FINISH, which must be in the next_break chain from word.  */

static void
put_paragraph (fmt_t *f, WORD *finish)
{
  WORD *w;

  put_line (f, f->word, f->first_indent);
  for (w = f->word->next_break; w != finish; w = w->next_break)
    put_line (f, w, f->other_indent);
}

/* Output to stdout the line beginning with word W, beginning in column
   INDENT, including the prefix (if any).  */

static void
put_line (fmt_t *f, WORD *w, int indent)
{
  WORD *endline;

  f->out_column = 0;
  put_space (f, f->prefix_indent);
  //fprintf(stderr, "prefix: |%s|\n", f->prefix);
  if (f->prefix && f->prefix[0] != '\0') {
    obstack_grow(f->pool, f->prefix, strlen(f->prefix));
    //fputs(f->prefix, stdout);
  }
  f->out_column += f->prefix_length;
  put_space (f, indent - f->out_column);

  endline = w->next_break - 1;
  for (; w != endline; w++)
    {
      put_word (f, w);
      put_space (f, w->space);
    }
  put_word (f, w);
  f->last_line_length = f->out_column;
  obstack_1grow(f->pool, '\n');
  //putchar('\n');
}

/* Output to stdout the word W.  */

static void
put_word (fmt_t *f, WORD *w)
{
  const char *s;
  int n;

  s = w->text;
  for (n = w->length; n != 0; n--) {
    //putchar(*s);
    obstack_1grow(f->pool, *s++);
  }
  f->out_column += w->length;
}

/* Output to stdout SPACE spaces, or equivalent tabs.  */

static void
put_space (fmt_t *f, int space)
{
  int space_target, tab_target;

  space_target = f->out_column + space;
  if (f->tabs)
    {
      tab_target = space_target / TABWIDTH * TABWIDTH;
      if (f->out_column + 1 < tab_target)
        while (f->out_column < tab_target)
          {
            //putchar('\t');
            obstack_1grow(f->pool, '\t');
            f->out_column = (f->out_column / TABWIDTH + 1) * TABWIDTH;
          }
    }
  while (f->out_column < space_target)
    {
      //putchar(' ');
      obstack_1grow(f->pool, ' ');
      f->out_column++;
    }
}


#ifdef TEST_FMT
#define BUFSIZE 128

int
main(int argc, char *argv[])
{
  int fd;
  int i;
  char buf[BUFSIZE + 1];
  char *s;
  int ch;
  int flags = FF_MALLOC_STR | FF_MALLOC_VEC;

  fmt_t *fmt;
  fmt = fmt_new(flags);

  for (i = 1; i < argc; i++) {
    fd = open(argv[i], O_RDONLY);
    if (fd < 0)
      continue;
    while ((ch = read(fd, buf, BUFSIZE)) > 0) {
      buf[ch] = '\0';
#if 1
      printf("==[BUF]==\n");
      printf("%s\n", buf);
      printf("--[FMT]--\n");
#endif  /* 0 */
      s = fmt_format(fmt, buf);
      printf("%s", s);
      printf("--[FMT VEC]--\n");

      {
        char **vp, **old;
        old = vp = fmt_vectorize(fmt);

        if (vp) {
          while (*vp != NULL) {
            printf("%s\n", *vp);
            if (flags & FF_MALLOC_STR)
              free(*vp);
            vp++;
          }
        }
        if (flags & FF_MALLOC_VEC)
          free(old);
      }
      printf("\n");
      fmt_reset(fmt);
    }
    close(fd);
  }
  fmt_delete(fmt);
  return 0;
}
#endif  /* TEST_FMT */
