/* $OpenBSD: modulus.h,v 1.5 2002/06/09 08:13:08 todd Exp $ */
/*
 * Copyright 1997-2000 Niels Provos <provos@citi.umich.edu>
 * All rights reserved.
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
 *      This product includes software developed by Niels Provos.
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
 * modulus.h:
 * modulus handling functions
 */

#ifndef _MODULUS_H_
#define _MODULUS_H_
#include <sys/queue.h>

/* Possible values for the status field */

#define MOD_UNUSED     0
#define MOD_COMPUTING  1
#define MOD_PRIME      2
#define MOD_NOTPRIME   3

#define MOD_PRIME_ITER 5                    /* Do each cycle */
#define MOD_PRIME_MAX  20                   /* > => Is prime */
#define MOD_PRIME_TIME 4                    /* max time in mod_check_prime */

#define MOD_TIMEOUT    120

struct moduli_cache {
     TAILQ_ENTRY(moduli_cache) next;	/* Link to next member */

     BIGNUM *modulus;			/* Modulus for computation */
     BIGNUM *generator;			/* Used generator */
     BIGNUM *private_value;		/* Our own private value */
     u_int8_t *exchangevalue;		/* Our own exchange value */
     size_t exchangesize;
     int iterations;			/* primality check iterations */
     int status;			/* Status of the modulus */
     time_t lifetime;			/* For modulus + exchange value */
};

/* Prototypes */
void mod_init(void);

int mod_insert(struct moduli_cache *ob);
int mod_unlink(struct moduli_cache *ob);

struct moduli_cache *mod_new_modgen(BIGNUM *, BIGNUM *);
struct moduli_cache *mod_new_modulus(BIGNUM *);

int mod_value_reset(struct moduli_cache *ob);

struct moduli_cache *mod_find_modgen(BIGNUM *, BIGNUM *);
struct moduli_cache *mod_find_modgen_next(struct moduli_cache *, BIGNUM *,
					  BIGNUM *);
struct moduli_cache *mod_find_modulus(BIGNUM *);
struct moduli_cache *mod_find_generator(BIGNUM *);
struct moduli_cache *mod_find_modulus_next(struct moduli_cache *, BIGNUM *);
struct moduli_cache *mod_find_generator_next(struct moduli_cache *, BIGNUM *);

void mod_check_prime(int iter, int tm);

void mod_cleanup(void);


#endif
