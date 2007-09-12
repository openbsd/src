/*	$OpenBSD: bwivar.h,v 1.2 2007/09/12 22:22:05 mglocker Exp $	*/

/*
 * Copyright (c) 2007 The DragonFly Project.  All rights reserved.
 * 
 * This code is derived from software contributed to The DragonFly Project
 * by Sepherosa Ziehau <sepherosa@gmail.com>
 * 
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in
 *    the documentation and/or other materials provided with the
 *    distribution.
 * 3. Neither the name of The DragonFly Project nor the names of its
 *    contributors may be used to endorse or promote products derived
 *    from this software without specific, prior written permission.
 * 
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * ``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS
 * FOR A PARTICULAR PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE
 * COPYRIGHT HOLDERS OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY OR CONSEQUENTIAL DAMAGES (INCLUDING,
 * BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED
 * AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
 * OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT
 * OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 * 
 * $DragonFly: src/sys/dev/netif/bwi/if_bwivar.h,v 1.1 2007/09/08 06:15:54 sephe Exp $
 */

#ifndef _IF_BWIVAR_H
#define _IF_BWIVAR_H

#define BWI_ALIGN		0x1000
#define BWI_RING_ALIGN		BWI_ALIGN
#define BWI_BUS_SPACE_MAXADDR	0x3fffffff

#define BWI_TX_NRING		6
#define BWI_TXRX_NRING		6
#define BWI_TX_NDESC		128
#define BWI_RX_NDESC		64
#define BWI_TXSTATS_NDESC	64
#define BWI_TX_NSPRDESC		2
#define BWI_TX_DATA_RING	1

/* XXX Onoe/Sample/AMRR probably need different configuration */
#define BWI_SHRETRY		7
#define BWI_LGRETRY		4
#define BWI_SHRETRY_FB		3
#define BWI_LGRETRY_FB		2

#define CSR_READ_4(sc, reg)		\
	bus_space_read_4((sc)->sc_mem_bt, (sc)->sc_mem_bh, (reg))
#define CSR_READ_2(sc, reg)		\
	bus_space_read_2((sc)->sc_mem_bt, (sc)->sc_mem_bh, (reg))

#define CSR_WRITE_4(sc, reg, val)	\
	bus_space_write_4((sc)->sc_mem_bt, (sc)->sc_mem_bh, (reg), (val))
#define CSR_WRITE_2(sc, reg, val)	\
	bus_space_write_2((sc)->sc_mem_bt, (sc)->sc_mem_bh, (reg), (val))

#define CSR_SETBITS_4(sc, reg, bits)	\
	CSR_WRITE_4((sc), (reg), CSR_READ_4((sc), (reg)) | (bits))
#define CSR_SETBITS_2(sc, reg, bits)	\
	CSR_WRITE_2((sc), (reg), CSR_READ_2((sc), (reg)) | (bits))

#define CSR_FILT_SETBITS_4(sc, reg, filt, bits) \
	CSR_WRITE_4((sc), (reg), (CSR_READ_4((sc), (reg)) & (filt)) | (bits))
#define CSR_FILT_SETBITS_2(sc, reg, filt, bits)	\
	CSR_WRITE_2((sc), (reg), (CSR_READ_2((sc), (reg)) & (filt)) | (bits))

#define CSR_CLRBITS_4(sc, reg, bits)	\
	CSR_WRITE_4((sc), (reg), CSR_READ_4((sc), (reg)) & ~(bits))
#define CSR_CLRBITS_2(sc, reg, bits)	\
	CSR_WRITE_2((sc), (reg), CSR_READ_2((sc), (reg)) & ~(bits))

struct bwi_desc32 {
	/* Little endian */
	uint32_t	ctrl;
	uint32_t	addr;	/* BWI_DESC32_A_ */
} __packed;

#define BWI_DESC32_A_FUNC_TXRX		0x1
#define BWI_DESC32_A_FUNC_MASK		__BITS(31, 30)
#define BWI_DESC32_A_ADDR_MASK		__BITS(29, 0)

#define BWI_DESC32_C_BUFLEN_MASK	__BITS(12, 0)
#define BWI_DESC32_C_ADDRHI_MASK	__BITS(17, 16)
#define BWI_DESC32_C_EOR		__BIT(28)
#define BWI_DESC32_C_INTR		__BIT(29)
#define BWI_DESC32_C_FRAME_END		__BIT(30)
#define BWI_DESC32_C_FRAME_START	__BIT(31)

