/*	$OpenBSD: inet.c,v 1.165 2018/12/18 10:16:24 benno Exp $	*/
/*	$NetBSD: inet.c,v 1.14 1995/10/03 21:42:37 thorpej Exp $	*/

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

#include <sys/queue.h>
#include <sys/select.h>
#include <sys/socket.h>
#include <sys/socketvar.h>
#include <sys/domain.h>
#include <sys/protosw.h>
#include <sys/sysctl.h>
#define _KERNEL
#include <sys/file.h>
#undef _KERNEL

#include <net/route.h>
#include <netinet/in.h>
#include <netinet/ip.h>
#include <netinet/in_pcb.h>
#include <netinet/ip_icmp.h>
#include <netinet/icmp_var.h>
#include <netinet/igmp_var.h>
#include <netinet/ip_var.h>
#include <netinet/tcp.h>
#include <netinet/tcp_seq.h>
#define TCPSTATES
#include <netinet/tcp_fsm.h>
#include <netinet/tcp_timer.h>
#include <netinet/tcp_var.h>
#include <netinet/udp.h>
#include <netinet/udp_var.h>
#include <netinet/ip_ipsp.h>
#include <netinet/ip_ah.h>
#include <netinet/ip_esp.h>
#include <netinet/ip_ipip.h>
#include <netinet/ip_ipcomp.h>
#include <netinet/ip_ether.h>
#include <netinet/ip_carp.h>
#include <netinet/ip_divert.h>
#include <net/if.h>
#include <net/pfvar.h>
#include <net/if_pfsync.h>
#include <net/if_pflow.h>

#include <rpc/rpc.h>
#include <rpc/pmap_prot.h>
#include <rpc/pmap_clnt.h>

#include <arpa/inet.h>
#include <err.h>
#include <limits.h>
#include <netdb.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <stdlib.h>
#include <errno.h>
#include "netstat.h"

struct	inpcb inpcb;
struct	tcpcb tcpcb;
struct	socket sockb;

char	*inetname(struct in_addr *);
void	inetprint(struct in_addr *, in_port_t, const char *, int);
char	*inet6name(struct in6_addr *);
void	sosplice_dump(u_long);
void	sockbuf_dump(struct sockbuf *, const char *);
void	protosw_dump(u_long, u_long);
void	domain_dump(u_long, u_long, short);
void	inpcb_dump(u_long, short, int);
void	tcpcb_dump(u_long);
int	kf_comp(const void *, const void *);

int type_map[] = { -1, 2, 3, 1, 4, 5 };

int
kf_comp(const void *a, const void *b)
{
	const struct kinfo_file *ka = a, *kb = b;

	if (ka->so_family != kb->so_family) {
		/* AF_INET < AF_INET6 < AF_LOCAL */
		if (ka->so_family == AF_INET)
			return (-1);
		if (ka->so_family == AF_LOCAL)
			return (1);
		if (kb->so_family == AF_LOCAL)
			return (-1);
		return (1);
	}
	if (ka->so_family == AF_LOCAL) {
		if (type_map[ka->so_type] < type_map[kb->so_type])
			return (-1);
		if (type_map[ka->so_type] > type_map[kb->so_type])
			return (1);
	} else if (ka->so_family == AF_INET || ka->so_family == AF_INET6) {
		if (ka->so_protocol < kb->so_protocol)
			return (-1);
		if (ka->so_protocol > kb->so_protocol)
			return (1);
		if (ka->so_type == SOCK_DGRAM || ka->so_type == SOCK_STREAM) {
			/* order sockets by remote port desc */
			if (ka->inp_fport > kb->inp_fport)
				return (-1);
			if (ka->inp_fport < kb->inp_fport)
				return (1);
		} else if (ka->so_type == SOCK_RAW) {
			if (ka->inp_proto > kb->inp_proto)
				return (-1);
			if (ka->inp_proto < kb->inp_proto)
				return (1);
		}
	}
	return (0);
}

void
protopr(kvm_t *kvmd, u_long pcbaddr, u_int tableid, int proto)
{
	struct kinfo_file *kf;
	int i, fcnt;

	kf = kvm_getfiles(kvmd, KERN_FILE_BYFILE, DTYPE_SOCKET,
	    sizeof(*kf), &fcnt);
	if (kf == NULL) {
		printf("Out of memory (file table).\n");
		return;
	}
	
	/* sort sockets by AF and type */
	qsort(kf, fcnt, sizeof(*kf), kf_comp);

	for (i = 0; i < fcnt; i++) {
		if (Pflag) {
			switch (kf[i].so_family) {
			case AF_INET:
			case AF_INET6:
				/*
				 * XXX at the moment fstat returns the pointer
				 * to the so_pcb or for tcp sockets the tcpcb
				 * pointer (inp_ppcb) so check both.
				 */
				if (pcbaddr == kf[i].so_pcb) {
					inpcb_dump(kf[i].so_pcb,
					    kf[i].so_protocol,
					    kf[i].so_family);
					return;
				} else if (pcbaddr == kf[i].inp_ppcb &&
				    kf[i].so_protocol == IPPROTO_TCP) {
					if (vflag)
						inpcb_dump(kf[i].so_pcb,
						    kf[i].so_protocol,
						    kf[i].so_family);
					else
						tcpcb_dump(kf[i].inp_ppcb);
					return;
				}
				break;
			case AF_UNIX:
				if (pcbaddr == kf[i].so_pcb) {
					unpcb_dump(pcbaddr);
					return;
				}
				break;
			}
			continue;
		}
		if (kf[i].so_family == AF_LOCAL && (kf[i].so_pcb != 0 ||
		    kf[i].unp_path[0] != '\0'))
			if ((af == AF_LOCAL || af == AF_UNSPEC) && !proto)
				unixdomainpr(&kf[i]);
		if (kf[i].so_family == AF_INET && kf[i].so_pcb != 0 &&
		    kf[i].inp_rtableid == tableid)
			if (af == AF_INET || af == AF_UNSPEC)
				netdomainpr(&kf[i], proto);
		if (kf[i].so_family == AF_INET6 && kf[i].so_pcb != 0 &&
		    kf[i].inp_rtableid == tableid)
			if (af == AF_INET6 || af == AF_UNSPEC)
				netdomainpr(&kf[i], proto);
	}
}

/*
 * Print a summary of connections related to an Internet
 * protocol.  For TCP, also give state of connection.
 * Listening processes (aflag) are suppressed unless the
 * -a (all) flag is specified.
 */
