/*	$OpenBSD: state.c,v 1.10 2002/06/10 19:58:20 espie Exp $	*/

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
 * state.c:
 * functions for handling states
 */

#include <sys/types.h>
#include <sys/queue.h>

#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <sys/time.h>

#define _STATE_C_

#include "photuris.h"
#include "state.h"
#include "schedule.h"
#include "log.h"

TAILQ_HEAD(statelist, stateob) statehead;

void
state_init(void)
{
	TAILQ_INIT(&statehead);
}

int
state_insert(struct stateob *ob)
{
	TAILQ_INSERT_TAIL(&statehead, ob, next);

	return (1);
}

int
state_unlink(struct stateob *ob)
{
	TAILQ_REMOVE(&statehead, ob, next);

	return (1);
}

int
state_save_verification(struct stateob *st, u_int8_t *buf, u_int16_t len)
{
	if (st->verification == NULL || len > st->versize) {
		if (st->verification != NULL)
			free(st->verification);

		st->verification = calloc(len, sizeof(u_int8_t));
		if (st->verification == NULL) {
			log_error("%s: calloc()", __func__);
			return (-1);
		}
	}

	bcopy(buf, st->verification, len);
	st->versize = len;

	return (0);
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

	if((p = calloc(1, sizeof(struct stateob)))==NULL) {
		log_error("%s: calloc", __func__);
		return (NULL);
	}

	p->modulus = BN_new();
	p->generator = BN_new();

	p->exchange_lifetime = exchange_lifetime;
	p->spi_lifetime = spi_lifetime;

	return (p);
}

int
state_value_reset(struct stateob *ob)
{
     BN_clear_free(ob->modulus);
     BN_clear_free(ob->generator);

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

     return (1);
}

/*
 * find the state ob with matching address
 */

struct stateob *
state_find(char *address)
{
	struct stateob *tmp;

	for (tmp = TAILQ_FIRST(&statehead); tmp; tmp = TAILQ_NEXT(tmp, next)) {
		if (address == NULL || !strcmp(address, tmp->address))
			break;
	}

	return (tmp);
}

struct stateob *
state_find_next(struct stateob *prev, char *address)
{
     struct stateob *tmp;

     for (tmp = TAILQ_NEXT(prev, next); tmp; tmp = TAILQ_NEXT(tmp, next)) {
	     if (address == NULL || !strcmp(address, tmp->address))
		     break;
     }

     return (tmp);
}

struct stateob *
state_find_icookie(u_int8_t *cookie)
{
	struct stateob *tmp;

	for (tmp = TAILQ_FIRST(&statehead); tmp; tmp = TAILQ_NEXT(tmp, next)) {
		if (!bcmp(tmp->icookie, cookie, COOKIE_SIZE))
			break;
	}

	return (tmp);
}

struct stateob *
state_find_cookies(char *address, u_int8_t *icookie, u_int8_t *rcookie)
{
     struct stateob *tmp;


     for (tmp = state_find(address); tmp;
	  tmp = state_find_next(tmp, address)) {
	  if (!bcmp(tmp->icookie, icookie, COOKIE_SIZE) &&
	      (rcookie == NULL || !bcmp(tmp->rcookie, rcookie, COOKIE_SIZE)))
		  break;
     }

     return (tmp);
}

void
state_cleanup(void)
{
     struct stateob *p;

     while ((p = TAILQ_FIRST(&statehead))) {
	     TAILQ_REMOVE(&statehead, p, next);

	     state_value_reset(p);
	     free(p);
     }
}

void
state_expire(void)
{
	struct stateob *tmp, *next;
	time_t tm;

	tm = time(NULL);
	for (tmp = TAILQ_FIRST(&statehead); tmp; tmp = next) {
		next = TAILQ_NEXT(tmp, next);

		if ((tmp->retries < max_retries || tmp->resource) &&
		    (tmp->lifetime == -1 || tmp->lifetime > tm))
			continue;

		LOG_DBG((LOG_MISC, 35, 
			 "%s: Expiring state to %s in phase %d",
			 __func__, tmp->address, tmp->phase));

		state_value_reset(tmp);
		state_unlink(tmp);
	}
}
