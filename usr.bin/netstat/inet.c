/*	$OpenBSD: inet.c,v 1.32 1999/04/09 23:30:05 niklas Exp $	*/
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
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *	This product includes software developed by the University of
 *	California, Berkeley and its contributors.
 * 4. Neither the name of the University nor the names of its contributors
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

#ifndef lint
#if 0
static char sccsid[] = "from: @(#)inet.c	8.4 (Berkeley) 4/20/94";
#else
static char *rcsid = "$OpenBSD: inet.c,v 1.32 1999/04/09 23:30:05 niklas Exp $";
#endif
#endif /* not lint */

#include <sys/param.h>
#include <sys/queue.h>
#include <sys/socket.h>
#include <sys/socketvar.h>
#include <sys/mbuf.h>
#include <sys/protosw.h>

#include <net/route.h>
#include <netinet/in.h>
#include <netinet/in_systm.h>
#include <netinet/ip.h>
#include <netinet/in_pcb.h>
#include <netinet/ip_icmp.h>
#include <netinet/icmp_var.h>
#include <netinet/igmp_var.h>
#include <netinet/ip_var.h>
#include <netinet/tcp.h>
#include <netinet/tcpip.h>
#include <netinet/tcp_seq.h>
#define TCPSTATES
#include <netinet/tcp_fsm.h>
#include <netinet/tcp_timer.h>
#include <netinet/tcp_var.h>
#include <netinet/tcp_debug.h>
#include <netinet/udp.h>
#include <netinet/udp_var.h>
#include <netinet/ip_ipsp.h>
#include <netinet/ip_ah.h>
#include <netinet/ip_esp.h>
#include <netinet/ip_ip4.h>

#include <arpa/inet.h>
#include <limits.h>
#include <netdb.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <stdlib.h>
#include "netstat.h"

#include <rpc/rpc.h>
#include <rpc/pmap_prot.h>
#include <rpc/pmap_clnt.h>

struct	inpcb inpcb;
struct	tcpcb tcpcb;
struct	socket sockb;

char	*inetname __P((struct in_addr *));
void	inetprint __P((struct in_addr *, int, char *, int));

/*
 * Print a summary of connections related to an Internet
 * protocol.  For TCP, also give state of connection.
 * Listening processes (aflag) are suppressed unless the
 * -a (all) flag is specified.
 */
void
protopr(off, name)
	u_long off;
	char *name;
{
	struct inpcbtable table;
	register struct inpcb *head, *next, *prev;
	struct inpcb inpcb;
	int istcp;
	static int first = 1;

	if (off == 0)
		return;
	istcp = strcmp(name, "tcp") == 0;
	kread(off, (char *)&table, sizeof table);
	prev = head =
	    (struct inpcb *)&((struct inpcbtable *)off)->inpt_queue.cqh_first;
	next = table.inpt_queue.cqh_first;

	while (next != head) {
		kread((u_long)next, (char *)&inpcb, sizeof inpcb);
		if (inpcb.inp_queue.cqe_prev != prev) {
			printf("???\n");
			break;
		}
		prev = next;
		next = inpcb.inp_queue.cqe_next;

		if (!aflag &&
		    inet_lnaof(inpcb.inp_laddr) == INADDR_ANY)
			continue;
		kread((u_long)inpcb.inp_socket, (char *)&sockb, sizeof (sockb));
		if (istcp) {
			kread((u_long)inpcb.inp_ppcb,
			    (char *)&tcpcb, sizeof (tcpcb));
		}
		if (first) {
			printf("Active Internet connections");
			if (aflag)
				printf(" (including servers)");
			putchar('\n');
			if (Aflag)
				printf("%-*.*s %-5.5s %-6.6s %-6.6s  %-18.18s %-18.18s %s\n",
				    PLEN, PLEN, "PCB", "Proto", "Recv-Q",
				    "Send-Q", "Local Address",
				    "Foreign Address", "(state)");
			else
				printf("%-5.5s %-6.6s %-6.6s  %-22.22s %-22.22s %s\n",
				    "Proto", "Recv-Q", "Send-Q",
				    "Local Address", "Foreign Address",
				    "(state)");
			first = 0;
		}
		if (Aflag)
			if (istcp)
				printf("%*p ", PLEN, inpcb.inp_ppcb);
			else
				printf("%*p ", PLEN, prev);
		printf("%-5.5s %6ld %6ld ", name, sockb.so_rcv.sb_cc,
			sockb.so_snd.sb_cc);
		inetprint(&inpcb.inp_laddr, (int)inpcb.inp_lport, name, 1);
		inetprint(&inpcb.inp_faddr, (int)inpcb.inp_fport, name, 0);
		if (istcp) {
			if (tcpcb.t_state < 0 || tcpcb.t_state >= TCP_NSTATES)
				printf(" %d", tcpcb.t_state);
			else
				printf(" %s", tcpstates[tcpcb.t_state]);
		}
		putchar('\n');
	}
}

