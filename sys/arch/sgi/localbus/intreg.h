/*	$OpenBSD: intreg.h,v 1.2 2012/04/15 20:44:52 miod Exp $	*/
/*	$NetBSD: int2reg.h,v 1.5 2009/02/12 06:33:57 rumble Exp $	*/

/*
 * Copyright (c) 2004 Christopher SEKIYA
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

/* The INT has known locations on all SGI machines */
#define	INT2_IP20		0x1fb801c0
#define	INT2_IP22		0x1fbd9000
#define	INT2_IP24		0x1fbd9880

/* The following registers are all 8 bit. */
#define INT2_LOCAL0_STATUS	0x03
#define INT2_LOCAL0_STATUS_FIFO	0x01
#define INT2_LOCAL0_MASK	0x07
#define INT2_LOCAL1_STATUS	0x0b
#define INT2_LOCAL1_MASK	0x0f
#define INT2_IP22_MAP_STATUS	0x13
#define INT2_IP22_MAP_MASK0	0x17
#define INT2_IP22_MAP_MASK1	0x1b
#define INT2_IP22_MAP_POL	0x1f
#define INT2_IP20_LED		0x1f
#define INT2_TIMER_CLEAR	0x23
#define INT2_ERROR_STATUS	0x27
#define INT2_TIMER_0		0x33
#define	INT2_TIMER_1		0x37
#define	INT2_TIMER_2		0x3b
#define INT2_TIMER_CONTROL	0x3f

/* LOCAL0 bits */
#define	INT2_L0_FIFO		0
#define	INT2_L0_GIO_SLOT0	0	/* IP24 */
#define	INT2_L0_GIO_LVL0	0	/* IP20/IP22 */
#define	INT2_L0_IP20_PARALLEL	1
#define	INT2_L0_IP22_SCSI0	1
#define	INT2_L0_SCSI1		2
#define	INT2_L0_ENET		3
#define	INT2_L0_GFX_DMA		4
#define	INT2_L0_IP20_SERIAL	5
#define	INT2_L0_IP22_PARALLEL	5
#define	INT2_L0_GIO_LVL1	6	/* IP20/IP22 */
#define	INT2_L0_IP20_VME0	7
#define	INT2_L0_IP22_MAP0	7

/* LOCAL1 bits */
#define	INT2_L1_IP24_ISDN_ISAC	0
#define	INT2_L1_IP20_GR1MODE	1	/* not an interrupt but a status bit */
#define	INT2_L1_IP22_PANEL	1
#define	INT2_L1_IP24_ISDN_HSCX	2
#define	INT2_L1_IP20_VME1	3
#define	INT2_L1_IP22_MAP1	3
#define	INT2_L1_IP20_DSP	4
#define	INT2_L1_IP22_HPC_DMA	4
#define	INT2_L1_ACFAIL		5
#define	INT2_L1_VIDEO		6
#define	INT2_L1_RETRACE		7
#define	INT2_L1_GIO_LVL2	7	/* IP20/IP22 */

/* MAP bits */
#define	INT2_MAP_NEWPORT	0	/* IP24 */
#define	INT2_MAP_PASSWD		1
#define	INT2_MAP_ISDN_POWER	2	/* IP24 */
#define	INT2_MAP_EISA		3	/* IP22 */
#define	INT2_MAP_PCKBC		4
#define	INT2_MAP_SERIAL		5
#define	INT2_MAP_GFX0_DRAIN	6	/* IP22 */
#define	INT2_MAP_GIO_SLOT0	6	/* IP24 */
#define	INT2_MAP_GFX1_DRAIN	7	/* IP22 */
#define	INT2_MAP_GIO_SLOT1	7	/* IP24 */

#define	INT2_L0_INTR(x)		((x) + 0)
#define	INT2_L1_INTR(x)		((x) + 8)
#define	INT2_MAP0_INTR(x)	((x) + 16)
#define	INT2_MAP1_INTR(x)	((x) + 24)
