/*      $OpenBSD: cast.c,v 1.1 1998/07/21 22:23:19 provos Exp $       */
/*
 *	CAST-128 in C
 *	Written by Steve Reid <sreid@sea-to-sky.net>
 *	100% Public Domain - no warranty
 *	Released 1997.10.11
 */

#include <netinet/ip_cast.h>
#include <netinet/ip_castsb.h>

/* Macros to access 8-bit bytes out of a 32-bit word */
#define U8a(x) ( (u8) (x>>24) )
#define U8b(x) ( (u8) ((x>>16)&255) )
#define U8c(x) ( (u8) ((x>>8)&255) )
#define U8d(x) ( (u8) ((x)&255) )

/* Circular left shift */
#define ROL(x, n) ( ((x)<<(n)) | ((x)>>(32-(n))) )

/* CAST-128 uses three different round functions */
#define F1(l, r, i) \
	t = ROL(key->xkey[i] + r, key->xkey[i+16]); \
	l ^= ((cast_sbox1[U8a(t)] ^ cast_sbox2[U8b(t)]) - \
	 cast_sbox3[U8c(t)]) + cast_sbox4[U8d(t)];
#define F2(l, r, i) \
	t = ROL(key->xkey[i] ^ r, key->xkey[i+16]); \
	l ^= ((cast_sbox1[U8a(t)] - cast_sbox2[U8b(t)]) + \
	 cast_sbox3[U8c(t)]) ^ cast_sbox4[U8d(t)];
#define F3(l, r, i) \
	t = ROL(key->xkey[i] - r, key->xkey[i+16]); \
	l ^= ((cast_sbox1[U8a(t)] + cast_sbox2[U8b(t)]) ^ \
	 cast_sbox3[U8c(t)]) - cast_sbox4[U8d(t)];


/***** Encryption Function *****/

void cast_encrypt(cast_key* key, u8* inblock, u8* outblock)
{
u32 t, l, r;

	/* Get inblock into l,r */
	l = ((u32)inblock[0] << 24) | ((u32)inblock[1] << 16) |
	 ((u32)inblock[2] << 8) | (u32)inblock[3];
	r = ((u32)inblock[4] << 24) | ((u32)inblock[5] << 16) |
	 ((u32)inblock[6] << 8) | (u32)inblock[7];
	/* Do the work */
	F1(l, r,  0);
	F2(r, l,  1);
	F3(l, r,  2);
	F1(r, l,  3);
	F2(l, r,  4);
	F3(r, l,  5);
	F1(l, r,  6);
	F2(r, l,  7);
	F3(l, r,  8);
	F1(r, l,  9);
	F2(l, r, 10);
	F3(r, l, 11);
	/* Only do full 16 rounds if key length > 80 bits */
	if (key->rounds > 12) {
		F1(l, r, 12);
		F2(r, l, 13);
		F3(l, r, 14);
		F1(r, l, 15);
	}
	/* Put l,r into outblock */
	outblock[0] = U8a(r);
	outblock[1] = U8b(r);
	outblock[2] = U8c(r);
	outblock[3] = U8d(r);
	outblock[4] = U8a(l);
	outblock[5] = U8b(l);
	outblock[6] = U8c(l);
	outblock[7] = U8d(l);
	/* Wipe clean */
	t = l = r = 0;
}


/***** Decryption Function *****/

