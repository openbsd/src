/*	$OpenBSD: if_san_xilinx.c,v 1.15 2005/11/08 20:23:42 canacar Exp $	*/

/*-
 * Copyright (c) 2001-2004 Sangoma Technologies (SAN)
 * All rights reserved.  www.sangoma.com
 *
 * This code is written by Nenad Corbic <ncorbic@sangoma.com> and
 * Alex Feldman <al.feldman@sangoma.com> for SAN.
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
#include <sys/malloc.h>
#include <sys/errno.h>
#include <sys/mbuf.h>
#include <sys/proc.h>
#include <sys/socket.h>
#include <sys/kernel.h>
#include <sys/time.h>
#include <sys/timeout.h>

#include <net/bpf.h>
#include <net/if.h>
#include <net/if_media.h>
#include <net/netisr.h>
#include <net/if_sppp.h>
#include <netinet/in_systm.h>
#include <netinet/in.h>
#include <netinet/in_var.h>
#include <netinet/udp.h>
#include <netinet/ip.h>

#include <machine/bus.h>

#include <dev/pci/if_san_common.h>
#include <dev/pci/if_san_obsd.h>
#include <dev/pci/if_san_xilinx.h>


/* Private critical flags */
enum {
	POLL_CRIT = PRIV_CRIT,
	TX_BUSY,
	RX_BUSY,
	TASK_POLL,
	CARD_DOWN
};

enum {
	LINK_DOWN,
	DEVICE_DOWN
};

#define MAX_IP_ERRORS	10

#define PORT(x)   (x == 0 ? "PRIMARY" : "SECONDARY" )
#define MAX_TX_BUF	10
#define MAX_RX_BUF	10

#undef DEB_XILINX

#if 1
# define TRUE_FIFO_SIZE 1
#else
# undef  TRUE_FIFO_SIZE
# define HARD_FIFO_CODE 0x01
#endif

static int aft_rx_copyback = MHLEN;


struct xilinx_rx_buffer {
	SIMPLEQ_ENTRY(xilinx_rx_buffer) entry;
	struct mbuf *mbuf;
	bus_dmamap_t dma_map;
	wp_rx_element_t rx_el;
};

SIMPLEQ_HEAD(xilinx_rx_head, xilinx_rx_buffer);

/*
 * This structure is placed in the private data area of the device structure.
 * The card structure used to occupy the private area but now the following
 * structure will incorporate the card structure along with Protocol specific
 * data
 */
typedef struct {
	wanpipe_common_t	common;

	struct ifqueue	wp_tx_pending_list;
	struct ifqueue	wp_tx_complete_list;
	struct xilinx_rx_head	wp_rx_free_list;
	struct xilinx_rx_head	wp_rx_complete_list;
	struct xilinx_rx_buffer *wp_rx_buffers;
	struct xilinx_rx_buffer *wp_rx_buffer_last;
	struct xilinx_rx_buffer	*rx_dma_buf;

	bus_dma_tag_t	dmatag;
	bus_dmamap_t	tx_dmamap;
	struct mbuf	*tx_dma_mbuf;
	u_int8_t	tx_dma_cnt;

	unsigned long	time_slot_map;
	unsigned char	num_of_time_slots;
	long		logic_ch_num;

	unsigned char	dma_status;
	unsigned char	ignore_modem;
	struct ifqueue	udp_queue;

	unsigned long	router_start_time;

	unsigned long	tick_counter;		/* For 5s timeout counter */
	unsigned long	router_up_time;

	unsigned char	mc;			/* Mulitcast support on/off */
	unsigned char	udp_pkt_src;		/* udp packet processing */
	unsigned short	timer_int_enabled;

	unsigned char	interface_down;

	u_int8_t	gateway;
	u_int8_t	true_if_encoding;

	char		if_name[IFNAMSIZ+1];

	u_int8_t	idle_flag;
	u_int16_t	max_idle_size;
	u_int8_t	idle_start;

	u_int8_t	pkt_error;
	u_int8_t	rx_fifo_err_cnt;

	int		first_time_slot;

	unsigned long	tx_dma_addr;
	unsigned int	tx_dma_len;
	unsigned char	rx_dma;
	unsigned char   pci_retry;

	unsigned char	fifo_size_code;
	unsigned char	fifo_base_addr;
	unsigned char	fifo_size;

	int		dma_mtu;

	void		*prot_ch;
	wan_trace_t	trace_info;
}xilinx_softc_t;
#define WAN_IFP_TO_SOFTC(ifp)	(xilinx_softc_t *)((ifp)->if_softc)

/* Route Status options */
#define NO_ROUTE	0x00
#define ADD_ROUTE	0x01
#define ROUTE_ADDED	0x02
#define REMOVE_ROUTE	0x03

#define WP_WAIT		0
#define WP_NO_WAIT	1

/* variable for keeping track of enabling/disabling FT1 monitor status */
/* static int rCount; */

extern void disable_irq(unsigned int);
extern void enable_irq(unsigned int);

/**SECTOIN**************************************************
 *
 * Function Prototypes
 *
 ***********************************************************/

/* WAN link driver entry points. These are called by the WAN router module. */
static int	wan_xilinx_release(sdla_t*, struct ifnet *);

/* Network device interface */
static int	wan_xilinx_up(struct ifnet *);
static int	wan_xilinx_down(struct ifnet *);
static int	wan_xilinx_ioctl(struct ifnet *, int cmd, struct ifreq *);
static int	wan_xilinx_send(struct mbuf *, struct ifnet *);

static void	handle_front_end_state(void *);
static void	enable_timer(void *);

/* Miscellaneous Functions */
static void	port_set_state (sdla_t *, int);

/* Interrupt handlers */
static void	wp_xilinx_isr (sdla_t *);

/* Miscellaneous functions */
static int	process_udp_mgmt_pkt(sdla_t *, struct ifnet *,
		    xilinx_softc_t *, int);
/* Bottom half handlers */
static void	xilinx_process_packet(xilinx_softc_t *);

static int	xilinx_chip_configure(sdla_t *);
static int	xilinx_chip_unconfigure(sdla_t *);
static int	xilinx_dev_configure(sdla_t *, xilinx_softc_t *);
static void	xilinx_dev_unconfigure(sdla_t *, xilinx_softc_t *);
static int	xilinx_dma_rx(sdla_t *, xilinx_softc_t *);
static void	xilinx_dev_enable(sdla_t *, xilinx_softc_t *);
static void	xilinx_dev_close(sdla_t *, xilinx_softc_t *);
static int	xilinx_dma_tx (sdla_t *, xilinx_softc_t *);
static void	xilinx_dma_tx_complete (sdla_t *, xilinx_softc_t *);
static void	xilinx_dma_rx_complete (sdla_t *, xilinx_softc_t *);
static void	xilinx_dma_max_logic_ch(sdla_t *);
static int	xilinx_init_rx_dev_fifo(sdla_t *, xilinx_softc_t *,
		    unsigned char);
static void	xilinx_init_tx_dma_descr(sdla_t *, xilinx_softc_t *);
static int	xilinx_init_tx_dev_fifo(sdla_t *, xilinx_softc_t *,
		    unsigned char);
static void	xilinx_tx_post_complete(sdla_t *, xilinx_softc_t *,
		    struct mbuf *);
static void	xilinx_rx_post_complete(sdla_t *, xilinx_softc_t *,
		    struct xilinx_rx_buffer *, struct mbuf **, u_char *);


static char	request_xilinx_logical_channel_num(sdla_t *, xilinx_softc_t *,
		    long *);
static void	free_xilinx_logical_channel_num (sdla_t *, int);


static unsigned char read_cpld(sdla_t *, unsigned short);
static unsigned char write_cpld(sdla_t *, unsigned short, unsigned char);

static void	front_end_interrupt(sdla_t *, unsigned long);
static void	enable_data_error_intr(sdla_t *);
static void	disable_data_error_intr(sdla_t *, unsigned char);

static void	xilinx_tx_fifo_under_recover(sdla_t *, xilinx_softc_t *);

static int	xilinx_write_ctrl_hdlc(sdla_t *, u_int32_t,
		    u_int8_t, u_int32_t);

static int	set_chan_state(sdla_t*, struct ifnet*, int);

static int	fifo_error_interrupt(sdla_t *, unsigned long);
static int	request_fifo_baddr_and_size(sdla_t *, xilinx_softc_t *);
static int	map_fifo_baddr_and_size(sdla_t *,
		    unsigned char, unsigned char *);
static int	free_fifo_baddr_and_size(sdla_t *, xilinx_softc_t *);

static void	aft_red_led_ctrl(sdla_t *, int);
static void	aft_led_timer(void *);

static int	aft_core_ready(sdla_t *);
static int	aft_alloc_rx_buffers(xilinx_softc_t *);
static void	aft_release_rx_buffers(xilinx_softc_t *);
static int	aft_alloc_rx_dma_buff(xilinx_softc_t *, int);
static void	aft_reload_rx_dma_buff(xilinx_softc_t *,
		    struct xilinx_rx_buffer *);
static void	aft_release_rx_dma_buff(xilinx_softc_t *,
		    struct xilinx_rx_buffer *);


/* TE1 Control registers  */
static WRITE_FRONT_END_REG_T write_front_end_reg;
static READ_FRONT_END_REG_T  read_front_end_reg;

static void	wan_ifmedia_sts(struct ifnet *, struct ifmediareq *);
static int	wan_ifmedia_upd(struct ifnet *);

static void
xilinx_delay(int sec)
{
#if 0
	unsigned long timeout = ticks;
	while ((ticks - timeout) < (sec * hz)) {
		schedule();
	}
#endif
}

void *
wan_xilinx_init(sdla_t *card)
{
	xilinx_softc_t	*sc;
	struct ifnet	*ifp;

	/* Verify configuration ID */
	bit_clear((u_int8_t *)&card->critical, CARD_DOWN);

	card->u.xilinx.num_of_ch = 0;
	card->u.xilinx.mru_trans = 1500;
	card->u.xilinx.dma_per_ch = 10;

	/* TE1 Make special hardware initialization for T1/E1 board */

	if (IS_TE1(&card->fe_te.te_cfg)) {
		card->write_front_end_reg = write_front_end_reg;
		card->read_front_end_reg = read_front_end_reg;
		card->te_enable_timer = enable_timer;
		card->te_link_state = handle_front_end_state;
	} else
		card->front_end_status = FE_CONNECTED;

	/* WARNING: After this point the init function
	 * must return with 0.  The following bind
	 * functions will cause problems if structures
	 * below are not initialized */

	card->del_if	 = &wan_xilinx_release;
	card->iface_up   = &wan_xilinx_up;
	card->iface_down = &wan_xilinx_down;
	card->iface_send = &wan_xilinx_send;
	card->iface_ioctl= &wan_xilinx_ioctl;

	write_cpld(card, LED_CONTROL_REG, 0x0E);

	sdla_getcfg(card->hw, SDLA_BASEADDR, &card->u.xilinx.bar);

	xilinx_delay(1);

	timeout_set(&card->u.xilinx.led_timer, aft_led_timer, (void *)card);

	/* allocate and initialize private data */
	sc = malloc(sizeof(xilinx_softc_t), M_DEVBUF, M_NOWAIT);
	if (sc == NULL)
		return (NULL);

	memset(sc, 0, sizeof(xilinx_softc_t));
	ifp = (struct ifnet *)&sc->common.ifp;
	ifp->if_softc = sc;
	sc->common.card	= card;
	if (wanpipe_generic_register(card, ifp, card->devname)) {
		free(sc, M_DEVBUF);
		return (NULL);
	}

	strlcpy(sc->if_name, ifp->if_xname, IFNAMSIZ);
	sc->first_time_slot = -1;
	sc->time_slot_map = 0;
	sdla_getcfg(card->hw, SDLA_DMATAG, &sc->dmatag);

	IFQ_SET_MAXLEN(&sc->wp_tx_pending_list, MAX_TX_BUF);
	sc->wp_tx_pending_list.ifq_len = 0;
	IFQ_SET_MAXLEN(&sc->wp_tx_complete_list, MAX_TX_BUF);
	sc->wp_tx_complete_list.ifq_len = 0;

	aft_alloc_rx_buffers(sc);

	xilinx_delay(1);

	ifmedia_init(&sc->common.ifm, 0, wan_ifmedia_upd, wan_ifmedia_sts);

	if (IS_TE1(&card->fe_te.te_cfg)) {
		ifmedia_add(&sc->common.ifm, IFM_TDM|IFM_TDM_T1, 0, NULL);
		ifmedia_add(&sc->common.ifm, IFM_TDM|IFM_TDM_T1_AMI, 0, NULL);
		ifmedia_add(&sc->common.ifm, IFM_TDM|IFM_TDM_E1, 0, NULL);
		ifmedia_add(&sc->common.ifm, IFM_TDM|IFM_TDM_E1_AMI, 0, NULL);

		ifmedia_add(&sc->common.ifm,
		    IFM_TDM|IFM_TDM_T1|IFM_TDM_PPP, 0, NULL);
		ifmedia_add(&sc->common.ifm,
		    IFM_TDM|IFM_TDM_T1_AMI|IFM_TDM_PPP, 0, NULL);
		ifmedia_add(&sc->common.ifm,
		    IFM_TDM|IFM_TDM_E1|IFM_TDM_PPP, 0, NULL);
		ifmedia_add(&sc->common.ifm,
		    IFM_TDM|IFM_TDM_E1_AMI|IFM_TDM_PPP, 0, NULL);

		ifmedia_set(&sc->common.ifm, IFM_TDM|IFM_TDM_T1);
	} else {
		/* Currently we not support ifmedia types for other
		 * front end types.
		 */
	}

	return (sc);
}

static int
wan_xilinx_release(sdla_t* card, struct ifnet* ifp)
{
	xilinx_softc_t *sc = ifp->if_softc;

	IF_PURGE(&sc->wp_tx_pending_list);

	if (sc->tx_dma_addr && sc->tx_dma_len) {
		sc->tx_dma_addr = 0;
		sc->tx_dma_len = 0;
	}

	if (sc->tx_dma_mbuf) {
		log(LOG_INFO, "freeing tx dma mbuf\n");
		bus_dmamap_unload(sc->dmatag, sc->tx_dmamap);
		m_freem(sc->tx_dma_mbuf);
		sc->tx_dma_mbuf = NULL;
	}

#if 0
	bus_dmamap_destroy(sc->dmatag, sc->tx_dmamap);
#endif
	if (sc->rx_dma_buf) {
		SIMPLEQ_INSERT_TAIL(&sc->wp_rx_free_list,
		    sc->rx_dma_buf, entry);
		sc->rx_dma_buf = NULL;
	}

	aft_release_rx_buffers(sc);

	wanpipe_generic_unregister(ifp);
	ifp->if_softc = NULL;
	free(sc, M_DEVBUF);

	return (0);
}

static void
wan_ifmedia_sts(struct ifnet *ifp, struct ifmediareq *ifmreq)
{
	wanpipe_common_t	*common = (wanpipe_common_t *)ifp->if_softc;
	struct ifmedia		*ifm;

	WAN_ASSERT1(common == NULL);
	ifm = &common->ifm;
	ifmreq->ifm_active = ifm->ifm_cur->ifm_media;
}

static int
wan_ifmedia_upd(struct ifnet *ifp)
{
	wanpipe_common_t	*common = (wanpipe_common_t *)ifp->if_softc;
	sdla_t			*card;

	WAN_ASSERT(common == NULL);
	WAN_ASSERT(common->card == NULL);
	card = (sdla_t *)common->card;

	if (IS_TE1(&card->fe_te.te_cfg))
		return (sdla_te_setcfg(ifp, &common->ifm));

	return (EINVAL);
}


/*
 * KERNEL Device Entry Interfaces
 */

