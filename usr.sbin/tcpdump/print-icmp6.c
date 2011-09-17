/*	$OpenBSD: print-icmp6.c,v 1.9 2011/09/17 15:59:44 naddy Exp $	*/

/*
 * Copyright (c) 1988, 1989, 1990, 1991, 1993, 1994
 *	The Regents of the University of California.  All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that: (1) source code distributions
 * retain the above copyright notice and this paragraph in its entirety, (2)
 * distributions including binary code include the above copyright notice and
 * this paragraph in its entirety in the documentation or other materials
 * provided with the distribution, and (3) all advertising materials mentioning
 * features or use of this software display the following acknowledgement:
 * ``This product includes software developed by the University of California,
 * Lawrence Berkeley Laboratory and its contributors.'' Neither the name of
 * the University nor the names of its contributors may be used to endorse
 * or promote products derived from this software without specific prior
 * written permission.
 * THIS SOFTWARE IS PROVIDED ``AS IS'' AND WITHOUT ANY EXPRESS OR IMPLIED
 * WARRANTIES, INCLUDING, WITHOUT LIMITATION, THE IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE.
 */

#ifdef INET6

#include <ctype.h>

#include <sys/param.h>
#include <sys/time.h>
#include <sys/types.h>
#include <sys/socket.h>

#include <net/if.h>

#include <netinet/in.h>
#include <netinet/if_ether.h>
#include <netinet/in_systm.h>
#include <netinet/ip.h>
#include <netinet/ip_icmp.h>
#include <netinet/ip_var.h>
#include <netinet/udp.h>
#include <netinet/udp_var.h>
#include <netinet/tcp.h>

#include <arpa/inet.h>

#include <stdio.h>

#include <netinet/ip6.h>
#include <netinet/icmp6.h>
#include <netinet6/mld6.h>

#include "interface.h"
#include "addrtoname.h"

void icmp6_opt_print(const u_char *, int);
void mld6_print(const u_char *);
void mldv2_query_print(const u_char *, u_int);
void mldv2_report_print(const u_char *, u_int);

/* mldv2 report types */
static struct tok mldv2report2str[] = {
	{ 1,	"is_in" },
	{ 2,	"is_ex" },
	{ 3,	"to_in" },
	{ 4,	"to_ex" },
	{ 5,	"allow" },
	{ 6,	"block" },
	{ 0,	NULL }
};

#define MLDV2_QUERY_QRV			24
#define MLDV2_QUERY_QQIC 		25
#define MLDV2_QUERY_NSRCS		26
#define MLDV2_QUERY_SRC0		28

#define MLDV2_QUERY_QRV_SFLAG	(1 << 3)

#define MLD_V1_QUERY_MINLEN		24

#define MLDV2_REPORT_GROUP0		8

#define MLDV2_REPORT_MINLEN		8
#define MLDV2_REPORT_MINGRPLEN	20

#define MLDV2_RGROUP_NSRCS		2
#define MLDV2_RGROUP_MADDR		4

#define MLDV2_MRC_FLOAT			(1 << 15)
#define MLDV2_MRD(mant, exp)	((mant | 0x1000) << (exp + 3))

#define MLDV2_QQIC_FLOAT		(1 << 7)
#define MLDV2_QQI(mant, exp)	((mant | 0x10) << (exp + 3))

