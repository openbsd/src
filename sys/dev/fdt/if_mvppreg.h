/*	$OpenBSD: if_mvppreg.h,v 1.3 2020/07/22 19:56:42 patrick Exp $	*/
/*
 * Copyright (c) 2008, 2019 Mark Kettenis <kettenis@openbsd.org>
 * Copyright (c) 2017, 2020 Patrick Wildt <patrick@blueri.se>
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
/*
 * Copyright (C) 2016 Marvell International Ltd.
 *
 * Marvell BSD License Option
 *
 * If you received this File from Marvell, you may opt to use, redistribute
 * and/or modify this File under the following licensing terms.
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 *   * Redistributions of source code must retain the above copyright notice,
 *     this list of conditions and the following disclaimer.
 *
 *   * Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 *
 *   * Neither the name of Marvell nor the names of its contributors may be
 *     used to endorse or promote products derived from this software without
 *     specific prior written permission.
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
 */

#ifndef __MVPP2_LIB_HW__
#define __MVPP2_LIB_HW__

#define BIT(nr)		(1U << (nr))

/* PP2v2 registers offsets */
#define MVPP22_SMI_OFFSET                                  0x1200
#define MVPP22_MPCS_OFFSET                                 0x7000
#define MVPP22_MPCS_REG_SIZE                               0x1000
#define MVPP22_XPCS_OFFSET                                 0x7400
#define MVPP22_XPCS_REG_SIZE                               0x1000
#define MVPP22_GMAC_OFFSET                                 0x7e00
#define MVPP22_GMAC_REG_SIZE                               0x1000
#define MVPP22_XLG_OFFSET                                  0x7f00
#define MVPP22_XLG_REG_SIZE                                0x1000
#define MVPP22_RFU1_OFFSET                                 0x318000
#define MVPP22_ADDR_SPACE_SIZE                             0x10000

/* RX Fifo Registers */
#define MVPP2_RX_DATA_FIFO_SIZE_REG(port)                 (0x00 + 4 * (port))
#define MVPP2_RX_ATTR_FIFO_SIZE_REG(port)                 (0x20 + 4 * (port))
#define MVPP2_RX_MIN_PKT_SIZE_REG                         0x60
#define MVPP2_RX_FIFO_INIT_REG                            0x64
#define MVPP22_TX_FIFO_THRESH_REG(port)                  (0x8840 + 4 * (port))
#define MVPP22_TX_FIFO_SIZE_REG(port)                    (0x8860 + 4 * (port))

/* RX DMA Top Registers */
#define MVPP2_RX_CTRL_REG(port)                           (0x140 + 4 * (port))
#define MVPP2_RX_LOW_LATENCY_PKT_SIZE(s)                  (((s) & 0xfff) << 16)
#define MVPP2_RX_USE_PSEUDO_FOR_CSUM_MASK                 BIT(31)
#define MVPP2_POOL_BUF_SIZE_REG(pool)                     (0x180 + 4 * (pool))
#define MVPP2_POOL_BUF_SIZE_OFFSET                        5
#define MVPP2_RXQ_CONFIG_REG(rxq)                         (0x800 + 4 * (rxq))
#define MVPP2_SNOOP_PKT_SIZE_MASK                         0x1ff
#define MVPP2_SNOOP_BUF_HDR_MASK                          BIT(9)
#define MVPP2_RXQ_POOL_SHORT_OFFS                         20
#define MVPP2_RXQ_POOL_SHORT_MASK                         0xf00000
#define MVPP2_RXQ_POOL_LONG_OFFS                          24
#define MVPP2_RXQ_POOL_LONG_MASK                          0xf000000
#define MVPP2_RXQ_PACKET_OFFSET_OFFS                      28
#define MVPP2_RXQ_PACKET_OFFSET_MASK                      0x70000000
#define MVPP2_RXQ_DISABLE_MASK                            BIT(31)

/* Parser Registers */
#define MVPP2_PRS_INIT_LOOKUP_REG                         0x1000
#define MVPP2_PRS_PORT_LU_MAX                             0xf
#define MVPP2_PRS_PORT_LU_MASK(port)                      (0xff << ((port) * 4))
#define MVPP2_PRS_PORT_LU_VAL(port, val)                  ((val) << ((port) * 4))
#define MVPP2_PRS_INIT_OFFS_REG(port)                     (0x1004 + ((port) & 4))
#define MVPP2_PRS_INIT_OFF_MASK(port)                     (0x3f << (((port) % 4) * 8))
#define MVPP2_PRS_INIT_OFF_VAL(port, val)                 ((val) << (((port) % 4) * 8))
#define MVPP2_PRS_MAX_LOOP_REG(port)                      (0x100c + ((port) & 4))
#define MVPP2_PRS_MAX_LOOP_MASK(port)                     (0xff << (((port) % 4) * 8))
#define MVPP2_PRS_MAX_LOOP_VAL(port, val)                 ((val) << (((port) % 4) * 8))
#define MVPP2_PRS_TCAM_IDX_REG                            0x1100
#define MVPP2_PRS_TCAM_DATA_REG(idx)                      (0x1104 + (idx) * 4)
#define MVPP2_PRS_TCAM_INV_MASK                           BIT(31)
#define MVPP2_PRS_SRAM_IDX_REG                            0x1200
#define MVPP2_PRS_SRAM_DATA_REG(idx)                      (0x1204 + (idx) * 4)
#define MVPP2_PRS_TCAM_CTRL_REG                           0x1230
#define MVPP2_PRS_TCAM_EN_MASK                            BIT(0)

/* Classifier Registers */
#define MVPP2_CLS_MODE_REG                                0x1800
#define MVPP2_CLS_MODE_ACTIVE_MASK                        BIT(0)
#define MVPP2_CLS_PORT_WAY_REG                            0x1810
#define MVPP2_CLS_PORT_WAY_MASK(port)                     (1 << (port))
#define MVPP2_CLS_LKP_INDEX_REG                           0x1814
#define MVPP2_CLS_LKP_INDEX_WAY_OFFS                      6
#define MVPP2_CLS_LKP_TBL_REG                             0x1818
#define MVPP2_CLS_LKP_TBL_RXQ_MASK                        0xff
#define MVPP2_CLS_LKP_TBL_LOOKUP_EN_MASK                  BIT(25)
#define MVPP2_CLS_FLOW_INDEX_REG                          0x1820
#define MVPP2_CLS_FLOW_TBL0_REG                           0x1824
#define MVPP2_CLS_FLOW_TBL1_REG                           0x1828
#define MVPP2_CLS_FLOW_TBL2_REG                           0x182c
#define MVPP2_CLS_OVERSIZE_RXQ_LOW_REG(port)              (0x1980 + ((port) * 4))
#define MVPP2_CLS_OVERSIZE_RXQ_LOW_BITS                   3
#define MVPP2_CLS_OVERSIZE_RXQ_LOW_MASK                   0x7
#define MVPP2_CLS_SWFWD_P2HQ_REG(port)                    (0x19b0 + ((port) * 4))
#define MVPP2_CLS_SWFWD_PCTRL_REG                         0x19d0
#define MVPP2_CLS_SWFWD_PCTRL_MASK(port)                  (1 << (port))

/* Descriptor Manager Top Registers */
#define MVPP2_RXQ_NUM_REG                                 0x2040
#define MVPP2_RXQ_DESC_ADDR_REG                           0x2044
#define MVPP2_RXQ_DESC_SIZE_REG                           0x2048
#define MVPP2_RXQ_DESC_SIZE_MASK                          0x3ff0
#define MVPP2_RXQ_STATUS_UPDATE_REG(rxq)                  (0x3000 + 4 * (rxq))
#define MVPP2_RXQ_NUM_PROCESSED_OFFSET                    0
#define MVPP2_RXQ_NUM_NEW_OFFSET                          16
#define MVPP2_RXQ_STATUS_REG(rxq)                         (0x3400 + 4 * (rxq))
#define MVPP2_RXQ_OCCUPIED_MASK                           0x3fff
#define MVPP2_RXQ_NON_OCCUPIED_OFFSET                     16
#define MVPP2_RXQ_NON_OCCUPIED_MASK                       0x3fff0000
#define MVPP2_RXQ_THRESH_REG                              0x204c
#define MVPP2_OCCUPIED_THRESH_OFFSET                      0
#define MVPP2_OCCUPIED_THRESH_MASK                        0x3fff
#define MVPP2_RXQ_INDEX_REG                               0x2050
#define MVPP2_TXQ_NUM_REG                                 0x2080
#define MVPP2_TXQ_DESC_ADDR_REG                           0x2084
#define MVPP22_TXQ_DESC_ADDR_HIGH_REG                     0x20a8
#define MVPP22_TXQ_DESC_ADDR_HIGH_MASK                    0xff
#define MVPP2_TXQ_DESC_SIZE_REG                           0x2088
#define MVPP2_TXQ_DESC_SIZE_MASK                          0x3ff0
#define MVPP2_AGGR_TXQ_UPDATE_REG                         0x2090
#define MVPP2_TXQ_THRESH_REG                              0x2094
#define MVPP2_TRANSMITTED_THRESH_OFFSET                   16
#define MVPP2_TRANSMITTED_THRESH_MASK                     0x3fff
#define MVPP2_TXQ_INDEX_REG                               0x2098
#define MVPP2_TXQ_PREF_BUF_REG                            0x209c
#define MVPP2_PREF_BUF_PTR(desc)                          ((desc) & 0xfff)
#define MVPP2_PREF_BUF_SIZE_4                             (BIT(12) | BIT(13))
#define MVPP2_PREF_BUF_SIZE_16                            (BIT(12) | BIT(14))
#define MVPP2_PREF_BUF_THRESH(val)                        ((val) << 17)
#define MVPP2_TXQ_DRAIN_EN_MASK                           BIT(31)
#define MVPP2_TXQ_PENDING_REG                             0x20a0
#define MVPP2_TXQ_PENDING_MASK                            0x3fff
#define MVPP2_TXQ_INT_STATUS_REG                          0x20a4
#define MVPP2_TXQ_SENT_REG(txq)                           (0x3c00 + 4 * (txq))
#define MVPP22_TXQ_SENT_REG(txq)                          (0x3e00 + 4 * (txq-128))
#define MVPP2_TRANSMITTED_COUNT_OFFSET                    16
#define MVPP2_TRANSMITTED_COUNT_MASK                      0x3fff0000
#define MVPP2_TXQ_RSVD_REQ_REG                            0x20b0
#define MVPP2_TXQ_RSVD_REQ_Q_OFFSET                       16
#define MVPP2_TXQ_RSVD_RSLT_REG                           0x20b4
#define MVPP2_TXQ_RSVD_RSLT_MASK                          0x3fff
#define MVPP2_TXQ_RSVD_CLR_REG                            0x20b8
#define MVPP2_TXQ_RSVD_CLR_OFFSET                         16
#define MVPP2_AGGR_TXQ_DESC_ADDR_REG(cpu)                 (0x2100 + 4 * (cpu))
#define MVPP2_AGGR_TXQ_DESC_SIZE_REG(cpu)                 (0x2140 + 4 * (cpu))
#define MVPP2_AGGR_TXQ_DESC_SIZE_MASK                     0x3ff0
#define MVPP2_AGGR_TXQ_STATUS_REG(cpu)                    (0x2180 + 4 * (cpu))
#define MVPP2_AGGR_TXQ_PENDING_MASK                       0x3fff
#define MVPP2_AGGR_TXQ_INDEX_REG(cpu)                     (0x21c0 + 4 * (cpu))

/* MBUS bridge registers */
#define MVPP2_WIN_BASE(w)                                 (0x4000 + ((w) << 2))
#define MVPP2_WIN_SIZE(w)                                 (0x4020 + ((w) << 2))
#define MVPP2_WIN_REMAP(w)                                (0x4040 + ((w) << 2))
#define MVPP2_BASE_ADDR_ENABLE                            0x4060

/* Interrupt Cause and Mask registers */
#define MVPP2_ISR_TX_THRESHOLD_REG(port)                  (0x5140 + 4 * (port))
#define MVPP2_ISR_RX_THRESHOLD_REG(rxq)                   (0x5200 + 4 * (rxq))
#define MVPP2_ISR_RXQ_GROUP_REG(rxq)                      (0x5400 + 4 * (rxq))
#define MVPP2_ISR_RXQ_GROUP_INDEX_REG                     0x5400
#define MVPP2_ISR_RXQ_GROUP_INDEX_GROUP_SHIFT             7
#define MVPP2_ISR_RXQ_SUB_GROUP_CONFIG_REG                0x5404
#define MVPP2_ISR_RXQ_SUB_GROUP_CONFIG_SIZE_SHIFT         8
#define MVPP2_ISR_ENABLE_REG(port)                        (0x5420 + 4 * (port))
#define MVPP2_ISR_ENABLE_INTERRUPT(mask)                  ((mask) & 0xffff)
#define MVPP2_ISR_DISABLE_INTERRUPT(mask)                 (((mask) << 16) & 0xffff0000)
#define MVPP2_ISR_RX_TX_CAUSE_REG(port)                   (0x5480 + 4 * (port))
#define MVPP2_CAUSE_RXQ_OCCUP_DESC_ALL_MASK               0xff
#define MVPP2_CAUSE_TXQ_OCCUP_DESC_ALL_MASK               0xff0000
#define MVPP2_CAUSE_TXQ_OCCUP_DESC_ALL_OFFSET             16
#define MVPP2_CAUSE_RX_FIFO_OVERRUN_MASK                  BIT(24)
#define MVPP2_CAUSE_FCS_ERR_MASK                          BIT(25)
#define MVPP2_CAUSE_TX_FIFO_UNDERRUN_MASK                 BIT(26)
#define MVPP2_CAUSE_TX_EXCEPTION_SUM_MASK                 BIT(29)
#define MVPP2_CAUSE_RX_EXCEPTION_SUM_MASK                 BIT(30)
#define MVPP2_CAUSE_MISC_SUM_MASK                         BIT(31)
#define MVPP2_ISR_RX_TX_MASK_REG(port)                    (0x54a0 + 4 * (port))
#define MVPP2_ISR_PON_RX_TX_MASK_REG                      0x54bc
#define MVPP2_PON_CAUSE_RXQ_OCCUP_DESC_ALL_MASK           0xffff
#define MVPP2_PON_CAUSE_TXP_OCCUP_DESC_ALL_MASK           0x3fc00000
#define MVPP2_PON_CAUSE_MISC_SUM_MASK                     BIT(31)
#define MVPP2_ISR_MISC_CAUSE_REG                          0x55b0

/* Buffer Manager registers */
#define MVPP2_BM_POOL_BASE_REG(pool)                      (0x6000 + ((pool) * 4))
#define MVPP2_BM_POOL_BASE_ADDR_MASK                      0xfffff80
#define MVPP2_BM_POOL_SIZE_REG(pool)                      (0x6040 + ((pool) * 4))
#define MVPP2_BM_POOL_SIZE_MASK                           0xfff0
#define MVPP2_BM_POOL_READ_PTR_REG(pool)                  (0x6080 + ((pool) * 4))
#define MVPP2_BM_POOL_GET_READ_PTR_MASK                   0xfff0
#define MVPP2_BM_POOL_PTRS_NUM_REG(pool)                  (0x60c0 + ((pool) * 4))
#define MVPP2_BM_POOL_PTRS_NUM_MASK                       0xfff8
#define MVPP2_BM_BPPI_READ_PTR_REG(pool)                  (0x6100 + ((pool) * 4))
#define MVPP2_BM_BPPI_PTRS_NUM_REG(pool)                  (0x6140 + ((pool) * 4))
#define MVPP2_BM_BPPI_PTR_NUM_MASK                        0x7ff
#define MVPP2_BM_BPPI_PREFETCH_FULL_MASK                  BIT(16)
#define MVPP2_BM_POOL_CTRL_REG(pool)                      (0x6200 + ((pool) * 4))
#define MVPP2_BM_START_MASK                               BIT(0)
#define MVPP2_BM_STOP_MASK                                BIT(1)
#define MVPP2_BM_STATE_MASK                               BIT(4)
#define MVPP2_BM_LOW_THRESH_OFFS                          8
#define MVPP2_BM_LOW_THRESH_MASK                          0x7f00
#define MVPP2_BM_LOW_THRESH_VALUE(val)                    ((val) << MVPP2_BM_LOW_THRESH_OFFS)
#define MVPP2_BM_HIGH_THRESH_OFFS                         16
#define MVPP2_BM_HIGH_THRESH_MASK                         0x7f0000
#define MVPP2_BM_HIGH_THRESH_VALUE(val)                   ((val) << MVPP2_BM_HIGH_THRESH_OFFS)
#define MVPP2_BM_INTR_CAUSE_REG(pool)                     (0x6240 + ((pool) * 4))
#define MVPP2_BM_RELEASED_DELAY_MASK                      BIT(0)
#define MVPP2_BM_ALLOC_FAILED_MASK                        BIT(1)
#define MVPP2_BM_BPPE_EMPTY_MASK                          BIT(2)
#define MVPP2_BM_BPPE_FULL_MASK                           BIT(3)
#define MVPP2_BM_AVAILABLE_BP_LOW_MASK                    BIT(4)
#define MVPP2_BM_INTR_MASK_REG(pool)                      (0x6280 + ((pool) * 4))
#define MVPP2_BM_PHY_ALLOC_REG(pool)                      (0x6400 + ((pool) * 4))
#define MVPP2_BM_PHY_ALLOC_GRNTD_MASK                     BIT(0)
#define MVPP2_BM_VIRT_ALLOC_REG                           0x6440
#define MVPP2_BM_PHY_RLS_REG(pool)                        (0x6480 + ((pool) * 4))
#define MVPP2_BM_PHY_RLS_MC_BUFF_MASK                     BIT(0)
#define MVPP2_BM_PHY_RLS_PRIO_EN_MASK                     BIT(1)
#define MVPP2_BM_PHY_RLS_GRNTD_MASK                       BIT(2)
#define MVPP2_BM_VIRT_RLS_REG                             0x64c0
#define MVPP2_BM_MC_RLS_REG                               0x64c4
#define MVPP2_BM_MC_ID_MASK                               0xfff
#define MVPP2_BM_FORCE_RELEASE_MASK                       BIT(12)

