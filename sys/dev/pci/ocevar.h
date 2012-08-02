/* 	$OpenBSD: ocevar.h,v 1.3 2012/08/02 22:14:31 mikeb Exp $	*/

/*-
 * Copyright (C) 2012 Emulex
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 * 1. Redistributions of source code must retain the above copyright notice,
 *    this list of conditions and the following disclaimer.
 *
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * 3. Neither the name of the Emulex Corporation nor the names of its
 *    contributors may be used to endorse or promote products derived from
 *    this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 *
 * Contact Information:
 * freebsd-drivers@emulex.com
 *
 * Emulex
 * 3333 Susan Street
 * Costa Mesa, CA 92626
 */

/* OCE device driver module component revision informaiton */
#define COMPONENT_REVISION "4.2.127.0"

#define IS_BE(sc)	(((sc->flags & OCE_FLAGS_BE3) | \
			 (sc->flags & OCE_FLAGS_BE2))? 1:0)
#define IS_XE201(sc)	((sc->flags & OCE_FLAGS_XE201) ? 1:0)
#define HAS_A0_CHIP(sc)	((sc->flags & OCE_FLAGS_HAS_A0_CHIP) ? 1:0)

/* proportion Service Level Interface queues */
#define OCE_MAX_UNITS			2
#define OCE_MAX_PPORT			OCE_MAX_UNITS
#define OCE_MAX_VPORT			OCE_MAX_UNITS

/* This should be powers of 2. Like 2,4,8 & 16 */
#define OCE_MAX_RSS			4 /* TODO: 8*/
#define OCE_LEGACY_MODE_RSS		4 /* For BE3 Legacy mode*/

#define OCE_MIN_RQ			1
#define OCE_MIN_WQ			1

#define OCE_MAX_RQ			OCE_MAX_RSS + 1 /* one default queue */
#define OCE_MAX_WQ			8

#define OCE_MAX_EQ			32
#define OCE_MAX_CQ			OCE_MAX_RQ + OCE_MAX_WQ + 1 /* one MCC queue */
#define OCE_MAX_CQ_EQ			8 /* Max CQ that can attached to an EQ */

#define OCE_DEFAULT_WQ_EQD		64
#define OCE_MAX_PACKET_Q		16
#define OCE_RQ_BUF_SIZE			2048
#define OCE_LSO_MAX_SIZE		(64 * 1024)
#define LONG_TIMEOUT			30
#define OCE_MAX_JUMBO_FRAME_SIZE	16360
#define OCE_MAX_MTU			(OCE_MAX_JUMBO_FRAME_SIZE - \
						ETHER_VLAN_ENCAP_LEN - \
						ETHER_HDR_LEN)

#define OCE_MAX_TX_ELEMENTS		29
#define OCE_MAX_TX_DESC			1024
#define OCE_MAX_TX_SIZE			65535
#define OCE_MAX_RX_SIZE			4096
#define OCE_MAX_RQ_POSTS		255
#define OCE_DEFAULT_PROMISCUOUS		0

#define RSS_ENABLE_IPV4			0x1
#define RSS_ENABLE_TCP_IPV4		0x2
#define RSS_ENABLE_IPV6			0x4
#define RSS_ENABLE_TCP_IPV6		0x8

/* flow control definitions */
#define OCE_FC_NONE			0x00000000
#define OCE_FC_TX			0x00000001
#define OCE_FC_RX			0x00000002
#define OCE_DEFAULT_FLOW_CONTROL	(OCE_FC_TX | OCE_FC_RX)

/* Interface capabilities to give device when creating interface */
#define  OCE_CAPAB_FLAGS 		(MBX_RX_IFACE_FLAGS_BROADCAST    | \
					MBX_RX_IFACE_FLAGS_UNTAGGED      | \
					MBX_RX_IFACE_FLAGS_PROMISCUOUS   | \
					MBX_RX_IFACE_FLAGS_MCAST_PROMISCUOUS | \
					MBX_RX_IFACE_FLAGS_RSS)
					/* MBX_RX_IFACE_FLAGS_RSS | \ */
					/* MBX_RX_IFACE_FLAGS_PASS_L3L4_ERR) */

/* Interface capabilities to enable by default (others set dynamically) */
#define  OCE_CAPAB_ENABLE		(MBX_RX_IFACE_FLAGS_BROADCAST | \
					MBX_RX_IFACE_FLAGS_UNTAGGED   | \
					MBX_RX_IFACE_FLAGS_RSS)
					/* MBX_RX_IFACE_FLAGS_RSS        | \ */
					/* MBX_RX_IFACE_FLAGS_PASS_L3L4_ERR) */