/*
 * Dump TCP statistics structure.
 */
void
tcp_stats(off, name)
	u_long off;
	char *name;
{
	struct tcpstat tcpstat;

	if (off == 0)
		return;
	printf ("%s:\n", name);
	kread(off, (char *)&tcpstat, sizeof (tcpstat));

#define	p(f, m) if (tcpstat.f || sflag <= 1) \
    printf(m, tcpstat.f, plural(tcpstat.f))
#define	p2(f1, f2, m) if (tcpstat.f1 || tcpstat.f2 || sflag <= 1) \
    printf(m, tcpstat.f1, plural(tcpstat.f1), tcpstat.f2, plural(tcpstat.f2))
#define	p3(f, m) if (tcpstat.f || sflag <= 1) \
    printf(m, tcpstat.f, plurales(tcpstat.f))

	p(tcps_sndtotal, "\t%ld packet%s sent\n");
	p2(tcps_sndpack,tcps_sndbyte,
		"\t\t%ld data packet%s (%qd byte%s)\n");
	p2(tcps_sndrexmitpack, tcps_sndrexmitbyte,
		"\t\t%ld data packet%s (%qd byte%s) retransmitted\n");
	p(tcps_sndrexmitfast, "\t\t%qd fast retransmitted packet%s\n");
	p2(tcps_sndacks, tcps_delack,
		"\t\t%ld ack-only packet%s (%ld delayed)\n");
	p(tcps_sndurg, "\t\t%ld URG only packet%s\n");
	p(tcps_sndprobe, "\t\t%ld window probe packet%s\n");
	p(tcps_sndwinup, "\t\t%ld window update packet%s\n");
	p(tcps_sndctrl, "\t\t%ld control packet%s\n");
	p(tcps_rcvtotal, "\t%ld packet%s received\n");
	p2(tcps_rcvackpack, tcps_rcvackbyte, "\t\t%ld ack%s (for %qd byte%s)\n");
	p(tcps_rcvdupack, "\t\t%ld duplicate ack%s\n");
	p(tcps_rcvacktoomuch, "\t\t%ld ack%s for unsent data\n");
	p2(tcps_rcvpack, tcps_rcvbyte,
		"\t\t%ld packet%s (%qd byte%s) received in-sequence\n");
	p2(tcps_rcvduppack, tcps_rcvdupbyte,
		"\t\t%ld completely duplicate packet%s (%qd byte%s)\n");
	p(tcps_pawsdrop, "\t\t%ld old duplicate packet%s\n");
	p2(tcps_rcvpartduppack, tcps_rcvpartdupbyte,
		"\t\t%ld packet%s with some dup. data (%qd byte%s duped)\n");
	p2(tcps_rcvoopack, tcps_rcvoobyte,
		"\t\t%ld out-of-order packet%s (%qd byte%s)\n");
	p2(tcps_rcvpackafterwin, tcps_rcvbyteafterwin,
		"\t\t%ld packet%s (%qd byte%s) of data after window\n");
	p(tcps_rcvwinprobe, "\t\t%ld window probe%s\n");
	p(tcps_rcvwinupd, "\t\t%ld window update packet%s\n");
	p(tcps_rcvafterclose, "\t\t%ld packet%s received after close\n");
	p(tcps_rcvbadsum, "\t\t%ld discarded for bad checksum%s\n");
	p(tcps_rcvbadoff, "\t\t%ld discarded for bad header offset field%s\n");
	p(tcps_rcvshort, "\t\t%ld discarded because packet too short\n");
	p(tcps_rcvnosec, "\t\t%ld discarded for missing IPSec protection\n");
	p(tcps_connattempt, "\t%ld connection request%s\n");
	p(tcps_accepts, "\t%ld connection accept%s\n");
	p(tcps_connects, "\t%ld connection%s established (including accepts)\n");
	p2(tcps_closed, tcps_drops,
		"\t%ld connection%s closed (including %ld drop%s)\n");
	p(tcps_conndrops, "\t%ld embryonic connection%s dropped\n");
	p2(tcps_rttupdated, tcps_segstimed,
		"\t%ld segment%s updated rtt (of %ld attempt%s)\n");
	p(tcps_rexmttimeo, "\t%ld retransmit timeout%s\n");
	p(tcps_timeoutdrop, "\t\t%ld connection%s dropped by rexmit timeout\n");
	p(tcps_persisttimeo, "\t%ld persist timeout%s\n");
	p(tcps_keeptimeo, "\t%ld keepalive timeout%s\n");
	p(tcps_keepprobe, "\t\t%ld keepalive probe%s sent\n");
	p(tcps_keepdrops, "\t\t%ld connection%s dropped by keepalive\n");
	p(tcps_predack, "\t%ld correct ACK header prediction%s\n");
	p(tcps_preddat, "\t%ld correct data packet header prediction%s\n");
	p3(tcps_pcbhashmiss, "\t%ld PCB cache miss%s\n");
	p(tcps_badsyn, "\t%ld SYN packet%s received with same src/dst address/port\n");
#undef p
#undef p2
#undef p3
}

