/* MD5.H - header file for MD5C.C
 * $OpenBSD: md5.h,v 1.5 2000/03/28 17:35:08 millert Exp $
 */

/* Copyright (C) 1991-2, RSA Data Security, Inc. Created 1991. All
rights reserved.

License to copy and use this software is granted provided that it
is identified as the "RSA Data Security, Inc. MD5 Message-Digest
Algorithm" in all material mentioning or referencing this software
or this function.

License is also granted to make and use derivative works provided
that such works are identified as "derived from the RSA Data
Security, Inc. MD5 Message-Digest Algorithm" in all material
mentioning or referencing the derived work.

RSA Data Security, Inc. makes no representations concerning either
the merchantability of this software or the suitability of this
software for any particular purpose. It is provided "as is"
without express or implied warranty of any kind.

These notices must be retained in any copies of any part of this
documentation and/or software.
 */

#ifndef _MD5_H_
#define _MD5_H_

/* MD5 context. */
typedef struct MD5Context {
    u_int32_t state[4];		/* state (ABCD) */
    u_int64_t count;		/* number of bits, modulo 2^64 */
    unsigned char buffer[64];	/* input buffer */
} MD5_CTX;

void   MD5Init __P((MD5_CTX *));
void   MD5Update __P((MD5_CTX *, const unsigned char *, size_t));
void   MD5Final __P((unsigned char [16], MD5_CTX *));
void   MD5Transform __P ((u_int32_t [4], const unsigned char [64]));
char * MD5End __P((MD5_CTX *, char *));
char * MD5File __P((char *, char *));
char * MD5Data __P((const unsigned char *, size_t, char *));

#endif /* _MD5_H_ */
