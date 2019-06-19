/*	$OpenBSD: inet6.c,v 1.53 2019/04/20 11:36:19 bluhm Exp $	*/
/*	BSDI inet.c,v 2.3 1995/10/24 02:19:29 prb Exp	*/
/*
 * Copyright (c) 1983, 1988, 1993
 *	The Regents of the University of California.  All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. Neither the name of the University nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE REGENTS AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#include <sys/types.h>
#include <sys/socket.h>
#include <sys/socketvar.h>
#include <sys/ioctl.h>
#include <sys/protosw.h>
#include <sys/sysctl.h>

#include <net/route.h>
#include <net/if.h>
#include <netinet/in.h>
#include <netinet/ip6.h>
#include <netinet/icmp6.h>
#include <netinet/ip.h>
#include <netinet/ip_var.h>
#include <netinet6/ip6_var.h>
#include <netinet6/in6_var.h>
#include <netinet6/raw_ip6.h>
#include <netinet6/ip6_divert.h>

#include <arpa/inet.h>
#include <netdb.h>

#include <err.h>
#include <errno.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <limits.h>
#include "netstat.h"

struct	socket sockb;

char	*inet6name(struct in6_addr *);

static	char *ip6nh[] = {
	"hop by hop",
	"ICMP",
	"IGMP",
	"#3",
	"IP",
	"#5",
	"TCP",
	"#7",
	"#8",
	"#9",
	"#10",
	"#11",
	"#12",
	"#13",
	"#14",
	"#15",
	"#16",
	"UDP",
	"#18",
	"#19",
	"#20",
	"#21",
	"IDP",
	"#23",
	"#24",
	"#25",
	"#26",
	"#27",
	"#28",
	"TP",
	"#30",
	"#31",
	"#32",
	"#33",
	"#34",
	"#35",
	"#36",
	"#37",
	"#38",
	"#39",
	"#40",
	"IP6",
	"#42",
	"routing",
	"fragment",
	"#45",
	"#46",
	"#47",
	"#48",
	"#49",
	"ESP",
	"AH",
	"#52",
	"#53",
	"#54",
	"#55",
	"#56",
	"#57",
	"ICMP6",
	"no next header",
	"destination option",
	"#61",
	"#62",
	"#63",
	"#64",
	"#65",
	"#66",
	"#67",
	"#68",
	"#69",
	"#70",
	"#71",
	"#72",
	"#73",
	"#74",
	"#75",
	"#76",
	"#77",
	"#78",
	"#79",
	"ISOIP",
	"#81",
	"#82",
	"#83",
	"#84",
	"#85",
	"#86",
	"#87",
	"#88",
	"OSPF",
	"#80",
	"#91",
	"#92",
	"#93",
	"#94",
	"#95",
	"#96",
	"Ethernet",
	"#98",
	"#99",
	"#100",
	"#101",
	"#102",
	"#103",
	"#104",
	"#105",
	"#106",
	"#107",
	"#108",
	"#109",
	"#110",
	"#111",
	"#112",
	"#113",
	"#114",
	"#115",
	"#116",
	"#117",
	"#118",
	"#119",
	"#120",
	"#121",
	"#122",
	"#123",
	"#124",
	"#125",
	"#126",
	"#127",
	"#128",
	"#129",
	"#130",
	"#131",
	"#132",
	"#133",
	"#134",
	"#135",
	"#136",
	"#137",
	"#138",
	"#139",
	"#140",
	"#141",
	"#142",
	"#143",
	"#144",
	"#145",
	"#146",
	"#147",
	"#148",
	"#149",
	"#150",
	"#151",
	"#152",
	"#153",
	"#154",
	"#155",
	"#156",
	"#157",
	"#158",
	"#159",
	"#160",
	"#161",
	"#162",
	"#163",
	"#164",
	"#165",
	"#166",
	"#167",
	"#168",
	"#169",
	"#170",
	"#171",
	"#172",
	"#173",
	"#174",
	"#175",
	"#176",
	"#177",
	"#178",
	"#179",
	"#180",
	"#181",
	"#182",
	"#183",
	"#184",
	"#185",
	"#186",
	"#187",
	"#188",
	"#189",
	"#180",
	"#191",
	"#192",
	"#193",
	"#194",
	"#195",
	"#196",
	"#197",
	"#198",
	"#199",
	"#200",
	"#201",
	"#202",
	"#203",
	"#204",
	"#205",
	"#206",
	"#207",
	"#208",
	"#209",
	"#210",
	"#211",
	"#212",
	"#213",
	"#214",
	"#215",
	"#216",
	"#217",
	"#218",
	"#219",
	"#220",
	"#221",
	"#222",
	"#223",
	"#224",
	"#225",
	"#226",
	"#227",
	"#228",
	"#229",
	"#230",
	"#231",
	"#232",
	"#233",
	"#234",
	"#235",
	"#236",
	"#237",
	"#238",
	"#239",
	"#240",
	"#241",
	"#242",
	"#243",
	"#244",
	"#245",
	"#246",
	"#247",
	"#248",
	"#249",
	"#250",
	"#251",
	"#252",
	"#253",
	"#254",
	"#255",
};

/*
 * Dump IP6 statistics structure.
 */
