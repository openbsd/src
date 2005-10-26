/*	$OpenBSD: musyccvar.h,v 1.8 2005/10/26 09:26:56 claudio Exp $ */

/*
 * Copyright (c) 2004,2005  Internet Business Solutions AG, Zurich, Switzerland
 * Written by: Claudio Jeker <jeker@accoom.net>
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
#ifndef __MUSYCCVAR_H__
#define __MUSYCCVAR_H__

#include <sys/queue.h>

#define PPP_HEADER_LEN 4		/* should be globaly defined by sppp */

/* some defaults */
#define MUSYCC_NUMCHAN		32	/* 32 channels per group */
#define MUSYCC_NUMPORT		8	/* max 8 ports per controller */
#define MUSYCC_SREQNUM		16	/* pending SREQ */
#define MUSYCC_SREQMASK		(MUSYCC_SREQNUM - 1)
#define MUSYCC_SREQTIMEOUT	2

/* dma ring sizes */
#define MUSYCC_DMA_CNT		256
#define MUSYCC_DMA_MAPSIZE	(MUSYCC_DMA_CNT * sizeof(struct dma_desc))
#define MUSYCC_DMA_SIZE		32

struct musycc_softc;
struct ebus_softc;

/* DMA descriptor for data */
struct dma_desc {
	u_int32_t		 status;
	u_int32_t		 data;
	u_int32_t		 next;
	/* Software only */
	struct mbuf		*mbuf;
	struct dma_desc		*nextdesc;
	bus_dmamap_t		 map;
};

#define MUSYCC_INTLEN		512	/* 512 pending interrups is enough */
struct musycc_intdesc {
	u_int32_t		 md_intrq[MUSYCC_INTLEN];
};

struct musycc_dma_data {
	/*
	 * received dma ring. rx_prod points to the frist descriptors that
	 * is under musycc control (first empty).
	 */
	struct dma_desc		*rx_prod;
	int			 rx_cnt;

	struct dma_desc		*tx_pend;	/* finished pointer */
	struct dma_desc		*tx_cur;	/* current insertion pointer */
	int			 tx_cnt;	/* number of descriptors */
	int			 tx_use;	/* number of used descriptors */
	int			 tx_pkts;	/* number of packets in queue */
};

enum musycc_state {
	CHAN_FLOAT,	/* unconnected channel */
	CHAN_IDLE,
	CHAN_RUNNING,
	CHAN_FAULT,
	CHAN_TRANSIENT	/* dummy state to protect ongoing state changes */
};

enum musycc_event {
	EV_NULL,	/* null event, ignore */
	EV_ACTIVATE,	/* activate channel go to running state */
	EV_STOP,	/* stop dma engine */
	EV_IDLE,	/* free timeslots et al. and go to idle state */
	EV_WATCHDOG	/* watchdog event, stop dma engine */
};

/* group structure */
struct musycc_group {
	struct musycc_softc	*mg_hdlc;	/* main controller */
	struct musycc_grpdesc	*mg_group;	/* group descriptor */
	u_int8_t		 mg_gnum;	/* group number */
	u_int8_t		 mg_port;	/* port number */
	u_int8_t		 mg_loaded;	/* sreq(5) done? */
	u_int64_t		 mg_fifomask;	/* fifo allocation mask */

	struct channel_softc	*mg_channels[MUSYCC_NUMCHAN];
	struct musycc_dma_data	 mg_dma_d[MUSYCC_NUMCHAN];
	struct dma_desc		*mg_freelist;
	int			 mg_freecnt;

	struct {
		long			timeout;
		u_int32_t		sreq;
		enum musycc_event	event;
	}			 mg_sreq[MUSYCC_SREQNUM];
	int			 mg_sreqpend;
	int			 mg_sreqprod;

	struct dma_desc		*mg_dma_pool;
	bus_dma_tag_t		 mg_dmat;	/* bus dma tag */
	caddr_t			 mg_listkva;
	bus_dmamap_t		 mg_listmap;
	bus_dma_segment_t	 mg_listseg[1];
	int			 mg_listnseg;
	bus_dmamap_t		 mg_tx_sparemap;
	bus_dmamap_t		 mg_rx_sparemap;
};

