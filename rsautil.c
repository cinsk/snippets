#include <assert.h>
#include <stdio.h>
#include <stdlib.h>

#include <openssl/evp.h>
#include <openssl/pem.h>
#include <openssl/rsa.h>
#include <openssl/err.h>
#include <openssl/aes.h>
#include <openssl/rand.h>

#include <arpa/inet.h> /* For htonl() */

#include "rsautil.h"
#include "xobstack.h"


int
hexstring_to_key(void *dst, const char *hexs)
{
  const char *p = hexs;
  char *q = dst;

  if (!hexs)
    return -1;

  for (p = hexs; *p != '\0'; p++) {
    unsigned val = 0;

    val = (*p >= '0' && *p <= '9') ? *p - '0' : *p - 'A' + 10;
    p++;
    val <<= 4;
    val += (*p >= '0' && *p <= '9') ? *p - '0' : *p - 'A' + 10;
    *q++ = val;
  }
  return 0;
}


char *
key_to_hexstring(const void *key, size_t size)
{
  char *p, *q;
  const unsigned char *s = (const unsigned char *)key;
  const unsigned char *end = s + size;

  p = malloc(size * 2 + 1);
  if (!p)
    return NULL;
  q = p;

  while (s < end) {
    char v;
    v = *s / 16;
    *q++ = (v >= 10) ? v - 10 + 'A' : v + '0';
    v = *s % 16;
    *q++ = (v >= 10) ? v - 10 + 'A' : v + '0';
    s++;
  }
  *q = '\0';
  return p;
}


int
aes_encrypt(struct xobs *pool, const void *src, size_t srcsz, const void *key, size_t keysz)
{
  unsigned char iv_enc[AES_BLOCK_SIZE * 2];
  size_t encbufsz;
  AES_KEY enckey;
  uint32_t inputlen;
  unsigned char *base;

  assert(keysz*8 == 128 || keysz*8 == 192 || keysz*8 == 256);

  encbufsz = ((srcsz + AES_BLOCK_SIZE) / AES_BLOCK_SIZE) * AES_BLOCK_SIZE;

  inputlen = htonl(srcsz);
  xobs_int_grow(pool, inputlen);

  RAND_bytes(iv_enc, sizeof(iv_enc));
  xobs_grow(pool, iv_enc, sizeof(iv_enc));

  xobs_blank(pool, encbufsz + 16);

  base = (unsigned char *)xobs_base(pool) + sizeof(inputlen) + sizeof(iv_enc);
  memset(base, 0xcc, encbufsz + 16); /* DEBUG */

  // memcpy(iv_enc, key, AES_BLOCK_SIZE);

  AES_set_encrypt_key(key, keysz * 8, &enckey);
  //AES_cbc_encrypt(src, base, srcsz, &enckey, iv_enc, AES_ENCRYPT);
  AES_cbc_encrypt(src, base, encbufsz, &enckey, iv_enc, AES_ENCRYPT);

  return 0;
}


int
aes_decrypt(struct xobs *pool, const void *src, size_t srcsz, const void *key, size_t keysz)
{
  unsigned char iv_dec[AES_BLOCK_SIZE * 2];
  AES_KEY deckey;
  const unsigned char *s = (const unsigned char *)src;
  uint32_t srclen;
  size_t sz = srcsz;

  assert(keysz * 8 == 128 || keysz * 8 == 192 || keysz * 8 == 256);

  srclen = ntohl(*(const int *)s);
  s += sizeof(uint32_t);
  sz -= sizeof(uint32_t);

  memcpy(iv_dec, s, sizeof(iv_dec));
  s += sizeof(iv_dec);
  sz -= sizeof(iv_dec);

  xobs_blank(pool, srclen);

  // memcpy(iv_dec, key, AES_BLOCK_SIZE);
  AES_set_decrypt_key(key, keysz * 8, &deckey);
  // AES_cbc_encrypt(src, xobs_base(pool), srcsz, &deckey, iv_dec, AES_DECRYPT);
  AES_cbc_encrypt(s, xobs_base(pool), sz, &deckey, iv_dec, AES_DECRYPT);

  return 0;
}


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
