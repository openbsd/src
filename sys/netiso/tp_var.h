/*	$OpenBSD: tp_var.h,v 1.1 1996/03/04 10:36:47 mickey Exp $	*/
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
int tpcons_pcbconnect __P((void *, struct mbuf *));
void *tpcons_ctlinput __P((int, struct sockaddr *, void *));
void tpcons_input __P((struct mbuf *, ...));
int tpcons_output __P((struct mbuf *, ...));
int tpcons_output_dg __P((struct mbuf *, ...));

/* tp_driver.c */
int tp_driver   __P((struct tp_pcb *, struct tp_event *));

/* tp_emit.c */
int tp_emit __P((int, struct tp_pcb *, SeqNum, u_int, struct mbuf *));
int tp_error_emit __P((int, u_long, struct sockaddr_iso *,
		       struct sockaddr_iso *, struct mbuf *, int,
		       struct tp_pcb *, caddr_t, 
		       int (*) (struct mbuf *, ...)));

/* tp_inet.c */
void in_getsufx  __P((void *, u_short *, caddr_t, int));
void in_putsufx __P((void *, caddr_t, int, int));
void in_recycle_tsuffix __P((void *));
void in_putnetaddr __P((void *, struct sockaddr *, int));
int in_cmpnetaddr __P((void *, struct sockaddr *, int));
void in_getnetaddr __P((void *, struct mbuf *, int));
int tpip_mtu    __P((void *));
int tpip_output __P((struct mbuf *, ...));
int tpip_output_dg __P((struct mbuf *, ...));
void tpip_input __P((struct mbuf *, ...));
void tpin_quench __P((struct inpcb *, int));
void *tpip_ctlinput __P((int, struct sockaddr *, void *));
void tpin_abort __P((struct inpcb *, int));
void dump_inaddr __P((struct sockaddr_in *));

/* tp_input.c */
struct mbuf    *tp_inputprep __P((struct mbuf *));
void tp_input   __P((struct mbuf *, ...));
int tp_headersize __P((int, struct tp_pcb *));

/* tp_iso.c */
void iso_getsufx __P((void *, u_short *, caddr_t, int));
void iso_putsufx __P((void *, caddr_t, int, int));
void iso_recycle_tsuffix __P((void *));
void iso_putnetaddr __P((void *, struct sockaddr *, int));
int iso_cmpnetaddr __P((void *, struct sockaddr *, int));
void iso_getnetaddr __P((void *, struct mbuf *, int));
int tpclnp_mtu  __P((void *));
int tpclnp_output __P((struct mbuf *, ...));
int tpclnp_output_dg __P((struct mbuf *, ...));
void tpclnp_input __P((struct mbuf *, ...));
void iso_rtchange __P((struct isopcb *));
void tpiso_decbit __P((struct isopcb *));
void tpiso_quench __P((struct isopcb *));
void *tpclnp_ctlinput __P((int, struct sockaddr *, void *));
void tpclnp_ctlinput1 __P((int, struct iso_addr *));
void tpiso_abort __P((struct isopcb *));
void tpiso_reset __P((struct isopcb *));

/* tp_meas.c */
void Tpmeas __P((u_int, u_int, struct timeval *, u_int, u_int, u_int));

/* tp_output.c */
int tp_consistency __P((struct tp_pcb *, u_int, struct tp_conn_param *));
int tp_ctloutput __P((int, struct socket *, int, int, struct mbuf **));

/* tp_pcb.c */
void tp_init    __P((void));
void tp_soisdisconnecting __P((struct socket *));
void tp_soisdisconnected __P((struct tp_pcb *));
void tp_freeref __P((RefNum));
u_long tp_getref __P((struct tp_pcb *));
int tp_set_npcb __P((struct tp_pcb *));
int tp_attach   __P((struct socket *, long));
void tp_detach  __P((struct tp_pcb *));
int tp_tselinuse __P((int, caddr_t, struct sockaddr_iso *, int));
int tp_pcbbind  __P((void *, struct mbuf *));

/* tp_subr.c */
int tp_goodXack __P((struct tp_pcb *, SeqNum));
void tp_rtt_rtv __P((struct tp_pcb *));
int tp_goodack  __P((struct tp_pcb *, u_int, SeqNum, u_int));
int tp_sbdrop   __P((struct tp_pcb *, SeqNum));
void tp_send    __P((struct tp_pcb *));
int tp_packetize __P((struct tp_pcb *, struct mbuf *, int));
int tp_stash    __P((struct tp_pcb *, struct tp_event *));
void tp_rsyflush __P((struct tp_pcb *));
void tp_rsyset   __P((struct tp_pcb *));
void tpsbcheck   __P((struct tp_pcb *, int));

/* tp_subr2.c */
void tp_local_credit __P((struct tp_pcb *));
int tp_protocol_error __P((struct tp_event *, struct tp_pcb *));
void tp_drain   __P((void));
void tp_indicate __P((int, struct tp_pcb *, u_short));
void tp_getoptions __P((struct tp_pcb *));
void tp_recycle_tsuffix __P((void *));
void tp_quench  __P((struct inpcb *, int));
void tp_netcmd   __P((struct tp_pcb *, int));
int tp_mask_to_num __P((u_char));
void tp_mss     __P((struct tp_pcb *, int));
int tp_route_to __P((struct mbuf *, struct tp_pcb *, caddr_t));
void tp0_stash  __P((struct tp_pcb *, struct tp_event *));
void tp0_openflow __P((struct tp_pcb *));
int tp_setup_perf __P((struct tp_pcb *));
void dump_addr   __P((struct sockaddr *));
void Dump_buf    __P((caddr_t, int));

/* tp_timer.c */
void tp_timerinit __P((void));
void tp_etimeout __P((struct tp_pcb *, int, int));
void tp_euntimeout __P((struct tp_pcb *, int));
void tp_slowtimo __P((void));
void tp_data_retrans __P((struct tp_pcb *));
void tp_fasttimo __P((void));
void tp_ctimeout __P((struct tp_pcb *, int, int));
void tp_ctimeout_MIN __P((struct tp_pcb *, int, int));
void tp_cuntimeout __P((struct tp_pcb *, int));

/* tp_trace.c */
void tpTrace    __P((struct tp_pcb *, u_int, u_int, u_int, u_int, u_int,
		     u_int));

/* tp_usrreq.c */
void dump_mbuf  __P((struct mbuf *, char *));
int tp_rcvoob   __P((struct tp_pcb *, struct socket *, struct mbuf *,
		     int *, int));
int tp_sendoob  __P((struct tp_pcb *, struct socket *, struct mbuf *, int *));
int tp_usrreq   __P((struct socket *, int, struct mbuf *, struct mbuf *,
		     struct mbuf *));
void tp_ltrace   __P((struct socket *, struct uio *));
int tp_confirm  __P((struct tp_pcb *));
int tp_snd_control __P((struct mbuf *, struct socket *, struct mbuf **));

#ifdef TPCONS
/* if_cons.c */
void nibble_copy __P((char *, unsigned, char *, unsigned, int));
int nibble_match __P((char *, unsigned, char *, unsigned, int));
void cons_init __P((void));
int tp_incoming __P((struct mbuf *, void *));
int cons_tpinput __P((struct mbuf *, void *));
int cons_connect __P((struct isopcb *));
void *cons_ctlinput __P((int, struct sockaddr *, void *));
int find_error_reason __P((struct x25_packet *));
#endif

#endif
