/*	$OpenBSD: exchange.c,v 1.6 2002/06/09 08:13:08 todd Exp $	*/

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
 * exchange.c:
 *
 */

#ifndef lint
static char rcsid[] = "$OpenBSD: exchange.c,v 1.6 2002/06/09 08:13:08 todd Exp $";
#endif

#define _EXCHANGE_C_

#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <string.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <ssl/bn.h>

#include "config.h"
#include "state.h"
#include "exchange.h"
#include "modulus.h"
#include "attributes.h"
#include "buffer.h"
#include "cookie.h"
#include "schedule.h"
#include "scheme.h"
#include "log.h"

/*
 * Get the number of bits from a variable precision number
 * according to draft-simpson-photuris-11
 */

u_int8_t *
varpre_get_number_bits(size_t *nbits, u_int8_t *varpre)
{
     int blocks;
     size_t bits;

     if (varpre == NULL)
	  return (NULL);

     /* We don't support numbers, that long */
     if (*varpre == 255 && *(varpre+1) == 255)
	     return (NULL);

     bits = 0;
     if (*varpre == 255) {
	  blocks = 3;
	  bits = 65280;
          varpre++;
     } else
	  blocks = 2;

     while (blocks-- > 0) {
	     bits = (bits << 8) + *varpre;
	     varpre++;
     }

     *nbits = bits;

     return (varpre);
}

/*
 * Convert a variable precision number to a bignum
 */

u_int8_t *
BN_varpre2bn(u_int8_t *varpre, size_t size, BIGNUM *a)
{
     u_int8_t *p;
     size_t bytes;

     BN_zero(a);
     p = varpre_get_number_bits(&bytes, varpre);
     if (p == NULL)
	     return (NULL);

     bytes = (bytes + 7) / 8;

     if (p + bytes != varpre + size)
	     return (NULL);

     while (bytes > 0) {
	  BN_lshift(a, a, 8);
	  BN_add_word(a, *p);

	  bytes--;
	  p++;
     }

     return (p);
}

int
BN_bn2varpre(BIGNUM *p, u_int8_t *value, size_t *size)
{
	size_t bits, bytes;
	int header;
	BIGNUM *a;

	bits = BN_num_bits(p);
	bytes = (bits + 7) / 8;

	/* We only support 4 octets */
	if (bits > 65279) {
		bits -= 65280;
		value[0] = 255;
		value[1] = (bits >> 16) & 0xFF;
		value[2] = (bits >>  8) & 0xFF;
		value[3] =  bits        & 0xFF;
		header = 4;
	} else {
		value[0] = (bits >> 8) & 0xFF;
		value[1] =  bits       & 0xFF;
		header = 2;
	}

	/* Check if the buffer is big enough */
	if (bytes + header > (*size - header))
		return (-1);

	a = BN_new();
	BN_copy(a, p);

	*size = bytes + header;

	while (bytes > 0) {
		bytes--;
		value[bytes + header] = BN_mod_word(a, 256);
		BN_rshift(a, a, 8);
	}
	BN_clear_free(a);

	return (0);
}


int
exchange_check_value(BIGNUM *exchange, BIGNUM *gen, BIGNUM *mod)
{
     size_t bits;
     BIGNUM *test;

     bits = BN_num_bits(mod);
     if (BN_num_bits(exchange) < bits/2)
	  return (0);

     test = BN_new();
     BN_copy(test, mod);
     BN_sub_word(test, 1);
     if (!BN_cmp(exchange, test)) {
	  BN_free(test);
	  return (0);
     }

     /* XXX - more tests need to go here */

     BN_free(test);
     return (1);
}

/*
 * Finds to a given modulus and generator cached information
 * which is used to create the private value and exchange value
 */