void cast_decrypt(cast_key* key, u8* inblock, u8* outblock)
{
u32 t, l, r;

	/* Get inblock into l,r */
	r = ((u32)inblock[0] << 24) | ((u32)inblock[1] << 16) |
	 ((u32)inblock[2] << 8) | (u32)inblock[3];
	l = ((u32)inblock[4] << 24) | ((u32)inblock[5] << 16) |
	 ((u32)inblock[6] << 8) | (u32)inblock[7];
	/* Do the work */
	/* Only do full 16 rounds if key length > 80 bits */
	if (key->rounds > 12) {
		F1(r, l, 15);
		F3(l, r, 14);
		F2(r, l, 13);
		F1(l, r, 12);
	}
	F3(r, l, 11);
	F2(l, r, 10);
	F1(r, l,  9);
	F3(l, r,  8);
	F2(r, l,  7);
	F1(l, r,  6);
	F3(r, l,  5);
	F2(l, r,  4);
	F1(r, l,  3);
	F3(l, r,  2);
	F2(r, l,  1);
	F1(l, r,  0);
	/* Put l,r into outblock */
	outblock[0] = U8a(l);
	outblock[1] = U8b(l);
	outblock[2] = U8c(l);
	outblock[3] = U8d(l);
	outblock[4] = U8a(r);
	outblock[5] = U8b(r);
	outblock[6] = U8c(r);
	outblock[7] = U8d(r);
	/* Wipe clean */
	t = l = r = 0;
}


/***** Key Schedual *****/