void
ip6_stats(char *name)
{
	struct ip6stat ip6stat;
	int first, i;
	struct protoent *ep;
	const char *n;
	int mib[] = { CTL_NET, PF_INET6, IPPROTO_IPV6, IPV6CTL_STATS };
	size_t len = sizeof(ip6stat);

	if (sysctl(mib, sizeof(mib) / sizeof(mib[0]),
	    &ip6stat, &len, NULL, 0) == -1) {
		if (errno != ENOPROTOOPT)
			warn("%s", name);
		return;
	}

	printf("%s:\n", name);
#define	p(f, m) if (ip6stat.f || sflag <= 1) \
	printf(m, (unsigned long long)ip6stat.f, plural(ip6stat.f))
#define	p1(f, m) if (ip6stat.f || sflag <= 1) \
	printf(m, (unsigned long long)ip6stat.f)

	p(ip6s_total, "\t%llu total packet%s received\n");
	p1(ip6s_toosmall, "\t%llu with size smaller than minimum\n");
	p1(ip6s_tooshort, "\t%llu with data size < data length\n");
	p1(ip6s_badoptions, "\t%llu with bad options\n");
	p1(ip6s_badvers, "\t%llu with incorrect version number\n");
	p(ip6s_fragments, "\t%llu fragment%s received\n");
	p(ip6s_fragdropped,
	    "\t%llu fragment%s dropped (duplicates or out of space)\n");
	p(ip6s_fragtimeout, "\t%llu fragment%s dropped after timeout\n");
	p(ip6s_fragoverflow, "\t%llu fragment%s that exceeded limit\n");
	p(ip6s_reassembled, "\t%llu packet%s reassembled ok\n");
	p(ip6s_delivered, "\t%llu packet%s for this host\n");
	p(ip6s_forward, "\t%llu packet%s forwarded\n");
	p(ip6s_cantforward, "\t%llu packet%s not forwardable\n");
	p(ip6s_redirectsent, "\t%llu redirect%s sent\n");
	p(ip6s_localout, "\t%llu packet%s sent from this host\n");
	p(ip6s_rawout, "\t%llu packet%s sent with fabricated ip header\n");
	p(ip6s_odropped,
	    "\t%llu output packet%s dropped due to no bufs, etc.\n");
	p(ip6s_noroute, "\t%llu output packet%s discarded due to no route\n");
	p(ip6s_fragmented, "\t%llu output datagram%s fragmented\n");
	p(ip6s_ofragments, "\t%llu fragment%s created\n");
	p(ip6s_cantfrag, "\t%llu datagram%s that can't be fragmented\n");
	p(ip6s_badscope, "\t%llu packet%s that violated scope rules\n");
	p(ip6s_notmember, "\t%llu multicast packet%s which we don't join\n");
	for (first = 1, i = 0; i < 256; i++)
		if (ip6stat.ip6s_nxthist[i] != 0) {
			if (first) {
				printf("\tInput packet histogram:\n");
				first = 0;
			}
			n = NULL;
			if (ip6nh[i])
				n = ip6nh[i];
			else if ((ep = getprotobynumber(i)) != NULL)
				n = ep->p_name;
			if (n)
				printf("\t\t%s: %llu\n", n,
				    (unsigned long long)ip6stat.ip6s_nxthist[i]);
			else
				printf("\t\t#%d: %llu\n", i,
				    (unsigned long long)ip6stat.ip6s_nxthist[i]);
		}
	printf("\tMbuf statistics:\n");
	p(ip6s_m1, "\t\t%llu one mbuf%s\n");
	for (first = 1, i = 0; i < 32; i++) {
		char ifbuf[IFNAMSIZ];
		if (ip6stat.ip6s_m2m[i] != 0) {
			if (first) {
				printf("\t\ttwo or more mbuf:\n");
				first = 0;
			}
			printf("\t\t\t%s = %llu\n",
			    if_indextoname(i, ifbuf),
			    (unsigned long long)ip6stat.ip6s_m2m[i]);
		}
	}
	p(ip6s_mext1, "\t\t%llu one ext mbuf%s\n");
	p(ip6s_mext2m, "\t\t%llu two or more ext mbuf%s\n");
	p(ip6s_nogif, "\t%llu tunneling packet%s that can't find gif\n");
	p(ip6s_toomanyhdr,
	    "\t%llu packet%s discarded due to too many headers\n");

	/* for debugging source address selection */
#define PRINT_SCOPESTAT(s,i) do {\
		switch(i) { /* XXX hardcoding in each case */\
		case 1:\
			p(s, "\t\t%llu node-local%s\n");\
			break;\
		case 2:\
			p(s, "\t\t%llu link-local%s\n");\
			break;\
		case 5:\
			p(s, "\t\t%llu site-local%s\n");\
			break;\
		case 14:\
			p(s, "\t\t%llu global%s\n");\
			break;\
		default:\
			printf("\t\t%llu addresses scope=%x\n",\
			    (unsigned long long)ip6stat.s, i);\
		}\
	} while(0);

	p(ip6s_sources_none,
	    "\t%llu failure%s of source address selection\n");
	for (first = 1, i = 0; i < 16; i++) {
		if (ip6stat.ip6s_sources_sameif[i]) {
			if (first) {
				printf("\tsource addresses on an outgoing I/F\n");
				first = 0;
			}
			PRINT_SCOPESTAT(ip6s_sources_sameif[i], i);
		}
	}
	for (first = 1, i = 0; i < 16; i++) {
		if (ip6stat.ip6s_sources_otherif[i]) {
			if (first) {
				printf("\tsource addresses on a non-outgoing I/F\n");
				first = 0;
			}
			PRINT_SCOPESTAT(ip6s_sources_otherif[i], i);
		}
	}
	for (first = 1, i = 0; i < 16; i++) {
		if (ip6stat.ip6s_sources_samescope[i]) {
			if (first) {
				printf("\tsource addresses of same scope\n");
				first = 0;
			}
			PRINT_SCOPESTAT(ip6s_sources_samescope[i], i);
		}
	}
	for (first = 1, i = 0; i < 16; i++) {
		if (ip6stat.ip6s_sources_otherscope[i]) {
			if (first) {
				printf("\tsource addresses of a different scope\n");
				first = 0;
			}
			PRINT_SCOPESTAT(ip6s_sources_otherscope[i], i);
		}
	}
	for (first = 1, i = 0; i < 16; i++) {
		if (ip6stat.ip6s_sources_deprecated[i]) {
			if (first) {
				printf("\tdeprecated source addresses\n");
				first = 0;
			}
			PRINT_SCOPESTAT(ip6s_sources_deprecated[i], i);
		}
	}

	p1(ip6s_forward_cachehit, "\t%llu forward cache hit\n");
	p1(ip6s_forward_cachemiss, "\t%llu forward cache miss\n");