static int
wan_xilinx_up(struct ifnet *ifp)
{
	xilinx_softc_t	*sc = ifp->if_softc;
	sdla_t		*card = NULL;
	struct timeval	 tv;
	int		 err = 0;

	WAN_ASSERT(sc == NULL);
	WAN_ASSERT(sc->common.card == NULL);
	card = (sdla_t *)sc->common.card;

	if (card->state != WAN_DISCONNECTED)
		return (0);

	sc->time_slot_map = card->fe_te.te_cfg.active_ch;
	sc->dma_mtu = xilinx_valid_mtu(ifp->if_mtu+100);

	if (!sc->dma_mtu) {
		log(LOG_INFO, "%s:%s: Error invalid MTU %d\n",
		    card->devname, sc->if_name, ifp->if_mtu);
		return (EINVAL);
	}

#ifdef DEBUG_INIT
	log(LOG_INFO, "%s: Allocating %d dma mbuf len=%d\n",
	    card->devname, card->u.xilinx.dma_per_ch, sc->dma_mtu);
#endif
	if (aft_alloc_rx_dma_buff(sc, card->u.xilinx.dma_per_ch) == 0)
		return (ENOMEM);

	if (bus_dmamap_create(sc->dmatag, sc->dma_mtu, 1, sc->dma_mtu,
	      0, BUS_DMA_NOWAIT | BUS_DMA_ALLOCNOW, &sc->tx_dmamap)) {
		log(LOG_INFO, "%s: Failed to allocate tx dmamap\n",
		    sc->if_name);
		return (ENOMEM);
	}

	err = xilinx_chip_configure(card);
	if (err)
		return (EINVAL);

	card->isr = &wp_xilinx_isr;

	err = xilinx_dev_configure(card, sc);
	if (err) {
		xilinx_chip_unconfigure(card);
		return (EINVAL);
	}
	xilinx_delay(1);

	/* Initialize the router start time.
	 * Used by wanpipemon debugger to indicate
	 * how long has	the interface been up */
	microtime(&tv);
	sc->router_start_time = tv.tv_sec;

	xilinx_init_tx_dma_descr(card, sc);
	xilinx_dev_enable(card, sc);

	sc->ignore_modem = 0x0F;
	bit_clear((u_int8_t *)&card->critical, CARD_DOWN);
	port_set_state(card, WAN_CONNECTING);

	return (err);
}

static int
wan_xilinx_down(struct ifnet *ifp)
{
	xilinx_softc_t	*sc = ifp->if_softc;
	sdla_t		*card = (sdla_t *)sc->common.card;
	struct xilinx_rx_buffer *buf;
	int		s;

	if (card->state == WAN_DISCONNECTED)
		return (0);

	xilinx_dev_close(card, sc);

	/* Disable DMA ENGINE before we perform
	 * core reset.  Otherwise, we will receive
	 * rx fifo errors on subsequent resetart. */
	disable_data_error_intr(card, DEVICE_DOWN);

	bit_set((u_int8_t *)&card->critical, CARD_DOWN);

	timeout_del(&card->u.xilinx.led_timer);

	/* TE1 - Unconfiging, only on shutdown */
	if (IS_TE1(&card->fe_te.te_cfg))
		sdla_te_unconfig(card);

	s = splnet();

	card->isr = NULL;

	if (sc->tx_dma_addr && sc->tx_dma_len) {
		sc->tx_dma_addr = 0;
		sc->tx_dma_len = 0;
	}

	if (sc->tx_dma_mbuf) {
		bus_dmamap_unload(sc->dmatag, sc->tx_dmamap);
		m_freem(sc->tx_dma_mbuf);
		sc->tx_dma_mbuf = NULL;
	}

	bus_dmamap_destroy(sc->dmatag, sc->tx_dmamap);

	/* If there is something left in rx_dma_buf, then move it to
	 * rx_free_list.
	 */
	if (sc->rx_dma_buf) {
		aft_reload_rx_dma_buff(sc, sc->rx_dma_buf);
		sc->rx_dma_buf = NULL;
	}

	while ((buf = SIMPLEQ_FIRST(&sc->wp_rx_free_list)) != NULL) {
		SIMPLEQ_REMOVE_HEAD(&sc->wp_rx_free_list, entry);
		aft_release_rx_dma_buff(sc, buf);
	}

	while ((buf = SIMPLEQ_FIRST(&sc->wp_rx_complete_list)) != NULL) {
		SIMPLEQ_REMOVE_HEAD(&sc->wp_rx_complete_list, entry);
		aft_release_rx_dma_buff(sc, buf);		
	}

	splx(s);

	DELAY(10);

	xilinx_dev_unconfigure(card, sc);
	xilinx_chip_unconfigure(card);

	port_set_state(card, WAN_DISCONNECTED);
	sc->ignore_modem = 0x00;
	return (0);
}

static int
wan_xilinx_send(struct mbuf* m, struct ifnet* ifp)
{

	xilinx_softc_t *sc = ifp->if_softc;
	sdla_t *card = (sdla_t *)sc->common.card;

	/* Mark interface as busy. The kernel will not
	 * attempt to send any more packets until we clear
	 * this condition */

	if (m == NULL)
		/* This should never happen. Just a sanity check.
		 */
		return (EINVAL);

	if (card->state != WAN_CONNECTED) {
		/*
		 * The card is still not ready to transmit...
		 * drop this packet!
		 */
		m_freem(m);
		return (EINVAL);

	} else {
		if (IF_QFULL(&sc->wp_tx_pending_list)) {
			int err;
#ifdef DEBUG_TX
			log(LOG_INFO, "%s: Tx pending queue FULL\n",
				ifp->if_xname);
#endif
			/*
			 * TX pending queue is full. Try to send packet
			 * from tx_pending queue (first)
			 */
			err = xilinx_dma_tx(card, sc);
			if (!err && !IF_QFULL(&sc->wp_tx_pending_list))
				/*
				 * On success, we have place for the new
				 * tx packet, try to send it now!
				 */
				goto wan_xilinx_dma_tx_try;

			/*
			 * Tx pedning queue is full. I can't accept new
			 * tx packet, drop this packet and set interface
			 * queue to OACTIVE
			 */
			m_freem(m);
			ifp->if_flags |= IFF_OACTIVE;

			return (EBUSY);
		} else {
wan_xilinx_dma_tx_try:
			IF_ENQUEUE(&sc->wp_tx_pending_list, m);
			xilinx_dma_tx(card, sc);
		}
	}

	return (0);
}

static int
wan_xilinx_ioctl(struct ifnet *ifp, int cmd, struct ifreq *ifr)
{
	xilinx_softc_t	*sc = (xilinx_softc_t *)ifp->if_softc;
	struct mbuf	*m;
	sdla_t		*card;
	wan_udp_pkt_t	*wan_udp_pkt;
	int err = 0;

	if (!sc)
		return (ENODEV);

	card = (sdla_t *)sc->common.card;

	switch (cmd) {
	case SIOC_WANPIPE_PIPEMON:

		if ((err = suser(curproc, 0)) != 0)
			break;

		if (IF_QFULL(&sc->udp_queue))
			return (EBUSY);

		/*
		 * For performance reasons test the critical
		 * here before spin lock
		 */
		if (bit_test((u_int8_t *)&card->in_isr, 0))
			return (EBUSY);

		m = wan_mbuf_alloc(sizeof(wan_udp_pkt_t));
		if (m == NULL)
			return (ENOMEM);

		wan_udp_pkt = mtod(m, wan_udp_pkt_t *);
		if (copyin(ifr->ifr_data, &wan_udp_pkt->wan_udp_hdr,
		    sizeof(wan_udp_hdr_t))) {
			m_freem(m);
			return (EFAULT);
		}
		IF_ENQUEUE(&sc->udp_queue, m);

		process_udp_mgmt_pkt(card, ifp, sc, 1);

		if (copyout(&wan_udp_pkt->wan_udp_hdr, ifr->ifr_data,
		    sizeof(wan_udp_hdr_t))) {
			m_freem(m);
			return (EFAULT);
		}

		IF_DEQUEUE(&sc->udp_queue, m);
		m_freem(m);
		return (0);

	default:
		if (card->ioctl)
			err = card->ioctl(ifp, cmd, ifr);
		break;
	}

	return (err);
}

/*
 * Process all "wanpipemon" debugger commands.  This function
 * performs all debugging tasks:
 *
 *	Line Tracing
 *	Line/Hardware Statistics
 *	Protocol Statistics
 *
 * "wanpipemon" utility is a user-space program that
 * is used to debug the WANPIPE product.
 */
static int
process_udp_mgmt_pkt(sdla_t* card, struct ifnet* ifp,
    xilinx_softc_t* sc, int local_dev )
{
	struct mbuf	*m;
	unsigned short	 buffer_length;
	wan_udp_pkt_t	*wan_udp_pkt;
	wan_trace_t	*trace_info = NULL;
	struct timeval	 tv;

	IF_POLL(&sc->udp_queue, m);
	if (m == NULL)
		return (EINVAL);

	wan_udp_pkt = mtod(m, wan_udp_pkt_t *);
	trace_info=&sc->trace_info;

	{
		struct mbuf *m0;

		wan_udp_pkt->wan_udp_opp_flag = 0;

		switch (wan_udp_pkt->wan_udp_command) {

		case READ_CONFIGURATION:
		case READ_CODE_VERSION:
			wan_udp_pkt->wan_udp_return_code = 0;
			wan_udp_pkt->wan_udp_data_len = 0;
			break;


		case ENABLE_TRACING:

			wan_udp_pkt->wan_udp_return_code = WAN_CMD_OK;
			wan_udp_pkt->wan_udp_data_len = 0;

			if (!bit_test((u_int8_t *)
			    &trace_info->tracing_enabled, 0)) {

				trace_info->trace_timeout = ticks;

				IF_PURGE(&trace_info->ifq);
				if (wan_udp_pkt->wan_udp_data[0] == 0) {
					bit_clear((u_int8_t *)
					    &trace_info->tracing_enabled, 1);
					log(LOG_INFO, "%s: ADSL L3 "
					    "trace enabled!\n", card->devname);
				} else if (wan_udp_pkt->wan_udp_data[0] == 1) {
					bit_clear((u_int8_t *)
					    &trace_info->tracing_enabled, 2 );
					bit_set((u_int8_t *)
					    &trace_info->tracing_enabled, 1);
					log(LOG_INFO, "%s: ADSL L2 "
					    "trace enabled!\n", card->devname);
				} else {
					bit_clear((u_int8_t *)
					    &trace_info->tracing_enabled, 1);
					bit_set((u_int8_t *)
					    &trace_info->tracing_enabled, 2);
					log(LOG_INFO, "%s: ADSL L1 "
					    "trace enabled!\n", card->devname);
				}
				bit_set((u_int8_t *)&
				    trace_info->tracing_enabled, 0);

			} else {
				log(LOG_INFO, "%s: Error: AFT "
				    "trace running!\n", card->devname);
				wan_udp_pkt->wan_udp_return_code = 2;
			}

			break;

		case DISABLE_TRACING:
			wan_udp_pkt->wan_udp_return_code = WAN_CMD_OK;

			if (bit_test((u_int8_t *)
			    &trace_info->tracing_enabled, 0)) {
				bit_clear((u_int8_t *)
				    &trace_info->tracing_enabled, 0);
				bit_clear((u_int8_t *)
				    &trace_info->tracing_enabled, 1);
				bit_clear((u_int8_t *)
				    &trace_info->tracing_enabled, 2);
				IF_PURGE(&trace_info->ifq);
				log(LOG_INFO, "%s: Disabling ADSL trace\n",
				    card->devname);
			} else {
				/*
				 * set return code to line trace already
				 * disabled
				 */
				wan_udp_pkt->wan_udp_return_code = 1;
			}

			break;

		case GET_TRACE_INFO:
			if (bit_test((u_int8_t *)
			    &trace_info->tracing_enabled, 0)) {
				trace_info->trace_timeout = ticks;
			} else {
				log(LOG_INFO, "%s: Error AFT trace "
				    "not enabled\n", card->devname);
				/* set return code */
				wan_udp_pkt->wan_udp_return_code = 1;
				break;
			}

			buffer_length = 0;
			wan_udp_pkt->wan_udp_aft_num_frames = 0;
			wan_udp_pkt->wan_udp_aft_ismoredata = 0;

			while (!IF_IS_EMPTY(&trace_info->ifq)) {
				IF_POLL(&trace_info->ifq, m0);
				if (m0 == NULL) {
					log(LOG_INFO, "%s: No more "
					    "trace packets in trace queue!\n",
					    card->devname);
					break;
				}
				if ((WAN_MAX_DATA_SIZE - buffer_length) <
				    m0->m_pkthdr.len) {
					/*
					 * indicate there are more frames
					 * on board & exit
					 */
					wan_udp_pkt->wan_udp_aft_ismoredata
								= 0x01;
					break;
				}

				m_copydata(m0, 0, m0->m_pkthdr.len,
				    &wan_udp_pkt->wan_udp_data[buffer_length]);
				buffer_length += m0->m_pkthdr.len;
				IF_DEQUEUE(&trace_info->ifq, m0);
				if (m0)
					m_freem(m0);
				wan_udp_pkt->wan_udp_aft_num_frames++;
			}
			/* set the data length and return code */
			wan_udp_pkt->wan_udp_data_len = buffer_length;
			wan_udp_pkt->wan_udp_return_code = WAN_CMD_OK;
			break;

		case ROUTER_UP_TIME:
			microtime(&tv);
			sc->router_up_time = tv.tv_sec;
			sc->router_up_time -= sc->router_start_time;
			*(unsigned long *)&wan_udp_pkt->wan_udp_data =
					sc->router_up_time;
			wan_udp_pkt->wan_udp_data_len = sizeof(unsigned long);
			wan_udp_pkt->wan_udp_return_code = 0;
			break;

		case WAN_GET_MEDIA_TYPE:
		case WAN_FE_GET_STAT:
		case WAN_FE_SET_LB_MODE:
		case WAN_FE_FLUSH_PMON:
		case WAN_FE_GET_CFG:

			if (IS_TE1(&card->fe_te.te_cfg)) {
				sdla_te_udp(card,
				    &wan_udp_pkt->wan_udp_cmd,
				    &wan_udp_pkt->wan_udp_data[0]);
			} else {
				if (wan_udp_pkt->wan_udp_command ==
				    WAN_GET_MEDIA_TYPE) {
					wan_udp_pkt->wan_udp_data_len =
					    sizeof(unsigned char);
					wan_udp_pkt->wan_udp_return_code =
					    WAN_CMD_OK;
				} else {
					wan_udp_pkt->wan_udp_return_code =
					    WAN_UDP_INVALID_CMD;
				}
			}
			break;

		case WAN_GET_PROTOCOL:
			wan_udp_pkt->wan_udp_aft_num_frames = WANCONFIG_AFT;
			wan_udp_pkt->wan_udp_return_code = WAN_CMD_OK;
			wan_udp_pkt->wan_udp_data_len = 1;
			break;

		case WAN_GET_PLATFORM:
			wan_udp_pkt->wan_udp_data[0] = WAN_PLATFORM_ID;
			wan_udp_pkt->wan_udp_return_code = WAN_CMD_OK;
			wan_udp_pkt->wan_udp_data_len = 1;
			break;

		default:
			wan_udp_pkt->wan_udp_data_len = 0;
			wan_udp_pkt->wan_udp_return_code = 0xCD;

			log(LOG_INFO, "%s: Warning, Illegal UDP "
			    "command attempted from network: %x\n",
			    card->devname, wan_udp_pkt->wan_udp_command);
			break;
		}
	}

	wan_udp_pkt->wan_udp_request_reply = UDPMGMT_REPLY;
	return (1);
}

/*
 * FIRMWARE Specific Interface Functions
 */

