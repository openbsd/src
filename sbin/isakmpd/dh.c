/*	$OpenBSD: dh.c,v 1.2 1998/11/15 00:43:52 niklas Exp $	*/

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

#include <sys/param.h>

#include "math_group.h"
#include "dh.h"
#include "log.h"

/*
 * Returns the length of our exchange value.
 */

int
dh_getlen (struct group *group)
{
  return group->getlen (group);
}

/*
 * Creates the exchange value we are offering to the other party.
 * Each time this function is called a new value is created, that
 * means the application has to save the exchange value itself,
 * dh_create_exchange should only be called once.
 */ 

void
dh_create_exchange (struct group *group, u_int8_t *buf)
{
  group->setrandom (group, group->c);
  group->operation (group, group->a, group->gen, group->c);
  group->getraw (group, group->a, buf);
}

/*
 * Creates the Diffie-Hellman shared secret in 'secret', where 'exchange'
 * is the exchange value offered by the other party. No length verification
 * is done for the value, the application has to do that.
 */

void
dh_create_shared (struct group *group, u_int8_t *secret, u_int8_t *exchange)
{
  group->setraw (group, group->b, exchange, group->getlen(group));
  group->operation (group, group->a, group->b, group->c);
  group->getraw (group, group->a, secret);
}
