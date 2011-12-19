/* xmlparse - easy expat wrapper
 * Copyright (C) 2008, 2009  Seong-Kook Shin <cinsky@gmail.com>
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

#ifndef XMLPARSE_H__
#define XMLPARSE_H__

#include <expat.h>
#include <setjmp.h>

#include "obsutil.h"

/* This indirect using of extern "C" { ... } makes Emacs happy */
#ifndef BEGIN_C_DECLS
# ifdef __cplusplus
#  define BEGIN_C_DECLS extern "C" {
#  define END_C_DECLS   };
# else
#  define BEGIN_C_DECLS
#  define END_C_DECLS
# endif
#endif /* BEGIN_C_DECLS */

BEGIN_C_DECLS

#define XML_NS_SEP      '|'


/*
 * XMLCONTEXT: an opaque XML parser context.
 *
 * Users should never access its member directly!
 */
struct xmlcontext_;
typedef struct xmlcontext_ XMLCONTEXT;

struct xmlelement_;
typedef struct xmlelement_ XMLELEMENT;

typedef void (*xp_start_handler)(XMLCONTEXT *, const char *, const char **);
typedef void (*xp_end_handler)(XMLCONTEXT *, const char *, const char *);
typedef void (*xp_char_handler)(XMLCONTEXT *, const char *, int);

struct xmlcontext_ {
  struct obstack *pool;

  XMLELEMENT *stack;
  unsigned lev;
  unsigned ignore_lev;

  XML_Parser parser;

  char *filename;

  void *data;
  xp_start_handler cb_start;
  xp_end_handler cb_end;
  xp_char_handler cb_char;

  sigjmp_buf jmp;

  struct obstack *tpool;
  struct obstack tpool_;

  struct obstack pool_;
};


static __inline__ unsigned
xp_level(XMLCONTEXT *context)
{
  return context->lev;
}


static __inline__ XML_Parser
xp_parser(XMLCONTEXT *context)
{
  return context->parser;
}


static __inline__ const char *
xp_filename(XMLCONTEXT *context)
{
  return context->filename;
}


/*
 * Create new XMLCONTEXT
 */
XMLCONTEXT *xp_open(const char *pathname);

/*
 * Close all resources of XMLCONTEXT.
 */
void xp_close(XMLCONTEXT *context);

/*
 * Do parse.
 *
 * xp_perform() returns zero on success.
 *
 * It returns -1 on error, and return -2 if xp_cancel() is called.
 */
int xp_perform(XMLCONTEXT *context, int fd);

/*
 * Simple wrappers for XML_SetStartElementHandler,
 * XML_SetEndElementHandler, and XML_SetCharacterDataHandler.
 *
 * Do use these functions instead of XML_SetStartElementHandler,
 * XML_SetEndElementHandler, and XML_SetCharacterDataHandler.  Other
 * handlers of expat library are safe to use directly with expat
 * functions.
 */
void xp_set_start_handler(XMLCONTEXT *context, xp_start_handler callback);
void xp_set_end_handler(XMLCONTEXT *context, xp_end_handler callback);
void xp_set_char_handler(XMLCONTEXT *context, xp_char_handler callback);

void xp_set_user_data(XMLCONTEXT *context, void *data);
void *xp_get_user_data(XMLCONTEXT *context);

/*
 * Parsing position test function.
 *
 * Callable from any of expat handler, it returns nonzero if the
 * current position is exactly the same as the specified position.
 *
 * Starts from the third parameters, all remaining arguments are
 * called ESPEC. ESPEC is the namespace expanded element name.  For
 * example, "http://openapi.samsung.com/api/2.0/|channel" indicates
 * <sec:channel> element. (Of course, somewhere in the XML document
 * specifies xmlns:sec="http://openapi.samsung.com/api/2.0/|channel")
 *
 * The third argument will be compared against the current (top)
 * element of the stack, whereas the fourth argument will be compared
 * against the parent of the current element, etc.
 *
 * For example, xp_equal(context, "foo", "bar") returns nonzero iff
 * the current element is <foo> and its direct parent is <bar>.
 */
