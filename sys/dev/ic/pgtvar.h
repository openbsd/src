/*-
 * Copyright (c) 2004 Fujitsu Laboratories of America, Inc.
 * Copyright (c) 2004 Brian Fundakowski Feldman
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
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

/*
 * The struct pgt_desc is used to either enqueue or dequeue pgt_frags
 * (packets) when either free or in flight.
 */
struct pgt_desc {
	TAILQ_ENTRY(pgt_desc)	pd_link;
	void		       *pd_mem;
	bus_addr_t		pd_dmaaddr;
	bus_dmamap_t		pd_dmam;
	bus_dma_segment_t	pd_dmas;
	struct pgt_frag	       *pd_fragp;
	unsigned int		pd_fragnum;
};
TAILQ_HEAD(pgt_descq, pgt_desc);

/*
 * The struct pgt_mgmt_desc is used to enqueue a management request
 * and await response.
 */
struct pgt_mgmt_desc {
	TAILQ_ENTRY(pgt_mgmt_desc)	pmd_link;
	const void		       *pmd_sendbuf;	/* NULL = get op */
	void			       *pmd_recvbuf;	/* NULL = set op */
	size_t				pmd_len;
	uint32_t			pmd_oid;
	int				pmd_error;
};
TAILQ_HEAD(pgt_mgmt_descq, pgt_mgmt_desc);

/*
 * These events are put on the per-device kthread to be
 * able to trigger actions from inside the interrupt; as most
 * operations require waiting for another interrupt for response
 * (that is, management packets), this is common.
 */
struct pgt_async_trap {
	TAILQ_ENTRY(pgt_async_trap)	pa_link;
	struct mbuf		       *pa_mbuf;
	/* followed by the rest of the mbuf data */
};

struct pgt_ieee80211_node {
	struct ieee80211_node	pin_node;
	enum pin_dot1x_authorization {
		PIN_DOT1X_UNAUTHORIZED,
		PIN_DOT1X_AUTHORIZED
	}			pin_dot1x_auth_desired, pin_dot1x_auth;
	uint16_t		pin_mlme_state;
};

struct pgt_ieee80211_radiotap {
	struct ieee80211_radiotap_header	pir_header;
	uint8_t					pir_flags;
	uint8_t					pir_rate;
	uint16_t				pir_channel;
	uint16_t				pir_channel_flags;
	uint8_t					pir_db_antsignal;
	uint8_t					pir_db_antnoise;
};
#define	PFF_IEEE80211_RADIOTAP_PRESENT			\
	(1 << IEEE80211_RADIOTAP_FLAGS |		\
	    1 << IEEE80211_RADIOTAP_RATE |		\
	    1 << IEEE80211_RADIOTAP_CHANNEL |		\
	    1 << IEEE80211_RADIOTAP_DB_ANTSIGNAL |	\
	    1 << IEEE80211_RADIOTAP_DB_ANTNOISE)

struct bpf_if;

