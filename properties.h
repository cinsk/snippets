#ifndef PROPERTIES_H__
#define PROPERTIES_H__

/*
 * Simple access to Java Properties file
 * Copyright (C) 2013  Seong-Kook Shin <cinsky@gmail.com>
 *
 * Permission is hereby granted, free of charge, to any person obtaining
 * a copy of this software and associated documentation files (the
 * "Software"), to deal in the Software without restriction, including
 * without limitation the rights to use, copy, modify, merge, publish,
 * distribute, sublicense, and/or sell copies of the Software, and to
 * permit persons to whom the Software is furnished to do so, subject to
 * the following conditions:
 *
 * The above copyright notice and this permission notice shall be
 * included in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
 * EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
 * NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE
 * LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION
 * OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION
 * WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 *
 * Except as contained in this notice, the name(s) of the above copyright
 * holders shall not be used in advertising or otherwise to promote the
 * sale, use or other dealings in this Software without prior written
 * authorization.
 */

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

struct properties;
typedef struct properties PROPERTIES;

/*
 * Load properties file from PATHNAME.
 *
 * This function will parse the properties file and returns the structure
 * that containing parsing result.
 *
 * Currently non-null REUSE is not permitted.
 *
 * if REUSE is non-null, this function will reuse the structure that
 * containing the result.  In other words, you can have union of
 * properties by calling this function multiple times.  In this case,
 * except the first call, remaining calls should use the return value
 * of the first call as REUSE argument.
 */
extern PROPERTIES *properties_load(const char *pathname, PROPERTIES *reuse);

/*
 * Release the structure returned by properties_load().
 */
extern void properties_close(PROPERTIES *props);

/*
 * Put (key, value) pair in PROPS.
 *
 * If KEY already exists in PROPS, this function will replace it.
 */
extern void properties_put(PROPERTIES *props, const char *key, const char *value);

/*
 * Get the value of the KEY in PROPS.
 *
 * If KEY does not exist, this function will return NULL.
 */
extern const char *properties_get(PROPERTIES *props, const char *key);

/*
 * Enumerate all (key, value) pair in PROPS.
 *
 * This function will call ITER on every (key, value) pair.  If ITER
 * returns -1, this function will stop the enumeration.  DATA will be
 * passed to ITER.
 *
 * This funtion will return the count of the enumeration.  For
 * example, if all ITER calls returns 0, this function will returns
 * the number of (key, value) pairs.  If every ITER calls returns -1,
 * this function will return zero.
 */
extern int properties_enum(PROPERTIES *props,
                           int (*iter)(const char *key, const char *value,
                                       void *data),
                           void *data);

END_C_DECLS

#endif  /* PROPERTIES_H__ */
