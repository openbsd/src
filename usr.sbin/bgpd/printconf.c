/*	$OpenBSD: printconf.c,v 1.1 2004/02/08 23:44:57 henning Exp $	*/

/*
 * Copyright (c) 2003, 2004 Henning Brauer <henning@openbsd.org>
 *
 * Permission to use, copy, modify, and distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA, PROFITS OR MIND, WHETHER IN
 * AN ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT
 * OF OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */

#include <stdio.h>

#include "bgpd.h"
#include "session.h"

void		 print_op(enum comp_ops);

void
print_op(enum comp_ops op)
{
	switch (op) {
	case OP_EQ:
		printf("=");
		break;
	case OP_NE:
		printf("!=");
		break;
	case OP_LE:
		printf("<=");
		break;
	case OP_LT:
		printf("<");
		break;
	case OP_GE:
		printf(">=");
		break;
	case OP_GT:
		printf(">");
		break;
	default:
		printf("?");
		break;
	}
}

void
print_rule(struct peer *peer_l, struct filter_rule *r)
{
	struct peer	*p;

	if (r->action == ACTION_ALLOW)
		printf("allow ");
	else if (r->action == ACTION_DENY)
		printf("deny ");
	else
		printf("match ");

	if (r->quick)
		printf("quick ");

	if (r->dir == DIR_IN)
		printf("from ");
	else if (r->dir == DIR_OUT)
		printf("to ");
	else
		printf("eeeeeeeps. ");

	if (r->peer.peerid) {
		for (p = peer_l; p != NULL && p->conf.id != r->peer.peerid;
		    p = p->next)
			;	/* nothing */
		if (p == NULL)
			printf("?");
		else
			printf("%s ", log_addr(&p->conf.remote_addr));
	} else if (r->peer.groupid) {
		for (p = peer_l; p != NULL &&
		    p->conf.groupid != r->peer.groupid; p = p->next)
			;	/* nothing */
		if (p == NULL)
			printf("group ? ");
		else
			printf("group %s ", p->conf.group);
	} else
		printf("any ");

	if (r->match.prefix.addr.af)
		printf("prefix %s/%u ", log_addr(&r->match.prefix.addr),
		    r->match.prefix.len);

	if (r->match.prefixlen.op) {
		if (r->match.prefixlen.op == OP_RANGE)
			printf("prefixlen %u - %u ", r->match.prefixlen.len_min,
			    r->match.prefixlen.len_max);
		else {
			printf("prefixlen ");
			print_op(r->match.prefixlen.op);
			printf(" %u ", r->match.prefixlen.len_min);
		}
	}

	if (r->match.as.type) {
		if (r->match.as.type == AS_ALL)
			printf("AS %u ", r->match.as.as);
		else if (r->match.as.type == AS_SOURCE)
			printf("source-AS %u ", r->match.as.as);
		else if (r->match.as.type == AS_TRANSIT)
			printf("transit-AS %u ", r->match.as.as);
		else
			printf("unfluffy-AS %u ", r->match.as.as);
	}

	if (r->set.flags) {
		printf("set { ");
		if (r->set.flags & SET_LOCALPREF)
			printf("localpref %u ", r->set.localpref);
		if (r->set.flags & SET_MED)
			printf("med %u ", r->set.med);
		if (r->set.flags & SET_NEXTHOP)
			printf("nexthop %s ", inet_ntoa(r->set.nexthop));
		if (r->set.flags & SET_PREPEND)
			printf("prepend-self %u ", r->set.prepend);


		printf("}");
	}

	printf("\n");
}
