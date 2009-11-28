/*	$OpenBSD: brconfig.c,v 1.2 2009/11/28 20:07:18 chl Exp $	*/

/*
 * Copyright (c) 1999, 2000 Jason L. Wright (jason@thought.net)
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

#ifndef SMALL

#include <stdio.h>
#include <sys/types.h>
#include <sys/stdint.h>
#include <unistd.h>
#include <stdlib.h>
#include <sys/param.h>
#include <sys/socket.h>
#include <sys/ioctl.h>
#include <net/if.h>
#include <net/if_dl.h>
#include <netinet/in.h>
#include <netinet/if_ether.h>
#include <net/if_bridge.h>
#include <string.h>
#include <err.h>
#include <errno.h>
#include <getopt.h>
#include <sysexits.h>
#include <limits.h>

#include "brconfig.h"

void bridge_ifsetflag(const char *, u_int32_t);
void bridge_ifclrflag(const char *, u_int32_t);

void bridge_list(char *);
void bridge_cfg(const char *);
void bridge_badrule(int, char **, int);
void bridge_showrule(struct ifbrlreq *);

#define	IFBAFBITS	"\020\1STATIC"
#define	IFBIFBITS	\
"\020\1LEARNING\2DISCOVER\3BLOCKNONIP\4STP\5EDGE\6AUTOEDGE\7PTP\10AUTOPTP\11SPAN"

#define	PV2ID(pv, epri, eaddr)	do {					\
	epri	 = pv >> 48;						\
	eaddr[0] = pv >> 40;						\
	eaddr[1] = pv >> 32;						\
	eaddr[2] = pv >> 24;						\
	eaddr[3] = pv >> 16;						\
	eaddr[4] = pv >> 8;						\
	eaddr[5] = pv >> 0;						\
} while (0)

char *stpstates[] = {
	"disabled",
	"listening",
	"learning",
	"forwarding",
	"blocking",
	"discarding"
};
char *stpproto[] = {
	"stp",
	"(none)",
	"rstp",
};
char *stproles[] = {
	"disabled",
	"root",
	"designated",
	"alternate",
	"backup"
};


void
setdiscover(const char *val, int d)
{
	bridge_ifsetflag(val, IFBIF_DISCOVER);
}

void
unsetdiscover(const char *val, int d)
{
	bridge_ifclrflag(val, IFBIF_DISCOVER);
}

void
setblocknonip(const char *val, int d)
{
	bridge_ifsetflag(val, IFBIF_BLOCKNONIP);
}

void
unsetblocknonip(const char *val, int d)
{
	bridge_ifclrflag(val, IFBIF_BLOCKNONIP);
}

void
setlearn(const char *val, int d)
{
	bridge_ifsetflag(val, IFBIF_LEARNING);
}

void
unsetlearn(const char *val, int d)
{
	bridge_ifclrflag(val, IFBIF_LEARNING);
}

void
setstp(const char *val, int d)
{
	bridge_ifsetflag(val, IFBIF_STP);
}

void
unsetstp(const char *val, int d)
{
	bridge_ifclrflag(val, IFBIF_STP);
}

void
setedge(const char *val, int d)
{
	bridge_ifsetflag(val, IFBIF_BSTP_EDGE);
}

void
unsetedge(const char *val, int d)
{
	bridge_ifclrflag(val, IFBIF_BSTP_EDGE);
}

void
setautoedge(const char *val, int d)
{
	bridge_ifsetflag(val, IFBIF_BSTP_AUTOEDGE);
}

void
unsetautoedge(const char *val, int d)
{
	bridge_ifclrflag(val, IFBIF_BSTP_AUTOEDGE);
}

void
setptp(const char *val, int d)
{
	bridge_ifsetflag(val, IFBIF_BSTP_PTP);
}

void
unsetptp(const char *val, int d)
{
	bridge_ifclrflag(val, IFBIF_BSTP_PTP);
}

void
setautoptp(const char *val, int d)
{
	bridge_ifsetflag(val, IFBIF_BSTP_AUTOPTP);
}

void
unsetautoptp(const char *val, int d)
{
	bridge_ifclrflag(val, IFBIF_BSTP_AUTOPTP);
}


void
bridge_ifsetflag(const char *ifsname, u_int32_t flag)
{
	struct ifbreq req;

	strlcpy(req.ifbr_name, name, sizeof(req.ifbr_name));
	strlcpy(req.ifbr_ifsname, ifsname, sizeof(req.ifbr_ifsname));
	if (ioctl(s, SIOCBRDGGIFFLGS, (caddr_t)&req) < 0)
		err(EX_IOERR, "%s: ioctl SIOCBRDGGIFFLGS %s", name, ifsname);

	req.ifbr_ifsflags |= flag & ~IFBIF_RO_MASK;

	if (ioctl(s, SIOCBRDGSIFFLGS, (caddr_t)&req) < 0)
		err(EX_IOERR, "%s: ioctl SIOCBRDGSIFFLGS %s", name, ifsname);
}

void
bridge_ifclrflag(const char *ifsname, u_int32_t flag)
{
	struct ifbreq req;

	strlcpy(req.ifbr_name, name, sizeof(req.ifbr_name));
	strlcpy(req.ifbr_ifsname, ifsname, sizeof(req.ifbr_ifsname));

	if (ioctl(s, SIOCBRDGGIFFLGS, (caddr_t)&req) < 0)
		err(EX_IOERR, "%s: ioctl SIOCBRDGGIFFLGS %s", name, ifsname);

	req.ifbr_ifsflags &= ~(flag | IFBIF_RO_MASK);

	if (ioctl(s, SIOCBRDGSIFFLGS, (caddr_t)&req) < 0)
		err(EX_IOERR, "%s: ioctl SIOCBRDGSIFFLGS %s", name, ifsname);
}

void
bridge_flushall(const char *val, int p)
{
	struct ifbreq req;

	strlcpy(req.ifbr_name, name, sizeof(req.ifbr_name));
	req.ifbr_ifsflags = IFBF_FLUSHALL;
	if (ioctl(s, SIOCBRDGFLUSH, &req) < 0)
		err(EX_IOERR, "%s", name);
}

void
bridge_flush(const char *val, int p)
{
	struct ifbreq req;

	strlcpy(req.ifbr_name, name, sizeof(req.ifbr_name));
	req.ifbr_ifsflags = IFBF_FLUSHDYN;
	if (ioctl(s, SIOCBRDGFLUSH, &req) < 0)
		err(EX_IOERR, "%s", name);
}

void
bridge_cfg(const char *delim)
{
	struct ifbropreq ifbp;
	u_int16_t pri;
	u_int8_t ht, fd, ma, hc, proto;
	u_int8_t lladdr[ETHER_ADDR_LEN];
	u_int16_t bprio;

	strlcpy(ifbp.ifbop_name, name, sizeof(ifbp.ifbop_name));
	if (ioctl(s, SIOCBRDGGPARAM, (caddr_t)&ifbp))
		err(EX_IOERR, "%s", name);
	printf("%s", delim);
	pri = ifbp.ifbop_priority;
	ht = ifbp.ifbop_hellotime;
	fd = ifbp.ifbop_fwddelay;
	ma = ifbp.ifbop_maxage;
	hc = ifbp.ifbop_holdcount;
	proto = ifbp.ifbop_protocol;

	printf("priority %u hellotime %u fwddelay %u maxage %u "
	    "holdcnt %u proto %s\n", pri, ht, fd, ma, hc, stpproto[proto]);

	if (aflag)
		return;

	PV2ID(ifbp.ifbop_desg_bridge, bprio, lladdr);
	printf("\tdesignated: id %s priority %u\n",
	    ether_ntoa((struct ether_addr *)lladdr), bprio);

	if (ifbp.ifbop_root_bridge == ifbp.ifbop_desg_bridge)
		return;

	PV2ID(ifbp.ifbop_root_bridge, bprio, lladdr);
	printf("\troot: id %s priority %u ifcost %u port %u\n",
	    ether_ntoa((struct ether_addr *)lladdr), bprio,
	    ifbp.ifbop_root_path_cost, ifbp.ifbop_root_port & 0xfff);
}

void
bridge_list(char *delim)
{
	struct ifbreq *reqp;
	struct ifbifconf bifc;
	int i, len = 8192;
	char buf[sizeof(reqp->ifbr_ifsname) + 1], *inbuf = NULL, *inb;

	while (1) {
		bifc.ifbic_len = len;
		inb = realloc(inbuf, len);
		if (inb == NULL)
			err(1, "malloc");
		bifc.ifbic_buf = inbuf = inb;
		strlcpy(bifc.ifbic_name, name, sizeof(bifc.ifbic_name));
		if (ioctl(s, SIOCBRDGIFS, &bifc) < 0)
			err(1, "%s", name);
		if (bifc.ifbic_len + sizeof(*reqp) < len)
			break;
		len *= 2;
	}
	for (i = 0; i < bifc.ifbic_len / sizeof(*reqp); i++) {
		reqp = bifc.ifbic_req + i;
		strlcpy(buf, reqp->ifbr_ifsname, sizeof(buf));
		printf("%s%s ", delim, buf);
		printb("flags", reqp->ifbr_ifsflags, IFBIFBITS);
		printf("\n");
		if (reqp->ifbr_ifsflags & IFBIF_SPAN)
			continue;
		printf("\t\t");
		printf("port %u ifpriority %u ifcost %u",
		    reqp->ifbr_portno, reqp->ifbr_priority,
		    reqp->ifbr_path_cost);
		if (reqp->ifbr_ifsflags & IFBIF_STP)
			printf(" %s role %s",
			    stpstates[reqp->ifbr_state],
			    stproles[reqp->ifbr_role]);
		printf("\n");
		bridge_rules(buf, 0);
	}
	free(bifc.ifbic_buf);
}

void
bridge_add(const char *ifn, int d)
{
	struct ifbreq req;

	strlcpy(req.ifbr_name, name, sizeof(req.ifbr_name));
	strlcpy(req.ifbr_ifsname, ifn, sizeof(req.ifbr_ifsname));
	if (ioctl(s, SIOCBRDGADD, &req) < 0) {
		if (errno == EEXIST)
			return;
		err(EX_IOERR, "%s: %s", name, ifn);
	}
}

void
bridge_delete(const char *ifn, int d)
{
	struct ifbreq req;

	strlcpy(req.ifbr_name, name, sizeof(req.ifbr_name));
	strlcpy(req.ifbr_ifsname, ifn, sizeof(req.ifbr_ifsname));
	if (ioctl(s, SIOCBRDGDEL, &req) < 0)
		err(EX_IOERR, "%s: %s", name, ifn);
}

void
bridge_addspan(const char *ifn, int d)
{
	struct ifbreq req;

	strlcpy(req.ifbr_name, name, sizeof(req.ifbr_name));
	strlcpy(req.ifbr_ifsname, ifn, sizeof(req.ifbr_ifsname));
	if (ioctl(s, SIOCBRDGADDS, &req) < 0)
		err(EX_IOERR, "%s: %s", name, ifn);
}

void
bridge_delspan(const char *ifn, int d)
{
	struct ifbreq req;

	strlcpy(req.ifbr_name, name, sizeof(req.ifbr_name));
	strlcpy(req.ifbr_ifsname, ifn, sizeof(req.ifbr_ifsname));
	if (ioctl(s, SIOCBRDGDELS, &req) < 0)
		err(EX_IOERR, "%s: %s", name, ifn);
}

void
bridge_timeout(const char *arg, int d)
{
	struct ifbrparam bp;
	long newtime;
	char *endptr;

	errno = 0;
	newtime = strtol(arg, &endptr, 0);
	if (arg[0] == '\0' || endptr[0] != '\0' ||
	    (newtime & ~INT_MAX) != 0L ||
	    (errno == ERANGE && newtime == LONG_MAX))
		errx(EX_USAGE, "invalid arg for timeout: %s\n", arg);

	strlcpy(bp.ifbrp_name, name, sizeof(bp.ifbrp_name));
	bp.ifbrp_ctime = newtime;
	if (ioctl(s, SIOCBRDGSTO, (caddr_t)&bp) < 0)
		err(EX_IOERR, "%s", name);
}

void
bridge_maxage(const char *arg, int d)
{
	struct ifbrparam bp;
	unsigned long v;
	char *endptr;

	errno = 0;
	v = strtoul(arg, &endptr, 0);
	if (arg[0] == '\0' || endptr[0] != '\0' || v > 0xffUL ||
	    (errno == ERANGE && v == ULONG_MAX))
		errx(EX_USAGE, "invalid arg for maxage: %s\n", arg);

	strlcpy(bp.ifbrp_name, name, sizeof(bp.ifbrp_name));
	bp.ifbrp_maxage = v;
	if (ioctl(s, SIOCBRDGSMA, (caddr_t)&bp) < 0)
		err(EX_IOERR, "%s", name);
}

void
bridge_priority(const char *arg, int d)
{
	struct ifbrparam bp;
	unsigned long v;
	char *endptr;

	errno = 0;
	v = strtoul(arg, &endptr, 0);
	if (arg[0] == '\0' || endptr[0] != '\0' || v > 0xffffUL ||
	    (errno == ERANGE && v == ULONG_MAX))
		errx(EX_USAGE, "invalid arg for spanpriority: %s\n", arg);

	strlcpy(bp.ifbrp_name, name, sizeof(bp.ifbrp_name));
	bp.ifbrp_prio = v;
	if (ioctl(s, SIOCBRDGSPRI, (caddr_t)&bp) < 0)
		err(EX_IOERR, "%s", name);
}

void
bridge_proto(const char *arg, int d)
{
	struct ifbrparam bp;
	int i, proto = -1;

	for (i = 0; i <= BSTP_PROTO_MAX; i++)
		if (strcmp(arg, stpproto[i]) == 0) {
			proto = i;
			break;
		}
	if (proto == -1)
		errx(EX_USAGE, "invalid arg for proto: %s\n", arg);

	strlcpy(bp.ifbrp_name, name, sizeof(bp.ifbrp_name));
	bp.ifbrp_prio = proto;
	if (ioctl(s, SIOCBRDGSPROTO, (caddr_t)&bp) < 0)
		err(EX_IOERR, "%s", name);
}

void
bridge_fwddelay(const char *arg, int d)
{
	struct ifbrparam bp;
	unsigned long v;
	char *endptr;

	errno = 0;
	v = strtoul(arg, &endptr, 0);
	if (arg[0] == '\0' || endptr[0] != '\0' || v > 0xffUL ||
	    (errno == ERANGE && v == ULONG_MAX))
		errx(EX_USAGE, "invalid arg for fwddelay: %s\n", arg);

	strlcpy(bp.ifbrp_name, name, sizeof(bp.ifbrp_name));
	bp.ifbrp_fwddelay = v;
	if (ioctl(s, SIOCBRDGSFD, (caddr_t)&bp) < 0)
		err(EX_IOERR, "%s", name);
}

void
bridge_hellotime(const char *arg, int d)
{
	struct ifbrparam bp;
	unsigned long v;
	char *endptr;

	errno = 0;
	v = strtoul(arg, &endptr, 0);
	if (arg[0] == '\0' || endptr[0] != '\0' || v > 0xffUL ||
	    (errno == ERANGE && v == ULONG_MAX))
		errx(EX_USAGE, "invalid arg for hellotime: %s\n", arg);

	strlcpy(bp.ifbrp_name, name, sizeof(bp.ifbrp_name));
	bp.ifbrp_hellotime = v;
	if (ioctl(s, SIOCBRDGSHT, (caddr_t)&bp) < 0)
		err(EX_IOERR, "%s", name);
}

void
bridge_maxaddr(const char *arg, int d)
{
	struct ifbrparam bp;
	unsigned long newsize;
	char *endptr;

	errno = 0;
	newsize = strtoul(arg, &endptr, 0);
	if (arg[0] == '\0' || endptr[0] != '\0' || newsize > 0xffffffffUL ||
	    (errno == ERANGE && newsize == ULONG_MAX))
		errx(EX_USAGE, "invalid arg for maxaddr: %s\n", arg);

	strlcpy(bp.ifbrp_name, name, sizeof(bp.ifbrp_name));
	bp.ifbrp_csize = newsize;
	if (ioctl(s, SIOCBRDGSCACHE, (caddr_t)&bp) < 0)
		err(EX_IOERR, "%s", name);
}

void
bridge_deladdr(const char *addr, int d)
{
	struct ifbareq ifba;
	struct ether_addr *ea;

	strlcpy(ifba.ifba_name, name, sizeof(ifba.ifba_name));
	ea = ether_aton(addr);
	if (ea == NULL)
		err(EX_USAGE, "Invalid address: %s", addr);

	bcopy(ea, &ifba.ifba_dst, sizeof(struct ether_addr));

	if (ioctl(s, SIOCBRDGDADDR, &ifba) < 0)
		err(EX_IOERR, "%s: %s", name, addr);
}

void
bridge_ifprio(const char *ifname, const char *val)
{
	struct ifbreq breq;
	unsigned long v;
	char *endptr;

	strlcpy(breq.ifbr_name, name, sizeof(breq.ifbr_name));
	strlcpy(breq.ifbr_ifsname, ifname, sizeof(breq.ifbr_ifsname));

	errno = 0;
	v = strtoul(val, &endptr, 0);
	if (val[0] == '\0' || endptr[0] != '\0' || v > 0xffUL ||
	    (errno == ERANGE && v == ULONG_MAX))
		err(EX_USAGE, "invalid arg for ifpriority: %s\n", val);
	breq.ifbr_priority = v;

	if (ioctl(s, SIOCBRDGSIFPRIO, (caddr_t)&breq) < 0)
		err(EX_IOERR, "%s: %s", name, val);
}

void
bridge_ifcost(const char *ifname, const char *val)
{
	struct ifbreq breq;
	unsigned long v;
	char *endptr;

	strlcpy(breq.ifbr_name, name, sizeof(breq.ifbr_name));
	strlcpy(breq.ifbr_ifsname, ifname, sizeof(breq.ifbr_ifsname));

	errno = 0;
	v = strtoul(val, &endptr, 0);
	if (val[0] == '\0' || endptr[0] != '\0' ||
	    v < 0 || v > 0xffffffffUL ||
	    (errno == ERANGE && v == ULONG_MAX))
		errx(EX_USAGE, "invalid arg for ifcost: %s\n", val);

	breq.ifbr_path_cost = v;

	if (ioctl(s, SIOCBRDGSIFCOST, (caddr_t)&breq) < 0)
		err(EX_IOERR, "%s: %s", name, val);
}

void
bridge_noifcost(const char *ifname, int d)
{
	struct ifbreq breq;

	strlcpy(breq.ifbr_name, name, sizeof(breq.ifbr_name));
	strlcpy(breq.ifbr_ifsname, ifname, sizeof(breq.ifbr_ifsname));

	breq.ifbr_path_cost = 0;

	if (ioctl(s, SIOCBRDGSIFCOST, (caddr_t)&breq) < 0)
		err(EX_IOERR, "%s", name);
}

void
bridge_addaddr(const char *ifname, const char *addr)
{
	struct ifbareq ifba;
	struct ether_addr *ea;

	strlcpy(ifba.ifba_name, name, sizeof(ifba.ifba_name));
	strlcpy(ifba.ifba_ifsname, ifname, sizeof(ifba.ifba_ifsname));

	ea = ether_aton(addr);
	if (ea == NULL)
		errx(EX_USAGE, "Invalid address: %s", addr);

	bcopy(ea, &ifba.ifba_dst, sizeof(struct ether_addr));
	ifba.ifba_flags = IFBAF_STATIC;

	if (ioctl(s, SIOCBRDGSADDR, &ifba) < 0)
		err(EX_IOERR, "%s: %s", name, addr);
}

void
bridge_addrs(const char *delim, int d)
{
	struct ifbaconf ifbac;
	struct ifbareq *ifba;
	char *inbuf = NULL, buf[sizeof(ifba->ifba_ifsname) + 1], *inb;
	int i, len = 8192;

	/* ifconfig will call us with the argv of the command */
	if (strcmp(delim, "addr") == 0)
		delim = "";

	while (1) {
		ifbac.ifbac_len = len;
		inb = realloc(inbuf, len);
		if (inb == NULL)
			err(EX_IOERR, "malloc");
		ifbac.ifbac_buf = inbuf = inb;
		strlcpy(ifbac.ifbac_name, name, sizeof(ifbac.ifbac_name));
		if (ioctl(s, SIOCBRDGRTS, &ifbac) < 0) {
			if (errno == ENETDOWN)
				return;
			err(EX_IOERR, "%s", name);
		}
		if (ifbac.ifbac_len + sizeof(*ifba) < len)
			break;
		len *= 2;
	}

	for (i = 0; i < ifbac.ifbac_len / sizeof(*ifba); i++) {
		ifba = ifbac.ifbac_req + i;
		strlcpy(buf, ifba->ifba_ifsname, sizeof(buf));
		printf("%s%s %s %u ", delim, ether_ntoa(&ifba->ifba_dst),
		    buf, ifba->ifba_age);
		printb("flags", ifba->ifba_flags, IFBAFBITS);
		printf("\n");
	}
	free(inbuf);
}

