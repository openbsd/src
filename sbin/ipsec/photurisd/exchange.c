/*
 * Copyright 1997 Niels Provos <provos@physnet.uni-hamburg.de>
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
static char rcsid[] = "$Id: exchange.c,v 1.3 1998/10/02 16:52:56 niklas Exp $";
#endif

#define _EXCHANGE_C_

#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <string.h>
#include <sys/socket.h> 
#include <netinet/in.h> 
#include <arpa/inet.h> 
#include "config.h"
#include "state.h"
#include "gmp.h"
#include "exchange.h"
#include "modulus.h"
#include "attributes.h"
#include "buffer.h"
#include "cookie.h"
#include "schedule.h"
#include "scheme.h"
#include "errlog.h"

void
make_random_mpz(mpz_t a, mpz_t bits)
{
     mpz_t d;

     mpz_init_set_str(d, "0x100000000", 0);

     /* XXX - we generate too many bits */

     mpz_set_ui(a, 0);
     mpz_cdiv_q_ui(bits,bits,32);               /* We work in 2^32 chucks */

     while(mpz_cmp_ui(bits,0)>0) {
	  mpz_mul(a, a, d);                   /* c = a * 0x100000000 */
	  mpz_add_ui(a, a, arc4random());     /* d = random */
	  mpz_sub_ui(bits, bits, 1);
     }
     mpz_clear(d);
}

/*
 * Get the number of bits from a variable precision number
 * according to draft-simpson-photuris-11
 */
 
u_int8_t *
varpre_get_number_bits(mpz_t bits, u_int8_t *varpre)
{    
     u_int8_t blocks;
     mpz_t a;

     mpz_init_set_ui(a,0);

     mpz_set_ui(bits, 0);
     if (varpre == NULL)
	  return NULL;

     if(*varpre == 255 && *(varpre+1) == 255) {
	  blocks = 6;
	  varpre += 2;
	  mpz_set_ui(bits, 16776960);
     } else if(*varpre == 255) { 
	  blocks = 3;
	  mpz_set_ui(bits, 65280);
          varpre++; 
     } else 
	  blocks = 2;

     while(blocks-->0) {
	  mpz_mul_ui(a,a,256);
	  mpz_add_ui(a,a,*varpre);
	  varpre++;
     }
     mpz_add(bits,a,bits);                    /* Add the above bits */
     mpz_clear(a);
     return varpre;
}

/*
 * Convert a variable precision number to a mpz number
 */

u_int8_t *
mpz_set_varpre(mpz_t a, u_int8_t *varpre)
{
     u_int8_t *p;
     mpz_t bytes;

     mpz_init(bytes);
     mpz_set_ui(a, 0);
     p = varpre_get_number_bits(bytes, varpre);
     mpz_cdiv_q_ui(bytes,bytes,8);                     /* Number of bytes */
     while(mpz_cmp_ui(bytes,0)) {
	  mpz_mul_ui(a, a, 256);
	  mpz_sub_ui(bytes, bytes, 1);
	  mpz_add_ui(a, a, *p);
	  p++;
     }
     mpz_clear(bytes);
     
     return p;
}

u_int8_t *
mpz_init_set_varpre(mpz_t a, u_int8_t *varpre)
{
     mpz_init(a);
     return mpz_set_varpre(a,varpre);
}

void
mpz_get_number_bits(mpz_t rop, mpz_t p)
{
     size_t bits;
     
     bits = mpz_sizeinbase(p, 2);
     mpz_set_ui(rop, bits); 
}

