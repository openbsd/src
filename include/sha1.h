/*	$OpenBSD: sha1.h,v 1.8 1997/07/15 01:54:23 millert Exp $	*/

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
  
void SHA1Transform __P((u_int32_t state[5], const u_char buffer[64]));
void SHA1Init __P((SHA1_CTX *context));
void SHA1Update __P((SHA1_CTX *context, const u_char *data, u_int len));
void SHA1Final __P((u_char digest[20], SHA1_CTX *context));
char *SHA1End __P((SHA1_CTX *, char *));
char *SHA1File __P((char *, char *));
char *SHA1Data __P((const u_char *, size_t, char *));

#endif /* _SHA1_H */