void
bridge_holdcnt(const char *value, int d)
{
	struct ifbrparam bp;
	const char *errstr;

	bp.ifbrp_txhc = strtonum(value, 0, UINT8_MAX, &errstr);
	if (errstr)
		err(1, "holdcnt %s %s", value, errstr);

	strlcpy(bp.ifbrp_name, name, sizeof(bp.ifbrp_name));
	if (ioctl(s, SIOCBRDGSTXHC, (caddr_t)&bp) < 0)
		err(EX_IOERR, "%s", name);
}

/*
 * Check to make sure 'brdg' is really a bridge interface.
 */
int
is_bridge(char *brdg)
{
	struct ifreq ifr;
	struct ifbaconf ifbac;

	strlcpy(ifr.ifr_name, brdg, sizeof(ifr.ifr_name));

	if (ioctl(s, SIOCGIFFLAGS, (caddr_t)&ifr) < 0)
		return (0);

	ifbac.ifbac_len = 0;
	strlcpy(ifbac.ifbac_name, brdg, sizeof(ifbac.ifbac_name));
	if (ioctl(s, SIOCBRDGRTS, (caddr_t)&ifbac) < 0) {
		if (errno == ENETDOWN)
			return (1);
		return (0);
	}
	return (1);
}

void
bridge_status(void)
{
	struct ifreq ifr;
	struct ifbrparam bp1, bp2;

	if (!is_bridge(name))
		return;

	strlcpy(ifr.ifr_name, name, sizeof(ifr.ifr_name));
	if (ioctl(s, SIOCGIFFLAGS, (caddr_t)&ifr) < 0)
		return;

	bridge_cfg("\t");

	bridge_list("\t");

	if (aflag && !ifaliases)
		return;

	strlcpy(bp1.ifbrp_name, name, sizeof(bp1.ifbrp_name));
	if (ioctl(s, SIOCBRDGGCACHE, (caddr_t)&bp1) < 0)
		return;

	strlcpy(bp2.ifbrp_name, name, sizeof(bp2.ifbrp_name));
	if (ioctl(s, SIOCBRDGGTO, (caddr_t)&bp2) < 0)
		return;

	printf("\tAddresses (max cache: %u, timeout: %u):\n",
	    bp1.ifbrp_csize, bp2.ifbrp_ctime);

	bridge_addrs("\t\t", 0);
}