static int
xilinx_chip_configure(sdla_t *card)
{
	u_int32_t reg, tmp;
	int err = 0;
	u_int16_t adapter_type, adptr_security;

#ifdef DEBUG_INIT
	log(LOG_DEBUG, "Xilinx Chip Configuration. -- \n");
#endif
	xilinx_delay(1);

	sdla_bus_read_4(card->hw, XILINX_CHIP_CFG_REG, &reg);

	/* Configure for T1 or E1 front end */
	if (IS_T1(&card->fe_te.te_cfg)) {
		card->u.xilinx.num_of_time_slots = NUM_OF_T1_CHANNELS;
		bit_clear((u_int8_t *)&reg, INTERFACE_TYPE_T1_E1_BIT);
		bit_set((u_int8_t *)&reg, FRONT_END_FRAME_FLAG_ENABLE_BIT);
	} else if (IS_E1(&card->fe_te.te_cfg)) {
		card->u.xilinx.num_of_time_slots = NUM_OF_E1_CHANNELS;
		bit_set((u_int8_t *)&reg, INTERFACE_TYPE_T1_E1_BIT);
		bit_set((u_int8_t *)&reg, FRONT_END_FRAME_FLAG_ENABLE_BIT);
	} else {
		log(LOG_INFO, "%s: Error: Xilinx doesn't "
		    "support non T1/E1 interface!\n", card->devname);
		return (EINVAL);
	}

	sdla_bus_write_4(card->hw, XILINX_CHIP_CFG_REG, reg);

	DELAY(10000);

	/* Reset PMC */
	sdla_bus_read_4(card->hw, XILINX_CHIP_CFG_REG, &reg);
	bit_clear((u_int8_t *)&reg, FRONT_END_RESET_BIT);
	sdla_bus_write_4(card->hw, XILINX_CHIP_CFG_REG, reg);
	DELAY(1000);

	bit_set((u_int8_t *)&reg, FRONT_END_RESET_BIT);
	sdla_bus_write_4(card->hw, XILINX_CHIP_CFG_REG, reg);
	DELAY(100);

#ifdef DEBUG_INIT
	log(LOG_DEBUG, "--- Chip Reset. -- \n");
#endif

	/* Reset Chip Core */
	sdla_bus_read_4(card->hw, XILINX_CHIP_CFG_REG, &reg);
	bit_set((u_int8_t *)&reg, CHIP_RESET_BIT);
	sdla_bus_write_4(card->hw, XILINX_CHIP_CFG_REG, reg);

	DELAY(100);

	/* Disable the chip/hdlc reset condition */
	bit_clear((u_int8_t *)&reg, CHIP_RESET_BIT);

	/* Disable ALL chip interrupts */
	bit_clear((u_int8_t *)&reg, GLOBAL_INTR_ENABLE_BIT);
	bit_clear((u_int8_t *)&reg, ERROR_INTR_ENABLE_BIT);
	bit_clear((u_int8_t *)&reg, FRONT_END_INTR_ENABLE_BIT);

	sdla_bus_write_4(card->hw, XILINX_CHIP_CFG_REG, reg);

	xilinx_delay(1);

	sdla_getcfg(card->hw, SDLA_ADAPTERTYPE, &adapter_type);
	DELAY(100);

#ifdef DEBUG_INIT
	log(LOG_INFO, "%s: Hardware Adapter Type 0x%X\n",
	    card->devname, adapter_type);
#endif

	adptr_security = read_cpld(card, SECURITY_CPLD_REG);
	adptr_security = adptr_security >> SECURITY_CPLD_SHIFT;
	adptr_security = adptr_security & SECURITY_CPLD_MASK;

#ifdef DEBUG_INIT
	switch (adptr_security) {
	case SECURITY_1LINE_UNCH:
		log(LOG_INFO, "%s: Security 1 Line UnCh\n", card->devname);
		break;
	case SECURITY_1LINE_CH:
		log(LOG_INFO, "%s: Security 1 Line Ch\n", card->devname);
		break;
	case SECURITY_2LINE_UNCH:
		log(LOG_INFO, "%s: Security 2 Line UnCh\n", card->devname);
		break;
	case SECURITY_2LINE_CH:
		log(LOG_INFO, "%s: Security 2 Line Ch\n", card->devname);
		break;
	default:
		log(LOG_INFO, "%s: Error Invalid Security ID = 0x%X\n",
		    card->devname, adptr_security);
		/* return EINVAL;*/
	}
#endif

	/* Turn off Onboard RED LED */
	sdla_bus_read_4(card->hw, XILINX_CHIP_CFG_REG, &reg);
	bit_set((u_int8_t *)&reg, XILINX_RED_LED);
	sdla_bus_write_4(card->hw, XILINX_CHIP_CFG_REG, reg);
	DELAY(10);

	err = aft_core_ready(card);
	if (err != 0)
		log(LOG_INFO, "%s: WARNING: HDLC Core Not Ready: B4 TE CFG!\n",
		    card->devname);

	log(LOG_INFO, "%s: Configuring A101 PMC T1/E1/J1 Front End\n",
	    card->devname);

	if (sdla_te_config(card)) {
		log(LOG_INFO, "%s: Failed %s configuratoin!\n", card->devname,
		    IS_T1(&card->fe_te.te_cfg)?"T1":"E1");
		return (EINVAL);
	}

	xilinx_delay(1);

	err = aft_core_ready(card);
	if (err != 0) {
		log(LOG_INFO, "%s: Error: HDLC Core Not Ready!\n",
		    card->devname);

		sdla_bus_read_4(card->hw, XILINX_CHIP_CFG_REG, &reg);

		/* Disable the chip/hdlc reset condition */
		bit_set((u_int8_t *)&reg, CHIP_RESET_BIT);

		sdla_bus_write_4(card->hw, XILINX_CHIP_CFG_REG, reg);
		return (err);
	}

#ifdef DEBUG_INIT
	log(LOG_INFO, "%s: HDLC Core Ready 0x%08X\n",
	    card->devname, reg);
#endif

	xilinx_delay(1);

	/* Setup global DMA parameters */
	reg = 0;
	reg|=(XILINX_DMA_SIZE    << DMA_SIZE_BIT_SHIFT);
	reg|=(XILINX_DMA_FIFO_UP << DMA_FIFO_HI_MARK_BIT_SHIFT);
	reg|=(XILINX_DMA_FIFO_LO << DMA_FIFO_LO_MARK_BIT_SHIFT);

	/*
	 * Enable global DMA engine and set to default
	 * number of active channels. Note: this value will
	 * change in dev configuration
	 */
	reg|=(XILINX_DEFLT_ACTIVE_CH << DMA_ACTIVE_CHANNEL_BIT_SHIFT);
	bit_set((u_int8_t *)&reg, DMA_ENGINE_ENABLE_BIT);

#ifdef DEBUG_INIT
	log(LOG_INFO, "--- Setup DMA control Reg. -- \n");
#endif

	sdla_bus_write_4(card->hw, XILINX_DMA_CONTROL_REG, reg);

#ifdef DEBUG_INIT
	log(LOG_INFO, "--- Tx/Rx global enable. -- \n");
#endif

	xilinx_delay(1);

	reg = 0;
	sdla_bus_write_4(card->hw, XILINX_TIMESLOT_HDLC_CHAN_REG, reg);

	/* Clear interrupt pending registers befor first interrupt enable */
	sdla_bus_read_4(card->hw, XILINX_DMA_RX_INTR_PENDING_REG, &tmp);
	sdla_bus_read_4(card->hw, XILINX_DMA_TX_INTR_PENDING_REG, &tmp);
	sdla_bus_read_4(card->hw, XILINX_HDLC_RX_INTR_PENDING_REG, &tmp);
	sdla_bus_read_4(card->hw, XILINX_HDLC_TX_INTR_PENDING_REG, &tmp);
	sdla_bus_read_4(card->hw, XILINX_CHIP_CFG_REG, (u_int32_t *)&reg);
	if (bit_test((u_int8_t *)&reg, DMA_INTR_FLAG)) {
		log(LOG_INFO, "%s: Error: Active DMA Interrupt Pending. !\n",
		    card->devname);

		reg = 0;
		/* Disable the chip/hdlc reset condition */
		bit_set((u_int8_t *)&reg, CHIP_RESET_BIT);
		sdla_bus_write_4(card->hw, XILINX_CHIP_CFG_REG, reg);
		return (err);
	}
	if (bit_test((u_int8_t *)&reg, ERROR_INTR_FLAG)) {
		log(LOG_INFO, "%s: Error: Active Error Interrupt Pending. !\n",
		    card->devname);

		reg = 0;
		/* Disable the chip/hdlc reset condition */
		bit_set((u_int8_t *)&reg, CHIP_RESET_BIT);
		sdla_bus_write_4(card->hw, XILINX_CHIP_CFG_REG, reg);
		return (err);
	}


	/* Alawys disable global data and error interrupts */
	bit_clear((u_int8_t *)&reg, GLOBAL_INTR_ENABLE_BIT);
	bit_clear((u_int8_t *)&reg, ERROR_INTR_ENABLE_BIT);

	/* Always enable the front end interrupt */
	bit_set((u_int8_t *)&reg, FRONT_END_INTR_ENABLE_BIT);

#ifdef DEBUG_INIT
	log(LOG_DEBUG, "--- Set Global Interrupts (0x%X)-- \n", reg);
#endif

	xilinx_delay(1);

	sdla_bus_write_4(card->hw, XILINX_CHIP_CFG_REG, reg);

	return (err);
}

static int
xilinx_chip_unconfigure(sdla_t *card)
{
	u_int32_t	reg = 0;

	sdla_bus_write_4(card->hw, XILINX_TIMESLOT_HDLC_CHAN_REG, reg);
	sdla_bus_read_4(card->hw, XILINX_CHIP_CFG_REG, &reg);
	/* Enable the chip/hdlc reset condition */
	reg = 0;
	bit_set((u_int8_t *)&reg, CHIP_RESET_BIT);

	sdla_bus_write_4(card->hw, XILINX_CHIP_CFG_REG, reg);
	return (0);
}

static int
xilinx_dev_configure(sdla_t *card, xilinx_softc_t *sc)
{
	u_int32_t reg;
	long free_logic_ch, i;

	sc->logic_ch_num=-1;

	if (!IS_TE1(&card->fe_te.te_cfg))
		return (EINVAL);

	if (IS_E1(&card->fe_te.te_cfg)) {
		log(LOG_DEBUG, "%s: Time Slot Orig 0x%lX  Shifted 0x%lX\n",
		    sc->if_name, sc->time_slot_map, sc->time_slot_map << 1);
		sc->time_slot_map = sc->time_slot_map << 1;
		bit_clear((u_int8_t *)&sc->time_slot_map, 0);
	}

	/*
	 * Channel definition section. If not channels defined
	 * return error
	 */
	if (sc->time_slot_map == 0) {
		log(LOG_INFO, "%s: Invalid Channel Selection 0x%lX\n",
		    card->devname, sc->time_slot_map);
		return (EINVAL);
	}

#ifdef DEBUG_INIT
	log(LOG_INFO, "%s:%s: Active channels = 0x%lX\n", card->devname,
	    sc->if_name, sc->time_slot_map);
#endif
	xilinx_delay(1);

	/*
	 * Check that the time slot is not being used. If it is
	 * stop the interface setup.  Notice, though we proceed
	 * to check for all timeslots before we start binding
	 * the channels in.  This way, we don't have to go back
	 * and clean the time_slot_map
	 */
	for (i = 0; i < card->u.xilinx.num_of_time_slots; i++) {
		if (bit_test((u_int8_t *)&sc->time_slot_map, i)) {

			if (sc->first_time_slot == -1) {
#ifdef DEBUG_INIT
				log(LOG_INFO, "%s: Setting first time "
				    "slot to %ld\n", card->devname, i);
#endif
				sc->first_time_slot = i;
			}

#ifdef DEBUG_INIT
			log(LOG_DEBUG, "%s: Configuring %s for timeslot %ld\n",
			    card->devname, sc->if_name,
			    IS_E1(&card->fe_te.te_cfg)?i:i+1);
#endif
			if (bit_test((u_int8_t *)
			    &card->u.xilinx.time_slot_map, i)) {
				log(LOG_INFO, "%s: Channel/Time Slot "
				    "resource conflict!\n", card->devname);
				log(LOG_INFO, "%s: %s: Channel/Time Slot "
				    "%ld, aready in use!\n",
				    card->devname, sc->if_name, (i+1));

				return (EEXIST);
			}

			/* Calculate the number of timeslots for this if */
			++sc->num_of_time_slots;
		}
	}

	xilinx_delay(1);

	sc->logic_ch_num = request_xilinx_logical_channel_num(card,
	    sc, &free_logic_ch);
	if (sc->logic_ch_num == -1)
		return (EBUSY);

	xilinx_delay(1);

	for (i = 0; i < card->u.xilinx.num_of_time_slots; i++) {
		if (bit_test((u_int8_t *)&sc->time_slot_map, i)) {

			bit_set((u_int8_t *)&card->u.xilinx.time_slot_map, i);

			sdla_bus_read_4(card->hw,
			    XILINX_TIMESLOT_HDLC_CHAN_REG, &reg);
			reg &= ~TIMESLOT_BIT_MASK;

			/* FIXME do not hardcode !*/
			reg &= HDLC_LCH_TIMESLOT_MASK; /* mask not valid bits*/

			/* Select a Timeslot for configuration */
			sdla_bus_write_4(card->hw,
			    XILINX_TIMESLOT_HDLC_CHAN_REG,
			    (reg | (i << TIMESLOT_BIT_SHIFT)));

			reg = sc->logic_ch_num & CONTROL_RAM_DATA_MASK;

#ifdef TRUE_FIFO_SIZE
			reg |= (sc->fifo_size_code & HDLC_FIFO_SIZE_MASK) <<
			    HDLC_FIFO_SIZE_SHIFT;
#else
			reg |= (HARD_FIFO_CODE &
			    HDLC_FIFO_SIZE_MASK) << HDLC_FIFO_SIZE_SHIFT;
#endif /* TRUE_FIFO_SIZE */

			reg |= (sc->fifo_base_addr & HDLC_FIFO_BASE_ADDR_MASK)
			    << HDLC_FIFO_BASE_ADDR_SHIFT;

#ifdef DEBUG_INIT
			log(LOG_INFO, "Setting Timeslot %ld to logic "
			    "ch %ld Reg=0x%X\n", i, sc->logic_ch_num, reg);
#endif
			xilinx_write_ctrl_hdlc(card, i,
			    XILINX_CONTROL_RAM_ACCESS_BUF, reg);
		}
	}

	if (free_logic_ch != -1) {

		char free_ch_used = 0;

		for (i = 0; i < card->u.xilinx.num_of_time_slots; i++) {
			if (!bit_test((u_int8_t *)
			    &card->u.xilinx.time_slot_map, i)) {

				sdla_bus_read_4(card->hw,
				    XILINX_TIMESLOT_HDLC_CHAN_REG, &reg);

				reg &= ~TIMESLOT_BIT_MASK;
				/* mask not valid bits */
				reg &= HDLC_LCH_TIMESLOT_MASK;

				/* Select a Timeslot for configuration */
				sdla_bus_write_4(card->hw,
				    XILINX_TIMESLOT_HDLC_CHAN_REG,
				    (reg | (i << TIMESLOT_BIT_SHIFT)));

				reg = free_logic_ch&CONTROL_RAM_DATA_MASK;

				/* For the rest of the unused logic channels
				 * bind them to timeslot 31 and set the fifo
				 * size to 32 byte = Code = 0x00 */
				reg |= (FIFO_32B & HDLC_FIFO_SIZE_MASK)
				    << HDLC_FIFO_SIZE_SHIFT;

				reg |= (free_logic_ch &
				    HDLC_FIFO_BASE_ADDR_MASK) <<
				    HDLC_FIFO_BASE_ADDR_SHIFT;

#ifdef DEBUG_INIT
				log(LOG_INFO, "Setting Timeslot "
				    "%ld to free logic ch %ld Reg=0x%X\n",
				    i, free_logic_ch, reg);
#endif
				xilinx_write_ctrl_hdlc(card, i,
				    XILINX_CONTROL_RAM_ACCESS_BUF, reg);

				free_ch_used = 1;
			}
		}

		/* We must check if the free logic has been bound
		 * to any timeslots */
		if (free_ch_used) {
#ifdef DEBUG_INIT
			log(LOG_INFO, "%s: Setting Free CH %ld to idle\n",
			    sc->if_name, free_logic_ch);
#endif
			xilinx_delay(1);

			/* Setup the free logic channel as IDLE */

			sdla_bus_read_4(card->hw,
			    XILINX_TIMESLOT_HDLC_CHAN_REG, &reg);

			reg &= ~HDLC_LOGIC_CH_BIT_MASK;

			/* mask not valid bits */
			reg &= HDLC_LCH_TIMESLOT_MASK;

			sdla_bus_write_4(card->hw,
			    XILINX_TIMESLOT_HDLC_CHAN_REG,
			    (reg|(free_logic_ch&HDLC_LOGIC_CH_BIT_MASK)));

			reg = 0;
			bit_clear((u_int8_t *)&reg, HDLC_RX_PROT_DISABLE_BIT);
			bit_clear((u_int8_t *)&reg, HDLC_TX_PROT_DISABLE_BIT);

			bit_set((u_int8_t *)&reg, HDLC_RX_ADDR_RECOGN_DIS_BIT);

			xilinx_write_ctrl_hdlc(card, sc->first_time_slot,
			    XILINX_HDLC_CONTROL_REG, reg);
		}
	}

	/* Select an HDLC logic channel for configuration */
	sdla_bus_read_4(card->hw, XILINX_TIMESLOT_HDLC_CHAN_REG, &reg);

	reg &= ~HDLC_LOGIC_CH_BIT_MASK;
	reg &= HDLC_LCH_TIMESLOT_MASK;         /* mask not valid bits */

	sdla_bus_write_4(card->hw, XILINX_TIMESLOT_HDLC_CHAN_REG,
	    (reg | (sc->logic_ch_num & HDLC_LOGIC_CH_BIT_MASK)));

	reg = 0;

	/* HDLC engine is enabled on the above logical channels */
	bit_clear((u_int8_t *)&reg, HDLC_RX_PROT_DISABLE_BIT);
	bit_clear((u_int8_t *)&reg, HDLC_TX_PROT_DISABLE_BIT);

	bit_set((u_int8_t *)&reg, HDLC_TX_CHAN_ENABLE_BIT);
	bit_set((u_int8_t *)&reg, HDLC_RX_ADDR_RECOGN_DIS_BIT);

	xilinx_write_ctrl_hdlc(card, sc->first_time_slot,
	    XILINX_HDLC_CONTROL_REG, reg);

	return (0);
}

