/*	$OpenBSD: if_enc.c,v 1.51 2010/07/01 02:09:45 reyk Exp $	*/

/*
 * Copyright (c) 2010 Reyk Floeter <reyk@vantronix.net>
 *
 * Permission to use, copy, modify, and distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 * ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
 * OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */

#include "enc.h"
#include "bpfilter.h"

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/kernel.h>
#include <sys/malloc.h>
#include <sys/socket.h>
#include <sys/sockio.h>
#include <sys/mbuf.h>

#include <net/if.h>
#include <net/if_enc.h>
#include <net/if_types.h>
#include <net/route.h>
#if NBPFILTER > 0
#include <net/bpf.h>
#endif

struct ifnet			**enc_ifps;	/* rdomain-mapped enc ifs */
u_int				  enc_max_id;
struct ifnet			**enc_allifps;	/* unit-mapped enc ifs */
u_int				  enc_max_unit;
#define ENC_MAX_UNITS		  4096		/* XXX n per rdomain */

void	 encattach(int);

int	 enc_clone_create(struct if_clone *, int);
int	 enc_clone_destroy(struct ifnet *);
void	 enc_start(struct ifnet *);
int	 enc_output(struct ifnet *, struct mbuf *, struct sockaddr *,
	    struct rtentry *);
int	 enc_ioctl(struct ifnet *, u_long, caddr_t);

int	 enc_setif(struct ifnet *, u_int);
void	 enc_unsetif(struct ifnet *);

struct if_clone enc_cloner =
    IF_CLONE_INITIALIZER("enc", enc_clone_create, enc_clone_destroy);

void
encattach(int count)
{
	/* Create enc0 by default */
	(void)enc_clone_create(&enc_cloner, 0);

	if_clone_attach(&enc_cloner);
}

int
enc_clone_create(struct if_clone *ifc, int unit)
{
	struct enc_softc	*sc;
	struct ifnet		*ifp;
	struct ifnet		**new;
	size_t			 newlen;

	if (unit > ENC_MAX_UNITS)
		return (EINVAL);

	if ((sc = malloc(sizeof(struct enc_softc),
	    M_DEVBUF, M_NOWAIT|M_ZERO)) == NULL)
		return (ENOMEM);

	sc->sc_unit = unit;

	ifp = &sc->sc_if;
	ifp->if_softc = sc;
	ifp->if_type = IFT_ENC;
	ifp->if_start = enc_start;
	ifp->if_output = enc_output;
	ifp->if_ioctl = enc_ioctl;
	ifp->if_hdrlen = ENC_HDRLEN;

	snprintf(ifp->if_xname, sizeof(ifp->if_xname), "%s%d",
	    ifc->ifc_name, unit);

	if_attach(ifp);
	if (unit == 0)
		if_addgroup(ifp, ifc->ifc_name);
	if_alloc_sadl(ifp);

#if NBPFILTER > 0
	bpfattach(&ifp->if_bpf, ifp, DLT_ENC, ENC_HDRLEN);
#endif

	if (enc_setif(ifp, 0) != 0) {
		if_detach(ifp);
		free(sc, M_DEVBUF);
		return (-1);
	}

	if (unit == 0 || unit > enc_max_unit) {
		newlen = sizeof(struct ifnet *) * (unit + 1);

		if ((new = malloc(newlen, M_DEVBUF, M_NOWAIT|M_ZERO)) == NULL)
			return (-1);
		if (enc_allifps != NULL) {
			memcpy(new, enc_allifps,
			    sizeof(struct ifnet *) * (enc_max_unit + 1));
			free(enc_allifps, M_DEVBUF);
		}
		enc_allifps = new;
		enc_max_unit = unit;
	}
	enc_allifps[unit] = ifp;

	return (0);
}

int
enc_clone_destroy(struct ifnet *ifp)
{
	struct enc_softc	*sc = ifp->if_softc;
	int			 s;

	/* Protect users from removing enc0 */
	if (sc->sc_unit == 0)
		return (EPERM);

	s = splnet();
	enc_allifps[sc->sc_unit] = NULL;
	enc_unsetif(ifp);
	if_detach(ifp);
	free(sc, M_DEVBUF);
	splx(s);

	return (0);
}