#undef p
#undef p1
}

static	char *icmp6names[] = {
	"#0",
	"unreach",
	"packet too big",
	"time exceed",
	"parameter problem",
	"#5",
	"#6",
	"#7",
	"#8",
	"#9",
	"#10",
	"#11",
	"#12",
	"#13",
	"#14",
	"#15",
	"#16",
	"#17",
	"#18",
	"#19",
	"#20",
	"#21",
	"#22",
	"#23",
	"#24",
	"#25",
	"#26",
	"#27",
	"#28",
	"#29",
	"#30",
	"#31",
	"#32",
	"#33",
	"#34",
	"#35",
	"#36",
	"#37",
	"#38",
	"#39",
	"#40",
	"#41",
	"#42",
	"#43",
	"#44",
	"#45",
	"#46",
	"#47",
	"#48",
	"#49",
	"#50",
	"#51",
	"#52",
	"#53",
	"#54",
	"#55",
	"#56",
	"#57",
	"#58",
	"#59",
	"#60",
	"#61",
	"#62",
	"#63",
	"#64",
	"#65",
	"#66",
	"#67",
	"#68",
	"#69",
	"#70",
	"#71",
	"#72",
	"#73",
	"#74",
	"#75",
	"#76",
	"#77",
	"#78",
	"#79",
	"#80",
	"#81",
	"#82",
	"#83",
	"#84",
	"#85",
	"#86",
	"#87",
	"#88",
	"#89",
	"#80",
	"#91",
	"#92",
	"#93",
	"#94",
	"#95",
	"#96",
	"#97",
	"#98",
	"#99",
	"#100",
	"#101",
	"#102",
	"#103",
	"#104",
	"#105",
	"#106",
	"#107",
	"#108",
	"#109",
	"#110",
	"#111",
	"#112",
	"#113",
	"#114",
	"#115",
	"#116",
	"#117",
	"#118",
	"#119",
	"#120",
	"#121",
	"#122",
	"#123",
	"#124",
	"#125",
	"#126",
	"#127",
	"echo",
	"echo reply",
	"multicast listener query",
	"multicast listener report",
	"multicast listener done",
	"router solicitation",
	"router advertisement",
	"neighbor solicitation",
	"neighbor advertisement",
	"redirect",
	"router renumbering",
	"node information request",
	"node information reply",
	"#141",
	"#142",
	"#143",
	"#144",
	"#145",
	"#146",
	"#147",
	"#148",
	"#149",
	"#150",
	"#151",
	"#152",
	"#153",
	"#154",
	"#155",
	"#156",
	"#157",
	"#158",
	"#159",
	"#160",
	"#161",
	"#162",
	"#163",
	"#164",
	"#165",
	"#166",
	"#167",
	"#168",
	"#169",
	"#170",
	"#171",
	"#172",
	"#173",
	"#174",
	"#175",
	"#176",
	"#177",
	"#178",
	"#179",
	"#180",
	"#181",
	"#182",
	"#183",
	"#184",
	"#185",
	"#186",
	"#187",
	"#188",
	"#189",
	"#180",
	"#191",
	"#192",
	"#193",
	"#194",
	"#195",
	"#196",
	"#197",
	"#198",
	"#199",
	"#200",
	"#201",
	"#202",
	"#203",
	"#204",
	"#205",
	"#206",
	"#207",
	"#208",
	"#209",
	"#210",
	"#211",
	"#212",
	"#213",
	"#214",
	"#215",
	"#216",
	"#217",
	"#218",
	"#219",
	"#220",
	"#221",
	"#222",
	"#223",
	"#224",
	"#225",
	"#226",
	"#227",
	"#228",
	"#229",
	"#230",
	"#231",
	"#232",
	"#233",
	"#234",
	"#235",
	"#236",
	"#237",
	"#238",
	"#239",
	"#240",
	"#241",
	"#242",
	"#243",
	"#244",
	"#245",
	"#246",
	"#247",
	"#248",
	"#249",
	"#250",
	"#251",
	"#252",
	"#253",
	"#254",
	"#255",
};