int
mpz_to_varpre(u_int8_t *value, u_int16_t *size, mpz_t p, mpz_t gbits)
{
     u_int16_t header;
     mpz_t a, tmp, bits, bytes;
     u_int32_t count;

     mpz_init(bytes);
     mpz_init(tmp);
     mpz_init_set(bits, gbits);

     mpz_cdiv_q_ui(bytes, bits, 8);

     count = mpz_get_ui(bytes);

     /* XXX - only support 4 octets at the moment */
     if(mpz_cmp_ui(bits, 65279) > 0) {
	  mpz_sub_ui(bits,bits,65280);
	  value[0] = 255;
	  value[3] = mpz_fdiv_qr_ui(bits,tmp,bits,256) & 0xFF;
	  value[2] = mpz_fdiv_qr_ui(bits,tmp,bits,256) & 0xFF;
	  value[1] = mpz_fdiv_qr_ui(bits,tmp,bits,256) & 0xFF;
	  header = 4;
     } else {
	  value[1] = mpz_fdiv_qr_ui(bits,tmp,bits,256) & 0xFF;
	  value[0] = mpz_fdiv_qr_ui(bits,tmp,bits,256) & 0xFF;
	  header = 2;
     }

     if(mpz_cmp_ui(bytes, *size-header)>0)
	  return -1;       /* Not enough buffer */

     mpz_init_set(a, p);

     /* XXX - int16 vs. int32 */
     *size = count+header;

     while(count>0) {
	  count--;
	  value[count+header]=mpz_fdiv_qr_ui(a, tmp, a, 256);
     }
     mpz_clear(a);
     mpz_clear(tmp);
     mpz_clear(bits);
     mpz_clear(bytes);

     return 0;
}


int
exchange_check_value(mpz_t exchange, mpz_t gen, mpz_t mod)
{
     size_t bits;
     mpz_t test;
     
     bits = mpz_sizeinbase(mod, 2);
     if (mpz_sizeinbase(exchange, 2) < bits/2)
	  return 0;

     mpz_init(test);
     mpz_sub_ui(test, mod, 1);
     if (!mpz_cmp(exchange,test)) {
	  mpz_clear(test);
	  return 0;
     }
     mpz_set_ui(test, 1);
     if (!mpz_cmp(exchange,test)) {
	  mpz_clear(test);
	  return 0;
     }

     /* XXX - more tests need to go here */

     mpz_clear(test);
     return 1;
}

/* 
 * Finds to a given modulus and generator cached information
 * which is used to create the private value and exchange value
 */

