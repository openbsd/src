/*	$OpenBSD: pk_extern.h,v 1.1 1996/03/04 07:36:40 niklas Exp $	*/
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

/* pk_acct.c */
int pk_accton __P((char *));
void pk_acct __P((struct pklcd *));

/* pk_debug.c */
void pk_trace __P((struct x25config *, struct mbuf *, char *));
void mbuf_cache __P((struct mbuf_cache *, struct mbuf *));

/* pk_input.c */
void ccittintr __P((void));
struct pkcb *pk_newlink __P((struct x25_ifaddr *, caddr_t));
int pk_dellink __P((struct pkcb *));
int pk_resize __P((struct pkcb *));
void *pk_ctlinput __P((int, struct sockaddr *, void *));
void pkintr __P((void));
void pk_input __P((struct mbuf *, ...));
void pk_simple_bsd __P((octet *, octet *, int, int));
void pk_from_bcd __P((struct x25_calladdr *, int, struct sockaddr_x25 *,
		      struct x25config *));
void pk_incoming_call __P((struct pkcb *, struct mbuf *));
void pk_call_accepted __P((struct pklcd *, struct mbuf *));
void pk_parse_facilities __P((octet *, struct sockaddr_x25 *));

/* pk_llcsubr.c */
int cons_rtrequest __P((int, struct rtentry *, struct sockaddr *));
struct rtentry *npaidb_enter __P((struct sockaddr_dl *, struct sockaddr *,
				  struct rtentry *, struct llc_linkcb *));
struct rtentry *npaidb_enrich __P((short, caddr_t, struct sockaddr_dl *));
int npaidb_destroy __P((struct rtentry *));
long x25_llcglue __P((int, struct sockaddr *));

/* pk_output.c */
void pk_output __P((struct pklcd *));
struct mbuf *nextpk __P((struct pklcd *));

/* pk_subr.c */
struct pklcd *pk_attach __P((struct socket *));
void pk_disconnect __P((struct pklcd *));
void pk_close __P((struct pklcd *));
struct mbuf *pk_template __P((int, int));
void pk_restart __P((struct pkcb *, int));
void pk_freelcd __P((struct pklcd *));
int pk_bind __P((struct pklcd *, struct mbuf *));
int pk_listen __P((struct pklcd *));
int pk_protolisten __P((int, int, int (*)(struct mbuf *, void *)));
void pk_assoc __P((struct pkcb *, struct pklcd *, struct sockaddr_x25 *));
int pk_connect __P((struct pklcd *, struct sockaddr_x25 *));
void pk_callcomplete __P((struct pkcb *));
void pk_callrequest __P((struct pklcd *, struct sockaddr_x25 *,
			 struct x25config *));
void pk_build_facilities __P((struct mbuf *, struct sockaddr_x25 *, int));
int to_bcd __P((struct bcdinfo *, struct sockaddr_x25 *, struct x25config *));
int pk_getlcn __P((struct pkcb *));
void pk_clear __P((struct pklcd *, int, int));
void pk_flowcontrol __P((struct pklcd *, int, int));
void pk_flush __P((struct pklcd *));
void pk_procerror __P((int, struct pklcd *, char *, int));
int pk_ack __P((struct pklcd *, unsigned));
int pk_decode __P((struct x25_packet *));
void pk_restartcause __P((struct pkcb *, struct x25_packet *));
void pk_resetcause __P((struct pkcb *, struct x25_packet *));
void pk_clearcause __P((struct pkcb *, struct x25_packet *));
char *format_ntn __P((struct x25config *));
void pk_message __P((int, struct x25config *, char *, ...));
int pk_fragment __P((struct pklcd *, struct mbuf *, int, int, int));

/* pk_timer.c */
void pk_timer __P((void));

/* pk_usrreq.c */
int pk_usrreq __P((struct socket *, int, struct mbuf *, struct mbuf *,
		   struct mbuf *));
int pk_start __P((struct pklcd *));
int pk_control __P((struct socket *, u_long, caddr_t, struct ifnet *));
int pk_ctloutput __P((int, struct socket *, int, int, struct mbuf **));
int pk_checksockaddr __P((struct mbuf *));
int pk_send __P((struct mbuf *, void *));

#endif