/*
 * Dump UDP statistics structure.
 */
void
udp_stats(off, name)
	u_long off;
	char *name;
{
	struct udpstat udpstat;
	u_long delivered;

	if (off == 0)
		return;
	kread(off, (char *)&udpstat, sizeof (udpstat));
	printf("%s:\n", name);
#define	p(f, m) if (udpstat.f || sflag <= 1) \
    printf(m, udpstat.f, plural(udpstat.f))
	p(udps_ipackets, "\t%lu datagram%s received\n");
	p(udps_hdrops, "\t%lu with incomplete header\n");
	p(udps_badlen, "\t%lu with bad data length field\n");
	p(udps_badsum, "\t%lu with bad checksum\n");
	p(udps_nosum, "\t%lu with no checksum\n");
	p(udps_noport, "\t%lu dropped due to no socket\n");
	p(udps_noportbcast, "\t%lu broadcast/multicast datagram%s dropped due to no socket\n");
	p(udps_nosec, "\t%lu dropped due to missing IPSec protection\n");
	p(udps_fullsock, "\t%lu dropped due to full socket buffers\n");
	delivered = udpstat.udps_ipackets -
		    udpstat.udps_hdrops -
		    udpstat.udps_badlen -
		    udpstat.udps_badsum -
		    udpstat.udps_noport -
		    udpstat.udps_noportbcast -
		    udpstat.udps_fullsock;
	if (delivered || sflag <= 1)
		printf("\t%lu delivered\n", delivered);
	p(udps_opackets, "\t%lu datagram%s output\n");
	p(udps_pcbhashmiss, "\t%lu missed PCB cache\n");
#undef p
}

/*
 * Dump IP statistics structure.
 */
void
ip_stats(off, name)
	u_long off;
	char *name;
{
	struct ipstat ipstat;

	if (off == 0)
		return;
	kread(off, (char *)&ipstat, sizeof (ipstat));
	printf("%s:\n", name);

#define	p(f, m) if (ipstat.f || sflag <= 1) \
    printf(m, ipstat.f, plural(ipstat.f))

	p(ips_total, "\t%lu total packet%s received\n");
	p(ips_badsum, "\t%lu bad header checksum%s\n");
	p(ips_toosmall, "\t%lu with size smaller than minimum\n");
	p(ips_tooshort, "\t%lu with data size < data length\n");
	p(ips_badhlen, "\t%lu with header length < data size\n");
	p(ips_badlen, "\t%lu with data length < header length\n");
	p(ips_badoptions, "\t%lu with bad options\n");
	p(ips_badvers, "\t%lu with incorrect version number\n");
	p(ips_fragments, "\t%lu fragment%s received\n");
	p(ips_fragdropped, "\t%lu fragment%s dropped (dup or out of space)\n");
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
	p(ips_rcvmemdrop, "\t%lu fragment floods\n");
#undef p
}