#define ETH_ADDR_LEN			6
#define MAX_VLANFILTER_SIZE		64
#define MAX_VLANS			4096

#define upper_32_bits(n)		((uint32_t)(((n) >> 16) >> 16))
#define BSWAP_8(x)			((x) & 0xff)
#define BSWAP_16(x)			((BSWAP_8(x) << 8) | BSWAP_8((x) >> 8))
#define BSWAP_32(x)			((BSWAP_16(x) << 16) | \
					 BSWAP_16((x) >> 16))
#define BSWAP_64(x)			((BSWAP_32(x) << 32) | \
					BSWAP_32((x) >> 32))

#define for_all_wq_queues(sc, wq, i) 	\
		for (i = 0, wq = sc->wq[0]; i < sc->nwqs; i++, wq = sc->wq[i])
#define for_all_rq_queues(sc, rq, i) 	\
		for (i = 0, rq = sc->rq[0]; i < sc->nrqs; i++, rq = sc->rq[i])
#define for_all_eq_queues(sc, eq, i) 	\
		for (i = 0, eq = sc->eq[0]; i < sc->neqs; i++, eq = sc->eq[i])
#define for_all_cq_queues(sc, cq, i) 	\
		for (i = 0, cq = sc->cq[0]; i < sc->ncqs; i++, cq = sc->cq[i])

/* Flash specific */
#define IOCTL_COOKIE			"SERVERENGINES CORP"
#define MAX_FLASH_COMP			32

#define IMG_ISCSI			160
#define IMG_REDBOOT			224
#define IMG_BIOS			34
#define IMG_PXEBIOS			32
#define IMG_FCOEBIOS			33
#define IMG_ISCSI_BAK			176
#define IMG_FCOE			162
#define IMG_FCOE_BAK			178
#define IMG_NCSI			16
#define IMG_PHY				192
#define FLASHROM_OPER_FLASH		1
#define FLASHROM_OPER_SAVE		2
#define FLASHROM_OPER_REPORT		4
#define FLASHROM_OPER_FLASH_PHY		9
#define FLASHROM_OPER_SAVE_PHY		10
#define TN_8022				13

enum {
	PHY_TYPE_CX4_10GB = 0,
	PHY_TYPE_XFP_10GB,
	PHY_TYPE_SFP_1GB,
	PHY_TYPE_SFP_PLUS_10GB,
	PHY_TYPE_KR_10GB,
	PHY_TYPE_KX4_10GB,
	PHY_TYPE_BASET_10GB,
	PHY_TYPE_BASET_1GB,
	PHY_TYPE_BASEX_1GB,
	PHY_TYPE_SGMII,
	PHY_TYPE_DISABLED = 255
};

/* Ring related */
#define	GET_Q_NEXT(_START, _STEP, _END)	\
	((((_START) + (_STEP)) < (_END)) ? ((_START) + (_STEP))	\
	: (((_START) + (_STEP)) - (_END)))

#define	RING_NUM_FREE(_r)	\
	(uint32_t)((_r)->num_items - (_r)->num_used)
#define	RING_GET(_r, _n)	\
	(_r)->cidx = GET_Q_NEXT((_r)->cidx, _n, (_r)->num_items)
#define	RING_PUT(_r, _n)	\
	(_r)->pidx = GET_Q_NEXT((_r)->pidx, _n, (_r)->num_items)

#define OCE_DMAPTR(_o, _t) 		((_t *)(_o)->vaddr)

#define	RING_GET_CONSUMER_ITEM_VA(_r, _t)	\
	(OCE_DMAPTR(&(_r)->dma, _t) + (_r)->cidx)
#define	RING_GET_PRODUCER_ITEM_VA(_r, _t)	\
	(OCE_DMAPTR(&(_r)->dma, _t) + (_r)->pidx)


struct oce_packet_desc {
	struct mbuf *mbuf;
	bus_dmamap_t map;
	int nsegs;
	uint32_t wqe_idx;
};

struct oce_dma_mem {
	bus_dma_tag_t tag;
	bus_dmamap_t map;
	bus_dma_segment_t segs;
	int nsegs;
	bus_size_t size;
	caddr_t vaddr;
	bus_addr_t paddr;
};

struct oce_ring {
	uint16_t cidx;	/* Get ptr */
	uint16_t pidx;	/* Put Ptr */
	size_t item_size;
	size_t num_items;
	uint32_t num_used;
	struct oce_dma_mem dma;
};

/* Stats */
#define OCE_UNICAST_PACKET	0
#define OCE_MULTICAST_PACKET	1
#define OCE_BROADCAST_PACKET	2
#define OCE_RSVD_PACKET		3