void
netdomainpr(struct kinfo_file *kf, int proto)
{
	static int af = 0, type = 0;
	struct in_addr laddr, faddr;
	struct in6_addr laddr6, faddr6;
	const char *name, *name6;
	int addrlen = 22;
	int isany = 0;
	int istcp = 0;
	int isudp = 0;
	int isip6 = 0;

	/* XXX should fix kinfo_file instead but not now */
	if (kf->so_pcb == -1)
		kf->so_pcb = 0;

	switch (proto) {
	case IPPROTO_TCP:
	case IPPROTO_UDP:
	case IPPROTO_DIVERT:
		if (kf->so_protocol != proto)
			return;
		break;
	case IPPROTO_IPV4:
		if (kf->so_type != SOCK_RAW || kf->so_family != AF_INET)
			return;
		break;
	case IPPROTO_IPV6:
		if (kf->so_type != SOCK_RAW || kf->so_family != AF_INET6)
			return;
		break;
	}

	/* make in_addr6 access a bit easier */
#define s6_addr32 __u6_addr.__u6_addr32
	laddr.s_addr = kf->inp_laddru[0];
	laddr6.s6_addr32[0] = kf->inp_laddru[0];
	laddr6.s6_addr32[1] = kf->inp_laddru[1];
	laddr6.s6_addr32[2] = kf->inp_laddru[2];
	laddr6.s6_addr32[3] = kf->inp_laddru[3];

	faddr.s_addr = kf->inp_faddru[0];
	faddr6.s6_addr32[0] = kf->inp_faddru[0];
	faddr6.s6_addr32[1] = kf->inp_faddru[1];
	faddr6.s6_addr32[2] = kf->inp_faddru[2];
	faddr6.s6_addr32[3] = kf->inp_faddru[3];
#undef s6_addr32

	switch (kf->so_family) {
	case AF_INET:
		isany = faddr.s_addr == INADDR_ANY;
		break;
	case AF_INET6:
		isany = IN6_IS_ADDR_UNSPECIFIED(&faddr6);
		isip6 = 1;
		break;
	}

	switch (kf->so_protocol) {
	case IPPROTO_TCP:
		name = "tcp";
		name6 = "tcp6";
		istcp = 1;
		break;
	case IPPROTO_UDP:
		name = "udp";
		name6 = "udp6";
		isudp = 1;
		break;
	case IPPROTO_DIVERT:
		name = "divert";
		name6 = "divert6";
		break;
	default:
		name = "ip";
		name6 = "ip6";
		break;
	}

	/* filter listening sockets out unless -a is set */
	if (!(aflag || lflag) && istcp && kf->t_state <= TCPS_LISTEN)
		return;
	else if (!(aflag || lflag) && isany)
		return;

	/* when -l is set, show only listening sockets */
	if (!aflag && lflag && istcp &&
	    kf->t_state != TCPS_LISTEN)
		return;
	if (!aflag && lflag && isudp &&
	    (kf->inp_lport == 0 || kf->inp_fport != 0))
		return;

	if (af != kf->so_family || type != kf->so_type) {
		af = kf->so_family;
		type = kf->so_type;
		printf("Active Internet connections");
		if (aflag)
			printf(" (including servers)");
		else if (lflag && (istcp || isudp))
			printf(" (only servers)");
		putchar('\n');
		if (Aflag) {
			addrlen = 18;
			printf("%-*.*s ", PLEN, PLEN, "PCB");
		}
		printf("%-7.7s %-6.6s %-6.6s ",
		    "Proto", "Recv-Q", "Send-Q");
		if (Bflag && istcp)
			printf("%-6.6s %-6.6s %-6.6s ",
			    "Recv-W", "Send-W", "Cgst-W");
		printf(" %-*.*s %-*.*s %s\n",
		    addrlen, addrlen, "Local Address",
		    addrlen, addrlen, "Foreign Address", "(state)");
	}

	if (Aflag)
		printf("%#*llx%s ", FAKE_PTR(kf->so_protocol == IPPROTO_TCP ?
		    kf->inp_ppcb : kf->so_pcb));

	printf("%-7.7s %6llu %6llu ",
	    isip6 ? name6: name, kf->so_rcv_cc, kf->so_snd_cc);
	if (Bflag && istcp)
		printf("%6llu %6llu %6llu ", kf->t_rcv_wnd, kf->t_snd_wnd,
		    (kf->t_state == TCPS_ESTABLISHED) ?
		    kf->t_snd_cwnd : 0);

	if (isip6) {
		inet6print(&laddr6, kf->inp_lport, name);
		inet6print(&faddr6, kf->inp_fport, name);
	} else {
		inetprint(&laddr, kf->inp_lport, name, 1);
		inetprint(&faddr, kf->inp_fport, name, 0);
	}
	if (istcp) {
		if (kf->t_state >= TCP_NSTATES)
			printf(" %u", kf->t_state);
		else
			printf(" %s", tcpstates[kf->t_state]);
	} else if (kf->so_type == SOCK_RAW) {
		printf(" %u", kf->inp_proto);
	}
	putchar('\n');
}

/*
 * Dump TCP statistics structure.
 */
