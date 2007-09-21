/*	$OpenBSD: print-pfsync.c,v 1.31 2007/09/21 08:42:00 markus Exp $	*/

/*
 * Copyright (c) 2002 Michael Shalayeff
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
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR OR HIS RELATIVES BE LIABLE FOR ANY DIRECT,
 * INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF MIND, USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT,
 * STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING
 * IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
 * THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef lint
static const char rcsid[] =
    "@(#) $Header: /home/cvs/src/usr.sbin/tcpdump/print-pfsync.c,v 1.31 2007/09/21 08:42:00 markus Exp $";
#endif

#include <sys/param.h>
#include <sys/time.h>
#include <sys/socket.h>
#include <sys/file.h>
#include <sys/ioctl.h>
#include <sys/mbuf.h>

#ifdef __STDC__
struct rtentry;
#endif
#include <net/if.h>

#include <netinet/in.h>
#include <netinet/in_systm.h>
#include <netinet/ip.h>

#include <net/pfvar.h>
#include <net/if_pfsync.h>

#include <ctype.h>
#include <netdb.h>
#include <pcap.h>
#include <signal.h>
#include <stdio.h>
#include <string.h>

#include "interface.h"
#include "addrtoname.h"
#include "pfctl_parser.h"
#include "pfctl.h"

const char *pfsync_acts[] = { PFSYNC_ACTIONS };

void	pfsync_print(struct pfsync_header *, int);

void
pfsync_if_print(u_char *user, const struct pcap_pkthdr *h,
     register const u_char *p)
{
	u_int caplen = h->caplen;

	ts_print(&h->ts);

	if (caplen < PFSYNC_HDRLEN) {
		printf("[|pfsync]");
		goto out;
	}

	pfsync_print((struct pfsync_header *)p,
	    caplen - sizeof(struct pfsync_header));
out:
	if (xflag) {
		default_print((const u_char *)p, caplen);
	}
	putchar('\n');
}

void
pfsync_ip_print(const u_char *bp, u_int len, const u_char *bp2)
{
	struct pfsync_header *hdr = (struct pfsync_header *)bp;
	struct ip *ip = (struct ip *)bp2;

	if (vflag)
		printf("%s > %s: ", ipaddr_string(&ip->ip_src),
		    ipaddr_string(&ip->ip_dst));
	else
		printf("%s: ", ipaddr_string(&ip->ip_src));

	if (len < PFSYNC_HDRLEN)
		printf("[|pfsync]");
	else
		pfsync_print(hdr, (len - sizeof(struct pfsync_header)));
	putchar('\n');
}

void
pfsync_print(struct pfsync_header *hdr, int len)
{
	struct pfsync_state *s;
	struct pfsync_state_upd *u;
	struct pfsync_state_del *d;
	struct pfsync_state_clr *c;
	struct pfsync_state_upd_req *r;
	struct pfsync_state_bus *b;
	struct pfsync_tdb *t;
	int i, flags = 0, min, sec;
	u_int64_t id;

	if (eflag)
		printf("PFSYNCv%d count %d: ",
		    hdr->version, hdr->count);

	if (hdr->action < PFSYNC_ACT_MAX)
		printf("%s:", pfsync_acts[hdr->action]);
	else
		printf("%d?:", hdr->action);

	if (vflag)
		flags |= PF_OPT_VERBOSE;
	if (vflag > 1)
		flags |= PF_OPT_VERBOSE2;
	if (!nflag)
		flags |= PF_OPT_USEDNS;

	switch (hdr->action) {
	case PFSYNC_ACT_CLR:
		if (sizeof(*c) <= len) {
			c = (void *)((char *)hdr + PFSYNC_HDRLEN);
			printf("\n\tcreatorid: %08x", htonl(c->creatorid));
			if (c->ifname[0] != '\0')
				printf(" interface: %s", c->ifname);
		}
	case PFSYNC_ACT_INS:
	case PFSYNC_ACT_UPD:
	case PFSYNC_ACT_DEL:
		for (i = 1, s = (void *)((char *)hdr + PFSYNC_HDRLEN);
		    i <= hdr->count && i * sizeof(*s) <= len; i++, s++) {

			putchar('\n');
			print_state(s, flags);
			if (vflag > 1 && hdr->action == PFSYNC_ACT_UPD)
				printf(" updates: %d", s->updates);
		}
		break;
	case PFSYNC_ACT_UPD_C:
		for (i = 1, u = (void *)((char *)hdr + PFSYNC_HDRLEN);
		    i <= hdr->count && i * sizeof(*u) <= len; i++, u++) {
			bcopy(&u->id, &id, sizeof(id));
			printf("\n\tid: %016llx creatorid: %08x",
			    betoh64(id), ntohl(u->creatorid));
			if (vflag > 1)
				printf(" updates: %d", u->updates);
		}
		break;
	case PFSYNC_ACT_DEL_C:
		for (i = 1, d = (void *)((char *)hdr + PFSYNC_HDRLEN);
		    i <= hdr->count && i * sizeof(*d) <= len; i++, d++) {
			bcopy(&d->id, &id, sizeof(id));
			printf("\n\tid: %016llx creatorid: %08x",
			    betoh64(id), ntohl(d->creatorid));
		}
		break;
	case PFSYNC_ACT_UREQ:
		for (i = 1, r = (void *)((char *)hdr + PFSYNC_HDRLEN);
		    i <= hdr->count && i * sizeof(*r) <= len; i++, r++) {
			bcopy(&r->id, &id, sizeof(id));
			printf("\n\tid: %016llx creatorid: %08x",
			    betoh64(id), ntohl(r->creatorid));
		}
		break;
	case PFSYNC_ACT_BUS:
		if (sizeof(*b) <= len) {
			b = (void *)((char *)hdr + PFSYNC_HDRLEN);
			printf("\n\tcreatorid: %08x", htonl(b->creatorid));
			sec = b->endtime % 60;
			b->endtime /= 60;
			min = b->endtime % 60;
			b->endtime /= 60;
			printf(" age %.2u:%.2u:%.2u", b->endtime, min, sec);
			switch (b->status) {
			case PFSYNC_BUS_START:
				printf(" status: start");
				break;
			case PFSYNC_BUS_END:
				printf(" status: end");
				break;
			default:
				printf(" status: ?");
				break;
			}
		}
		break;
	case PFSYNC_ACT_TDB_UPD:
		for (i = 1, t = (void *)((char *)hdr + PFSYNC_HDRLEN);
		    i <= hdr->count && i * sizeof(*t) <= len; i++, t++)
			printf("\n\tspi: %08x rpl: %u cur_bytes: %llu",
			    htonl(t->spi), htonl(t->rpl),
			    betoh64(t->cur_bytes));
			/* XXX add dst and sproto? */
		break;
	default:
		break;
	}
}