int xp_equal(XMLCONTEXT *context, int nelem, ...);


/*
 * Cancel parsing on user defined handlers.
 *
 * This function is provided to the user so that he or she can cancel
 * the parsing process on any handler.  Even it is safe to call
 * xp_cancel on any EXPAT handler includes start/end/char handlers.
 */
void xp_cancel(XMLCONTEXT *context, int val);


/*
 * Set the ignore value of the parser.
 *
 * The XML parser will not call any user-defined handler
 * if the element depth is greater than the ignore value.
 *
 * To unset the ignore value, call xp_ignore with LEV as -1.
 *
 * xp_ignore returns the previous ignore value.
 *
 * For example, if the user-defined start-handler for "<person>" calls
 * xp_ignore(c, 2) on the first encounter of that element, any
 * user-defined start/end-handler for the child elements of the
 * "<person>" will not be called:
 *
 * <list>                 <!-- depth 1 -->
 *   <person>             <!-- depth 2: call xp_ignore(c, 2) -->
 *     <addr>...</addr>   <!-- depth 3: ignored -->
 *     <name>...</name>   <!-- depth 3: ignored -->
 *     ...                <!-- depth >3: ignored -->
 *   </person>
 *
 *   <person id="xxxyyy">         <!-- depth 2: ignore level reset -->
 *   </person>
 * </list>
 *
 * The purpose of xp_ignore() is that to ignore specific elements tree
 * that is not suitable for parsing.  In the above XML document, the
 * first <person> has no "id" attribute.  To ignore the first <person>
 * and its descendants, the start-handler of <person> calls
 * xp_ignore(c, 2) so that any elements that has the ascendant of
 * first <person> will be ignored.
 *
 * Once a element that has a lower depth than the ignore value, the
 * ignore value will be reset to -1 so that the start-handler of
 * <person> will be called for the second <person>.
 */
unsigned xp_ignore(XMLCONTEXT *context, int lev);

/*
 * If you call xp_grab_string() inside of your start handler,
 * you'll get the text string when your end handler is called.
 *
 * For example, consider the XML stream "<title>hello</title>".  If
 * you call this function with nonzero value in GRAB, you'll get
 * "hello" as a third parameter value in your end handler.
 * Otherwise NULL is returned for the third argument in the
 * end handler.
 */
void xp_grab_string(XMLCONTEXT *context, int grab);


int xp_qnamecmp(const char *spec1, const char *spec2);

/*
 * Find the attribute value from the attribute vector.
 *
 * For the attribute with a namespace, pass a form
 * "namespace-name|unprefixed-name" in NAME.
 * (e.g. "http://ecommerce.example.org/schema|taxClass".)
 *
 * Note that `namespace-name' is NOT a namespace prefix!
 */
const char *xp_find_attr(const char *attrs[], const char *name);


/*
 * Set the private data for the the top element of the stack.
 *
 * In the start/end handler, the top element of the stack is the
 * current element.
 *
 * The memory region pointed by DATA (in SIZE bytes) will be copied
 * into the private data area, and automatically released when the
 * element is popped from the element stack.
 *
 * You can retrive the pointer to the private data by calling
 * xp_get_stack_data().
 */
int xp_set_stack_data(XMLCONTEXT *context, void *data, size_t size);

/*
 * Convinient wrapper for xp_set_stack_data().
 */
int xp_set_stack_data_in_string(XMLCONTEXT *context, const char *s);


/*
 * Get the pointer to the private data
 *
 * Note that you need element-spec string to retrive the user-defined
 * value from the specified element.
 *
 * Note that when the private data is inserted by xp_set_stack_data(),
 * there is no way to retrive the SIZE information in
 * xp_set_stack_data() again.
 */
void *xp_get_stack_data(XMLCONTEXT *context, const char *espec);

END_C_DECLS

#endif  /* XMLPARSE_H__ */
