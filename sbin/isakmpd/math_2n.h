/*	$OpenBSD: math_2n.h,v 1.3 1998/11/17 11:10:16 niklas Exp $	*/
/*	$EOM: math_2n.h,v 1.6 1998/07/18 21:09:40 provos Exp $	*/

/*
 * Copyright (c) 1998 Niels Provos.  All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *	This product includes software developed by Ericsson Radio Systems.
 * 4. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

/*
 * This code was written under funding by Ericsson Radio Systems.
 */

#ifndef _MATH_2N_H
#define _MATH_2N_H_

/*
 * The chunk size we use is variable, this allows speed ups
 * for processors like the Alpha with 64bit words.
 * XXX - b2n_mask is only up to 32 bit at the moment.
 */

#define USE_32BIT	/* XXX - This obviously needs fixing */

#ifdef USE_32BIT
#define CHUNK_TYPE	u_int32_t
#define CHUNK_BITS	32
#define CHUNK_SHIFTS	5
#define CHUNK_BMASK	0xffffffff
#define CHUNK_MASK	(CHUNK_BITS - 1)
#define CHUNK_BYTES	(CHUNK_BITS >> 3)
#define CHUNK_NIBBLES	(CHUNK_BITS >> 2)
#else
#define CHUNK_TYPE	u_int8_t
#define CHUNK_BITS	8
#define CHUNK_SHIFTS	3
#define CHUNK_BMASK	0xff
#define CHUNK_MASK	(CHUNK_BITS - 1)
#define CHUNK_BYTES	(CHUNK_BITS >> 3)
#define CHUNK_NIBBLES	(CHUNK_BITS >> 2)
#endif

extern CHUNK_TYPE b2n_mask[CHUNK_BITS];

/* An element of GF(2**n), n = bits */

typedef struct {
  u_int16_t	chunks;
  u_int16_t	bits;
  u_int8_t	dirty;		/* Sig bits are dirty */
  CHUNK_TYPE	*limp;
} _b2n;

typedef _b2n *b2n_ptr;
typedef _b2n b2n_t[1];

#define B2N_SET(x,y) (x)->chunks = (y)->chunks; (x)->bits = (y)->bits; \
    (x)->limp = (y)->limp; (x)->dirty = (y)->dirty;

#define B2N_MIN(x,y) ((x)->chunks > (y)->chunks ? (y) : (x))
#define B2N_MAX(x,y) ((x)->chunks > (y)->chunks ? (x) : (y))

void b2n_init (b2n_ptr);
void b2n_clear (b2n_ptr);
void b2n_resize (b2n_ptr, unsigned int);

void b2n_random (b2n_ptr, u_int32_t);

void b2n_set (b2n_ptr, b2n_ptr);
void b2n_set_null (b2n_ptr);
void b2n_set_ui (b2n_ptr, unsigned int);
void b2n_set_str (b2n_ptr, char *);

void b2n_print (b2n_ptr);
int b2n_sprint (char *, b2n_ptr);

int b2n_cmp (b2n_ptr, b2n_ptr);
int b2n_cmp_null (b2n_ptr);

void b2n_add (b2n_ptr, b2n_ptr, b2n_ptr);
#define b2n_sub b2n_add
void b2n_lshift (b2n_ptr, b2n_ptr, unsigned int);
void b2n_rshift (b2n_ptr, b2n_ptr, unsigned int);
u_int32_t b2n_sigbit (b2n_ptr);

void b2n_nadd (b2n_ptr, b2n_ptr, b2n_ptr);
void b2n_nsub (b2n_ptr, b2n_ptr, b2n_ptr);
void b2n_3mul (b2n_ptr, b2n_ptr);

void b2n_mul (b2n_ptr, b2n_ptr, b2n_ptr);
void b2n_square (b2n_ptr, b2n_ptr);

void b2n_div (b2n_ptr, b2n_ptr, b2n_ptr, b2n_ptr);
void b2n_div_q (b2n_ptr, b2n_ptr, b2n_ptr);
void b2n_div_r (b2n_ptr, b2n_ptr, b2n_ptr);

/* Operation on GF(2**n) */
#define B2N_SWAP(x,y) {b2n_t _t_; B2N_SET(_t_, (x)); \
 B2N_SET ((x), (y)); B2N_SET ((y),_t_); }

void b2n_mod (b2n_ptr, b2n_ptr, b2n_ptr);

void b2n_gcd (b2n_ptr, b2n_ptr, b2n_ptr);
void b2n_mul_inv (b2n_ptr, b2n_ptr, b2n_ptr);
void b2n_div_mod (b2n_ptr, b2n_ptr, b2n_ptr, b2n_ptr);
void b2n_trace (b2n_ptr, b2n_ptr, b2n_ptr);
void b2n_halftrace (b2n_ptr, b2n_ptr, b2n_ptr);
void b2n_sqrt (b2n_ptr, b2n_ptr, b2n_ptr);
void b2n_exp_mod (b2n_ptr, b2n_ptr, u_int32_t, b2n_ptr);

#endif /* _MATH_2N_H_ */