void
bridge_flushrule(const char *ifname, int d)
{
	struct ifbrlreq req;

	strlcpy(req.ifbr_name, name, sizeof(req.ifbr_name));
	strlcpy(req.ifbr_ifsname, ifname, sizeof(req.ifbr_ifsname));
	if (ioctl(s, SIOCBRDGFRL, &req) < 0)
		err(EX_IOERR, "%s: %s", name, ifname);
}

void
bridge_rules(const char *ifname, int d)
{
	char *inbuf = NULL, *inb;
	struct ifbrlconf ifc;
	struct ifbrlreq *ifrp;
	int len = 8192, i;

	while (1) {
		ifc.ifbrl_len = len;
		inb = realloc(inbuf, len);
		if (inb == NULL)
			err(1, "malloc");
		ifc.ifbrl_buf = inbuf = inb;
		strlcpy(ifc.ifbrl_name, name, sizeof(ifc.ifbrl_name));
		strlcpy(ifc.ifbrl_ifsname, ifname, sizeof(ifc.ifbrl_ifsname));
		if (ioctl(s, SIOCBRDGGRL, &ifc) < 0)
			err(1, "ioctl(SIOCBRDGGRL)");
		if (ifc.ifbrl_len + sizeof(*ifrp) < len)
			break;
		len *= 2;
	}
	ifrp = ifc.ifbrl_req;
	for (i = 0; i < ifc.ifbrl_len; i += sizeof(*ifrp)) {
		ifrp = (struct ifbrlreq *)((caddr_t)ifc.ifbrl_req + i);
		bridge_showrule(ifrp);
	}
}

