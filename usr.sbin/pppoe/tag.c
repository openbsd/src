/*	$OpenBSD: tag.c,v 1.2 2003/06/04 04:46:13 jason Exp $	*/

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

void
tag_init(struct tag_list *l)
{
	LIST_INIT(&l->thelist);
}

void
tag_destroy(struct tag_list *l)
{
	struct tag_node *p;

	while (1) {
		p = LIST_FIRST(&l->thelist);
		if (p == NULL)
			break;
		p->_ref--;
		if (p->_ref == 0 && p->val)
			free(p->val);
		LIST_REMOVE(p, next);
		free(p);
	}
}

struct tag_node *
tag_lookup(struct tag_list *l, u_int16_t type, int idx)
{
	struct tag_node *p;

	p = LIST_FIRST(&l->thelist);
	while (p != NULL) {
		if (p->type == type) {
			if (idx == 0)
				break;
			idx--;
		}
		p = LIST_NEXT(p, next);
	}
	return (p);
}

int
tag_add(struct tag_list *l, u_int16_t type, u_int16_t len, u_int8_t *val)
{
	struct tag_node *p;

	p = (struct tag_node *)malloc(sizeof(*p));
	if (p == NULL)
		return (-1);
	if (len) {
		p->val = (u_int8_t *)malloc(len);
		if (p->val == NULL) {
			free(p);
			return (-1);
		}
		memcpy(p->val, val, len);
	}
	else
		p->val = NULL;
	p->type = type;
	p->len = len;
	p->_ref = 1;
	LIST_INSERT_HEAD(&l->thelist, p, next);
	return (0);
}

int
tag_pkt(struct tag_list *l, u_long pktlen, u_int8_t *pkt)
{
	u_int16_t ttype, tlen;

	while (pktlen != 0) {
		if (pktlen < sizeof(u_int16_t))
			break;
		ttype = pkt[1] | (pkt[0] << 8);
		pkt += sizeof(u_int16_t);
		pktlen -= sizeof(u_int16_t);

		if (pktlen < sizeof(u_int16_t))
			break;
		tlen = pkt[1] | (pkt[0] << 8);
		pkt += sizeof(u_int16_t);
		pktlen -= sizeof(u_int16_t);

		if (pktlen < tlen)
			break;

		if (tag_add(l, ttype, tlen, pkt) < 0)
			return (-1);
		pkt += tlen;
		pktlen -= tlen;
	}

	if (pktlen != 0)
		return (-1);
	return (0);
}

void
tag_show(struct tag_list *l)
{
	struct tag_node *p;
	int i;

	for (p = LIST_FIRST(&l->thelist); p; p = LIST_NEXT(p, next)) {
		printf("\ttag type=0x%04x, length=%d", p->type, p->len);
		for (i = 0; i < p->len; i++)
			printf("%c%02x", (i == 0) ? ' ' : ':', p->val[i]);
		printf("\n");
	}
}

void
tag_hton(struct tag_list *l)
{
	struct tag_node *p;

	for (p = LIST_FIRST(&l->thelist); p; p = LIST_NEXT(p, next)) {
		p->len = htons(p->len);
		p->type = htons(p->type);
	}
}

void
tag_ntoh(struct tag_list *l)
{
	struct tag_node *p;

	for (p = LIST_FIRST(&l->thelist); p; p = LIST_NEXT(p, next)) {
		p->len = htons(p->len);
		p->type = htons(p->type);
	}
}
