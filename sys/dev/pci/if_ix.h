/*	$OpenBSD: if_ix.h,v 1.9 2010/02/23 18:43:15 jsg Exp $	*/

/******************************************************************************

  Copyright (c) 2001-2008, Intel Corporation 
  All rights reserved.
  
  Redistribution and use in source and binary forms, with or without 
  modification, are permitted provided that the following conditions are met:
  
   1. Redistributions of source code must retain the above copyright notice, 
      this list of conditions and the following disclaimer.
  
   2. Redistributions in binary form must reproduce the above copyright 
      notice, this list of conditions and the following disclaimer in the 
      documentation and/or other materials provided with the distribution.
  
   3. Neither the name of the Intel Corporation nor the names of its 
      contributors may be used to endorse or promote products derived from 
      this software without specific prior written permission.
  
  THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
  AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE 
  IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE 
  ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE 
  LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR 
  CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF 
  SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS 
  INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN 
  CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) 
  ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
  POSSIBILITY OF SUCH DAMAGE.

******************************************************************************/
/*$FreeBSD: src/sys/dev/ixgbe/ixgbe.h,v 1.4 2008/05/16 18:46:30 jfv Exp $*/

#ifndef _IX_H_
#define _IX_H_

#include <dev/pci/ixgbe.h>

#if 0
#include "tcp_lro.h"
#endif

/* Tunables */

/*
 * TxDescriptors Valid Range: 64-4096 Default Value: 256 This value is the
 * number of transmit descriptors allocated by the driver. Increasing this
 * value allows the driver to queue more transmits. Each descriptor is 16
 * bytes. Performance tests have show the 2K value to be optimal for top
 * performance.
 */
#define DEFAULT_TXD	256
#define PERFORM_TXD	2048
#define MAX_TXD		4096
#define MIN_TXD		64

/*
 * RxDescriptors Valid Range: 64-4096 Default Value: 256 This value is the
 * number of receive descriptors allocated for each RX queue. Increasing this
 * value allows the driver to buffer more incoming packets. Each descriptor
 * is 16 bytes.  A receive buffer is also allocated for each descriptor. 
 * 
 * Note: with 8 rings and a dual port card, it is possible to bump up 
 *	against the system mbuf pool limit, you can tune nmbclusters
 *	to adjust for this.
 */
#define DEFAULT_RXD	256
#define PERFORM_RXD	2048
#define MAX_RXD		4096
#define MIN_RXD		64

/* Alignment for rings */
#define DBA_ALIGN	128

/*
 * This parameter controls the maximum no of times the driver will loop in
 * the isr. Minimum Value = 1
 */
#define MAX_INTR	10

/*
 * This parameter controls the duration of transmit watchdog timer.
 */
#define IXGBE_TX_TIMEOUT                   5	/* set to 5 seconds */

/*
 * This parameters control when the driver calls the routine to reclaim
 * transmit descriptors.
 */
#define IXGBE_TX_CLEANUP_THRESHOLD	(sc->num_tx_desc / 8)
#define IXGBE_TX_OP_THRESHOLD		(sc->num_tx_desc / 32)

#define IXGBE_MAX_FRAME_SIZE	0x3F00

/* Flow control constants */
#define IXGBE_FC_PAUSE		0x680
#define IXGBE_FC_HI		0x20000
#define IXGBE_FC_LO		0x10000

/* Defines for printing debug information */
#define DEBUG_INIT  0
#define DEBUG_IOCTL 0
#define DEBUG_HW    0

#define INIT_DEBUGOUT(S)            if (DEBUG_INIT)  printf(S "\n")
#define INIT_DEBUGOUT1(S, A)        if (DEBUG_INIT)  printf(S "\n", A)
#define INIT_DEBUGOUT2(S, A, B)     if (DEBUG_INIT)  printf(S "\n", A, B)
#define IOCTL_DEBUGOUT(S)           if (DEBUG_IOCTL) printf(S "\n")
#define IOCTL_DEBUGOUT1(S, A)       if (DEBUG_IOCTL) printf(S "\n", A)
#define IOCTL_DEBUGOUT2(S, A, B)    if (DEBUG_IOCTL) printf(S "\n", A, B)
#define HW_DEBUGOUT(S)              if (DEBUG_HW) printf(S "\n")
#define HW_DEBUGOUT1(S, A)          if (DEBUG_HW) printf(S "\n", A)
#define HW_DEBUGOUT2(S, A, B)       if (DEBUG_HW) printf(S "\n", A, B)

#define MAX_NUM_MULTICAST_ADDRESSES     128
#define IXGBE_MAX_SCATTER		100
#define IXGBE_MSIX_BAR			3
#if 0
#define IXGBE_TSO_SIZE			65535
#else
#define IXGBE_TSO_SIZE			IXGBE_MAX_FRAME_SIZE
#endif
#define IXGBE_TX_BUFFER_SIZE		((uint32_t) 1514)
#define IXGBE_RX_HDR_SIZE		((uint32_t) 256)
#define CSUM_OFFLOAD			7	/* Bits in csum flags */

/* The number of MSIX messages the 82598 supports */
#define IXGBE_MSGS			18

