/*	$OpenBSD: md5.h,v 1.12 2004/04/28 16:46:02 millert Exp $	*/

/*
 * This code implements the MD5 message-digest algorithm.
 * The algorithm is due to Ron Rivest.  This code was
 * written by Colin Plumb in 1993, no copyright is claimed.
 * This code is in the public domain; do with it what you wish.
 *
 * Equivalent code is available from RSA Data Security, Inc.
 * This code has been tested against that, and is equivalent,
 * except that you don't need to include two pages of legalese
 * with every copy.
 */

#ifndef _MD5_H_
#define _MD5_H_

typedef struct MD5Context {
	u_int32_t buf[4];			/* state */
	u_int32_t bits[2];			/* number of bits, mod 2^64 */
	unsigned char in[64];	/* input buffer */
} MD5_CTX;

#include <sys/cdefs.h>

__BEGIN_DECLS
void	 MD5Init(MD5_CTX *);
void	 MD5Update(MD5_CTX *, const u_int8_t *, size_t)
		__attribute__((__bounded__(__string__,2,3)));
void	 MD5Final(u_int8_t [16], MD5_CTX *)
		__attribute__((__bounded__(__minbytes__,1,16)));
void	 MD5Transform(u_int32_t [4], const u_int8_t [64])
		__attribute__((__bounded__(__minbytes__,1,4)))
		__attribute__((__bounded__(__minbytes__,2,64)));
char	*MD5End(MD5_CTX *, char [33])
		__attribute__((__bounded__(__minbytes__,2,33)));
char	*MD5File(char *, char [33])
		__attribute__((__bounded__(__minbytes__,2,33)));
char	*MD5Data(const u_int8_t *, size_t, char [33])
		__attribute__((__bounded__(__string__,1,2)))
		__attribute__((__bounded__(__minbytes__,3,33)));
__END_DECLS

#endif /* _MD5_H_ */
