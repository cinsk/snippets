#include <assert.h>
#include <stdio.h>
#include <stdlib.h>

#include <openssl/evp.h>
#include <openssl/pem.h>
#include <openssl/rsa.h>
#include <openssl/err.h>

#include <arpa/inet.h> /* For htonl() */

#include "rsautil.h"
#include "xobstack.h"


int
evp_seal_full(struct xobs *pool, EVP_PKEY *key, const void *src, size_t size)
{
  EVP_CIPHER_CTX ctx;
  unsigned char *ek = NULL;
  int eklen;
  uint32_t eklen_n;
  unsigned char iv[EVP_MAX_IV_LENGTH];
  unsigned char buffer[4096];
  unsigned char buffer_out[4096 + EVP_MAX_IV_LENGTH];
  const unsigned char *s = (const unsigned char *)src;
  const unsigned char *end = s + size;
  int blksize, len_out;
  int ret = 0;

  assert(xobs_object_size(pool) == 0);

  EVP_CIPHER_CTX_init(&ctx);

  ek = malloc(EVP_PKEY_size(key));
  if (!ek) {
    ret = -1;
    goto err;
  }

  if (!EVP_SealInit(&ctx, EVP_aes_128_cbc(), &ek, &eklen, iv, &key, 1)) {
    ret = -2;
    goto err;
  }
  eklen_n = htonl(eklen);

  xobs_grow(pool, &eklen_n, sizeof(eklen_n));
  xobs_grow(pool, ek, eklen);
  xobs_grow(pool, iv, EVP_CIPHER_iv_length(EVP_aes_128_cbc()));

  for (; s < end; s += sizeof(buffer)) {
    blksize = s + sizeof(buffer) > end ? end - s : sizeof(buffer);
    if (!EVP_SealUpdate(&ctx, buffer_out, &len_out, s, blksize)) {
      ret = -3;
      goto err;
    }
    xobs_grow(pool, buffer_out, len_out);
  }

  if (!EVP_SealFinal(&ctx, buffer_out, &len_out)) {
    ret = -4;
    goto err;
  }

  xobs_grow(pool, buffer_out, len_out);

  free(ek);
  EVP_CIPHER_CTX_cleanup(&ctx);
  return 0;

 err:
  free(ek);
  if (xobs_object_size(pool) > 0)
    xobs_free(pool, xobs_finish(pool));
  EVP_CIPHER_CTX_cleanup(&ctx);
  return ret;
}


int
evp_unseal_full(struct xobs *pool, EVP_PKEY *key, const void *src, size_t size)
{
  EVP_CIPHER_CTX ctx;
  unsigned char *ek = NULL;
  uint32_t eklen_n;
  size_t eklen;
  const unsigned char *s = (const unsigned char *)src;
  const unsigned char *end = s + size;
  unsigned char iv[EVP_MAX_IV_LENGTH];
  int blksize, len_out;
  unsigned char buffer[4096];
  unsigned char buffer_out[4096 + EVP_MAX_IV_LENGTH];
  int ret = 0;

  EVP_CIPHER_CTX_init(&ctx);

  ek = malloc(EVP_PKEY_size(key));
  if (!ek) {
    EVP_CIPHER_CTX_cleanup(&ctx);
    return -1;
  }

  memcpy(&eklen_n, s, sizeof(eklen_n));
  s += sizeof(eklen_n);

  eklen = ntohl(eklen_n);
  if (eklen > EVP_PKEY_size(key)) {
    ret = -5;
    goto err;
  }

  memcpy(ek, s, eklen);
  s += eklen;
  memcpy(iv, s, EVP_CIPHER_iv_length(EVP_aes_128_cbc()));
  s += EVP_CIPHER_iv_length(EVP_aes_128_cbc());

  if (!EVP_OpenInit(&ctx, EVP_aes_128_cbc(), ek, eklen, iv, key)) {
    ret = -2;
    goto err;
  }

  for (; s < end; s += blksize) {
    blksize = s + sizeof(buffer) > end ? end - s : sizeof(buffer);
    if (!EVP_OpenUpdate(&ctx, buffer_out, &len_out, s, blksize)) {
      ret = -3;
      goto err;
    }
    xobs_grow(pool, buffer_out, len_out);
  }

  if (!EVP_OpenFinal(&ctx, buffer_out, &len_out)) {
    ret = -4;
    goto err;
  }
  xobs_grow(pool, buffer_out, len_out);
  free(ek);
  EVP_CIPHER_CTX_cleanup(&ctx);
  return 0;

 err:
  free(ek);
  if (xobs_object_size(pool) > 0)
    xobs_free(pool, xobs_finish(pool));
  EVP_CIPHER_CTX_cleanup(&ctx);
  return ret;

}
