/*	$OpenBSD: bereg.h,v 1.1 1998/07/04 07:57:52 deraadt Exp $	*/

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

struct be_cregs {
	volatile u_int32_t ctrl;       /* Control */
	volatile u_int32_t stat;       /* Status */
	volatile u_int32_t rxds;       /* RX descriptor ring ptr */
	volatile u_int32_t txds;       /* TX descriptor ring ptr */
	volatile u_int32_t rimask;     /* RX Interrupt Mask */
	volatile u_int32_t timask;     /* TX Interrupt Mask */
	volatile u_int32_t qmask;      /* QEC Error Interrupt Mask */
	volatile u_int32_t bmask;      /* BigMAC Error Interrupt Mask */
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
