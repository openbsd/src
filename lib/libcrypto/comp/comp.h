/* $OpenBSD: comp.h,v 1.9 2022/01/14 08:21:12 tb Exp $ */

#ifndef HEADER_COMP_H
#define HEADER_COMP_H

#include <openssl/crypto.h>

#ifdef  __cplusplus
extern "C" {
#endif

COMP_CTX *COMP_CTX_new(COMP_METHOD *meth);
void COMP_CTX_free(COMP_CTX *ctx);
int COMP_compress_block(COMP_CTX *ctx, unsigned char *out, int olen,
    unsigned char *in, int ilen);
int COMP_expand_block(COMP_CTX *ctx, unsigned char *out, int olen,
    unsigned char *in, int ilen);
COMP_METHOD *COMP_rle(void );
COMP_METHOD *COMP_zlib(void );
void COMP_zlib_cleanup(void);

#ifdef HEADER_BIO_H
#ifdef ZLIB
BIO_METHOD *BIO_f_zlib(void);
#endif
#endif

void ERR_load_COMP_strings(void);

/* Error codes for the COMP functions. */

/* Function codes. */
#define COMP_F_BIO_ZLIB_FLUSH				 99
#define COMP_F_BIO_ZLIB_NEW				 100
#define COMP_F_BIO_ZLIB_READ				 101
#define COMP_F_BIO_ZLIB_WRITE				 102

/* Reason codes. */
#define COMP_R_ZLIB_DEFLATE_ERROR			 99
#define COMP_R_ZLIB_INFLATE_ERROR			 100
#define COMP_R_ZLIB_NOT_SUPPORTED			 101

#ifdef  __cplusplus
}
#endif
#endif