void
tcp_stats(char *name)
{
	struct tcpstat tcpstat;
	int mib[] = { CTL_NET, PF_INET, IPPROTO_TCP, TCPCTL_STATS };
	size_t len = sizeof(tcpstat);

	if (sysctl(mib, sizeof(mib) / sizeof(mib[0]),
	    &tcpstat, &len, NULL, 0) == -1) {
		if (errno != ENOPROTOOPT)
			warn("%s", name);
		return;
	}

	printf("%s:\n", name);
#define	p(f, m) if (tcpstat.f || sflag <= 1) \
	printf(m, tcpstat.f, plural(tcpstat.f))
#define	p1(f, m) if (tcpstat.f || sflag <= 1) \
	printf(m, tcpstat.f)
#define	p2(f1, f2, m) if (tcpstat.f1 || tcpstat.f2 || sflag <= 1) \
	printf(m, tcpstat.f1, plural(tcpstat.f1), tcpstat.f2, plural(tcpstat.f2))
#define	p2a(f1, f2, m) if (tcpstat.f1 || tcpstat.f2 || sflag <= 1) \
	printf(m, tcpstat.f1, plural(tcpstat.f1), tcpstat.f2)
#define	p2b(f1, f2, m) if (tcpstat.f1 || sflag <= 1) \
	printf(m, tcpstat.f1, tcpstat.f2)
#define	p2bys(f1, f2, m) if (tcpstat.f1 || sflag <= 1) \
	printf(m, tcpstat.f1, pluralys(tcpstat.f1), tcpstat.f2)
#define	pes(f, m) if (tcpstat.f || sflag <= 1) \
	printf(m, tcpstat.f, plurales(tcpstat.f))
#define	pys(f, m) if (tcpstat.f || sflag <= 1) \
	printf(m, tcpstat.f, pluralys(tcpstat.f))

	p(tcps_sndtotal, "\t%u packet%s sent\n");
	p2(tcps_sndpack,tcps_sndbyte,
	    "\t\t%u data packet%s (%llu byte%s)\n");
	p2(tcps_sndrexmitpack, tcps_sndrexmitbyte,
	    "\t\t%u data packet%s (%llu byte%s) retransmitted\n");
	p(tcps_sndrexmitfast, "\t\t%llu fast retransmitted packet%s\n");
	p2a(tcps_sndacks, tcps_delack,
	    "\t\t%u ack-only packet%s (%u delayed)\n");
	p(tcps_sndurg, "\t\t%u URG only packet%s\n");
	p(tcps_sndprobe, "\t\t%u window probe packet%s\n");
	p(tcps_sndwinup, "\t\t%u window update packet%s\n");
	p(tcps_sndctrl, "\t\t%u control packet%s\n");
	p(tcps_outswcsum, "\t\t%u packet%s software-checksummed\n");
	p(tcps_rcvtotal, "\t%u packet%s received\n");
	p2(tcps_rcvackpack, tcps_rcvackbyte, "\t\t%u ack%s (for %llu byte%s)\n");
	p(tcps_rcvdupack, "\t\t%u duplicate ack%s\n");
	p(tcps_rcvacktoomuch, "\t\t%u ack%s for unsent data\n");
	p(tcps_rcvacktooold, "\t\t%u ack%s for old data\n");
	p2(tcps_rcvpack, tcps_rcvbyte,
	    "\t\t%u packet%s (%llu byte%s) received in-sequence\n");
	p2(tcps_rcvduppack, tcps_rcvdupbyte,
	    "\t\t%u completely duplicate packet%s (%llu byte%s)\n");
	p(tcps_pawsdrop, "\t\t%u old duplicate packet%s\n");
	p2(tcps_rcvpartduppack, tcps_rcvpartdupbyte,
	    "\t\t%u packet%s with some duplicate data (%llu byte%s duplicated)\n");
	p2(tcps_rcvoopack, tcps_rcvoobyte,
	    "\t\t%u out-of-order packet%s (%llu byte%s)\n");
	p2(tcps_rcvpackafterwin, tcps_rcvbyteafterwin,
	    "\t\t%u packet%s (%llu byte%s) of data after window\n");
	p(tcps_rcvwinprobe, "\t\t%u window probe%s\n");
	p(tcps_rcvwinupd, "\t\t%u window update packet%s\n");
	p(tcps_rcvafterclose, "\t\t%u packet%s received after close\n");
	p(tcps_rcvbadsum, "\t\t%u discarded for bad checksum%s\n");
	p(tcps_rcvbadoff, "\t\t%u discarded for bad header offset field%s\n");
	p1(tcps_rcvshort, "\t\t%u discarded because packet too short\n");
	p1(tcps_rcvnosec, "\t\t%u discarded for missing IPsec protection\n");
	p1(tcps_rcvmemdrop, "\t\t%u discarded due to memory shortage\n");
	p(tcps_inswcsum, "\t\t%u packet%s software-checksummed\n");
	p(tcps_rcvbadsig, "\t\t%u bad/missing md5 checksum%s\n");
	p(tcps_rcvgoodsig, "\t\t%llu good md5 checksum%s\n");
	p(tcps_connattempt, "\t%u connection request%s\n");
	p(tcps_accepts, "\t%u connection accept%s\n");
	p(tcps_connects, "\t%u connection%s established (including accepts)\n");
	p2(tcps_closed, tcps_drops,
	    "\t%u connection%s closed (including %u drop%s)\n");
	p(tcps_conndrained, "\t%llu connection%s drained\n");
	p(tcps_conndrops, "\t%u embryonic connection%s dropped\n");
	p2(tcps_rttupdated, tcps_segstimed,
	    "\t%u segment%s updated rtt (of %u attempt%s)\n");
	p(tcps_rexmttimeo, "\t%u retransmit timeout%s\n");
	p(tcps_timeoutdrop, "\t\t%u connection%s dropped by rexmit timeout\n");
	p(tcps_persisttimeo, "\t%u persist timeout%s\n");
	p(tcps_keeptimeo, "\t%u keepalive timeout%s\n");
	p(tcps_keepprobe, "\t\t%u keepalive probe%s sent\n");
	p(tcps_keepdrops, "\t\t%u connection%s dropped by keepalive\n");
	p(tcps_predack, "\t%u correct ACK header prediction%s\n");
	p(tcps_preddat, "\t%u correct data packet header prediction%s\n");
	pes(tcps_pcbhashmiss, "\t%u PCB cache miss%s\n");
	p1(tcps_noport, "\t%u dropped due to no socket\n");

	p(tcps_ecn_accepts, "\t%u ECN connection%s accepted\n");
	p(tcps_ecn_rcvece, "\t\t%u ECE packet%s received\n");
	p(tcps_ecn_rcvcwr, "\t\t%u CWR packet%s received\n");
	p(tcps_ecn_rcvce, "\t\t%u CE packet%s received\n");
	p(tcps_ecn_sndect, "\t\t%u ECT packet%s sent\n");
	p(tcps_ecn_sndece, "\t\t%u ECE packet%s sent\n");
	p(tcps_ecn_sndcwr, "\t\t%u CWR packet%s sent\n");
	p1(tcps_cwr_frecovery, "\t\t\tcwr by fastrecovery: %u\n");
	p1(tcps_cwr_timeout, "\t\t\tcwr by timeout: %u\n");
	p1(tcps_cwr_ecn, "\t\t\tcwr by ecn: %u\n");

	p(tcps_badsyn, "\t%u bad connection attempt%s\n");
	p(tcps_dropsyn, "\t%u SYN packet%s dropped due to queue or memory full\n");
	pys(tcps_sc_added, "\t%llu SYN cache entr%s added\n");
	p(tcps_sc_collisions, "\t\t%llu hash collision%s\n");
	p1(tcps_sc_completed, "\t\t%llu completed\n");
	p1(tcps_sc_aborted, "\t\t%llu aborted (no space to build PCB)\n");
	p1(tcps_sc_timed_out, "\t\t%llu timed out\n");
	p1(tcps_sc_overflowed, "\t\t%llu dropped due to overflow\n");
	p1(tcps_sc_bucketoverflow, "\t\t%llu dropped due to bucket overflow\n");
	p1(tcps_sc_reset, "\t\t%llu dropped due to RST\n");
	p1(tcps_sc_unreach, "\t\t%llu dropped due to ICMP unreachable\n");
	p(tcps_sc_retransmitted, "\t%llu SYN,ACK%s retransmitted\n");
	p(tcps_sc_dupesyn, "\t%llu duplicate SYN%s received for entries "
	    "already in the cache\n");
	p(tcps_sc_dropped, "\t%llu SYN%s dropped (no route or no space)\n");
	p(tcps_sc_seedrandom, "\t%llu SYN cache seed%s with new random\n");
	p1(tcps_sc_hash_size, "\t%llu hash bucket array size in current "
	    "SYN cache\n");
	p2bys(tcps_sc_entry_count, tcps_sc_entry_limit,
	    "\t%llu entr%s in current SYN cache, limit is %llu\n");
	p2b(tcps_sc_bucket_maxlen, tcps_sc_bucket_limit,
	    "\t%llu longest bucket length in current SYN cache, limit is %llu\n");
	p(tcps_sc_uses_left, "\t%llu use%s of current SYN cache left\n");

	p(tcps_sack_recovery_episode, "\t%llu SACK recovery episode%s\n");
	p(tcps_sack_rexmits,
		"\t\t%llu segment rexmit%s in SACK recovery episodes\n");
	p(tcps_sack_rexmit_bytes,
		"\t\t%llu byte rexmit%s in SACK recovery episodes\n");
	p(tcps_sack_rcv_opts,
		"\t%llu SACK option%s received\n");
	p(tcps_sack_snd_opts, "\t%llu SACK option%s sent\n");

#undef p
#undef p1
#undef p2
#undef p2a
#undef p2b
#undef p2bys
#undef pes
#undef pys
}

/*
 * Dump UDP statistics structure.
 */
void
udp_stats(char *name)
{
	struct udpstat udpstat;
	u_long delivered;
	int mib[] = { CTL_NET, PF_INET, IPPROTO_UDP, UDPCTL_STATS };
	size_t len = sizeof(udpstat);

	if (sysctl(mib, sizeof(mib) / sizeof(mib[0]),
	    &udpstat, &len, NULL, 0) == -1) {
		if (errno != ENOPROTOOPT)
			warn("%s", name);
		return;
	}

	printf("%s:\n", name);
#define	p(f, m) if (udpstat.f || sflag <= 1) \
	printf(m, udpstat.f, plural(udpstat.f))
#define	p1(f, m) if (udpstat.f || sflag <= 1) \
	printf(m, udpstat.f)

	p(udps_ipackets, "\t%lu datagram%s received\n");
	p1(udps_hdrops, "\t%lu with incomplete header\n");
	p1(udps_badlen, "\t%lu with bad data length field\n");
	p1(udps_badsum, "\t%lu with bad checksum\n");
	p1(udps_nosum, "\t%lu with no checksum\n");
	p(udps_inswcsum, "\t%lu input packet%s software-checksummed\n");
	p(udps_outswcsum, "\t%lu output packet%s software-checksummed\n");
	p1(udps_noport, "\t%lu dropped due to no socket\n");
	p(udps_noportbcast, "\t%lu broadcast/multicast datagram%s dropped due to no socket\n");
	p1(udps_nosec, "\t%lu dropped due to missing IPsec protection\n");
	p1(udps_fullsock, "\t%lu dropped due to full socket buffers\n");
	delivered = udpstat.udps_ipackets - udpstat.udps_hdrops -
	    udpstat.udps_badlen - udpstat.udps_badsum -
	    udpstat.udps_noport - udpstat.udps_noportbcast -
	    udpstat.udps_fullsock;
	if (delivered || sflag <= 1)
		printf("\t%lu delivered\n", delivered);
	p(udps_opackets, "\t%lu datagram%s output\n");
	p1(udps_pcbhashmiss, "\t%lu missed PCB cache\n");
#undef p
#undef p1
}

/*
 * Dump IP statistics structure.
 */
