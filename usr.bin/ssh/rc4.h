/*

Alleged RC4 (based on the Usenet posting in Spring-95)

*/

/* RCSID("$Id: rc4.h,v 1.1 1999/09/26 20:53:37 deraadt Exp $"); */

#ifndef RC4_H
#define RC4_H

typedef struct
{
   unsigned int x;
   unsigned int y;
   unsigned char state[256];
} RC4Context;

/* Initializes the context and sets the key. */
void rc4_init(RC4Context *ctx, const unsigned char *key, unsigned int keylen);

/* Returns the next pseudo-random byte from the RC4 (pseudo-random generator)
   stream. */
unsigned int rc4_byte(RC4Context *ctx);

/* Encrypts data. */
void rc4_encrypt(RC4Context *ctx, unsigned char *dest, 
		 const unsigned char *src, unsigned int len);

/* Decrypts data. */
void rc4_decrypt(RC4Context *ctx, unsigned char *dest, 
		 const unsigned char *src, unsigned int len);

#endif /* RC4_H */
