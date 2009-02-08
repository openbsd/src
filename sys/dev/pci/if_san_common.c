/*	$OpenBSD: if_san_common.c,v 1.13 2009/02/08 20:07:44 chl Exp $	*/

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


# include <sys/types.h>
# include <sys/param.h>
# include <sys/systm.h>
# include <sys/syslog.h>
# include <sys/ioccom.h>
# include <sys/conf.h>
# include <sys/malloc.h>
# include <sys/errno.h>
# include <sys/exec.h>
# include <sys/mbuf.h>
# include <sys/proc.h>
# include <sys/socket.h>
# include <sys/kernel.h>
# include <sys/time.h>
# include <sys/timeout.h>

# include <net/bpf.h>
# include <net/if_dl.h>
# include <net/if_types.h>
# include <net/if.h>
# include <net/netisr.h>
# include <net/route.h>
# include <net/if_media.h>
# include <net/ppp_defs.h>
# include <net/if_ppp.h>
# include <net/if_sppp.h>
# include <netinet/in_systm.h>
# include <netinet/in.h>
# include <netinet/in_var.h>
# include <netinet/udp.h>
# include <netinet/ip.h>

# include <dev/pci/if_san_common.h>
# include <dev/pci/if_san_obsd.h>

#ifdef	_DEBUG_
#define	STATIC
#else
#define	STATIC		static
#endif

/* WAN link driver entry points */
#if 0
static int	shutdown(sdla_t *card);
#endif

/* Miscellaneous functions */
static int wan_ioctl(struct ifnet*, int, struct ifreq *);
static int sdla_isr(void *);

static void release_hw(sdla_t *card);

static int wan_ioctl_dump(sdla_t *, void *);
static int wan_ioctl_hwprobe(struct ifnet *, void *);

/*
 * Global Data
 * Note: All data must be explicitly initialized!!!
 */

/* private data */
extern char	*san_drvname;
LIST_HEAD(, sdla) wan_cardlist = LIST_HEAD_INITIALIZER(&wan_cardlist);

#if 0
static san_detach(void)
{
	wanpipe_common_t	*common;
	sdla_t			*card, *tmp_card;
	int			err = 0;

	card = LIST_FIRST(&wan_cardlist);
	while (card) {
		if (card->disable_comm)
			card->disable_comm(card);

		while ((common = LIST_FIRST(&card->dev_head))) {
			LIST_REMOVE(common, next);
			if (card->del_if) {
				struct ifnet *ifp =
				    (struct ifnet*)&common->ifp;
				log(LOG_INFO, "%s: Deleting interface...\n",
						ifp->if_xname);
				card->del_if(card, ifp);
			}
		}
		log(LOG_INFO, "%s: Shutdown device\n", card->devname);
		shutdown(card);
		tmp_card = card;
		card = LIST_NEXT(card, next);
		LIST_REMOVE(tmp_card, next);
		free(tmp_card, M_DEVBUF);
	}

	log(LOG_INFO, "\n");
	log(LOG_INFO, "%s: WANPIPE Generic Modules Unloaded.\n",
						san_drvname);

	err = sdladrv_exit();
	return err;
}
#endif


