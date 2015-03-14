/*	$OpenBSD: if_san_obsd.c,v 1.18 2015/03/14 03:38:48 jsg Exp $	*/

/*-
 * Copyright (c) 2001-2004 Sangoma Technologies (SAN)
 * All rights reserved.  www.sangoma.com
 *
 * This code is written by Alex Feldman <al.feldman@sangoma.com> for SAN.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above
 *    copyright notice, this list of conditions and the following disclaimer
 *    in the documentation and/or other materials provided with the
 *    distribution.
 * 3. Neither the name of Sangoma Technologies nor the names of its
 *    contributors may be used to endorse or promote products derived
 *    from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY SANGOMA TECHNOLOGIES AND CONTRIBUTORS
 * ``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED
 * TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE FOUNDATION OR CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
 * THE POSSIBILITY OF SUCH DAMAGE.
 */

#include <sys/types.h>
#include <sys/param.h>
#include <sys/systm.h>
#include <sys/syslog.h>
#include <sys/ioccom.h>
#include <sys/conf.h>
#include <sys/malloc.h>
#include <sys/errno.h>
#include <sys/exec.h>
#include <sys/mbuf.h>
#include <sys/sockio.h>
#include <sys/socket.h>
#include <sys/kernel.h>
#include <sys/device.h>
#include <sys/time.h>
#include <sys/timeout.h>

#include "bpfilter.h"
#if NBPFILTER > 0
# include <net/bpf.h>
#endif
#include <net/if.h>
#include <net/if_var.h>
#include <net/if_media.h>
#include <net/if_sppp.h>

#include <netinet/udp.h>
#include <netinet/ip.h>

#include <dev/pci/if_san_common.h>
#include <dev/pci/if_san_obsd.h>


#ifdef	_DEBUG_
#define	STATIC
#else
#define	STATIC		static
#endif

#define PPP_HEADER_LEN 4		/* should be globaly defined by sppp */


static sdla_t *wanpipe_generic_getcard(struct ifnet *);
static int wanpipe_generic_ioctl(struct ifnet *, u_long, caddr_t);
static void wanpipe_generic_watchdog(struct ifnet*);
static void wanpipe_generic_start(struct ifnet *);


static sdla_t *
wanpipe_generic_getcard(struct ifnet *ifp)
{
	sdla_t*	card;

	if (ifp->if_softc == NULL) {
		log(LOG_INFO, "%s: Invalid device private structure pointer\n",
				ifp->if_xname);
		return (NULL);
	}
	card = ((sdla_t*)((wanpipe_common_t*)ifp->if_softc)->card);
	if (card == NULL) {
		log(LOG_INFO, "%s: Invalid Sangoma device card\n",
		    ifp->if_xname);
		return (NULL);
	}
	return (card);
}

int
wanpipe_generic_name(sdla_t *card, char *ifname, int len)
{
	static int	ifunit = 0;

	snprintf(ifname, len, "san%d", ifunit++);
	return (0);
}

int
wanpipe_generic_register(sdla_t *card, struct ifnet *ifp, char *ifname)
{
	wanpipe_common_t*	common = WAN_IFP_TO_COMMON(ifp);

	if (ifname == NULL || strlen(ifname) > IFNAMSIZ)
		return (EINVAL);
	else
		bcopy(ifname, ifp->if_xname, strlen(ifname));

	IFQ_SET_MAXLEN(&ifp->if_snd, IFQ_MAXLEN);
	IFQ_SET_READY(&ifp->if_snd);
	ifp->if_mtu = PP_MTU;
	ifp->if_flags = IFF_POINTOPOINT | IFF_MULTICAST;
	common->protocol = IF_PROTO_CISCO;

	((struct sppp *)ifp)->pp_flags |= PP_CISCO;
	((struct sppp *)ifp)->pp_flags |= PP_KEEPALIVE;
	((struct sppp *)ifp)->pp_framebytes = 3;

	ifp->if_ioctl = wanpipe_generic_ioctl;	/* Will set from new_if() */
	ifp->if_start = wanpipe_generic_start;
	ifp->if_watchdog = wanpipe_generic_watchdog;

	if_attach(ifp);
	if_alloc_sadl(ifp);
	sppp_attach(ifp);

#if NBPFILTER > 0
	bpfattach(&ifp->if_bpf, ifp, DLT_PPP, PPP_HEADER_LEN);
#endif /* NBPFILTER > 0 */

	return (0);
}

