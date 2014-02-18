#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <errno.h>

#ifdef HAVE_ICONV
# include <iconv.h>
#endif

#define QSE_OK                  0
#define QSE_EMPTY               -1
#define QSE_UNQUOTED            -2
#define QSE_PREMATURE_ESC       -3
#define QSE_INVALID_ESC         -4
#define QSE_PREMATURE_EOS       -5
#define QSE_UNICODE_NOCONV      -6


#define CHARSET_MAX     32

#ifdef HAVE_ICONV
static int converter_initialized = 0;
static iconv_t converter = (iconv_t)-1;
#endif

static const char *
locale_charset(void)
{
  static char buf[CHARSET_MAX];
  char *locale;
  char *codeset;

  locale = getenv("LC_ALL");
  if (locale == NULL || locale[0] == '\0') {
    locale = getenv("LC_CTYPE");
    if (locale == NULL || locale[0] == '\0') {
      locale = getenv("LANG");
    }
  }

  if (locale != NULL || locale[0] != '\0') {
    const char *dot = strchr(locale, '.');

    if (dot != NULL) {
      const char *modifier;

      dot++;
      modifier = strchr(dot, '@');
      if (modifier == NULL)
        return dot;
      if (modifier - dot < sizeof(buf)) {
        memcpy(buf, dot, modifier - dot);
        buf[modifier - dot] = '\0';
        return buf;
      }
    }
  }
  codeset = locale;

  if (codeset[0] == '\0')
    codeset = "ASCII";

  return codeset;
}


void
release_converter(void)
{
#ifdef HAVE_ICONV
  if (converter != (iconv_t)-1)
    iconv_close(converter);
#endif
}

int
parse_qstring(char **parsed, size_t *len, const char *source)
{
  const char *src;
  int quote;
  int code;
  int i;
  int ret;

  char *dst;
  char *dp;

#ifdef HAVE_ICONV
  char *inbuf = 0;
  size_t inleft = 0;
  char outbuf_[16];
  char *outbuf = outbuf_;
  size_t outleft = sizeof(outbuf_);
  size_t iret;

  if (!converter_initialized) {
    converter = iconv_open("UTF-8", "UTF32-LE");
    if (converter != (iconv_t)-1)
      atexit(release_converter);

    converter_initialized = 1;

    iconv(converter, &inbuf, &inleft, &outbuf, &outleft);
  }
#endif

  if (!source || source[0] == '\0') {
    if (len) *len = -1;
    return QSE_EMPTY;
  }

  if (source[0] != '\'' && source[0] != '"') {
    if (len) *len = -1;
    return QSE_UNQUOTED;
  }

  dst = malloc(strlen(source) + 1);
  dp = dst;

  quote = *source++;
  src = source;

  while (*src) {
    if (*src == '\\') {
      src++;
      switch (*src++) {
      case '\0':
        ret = QSE_PREMATURE_ESC;
        goto err;
      case '"':
      case '\'':
        *dp++ = *(src - 1);
        break;
      case 'b':
        *dp++ = '\b';
        break;
      case 'f':
        *dp++ = '\f';
        break;
      case 'n':
        *dp++ = '\n';
        break;
      case 'r':
        *dp++ = '\r';
        break;
      case 't':
        *dp++ = '\t';
        break;
      case '\\':
        *dp++ = '\\';
        break;
      case '\n':                /* handle \\ at the end of the line */
        break;
      case '0': case '1': case '2': case '3':
      case '4': case '5': case '6': case '7':
        code = 0; i = 0;
        src--;
        while (*src >= '0' && *src < '8' && i < 3) {
          code <<= 3;           /* code = code * 8 */
          code += *src - '0';
          src++;
          i++;
        }
        *dp++ = (unsigned char)code;
        break;

      case 'x':
      case 'X':
        code = 0; i = 0;
        while (i < 2 &&
               ((*src >= '0' && *src <= '9') ||
                (*src >= 'A' && *src <= 'F') ||
                (*src >= 'a' && *src <= 'f'))) {
          code <<= 4;           /* code = code * 16 */
          code += *src - '0';
          src++;
          i++;
        }
        *dp++ = (unsigned char)code;
        break;

#ifdef HAVE_ICONV
      case 'u':                 /* TODO: \uHHHH */
      case 'U':                 /* TODO: \UHHHHHH */
        if (converter_initialized && converter == (iconv_t)-1) {
          ret = QSE_UNICODE_NOCONV;
          goto err;
        }

        code = 0;
        i = 0;

        while (i < 4 && isxdigit((unsigned char)*src)) {
          code <<= 4;
          if (*src >= '0' && *src <= '9')
            code += *src - '0';
          else
            code += toupper((unsigned char)*src) - 'A' + 10;
          src++;
          i++;
        }

        // TODO: call iconv from UTF32 to UTF-8
        inbuf = (char *)&code;
        inleft = sizeof(code);
        outbuf = outbuf_;
        outleft = sizeof(outbuf_);

        iret = iconv(converter, &inbuf, &inleft, &outbuf, &outleft);
        printf("iret = %zu, errno = %d\n", iret, errno);
        break;
#endif  /* HAVE_ICONV */

#if 0
        {
          char esc_char = *src;
          unsigned int uni_value = 0;
          int esc_len;

          for (esc_len = 0, ++src;
               esc_len < 2 && isxdigit((unsigned char)(*src));
               ++esc_len, ++src) {
            code =
          }
#endif  /* 0 */

      default:
        ret = QSE_INVALID_ESC;
        goto err;
      }
    }
    else {                      /* *src != '\\' */
      if (*src == quote) {      /* done! */
        *dp = '\0';
        *parsed = dst;
        return QSE_OK;
      }
      else {
        *dp++ = *src++;
      }
    }
  }
  ret = QSE_PREMATURE_EOS;
  /* error: premature end of quoted string */
 err:
  free(dst);
  return ret;
}


#include <limits.h>

int
main(void)
{
  char buf[LINE_MAX];
  size_t readch;
  char *parsed;
  int ret;
  size_t len;

  printf("charset: %s\n", locale_charset());

  readch = fread(buf, 1, LINE_MAX, stdin);

  printf("read %zu\n", readch);

  buf[readch] = '\0';

  printf("%s", buf);

  ret = parse_qstring(&parsed, &len, buf);
  printf("ret: %d\n", ret);
  printf("parsed: %s\n", parsed);

  return 0;
}