#define MVPP22_BM_PHY_VIRT_HIGH_ALLOC_REG                 0x6444
#define MVPP22_BM_PHY_HIGH_ALLOC_OFFSET                   0
#define MVPP22_BM_VIRT_HIGH_ALLOC_OFFSET                  8
#define MVPP22_BM_VIRT_HIGH_ALLOC_MASK                    0xff00

#define MVPP22_BM_PHY_VIRT_HIGH_RLS_REG                   0x64c4

#define MVPP22_BM_PHY_HIGH_RLS_OFFSET                     0
#define MVPP22_BM_VIRT_HIGH_RLS_OFFST                     8

#define MVPP22_BM_POOL_BASE_HIGH_REG                      0x6310
#define MVPP22_BM_POOL_BASE_HIGH_MASK                     0xff
#define MVPP2_BM_PRIO_CTRL_REG                            0x6800

/* TX Scheduler registers */
#define MVPP2_TXP_SCHED_PORT_INDEX_REG                    0x8000
#define MVPP2_TXP_SCHED_Q_CMD_REG                         0x8004
#define MVPP2_TXP_SCHED_ENQ_MASK                          0xff
#define MVPP2_TXP_SCHED_DISQ_OFFSET                       8
#define MVPP2_TXP_SCHED_CMD_1_REG                         0x8010
#define MVPP2_TXP_SCHED_PERIOD_REG                        0x8018
#define MVPP2_TXP_SCHED_MTU_REG                           0x801c
#define MVPP2_TXP_MTU_MAX                                 0x7FFFF
#define MVPP2_TXP_SCHED_REFILL_REG                        0x8020
#define MVPP2_TXP_REFILL_TOKENS_ALL_MASK                  0x7ffff
#define MVPP2_TXP_REFILL_PERIOD_ALL_MASK                  0x3ff00000
#define MVPP2_TXP_REFILL_PERIOD_MASK(v)                   ((v) << 20)
#define MVPP2_TXP_SCHED_TOKEN_SIZE_REG                    0x8024
#define MVPP2_TXP_TOKEN_SIZE_MAX                          0xffffffff
#define MVPP2_TXQ_SCHED_REFILL_REG(q)                     (0x8040 + ((q) << 2))
#define MVPP2_TXQ_REFILL_TOKENS_ALL_MASK                  0x7ffff
#define MVPP2_TXQ_REFILL_PERIOD_ALL_MASK                  0x3ff00000
#define MVPP2_TXQ_REFILL_PERIOD_MASK(v)                   ((v) << 20)
#define MVPP2_TXQ_SCHED_TOKEN_SIZE_REG(q)                 (0x8060 + ((q) << 2))
#define MVPP2_TXQ_TOKEN_SIZE_MAX                          0x7fffffff
#define MVPP2_TXQ_SCHED_TOKEN_CNTR_REG(q)                 (0x8080 + ((q) << 2))
#define MVPP2_TXQ_TOKEN_CNTR_MAX                          0xffffffff

/* TX general registers */
#define MVPP2_TX_SNOOP_REG                                0x8800
#define MVPP2_TX_PORT_FLUSH_REG                           0x8810
#define MVPP2_TX_PORT_FLUSH_MASK(port)                    (1 << (port))

/* LMS registers */
#define MVPP2_SRC_ADDR_MIDDLE                             0x24
#define MVPP2_SRC_ADDR_HIGH                               0x28
#define MVPP2_PHY_AN_CFG0_REG                             0x34
#define MVPP2_PHY_AN_STOP_SMI0_MASK                       BIT(7)
#define MVPP2_MIB_COUNTERS_BASE(port)                     (0x1000 + ((port) >> 1) * 0x400 + (port) * 0x400)
#define MVPP2_MIB_LATE_COLLISION                          0x7c
#define MVPP2_ISR_SUM_MASK_REG                            0x220c
#define MVPP2_MNG_EXTENDED_GLOBAL_CTRL_REG                0x305c
#define MVPP2_EXT_GLOBAL_CTRL_DEFAULT                     0x27

/* Per-port registers */
#define MVPP2_GMAC_CTRL_0_REG                             0x0
#define MVPP2_GMAC_PORT_EN_MASK                           BIT(0)
#define MVPP2_GMAC_PORT_TYPE_MASK                         BIT(1)
#define MVPP2_GMAC_MAX_RX_SIZE_OFFS                       2
#define MVPP2_GMAC_MAX_RX_SIZE_MASK                       0x7ffc
#define MVPP2_GMAC_MIB_CNTR_EN_MASK                       BIT(15)
#define MVPP2_GMAC_CTRL_1_REG                             0x4
#define MVPP2_GMAC_PERIODIC_XON_EN_MASK                   BIT(1)
#define MVPP2_GMAC_GMII_LB_EN_MASK                        BIT(5)
#define MVPP2_GMAC_PCS_LB_EN_BIT                          6
#define MVPP2_GMAC_PCS_LB_EN_MASK                         BIT(6)
#define MVPP2_GMAC_SA_LOW_OFFS                            7
#define MVPP2_GMAC_CTRL_2_REG                             0x8
#define MVPP2_GMAC_INBAND_AN_MASK                         BIT(0)
#define MVPP2_GMAC_PCS_ENABLE_MASK                        BIT(3)
#define MVPP2_GMAC_PORT_RGMII_MASK                        BIT(4)
#define MVPP2_GMAC_PORT_RESET_MASK                        BIT(6)
#define MVPP2_GMAC_AUTONEG_CONFIG                         0xc
#define MVPP2_GMAC_FORCE_LINK_DOWN                        BIT(0)
#define MVPP2_GMAC_FORCE_LINK_PASS                        BIT(1)
#define MVPP2_GMAC_IN_BAND_AUTONEG                        BIT(2)
#define MVPP2_GMAC_IN_BAND_AUTONEG_BYPASS                 BIT(3)
#define MVPP2_GMAC_IN_BAND_RESTART_AN                     BIT(4)
#define MVPP2_GMAC_CONFIG_MII_SPEED                       BIT(5)
#define MVPP2_GMAC_CONFIG_GMII_SPEED                      BIT(6)
#define MVPP2_GMAC_AN_SPEED_EN                            BIT(7)
#define MVPP2_GMAC_FC_ADV_EN                              BIT(9)
#define MVPP2_GMAC_FC_ADV_ASM_EN                          BIT(10)
#define MVPP2_GMAC_FLOW_CTRL_AUTONEG                      BIT(11)
#define MVPP2_GMAC_CONFIG_FULL_DUPLEX                     BIT(12)
#define MVPP2_GMAC_AN_DUPLEX_EN                           BIT(13)
#define MVPP2_GMAC_PORT_FIFO_CFG_1_REG                    0x1c
#define MVPP2_GMAC_TX_FIFO_MIN_TH_OFFS                    6
#define MVPP2_GMAC_TX_FIFO_MIN_TH_ALL_MASK                0x1fc0
#define MVPP2_GMAC_TX_FIFO_MIN_TH_MASK(v)                 (((v) << 6) & MVPP2_GMAC_TX_FIFO_MIN_TH_ALL_MASK)

/* Port Interrupts */
#define MV_GMAC_INTERRUPT_CAUSE_REG                       (0x0020)
#define MV_GMAC_INTERRUPT_MASK_REG                        (0x0024)
#define MV_GMAC_INTERRUPT_CAUSE_LINK_CHANGE_OFFS          1
#define MV_GMAC_INTERRUPT_CAUSE_LINK_CHANGE_MASK          (0x1 << MV_GMAC_INTERRUPT_CAUSE_LINK_CHANGE_OFFS)

/* Port Interrupt Summary */
#define MV_GMAC_INTERRUPT_SUM_CAUSE_REG                   (0x00A0)
#define MV_GMAC_INTERRUPT_SUM_MASK_REG                    (0x00A4)
#define MV_GMAC_INTERRUPT_SUM_CAUSE_LINK_CHANGE_OFFS      1
#define MV_GMAC_INTERRUPT_SUM_CAUSE_LINK_CHANGE_MASK      (0x1 << MV_GMAC_INTERRUPT_SUM_CAUSE_LINK_CHANGE_OFFS)

/* Port Mac Control0 */
#define MVPP2_PORT_CTRL0_REG                              (0x0000)
#define MVPP2_PORT_CTRL0_PORTEN_OFFS    0
#define MVPP2_PORT_CTRL0_PORTEN_MASK    \
    (0x00000001 << MVPP2_PORT_CTRL0_PORTEN_OFFS)

#define MVPP2_PORT_CTRL0_PORTTYPE_OFFS    1
#define MVPP2_PORT_CTRL0_PORTTYPE_MASK    \
    (0x00000001 << MVPP2_PORT_CTRL0_PORTTYPE_OFFS)

#define MVPP2_PORT_CTRL0_FRAMESIZELIMIT_OFFS    2
#define MVPP2_PORT_CTRL0_FRAMESIZELIMIT_MASK    \
    (0x00001fff << MVPP2_PORT_CTRL0_FRAMESIZELIMIT_OFFS)

#define MVPP2_PORT_CTRL0_COUNT_EN_OFFS    15
#define MVPP2_PORT_CTRL0_COUNT_EN_MASK    \
    (0x00000001 << MVPP2_PORT_CTRL0_COUNT_EN_OFFS)

/* Port Mac Control1 */
#define MVPP2_PORT_CTRL1_REG                              (0x0004)
#define MVPP2_PORT_CTRL1_EN_RX_CRC_CHECK_OFFS    0
#define MVPP2_PORT_CTRL1_EN_RX_CRC_CHECK_MASK    \
    (0x00000001 << MVPP2_PORT_CTRL1_EN_RX_CRC_CHECK_OFFS)

#define MVPP2_PORT_CTRL1_EN_PERIODIC_FC_XON_OFFS    1
#define MVPP2_PORT_CTRL1_EN_PERIODIC_FC_XON_MASK    \
    (0x00000001 << MVPP2_PORT_CTRL1_EN_PERIODIC_FC_XON_OFFS)

#define MVPP2_PORT_CTRL1_MGMII_MODE_OFFS    2
#define MVPP2_PORT_CTRL1_MGMII_MODE_MASK    \
    (0x00000001 << MVPP2_PORT_CTRL1_MGMII_MODE_OFFS)

#define MVPP2_PORT_CTRL1_PFC_CASCADE_PORT_ENABLE_OFFS   3
#define MVPP2_PORT_CTRL1_PFC_CASCADE_PORT_ENABLE_MASK    \
    (0x00000001 << MVPP2_PORT_CTRL1_PFC_CASCADE_PORT_ENABLE_OFFS)

#define MVPP2_PORT_CTRL1_DIS_EXCESSIVE_COL_OFFS   4
#define MVPP2_PORT_CTRL1_DIS_EXCESSIVE_COL_MASK    \
    (0x00000001 << MVPP2_PORT_CTRL1_DIS_EXCESSIVE_COL_OFFS)

#define MVPP2_PORT_CTRL1_GMII_LOOPBACK_OFFS   5
#define MVPP2_PORT_CTRL1_GMII_LOOPBACK_MASK    \
    (0x00000001 << MVPP2_PORT_CTRL1_GMII_LOOPBACK_OFFS)

#define MVPP2_PORT_CTRL1_PCS_LOOPBACK_OFFS    6
#define MVPP2_PORT_CTRL1_PCS_LOOPBACK_MASK    \
    (0x00000001 << MVPP2_PORT_CTRL1_PCS_LOOPBACK_OFFS)

#define MVPP2_PORT_CTRL1_FC_SA_ADDR_LO_OFFS   7
#define MVPP2_PORT_CTRL1_FC_SA_ADDR_LO_MASK    \
    (0x000000ff << MVPP2_PORT_CTRL1_FC_SA_ADDR_LO_OFFS)

#define MVPP2_PORT_CTRL1_EN_SHORT_PREAMBLE_OFFS   15
#define MVPP2_PORT_CTRL1_EN_SHORT_PREAMBLE_MASK    \
    (0x00000001 << MVPP2_PORT_CTRL1_EN_SHORT_PREAMBLE_OFFS)

/* Port Mac Control2 */
#define MVPP2_PORT_CTRL2_REG                              (0x0008)
#define MVPP2_PORT_CTRL2_SGMII_MODE_OFFS    0
#define MVPP2_PORT_CTRL2_SGMII_MODE_MASK    \
    (0x00000001 << MVPP2_PORT_CTRL2_SGMII_MODE_OFFS)

#define MVPP2_PORT_CTRL2_FC_MODE_OFFS   1
#define MVPP2_PORT_CTRL2_FC_MODE_MASK    \
    (0x00000003 << MVPP2_PORT_CTRL2_FC_MODE_OFFS)

#define MVPP2_PORT_CTRL2_PCS_EN_OFFS    3
#define MVPP2_PORT_CTRL2_PCS_EN_MASK    \
    (0x00000001 << MVPP2_PORT_CTRL2_PCS_EN_OFFS)

#define MVPP2_PORT_CTRL2_RGMII_MODE_OFFS    4
#define MVPP2_PORT_CTRL2_RGMII_MODE_MASK    \
    (0x00000001 << MVPP2_PORT_CTRL2_RGMII_MODE_OFFS)

#define MVPP2_PORT_CTRL2_DIS_PADING_OFFS    5
#define MVPP2_PORT_CTRL2_DIS_PADING_MASK    \
    (0x00000001 << MVPP2_PORT_CTRL2_DIS_PADING_OFFS)

#define MVPP2_PORT_CTRL2_PORTMACRESET_OFFS    6
#define MVPP2_PORT_CTRL2_PORTMACRESET_MASK    \
    (0x00000001 << MVPP2_PORT_CTRL2_PORTMACRESET_OFFS)

#define MVPP2_PORT_CTRL2_TX_DRAIN_OFFS    7
#define MVPP2_PORT_CTRL2_TX_DRAIN_MASK    \
    (0x00000001 << MVPP2_PORT_CTRL2_TX_DRAIN_OFFS)

#define MVPP2_PORT_CTRL2_EN_MII_ODD_PRE_OFFS    8
#define MVPP2_PORT_CTRL2_EN_MII_ODD_PRE_MASK    \
    (0x00000001 << MVPP2_PORT_CTRL2_EN_MII_ODD_PRE_OFFS)

#define MVPP2_PORT_CTRL2_CLK_125_BYPS_EN_OFFS   9
#define MVPP2_PORT_CTRL2_CLK_125_BYPS_EN_MASK    \
    (0x00000001 << MVPP2_PORT_CTRL2_CLK_125_BYPS_EN_OFFS)

#define MVPP2_PORT_CTRL2_PRBS_CHECK_EN_OFFS   10
#define MVPP2_PORT_CTRL2_PRBS_CHECK_EN_MASK    \
    (0x00000001 << MVPP2_PORT_CTRL2_PRBS_CHECK_EN_OFFS)

#define MVPP2_PORT_CTRL2_PRBS_GEN_EN_OFFS   11
#define MVPP2_PORT_CTRL2_PRBS_GEN_EN_MASK    \
    (0x00000001 << MVPP2_PORT_CTRL2_PRBS_GEN_EN_OFFS)

#define MVPP2_PORT_CTRL2_SELECT_DATA_TO_TX_OFFS   12
#define MVPP2_PORT_CTRL2_SELECT_DATA_TO_TX_MASK    \
    (0x00000003 << MVPP2_PORT_CTRL2_SELECT_DATA_TO_TX_OFFS)

#define MVPP2_PORT_CTRL2_EN_COL_ON_BP_OFFS    14
#define MVPP2_PORT_CTRL2_EN_COL_ON_BP_MASK    \
    (0x00000001 << MVPP2_PORT_CTRL2_EN_COL_ON_BP_OFFS)

#define MVPP2_PORT_CTRL2_EARLY_REJECT_MODE_OFFS   15
#define MVPP2_PORT_CTRL2_EARLY_REJECT_MODE_MASK    \
    (0x00000001 << MVPP2_PORT_CTRL2_EARLY_REJECT_MODE_OFFS)

/* Port Auto-negotiation Configuration */
#define MVPP2_PORT_AUTO_NEG_CFG_REG                       (0x000c)
#define MVPP2_PORT_AUTO_NEG_CFG_FORCE_LINK_DOWN_OFFS    0
#define MVPP2_PORT_AUTO_NEG_CFG_FORCE_LINK_DOWN_MASK    \
    (0x00000001 << MVPP2_PORT_AUTO_NEG_CFG_FORCE_LINK_DOWN_OFFS)

#define MVPP2_PORT_AUTO_NEG_CFG_FORCE_LINK_UP_OFFS    1
#define MVPP2_PORT_AUTO_NEG_CFG_FORCE_LINK_UP_MASK    \
    (0x00000001 << MVPP2_PORT_AUTO_NEG_CFG_FORCE_LINK_UP_OFFS)

#define MVPP2_PORT_AUTO_NEG_CFG_EN_PCS_AN_OFFS    2
#define MVPP2_PORT_AUTO_NEG_CFG_EN_PCS_AN_MASK    \
    (0x00000001 << MVPP2_PORT_AUTO_NEG_CFG_EN_PCS_AN_OFFS)

#define MVPP2_PORT_AUTO_NEG_CFG_AN_BYPASS_EN_OFFS    3
#define MVPP2_PORT_AUTO_NEG_CFG_AN_BYPASS_EN_MASK    \
    (0x00000001 << MVPP2_PORT_AUTO_NEG_CFG_AN_BYPASS_EN_OFFS)

#define MVPP2_PORT_AUTO_NEG_CFG_INBAND_RESTARTAN_OFFS    4
#define MVPP2_PORT_AUTO_NEG_CFG_INBAND_RESTARTAN_MASK    \
    (0x00000001 << MVPP2_PORT_AUTO_NEG_CFG_INBAND_RESTARTAN_OFFS)

#define MVPP2_PORT_AUTO_NEG_CFG_SET_MII_SPEED_OFFS    5
#define MVPP2_PORT_AUTO_NEG_CFG_SET_MII_SPEED_MASK    \
    (0x00000001 << MVPP2_PORT_AUTO_NEG_CFG_SET_MII_SPEED_OFFS)

#define MVPP2_PORT_AUTO_NEG_CFG_SET_GMII_SPEED_OFFS   6
#define MVPP2_PORT_AUTO_NEG_CFG_SET_GMII_SPEED_MASK    \
    (0x00000001 << MVPP2_PORT_AUTO_NEG_CFG_SET_GMII_SPEED_OFFS)