void
wanpipe_generic_unregister(struct ifnet *ifp)
{
	log(LOG_INFO, "%s: Unregister interface!\n", ifp->if_xname);

	sppp_detach(ifp);
	if_free_sadl(ifp);
	if_detach(ifp);
}

static void
wanpipe_generic_start(struct ifnet *ifp)
{
	sdla_t		*card;
	struct mbuf	*opkt;
	int		 err = 0;

	if ((card = wanpipe_generic_getcard(ifp)) == NULL)
		return;

	while (1) {
		if (sppp_isempty(ifp)) {
			/* No more packets in send queue */
			break;
		}

		if ((opkt = sppp_dequeue(ifp)) == NULL) {
			/* Should never happened, packet pointer is NULL */
			break;
		}
		if (card->iface_send == NULL) {
			m_freem(opkt);
			break;
		}
		/* report the packet to BPF if present and attached */
#if NBPFILTER > 0
		if (ifp->if_bpf)
			bpf_mtap(ifp->if_bpf, opkt, BPF_DIRECTION_OUT);
#endif /* NBPFILTER > 0 */

		if (wan_mbuf_to_buffer(&opkt)) {
			m_freem(opkt);
			break;
		}

		err = card->iface_send(opkt, ifp);
		if (err)
			break;
	}
}


static int
wanpipe_generic_ioctl(struct ifnet *ifp, u_long cmd, caddr_t data)
{
	struct proc		*p = curproc;
	struct ifreq		*ifr = (struct ifreq*)data;
	sdla_t			*card;
	wanpipe_common_t*	common = WAN_IFP_TO_COMMON(ifp);
	struct if_settings	ifsettings;
	unsigned long		ts_map;
	int			err = 0, s;

	if ((card = wanpipe_generic_getcard(ifp)) == NULL)
		return (EINVAL);

	s = splnet();

	switch (cmd) {
	case SIOCSIFADDR:
		// XXX because sppp does an implicit setflags
		log(LOG_INFO, "%s: Bringing interface up.\n",
		    ifp->if_xname);
		if (card->iface_up)
			card->iface_up(ifp);
		wanpipe_generic_start(ifp);
		err = 1;
		break;

	case SIOCSIFMEDIA:
		/* You can't set new media type while card is running */
		if (card->state != WAN_DISCONNECTED) {
			log(LOG_INFO, "%s: Unable to change media type!\n",
			    ifp->if_xname);
			err = EINVAL;
		} else
			err = ifmedia_ioctl(ifp, ifr, &common->ifm, cmd);
		goto ioctl_out;

	case SIOCGIFMEDIA:
		err = ifmedia_ioctl(ifp, ifr, &common->ifm, cmd);
		goto ioctl_out;

	case SIOCSIFTIMESLOT:
		if ((err = suser(p, 0)) != 0)
			goto ioctl_out;
		if (card->state != WAN_DISCONNECTED) {
			log(LOG_INFO, "%s: Unable to change timeslot map!\n",
			    ifp->if_xname);
			err = EINVAL;
			goto ioctl_out;
		}

		err = copyin(ifr->ifr_data, &ts_map, sizeof(ts_map));
		if (err == 0)
			sdla_te_settimeslot(card, ts_map);

		goto ioctl_out;

	case SIOCGIFTIMESLOT:
		ts_map = sdla_te_gettimeslot(card);
		err = copyout(ifr->ifr_data, &ts_map, sizeof(ts_map));
		goto ioctl_out;

	case SIOCSIFFLAGS:
	    	/*
     		** If the interface is marked up - enable communications. 
	     	** If down - disable communications.  IFF_UP is taken 
		** care of before entering this function.
	     	*/
		err = 1;
		if ((ifp->if_flags & IFF_UP) == 0) {
			if ((ifp->if_flags & IFF_RUNNING) == 0)
				break;
			/* bring it down */
			log(LOG_INFO, "%s: Bringing interface down.\n",
			    ifp->if_xname);
			if (card->iface_down)
				card->iface_down(ifp);
		} else { /* bring it up */ 
			if (ifp->if_flags & IFF_RUNNING)
				break;
			log(LOG_INFO, "%s: Bringing interface up.\n",
			    ifp->if_xname);
			if (card->iface_up)
				card->iface_up(ifp);
			wanpipe_generic_start(ifp);
		}
		break;

	case SIOC_WANPIPE_DEVICE:
		err = copyin(ifr->ifr_data, &ifsettings,
		    sizeof(struct if_settings));

		if (err) {
			log(LOG_INFO, "%s: Failed to copy from user space!\n",
						card->devname);
			goto ioctl_out;
		}

		switch (ifsettings.type) {
		case IF_GET_PROTO:
			ifsettings.type = common->protocol;
			err = copyout(&ifsettings, ifr->ifr_data,
			    sizeof(struct if_settings));
			if (err)
				log(LOG_INFO,
				    "%s: Failed to copy to uspace!\n",
				    card->devname);
			break;

		case IF_PROTO_CISCO:
		case IF_PROTO_PPP:
			if ((err = suser(p, 0)) != 0)
				goto ioctl_out;
			err = wp_lite_set_proto(ifp, (struct ifreq*)data);
			break;

		case IF_IFACE_T1:
		case IF_IFACE_E1:
			if ((err = suser(p, 0)) != 0)
				goto ioctl_out;
			err = wp_lite_set_te1_cfg(ifp, (struct ifreq*)data);
			break;

		default:
			if (card->iface_ioctl)
				err = card->iface_ioctl(ifp, cmd,
				    (struct ifreq*)data);
			break;
		}
		goto ioctl_out;

	default:
		if (card->iface_ioctl) {
			/* Argument seqeunce is change for Linux order */
			err = card->iface_ioctl(ifp, cmd, (struct ifreq*)data);
		}
		break;
	}

	if (err)
		err = sppp_ioctl(ifp, cmd, data);

ioctl_out:
	splx(s);
	return (err);
}

