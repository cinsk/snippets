/*
 * Parse quoted string
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


#ifndef PARSE_QSTR_H__
#define PARSE_QSTR_H__

#ifndef BEGIN_C_DECLS
# ifdef __cplusplus
#  define BEGIN_C_DECLS extern "C" {
#  define END_C_DECLS   }
# else
#  define BEGIN_C_DECLS
#  define END_C_DECLS
# endif
#endif /* BEGIN_C_DECLS */

BEGIN_C_DECLS

#define QSE_OK                  0
#define QSE_EMPTY               -1
#define QSE_UNQUOTED            -2
#define QSE_PREMATURE_ESC       -3
#define QSE_INVALID_ESC         -4
#define QSE_PREMATURE_EOS       -5
#define QSE_UNICODE_NOCONV      -6

extern int parse_qstring(char **parsed, size_t *len, const char *source);

END_C_DECLS

#endif  /* PARSE_QSTR_H__ */