struct bwi_desc64 {
	/* Little endian */
	uint32_t	ctrl0;
	uint32_t	ctrl1;
	uint32_t	addr_lo;
	uint32_t	addr_hi;
} __packed;

struct bwi_rxbuf_hdr {
	/* Little endian */
	uint16_t	rxh_buflen;	/* exclude bwi_rxbuf_hdr */
	uint8_t		rxh_pad1[2];
	uint16_t	rxh_flags1;
	uint8_t		rxh_rssi;
	uint8_t		rxh_sq;
	uint8_t		rxh_pad2[2];
	uint16_t	rxh_flags3;
	uint16_t	rxh_flags2;	/* BWI_RXH_F2_ */
	uint16_t	rxh_tsf;
	uint8_t		rxh_pad3[14];	/* Padded to 30bytes */
} __packed;

#define BWI_RXH_F2_TYPE2FRAME	__BIT(2)

struct bwi_txbuf_hdr {
	/* Little endian */
	uint32_t	txh_mac_ctrl;	/* BWI_TXH_MAC_C_ */
	uint8_t		txh_fc[2];
	uint16_t	txh_unknown1;
	uint16_t	txh_phy_ctrl;	/* BWI_TXH_PHY_C_ */
	uint8_t		txh_ivs[16];
	uint8_t		txh_addr1[IEEE80211_ADDR_LEN];
	uint16_t	txh_unknown2;
	uint8_t		txh_rts_fb_plcp[4];
	uint16_t	txh_rts_fb_duration;
	uint8_t		txh_fb_plcp[4];
	uint16_t	txh_fb_duration;
	uint8_t		txh_pad2[2];
	uint16_t	txh_id;		/* BWI_TXH_ID_ */
	uint16_t	txh_unknown3;
	uint8_t		txh_rts_plcp[6];
	uint8_t		txh_rts_fc[2];
	uint16_t	txh_rts_duration;
	uint8_t		txh_rts_ra[IEEE80211_ADDR_LEN];
	uint8_t		txh_rts_ta[IEEE80211_ADDR_LEN];
	uint8_t		txh_pad3[2];
	uint8_t		txh_plcp[6];
} __packed;

#define BWI_TXH_ID_RING_MASK		__BITS(15, 13)
#define BWI_TXH_ID_IDX_MASK		__BITS(12, 0)

#define BWI_TXH_PHY_C_OFDM		__BIT(0)
#define BWI_TXH_PHY_C_SHPREAMBLE	__BIT(4)
#define BWI_TXH_PHY_C_ANTMODE_MASK	__BITS(9, 8)

#define BWI_TXH_MAC_C_ACK		__BIT(0)
#define BWI_TXH_MAC_C_FIRST_FRAG	__BIT(3)
#define BWI_TXH_MAC_C_HWSEQ		__BIT(4)
#define BWI_TXH_MAC_C_FB_OFDM		__BIT(8)

struct bwi_txstats {
	/* Little endian */
	uint8_t		txs_pad1[4];
	uint16_t	txs_id;
	uint8_t		txs_flags;
	uint8_t		txs_retry_cnt;
	uint8_t		txs_pad2[2];
	uint16_t	txs_seq;
	uint16_t	txs_unknown;
	uint8_t		txs_pad3[2];		/* Padded to 16bytes */
} __packed;

struct bwi_ring_data {
	uint32_t		rdata_txrx_ctrl;
	bus_dmamap_t		rdata_dmap;
	bus_addr_t		rdata_paddr;
	void			*rdata_desc;
};

struct bwi_txbuf {
	struct mbuf		*tb_mbuf;
	bus_dmamap_t		tb_dmap;

	struct ieee80211_node	*tb_ni;
	int			tb_rate_idx[2];
};

struct bwi_txbuf_data {
	struct bwi_txbuf	tbd_buf[BWI_TX_NDESC];
	int			tbd_used;
	int			tbd_idx;
};

struct bwi_rxbuf {
	struct mbuf		*rb_mbuf;
	bus_addr_t		rb_paddr;
	bus_dmamap_t		rb_dmap;
};

