/*	$OpenBSD: print-pfsync.c,v 1.16 2003/12/27 19:50:47 mcbride Exp $	*/

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
    "@(#) $Header: /home/cvs/src/usr.sbin/tcpdump/print-pfsync.c,v 1.16 2003/12/27 19:50:47 mcbride Exp $";
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
		default_print((const u_char *)h, caplen);
		putchar('\n');
	}
}

void
pfsync_ip_print(const u_char *bp, u_int len, const u_char *bp2)
{
	const struct ip *ip = (const struct ip *)bp2;
	struct pfsync_header *hdr = (struct pfsync_header *)bp;
	u_int hlen = ip->ip_hl << 2;

	if (len < PFSYNC_HDRLEN)
		printf("[|pfsync]");
	else
		pfsync_print(hdr, (len - sizeof(struct pfsync_header)));
}

void
pfsync_print(struct pfsync_header *hdr, int len)
{
	struct pfsync_state *s;
	struct pfsync_state_upd *u;
	struct pfsync_state_del *d;
	int i, flags;

	if (eflag)
		printf("version %d count %d: ",
		    hdr->version, hdr->count);

	if (hdr->action < PFSYNC_ACT_MAX)
		printf("%s:\n", pfsync_acts[hdr->action]);
	else
		printf("%d?:\n", hdr->action);

	flags = 0;
	if (vflag)
		flags |= PF_OPT_VERBOSE;
	if (!nflag)
		flags |= PF_OPT_USEDNS;

	switch (hdr->action) {
	case PFSYNC_ACT_INS:
	case PFSYNC_ACT_UPD:
	case PFSYNC_ACT_DEL:
		for (i = 1, s = (void *)((char *)hdr + PFSYNC_HDRLEN);
		    i <= hdr->count && i * sizeof(*s) <= len; i++, s++) {
			struct pf_state st;

			bzero(&st, sizeof(st));
			st.id = s->id;
			pf_state_host_ntoh(&s->lan, &st.lan);
			pf_state_host_ntoh(&s->gwy, &st.gwy);
			pf_state_host_ntoh(&s->ext, &st.ext);
			pf_state_peer_ntoh(&s->src, &st.src);
			pf_state_peer_ntoh(&s->dst, &st.dst);
			st.rule.nr = ntohl(s->rule);
			st.nat_rule.nr = ntohl(s->nat_rule);
			st.anchor.nr = ntohl(s->anchor);
			bcopy(&s->rt_addr, &st.rt_addr, sizeof(st.rt_addr));
			st.creation = ntohl(s->creation);
			st.expire = ntohl(s->expire);
			st.packets[0] = ntohl(s->packets[0]);
			st.packets[1] = ntohl(s->packets[1]);
			st.bytes[0] = ntohl(s->bytes[0]);
			st.bytes[1] = ntohl(s->bytes[1]);
			st.creatorid = s->creatorid;
			st.af = s->af;
			st.proto = s->proto;
			st.direction = s->direction;
			st.log = s->log;
			st.allow_opts = s->allow_opts;
			st.sync_flags = s->sync_flags;

			print_state(&st, flags);
		}
		break;
	case PFSYNC_ACT_UPD_C:
		for (i = 1, u = (void *)((char *)hdr + PFSYNC_HDRLEN);
		    i <= hdr->count && i * sizeof(*u) <= len; i++, d++) {
			printf("\tid: %016llx creatorid: %08x\n",
			    betoh64(u->id), htonl(u->creatorid));
		}
		break;
	case PFSYNC_ACT_DEL_C:
		for (i = 1, d = (void *)((char *)hdr + PFSYNC_HDRLEN);
		    i <= hdr->count && i * sizeof(*d) <= len; i++, d++) {
			printf("\tid: %016llx creatorid: %08x\n",
			    betoh64(d->id), htonl(d->creatorid));
		}
		break;
	default:
		break;
	}
}