void
ip_stats(char *name)
{
	struct ipstat ipstat;
	int mib[] = { CTL_NET, PF_INET, IPPROTO_IP, IPCTL_STATS };
	size_t len = sizeof(ipstat);

	if (sysctl(mib, sizeof(mib) / sizeof(mib[0]),
	    &ipstat, &len, NULL, 0) == -1) {
		if (errno != ENOPROTOOPT)
			warn("%s", name);
		return;
	}

	printf("%s:\n", name);
#define	p(f, m) if (ipstat.f || sflag <= 1) \
	printf(m, ipstat.f, plural(ipstat.f))
#define	p1(f, m) if (ipstat.f || sflag <= 1) \
	printf(m, ipstat.f)

	p(ips_total, "\t%lu total packet%s received\n");
	p(ips_badsum, "\t%lu bad header checksum%s\n");
	p1(ips_toosmall, "\t%lu with size smaller than minimum\n");
	p1(ips_tooshort, "\t%lu with data size < data length\n");
	p1(ips_badhlen, "\t%lu with header length < data size\n");
	p1(ips_badlen, "\t%lu with data length < header length\n");
	p1(ips_badoptions, "\t%lu with bad options\n");
	p1(ips_badvers, "\t%lu with incorrect version number\n");
	p(ips_fragments, "\t%lu fragment%s received\n");
	p(ips_fragdropped, "\t%lu fragment%s dropped (duplicates or out of space)\n");
	p(ips_badfrags, "\t%lu malformed fragment%s dropped\n");
	p(ips_fragtimeout, "\t%lu fragment%s dropped after timeout\n");
	p(ips_reassembled, "\t%lu packet%s reassembled ok\n");
	p(ips_delivered, "\t%lu packet%s for this host\n");
	p(ips_noproto, "\t%lu packet%s for unknown/unsupported protocol\n");
	p(ips_forward, "\t%lu packet%s forwarded\n");
	p(ips_cantforward, "\t%lu packet%s not forwardable\n");
	p(ips_redirectsent, "\t%lu redirect%s sent\n");
	p(ips_localout, "\t%lu packet%s sent from this host\n");
	p(ips_rawout, "\t%lu packet%s sent with fabricated ip header\n");
	p(ips_odropped, "\t%lu output packet%s dropped due to no bufs, etc.\n");
	p(ips_noroute, "\t%lu output packet%s discarded due to no route\n");
	p(ips_fragmented, "\t%lu output datagram%s fragmented\n");
	p(ips_ofragments, "\t%lu fragment%s created\n");
	p(ips_cantfrag, "\t%lu datagram%s that can't be fragmented\n");
	p1(ips_rcvmemdrop, "\t%lu fragment floods\n");
	p(ips_toolong, "\t%lu packet%s with ip length > max ip packet size\n");
	p(ips_nogif, "\t%lu tunneling packet%s that can't find gif\n");
	p(ips_badaddr, "\t%lu datagram%s with bad address in header\n");
	p(ips_inswcsum, "\t%lu input datagram%s software-checksummed\n");
	p(ips_outswcsum, "\t%lu output datagram%s software-checksummed\n");
	p(ips_notmember, "\t%lu multicast packet%s which we don't join\n");
#undef p
#undef p1
}

/*
 * Dump DIVERT statistics structure.
 */
void
div_stats(char *name)
{
	struct divstat divstat;
	int mib[] = { CTL_NET, PF_INET, IPPROTO_DIVERT, DIVERTCTL_STATS };
	size_t len = sizeof(divstat);

	if (sysctl(mib, sizeof(mib) / sizeof(mib[0]),
	    &divstat, &len, NULL, 0) == -1) {
		if (errno != ENOPROTOOPT)
			warn("%s", name);
		return;
	}

	printf("%s:\n", name);
#define	p(f, m) if (divstat.f || sflag <= 1) \
	printf(m, divstat.f, plural(divstat.f))
#define	p1(f, m) if (divstat.f || sflag <= 1) \
	printf(m, divstat.f)
	p(divs_ipackets, "\t%lu total packet%s received\n");
	p1(divs_noport, "\t%lu dropped due to no socket\n");
	p1(divs_fullsock, "\t%lu dropped due to full socket buffers\n");
	p(divs_opackets, "\t%lu packet%s output\n");
	p1(divs_errors, "\t%lu errors\n");
#undef p
#undef p1
}

static	char *icmpnames[ICMP_MAXTYPE + 1] = {
	"echo reply",
	"#1",
	"#2",
	"destination unreachable",
	"source quench",
	"routing redirect",
	"#6",
	"#7",
	"echo",
	"router advertisement",
	"router solicitation",
	"time exceeded",
	"parameter problem",
	"time stamp",
	"time stamp reply",
	"information request",
	"information request reply",
	"address mask request",
	"address mask reply",
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
	"traceroute",
	"data conversion error",
	"mobile host redirect",
	"IPv6 where-are-you",
	"IPv6 i-am-here",
	"mobile registration request",
	"mobile registration reply",
	"#37",
	"#38",
	"SKIP",
	"Photuris",
};

/*
 * Dump ICMP statistics.
 */
void
icmp_stats(char *name)
{
	struct icmpstat icmpstat;
	int i, first;
	int mib[] = { CTL_NET, PF_INET, IPPROTO_ICMP, ICMPCTL_STATS };
	size_t len = sizeof(icmpstat);

	if (sysctl(mib, sizeof(mib) / sizeof(mib[0]),
	    &icmpstat, &len, NULL, 0) == -1) {
		if (errno != ENOPROTOOPT)
			warn("%s", name);
		return;
	}

	printf("%s:\n", name);
#define	p(f, m) if (icmpstat.f || sflag <= 1) \
	printf(m, icmpstat.f, plural(icmpstat.f))

	p(icps_error, "\t%lu call%s to icmp_error\n");
	p(icps_oldicmp,
	    "\t%lu error%s not generated because old message was icmp\n");
	p(icps_toofreq,
	    "\t%lu error%s not generated because of rate limitation\n");

	for (first = 1, i = 0; i < ICMP_MAXTYPE + 1; i++)
		if (icmpstat.icps_outhist[i] != 0) {
			if (first) {
				printf("\tOutput packet histogram:\n");
				first = 0;
			}
			if (icmpnames[i])
				printf("\t\t%s:", icmpnames[i]);
			else
				printf("\t\t#%d:", i);
			printf(" %lu\n", icmpstat.icps_outhist[i]);
		}
	p(icps_badcode, "\t%lu message%s with bad code fields\n");
	p(icps_tooshort, "\t%lu message%s < minimum length\n");
	p(icps_checksum, "\t%lu bad checksum%s\n");
	p(icps_badlen, "\t%lu message%s with bad length\n");
	p(icps_bmcastecho, "\t%lu echo request%s to broadcast/multicast "
	    "rejected\n");
	for (first = 1, i = 0; i < ICMP_MAXTYPE + 1; i++)
		if (icmpstat.icps_inhist[i] != 0) {
			if (first) {
				printf("\tInput packet histogram:\n");
				first = 0;
			}
			if (icmpnames[i])
				printf("\t\t%s:", icmpnames[i]);
			else
				printf("\t\t#%d:", i);
			printf(" %lu\n", icmpstat.icps_inhist[i]);
		}
	p(icps_reflect, "\t%lu message response%s generated\n");
#undef p
}

/*
 * Dump IGMP statistics structure.
 */
void
igmp_stats(char *name)
{
	struct igmpstat igmpstat;
	int mib[] = { CTL_NET, PF_INET, IPPROTO_IGMP, IGMPCTL_STATS };
	size_t len = sizeof(igmpstat);

	if (sysctl(mib, sizeof(mib) / sizeof(mib[0]),
	    &igmpstat, &len, NULL, 0) == -1) {
		if (errno != ENOPROTOOPT)
			warn("%s", name);
		return;
	}

	printf("%s:\n", name);
#define	p(f, m) if (igmpstat.f || sflag <= 1) \
	printf(m, igmpstat.f, plural(igmpstat.f))
#define	py(f, m) if (igmpstat.f || sflag <= 1) \
	printf(m, igmpstat.f, igmpstat.f != 1 ? "ies" : "y")

	p(igps_rcv_total, "\t%lu message%s received\n");
	p(igps_rcv_tooshort, "\t%lu message%s received with too few bytes\n");
	p(igps_rcv_badsum, "\t%lu message%s received with bad checksum\n");
	py(igps_rcv_queries, "\t%lu membership quer%s received\n");
	py(igps_rcv_badqueries, "\t%lu membership quer%s received with invalid field(s)\n");
	p(igps_rcv_reports, "\t%lu membership report%s received\n");
	p(igps_rcv_badreports, "\t%lu membership report%s received with invalid field(s)\n");
	p(igps_rcv_ourreports, "\t%lu membership report%s received for groups to which we belong\n");
	p(igps_snd_reports, "\t%lu membership report%s sent\n");
#undef p
#undef py
}

