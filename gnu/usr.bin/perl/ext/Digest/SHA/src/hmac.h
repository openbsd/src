/*
 * hmac.h: header file for HMAC-SHA-1/224/256/384/512 routines
 *
 * Ref: FIPS PUB 198 The Keyed-Hash Message Authentication Code
 *
 * Copyright (C) 2003-2007 Mark Shelor, All Rights Reserved
 *
 * Version: 5.45
 * Tue Jun 26 02:36:00 MST 2007
 *
 */

#ifndef _INCLUDE_HMAC_H_
#define _INCLUDE_HMAC_H_

#include "sha.h"

typedef struct {
	SHA *ksha;
	SHA *isha;
	SHA *osha;
	unsigned char key[SHA_MAX_BLOCK_BITS/8];
} HMAC;

#define _HMAC_STATE	HMAC *h
#define _HMAC_ALG	int alg
#define _HMAC_DATA	unsigned char *bitstr, unsigned long bitcnt
#define _HMAC_KEY	unsigned char *key, unsigned int keylen

HMAC		*hmacopen 	(_HMAC_ALG, _HMAC_KEY);
unsigned long	 hmacwrite	(_HMAC_DATA, _HMAC_STATE);
void		 hmacfinish	(_HMAC_STATE);
unsigned char	*hmacdigest	(_HMAC_STATE);
char		*hmachex	(_HMAC_STATE);
char		*hmacbase64	(_HMAC_STATE);
int		 hmacclose	(_HMAC_STATE);

#ifndef SHA_PERL_MODULE

unsigned char	*hmac1digest	(_HMAC_DATA, _HMAC_KEY);
char		*hmac1hex	(_HMAC_DATA, _HMAC_KEY);
char		*hmac1base64	(_HMAC_DATA, _HMAC_KEY);
unsigned char	*hmac224digest	(_HMAC_DATA, _HMAC_KEY);
char		*hmac224hex	(_HMAC_DATA, _HMAC_KEY);
char		*hmac224base64	(_HMAC_DATA, _HMAC_KEY);
unsigned char	*hmac256digest	(_HMAC_DATA, _HMAC_KEY);
char		*hmac256hex	(_HMAC_DATA, _HMAC_KEY);
char		*hmac256base64	(_HMAC_DATA, _HMAC_KEY);
unsigned char	*hmac384digest	(_HMAC_DATA, _HMAC_KEY);
char		*hmac384hex	(_HMAC_DATA, _HMAC_KEY);
char		*hmac384base64	(_HMAC_DATA, _HMAC_KEY);
unsigned char	*hmac512digest	(_HMAC_DATA, _HMAC_KEY);
char		*hmac512hex	(_HMAC_DATA, _HMAC_KEY);
char		*hmac512base64	(_HMAC_DATA, _HMAC_KEY);

#endif

#endif	/* _INCLUDE_HMAC_H_ */