static void
xilinx_dev_unconfigure(sdla_t *card, xilinx_softc_t *sc)
{
	u_int32_t reg;
	int i, s;

#ifdef DEBUG_INIT
	log(LOG_DEBUG, "\n-- Unconfigure Xilinx. --\n");
#endif

	/* Select an HDLC logic channel for configuration */
	if (sc->logic_ch_num != -1) {

		sdla_bus_read_4(card->hw, XILINX_TIMESLOT_HDLC_CHAN_REG, &reg);
		reg &= ~HDLC_LOGIC_CH_BIT_MASK;
		reg &= HDLC_LCH_TIMESLOT_MASK;	/* mask not valid bits */

		sdla_bus_write_4(card->hw, XILINX_TIMESLOT_HDLC_CHAN_REG,
		    (reg | (sc->logic_ch_num & HDLC_LOGIC_CH_BIT_MASK)));

		reg = 0x00020000;
		xilinx_write_ctrl_hdlc(card, sc->first_time_slot,
		    XILINX_HDLC_CONTROL_REG, reg);

		for (i = 0; i < card->u.xilinx.num_of_time_slots; i++) {
			if (bit_test((u_int8_t *)&sc->time_slot_map, i)) {
				sdla_bus_read_4(card->hw,
				    XILINX_TIMESLOT_HDLC_CHAN_REG, &reg);
				reg &= ~TIMESLOT_BIT_MASK;

				/* mask not valid bits */
				reg &= HDLC_LCH_TIMESLOT_MASK;

				/* Select a Timeslot for configuration */
				sdla_bus_write_4(card->hw,
				    XILINX_TIMESLOT_HDLC_CHAN_REG,
				    (reg | (i<<TIMESLOT_BIT_SHIFT)));

				reg = 31 & CONTROL_RAM_DATA_MASK;
				reg |= (FIFO_32B & HDLC_FIFO_SIZE_MASK) <<
				    HDLC_FIFO_SIZE_SHIFT;
				reg |= (31 & HDLC_FIFO_BASE_ADDR_MASK) <<
				    HDLC_FIFO_BASE_ADDR_SHIFT;

#ifdef DEBUG_INIT
				log(LOG_INFO, "Setting Timeslot %d "
				    "to logic ch %d Reg=0x%X\n", i, 31 , reg);
#endif
				xilinx_write_ctrl_hdlc(card, i,
				    XILINX_CONTROL_RAM_ACCESS_BUF, reg);
			}
		}

		/*
		 * Lock to protect the logic ch map to sc device array
		 */
		s = splnet();
		free_xilinx_logical_channel_num(card, sc->logic_ch_num);
		for (i = 0; i < card->u.xilinx.num_of_time_slots; i++)
			if (bit_test((u_int8_t *)&sc->time_slot_map, i))
				--sc->num_of_time_slots;

		free_fifo_baddr_and_size(card, sc);
		splx(s);

		sc->logic_ch_num = -1;

		for (i = 0; i < card->u.xilinx.num_of_time_slots; i++)
			if (bit_test((u_int8_t *)&sc->time_slot_map, i))
				bit_clear((u_int8_t *)
				    &card->u.xilinx.time_slot_map, i);
	}
}

#define FIFO_RESET_TIMEOUT_CNT 1000
#define FIFO_RESET_TIMEOUT_US  10
static int
xilinx_init_rx_dev_fifo(sdla_t *card, xilinx_softc_t *sc, unsigned char wait)
{
	u_int32_t reg;
	u_int32_t dma_descr;
	u_int8_t  timeout = 1;
	u_int16_t i;

	/* Clean RX DMA fifo */
	dma_descr = (unsigned long)(sc->logic_ch_num << 4) +
	    XILINX_RxDMA_DESCRIPTOR_HI;
	reg = 0;
	bit_set((u_int8_t *)&reg, INIT_DMA_FIFO_CMD_BIT);

#ifdef DEBUG_INIT
	log(LOG_DEBUG,
	    "%s: Clearing RX Fifo DmaDescr=(0x%X) Reg=(0x%X) (%s)\n",
	    sc->if_name, dma_descr, reg, __FUNCTION__);
#endif

	sdla_bus_write_4(card->hw, dma_descr, reg);

	if (wait == WP_WAIT) {
		for (i = 0; i < FIFO_RESET_TIMEOUT_CNT; i++) {
			sdla_bus_read_4(card->hw, dma_descr, &reg);
			if (bit_test((u_int8_t *)&reg, INIT_DMA_FIFO_CMD_BIT)) {
				DELAY(FIFO_RESET_TIMEOUT_US);
				continue;
			}
			timeout = 0;
			break;
		}

#ifdef DEBUG_INIT
		if (timeout)
			log(LOG_INFO, "%s:%s: Error: Rx fifo reset "
			    "timedout %u us\n", card->devname,
			    sc->if_name, i * FIFO_RESET_TIMEOUT_US);
		else
			log(LOG_INFO, "%s:%s: Rx Fifo reset "
			    "successful %u us\n", card->devname, sc->if_name,
			    i * FIFO_RESET_TIMEOUT_US);
#endif
	} else
		timeout = 0;

	return (timeout);
}

static int
xilinx_init_tx_dev_fifo(sdla_t *card, xilinx_softc_t *sc, unsigned char wait)
{
	u_int32_t reg;
	u_int32_t dma_descr;
	u_int8_t  timeout = 1;
	u_int16_t i;

	/* Clean TX DMA fifo */
	dma_descr = (unsigned long)(sc->logic_ch_num << 4) +
	    XILINX_TxDMA_DESCRIPTOR_HI;
	reg = 0;
	bit_set((u_int8_t *)&reg, INIT_DMA_FIFO_CMD_BIT);

#ifdef DEBUG_INIT
	log(LOG_DEBUG,
	    "%s: Clearing TX Fifo DmaDescr=(0x%X) Reg=(0x%X) (%s)\n",
	    sc->if_name, dma_descr, reg, __FUNCTION__);
#endif
	sdla_bus_write_4(card->hw, dma_descr, reg);

	if (wait == WP_WAIT) {
		for (i = 0; i < FIFO_RESET_TIMEOUT_CNT; i++) {
			sdla_bus_read_4(card->hw, dma_descr, &reg);
			if (bit_test((u_int8_t *)&reg, INIT_DMA_FIFO_CMD_BIT)) {
				DELAY(FIFO_RESET_TIMEOUT_US);
				continue;
			}
			timeout = 0;
			break;
		}

#ifdef DEBUG_INIT
		if (timeout)
			log(LOG_INFO, "%s:%s: Error: Tx fifo reset "
			    "timedout %u us\n", card->devname, sc->if_name,
			    i * FIFO_RESET_TIMEOUT_US);
		else
			log(LOG_INFO, "%s:%s: Tx Fifo reset "
			    "successful %u us\n", card->devname, sc->if_name,
			    i * FIFO_RESET_TIMEOUT_US);
#endif
	} else
		timeout = 0;

	return (timeout);
}


static void
xilinx_dev_enable(sdla_t *card, xilinx_softc_t *sc)
{
	u_int32_t reg;

#ifdef DEBUG_INIT
	log(LOG_INFO, "%s: Enabling Global Inter Mask !\n", sc->if_name);
#endif
	/* Enable Logic Channel Interrupts for DMA and fifo */
	sdla_bus_read_4(card->hw, XILINX_GLOBAL_INTER_MASK, &reg);
	bit_set((u_int8_t *)&reg, sc->logic_ch_num);

	sdla_bus_write_4(card->hw, XILINX_GLOBAL_INTER_MASK, reg);

	bit_set((u_int8_t *)&card->u.xilinx.active_ch_map, sc->logic_ch_num);
}

static void
xilinx_dev_close(sdla_t *card, xilinx_softc_t *sc)
{
	u_int32_t reg;
	unsigned long dma_descr;
	int s;

#ifdef DEBUG_INIT
	log(LOG_DEBUG, "-- Close Xilinx device. --\n");
#endif
	/* Disable Logic Channel Interrupts for DMA and fifo */
	sdla_bus_read_4(card->hw, XILINX_GLOBAL_INTER_MASK, &reg);

	bit_clear((u_int8_t *)&reg, sc->logic_ch_num);
	bit_clear((u_int8_t *)&card->u.xilinx.active_ch_map, sc->logic_ch_num);

	/*
	 * We are masking the sc interrupt.
	 * Lock to make sure that the interrupt is
	 * not running
	 */
	s = splnet();
	sdla_bus_write_4(card->hw, XILINX_GLOBAL_INTER_MASK, reg);
	splx(s);

	reg = 0;

	/* Select an HDLC logic channel for configuration */
	sdla_bus_read_4(card->hw, XILINX_TIMESLOT_HDLC_CHAN_REG, &reg);

	reg &= ~HDLC_LOGIC_CH_BIT_MASK;
	reg &= HDLC_LCH_TIMESLOT_MASK;         /* mask not valid bits */

	sdla_bus_write_4(card->hw, XILINX_TIMESLOT_HDLC_CHAN_REG,
	    (reg | (sc->logic_ch_num & HDLC_LOGIC_CH_BIT_MASK)));


	reg = 0;
	xilinx_write_ctrl_hdlc(card, sc->first_time_slot,
	    XILINX_HDLC_CONTROL_REG, reg);

	/* Clear descriptors */
	reg = 0;
	dma_descr=(sc->logic_ch_num<<4) + XILINX_RxDMA_DESCRIPTOR_HI;
	sdla_bus_write_4(card->hw, dma_descr, reg);
	dma_descr=(sc->logic_ch_num<<4) + XILINX_TxDMA_DESCRIPTOR_HI;
	sdla_bus_write_4(card->hw, dma_descr, reg);

	/* FIXME: Cleanp up Tx and Rx buffers */
}

static int
xilinx_dma_rx(sdla_t *card, xilinx_softc_t *sc)
{
	u_int32_t reg;
	unsigned long dma_descr;
	unsigned long bus_addr;
	wp_rx_element_t *rx_el;

	/* sanity check: make sure that DMA is in ready state */
#if 0
	dma_descr=(sc->logic_ch_num<<4) + XILINX_RxDMA_DESCRIPTOR_HI;
	sdla_bus_read_4(card->hw, dma_descr, &reg);

	if (bit_test((u_int8_t *)&reg, RxDMA_HI_DMA_GO_READY_BIT)) {
		log(LOG_INFO, "%s: Error: RxDMA GO Ready bit set on dma Rx\n",
				card->devname);
		return (EFAULT);
	}
#endif

	if (sc->rx_dma_buf) {
		log(LOG_INFO, "%s: Critial Error: Rx Dma Buf busy!\n",
		    sc->if_name);
		return (EINVAL);
	}

	sc->rx_dma_buf = SIMPLEQ_FIRST(&sc->wp_rx_free_list);

	if (sc->rx_dma_buf == NULL) {
		if (aft_alloc_rx_dma_buff(sc, 1) == 0) {
			log(LOG_INFO, "%s: Critical Error no rx dma buf!",
			    sc->if_name);
			return (ENOMEM);
		}
		sc->rx_dma_buf = SIMPLEQ_FIRST(&sc->wp_rx_free_list);
	}

	SIMPLEQ_REMOVE_HEAD(&sc->wp_rx_free_list, entry);

	bus_dmamap_sync(sc->dmatag, sc->rx_dma_buf->dma_map, 0, sc->dma_mtu,
             BUS_DMASYNC_PREREAD);

	rx_el = &sc->rx_dma_buf->rx_el;
	memset(rx_el, 0, sizeof(*rx_el));

	bus_addr = sc->rx_dma_buf->dma_map->dm_segs[0].ds_addr;
	rx_el->dma_addr = bus_addr;

	/* Write the pointer of the data packet to the
	 * DMA address register */
	reg = bus_addr;

	/* Set the 32bit alignment of the data length.
	 * Since we are setting up for rx, set this value
	 * to Zero */
	reg &= ~(RxDMA_LO_ALIGNMENT_BIT_MASK);

	dma_descr = (sc->logic_ch_num<<4) + XILINX_RxDMA_DESCRIPTOR_LO;

#ifdef DEBUG_RX
	log(LOG_INFO, "%s: RxDMA_LO = 0x%X, BusAddr=0x%lX "
	    "DmaDescr=0x%lX (%s)\n", card->devname, reg, bus_addr,
	    dma_descr, __FUNCTION__);
#endif
	sdla_bus_write_4(card->hw, dma_descr, reg);

	dma_descr=(unsigned long)(sc->logic_ch_num << 4) +
	    XILINX_RxDMA_DESCRIPTOR_HI;

	reg = 0;

	reg |= (sc->dma_mtu >> 2) & RxDMA_HI_DMA_DATA_LENGTH_MASK;

#ifdef TRUE_FIFO_SIZE
	reg |= (sc->fifo_size_code & DMA_FIFO_SIZE_MASK) <<
	    DMA_FIFO_SIZE_SHIFT;
#else

	reg |= (HARD_FIFO_CODE & DMA_FIFO_SIZE_MASK) << DMA_FIFO_SIZE_SHIFT;
#endif
	reg |= (sc->fifo_base_addr&DMA_FIFO_BASE_ADDR_MASK) <<
	    DMA_FIFO_BASE_ADDR_SHIFT;

	bit_set((u_int8_t *)&reg, RxDMA_HI_DMA_GO_READY_BIT);

#ifdef DEBUG_RX
	log(LOG_INFO, "%s: RXDMA_HI = 0x%X, BusAddr=0x%lX DmaDescr=0x%lX "
	    "(%s)\n", sc->if_name, reg, bus_addr, dma_descr, __FUNCTION__);
#endif

	sdla_bus_write_4(card->hw, dma_descr, reg);

	bit_set((u_int8_t *)&sc->rx_dma, 0);

	return (0);
}


