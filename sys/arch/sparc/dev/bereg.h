/*	$OpenBSD: bereg.h,v 1.2 1998/07/05 09:25:55 deraadt Exp $	*/

/*
 * Copyright (c) 1998 Theo de Raadt.  All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

struct be_bregs {
	volatile u_int32_t xif_cfg;          /* XIF config register */
	volatile u_int32_t _unused[63];      /* Reserved... */
	volatile u_int32_t status;           /* Status register, clear on read */
	volatile u_int32_t imask;            /* Interrupt mask register */
	volatile u_int32_t _unused2[64];     /* Reserved... */
	volatile u_int32_t tx_swreset;       /* Transmitter software reset */
	volatile u_int32_t tx_cfg;           /* Transmitter config register */
	volatile u_int32_t ipkt_gap1;        /* Inter-packet gap 1 */
	volatile u_int32_t ipkt_gap2;        /* Inter-packet gap 2 */
	volatile u_int32_t attempt_limit;    /* Transmit attempt limit */
	volatile u_int32_t stime;            /* Transmit slot time */
	volatile u_int32_t preamble_len;     /* Size of transmit preamble */
	volatile u_int32_t preamble_pattern; /* Pattern for transmit preamble */
	volatile u_int32_t tx_sframe_delim;  /* Transmit delimiter */
	volatile u_int32_t jsize;            /* Toe jam... */
	volatile u_int32_t tx_pkt_max;       /* Transmit max pkt size */
	volatile u_int32_t tx_pkt_min;       /* Transmit min pkt size */
	volatile u_int32_t peak_attempt;     /* Count of transmit peak attempts */
	volatile u_int32_t dt_ctr;           /* Transmit defer timer */
	volatile u_int32_t nc_ctr;           /* Transmit normal-collision counter */
	volatile u_int32_t fc_ctr;           /* Transmit first-collision counter */
	volatile u_int32_t ex_ctr;           /* Transmit excess-collision counter */
	volatile u_int32_t lt_ctr;           /* Transmit late-collision counter */
	volatile u_int32_t rand_seed;        /* Transmit random number seed */
	volatile u_int32_t tx_smachine;      /* Transmit state machine */
	volatile u_int32_t _unused3[44];     /* Reserved */
	volatile u_int32_t rx_swreset;       /* Receiver software reset */
	volatile u_int32_t rx_cfg;           /* Receiver config register */
	volatile u_int32_t rx_pkt_max;       /* Receive max pkt size */
	volatile u_int32_t rx_pkt_min;       /* Receive min pkt size */
	volatile u_int32_t mac_addr2;        /* Ether address register 2 */
	volatile u_int32_t mac_addr1;        /* Ether address register 1 */
	volatile u_int32_t mac_addr0;        /* Ether address register 0 */
	volatile u_int32_t fr_ctr;           /* Receive frame receive counter */
	volatile u_int32_t gle_ctr;          /* Receive giant-length error counter */
	volatile u_int32_t unale_ctr;        /* Receive unaligned error counter */
	volatile u_int32_t rcrce_ctr;        /* Receive CRC error counter */
	volatile u_int32_t rx_smachine;      /* Receiver state machine */
	volatile u_int32_t rx_cvalid;        /* Receiver code violation */
	volatile u_int32_t _unused4;         /* Reserved... */
	volatile u_int32_t htable3;          /* Hash table 3 */
	volatile u_int32_t htable2;          /* Hash table 2 */
	volatile u_int32_t htable1;          /* Hash table 1 */
	volatile u_int32_t htable0;          /* Hash table 0 */
	volatile u_int32_t afilter2;         /* Address filter 2 */
	volatile u_int32_t afilter1;         /* Address filter 1 */
	volatile u_int32_t afilter0;         /* Address filter 0 */
	volatile u_int32_t afilter_mask;     /* Address filter mask */
};

/* BE XIF config register. */
#define BE_XCFG_ODENABLE   0x00000001 /* Output driver enable */
#define BE_XCFG_RESV       0x00000002 /* Reserved, write always as 1 */
#define BE_XCFG_MLBACK     0x00000004 /* Loopback-mode MII enable */
#define BE_XCFG_SMODE      0x00000008 /* Enable serial mode */

/* BE status register. */
#define BE_STAT_GOTFRAME   0x00000001 /* Received a frame */
#define BE_STAT_RCNTEXP    0x00000002 /* Receive frame counter expired */
#define BE_STAT_ACNTEXP    0x00000004 /* Align-error counter expired */
#define BE_STAT_CCNTEXP    0x00000008 /* CRC-error counter expired */
#define BE_STAT_LCNTEXP    0x00000010 /* Length-error counter expired */
#define BE_STAT_RFIFOVF    0x00000020 /* Receive FIFO overflow */
#define BE_STAT_CVCNTEXP   0x00000040 /* Code-violation counter expired */
#define BE_STAT_SENTFRAME  0x00000100 /* Transmitted a frame */
#define BE_STAT_TFIFO_UND  0x00000200 /* Transmit FIFO underrun */
#define BE_STAT_MAXPKTERR  0x00000400 /* Max-packet size error */
#define BE_STAT_NCNTEXP    0x00000800 /* Normal-collision counter expired */
#define BE_STAT_ECNTEXP    0x00001000 /* Excess-collision counter expired */
#define BE_STAT_LCCNTEXP   0x00002000 /* Late-collision counter expired */
#define BE_STAT_FCNTEXP    0x00004000 /* First-collision counter expired */
#define BE_STAT_DTIMEXP    0x00008000 /* Defer-timer expired */

