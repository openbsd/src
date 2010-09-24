/*
 * hmac.c: routines to compute HMAC-SHA-1/224/256/384/512 digests
 *
 * Ref: FIPS PUB 198 The Keyed-Hash Message Authentication Code
 *
 * Copyright (C) 2003-2008 Mark Shelor, All Rights Reserved
 *
 * Version: 5.47
 * Wed Apr 30 04:00:54 MST 2008
 *
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "hmac.h"
#include "sha.h"

/* hmacopen: creates a new HMAC-SHA digest object */
HMAC *hmacopen(int alg, unsigned char *key, unsigned int keylen)
{
	unsigned int i;
	HMAC *h;

	SHA_newz(0, h, 1, HMAC);
	if (h == NULL)
		return(NULL);
	if ((h->isha = shaopen(alg)) == NULL) {
		SHA_free(h);
		return(NULL);
	}
	if ((h->osha = shaopen(alg)) == NULL) {
		shaclose(h->isha);
		SHA_free(h);
		return(NULL);
	}
	if (keylen <= h->osha->blocksize / 8)
		memcpy(h->key, key, keylen);
	else {
		if ((h->ksha = shaopen(alg)) == NULL) {
			shaclose(h->isha);
			shaclose(h->osha);
			SHA_free(h);
			return(NULL);
		}
		shawrite(key, keylen * 8, h->ksha);
		shafinish(h->ksha);
		memcpy(h->key, shadigest(h->ksha), h->ksha->digestlen);
		shaclose(h->ksha);
	}
	for (i = 0; i < h->osha->blocksize / 8; i++)
		h->key[i] ^= 0x5c;
	shawrite(h->key, h->osha->blocksize, h->osha);
	for (i = 0; i < h->isha->blocksize / 8; i++)
		h->key[i] ^= (0x5c ^ 0x36);
	shawrite(h->key, h->isha->blocksize, h->isha);
	memset(h->key, 0, sizeof(h->key));
	return(h);
}

/* hmacwrite: triggers a state update using data in bitstr/bitcnt */
unsigned long hmacwrite(unsigned char *bitstr, unsigned long bitcnt, HMAC *h)
{
	return(shawrite(bitstr, bitcnt, h->isha));
}

/* hmacfinish: computes final digest state */
void hmacfinish(HMAC *h)
{
	shafinish(h->isha);
	shawrite(shadigest(h->isha), h->isha->digestlen * 8, h->osha);
	shaclose(h->isha);
	shafinish(h->osha);
}

/* hmacdigest: returns pointer to digest (binary) */
unsigned char *hmacdigest(HMAC *h)
{
	return(shadigest(h->osha));
}

/* hmachex: returns pointer to digest (hexadecimal) */
char *hmachex(HMAC *h)
{
	return(shahex(h->osha));
}

/* hmacbase64: returns pointer to digest (Base 64) */
char *hmacbase64(HMAC *h)
{
	return(shabase64(h->osha));
}

/* hmacclose: de-allocates digest object */
int hmacclose(HMAC *h)
{
	if (h != NULL) {
		shaclose(h->osha);
		memset(h, 0, sizeof(HMAC));
		SHA_free(h);
	}
	return(0);
}