/*
 * Dump ICMPv6 statistics.
 */
void
icmp6_stats(char *name)
{
	struct icmp6stat icmp6stat;
	int i, first;
	int mib[] = { CTL_NET, PF_INET6, IPPROTO_ICMPV6, ICMPV6CTL_STATS };
	size_t len = sizeof(icmp6stat);

	if (sysctl(mib, sizeof(mib) / sizeof(mib[0]),
	    &icmp6stat, &len, NULL, 0) == -1) {
		if (errno != ENOPROTOOPT)
			warn("%s", name);
		return;
	}

	printf("%s:\n", name);
#define	p(f, m) if (icmp6stat.f || sflag <= 1) \
	printf(m, (unsigned long long)icmp6stat.f, plural(icmp6stat.f))
#define p_5(f, m) if (icmp6stat.f || sflag <= 1) \
	printf(m, (unsigned long long)icmp6stat.f)

	p(icp6s_error, "\t%llu call%s to icmp6_error\n");
	p(icp6s_canterror,
	    "\t%llu error%s not generated because old message was icmp6 or so\n");
	p(icp6s_toofreq,
	    "\t%llu error%s not generated because of rate limitation\n");
	for (first = 1, i = 0; i < 256; i++)
		if (icmp6stat.icp6s_outhist[i] != 0) {
			if (first) {
				printf("\tOutput packet histogram:\n");
				first = 0;
			}
			printf("\t\t%s: %llu\n", icmp6names[i],
			    (unsigned long long)icmp6stat.icp6s_outhist[i]);
		}
	p(icp6s_badcode, "\t%llu message%s with bad code fields\n");
	p(icp6s_tooshort, "\t%llu message%s < minimum length\n");
	p(icp6s_checksum, "\t%llu bad checksum%s\n");
	p(icp6s_badlen, "\t%llu message%s with bad length\n");
	for (first = 1, i = 0; i < ICMP6_MAXTYPE; i++)
		if (icmp6stat.icp6s_inhist[i] != 0) {
			if (first) {
				printf("\tInput packet histogram:\n");
				first = 0;
			}
			printf("\t\t%s: %llu\n", icmp6names[i],
			    (unsigned long long)icmp6stat.icp6s_inhist[i]);
		}
	printf("\tHistogram of error messages to be generated:\n");
	p_5(icp6s_odst_unreach_noroute, "\t\t%llu no route\n");
	p_5(icp6s_odst_unreach_admin, "\t\t%llu administratively prohibited\n");
	p_5(icp6s_odst_unreach_beyondscope, "\t\t%llu beyond scope\n");
	p_5(icp6s_odst_unreach_addr, "\t\t%llu address unreachable\n");
	p_5(icp6s_odst_unreach_noport, "\t\t%llu port unreachable\n");
	p_5(icp6s_opacket_too_big, "\t\t%llu packet too big\n");
	p_5(icp6s_otime_exceed_transit, "\t\t%llu time exceed transit\n");
	p_5(icp6s_otime_exceed_reassembly, "\t\t%llu time exceed reassembly\n");
	p_5(icp6s_oparamprob_header, "\t\t%llu erroneous header field\n");
	p_5(icp6s_oparamprob_nextheader, "\t\t%llu unrecognized next header\n");
	p_5(icp6s_oparamprob_option, "\t\t%llu unrecognized option\n");
	p_5(icp6s_oredirect, "\t\t%llu redirect\n");
	p_5(icp6s_ounknown, "\t\t%llu unknown\n");

	p(icp6s_reflect, "\t%llu message response%s generated\n");
	p(icp6s_nd_toomanyopt, "\t%llu message%s with too many ND options\n");
	p(icp6s_nd_badopt, "\t%llu message%s with bad ND options\n");
	p(icp6s_badns, "\t%llu bad neighbor solicitation message%s\n");
	p(icp6s_badna, "\t%llu bad neighbor advertisement message%s\n");
	p(icp6s_badrs, "\t%llu bad router solicitation message%s\n");
	p(icp6s_badra, "\t%llu bad router advertisement message%s\n");
	p(icp6s_badredirect, "\t%llu bad redirect message%s\n");
	p(icp6s_pmtuchg, "\t%llu path MTU change%s\n");
#undef p
#undef p_5
}