/* BE interrupt mask register. */
#define BE_IMASK_GOTFRAME  0x00000001 /* Received a frame */
#define BE_IMASK_RCNTEXP   0x00000002 /* Receive frame counter expired */
#define BE_IMASK_ACNTEXP   0x00000004 /* Align-error counter expired */
#define BE_IMASK_CCNTEXP   0x00000008 /* CRC-error counter expired */
#define BE_IMASK_LCNTEXP   0x00000010 /* Length-error counter expired */
#define BE_IMASK_RFIFOVF   0x00000020 /* Receive FIFO overflow */
#define BE_IMASK_CVCNTEXP  0x00000040 /* Code-violation counter expired */
#define BE_IMASK_SENTFRAME 0x00000100 /* Transmitted a frame */
#define BE_IMASK_TFIFO_UND 0x00000200 /* Transmit FIFO underrun */
#define BE_IMASK_MAXPKTERR 0x00000400 /* Max-packet size error */
#define BE_IMASK_NCNTEXP   0x00000800 /* Normal-collision counter expired */
#define BE_IMASK_ECNTEXP   0x00001000 /* Excess-collision counter expired */
#define BE_IMASK_LCCNTEXP  0x00002000 /* Late-collision counter expired */
#define BE_IMASK_FCNTEXP   0x00004000 /* First-collision counter expired */
#define BE_IMASK_DTIMEXP   0x00008000 /* Defer-timer expired */

/* BE transmit config register. */
#define BE_TXCFG_ENABLE    0x00000001 /* Enable the transmitter */
#define BE_TXCFG_FIFO      0x00000010 /* Default tx fthresh... */
#define BE_TXCFG_SMODE     0x00000020 /* Enable slow transmit mode */
#define BE_TXCFG_CIGN      0x00000040 /* Ignore transmit collisions */
#define BE_TXCFG_FCSOFF    0x00000080 /* Do not emit FCS */
#define BE_TXCFG_DBACKOFF  0x00000100 /* Disable backoff */
#define BE_TXCFG_FULLDPLX  0x00000200 /* Enable full-duplex */

/* BE receive config register. */
#define BE_RXCFG_ENABLE    0x00000001 /* Enable the receiver */
#define BE_RXCFG_FIFO      0x0000000e /* Default rx fthresh... */
#define BE_RXCFG_PSTRIP    0x00000020 /* Pad byte strip enable */
#define BE_RXCFG_PMISC     0x00000040 /* Enable promiscous mode */
#define BE_RXCFG_DERR      0x00000080 /* Disable error checking */
#define BE_RXCFG_DCRCS     0x00000100 /* Disable CRC stripping */
#define BE_RXCFG_ME        0x00000200 /* Receive packets addressed to me */
#define BE_RXCFG_PGRP      0x00000400 /* Enable promisc group mode */
#define BE_RXCFG_HENABLE   0x00000800 /* Enable the hash filter */
#define BE_RXCFG_AENABLE   0x00001000 /* Enable the address filter */

struct be_cregs {
	volatile u_int32_t ctrl;       /* Control */
	volatile u_int32_t stat;       /* Status */
	volatile u_int32_t rxds;       /* RX descriptor ring ptr */
	volatile u_int32_t txds;       /* TX descriptor ring ptr */
	volatile u_int32_t rimask;     /* RX Interrupt Mask */
	volatile u_int32_t timask;     /* TX Interrupt Mask */
	volatile u_int32_t qmask;      /* QEC Error Interrupt Mask */
	volatile u_int32_t bmask;      /* BE Error Interrupt Mask */
	volatile u_int32_t rxwbufptr;  /* Local memory rx write ptr */
	volatile u_int32_t rxrbufptr;  /* Local memory rx read ptr */
	volatile u_int32_t txwbufptr;  /* Local memory tx write ptr */
	volatile u_int32_t txrbufptr;  /* Local memory tx read ptr */
	volatile u_int32_t ccnt;       /* Collision Counter */
};

struct be_tregs {
	u_int32_t	tcvr_pal;
	u_int32_t	mgmt_pal;
};

struct be_rxd {
	u_int32_t rx_flags;
	u_int32_t rx_addr;
};

#define RXD_OWN      0x80000000 /* Ownership. */
#define RXD_UPDATE   0x10000000 /* Being Updated? */
#define RXD_LENGTH   0x000007ff /* Packet Length. */

struct be_txd {
	u_int32_t tx_flags;
	u_int32_t tx_addr;
};

#define TXD_OWN      0x80000000 /* Ownership. */
#define TXD_SOP      0x40000000 /* Start Of Packet */
#define TXD_EOP      0x20000000 /* End Of Packet */
#define TXD_UPDATE   0x10000000 /* Being Updated? */
#define TXD_LENGTH   0x000007ff /* Packet Length. */




#define TX_RING_MAXSIZE   256
#define RX_RING_MAXSIZE   256
#define TX_RING_SIZE      256
#define RX_RING_SIZE      256

#define SUN4C_PKT_BUF_SZ        1546
#define SUN4C_RX_BUFF_SIZE      SUN4C_PKT_BUF_SZ
#define SUN4C_TX_BUFF_SIZE      SUN4C_PKT_BUF_SZ
#define SUN4C_RX_RING_SIZE      16
#define SUN4C_TX_RING_SIZE      16

struct be_desc {
	struct be_rxd be_rxd[RX_RING_MAXSIZE];
	struct be_txd be_txd[TX_RING_MAXSIZE];
};

struct be_bufs {
	char	tx_buf[SUN4C_TX_RING_SIZE][SUN4C_TX_BUFF_SIZE];
	char	pad[2];
	char	rx_buf[SUN4C_RX_RING_SIZE][SUN4C_RX_BUFF_SIZE];
};
