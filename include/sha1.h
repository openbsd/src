/*	$OpenBSD: sha1.h,v 1.6 1997/07/10 23:37:49 millert Exp $	*/

/*
 * SHA-1 in C
 * By Steve Reid <steve@edmweb.com>
 * 100% Public Domain
 */

#ifndef _SHA1_H
#define _SHA1_H

typedef struct {
    u_int32_t state[5];
    u_int32_t count[2];  
    u_char buffer[64];
} SHA1_CTX;
  
void SHA1Transform __P((u_int32_t state[5], u_char buffer[64]));
void SHA1Init __P((SHA1_CTX *context));
void SHA1Update __P((SHA1_CTX *context, u_char *data, u_int len));
void SHA1Final __P((u_char digest[20], SHA1_CTX *context));

#endif /* _SHA1_H */