static	char *icmpnames[] = {
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
};

/*
 * Dump ICMP statistics.
 */
void
icmp_stats(off, name)
	u_long off;
	char *name;
{
	struct icmpstat icmpstat;
	register int i, first;

	if (off == 0)
		return;
	kread(off, (char *)&icmpstat, sizeof (icmpstat));
	printf("%s:\n", name);

#define	p(f, m) if (icmpstat.f || sflag <= 1) \
    printf(m, icmpstat.f, plural(icmpstat.f))

	p(icps_error, "\t%lu call%s to icmp_error\n");
	p(icps_oldicmp,
	    "\t%lu error%s not generated 'cuz old message was icmp\n");
	for (first = 1, i = 0; i < ICMP_MAXTYPE + 1; i++)
		if (icmpstat.icps_outhist[i] != 0) {
			if (first) {
				printf("\tOutput histogram:\n");
				first = 0;
			}
			printf("\t\t%s: %lu\n", icmpnames[i],
				icmpstat.icps_outhist[i]);
		}
	p(icps_badcode, "\t%lu message%s with bad code fields\n");
	p(icps_tooshort, "\t%lu message%s < minimum length\n");
	p(icps_checksum, "\t%lu bad checksum%s\n");
	p(icps_badlen, "\t%lu message%s with bad length\n");
	for (first = 1, i = 0; i < ICMP_MAXTYPE + 1; i++)
		if (icmpstat.icps_inhist[i] != 0) {
			if (first) {
				printf("\tInput histogram:\n");
				first = 0;
			}
			printf("\t\t%s: %lu\n", icmpnames[i],
				icmpstat.icps_inhist[i]);
		}
	p(icps_reflect, "\t%lu message response%s generated\n");
#undef p
}

/*
 * Dump IGMP statistics structure.
 */