struct rpcnams {
	struct rpcnams *next;
	in_port_t port;
	int	  proto;
	char	*rpcname;
};

static char *
getrpcportnam(in_port_t port, int proto)
{
	struct sockaddr_in server_addr;
	struct hostent *hp;
	static struct pmaplist *head;
	int socket = RPC_ANYSOCK;
	struct timeval minutetimeout;
	CLIENT *client;
	struct rpcent *rpc;
	static int first;
	static struct rpcnams *rpcn;
	struct rpcnams *n;
	char num[20];

	if (first == 0) {
		first = 1;
		memset(&server_addr, 0, sizeof server_addr);
		server_addr.sin_family = AF_INET;
		if ((hp = gethostbyname("localhost")) != NULL)
			memmove((caddr_t)&server_addr.sin_addr, hp->h_addr,
			    hp->h_length);
		else
			(void) inet_aton("0.0.0.0", &server_addr.sin_addr);

		minutetimeout.tv_sec = 60;
		minutetimeout.tv_usec = 0;
		server_addr.sin_port = htons(PMAPPORT);
		if ((client = clnttcp_create(&server_addr, PMAPPROG,
		    PMAPVERS, &socket, 50, 500)) == NULL)
			return (NULL);
		if (clnt_call(client, PMAPPROC_DUMP, xdr_void, NULL,
		    xdr_pmaplist, &head, minutetimeout) != RPC_SUCCESS) {
			clnt_destroy(client);
			return (NULL);
		}
		for (; head != NULL; head = head->pml_next) {
			n = malloc(sizeof(struct rpcnams));
			if (n == NULL)
				continue;
			n->next = rpcn;
			rpcn = n;
			n->port = head->pml_map.pm_port;
			n->proto = head->pml_map.pm_prot;

			rpc = getrpcbynumber(head->pml_map.pm_prog);
			if (rpc)
				n->rpcname = strdup(rpc->r_name);
			else {
				snprintf(num, sizeof num, "%ld",
				    head->pml_map.pm_prog);
				n->rpcname = strdup(num);
			}
		}
		clnt_destroy(client);
	}

	for (n = rpcn; n; n = n->next)
		if (n->port == port && n->proto == proto)
			return (n->rpcname);
	return (NULL);
}

/*
 * Pretty print an Internet address (net address + port).
 * If the nflag was specified, use numbers instead of names.
 */
void
inetprint(struct in_addr *in, in_port_t port, const char *proto, int local)
{
	struct servent *sp = 0;
	char line[80], *cp, *nam;
	int width;

	snprintf(line, sizeof line, "%.*s.", (Aflag && !nflag) ? 12 : 16,
	    inetname(in));
	cp = strchr(line, '\0');
	if (!nflag && port)
		sp = getservbyport((int)port, proto);
	if (sp || port == 0)
		snprintf(cp, line + sizeof line - cp, "%.8s",
		    sp ? sp->s_name : "*");
	else if (local && !nflag && (nam = getrpcportnam(ntohs(port),
	    (strcmp(proto, "tcp") == 0 ? IPPROTO_TCP : IPPROTO_UDP))))
		snprintf(cp, line + sizeof line - cp, "%d[%.8s]",
		    ntohs(port), nam);
	else
		snprintf(cp, line + sizeof line - cp, "%d", ntohs(port));
	width = Aflag ? 18 : 22;
	printf(" %-*.*s", width, width, line);
}

/*
 * Construct an Internet address representation.
 * If the nflag has been supplied, give
 * numeric value, otherwise try for symbolic name.
 */
char *
inetname(struct in_addr *inp)
{
	char *cp;
	static char line[50];
	struct hostent *hp;
	static char domain[HOST_NAME_MAX+1];
	static int first = 1;

	if (first && !nflag) {
		first = 0;
		if (gethostname(domain, sizeof(domain)) == 0 &&
		    (cp = strchr(domain, '.')))
			(void) strlcpy(domain, cp + 1, sizeof domain);
		else
			domain[0] = '\0';
	}
	cp = NULL;
	if (!nflag && inp->s_addr != INADDR_ANY) {
		hp = gethostbyaddr((char *)inp, sizeof (*inp), AF_INET);
		if (hp) {
			if ((cp = strchr(hp->h_name, '.')) &&
			    !strcmp(cp + 1, domain))
				*cp = '\0';
			cp = hp->h_name;
		}
	}
	if (inp->s_addr == INADDR_ANY)
		snprintf(line, sizeof line, "*");
	else if (cp)
		snprintf(line, sizeof line, "%s", cp);
	else {
		inp->s_addr = ntohl(inp->s_addr);
#define C(x)	((x) & 0xff)
		snprintf(line, sizeof line, "%u.%u.%u.%u",
		    C(inp->s_addr >> 24), C(inp->s_addr >> 16),
		    C(inp->s_addr >> 8), C(inp->s_addr));
	}
	return (line);
}

/*
 * Dump AH statistics structure.
 */
void
ah_stats(char *name)
{
	struct ahstat ahstat;
	int mib[] = { CTL_NET, PF_INET, IPPROTO_AH, AHCTL_STATS };
	size_t len = sizeof(ahstat);

	if (sysctl(mib, sizeof(mib) / sizeof(mib[0]),
	    &ahstat, &len, NULL, 0) == -1) {
		if (errno != ENOPROTOOPT)
			warn("%s", name);
		return;
	}

	printf("%s:\n", name);
#define p(f, m) if (ahstat.f || sflag <= 1) \
	printf(m, ahstat.f, plural(ahstat.f))
#define p1(f, m) if (ahstat.f || sflag <= 1) \
	printf(m, ahstat.f)

	p1(ahs_input, "\t%llu input AH packets\n");
	p1(ahs_output, "\t%llu output AH packets\n");
	p(ahs_nopf, "\t%llu packet%s from unsupported protocol families\n");
	p(ahs_hdrops, "\t%llu packet%s shorter than header shows\n");
	p(ahs_pdrops, "\t%llu packet%s dropped due to policy\n");
	p(ahs_notdb, "\t%llu packet%s for which no TDB was found\n");
	p(ahs_badkcr, "\t%llu input packet%s that failed to be processed\n");
	p(ahs_badauth, "\t%llu packet%s that failed verification received\n");
	p(ahs_noxform, "\t%llu packet%s for which no XFORM was set in TDB received\n");
	p(ahs_qfull, "\t%llu packet%s were dropped due to full output queue\n");
	p(ahs_wrap, "\t%llu packet%s where counter wrapping was detected\n");
	p(ahs_replay, "\t%llu possibly replayed packet%s received\n");
	p(ahs_badauthl, "\t%llu packet%s with bad authenticator length received\n");
	p(ahs_invalid, "\t%llu packet%s attempted to use an invalid TDB\n");
	p(ahs_toobig, "\t%llu packet%s got larger than max IP packet size\n");
	p(ahs_crypto, "\t%llu packet%s that failed crypto processing\n");
	p(ahs_outfail, "\t%llu output packet%s could not be sent\n");
	p(ahs_ibytes, "\t%llu input byte%s\n");
	p(ahs_obytes, "\t%llu output byte%s\n");

#undef p
#undef p1
}

/*
 * Dump etherip statistics structure.
 */
void
etherip_stats(char *name)
{
	struct etheripstat etheripstat;
	int mib[] = { CTL_NET, PF_INET, IPPROTO_ETHERIP, ETHERIPCTL_STATS };
	size_t len = sizeof(etheripstat);

	if (sysctl(mib, sizeof(mib) / sizeof(mib[0]),
	    &etheripstat, &len, NULL, 0) == -1) {
		if (errno != ENOPROTOOPT)
			warn("%s", name);
		return;
	}

	printf("%s:\n", name);
#define p(f, m) if (etheripstat.f || sflag <= 1) \
	printf(m, etheripstat.f, plural(etheripstat.f))

	p(etherips_hdrops, "\t%llu packet%s shorter than header shows\n");
	p(etherips_qfull, "\t%llu packet%s were dropped due to full output queue\n");
	p(etherips_noifdrops, "\t%llu packet%s were dropped because of no interface/bridge information\n");
	p(etherips_pdrops, "\t%llu packet%s dropped due to policy\n");
	p(etherips_adrops, "\t%llu packet%s dropped for other reasons\n");
	p(etherips_ipackets, "\t%llu input ethernet-in-IP packet%s\n");
	p(etherips_opackets, "\t%llu output ethernet-in-IP packet%s\n");
	p(etherips_ibytes, "\t%llu input byte%s\n");
	p(etherips_obytes, "\t%llu output byte%s\n");
#undef p
}