struct oce_rx_stats {
	/* Total Receive Stats */
	uint64_t t_rx_pkts;
	uint64_t t_rx_bytes;
	uint32_t t_rx_frags;
	uint32_t t_rx_mcast_pkts;
	uint32_t t_rx_ucast_pkts;
	uint32_t t_rxcp_errs;
};

struct oce_tx_stats {
	/*Total Transmit Stats */
	uint64_t t_tx_pkts;
	uint64_t t_tx_bytes;
	uint32_t t_tx_reqs;
	uint32_t t_tx_stops;
	uint32_t t_tx_wrbs;
	uint32_t t_tx_compl;
	uint32_t t_ipv6_ext_hdr_tx_drop;
};

struct oce_be_stats {
	uint8_t  be_on_die_temperature;
	uint32_t be_tx_events;
	uint32_t eth_red_drops;
	uint32_t rx_drops_no_pbuf;
	uint32_t rx_drops_no_txpb;
	uint32_t rx_drops_no_erx_descr;
	uint32_t rx_drops_no_tpre_descr;
	uint32_t rx_drops_too_many_frags;
	uint32_t rx_drops_invalid_ring;
	uint32_t forwarded_packets;
	uint32_t rx_drops_mtu;
	uint32_t rx_crc_errors;
	uint32_t rx_alignment_symbol_errors;
	uint32_t rx_pause_frames;
	uint32_t rx_priority_pause_frames;
	uint32_t rx_control_frames;
	uint32_t rx_in_range_errors;
	uint32_t rx_out_range_errors;
	uint32_t rx_frame_too_long;
	uint32_t rx_address_match_errors;
	uint32_t rx_dropped_too_small;
	uint32_t rx_dropped_too_short;
	uint32_t rx_dropped_header_too_small;
	uint32_t rx_dropped_tcp_length;
	uint32_t rx_dropped_runt;
	uint32_t rx_ip_checksum_errs;
	uint32_t rx_tcp_checksum_errs;
	uint32_t rx_udp_checksum_errs;
	uint32_t rx_switched_unicast_packets;
	uint32_t rx_switched_multicast_packets;
	uint32_t rx_switched_broadcast_packets;
	uint32_t tx_pauseframes;
	uint32_t tx_priority_pauseframes;
	uint32_t tx_controlframes;
	uint32_t rxpp_fifo_overflow_drop;
	uint32_t rx_input_fifo_overflow_drop;
	uint32_t pmem_fifo_overflow_drop;
	uint32_t jabber_events;
};