/* attach arguments for framer devices */
struct musycc_attach_args {
	char				ma_product[64];
	bus_size_t			ma_base;
	bus_size_t			ma_size;
	u_int32_t			ma_type;
	u_int8_t			ma_gnum;
	u_int8_t			ma_port;
	u_int8_t			ma_flags;
	char				ma_slot;
};

/* generic ebus device handle */
struct ebus_dev {
	bus_size_t			base;
	bus_size_t			size;
	bus_space_tag_t			st;
	bus_space_handle_t		sh;
};

/* Softc for each HDLC channel config */
struct channel_softc {
	struct sppp		 cc_ppp;	/* sppp network attachement */
	struct ifnet		*cc_ifp;	/* pointer to the active ifp */
	struct musycc_group	*cc_group;
	struct device		*cc_parent;	/* parent framer */

	u_int32_t		 cc_tslots;	/* timeslot map */
	int			 cc_unit;
	enum musycc_state	 cc_state;	/* state machine info */
	u_int8_t		 cc_channel;	/* HDLC channel */
	u_int8_t		 cc_locked;
};

/* Softc for the HDLC Controller (function 0) */
struct musycc_softc {
	struct device		 mc_dev;	/* generic device structures */
	void			*mc_ih;		/* interrupt handler cookie */
	bus_space_tag_t		 mc_st;		/* bus space tag */
	bus_space_handle_t	 mc_sh;		/* bus space handle */
	bus_dma_tag_t		 mc_dmat;	/* bus dma tag */
	bus_size_t		 mc_iosize;	/* size of bus space */

	caddr_t			 mc_groupkva;	/* group configuration mem */
	bus_dmamap_t		 mc_cfgmap;
	bus_dma_segment_t	 mc_cfgseg[1];
	bus_dmamap_t		 mc_intrmap;
	bus_dma_segment_t	 mc_intrseg[1];
	int			 mc_cfgnseg;
	int			 mc_intrnseg;

	struct musycc_group	*mc_groups;	/* mc_ngroups groups */
	struct musycc_intdesc	*mc_intrd;
	u_int32_t		 mc_global_conf; /* global config descriptor */
	u_int32_t		 mc_intrqptr;	 /* interrupt queue pointer */
	int			 mc_ngroups;
	int			 mc_nports;

	struct musycc_softc	*mc_other;	/* the other EBUS/HDLC dev */
	bus_size_t		 mc_ledbase;
	u_int8_t		 mc_ledmask;
	u_int8_t		 mc_ledstate;	/* current state of the LEDs */
	int			 bus, device;	/* location of card */
	SLIST_ENTRY(musycc_softc) list;		/* list of all hdlc ctrls */
};

int	musycc_attach_common(struct musycc_softc *, u_int32_t, u_int32_t);
void	musycc_set_port(struct musycc_group *, int);
int	musycc_init_channel(struct channel_softc *, char);
void	musycc_stop_channel(struct channel_softc *);
void	musycc_free_channel(struct musycc_group *, int);
void	musycc_start(struct ifnet *);
void	musycc_watchdog(struct ifnet *);
void	musycc_tick(struct channel_softc *);

int	musycc_intr(void *);
int	ebus_intr(void *);

/* EBUS API */
int		ebus_attach_device(struct ebus_dev *, struct musycc_softc *,
		    bus_size_t, bus_size_t);
u_int8_t	ebus_read(struct ebus_dev *, bus_size_t);
void		ebus_write(struct ebus_dev *, bus_size_t, u_int8_t);
void		ebus_read_buf(struct ebus_dev *, bus_size_t, void *, size_t);
void		ebus_set_led(struct channel_softc *, int, u_int8_t);

#define MUSYCC_LED_GREEN	0x1
#define MUSYCC_LED_RED		0x2
#define MUSYCC_LED_MASK		0x3

/* channel API */
struct channel_softc	*musycc_channel_create(const char *, u_int8_t);
void		musycc_attach_sppp(struct channel_softc *,
		    int (*)(struct ifnet *, u_long, caddr_t));
int		musycc_channel_attach(struct musycc_softc *,
		    struct channel_softc *, struct device *, u_int8_t);
void		musycc_channel_detach(struct ifnet *);


#ifndef ACCOOM_DEBUG
#define ACCOOM_PRINTF(n, x)
#else
extern int accoom_debug;

#define ACCOOM_PRINTF(n, x)					\
	do {							\
		if (accoom_debug >= n)				\
			printf x;				\
	} while (0)
#endif

#endif