static void
wanpipe_generic_watchdog(struct ifnet *ifp)
{
	return;
}

int
wanpipe_generic_open(struct ifnet *ifp)
{
	return (0);
}

int
wanpipe_generic_close(struct ifnet *ifp)
{
	return (0);
}

int
wanpipe_generic_input(struct ifnet *ifp, struct mbuf *m)
{
	sdla_t		*card;
#if NBPFILTER > 0
#endif /* NBPFILTER > 0 */

	if ((card = wanpipe_generic_getcard(ifp)) == NULL) {
		return (-EINVAL);
	}
	m->m_pkthdr.rcvif = ifp;
#if NBPFILTER > 0
	if (ifp->if_bpf)
		bpf_mtap(ifp->if_bpf, m, BPF_DIRECTION_IN);
#endif /* NBPFILTER > 0 */
	ifp->if_ipackets ++;
	sppp_input(ifp, m);
	return (0);
}

int
wp_lite_set_proto(struct ifnet *ifp, struct ifreq *ifr)
{
	wanpipe_common_t	*common;
	struct if_settings	*ifsettings;
	int			 err = 0;

	if ((common = ifp->if_softc) == NULL) {
		log(LOG_INFO, "%s: Private structure is null!\n",
				ifp->if_xname);
		return (EINVAL);
	}

	ifsettings = (struct if_settings*) ifr->ifr_data;
	
	switch (ifsettings->type) {
	case IF_PROTO_CISCO:
		if (common->protocol == IF_PROTO_CISCO)
			return 0;
		((struct sppp *)ifp)->pp_flags |= PP_CISCO;
		((struct sppp *)ifp)->pp_flags |= PP_KEEPALIVE;
		common->protocol = IF_PROTO_CISCO;
		break;
	case IF_PROTO_PPP:
		if (common->protocol == IF_PROTO_PPP)
			return 0;
		((struct sppp *)ifp)->pp_flags &= ~PP_CISCO;
		((struct sppp *)ifp)->pp_flags |= PP_KEEPALIVE;
		common->protocol = IF_PROTO_PPP;
		break;
	}

	err = sppp_ioctl(ifp, SIOCSIFFLAGS, ifr);
	return (err);
}

int
wp_lite_set_te1_cfg(struct ifnet *ifp, struct ifreq *ifr)
{
	sdla_t			*card;
	struct if_settings	*ifsettings;
	sdla_te_cfg_t		te_cfg;
	int			 err = 0;

	if ((card = wanpipe_generic_getcard(ifp)) == NULL)
		return (EINVAL);

	ifsettings = (struct if_settings*)ifr->ifr_data;
	err = copyin(ifsettings->ifs_te1, &te_cfg, sizeof(sdla_te_cfg_t));

	if (ifsettings->flags & SANCFG_CLOCK_FLAG)
		card->fe_te.te_cfg.te_clock = te_cfg.te_clock;

	switch (ifsettings->type) {
	case IF_IFACE_T1:
		if (ifsettings->flags & SANCFG_LBO_FLAG)
			card->fe_te.te_cfg.lbo = te_cfg.lbo;
		break;
	}

	return (err);
}