struct oce_xe201_stats {
	uint64_t tx_pkts;
	uint64_t tx_unicast_pkts;
	uint64_t tx_multicast_pkts;
	uint64_t tx_broadcast_pkts;
	uint64_t tx_bytes;
	uint64_t tx_unicast_bytes;
	uint64_t tx_multicast_bytes;
	uint64_t tx_broadcast_bytes;
	uint64_t tx_discards;
	uint64_t tx_errors;
	uint64_t tx_pause_frames;
	uint64_t tx_pause_on_frames;
	uint64_t tx_pause_off_frames;
	uint64_t tx_internal_mac_errors;
	uint64_t tx_control_frames;
	uint64_t tx_pkts_64_bytes;
	uint64_t tx_pkts_65_to_127_bytes;
	uint64_t tx_pkts_128_to_255_bytes;
	uint64_t tx_pkts_256_to_511_bytes;
	uint64_t tx_pkts_512_to_1023_bytes;
	uint64_t tx_pkts_1024_to_1518_bytes;
	uint64_t tx_pkts_1519_to_2047_bytes;
	uint64_t tx_pkts_2048_to_4095_bytes;
	uint64_t tx_pkts_4096_to_8191_bytes;
	uint64_t tx_pkts_8192_to_9216_bytes;
	uint64_t tx_lso_pkts;
	uint64_t rx_pkts;
	uint64_t rx_unicast_pkts;
	uint64_t rx_multicast_pkts;
	uint64_t rx_broadcast_pkts;
	uint64_t rx_bytes;
	uint64_t rx_unicast_bytes;
	uint64_t rx_multicast_bytes;
	uint64_t rx_broadcast_bytes;
	uint32_t rx_unknown_protos;
	uint64_t rx_discards;
	uint64_t rx_errors;
	uint64_t rx_crc_errors;
	uint64_t rx_alignment_errors;
	uint64_t rx_symbol_errors;
	uint64_t rx_pause_frames;
	uint64_t rx_pause_on_frames;
	uint64_t rx_pause_off_frames;
	uint64_t rx_frames_too_long;
	uint64_t rx_internal_mac_errors;
	uint32_t rx_undersize_pkts;
	uint32_t rx_oversize_pkts;
	uint32_t rx_fragment_pkts;
	uint32_t rx_jabbers;
	uint64_t rx_control_frames;
	uint64_t rx_control_frames_unknown_opcode;
	uint32_t rx_in_range_errors;
	uint32_t rx_out_of_range_errors;
	uint32_t rx_address_match_errors;
	uint32_t rx_vlan_mismatch_errors;
	uint32_t rx_dropped_too_small;
	uint32_t rx_dropped_too_short;
	uint32_t rx_dropped_header_too_small;
	uint32_t rx_dropped_invalid_tcp_length;
	uint32_t rx_dropped_runt;
	uint32_t rx_ip_checksum_errors;
	uint32_t rx_tcp_checksum_errors;
	uint32_t rx_udp_checksum_errors;
	uint32_t rx_non_rss_pkts;
	uint64_t rx_ipv4_pkts;
	uint64_t rx_ipv6_pkts;
	uint64_t rx_ipv4_bytes;
	uint64_t rx_ipv6_bytes;
	uint64_t rx_nic_pkts;
	uint64_t rx_tcp_pkts;
	uint64_t rx_iscsi_pkts;
	uint64_t rx_management_pkts;
	uint64_t rx_switched_unicast_pkts;
	uint64_t rx_switched_multicast_pkts;
	uint64_t rx_switched_broadcast_pkts;
	uint64_t num_forwards;
	uint32_t rx_fifo_overflow;
	uint32_t rx_input_fifo_overflow;
	uint64_t rx_drops_too_many_frags;
	uint32_t rx_drops_invalid_queue;
	uint64_t rx_drops_mtu;
	uint64_t rx_pkts_64_bytes;
	uint64_t rx_pkts_65_to_127_bytes;
	uint64_t rx_pkts_128_to_255_bytes;
	uint64_t rx_pkts_256_to_511_bytes;
	uint64_t rx_pkts_512_to_1023_bytes;
	uint64_t rx_pkts_1024_to_1518_bytes;
	uint64_t rx_pkts_1519_to_2047_bytes;
	uint64_t rx_pkts_2048_to_4095_bytes;
	uint64_t rx_pkts_4096_to_8191_bytes;
	uint64_t rx_pkts_8192_to_9216_bytes;
};

struct oce_drv_stats {
	struct oce_rx_stats rx;
	struct oce_tx_stats tx;
	union {
		struct oce_be_stats be;
		struct oce_xe201_stats xe201;
	} u0;
};

typedef int boolean_t;
#define TRUE					1
#define FALSE					0

#define	DEFAULT_MQ_MBOX_TIMEOUT			(5 * 1000 * 1000)
#define	MBX_READY_TIMEOUT			(1 * 1000 * 1000)
#define	DEFAULT_DRAIN_TIME			200
#define	MBX_TIMEOUT_SEC				5
#define	STAT_TIMEOUT				2000000

/* size of the packet descriptor array in a transmit queue */
#define OCE_TX_RING_SIZE			512
#define OCE_RX_RING_SIZE			1024
#define OCE_WQ_PACKET_ARRAY_SIZE		(OCE_TX_RING_SIZE/2)
#define OCE_RQ_PACKET_ARRAY_SIZE		(OCE_RX_RING_SIZE)

struct oce_dev;

enum eq_len {
	EQ_LEN_256  = 256,
	EQ_LEN_512  = 512,
	EQ_LEN_1024 = 1024,
	EQ_LEN_2048 = 2048,
	EQ_LEN_4096 = 4096
};

enum eqe_size {
	EQE_SIZE_4  = 4,
	EQE_SIZE_16 = 16
};

enum qtype {
	QTYPE_EQ,
	QTYPE_MQ,
	QTYPE_WQ,
	QTYPE_RQ,
	QTYPE_CQ,
	QTYPE_RSS
};

typedef enum qstate_e {
	QDELETED = 0x0,
	QCREATED = 0x1
} qstate_t;

struct eq_config {
	enum eq_len q_len;
	enum eqe_size item_size;
	uint32_t q_vector_num;
	uint8_t min_eqd;
	uint8_t max_eqd;
	uint8_t cur_eqd;
};

struct oce_eq {
	uint32_t eq_id;
	void *parent;
	void *cb_context;
	struct oce_ring *ring;
	uint32_t ref_count;
	qstate_t qstate;
	struct oce_cq *cq[OCE_MAX_CQ_EQ];
	int cq_valid;
	struct eq_config eq_cfg;
	int vector;
};