static int
xilinx_dma_tx(sdla_t *card, xilinx_softc_t *sc)
{
	u_int32_t reg = 0;
	struct mbuf *m;
	unsigned long dma_descr;
	unsigned char len_align = 0;
	int len = 0;

#ifdef DEBUG_TX
	log(LOG_INFO, "------ Setup Tx DMA descriptor. --\n");
#endif

	if (bit_test((u_int8_t *)&sc->dma_status, TX_BUSY)) {
#ifdef DEBUG_TX
		log(LOG_INFO, "%s:  TX_BUSY set (%s:%d)!\n",
		    sc->if_name, __FUNCTION__, __LINE__);
#endif
		return EBUSY;
	}
	bit_set((u_int8_t *)&sc->dma_status, TX_BUSY);


	/*
	 * Free the previously skb dma mapping.
	 * In this case the tx interrupt didn't finish and we must re-transmit.
	 */
	if (sc->tx_dma_addr && sc->tx_dma_len) {
		log(LOG_INFO, "%s: Unmaping tx_dma_addr in %s\n",
		    sc->if_name, __FUNCTION__);

		sc->tx_dma_addr = 0;
		sc->tx_dma_len = 0;
	}

	/* Free the previously sent tx packet. To
	 * minimize tx isr, the previously transmitted
	 * packet is deallocated here */
	if (sc->tx_dma_mbuf) {
		bus_dmamap_unload(sc->dmatag, sc->tx_dmamap);
		m_freem(sc->tx_dma_mbuf);
		sc->tx_dma_mbuf = NULL;
	}

	/* check queue pointers before starting transmission */

	/* sanity check: make sure that DMA is in ready state */
	dma_descr = (sc->logic_ch_num << 4) + XILINX_TxDMA_DESCRIPTOR_HI;

#ifdef DEBUG_TX
	log(LOG_INFO, "%s: sc logic ch=%ld dma_descr=0x%lx set (%s:%d)!\n",
	    sc->if_name, sc->logic_ch_num, dma_descr, __FUNCTION__, __LINE__);
#endif

	sdla_bus_read_4(card->hw, dma_descr, &reg);

	if (bit_test((u_int8_t *)&reg, TxDMA_HI_DMA_GO_READY_BIT)) {
		log(LOG_INFO, "%s: Error: TxDMA GO Ready bit set "
		    "on dma Tx 0x%X\n", card->devname, reg);
		bit_clear((u_int8_t *)&sc->dma_status, TX_BUSY);
		return (EFAULT);
	}

	IF_DEQUEUE(&sc->wp_tx_pending_list, m);

	if (!m) {
		bit_clear((u_int8_t *)&sc->dma_status, TX_BUSY);
		return (ENOBUFS);
	}

	len = m->m_len;
	if (len > MAX_XILINX_TX_DMA_SIZE) {
		/* FIXME: We need to split this frame into
		 *        multiple parts.  For now though
		 *        just drop it :) */
		log(LOG_INFO, "%s: Tx len %d > %d (MAX TX DMA LEN)\n",
		    sc->if_name, len, MAX_XILINX_TX_DMA_SIZE);
		m_freem(m);
		bit_clear((u_int8_t *)&sc->dma_status, TX_BUSY);
		return (EINVAL);
	}

	if (ADDR_MASK(mtod(m, caddr_t), 0x03)) {
		/* The mbuf should already be aligned */
		log(LOG_INFO, "%s: TX packet not aligned!\n",
		    sc->if_name, MAX_XILINX_TX_DMA_SIZE);
		m_freem(m);
		bit_clear((u_int8_t *)&sc->dma_status, TX_BUSY);
		return (EINVAL);
	}

	if (bus_dmamap_load(sc->dmatag, sc->tx_dmamap,
	    mtod(m, void *), len, NULL, BUS_DMA_NOWAIT | BUS_DMA_WRITE)) {
		log(LOG_INFO, "%s: Failed to load TX mbuf for DMA!\n",
		    sc->if_name);
		m_freem(m);
		bit_clear((u_int8_t *)&sc->dma_status, TX_BUSY);
		return (EINVAL);		
	}

	sc->tx_dma_addr = sc->tx_dmamap->dm_segs[0].ds_addr;
	sc->tx_dma_len = len;

	if (sc->tx_dma_addr & 0x03) {
		log(LOG_INFO, "%s: Error: Tx Ptr not aligned "
		    "to 32bit boundary!\n", card->devname);
		m_freem(m);
		bit_clear((u_int8_t *)&sc->dma_status, TX_BUSY);
		return (EINVAL);
	}

	sc->tx_dma_mbuf = m;

	/* WARNING: Do not use the "skb" pointer from
	 *          here on.  The skb pointer might not exist if
	 *          we are in transparent mode */

	dma_descr = (sc->logic_ch_num << 4) + XILINX_TxDMA_DESCRIPTOR_LO;

	/* Write the pointer of the data packet to the
	 * DMA address register */
	reg = sc->tx_dma_addr;

	bus_dmamap_sync(sc->dmatag, sc->tx_dmamap, 0, len,
	    BUS_DMASYNC_PREWRITE);

	/* Set the 32bit alignment of the data length.
	 * Used to pad the tx packet to the 32 bit
	 * boundary */
	reg &= ~(TxDMA_LO_ALIGNMENT_BIT_MASK);
	reg |= (len & 0x03);

	if (len & 0x03)
		len_align = 1;

#ifdef DEBUG_TX
	log(LOG_INFO, "%s: TXDMA_LO=0x%X PhyAddr=0x%lX DmaDescr=0x%lX (%s)\n",
		sc->if_name, reg, sc->tx_dma_addr, dma_descr, __FUNCTION__);
#endif

	sdla_bus_write_4(card->hw, dma_descr, reg);

	dma_descr = (sc->logic_ch_num << 4) + XILINX_TxDMA_DESCRIPTOR_HI;

	reg = 0;
	reg |= (((len >> 2) + len_align) & TxDMA_HI_DMA_DATA_LENGTH_MASK);

#ifdef TRUE_FIFO_SIZE
	reg |= (sc->fifo_size_code & DMA_FIFO_SIZE_MASK) <<
	    DMA_FIFO_SIZE_SHIFT;
#else

	reg |= (HARD_FIFO_CODE & DMA_FIFO_SIZE_MASK) << DMA_FIFO_SIZE_SHIFT;
#endif
	reg |= (sc->fifo_base_addr & DMA_FIFO_BASE_ADDR_MASK) <<
	    DMA_FIFO_BASE_ADDR_SHIFT;

	/*
	 * Only enable the Frame Start/Stop on
	 * non-transparent hdlc configuration
	 */
	bit_set((u_int8_t *)&reg, TxDMA_HI_DMA_FRAME_START_BIT);
	bit_set((u_int8_t *)&reg, TxDMA_HI_DMA_FRAME_END_BIT);

	bit_set((u_int8_t *)&reg, TxDMA_HI_DMA_GO_READY_BIT);

#ifdef DEBUG_TX
	log(LOG_INFO, "%s: TXDMA_HI=0x%X DmaDescr=0x%lX (%s)\n",
	    sc->if_name, reg, dma_descr, __FUNCTION__);
#endif

	sdla_bus_write_4(card->hw, dma_descr, reg);

	return (0);

}

static void
xilinx_dma_tx_complete(sdla_t *card, xilinx_softc_t *sc)
{
	u_int32_t reg = 0;
	unsigned long dma_descr;

#ifdef DEBUG_TX
	log(LOG_INFO, "%s: TX DMA complete\n", card->devname);
#endif
	/* DEBUGTX */
/*	sdla_bus_read_4(card->hw, 0x78, &tmp1); */

	dma_descr = (sc->logic_ch_num << 4) + XILINX_TxDMA_DESCRIPTOR_HI;
	sdla_bus_read_4(card->hw, dma_descr, &reg);

	if (sc->tx_dma_mbuf == NULL) {
		log(LOG_INFO,
		    "%s: Critical Error: Tx DMA intr: no tx mbuf !\n",
		    card->devname);
		bit_clear((u_int8_t *)&sc->dma_status, TX_BUSY);
		return;
	}

	bus_dmamap_sync(sc->dmatag, sc->tx_dmamap, 0, sc->tx_dma_len,
             BUS_DMASYNC_POSTWRITE);

	sc->tx_dma_addr = 0;
	sc->tx_dma_len = 0;

	/* Do not free the packet here,
	 * copy the packet dma info into csum
	 * field and let the bh handler analyze
	 * the transmitted packet.
	 */

	if (reg & TxDMA_HI_DMA_PCI_ERROR_RETRY_TOUT) {
		log(LOG_INFO, "%s:%s: PCI Error: 'Retry' "
		    "exceeds maximum (64k): Reg=0x%X!\n",
		    card->devname, sc->if_name, reg);

		if (++sc->pci_retry < 3) {
			bit_set((u_int8_t *)&reg,
				TxDMA_HI_DMA_GO_READY_BIT);

			log(LOG_INFO, "%s: Retry: TXDMA_HI=0x%X "
			    "DmaDescr=0x%lX (%s)\n",
			    sc->if_name, reg, dma_descr, __FUNCTION__);

			sdla_bus_write_4(card->hw, dma_descr, reg);
			return;
		}
	}

	sc->pci_retry = 0;
	sc->tx_dma_mbuf->m_pkthdr.csum_flags = reg;
	IF_ENQUEUE(&sc->wp_tx_complete_list, sc->tx_dma_mbuf);
	sc->tx_dma_mbuf = NULL;

	bit_clear((u_int8_t *)&sc->dma_status, TX_BUSY);

	xilinx_process_packet(sc);
}

static void
xilinx_tx_post_complete(sdla_t *card, xilinx_softc_t *sc, struct mbuf *m)
{
	struct ifnet	*ifp;
	unsigned long	reg = m->m_pkthdr.csum_flags;

	WAN_ASSERT1(sc == NULL);
	ifp = (struct ifnet *)&sc->common.ifp;
	if ((bit_test((u_int8_t *)&reg, TxDMA_HI_DMA_GO_READY_BIT)) ||
	    (reg & TxDMA_HI_DMA_DATA_LENGTH_MASK) ||
	    (reg & TxDMA_HI_DMA_PCI_ERROR_MASK)) {

#ifdef DEBUG_TX
		log(LOG_INFO, "%s:%s: Tx DMA Descriptor=0x%lX\n",
			card->devname, sc->if_name, reg);
#endif

		/* Checking Tx DMA Go bit. Has to be '0' */
		if (bit_test((u_int8_t *)&reg, TxDMA_HI_DMA_GO_READY_BIT))
			log(LOG_INFO, "%s:%s: Error: TxDMA Intr: "
			    "GO bit set on Tx intr\n",
			    card->devname, sc->if_name);

		if (reg & TxDMA_HI_DMA_DATA_LENGTH_MASK)
			log(LOG_INFO, "%s:%s: Error: TxDMA Length "
			"not equal 0 \n", card->devname, sc->if_name);

		/* Checking Tx DMA PCI error status. Has to be '0's */
		if (reg & TxDMA_HI_DMA_PCI_ERROR_MASK) {

			if (reg & TxDMA_HI_DMA_PCI_ERROR_M_ABRT)
				log(LOG_INFO, "%s:%s: Tx Error: "
				    "Abort from Master: pci fatal error!\n",
				    card->devname, sc->if_name);

			if (reg & TxDMA_HI_DMA_PCI_ERROR_T_ABRT)
				log(LOG_INFO, "%s:%s: Tx Error: "
				    "Abort from Target: pci fatal error!\n",
				    card->devname, sc->if_name);

			if (reg & TxDMA_HI_DMA_PCI_ERROR_DS_TOUT) {
				log(LOG_INFO, "%s:%s: Tx Warning: "
				    "PCI Latency Timeout!\n",
				    card->devname, sc->if_name);
				goto tx_post_ok;
			}
			if (reg & TxDMA_HI_DMA_PCI_ERROR_RETRY_TOUT)
				log(LOG_INFO, "%s:%s: Tx Error: 'Retry' "
				    "exceeds maximum (64k): pci fatal error!\n",
				    card->devname, sc->if_name);
		}
		goto tx_post_exit;
	}

tx_post_ok:

	if (ifp) {
		ifp->if_opackets++;
		ifp->if_obytes += m->m_len;
	}

	/* Indicate that the first tx frame went
	 * out on the transparent link */
	bit_set((u_int8_t *)&sc->idle_start, 0);

tx_post_exit:

	if (!xilinx_dma_tx(card, sc)) {
		/*
		 * If we were able to transmit and the interface is set to
		 * OACTIVE remove this flag and let kernel try to transmit.
		 */
		if (ifp->if_flags & IFF_OACTIVE)
			ifp->if_flags &= ~IFF_OACTIVE;
	}
	return;
}

static void
xilinx_dma_rx_complete(sdla_t *card, xilinx_softc_t *sc)
{
	struct xilinx_rx_buffer *buf;
	unsigned long dma_descr;
	wp_rx_element_t *rx_el;

	bit_clear((u_int8_t *)&sc->rx_dma, 0);

	if (sc->rx_dma_buf == NULL) {
		log(LOG_INFO,
		    "%s: Critical Error: rx_dma_mbuf\n", sc->if_name);
		return;
	}

	rx_el = &sc->rx_dma_buf->rx_el;

	/* Reading Rx DMA descriptor information */
	dma_descr=(sc->logic_ch_num << 4) + XILINX_RxDMA_DESCRIPTOR_LO;
	sdla_bus_read_4(card->hw, dma_descr, &rx_el->align);
	rx_el->align &= RxDMA_LO_ALIGNMENT_BIT_MASK;

	dma_descr = (sc->logic_ch_num << 4) + XILINX_RxDMA_DESCRIPTOR_HI;
	sdla_bus_read_4(card->hw, dma_descr, &rx_el->reg);

	rx_el->pkt_error = sc->pkt_error;
	sc->pkt_error = 0;

#ifdef DEBUG_RX
	log(LOG_INFO, "%s: RX HI=0x%X  LO=0x%X DMA=0x%lX (%s:%d)\n",
	    sc->if_name, rx_el->reg, rx_el->align, rx_el->dma_addr,
	    __FUNCTION__, __LINE__);
#endif

	buf = sc->rx_dma_buf;
	sc->rx_dma_buf = NULL;

	xilinx_dma_rx(card, sc);

	SIMPLEQ_INSERT_TAIL(&sc->wp_rx_complete_list, buf, entry);

	xilinx_process_packet(sc);

/*	sdla_bus_read_4(card->hw, 0x80, &rx_empty); */
}


static void
xilinx_rx_post_complete(sdla_t *card, xilinx_softc_t *sc,
    struct xilinx_rx_buffer *buf, struct mbuf **new_m, u_char *pkt_error)
{
	struct ifnet	*ifp;
	unsigned int len, data_error = 0;
	wp_rx_element_t *rx_el = &buf->rx_el;
	struct mbuf *m = buf->mbuf;

	WAN_ASSERT1(sc == NULL);
	ifp = (struct ifnet *)&sc->common.ifp;	/*m->m_pkthdr.rcvif;*/

#ifdef DEBUG_RX
	log(LOG_INFO, "%s: RX HI=0x%X  LO=0x%X DMA=0x%lX (%s:%d)\n",
	    sc->if_name, rx_el->reg, rx_el->align, rx_el->dma_addr,
	    __FUNCTION__, __LINE__);
#endif
	rx_el->align &= RxDMA_LO_ALIGNMENT_BIT_MASK;
	*pkt_error = 0;
	*new_m = NULL;


	/* Checking Rx DMA Go bit. Has to be '0' */
	if (bit_test((u_int8_t *)&rx_el->reg, RxDMA_HI_DMA_GO_READY_BIT)) {
		log(LOG_INFO, "%s: Error: RxDMA Intr: GO bit set on Rx intr\n",
		    card->devname);
		ifp->if_ierrors++;
		goto rx_comp_error;
	}

	/* Checking Rx DMA PCI error status. Has to be '0's */
	if (rx_el->reg & RxDMA_HI_DMA_PCI_ERROR_MASK) {
#ifdef DEBUG_ERR
		if (rx_el->reg & RxDMA_HI_DMA_PCI_ERROR_M_ABRT)
			log(LOG_INFO, "%s: Rx Error: Abort from Master: "
			    "pci fatal error!\n", card->devname);

		if (rx_el->reg & RxDMA_HI_DMA_PCI_ERROR_T_ABRT)
			log(LOG_INFO, "%s: Rx Error: Abort from Target: "
			    "pci fatal error!\n", card->devname);

		if (rx_el->reg & RxDMA_HI_DMA_PCI_ERROR_DS_TOUT)
			log(LOG_INFO, "%s: Rx Error: No 'DeviceSelect' "
			    "from target: pci fatal error!\n", card->devname);

		if (rx_el->reg & RxDMA_HI_DMA_PCI_ERROR_RETRY_TOUT)
			log(LOG_INFO, "%s: Rx Error: 'Retry' exceeds maximum "
			    "(64k): pci fatal error!\n", card->devname);

		log(LOG_INFO, "%s: RXDMA PCI ERROR = 0x%x\n",
		    card->devname, rx_el->reg);
#endif
		if (ifp)
			ifp->if_ierrors++;

		goto rx_comp_error;
	}

