/*
 * Copyright (C) 1999, 2000 and 2001 AYAME Project, WIDE Project.
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

/*
 *
 *	$Id: mpls_lse.c,v 1.1 2008/04/23 11:00:35 norby Exp $
 */

#include <sys/param.h>
#include <sys/malloc.h>
#include <sys/mbuf.h>
#include <sys/errno.h>
#include <sys/protosw.h>
#include <sys/socket.h>
#include <sys/socketvar.h>
#include <sys/systm.h>

#include <net/if.h>
#include <net/if_types.h>
#include <net/route.h>

#include <netmpls/mpls.h>
#include <netmpls/mpls_var.h>

/*
 * MPLS Label Switching Engine.
 */

/*
 * Process a received MPLS packet;
 * the packet is in the mbuf chain m from mpls_{ether,...}.
 */
int
mpls_lse(struct ifnet *ifp, struct mbuf *m, struct sockaddr_mpls *dst,
    u_int8_t bosf)
{
	struct route mplsroute;
	struct route *ro = &mplsroute;
	struct sockaddr_mpls *rodst = satosmpls(&mplsroute.ro_dst); /* Actual labeland stuff */
	struct rtentry *rt = NULL;
	struct sockaddr_mpls *dstnew;
	struct sockaddr_mpls smpls;	 /* XXX adhoc XXX */
	u_int8_t exp_bits;		/* XXX adhoc XXX */
	int ink_loop, error = 0, bad = 0;

	bzero((caddr_t)ro, sizeof(*ro));
	bcopy((caddr_t)dst, (caddr_t)rodst, dst->smpls_len);

/* XXX: Kefren: I don't understand this inkloop so I've replaced it with asingle
	loop for easy break */
	for (ink_loop = 0; ink_loop < mpls_inkloop; ink_loop++) {
/*	for (ink_loop = 0; ink_loop < 1; ink_loop++) { */

#ifdef MPLS_DEBUG
		printf("MPLS_DEBUG: %s(%d). [label=%d]\n",
		    __FILE__, __LINE__, ntohl(rodst->smpls_in_label));
#endif	/* MPLS_DEBUG */

		/*
		 * OPERATIONS for Reserved Labels
		 */
		if (ntohl(rodst->smpls_in_label) <= MPLS_LABEL_RESERVED_MAX) {
			printf("MPLS_DEBUG: %s(%d). [bosf = %d label = %s]\n",
	    		    __FILE__, __LINE__, bosf,
	    		    ntohl(rodst->smpls_in_label));
			switch (ntohl(rodst->smpls_in_label)) {
#ifdef INET
			case MPLS_LABEL_IPV4NULL:
#if 0 /* claudio@ */
				if (bosf) {
					error = mpls_ip_input(m);
					goto done;
				}
#endif
#ifdef MPLS_DEBUG
#endif	/* MPLS_DEBUG */
				break;
#endif	/* INET */
#ifdef INET6
			case MPLS_LABEL_IPV6NULL:
#if 0 /* claudio@ */
				if (bosf) {
					error = mpls_ip6_input(m);
					goto done;
				}
#endif
				break;
#endif	/* INET6 */
			}

			/* label is reserved, */
			/* but operation is unsupported or invalid */
			error = EINVAL;
			printf("MPLS_DEBUG: %s(%d).\n", __FILE__, __LINE__);
			bad = 1;
			break;
		} /* MPLS RESERVED label */

		/*
		 * switch packet
		 *
		 * XXX: no really, this is fun. AYAME sucks so much. */
		exp_bits = rodst->smpls_out_exp;
		rodst->smpls_out_exp = 0;

		rtalloc(ro);
		rt = ro->ro_rt;
		bzero((caddr_t)ro, sizeof(*ro));

		if (rt == 0) {
			/* no entry for this label (silent discard) */
			/* as the scope_id you use is not set interface, etc. */
			/* error = EHOSTUNREACH; */
			error = 0;
#ifdef MPLS_DEBUG
			printf("MPLS_DEBUG: %s(%d).\n", __FILE__, __LINE__);
#endif	/* MPLS_DEBUG */
			bad = 1;
			break;
		}

		/*
		if ((mtu = rt->rt_rmx.rmx_mtu) == 0)
			mtu = ifp->if_mtu;
		*/


		/*
		 * label operations
		 */
		rt->rt_use++;
		if (rt->rt_flags & RTF_GATEWAY)	/* XXX */
			dstnew = satosmpls(rt->rt_gateway);
		else {
			/* XXX silent discard XXX */
			/* error = EHOSTUNREACH;  */
			error = 0;
#ifdef MPLS_DEBUG
			printf("MPLS_DEBUG: %s(%d).\n", __FILE__, __LINE__);
#endif	/* MPLS_DEBUG */
			bad = 1;
			break;
		}

#ifdef MPLS_MCAST
		printf("MPLS_MCAST label=%d mclabel=%d\n",
		       ntohl(dstnew->smpls_in_label) & 0x000fffff,
		       ntohl(dstnew->smpls_mclabel));
		/*
		 * if gw_entry have next mplsmc entry,
		 * call mpls_lse again.
		 */
		if (dstnew->smpls_mclabel != 0) {
			struct sockaddr_mpls smpmpls;
			struct mbuf *nm;
			memset(&smpmpls, 0, sizeof(smpmpls));
			smpmpls.smpls_len = sizeof(smpmpls);
			smpmpls.smpls_family = AF_MPLS;
			smpmpls.smpls_in_label = dstnew->smpls_mclabel;
#ifdef MPLS_DEBUG
			printf("MPLS_MCAST label=%d mclabel=%d\n",
			       ntohl(dstnew->smpls_in_label) & 0x000fffff,
			       ntohl(dstnew->smpls_mclabel));
#endif /* MPLS_DEBUG */
			nm = m_copym(m, 0, M_COPYALL, M_DONTWAIT);
			if (nm != NULL) {
#ifdef MPLS_DEBUG
				printf("MPLS_MCAST dup\n");
#endif /* MPLS_DEBUG */
				mpls_lse(ifp, nm, &smpmpls, bosf);
				m_freem(nm);
			}
		}
#endif /* MPLS_MCAST */

		if (dstnew->smpls_family != AF_MPLS) {
			if (bosf) {
				/* send to L3? */
				switch (dstnew->smpls_family) {
#if 0 /* claudio@ */
#ifdef INET
				case AF_INET:
					error = mpls_ip_input(m);
					goto done;
#endif	/* INET */
#ifdef INET6
				case AF_INET6:
					error = mpls_ip6_input(m);
					goto done;
#endif	/* INET6 */
#endif
				default:
					error = EAFNOSUPPORT;
#ifdef MPLS_DEBUG
					printf("MPLS_DEBUG: %s(%d).\n", __FILE__, __LINE__);
#endif	/* MPLS_DEBUG */
					bad = 1;
				}
				break;
			} else {
				/* XXX */
				error = EINVAL;
#ifdef MPLS_DEBUG
				printf("MPLS_DEBUG: %s(%d).\n", __FILE__, __LINE__);
#endif	/* MPLS_DEBUG */
				bad = 1;
				break;
			}
		}

		/* XXX */
		smpls = *dstnew;
		smpls.smpls_out_exp = exp_bits;
		dstnew = &smpls;
		/* XXX Kefren */
/*		dstnew->smpls_exp = exp_bits;*/

		switch(dstnew->smpls_operation) {

		case MPLS_OP_POP: 			/* Label Pop */

			/* check bos flag */
#if 0 /* claudio@ */
			if (bosf) {
				/*
				 * XXXXX No ExpNULL XXXXX
				 *
				 * I can't know which l3 I shuld send to!!
				 * now, send to IPv4 stack (XXXXX)
				 * We have to 'look' into the packet
				 * and clasify it
				 *
				 * error = EINVAL;
				 * goto bad;
				 */

				error = mpls_ip_input(m);
				goto done;
			}
#endif
			/* label pop */
			if ((m = mpls_shim_pop(m, rodst, NULL, &bosf, NULL)) == 0) {
				error = ENOBUFS;
				goto done;
			}

			break;

		case MPLS_OP_PUSH: 			/* Label Push */

			if ((m = mpls_shim_push(m, dstnew, NULL, NULL, NULL)) == 0) {
				error = ENOBUFS;
				goto done;
			}
			bosf = 0;

			break;

		case MPLS_OP_SWAP:	/* Label Swap */
		default:

			if ((m = mpls_shim_swap(m, dstnew, NULL)) == 0) {
				error = ENOBUFS;
				goto done;
			}
		}

		ifp = rt->rt_ifp;
		/* send to L2 : select outgoing i/f */
		switch (ifp->if_type) {
		case IFT_ETHER:
		case IFT_ATM:		/* now only support scope 0 */
		case IFT_GIF:		/* now only support scope 0 */
			error = (*ifp->if_output)(ifp, m, smplstosa(dstnew), rt);
			goto done;
		case IFT_LOOP:
			break;
		default:
			/* not supported yet: discard */
#ifdef MPLS_DEBUG
			printf("mpls_lse: interface type not supported yet!\n");
#endif	/* MPLS_DEBUG */
			error = EHOSTUNREACH;
			bad=1;
		}
		if (bad) break;
		if (rt) {
			RTFREE(rt);
			rt = 0;
		}
	} /* The big inkloop for */

	/*
	 * Discard broken packet
	 */
if (bad) {
#ifdef MPLS_DEBUG
	printf("MPLS_DEBUG: %s(%d) [packet discard!!].\n", __FILE__, __LINE__);
#endif	/* MPLS_DEBUG */
	m_freem(m);
}

done:
	if (rt)
		RTFREE(rt);
	return(error);
}
