/*	$OpenBSD: sha1.h,v 1.16 2004/01/22 21:48:02 espie Exp $	*/

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
    unsigned char buffer[64];
} SHA1_CTX;

#include <sys/cdefs.h>

__BEGIN_DECLS
void SHA1Transform(u_int32_t [5], const unsigned char [64])
		__attribute__((__bounded__(__minbytes__,1,5)))
		__attribute__((__bounded__(__minbytes__,2,64)));
void SHA1Init(SHA1_CTX *);
void SHA1Update(SHA1_CTX *, const unsigned char *, unsigned int)
		__attribute__((__bounded__(__string__,2,3)));
void SHA1Final(unsigned char [20], SHA1_CTX *)
		__attribute__((__bounded__(__minbytes__,1,20)));
char *SHA1End(SHA1_CTX *, char *)
		__attribute__((__bounded__(__minbytes__,2,41)));
char *SHA1File(char *, char *)
		__attribute__((__bounded__(__minbytes__,2,41)));
char *SHA1Data(const unsigned char *, size_t, char *)
		__attribute__((__bounded__(__string__,1,2)))
		__attribute__((__bounded__(__minbytes__,3,41)));
__END_DECLS

#define SHA1_DIGESTSIZE       20
#define SHA1_BLOCKSIZE        64
#define HTONDIGEST(x) do {                                              \
        x[0] = htonl(x[0]);                                             \
        x[1] = htonl(x[1]);                                             \
        x[2] = htonl(x[2]);                                             \
        x[3] = htonl(x[3]);                                             \
        x[4] = htonl(x[4]); } while (0)

#define NTOHDIGEST(x) do {                                              \
        x[0] = ntohl(x[0]);                                             \
        x[1] = ntohl(x[1]);                                             \
        x[2] = ntohl(x[2]);                                             \
        x[3] = ntohl(x[3]);                                             \
        x[4] = ntohl(x[4]); } while (0)

#endif /* _SHA1_H */