void
icmp6_print(const u_char *bp, u_int length, const u_char *bp2)
{
	register const struct icmp6_hdr *dp;
	register const struct ip6_hdr *ip;
	register const char *str;
	register const struct ip6_hdr *oip;
	register const struct udphdr *ouh;
	register int hlen, dport;
	register const u_char *ep;
	char buf[256];
	int icmp6len;

#if 0
#define TCHECK(var) if ((u_char *)&(var) > ep - sizeof(var)) goto trunc
#endif

	dp = (struct icmp6_hdr *)bp;
	ip = (struct ip6_hdr *)bp2;
	oip = (struct ip6_hdr *)(dp + 1);
	str = buf;
	/* 'ep' points to the end of avaible data. */
	ep = snapend;
	if (ip->ip6_plen)
		icmp6len = (ntohs(ip->ip6_plen) + sizeof(struct ip6_hdr) -
			    (bp - bp2));
	else			/* XXX: jumbo payload case... */
		icmp6len = snapend - bp;

#if 0
        (void)printf("%s > %s: ",
		ip6addr_string(&ip->ip6_src),
		ip6addr_string(&ip->ip6_dst));
#endif

	TCHECK(dp->icmp6_code);
	switch (dp->icmp6_type) {
	case ICMP6_DST_UNREACH:
		TCHECK(oip->ip6_dst);
		switch (dp->icmp6_code) {
		case ICMP6_DST_UNREACH_NOROUTE:
			printf("icmp6: %s unreachable route",
			       ip6addr_string(&oip->ip6_dst));
			break;
		case ICMP6_DST_UNREACH_ADMIN:
			printf("icmp6: %s unreachable prohibited",
			       ip6addr_string(&oip->ip6_dst));
			break;
#ifdef ICMP6_DST_UNREACH_BEYONDSCOPE
		case ICMP6_DST_UNREACH_BEYONDSCOPE:
#else
		case ICMP6_DST_UNREACH_NOTNEIGHBOR:
#endif
			printf("icmp6: %s beyond scope of source address %s",
			       ip6addr_string(&oip->ip6_dst),
			       ip6addr_string(&oip->ip6_src));
			break;
		case ICMP6_DST_UNREACH_ADDR:
			printf("icmp6: %s unreachable address",
			       ip6addr_string(&oip->ip6_dst));
			break;
		case ICMP6_DST_UNREACH_NOPORT:
			TCHECK(oip->ip6_nxt);
			hlen = sizeof(struct ip6_hdr);
			ouh = (struct udphdr *)(((u_char *)oip) + hlen);
			TCHECK(ouh->uh_dport);
			dport = ntohs(ouh->uh_dport);
			switch (oip->ip6_nxt) {
			case IPPROTO_TCP:
				printf("icmp6: %s tcp port %s unreachable",
					ip6addr_string(&oip->ip6_dst),
					tcpport_string(dport));
				break;
			case IPPROTO_UDP:
				printf("icmp6: %s udp port %s unreachable",
					ip6addr_string(&oip->ip6_dst),
					udpport_string(dport));
				break;
			default:
				printf("icmp6: %s protocol %d port %d unreachable",
					ip6addr_string(&oip->ip6_dst),
					oip->ip6_nxt, dport);
				break;
			}
			break;
		default:
			printf("icmp6: %s unreachable code-#%d",
				ip6addr_string(&oip->ip6_dst),
				dp->icmp6_code);
			break;
		}
		break;
	case ICMP6_PACKET_TOO_BIG:
		TCHECK(dp->icmp6_mtu);
		printf("icmp6: too big %u", (u_int32_t)ntohl(dp->icmp6_mtu));
		break;
	case ICMP6_TIME_EXCEEDED:
		TCHECK(oip->ip6_dst);
		switch (dp->icmp6_code) {
		case ICMP6_TIME_EXCEED_TRANSIT:
			printf("icmp6: time exceeded in-transit for %s",
				ip6addr_string(&oip->ip6_dst));
			break;
		case ICMP6_TIME_EXCEED_REASSEMBLY:
			printf("icmp6: ip6 reassembly time exceeded");
			break;
		default:
			printf("icmp6: time exceeded code-#%d",
				dp->icmp6_code);
			break;
		}
		break;
	case ICMP6_PARAM_PROB:
		TCHECK(oip->ip6_dst);
		switch (dp->icmp6_code) {
		case ICMP6_PARAMPROB_HEADER:
			printf("icmp6: parameter problem errorneous - octet %u",
				(u_int32_t)ntohl(dp->icmp6_pptr));
			break;
		case ICMP6_PARAMPROB_NEXTHEADER:
			printf("icmp6: parameter problem next header - octet %u",
				(u_int32_t)ntohl(dp->icmp6_pptr));
			break;
		case ICMP6_PARAMPROB_OPTION:
			printf("icmp6: parameter problem option - octet %u",
				(u_int32_t)ntohl(dp->icmp6_pptr));
			break;
		default:
			printf("icmp6: parameter problem code-#%d",
			       dp->icmp6_code);
			break;
		}
		break;
	case ICMP6_ECHO_REQUEST:
		printf("icmp6: echo request");
		break;
	case ICMP6_ECHO_REPLY:
		printf("icmp6: echo reply");
		break;
	case ICMP6_MEMBERSHIP_QUERY:
		printf("icmp6: multicast listener query ");
		if (length == MLD_V1_QUERY_MINLEN) {
			mld6_print((const u_char *)dp);
		} else if (length >= MLD_V2_QUERY_MINLEN) {
			printf("v2 ");
			mldv2_query_print((const u_char *)dp, length);
		} else {
			printf("unknown-version (len %u) ", length);
		}
		break;
	case ICMP6_MEMBERSHIP_REPORT:
		printf("icmp6: multicast listener report ");
		mld6_print((const u_char *)dp);
		break;
	case ICMP6_MEMBERSHIP_REDUCTION:
		printf("icmp6: multicast listener done ");
		mld6_print((const u_char *)dp);
		break;
	case ND_ROUTER_SOLICIT:
		printf("icmp6: router solicitation ");
		if (vflag) {
#define RTSOLLEN 8
		        icmp6_opt_print((const u_char *)dp + RTSOLLEN,
					icmp6len - RTSOLLEN);
		}
		break;
	case ND_ROUTER_ADVERT:
		printf("icmp6: router advertisement");
		if (vflag) {
			struct nd_router_advert *p;

			p = (struct nd_router_advert *)dp;
			TCHECK(p->nd_ra_retransmit);
			printf("(chlim=%d, ", (int)p->nd_ra_curhoplimit);
			if (p->nd_ra_flags_reserved & ND_RA_FLAG_MANAGED)
				printf("M");
			if (p->nd_ra_flags_reserved & ND_RA_FLAG_OTHER)
				printf("O");
			if (p->nd_ra_flags_reserved != 0)
				printf(" ");
			printf("router_ltime=%d, ", ntohs(p->nd_ra_router_lifetime));
			printf("reachable_time=%u, ",
				(u_int32_t)ntohl(p->nd_ra_reachable));
			printf("retrans_time=%u)",
				(u_int32_t)ntohl(p->nd_ra_retransmit));
#define RTADVLEN 16
		        icmp6_opt_print((const u_char *)dp + RTADVLEN,
					icmp6len - RTADVLEN);
		}
		break;
	case ND_NEIGHBOR_SOLICIT:
	    {
		struct nd_neighbor_solicit *p;
		p = (struct nd_neighbor_solicit *)dp;
		TCHECK(p->nd_ns_target);
		printf("icmp6: neighbor sol: who has %s",
			ip6addr_string(&p->nd_ns_target));
		if (vflag) {
#define NDSOLLEN 24
		        icmp6_opt_print((const u_char *)dp + NDSOLLEN,
					icmp6len - NDSOLLEN);
		}
	    }
		break;
	case ND_NEIGHBOR_ADVERT:
	    {
		struct nd_neighbor_advert *p;

		p = (struct nd_neighbor_advert *)dp;
		TCHECK(p->nd_na_target);
		printf("icmp6: neighbor adv: tgt is %s",
			ip6addr_string(&p->nd_na_target));
                if (vflag) {
#define ND_NA_FLAG_ALL	\
	(ND_NA_FLAG_ROUTER|ND_NA_FLAG_SOLICITED|ND_NA_FLAG_OVERRIDE)
			/* we don't need ntohl() here.  see advanced-api-04. */
			if (p->nd_na_flags_reserved &  ND_NA_FLAG_ALL) {
#undef ND_NA_FLAG_ALL
				u_int32_t flags;

				flags = p->nd_na_flags_reserved;
				printf("(");
				if (flags & ND_NA_FLAG_ROUTER)
					printf("R");
				if (flags & ND_NA_FLAG_SOLICITED)
					printf("S");
				if (flags & ND_NA_FLAG_OVERRIDE)
					printf("O");
				printf(")");
			}
#define NDADVLEN 24
		        icmp6_opt_print((const u_char *)dp + NDADVLEN,
					icmp6len - NDADVLEN);
		}
	    }
		break;
	case ND_REDIRECT:
	{
#define RDR(i) ((struct nd_redirect *)(i))
		char tgtbuf[INET6_ADDRSTRLEN], dstbuf[INET6_ADDRSTRLEN];

		TCHECK(RDR(dp)->nd_rd_dst);
		inet_ntop(AF_INET6, &RDR(dp)->nd_rd_target,
			  tgtbuf, INET6_ADDRSTRLEN);
		inet_ntop(AF_INET6, &RDR(dp)->nd_rd_dst,
			  dstbuf, INET6_ADDRSTRLEN);
		printf("icmp6: redirect %s to %s", dstbuf, tgtbuf);
#define REDIRECTLEN 40
		if (vflag) {
			icmp6_opt_print((const u_char *)dp + REDIRECTLEN,
					icmp6len - REDIRECTLEN);
		}
		break;
	}
	case ICMP6_ROUTER_RENUMBERING:
		switch (dp->icmp6_code) {
		case ICMP6_ROUTER_RENUMBERING_COMMAND:
			printf("icmp6: router renum command");
			break;
		case ICMP6_ROUTER_RENUMBERING_RESULT:
			printf("icmp6: router renum result");
			break;
		default:
			printf("icmp6: router renum code-#%d", dp->icmp6_code);
			break;
		}
		break;
#ifdef ICMP6_WRUREQUEST
	case ICMP6_WRUREQUEST:	/*ICMP6_FQDN_QUERY*/
	    {
		int siz;
		siz = ep - (u_char *)(dp + 1);
		if (siz == 4)
			printf("icmp6: who-are-you request");
		else {
			printf("icmp6: FQDN request");
			if (vflag) {
				if (siz < 8)
					printf("?(icmp6_data %d bytes)", siz);
				else if (8 < siz)
					printf("?(extra %d bytes)", siz - 8);
			}
		}
		break;
	    }
#endif /*ICMP6_WRUREQUEST*/
#ifdef ICMP6_WRUREPLY
	case ICMP6_WRUREPLY:	/*ICMP6_FQDN_REPLY*/
	    {
		enum { UNKNOWN, WRU, FQDN } mode = UNKNOWN;
		u_char const *buf;
		u_char const *cp = NULL;

		buf = (u_char *)(dp + 1);

		/* fair guess */
		if (buf[12] == ep - buf - 13)
			mode = FQDN;
		else if (dp->icmp6_code == 1)
			mode = FQDN;

		/* wild guess */
		if (mode == UNKNOWN) {
			cp = buf + 4;
			while (cp < ep) {
				if (!isprint(*cp++))
					mode = FQDN;
			}
		}
#ifndef abs
#define abs(a)	((0 < (a)) ? (a) : -(a))
#endif
		if (mode == UNKNOWN && 2 < abs(buf[12] - (ep - buf - 13)))
			mode = WRU;
		if (mode == UNKNOWN)
			mode = FQDN;

		if (mode == WRU) {
			cp = buf + 4;
			printf("icmp6: who-are-you reply(\"");
		} else if (mode == FQDN) {
			cp = buf + 13;
			printf("icmp6: FQDN reply(\"");
		}
		for (; cp < ep; cp++)
			printf((isprint(*cp) ? "%c" : "\\%03o"), *cp);
		printf("\"");
		if (vflag) {
			printf(",%s", mode == FQDN ? "FQDN" : "WRU");
			if (mode == FQDN) {
				int ttl;
				ttl = (int)ntohl(*(u_int32_t *)&buf[8]);
				if (dp->icmp6_code == 1)
					printf(",TTL=unknown");
				else if (ttl < 0)
					printf(",TTL=%d:invalid", ttl);
				else
					printf(",TTL=%d", ttl);
				if (buf[12] != ep - buf - 13) {
					(void)printf(",invalid namelen:%d/%u",
						buf[12],
						(unsigned int)(ep - buf - 13));
				}
			}
		}
		printf(")");
		break;
	    }
#endif /*ICMP6_WRUREPLY*/
	case MLDV2_LISTENER_REPORT:
		printf("multicast listener report v2");
		mldv2_report_print((const u_char *) dp, length);
		break;
	default:
		printf("icmp6: type-#%d", dp->icmp6_type);
		break;
	}
	return;
trunc:
	fputs("[|icmp6]", stdout);
#if 0
#undef TCHECK
#endif
}