enum cq_len {
	CQ_LEN_256  = 256,
	CQ_LEN_512  = 512,
	CQ_LEN_1024 = 1024
};

struct cq_config {
	enum cq_len q_len;
	uint32_t item_size;
	boolean_t is_eventable;
	boolean_t sol_eventable;
	boolean_t nodelay;
	uint16_t dma_coalescing;
};

struct oce_cq {
	uint32_t cq_id;
	void *parent;
	struct oce_eq *eq;
	void (*cq_handler)(void *);
	void *cb_arg;
	struct oce_ring *ring;
	qstate_t qstate;
	struct cq_config cq_cfg;
	uint32_t ref_count;
};

struct mq_config {
	uint32_t eqd;
	uint8_t q_len;
};

struct oce_mq {
	void *parent;
	struct oce_ring *ring;
	uint32_t mq_id;
	struct oce_cq *cq;
	struct oce_cq *async_cq;
	uint32_t mq_free;
	qstate_t qstate;
	struct mq_config cfg;
};

struct oce_mbx_ctx {
	struct oce_mbx *mbx;
	void (*cb) (void *ctx);
	void *cb_ctx;
};

struct wq_config {
	uint8_t wq_type;
	uint16_t buf_size;
	uint32_t q_len;
	uint16_t pd_id;
	uint16_t pci_fn_num;
	uint32_t eqd;	/* interrupt delay */
	uint32_t nbufs;
	uint32_t nhdl;
};

struct oce_tx_queue_stats {
	uint64_t tx_pkts;
	uint64_t tx_bytes;
	uint32_t tx_reqs;
	uint32_t tx_stops; /* number of times TX Q was stopped */
	uint32_t tx_wrbs;
	uint32_t tx_compl;
	uint32_t tx_rate;
	uint32_t ipv6_ext_hdr_tx_drop;
};

struct oce_wq {
	void *parent;
	struct oce_ring *ring;
	struct oce_cq *cq;
	bus_dma_tag_t tag;
	struct oce_packet_desc pckts[OCE_WQ_PACKET_ARRAY_SIZE];
	uint32_t packets_in;
	uint32_t packets_out;
	uint32_t wqm_used;
	boolean_t resched;
	uint32_t wq_free;
	uint32_t tx_deferd;
	uint32_t pkt_drops;
	qstate_t qstate;
	uint16_t wq_id;
	struct wq_config cfg;
	int queue_index;
	struct oce_tx_queue_stats tx_stats;
};

struct rq_config {
	uint32_t q_len;
	uint32_t frag_size;
	uint32_t mtu;
	uint32_t if_id;
	uint32_t is_rss_queue;
	uint32_t eqd;
	uint32_t nbufs;
};

struct oce_rx_queue_stats {
	uint32_t rx_post_fail;
	uint32_t rx_ucast_pkts;
	uint32_t rx_compl;
	uint64_t rx_bytes;
	uint64_t rx_bytes_prev;
	uint64_t rx_pkts;
	uint32_t rx_rate;
	uint32_t rx_mcast_pkts;
	uint32_t rxcp_err;
	uint32_t rx_frags;
	uint32_t prev_rx_frags;
	uint32_t rx_fps;
};

struct oce_rq {
	struct rq_config cfg;
	uint32_t rq_id;
	int queue_index;
	uint32_t rss_cpuid;
	void *parent;
	struct oce_ring *ring;
	struct oce_cq *cq;
	bus_dma_tag_t tag;
	struct oce_packet_desc pckts[OCE_RQ_PACKET_ARRAY_SIZE];
	uint32_t packets_in;
	uint32_t packets_out;
	uint32_t pending;
#ifdef notdef
	struct mbuf *head;
	struct mbuf *tail;
	int fragsleft;
#endif
	qstate_t qstate;
	struct oce_rx_queue_stats rx_stats;
#ifdef OCE_LRO
	struct lro_ctrl lro;
	int lro_pkts_queued;
#endif
};

struct link_status {
	uint8_t physical_port;
	uint8_t mac_duplex;
	uint8_t mac_speed;
	uint8_t mac_fault;
	uint8_t mgmt_mac_duplex;
	uint8_t mgmt_mac_speed;
	uint16_t qos_link_speed;
	uint32_t logical_link_status;
} __packed;