	/* Checking Rx DMA Frame start bit. (information for api) */
	if (!bit_test((u_int8_t *)&rx_el->reg, RxDMA_HI_DMA_FRAME_START_BIT)) {
#ifdef DEBUG_ERR
		log(LOG_INFO, "%s: RxDMA Intr: Start flag missing: "
		    "MTU Mismatch! Reg=0x%X\n", card->devname, rx_el->reg);
#endif
		if (ifp)
			ifp->if_ierrors++;
		goto rx_comp_error;
	}

	/* Checking Rx DMA Frame end bit. (information for api) */
	if (!bit_test((u_int8_t *)&rx_el->reg, RxDMA_HI_DMA_FRAME_END_BIT)) {
#ifdef DEBUG_ERR
		log(LOG_INFO, "%s: RxDMA Intr: End flag missing: "
		    "MTU Mismatch! Reg=0x%X\n", card->devname, rx_el->reg);
#endif
		if (ifp)
			ifp->if_ierrors++;
		goto rx_comp_error;

	} else {  /* Check CRC error flag only if this is the end of Frame */

		if (bit_test((u_int8_t *)&rx_el->reg,
		    RxDMA_HI_DMA_CRC_ERROR_BIT)) {
#ifdef DEBUG_ERR
			log(LOG_INFO, "%s: RxDMA Intr: CRC Error! Reg=0x%X\n",
			    card->devname, rx_el->reg);
#endif
			if (ifp)
				ifp->if_ierrors++;

			bit_set((u_int8_t *)&rx_el->pkt_error,
			    WP_CRC_ERROR_BIT);
			data_error = 1;
		}

		/* Check if this frame is an abort, if it is
		 * drop it and continue receiving */
		if (bit_test((u_int8_t *)&rx_el->reg,
		    RxDMA_HI_DMA_FRAME_ABORT_BIT)) {
#ifdef DEBUG_ERR
			log(LOG_INFO, "%s: RxDMA Intr: Abort! Reg=0x%X\n",
			    card->devname, rx_el->reg);
#endif
			if (ifp)
			    ifp->if_ierrors++;

			bit_set((u_int8_t *)&rx_el->pkt_error,
			    WP_ABORT_ERROR_BIT);
			data_error = 1;
		}

		if (data_error)
			goto rx_comp_error;
	}

	len = rx_el->reg & RxDMA_HI_DMA_DATA_LENGTH_MASK;

	/* In HDLC mode, calculate rx length based
	 * on alignment value, received from DMA */
	len = (((sc->dma_mtu >> 2) - len) << 2) -
	    (~(rx_el->align) & RxDMA_LO_ALIGNMENT_BIT_MASK);

	*pkt_error = rx_el->pkt_error;

	/* After a RX FIFO overflow, we must mark max 7
	 * subsequent frames since firmware, cannot
	 * guarantee the contents of the fifo */

	if (bit_test((u_int8_t *)&rx_el->pkt_error, WP_FIFO_ERROR_BIT)) {
		if (++sc->rx_fifo_err_cnt >= WP_MAX_FIFO_FRAMES) {
			sc->rx_fifo_err_cnt = 0;
		}
		bit_set((u_int8_t *)pkt_error, WP_FIFO_ERROR_BIT);
	} else {
		if (sc->rx_fifo_err_cnt) {
			if (++sc->rx_fifo_err_cnt >= WP_MAX_FIFO_FRAMES) {
				sc->rx_fifo_err_cnt = 0;
			}
			bit_set((u_int8_t *)pkt_error, WP_FIFO_ERROR_BIT);
		}
	}

	bus_dmamap_sync(sc->dmatag, sc->rx_dma_buf->dma_map, 0, len,
             BUS_DMASYNC_POSTREAD);

	m->m_len = m->m_pkthdr.len = len;

	if (len > aft_rx_copyback) {
		/* The rx size is big enough, thus
		 * send this buffer up the stack
		 * and allocate another one */
		*new_m = m;
		buf->mbuf = NULL;
	} else {
		struct mbuf *m0;
		/* The rx packet is very
		 * small thus, allocate a new
		 * buffer and pass it up */
		if ((m0 = m_copym2(m, 0, len, M_NOWAIT)) == NULL) {
			log(LOG_INFO, "%s: Failed to allocate mbuf!\n",
			    sc->if_name);
			if (ifp)
			    ifp->if_ierrors++;
		} else
			*new_m = m0;
	}

 rx_comp_error:
	aft_reload_rx_dma_buff(sc, buf);

	return;
}


static char
request_xilinx_logical_channel_num(sdla_t *card, xilinx_softc_t *sc,
    long *free_ch)
{
	char logic_ch = -1, free_logic_ch = -1;
	int i, err;

	*free_ch = -1;

#ifdef DEBUG_INIT
	log(LOG_INFO, "-- Request_Xilinx_logic_channel_num:--\n");
	log(LOG_INFO, "%s: Global Num Timeslots=%d  "
	    "Global Logic ch Map 0x%lX (%s:%d)\n",
	    sc->if_name, card->u.xilinx.num_of_time_slots,
	    card->u.xilinx.logic_ch_map, __FUNCTION__, __LINE__);
#endif

	err = request_fifo_baddr_and_size(card, sc);
	if (err)
		return (-1);

	for (i = 0; i < card->u.xilinx.num_of_time_slots; i++) {
		if (!bit_test((u_int8_t *)&card->u.xilinx.logic_ch_map, i)) {
			bit_set((u_int8_t *)&card->u.xilinx.logic_ch_map, i);
			logic_ch = i;
			break;
		}
	}

	if (logic_ch == -1)
		return (logic_ch);

	for (i = 0; i < card->u.xilinx.num_of_time_slots; i++) {
		if (!bit_test((u_int8_t *)&card->u.xilinx.logic_ch_map, i)) {
			free_logic_ch = HDLC_FREE_LOGIC_CH;
			break;
		}
	}

	if (card->u.xilinx.dev_to_ch_map[(unsigned char)logic_ch]) {
		log(LOG_INFO, "%s: Error, request logical ch=%d map busy\n",
		    card->devname, logic_ch);
		return (-1);
	}

	*free_ch = free_logic_ch;

	card->u.xilinx.dev_to_ch_map[(unsigned char)logic_ch] = (void *)sc;

	if (logic_ch > card->u.xilinx.top_logic_ch) {
		card->u.xilinx.top_logic_ch = logic_ch;
		xilinx_dma_max_logic_ch(card);
	}

	return (logic_ch);
}

static void
free_xilinx_logical_channel_num(sdla_t *card, int logic_ch)
{
	bit_clear((u_int8_t *)&card->u.xilinx.logic_ch_map, logic_ch);
	card->u.xilinx.dev_to_ch_map[logic_ch] = NULL;

	if (logic_ch >= card->u.xilinx.top_logic_ch) {
		int i;

		card->u.xilinx.top_logic_ch = XILINX_DEFLT_ACTIVE_CH;

		for (i = 0; i < card->u.xilinx.num_of_time_slots; i++) {
			if (card->u.xilinx.dev_to_ch_map[logic_ch])
				card->u.xilinx.top_logic_ch = i;
		}

		xilinx_dma_max_logic_ch(card);
	}

}

static void
xilinx_dma_max_logic_ch(sdla_t *card)
{
	u_int32_t reg;

#ifdef DEBUG_INIT
	log(LOG_INFO, "-- Xilinx_dma_max_logic_ch :--\n");
#endif

	sdla_bus_read_4(card->hw, XILINX_DMA_CONTROL_REG, &reg);

	/* Set up the current highest active logic channel */

	reg &= DMA_ACTIVE_CHANNEL_BIT_MASK;
	reg |= (card->u.xilinx.top_logic_ch << DMA_ACTIVE_CHANNEL_BIT_SHIFT);

	sdla_bus_write_4(card->hw, XILINX_DMA_CONTROL_REG, reg);
}

static int
aft_alloc_rx_buffers(xilinx_softc_t *sc)
{
	struct xilinx_rx_buffer *buf;

	SIMPLEQ_INIT(&sc->wp_rx_free_list);
	SIMPLEQ_INIT(&sc->wp_rx_complete_list);

	/* allocate receive buffers in one cluster */
	buf = malloc(sizeof(*buf) * MAX_RX_BUF, M_DEVBUF, M_NOWAIT);
	if (buf == NULL)
		return (1);

	bzero(buf, sizeof(*buf) * MAX_RX_BUF);
	sc->wp_rx_buffers = buf;
	sc->wp_rx_buffer_last = buf;

	return (0);
}

static void
aft_release_rx_buffers(xilinx_softc_t *sc)
{
	struct xilinx_rx_buffer *buf;

	if (sc->wp_rx_buffers == NULL) {
		printf("%s: release_rx_buffers called with no buffers!\n",
		       sc->if_name, MAX_RX_BUF, sizeof(*buf), (void *)buf);
		return;
	}

	while ((buf = SIMPLEQ_FIRST(&sc->wp_rx_free_list)) != NULL) {
		SIMPLEQ_REMOVE_HEAD(&sc->wp_rx_free_list, entry);
		aft_release_rx_dma_buff(sc, buf);
	}

	while ((buf = SIMPLEQ_FIRST(&sc->wp_rx_complete_list)) != NULL) {
		SIMPLEQ_REMOVE_HEAD(&sc->wp_rx_complete_list, entry);
		aft_release_rx_dma_buff(sc, buf);		
	}

	free(sc->wp_rx_buffers, M_DEVBUF);

	sc->wp_rx_buffers = NULL;
	sc->wp_rx_buffer_last = NULL;
}

/* Allocate an mbuf and setup dma_map. */
static int
aft_alloc_rx_dma_buff(xilinx_softc_t *sc, int num)
{
	struct xilinx_rx_buffer *buf, *ebuf;
	int n;

	ebuf = sc->wp_rx_buffers + MAX_RX_BUF;
	buf = sc->wp_rx_buffer_last;

	for (n = 0; n < num; n++) {
		int i;
		for (i = 0; i < MAX_RX_BUF; i++) {
			if (buf->mbuf == NULL)
				break;
			if (++buf == ebuf)
				buf = sc->wp_rx_buffers;
		}

		if (buf->mbuf != NULL)
			break;

		sc->wp_rx_buffer_last = buf;

		buf->mbuf = wan_mbuf_alloc(sc->dma_mtu);
		if (buf->mbuf == NULL)
			break;

		if (bus_dmamap_create(sc->dmatag, sc->dma_mtu, 1, sc->dma_mtu,
		    0, BUS_DMA_NOWAIT | BUS_DMA_ALLOCNOW, &buf->dma_map)) {
			m_freem(buf->mbuf);
			buf->mbuf = NULL;
			break;
		}

		if (bus_dmamap_load(sc->dmatag, buf->dma_map,
		    mtod(buf->mbuf, void *), sc->dma_mtu, NULL,
		    BUS_DMA_NOWAIT | BUS_DMA_READ)) {
			aft_release_rx_dma_buff(sc, buf);
			break;
		}

		SIMPLEQ_INSERT_TAIL(&sc->wp_rx_free_list, buf, entry);
	}

	return (n);
}

static void
aft_reload_rx_dma_buff(xilinx_softc_t *sc, struct xilinx_rx_buffer *buf)
{
	bus_dmamap_unload(sc->dmatag, buf->dma_map);
	if (buf->mbuf == NULL) {
		buf->mbuf = wan_mbuf_alloc(sc->dma_mtu);
		if (buf->mbuf == NULL) {
			bus_dmamap_destroy(sc->dmatag, buf->dma_map);
			return;
		}
	}
	if (bus_dmamap_load(sc->dmatag, buf->dma_map, mtod(buf->mbuf, void *),
	    sc->dma_mtu, NULL, BUS_DMA_NOWAIT | BUS_DMA_READ)) {
		aft_release_rx_dma_buff(sc, buf);
		return;
	}

	SIMPLEQ_INSERT_TAIL(&sc->wp_rx_free_list, buf, entry);
}

static void
aft_release_rx_dma_buff(xilinx_softc_t *sc, struct xilinx_rx_buffer *buf)
{
	if (buf->mbuf == NULL) {
		printf("%s: Error, buffer already free!\n");
		return;
	}

	bus_dmamap_destroy(sc->dmatag, buf->dma_map);
	m_freem(buf->mbuf);
	buf->mbuf = NULL;
}

static void
enable_timer(void *card_id)
{
	sdla_t *card = (sdla_t *)card_id;
	int	s;

	s = splnet();
	sdla_te_polling(card);
	splx(s);

	return;
}

static void
xilinx_process_packet(xilinx_softc_t *sc)
{
	struct ifnet	*ifp;
	struct mbuf	*new_m, *m;
	unsigned char	 pkt_error;

	WAN_ASSERT1(sc == NULL);
	for (;;) {
		struct xilinx_rx_buffer *buf;
		buf = SIMPLEQ_FIRST(&sc->wp_rx_complete_list);
		if (buf == NULL)
			break;

		SIMPLEQ_REMOVE_HEAD(&sc->wp_rx_complete_list, entry);

		new_m = NULL;
		pkt_error = 0;

		xilinx_rx_post_complete(sc->common.card, sc, buf, &new_m,
		    &pkt_error);
		if (new_m) {
			ifp = (struct ifnet *)&sc->common.ifp;
#ifdef DEBUG_RX
			log(LOG_INFO, "%s: Receiving packet %d bytes!\n",
			    ifp->if_xname, new_m->m_len);
#endif
			wanpipe_generic_input(ifp, new_m);
		}
	}

	for (;;) {
		IF_DEQUEUE(&sc->wp_tx_complete_list, m);
		if (m == NULL)
			break;
		xilinx_tx_post_complete(sc->common.card, sc, m);
		m_freem(m);
	}

	return;
}