void
enc_start(struct ifnet *ifp)
{
	struct mbuf	*m;

	for (;;) {
		IF_DROP(&ifp->if_snd);
		IF_DEQUEUE(&ifp->if_snd, m);
		if (m == NULL)
			break;
		m_freem(m);
	}
}

int
enc_output(struct ifnet *ifp, struct mbuf *m, struct sockaddr *sa,
    struct rtentry *rt)
{
	m_freem(m);	/* drop packet */
	return (0);
}

int
enc_ioctl(struct ifnet *ifp, u_long cmd, caddr_t data)
{
	struct ifreq	*ifr = (struct ifreq *)data;
	int		 error = 0;

	switch (cmd) {
	case SIOCAIFADDR:
	case SIOCSIFADDR:
	case SIOCSIFDSTADDR:
	case SIOCSIFFLAGS:
		if (ifp->if_flags & IFF_UP)
			ifp->if_flags |= IFF_RUNNING;
		else
			ifp->if_flags &= ~IFF_RUNNING;
		break;
	case SIOCSIFRTABLEID:
		if ((error = enc_setif(ifp, ifr->ifr_rdomainid)) != 0)
			return (error);
		/* FALLTHROUGH */
	default:
		return (ENOTTY);
	}

	return (0);
}

struct ifnet *
enc_getif(u_int id, u_int unit)
{
	struct ifnet	*ifp;

	/* Check if the caller wants to get a non-default enc interface */
	if (unit > 0) {
		if (unit > enc_max_unit)
			return (NULL);
		ifp = enc_allifps[unit];
		if (ifp == NULL || ifp->if_rdomain != id)
			return (NULL);
		return (ifp);
	}

	/* Otherwise return the default enc interface for this rdomain */
	if (enc_ifps == NULL)
		return (NULL);
	else if (id > RT_TABLEID_MAX)
		return (NULL);
	else if (id > enc_max_id)
		return (NULL);
	return (enc_ifps[id]);
}

int
enc_setif(struct ifnet *ifp, u_int id)
{
	struct ifnet	**new;
	size_t		 newlen;

	enc_unsetif(ifp);

	/*
	 * There can only be one default encif per rdomain -
	 * Don't overwrite the existing enc iface that is stored
	 * for this rdomain, so only the first enc interface that
	 * was added for this rdomain becomes the default.
	 */
	if (enc_getif(id, 0) != NULL)
		return (0);

	if (id > RT_TABLEID_MAX)
		return (-1);

	if (id == 0 || id > enc_max_id) {
		newlen = sizeof(struct ifnet *) * (id + 1);

		if ((new = malloc(newlen, M_DEVBUF, M_NOWAIT|M_ZERO)) == NULL)
			return (-1);
		if (enc_ifps != NULL) {
			memcpy(new, enc_ifps,
			    sizeof(struct ifnet *) * (enc_max_id + 1));
			free(enc_ifps, M_DEVBUF);
		}
		enc_ifps = new;
		enc_max_id = id;
	}

	enc_ifps[id] = ifp;

	/* Indicate that this interface is the rdomain default */
	ifp->if_link_state = LINK_STATE_UP;

	return (0);
}

void
enc_unsetif(struct ifnet *ifp)
{
	u_int			 id = ifp->if_rdomain, i;
	struct ifnet		*oifp, *nifp;

	if ((oifp = enc_getif(id, 0)) == NULL || oifp != ifp)
		return;

	/* Clear slot for this rdomain */
	enc_ifps[id] = NULL;
	ifp->if_link_state = LINK_STATE_UNKNOWN;

	/*
	 * Now find the next available encif to be the default interface
	 * for this rdomain.
	 */
	for (i = 0; i < (enc_max_unit + 1); i++) {
		nifp = enc_allifps[i];

		if (nifp == NULL || nifp == ifp || nifp->if_rdomain != id)
			continue;

		enc_ifps[id] = nifp;
		nifp->if_link_state = LINK_STATE_UP;
		break;
	}
}