void cast_setkey(cast_key* key, u8* rawkey, int keybytes)
{
u32 t[4], z[4], x[4];
int i;

	/* Set number of rounds to 12 or 16, depending on key length */
	key->rounds = (keybytes <= 10 ? 12 : 16);

	/* Copy key to workspace x */
	for (i = 0; i < 4; i++) {
		x[i] = 0;
		if ((i*4+0) < keybytes) x[i] = (u32)rawkey[i*4+0] << 24;
		if ((i*4+1) < keybytes) x[i] |= (u32)rawkey[i*4+1] << 16;
		if ((i*4+2) < keybytes) x[i] |= (u32)rawkey[i*4+2] << 8;
		if ((i*4+3) < keybytes) x[i] |= (u32)rawkey[i*4+3];
	}
	/* Generate 32 subkeys, four at a time */
	for (i = 0; i < 32; i+=4) {
		switch (i & 4) {
		 case 0:
			t[0] = z[0] = x[0] ^ cast_sbox5[U8b(x[3])] ^
			 cast_sbox6[U8d(x[3])] ^ cast_sbox7[U8a(x[3])] ^
			 cast_sbox8[U8c(x[3])] ^ cast_sbox7[U8a(x[2])];
			t[1] = z[1] = x[2] ^ cast_sbox5[U8a(z[0])] ^
			 cast_sbox6[U8c(z[0])] ^ cast_sbox7[U8b(z[0])] ^
			 cast_sbox8[U8d(z[0])] ^ cast_sbox8[U8c(x[2])];
			t[2] = z[2] = x[3] ^ cast_sbox5[U8d(z[1])] ^
			 cast_sbox6[U8c(z[1])] ^ cast_sbox7[U8b(z[1])] ^
			 cast_sbox8[U8a(z[1])] ^ cast_sbox5[U8b(x[2])];
			t[3] = z[3] = x[1] ^ cast_sbox5[U8c(z[2])] ^
			 cast_sbox6[U8b(z[2])] ^ cast_sbox7[U8d(z[2])] ^
			 cast_sbox8[U8a(z[2])] ^ cast_sbox6[U8d(x[2])];
			break;
		 case 4:
			t[0] = x[0] = z[2] ^ cast_sbox5[U8b(z[1])] ^
			 cast_sbox6[U8d(z[1])] ^ cast_sbox7[U8a(z[1])] ^
			 cast_sbox8[U8c(z[1])] ^ cast_sbox7[U8a(z[0])];
			t[1] = x[1] = z[0] ^ cast_sbox5[U8a(x[0])] ^
			 cast_sbox6[U8c(x[0])] ^ cast_sbox7[U8b(x[0])] ^
			 cast_sbox8[U8d(x[0])] ^ cast_sbox8[U8c(z[0])];
			t[2] = x[2] = z[1] ^ cast_sbox5[U8d(x[1])] ^
			 cast_sbox6[U8c(x[1])] ^ cast_sbox7[U8b(x[1])] ^
			 cast_sbox8[U8a(x[1])] ^ cast_sbox5[U8b(z[0])];
			t[3] = x[3] = z[3] ^ cast_sbox5[U8c(x[2])] ^
			 cast_sbox6[U8b(x[2])] ^ cast_sbox7[U8d(x[2])] ^
			 cast_sbox8[U8a(x[2])] ^ cast_sbox6[U8d(z[0])];
			break;
		}
		switch (i & 12) {
		 case 0:
		 case 12:
			key->xkey[i+0] = cast_sbox5[U8a(t[2])] ^ cast_sbox6[U8b(t[2])] ^
			 cast_sbox7[U8d(t[1])] ^ cast_sbox8[U8c(t[1])];
			key->xkey[i+1] = cast_sbox5[U8c(t[2])] ^ cast_sbox6[U8d(t[2])] ^
			 cast_sbox7[U8b(t[1])] ^ cast_sbox8[U8a(t[1])];
			key->xkey[i+2] = cast_sbox5[U8a(t[3])] ^ cast_sbox6[U8b(t[3])] ^
			 cast_sbox7[U8d(t[0])] ^ cast_sbox8[U8c(t[0])];
			key->xkey[i+3] = cast_sbox5[U8c(t[3])] ^ cast_sbox6[U8d(t[3])] ^
			 cast_sbox7[U8b(t[0])] ^ cast_sbox8[U8a(t[0])];
			break;
		 case 4:
		 case 8:
			key->xkey[i+0] = cast_sbox5[U8d(t[0])] ^ cast_sbox6[U8c(t[0])] ^
			 cast_sbox7[U8a(t[3])] ^ cast_sbox8[U8b(t[3])];
			key->xkey[i+1] = cast_sbox5[U8b(t[0])] ^ cast_sbox6[U8a(t[0])] ^
			 cast_sbox7[U8c(t[3])] ^ cast_sbox8[U8d(t[3])];
			key->xkey[i+2] = cast_sbox5[U8d(t[1])] ^ cast_sbox6[U8c(t[1])] ^
			 cast_sbox7[U8a(t[2])] ^ cast_sbox8[U8b(t[2])];
			key->xkey[i+3] = cast_sbox5[U8b(t[1])] ^ cast_sbox6[U8a(t[1])] ^
			 cast_sbox7[U8c(t[2])] ^ cast_sbox8[U8d(t[2])];
			break;
		}
		switch (i & 12) {
		 case 0:
			key->xkey[i+0] ^= cast_sbox5[U8c(z[0])];
			key->xkey[i+1] ^= cast_sbox6[U8c(z[1])];
			key->xkey[i+2] ^= cast_sbox7[U8b(z[2])];
			key->xkey[i+3] ^= cast_sbox8[U8a(z[3])];
			break;
		 case 4:
			key->xkey[i+0] ^= cast_sbox5[U8a(x[2])];
			key->xkey[i+1] ^= cast_sbox6[U8b(x[3])];
			key->xkey[i+2] ^= cast_sbox7[U8d(x[0])];
			key->xkey[i+3] ^= cast_sbox8[U8d(x[1])];
			break;
		 case 8:
			key->xkey[i+0] ^= cast_sbox5[U8b(z[2])];
			key->xkey[i+1] ^= cast_sbox6[U8a(z[3])];
			key->xkey[i+2] ^= cast_sbox7[U8c(z[0])];
			key->xkey[i+3] ^= cast_sbox8[U8c(z[1])];
			break;
		 case 12:
			key->xkey[i+0] ^= cast_sbox5[U8d(x[0])];
			key->xkey[i+1] ^= cast_sbox6[U8d(x[1])];
			key->xkey[i+2] ^= cast_sbox7[U8a(x[2])];
			key->xkey[i+3] ^= cast_sbox8[U8b(x[3])];
			break;
		}
		if (i >= 16) {
			key->xkey[i+0] &= 31;
			key->xkey[i+1] &= 31;
			key->xkey[i+2] &= 31;
			key->xkey[i+3] &= 31;
		}
	}
	/* Wipe clean */
	for (i = 0; i < 4; i++) {
		t[i] = x[i] = z[i] = 0;
	}
}

/* Made in Canada */