void
bridge_showrule(struct ifbrlreq *r)
{
	if (r->ifbr_action == BRL_ACTION_BLOCK)
		printf("block ");
	else if (r->ifbr_action == BRL_ACTION_PASS)
		printf("pass ");
	else
		printf("[neither block nor pass?]\n");

	if ((r->ifbr_flags & (BRL_FLAG_IN | BRL_FLAG_OUT)) ==
	    (BRL_FLAG_IN | BRL_FLAG_OUT))
		printf("in/out ");
	else if (r->ifbr_flags & BRL_FLAG_IN)
		printf("in ");
	else if (r->ifbr_flags & BRL_FLAG_OUT)
		printf("out ");
	else
		printf("[neither in nor out?]\n");

	printf("on %s", r->ifbr_ifsname);

	if (r->ifbr_flags & BRL_FLAG_SRCVALID)
		printf(" src %s", ether_ntoa(&r->ifbr_src));
	if (r->ifbr_flags & BRL_FLAG_DSTVALID)
		printf(" dst %s", ether_ntoa(&r->ifbr_dst));
	if (r->ifbr_tagname[0])
		printf(" tag %s", r->ifbr_tagname);

	printf("\n");
}

/*
 * Parse a rule definition and send it upwards.
 *
 * Syntax:
 *	{block|pass} {in|out|in/out} on {ifs} [src {mac}] [dst {mac}]
 */