/* For 6.X code compatibility */
#if __FreeBSD_version < 700000
#define ETHER_BPF_MTAP		BPF_MTAP
#define CSUM_TSO		0
#define IFCAP_TSO4		0
#define FILTER_STRAY
#define FILTER_HANDLED
#endif

/*
 * Interrupt Moderation parameters 
 * 	for now we hardcode, later
 *	it would be nice to do dynamic
 */
#define MAX_IRQ_SEC	8000
#define DEFAULT_ITR	1000000000/(MAX_IRQ_SEC * 256)
#define LINK_ITR	1000000000/(1950 * 256)

/* Used for auto RX queue configuration */
extern int mp_ncpus;

struct ixgbe_tx_buf {
	struct mbuf	*m_head;
	bus_dmamap_t	map;
};

struct ixgbe_rx_buf {
	struct mbuf	*m_head;
	bus_dmamap_t	 map;
};

/*
 * Bus dma allocation structure used by ixgbe_dma_malloc and ixgbe_dma_free.
 */
struct ixgbe_dma_alloc {
	caddr_t			dma_vaddr;
	bus_dma_tag_t		dma_tag;
	bus_dmamap_t		dma_map;
	bus_dma_segment_t	dma_seg;
	bus_size_t		dma_size;
	int			dma_nseg;
};

/*
 * The transmit ring, one per tx queue
 */
struct tx_ring {
        struct ix_softc		*sc;
	struct mutex		tx_mtx;
	uint32_t		me;
	uint32_t		msix;
	uint32_t		eims;
	uint32_t		watchdog_timer;
	union ixgbe_adv_tx_desc	*tx_base;
	uint32_t		*tx_hwb;
	struct ixgbe_dma_alloc	txdma;
	struct ixgbe_dma_alloc	txwbdma;
	uint32_t		next_avail_tx_desc;
	uint32_t		next_tx_to_clean;
	struct ixgbe_tx_buf	*tx_buffers;
	volatile uint16_t	tx_avail;
	uint32_t		txd_cmd;
	bus_dma_tag_t		txtag;
	/* Soft Stats */
	uint32_t		no_tx_desc_avail;
	uint32_t		no_tx_desc_late;
	uint64_t		tx_irq;
	uint64_t		tx_packets;
};


/*
 * The Receive ring, one per rx queue
 */
struct rx_ring {
        struct ix_softc		*sc;
	struct mutex		rx_mtx;
	uint32_t		me;
	uint32_t		msix;
	uint32_t		eims;
	uint32_t		payload;
	union ixgbe_adv_rx_desc	*rx_base;
	struct ixgbe_dma_alloc	rxdma;
#if 0
	struct lro_ctrl		lro;
#endif
        unsigned int		last_rx_desc_filled;
        unsigned int		next_to_check;
	int			rx_ndescs;
	struct ixgbe_rx_buf	*rx_buffers;
	bus_dma_tag_t		rxtag;
	struct mbuf		*fmp;
	struct mbuf		*lmp;
	/* Soft stats */
	uint64_t		rx_irq;
	uint64_t		packet_count;
	uint64_t 		byte_count;
};

/* Our adapter structure */
struct ix_softc {
	struct device		 dev;
	struct arpcom		 arpcom;

	struct ixgbe_hw	hw;
	struct ixgbe_osdep	 osdep;
	void			*powerhook;

	struct resource	*pci_mem;
	struct resource	*msix_mem;

	/*
	 * Interrupt resources:
	 *  Oplin has 20 MSIX messages
	 *  so allocate that for now.
	 */
	void		*tag[IXGBE_MSGS];
	struct resource *res[IXGBE_MSGS];
	int		rid[IXGBE_MSGS];
	uint32_t	eims_mask;

	struct ifmedia	media;
	struct timeout	timer;
	int		msix;
	int		if_flags;

	struct mutex	core_mtx;

	/* Legacy Fast Intr handling */
	int		sfp_probe;
	workq_fn	link_task;

	/* Info about the board itself */
	uint32_t	part_num;
	int		link_active;
	uint16_t	max_frame_size;
	uint32_t	link_speed;
	uint32_t	tx_int_delay;
	uint32_t	tx_abs_int_delay;
	uint32_t	rx_int_delay;
	uint32_t	rx_abs_int_delay;

	/* Indicates the cluster size to use */
	int		bigbufs;

	/*
	 * Transmit rings:
	 *	Allocated at run time, an array of rings.
	 */
	struct tx_ring	*tx_rings;
	int		num_tx_desc;
	int		num_tx_queues;

	/*
	 * Receive rings:
	 *	Allocated at run time, an array of rings.
	 */
	struct rx_ring	*rx_rings;
	int		num_rx_desc;
	int		num_rx_queues;
	uint32_t	rx_process_limit;
	uint		optics;

	/* Misc stats maintained by the driver */
	unsigned long   dropped_pkts;
	unsigned long   mbuf_alloc_failed;
	unsigned long   mbuf_cluster_failed;
	unsigned long   no_tx_map_avail;
	unsigned long   no_tx_dma_setup;
	unsigned long   watchdog_events;
	unsigned long   tso_tx;
	unsigned long	linkvec;
	unsigned long	link_irq;

	struct ixgbe_hw_stats stats;
};

#endif /* _IX_H_ */
