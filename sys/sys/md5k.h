/*	$OpenBSD: md5k.h,v 1.6 2002/03/14 01:27:14 millert Exp $	*/

/*
 * Copyright (C) 1991-2, RSA Data Security, Inc. Created 1991. All
 * rights reserved.
 * 
 * License to copy and use this software is granted provided that it
 * is identified as the "RSA Data Security, Inc. MD5 Message-Digest
 * Algorithm" in all material mentioning or referencing this software
 * or this function.
 * 
 * License is also granted to make and use derivative works provided
 * that such works are identified as "derived from the RSA Data
 * Security, Inc. MD5 Message-Digest Algorithm" in all material
 * mentioning or referencing the derived work.
 * 
 * RSA Data Security, Inc. makes no representations concerning either
 * the merchantability of this software or the suitability of this
 * software for any particular purpose. It is provided "as is"
 * without express or implied warranty of any kind.
 * 
 * These notices must be retained in any copies of any part of this
 * documentation and/or software.
 */

#ifndef _MD5K_H_
#define _MD5K_H_

/* POINTER defines a generic pointer type */
typedef void *POINTER;

/* UINT2 defines a two byte word */
typedef u_int16_t UINT2;

/* UINT4 defines a four byte word */
typedef u_int32_t UINT4;

/* MD5 context. */
typedef struct {
	UINT4 state[4];		/* state (ABCD) */
	UINT4 count[2];		/* number of bits, modulo 2^64 (lsb first) */
	u_int8_t buffer[64];	/* input buffer */
} MD5_CTX;

#include <sys/cdefs.h>

void MD5Init(MD5_CTX *);
void MD5Update(MD5_CTX *, unsigned char *, unsigned int);
void MD5Final(unsigned char [16], MD5_CTX *);

#define _MD5_H_

#endif /* _MD5K_H_ */
