#include <stdio.h>
#include <string.h>
#include <openssl/err.h>

#include "rsautil.h"
#include "xobstack.h"


int encrypt_mode = 0;

int
main(int argc, char *argv[])
{
  RSA *rsa = NULL;
  FILE *fpkey, *fpdata;
  EVP_PKEY *key = EVP_PKEY_new();
  struct xobs pool;

  char inbuf[10000];
  int readch;

  if (argc != 4) {
    fprintf(stderr, "usage: %s [enc|dec] KEY-FILE DATA-FILE\n", argv[0]);
    return 1;
  }

  if (strcasecmp(argv[1], "enc") == 0)
    encrypt_mode = 1;

  fpkey = fopen(argv[2], "r");
  fpdata = fopen(argv[3], "r");

  readch = fread(inbuf, 1, sizeof(inbuf), fpdata);

  if (encrypt_mode) {
    if (!PEM_read_RSA_PUBKEY(fpkey, &rsa, NULL, NULL)) {
      ERR_print_errors_fp(stderr);
      return 1;
    }
  }
  else {
    if (!PEM_read_RSAPrivateKey(fpkey, &rsa, NULL, NULL)) {
      ERR_print_errors_fp(stderr);
      return 1;
    }
  }

  EVP_PKEY_assign_RSA(key, rsa);

  xobs_init(&pool);

  if (encrypt_mode) {
    evp_seal_full(&pool, key, inbuf, readch);
    fwrite(xobs_base(&pool), 1, xobs_object_size(&pool), stdout);
    fflush(stdout);
    xobs_free(&pool, xobs_finish(&pool));
  }
  else {
    evp_unseal_full(&pool, key, inbuf, readch);
    fwrite(xobs_base(&pool), 1, xobs_object_size(&pool), stdout);
    fflush(stdout);
    xobs_free(&pool, xobs_finish(&pool));
  }

  EVP_PKEY_free(key);

  fclose(fpkey);
  fclose(fpdata);

  xobs_free(&pool, NULL);
  return 0;
}
