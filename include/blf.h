/* $OpenBSD: blf.h,v 1.1 1997/02/13 16:32:11 provos Exp $ */
/*
 * Blowfish - a fast block cipher designed by Bruce Schneier
 *
 * Copyright 1997 Niels Provos <provos@physnet.uni-hamburg.de>
 *
 * Modification and redistribution in source and binary forms is
 * permitted provided that due credit is given to the author and the
 * OpenBSD project (for instance by leaving this copyright notice
 * intact).
 */

#ifndef _BLF_H_
#define _BLF_H_

/* Schneier states the maximum key length to be 56 bytes.
 * The way how the subkeys are initalized by the key up
 * to (N+2)*4 i.e. 72 bytes are utilized.
 * Warning: For normal blowfish encryption only 56 bytes
 * of the key affect all cipherbits.
 */

#define BLF_N	16			/* Number of Subkeys */
#define BLF_MAXKEYLEN ((BLF_N-2)*4)	/* 448 bits */

/* Blowfish context */
typedef struct BlowfishContext {
	u_int32_t S[4][256];	/* S-Boxes */
	u_int32_t P[BLF_N + 2];	/* Subkeys */
} blf_ctx;

/* Raw access to customized Blowfish
 *	blf_key is just:
 *	Blowfish_initstate( state )
 *	Blowfish_expand0state( state, key, keylen )
 */

void Blowfish_encipher __P((blf_ctx *, u_int32_t *, u_int32_t *));
void Blowfish_decipher __P((blf_ctx *, u_int32_t *, u_int32_t *));
void Blowfish_initstate __P((blf_ctx *));
void Blowfish_expand0state __P((blf_ctx *, u_int8_t *, u_int16_t));
void    Blowfish_expandstate
        __P((blf_ctx *, u_int8_t *, u_int16_t, u_int8_t *, u_int16_t));

/* Standard Blowfish */

void blf_key __P((blf_ctx *, u_int8_t *, u_int16_t));
void blf_enc __P((blf_ctx *, u_int32_t *, u_int16_t));
void blf_dec __P((blf_ctx *, u_int32_t *, u_int16_t));

/* Converts u_int8_t to u_int32_t */
u_int32_t Blowfish_stream2word __P((u_int8_t *, u_int16_t , u_int16_t *));

#endif