#define MVPP2_PORT_AUTO_NEG_CFG_EN_AN_SPEED_OFFS    7
#define MVPP2_PORT_AUTO_NEG_CFG_EN_AN_SPEED_MASK    \
    (0x00000001 << MVPP2_PORT_AUTO_NEG_CFG_EN_AN_SPEED_OFFS)

#define MVPP2_PORT_AUTO_NEG_CFG_ADV_PAUSE_OFFS    9
#define MVPP2_PORT_AUTO_NEG_CFG_ADV_PAUSE_MASK    \
    (0x00000001 << MVPP2_PORT_AUTO_NEG_CFG_ADV_PAUSE_OFFS)

#define MVPP2_PORT_AUTO_NEG_CFG_ADV_ASM_PAUSE_OFFS    10
#define MVPP2_PORT_AUTO_NEG_CFG_ADV_ASM_PAUSE_MASK    \
    (0x00000001 << MVPP2_PORT_AUTO_NEG_CFG_ADV_ASM_PAUSE_OFFS)

#define MVPP2_PORT_AUTO_NEG_CFG_EN_FC_AN_OFFS    11
#define MVPP2_PORT_AUTO_NEG_CFG_EN_FC_AN_MASK    \
    (0x00000001 << MVPP2_PORT_AUTO_NEG_CFG_EN_FC_AN_OFFS)

#define MVPP2_PORT_AUTO_NEG_CFG_SET_FULL_DX_OFFS    12
#define MVPP2_PORT_AUTO_NEG_CFG_SET_FULL_DX_MASK    \
    (0x00000001 << MVPP2_PORT_AUTO_NEG_CFG_SET_FULL_DX_OFFS)

#define MVPP2_PORT_AUTO_NEG_CFG_EN_FDX_AN_OFFS    13
#define MVPP2_PORT_AUTO_NEG_CFG_EN_FDX_AN_MASK    \
    (0x00000001 << MVPP2_PORT_AUTO_NEG_CFG_EN_FDX_AN_OFFS)

#define MVPP2_PORT_AUTO_NEG_CFG_PHY_MODE_OFFS    14
#define MVPP2_PORT_AUTO_NEG_CFG_PHY_MODE_MASK    \
    (0x00000001 << MVPP2_PORT_AUTO_NEG_CFG_PHY_MODE_OFFS)

#define MVPP2_PORT_AUTO_NEG_CFG_CHOOSE_SAMPLE_TX_CONFIG_OFFS    15
#define MVPP2_PORT_AUTO_NEG_CFG_CHOOSE_SAMPLE_TX_CONFIG_MASK    \
    (0x00000001 << MVPP2_PORT_AUTO_NEG_CFG_CHOOSE_SAMPLE_TX_CONFIG_OFFS)

/* Port Status0 */
#define MVPP2_PORT_STATUS0_REG                            (0x0010)
#define MVPP2_PORT_STATUS0_LINKUP_OFFS    0
#define MVPP2_PORT_STATUS0_LINKUP_MASK    \
    (0x00000001 << MVPP2_PORT_STATUS0_LINKUP_OFFS)

#define MVPP2_PORT_STATUS0_GMIISPEED_OFFS    1
#define MVPP2_PORT_STATUS0_GMIISPEED_MASK    \
    (0x00000001 << MVPP2_PORT_STATUS0_GMIISPEED_OFFS)

#define MVPP2_PORT_STATUS0_MIISPEED_OFFS    2
#define MVPP2_PORT_STATUS0_MIISPEED_MASK    \
    (0x00000001 << MVPP2_PORT_STATUS0_MIISPEED_OFFS)

#define MVPP2_PORT_STATUS0_FULLDX_OFFS    3
#define MVPP2_PORT_STATUS0_FULLDX_MASK    \
    (0x00000001 << MVPP2_PORT_STATUS0_FULLDX_OFFS)

#define MVPP2_PORT_STATUS0_RXFCEN_OFFS    4
#define MVPP2_PORT_STATUS0_RXFCEN_MASK    \
    (0x00000001 << MVPP2_PORT_STATUS0_RXFCEN_OFFS)

#define MVPP2_PORT_STATUS0_TXFCEN_OFFS    5
#define MVPP2_PORT_STATUS0_TXFCEN_MASK    \
    (0x00000001 << MVPP2_PORT_STATUS0_TXFCEN_OFFS)

#define MVPP2_PORT_STATUS0_PORTRXPAUSE_OFFS    6
#define MVPP2_PORT_STATUS0_PORTRXPAUSE_MASK    \
    (0x00000001 << MVPP2_PORT_STATUS0_PORTRXPAUSE_OFFS)

#define MVPP2_PORT_STATUS0_PORTTXPAUSE_OFFS    7
#define MVPP2_PORT_STATUS0_PORTTXPAUSE_MASK    \
    (0x00000001 << MVPP2_PORT_STATUS0_PORTTXPAUSE_OFFS)

#define MVPP2_PORT_STATUS0_PORTIS_DOINGPRESSURE_OFFS    8
#define MVPP2_PORT_STATUS0_PORTIS_DOINGPRESSURE_MASK    \
    (0x00000001 << MVPP2_PORT_STATUS0_PORTIS_DOINGPRESSURE_OFFS)

#define MVPP2_PORT_STATUS0_PORTBUFFULL_OFFS    9
#define MVPP2_PORT_STATUS0_PORTBUFFULL_MASK    \
    (0x00000001 << MVPP2_PORT_STATUS0_PORTBUFFULL_OFFS)

#define MVPP2_PORT_STATUS0_SYNCFAIL10MS_OFFS    10
#define MVPP2_PORT_STATUS0_SYNCFAIL10MS_MASK    \
    (0x00000001 << MVPP2_PORT_STATUS0_SYNCFAIL10MS_OFFS)

#define MVPP2_PORT_STATUS0_ANDONE_OFFS    11
#define MVPP2_PORT_STATUS0_ANDONE_MASK    \
    (0x00000001 << MVPP2_PORT_STATUS0_ANDONE_OFFS)

#define MVPP2_PORT_STATUS0_INBAND_AUTONEG_BYPASSACT_OFFS    12
#define MVPP2_PORT_STATUS0_INBAND_AUTONEG_BYPASSACT_MASK    \
    (0x00000001 << MVPP2_PORT_STATUS0_INBAND_AUTONEG_BYPASSACT_OFFS)

#define MVPP2_PORT_STATUS0_SERDESPLL_LOCKED_OFFS    13
#define MVPP2_PORT_STATUS0_SERDESPLL_LOCKED_MASK    \
    (0x00000001 << MVPP2_PORT_STATUS0_SERDESPLL_LOCKED_OFFS)

#define MVPP2_PORT_STATUS0_SYNCOK_OFFS    14
#define MVPP2_PORT_STATUS0_SYNCOK_MASK    \
    (0x00000001 << MVPP2_PORT_STATUS0_SYNCOK_OFFS)

#define MVPP2_PORT_STATUS0_SQUELCHNOT_DETECTED_OFFS    15
#define MVPP2_PORT_STATUS0_SQUELCHNOT_DETECTED_MASK    \
    (0x00000001 << MVPP2_PORT_STATUS0_SQUELCHNOT_DETECTED_OFFS)

/* Port Serial Parameters Configuration */
#define MVPP2_PORT_SERIAL_PARAM_CFG_REG                   (0x0014)
#define MVPP2_PORT_SERIAL_PARAM_CFG_UNIDIRECTIONAL_ENABLE_OFFS    0
#define MVPP2_PORT_SERIAL_PARAM_CFG_UNIDIRECTIONAL_ENABLE_MASK    \
    (0x00000001 << MVPP2_PORT_SERIAL_PARAM_CFG_UNIDIRECTIONAL_ENABLE_OFFS)

#define MVPP2_PORT_SERIAL_PARAM_CFG_RETRANSMIT_COLLISION_DOMAIN_OFFS    1
#define MVPP2_PORT_SERIAL_PARAM_CFG_RETRANSMIT_COLLISION_DOMAIN_MASK    \
    (0x00000001 << MVPP2_PORT_SERIAL_PARAM_CFG_RETRANSMIT_COLLISION_DOMAIN_OFFS)

#define MVPP2_PORT_SERIAL_PARAM_CFG_PUMA2_BTS1444_EN_OFFS    2
#define MVPP2_PORT_SERIAL_PARAM_CFG_PUMA2_BTS1444_EN_MASK    \
    (0x00000001 << MVPP2_PORT_SERIAL_PARAM_CFG_PUMA2_BTS1444_EN_OFFS)

#define MVPP2_PORT_SERIAL_PARAM_CFG_FORWARD_802_3X_FC_EN_OFFS    3
#define MVPP2_PORT_SERIAL_PARAM_CFG_FORWARD_802_3X_FC_EN_MASK    \
    (0x00000001 << MVPP2_PORT_SERIAL_PARAM_CFG_FORWARD_802_3X_FC_EN_OFFS)

#define MVPP2_PORT_SERIAL_PARAM_CFG_BP_EN_OFFS    4
#define MVPP2_PORT_SERIAL_PARAM_CFG_BP_EN_MASK    \
    (0x00000001 << MVPP2_PORT_SERIAL_PARAM_CFG_BP_EN_OFFS)

#define MVPP2_PORT_SERIAL_PARAM_CFG_RX_NEGEDGE_SAMPLE_EN_OFFS   5
#define MVPP2_PORT_SERIAL_PARAM_CFG_RX_NEGEDGE_SAMPLE_EN_MASK   \
    (0x00000001 << MVPP2_PORT_SERIAL_PARAM_CFG_RX_NEGEDGE_SAMPLE_EN_OFFS)

#define MVPP2_PORT_SERIAL_PARAM_CFG_COL_DOMAIN_LIMIT_OFFS    6
#define MVPP2_PORT_SERIAL_PARAM_CFG_COL_DOMAIN_LIMIT_MASK    \
    (0x0000003f << MVPP2_PORT_SERIAL_PARAM_CFG_COL_DOMAIN_LIMIT_OFFS)

#define MVPP2_PORT_SERIAL_PARAM_CFG_PERIODIC_TYPE_SELECT_OFFS   12
#define MVPP2_PORT_SERIAL_PARAM_CFG_PERIODIC_TYPE_SELECT_MASK   \
    (0x00000001 << MVPP2_PORT_SERIAL_PARAM_CFG_PERIODIC_TYPE_SELECT_OFFS)

#define MVPP2_PORT_SERIAL_PARAM_CFG_PER_PRIORITY_FC_EN_OFFS   13
#define MVPP2_PORT_SERIAL_PARAM_CFG_PER_PRIORITY_FC_EN_MASK   \
    (0x00000001 << MVPP2_PORT_SERIAL_PARAM_CFG_PER_PRIORITY_FC_EN_OFFS)

#define MVPP2_PORT_SERIAL_PARAM_CFG_TX_STANDARD_PRBS7_OFFS    14
#define MVPP2_PORT_SERIAL_PARAM_CFG_TX_STANDARD_PRBS7_MASK    \
    (0x00000001 << MVPP2_PORT_SERIAL_PARAM_CFG_TX_STANDARD_PRBS7_OFFS)

#define MVPP2_PORT_SERIAL_PARAM_CFG_REVERSE_PRBS_RX_OFFS    15
#define MVPP2_PORT_SERIAL_PARAM_CFG_REVERSE_PRBS_RX_MASK    \
    (0x00000001 << MVPP2_PORT_SERIAL_PARAM_CFG_REVERSE_PRBS_RX_OFFS)

/* Port Fifo Configuration 0 */
#define MVPP2_PORT_FIFO_CFG_0_REG                         (0x0018)
#define MVPP2_PORT_FIFO_CFG_0_TX_FIFO_HIGH_WM_OFFS    0
#define MVPP2_PORT_FIFO_CFG_0_TX_FIFO_HIGH_WM_MASK    \
    (0x000000ff << MVPP2_PORT_FIFO_CFG_0_TX_FIFO_HIGH_WM_OFFS)

#define MVPP2_PORT_FIFO_CFG_0_TX_FIFO_LOW_WM_OFFS    8
#define MVPP2_PORT_FIFO_CFG_0_TX_FIFO_LOW_WM_MASK    \
    (0x000000ff << MVPP2_PORT_FIFO_CFG_0_TX_FIFO_LOW_WM_OFFS)

/* Port Fifo Configuration 1 */
#define MVPP2_PORT_FIFO_CFG_1_REG                         (0x001c)
#define MVPP2_PORT_FIFO_CFG_1_RX_FIFO_MAX_TH_OFFS    0
#define MVPP2_PORT_FIFO_CFG_1_RX_FIFO_MAX_TH_MASK    \
    (0x0000003f << MVPP2_PORT_FIFO_CFG_1_RX_FIFO_MAX_TH_OFFS)

#define MVPP2_PORT_FIFO_CFG_1_TX_FIFO_MIN_TH_OFFS    6
#define MVPP2_PORT_FIFO_CFG_1_TX_FIFO_MIN_TH_MASK    \
    (0x000000ff << MVPP2_PORT_FIFO_CFG_1_TX_FIFO_MIN_TH_OFFS)

#define MVPP2_PORT_FIFO_CFG_1_PORT_EN_FIX_EN_OFFS    15
#define MVPP2_PORT_FIFO_CFG_1_PORT_EN_FIX_EN_MASK    \
    (0x00000001 << MVPP2_PORT_FIFO_CFG_1_PORT_EN_FIX_EN_OFFS)

/* Port Serdes Configuration0 */
#define MVPP2_PORT_SERDES_CFG0_REG                        (0x0028)
#define MVPP2_PORT_SERDES_CFG0_SERDESRESET_OFFS    0
#define MVPP2_PORT_SERDES_CFG0_SERDESRESET_MASK    \
    (0x00000001 << MVPP2_PORT_SERDES_CFG0_SERDESRESET_OFFS)

#define MVPP2_PORT_SERDES_CFG0_PU_TX_OFFS    1
#define MVPP2_PORT_SERDES_CFG0_PU_TX_MASK    \
    (0x00000001 << MVPP2_PORT_SERDES_CFG0_PU_TX_OFFS)

#define MVPP2_PORT_SERDES_CFG0_PU_RX_OFFS    2
#define MVPP2_PORT_SERDES_CFG0_PU_RX_MASK    \
    (0x00000001 << MVPP2_PORT_SERDES_CFG0_PU_RX_OFFS)

#define MVPP2_PORT_SERDES_CFG0_PU_PLL_OFFS    3
#define MVPP2_PORT_SERDES_CFG0_PU_PLL_MASK    \
    (0x00000001 << MVPP2_PORT_SERDES_CFG0_PU_PLL_OFFS)

#define MVPP2_PORT_SERDES_CFG0_PU_IVREF_OFFS    4
#define MVPP2_PORT_SERDES_CFG0_PU_IVREF_MASK    \
    (0x00000001 << MVPP2_PORT_SERDES_CFG0_PU_IVREF_OFFS)

#define MVPP2_PORT_SERDES_CFG0_TESTEN_OFFS    5
#define MVPP2_PORT_SERDES_CFG0_TESTEN_MASK    \
    (0x00000001 << MVPP2_PORT_SERDES_CFG0_TESTEN_OFFS)

#define MVPP2_PORT_SERDES_CFG0_DPHER_EN_OFFS    6
#define MVPP2_PORT_SERDES_CFG0_DPHER_EN_MASK    \
    (0x00000001 << MVPP2_PORT_SERDES_CFG0_DPHER_EN_OFFS)

#define MVPP2_PORT_SERDES_CFG0_RUDI_INVALID_ENABLE_OFFS    7
#define MVPP2_PORT_SERDES_CFG0_RUDI_INVALID_ENABLE_MASK    \
    (0x00000001 << MVPP2_PORT_SERDES_CFG0_RUDI_INVALID_ENABLE_OFFS)

#define MVPP2_PORT_SERDES_CFG0_ACK_OVERRIDE_ENABLE_OFFS    8
#define MVPP2_PORT_SERDES_CFG0_ACK_OVERRIDE_ENABLE_MASK    \
    (0x00000001 << MVPP2_PORT_SERDES_CFG0_ACK_OVERRIDE_ENABLE_OFFS)

#define MVPP2_PORT_SERDES_CFG0_CONFIG_WORD_ENABLE_OFFS    9
#define MVPP2_PORT_SERDES_CFG0_CONFIG_WORD_ENABLE_MASK    \
    (0x00000001 << MVPP2_PORT_SERDES_CFG0_CONFIG_WORD_ENABLE_OFFS)

#define MVPP2_PORT_SERDES_CFG0_SYNC_FAIL_INT_ENABLE_OFFS    10
#define MVPP2_PORT_SERDES_CFG0_SYNC_FAIL_INT_ENABLE_MASK    \
    (0x00000001 << MVPP2_PORT_SERDES_CFG0_SYNC_FAIL_INT_ENABLE_OFFS)

#define MVPP2_PORT_SERDES_CFG0_MASTER_MODE_ENABLE_OFFS    11
#define MVPP2_PORT_SERDES_CFG0_MASTER_MODE_ENABLE_MASK    \
    (0x00000001 << MVPP2_PORT_SERDES_CFG0_MASTER_MODE_ENABLE_OFFS)

#define MVPP2_PORT_SERDES_CFG0_TERM75_TX_OFFS    12
#define MVPP2_PORT_SERDES_CFG0_TERM75_TX_MASK    \
    (0x00000001 << MVPP2_PORT_SERDES_CFG0_TERM75_TX_OFFS)

#define MVPP2_PORT_SERDES_CFG0_OUTAMP_OFFS    13
#define MVPP2_PORT_SERDES_CFG0_OUTAMP_MASK    \
    (0x00000001 << MVPP2_PORT_SERDES_CFG0_OUTAMP_OFFS)

#define MVPP2_PORT_SERDES_CFG0_BTS712_FIX_EN_OFFS    14
#define MVPP2_PORT_SERDES_CFG0_BTS712_FIX_EN_MASK    \
    (0x00000001 << MVPP2_PORT_SERDES_CFG0_BTS712_FIX_EN_OFFS)