struct pgt_softc {
	struct device		sc_dev;
	struct ieee80211com	sc_ic;
	struct bpf_if          *sc_drvbpf;
//	struct mtx		sc_lock;
	unsigned int		sc_flags;
#define	SC_NEEDS_FIRMWARE	0x00000001 /* do firmware upload on reset */
#define	SC_UNINITIALIZED	0x00000002 /* still awaiting initial intr */
#define	SC_DYING		0x00000004 /* going away */
#define	SC_NEEDS_RESET		0x00000008 /* going to reset when refcnt = 1 */
#define	SC_INTR_RESET		0x00000020 /* interrupt resets at end */
#define	SC_POWERSAVE		0x00000040 /* device is asleep */
#define	SC_GONE			0x00000080 /* device did not come back */
#define	SC_NOFREE_ALLNODES	0x00000100 /* do not free assoc w/reinit */
#define	SC_START_DESIRED	0x00000200 /* tried to start during mgmt-crit */
#define	SC_KTHREAD		0x00000400 /* has a kthread around */
#define	SC_ISL3877		0x00000800 /* chipset */
	/* configuration sysctls */
	int			sc_dot1x;
	int			sc_wds;
	/* cached values */
	int			sc_if_flags;
	int16_t			sc_80211_ioc_wep;
	int16_t			sc_80211_ioc_auth;
	uint32_t		sc_noise;
	unsigned int		sc_refcnt;	/* # sleeping with sc */
//	struct cv		sc_critical_cv;
	struct thread	       *sc_critical_thread;  /* allow mgmt recursion */
	int			sc_critical;	/* -1- = mgmt < 0 < 1+ data */
	struct thread	       *sc_drainer;	/* who's doing removal/reset? */
	unsigned int		sc_debug;
#define	SC_DEBUG_QUEUES		0x00000001
#define	SC_DEBUG_MGMT		0x00000002
#define	SC_DEBUG_UNEXPECTED	0x00000004
#define	SC_DEBUG_TRIGGER	0x00000008
#define	SC_DEBUG_EVENTS		0x00000010
#define	SC_DEBUG_POWER		0x00000020
#define	SC_DEBUG_TRAP		0x00000040
#define	SC_DEBUG_LINK		0x00000080
#define	SC_DEBUG_RXANNEX	0x00000100
#define	SC_DEBUG_RXFRAG		0x00000200
#define	SC_DEBUG_RXETHER	0x00000400
	struct resource	       *sc_intres;	/* interrupt resource */
	void		       *sc_intcookie;
	struct resource	       *sc_iores;	/* IO memory resource */
	bus_space_tag_t		sc_iotag;
	bus_space_handle_t	sc_iohandle; 
	bus_dma_tag_t		sc_dmat;
	//bus_dma_tag_t		sc_cbdmat;	/* control block DMA */
	bus_dmamap_t		sc_cbdmam;
	bus_dma_segment_t	sc_cbdmas;
	bus_addr_t		sc_cbdmabusaddr;
	struct pgt_control_block *sc_cb;	/* DMA-mapped control block */
	//bus_dma_tag_t		sc_psmdmat;	/* power save buffer DMA */
	bus_dmamap_t		sc_psmdmam;
	bus_dma_segment_t	sc_psmdmas;
	bus_addr_t		sc_psmdmabusaddr;
	void		       *sc_psmbuf;	/* DMA-mapped psm frame area */
	//bus_dma_tag_t		sc_fragdmat;	/* tags for all queues */
	struct pgt_mgmt_descq	sc_mgmtinprog;
	struct pgt_descq	sc_freeq[PFF_QUEUE_COUNT];
	size_t			sc_freeq_count[PFF_QUEUE_COUNT];
	struct pgt_descq	sc_dirtyq[PFF_QUEUE_COUNT];
	size_t			sc_dirtyq_count[PFF_QUEUE_COUNT];
	struct bintime		sc_data_tx_started;
	struct pgt_softc_kthread {
		struct proc		       *sck_proc;
//		struct cv			sck_needed;
		int				sck_exit, sck_reset, sck_update;
		TAILQ_HEAD(, pgt_async_trap)	sck_traps;
	}			sc_kthread;
};

void	pgt_attachhook(void *);
int	pgt_intr(void *);
int	pgt_attach(struct pgt_softc *);
int	pgt_detach(struct pgt_softc *sc);
void	pgt_reboot(struct pgt_softc *);
/* Load one seg into the bus_addr_t * arg. */
//void	pgt_load_busaddr(void *, bus_dma_segment_t *, int, int);

static __inline int
pgt_queue_is_rx(enum pgt_queue pq)
{
	return (pq == PFF_QUEUE_DATA_LOW_RX ||
	    pq == PFF_QUEUE_DATA_HIGH_RX ||
	    pq == PFF_QUEUE_MGMT_RX);
}

static __inline int
pgt_queue_is_tx(enum pgt_queue pq)
{
	return (pq == PFF_QUEUE_DATA_LOW_TX ||
	    pq == PFF_QUEUE_DATA_HIGH_TX ||
	    pq == PFF_QUEUE_MGMT_TX);
}

static __inline int
pgt_queue_is_data(enum pgt_queue pq)
{
	return (pq == PFF_QUEUE_DATA_LOW_RX ||
	    pq == PFF_QUEUE_DATA_HIGH_RX ||
	    pq == PFF_QUEUE_DATA_LOW_TX ||
	    pq == PFF_QUEUE_DATA_HIGH_TX);
}

static __inline int
pgt_queue_is_mgmt(enum pgt_queue pq)
{
	return (pq == PFF_QUEUE_MGMT_RX ||
	    pq == PFF_QUEUE_MGMT_TX);
}