struct bwi_rxbuf_data {
	struct bwi_rxbuf	rbd_buf[BWI_RX_NDESC];
	bus_dmamap_t		rbd_tmp_dmap;
	int			rbd_idx;
};

struct bwi_txstats_data {
	bus_dma_tag_t		stats_ring_dtag;
	bus_dmamap_t		stats_ring_dmap;
	bus_addr_t		stats_ring_paddr;
	void			*stats_ring;

	bus_dma_tag_t		stats_dtag;
	bus_dmamap_t		stats_dmap;
	bus_addr_t		stats_paddr;
	struct bwi_txstats	*stats;

	uint32_t		stats_ctrl_base;
	int			stats_idx;
};

struct bwi_fwhdr {
	/* Big endian */
	uint8_t		fw_type;	/* BWI_FW_T_ */
	uint8_t		fw_gen;		/* BWI_FW_GEN */
	uint8_t		fw_pad[2];
	uint32_t	fw_size;
#define fw_iv_cnt	fw_size
} __packed;

#define BWI_FWHDR_SZ		sizeof(struct bwi_fwhdr)

#define BWI_FW_T_UCODE		'u'
#define BWI_FW_T_PCM		'p'
#define BWI_FW_T_IV		'i'

#define BWI_FW_GEN_1		1

#define BWI_FW_VERSION3		3
#define BWI_FW_VERSION4		4
#define BWI_FW_VERSION3_REVMAX	0x128

#define BWI_FW_PATH		"bwi/v%d/"
#define BWI_FW_UCODE_PATH	BWI_FW_PATH "ucode%d.fw"
#define BWI_FW_PCM_PATH		BWI_FW_PATH "pcm%d.fw"
#define BWI_FW_IV_PATH		BWI_FW_PATH "b0g0initvals%d.fw"
#define BWI_FW_IV_EXT_PATH	BWI_FW_PATH "b0g0bsinitvals%d.fw"

struct bwi_fw_iv {
	/* Big endian */
	uint16_t		iv_ofs;
	union {
		uint32_t	val32;
		uint16_t	val16;
	} 			iv_val;
} __packed;

#define BWI_FW_IV_OFS_MASK	__BITS(14, 0)
#define BWI_FW_IV_IS_32BIT	__BIT(15)

enum bwi_clock_mode {
	BWI_CLOCK_MODE_SLOW,
	BWI_CLOCK_MODE_FAST,
	BWI_CLOCK_MODE_DYN
};

struct bwi_regwin {
	uint32_t		rw_flags;	/* BWI_REGWIN_F_ */
	uint16_t		rw_type;	/* BWI_REGWIN_T_ */
	uint8_t			rw_id;
	uint8_t			rw_rev;
};

#define BWI_REGWIN_F_EXIST	0x1

#define BWI_CREATE_REGWIN(rw, id, type, rev)	\
do {						\
	(rw)->rw_flags = BWI_REGWIN_F_EXIST;	\
	(rw)->rw_type = (type);			\
	(rw)->rw_id = (id);			\
	(rw)->rw_rev = (rev);			\
} while (0)

#define BWI_REGWIN_EXIST(rw)	((rw)->rw_flags & BWI_REGWIN_F_EXIST)
#define BWI_GPIO_REGWIN(sc) \
	(BWI_REGWIN_EXIST(&(sc)->sc_com_regwin) ? \
		&(sc)->sc_com_regwin : &(sc)->sc_bus_regwin)

struct bwi_mac;

struct bwi_phy {
	enum ieee80211_phymode	phy_mode;
	int			phy_rev;
	int			phy_version;

	uint32_t		phy_flags;		/* BWI_PHY_F_ */
	uint16_t		phy_tbl_ctrl;
	uint16_t		phy_tbl_data_lo;
	uint16_t		phy_tbl_data_hi;

	void			(*phy_init)(struct bwi_mac *);
};

#define BWI_PHY_F_CALIBRATED	0x1
#define BWI_PHY_F_LINKED	0x2
#define BWI_CLEAR_PHY_FLAGS	(BWI_PHY_F_CALIBRATED)

/* TX power control */
struct bwi_tpctl {
	uint16_t		bbp_atten;	/* BBP attenuation: 4bits */
	uint16_t		rf_atten;	/* RF attenuation */
	uint16_t		tp_ctrl1;	/* ??: 3bits */
	uint16_t		tp_ctrl2;	/* ??: 4bits */
};