static int
fifo_error_interrupt(sdla_t *card, unsigned long reg)
{
	u_int32_t rx_status, tx_status;
	u_int32_t err = 0;
	u_int32_t i;
	xilinx_softc_t *sc;

#ifdef DEBUG_ERR
	log(LOG_INFO, "%s: Fifo error interrupt!\n", card->devname);
#endif

	/* Clear HDLC pending registers */
	sdla_bus_read_4(card->hw, XILINX_HDLC_TX_INTR_PENDING_REG, &tx_status);
	sdla_bus_read_4(card->hw, XILINX_HDLC_RX_INTR_PENDING_REG, &rx_status);

	if (card->state != WAN_CONNECTED) {
		log(LOG_INFO, "%s: Warning: Ignoring Error Intr: link disc!\n",
		    card->devname);
		return (0);
	}

	tx_status &= card->u.xilinx.active_ch_map;
	rx_status &= card->u.xilinx.active_ch_map;

	if (tx_status != 0) {
		for (i = 0; i < card->u.xilinx.num_of_time_slots; i++) {
			if (bit_test((u_int8_t *)&tx_status, i) &&
			    bit_test((u_int8_t *)&card->u.xilinx.logic_ch_map,
			    i)) {
				struct ifnet	*ifp;

				sc = (xilinx_softc_t *)
				    card->u.xilinx.dev_to_ch_map[i];
				if (!sc) {
					log(LOG_INFO, "Warning: ignoring tx "
					    "error intr: no dev!\n");
					continue;
				}

				ifp = (struct ifnet *)&sc->common.ifp;
#if 0
				if (!(ifp->if_flags & IFF_UP)) {
					log(LOG_INFO, "%s: Warning: ignoring "
					    "tx error intr: dev down 0x%X "
					    "UP=0x%X!\n", ifp->if_xname,
					    sc->common.state,
					    sc->ignore_modem);
					continue;
				}
#endif

				if (card->state != WAN_CONNECTED) {
					log(LOG_INFO, "%s: Warning: ignoring "
					    "tx error intr: dev disc!\n",
					     ifp->if_xname);
					continue;
				}

#ifdef DEBUG_ERR
				log(LOG_INFO, "%s:%s: Warning TX Fifo Error "
				    "on LogicCh=%ld Slot=%d!\n",
				    card->devname, sc->if_name,
				    sc->logic_ch_num, i);
#endif
				xilinx_tx_fifo_under_recover(card, sc);
				err=EINVAL;
			}
		}
	}


	if (rx_status != 0) {
		for (i = 0; i < card->u.xilinx.num_of_time_slots; i++) {
			if (bit_test((u_int8_t *)&rx_status, i) &&
			    bit_test((u_int8_t *)&card->u.xilinx.logic_ch_map,
			    i)) {
				struct ifnet	*ifp;
				sc = (xilinx_softc_t *)
				    card->u.xilinx.dev_to_ch_map[i];
				if (!sc)
					continue;

				ifp = (struct ifnet *)&sc->common.ifp;
#if 0
				if (!(ifp->if_flags & IFF_UP)) {
					log(LOG_INFO, "%s: Warning: ignoring "
					    "rx error intr: dev down "
					    "0x%X UP=0x%X!\n", ifp->if_xname,
					    sc->common.state,
					    sc->ignore_modem);
					continue;
				}
#endif

				if (card->state != WAN_CONNECTED) {
					log(LOG_INFO, "%s: Warning: ignoring "
					    "rx error intr: dev disc!\n",
					    ifp->if_xname);
					continue;
				}

#ifdef DEBUG_ERR
				log(LOG_INFO, "%s:%s: Warning RX Fifo Error "
				    "on LCh=%ld Slot=%d RxDMA=%d\n",
				    card->devname, sc->if_name,
				     sc->logic_ch_num, i,
				     sc->rx_dma);
#endif

#if 0
{
				unsigned long dma_descr;
				unsigned int reg;
				dma_descr = (sc->logic_ch_num << 4) +
				    XILINX_RxDMA_DESCRIPTOR_HI;
				sdla_bus_read_4(card->hw, dma_descr, &reg);
				log(LOG_INFO, "%s: Hi Descriptor 0x%X\n",
				    sc->if_name, reg);
}
#endif

				bit_set((u_int8_t *)&sc->pkt_error,
				    WP_FIFO_ERROR_BIT);
				err = EINVAL;
			}
		}
	}

	return (err);
}


static void
front_end_interrupt(sdla_t *card, unsigned long reg)
{
	sdla_te_intr(card);
	handle_front_end_state(card);
	return;
}

/*
 * HARDWARE Interrupt Handlers
 */


/*
 * Main interrupt service routine.
 * Determine the interrupt received and handle it.
 */
static void
wp_xilinx_isr(sdla_t* card)
{
	int i;
	u_int32_t reg;
	u_int32_t dma_tx_reg, dma_rx_reg;
	xilinx_softc_t *sc;

	if (bit_test((u_int8_t *)&card->critical, CARD_DOWN)) {
		log(LOG_INFO, "%s: Card down, ignoring interrupt !!!!!!!!\n",
		    card->devname);
		return;
	}

	bit_set((u_int8_t *)&card->in_isr, 0);

/*	write_cpld(card, LED_CONTROL_REG, 0x0F);*/

	/*
	 * Disable all chip Interrupts  (offset 0x040)
	 *  -- "Transmit/Receive DMA Engine"  interrupt disable
	 *  -- "FiFo/Line Abort Error"        interrupt disable
	 */
	sdla_bus_read_4(card->hw, XILINX_CHIP_CFG_REG, &reg);

	if (bit_test((u_int8_t *)&reg, SECURITY_STATUS_FLAG)) {
		log(LOG_INFO, "%s: Critical: Chip Security Compromised!\n",
					card->devname);
		log(LOG_INFO, "%s: Disabling Driver!\n",
					card->devname);

		port_set_state(card, WAN_DISCONNECTED);
		disable_data_error_intr(card, CARD_DOWN);
		goto isr_end;
	}

	/*
	 * Note: If interrupts are received without pending flags, it usually
	 * indicates that the interrupt * is being shared.
	 */
	if (bit_test((u_int8_t *)&reg, FRONT_END_INTR_ENABLE_BIT)) {
		if (bit_test((u_int8_t *)&reg, FRONT_END_INTR_FLAG)) {
			front_end_interrupt(card, reg);
			if (card->u.xilinx.state_change_exit_isr) {
				card->u.xilinx.state_change_exit_isr = 0;
				/*
				 * The state change occured, skip all
				 * other interrupts
				 */
				goto isr_end;
			}
		}
	}

	/*
	 * Test Fifo Error Interrupt
	 * If set shutdown all interfaces and reconfigure
	 */
	if (bit_test((u_int8_t *)&reg, ERROR_INTR_ENABLE_BIT))
		if (bit_test((u_int8_t *)&reg, ERROR_INTR_FLAG))
			fifo_error_interrupt(card, reg);

	/*
	 * Checking for Interrupt source:
	 * 1. Receive DMA Engine
	 * 2. Transmit DMA Engine
	 * 3. Error conditions.
	 */
	if (bit_test((u_int8_t *)&reg, GLOBAL_INTR_ENABLE_BIT) &&
		bit_test((u_int8_t *)&reg, DMA_INTR_FLAG)) {

		/* Receive DMA Engine */
		sdla_bus_read_4(card->hw, XILINX_DMA_RX_INTR_PENDING_REG,
		    &dma_rx_reg);

		dma_rx_reg &= card->u.xilinx.active_ch_map;

		if (dma_rx_reg == 0)
			goto isr_rx;

		for (i = 0; i < card->u.xilinx.num_of_time_slots; i++) {
			if (bit_test((u_int8_t *)&dma_rx_reg, i) &&
				bit_test((u_int8_t *)
				    &card->u.xilinx.logic_ch_map, i)) {
				sc = (xilinx_softc_t *)
				    card->u.xilinx.dev_to_ch_map[i];
				if (!sc) {
					log(LOG_INFO, "%s: Error: No Dev for "
					    "Rx logical ch=%d\n",
					    card->devname, i);
					continue;
				}

				xilinx_dma_rx_complete(card, sc);
			}
		}
isr_rx:

		/* Transmit DMA Engine */
		sdla_bus_read_4(card->hw, XILINX_DMA_TX_INTR_PENDING_REG,
		    &dma_tx_reg);

		dma_tx_reg &= card->u.xilinx.active_ch_map;

		if (dma_tx_reg == 0)
			goto isr_tx;

		for (i = 0; i < card->u.xilinx.num_of_time_slots; i++) {
			if (bit_test((u_int8_t *)&dma_tx_reg, i) &&
				bit_test((u_int8_t *)
				     &card->u.xilinx.logic_ch_map, i)) {
				sc = (xilinx_softc_t *)
				    card->u.xilinx.dev_to_ch_map[i];
				if (!sc) {
					log(LOG_INFO, "%s: Error: No Dev for "
					    "Tx logical ch=%d\n",
					    card->devname, i);
					continue;
				}

				xilinx_dma_tx_complete(card, sc);
			}
		}
	}

isr_tx:

isr_end:

/*	write_cpld(card, LED_CONTROL_REG, 0x0E); */

	bit_clear((u_int8_t *)&card->in_isr, 0);
	return;
}


/*
 * TASK Functions and Triggers
 */

/*
 * port_set_state
 *
 * Set PORT state.
 *
 */
static void
port_set_state(sdla_t *card, int state)
{
	wanpipe_common_t	*common;

	if (card->state != state) {
		switch (state) {
		case WAN_CONNECTED:
			log(LOG_INFO, "%s: Link connected!\n", card->devname);
			aft_red_led_ctrl(card, AFT_LED_OFF);
			aft_green_led_ctrl(card, AFT_LED_ON);
			break;

		case WAN_CONNECTING:
			log(LOG_INFO, "%s: Link connecting...\n",
			    card->devname);
			aft_red_led_ctrl(card, AFT_LED_ON);
			aft_green_led_ctrl(card, AFT_LED_OFF);
			break;

		case WAN_DISCONNECTED:
			log(LOG_INFO, "%s: Link disconnected!\n",
			    card->devname);
			aft_red_led_ctrl(card, AFT_LED_ON);
			aft_green_led_ctrl(card, AFT_LED_OFF);
			break;
		}
		card->state = state;
		LIST_FOREACH(common, &card->dev_head, next) {
			struct ifnet *ifp = (struct ifnet *)&common->ifp;
			if (ifp)
				set_chan_state(card, ifp, state);
		}
	}
}


/*
 * handle_front_end_state
 */

static void
handle_front_end_state(void *card_id)
{
	sdla_t *card = (sdla_t *)card_id;

	if (card->front_end_status == FE_CONNECTED) {
		enable_data_error_intr(card);
		port_set_state(card, WAN_CONNECTED);
		card->u.xilinx.state_change_exit_isr = 1;
	} else {
		port_set_state(card, WAN_CONNECTING);
		disable_data_error_intr(card, LINK_DOWN);
		card->u.xilinx.state_change_exit_isr = 1;
	}
}

static unsigned char
read_cpld(sdla_t *card, unsigned short cpld_off)
{
	u_int16_t     org_off;
	u_int8_t      tmp;

	cpld_off &= ~BIT_DEV_ADDR_CLEAR;
	cpld_off |= BIT_DEV_ADDR_CPLD;

	/* Save the current address. */
	sdla_bus_read_2(card->hw, XILINX_MCPU_INTERFACE_ADDR, &org_off);

	sdla_bus_write_2(card->hw, XILINX_MCPU_INTERFACE_ADDR, cpld_off);

	sdla_bus_read_1(card->hw, XILINX_MCPU_INTERFACE, &tmp);

	/* Restore original address */
	sdla_bus_write_2(card->hw, XILINX_MCPU_INTERFACE_ADDR, org_off);

	return (tmp);
}

static unsigned char
write_cpld(sdla_t *card, unsigned short off, unsigned char data)
{
	u_int16_t org_off;

	off &= ~BIT_DEV_ADDR_CLEAR;
	off |= BIT_DEV_ADDR_CPLD;

	/* Save the current original address */
	sdla_bus_read_2(card->hw, XILINX_MCPU_INTERFACE_ADDR, &org_off);

	sdla_bus_write_2(card->hw, XILINX_MCPU_INTERFACE_ADDR, off);

	/* This delay is required to avoid bridge optimization
	 * (combining two writes together)*/
	DELAY(5);

	sdla_bus_write_1(card->hw, XILINX_MCPU_INTERFACE, data);

	/* This delay is required to avoid bridge optimization
	 * (combining two writes together)*/
	DELAY(5);

	/* Restore the original address */
	sdla_bus_write_2(card->hw, XILINX_MCPU_INTERFACE_ADDR, org_off);

	return (0);
}

static unsigned char
write_front_end_reg(void *card1, unsigned short off, unsigned char value)
{
	sdla_t *card = (sdla_t *)card1;

	off &= ~BIT_DEV_ADDR_CLEAR;
	sdla_bus_write_2(card->hw, XILINX_MCPU_INTERFACE_ADDR, off);
	/*
	 * These delays are required to avoid bridge optimization
	 * (combining two writes together)
	 */
	DELAY(5);
	sdla_bus_write_1(card->hw, XILINX_MCPU_INTERFACE, value);
	DELAY(5);

	return (0);
}


/*
 * Read TE1/56K Front end registers
 */
static unsigned char
read_front_end_reg(void *card1, unsigned short off)
{
	sdla_t* card = (sdla_t *)card1;
	u_int8_t	tmp;

	off &= ~BIT_DEV_ADDR_CLEAR;
	sdla_bus_write_2(card->hw, XILINX_MCPU_INTERFACE_ADDR, off);
	sdla_bus_read_1(card->hw, XILINX_MCPU_INTERFACE, &tmp);
	DELAY(5);

	return (tmp);
}


/*
 *    Run only after the front end comes up from down state.
 *
 *    Clean the DMA Tx/Rx pending interrupts.
 *       (Ignore since we will reconfigure
 *        all dma descriptors. DMA controler
 *        was already disabled on link down)
 *
 *    For all channels clean Tx/Rx Fifo
 *
 *    Enable DMA controler
 *        (This starts the fifo cleaning
 *         process)
 *
 *    For all channels reprogram Tx/Rx DMA
 *    descriptors.
 *
 *    Clean the Tx/Rx Error pending interrupts.
 *        (Since dma fifo's are now empty)
 *
 *    Enable global DMA and Error interrutps.
 *
 */

static void
enable_data_error_intr(sdla_t *card)
{
	wanpipe_common_t	*common;
	struct ifnet		*ifp;
	u_int32_t		reg;

	/* Clean Tx/Rx DMA interrupts */
	sdla_bus_read_4(card->hw, XILINX_DMA_RX_INTR_PENDING_REG, &reg);
	sdla_bus_read_4(card->hw, XILINX_DMA_TX_INTR_PENDING_REG, &reg);

	/* For all channels clean Tx/Rx fifos */
	LIST_FOREACH(common, &card->dev_head, next) {
		xilinx_softc_t *sc;

		ifp = (struct ifnet *)&common->ifp;
		if (!ifp || !ifp->if_softc)
			continue;
		sc = ifp->if_softc;
#if 0
		if (!(ifp->if_flags & IFF_UP))
			continue;
#endif

#ifdef DEBUG_INIT
		log(LOG_INFO, "%s: Init interface fifo no wait %s\n",
			sc->if_name, __FUNCTION__);
#endif
		xilinx_init_rx_dev_fifo(card, sc, WP_NO_WAIT);
		xilinx_init_tx_dev_fifo(card, sc, WP_NO_WAIT);
	}

	/*
	 * Enable DMA controler, in order to start the
	 * fifo cleaning
	 */
	sdla_bus_read_4(card->hw, XILINX_DMA_CONTROL_REG, &reg);
	bit_set((u_int8_t *)&reg, DMA_ENGINE_ENABLE_BIT);
	sdla_bus_write_4(card->hw, XILINX_DMA_CONTROL_REG, reg);

	/* For all channels clean Tx/Rx fifos */
	LIST_FOREACH(common, &card->dev_head, next) {
		xilinx_softc_t *sc;

		ifp = (struct ifnet *)&common->ifp;
		if (!ifp || ifp->if_softc == NULL)
			continue;
		sc = ifp->if_softc;
#if 0
		if (!(ifp->if_flags & IFF_UP))
			continue;
#endif


#ifdef DEBUG_INIT
		log(LOG_INFO, "%s: Init interface fifo %s\n",
		    sc->if_name, __FUNCTION__);
#endif

		xilinx_init_rx_dev_fifo(card, sc, WP_WAIT);
		xilinx_init_tx_dev_fifo(card, sc, WP_WAIT);

#ifdef DEBUG_INIT
		log(LOG_INFO, "%s: Clearing Fifo and idle_flag %s\n",
		    card->devname, sc->if_name);
#endif
		bit_clear((u_int8_t *)&sc->idle_start, 0);
	}

	/* For all channels, reprogram Tx/Rx DMA descriptors.
	 * For Tx also make sure that the BUSY flag is clear
	 * and previoulsy Tx packet is deallocated */
	LIST_FOREACH(common, &card->dev_head, next) {
		xilinx_softc_t *sc;

		ifp = (struct ifnet *)&common->ifp;
		if (!ifp || !ifp->if_softc)
			continue;
		sc = ifp->if_softc;
#if 0
		if (!(ifp->if_flags & IFF_UP)) {
			continue;
		}
#endif

#ifdef DEBUG_INIT
		log(LOG_INFO, "%s: Init interface %s\n",
		    sc->if_name, __FUNCTION__);
#endif

		if (sc->rx_dma_buf) {
			aft_reload_rx_dma_buff(sc, sc->rx_dma_buf);
			sc->rx_dma_buf = NULL;
		}

		xilinx_dma_rx(card, sc);

		if (sc->tx_dma_addr && sc->tx_dma_len) {
			sc->tx_dma_addr = 0;
			sc->tx_dma_len = 0;
		}

		if (sc->tx_dma_mbuf) {
			bus_dmamap_unload(sc->dmatag, sc->tx_dmamap);
			m_freem(sc->tx_dma_mbuf);
			sc->tx_dma_mbuf = NULL;
		}

		bit_clear((u_int8_t *)&sc->dma_status, TX_BUSY);
		bit_clear((u_int8_t *)&sc->idle_start, 0);

#ifdef DEBUG_INIT
		log(LOG_INFO, "%s: Clearing Fifo and idle_flag %s\n",
		    card->devname, sc->if_name);
#endif
	}

	/*
	 * Clean Tx/Rx Error interrupts, since fifos are now
	 * empty, and Tx fifo may generate an underrun which
	 * we want to ignore :)
	 */
	sdla_bus_read_4(card->hw, XILINX_HDLC_RX_INTR_PENDING_REG, &reg);
	sdla_bus_read_4(card->hw, XILINX_HDLC_TX_INTR_PENDING_REG, &reg);

	/* Enable Global DMA and Error Interrupts */
	reg = 0;
	sdla_bus_read_4(card->hw, XILINX_CHIP_CFG_REG, &reg);
	bit_set((u_int8_t *)&reg, GLOBAL_INTR_ENABLE_BIT);
	bit_set((u_int8_t *)&reg, ERROR_INTR_ENABLE_BIT);
	sdla_bus_write_4(card->hw, XILINX_CHIP_CFG_REG, reg);

	return;
}