#define MVPP2_PORT_SERDES_CFG0_BTS156_FIX_EN_OFFS    15
#define MVPP2_PORT_SERDES_CFG0_BTS156_FIX_EN_MASK    \
    (0x00000001 << MVPP2_PORT_SERDES_CFG0_BTS156_FIX_EN_OFFS)

/* Port Serdes Configuration1 */
#define MVPP2_PORT_SERDES_CFG1_REG                        (0x002c)
#define MVPP2_PORT_SERDES_CFG1_SMII_RX_10MB_CLK_EDGE_SEL_OFFS    0
#define MVPP2_PORT_SERDES_CFG1_SMII_RX_10MB_CLK_EDGE_SEL_MASK    \
    (0x00000001 << MVPP2_GMAC_PORT_SERDES_CFG1_SMII_RX_10MB_CLK_EDGE_SEL_OFFS)

#define MVPP2_GMAC_PORT_SERDES_CFG1_SMII_TX_10MB_CLK_EDGE_SEL_OFFS    1
#define MVPP2_GMAC_PORT_SERDES_CFG1_SMII_TX_10MB_CLK_EDGE_SEL_MASK    \
    (0x00000001 << MVPP2_GMAC_PORT_SERDES_CFG1_SMII_TX_10MB_CLK_EDGE_SEL_OFFS)

#define MVPP2_GMAC_PORT_SERDES_CFG1_MEN_OFFS    2
#define MVPP2_GMAC_PORT_SERDES_CFG1_MEN_MASK    \
    (0x00000003 << MVPP2_GMAC_PORT_SERDES_CFG1_MEN_OFFS)

#define MVPP2_GMAC_PORT_SERDES_CFG1_VCMS_OFFS    4
#define MVPP2_GMAC_PORT_SERDES_CFG1_VCMS_MASK    \
    (0x00000001 << MVPP2_GMAC_PORT_SERDES_CFG1_VCMS_OFFS)

#define MVPP2_GMAC_PORT_SERDES_CFG1_100FX_PCS_USE_SIGDET_OFFS    5
#define MVPP2_GMAC_PORT_SERDES_CFG1_100FX_PCS_USE_SIGDET_MASK    \
    (0x00000001 << MVPP2_GMAC_PORT_SERDES_CFG1_100FX_PCS_USE_SIGDET_OFFS)

#define MVPP2_GMAC_PORT_SERDES_CFG1_EN_CRS_MASK_TX_OFFS    6
#define MVPP2_GMAC_PORT_SERDES_CFG1_EN_CRS_MASK_TX_MASK    \
    (0x00000001 << MVPP2_GMAC_PORT_SERDES_CFG1_EN_CRS_MASK_TX_OFFS)

#define MVPP2_GMAC_PORT_SERDES_CFG1_100FX_ENABLE_OFFS    7
#define MVPP2_GMAC_PORT_SERDES_CFG1_100FX_ENABLE_MASK    \
    (0x00000001 << MVPP2_GMAC_PORT_SERDES_CFG1_100FX_ENABLE_OFFS)

#define MVPP2_GMAC_PORT_SERDES_CFG1_100FX_PCS_PHY_ADDRESS_OFFS    8
#define MVPP2_GMAC_PORT_SERDES_CFG1_100FX_PCS_PHY_ADDRESS_MASK    \
    (0x0000001f << MVPP2_GMAC_PORT_SERDES_CFG1_100FX_PCS_PHY_ADDRESS_OFFS)

#define MVPP2_GMAC_PORT_SERDES_CFG1_100FX_PCS_SIGDET_POLARITY_OFFS    13
#define MVPP2_GMAC_PORT_SERDES_CFG1_100FX_PCS_SIGDET_POLARITY_MASK    \
    (0x00000001 << MVPP2_GMAC_PORT_SERDES_CFG1_100FX_PCS_SIGDET_POLARITY_OFFS)

#define MVPP2_GMAC_PORT_SERDES_CFG1_100FX_PCS_INTERRUPT_POLARITY_OFFS    14
#define MVPP2_GMAC_PORT_SERDES_CFG1_100FX_PCS_INTERRUPT_POLARITY_MASK    \
    (0x00000001 << MVPP2_GMAC_PORT_SERDES_CFG1_100FX_PCS_INTERRUPT_POLARITY_OFFS)

#define MVPP2_GMAC_PORT_SERDES_CFG1_100FX_PCS_SERDES_POLARITY_OFFS    15
#define MVPP2_GMAC_PORT_SERDES_CFG1_100FX_PCS_SERDES_POLARITY_MASK    \
    (0x00000001 << MVPP2_GMAC_PORT_SERDES_CFG1_100FX_PCS_SERDES_POLARITY_OFFS)

/* Port Serdes Configuration2 */
#define MVPP2_PORT_SERDES_CFG2_REG                        (0x0030)
#define MVPP2_PORT_SERDES_CFG2_AN_ADV_CONFIGURATION_OFFS    0
#define MVPP2_PORT_SERDES_CFG2_AN_ADV_CONFIGURATION_MASK    \
    (0x0000ffff << MVPP2_PORT_SERDES_CFG2_AN_ADV_CONFIGURATION_OFFS)

/* Port Serdes Configuration3 */
#define MVPP2_PORT_SERDES_CFG3_REG                        (0x0034)
#define MVPP2_PORT_SERDES_CFG3_ABILITY_MATCH_STATUS_OFFS    0
#define MVPP2_PORT_SERDES_CFG3_ABILITY_MATCH_STATUS_MASK    \
    (0x0000ffff << MVPP2_PORT_SERDES_CFG3_ABILITY_MATCH_STATUS_OFFS)

/* Port Prbs Status */
#define MVPP2_PORT_PRBS_STATUS_REG                        (0x0038)
#define MVPP2_PORT_PRBS_STATUS_PRBSCHECK_LOCKED_OFFS    0
#define MVPP2_PORT_PRBS_STATUS_PRBSCHECK_LOCKED_MASK    \
    (0x00000001 << MVPP2_PORT_PRBS_STATUS_PRBSCHECK_LOCKED_OFFS)

#define MVPP2_PORT_PRBS_STATUS_PRBSCHECKRDY_OFFS    1
#define MVPP2_PORT_PRBS_STATUS_PRBSCHECKRDY_MASK    \
    (0x00000001 << MVPP2_PORT_PRBS_STATUS_PRBSCHECKRDY_OFFS)

/* Port Prbs Error Counter */
#define MVPP2_PORT_PRBS_ERR_CNTR_REG                      (0x003c)
#define MVPP2_PORT_PRBS_ERR_CNTR_PRBSBITERRCNT_OFFS    0
#define MVPP2_PORT_PRBS_ERR_CNTR_PRBSBITERRCNT_MASK    \
    (0x0000ffff << MVPP2_PORT_PRBS_ERR_CNTR_PRBSBITERRCNT_OFFS)

/* Port Status1 */
#define MVPP2_PORT_STATUS1_REG                            (0x0040)
#define MVPP2_PORT_STATUS1_MEDIAACTIVE_OFFS    0
#define MVPP2_PORT_STATUS1_MEDIAACTIVE_MASK    \
    (0x00000001 << MVPP2_PORT_STATUS1_MEDIAACTIVE_OFFS)

/* Port Mib Counters Control */
#define MVPP2_PORT_MIB_CNTRS_CTRL_REG                     (0x0044)
#define MVPP2_PORT_MIB_CNTRS_CTRL_MIB_COPY_TRIGGER_OFFS    0
#define MVPP2_PORT_MIB_CNTRS_CTRL_MIB_COPY_TRIGGER_MASK    \
    (0x00000001 << MVPP2_PORT_MIB_CNTRS_CTRL_MIB_COPY_TRIGGER_OFFS)

#define MVPP2_PORT_MIB_CNTRS_CTRL_MIB_CLEAR_ON_READ__OFFS     1
#define MVPP2_PORT_MIB_CNTRS_CTRL_MIB_CLEAR_ON_READ__MASK     \
    (0x00000001 << MVPP2_PORT_MIB_CNTRS_CTRL_MIB_CLEAR_ON_READ__OFFS)

#define MVPP2_PORT_MIB_CNTRS_CTRL_RX_HISTOGRAM_EN_OFFS    2
#define MVPP2_PORT_MIB_CNTRS_CTRL_RX_HISTOGRAM_EN_MASK    \
    (0x00000001 << MVPP2_PORT_MIB_CNTRS_CTRL_RX_HISTOGRAM_EN_OFFS)

#define MVPP2_PORT_MIB_CNTRS_CTRL_TX_HISTOGRAM_EN_OFFS    3
#define MVPP2_PORT_MIB_CNTRS_CTRL_TX_HISTOGRAM_EN_MASK    \
    (0x00000001 << MVPP2_PORT_MIB_CNTRS_CTRL_TX_HISTOGRAM_EN_OFFS)

#define MVPP2_PORT_MIB_CNTRS_CTRL_MFA1_BTT940_FIX_ENABLE__OFFS    4
#define MVPP2_PORT_MIB_CNTRS_CTRL_MFA1_BTT940_FIX_ENABLE__MASK    \
    (0x00000001 << MVPP2_PORT_MIB_CNTRS_CTRL_MFA1_BTT940_FIX_ENABLE__OFFS)

#define MVPP2_PORT_MIB_CNTRS_CTRL_XCAT_BTS_340_EN__OFFS    5
#define MVPP2_PORT_MIB_CNTRS_CTRL_XCAT_BTS_340_EN__MASK    \
    (0x00000001 << MVPP2_PORT_MIB_CNTRS_CTRL_XCAT_BTS_340_EN__OFFS)

#define MVPP2_PORT_MIB_CNTRS_CTRL_MIB_4_COUNT_HIST_OFFS    6
#define MVPP2_PORT_MIB_CNTRS_CTRL_MIB_4_COUNT_HIST_MASK    \
    (0x00000001 << MVPP2_PORT_MIB_CNTRS_CTRL_MIB_4_COUNT_HIST_OFFS)

#define MVPP2_PORT_MIB_CNTRS_CTRL_MIB_4_LIMIT_1518_1522_OFFS    7
#define MVPP2_PORT_MIB_CNTRS_CTRL_MIB_4_LIMIT_1518_1522_MASK    \
    (0x00000001 << MVPP2_PORT_MIB_CNTRS_CTRL_MIB_4_LIMIT_1518_1522_OFFS)

/* Port Mac Control3 */
#define MVPP2_PORT_CTRL3_REG                              (0x0048)
#define MVPP2_PORT_CTRL3_BUF_SIZE_OFFS    0
#define MVPP2_PORT_CTRL3_BUF_SIZE_MASK    \
    (0x0000003f << MVPP2_PORT_CTRL3_BUF_SIZE_OFFS)

#define MVPP2_PORT_CTRL3_IPG_DATA_OFFS    6
#define MVPP2_PORT_CTRL3_IPG_DATA_MASK    \
    (0x000001ff << MVPP2_PORT_CTRL3_IPG_DATA_OFFS)

#define MVPP2_PORT_CTRL3_LLFC_GLOBAL_FC_ENABLE_OFFS    15
#define MVPP2_PORT_CTRL3_LLFC_GLOBAL_FC_ENABLE_MASK    \
    (0x00000001 << MVPP2_PORT_CTRL3_LLFC_GLOBAL_FC_ENABLE_OFFS)
#define MVPP2_CAUSE_TXQ_SENT_DESC_ALL_MASK    0xff

/* Port Mac Control4 */
#define MVPP2_PORT_CTRL4_REG                              (0x0090)
#define MVPP2_PORT_CTRL4_EXT_PIN_GMII_SEL_OFFS    0
#define MVPP2_PORT_CTRL4_EXT_PIN_GMII_SEL_MASK    \
    (0x00000001 << MVPP2_PORT_CTRL4_EXT_PIN_GMII_SEL_OFFS)

#define MVPP2_PORT_CTRL4_PREAMBLE_FIX_OFFS    1
#define MVPP2_PORT_CTRL4_PREAMBLE_FIX_MASK    \
    (0x00000001 << MVPP2_PORT_CTRL4_PREAMBLE_FIX_OFFS)

#define MVPP2_PORT_CTRL4_SQ_DETECT_FIX_EN_OFFS    2
#define MVPP2_PORT_CTRL4_SQ_DETECT_FIX_EN_MASK    \
    (0x00000001 << MVPP2_PORT_CTRL4_SQ_DETECT_FIX_EN_OFFS)

#define MVPP2_PORT_CTRL4_FC_EN_RX_OFFS    3
#define MVPP2_PORT_CTRL4_FC_EN_RX_MASK    \
    (0x00000001 << MVPP2_PORT_CTRL4_FC_EN_RX_OFFS)

#define MVPP2_PORT_CTRL4_FC_EN_TX_OFFS    4
#define MVPP2_PORT_CTRL4_FC_EN_TX_MASK    \
    (0x00000001 << MVPP2_PORT_CTRL4_FC_EN_TX_OFFS)

#define MVPP2_PORT_CTRL4_DP_CLK_SEL_OFFS    5
#define MVPP2_PORT_CTRL4_DP_CLK_SEL_MASK    \
    (0x00000001 << MVPP2_PORT_CTRL4_DP_CLK_SEL_OFFS)

#define MVPP2_PORT_CTRL4_SYNC_BYPASS_OFFS   6
#define MVPP2_PORT_CTRL4_SYNC_BYPASS_MASK    \
    (0x00000001 << MVPP2_PORT_CTRL4_SYNC_BYPASS_OFFS)

#define MVPP2_PORT_CTRL4_QSGMII_BYPASS_ACTIVE_OFFS    7
#define MVPP2_PORT_CTRL4_QSGMII_BYPASS_ACTIVE_MASK    \
    (0x00000001 << MVPP2_PORT_CTRL4_QSGMII_BYPASS_ACTIVE_OFFS)

#define MVPP2_PORT_CTRL4_COUNT_EXTERNAL_FC_EN_OFFS    8
#define MVPP2_PORT_CTRL4_COUNT_EXTERNAL_FC_EN_MASK    \
    (0x00000001 << MVPP2_PORT_CTRL4_COUNT_EXTERNAL_FC_EN_OFFS)

#define MVPP2_PORT_CTRL4_MARVELL_HEADER_EN_OFFS    9
#define MVPP2_PORT_CTRL4_MARVELL_HEADER_EN_MASK    \
    (0x00000001 << MVPP2_PORT_CTRL4_MARVELL_HEADER_EN_OFFS)

#define MVPP2_PORT_CTRL4_LEDS_NUMBER_OFFS    10
#define MVPP2_PORT_CTRL4_LEDS_NUMBER_MASK    \
    (0x0000003f << MVPP2_PORT_CTRL4_LEDS_NUMBER_OFFS)

/* XPCS registers */

/* Global Configuration 0 */
#define MVPP22_XPCS_GLOBAL_CFG_0_REG                   0x0
#define MVPP22_XPCS_PCSRESET                           BIT(0)
#define MVPP22_XPCS_PCSMODE_OFFS                       3
#define MVPP22_XPCS_PCSMODE_MASK                       (0x3 << MVPP22_XPCS_PCSMODE_OFFS)
#define MVPP22_XPCS_LANEACTIVE_OFFS                    5
#define MVPP22_XPCS_LANEACTIVE_MASK                    (0x3 << MVPP22_XPCS_LANEACTIVE_OFFS)

/* MPCS registers */

#define MVPP22_MPCS40G_COMMON_CONTROL                  0x14
#define MVPP22_MPCS_FORWARD_ERROR_CORRECTION_MASK      BIT(10)

#define MVPP22_MPCS_CLOCK_RESET                        0x14c
#define MVPP22_MPCS_TX_SD_CLK_RESET_MASK               BIT(0)
#define MVPP22_MPCS_RX_SD_CLK_RESET_MASK               BIT(1)
#define MVPP22_MPCS_MAC_CLK_RESET_MASK                 BIT(2)
#define MVPP22_MPCS_CLK_DIVISION_RATIO_OFFS            4
#define MVPP22_MPCS_CLK_DIVISION_RATIO_MASK            (0x7 << MVPP22_MPCS_CLK_DIVISION_RATIO_OFFS)
#define MVPP22_MPCS_CLK_DIVISION_RATIO_DEFAULT         (0x1 << MVPP22_MPCS_CLK_DIVISION_RATIO_OFFS)
#define MVPP22_MPCS_CLK_DIV_PHASE_SET_MASK             BIT(11)

/* Descriptor ring Macros */
#define MVPP2_QUEUE_NEXT_DESC(q, index)                   (((index) < (q)->LastDesc) ? ((index) + 1) : 0)

/* Various constants */

/* Coalescing */
#define MVPP2_TXDONE_COAL_PKTS_THRESH                     64
#define MVPP2_TXDONE_HRTIMER_PERIOD_NS                    1000000UL
#define MVPP2_TXDONE_COAL_USEC                            1000
#define MVPP2_RX_COAL_PKTS                                32
#define MVPP2_RX_COAL_USEC                                64

/*
 * The two bytes Marvell header. Either contains a special value used
 * by Marvell switches when a specific hardware mode is enabled (not
 * supported by this driver) or is filled automatically by zeroes on
 * the RX side. Those two bytes being at the front of the Ethernet
 * header, they allow to have the IP header aligned on a 4 bytes
 * boundary automatically: the hardware skips those two bytes on its
 * own.
 */
#define MVPP2_MH_SIZE                                     2
#define MVPP2_ETH_TYPE_LEN                                2
#define MVPP2_PPPOE_HDR_SIZE                              8
#define MVPP2_VLAN_TAG_LEN                                4

/* Lbtd 802.3 type */
#define MVPP2_IP_LBDT_TYPE                                0xfffa

#define MVPP2_CPU_D_CACHE_LINE_SIZE                       32
#define MVPP2_TX_CSUM_MAX_SIZE                            9800

/* Timeout constants */
#define MVPP2_TX_DISABLE_TIMEOUT_MSEC                     1000
#define MVPP2_TX_PENDING_TIMEOUT_MSEC                     1000

#define MVPP2_TX_MTU_MAX                                  0x7ffff

/* Maximum number of T-CONTs of PON port */
#define MVPP2_MAX_TCONT                                   16

/* Maximum number of supported ports */
#define MVPP2_MAX_PORTS                                   4

/* Maximum number of TXQs used by single port */
#define MVPP2_MAX_TXQ                                     8

/* Maximum number of RXQs used by single port */
#define MVPP2_MAX_RXQ                                     8