#define OCE_FLAGS_PCIX			0x00000001
#define OCE_FLAGS_PCIE			0x00000002
#define OCE_FLAGS_MSI_CAPABLE		0x00000004
#define OCE_FLAGS_MSIX_CAPABLE		0x00000008
#define OCE_FLAGS_USING_MSI		0x00000010
#define OCE_FLAGS_USING_MSIX		0x00000020
#define OCE_FLAGS_FUNCRESET_RQD		0x00000040
#define OCE_FLAGS_VIRTUAL_PORT		0x00000080
#define OCE_FLAGS_MBOX_ENDIAN_RQD	0x00000100
#define OCE_FLAGS_BE3			0x00000200
#define OCE_FLAGS_XE201			0x00000400
#define OCE_FLAGS_BE2			0x00000800

struct oce_softc {
	struct device dev;

	uint32_t flags;

	struct pci_attach_args pa;

	bus_space_tag_t cfg_btag;
	bus_space_handle_t cfg_bhandle;
	bus_size_t cfg_size;

	bus_space_tag_t csr_btag;
	bus_space_handle_t csr_bhandle;
	bus_size_t csr_size;

	bus_space_tag_t db_btag;
	bus_space_handle_t db_bhandle;
	bus_size_t db_size;

	struct arpcom arpcom;
	struct ifmedia media;
	int link_active;
	uint8_t link_status;
	uint8_t link_speed;
	uint8_t duplex;
	uint32_t qos_link_speed;

	char fw_version[32];
	struct mac_address_format macaddr;

	struct oce_dma_mem bsmbx;

	uint32_t config_number;
	uint32_t asic_revision;
	uint32_t port_id;
	uint32_t function_mode;
	uint32_t function_caps;
	uint32_t max_tx_rings;
	uint32_t max_rx_rings;

	struct oce_wq *wq[OCE_MAX_WQ];	/* TX work queues */
	struct oce_rq *rq[OCE_MAX_RQ];	/* RX work queues */
	struct oce_cq *cq[OCE_MAX_CQ];	/* Completion queues */
	struct oce_eq *eq[OCE_MAX_EQ];	/* Event queues */
	struct oce_mq *mq;		/* Mailbox queue */

	ushort neqs;
	ushort ncqs;
	ushort nrqs;
	ushort nwqs;
	ushort intr_count;
	ushort tx_ring_size;
	ushort rx_ring_size;
	ushort rq_frag_size;
	ushort rss_enable;

	uint32_t if_id;		/* interface ID */
	uint32_t nifs;		/* number of adapter interfaces, 0 or 1 */
	uint32_t pmac_id;	/* PMAC id */

	uint32_t if_cap_flags;

	uint32_t flow_control;
	char promisc;

	char be3_native;
	uint32_t pvid;

	struct oce_dma_mem stats_mem;
	struct oce_drv_stats oce_stats_info;
	struct timeout timer;
	struct timeout rxrefill;
};

/**************************************************
 * BUS memory read/write macros
 * BE3: accesses three BAR spaces (CFG, CSR, DB)
 * Lancer: accesses one BAR space (CFG)
 **************************************************/