/*
 * Dump raw ip6 statistics structure.
 */
void
rip6_stats(char *name)
{
	struct rip6stat rip6stat;
	u_int64_t delivered;
	int mib[] = { CTL_NET, PF_INET6, IPPROTO_RAW, RIPV6CTL_STATS };
	size_t len = sizeof(rip6stat);

	if (sysctl(mib, sizeof(mib) / sizeof(mib[0]),
	    &rip6stat, &len, NULL, 0) == -1) {
		if (errno != ENOPROTOOPT)
			warn("%s", name);
		return;
	}

	printf("%s:\n", name);

#define	p(f, m) if (rip6stat.f || sflag <= 1) \
    printf(m, (unsigned long long)rip6stat.f, plural(rip6stat.f))
	p(rip6s_ipackets, "\t%llu message%s received\n");
	p(rip6s_isum, "\t%llu checksum calculation%s on inbound\n");
	p(rip6s_badsum, "\t%llu message%s with bad checksum\n");
	p(rip6s_nosock, "\t%llu message%s dropped due to no socket\n");
	p(rip6s_nosockmcast,
	    "\t%llu multicast message%s dropped due to no socket\n");
	p(rip6s_fullsock,
	    "\t%llu message%s dropped due to full socket buffers\n");
	delivered = rip6stat.rip6s_ipackets -
		    rip6stat.rip6s_nosock -
		    rip6stat.rip6s_nosockmcast -
		    rip6stat.rip6s_fullsock;
	if (delivered || sflag <= 1)
		printf("\t%llu delivered\n", (unsigned long long)delivered);
	p(rip6s_opackets, "\t%llu datagram%s output\n");
#undef p
}