static void
disable_data_error_intr(sdla_t *card, unsigned char event)
{
	u_int32_t reg;

	sdla_bus_read_4(card->hw, XILINX_CHIP_CFG_REG, &reg);
	bit_clear((u_int8_t *)&reg, GLOBAL_INTR_ENABLE_BIT);
	bit_clear((u_int8_t *)&reg, ERROR_INTR_ENABLE_BIT);
	if (event == DEVICE_DOWN)
		bit_clear((u_int8_t *)&reg, FRONT_END_INTR_ENABLE_BIT);

	sdla_bus_write_4(card->hw, XILINX_CHIP_CFG_REG, reg);

	sdla_bus_read_4(card->hw, XILINX_DMA_CONTROL_REG, &reg);
	bit_clear((u_int8_t *)&reg, DMA_ENGINE_ENABLE_BIT);
	sdla_bus_write_4(card->hw, XILINX_DMA_CONTROL_REG, reg);

}

static void
xilinx_init_tx_dma_descr(sdla_t *card, xilinx_softc_t *sc)
{
	unsigned long dma_descr;
	unsigned long reg = 0;

	dma_descr = (sc->logic_ch_num << 4) + XILINX_TxDMA_DESCRIPTOR_HI;
	sdla_bus_write_4(card->hw, dma_descr, reg);
}



static void
xilinx_tx_fifo_under_recover(sdla_t *card, xilinx_softc_t *sc)
{
	struct ifnet *ifp = (struct ifnet *)&sc->common.ifp;
	u_int32_t reg = 0;
	unsigned long dma_descr;

#ifdef DEBUG_ERR
	log(LOG_INFO, "%s:%s: Tx Fifo Recovery \n",
	    card->devname, sc->if_name);
#endif

	/* Initialize Tx DMA descriptor: Stop DMA */
	dma_descr = (sc->logic_ch_num << 4) + XILINX_TxDMA_DESCRIPTOR_HI;
	sdla_bus_write_4(card->hw, dma_descr, reg);

	/* Clean the TX FIFO */
	xilinx_init_tx_dev_fifo(card, sc, WP_WAIT);
	if (sc->tx_dma_addr && sc->tx_dma_len) {
		sc->tx_dma_addr = 0;
		sc->tx_dma_len = 0;
	}

	/* Requeue the current tx packet, for re-transmission */
	if (sc->tx_dma_mbuf) {
		IF_PREPEND(&sc->wp_tx_pending_list,
		    (struct mbuf *)sc->tx_dma_mbuf);
		sc->tx_dma_mbuf = NULL;
	}

	/*
	 * Wake up the stack, because tx dma interrupt failed
	 */
	if (ifp)
		ifp->if_oerrors++;

#ifdef DEBUG_ERR
	log(LOG_INFO, "%s:%s: Tx Fifo Recovery: Restarting Transmission \n",
	    card->devname, sc->if_name);
#endif

	/* Re-start transmission */
	bit_clear((u_int8_t *)&sc->dma_status, TX_BUSY);
	if (!xilinx_dma_tx(card, sc)) {
		/* If we was able to transmit and the interface is set
		 * to OACTIVE remove this flag and let kernel try to
		 * transmit.
		 */
		if (ifp->if_flags & IFF_OACTIVE)
			ifp->if_flags &= ~IFF_OACTIVE;
	}
	return;
}

static int
xilinx_write_ctrl_hdlc(sdla_t *card, u_int32_t timeslot,
    u_int8_t reg_off, u_int32_t data)
{
	u_int32_t reg;
	u_int32_t ts_orig = timeslot;
	unsigned long timeout = ticks;

	if (timeslot == 0)
		timeslot = card->u.xilinx.num_of_time_slots - 2;
	else if (timeslot == 1)
		timeslot = card->u.xilinx.num_of_time_slots - 1;
	else
		timeslot -= 2;

	timeslot = timeslot << XILINX_CURRENT_TIMESLOT_SHIFT;
	timeslot &= XILINX_CURRENT_TIMESLOT_MASK;

	for (;;) {
		sdla_bus_read_4(card->hw, XILINX_TIMESLOT_HDLC_CHAN_REG, &reg);
		reg &= XILINX_CURRENT_TIMESLOT_MASK;

		if (reg == timeslot) {
			sdla_bus_write_4(card->hw, reg_off, data);
			return (0);
		}

		if ((ticks-timeout) > 1) {
			log(LOG_INFO, "%s: Error: Access to timeslot %d "
			    "timed out!\n", card->devname, ts_orig);
			return (EIO);
		}
	}

	return (EIO);
}

static int
set_chan_state(sdla_t *card, struct ifnet *ifp, int state)
{
	xilinx_softc_t *sc = ifp->if_softc;

	if (sc == NULL)
		return (0);

	if (state == WAN_CONNECTED) {
#ifdef DEBUG_INIT
		log(LOG_INFO, "%s: Setting idle_start to 0\n", sc->if_name);
#endif
		bit_clear((u_int8_t *)&sc->idle_start, 0);
		sc->common.ifp.pp_up(&sc->common.ifp);
	} else if (state == WAN_DISCONNECTED)
		sc->common.ifp.pp_down(&sc->common.ifp);

	return (0);
}


static char fifo_size_vector[] = {1, 2, 4, 8, 16, 32};
static char fifo_code_vector[] = {0, 1, 3, 7, 0xF, 0x1F};

static int
request_fifo_baddr_and_size(sdla_t *card, xilinx_softc_t *sc)
{
	unsigned char req_fifo_size, fifo_size;
	int i;

	/*
	 * Calculate the optimal fifo size based
	 * on the number of time slots requested
	 */

	if (IS_T1(&card->fe_te.te_cfg)) {
		if (sc->num_of_time_slots == NUM_OF_T1_CHANNELS)
			req_fifo_size = 32;
		else if (sc->num_of_time_slots == 1)
			req_fifo_size = 1;
		else if (sc->num_of_time_slots == 2 ||
		    sc->num_of_time_slots == 3)
			req_fifo_size = 2;
		else if (sc->num_of_time_slots >= 4 &&
		    sc->num_of_time_slots <= 7)
			req_fifo_size = 4;
		else if (sc->num_of_time_slots >= 8 &&
		    sc->num_of_time_slots <= 15)
			req_fifo_size = 8;
		else if (sc->num_of_time_slots >= 16 &&
		    sc->num_of_time_slots <= 23)
			req_fifo_size = 16;
		else {
			log(LOG_INFO, "%s: Invalid number of timeslots %d\n",
			    card->devname, sc->num_of_time_slots);
			return (EINVAL);
		}
	} else {
		if (sc->num_of_time_slots == (NUM_OF_E1_CHANNELS-1))
			req_fifo_size = 32;
		else if (sc->num_of_time_slots == 1)
			req_fifo_size = 1;
		else if (sc->num_of_time_slots == 2 ||
		    sc->num_of_time_slots == 3)
			req_fifo_size = 2;
		else if (sc->num_of_time_slots >= 4 &&
		    sc->num_of_time_slots <= 7)
			req_fifo_size = 4;
		else if (sc->num_of_time_slots >= 8 &&
		    sc->num_of_time_slots <= 15)
			req_fifo_size = 8;
		else if (sc->num_of_time_slots >= 16 &&
		    sc->num_of_time_slots <= 31)
			req_fifo_size = 16;
		else {
			log(LOG_INFO,
			    "%s:%s: Invalid number of timeslots %d\n",
			    card->devname, sc->if_name, sc->num_of_time_slots);
			return (EINVAL);
		}
	}

#ifdef DEBUG_INIT
	log(LOG_INFO, "%s:%s: Optimal Fifo Size =%d  Timeslots=%d \n",
	    card->devname, sc->if_name, req_fifo_size, sc->num_of_time_slots);
#endif
	fifo_size = map_fifo_baddr_and_size(card, req_fifo_size,
	    &sc->fifo_base_addr);

	if (fifo_size == 0 || sc->fifo_base_addr == 31) {
		log(LOG_INFO, "%s:%s: Error: Failed to obtain fifo size %d "
		    "or addr %d\n", card->devname, sc->if_name, fifo_size,
		    sc->fifo_base_addr);
		return (EINVAL);
	}

#ifdef DEBUG_INIT
	log(LOG_INFO, "%s:%s: Optimal Fifo Size =%d TS=%d New Fifo Size=%d\n",
	    card->devname, sc->if_name, req_fifo_size, sc->num_of_time_slots,
	    fifo_size);
#endif

	for (i = 0; i < sizeof(fifo_size_vector); i++) {
		if (fifo_size_vector[i] == fifo_size) {
			sc->fifo_size_code = fifo_code_vector[i];
			break;
		}
	}

	if (fifo_size != req_fifo_size)
		log(LOG_INFO, "%s:%s: WARN: Failed to obtain the req "
		    "fifo %d got %d\n", card->devname, sc->if_name,
		    req_fifo_size, fifo_size);

#ifdef DEBUG_INIT
	log(LOG_INFO, "%s: %s:Fifo Size=%d  TS=%d Fifo Code=%d Addr=%d\n",
	    card->devname, sc->if_name, fifo_size, sc->num_of_time_slots,
	    sc->fifo_size_code, sc->fifo_base_addr);
#endif
	sc->fifo_size = fifo_size;

	return (0);
}


static int
map_fifo_baddr_and_size(sdla_t *card, unsigned char fifo_size,
    unsigned char *addr)
{
	u_int32_t reg = 0;
	int i;

	for (i = 0; i < fifo_size; i++)
		bit_set((u_int8_t *)&reg, i);

#ifdef DEBUG_INIT
	log(LOG_INFO, "%s: Trying to MAP 0x%X  to 0x%lX\n",
	     card->devname, reg, card->u.xilinx.fifo_addr_map);
#endif
	for (i = 0; i < 32; i += fifo_size) {
		if (card->u.xilinx.fifo_addr_map & (reg << i))
			continue;
		card->u.xilinx.fifo_addr_map |= reg << i;
		*addr = i;

#ifdef DEBUG_INIT
		log(LOG_INFO, "%s: Card fifo Map 0x%lX Addr =%d\n",
		    card->devname, card->u.xilinx.fifo_addr_map, i);
#endif
		return (fifo_size);
	}

	if (fifo_size == 1)
		return (0);

	fifo_size = fifo_size >> 1;

	return map_fifo_baddr_and_size(card, fifo_size, addr);
}


static int
free_fifo_baddr_and_size(sdla_t *card, xilinx_softc_t *sc)
{
	u_int32_t reg = 0;
	int i;

	for (i = 0; i < sc->fifo_size; i++)
		bit_set((u_int8_t *)&reg, i);

#ifdef DEBUG_INIT
	log(LOG_INFO, "%s: Unmapping 0x%X from 0x%lX\n", card->devname,
	    reg << sc->fifo_base_addr, card->u.xilinx.fifo_addr_map);
#endif
	card->u.xilinx.fifo_addr_map &= ~(reg << sc->fifo_base_addr);

#ifdef DEBUG_INIT
	log(LOG_INFO, "%s: New Map is 0x%lX\n",
		card->devname, card->u.xilinx.fifo_addr_map);
#endif

	sc->fifo_size = 0;
	sc->fifo_base_addr = 0;

	return (0);
}

static void
aft_red_led_ctrl(sdla_t *card, int mode)
{
	unsigned int led;

	sdla_bus_read_4(card->hw, XILINX_CHIP_CFG_REG, &led);

	if (mode == AFT_LED_ON)
		bit_clear((u_int8_t *)&led, XILINX_RED_LED);
	else if (mode == AFT_LED_OFF)
		bit_set((u_int8_t *)&led, XILINX_RED_LED);
	else {
		if (bit_test((u_int8_t *)&led, XILINX_RED_LED))
			bit_clear((u_int8_t *)&led, XILINX_RED_LED);
		else
			bit_set((u_int8_t *)&led, XILINX_RED_LED);
	}

	sdla_bus_write_4(card->hw, XILINX_CHIP_CFG_REG, led);
}

static void
aft_led_timer(void *data)
{
	sdla_t *card=(sdla_t *)data;
	unsigned int te_alarm;

	if (bit_test((u_int8_t *)&card->critical, CARD_DOWN))
		return;

	if (IS_TE1(&card->fe_te.te_cfg)) {
		int s = splnet();

		te_alarm = sdla_te_alarm(card, 0);
		te_alarm &= ~(BIT_OOSMF_ALARM|BIT_OOCMF_ALARM);

		if (!te_alarm) {
			if (card->state == WAN_CONNECTED) {
				aft_red_led_ctrl(card, AFT_LED_OFF);
				aft_green_led_ctrl(card, AFT_LED_ON);
			} else {
				aft_red_led_ctrl(card, AFT_LED_OFF);
				aft_green_led_ctrl(card, AFT_LED_TOGGLE);
			}

		} else if (te_alarm & (BIT_RED_ALARM|BIT_LOS_ALARM)) {
			/* Red or LOS Alarm solid RED */
			aft_red_led_ctrl(card, AFT_LED_ON);
			aft_green_led_ctrl(card, AFT_LED_OFF);
		} else if (te_alarm & BIT_OOF_ALARM) {
			/* OOF Alarm flashing RED */
			aft_red_led_ctrl(card, AFT_LED_TOGGLE);
			aft_green_led_ctrl(card, AFT_LED_OFF);
		} else if (te_alarm & BIT_AIS_ALARM) {
			/* AIS - Blue Alarm flasing RED and GREEN */
			aft_red_led_ctrl(card, AFT_LED_TOGGLE);
			aft_green_led_ctrl(card, AFT_LED_TOGGLE);
		} else if (te_alarm & BIT_YEL_ALARM) {
			/* Yellow Alarm */
			aft_red_led_ctrl(card, AFT_LED_ON);
			aft_green_led_ctrl(card, AFT_LED_ON);
		} else {

			/* Default case shouldn't happen */
			log(LOG_INFO, "%s: Unknown Alarm 0x%X\n",
			    card->devname, te_alarm);
			aft_red_led_ctrl(card, AFT_LED_ON);
			aft_green_led_ctrl(card, AFT_LED_ON);
		}

		splx(s);
		timeout_add(&card->u.xilinx.led_timer, hz);
	}
}


int
aft_core_ready(sdla_t *card)
{
	u_int32_t reg;
	volatile unsigned char cnt = 0;

	for (;;) {
		sdla_bus_read_4(card->hw, XILINX_CHIP_CFG_REG, &reg);
		if (!bit_test((u_int8_t *)&reg, HDLC_CORE_READY_FLAG_BIT)) {
			/* The HDLC Core is not ready! we have
			** an error. */
			if (++cnt > 5)
				return  (EINVAL);
			else
				DELAY(500);
				/* WARNING: we cannot do this while in
				 * critical area */
		} else
			return (0);
	}

	return (EINVAL);
}