/* Dfault number of RXQs in use */
#define MVPP2_DEFAULT_RXQ                                 4

/* Total number of RXQs available to all ports */
#define MVPP2_RXQ_TOTAL_NUM                               (MVPP2_MAX_PORTS * MVPP2_MAX_RXQ)

/* Max number of Rx descriptors */
#define MVPP2_MAX_RXD                                     64

/* Max number of Tx descriptors */
#define MVPP2_MAX_TXD                                     32

/* Amount of Tx descriptors that can be reserved at once by CPU */
#define MVPP2_CPU_DESC_CHUNK                              64

/* Max number of Tx descriptors in each aggregated queue */
#define MVPP2_AGGR_TXQ_SIZE                               256

/* Descriptor aligned size */
#define MVPP2_DESC_ALIGNED_SIZE                           32

/* Descriptor alignment mask */
#define MVPP2_TX_DESC_ALIGN                               (MVPP2_DESC_ALIGNED_SIZE - 1)

/* RX FIFO constants */
#define MVPP2_RX_FIFO_PORT_DATA_SIZE_32KB       0x8000
#define MVPP2_RX_FIFO_PORT_DATA_SIZE_8KB        0x2000
#define MVPP2_RX_FIFO_PORT_DATA_SIZE_4KB        0x1000
#define MVPP2_RX_FIFO_PORT_ATTR_SIZE_32KB       0x200
#define MVPP2_RX_FIFO_PORT_ATTR_SIZE_8KB        0x80
#define MVPP2_RX_FIFO_PORT_ATTR_SIZE_4KB        0x40
#define MVPP2_RX_FIFO_PORT_MIN_PKT              0x80

/* TX FIFO constants */
#define MVPP22_TX_FIFO_DATA_SIZE_10KB           0xa
#define MVPP22_TX_FIFO_DATA_SIZE_3KB            0x3
#define MVPP2_TX_FIFO_THRESHOLD_MIN             256
#define MVPP2_TX_FIFO_THRESHOLD_10KB    \
        (MVPP22_TX_FIFO_DATA_SIZE_10KB * 1024 - MVPP2_TX_FIFO_THRESHOLD_MIN)
#define MVPP2_TX_FIFO_THRESHOLD_3KB     \
        (MVPP22_TX_FIFO_DATA_SIZE_3KB * 1024 - MVPP2_TX_FIFO_THRESHOLD_MIN)

#define MVPP2_BIT_TO_BYTE(bit)                            ((bit) / 8)

/* IPv6 max L3 address size */
#define MVPP2_MAX_L3_ADDR_SIZE                            16

/* Port flags */
#define MVPP2_F_LOOPBACK                                  BIT(0)

/* SD1 Control1 */
#define SD1_CONTROL_1_REG                                 (0x148)

#define SD1_CONTROL_XAUI_EN_OFFSET    28
#define SD1_CONTROL_XAUI_EN_MASK    (0x1 << SD1_CONTROL_XAUI_EN_OFFSET)

#define SD1_CONTROL_RXAUI0_L23_EN_OFFSET    27
#define SD1_CONTROL_RXAUI0_L23_EN_MASK    (0x1 << SD1_CONTROL_RXAUI0_L23_EN_OFFSET)

#define SD1_CONTROL_RXAUI1_L45_EN_OFFSET    26
#define SD1_CONTROL_RXAUI1_L45_EN_MASK    (0x1 << SD1_CONTROL_RXAUI1_L45_EN_OFFSET)

/* System Soft Reset 1 */
#define MV_GOP_SOFT_RESET_1_REG                           (0x108)

#define NETC_GOP_SOFT_RESET_OFFSET    6
#define NETC_GOP_SOFT_RESET_MASK    (0x1 << NETC_GOP_SOFT_RESET_OFFSET)

/* Ports Control 0 */
#define MV_NETCOMP_PORTS_CONTROL_0                        (0x110)

#define NETC_CLK_DIV_PHASE_OFFSET    31
#define NETC_CLK_DIV_PHASE_MASK    (0x1 << NETC_CLK_DIV_PHASE_OFFSET)

#define NETC_GIG_RX_DATA_SAMPLE_OFFSET    29
#define NETC_GIG_RX_DATA_SAMPLE_MASK    (0x1 << NETC_GIG_RX_DATA_SAMPLE_OFFSET)

#define NETC_BUS_WIDTH_SELECT_OFFSET    1
#define NETC_BUS_WIDTH_SELECT_MASK    (0x1 << NETC_BUS_WIDTH_SELECT_OFFSET)

#define NETC_GOP_ENABLE_OFFSET    0
#define NETC_GOP_ENABLE_MASK    (0x1 << NETC_GOP_ENABLE_OFFSET)

/* Ports Control 1 */
#define MV_NETCOMP_PORTS_CONTROL_1                        (0x114)

#define NETC_PORT_GIG_RF_RESET_OFFSET(port)    (28 + port)
#define NETC_PORT_GIG_RF_RESET_MASK(port)    (0x1 << NETC_PORT_GIG_RF_RESET_OFFSET(port))

#define NETC_PORTS_ACTIVE_OFFSET(port)    (0 + port)
#define NETC_PORTS_ACTIVE_MASK(port)    (0x1 << NETC_PORTS_ACTIVE_OFFSET(port))

/* Ports Status */
#define MV_NETCOMP_PORTS_STATUS                           (0x11C)
#define NETC_PORTS_STATUS_OFFSET(port)    (0 + port)
#define NETC_PORTS_STATUS_MASK(port)    (0x1 << NETC_PORTS_STATUS_OFFSET(port))

/* Networking Complex Control 0 */
#define MV_NETCOMP_CONTROL_0                              (0x120)

#define NETC_GBE_PORT1_MII_MODE_OFFSET    2
#define NETC_GBE_PORT1_MII_MODE_MASK    (0x1 << NETC_GBE_PORT1_MII_MODE_OFFSET)

#define NETC_GBE_PORT1_SGMII_MODE_OFFSET    1
#define NETC_GBE_PORT1_SGMII_MODE_MASK    (0x1 << NETC_GBE_PORT1_SGMII_MODE_OFFSET)

#define NETC_GBE_PORT0_SGMII_MODE_OFFSET    0
#define NETC_GBE_PORT0_SGMII_MODE_MASK    (0x1 << NETC_GBE_PORT0_SGMII_MODE_OFFSET)

/* Port Mac Control0 */
#define MV_XLG_PORT_MAC_CTRL0_REG     (                   0x0000)
#define MV_XLG_MAC_CTRL0_PORTEN_OFFS    0
#define MV_XLG_MAC_CTRL0_PORTEN_MASK    \
    (0x00000001 << MV_XLG_MAC_CTRL0_PORTEN_OFFS)

#define MV_XLG_MAC_CTRL0_MACRESETN_OFFS    1
#define MV_XLG_MAC_CTRL0_MACRESETN_MASK    \
    (0x00000001 << MV_XLG_MAC_CTRL0_MACRESETN_OFFS)

#define MV_XLG_MAC_CTRL0_FORCELINKDOWN_OFFS    2
#define MV_XLG_MAC_CTRL0_FORCELINKDOWN_MASK    \
    (0x00000001 << MV_XLG_MAC_CTRL0_FORCELINKDOWN_OFFS)

#define MV_XLG_MAC_CTRL0_FORCELINKPASS_OFFS    3
#define MV_XLG_MAC_CTRL0_FORCELINKPASS_MASK    \
    (0x00000001 << MV_XLG_MAC_CTRL0_FORCELINKPASS_OFFS)

#define MV_XLG_MAC_CTRL0_TXIPGMODE_OFFS    5
#define MV_XLG_MAC_CTRL0_TXIPGMODE_MASK    \
    (0x00000003 << MV_XLG_MAC_CTRL0_TXIPGMODE_OFFS)

#define MV_XLG_MAC_CTRL0_RXFCEN_OFFS    7
#define MV_XLG_MAC_CTRL0_RXFCEN_MASK    \
    (0x00000001 << MV_XLG_MAC_CTRL0_RXFCEN_OFFS)

#define MV_XLG_MAC_CTRL0_TXFCEN_OFFS    8
#define MV_XLG_MAC_CTRL0_TXFCEN_MASK    \
    (0x00000001 << MV_XLG_MAC_CTRL0_TXFCEN_OFFS)

#define MV_XLG_MAC_CTRL0_RXCRCCHECKEN_OFFS    9
#define MV_XLG_MAC_CTRL0_RXCRCCHECKEN_MASK    \
    (0x00000001 << MV_XLG_MAC_CTRL0_RXCRCCHECKEN_OFFS)

#define MV_XLG_MAC_CTRL0_PERIODICXONEN_OFFS    10
#define MV_XLG_MAC_CTRL0_PERIODICXONEN_MASK    \
    (0x00000001 << MV_XLG_MAC_CTRL0_PERIODICXONEN_OFFS)

#define MV_XLG_MAC_CTRL0_RXCRCSTRIPEN_OFFS    11
#define MV_XLG_MAC_CTRL0_RXCRCSTRIPEN_MASK    \
    (0x00000001 << MV_XLG_MAC_CTRL0_RXCRCSTRIPEN_OFFS)

#define MV_XLG_MAC_CTRL0_PADDINGDIS_OFFS    13
#define MV_XLG_MAC_CTRL0_PADDINGDIS_MASK    \
    (0x00000001 << MV_XLG_MAC_CTRL0_PADDINGDIS_OFFS)

#define MV_XLG_MAC_CTRL0_MIBCNTDIS_OFFS    14
#define MV_XLG_MAC_CTRL0_MIBCNTDIS_MASK    \
    (0x00000001 << MV_XLG_MAC_CTRL0_MIBCNTDIS_OFFS)

#define MV_XLG_MAC_CTRL0_PFC_CASCADE_PORT_ENABLE_OFFS    15
#define MV_XLG_MAC_CTRL0_PFC_CASCADE_PORT_ENABLE_MASK    \
    (0x00000001 << MV_XLG_MAC_CTRL0_PFC_CASCADE_PORT_ENABLE_OFFS)

/* Port Mac Control1 */
#define MV_XLG_PORT_MAC_CTRL1_REG                         (0x0004)
#define MV_XLG_MAC_CTRL1_FRAMESIZELIMIT_OFFS    0
#define MV_XLG_MAC_CTRL1_FRAMESIZELIMIT_MASK    \
    (0x00001fff << MV_XLG_MAC_CTRL1_FRAMESIZELIMIT_OFFS)
#define MV_XLG_MAC_CTRL1_FRAMESIZELIMIT_DEFAULT     0x1400

#define MV_XLG_MAC_CTRL1_MACLOOPBACKEN_OFFS    13
#define MV_XLG_MAC_CTRL1_MACLOOPBACKEN_MASK    \
    (0x00000001 << MV_XLG_MAC_CTRL1_MACLOOPBACKEN_OFFS)

#define MV_XLG_MAC_CTRL1_XGMIILOOPBACKEN_OFFS    14
#define MV_XLG_MAC_CTRL1_XGMIILOOPBACKEN_MASK    \
    (0x00000001 << MV_XLG_MAC_CTRL1_XGMIILOOPBACKEN_OFFS)

#define MV_XLG_MAC_CTRL1_LOOPBACKCLOCKSELECT_OFFS    15
#define MV_XLG_MAC_CTRL1_LOOPBACKCLOCKSELECT_MASK    \
    (0x00000001 << MV_XLG_MAC_CTRL1_LOOPBACKCLOCKSELECT_OFFS)

/* Port Mac Control2 */
#define MV_XLG_PORT_MAC_CTRL2_REG                         (0x0008)
#define MV_XLG_MAC_CTRL2_SALOW_7_0_OFFS    0
#define MV_XLG_MAC_CTRL2_SALOW_7_0_MASK    \
    (0x000000ff << MV_XLG_MAC_CTRL2_SALOW_7_0_OFFS)

#define MV_XLG_MAC_CTRL2_UNIDIRECTIONALEN_OFFS    8
#define MV_XLG_MAC_CTRL2_UNIDIRECTIONALEN_MASK    \
    (0x00000001 << MV_XLG_MAC_CTRL2_UNIDIRECTIONALEN_OFFS)

#define MV_XLG_MAC_CTRL2_FIXEDIPGBASE_OFFS    9
#define MV_XLG_MAC_CTRL2_FIXEDIPGBASE_MASK    \
    (0x00000001 << MV_XLG_MAC_CTRL2_FIXEDIPGBASE_OFFS)

#define MV_XLG_MAC_CTRL2_PERIODICXOFFEN_OFFS    10
#define MV_XLG_MAC_CTRL2_PERIODICXOFFEN_MASK    \
    (0x00000001 << MV_XLG_MAC_CTRL2_PERIODICXOFFEN_OFFS)

#define MV_XLG_MAC_CTRL2_SIMPLEXMODEEN_OFFS    13
#define MV_XLG_MAC_CTRL2_SIMPLEXMODEEN_MASK    \
    (0x00000001 << MV_XLG_MAC_CTRL2_SIMPLEXMODEEN_OFFS)

#define MV_XLG_MAC_CTRL2_FC_MODE_OFFS    14
#define MV_XLG_MAC_CTRL2_FC_MODE_MASK    \
    (0x00000003 << MV_XLG_MAC_CTRL2_FC_MODE_OFFS)

/* Port Status */
#define MV_XLG_MAC_PORT_STATUS_REG                        (0x000c)
#define MV_XLG_MAC_PORT_STATUS_LINKSTATUS_OFFS    0
#define MV_XLG_MAC_PORT_STATUS_LINKSTATUS_MASK    \
    (0x00000001 << MV_XLG_MAC_PORT_STATUS_LINKSTATUS_OFFS)

#define MV_XLG_MAC_PORT_STATUS_REMOTEFAULT_OFFS    1
#define MV_XLG_MAC_PORT_STATUS_REMOTEFAULT_MASK    \
    (0x00000001 << MV_XLG_MAC_PORT_STATUS_REMOTEFAULT_OFFS)

#define MV_XLG_MAC_PORT_STATUS_LOCALFAULT_OFFS    2
#define MV_XLG_MAC_PORT_STATUS_LOCALFAULT_MASK    \
    (0x00000001 << MV_XLG_MAC_PORT_STATUS_LOCALFAULT_OFFS)

#define MV_XLG_MAC_PORT_STATUS_LINKSTATUSCLEAN_OFFS    3
#define MV_XLG_MAC_PORT_STATUS_LINKSTATUSCLEAN_MASK    \
    (0x00000001 << MV_XLG_MAC_PORT_STATUS_LINKSTATUSCLEAN_OFFS)

#define MV_XLG_MAC_PORT_STATUS_LOCALFAULTCLEAN_OFFS    4
#define MV_XLG_MAC_PORT_STATUS_LOCALFAULTCLEAN_MASK    \
    (0x00000001 << MV_XLG_MAC_PORT_STATUS_LOCALFAULTCLEAN_OFFS)

#define MV_XLG_MAC_PORT_STATUS_REMOTEFAULTCLEAN_OFFS    5
#define MV_XLG_MAC_PORT_STATUS_REMOTEFAULTCLEAN_MASK    \
    (0x00000001 << MV_XLG_MAC_PORT_STATUS_REMOTEFAULTCLEAN_OFFS)

#define MV_XLG_MAC_PORT_STATUS_PORTRXPAUSE_OFFS    6
#define MV_XLG_MAC_PORT_STATUS_PORTRXPAUSE_MASK    \
    (0x00000001 << MV_XLG_MAC_PORT_STATUS_PORTRXPAUSE_OFFS)

#define MV_XLG_MAC_PORT_STATUS_PORTTXPAUSE_OFFS    7
#define MV_XLG_MAC_PORT_STATUS_PORTTXPAUSE_MASK    \
    (0x00000001 << MV_XLG_MAC_PORT_STATUS_PORTTXPAUSE_OFFS)

#define MV_XLG_MAC_PORT_STATUS_PFC_SYNC_FIFO_FULL_OFFS    8
#define MV_XLG_MAC_PORT_STATUS_PFC_SYNC_FIFO_FULL_MASK    \
    (0x00000001 << MV_XLG_MAC_PORT_STATUS_PFC_SYNC_FIFO_FULL_OFFS)

/* Port Fifos Thresholds Configuration */
#define MV_XLG_PORT_FIFOS_THRS_CFG_REG                    (0x0010)
#define MV_XLG_MAC_PORT_FIFOS_THRS_CFG_RXFULLTHR_OFFS    0
#define MV_XLG_MAC_PORT_FIFOS_THRS_CFG_RXFULLTHR_MASK    \
    (0x0000001f << MV_XLG_MAC_PORT_FIFOS_THRS_CFG_RXFULLTHR_OFFS)

#define MV_XLG_MAC_PORT_FIFOS_THRS_CFG_TXFIFOSIZE_OFFS    5
#define MV_XLG_MAC_PORT_FIFOS_THRS_CFG_TXFIFOSIZE_MASK    \
    (0x0000003f << MV_XLG_MAC_PORT_FIFOS_THRS_CFG_TXFIFOSIZE_OFFS)

#define MV_XLG_MAC_PORT_FIFOS_THRS_CFG_TXRDTHR_OFFS    11
#define MV_XLG_MAC_PORT_FIFOS_THRS_CFG_TXRDTHR_MASK    \
    (0x0000001f << MV_XLG_MAC_PORT_FIFOS_THRS_CFG_TXRDTHR_OFFS)

/* Port Mac Control3 */
#define MV_XLG_PORT_MAC_CTRL3_REG                         (0x001c)
#define MV_XLG_MAC_CTRL3_BUFSIZE_OFFS    0
#define MV_XLG_MAC_CTRL3_BUFSIZE_MASK    \
    (0x0000003f << MV_XLG_MAC_CTRL3_BUFSIZE_OFFS)

#define MV_XLG_MAC_CTRL3_XTRAIPG_OFFS    6
#define MV_XLG_MAC_CTRL3_XTRAIPG_MASK    \
    (0x0000007f << MV_XLG_MAC_CTRL3_XTRAIPG_OFFS)

#define MV_XLG_MAC_CTRL3_MACMODESELECT_OFFS    13
#define MV_XLG_MAC_CTRL3_MACMODESELECT_MASK    \
    (0x00000007 << MV_XLG_MAC_CTRL3_MACMODESELECT_OFFS)
