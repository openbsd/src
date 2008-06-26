/*	$OpenBSD: pciide_opti_reg.h,v 1.4 2008/06/26 05:42:17 ray Exp $	*/
/*	$NetBSD: pciide_opti_reg.h,v 1.2 2000/06/07 20:42:53 scw Exp $	*/

/*-
 * Copyright (c) 2000 The NetBSD Foundation, Inc.
 * All rights reserved.
 *
 * This code is derived from software contributed to The NetBSD Foundation
 * by Steve C. Woodford.
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
 * THIS SOFTWARE IS PROVIDED BY THE NETBSD FOUNDATION, INC. AND CONTRIBUTORS
 * ``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED
 * TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE FOUNDATION OR CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef _DEV_PCI_PCIIDE_OPTI_REG_H_
#define _DEV_PCI_PCIIDE_OPTI_REG_H_

/*
 * Register definitions for OPTi PCIIDE controllers based on
 * their 82c621 chip.
 */

/* IDE Initialization Control Register */
#define OPTI_REG_INIT_CONTROL		0x40
#define  OPTI_INIT_CONTROL_MODE_PIO_0	0
#define  OPTI_INIT_CONTROL_MODE_PIO_1	2
#define  OPTI_INIT_CONTROL_MODE_PIO_2	1
#define  OPTI_INIT_CONTROL_MODE_PIO_3	3
#define  OPTI_INIT_CONTROL_ADDR_RELOC	(1u << 2)
#define  OPTI_INIT_CONTROL_CH2_ENABLE	0
#define  OPTI_INIT_CONTROL_CH2_DISABLE	(1u << 3)
#define  OPTI_INIT_CONTROL_FIFO_16	0
#define  OPTI_INIT_CONTROL_FIFO_32	(1u << 5)
#define  OPTI_INIT_CONTROL_FIFO_REQ_32	0
#define  OPTI_INIT_CONTROL_FIFO_REQ_30	(1u << 6)
#define  OPTI_INIT_CONTROL_FIFO_REQ_28	(2u << 6)
#define  OPTI_INIT_CONTROL_FIFO_REQ_26	(3u << 6)

/* IDE Enhanced Features Register */
#define OPTI_REG_ENH_FEAT		0x42
#define  OPTI_ENH_FEAT_X111_ENABLE	(1u << 1)
#define  OPTI_ENH_FEAT_CONCURRENT_MAST	(1u << 2)
#define  OPTI_ENH_FEAT_PCI_INVALIDATE	(1u << 3)
#define  OPTI_ENH_FEAT_IDE_CONCUR	(1u << 4)
#define  OPTI_ENH_FEAT_SLAVE_FIFO_ISA	(1u << 5)

/* IDE Enhanced Mode Register */
#define OPTI_REG_ENH_MODE		0x43
#define  OPTI_ENH_MODE_MASK(c,d)	(3u << (((c) * 4) + ((d) * 2)))
#define  OPTI_ENH_MODE_USE_TIMING(c,d)	0
#define  OPTI_ENH_MODE(c,d,m)		((m) << (((c) * 4) + ((d) * 2)))

/* Timing registers */
#define OPTI_REG_READ_CYCLE_TIMING	0x00
#define OPTI_REG_WRITE_CYCLE_TIMING	0x01
#define  OPTI_RECOVERY_TIME_SHIFT	0
#define  OPTI_PULSE_WIDTH_SHIFT		4

/*
 * Control register.
 */
#define OPTI_REG_CONTROL		0x03
#define  OPTI_CONTROL_DISABLE		0x11
#define  OPTI_CONTROL_ENABLE		0x95

/* Strap register */
#define OPTI_REG_STRAP			0x05
#define  OPTI_STRAP_PCI_SPEED_MASK	0x1u
#define  OPTI_STRAP_PCI_33		0
#define  OPTI_STRAP_PCI_25		1

/* Miscellaneous register */
#define OPTI_REG_MISC			0x06
#define  OPTI_MISC_INDEX(d)		((unsigned)(d))
#define  OPTI_MISC_INDEX_MASK		0x01u
#define  OPTI_MISC_DELAY_MASK		0x07u
#define  OPTI_MISC_DELAY_SHIFT		1
#define  OPTI_MISC_ADDR_SETUP_MASK	0x3u
#define  OPTI_MISC_ADDR_SETUP_SHIFT	4
#define  OPTI_MISC_READ_PREFETCH_ENABLE	(1u << 6)
#define  OPTI_MISC_ADDR_SETUP_MASK	0x3u
#define  OPTI_MISC_WRITE_MASK		0x7fu

/*
 * These are the timing register values for the various IDE modes
 * supported by the OPTi chip. The first index of the two-dimensional
 * arrays is used for a 33MHz PCIbus, the second for a 25MHz PCIbus.
 */
const static u_int8_t opti_tim_cp[2][8] = {		/* Command Pulse */
	{5, 4, 3, 2, 2, 7, 2, 2},
	{4, 3, 2, 2, 1, 5, 2, 1}
};

const static u_int8_t opti_tim_rt[2][8] = {		/* Recovery Time */
	{9, 4, 0, 0, 0, 6, 0, 0},
	{6, 2, 0, 0, 0, 4, 0, 0}
};

const static u_int8_t opti_tim_as[2][8] = {		/* Address Setup */
	{2, 1, 1, 1, 0, 0, 0, 0},
	{1, 1, 0, 0, 0, 0, 0, 0}
};

const static u_int8_t opti_tim_em[8] = {		/* Enhanced Mode */
	0, 0, 0, 1, 2, 0, 1 ,2
};

#endif	/* !_DEV_PCI_PCIIDE_OPTI_REG_H_ */