int
san_dev_attach(void *hw, u_int8_t *devname, int namelen)
{
	sdla_t			*card;
	wanpipe_common_t	*common = NULL;
	int			err = 0;

	card = malloc(sizeof(*card), M_DEVBUF, M_NOWAIT | M_ZERO);
	if (!card) {
		log(LOG_INFO, "%s: Failed allocate new card!\n",
				san_drvname);
		return (EINVAL);
	}
	card->magic = WANPIPE_MAGIC;
	wanpipe_generic_name(card, card->devname, sizeof(card->devname));
	strlcpy(devname, card->devname, namelen);
	card->hw = hw;
	LIST_INIT(&card->dev_head);

	sdla_getcfg(card->hw, SDLA_CARDTYPE, &card->type);
	if (sdla_is_te1(card->hw))
		sdla_te_defcfg(&card->fe_te.te_cfg);

	err = sdla_setup(card->hw);
	if (err) {
		log(LOG_INFO, "%s: Hardware setup Failed %d\n",
			card->devname,err);
		return (EINVAL);
	}
	err = sdla_intr_establish(card->hw, sdla_isr, (void*)card);
	if (err) {
		log(LOG_INFO, "%s: Failed set interrupt handler!\n",
					card->devname);
		sdla_down(card->hw);
		return (EINVAL);
	}

	switch (card->type) {
	case SDLA_AFT:
#if defined(DEBUG_INIT)
		log(LOG_INFO, "%s: Starting AFT Hardware Init.\n",
				card->devname);
#endif
		common = wan_xilinx_init(card);
		break;
	}
	if (common == NULL) {
		release_hw(card);
		card->configured = 0;
		return (EINVAL);
	}
	LIST_INSERT_HEAD(&card->dev_head, common, next);

	/* Reserve I/O region and schedule background task */
	card->critical	= 0;
	card->state	= WAN_DISCONNECTED;
	card->ioctl	= wan_ioctl;
	return (0);
}


/*
 * Shut down WAN link driver.
 * o shut down adapter hardware
 * o release system resources.
 *
 */
#if 0
static int
shutdown (sdla_t *card)
{
	int err=0;

	if (card->state == WAN_UNCONFIGURED) {
		return 0;
	}
	card->state = WAN_UNCONFIGURED;

	bit_set((u_int8_t*)&card->critical, PERI_CRIT);

	/* In case of piggibacking, make sure that
         * we never try to shutdown both devices at the same
         * time, because they depend on one another */

	card->state = WAN_UNCONFIGURED;

	/* Release Resources */
	release_hw(card);

        /* only free the allocated I/O range if not an S514 adapter */
	if (!card->configured) {
		card->hw = NULL;
		if (card->same_cpu) {
			card->same_cpu->hw = NULL;
			card->same_cpu->same_cpu = NULL;
			card->same_cpu=NULL;
		}
	}

	bit_clear((u_int8_t*)&card->critical, PERI_CRIT);

	return err;
}
#endif

static void
release_hw(sdla_t *card)
{
	log(LOG_INFO, "%s: Master shutting down\n",card->devname);
	sdla_down(card->hw);
	sdla_intr_disestablish(card->hw);
	card->configured = 0;
	return;
}


/*
 * Driver IOCTL Handlers
 */

static int
wan_ioctl(struct ifnet *ifp, int cmd, struct ifreq *ifr)
{
	wanpipe_common_t	*common = WAN_IFP_TO_COMMON(ifp);
	int			err;

	SAN_ASSERT(common == NULL);
	SAN_ASSERT(common->card == NULL);

	if ((err = suser(curproc, 0)) != 0)
		return err;

	switch (cmd) {
	case SIOC_WANPIPE_HWPROBE:
		err = wan_ioctl_hwprobe(ifp, ifr->ifr_data);
		break;

	case SIOC_WANPIPE_DUMP:
		err = wan_ioctl_dump(common->card, ifr->ifr_data);
		break;

	default:
		err = ENOTTY;
		break;
	}
	return err;
}

static int
wan_ioctl_hwprobe(struct ifnet *ifp, void *u_def)
{
	sdla_t			*card = NULL;
	wanpipe_common_t	*common = WAN_IFP_TO_COMMON(ifp);
	wanlite_def_t	 	def;
	unsigned char		*str;
	int			err;

	SAN_ASSERT(common == NULL);
	SAN_ASSERT(common->card == NULL);
	card = common->card;
	bzero(&def, sizeof(wanlite_def_t));
	/* Get protocol type */
	def.proto = common->protocol;

	/* Get hardware configuration */
	err = sdla_get_hwprobe(card->hw, (void**)&str);
	if (err)
		return EINVAL;

	strlcpy(def.hwprobe, str, sizeof(def.hwprobe));
	/* Get interface configuration */
	if (IS_TE1(&card->fe_te.te_cfg)) {
		if (IS_T1(&card->fe_te.te_cfg))
			def.iface = IF_IFACE_T1;
		else
			def.iface = IF_IFACE_E1;

		bcopy(&card->fe_te.te_cfg, &def.te_cfg, sizeof(sdla_te_cfg_t));
	}

	err = copyout(&def, u_def, sizeof(def));
	if (err) {
		log(LOG_INFO, "%s: Failed to copy to user space (%d)\n",
		    card->devname, __LINE__);
		return ENOMEM;
	}
	return 0;
}

