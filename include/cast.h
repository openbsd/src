/*      $OpenBSD: cast.h,v 1.1 1998/07/21 22:23:17 provos Exp $       */
/*
 *	CAST-128 in C
 *	Written by Steve Reid <sreid@sea-to-sky.net>
 *	100% Public Domain - no warranty
 *	Released 1997.10.11
 */

#ifndef _CAST_H_
#define _CAST_H_

typedef unsigned char u8;	/* 8-bit unsigned */
typedef unsigned long u32;	/* 32-bit unsigned */

typedef struct {
	u32 xkey[32];	/* Key, after expansion */
	int rounds;		/* Number of rounds to use, 12 or 16 */
} cast_key;

void cast_setkey(cast_key* key, u8* rawkey, int keybytes);
void cast_encrypt(cast_key* key, u8* inblock, u8* outblock);
void cast_decrypt(cast_key* key, u8* inblock, u8* outblock);

#endif /* ifndef _CAST_H_ */

