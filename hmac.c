// compilation: cc hmac.c -lssl -lcrypto -lm

#include <stdlib.h>
#include <math.h>

#include <openssl/hmac.h>

char *HMAC_base64(const void *key, size_t keysize,
                  const void *src, size_t srcsize);

char *HMAC_base64_url(const void *key, size_t keysize,
                      const void *src, size_t srcsize);


static int
base64_encode(char **buf, const void *msg, size_t sz)
{
  BIO *bio, *b64;
  FILE *s;
  int encsize = 4 * ceil((double)sz / 3);

  /* TODO: deal with when SZ is zero */

  *buf = (char *)malloc(encsize + 1);
  if (!*buf)
    return -1;

  s = fmemopen(*buf, encsize + 1, "w");

  b64 = BIO_new(BIO_f_base64());
  bio = BIO_new_fp(s, BIO_NOCLOSE);
  BIO_push(b64, bio);
  BIO_set_flags(b64, BIO_FLAGS_BASE64_NO_NL);
  BIO_write(b64, msg, sz);
  BIO_flush(b64);
  BIO_free_all(b64);

  fclose(s);

  return encsize;
}


/*
 * Create new base64 encoded string using URL-safe alphabet, which
 * substitutes '-' instead of '+' and '_' instead of '.' in the
 * standard Base64 alphabet.  The result can still contain '='.
 */
static int
base64url_encode(char **buf, const void *msg, size_t sz)
{
  int len = base64_encode(buf, msg, sz);
  char *p = *buf;

  for (p = *buf; p < *buf + len + 1; p++) {
    switch (*p) {
    case '+': *p = '-'; break;
    case '/': *p = '_'; break;
    default:  break;
    }
  }

  return len;
}


char *
HMAC_base64(const void *key, size_t keysize, const void *src, size_t srcsize)
{
  unsigned char result[EVP_MAX_MD_SIZE];
  unsigned int dlen;
  char *out = 0;

  if (HMAC(EVP_sha1(), key, keysize, (const unsigned char *)src, srcsize, result, &dlen)) {
    base64_encode(&out, result, dlen);
    return out;
  }
  return NULL;
}


char *
HMAC_base64_url(const void *key, size_t keysize,
                const void *src, size_t srcsize)
{
  unsigned char result[EVP_MAX_MD_SIZE];
  unsigned int dlen;
  char *out = 0;

  if (HMAC(EVP_sha1(), key, keysize,
           (const unsigned char *)src, srcsize, result, &dlen)) {
    base64url_encode(&out, result, dlen);
    return out;
  }
  return NULL;
}



int
main(int argc, char *argv[])
{
  //SSL_library_init();
  int i;
  for (i = 0; i < 100; i++) {
    char *p = HMAC_base64_url("cinsk", 5, "hello world", 11);
    printf("%s\n", p);
    free(p);
  }

  return 0;
}
