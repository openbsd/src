/*	$OpenBSD: session.c,v 1.3 2004/05/06 20:29:04 deraadt Exp $	*/

/*
 * Copyright (c) 2000 Network Security Technologies, Inc. http://www.netsec.net
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT,
 * INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT,
 * STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN
 * ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

#include <stdio.h>
#include <sys/types.h>
#include <sys/uio.h>
#include <sys/time.h>
#include <sys/socket.h>
#include <sys/ioctl.h>
#include <net/if.h>
#include <net/if_dl.h>
#include <net/if_types.h>
#include <netinet/in.h>
#include <netinet/if_ether.h>
#include <net/bpf.h>
#include <errno.h>
#include <string.h>
#include <err.h>
#include <fcntl.h>
#include <unistd.h>
#include <sysexits.h>
#include <stdlib.h>

#include "pppoe.h"

struct pppoe_session_master session_master;

void
session_destroy(struct pppoe_session *s)
{
	if (s->s_fd != -1)
		close(s->s_fd);
	LIST_REMOVE(s, s_next);
	free(s);
}

struct pppoe_session *
session_new(struct ether_addr *ea)
{
	struct pppoe_session *s;
	u_int32_t x;
	u_int16_t id = 1;
	int tries = 1000;

	if (session_master.sm_nsessions == PPPOE_MAXSESSIONS)
		return (NULL);

	while (tries--) {
		x = cookie_bake();
		id = ((x >> 16) & 0xffff) ^ (x & 0xffff);
		s = LIST_FIRST(&session_master.sm_sessions);
		while (s) {
			if (memcmp(ea, &s->s_ea, ETHER_ADDR_LEN) == 0 &&
			    s->s_id == id)
				break;
			s = LIST_NEXT(s, s_next);
		}
		if (s == NULL)
			break;
	}
	if (tries == 0)
		return (NULL);

	s = (struct pppoe_session *)malloc(sizeof(*s));
	if (s == NULL)
		return (NULL);

	s->s_id = id;
	s->s_fd = -1;
	s->s_first = 1;
	memcpy(&s->s_ea, ea, ETHER_ADDR_LEN);
	LIST_INSERT_HEAD(&session_master.sm_sessions, s, s_next);

	return (s);
}

struct pppoe_session *
session_find_eaid(struct ether_addr *ea, u_int16_t id)
{
	struct pppoe_session *s;

	s = LIST_FIRST(&session_master.sm_sessions);
	while (s) {
		if (memcmp(ea, &s->s_ea, ETHER_ADDR_LEN) == 0 && s->s_id == id)
			return (s);
		s = LIST_NEXT(s, s_next);
	}
	return (NULL);
}