#define BWI_RF_ATTEN_FACTOR	4
#define BWI_RF_ATTEN_MAX0	9
#define BWI_RF_ATTEN_MAX1	31
#define BWI_BBP_ATTEN_MAX	11
#define BWI_TPCTL1_MAX		7

struct bwi_rf_lo {
	int8_t			ctrl_lo;
	int8_t			ctrl_hi;
};

struct bwi_rf {
	uint16_t		rf_type;	/* BWI_RF_T_ */
	uint16_t		rf_manu;
	int			rf_rev;

	uint32_t		rf_flags;	/* BWI_RF_F_ */

#define BWI_RFLO_MAX		56
	struct bwi_rf_lo	rf_lo[BWI_RFLO_MAX];
	uint8_t			rf_lo_used[8];

#define BWI_INVALID_NRSSI	-1000
	int16_t			rf_nrssi[2];	/* Narrow RSSI */
	int32_t			rf_nrssi_slope;

#define BWI_NRSSI_TBLSZ		64
	int8_t			rf_nrssi_table[BWI_NRSSI_TBLSZ];

	uint16_t		rf_lo_gain;	/* loopback gain */
	uint16_t		rf_rx_gain;	/* TRSW RX gain */

	uint16_t		rf_calib;	/* RF calibration value */
	u_int			rf_curchan;	/* current channel */

	uint16_t		rf_ctrl_rd;
	int			rf_ctrl_adj;
	void			(*rf_off)(struct bwi_mac *);
	void			(*rf_on)(struct bwi_mac *);

	void			(*rf_set_nrssi_thr)(struct bwi_mac *);
	void			(*rf_calc_nrssi_slope)(struct bwi_mac *);

#define BWI_TSSI_MAX		64
	int8_t			rf_txpower_map0[BWI_TSSI_MAX];
						/* Indexed by TSSI */
	int			rf_idle_tssi0;

	int8_t			rf_txpower_map[BWI_TSSI_MAX];
	int			rf_idle_tssi;

	int			rf_base_tssi;

	int			rf_txpower_max;	/* dBm */

	int			rf_ant_mode;	/* BWI_ANT_MODE_ */
};

#define BWI_RF_F_INITED		0x1
#define BWI_RF_F_ON		0x2
#define BWI_RF_CLEAR_FLAGS	(BWI_RF_F_INITED)

#define BWI_ANT_MODE_0		0
#define BWI_ANT_MODE_1		1
#define BWI_ANT_MODE_UNKN	2
#define BWI_ANT_MODE_AUTO	3

struct bwi_softc;
struct fw_image;

struct bwi_mac {
	struct bwi_regwin	mac_regwin;	/* MUST be first field */
#define mac_rw_flags	mac_regwin.rw_flags
#define mac_type	mac_regwin.rw_type
#define mac_id		mac_regwin.rw_id
#define mac_rev		mac_regwin.rw_rev

	struct bwi_softc	*mac_sc;

	struct bwi_phy		mac_phy;	/* PHY I/F */
	struct bwi_rf		mac_rf;		/* RF I/F */

	struct bwi_tpctl	mac_tpctl;	/* TX power control */
	uint32_t		mac_flags;	/* BWI_MAC_F_ */

	struct fw_image		*mac_ucode;
	struct fw_image		*mac_pcm;
	struct fw_image		*mac_iv;
	struct fw_image		*mac_iv_ext;
};

#define BWI_MAC_F_BSWAP		0x1
#define BWI_MAC_F_TPCTL_INITED	0x2
#define BWI_MAC_F_HAS_TXSTATS	0x4
#define BWI_MAC_F_INITED	0x8
#define BWI_MAC_F_ENABLED	0x10
#define BWI_MAC_F_LOCKED	0x20	/* for debug */
#define BWI_MAC_F_TPCTL_ERROR	0x40

#define BWI_CREATE_MAC(mac, sc, id, rev)	\
do {						\
	BWI_CREATE_REGWIN(&(mac)->mac_regwin,	\
			  (id),			\
			  BWI_REGWIN_T_MAC,	\
			  (rev));		\
	(mac)->mac_sc = (sc);			\
} while (0)

#define BWI_MAC_MAX		2