void
icmp6_opt_print(register const u_char *bp, int resid)
{
	register const struct nd_opt_hdr *op;
	register const struct nd_opt_hdr *opl;	/* why there's no struct? */
	register const struct nd_opt_prefix_info *opp;
	register const struct icmp6_opts_redirect *opr;
	register const struct nd_opt_mtu *opm;
	register const u_char *ep;
	int	opts_len;
#if 0
	register const struct ip6_hdr *ip;
	register const char *str;
	register const struct ip6_hdr *oip;
	register const struct udphdr *ouh;
	register int hlen, dport;
	char buf[256];
#endif

#if 0
#define TCHECK(var) if ((u_char *)&(var) > ep - sizeof(var)) goto trunc
#endif
#define ECHECK(var) if ((u_char *)&(var) > ep - sizeof(var)) return

	op = (struct nd_opt_hdr *)bp;
#if 0
	ip = (struct ip6_hdr *)bp2;
	oip = &dp->icmp6_ip6;
	str = buf;
#endif
	/* 'ep' points to the end of avaible data. */
	ep = snapend;

	ECHECK(op->nd_opt_len);
	if (resid <= 0)
		return;
	if (op->nd_opt_len == 0)
		goto trunc;
	if (bp + (op->nd_opt_len << 3) > ep)
		goto trunc;
	switch (op->nd_opt_type) {
	case ND_OPT_SOURCE_LINKADDR:
		opl = (struct nd_opt_hdr *)op;
#if 1
		if ((u_char *)opl + (opl->nd_opt_len << 3) > ep)
			goto trunc;
#else
		TCHECK((u_char *)opl + (opl->nd_opt_len << 3) - 1);
#endif
		printf("(src lladdr: %s",
			etheraddr_string((u_char *)(opl + 1)));
		if (opl->nd_opt_len != 1)
			printf("!");
		printf(")");
		icmp6_opt_print((const u_char *)op + (op->nd_opt_len << 3),
				resid - (op->nd_opt_len << 3));
		break;
	case ND_OPT_TARGET_LINKADDR:
		opl = (struct nd_opt_hdr *)op;
#if 1
		if ((u_char *)opl + (opl->nd_opt_len << 3) > ep)
			goto trunc;
#else
		TCHECK((u_char *)opl + (opl->nd_opt_len << 3) - 1);
#endif
		printf("(tgt lladdr: %s",
			etheraddr_string((u_char *)(opl + 1)));
		if (opl->nd_opt_len != 1)
			printf("!");
		printf(")");
		icmp6_opt_print((const u_char *)op + (op->nd_opt_len << 3),
				resid - (op->nd_opt_len << 3));
		break;
	case ND_OPT_PREFIX_INFORMATION:
		opp = (struct nd_opt_prefix_info *)op;
		TCHECK(opp->nd_opt_pi_prefix);
		printf("(prefix info: ");
		if (opp->nd_opt_pi_flags_reserved & ND_OPT_PI_FLAG_ONLINK)
		       printf("L");
		if (opp->nd_opt_pi_flags_reserved & ND_OPT_PI_FLAG_AUTO)
		       printf("A");
		if (opp->nd_opt_pi_flags_reserved)
			printf(" ");
		printf("valid_ltime=");
		if ((u_int32_t)ntohl(opp->nd_opt_pi_valid_time) == ~0U)
			printf("infinity");
		else {
			printf("%u", (u_int32_t)ntohl(opp->nd_opt_pi_valid_time));
		}
		printf(", ");
		printf("preferred_ltime=");
		if ((u_int32_t)ntohl(opp->nd_opt_pi_preferred_time) == ~0U)
			printf("infinity");
		else {
			printf("%u", (u_int32_t)ntohl(opp->nd_opt_pi_preferred_time));
		}
		printf(", ");
		printf("prefix=%s/%d", ip6addr_string(&opp->nd_opt_pi_prefix),
			opp->nd_opt_pi_prefix_len);
		if (opp->nd_opt_pi_len != 4)
			printf("!");
		printf(")");
		icmp6_opt_print((const u_char *)op + (op->nd_opt_len << 3),
				resid - (op->nd_opt_len << 3));
		break;
	case ND_OPT_REDIRECTED_HEADER:
		opr = (struct icmp6_opts_redirect *)op;
		printf("(redirect)");
		/* xxx */
		icmp6_opt_print((const u_char *)op + (op->nd_opt_len << 3),
				resid - (op->nd_opt_len << 3));
		break;
	case ND_OPT_MTU:
		opm = (struct nd_opt_mtu *)op;
		TCHECK(opm->nd_opt_mtu_mtu);
		printf("(mtu: ");
		printf("mtu=%u", (u_int32_t)ntohl(opm->nd_opt_mtu_mtu));
		if (opm->nd_opt_mtu_len != 1)
			printf("!");
		printf(")");
		icmp6_opt_print((const u_char *)op + (op->nd_opt_len << 3),
				resid - (op->nd_opt_len << 3));
		break;
	default:
		opts_len = op->nd_opt_len;
		printf("(unknown opt_type=%d, opt_len=%d)",
		       op->nd_opt_type, opts_len);
		if (opts_len == 0)
			opts_len = 1; /* XXX */
		icmp6_opt_print((const u_char *)op + (opts_len << 3),
				resid - (opts_len << 3));
		break;
	}
	return;
 trunc:
	fputs("[ndp opt]", stdout);
	return;
#if 0
#undef TCHECK
#endif
#undef ECHECK
}

