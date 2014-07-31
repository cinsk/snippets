#ifndef RSAUTIL_H__
#define RSAUTIL_H__

#ifndef BEGIN_C_DECLS
# ifdef __cplusplus
#  define BEGIN_C_DECLS extern "C" {
#  define END_C_DECLS   }
# else
#  define BEGIN_C_DECLS
#  define END_C_DECLS
# endif
#endif /* BEGIN_C_DECLS */

#include <stddef.h>
#include <openssl/pem.h>

BEGIN_C_DECLS

struct xobs;

char *key_to_hexstring(const void *key, size_t size);
int hexstring_to_key(void *dst, const char *hexs);

int aes_encrypt(struct xobs *pool, const void *src, size_t srcsz, const void *key, size_t keysz);
int aes_decrypt(struct xobs *pool, const void *src, size_t srcsz, const void *key, size_t keysz);

int evp_seal_full(struct xobs *pool, EVP_PKEY *key, const void *src, size_t size);
int evp_unseal_full(struct xobs *pool, EVP_PKEY *key, const void *src, size_t size);

END_C_DECLS

#endif /* RSAUTIL_H__ */
