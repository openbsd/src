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
#include <gmp.h>
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

#ifdef DEBUG 
     char *hex; 
#endif 

     now = time(NULL);
     while(p != NULL && (tm == 0 || (time(NULL) - now < tm))) {
	  if (p->iterations < MOD_PRIME_MAX &&
	      (p->status == MOD_UNUSED || p->status == MOD_COMPUTING)) {
#ifdef DEBUG 
	       hex = mpz_get_str(NULL, 16, p->modulus); 
	       printf(" Checking 0x%s for primality: ", hex);  
	       fflush(stdout); 
	       free(hex); 
#endif 
	       flag = mpz_probab_prime_p(p->modulus, iter);
	       if (!flag)
		    log_error(0, "found a non prime in mod_check_prime()");

	       tmp = mod_find_modulus(p->modulus);
	       while(tmp != NULL) {
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
}

struct moduli_cache *
mod_new_modgen(mpz_t m, mpz_t g)
{
     struct moduli_cache *p;

     if((p = calloc(1, sizeof(struct moduli_cache)))==NULL)
	  return NULL;

     mpz_init_set(p->modulus,m);
     mpz_init_set(p->generator,g);
     mpz_init(p->private_value);

     /* XXX - change lifetime later */
     p->lifetime = time(NULL) + MOD_TIMEOUT;
     p->status = MOD_UNUSED;

     return p;
}

struct moduli_cache * 
mod_new_modulus(mpz_t m) 
{ 
     struct moduli_cache *tmp;

     mpz_t generator;
     mpz_init(generator);
     tmp = mod_new_modgen(m, generator);
     mpz_clear(generator);

     return tmp;
}

int
mod_value_reset(struct moduli_cache *ob)
{ 
     mpz_clear(ob->private_value);
     mpz_clear(ob->modulus);
     mpz_clear(ob->generator);

     if (ob->exchangevalue != NULL)
	  free(ob->exchangevalue);

     return 1;
}

/* Find a proper modulus and generator in the queue.
 * 0 matches everything.
 */

struct moduli_cache *
mod_find_modgen_next(struct moduli_cache *ob, mpz_t modulus, mpz_t generator)
{
     struct moduli_cache *tmp = ob; 

     if (tmp == NULL)
	  tmp = modob;
     else
	  tmp = tmp->next;

     while(tmp!=NULL) { 
          if((!mpz_cmp_ui(generator,0) ||  
              !mpz_cmp(tmp->generator,generator)) && 
             (!mpz_cmp_ui(modulus,0) || !mpz_cmp(modulus,tmp->modulus))) 
               return tmp; 
          tmp = tmp->next; 
     } 
     return NULL; 
}

struct moduli_cache *
mod_find_modgen(mpz_t modulus, mpz_t generator)
{
     return mod_find_modgen_next(NULL, modulus, generator);
}

struct moduli_cache *   
mod_find_generator_next(struct moduli_cache *ob, mpz_t generator)
{
     struct moduli_cache *tmp;
     mpz_t modulus;  
  
     mpz_init(modulus);                     /* Is set to zero by init */  
     tmp = mod_find_modgen_next(ob, modulus, generator);  
     mpz_clear(modulus);  
  
     return tmp;  
}  

struct moduli_cache *  
mod_find_generator(mpz_t generator)  
{ 
     struct moduli_cache *tmp;
     mpz_t modulus; 
 
     mpz_init(modulus);                     /* Is set to zero by init */ 
     tmp = mod_find_modgen(modulus,generator); 
     mpz_clear(modulus); 
 
     return tmp; 
} 

struct moduli_cache *  
mod_find_modulus_next(struct moduli_cache *ob, mpz_t modulus)  
{ 
     struct moduli_cache *tmp;
     mpz_t generator; 
 
     mpz_init(generator);                    /* Is set to zero by init */ 
     tmp = mod_find_modgen_next(ob, modulus, generator); 
     mpz_clear(generator); 
 
     return tmp; 
} 

struct moduli_cache * 
mod_find_modulus(mpz_t modulus) 
{
     struct moduli_cache *tmp;
     mpz_t generator;

     mpz_init(generator);                    /* Is set to zero by init */
     tmp = mod_find_modgen(modulus,generator);
     mpz_clear(generator);

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

