/*	$OpenBSD: mpls_shim.c,v 1.1 2008/04/23 11:00:35 norby Exp $	*/

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
#include <sys/socket.h>
#include <sys/systm.h>

#include <net/if.h>
#include <net/route.h>

#include <netmpls/mpls.h>

struct mbuf *
mpls_shim_pop(struct mbuf *m)
{
	/* shaves off top shim header from mbuf */
	m_adj(m, sizeof(struct shim_hdr));

	/* catch-up next shim_hdr */
	if (m->m_len < sizeof(struct shim_hdr))
		if ((m = m_pullup(m, sizeof(struct shim_hdr))) == 0)
			return (NULL);

	/* return mbuf */
	return (m);
}

struct mbuf *
mpls_shim_swap(struct mbuf *m, struct sockaddr_mpls *smplsp)
{
	struct shim_hdr *shim;

	/* pullup shim_hdr */
	if (m->m_len < sizeof(struct shim_hdr))
		if ((m = m_pullup(m, sizeof(struct shim_hdr))) == 0)
			return (NULL);
	shim = mtod(m, struct shim_hdr *);

	/* swap label */
	shim->shim_label &= ~MPLS_LABEL_MASK;
	shim->shim_label |= smplsp->smpls_out_label & MPLS_LABEL_MASK;

	/* swap exp : XXX exp override */
	{
		u_int32_t	t;

		shim->shim_label &= ~MPLS_EXP_MASK;
		t = smplsp->smpls_out_exp << MPLS_EXP_OFFSET;
		shim->shim_label |= htonl(t) & MPLS_EXP_MASK;
	}
	shim->shim_label = htonl(shim->shim_label);

	return (m);
}

struct mbuf *
mpls_shim_push(struct mbuf *m, struct sockaddr_mpls *smplsp)
{
	struct shim_hdr *shim;

	M_PREPEND(m, sizeof(struct shim_hdr), M_DONTWAIT);
	if (m == 0)
		return (NULL);

	shim = mtod(m, struct shim_hdr *);
	bzero((caddr_t)shim, sizeof(*shim));

	return (mpls_shim_swap(m, smplsp));
}
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
 *	$Id: mpls_shim.c,v 1.1 2008/04/23 11:00:35 norby Exp $
 */

#include <sys/param.h>
#include <sys/malloc.h>
#include <sys/mbuf.h>
#include <sys/errno.h>
#include <sys/socket.h>
#include <sys/systm.h>

#include <net/if.h>
#include <net/route.h>

#include <netmpls/mpls.h>
#include <netmpls/mpls_var.h>

struct mbuf *
mpls_shim_pop(struct mbuf *m, struct sockaddr_mpls *smplsp, u_int32_t *labelp,
    u_int8_t *bosp, u_int8_t *ttlp)
{
	u_int32_t label;

	/* shaves off top shim header from mbuf */
	m_adj(m, sizeof(struct shim_hdr));

	/* catch-up next shim_hdr */
	if (m->m_len < sizeof(struct shim_hdr))
		if ((m = m_pullup(m, sizeof(struct shim_hdr))) == 0)
			return(0);

	label = ntohl(*mtod(m, u_int32_t *));

	/* set each values, if need */
	if (bosp)
		*bosp = MPLS_SHIM_BOS_ISSET(label);
	if (ttlp)
		*ttlp = MPLS_SHIM_TTL_GET(label);
	if (labelp)
		*labelp = MPLS_SHIM_LABEL_GET(label);
	if (smplsp) {
		bzero(smplsp, sizeof(*smplsp));
		smplsp->smpls_family = AF_MPLS;
		smplsp->smpls_len = sizeof(*smplsp);
		smplsp->smpls_in_label = MPLS_SHIM_LABEL_GET(label);
	}

	/* return mbuf */
	return(m);
}

struct mbuf *
mpls_shim_swap(struct mbuf *m, struct sockaddr_mpls *smplsp, u_int32_t *labelp)
{
	struct shim_hdr *shim;
	u_int32_t label;

	/* pullup shim_hdr */
	if (m->m_len < sizeof(struct shim_hdr))		/* XXX isn't this
					already checked by mpls_shim_peep ? */
		if ((m = m_pullup(m, sizeof(struct shim_hdr))) == 0)
			return(0);
        shim = mtod(m, struct shim_hdr *);
	shim->shim_label = ntohl(shim->shim_label);

	if (smplsp == NULL && labelp == NULL)
		/* can't swap, because no dst label */
		return(m);	/* XXX discard? XXX */

	if (labelp) {
		label = *labelp;
		/*
		if (smplsp)
			smplsp->smpls_in_label = htonl(*labelp);
		*/
	} else
		label = ntohl(smplsp->smpls_in_label);

	/* shim swap label */
	shim->shim_label &= ~SHIM_LABEL_MASK;
	shim->shim_label |= MPLS_SHIM_LABEL_GET(label) << SHIM_LABEL_OFFSET;

	/* shim swap exp : XXX exp override */
	if (smplsp) {
		shim->shim_label &= ~SHIM_EXP_MASK;
		shim->shim_label |=
			smplsp->smpls_out_exp << SHIM_EXP_OFFSET & SHIM_EXP_MASK;
	}
	shim->shim_label = htonl(shim->shim_label);

	return(m);
}

struct mbuf *
mpls_shim_push(struct mbuf *m, struct sockaddr_mpls *smplsp, u_int32_t *labelp,
    u_int8_t *bosp, u_int8_t *ttlp)
{
	struct shim_hdr *shim;

        M_PREPEND(m, sizeof(struct shim_hdr), M_DONTWAIT);
        if (m == 0)
		return(0);

        shim = mtod(m, struct shim_hdr *);
	bzero((caddr_t)shim, sizeof(*shim));

	if (bosp && *bosp)
		shim->shim_label |= SHIM_BOS_MASK;
	if (ttlp)
		shim->shim_label |= *ttlp & SHIM_TTL_MASK;
	else
		shim->shim_label |= 255;	/* XXX */
	m = mpls_shim_swap(m, smplsp, labelp);

	return m;
}