/*
 * Dump IPsec statistics structure.
 */
void
ipsec_stats(char *name)
{
	struct ipsecstat ipsecstat;
	int mib[] = { CTL_NET, PF_INET, IPPROTO_IP, IPCTL_IPSEC_STATS };
	size_t len = sizeof(ipsecstat);

	if (sysctl(mib, sizeof(mib) / sizeof(mib[0]),
	    &ipsecstat, &len, NULL, 0) == -1) {
		if (errno != ENOPROTOOPT)
			warn("%s", name);
		return;
	}

	printf("%s:\n", name);
#define p(f, m) if (ipsecstat.f || sflag <= 1) \
	printf(m, ipsecstat.f, plural(ipsecstat.f))
	p(ipsec_ipackets, "\t%llu input IPsec packet%s\n");
	p(ipsec_opackets, "\t%llu output IPsec packet%s\n");
	p(ipsec_ibytes, "\t%llu input byte%s\n");
	p(ipsec_obytes, "\t%llu output byte%s\n");
	p(ipsec_idecompbytes, "\t%llu input byte%s, decompressed\n");
	p(ipsec_ouncompbytes, "\t%llu output byte%s, uncompressed\n");
	p(ipsec_idrops, "\t%llu packet%s dropped on input\n");
	p(ipsec_odrops, "\t%llu packet%s dropped on output\n");
	p(ipsec_crypto, "\t%llu packet%s that failed crypto processing\n");
	p(ipsec_noxform, "\t%llu packet%s for which no XFORM was set in TDB received\n");
	p(ipsec_notdb, "\t%llu packet%s for which no TDB was found\n");
#undef p
}

/*
 * Dump ESP statistics structure.
 */
void
esp_stats(char *name)
{
	struct espstat espstat;
	int mib[] = { CTL_NET, PF_INET, IPPROTO_ESP, ESPCTL_STATS };
	size_t len = sizeof(espstat);

	if (sysctl(mib, sizeof(mib) / sizeof(mib[0]),
	    &espstat, &len, NULL, 0) == -1) {
		if (errno != ENOPROTOOPT)
			warn("%s", name);
		return;
	}

	printf("%s:\n", name);
#define p(f, m) if (espstat.f || sflag <= 1) \
	printf(m, espstat.f, plural(espstat.f))

	p(esps_input, "\t%llu input ESP packet%s\n");
	p(esps_output, "\t%llu output ESP packet%s\n");
	p(esps_nopf, "\t%llu packet%s from unsupported protocol families\n");
	p(esps_hdrops, "\t%llu packet%s shorter than header shows\n");
	p(esps_pdrops, "\t%llu packet%s dropped due to policy\n");
	p(esps_notdb, "\t%llu packet%s for which no TDB was found\n");
	p(esps_badkcr, "\t%llu input packet%s that failed to be processed\n");
	p(esps_badenc, "\t%llu packet%s with bad encryption received\n");
	p(esps_badauth, "\t%llu packet%s that failed verification received\n");
	p(esps_noxform, "\t%llu packet%s for which no XFORM was set in TDB received\n");
	p(esps_qfull, "\t%llu packet%s were dropped due to full output queue\n");
	p(esps_wrap, "\t%llu packet%s where counter wrapping was detected\n");
	p(esps_replay, "\t%llu possibly replayed packet%s received\n");
	p(esps_badilen, "\t%llu packet%s with bad payload size or padding received\n");
	p(esps_invalid, "\t%llu packet%s attempted to use an invalid TDB\n");
	p(esps_toobig, "\t%llu packet%s got larger than max IP packet size\n");
	p(esps_crypto, "\t%llu packet%s that failed crypto processing\n");
	p(esps_outfail, "\t%llu output packet%s could not be sent\n");
	p(esps_udpencin, "\t%llu input UDP encapsulated ESP packet%s\n");
	p(esps_udpencout, "\t%llu output UDP encapsulated ESP packet%s\n");
	p(esps_udpinval, "\t%llu UDP packet%s for non-encapsulating TDB received\n");
	p(esps_udpneeded, "\t%llu raw ESP packet%s for encapsulating TDB received\n");
	p(esps_ibytes, "\t%llu input byte%s\n");
	p(esps_obytes, "\t%llu output byte%s\n");

#undef p
}

/*
 * Dump IP-in-IP statistics structure.
 */
void
ipip_stats(char *name)
{
	struct ipipstat ipipstat;
	int mib[] = { CTL_NET, PF_INET, IPPROTO_IPIP, IPIPCTL_STATS };
	size_t len = sizeof(ipipstat);

	if (sysctl(mib, sizeof(mib) / sizeof(mib[0]),
	    &ipipstat, &len, NULL, 0) == -1) {
		if (errno != ENOPROTOOPT)
			warn("%s", name);
		return;
	}

	printf("%s:\n", name);
#define p(f, m) if (ipipstat.f || sflag <= 1) \
	printf(m, ipipstat.f, plural(ipipstat.f))

	p(ipips_ipackets, "\t%llu total input packet%s\n");
	p(ipips_opackets, "\t%llu total output packet%s\n");
	p(ipips_hdrops, "\t%llu packet%s shorter than header shows\n");
	p(ipips_pdrops, "\t%llu packet%s dropped due to policy\n");
	p(ipips_spoof, "\t%llu packet%s with possibly spoofed local addresses\n");
	p(ipips_qfull, "\t%llu packet%s were dropped due to full output queue\n");
	p(ipips_ibytes, "\t%llu input byte%s\n");
	p(ipips_obytes, "\t%llu output byte%s\n");
	p(ipips_family, "\t%llu protocol family mismatche%s\n");
	p(ipips_unspec, "\t%llu attempt%s to use tunnel with unspecified endpoint(s)\n");
#undef p
}

/*
 * Dump CARP statistics structure.
 */
void
carp_stats(char *name)
{
	struct carpstats carpstat;
	int mib[] = { CTL_NET, PF_INET, IPPROTO_CARP, CARPCTL_STATS };
	size_t len = sizeof(carpstat);

	if (sysctl(mib, sizeof(mib) / sizeof(mib[0]),
	    &carpstat, &len, NULL, 0) == -1) {
		if (errno != ENOPROTOOPT)
			warn("%s", name);
		return;
	}

	printf("%s:\n", name);
#define p(f, m) if (carpstat.f || sflag <= 1) \
	printf(m, carpstat.f, plural(carpstat.f))
#define p2(f, m) if (carpstat.f || sflag <= 1) \
	printf(m, carpstat.f)

	p(carps_ipackets, "\t%llu packet%s received (IPv4)\n");
	p(carps_ipackets6, "\t%llu packet%s received (IPv6)\n");
	p(carps_badif, "\t\t%llu packet%s discarded for bad interface\n");
	p(carps_badttl, "\t\t%llu packet%s discarded for wrong TTL\n");
	p(carps_hdrops, "\t\t%llu packet%s shorter than header\n");
	p(carps_badsum, "\t\t%llu discarded for bad checksum%s\n");
	p(carps_badver,	"\t\t%llu discarded packet%s with a bad version\n");
	p2(carps_badlen, "\t\t%llu discarded because packet too short\n");
	p2(carps_badauth, "\t\t%llu discarded for bad authentication\n");
	p2(carps_badvhid, "\t\t%llu discarded for unknown vhid\n");
	p2(carps_badaddrs, "\t\t%llu discarded because of a bad address list\n");
	p(carps_opackets, "\t%llu packet%s sent (IPv4)\n");
	p(carps_opackets6, "\t%llu packet%s sent (IPv6)\n");
	p2(carps_onomem, "\t\t%llu send failed due to mbuf memory error\n");
	p(carps_preempt, "\t%llu transition%s to master\n");
#undef p
#undef p2
}

