/* $OpenBSD: whrlpool.h,v 1.8 2024/06/01 17:56:44 tb Exp $ */

#include <stddef.h>

#ifndef HEADER_WHRLPOOL_H
#define HEADER_WHRLPOOL_H

#if !defined(HAVE_ATTRIBUTE__BOUNDED__) && !defined(__OpenBSD__)
#define __bounded__(x, y, z)
#endif

#include <openssl/opensslconf.h>

#ifdef __cplusplus
extern "C" {
#endif

#define WHIRLPOOL_DIGEST_LENGTH	(512/8)
#define WHIRLPOOL_BBLOCK	512
#define WHIRLPOOL_COUNTER	(256/8)

typedef struct	{
	union	{
		unsigned char	c[WHIRLPOOL_DIGEST_LENGTH];
		/* double q is here to ensure 64-bit alignment */
		double		q[WHIRLPOOL_DIGEST_LENGTH/sizeof(double)];
		}	H;
	unsigned char	data[WHIRLPOOL_BBLOCK/8];
	unsigned int	bitoff;
	size_t		bitlen[WHIRLPOOL_COUNTER/sizeof(size_t)];
	} WHIRLPOOL_CTX;

#ifndef OPENSSL_NO_WHIRLPOOL
int WHIRLPOOL_Init	(WHIRLPOOL_CTX *c);
int WHIRLPOOL_Update	(WHIRLPOOL_CTX *c,const void *inp,size_t bytes)
    __attribute__ ((__bounded__(__buffer__, 2, 3)));
void WHIRLPOOL_BitUpdate(WHIRLPOOL_CTX *c,const void *inp,size_t bits);
int WHIRLPOOL_Final	(unsigned char *md,WHIRLPOOL_CTX *c);
unsigned char *WHIRLPOOL(const void *inp,size_t bytes,unsigned char *md)
    __attribute__ ((__bounded__(__buffer__, 1, 2)))
    __attribute__ ((__nonnull__(3)));
#endif

#ifdef __cplusplus
}
#endif

#endif