static int
wan_ioctl_dump(sdla_t *card, void *u_dump)
{
	sdla_dump_t	dump;
	void*		data;
	u_int32_t	memory;
	int		err = 0;

	err = copyin(u_dump, &dump, sizeof(sdla_dump_t));
	if (err)
		return err;

	sdla_getcfg(card->hw, SDLA_MEMORY, &memory);
	if (dump.magic != WANPIPE_MAGIC)
		return EINVAL;

	if ((dump.offset + dump.length) > memory)
		return EINVAL;

	data = malloc(dump.length, M_DEVBUF, M_NOWAIT);
	if (data == NULL)
		return ENOMEM;

	sdla_peek(card->hw, dump.offset, data, dump.length);
	err = copyout(data, dump.ptr, dump.length);
	if (err) {
		log(LOG_INFO, "%s: Failed to copy to user space (%d)\n",
				card->devname, __LINE__);
	}
	free(data, M_DEVBUF);
	return err;
}


/*
 * SDLA Interrupt Service Routine.
 * o call protocol-specific interrupt service routine, if any.
 */
int
sdla_isr(void *pcard)
{
	sdla_t *card = (sdla_t*)pcard;

	if (card == NULL || card->magic != WANPIPE_MAGIC)
		return 0;

	switch (card->type) {
	case SDLA_AFT:
		if (card->isr)
			card->isr(card);
		break;
	}
	return (1);
}

struct mbuf* 
wan_mbuf_alloc(int len)
{
	struct mbuf	*m;

	/* XXX handle len > MCLBYTES */
	if (len <= 0 || len > MCLBYTES)
		return (NULL);

	MGETHDR(m, M_DONTWAIT, MT_DATA);

	if (m == NULL || len <= MHLEN)
		return (m);

	m->m_pkthdr.len = len;
	m->m_len = len;
	MCLGET(m, M_DONTWAIT);

	if ((m->m_flags & M_EXT) == 0) {
		m_freem(m);
		return (NULL);
	}

	return (m);
}

int 
wan_mbuf_to_buffer(struct mbuf **m_org)
{
	struct mbuf	*m, *m0, *tmp;
	char		*buffer;
	size_t	 	 len;

	if (m_org == NULL || *m_org == NULL)
		return (EINVAL);

	m0 = *m_org;
#if 0
	/* no need to copy if it is a single, properly aligned mbuf */
	if (m0->m_next == NULL && (mtod(m0, u_int32_t)  & 0x03) == 0)
		return (0);
#endif
	MGET(m, M_DONTWAIT, MT_DATA);

	if (m == NULL)
		return (ENOMEM);

	MCLGET(m, M_DONTWAIT);

	if ((m->m_flags & M_EXT) == 0) {
		m_freem(m);
		return (ENOMEM);
	}

	m->m_len = 0;

	/* XXX handle larger packets? */
	len = MCLBYTES ;
	buffer = mtod(m, caddr_t);

	len -= 16;
	buffer += 16;

	/* make sure the buffer is aligned to a 4-byte boundary */
	if (ADDR_MASK(buffer, 0x03)) {
		unsigned int inc = 4 - ADDR_MASK(buffer, 0x03);
		buffer += inc;
		len -= inc;
	}

	m->m_data = buffer;

	for (tmp = m0; tmp; tmp = tmp->m_next) {
		if (tmp->m_len > len) {
			m_freem(m);
			return (EINVAL);
		}
		bcopy(mtod(tmp, caddr_t), buffer, tmp->m_len);
		buffer += tmp->m_len;
		m->m_len += tmp->m_len;
		len -= tmp->m_len;
	}

	m_freem(m0);
	*m_org = m;

	return (0);
}