int
bridge_rule(int targc, char **targv, int ln)
{
	char **argv = targv;
	int argc = targc;
	struct ifbrlreq rule;
	struct ether_addr *ea, *dea;

	if (argc == 0) {
		fprintf(stderr, "invalid rule\n");
		return (EX_USAGE);
	}
	rule.ifbr_tagname[0] = 0;
	rule.ifbr_flags = 0;
	rule.ifbr_action = 0;
	strlcpy(rule.ifbr_name, name, sizeof(rule.ifbr_name));

	if (strcmp(argv[0], "block") == 0)
		rule.ifbr_action = BRL_ACTION_BLOCK;
	else if (strcmp(argv[0], "pass") == 0)
		rule.ifbr_action = BRL_ACTION_PASS;
	else
		goto bad_rule;
	argc--;	argv++;

	if (argc == 0) {
		bridge_badrule(targc, targv, ln);
		return (EX_USAGE);
	}
	if (strcmp(argv[0], "in") == 0)
		rule.ifbr_flags |= BRL_FLAG_IN;
	else if (strcmp(argv[0], "out") == 0)
		rule.ifbr_flags |= BRL_FLAG_OUT;
	else if (strcmp(argv[0], "in/out") == 0)
		rule.ifbr_flags |= BRL_FLAG_IN | BRL_FLAG_OUT;
	else if (strcmp(argv[0], "on") == 0) {
		rule.ifbr_flags |= BRL_FLAG_IN | BRL_FLAG_OUT;
		argc++; argv--;
	} else
		goto bad_rule;
	argc--; argv++;

	if (argc == 0 || strcmp(argv[0], "on"))
		goto bad_rule;
	argc--; argv++;

	if (argc == 0)
		goto bad_rule;
	strlcpy(rule.ifbr_ifsname, argv[0], sizeof(rule.ifbr_ifsname));
	argc--; argv++;

	while (argc) {
		if (strcmp(argv[0], "dst") == 0) {
			if (rule.ifbr_flags & BRL_FLAG_DSTVALID)
				goto bad_rule;
			rule.ifbr_flags |= BRL_FLAG_DSTVALID;
			dea = &rule.ifbr_dst;
		} else if (strcmp(argv[0], "src") == 0) {
			if (rule.ifbr_flags & BRL_FLAG_SRCVALID)
				goto bad_rule;
			rule.ifbr_flags |= BRL_FLAG_SRCVALID;
			dea = &rule.ifbr_src;
		} else if (strcmp(argv[0], "tag") == 0) {
			if (argc < 2) {
				fprintf(stderr, "missing tag name\n");
				goto bad_rule;
			}
			if (rule.ifbr_tagname[0]) {
				fprintf(stderr, "tag already defined\n");
				goto bad_rule;
			}
			if (strlcpy(rule.ifbr_tagname, argv[1],
			    PF_TAG_NAME_SIZE) > PF_TAG_NAME_SIZE) {
				fprintf(stderr, "tag name too long\n");
				goto bad_rule;
			}
			dea = NULL;
		} else
			goto bad_rule;

		argc--; argv++;

		if (argc == 0)
			goto bad_rule;
		if (dea != NULL) {
			ea = ether_aton(argv[0]);
			if (ea == NULL) {
				warnx("Invalid address: %s", argv[0]);
				return (EX_USAGE);
			}
			bcopy(ea, dea, sizeof(*dea));
		}
		argc--; argv++;
	}

	if (ioctl(s, SIOCBRDGARL, &rule) < 0) {
		warn("%s", name);
		return (EX_IOERR);
	}
	return (0);

bad_rule:
	bridge_badrule(targc, targv, ln);
	return (EX_USAGE);
}

