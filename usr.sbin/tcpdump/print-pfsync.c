/*	$OpenBSD: print-pfsync.c,v 1.1 2002/11/29 18:27:54 mickey Exp $	*/

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
    "@(#) $Header: /home/cvs/src/usr.sbin/tcpdump/print-pfsync.c,v 1.1 2002/11/29 18:27:54 mickey Exp $";
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

#include "interface.h"
#include "addrtoname.h"
#include "pfctl_parser.h"
#include "pf_print_state.h"

const char *pfsync_acts[] = { PFSYNC_ACTIONS };

void
pfsync_if_print(u_char *user, const struct pcap_pkthdr *h,
     register const u_char *p)
{
	/*u_int length = h->len;*/
	u_int caplen = h->caplen;
	struct pfsync_header *hdr;
	struct pf_state *s;
	int i, flags;

	ts_print(&h->ts);

	if (caplen < PFSYNC_HDRLEN) {
		printf("[|pflog]");
		goto out;
	}

	packetp = p;
	snapend = p + caplen;

	hdr = (struct pfsync_header *)p;
	if (eflag)
		printf("version %d count %d: ",
		    hdr->version, hdr->count);

	if (hdr->action < PFSYNC_ACT_MAX)
		printf("%s: ", pfsync_acts[hdr->action]);
	else
		printf("%d?: ", hdr->action);

	flags = 0;
	if (vflag)
		flags |= PF_OPT_VERBOSE;
	if (!nflag)
		flags |= PF_OPT_USEDNS;

	for (i = 1, s = (struct pf_state *)(p + PFSYNC_HDRLEN);
	    i <= hdr->count && PFSYNC_HDRLEN + i * sizeof(*s) <= caplen;
	    i++, s++) {
		struct pf_state st;

		st.lan = s->lan; NTOHS(st.lan.port);
		st.gwy = s->gwy; NTOHS(st.gwy.port);
		st.ext = s->ext; NTOHS(st.ext.port);
		pf_state_peer_ntoh(&s->src, &st.src);
		pf_state_peer_ntoh(&s->dst, &st.dst);
		st.rule.nr = ntohl(s->rule.nr);
		st.rt_addr = s->rt_addr;
		st.creation = ntohl(s->creation);
		st.expire = ntohl(s->expire);
		st.packets = ntohl(s->packets);
		st.bytes = ntohl(s->bytes);
		st.af = s->af;
		st.proto = s->proto;
		st.direction = s->direction;
		st.log = s->log;
		st.allow_opts = s->allow_opts;

		printf("rule %d ", st.rule.nr);

		print_state(&st, flags);
	}
out:
	putchar('\n');
}