int
exchange_make_values(struct stateob *st, BIGNUM *modulus, BIGNUM *generator)
{
     struct moduli_cache *p, *tmp;
     u_int8_t *mod;
     time_t tm;

     tm = time(NULL);

     /* See if we have this cached already */
     if ((p = mod_find_modgen(modulus,generator)) == NULL) {
	  /* Create a new modulus, generator pair */
	  if((p = mod_new_modgen(modulus,generator)) == NULL) {
	       BN_clear_free(generator);
	       BN_clear_free(modulus);
	       log_error("Not enough memory in exchange_make_values()");
	       return (-1);
	  }
	  mod_insert(p);
     }
     /* If we don't have a private value calculate a new one */
     if (p->lifetime < tm || BN_is_zero(p->private_value)) {
	  if (p->exchangevalue != NULL)
	       free(p->exchangevalue);

	  /* See if we can find a cached private value */
	  if ((tmp = mod_find_modulus(modulus)) != NULL &&
	      tmp->lifetime > tm && !BN_is_zero(tmp->private_value)) {
	       BN_copy(p->private_value, tmp->private_value);

	       /* Keep exchange value on same (gen,mod) pair */
	       if (!BN_cmp(p->generator, tmp->generator)) {
		    p->exchangevalue = calloc(tmp->exchangesize,sizeof(u_int8_t));
		    if (p->exchangevalue == NULL) {
			 log_error("calloc() in exchange_make_values()");
			 return (-1);
		    }
		    bcopy(tmp->exchangevalue, p->exchangevalue,
			  tmp->exchangesize);
		    p->exchangesize = tmp->exchangesize;
	       } else
		    p->exchangevalue = NULL;
		
	       p->iterations = tmp->iterations;
	       p->status = tmp->status;
	       p->lifetime = tmp->lifetime;
	  } else {
		  size_t bits;

	       /*
		* Make a new private value and change responder secrets
		* as required by draft.
		*/

	       schedule_remove(REKEY, NULL);
	       schedule_insert(REKEY, REKEY_TIMEOUT, NULL, 0);
	       reset_secret();

	       p->lifetime = tm + MOD_TIMEOUT;
	       p->exchangevalue = NULL;

	       /* Find pointer to the VPN containing the modulus */
	       mod = scheme_get_mod(st->scheme);
	       varpre_get_number_bits(&bits, mod);
	       BN_rand(p->private_value, bits, 0, 0);
	  }
	  /* Do we need to generate a new exchange value */
	  if (p->exchangevalue == NULL) {
	       BIGNUM *tmp;
	       BN_CTX *ctx;
	       size_t bits;

	       mod = scheme_get_mod(st->scheme);
	       varpre_get_number_bits(&bits, mod);

	       tmp = BN_new();
	       ctx = BN_CTX_new();
	       BN_mod_exp(tmp, p->generator, p->private_value, p->modulus,
			  ctx);

	       /*
		* If our exchange value is defective we need to make a new one
		* to avoid subgroup confinement.
		*/
	       while (!exchange_check_value(tmp, p->generator, p->modulus)) {
		    BN_rand(p->private_value, bits, 0, 0);
		    BN_mod_exp(tmp, p->generator, p->private_value, p->modulus,
			       ctx);
	       }

	       BN_CTX_free(ctx);

	       p->exchangesize = BUFFER_SIZE;
	       BN_bn2varpre(tmp, buffer, &(p->exchangesize));

	       p->exchangevalue = calloc(p->exchangesize, sizeof(u_int8_t));
	       if (p->exchangevalue == NULL) {
		    log_error("calloc() in exchange_make_value()");
		    BN_clear_free(tmp);
		    return (-1);
	       }
	       bcopy(buffer, p->exchangevalue, p->exchangesize);

	       BN_clear_free(tmp);
	  }
     }

     if (st->exchangevalue != NULL)
	  free(st->exchangevalue);

     st->exchangevalue = calloc(p->exchangesize, sizeof(u_int8_t));
     if (st->exchangevalue == NULL) {
	  log_error("calloc() in exchange_make_values()");
	  return (-1);
     }
     bcopy(p->exchangevalue, st->exchangevalue, p->exchangesize);

     st->exchangesize = p->exchangesize;
     BN_copy(st->modulus, p->modulus);
     BN_copy(st->generator, p->generator);

     return (0);
}

int
exchange_set_generator(BIGNUM *generator, u_int8_t *scheme, u_int8_t *gen)
{
	switch (ntohs(*((u_int16_t *)scheme))) {
	case DH_G_2_MD5:                    /* DH: Generator of 2 */
	case DH_G_2_DES_MD5:                /* DH: Generator of 2 + privacy */
	case DH_G_2_3DES_SHA1:
	     BN_set_word(generator,2);
	     break;
	case DH_G_3_MD5:
	case DH_G_3_DES_MD5:
	case DH_G_3_3DES_SHA1:
	     BN_set_word(generator,3);
	     break;
	case DH_G_5_MD5:
	case DH_G_5_DES_MD5:
	case DH_G_5_3DES_SHA1:
             BN_set_word(generator,5);
	     break;
	default:
	     log_print("Unsupported exchange scheme %d",
		       *((u_int16_t *)scheme));
	     return (-1);
	}
	return (0);
}

/*
 * Generates the exchange values needed for the value_request
 * and value_response packets.
 */

int
exchange_value_generate(struct stateob *st, u_int8_t *value, u_int16_t *size)
{
        BIGNUM *modulus, *generator;
	struct moduli_cache *p;
	u_int8_t *varpre;

	if ((varpre = scheme_get_mod(st->scheme)) == NULL)
	     return (-1);

	generator = BN_new();
	if (exchange_set_generator(generator, st->scheme,
				   scheme_get_gen(st->scheme)) == -1) {
	     BN_clear_free(generator);
	     return (-1);
	}

	modulus = BN_new();
	BN_varpre2bn(varpre, varpre2octets(varpre), modulus);

	if(exchange_make_values(st, modulus, generator) == -1) {
	     BN_clear_free(modulus);
	     BN_clear_free(generator);
	     return (-1);
	}

	p = mod_find_modgen(modulus,generator);
	if (*size < p->exchangesize)
	     return (-1);

	bcopy(p->exchangevalue, value, p->exchangesize);
	BN_clear_free(modulus);
	BN_clear_free(generator);
	
	*size = p->exchangesize;
	return (1);
}
