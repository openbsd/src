/*      $OpenBSD: cast.h,v 1.1 2000/02/28 23:13:04 deraadt Exp $       */

/*
 *	CAST-128 in C
 *	Written by Steve Reid <sreid@sea-to-sky.net>
 *	100% Public Domain - no warranty
 *	Released 1997.10.11
 */

#ifndef _CAST_H_
#define _CAST_H_

typedef struct {
	u_int32_t	xkey[32];	/* Key, after expansion */
	int		rounds;		/* Number of rounds to use, 12 or 16 */
} cast_key;

void cast_setkey __P((cast_key * key, u_int8_t * rawkey, int keybytes));
void cast_encrypt __P((cast_key * key, u_int8_t * inblock, u_int8_t * outblock));
void cast_decrypt __P((cast_key * key, u_int8_t * inblock, u_int8_t * outblock));

#endif /* ifndef _CAST_H_ */
