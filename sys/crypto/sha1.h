/*	$OpenBSD: sha1.h,v 1.3 2002/03/14 01:26:51 millert Exp $	*/

/*
 * SHA-1 in C
 * By Steve Reid <steve@edmweb.com>
 * 100% Public Domain
 */

#ifndef _SHA1_H_
#define _SHA1_H_

typedef struct {
	u_int32_t	state[5];
	u_int32_t	count[2];  
	unsigned char	buffer[64];
} SHA1_CTX;
  
void SHA1Transform(u_int32_t state[5], unsigned char buffer[64]);
void SHA1Init(SHA1_CTX * context);
void SHA1Update(SHA1_CTX * context, unsigned char * data, unsigned int len);
void SHA1Final(unsigned char digest[20], SHA1_CTX * context);

#endif /* _SHA1_H_ */