/*
 * Dump divert6 statistics structure.
 */
void
div6_stats(char *name)
{
	struct div6stat div6stat;
	int mib[] = { CTL_NET, PF_INET6, IPPROTO_DIVERT, DIVERT6CTL_STATS };
	size_t len = sizeof(div6stat);

	if (sysctl(mib, sizeof(mib) / sizeof(mib[0]),
	    &div6stat, &len, NULL, 0) == -1) {
		if (errno != ENOPROTOOPT)
			warn("%s", name);
		return;
	}

	printf("%s:\n", name);
#define p(f, m) if (div6stat.f || sflag <= 1) \
    printf(m, div6stat.f, plural(div6stat.f))
#define p1(f, m) if (div6stat.f || sflag <= 1) \
    printf(m, div6stat.f)
	p(divs_ipackets, "\t%lu total packet%s received\n");
	p1(divs_noport, "\t%lu dropped due to no socket\n");
	p1(divs_fullsock, "\t%lu dropped due to full socket buffers\n");
	p(divs_opackets, "\t%lu packet%s output\n");
	p1(divs_errors, "\t%lu errors\n");
#undef p
#undef p1
}

/*
 * Pretty print an Internet address (net address + port).
 * If the nflag was specified, use numbers instead of names.
 */

void
inet6print(struct in6_addr *in6, int port, const char *proto)
{

#define GETSERVBYPORT6(port, proto, ret) do { \
	if (strcmp((proto), "tcp6") == 0) \
		(ret) = getservbyport((int)(port), "tcp"); \
	else if (strcmp((proto), "udp6") == 0) \
		(ret) = getservbyport((int)(port), "udp"); \
	else \
		(ret) = getservbyport((int)(port), (proto)); \
	} while (0)

	struct servent *sp = 0;
	char line[80], *cp;
	int width;
	int len = sizeof line;

	width = Aflag ? 12 : 16;
	if (vflag && width < strlen(inet6name(in6)))
		width = strlen(inet6name(in6));
	snprintf(line, len, "%.*s.", width, inet6name(in6));
	len -= strlen(line);
	if (len <= 0)
		goto bail;

	cp = strchr(line, '\0');
	if (!nflag && port)
		GETSERVBYPORT6(port, proto, sp);
	if (sp || port == 0)
		snprintf(cp, len, "%.8s", sp ? sp->s_name : "*");
	else
		snprintf(cp, len, "%d", ntohs((u_short)port));
	width = Aflag ? 18 : 22;
	if (vflag && width < strlen(line))
		width = strlen(line);