/*
 * Dump pfsync statistics structure.
 */
void
pfsync_stats(char *name)
{
	struct pfsyncstats pfsyncstat;
	int mib[] = { CTL_NET, PF_INET, IPPROTO_PFSYNC, PFSYNCCTL_STATS };
	size_t len = sizeof(pfsyncstat);

	if (sysctl(mib, sizeof(mib) / sizeof(mib[0]),
	    &pfsyncstat, &len, NULL, 0) == -1) {
		if (errno != ENOPROTOOPT)
			warn("%s", name);
		return;
	}

	printf("%s:\n", name);
#define p(f, m) if (pfsyncstat.f || sflag <= 1) \
	printf(m, pfsyncstat.f, plural(pfsyncstat.f))
#define p2(f, m) if (pfsyncstat.f || sflag <= 1) \
	printf(m, pfsyncstat.f)

	p(pfsyncs_ipackets, "\t%llu packet%s received (IPv4)\n");
	p(pfsyncs_ipackets6, "\t%llu packet%s received (IPv6)\n");
	p(pfsyncs_badif, "\t\t%llu packet%s discarded for bad interface\n");
	p(pfsyncs_badttl, "\t\t%llu packet%s discarded for bad ttl\n");
	p(pfsyncs_hdrops, "\t\t%llu packet%s shorter than header\n");
	p(pfsyncs_badver, "\t\t%llu packet%s discarded for bad version\n");
	p(pfsyncs_badauth, "\t\t%llu packet%s discarded for bad HMAC\n");
	p(pfsyncs_badact,"\t\t%llu packet%s discarded for bad action\n");
	p(pfsyncs_badlen, "\t\t%llu packet%s discarded for short packet\n");
	p(pfsyncs_badval, "\t\t%llu state%s discarded for bad values\n");
	p(pfsyncs_stale, "\t\t%llu stale state%s\n");
	p(pfsyncs_badstate, "\t\t%llu failed state lookup/insert%s\n");
	p(pfsyncs_opackets, "\t%llu packet%s sent (IPv4)\n");
	p(pfsyncs_opackets6, "\t%llu packet%s sent (IPv6)\n");
	p2(pfsyncs_onomem, "\t\t%llu send failed due to mbuf memory error\n");
	p2(pfsyncs_oerrors, "\t\t%llu send error\n");
#undef p
#undef p2
}

/*
 * Dump pflow statistics structure.
 */
void
pflow_stats(char *name)
{
	struct pflowstats flowstats;
	int mib[] = { CTL_NET, PF_PFLOW, NET_PFLOW_STATS };
	size_t len = sizeof(struct pflowstats);

	if (sysctl(mib, sizeof(mib) / sizeof(mib[0]), &flowstats, &len,
	    NULL, 0) == -1) {
		if (errno != ENOPROTOOPT)
			warn("%s", name);
		return;
	}

	printf("%s:\n", name);
#define p(f, m) if (flowstats.f || sflag <= 1) \
	printf(m, flowstats.f, plural(flowstats.f))
#define p2(f, m) if (flowstats.f || sflag <= 1) \
	printf(m, flowstats.f)

	p(pflow_flows, "\t%llu flow%s sent\n");
	p(pflow_packets, "\t%llu packet%s sent\n");
	p2(pflow_onomem, "\t\t%llu send failed due to mbuf memory error\n");
	p2(pflow_oerrors, "\t\t%llu send error\n");
#undef p
#undef p2
}

/*
 * Dump IPCOMP statistics structure.
 */
void
ipcomp_stats(char *name)
{
	struct ipcompstat ipcompstat;
	int mib[] = { CTL_NET, PF_INET, IPPROTO_IPCOMP, IPCOMPCTL_STATS };
	size_t len = sizeof(ipcompstat);

	if (sysctl(mib, sizeof(mib) / sizeof(mib[0]),
	    &ipcompstat, &len, NULL, 0) == -1) {
		if (errno != ENOPROTOOPT)
			warn("%s", name);
		return;
	}

	printf("%s:\n", name);
#define p(f, m) if (ipcompstat.f || sflag <= 1) \
	printf(m, ipcompstat.f, plural(ipcompstat.f))

	p(ipcomps_input, "\t%llu input IPCOMP packet%s\n");
	p(ipcomps_output, "\t%llu output IPCOMP packet%s\n");
	p(ipcomps_nopf, "\t%llu packet%s from unsupported protocol families\n");
	p(ipcomps_hdrops, "\t%llu packet%s shorter than header shows\n");
	p(ipcomps_pdrops, "\t%llu packet%s dropped due to policy\n");
	p(ipcomps_notdb, "\t%llu packet%s for which no TDB was found\n");
	p(ipcomps_badkcr, "\t%llu input packet%s that failed to be processed\n");
	p(ipcomps_noxform, "\t%llu packet%s for which no XFORM was set in TDB received\n");
	p(ipcomps_qfull, "\t%llu packet%s were dropped due to full output queue\n");
	p(ipcomps_wrap, "\t%llu packet%s where counter wrapping was detected\n");
	p(ipcomps_invalid, "\t%llu packet%s attempted to use an invalid TDB\n");
	p(ipcomps_toobig, "\t%llu packet%s got larger than max IP packet size\n");
	p(ipcomps_crypto, "\t%llu packet%s that failed (de)compression processing\n");
	p(ipcomps_outfail, "\t%llu output packet%s could not be sent\n");
	p(ipcomps_minlen, "\t%llu packet%s less than minimum compression length\n");
	p(ipcomps_ibytes, "\t%llu input byte%s\n");
	p(ipcomps_obytes, "\t%llu output byte%s\n");

#undef p
}

/*
 * Dump the contents of a socket structure
 */
void
socket_dump(u_long off)
{
	struct socket so;

	if (off == 0)
		return;
	kread(off, &so, sizeof(so));

#define	p(fmt, v, sep) printf(#v " " fmt sep, so.v);
#define	pp(fmt, v, sep) printf(#v " " fmt sep, so.v);
	printf("socket %#lx\n ", off);
	p("%#.4x", so_type, "\n ");
	p("%#.4x", so_options, "\n ");
	p("%d", so_linger, "\n ");
	p("%#.4x", so_state, "\n ");
	pp("%p", so_pcb, ", ");
	pp("%p", so_proto, ", ");
	pp("%p", so_head, "\n ");
	p("%d", so_q0len, ", ");
	p("%d", so_qlen, ", ");
	p("%d", so_qlimit, "\n ");
	p("%d", so_timeo, "\n ");
	p("%u", so_error, "\n ");
	p("%p", so_sigio.sir_sigio, "\n ");
	p("%lu", so_oobmark, "\n ");
	if (so.so_sp)
		sosplice_dump((u_long)so.so_sp);
	sockbuf_dump(&so.so_rcv, "so_rcv");
	sockbuf_dump(&so.so_snd, "so_snd");
	p("%u", so_euid, ", ");
	p("%u", so_ruid, ", ");
	p("%u", so_egid, ", ");
	p("%u", so_rgid, "\n ");
	p("%d", so_cpid, "\n");
#undef	p
#undef	pp

	protosw_dump((u_long)so.so_proto, (u_long)so.so_pcb);
}

/*
 * Dump the contents of a struct sosplice
 */
void
sosplice_dump(u_long off)
{
	struct sosplice ssp;

	if (off == 0)
		return;
	kread(off, &ssp, sizeof(ssp));

#define	p(fmt, v, sep) printf(#v " " fmt sep, ssp.v);
#define	pll(fmt, v, sep) printf(#v " " fmt sep, (long long) ssp.v);
#define	pp(fmt, v, sep) printf(#v " " fmt sep, ssp.v);
	pp("%p", ssp_socket, ", ");
	pp("%p", ssp_soback, "\n ");
	p("%lld", ssp_len, ", ");
	p("%lld", ssp_max, ", ");
	pll("%lld", ssp_idletv.tv_sec, ", ");
	p("%ld", ssp_idletv.tv_usec, "\n ");
#undef	p
#undef	pll
#undef	pp
}