#define MAXRULEWORDS 8

void
bridge_rulefile(const char *fname, int d)
{
	FILE *f;
	char *str, *argv[MAXRULEWORDS], buf[1024];
	int ln = 0, argc = 0;

	f = fopen(fname, "r");
	if (f == NULL)
		err(EX_IOERR, "%s", fname);

	while (fgets(buf, sizeof(buf), f) != NULL) {
		ln++;
		if (buf[0] == '#' || buf[0] == '\n')
			continue;

		argc = 0;
		str = strtok(buf, "\n\t\r ");
		while (str != NULL && argc < MAXRULEWORDS) {
			argv[argc++] = str;
			str = strtok(NULL, "\n\t\r ");
		}

		/* Rule is too long if there's more. */
		if (str != NULL) {
			fprintf(stderr, "invalid rule: %d: %s ...\n", ln, buf);
			continue;
		}

		bridge_rule(argc, argv, ln);
	}
	fclose(f);
}

void
bridge_badrule(int argc, char *argv[], int ln)
{
	int i;

	fprintf(stderr, "invalid rule: ");
	if (ln != -1)
		fprintf(stderr, "%d: ", ln);
	for (i = 0; i < argc; i++) {
		fprintf(stderr, "%s ", argv[i]);
	}
	fprintf(stderr, "\n");
}

#endif
