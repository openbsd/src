#include "mpe.h"

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/mbuf.h>
#include <sys/proc.h>
#include <sys/socket.h>
#include <sys/sockio.h>
#include <sys/ioctl.h>

#include <net/if.h>
#include <net/if_types.h>
#include <net/netisr.h>
#include <net/route.h>

#ifdef	INET
#include <netinet/in.h>
#include <netinet/in_var.h>
#include <netinet/in_systm.h>
#include <netinet/ip.h>
#endif

#ifdef INET6
#ifndef INET
#include <netinet/in.h>
#endif
#include <netinet6/nd6.h>
#endif /* INET6 */

#include <netmpls/mpls.h>

#ifdef MPLS_DEBUG
#define DPRINTF(x)    do { if (mpedebug) printf x ; } while (0)
#else
#define DPRINTF(x)
#endif

void	mpeattach(int);
int	mpeoutput(struct ifnet *, struct mbuf *, struct sockaddr *,
	    	       struct rtentry *);
int	mpeioctl(struct ifnet *, u_long, caddr_t);
void	mpestart(struct ifnet *);
int	mpe_clone_create(struct if_clone *, int);
int	mpe_clone_destroy(struct ifnet *);

LIST_HEAD(, mpe_softc)	mpeif_list;
struct if_clone	mpe_cloner =
    IF_CLONE_INITIALIZER("mpe", mpe_clone_create, mpe_clone_destroy);

void
mpeattach(int nmpe)
{
	LIST_INIT(&mpeif_list);
	if_clone_attach(&mpe_cloner);
}

int
mpe_clone_create(struct if_clone *ifc, int unit)
{
	struct ifnet 		*ifp;
	struct mpe_softc	*mpeif;
	int 			 s;

	if ((mpeif = malloc(sizeof(*mpeif),
	    M_DEVBUF, M_NOWAIT|M_ZERO)) == NULL)
		return (ENOMEM);

	mpeif->sc_shim.shim_label = MPLS_BOS_MASK | htonl(mpls_defttl);
	mpeif->sc_unit = unit;
	ifp = &mpeif->sc_if;
	snprintf(ifp->if_xname, sizeof ifp->if_xname, "mpe%d", unit);
	ifp->if_flags = IFF_POINTOPOINT;
	ifp->if_softc = mpeif;
	ifp->if_mtu = MPE_MTU;
	ifp->if_ioctl = mpeioctl;
	ifp->if_output = mpeoutput;
	ifp->if_start = mpestart;
	ifp->if_type = IFT_MPLS;
	ifp->if_snd.ifq_maxlen = ifqmaxlen;
	ifp->if_hdrlen = MPE_HDRLEN;
	IFQ_SET_MAXLEN(&ifp->if_snd, ifqmaxlen);
	IFQ_SET_READY(&ifp->if_snd);
	if_attach(ifp);
	if_alloc_sadl(ifp);

#if NBPFILTER > 0
	bpfattach(&mpeif->sc_if.if_bpf, ifp, DLT_MPLS, MPE_HDRLEN);
#endif

	s = splnet();
	LIST_INSERT_HEAD(&mpeif_list, mpeif, sc_list);
	splx(s);

	return (0);
}

int
mpe_clone_destroy(struct ifnet *ifp)
{
	struct mpe_softc	*mpeif = ifp->if_softc;
	int			 s;

	s = splnet();
	LIST_REMOVE(mpeif, sc_list);
	splx(s);

#if NBPFILTER > 0
	bpfdetach(ifp);
#endif
	if_detach(ifp);
	free(mpeif, M_DEVBUF);
	return (0);
}

/*
 * Start output on the mpe interface.
 */
