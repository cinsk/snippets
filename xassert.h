/* $Id$ */

/*
 * Enhanced assert(3)
 * Copyright (C) 2003, 2004  Seong-Kook Shin <cinsk.shin@samsung.com>
 */

#ifndef XASSERT_H_
#define XASSERT_H_

#ifdef HAVE_CONFIG_H
# include <config.h>
#endif

/* This indirect writing of extern "C" { ... } makes Emacs happy */
#ifndef BEGIN_C_DECLS
# define BEGIN_C_DECLS  extern "C" {
# define END_C_DECLS    }
#endif /* BEGIN_C_DECLS */

#ifdef __cplusplus
BEGIN_C_DECLS
#endif


#ifndef NDEBUG
extern void assert_(const char *file, long line, const char *func,
                    const char *expr, const char *format, ...);
# define ASSERT(condition, ...) do {                                          \
                                  if (!(condition))                           \
                                    assert_(__FILE__, __LINE__, __func__,     \
                                            #condition, __VA_ARGS__);         \
                                } while (0)

#else
# define ASSERT(condition, ...) ((void)0)
#endif /* NDEBUG */


#ifdef __cplusplus
END_C_DECLS
#endif

#endif /* XASSERT_H_ */