#define MV_XLG_MAC_CTRL3_MACMODESELECT_GMAC    \
    (0x00000000 << MV_XLG_MAC_CTRL3_MACMODESELECT_OFFS)
#define MV_XLG_MAC_CTRL3_MACMODESELECT_10G     \
    (0x00000001 << MV_XLG_MAC_CTRL3_MACMODESELECT_OFFS)

/* Port Per Prio Flow Control Status */
#define MV_XLG_PORT_PER_PRIO_FLOW_CTRL_STATUS_REG         (0x0020)
#define MV_XLG_MAC_PORT_PER_PRIO_FLOW_CTRL_STATUS_PRIONSTATUS_OFFS    0
#define MV_XLG_MAC_PORT_PER_PRIO_FLOW_CTRL_STATUS_PRIONSTATUS_MASK    \
    (0x00000001 << MV_XLG_MAC_PORT_PER_PRIO_FLOW_CTRL_STATUS_PRIONSTATUS_OFFS)

/* Debug Bus Status */
#define MV_XLG_DEBUG_BUS_STATUS_REG                       (0x0024)
#define MV_XLG_MAC_DEBUG_BUS_STATUS_DEBUG_BUS_OFFS    0
#define MV_XLG_MAC_DEBUG_BUS_STATUS_DEBUG_BUS_MASK    \
    (0x0000ffff << MV_XLG_MAC_DEBUG_BUS_STATUS_DEBUG_BUS_OFFS)

/* Port Metal Fix */
#define MV_XLG_PORT_METAL_FIX_REG                         (0x002c)
#define MV_XLG_MAC_PORT_METAL_FIX_EN_EOP_IN_FIFO__OFFS    0
#define MV_XLG_MAC_PORT_METAL_FIX_EN_EOP_IN_FIFO__MASK    \
    (0x00000001 << MV_XLG_MAC_PORT_METAL_FIX_EN_EOP_IN_FIFO__OFFS)

#define MV_XLG_MAC_PORT_METAL_FIX_EN_LTF_FIX__OFFS    1
#define MV_XLG_MAC_PORT_METAL_FIX_EN_LTF_FIX__MASK    \
    (0x00000001 << MV_XLG_MAC_PORT_METAL_FIX_EN_LTF_FIX__OFFS)

#define MV_XLG_MAC_PORT_METAL_FIX_EN_HOLD_FIX__OFFS    2
#define MV_XLG_MAC_PORT_METAL_FIX_EN_HOLD_FIX__MASK    \
    (0x00000001 << MV_XLG_MAC_PORT_METAL_FIX_EN_HOLD_FIX__OFFS)

#define MV_XLG_MAC_PORT_METAL_FIX_EN_LED_FIX__OFFS    3
#define MV_XLG_MAC_PORT_METAL_FIX_EN_LED_FIX__MASK    \
    (0x00000001 << MV_XLG_MAC_PORT_METAL_FIX_EN_LED_FIX__OFFS)

#define MV_XLG_MAC_PORT_METAL_FIX_EN_PAD_PROTECT__OFFS    4
#define MV_XLG_MAC_PORT_METAL_FIX_EN_PAD_PROTECT__MASK    \
    (0x00000001 << MV_XLG_MAC_PORT_METAL_FIX_EN_PAD_PROTECT__OFFS)

#define MV_XLG_MAC_PORT_METAL_FIX_EN_NX_BTS44__OFFS    5
#define MV_XLG_MAC_PORT_METAL_FIX_EN_NX_BTS44__MASK    \
    (0x00000001 << MV_XLG_MAC_PORT_METAL_FIX_EN_NX_BTS44__OFFS)

#define MV_XLG_MAC_PORT_METAL_FIX_EN_NX_BTS42__OFFS    6
#define MV_XLG_MAC_PORT_METAL_FIX_EN_NX_BTS42__MASK    \
    (0x00000001 << MV_XLG_MAC_PORT_METAL_FIX_EN_NX_BTS42__OFFS)

#define MV_XLG_MAC_PORT_METAL_FIX_EN_FLUSH_FIX_OFFS    7
#define MV_XLG_MAC_PORT_METAL_FIX_EN_FLUSH_FIX_MASK    \
    (0x00000001 << MV_XLG_MAC_PORT_METAL_FIX_EN_FLUSH_FIX_OFFS)

#define MV_XLG_MAC_PORT_METAL_FIX_EN_PORT_EN_FIX_OFFS    8
#define MV_XLG_MAC_PORT_METAL_FIX_EN_PORT_EN_FIX_MASK    \
    (0x00000001 << MV_XLG_MAC_PORT_METAL_FIX_EN_PORT_EN_FIX_OFFS)

#define MV_XLG_MAC_PORT_METAL_FIX_SPARE_DEF0_BITS_OFFS    9
#define MV_XLG_MAC_PORT_METAL_FIX_SPARE_DEF0_BITS_MASK    \
    (0x0000000f << MV_XLG_MAC_PORT_METAL_FIX_SPARE_DEF0_BITS_OFFS)

#define MV_XLG_MAC_PORT_METAL_FIX_SPARE_DEF1_BITS_OFFS    13
#define MV_XLG_MAC_PORT_METAL_FIX_SPARE_DEF1_BITS_MASK    \
    (0x00000007 << MV_XLG_MAC_PORT_METAL_FIX_SPARE_DEF1_BITS_OFFS)

/* Xg Mib Counters Control */
#define MV_XLG_MIB_CNTRS_CTRL_REG                         (0x0030)
#define MV_XLG_MAC_XG_MIB_CNTRS_CTRL_XGCAPTURETRIGGER_OFFS    0
#define MV_XLG_MAC_XG_MIB_CNTRS_CTRL_XGCAPTURETRIGGER_MASK    \
    (0x00000001 << MV_XLG_MAC_XG_MIB_CNTRS_CTRL_XGCAPTURETRIGGER_OFFS)

#define MV_XLG_MAC_XG_MIB_CNTRS_CTRL_XGDONTCLEARAFTERREAD_OFFS    1
#define MV_XLG_MAC_XG_MIB_CNTRS_CTRL_XGDONTCLEARAFTERREAD_MASK    \
    (0x00000001 << MV_XLG_MAC_XG_MIB_CNTRS_CTRL_XGDONTCLEARAFTERREAD_OFFS)

#define MV_XLG_MAC_XG_MIB_CNTRS_CTRL_XGRXHISTOGRAMEN_OFFS    2
#define MV_XLG_MAC_XG_MIB_CNTRS_CTRL_XGRXHISTOGRAMEN_MASK    \
    (0x00000001 << MV_XLG_MAC_XG_MIB_CNTRS_CTRL_XGRXHISTOGRAMEN_OFFS)

#define MV_XLG_MAC_XG_MIB_CNTRS_CTRL_XGTXHISTOGRAMEN_OFFS    3
#define MV_XLG_MAC_XG_MIB_CNTRS_CTRL_XGTXHISTOGRAMEN_MASK    \
    (0x00000001 << MV_XLG_MAC_XG_MIB_CNTRS_CTRL_XGTXHISTOGRAMEN_OFFS)

#define MV_XLG_MAC_XG_MIB_CNTRS_CTRL_MFA1_BTT940_FIX_ENABLE__OFFS    4
#define MV_XLG_MAC_XG_MIB_CNTRS_CTRL_MFA1_BTT940_FIX_ENABLE__MASK    \
    (0x00000001 << MV_XLG_MAC_XG_MIB_CNTRS_CTRL_MFA1_BTT940_FIX_ENABLE__OFFS)

#define MV_XLG_MAC_XG_MIB_CNTRS_CTRL_LEDS_NUMBER_OFFS    5
#define MV_XLG_MAC_XG_MIB_CNTRS_CTRL_LEDS_NUMBER_MASK    \
    (0x0000003f << MV_XLG_MAC_XG_MIB_CNTRS_CTRL_LEDS_NUMBER_OFFS)

#define MV_XLG_MAC_XG_MIB_CNTRS_CTRL_MIB_4_COUNT_HIST_OFFS    11
#define MV_XLG_MAC_XG_MIB_CNTRS_CTRL_MIB_4_COUNT_HIST_MASK    \
    (0x00000001 << MV_XLG_MAC_XG_MIB_CNTRS_CTRL_MIB_4_COUNT_HIST_OFFS)

#define MV_XLG_MAC_XG_MIB_CNTRS_CTRL_MIB_4_LIMIT_1518_1522_OFFS    12
#define MV_XLG_MAC_XG_MIB_CNTRS_CTRL_MIB_4_LIMIT_1518_1522_MASK    \
    (0x00000001 << MV_XLG_MAC_XG_MIB_CNTRS_CTRL_MIB_4_LIMIT_1518_1522_OFFS)

/* Cn/ccfc Timer%i */
#define MV_XLG_CNCCFC_TIMERI_REG(t)                       ((0x0038 + (t) * 4))
#define MV_XLG_MAC_CNCCFC_TIMERI_PORTSPEEDTIMER_OFFS    0
#define MV_XLG_MAC_CNCCFC_TIMERI_PORTSPEEDTIMER_MASK    \
    (0x0000ffff << MV_XLG_MAC_CNCCFC_TIMERI_PORTSPEEDTIMER_OFFS)

/* Ppfc Control */
#define MV_XLG_MAC_PPFC_CTRL_REG                          (0x0060)
#define MV_XLG_MAC_PPFC_CTRL_GLOBAL_PAUSE_ENI_OFFS    0
#define MV_XLG_MAC_PPFC_CTRL_GLOBAL_PAUSE_ENI_MASK    \
    (0x00000001 << MV_XLG_MAC_PPFC_CTRL_GLOBAL_PAUSE_ENI_OFFS)

#define MV_XLG_MAC_PPFC_CTRL_DIP_BTS_677_EN_OFFS    9
#define MV_XLG_MAC_PPFC_CTRL_DIP_BTS_677_EN_MASK    \
    (0x00000001 << MV_XLG_MAC_PPFC_CTRL_DIP_BTS_677_EN_OFFS)

/* Fc Dsa Tag 0 */
#define MV_XLG_MAC_FC_DSA_TAG_0_REG                       (0x0068)
#define MV_XLG_MAC_FC_DSA_TAG_0_DSATAGREG0_OFFS    0
#define MV_XLG_MAC_FC_DSA_TAG_0_DSATAGREG0_MASK    \
    (0x0000ffff << MV_XLG_MAC_FC_DSA_TAG_0_DSATAGREG0_OFFS)

/* Fc Dsa Tag 1 */
#define MV_XLG_MAC_FC_DSA_TAG_1_REG                       (0x006c)
#define MV_XLG_MAC_FC_DSA_TAG_1_DSATAGREG1_OFFS    0
#define MV_XLG_MAC_FC_DSA_TAG_1_DSATAGREG1_MASK    \
    (0x0000ffff << MV_XLG_MAC_FC_DSA_TAG_1_DSATAGREG1_OFFS)

/* Fc Dsa Tag 2 */
#define MV_XLG_MAC_FC_DSA_TAG_2_REG                       (0x0070)
#define MV_XLG_MAC_FC_DSA_TAG_2_DSATAGREG2_OFFS    0
#define MV_XLG_MAC_FC_DSA_TAG_2_DSATAGREG2_MASK    \
    (0x0000ffff << MV_XLG_MAC_FC_DSA_TAG_2_DSATAGREG2_OFFS)

/* Fc Dsa Tag 3 */
#define MV_XLG_MAC_FC_DSA_TAG_3_REG                       (0x0074)
#define MV_XLG_MAC_FC_DSA_TAG_3_DSATAGREG3_OFFS    0
#define MV_XLG_MAC_FC_DSA_TAG_3_DSATAGREG3_MASK    \
    (0x0000ffff << MV_XLG_MAC_FC_DSA_TAG_3_DSATAGREG3_OFFS)

/* Dic Budget Compensation */
#define MV_XLG_MAC_DIC_BUDGET_COMPENSATION_REG            (0x0080)
#define MV_XLG_MAC_DIC_BUDGET_COMPENSATION_DIC_COUNTER_TO_ADD_8BYTES_OFFS    0
#define MV_XLG_MAC_DIC_BUDGET_COMPENSATION_DIC_COUNTER_TO_ADD_8BYTES_MASK    \
    (0x0000ffff << MV_XLG_MAC_DIC_BUDGET_COMPENSATION_DIC_COUNTER_TO_ADD_8BYTES_OFFS)

/* Port Mac Control4 */
#define MV_XLG_PORT_MAC_CTRL4_REG                         (0x0084)
#define MV_XLG_MAC_CTRL4_LLFC_GLOBAL_FC_ENABLE_OFFS    0
#define MV_XLG_MAC_CTRL4_LLFC_GLOBAL_FC_ENABLE_MASK    \
    (0x00000001 << MV_XLG_MAC_CTRL4_LLFC_GLOBAL_FC_ENABLE_OFFS)

#define MV_XLG_MAC_CTRL4_LED_STREAM_SELECT_OFFS    1
#define MV_XLG_MAC_CTRL4_LED_STREAM_SELECT_MASK    \
    (0x00000001 << MV_XLG_MAC_CTRL4_LED_STREAM_SELECT_OFFS)

#define MV_XLG_MAC_CTRL4_DEBUG_BUS_SELECT_OFFS    2
#define MV_XLG_MAC_CTRL4_DEBUG_BUS_SELECT_MASK    \
    (0x00000001 << MV_XLG_MAC_CTRL4_DEBUG_BUS_SELECT_OFFS)

#define MV_XLG_MAC_CTRL4_MASK_PCS_RESET_OFFS    3
#define MV_XLG_MAC_CTRL4_MASK_PCS_RESET_MASK    \
    (0x00000001 << MV_XLG_MAC_CTRL4_MASK_PCS_RESET_OFFS)

#define MV_XLG_MAC_CTRL4_ENABLE_SHORT_PREAMBLE_FOR_XLG_OFFS    4
#define MV_XLG_MAC_CTRL4_ENABLE_SHORT_PREAMBLE_FOR_XLG_MASK    \
    (0x00000001 << MV_XLG_MAC_CTRL4_ENABLE_SHORT_PREAMBLE_FOR_XLG_OFFS)

#define MV_XLG_MAC_CTRL4_FORWARD_802_3X_FC_EN_OFFS    5
#define MV_XLG_MAC_CTRL4_FORWARD_802_3X_FC_EN_MASK    \
    (0x00000001 << MV_XLG_MAC_CTRL4_FORWARD_802_3X_FC_EN_OFFS)

#define MV_XLG_MAC_CTRL4_FORWARD_PFC_EN_OFFS    6
#define MV_XLG_MAC_CTRL4_FORWARD_PFC_EN_MASK    \
    (0x00000001 << MV_XLG_MAC_CTRL4_FORWARD_PFC_EN_OFFS)

#define MV_XLG_MAC_CTRL4_FORWARD_UNKNOWN_FC_EN_OFFS    7
#define MV_XLG_MAC_CTRL4_FORWARD_UNKNOWN_FC_EN_MASK    \
    (0x00000001 << MV_XLG_MAC_CTRL4_FORWARD_UNKNOWN_FC_EN_OFFS)

#define MV_XLG_MAC_CTRL4_USE_XPCS_OFFS    8
#define MV_XLG_MAC_CTRL4_USE_XPCS_MASK    \
    (0x00000001 << MV_XLG_MAC_CTRL4_USE_XPCS_OFFS)

#define MV_XLG_MAC_CTRL4_DMA_INTERFACE_IS_64_BIT_OFFS    9
#define MV_XLG_MAC_CTRL4_DMA_INTERFACE_IS_64_BIT_MASK    \
    (0x00000001 << MV_XLG_MAC_CTRL4_DMA_INTERFACE_IS_64_BIT_OFFS)

#define MV_XLG_MAC_CTRL4_TX_DMA_INTERFACE_BITS_OFFS    10
#define MV_XLG_MAC_CTRL4_TX_DMA_INTERFACE_BITS_MASK    \
    (0x00000003 << MV_XLG_MAC_CTRL4_TX_DMA_INTERFACE_BITS_OFFS)

#define MV_XLG_MAC_CTRL4_MAC_MODE_DMA_1G_OFFS    12
#define MV_XLG_MAC_CTRL4_MAC_MODE_DMA_1G_MASK    \
    (0x00000001 << MV_XLG_MAC_CTRL4_MAC_MODE_DMA_1G_OFFS)

#define MV_XLG_MAC_CTRL4_EN_IDLE_CHECK_FOR_LINK         14
#define MV_XLG_MAC_CTRL4_EN_IDLE_CHECK_FOR_LINK_MASK    \
    (0x00000001 << MV_XLG_MAC_CTRL4_EN_IDLE_CHECK_FOR_LINK)

/* Port Mac Control5 */
#define MV_XLG_PORT_MAC_CTRL5_REG                         (0x0088)
#define MV_XLG_MAC_CTRL5_TXIPGLENGTH_OFFS    0
#define MV_XLG_MAC_CTRL5_TXIPGLENGTH_MASK    \
    (0x0000000f << MV_XLG_MAC_CTRL5_TXIPGLENGTH_OFFS)

#define MV_XLG_MAC_CTRL5_PREAMBLELENGTHTX_OFFS    4
#define MV_XLG_MAC_CTRL5_PREAMBLELENGTHTX_MASK    \
    (0x00000007 << MV_XLG_MAC_CTRL5_PREAMBLELENGTHTX_OFFS)

#define MV_XLG_MAC_CTRL5_PREAMBLELENGTHRX_OFFS    7
#define MV_XLG_MAC_CTRL5_PREAMBLELENGTHRX_MASK    \
    (0x00000007 << MV_XLG_MAC_CTRL5_PREAMBLELENGTHRX_OFFS)

#define MV_XLG_MAC_CTRL5_TXNUMCRCBYTES_OFFS    10
#define MV_XLG_MAC_CTRL5_TXNUMCRCBYTES_MASK    \
    (0x00000007 << MV_XLG_MAC_CTRL5_TXNUMCRCBYTES_OFFS)

#define MV_XLG_MAC_CTRL5_RXNUMCRCBYTES_OFFS    13
#define MV_XLG_MAC_CTRL5_RXNUMCRCBYTES_MASK    \
    (0x00000007 << MV_XLG_MAC_CTRL5_RXNUMCRCBYTES_OFFS)

