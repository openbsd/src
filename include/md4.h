/* MD4.H - header file for MD4C.C
 * $OpenBSD: md4.h,v 1.5 2000/03/28 17:35:07 millert Exp $
 */

/* Copyright (C) 1991-2, RSA Data Security, Inc. Created 1991. All
   rights reserved.

   License to copy and use this software is granted provided that it
   is identified as the "RSA Data Security, Inc. MD4 Message-Digest
   Algorithm" in all material mentioning or referencing this software
   or this function.
   License is also granted to make and use derivative works provided
   that such works are identified as "derived from the RSA Data
   Security, Inc. MD4 Message-Digest Algorithm" in all material
   mentioning or referencing the derived work.

   RSA Data Security, Inc. makes no representations concerning either
   the merchantability of this software or the suitability of this
   software for any particular purpose. It is provided "as is"
   without express or implied warranty of any kind.

   These notices must be retained in any copies of any part of this
   documentation and/or software.
 */

#ifndef _MD4_H_
#define _MD4_H_

/* MD4 context. */
typedef struct MD4Context {
    u_int32_t state[4];		/* state (ABCD) */
    u_int64_t count;		/* number of bits, modulo 2^64 */
    unsigned char buffer[64];	/* input buffer */
} MD4_CTX;

void   MD4Init __P((MD4_CTX *));
void   MD4Update __P((MD4_CTX *, const unsigned char *, size_t));
void   MD4Final __P((unsigned char [16], MD4_CTX *));
void   MD4Transform __P ((u_int32_t [4], const unsigned char [64]));
char * MD4End __P((MD4_CTX *, char *));
char * MD4File __P((char *, char *));
char * MD4Data __P((const unsigned char *, size_t, char *));

#endif /* _MD4_H_ */
