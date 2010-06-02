/*	$OpenBSD: mpls_raw.c,v 1.6 2010/06/02 15:41:07 claudio Exp $	*/

/*
 * Copyright (C) 1999, 2000 and 2001 AYAME Project, WIDE Project.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
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

#include <sys/param.h>
#include <sys/malloc.h>
#include <sys/mbuf.h>
#include <sys/errno.h>
#include <sys/protosw.h>
#include <sys/sockio.h>
#include <sys/socket.h>
#include <sys/socketvar.h>
#include <sys/systm.h>
#include <sys/proc.h>
#include <sys/sysctl.h>

#include <net/if.h>
#include <net/if_types.h>
#include <net/route.h>

#include <netmpls/mpls.h>

#define MPLS_RAW_SNDQ	8192
#define MPLS_RAW_RCVQ	8192

u_long mpls_raw_sendspace = MPLS_RAW_SNDQ;
u_long mpls_raw_recvspace = MPLS_RAW_RCVQ;

int mpls_defttl = 255;
int mpls_inkloop = 16;
int mpls_push_expnull_ip = 0;
int mpls_push_expnull_ip6 = 0;
int mpls_mapttl_ip = 1;
int mpls_mapttl_ip6 = 0;

int *mplsctl_vars[MPLSCTL_MAXID] = MPLSCTL_VARS;

int	mpls_control(struct socket *, u_long, caddr_t, struct ifnet *);

/*
 * Generic MPLS control operations (ioctl's).
 * Ifp is 0 if not an interface-specific ioctl.
 */
/* ARGSUSED */
int
mpls_control(struct socket *so, u_long cmd, caddr_t data, struct ifnet *ifp)
{
	return (EOPNOTSUPP);
}

int
mpls_raw_usrreq(struct socket *so, int req, struct mbuf *m, struct mbuf *nam,
    struct mbuf *control, struct proc *p)
{
	int error = 0;

#ifdef MPLS_DEBUG
	printf("mpls_raw_usrreq: called! (reqid=%d).\n", req);
#endif	/* MPLS_DEBUG */

	if (req == PRU_CONTROL)
		return (mpls_control(so, (u_long)m, (caddr_t)nam,
		    (struct ifnet *)control));

	switch (req) {
	case PRU_ATTACH:
		if (so->so_snd.sb_hiwat == 0 || so->so_rcv.sb_hiwat == 0) {
			error = soreserve(so, mpls_raw_sendspace,
				mpls_raw_recvspace);
			if (error)
				break;
		}
		break;

	case PRU_DETACH:
	case PRU_BIND:
	case PRU_LISTEN:
	case PRU_CONNECT:
	case PRU_CONNECT2:
	case PRU_DISCONNECT:
	case PRU_SHUTDOWN:
	case PRU_RCVD:
	case PRU_SEND:
	case PRU_SENSE:
	case PRU_RCVOOB:
	case PRU_SENDOOB:
	case PRU_SOCKADDR:
	case PRU_PEERADDR:
		error = EOPNOTSUPP;
		break;

	default:
		panic("rip_usrreq");
	}

	return (error);
}

int
mpls_sysctl(int *name, u_int namelen, void *oldp, size_t *oldlenp, void *newp,
    size_t newlen)
{
	if (name[0] >= MPLSCTL_MAXID)
		return (EOPNOTSUPP);

	/* Almost all sysctl names at this level are terminal. */
	if (namelen != 1 && name[0] != MPLSCTL_IFQUEUE)
		return (ENOTDIR);

	switch (name[0]) {
	case MPLSCTL_IFQUEUE:
		return (sysctl_ifq(name + 1, namelen - 1,
		    oldp, oldlenp, newp, newlen, &mplsintrq));
	default:
		return sysctl_int_arr(mplsctl_vars, name, namelen,
		    oldp, oldlenp, newp, newlen);
	}
}