enum bwi_bus_space {
	BWI_BUS_SPACE_30BIT = 1,
	BWI_BUS_SPACE_32BIT,
	BWI_BUS_SPACE_64BIT
};

struct bwi_softc {
	struct ieee80211com	 sc_ic;
	uint32_t		 sc_flags;	/* BWI_F_ */
	struct device		 sc_dev;

	uint32_t		 sc_cap;	/* BWI_CAP_ */
	uint16_t		 sc_bbp_id;	/* BWI_BBPID_ */
	uint8_t			 sc_bbp_rev;
	uint8_t			 sc_bbp_pkg;

	uint8_t			 sc_pci_revid;
	uint16_t		 sc_pci_subvid;
	uint16_t		 sc_pci_subdid;

	uint16_t		 sc_card_flags;	/* BWI_CARD_F_ */
	uint16_t		 sc_pwron_delay;
	int			 sc_locale;

	int			 sc_irq_rid;
	struct resource		*sc_irq_res;
	void			*sc_irq_handle;

	int			 sc_mem_rid;
	struct resource		*sc_mem_res;
	bus_dma_tag_t		 sc_dmat;
	bus_space_tag_t		 sc_mem_bt;
	bus_space_handle_t	 sc_mem_bh;

	struct timeout		 sc_scan_ch;
	struct timeout		 sc_calib_ch;

	struct bwi_regwin	*sc_cur_regwin;
	struct bwi_regwin	 sc_com_regwin;
	struct bwi_regwin	 sc_bus_regwin;

	int			 sc_nmac;
	struct bwi_mac		 sc_mac[BWI_MAC_MAX];
 
	enum bwi_bus_space	 sc_bus_space;
	bus_dma_tag_t		 sc_parent_dtag;

	bus_dma_tag_t		 sc_buf_dtag;
	struct bwi_txbuf_data	 sc_tx_bdata[BWI_TX_NRING];
	struct bwi_rxbuf_data	 sc_rx_bdata;

	bus_dma_tag_t		 sc_txring_dtag;
	struct bwi_ring_data	 sc_tx_rdata[BWI_TX_NRING];
	bus_dma_tag_t		 sc_rxring_dtag;
	struct bwi_ring_data	 sc_rx_rdata;

	struct bwi_txstats_data	*sc_txstats;

	int			 sc_tx_timer;

	int			 (*sc_newstate)
				 (struct ieee80211com *,
				     enum ieee80211_state, int);

	int			 (*sc_init_tx_ring)(struct bwi_softc *, int);
	void			 (*sc_free_tx_ring)(struct bwi_softc *, int);

	int			 (*sc_init_rx_ring)(struct bwi_softc *);
	void			 (*sc_free_rx_ring)(struct bwi_softc *);

	int			 (*sc_init_txstats)(struct bwi_softc *);
	void			 (*sc_free_txstats)(struct bwi_softc *);

	void			 (*sc_setup_rxdesc)
				 (struct bwi_softc *, int, bus_addr_t, int);
	void			 (*sc_rxeof)(struct bwi_softc *);

	void			 (*sc_setup_txdesc)
				 (struct bwi_softc *, struct bwi_ring_data *,
				     int, bus_addr_t, int);
	void			 (*sc_start_tx)
				 (struct bwi_softc *, uint32_t, int);

	void			 (*sc_txeof_status)(struct bwi_softc *);

	int			 (*sc_enable)(struct bwi_softc *); 
	void			 (*sc_disable)(struct bwi_softc *);

	/* Sysctl variables */
	int			sc_fw_version;	/* BWI_FW_VERSION[34] */
	int			sc_dwell_time;	/* milliseconds */
};

#define BWI_F_BUS_INITED	0x1
#define BWI_F_PROMISC		0x2

uint16_t	bwi_read_sprom(struct bwi_softc *, uint16_t);
int		bwi_regwin_switch(struct bwi_softc *, struct bwi_regwin *,
		    struct bwi_regwin **);
int		bwi_regwin_is_enabled(struct bwi_softc *, struct bwi_regwin *);
void		bwi_regwin_enable(struct bwi_softc *, struct bwi_regwin *,
		    uint32_t);
void		bwi_regwin_disable(struct bwi_softc *, struct bwi_regwin *,
		    uint32_t);
int		bwi_bus_init(struct bwi_softc *, struct bwi_mac *);
uint8_t		bwi_rate2plcp(uint8_t);	/* XXX belongs to 802.11 */

