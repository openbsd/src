/*	$OpenBSD: tp_var.h,v 1.3 2002/03/14 03:16:12 millert Exp $	*/
/*	$NetBSD: tp_var.h,v 1.1 1996/02/13 22:12:29 christos Exp $	*/

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
struct isopcb;
struct mbuf;
struct sockaddr_iso;
struct tp_pcb;
struct sockaddr_in;
struct iso_addr;
struct tp_conn_param;
struct tp_event;
struct inpcb;
struct route;
struct pklcd;
struct sockaddr;
struct x25_packet;
struct in_addr;


/* tp_cons.c */
int tpcons_pcbconnect(void *, struct mbuf *);
void *tpcons_ctlinput(int, struct sockaddr *, void *);
void tpcons_input(struct mbuf *, ...);
int tpcons_output(struct mbuf *, ...);
int tpcons_output_dg(struct mbuf *, ...);

/* tp_driver.c */
int tp_driver(struct tp_pcb *, struct tp_event *);

/* tp_emit.c */
int tp_emit(int, struct tp_pcb *, SeqNum, u_int, struct mbuf *);
int tp_error_emit(int, u_long, struct sockaddr_iso *,
		  struct sockaddr_iso *, struct mbuf *, int,
		  struct tp_pcb *, caddr_t, 
		  int (*) (struct mbuf *, ...));

/* tp_inet.c */
void in_getsufx(void *, u_short *, caddr_t, int);
void in_putsufx(void *, caddr_t, int, int);
void in_recycle_tsuffix(void *);
void in_putnetaddr(void *, struct sockaddr *, int);
int in_cmpnetaddr(void *, struct sockaddr *, int);
void in_getnetaddr(void *, struct mbuf *, int);
int tpip_mtu(void *);
int tpip_output(struct mbuf *, ...);
int tpip_output_dg(struct mbuf *, ...);
void tpip_input(struct mbuf *, ...);
void tpin_quench(struct inpcb *, int);
void *tpip_ctlinput(int, struct sockaddr *, void *);
void tpin_abort(struct inpcb *, int);
void dump_inaddr(struct sockaddr_in *);

/* tp_input.c */
struct mbuf    *tp_inputprep(struct mbuf *);
void tp_input(struct mbuf *, ...);
int tp_headersize(int, struct tp_pcb *);

/* tp_iso.c */
void iso_getsufx(void *, u_short *, caddr_t, int);
void iso_putsufx(void *, caddr_t, int, int);
void iso_recycle_tsuffix(void *);
void iso_putnetaddr(void *, struct sockaddr *, int);
int iso_cmpnetaddr(void *, struct sockaddr *, int);
void iso_getnetaddr(void *, struct mbuf *, int);
int tpclnp_mtu(void *);
int tpclnp_output(struct mbuf *, ...);
int tpclnp_output_dg(struct mbuf *, ...);
void tpclnp_input(struct mbuf *, ...);
void iso_rtchange(struct isopcb *);
void tpiso_decbit(struct isopcb *);
void tpiso_quench(struct isopcb *);
void *tpclnp_ctlinput(int, struct sockaddr *, void *);
void tpclnp_ctlinput1(int, struct iso_addr *);
void tpiso_abort(struct isopcb *);
void tpiso_reset(struct isopcb *);

/* tp_meas.c */
void Tpmeas(u_int, u_int, struct timeval *, u_int, u_int, u_int);

/* tp_output.c */
int tp_consistency(struct tp_pcb *, u_int, struct tp_conn_param *);
int tp_ctloutput(int, struct socket *, int, int, struct mbuf **);

/* tp_pcb.c */
void tp_init(void);
void tp_soisdisconnecting(struct socket *);
void tp_soisdisconnected(struct tp_pcb *);
void tp_freeref(RefNum);
u_long tp_getref(struct tp_pcb *);
int tp_set_npcb(struct tp_pcb *);
int tp_attach(struct socket *, long);
void tp_detach(struct tp_pcb *);
int tp_tselinuse(int, caddr_t, struct sockaddr_iso *, int);
int tp_pcbbind(void *, struct mbuf *);

/* tp_subr.c */
int tp_goodXack(struct tp_pcb *, SeqNum);
void tp_rtt_rtv(struct tp_pcb *);
int tp_goodack(struct tp_pcb *, u_int, SeqNum, u_int);
int tp_sbdrop(struct tp_pcb *, SeqNum);
void tp_send(struct tp_pcb *);
int tp_packetize(struct tp_pcb *, struct mbuf *, int);
int tp_stash(struct tp_pcb *, struct tp_event *);
void tp_rsyflush(struct tp_pcb *);
void tp_rsyset(struct tp_pcb *);
void tpsbcheck(struct tp_pcb *, int);

/* tp_subr2.c */
void tp_local_credit(struct tp_pcb *);
int tp_protocol_error(struct tp_event *, struct tp_pcb *);
void tp_drain(void);
void tp_indicate(int, struct tp_pcb *, u_short);
void tp_getoptions(struct tp_pcb *);
void tp_recycle_tsuffix(void *);
void tp_quench(struct inpcb *, int);
void tp_netcmd(struct tp_pcb *, int);
int tp_mask_to_num(u_char);
void tp_mss(struct tp_pcb *, int);
int tp_route_to(struct mbuf *, struct tp_pcb *, caddr_t);
void tp0_stash(struct tp_pcb *, struct tp_event *);
void tp0_openflow(struct tp_pcb *);
int tp_setup_perf(struct tp_pcb *);
void dump_addr(struct sockaddr *);
void Dump_buf(caddr_t, int);

/* tp_timer.c */
void tp_timerinit(void);
void tp_etimeout(struct tp_pcb *, int, int);
void tp_euntimeout(struct tp_pcb *, int);
void tp_slowtimo(void);
void tp_data_retrans(struct tp_pcb *);
void tp_fasttimo(void);
void tp_ctimeout(struct tp_pcb *, int, int);
void tp_ctimeout_MIN(struct tp_pcb *, int, int);
void tp_cuntimeout(struct tp_pcb *, int);

/* tp_trace.c */
void tpTrace(struct tp_pcb *, u_int, u_int, u_int, u_int, u_int,
		     u_int);

/* tp_usrreq.c */
void dump_mbuf(struct mbuf *, char *);
int tp_rcvoob(struct tp_pcb *, struct socket *, struct mbuf *,
		     int *, int);
int tp_sendoob(struct tp_pcb *, struct socket *, struct mbuf *, int *);
int tp_usrreq(struct socket *, int, struct mbuf *, struct mbuf *,
		     struct mbuf *);
void tp_ltrace(struct socket *, struct uio *);
int tp_confirm(struct tp_pcb *);
int tp_snd_control(struct mbuf *, struct socket *, struct mbuf **);

#ifdef TPCONS
/* if_cons.c */
void nibble_copy(char *, unsigned, char *, unsigned, int);
int nibble_match(char *, unsigned, char *, unsigned, int);
void cons_init(void);
int tp_incoming(struct mbuf *, void *);
int cons_tpinput(struct mbuf *, void *);
int cons_connect(struct isopcb *);
void *cons_ctlinput(int, struct sockaddr *, void *);
int find_error_reason(struct x25_packet *);
#endif

#endif