#if 1
#define OCE_READ_REG32(sc, space, o) \
	((IS_BE(sc)) ? (bus_space_read_4((sc)->space##_btag, \
				      (sc)->space##_bhandle,o)) \
		  : (bus_space_read_4((sc)->cfg_btag, \
				      (sc)->cfg_bhandle,o)))
#define OCE_READ_REG16(sc, space, o) \
	((IS_BE(sc)) ? (bus_space_read_2((sc)->space##_btag, \
				      (sc)->space##_bhandle,o)) \
		  : (bus_space_read_2((sc)->cfg_btag, \
				      (sc)->cfg_bhandle,o)))
#define OCE_READ_REG8(sc, space, o) \
	((IS_BE(sc)) ? (bus_space_read_1((sc)->space##_btag, \
				      (sc)->space##_bhandle,o)) \
		  : (bus_space_read_1((sc)->cfg_btag, \
				      (sc)->cfg_bhandle,o)))

#define OCE_WRITE_REG32(sc, space, o, v) \
	((IS_BE(sc)) ? (bus_space_write_4((sc)->space##_btag, \
				       (sc)->space##_bhandle,o,v)) \
		  : (bus_space_write_4((sc)->cfg_btag, \
				       (sc)->cfg_bhandle,o,v)))
#define OCE_WRITE_REG16(sc, space, o, v) \
	((IS_BE(sc)) ? (bus_space_write_2((sc)->space##_btag, \
				       (sc)->space##_bhandle,o,v)) \
		  : (bus_space_write_2((sc)->cfg_btag, \
				       (sc)->cfg_bhandle,o,v)))
#define OCE_WRITE_REG8(sc, space, o, v) \
	((IS_BE(sc)) ? (bus_space_write_1((sc)->space##_btag, \
				       (sc)->space##_bhandle,o,v)) \
		  : (bus_space_write_1((sc)->cfg_btag, \
				       (sc)->cfg_bhandle,o,v)))
#else
static __inline u_int32_t
oce_bus_read_4(bus_space_tag_t tag, bus_space_handle_t handle, bus_size_t reg)
{
	bus_space_barrier(tag, handle, reg, 4, BUS_SPACE_BARRIER_READ);
	return (bus_space_read_4(tag, handle, reg));
}

static __inline u_int16_t
oce_bus_read_2(bus_space_tag_t tag, bus_space_handle_t handle, bus_size_t reg)
{
	bus_space_barrier(tag, handle, reg, 2, BUS_SPACE_BARRIER_READ);
	return (bus_space_read_2(tag, handle, reg));
}

static __inline u_int8_t
oce_bus_read_1(bus_space_tag_t tag, bus_space_handle_t handle, bus_size_t reg)
{
	bus_space_barrier(tag, handle, reg, 1, BUS_SPACE_BARRIER_READ);
	return (bus_space_read_1(tag, handle, reg));
}

static __inline void
oce_bus_write_4(bus_space_tag_t tag, bus_space_handle_t handle, bus_size_t reg,
    u_int32_t val)
{
	bus_space_write_4(tag, handle, reg, val);
	bus_space_barrier(tag, handle, reg, 4, BUS_SPACE_BARRIER_WRITE);
}

static __inline void
oce_bus_write_2(bus_space_tag_t tag, bus_space_handle_t handle, bus_size_t reg,
    u_int16_t val)
{
	bus_space_write_2(tag, handle, reg, val);
	bus_space_barrier(tag, handle, reg, 2, BUS_SPACE_BARRIER_WRITE);
}

static __inline void
oce_bus_write_1(bus_space_tag_t tag, bus_space_handle_t handle, bus_size_t reg,
    u_int8_t val)
{
	bus_space_write_1(tag, handle, reg, val);
	bus_space_barrier(tag, handle, reg, 1, BUS_SPACE_BARRIER_WRITE);
}

#define OCE_READ_REG32(sc, space, o) \
	((IS_BE(sc)) ? (oce_bus_read_4((sc)->space##_btag, \
				      (sc)->space##_bhandle,o)) \
		  : (oce_bus_read_4((sc)->cfg_btag, \
				      (sc)->cfg_bhandle,o)))
#define OCE_READ_REG16(sc, space, o) \
	((IS_BE(sc)) ? (oce_bus_read_2((sc)->space##_btag, \
				      (sc)->space##_bhandle,o)) \
		  : (oce_bus_read_2((sc)->cfg_btag, \
				      (sc)->cfg_bhandle,o)))
#define OCE_READ_REG8(sc, space, o) \
	((IS_BE(sc)) ? (oce_bus_read_1((sc)->space##_btag, \
				      (sc)->space##_bhandle,o)) \
		  : (oce_bus_read_1((sc)->cfg_btag, \
				      (sc)->cfg_bhandle,o)))

#define OCE_WRITE_REG32(sc, space, o, v) \
	((IS_BE(sc)) ? (oce_bus_write_4((sc)->space##_btag, \
				       (sc)->space##_bhandle,o,v)) \
		  : (oce_bus_write_4((sc)->cfg_btag, \
				       (sc)->cfg_bhandle,o,v)))
#define OCE_WRITE_REG16(sc, space, o, v) \
	((IS_BE(sc)) ? (oce_bus_write_2((sc)->space##_btag, \
				       (sc)->space##_bhandle,o,v)) \
		  : (oce_bus_write_2((sc)->cfg_btag, \
				       (sc)->cfg_bhandle,o,v)))
#define OCE_WRITE_REG8(sc, space, o, v) \
	((IS_BE(sc)) ? (oce_bus_write_1((sc)->space##_btag, \
				       (sc)->space##_bhandle,o,v)) \
		  : (oce_bus_write_1((sc)->cfg_btag, \
				       (sc)->cfg_bhandle,o,v)))
#endif

/***********************************************************
 * DMA memory functions
 ***********************************************************/
#define oce_dma_sync(d, f) \
	bus_dmamap_sync((d)->tag, (d)->map, 0, (d)->map->dm_mapsize, f)
#define oce_dmamap_sync(t, m, f) \
	bus_dmamap_sync(t, m, 0, (m)->dm_mapsize, f)
int  oce_dma_alloc(struct oce_softc *sc, bus_size_t size,
    struct oce_dma_mem *dma, int flags);
void oce_dma_free(struct oce_softc *sc, struct oce_dma_mem *dma);
void oce_dma_map_addr(void *arg, bus_dma_segment_t *segs, int nseg,
    int error);
void oce_destroy_ring(struct oce_softc *sc, struct oce_ring *ring);
struct oce_ring *oce_create_ring(struct oce_softc *sc, int q_len,
    int num_entries, int max_segs);
uint32_t oce_page_list(struct oce_softc *sc, struct oce_ring *ring,
    struct phys_addr *pa_list, int max_segs);

/************************************************************
 * oce_hw_xxx functions
 ************************************************************/
int  oce_hw_pci_alloc(struct oce_softc *sc);
int  oce_hw_init(struct oce_softc *sc);
int  oce_create_nw_interface(struct oce_softc *sc);
void oce_delete_nw_interface(struct oce_softc *sc);
int  oce_hw_update_multicast(struct oce_softc *sc);
void oce_hw_intr_enable(struct oce_softc *sc);
void oce_hw_intr_disable(struct oce_softc *sc);

/************************************************************
 * Mailbox functions
 ************************************************************/
int oce_mbox_init(struct oce_softc *sc);
int oce_mbox_dispatch(struct oce_softc *sc, uint32_t tmo_sec);
int oce_mbox_post(struct oce_softc *sc, struct oce_mbx *mbx,
    struct oce_mbx_ctx *mbxctx);
int oce_mbox_wait(struct oce_softc *sc, uint32_t tmo_sec);
int oce_first_mcc_cmd(struct oce_softc *sc);

int oce_get_link_status(struct oce_softc *sc);
int oce_rxf_set_promiscuous(struct oce_softc *sc, uint32_t enable);
int oce_config_nic_rss(struct oce_softc *sc, uint32_t if_id,
    uint16_t enable_rss);

int oce_mbox_macaddr_del(struct oce_softc *sc, uint32_t if_id,
    uint32_t pmac_id);
int oce_mbox_macaddr_add(struct oce_softc *sc, uint8_t *mac_addr,
    uint32_t if_id, uint32_t *pmac_id);
int oce_read_mac_addr(struct oce_softc *sc, uint32_t if_id, uint8_t perm,
    uint8_t type, struct mac_address_format *mac);

int oce_mbox_create_rq(struct oce_rq *rq);
int oce_mbox_create_wq(struct oce_wq *wq);
int oce_mbox_create_mq(struct oce_mq *mq);
int oce_mbox_create_eq(struct oce_eq *eq);
int oce_mbox_create_cq(struct oce_cq *cq, uint32_t ncoalesce,
    uint32_t is_eventable);
void mbx_common_req_hdr_init(struct mbx_hdr *hdr, uint8_t dom, uint8_t port,
    uint8_t subsys, uint8_t opcode, uint32_t timeout, uint32_t payload_len,
    uint8_t version);

/************************************************************
 * Statistics functions
 ************************************************************/
void oce_refresh_queue_stats(struct oce_softc *sc);
int  oce_refresh_nic_stats(struct oce_softc *sc);
int  oce_stats_init(struct oce_softc *sc);
void oce_stats_free(struct oce_softc *sc);

/* Capabilities */
#define OCE_MAX_RSP_HANDLED		64

#define OCE_MAC_LOOPBACK		0x0
#define OCE_PHY_LOOPBACK		0x1
#define OCE_ONE_PORT_EXT_LOOPBACK	0x2
#define OCE_NO_LOOPBACK			0xff

#define DW_SWAP(x, l)
#define IS_ALIGNED(x,a)		((x % a) == 0)
#define ADDR_HI(x)		((uint32_t)((uint64_t)(x) >> 32))
#define ADDR_LO(x)		((uint32_t)((uint64_t)(x) & 0xffffffff));

#define IFCAP_HWCSUM \
	(IFCAP_CSUM_IPv4 | IFCAP_CSUM_TCPv4 | IFCAP_CSUM_UDPv4)
#define IF_LRO_ENABLED(ifp)  (((ifp)->if_capabilities & IFCAP_LRO) ? 1:0)
#define IF_LSO_ENABLED(ifp)  (((ifp)->if_capabilities & IFCAP_TSO4) ? 1:0)
#define IF_CSUM_ENABLED(ifp) (((ifp)->if_capabilities & IFCAP_HWCSUM) ? 1:0)

#define OCE_LOG2(x) 		(oce_highbit(x))
static inline uint32_t oce_highbit(uint32_t x)
{
	int i;
	int c;
	int b;

	c = 0;
	b = 0;

	for (i = 0; i < 32; i++) {
		if ((1 << i) & x) {
			c++;
			b = i;
		}
	}

	if (c == 1)
		return b;

	return 0;
}