#define abs(a)	__builtin_abs(a)

int		bwi_intr(void *);
int		bwi_attach(struct bwi_softc *);
int		bwi_detach(void *);

/* MAC */

int		bwi_mac_attach(struct bwi_softc *, int, uint8_t);
int		bwi_mac_lateattach(struct bwi_mac *);
void		bwi_mac_detach(struct bwi_mac *);
int		bwi_mac_init(struct bwi_mac *);
void		bwi_mac_reset(struct bwi_mac *, int);
int		bwi_mac_start(struct bwi_mac *);
int		bwi_mac_stop(struct bwi_mac *);
void		bwi_mac_shutdown(struct bwi_mac *);
void		bwi_mac_updateslot(struct bwi_mac *, int);
void		bwi_mac_set_promisc(struct bwi_mac *, int);
void		bwi_mac_calibrate_txpower(struct bwi_mac *);
void		bwi_mac_set_tpctl_11bg(struct bwi_mac *,
		    const struct bwi_tpctl *);
void		bwi_mac_init_tpctl_11bg(struct bwi_mac *);
void		bwi_mac_dummy_xmit(struct bwi_mac *);
void		bwi_mac_reset_hwkeys(struct bwi_mac *);
int		bwi_mac_config_ps(struct bwi_mac *);
uint16_t	bwi_memobj_read_2(struct bwi_mac *, uint16_t, uint16_t);
uint32_t	bwi_memobj_read_4(struct bwi_mac *, uint16_t, uint16_t);
void		bwi_memobj_write_2(struct bwi_mac *, uint16_t, uint16_t,
		    uint16_t);
void		bwi_memobj_write_4(struct bwi_mac *, uint16_t, uint16_t,
		    uint32_t);
void		bwi_tmplt_write_4(struct bwi_mac *, uint32_t, uint32_t);
void		bwi_hostflags_write(struct bwi_mac *, uint64_t);
uint64_t	bwi_hostflags_read(struct bwi_mac *);

#define MOBJ_WRITE_2(mac, objid, ofs, val)	\
	bwi_memobj_write_2((mac), (objid), (ofs), (val))
#define MOBJ_WRITE_4(mac, objid, ofs, val)	\
	bwi_memobj_write_4((mac), (objid), (ofs), (val))
#define MOBJ_READ_2(mac, objid, ofs)		\
	bwi_memobj_read_2((mac), (objid), (ofs))
#define MOBJ_READ_4(mac, objid, ofs)		\
	bwi_memobj_read_4((mac), (objid), (ofs))

#define MOBJ_SETBITS_4(mac, objid, ofs, bits)	\
	MOBJ_WRITE_4((mac), (objid), (ofs),	\
		     MOBJ_READ_4((mac), (objid), (ofs)) | (bits))
#define MOBJ_CLRBITS_4(mac, objid, ofs, bits)	\
	MOBJ_WRITE_4((mac), (objid), (ofs),	\
		     MOBJ_READ_4((mac), (objid), (ofs)) & ~(bits))

#define MOBJ_FILT_SETBITS_2(mac, objid, ofs, filt, bits) \
	MOBJ_WRITE_2((mac), (objid), (ofs),	\
		     (MOBJ_READ_2((mac), (objid), (ofs)) & (filt)) | (bits))

#define TMPLT_WRITE_4(mac, ofs, val)	bwi_tmplt_write_4((mac), (ofs), (val))

#define HFLAGS_WRITE(mac, flags)	bwi_hostflags_write((mac), (flags))
#define HFLAGS_READ(mac)		bwi_hostflags_read((mac))
#define HFLAGS_CLRBITS(mac, bits)	\
	HFLAGS_WRITE((mac), HFLAGS_READ((mac)) | (bits))
#define HFLAGS_SETBITS(mac, bits)	\
	HFLAGS_WRITE((mac), HFLAGS_READ((mac)) & ~(bits))

/* PHY */

struct bwi_gains {
	int16_t	tbl_gain1;
	int16_t	tbl_gain2;
	int16_t	phy_gain;
};

