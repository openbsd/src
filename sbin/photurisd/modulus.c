/*	$OpenBSD: modulus.c,v 1.7 2002/06/09 08:13:08 todd Exp $	*/

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
 * modulus.c:
 * functions for handling moduli
 */

#define _MODULUS_C_

#ifdef DEBUG
#include <stdio.h>
#endif

#include <stdlib.h>
#include <time.h>
#include <ssl/bn.h>
#include "config.h"
#include "modulus.h"
#include "log.h"

TAILQ_HEAD(modlist, moduli_cache) modhead;

void
mod_init(void)
{
	TAILQ_INIT(&modhead);
}

int
mod_insert(struct moduli_cache *ob)
{
	TAILQ_INSERT_TAIL(&modhead, ob, next);

	return (1);
}

int
mod_unlink(struct moduli_cache *ob)
{
	TAILQ_REMOVE(&modhead, ob, next);
	free(ob);

	return (0);
}

/*
 * Check moduli for primality:
 * check iter iterations, remain at max tm seconds here
 * tm == 0, check all.
 */

void
mod_check_prime(int iter, int tm)
{
	struct moduli_cache *p, *tmp = NULL, *next;
	time_t now;
	int flag;
	BN_CTX *ctx;

	ctx = BN_CTX_new();

	now = time(NULL);
	for (p = TAILQ_FIRST(&modhead);
	     p != NULL && (tm == 0 || (time(NULL) - now < tm)); p = next) {
		next = TAILQ_NEXT(p, next);

		if (p->iterations < MOD_PRIME_MAX &&
		    (p->status == MOD_UNUSED || p->status == MOD_COMPUTING)) {
			flag = BN_is_prime(p->modulus, iter, NULL, ctx, NULL);
			if (!flag)
				log_print(__FUNCTION__": found a non prime");

			tmp = mod_find_modulus(p->modulus);
			while (tmp != NULL) {
				if (!flag) {
					tmp->status = MOD_NOTPRIME;
					tmp->lifetime = now + 2*MOD_TIMEOUT;
				} else {
					tmp->iterations += iter;
					if (tmp->iterations >= MOD_PRIME_MAX)
						tmp->status = MOD_PRIME;
					else
						tmp->status = MOD_COMPUTING;
				}
				tmp = mod_find_modulus_next(tmp, p->modulus);
			}
#ifdef USE_DEBUG
			{
				char *hex, *msg;
				if (!flag)
					msg = "not prime.";
				else if (p->iterations >= MOD_PRIME_MAX)
					msg = "probably prime.";
				else
					msg = "undecided.";
				hex = BN_bn2hex(p->modulus);
				LOG_DBG((LOG_CRYPTO, 50, __FUNCTION__
					 ": check prime: %s: %s",
					 hex, msg));
				free(hex);
			}
#endif
		}

		if (p->status == MOD_NOTPRIME && p->lifetime < now) {
			LOG_DBG((LOG_CRYPTO, 40, __FUNCTION__
				 ": unlinking non prime modulus"));
			mod_value_reset(tmp);
			mod_unlink(tmp);
		}
	}

	BN_CTX_free(ctx);
}

struct moduli_cache *
mod_new_modgen(BIGNUM *m, BIGNUM *g)
{
     struct moduli_cache *p;

     if((p = calloc(1, sizeof(struct moduli_cache)))==NULL)
	  return NULL;

     p->modulus = BN_new(); BN_copy(p->modulus, m);
     p->generator = BN_new(); BN_copy(p->generator, g);
     p->private_value = BN_new();

     /* XXX - change lifetime later */
     p->lifetime = time(NULL) + MOD_TIMEOUT;
     p->status = MOD_UNUSED;

     return p;
}

struct moduli_cache *
mod_new_modulus(BIGNUM *m)
{
     struct moduli_cache *tmp;

     BIGNUM *generator;
     generator = BN_new();
     tmp = mod_new_modgen(m, generator);
     BN_clear_free(generator);

     return (tmp);
}

int
mod_value_reset(struct moduli_cache *ob)
{
     BN_clear_free(ob->private_value);
     BN_clear_free(ob->modulus);
     BN_clear_free(ob->generator);

     if (ob->exchangevalue != NULL)
	  free(ob->exchangevalue);

     return (1);
}

/* Find a proper modulus and generator in the queue.
 * 0 matches everything.
 */

struct moduli_cache *
mod_find_modgen_next(struct moduli_cache *ob, BIGNUM *modulus,
		     BIGNUM *generator)
{
     if (ob != NULL)
	     ob = TAILQ_NEXT(ob, next);
     else
	     ob = TAILQ_FIRST(&modhead);

     for ( ; ob; ob = TAILQ_NEXT(ob, next)) {
          if ((BN_is_zero(generator) ||
	       !BN_cmp(ob->generator, generator)) &&
	      (BN_is_zero(modulus) || !BN_cmp(modulus, ob->modulus)))
		  break;
     }

     return (ob);
}

struct moduli_cache *
mod_find_modgen(BIGNUM *modulus, BIGNUM *generator)
{
     return (mod_find_modgen_next(NULL, modulus, generator));
}

struct moduli_cache *
mod_find_generator_next(struct moduli_cache *ob, BIGNUM *generator)
{
     struct moduli_cache *tmp;
     BIGNUM *modulus;

     modulus = BN_new();
     BN_zero(modulus);

     tmp = mod_find_modgen_next(ob, modulus, generator);

     BN_free(modulus);

     return (tmp);
}

struct moduli_cache *
mod_find_generator(BIGNUM *generator)
{
     struct moduli_cache *tmp;
     BIGNUM *modulus;

     modulus = BN_new();
     BN_zero(modulus);

     tmp = mod_find_modgen(modulus,generator);

     BN_free(modulus);

     return (tmp);
}

struct moduli_cache *
mod_find_modulus_next(struct moduli_cache *ob, BIGNUM *modulus)
{
     struct moduli_cache *tmp;
     BIGNUM *generator;

     generator = BN_new();
     BN_zero(generator);

     tmp = mod_find_modgen_next(ob, modulus, generator);

     BN_free(generator);

     return (tmp);
}

struct moduli_cache *
mod_find_modulus(BIGNUM *modulus)
{
     struct moduli_cache *tmp;
     BIGNUM *generator;

     generator = BN_new();
     BN_zero(generator);

     tmp = mod_find_modgen(modulus,generator);

     BN_free(generator);

     return (tmp);
}


void
mod_cleanup(void)
{
     struct moduli_cache *p;

     while ((p = TAILQ_FIRST(&modhead))) {
	     TAILQ_REMOVE(&modhead, p, next);
	     mod_value_reset(p);
	     free(p);
     }
}

