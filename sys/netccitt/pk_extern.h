/*	$OpenBSD: pk_extern.h,v 1.5 2002/03/14 03:16:11 millert Exp $	*/
/*	$NetBSD: pk_extern.h,v 1.1 1996/02/13 22:05:17 christos Exp $	*/

/*
 * Copyright (c) 1995 Christos Zoulas.  All rights reserved.
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
 *	This product includes software developed by Christos Zoulas.
 * 4. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifdef _KERNEL
struct pklcd;
struct mbuf_cache;
struct x25_ifaddr;
struct x25_calladdr;
struct x25_packet;
struct llc_linkcb;
struct bcdinfo;
struct sockaddr_dl;
struct rt_addrinfo;

/* pk_acct.c */
int pk_accton(char *);
void pk_acct(struct pklcd *);

/* pk_debug.c */
void pk_trace(struct x25config *, struct mbuf *, char *);
void mbuf_cache(struct mbuf_cache *, struct mbuf *);

/* pk_input.c */
void ccittintr(void);
struct pkcb *pk_newlink(struct x25_ifaddr *, caddr_t);
int pk_dellink(struct pkcb *);
int pk_resize(struct pkcb *);
void *pk_ctlinput(int, struct sockaddr *, void *);
void pkintr(void);
void pk_input(struct mbuf *, ...);
void pk_simple_bsd(octet *, octet *, int, int);
void pk_from_bcd(struct x25_calladdr *, int, struct sockaddr_x25 *,
		      struct x25config *);
void pk_incoming_call(struct pkcb *, struct mbuf *);
void pk_call_accepted(struct pklcd *, struct mbuf *);
void pk_parse_facilities(octet *, struct sockaddr_x25 *);

/* pk_llcsubr.c */
void cons_rtrequest(int, struct rtentry *, struct rt_addrinfo *);
struct rtentry *npaidb_enter(struct sockaddr_dl *, struct sockaddr *,
				  struct rtentry *, struct llc_linkcb *);
struct rtentry *npaidb_enrich(short, caddr_t, struct sockaddr_dl *);
int npaidb_destroy(struct rtentry *);
long x25_llcglue(int, struct sockaddr *);

/* pk_output.c */
void pk_output(struct pklcd *);
struct mbuf *nextpk(struct pklcd *);

/* pk_subr.c */
struct pklcd *pk_attach(struct socket *);
void pk_disconnect(struct pklcd *);
void pk_close(struct pklcd *);
struct mbuf *pk_template(int, int);
void pk_restart(struct pkcb *, int);
void pk_freelcd(struct pklcd *);
int pk_bind(struct pklcd *, struct mbuf *);
int pk_listen(struct pklcd *);
int pk_protolisten(int, int, int (*)(struct mbuf *, void *));
void pk_assoc(struct pkcb *, struct pklcd *, struct sockaddr_x25 *);
int pk_connect(struct pklcd *, struct sockaddr_x25 *);
void pk_callcomplete(struct pkcb *);
void pk_callrequest(struct pklcd *, struct sockaddr_x25 *,
			 struct x25config *);
void pk_build_facilities(struct mbuf *, struct sockaddr_x25 *, int);
int to_bcd(struct bcdinfo *, struct sockaddr_x25 *, struct x25config *);
int pk_getlcn(struct pkcb *);
void pk_clear(struct pklcd *, int, int);
void pk_flowcontrol(struct pklcd *, int, int);
void pk_flush(struct pklcd *);
void pk_procerror(int, struct pklcd *, char *, int);
int pk_ack(struct pklcd *, unsigned);
int pk_decode(struct x25_packet *);
void pk_restartcause(struct pkcb *, struct x25_packet *);
void pk_resetcause(struct pkcb *, struct x25_packet *);
void pk_clearcause(struct pkcb *, struct x25_packet *);
char *format_ntn(struct x25config *);
void pk_message(int, struct x25config *, char *, ...);
int pk_fragment(struct pklcd *, struct mbuf *, int, int, int);

/* pk_timer.c */
void pk_timer(void);

/* pk_usrreq.c */
int pk_usrreq(struct socket *, int, struct mbuf *, struct mbuf *,
		   struct mbuf *);
int pk_start(struct pklcd *);
int pk_control(struct socket *, u_long, caddr_t, struct ifnet *);
int pk_ctloutput(int, struct socket *, int, int, struct mbuf **);
int pk_checksockaddr(struct mbuf *);
int pk_send(struct mbuf *, void *);

#endif