int		bwi_phy_attach(struct bwi_mac *);
void		bwi_phy_clear_state(struct bwi_phy *);
int		bwi_phy_calibrate(struct bwi_mac *);
void		bwi_phy_set_bbp_atten(struct bwi_mac *, uint16_t);
void		bwi_set_gains(struct bwi_mac *, const struct bwi_gains *);
int16_t		bwi_nrssi_read(struct bwi_mac *, uint16_t);
void		bwi_nrssi_write(struct bwi_mac *, uint16_t, int16_t);
uint16_t	bwi_phy_read(struct bwi_mac *, uint16_t);
void		bwi_phy_write(struct bwi_mac *, uint16_t, uint16_t);

static __inline void
bwi_phy_init(struct bwi_mac *_mac)
{
	_mac->mac_phy.phy_init(_mac);
}

#define PHY_WRITE(mac, ctrl, val)	bwi_phy_write((mac), (ctrl), (val))
#define PHY_READ(mac, ctrl)		bwi_phy_read((mac), (ctrl))

#define PHY_SETBITS(mac, ctrl, bits)		\
	PHY_WRITE((mac), (ctrl), PHY_READ((mac), (ctrl)) | (bits))
#define PHY_CLRBITS(mac, ctrl, bits)		\
	PHY_WRITE((mac), (ctrl), PHY_READ((mac), (ctrl)) & ~(bits))
#define PHY_FILT_SETBITS(mac, ctrl, filt, bits)	\
	PHY_WRITE((mac), (ctrl), (PHY_READ((mac), (ctrl)) & (filt)) | (bits))

int		bwi_rf_attach(struct bwi_mac *);
void		bwi_rf_clear_state(struct bwi_rf *);
int		bwi_rf_map_txpower(struct bwi_mac *);
void		bwi_rf_lo_adjust(struct bwi_mac *, const struct bwi_tpctl *);
void		bwi_rf_set_chan(struct bwi_mac *, u_int, int);
void		bwi_rf_get_gains(struct bwi_mac *);
void		bwi_rf_init(struct bwi_mac *);
void		bwi_rf_init_bcm2050(struct bwi_mac *);
void		bwi_rf_lo_update(struct bwi_mac *);
void		bwi_rf_init_hw_nrssi_table(struct bwi_mac *, uint16_t);
void		bwi_rf_set_ant_mode(struct bwi_mac *, int);
void		bwi_rf_clear_tssi(struct bwi_mac *);
int		bwi_rf_get_latest_tssi(struct bwi_mac *, int8_t[], uint16_t);
int		bwi_rf_tssi2dbm(struct bwi_mac *, int8_t, int8_t *);
void		bwi_rf_write(struct bwi_mac *, uint16_t, uint16_t);
uint16_t	bwi_rf_read(struct bwi_mac *, uint16_t);

static __inline void
bwi_rf_off(struct bwi_mac *_mac)
{
	_mac->mac_rf.rf_off(_mac);
	/* TODO:LED */

	_mac->mac_rf.rf_flags &= ~BWI_RF_F_ON;
}

static __inline void
bwi_rf_on(struct bwi_mac *_mac)
{
	if (_mac->mac_rf.rf_flags & BWI_RF_F_ON)
		return;

	_mac->mac_rf.rf_on(_mac);
	/* TODO: LED */

	_mac->mac_rf.rf_flags |= BWI_RF_F_ON;
}

static __inline void
bwi_rf_calc_nrssi_slope(struct bwi_mac *_mac)
{
	_mac->mac_rf.rf_calc_nrssi_slope(_mac);
}

static __inline void
bwi_rf_set_nrssi_thr(struct bwi_mac *_mac)
{
	_mac->mac_rf.rf_set_nrssi_thr(_mac);
}

#define RF_WRITE(mac, ofs, val)		bwi_rf_write((mac), (ofs), (val))
#define RF_READ(mac, ofs)		bwi_rf_read((mac), (ofs))

#define RF_SETBITS(mac, ofs, bits)		\
	RF_WRITE((mac), (ofs), RF_READ((mac), (ofs)) | (bits))
#define RF_CLRBITS(mac, ofs, bits)		\
	RF_WRITE((mac), (ofs), RF_READ((mac), (ofs)) & ~(bits))
#define RF_FILT_SETBITS(mac, ofs, filt, bits)	\
	RF_WRITE((mac), (ofs), (RF_READ((mac), (ofs)) & (filt)) | (bits))

#endif	/* !_IF_BWIVAR_H */