bail:
	printf(" %-*.*s", width, width, line);
}

/*
 * Construct an Internet address representation.
 * If the nflag has been supplied, give
 * numeric value, otherwise try for symbolic name.
 */

char *
inet6name(struct in6_addr *in6p)
{
	char *cp;
	static char line[NI_MAXHOST];
	struct hostent *hp;
	static char domain[HOST_NAME_MAX+1];
	static int first = 1;
	char hbuf[NI_MAXHOST];
	struct sockaddr_in6 sin6;
	const int niflag = NI_NUMERICHOST;

	if (first && !nflag) {
		first = 0;
		if (gethostname(domain, sizeof(domain)) == 0 &&
		    (cp = strchr(domain, '.')))
			(void) strlcpy(domain, cp + 1, sizeof domain);
		else
			domain[0] = '\0';
	}
	cp = 0;
	if (!nflag && !IN6_IS_ADDR_UNSPECIFIED(in6p)) {
		hp = gethostbyaddr((char *)in6p, sizeof(*in6p), AF_INET6);
		if (hp) {
			if ((cp = strchr(hp->h_name, '.')) &&
			    !strcmp(cp + 1, domain))
				*cp = 0;
			cp = hp->h_name;
		}
	}
	if (IN6_IS_ADDR_UNSPECIFIED(in6p))
		strlcpy(line, "*", sizeof(line));
	else if (cp)
		strlcpy(line, cp, sizeof(line));
	else {
		memset(&sin6, 0, sizeof(sin6));
		sin6.sin6_family = AF_INET6;
		sin6.sin6_addr = *in6p;
#ifdef __KAME__
		if (IN6_IS_ADDR_LINKLOCAL(in6p) ||
		    IN6_IS_ADDR_MC_LINKLOCAL(in6p) ||
		    IN6_IS_ADDR_MC_INTFACELOCAL(in6p)) {
			sin6.sin6_scope_id =
			    ntohs(*(u_int16_t *)&in6p->s6_addr[2]);
			sin6.sin6_addr.s6_addr[2] = 0;
			sin6.sin6_addr.s6_addr[3] = 0;
		}
#endif
		if (getnameinfo((struct sockaddr *)&sin6, sizeof(sin6),
		    hbuf, sizeof(hbuf), NULL, 0, niflag) != 0)
			strlcpy(hbuf, "?", sizeof hbuf);
		strlcpy(line, hbuf, sizeof(line));
	}
	return (line);
}
