/*
 * Copyright 1997,1998 Niels Provos <provos@physnet.uni-hamburg.de>
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
 * state.c:
 * functions for handling states
 */

#define _STATE_C_

#ifdef DEBUG
#include <stdio.h>
#endif
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <sys/time.h>
#include "photuris.h"
#include "state.h"
#include "schedule.h"
#include "errlog.h"

static struct stateob *stateob = NULL;

int
state_insert(struct stateob *ob)
{
     struct stateob *tmp;

     ob->next = NULL;

     if(stateob == NULL) {
	  stateob = ob;
	  return 1;
     }
     
     tmp=stateob;
     while(tmp->next!=NULL)
	  tmp = tmp->next;

     tmp->next = ob;
     return 1;
}

int
state_unlink(struct stateob *ob)
{
     struct stateob *tmp;
     if(stateob == ob) {
	  stateob = ob->next;
	  free(ob);
	  return 1;
     }

     for(tmp=stateob; tmp!=NULL; tmp=tmp->next) {
	  if(tmp->next==ob) {
	       tmp->next=ob->next;
	       free(ob);
	       return 1;
	  }
     }
     return 0;
}

int 
state_save_verification(struct stateob *st, u_int8_t *buf, u_int16_t len)
{
     if (st->verification == NULL || len > st->versize) {
	  if (st->verification != NULL)
	       free(st->verification);

	  if ((st->verification = calloc(len, sizeof(u_int8_t))) == NULL) {
	       log_error(1, "calloc() in state_save_verification()");
	       return -1;
	  }
     }

     bcopy(buf, st->verification, len);
     st->versize = len;
     return 0;
}


/*
 * Copies configuration flags from one state to the other
 */

void
state_copy_flags(struct stateob *src, struct stateob *dst)
{
     dst->initiator = src->initiator;
     
     if (src->user != NULL)
	  dst->user = strdup(src->user);

     dst->flags = src->flags;
     dst->isrc = src->isrc;
     dst->ismask = src->ismask;
     dst->idst = src->idst;
     dst->idmask = src->idmask;

     strncpy(dst->address, src->address, sizeof(src->address)-1);
     dst->address[sizeof(dst->address)-1] = 0;

     dst->lifetime = src->lifetime;
     dst->exchange_lifetime = src->exchange_lifetime;
     dst->spi_lifetime = src->spi_lifetime;
}

struct stateob *
state_new(void)
{
     struct stateob *p;

     if((p = calloc(1, sizeof(struct stateob)))==NULL)
	  return NULL;

     mpz_init(p->modulus);
     mpz_init(p->generator);
  
     p->exchange_lifetime = exchange_lifetime;
     p->spi_lifetime = spi_lifetime;

     return p;
}

int
state_value_reset(struct stateob *ob)
{ 
     mpz_clear(ob->modulus);
     mpz_clear(ob->generator);

     if (ob->texchange != NULL)
	  free(ob->texchange);
     if (ob->exchangevalue != NULL)
	  free(ob->exchangevalue);

     if (ob->verification != NULL)
	  free(ob->verification);
     if (ob->roschemes != NULL)
	  free(ob->roschemes);
     if (ob->scheme != NULL)
	  free(ob->scheme);
     if (ob->shared != NULL)
	  free(ob->shared);

     if (ob->user != NULL)
	  free(ob->user);

     if (ob->oSPIident != NULL)
	  free(ob->oSPIident);
     if (ob->oSPIattrib != NULL)
	  free(ob->oSPIattrib);
     if (ob->oSPIoattrib != NULL)
	  free(ob->oSPIoattrib);
     if (ob->oSPIsecret != NULL)
	  free(ob->oSPIsecret);
     if (ob->oSPIidentver != NULL)
	  free(ob->oSPIidentver);
     if (ob->oSPIidentchoice != NULL)
	  free(ob->oSPIidentchoice);
     if (ob->oSPIprivacyctx != NULL)
	  free(ob->oSPIprivacyctx);

     if (ob->uSPIident != NULL)
	  free(ob->uSPIident);
     if (ob->uSPIattrib != NULL)
	  free(ob->uSPIattrib);
     if (ob->uSPIoattrib != NULL)
	  free(ob->uSPIoattrib);
     if (ob->uSPIsecret != NULL)
	  free(ob->uSPIsecret);
     if (ob->uSPIidentver != NULL)
	  free(ob->uSPIidentver);
     if (ob->uSPIidentchoice != NULL)
	  free(ob->uSPIidentchoice);
     if (ob->uSPIprivacyctx != NULL)
	  free(ob->uSPIprivacyctx);

     if (ob->packet != NULL)
	  free(ob->packet);

     return 1;
}

/* 
 * find the state ob with matching address
 */

struct stateob *
state_root(void)
{
     return stateob;
}

struct stateob *
state_find(char *address)
{
     struct stateob *tmp = stateob;
     while (tmp != NULL) {
          if (address == NULL || !strcmp(address, tmp->address))
	       return tmp;
	  tmp = tmp->next;
     }
     return NULL;
}

struct stateob * 
state_find_next(struct stateob *prev, char *address) 
{ 
     struct stateob *tmp = prev->next; 
     while(tmp!=NULL) { 
          if(address == NULL || !strcmp(address, tmp->address)) 
               return tmp; 
          tmp = tmp->next; 
     } 
     return NULL; 
} 


struct stateob * 
state_find_cookies(char *address, u_int8_t *icookie, u_int8_t *rcookie) 
{
     struct stateob *tmp;

     tmp = state_find(address);
     while(tmp!=NULL) {
	  if (!bcmp(tmp->icookie, icookie, COOKIE_SIZE) && 
	      (rcookie == NULL || !bcmp(tmp->rcookie, rcookie, COOKIE_SIZE)))
	       return tmp;
	  tmp = state_find_next(tmp, address);
     }

     return NULL;
}

void
state_cleanup()
{
     struct stateob *p;
     struct stateob *tmp = stateob;
     while(tmp!=NULL) {
	  p = tmp;
	  tmp = tmp->next;
	  state_value_reset(p);
	  free(p);
     }
     stateob = NULL;
}

void
state_expire(void)
{
     struct stateob *tmp = stateob, *p;
     time_t tm;

     tm = time(NULL);
     while (tmp != NULL) {
	  if ((tmp->retries < max_retries || tmp->resource) &&
	      (tmp->lifetime == -1 || tmp->lifetime > tm)) {
	       tmp = tmp->next;
	       continue;
	  }
#ifdef DEBUG
	  printf("Expiring state to %s in phase %d\n",
		tmp->address, tmp->phase);
#endif
	  p = tmp;
	  tmp = tmp->next;
	  state_value_reset(p);
	  state_unlink(p);
     }
}