/* External Control */
#define MV_XLG_MAC_EXT_CTRL_REG                           (0x0090)
#define MV_XLG_MAC_EXT_CTRL_EXTERNAL_CTRL0_OFFS    0
#define MV_XLG_MAC_EXT_CTRL_EXTERNAL_CTRL0_MASK    \
    (0x00000001 << MV_XLG_MAC_EXT_CTRL_EXTERNAL_CTRL0_OFFS)

#define MV_XLG_MAC_EXT_CTRL_EXTERNAL_CTRL1_OFFS    1
#define MV_XLG_MAC_EXT_CTRL_EXTERNAL_CTRL1_MASK    \
    (0x00000001 << MV_XLG_MAC_EXT_CTRL_EXTERNAL_CTRL1_OFFS)

#define MV_XLG_MAC_EXT_CTRL_EXTERNAL_CTRL2_OFFS    2
#define MV_XLG_MAC_EXT_CTRL_EXTERNAL_CTRL2_MASK    \
    (0x00000001 << MV_XLG_MAC_EXT_CTRL_EXTERNAL_CTRL2_OFFS)

#define MV_XLG_MAC_EXT_CTRL_EXTERNAL_CTRL3_OFFS    3
#define MV_XLG_MAC_EXT_CTRL_EXTERNAL_CTRL3_MASK    \
    (0x00000001 << MV_XLG_MAC_EXT_CTRL_EXTERNAL_CTRL3_OFFS)

#define MV_XLG_MAC_EXT_CTRL_EXTERNAL_CTRL4_OFFS    4
#define MV_XLG_MAC_EXT_CTRL_EXTERNAL_CTRL4_MASK    \
    (0x00000001 << MV_XLG_MAC_EXT_CTRL_EXTERNAL_CTRL4_OFFS)

#define MV_XLG_MAC_EXT_CTRL_EXTERNAL_CTRL5_OFFS    5
#define MV_XLG_MAC_EXT_CTRL_EXTERNAL_CTRL5_MASK    \
    (0x00000001 << MV_XLG_MAC_EXT_CTRL_EXTERNAL_CTRL5_OFFS)

#define MV_XLG_MAC_EXT_CTRL_EXTERNAL_CTRL6_OFFS    6
#define MV_XLG_MAC_EXT_CTRL_EXTERNAL_CTRL6_MASK    \
    (0x00000001 << MV_XLG_MAC_EXT_CTRL_EXTERNAL_CTRL6_OFFS)

#define MV_XLG_MAC_EXT_CTRL_EXTERNAL_CTRL7_OFFS    7
#define MV_XLG_MAC_EXT_CTRL_EXTERNAL_CTRL7_MASK    \
    (0x00000001 << MV_XLG_MAC_EXT_CTRL_EXTERNAL_CTRL7_OFFS)

#define MV_XLG_MAC_EXT_CTRL_EXTERNAL_CTRL8_OFFS    8
#define MV_XLG_MAC_EXT_CTRL_EXTERNAL_CTRL8_MASK    \
    (0x00000001 << MV_XLG_MAC_EXT_CTRL_EXTERNAL_CTRL8_OFFS)

#define MV_XLG_MAC_EXT_CTRL_EXTERNAL_CTRL9_OFFS    9
#define MV_XLG_MAC_EXT_CTRL_EXTERNAL_CTRL9_MASK    \
    (0x00000001 << MV_XLG_MAC_EXT_CTRL_EXTERNAL_CTRL9_OFFS)

#define MV_XLG_MAC_EXT_CTRL_EXT_CTRL_10_OFFS    10
#define MV_XLG_MAC_EXT_CTRL_EXT_CTRL_10_MASK    \
    (0x00000001 << MV_XLG_MAC_EXT_CTRL_EXT_CTRL_10_OFFS)

#define MV_XLG_MAC_EXT_CTRL_EXT_CTRL_11_OFFS    11
#define MV_XLG_MAC_EXT_CTRL_EXT_CTRL_11_MASK    \
    (0x00000001 << MV_XLG_MAC_EXT_CTRL_EXT_CTRL_11_OFFS)

#define MV_XLG_MAC_EXT_CTRL_EXT_CTRL_12_OFFS    12
#define MV_XLG_MAC_EXT_CTRL_EXT_CTRL_12_MASK    \
    (0x00000001 << MV_XLG_MAC_EXT_CTRL_EXT_CTRL_12_OFFS)

#define MV_XLG_MAC_EXT_CTRL_EXT_CTRL_13_OFFS    13
#define MV_XLG_MAC_EXT_CTRL_EXT_CTRL_13_MASK    \
    (0x00000001 << MV_XLG_MAC_EXT_CTRL_EXT_CTRL_13_OFFS)

#define MV_XLG_MAC_EXT_CTRL_EXT_CTRL_14_OFFS    14
#define MV_XLG_MAC_EXT_CTRL_EXT_CTRL_14_MASK    \
    (0x00000001 << MV_XLG_MAC_EXT_CTRL_EXT_CTRL_14_OFFS)

#define MV_XLG_MAC_EXT_CTRL_EXT_CTRL_15_OFFS    15
#define MV_XLG_MAC_EXT_CTRL_EXT_CTRL_15_MASK    \
    (0x00000001 << MV_XLG_MAC_EXT_CTRL_EXT_CTRL_15_OFFS)

/* Macro Control */
#define MV_XLG_MAC_MACRO_CTRL_REG                         (0x0094)
#define MV_XLG_MAC_MACRO_CTRL_MACRO_CTRL_0_OFFS    0
#define MV_XLG_MAC_MACRO_CTRL_MACRO_CTRL_0_MASK    \
    (0x00000001 << MV_XLG_MAC_MACRO_CTRL_MACRO_CTRL_0_OFFS)

#define MV_XLG_MAC_MACRO_CTRL_MACRO_CTRL_1_OFFS    1
#define MV_XLG_MAC_MACRO_CTRL_MACRO_CTRL_1_MASK    \
    (0x00000001 << MV_XLG_MAC_MACRO_CTRL_MACRO_CTRL_1_OFFS)

#define MV_XLG_MAC_MACRO_CTRL_MACRO_CTRL_2_OFFS    2
#define MV_XLG_MAC_MACRO_CTRL_MACRO_CTRL_2_MASK    \
    (0x00000001 << MV_XLG_MAC_MACRO_CTRL_MACRO_CTRL_2_OFFS)

#define MV_XLG_MAC_MACRO_CTRL_MACRO_CTRL_3_OFFS    3
#define MV_XLG_MAC_MACRO_CTRL_MACRO_CTRL_3_MASK    \
    (0x00000001 << MV_XLG_MAC_MACRO_CTRL_MACRO_CTRL_3_OFFS)

#define MV_XLG_MAC_MACRO_CTRL_MACRO_CTRL_4_OFFS    4
#define MV_XLG_MAC_MACRO_CTRL_MACRO_CTRL_4_MASK    \
    (0x00000001 << MV_XLG_MAC_MACRO_CTRL_MACRO_CTRL_4_OFFS)

#define MV_XLG_MAC_MACRO_CTRL_MACRO_CTRL_5_OFFS    5
#define MV_XLG_MAC_MACRO_CTRL_MACRO_CTRL_5_MASK    \
    (0x00000001 << MV_XLG_MAC_MACRO_CTRL_MACRO_CTRL_5_OFFS)

#define MV_XLG_MAC_MACRO_CTRL_MACRO_CTRL_6_OFFS    6
#define MV_XLG_MAC_MACRO_CTRL_MACRO_CTRL_6_MASK    \
    (0x00000001 << MV_XLG_MAC_MACRO_CTRL_MACRO_CTRL_6_OFFS)

#define MV_XLG_MAC_MACRO_CTRL_MACRO_CTRL_7_OFFS    7
#define MV_XLG_MAC_MACRO_CTRL_MACRO_CTRL_7_MASK    \
    (0x00000001 << MV_XLG_MAC_MACRO_CTRL_MACRO_CTRL_7_OFFS)

#define MV_XLG_MAC_MACRO_CTRL_MACRO_CTRL_8_OFFS    8
#define MV_XLG_MAC_MACRO_CTRL_MACRO_CTRL_8_MASK    \
    (0x00000001 << MV_XLG_MAC_MACRO_CTRL_MACRO_CTRL_8_OFFS)

#define MV_XLG_MAC_MACRO_CTRL_MACRO_CTRL_9_OFFS    9
#define MV_XLG_MAC_MACRO_CTRL_MACRO_CTRL_9_MASK    \
    (0x00000001 << MV_XLG_MAC_MACRO_CTRL_MACRO_CTRL_9_OFFS)

#define MV_XLG_MAC_MACRO_CTRL_MACRO_CTRL_10_OFFS    10
#define MV_XLG_MAC_MACRO_CTRL_MACRO_CTRL_10_MASK    \
    (0x00000001 << MV_XLG_MAC_MACRO_CTRL_MACRO_CTRL_10_OFFS)

#define MV_XLG_MAC_MACRO_CTRL_MACRO_CTRL_11_OFFS    11
#define MV_XLG_MAC_MACRO_CTRL_MACRO_CTRL_11_MASK    \
    (0x00000001 << MV_XLG_MAC_MACRO_CTRL_MACRO_CTRL_11_OFFS)

#define MV_XLG_MAC_MACRO_CTRL_MACRO_CTRL_12_OFFS    12
#define MV_XLG_MAC_MACRO_CTRL_MACRO_CTRL_12_MASK    \
    (0x00000001 << MV_XLG_MAC_MACRO_CTRL_MACRO_CTRL_12_OFFS)

#define MV_XLG_MAC_MACRO_CTRL_MACRO_CTRL_13_OFFS    13
#define MV_XLG_MAC_MACRO_CTRL_MACRO_CTRL_13_MASK    \
    (0x00000001 << MV_XLG_MAC_MACRO_CTRL_MACRO_CTRL_13_OFFS)

#define MV_XLG_MAC_MACRO_CTRL_MACRO_CTRL_14_OFFS    14
#define MV_XLG_MAC_MACRO_CTRL_MACRO_CTRL_14_MASK    \
    (0x00000001 << MV_XLG_MAC_MACRO_CTRL_MACRO_CTRL_14_OFFS)

#define MV_XLG_MAC_MACRO_CTRL_MACRO_CTRL_15_OFFS    15
#define MV_XLG_MAC_MACRO_CTRL_MACRO_CTRL_15_MASK    \
    (0x00000001 << MV_XLG_MAC_MACRO_CTRL_MACRO_CTRL_15_OFFS)

#define MV_XLG_MAC_DIC_PPM_IPG_REDUCE_REG                 (0x0094)

/* Port Interrupt Cause */
#define MV_XLG_INTERRUPT_CAUSE_REG                        (0x0014)
/* Port Interrupt Mask */
#define MV_XLG_INTERRUPT_MASK_REG                         (0x0018)
#define MV_XLG_SUMMARY_INTERRUPT_OFFSET      0
#define MV_XLG_SUMMARY_INTERRUPT_MASK        \
    (0x1 << MV_XLG_SUMMARY_INTERRUPT_OFFSET)
#define MV_XLG_INTERRUPT_LINK_CHANGE_OFFS    1
#define MV_XLG_INTERRUPT_LINK_CHANGE_MASK    \
    (0x1 << MV_XLG_INTERRUPT_LINK_CHANGE_OFFS)

/* Port Interrupt Summary Cause */
#define MV_XLG_EXTERNAL_INTERRUPT_CAUSE_REG               (0x0058)
/* Port Interrupt Summary Mask */
#define MV_XLG_EXTERNAL_INTERRUPT_MASK_REG                (0x005C)
#define MV_XLG_EXTERNAL_INTERRUPT_LINK_CHANGE_XLG_OFFS    1
#define MV_XLG_EXTERNAL_INTERRUPT_LINK_CHANGE_XLG_MASK    \
    (0x1 << MV_XLG_EXTERNAL_INTERRUPT_LINK_CHANGE_XLG_OFFS)
#define MV_XLG_EXTERNAL_INTERRUPT_LINK_CHANGE_GIG_OFFS    2
#define MV_XLG_EXTERNAL_INTERRUPT_LINK_CHANGE_GIG_MASK    \
    (0x1 << MV_XLG_EXTERNAL_INTERRUPT_LINK_CHANGE_GIG_OFFS)

/*All PPV22 Addresses are 40-bit */
#define MVPP22_ADDR_HIGH_SIZE    8
#define MVPP22_ADDR_HIGH_MASK    ((1<<MVPP22_ADDR_HIGH_SIZE) - 1)
#define MVPP22_ADDR_MASK    (0xFFFFFFFFFF)

/* Desc addr shift */
#define MVPP21_DESC_ADDR_SHIFT    0 /*Applies to RXQ, AGGR_TXQ*/
#define MVPP22_DESC_ADDR_SHIFT    8 /*Applies to RXQ, AGGR_TXQ*/

/* AXI Bridge Registers */
#define MVPP22_AXI_BM_WR_ATTR_REG                         0x4100
#define MVPP22_AXI_BM_RD_ATTR_REG                         0x4104
#define MVPP22_AXI_AGGRQ_DESCR_RD_ATTR_REG                0x4110
#define MVPP22_AXI_TXQ_DESCR_WR_ATTR_REG                  0x4114
#define MVPP22_AXI_TXQ_DESCR_RD_ATTR_REG                  0x4118
#define MVPP22_AXI_RXQ_DESCR_WR_ATTR_REG                  0x411c
#define MVPP22_AXI_RX_DATA_WR_ATTR_REG                    0x4120
#define MVPP22_AXI_TX_DATA_RD_ATTR_REG                    0x4130
#define MVPP22_AXI_RD_NORMAL_CODE_REG                     0x4150
#define MVPP22_AXI_RD_SNOOP_CODE_REG                      0x4154
#define MVPP22_AXI_WR_NORMAL_CODE_REG                     0x4160
#define MVPP22_AXI_WR_SNOOP_CODE_REG                      0x4164

#define MVPP22_AXI_ATTR_CACHE_OFFS                        0
#define MVPP22_AXI_ATTR_DOMAIN_OFFS                       12

#define MVPP22_AXI_CODE_CACHE_OFFS                        0
#define MVPP22_AXI_CODE_DOMAIN_OFFS                       4

#define MVPP22_AXI_CODE_CACHE_NON_CACHE                   0x3
#define MVPP22_AXI_CODE_CACHE_WR_CACHE                    0x7
#define MVPP22_AXI_CODE_CACHE_RD_CACHE                    0xb

#define MVPP22_AXI_CODE_DOMAIN_OUTER_DOM                  2
#define MVPP22_AXI_CODE_DOMAIN_SYSTEM                     3

/* PHY address register */
#define MV_SMI_PHY_ADDRESS_REG(n)                         (0xC + 0x4 * (n))
#define MV_SMI_PHY_ADDRESS_PHYAD_OFFS    0
#define MV_SMI_PHY_ADDRESS_PHYAD_MASK    \
  (0x1F << MV_SMI_PHY_ADDRESS_PHYAD_OFFS)

/* Marvell tag types */
enum Mvpp2TagType {
  MVPP2_TAG_TYPE_NONE = 0,
  MVPP2_TAG_TYPE_MH   = 1,
  MVPP2_TAG_TYPE_DSA  = 2,
  MVPP2_TAG_TYPE_EDSA = 3,
  MVPP2_TAG_TYPE_VLAN = 4,
  MVPP2_TAG_TYPE_LAST = 5
};

/* Parser constants */
#define MVPP2_PRS_TCAM_SRAM_SIZE    256
#define MVPP2_PRS_TCAM_WORDS    6
#define MVPP2_PRS_SRAM_WORDS    4
#define MVPP2_PRS_FLOW_ID_SIZE    64
#define MVPP2_PRS_FLOW_ID_MASK    0x3f
#define MVPP2_PRS_TCAM_ENTRY_INVALID    1
#define MVPP2_PRS_TCAM_DSA_TAGGED_BIT    BIT(5)
#define MVPP2_PRS_IPV4_HEAD    0x40
#define MVPP2_PRS_IPV4_HEAD_MASK    0xf0
#define MVPP2_PRS_IPV4_MC    0xe0
#define MVPP2_PRS_IPV4_MC_MASK    0xf0
#define MVPP2_PRS_IPV4_BC_MASK    0xff
#define MVPP2_PRS_IPV4_IHL    0x5
#define MVPP2_PRS_IPV4_IHL_MASK    0xf
#define MVPP2_PRS_IPV6_MC    0xff
#define MVPP2_PRS_IPV6_MC_MASK    0xff
#define MVPP2_PRS_IPV6_HOP_MASK    0xff
#define MVPP2_PRS_TCAM_PROTO_MASK    0xff
#define MVPP2_PRS_TCAM_PROTO_MASK_L    0x3f
#define MVPP2_PRS_DBL_VLANS_MAX    100

/*
 * Tcam structure:
 * - lookup ID - 4 bits
 * - port ID - 1 byte
 * - additional information - 1 byte
 * - header data - 8 bytes
 * The fields are represented by MVPP2_PRS_TCAM_DATA_REG(5)->(0).
 */
