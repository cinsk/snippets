/* easy interface for GNU iconv module.
 * Copyright (C) 2009  Seong-Kook Shin <cinsky@gmail.com>
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

#ifndef EASYCONV_H__
#define EASYCONV_H__

#include <stddef.h>
#include <iconv.h>

/* This indirect using of extern "C" { ... } makes Emacs happy */
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

/*
 * If EASYCONV_VERBOSE is defined, most easyconv functions print
 * error/warning messages into stderr.
 */
#define EASYCONV_VERBOSE

/*
 * EASYCONV_CHUNK_SIZE is the default size of memory chunk of the DST.
 * If the memory chunk is too small, it will be increased by doubling.
 */
#define EASYCONV_CHUNK_SIZE     256


/*
 * easyconv_str2str(): character set conversion from the source string
 * to another string.
 *
 * easyconv_str2str() convert the character set of bytes in `SRC'
 * buffer with the size `SRC_SIZE' using iconv module `CD'.  The
 * successfully converted characters are stored in `*DST'.  Note that
 * `SRC_SIZE' is the number of bytes in `SRC', not the character count
 * in `SRC'.  If you pass `-1' in `SRC_SIZE', easyconv_str2str() will
 * call strlen() to calculate the actual count.  Thus, if the source
 * string is not UTF-8 related encoding, you should not pass `-1'
 * here.  You should calculate the `SRC_SIZE' by yourself.
 *
 * easyconv_str2str() dynamically allocated the storage for the
 * destination string.  Before calling this function, you should place
 * in `*DST' the address of a buffer `*DST_SIZE' bytes long, allocated
 * with `malloc'.  If this buffer is not enough for the conversion,
 * this function make the buffer bigger using `realloc()', storing the
 * new buffer address back in `*DST' and the increased size back in
 * `*DST_SIZE'.
 *
 * If you set `*LINEPTR' to a null pointer and `*DST_SIZE' to zero,
 * this function will allocates the initial buffer for you by calling
 * `malloc()'.  The default buffer size is controllable via
 * EASYCONV_CHUNK_SIZE.
 *
 * If you set `*LINEPTR' to a null pointer with nonzero `*DST_SIZE',
 * this function will allocate the initial buffer with the size in
 * `*DST_SIZE'.
 *
 * In any case (even if easyconv_str2str reports an error), you must
 * call `free()' with `*DST' if the storage is no longer required.
 *
 * easyconv_str2str returns the size of the successfully converted
 * bytes with cleared `errno'.  Note that the return value is not the
 * same as that of `iconv()'.  The return value of internal `iconv()'
 * will be stored in `*NREVCONV' if `NREVCONV' is not NULL.
 *
 * There are two error conditions; one for that it cannot handle, and
 * one for that easyconv_str2str() can handle.  In the former case,
 * easyconv_str2str() returns (size_t)-1 with the appropriate error in
 * `errno'.  In case of the latter, easyconv_str2str() returns the
 * successfully converted bytes, with the appropriate code in `errno'.
 *
 * easyconv_str2str silently handles EINVAL and EILSEQ of iconv().
 *
 * EINVAL: easyconv_str2str() will truncate any incomplete bytes
 * sequence at the input buffer.  Thus, it is recommended to pass a
 * complete byte sequence in SRC.
 *
 * EILSEQ: easyconv_str2str() will truncate any invalid byte sequence
 * in the input, then returns only the first valid converted byte
 * sequence.
 *
 * For the example using this function, see the main() in <easyconv.c>
 */
extern size_t easyconv_str2str(iconv_t CD, char **DST, size_t *DST_SIZE,
                               const char *SRC, size_t SRC_SIZE,
                               size_t *NREVCONV);

END_C_DECLS

#endif  /* EASYCONV_H__ */