/*
 * Dump the contents of a socket buffer
 */
void
sockbuf_dump(struct sockbuf *sb, const char *name)
{
#define	p(fmt, v, sep) printf(#v " " fmt sep, sb->v);
	printf("%s ", name);
	p("%lu", sb_cc, ", ");
	p("%lu", sb_datacc, ", ");
	p("%lu", sb_hiwat, ", ");
	p("%lu", sb_wat, "\n ");
	printf("%s ", name);
	p("%lu", sb_mbcnt, ", ");
	p("%lu", sb_mbmax, ", ");
	p("%ld", sb_lowat, "\n ");
	printf("%s ", name);
	p("%#.8x", sb_flagsintr, ", ");
	p("%#.4x", sb_flags, ", ");
	p("%u", sb_timeo, "\n ");
#undef	p
}

/*
 * Dump the contents of a protosw structure
 */
void
protosw_dump(u_long off, u_long pcb)
{
	struct protosw proto;

	if (off == 0)
		return;
	kread(off, &proto, sizeof(proto));

#define	p(fmt, v, sep) printf(#v " " fmt sep, proto.v);
#define	pp(fmt, v, sep) printf(#v " " fmt sep, proto.v);
	printf("protosw %#lx\n ", off);
	p("%#.4x", pr_type, "\n ");
	pp("%p", pr_domain, "\n ");
	p("%d", pr_protocol, "\n ");
	p("%#.4x", pr_flags, "\n");
#undef	p
#undef	pp

	domain_dump((u_long)proto.pr_domain, pcb, proto.pr_protocol);
}

/*
 * Dump the contents of a domain structure
 */
void
domain_dump(u_long off, u_long pcb, short protocol)
{
	struct domain dom;
	char name[256];

	if (off == 0)
		return;
	kread(off, &dom, sizeof(dom));
	kread((u_long)dom.dom_name, name, sizeof(name));

#define	p(fmt, v, sep) printf(#v " " fmt sep, dom.v);
	printf("domain %#lx\n ", off);
	p("%d", dom_family, "\n ");
	printf("dom_name %.*s\n", (int)sizeof(name), name);
#undef	p
}

/*
 * Dump the contents of a internet PCB
 */
void
inpcb_dump(u_long off, short protocol, int af)
{
	struct inpcb inp;
	char faddr[256], laddr[256], raddr[256];

	if (off == 0)
		return;
	kread(off, &inp, sizeof(inp));

	if (vflag)
		socket_dump((u_long)inp.inp_socket);

	switch (af) {
	case AF_INET:
		inet_ntop(af, &inp.inp_faddr, faddr, sizeof(faddr));
		inet_ntop(af, &inp.inp_laddr, laddr, sizeof(laddr));
		inet_ntop(af, &((struct sockaddr_in *)
		    (&inp.inp_route.ro_dst))->sin_addr, raddr, sizeof(raddr));
		break;
	case AF_INET6:
		inet_ntop(af, &inp.inp_faddr6, faddr, sizeof(faddr));
		inet_ntop(af, &inp.inp_laddr6, laddr, sizeof(laddr));
		inet_ntop(af, &inp.inp_route6.ro_dst.sin6_addr,
		    raddr, sizeof(raddr));
		break;
	default:
		faddr[0] = laddr[0] = '\0';
	}

#define	p(fmt, v, sep) printf(#v " " fmt sep, inp.v);
#define	pp(fmt, v, sep) printf(#v " " fmt sep, inp.v);
	printf("inpcb %#lx\n ", off);
	pp("%p", inp_table, "\n ");
	printf("inp_faddru %s, inp_laddru %s\n ", faddr, laddr);
	HTONS(inp.inp_fport);
	HTONS(inp.inp_lport);
	p("%u", inp_fport, ", ");
	p("%u", inp_lport, "\n ");
	pp("%p", inp_socket, ", ");
	pp("%p", inp_ppcb, "\n ");
	pp("%p", inp_route.ro_rt, ", ");
	printf("ro_dst %s\n ", raddr);
	p("%#.8x", inp_flags, "\n ");
	p("%d", inp_hops, "\n ");
	p("%u", inp_seclevel[0], ", ");
	p("%u", inp_seclevel[1], ", ");
	p("%u", inp_seclevel[2], ", ");
	p("%u", inp_seclevel[3], "\n ");
	p("%u", inp_ip_minttl, "\n ");
	p("%d", inp_cksum6, "\n ");
	pp("%p", inp_icmp6filt, "\n ");
	pp("%p", inp_pf_sk, "\n ");
	p("%u", inp_rtableid, "\n ");
	p("%d", inp_pipex, "\n");
#undef	p
#undef	pp

	switch (protocol) {
	case IPPROTO_TCP:
		tcpcb_dump((u_long)inp.inp_ppcb);
		break;
	}
}

/*
 * Dump the contents of a TCP PCB
 */
void
tcpcb_dump(u_long off)
{
	struct tcpcb tcpcb;

	if (off == 0)
		return;
	kread(off, (char *)&tcpcb, sizeof (tcpcb));

#define	p(fmt, v, sep) printf(#v " " fmt sep, tcpcb.v);
#define	pp(fmt, v, sep) printf(#v " " fmt sep, tcpcb.v);
	printf("tcpcb %#lx\n ", off);
	pp("%p", t_inpcb, "\n ");
	p("%d", t_state, "");
	if (tcpcb.t_state >= 0 && tcpcb.t_state < TCP_NSTATES)
		printf(" (%s)", tcpstates[tcpcb.t_state]);
	printf("\n ");
	p("%d", t_rxtshift, ", ");
	p("%d", t_rxtcur, ", ");
	p("%d", t_dupacks, "\n ");
	p("%u", t_maxseg, ", ");
	p("%u", t_maxopd, ", ");
	p("%u", t_peermss, "\n ");
	p("0x%x", t_flags, ", ");
	p("%u", t_force, "\n ");
	p("%u", iss, "\n ");
	p("%u", snd_una, ", ");
	p("%u", snd_nxt, ", ");
	p("%u", snd_up, "\n ");
	p("%u", snd_wl1, ", ");
	p("%u", snd_wl2, ", ");
	p("%lu", snd_wnd, "\n ");
	p("%d", sack_enable, ", ");
	p("%d", snd_numholes, ", ");
	p("%u", snd_last, "\n ");
	p("%u", irs, "\n ");
	p("%u", rcv_nxt, ", ");
	p("%u", rcv_up, ", ");
	p("%lu", rcv_wnd, "\n ");
	p("%u", rcv_lastsack, "\n ");
	p("%d", rcv_numsacks, "\n ");
	p("%u", rcv_adv, ", ");
	p("%u", snd_max, "\n ");
	p("%lu", snd_cwnd, ", ");
	p("%lu", snd_ssthresh, ", ");
	p("%lu", max_sndwnd, "\n ");
	p("%u", t_rcvtime, ", ");
	p("%u", t_rtttime, ", ");
	p("%u", t_rtseq, "\n ");
	p("%u", t_srtt, ", ");
	p("%u", t_rttvar, ", ");
	p("%u", t_rttmin, "\n ");
	p("%u", t_oobflags, ", ");
	p("%u", t_iobc, "\n ");
	p("%u", t_softerror, "\n ");
	p("%u", snd_scale, ", ");
	p("%u", rcv_scale, ", ");
	p("%u", request_r_scale, ", ");
	p("%u", requested_s_scale, "\n ");
	p("%u", ts_recent, ", ");
	p("%u", ts_recent_age, "\n ");
	p("%u", last_ack_sent, "\n ");
	HTONS(tcpcb.t_pmtud_ip_len);
	HTONS(tcpcb.t_pmtud_nextmtu);
	p("%u", t_pmtud_mss_acked, ", ");
	p("%u", t_pmtud_mtu_sent, "\n ");
	p("%u", t_pmtud_nextmtu, ", ");
	p("%u", t_pmtud_ip_len, ", ");
	p("%u", t_pmtud_ip_hl, "\n ");
	p("%u", t_pmtud_th_seq, "\n ");
	p("%u", pf, "\n");
#undef	p
#undef	pp
}