#define MVPP2_PRS_AI_BITS    8
#define MVPP2_PRS_PORT_MASK    0xff
#define MVPP2_PRS_LU_MASK    0xf
#define MVPP2_PRS_TCAM_DATA_BYTE(offs)    (((offs) - ((offs) % 2)) * 2 + ((offs) % 2))
#define MVPP2_PRS_TCAM_DATA_BYTE_EN(offs)    (((offs) * 2) - ((offs) % 2)  + 2)
#define MVPP2_PRS_TCAM_AI_BYTE    16
#define MVPP2_PRS_TCAM_PORT_BYTE    17
#define MVPP2_PRS_TCAM_LU_BYTE    20
#define MVPP2_PRS_TCAM_EN_OFFS(offs)    ((offs) + 2)
#define MVPP2_PRS_TCAM_INV_WORD    5
/* Tcam entries ID */
#define MVPP2_PE_DROP_ALL    0
#define MVPP2_PE_FIRST_FREE_TID    1
#define MVPP2_PE_LAST_FREE_TID    (MVPP2_PRS_TCAM_SRAM_SIZE - 31)
#define MVPP2_PE_IP6_EXT_PROTO_UN    (MVPP2_PRS_TCAM_SRAM_SIZE - 30)
#define MVPP2_PE_MAC_MC_IP6    (MVPP2_PRS_TCAM_SRAM_SIZE - 29)
#define MVPP2_PE_IP6_ADDR_UN    (MVPP2_PRS_TCAM_SRAM_SIZE - 28)
#define MVPP2_PE_IP4_ADDR_UN    (MVPP2_PRS_TCAM_SRAM_SIZE - 27)
#define MVPP2_PE_LAST_DEFAULT_FLOW    (MVPP2_PRS_TCAM_SRAM_SIZE - 26)
#define MVPP2_PE_FIRST_DEFAULT_FLOW    (MVPP2_PRS_TCAM_SRAM_SIZE - 19)
#define MVPP2_PE_EDSA_TAGGED    (MVPP2_PRS_TCAM_SRAM_SIZE - 18)
#define MVPP2_PE_EDSA_UNTAGGED    (MVPP2_PRS_TCAM_SRAM_SIZE - 17)
#define MVPP2_PE_DSA_TAGGED    (MVPP2_PRS_TCAM_SRAM_SIZE - 16)
#define MVPP2_PE_DSA_UNTAGGED    (MVPP2_PRS_TCAM_SRAM_SIZE - 15)
#define MVPP2_PE_ETYPE_EDSA_TAGGED    (MVPP2_PRS_TCAM_SRAM_SIZE - 14)
#define MVPP2_PE_ETYPE_EDSA_UNTAGGED    (MVPP2_PRS_TCAM_SRAM_SIZE - 13)
#define MVPP2_PE_ETYPE_DSA_TAGGED    (MVPP2_PRS_TCAM_SRAM_SIZE - 12)
#define MVPP2_PE_ETYPE_DSA_UNTAGGED    (MVPP2_PRS_TCAM_SRAM_SIZE - 11)
#define MVPP2_PE_MH_DEFAULT    (MVPP2_PRS_TCAM_SRAM_SIZE - 10)
#define MVPP2_PE_DSA_DEFAULT    (MVPP2_PRS_TCAM_SRAM_SIZE - 9)
#define MVPP2_PE_IP6_PROTO_UN    (MVPP2_PRS_TCAM_SRAM_SIZE - 8)
#define MVPP2_PE_IP4_PROTO_UN    (MVPP2_PRS_TCAM_SRAM_SIZE - 7)
#define MVPP2_PE_ETH_TYPE_UN    (MVPP2_PRS_TCAM_SRAM_SIZE - 6)
#define MVPP2_PE_VLAN_DBL    (MVPP2_PRS_TCAM_SRAM_SIZE - 5)
#define MVPP2_PE_VLAN_NONE    (MVPP2_PRS_TCAM_SRAM_SIZE - 4)
#define MVPP2_PE_MAC_MC_ALL    (MVPP2_PRS_TCAM_SRAM_SIZE - 3)
#define MVPP2_PE_MAC_PROMISCUOUS    (MVPP2_PRS_TCAM_SRAM_SIZE - 2)
#define MVPP2_PE_MAC_NON_PROMISCUOUS    (MVPP2_PRS_TCAM_SRAM_SIZE - 1)

/*
 * Sram structure
 * The fields are represented by MVPP2_PRS_TCAM_DATA_REG(3)->(0).
 */
#define MVPP2_PRS_SRAM_RI_OFFS    0
#define MVPP2_PRS_SRAM_RI_WORD    0
#define MVPP2_PRS_SRAM_RI_CTRL_OFFS    32
#define MVPP2_PRS_SRAM_RI_CTRL_WORD    1
#define MVPP2_PRS_SRAM_RI_CTRL_BITS    32
#define MVPP2_PRS_SRAM_SHIFT_OFFS    64
#define MVPP2_PRS_SRAM_SHIFT_SIGN_BIT    72
#define MVPP2_PRS_SRAM_SHIFT_MASK    0xff
#define MVPP2_PRS_SRAM_UDF_OFFS    73
#define MVPP2_PRS_SRAM_UDF_BITS    8
#define MVPP2_PRS_SRAM_UDF_MASK    0xff
#define MVPP2_PRS_SRAM_UDF_SIGN_BIT    81
#define MVPP2_PRS_SRAM_UDF_TYPE_OFFS    82
#define MVPP2_PRS_SRAM_UDF_TYPE_MASK    0x7
#define MVPP2_PRS_SRAM_UDF_TYPE_L3    1
#define MVPP2_PRS_SRAM_UDF_TYPE_L4    4
#define MVPP2_PRS_SRAM_OP_SEL_SHIFT_OFFS    85
#define MVPP2_PRS_SRAM_OP_SEL_SHIFT_MASK    0x3
#define MVPP2_PRS_SRAM_OP_SEL_SHIFT_ADD    1
#define MVPP2_PRS_SRAM_OP_SEL_SHIFT_IP4_ADD    2
#define MVPP2_PRS_SRAM_OP_SEL_SHIFT_IP6_ADD    3
#define MVPP2_PRS_SRAM_OP_SEL_UDF_OFFS    87
#define MVPP2_PRS_SRAM_OP_SEL_UDF_BITS    2
#define MVPP2_PRS_SRAM_OP_SEL_UDF_MASK    0x3
#define MVPP2_PRS_SRAM_OP_SEL_UDF_ADD    0
#define MVPP2_PRS_SRAM_OP_SEL_UDF_IP4_ADD    2
#define MVPP2_PRS_SRAM_OP_SEL_UDF_IP6_ADD    3
#define MVPP2_PRS_SRAM_OP_SEL_BASE_OFFS    89
#define MVPP2_PRS_SRAM_AI_OFFS    90
#define MVPP2_PRS_SRAM_AI_CTRL_OFFS    98
#define MVPP2_PRS_SRAM_AI_CTRL_BITS    8
#define MVPP2_PRS_SRAM_AI_MASK    0xff
#define MVPP2_PRS_SRAM_NEXT_LU_OFFS    106
#define MVPP2_PRS_SRAM_NEXT_LU_MASK    0xf
#define MVPP2_PRS_SRAM_LU_DONE_BIT    110
#define MVPP2_PRS_SRAM_LU_GEN_BIT    111

/* Sram result info bits assignment */
#define MVPP2_PRS_RI_MAC_ME_MASK    0x1
#define MVPP2_PRS_RI_DSA_MASK    0x2
#define MVPP2_PRS_RI_VLAN_MASK    0xc
#define MVPP2_PRS_RI_VLAN_NONE    ~(BIT(2) | BIT(3))
#define MVPP2_PRS_RI_VLAN_SINGLE    BIT(2)
#define MVPP2_PRS_RI_VLAN_DOUBLE    BIT(3)
#define MVPP2_PRS_RI_VLAN_TRIPLE    (BIT(2) | BIT(3))
#define MVPP2_PRS_RI_CPU_CODE_MASK    0x70
#define MVPP2_PRS_RI_CPU_CODE_RX_SPEC    BIT(4)
#define MVPP2_PRS_RI_L2_CAST_MASK    0x600
#define MVPP2_PRS_RI_L2_UCAST    ~(BIT(9) | BIT(10))
#define MVPP2_PRS_RI_L2_MCAST    BIT(9)
#define MVPP2_PRS_RI_L2_BCAST    BIT(10)
#define MVPP2_PRS_RI_PPPOE_MASK    0x800
#define MVPP2_PRS_RI_L3_PROTO_MASK    0x7000
#define MVPP2_PRS_RI_L3_UN    ~(BIT(12) | BIT(13) | BIT(14))
#define MVPP2_PRS_RI_L3_IP4    BIT(12)
#define MVPP2_PRS_RI_L3_IP4_OPT    BIT(13)
#define MVPP2_PRS_RI_L3_IP4_OTHER    (BIT(12) | BIT(13))
#define MVPP2_PRS_RI_L3_IP6    BIT(14)
#define MVPP2_PRS_RI_L3_IP6_EXT    (BIT(12) | BIT(14))
#define MVPP2_PRS_RI_L3_ARP    (BIT(13) | BIT(14))
#define MVPP2_PRS_RI_L3_ADDR_MASK    0x18000
#define MVPP2_PRS_RI_L3_UCAST    ~(BIT(15) | BIT(16))
#define MVPP2_PRS_RI_L3_MCAST    BIT(15)
#define MVPP2_PRS_RI_L3_BCAST    (BIT(15) | BIT(16))
#define MVPP2_PRS_RI_IP_FRAG_MASK    0x20000
#define MVPP2_PRS_RI_UDF3_MASK    0x300000
#define MVPP2_PRS_RI_UDF3_RX_SPECIAL    BIT(21)
#define MVPP2_PRS_RI_L4_PROTO_MASK    0x1c00000
#define MVPP2_PRS_RI_L4_TCP    BIT(22)
#define MVPP2_PRS_RI_L4_UDP    BIT(23)
#define MVPP2_PRS_RI_L4_OTHER    (BIT(22) | BIT(23))
#define MVPP2_PRS_RI_UDF7_MASK    0x60000000
#define MVPP2_PRS_RI_UDF7_IP6_LITE    BIT(29)
#define MVPP2_PRS_RI_DROP_MASK    0x80000000

/* Sram additional info bits assignment */
#define MVPP2_PRS_IPV4_DIP_AI_BIT    BIT(0)
#define MVPP2_PRS_IPV6_NO_EXT_AI_BIT    BIT(0)
#define MVPP2_PRS_IPV6_EXT_AI_BIT    BIT(1)
#define MVPP2_PRS_IPV6_EXT_AH_AI_BIT    BIT(2)
#define MVPP2_PRS_IPV6_EXT_AH_LEN_AI_BIT    BIT(3)
#define MVPP2_PRS_IPV6_EXT_AH_L4_AI_BIT    BIT(4)
#define MVPP2_PRS_SINGLE_VLAN_AI    0
#define MVPP2_PRS_DBL_VLAN_AI_BIT    BIT(7)

/* DSA/EDSA type */
#define MVPP2_PRS_TAGGED    1
#define MVPP2_PRS_UNTAGGED    0
#define MVPP2_PRS_EDSA    1
#define MVPP2_PRS_DSA    0

/* MAC entries, shadow udf */
enum Mvpp2PrsUdf {
  MVPP2_PRS_UDF_MAC_DEF,
  MVPP2_PRS_UDF_MAC_RANGE,
  MVPP2_PRS_UDF_L2_DEF,
  MVPP2_PRS_UDF_L2_DEF_COPY,
  MVPP2_PRS_UDF_L2_USER,
};

/* Lookup ID */
enum Mvpp2PrsLookup {
  MVPP2_PRS_LU_MH,
  MVPP2_PRS_LU_MAC,
  MVPP2_PRS_LU_DSA,
  MVPP2_PRS_LU_VLAN,
  MVPP2_PRS_LU_L2,
  MVPP2_PRS_LU_PPPOE,
  MVPP2_PRS_LU_IP4,
  MVPP2_PRS_LU_IP6,
  MVPP2_PRS_LU_FLOWS,
  MVPP2_PRS_LU_LAST,
};

/* L3 cast enum */
enum Mvpp2PrsL3Cast {
  MVPP2_PRS_L3_UNI_CAST,
  MVPP2_PRS_L3_MULTI_CAST,
  MVPP2_PRS_L3_BROAD_CAST
};

/* Classifier constants */
#define MVPP2_CLS_FLOWS_TBL_SIZE    512
#define MVPP2_CLS_FLOWS_TBL_DATA_WORDS    3
#define MVPP2_CLS_LKP_TBL_SIZE    64

/* BM cookie (32 bits) definition */
#define MVPP2_BM_COOKIE_POOL_OFFS    8
#define MVPP2_BM_COOKIE_CPU_OFFS    24

/*
 * The MVPP2_TX_DESC and MVPP2_RX_DESC structures describe the
 * layout of the transmit and reception DMA descriptors, and their
 * layout is therefore defined by the hardware design
 */
#define MVPP2_TXD_L3_OFF_SHIFT    0
#define MVPP2_TXD_IP_HLEN_SHIFT    8
#define MVPP2_TXD_L4_CSUM_FRAG    BIT(13)
#define MVPP2_TXD_L4_CSUM_NOT    BIT(14)
#define MVPP2_TXD_IP_CSUM_DISABLE    BIT(15)
#define MVPP2_TXD_PADDING_DISABLE    BIT(23)
#define MVPP2_TXD_L4_UDP    BIT(24)
#define MVPP2_TXD_L3_IP6    BIT(26)
#define MVPP2_TXD_L_DESC    BIT(28)
#define MVPP2_TXD_F_DESC    BIT(29)

#define MVPP2_RXD_ERR_SUMMARY    BIT(15)
#define MVPP2_RXD_ERR_CODE_MASK    (BIT(13) | BIT(14))
#define MVPP2_RXD_ERR_CRC    0x0
#define MVPP2_RXD_ERR_OVERRUN    BIT(13)
#define MVPP2_RXD_ERR_RESOURCE    (BIT(13) | BIT(14))
#define MVPP2_RXD_BM_POOL_ID_OFFS    16
#define MVPP2_RXD_BM_POOL_ID_MASK    (BIT(16) | BIT(17) | BIT(18))
#define MVPP2_RXD_HWF_SYNC    BIT(21)
#define MVPP2_RXD_L4_CSUM_OK    BIT(22)
#define MVPP2_RXD_IP4_HEADER_ERR    BIT(24)
#define MVPP2_RXD_L4_TCP    BIT(25)
#define MVPP2_RXD_L4_UDP    BIT(26)
#define MVPP2_RXD_L3_IP4    BIT(28)
#define MVPP2_RXD_L3_IP6    BIT(30)
#define MVPP2_RXD_BUF_HDR    BIT(31)

struct mvpp2_tx_desc {
  uint32_t command;   /* Options used by HW for packet transmitting.*/
  uint8_t  packet_offset; /* the offset from the buffer beginning */
  uint8_t  phys_txq;    /* destination queue ID     */
  uint16_t data_size;   /* data size of transmitted packet in bytes */
  uint64_t rsrvd_hw_cmd1; /* HwCmd (BM, PON, PNC) */
  uint64_t buf_phys_addr_hw_cmd2;
  uint64_t buf_cookie_bm_qset_hw_cmd3;
};

struct mvpp2_rx_desc {
  uint32_t status;    /* info about received packet   */
  uint16_t reserved1;   /* ParserInfo (for future use, PnC)  */
  uint16_t data_size;   /* size of received packet in bytes */
  uint16_t rsrvd_gem;   /* GemPortId (for future use, PON)  */
  uint16_t rsrvd_l4_csum;  /* CsumL4 (for future use, PnC)  */
  uint32_t rsrvd_timestamp;
  uint64_t buf_phys_addr_key_hash;
  uint64_t buf_cookie_bm_qset_cls_info;
};

union mvpp2_prs_tcam_entry {
  uint32_t word[MVPP2_PRS_TCAM_WORDS];
  uint8_t byte[MVPP2_PRS_TCAM_WORDS * 4];
};

union mvpp2_prs_sram_entry {
  uint32_t word[MVPP2_PRS_SRAM_WORDS];
  uint8_t byte[MVPP2_PRS_SRAM_WORDS * 4];
};

struct mvpp2_prs_entry {
 uint32_t index;
 union mvpp2_prs_tcam_entry tcam;
 union mvpp2_prs_sram_entry sram;
};

struct mvpp2_prs_shadow {
  int valid;
  int finish;

  /* Lookup ID */
  int32_t lu;

  /* User defined offset */
  int32_t udf;

  /* Result info */
  uint32_t ri;
  uint32_t ri_mask;
};

struct mvpp2_cls_flow_entry {
  uint32_t index;
  uint32_t data[MVPP2_CLS_FLOWS_TBL_DATA_WORDS];
};

struct mvpp2_cls_lookup_entry {
  uint32_t lkpid;
  uint32_t way;
  uint32_t data;
};

typedef struct {
  uint32_t NextBuffPhysAddr;
  uint32_t NextBuffVirtAddr;
  uint16_t ByteCount;
  uint16_t info;
  uint8_t  reserved1;   /* BmQset (for future use, BM)   */
} MVPP2_BUFF_HDR;

/* Buffer header info bits */
#define MVPP2_B_HDR_INFO_MC_ID_MASK    0xfff
#define MVPP2_B_HDR_INFO_MC_ID(info)    ((info) & MVPP2_B_HDR_INFO_MC_ID_MASK)
#define MVPP2_B_HDR_INFO_LAST_OFFS    12
#define MVPP2_B_HDR_INFO_LAST_MASK    BIT(12)
#define MVPP2_B_HDR_INFO_IS_LAST(info)    ((info & MVPP2_B_HDR_INFO_LAST_MASK) >> MVPP2_B_HDR_INFO_LAST_OFFS)

/* SerDes */
#define MVPP2_SFI_LANE_COUNT          1

/* Net Complex */
enum MvNetcTopology {
  MV_NETC_GE_MAC0_RXAUI_L23 = BIT(0),
  MV_NETC_GE_MAC0_RXAUI_L45 = BIT(1),
  MV_NETC_GE_MAC0_XAUI = BIT(2),
  MV_NETC_GE_MAC2_SGMII = BIT(3),
  MV_NETC_GE_MAC3_SGMII = BIT(4),
  MV_NETC_GE_MAC3_RGMII = BIT(5),
};

enum MvNetcPhase {
  MV_NETC_FIRST_PHASE,
  MV_NETC_SECOND_PHASE,
};

enum MvNetcSgmiiXmiMode {
  MV_NETC_GBE_SGMII,
  MV_NETC_GBE_XMII,
};

enum MvNetcMiiMode {
  MV_NETC_GBE_RGMII,
  MV_NETC_GBE_MII,
};

enum MvNetcLanes {
  MV_NETC_LANE_23,
  MV_NETC_LANE_45,
};

/* Port related */
enum MvReset {
  RESET,
  UNRESET
};

enum Mvpp2Command {
  MVPP2_START,    /* Start     */
  MVPP2_STOP,    /* Stop     */
  MVPP2_PAUSE,    /* Pause    */
  MVPP2_RESTART    /* Restart  */
};

enum MvPortDuplex {
  MV_PORT_DUPLEX_AN,
  MV_PORT_DUPLEX_HALF,
  MV_PORT_DUPLEX_FULL
};

#endif /* __MVPP2_LIB_HW__ */
