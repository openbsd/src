/*	$OpenBSD: md4.h,v 1.13 2004/04/29 15:51:16 millert Exp $	*/

/*
 * This code implements the MD4 message-digest algorithm.
 * The algorithm is due to Ron Rivest.  This code was
 * written by Colin Plumb in 1993, no copyright is claimed.
 * This code is in the public domain; do with it what you wish.
 * Todd C. Miller modified the MD5 code to do MD4 based on RFC 1186.
 *
 * Equivalent code is available from RSA Data Security, Inc.
 * This code has been tested against that, and is equivalent,
 * except that you don't need to include two pages of legalese
 * with every copy.
 */

#ifndef _MD4_H_
#define _MD4_H_

#define	MD4_BLOCK_LENGTH		64
#define	MD4_DIGEST_LENGTH		16
#define	MD4_DIGEST_STRING_LENGTH	(MD4_DIGEST_LENGTH * 2 + 1)

typedef struct MD4Context {
	u_int32_t state[4];			/* state */
	u_int64_t count;			/* number of bits, mod 2^64 */
	u_int8_t buffer[MD4_BLOCK_LENGTH];	/* input buffer */
} MD4_CTX;

#include <sys/cdefs.h>

__BEGIN_DECLS
void	 MD4Init(MD4_CTX *);
void	 MD4Update(MD4_CTX *, const u_int8_t *, size_t)
		__attribute__((__bounded__(__string__,2,3)));
void	 MD4Final(u_int8_t [MD4_DIGEST_LENGTH], MD4_CTX *)
		__attribute__((__bounded__(__minbytes__,1,MD4_DIGEST_LENGTH)));
void	 MD4Transform(u_int32_t [4], const u_int8_t [MD4_BLOCK_LENGTH])
		__attribute__((__bounded__(__minbytes__,1,4)))
		__attribute__((__bounded__(__minbytes__,2,MD4_BLOCK_LENGTH)));
char	*MD4End(MD4_CTX *, char *)
		__attribute__((__bounded__(__minbytes__,2,MD4_DIGEST_STRING_LENGTH)));
char	*MD4File(char *, char *)
		__attribute__((__bounded__(__minbytes__,2,MD4_DIGEST_STRING_LENGTH)));
char	*MD4Data(const u_int8_t *, size_t, char *)
		__attribute__((__bounded__(__string__,1,2)))
		__attribute__((__bounded__(__minbytes__,3,MD4_DIGEST_STRING_LENGTH)));
__END_DECLS

#endif /* _MD4_H_ */
