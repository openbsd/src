/*      $OpenBSD: ip_gre.c,v 1.74 2022/06/26 15:50:21 mvs Exp $ */
/*	$NetBSD: ip_gre.c,v 1.9 1999/10/25 19:18:11 drochner Exp $ */

/*
 * Copyright (c) 1998 The NetBSD Foundation, Inc.
 * All rights reserved.
 *
 * This code is derived from software contributed to The NetBSD Foundation
 * by Heiko W.Rupp <hwr@pilhuhn.de>
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
 * THIS SOFTWARE IS PROVIDED BY THE NETBSD FOUNDATION, INC. AND CONTRIBUTORS
 * ``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED
 * TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE FOUNDATION OR CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

/*
 * decapsulate tunneled packets and send them on
 * output half is in net/if_gre.[ch]
 * This currently handles IPPROTO_GRE, IPPROTO_MOBILE
 */


#include "gre.h"
#if NGRE > 0

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/mbuf.h>
#include <sys/protosw.h>
#include <sys/socket.h>
#include <sys/socketvar.h>
#include <sys/sysctl.h>

#include <net/if.h>
#include <net/route.h>

#include <netinet/in.h>
#include <netinet/ip.h>
#include <netinet/ip_var.h>
#include <netinet/in_pcb.h>

#ifdef PIPEX
#include <net/pipex.h>
#endif

int
gre_usrreq(struct socket *so, int req, struct mbuf *m, struct mbuf *nam,
    struct mbuf *control, struct proc *p)
{
#ifdef  PIPEX 
	struct inpcb *inp = sotoinpcb(so);

	if (inp != NULL && inp->inp_pipex && req == PRU_SEND) {
		struct sockaddr_in *sin4;
		struct in_addr *ina_dst;

		ina_dst = NULL;
		if ((so->so_state & SS_ISCONNECTED) != 0) {
			inp = sotoinpcb(so);
			if (inp)
				ina_dst = &inp->inp_laddr;
		} else if (nam) {
			if (in_nam2sin(nam, &sin4) == 0)
				ina_dst = &sin4->sin_addr;
		}
		if (ina_dst != NULL) {
			struct pipex_session *session;

			session = pipex_pptp_userland_lookup_session_ipv4(m,
			    *ina_dst);

			if(session != NULL) {
				m = pipex_pptp_userland_output(m, session);
				pipex_rele_session(session);
			}
		}

		if (m == NULL)
			return (ENOMEM);
	}
#endif
	return rip_usrreq(so, req, m, nam, control, p);
}
#endif /* if NGRE > 0 */