void
mld6_print(register const u_char *bp)
{
	register struct mld6_hdr *mp = (struct mld6_hdr *)bp;
	register const u_char *ep;

	/* 'ep' points to the end of avaible data. */
	ep = snapend;

	if ((u_char *)mp + sizeof(*mp) > ep)
		return;

	printf("max resp delay: %d ", ntohs(mp->mld6_maxdelay));
	printf("addr: %s", ip6addr_string(&mp->mld6_addr));

	return;
}

void
mldv2_report_print(const u_char *bp, u_int len)
{
	struct icmp6_hdr *icp = (struct icmp6_hdr *) bp;
	u_int group, nsrcs, ngroups;
	u_int i, j;

	if (len < MLDV2_REPORT_MINLEN) {
		printf(" [invalid len %d]", len);
		return;
	}

	TCHECK(icp->icmp6_data16[1]);
	ngroups = ntohs(icp->icmp6_data16[1]);
	printf(", %d group record(s)", ngroups);
	if (vflag > 0) {
		/* Print the group records */
		group = MLDV2_REPORT_GROUP0;
		for (i = 0; i < ngroups; i++) {
			/* type(1) + auxlen(1) + numsrc(2) + grp(16) */
			if (len < group + MLDV2_REPORT_MINGRPLEN) {
				printf(" [invalid number of groups]");
				return;
			}
			TCHECK2(bp[group + MLDV2_RGROUP_MADDR],
			    sizeof(struct in6_addr));
			printf(" [gaddr %s",
			    ip6addr_string(&bp[group + MLDV2_RGROUP_MADDR]));
			printf(" %s", tok2str(mldv2report2str,
			    " [v2-report-#%d]", bp[group]));
			nsrcs = (bp[group + MLDV2_RGROUP_NSRCS] << 8) +
			    bp[group + MLDV2_RGROUP_NSRCS + 1];
			/* Check the number of sources and print them */
			if (len < group + MLDV2_REPORT_MINGRPLEN +
				    (nsrcs * sizeof(struct in6_addr))) {
				printf(" [invalid number of sources %d]", nsrcs);
				return;
			}
			if (vflag == 1)
				printf(", %d source(s)", nsrcs);
			else {
				/* Print the sources */
				(void)printf(" {");
				for (j = 0; j < nsrcs; j++) {
					TCHECK2(bp[group +
					    MLDV2_REPORT_MINGRPLEN +
					    j * sizeof(struct in6_addr)],
					    sizeof(struct in6_addr));
					printf(" %s", ip6addr_string(&bp[group +
					    MLDV2_REPORT_MINGRPLEN + j *
					    sizeof(struct in6_addr)]));
				}
				(void)printf(" }");
			}
			/* Next group record */
			group += MLDV2_REPORT_MINGRPLEN + nsrcs *
			    sizeof(struct in6_addr);
			printf("]");
		}
	}
	return;
trunc:
	(void)printf("[|icmp6]");
	return;
}