void
mpestart(struct ifnet *ifp)
{
	struct mbuf 		*m;
	struct mpe_softc	*ifm;
	struct shim_hdr		 shim;
	int			 s;

	for (;;) {
		s = splnet();
		IF_DEQUEUE(&ifp->if_snd, m);
		splx(s);

		if (m == NULL)
			return;

		ifm = ifp->if_softc;
		shim.shim_label = ifm->sc_shim.shim_label;
		M_PREPEND(m, sizeof(shim), M_DONTWAIT);
		m_copyback(m, 0, sizeof(shim), (caddr_t)&shim);
		if (m == NULL) {
			ifp->if_ierrors++;
			continue;
		}
		m->m_pkthdr.rcvif = ifp;
		mpls_input(m);
	}
}

int
mpeoutput(struct ifnet *ifp, struct mbuf *m, struct sockaddr *dst,
	struct rtentry *rt)
{
	int	s;
	int	error;

	error = 0;
	s = splnet();
	IFQ_ENQUEUE(&ifp->if_snd, m, NULL, error);
	if (error) {
		/* mbuf is already freed */
		splx(s);
		return (error);
	}
	splx(s);
	if_start(ifp);
	return (error);
}

/* ARGSUSED */
int
mpeioctl(struct ifnet *ifp, u_long cmd, caddr_t data)
{
	int			 error;
	struct mpe_softc	*ifm;
	struct ifreq		*ifr;
	struct shim_hdr		 shim;
	u_int32_t		 ttl = htonl(mpls_defttl);

	ifr = (struct ifreq *)data;
	error = 0;
	switch (cmd) {
	case SIOCSIFADDR:
		break;
	case SIOCSIFFLAGS:
		if (ifp->if_flags & IFF_UP)
			ifp->if_flags |= IFF_RUNNING;
		else
			ifp->if_flags &= ~IFF_RUNNING;
		break;
	case SIOCSIFMTU:
		if (ifr->ifr_mtu < MPE_MTU_MIN ||
		    ifr->ifr_mtu > MPE_MTU_MAX)
			error = EINVAL;
		else
			ifp->if_mtu = ifr->ifr_mtu;
		break;
	case SIOCGETLABEL:
		ifm = ifp->if_softc;
		shim.shim_label =
		    ((ntohl(ifm->sc_shim.shim_label & MPLS_LABEL_MASK)) >>
		    MPLS_LABEL_OFFSET);
		error = copyout(&shim, ifr->ifr_data, sizeof(shim));
		break;
	case SIOCSETLABEL:
		ifm = ifp->if_softc;
		if ((error = copyin(ifr->ifr_data, &shim, sizeof(shim))))
			break;
		if (shim.shim_label > MPLS_LABEL_MAX) {
			error = EINVAL;
			break;
		}
		shim.shim_label = (htonl(shim.shim_label << MPLS_LABEL_OFFSET))
		    | MPLS_BOS_MASK | ttl;
		if (ifm->sc_shim.shim_label == shim.shim_label)
			break;
		LIST_FOREACH(ifm, &mpeif_list, sc_list) {
			if (ifm != ifp->if_softc &&
			    ifm->sc_shim.shim_label == shim.shim_label) {
				error = EEXIST;
				break;
			}
		}
		if (error)
			break;
		ifm = ifp->if_softc;
		ifm->sc_shim.shim_label = shim.shim_label;
		break;
	default:
		return (ENOTTY);
	}

	return (error);
}

void
mpe_input(struct mbuf *m)
{
	int		 s;
	struct shim_hdr	*shim;

	shim = mtod(m, struct shim_hdr *);
	if (!(MPLS_BOS_ISSET(shim->shim_label))) {
#ifdef MPLS_DEBUG
		printf("mpe_input: invalid packet with non BoS label\n");
#endif
		m_free(m);
		return;
	}

	
#ifdef MPLS_DEBUG
	printf("mpe_input: got packet with label: %d\n",
	    ((ntohl(shim->shim_label & MPLS_LABEL_MASK)) >> MPLS_LABEL_OFFSET));
#endif
	m_adj(m, sizeof(shim));
	
	s = splnet();
	/*
	 * assume we only get fed ipv4 packets for now.
	 */
	IF_ENQUEUE(&ipintrq, m);
	schednetisr(NETISR_IP);
	splx(s);
}
