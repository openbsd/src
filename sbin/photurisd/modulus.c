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
#include "errlog.h"

static struct moduli_cache *modob = NULL;

int
mod_insert(struct moduli_cache *ob)
{
     struct moduli_cache *tmp;

     ob->next = NULL;

     if(modob == NULL) {
	  modob = ob;
	  return 1;
     }
     
     tmp=modob;
     while(tmp->next!=NULL)
	  tmp = tmp->next;

     tmp->next = ob;
     return 1;
}

int
mod_unlink(struct moduli_cache *ob)
{
     struct moduli_cache *tmp;
     if(modob == ob) {
	  modob = ob->next;
	  free(ob);
	  return 1;
     }

     for(tmp=modob; tmp!=NULL; tmp=tmp->next) {
	  if(tmp->next==ob) {
	       tmp->next=ob->next;
	       free(ob);
	       return 1;
	  }
     }
     return 0;
}

/* 
 * Check moduli for primality:
 * check iter iterations, remain at max tm seconds here
 * tm == 0, check all.
 */

void
mod_check_prime(int iter, int tm)
{
     struct moduli_cache *p = modob, *tmp;
     time_t now;
     int flag;
     BN_CTX *ctx;

#ifdef DEBUG 
     char *hex; 
#endif 

     ctx = BN_CTX_new();

     now = time(NULL);
     while (p != NULL && (tm == 0 || (time(NULL) - now < tm))) {
	  if (p->iterations < MOD_PRIME_MAX &&
	      (p->status == MOD_UNUSED || p->status == MOD_COMPUTING)) {
#ifdef DEBUG 
	       hex = BN_bn2hex(p->modulus); 
	       printf(" Checking 0x%s for primality: ", hex);  
	       fflush(stdout); 
	       free(hex); 
#endif 
	       flag = BN_is_prime(p->modulus, iter, NULL, ctx, NULL);
	       if (!flag)
		    log_error(0, "found a non prime in mod_check_prime()");

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
#ifdef DEBUG
	       if (!flag)
		    printf("not prime\n");
	       else if (p->iterations >= MOD_PRIME_MAX)
		    printf("probably prime.\n");
	       else
		    printf("undecided.\n");
#endif
	  }  

	  if (p->status == MOD_NOTPRIME && p->lifetime < now) {
	       struct moduli_cache *tmp;
#ifdef DEBUG
	       printf("Unlinking non prime modulus.\n");
#endif
	       tmp = p;
	       p = p->next;
	       mod_value_reset(tmp);
	       mod_unlink(tmp);
	  }
	  p = p->next;
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

     return tmp;
}

int
mod_value_reset(struct moduli_cache *ob)
{ 
     BN_clear_free(ob->private_value);
     BN_clear_free(ob->modulus);
     BN_clear_free(ob->generator);

     if (ob->exchangevalue != NULL)
	  free(ob->exchangevalue);

     return 1;
}

/* Find a proper modulus and generator in the queue.
 * 0 matches everything.
 */

struct moduli_cache *
mod_find_modgen_next(struct moduli_cache *ob, BIGNUM *modulus,
		     BIGNUM *generator)
{
     struct moduli_cache *tmp = ob; 

     if (tmp == NULL)
	  tmp = modob;
     else
	  tmp = tmp->next;

     while(tmp!=NULL) { 
          if((BN_is_zero(generator) ||  
              !BN_cmp(tmp->generator, generator)) && 
             (BN_is_zero(modulus) || !BN_cmp(modulus, tmp->modulus))) 
               return tmp; 
          tmp = tmp->next; 
     } 
     return NULL; 
}

struct moduli_cache *
mod_find_modgen(BIGNUM *modulus, BIGNUM *generator)
{
     return mod_find_modgen_next(NULL, modulus, generator);
}

struct moduli_cache *   
mod_find_generator_next(struct moduli_cache *ob, BIGNUM *generator)
{
     struct moduli_cache *tmp;
     BIGNUM *modulus;  
  
     modulus = BN_new();                     /* Is set to zero by init */  
     tmp = mod_find_modgen_next(ob, modulus, generator);  
     BN_clear_free(modulus);  
  
     return tmp;  
}  

struct moduli_cache *  
mod_find_generator(BIGNUM *generator)  
{ 
     struct moduli_cache *tmp;
     BIGNUM *modulus; 
 
     modulus = BN_new();                     /* Is set to zero by init */ 
     tmp = mod_find_modgen(modulus,generator); 
     BN_clear_free(modulus); 
 
     return tmp; 
} 

struct moduli_cache *  
mod_find_modulus_next(struct moduli_cache *ob, BIGNUM *modulus)  
{ 
     struct moduli_cache *tmp;
     BIGNUM *generator; 
 
     generator = BN_new();                    /* Is set to zero by init */ 
     tmp = mod_find_modgen_next(ob, modulus, generator); 
     BN_clear_free(generator); 
 
     return tmp; 
} 

struct moduli_cache * 
mod_find_modulus(BIGNUM *modulus) 
{
     struct moduli_cache *tmp;
     BIGNUM *generator;

     generator = BN_new();                    /* Is set to zero by init */
     tmp = mod_find_modgen(modulus,generator);
     BN_clear_free(generator);

     return tmp;
}


void
mod_cleanup()
{
     struct moduli_cache *p;
     struct moduli_cache *tmp = modob;
     while(tmp!=NULL) {
	  p = tmp;
	  mod_value_reset(tmp);
	  tmp = tmp->next;
	  free(p);
     }
     modob = NULL;
}