int
exchange_make_values(struct stateob *st, mpz_t modulus, mpz_t generator)
{
     struct moduli_cache *p, *tmp;
     u_int8_t *mod;
     time_t tm;

     tm = time(NULL);

     /* See if we have this cached already */
     if((p = mod_find_modgen(modulus,generator)) == NULL) {
	  /* Create a new modulus, generator pair */
	  if((p = mod_new_modgen(modulus,generator)) == NULL) {
	       mpz_clear(generator);
	       mpz_clear(modulus);
	       log_error(1, "Not enough memory in exchange_make_values()");
	       return -1;
	  }
	  mod_insert(p);
     }
     /* If we don't have a private value calculate a new one */
     if(p->lifetime < tm || !mpz_cmp_ui(p->private_value,0)) {
	  if (p->exchangevalue != NULL)
	       free(p->exchangevalue);

	  /* See if we can find a cached private value */
	  if((tmp = mod_find_modulus(modulus)) != NULL &&
	     tmp->lifetime > tm && mpz_cmp_ui(tmp->private_value,0)) {
	       mpz_set(p->private_value, tmp->private_value);


	       /* Keep exchange value on same (gen,mod) pair */
	       if (!mpz_cmp(p->generator, tmp->generator)) {
		    p->exchangevalue = calloc(tmp->exchangesize,sizeof(u_int8_t));
		    if (p->exchangevalue == NULL) {
			 log_error(1, "calloc() in exchange_make_values()");
			 return -1;
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
	       mpz_t bits;

	       /* 
		* Make a new private value and change responder secrets
		* as required by draft.
		*/

	       schedule_remove(REKEY, NULL);
	       schedule_insert(REKEY, REKEY_TIMEOUT, NULL, 0);
	       reset_secret();

	       mpz_init(bits);
	       
	       p->lifetime = tm + MOD_TIMEOUT;
	       p->exchangevalue = NULL;

	       /* Find pointer to the VPN containing the modulus */
	       mod = scheme_get_mod(st->scheme);
	       varpre_get_number_bits(bits, mod);
	       make_random_mpz(p->private_value, bits);
	       mpz_clear(bits);
	  }
	  /* Do we need to generate a new exchange value */
	  if (p->exchangevalue == NULL) {
	       mpz_t tmp, bits;

	       mpz_init(bits);
	       mod = scheme_get_mod(st->scheme);
	       varpre_get_number_bits(bits, mod);

	       mpz_init(tmp);

	       mpz_powm(tmp, p->generator, p->private_value, p->modulus);

	       /* 
		* If our exchange value is defective we need to make a new one
		* to avoid subgroup confinement.
		*/
	       while (!exchange_check_value(tmp, p->generator, p->modulus)) {
		    make_random_mpz(p->private_value, bits);
		    mpz_powm(tmp, p->generator, p->private_value, p->modulus);
	       }


	       p->exchangesize = BUFFER_SIZE;
	       mpz_to_varpre(buffer, &(p->exchangesize), tmp, bits);

	       p->exchangevalue = calloc(p->exchangesize, sizeof(u_int8_t));
	       if (p->exchangevalue == NULL) {
		    log_error(1, "calloc() in exchange_make_value()");
		    mpz_clear(bits); mpz_clear(tmp);
		    return -1;
	       }
	       bcopy(buffer, p->exchangevalue, p->exchangesize);

	       mpz_clear(bits);
	       mpz_clear(tmp);
	  }
     }
     if (st->exchangevalue != NULL)
	  free(st->exchangevalue);
     st->exchangevalue = calloc(p->exchangesize, sizeof(u_int8_t));
     if (st->exchangevalue == NULL) {
	  log_error(1, "calloc() in exchange_make_values()");
	  return -1;
     }
     bcopy(p->exchangevalue, st->exchangevalue, p->exchangesize);
     st->exchangesize = p->exchangesize;
     mpz_set(st->modulus, p->modulus);
     mpz_set(st->generator, p->generator);
     return 0;
}

int
exchange_set_generator(mpz_t generator, u_int8_t *scheme, u_int8_t *gen)
{
	switch (ntohs(*((u_int16_t *)scheme))) {
	case DH_G_2_MD5:                    /* DH: Generator of 2 */
	case DH_G_2_DES_MD5:                /* DH: Generator of 2 + privacy */
	case DH_G_2_3DES_SHA1:
	     mpz_set_ui(generator,2);
	     break;
	case DH_G_3_MD5:
	case DH_G_3_DES_MD5:
	case DH_G_3_3DES_SHA1:
	     mpz_set_ui(generator,3);
	     break;
	case DH_G_5_MD5:
	case DH_G_5_DES_MD5:
	case DH_G_5_3DES_SHA1:
             mpz_set_ui(generator,5); 
	     break;
	default:
	     log_error(0, "Unsupported exchange scheme %d",
		       *((u_int16_t *)scheme)); 
	     return -1;
	}
	return 0;
}

/* 
 * Generates the exchange values needed for the value_request
 * and value_response packets.
 */

int
exchange_value_generate(struct stateob *st, u_int8_t *value, u_int16_t *size)
{
        mpz_t modulus,generator;
	struct moduli_cache *p;
	u_int8_t *varpre;

	if ((varpre = scheme_get_mod(st->scheme)) == NULL)
	     return -1;

	mpz_init(generator);
	if (exchange_set_generator(generator, st->scheme,
				   scheme_get_gen(st->scheme)) == -1) {
	     mpz_clear(generator);
	     return -1;
	}

	mpz_init_set_varpre(modulus, varpre);

	if(exchange_make_values(st, modulus, generator) == -1) {
	     mpz_clear(modulus);
	     mpz_clear(generator);
	     return -1;
	}

	p = mod_find_modgen(modulus,generator);
	if (*size < p->exchangesize)
	     return -1;

	bcopy(p->exchangevalue, value, p->exchangesize);
	mpz_clear(modulus);
	mpz_clear(generator);
	
	*size = p->exchangesize;
	return 1;
}