void
mldv2_query_print(const u_char *bp, u_int len)
{
	struct icmp6_hdr *icp = (struct icmp6_hdr *) bp;
	u_int mrc, qqic;
	int mrd, qqi;
	int mant, exp;
	u_int nsrcs;
	u_int i;

	if (len < MLD_V2_QUERY_MINLEN) {
		printf(" [invalid len %d]", len);
		return;
	}
	TCHECK(icp->icmp6_data16[0]);
	mrc = ntohs(icp->icmp6_data16[0]);
	if (mrc & MLDV2_MRC_FLOAT) {
		mant = MLD_MRC_MANT(mrc);
		exp = MLD_MRC_EXP(mrc);
		mrd = MLDV2_MRD(mant, exp);
	} else {
		mrd = mrc;
	}
	if (vflag) {
		(void)printf(" [max resp delay=%d]", mrd);
	} 
	TCHECK2(bp[8], sizeof(struct in6_addr));
	printf(" [gaddr %s", ip6addr_string(&bp[8]));

	if (vflag) {
		TCHECK(bp[MLDV2_QUERY_QQIC]);
		if (bp[MLDV2_QUERY_QRV] & MLDV2_QUERY_QRV_SFLAG) {
			printf(" sflag");
		}
		if (MLD_QRV(bp[MLDV2_QUERY_QRV])) {
			printf(" robustness=%d", MLD_QRV(bp[MLDV2_QUERY_QRV]));
		}
		qqic = bp[MLDV2_QUERY_QQIC];
		if (qqic & MLDV2_QQIC_FLOAT) {
			mant = MLD_QQIC_MANT(qqic);
			exp = MLD_QQIC_EXP(qqic);
			qqi = MLDV2_QQI(mant, exp);
		} else {
			qqi = bp[MLDV2_QUERY_QQIC];
		}
		printf(" qqi=%d", qqi);
	}

	TCHECK2(bp[MLDV2_QUERY_NSRCS], 2);
	nsrcs = ntohs(*(u_short *)&bp[MLDV2_QUERY_NSRCS]);
	if (nsrcs > 0) {
		if (len < MLD_V2_QUERY_MINLEN + nsrcs * sizeof(struct in6_addr))
			printf(" [invalid number of sources]");
		else if (vflag > 1) {
			printf(" {");
			for (i = 0; i < nsrcs; i++) {
				TCHECK2(bp[MLDV2_QUERY_SRC0 + i *
				    sizeof(struct in6_addr)],
				    sizeof(struct in6_addr));
				printf(" %s",
				    ip6addr_string(&bp[MLDV2_QUERY_SRC0 + i *
				    sizeof(struct in6_addr)]));
			}
			printf(" }");
		} else
			printf(", %d source(s)", nsrcs);
	}
	printf("]");
	return;
trunc:
	(void)printf("[|icmp6]");
	return;
}


#endif /* INET6 */