void
igmp_stats(off, name)
	u_long off;
	char *name;
{
	struct igmpstat igmpstat;

	if (off == 0)
		return;
	kread(off, (char *)&igmpstat, sizeof (igmpstat));
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

char *
getrpcportnam(port, proto)
	in_port_t port;
	int proto;
{
	struct sockaddr_in server_addr;
	register struct hostent *hp;
	static struct pmaplist *head;
	int socket = RPC_ANYSOCK;
	struct timeval minutetimeout;
	register CLIENT *client;
	struct rpcent *rpc;
	static int first;
	static struct rpcnams *rpcn;
	struct rpcnams *n;
	char num[20];
	
	if (first == 0) {
		first = 1;
		memset((char *)&server_addr, 0, sizeof server_addr);
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
			n = (struct rpcnams *)malloc(sizeof(struct rpcnams));
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
inetprint(in, port, proto, local)
	register struct in_addr *in;
	in_port_t port;
	char *proto;
	int local;
{
	struct servent *sp = 0;
	char line[80], *cp, *nam;
	int proton;
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
inetname(inp)
	struct in_addr *inp;
{
	register char *cp;
	static char line[50];
	struct hostent *hp;
	struct netent *np;
	static char domain[MAXHOSTNAMELEN + 1];
	static int first = 1;

	if (first && !nflag) {
		first = 0;
		if (gethostname(domain, MAXHOSTNAMELEN) == 0 &&
		    (cp = strchr(domain, '.')))
			(void) strcpy(domain, cp + 1);
		else
			domain[0] = 0;
	}
	cp = 0;
	if (!nflag && inp->s_addr != INADDR_ANY) {
		int net = inet_netof(*inp);
		int lna = inet_lnaof(*inp);

		if (lna == INADDR_ANY) {
			np = getnetbyaddr(net, AF_INET);
			if (np)
				cp = np->n_name;
		}
		if (cp == 0) {
			hp = gethostbyaddr((char *)inp, sizeof (*inp), AF_INET);
			if (hp) {
				if ((cp = strchr(hp->h_name, '.')) &&
				    !strcmp(cp + 1, domain))
					*cp = 0;
				cp = hp->h_name;
			}
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
ah_stats(off, name)
        u_long off;
        char *name;
{
        struct ahstat ahstat;

        if (off == 0)
                return;
        kread(off, (char *)&ahstat, sizeof (ahstat));
        printf("%s:\n", name);

#define p(f, m) if (ahstat.f || sflag <= 1) \
    printf(m, ahstat.f, plural(ahstat.f))

	p(ahs_input, "\t%u input AH packets\n");
	p(ahs_output, "\t%u output AH packets\n");
        p(ahs_hdrops, "\t%u packet%s shorter than header shows\n");
        p(ahs_notdb, "\t%u packet%s for which no TDB was found\n");
        p(ahs_badkcr, "\t%u input packet%s that failed to be processed\n");
        p(ahs_badauth, "\t%u packet%s that failed verification received\n");
        p(ahs_noxform, "\t%u packet%s for which no XFORM was set in TDB received\n");
        p(ahs_qfull, "\t%u packet%s were dropped due to full output queue\n");
        p(ahs_wrap, "\t%u packet%s where counter wrapping was detected\n");
        p(ahs_replay, "\t%u possibly replayed packet%s received\n");
        p(ahs_badauthl, "\t%u packet%s with bad authenticator length received\n");
	p(ahs_invalid, "\t%u packet%s attempted to use an invalid tdb\n");
	p(ahs_toobig, "\t%u packet%s got larger than max IP packet size\n");
	p(ahs_ibytes, "\t%qu input byte%s\n");
	p(ahs_obytes, "\t%qu output byte%s\n");

#undef p
}

/*
 * Dump ESP statistics structure.
 */
void
esp_stats(off, name)
        u_long off;
        char *name;
{
        struct espstat espstat;

	
        if (off == 0)
                return;
        kread(off, (char *)&espstat, sizeof (espstat));
        printf("%s:\n", name);

#define p(f, m) if (espstat.f || sflag <= 1) \
    printf(m, espstat.f, plural(espstat.f))

	p(esps_input, "\t%u input ESP packets\n");
	p(esps_output, "\t%u output ESP packets\n");
        p(esps_hdrops, "\t%u packet%s shorter than header shows\n");
        p(esps_notdb, "\t%u packet%s for which no TDB was found\n");
        p(esps_badkcr, "\t%u input packet%s that failed to be processed\n");
        p(esps_badauth, "\t%u packet%s that failed verification received\n");
        p(esps_noxform, "\t%u packet%s for which no XFORM was set in TDB received\n");   
        p(esps_qfull, "\t%u packet%s were dropped due to full output queue\n");
        p(esps_wrap, "\t%u packet%s where counter wrapping was detected\n");
        p(esps_replay, "\t%u possibly replayed packet%s received\n"); 
        p(esps_badilen, "\t%u packet%s with payload not a multiple of 8 received\n");
	p(esps_invalid, "\t%u packet%s attempted to use an invalid tdb\n");
	p(esps_toobig, "\t%u packet%s got larger than max IP packet size\n");
	p(esps_ibytes, "\t%qu input byte%s\n");
	p(esps_obytes, "\t%qu output byte%s\n");

#undef p
}

/*
 * Dump ESP statistics structure.
 */
void
ip4_stats(off, name)
        u_long off;
        char *name;
{
        struct ip4stat ip4stat;

        if (off == 0)
                return;
        kread(off, (char *)&ip4stat, sizeof (ip4stat));
        printf("%s:\n", name);

#define p(f, m) if (ip4stat.f || sflag <= 1) \
    printf(m, ip4stat.f, plural(ip4stat.f))

        p(ip4s_ipackets, "\t%u total input packet%s\n");
        p(ip4s_opackets, "\t%u total output packet%s\n");
        p(ip4s_hdrops, "\t%u packet%s shorter than header shows\n");
        p(ip4s_pdrops, "\t%u packet%s dropped due to policy\n");
        p(ip4s_spoof, "\t%u packet%s with possibly spoofed local addresses\n");
        p(ip4s_notip4, "\t%u packet%s with internal header not IPv4 received\n");
        p(ip4s_qfull, "\t%u packet%s were dropped due to full output queue\n");
	p(ip4s_ibytes, "\t%qu input byte%s\n");
	p(ip4s_obytes, "\t%qu output byte%s\n");

#undef p
}
