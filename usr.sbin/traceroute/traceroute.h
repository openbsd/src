/*	$OpenBSD: traceroute.h,v 1.1 2016/09/03 22:00:06 benno Exp $	*/
/*	$NetBSD: traceroute.c,v 1.10 1995/05/21 15:50:45 mycroft Exp $	*/

/*
 * Copyright (C) 1995, 1996, 1997, and 1998 WIDE Project.
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
 * 3. Neither the name of the project nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE PROJECT AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE PROJECT OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

/*-
 * Copyright (c) 1990, 1993
 *	The Regents of the University of California.  All rights reserved.
 *
 * This code is derived from software contributed to Berkeley by
 * Van Jacobson.
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


#include <netinet/ip_var.h>
#include <netmpls/mpls.h>
#include <sys/cdefs.h>
#include <sys/types.h>

#define DUMMY_PORT 10010

#define MAX_LSRR		((MAX_IPOPTLEN - 4) / 4)

#define MPLS_LABEL(m)		((m & MPLS_LABEL_MASK) >> MPLS_LABEL_OFFSET)
#define MPLS_EXP(m)		((m & MPLS_EXP_MASK) >> MPLS_EXP_OFFSET)

/*
 * Format of the data in a (udp) probe packet.
 */
struct packetdata {
	u_char seq;		/* sequence number of this packet */
	u_int8_t ttl;		/* ttl packet left with */
	u_char pad[2];
	u_int32_t sec;		/* time packet left */
	u_int32_t usec;
} __packed;

extern struct in_addr	 gateway[MAX_LSRR + 1];
extern int		 lsrrlen;
extern int32_t		 sec_perturb;
extern int32_t		 usec_perturb;

extern u_char		 packet[512];
extern u_char		*outpacket;	/* last inbound (icmp) packet */

int		 wait_for_reply(int, struct msghdr *);
void		 dump_packet(void);
void		 build_probe4(int, u_int8_t, int);
void		 build_probe6(int, u_int8_t, int, struct sockaddr *);
void		 send_probe(int, u_int8_t, int, struct sockaddr *);
struct udphdr	*get_udphdr(struct ip6_hdr *, u_char *);
int		 packet_ok(int, struct msghdr *, int, int, int);
int		 packet_ok4(struct msghdr *, int, int, int);
int		 packet_ok6(struct msghdr *, int, int, int);
void		 icmp_code(int, int, int *, int *);
void		 icmp4_code(int, int *, int *);
void		 icmp6_code(int, int *, int *);
void		 dump_packet(void);
void		 print_exthdr(u_char *, int);
void		 check_tos(struct ip*);
void		 print(struct sockaddr *, int, const char *);
const char	*inetname(struct sockaddr*);
void		 print_asn(struct sockaddr_storage *);
u_short		 in_cksum(u_short *, int);
char		*pr_type(u_int8_t);
int		 map_tos(char *, int *);
double		 deltaT(struct timeval *, struct timeval *);

void		 gettime(struct timeval *);

extern int		 rcvsock;  /* receive (icmp) socket file descriptor */
extern int		 sndsock;  /* send (udp) socket file descriptor */

extern int		 rcvhlim;
extern struct in6_pktinfo *rcvpktinfo;

extern int		 datalen;  /* How much data */

extern char		*hostname;

extern u_short		 ident;
extern u_int16_t	 srcport;
extern u_int16_t	 port;	   /* start udp dest port # for probe packets */
extern u_char		 proto;

#define ICMP_CODE 0;

extern int verbose;
extern int waittime;		/* time to wait for response (in seconds) */
extern int nflag;		/* print addresses numerically */
extern int dump;
extern int Aflag;		/* lookup ASN */
extern int last_tos;

extern char *__progname;
