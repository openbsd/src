/*	$OpenBSD: psc.h,v 1.2 1998/05/08 22:13:02 gene Exp $	*/
/*	$NetBSD: psc.h,v 1.3 1998/04/24 05:27:24 scottr Exp $	*/

/*-
 * Copyright (c) 1997 David Huang <khym@bga.com>
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission
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
 *
 */

/*
 * Some register definitions for the PSC, present only on the
 * Centris/Quadra 660av and the Quadra 840av.
 */

extern volatile u_int8_t *PSCBase;

#define psc_reg1(r) (*((volatile u_int8_t *)(PSCBase+r)))
#define	psc_reg2(r) (*((volatile u_int16_t *)(PSCBase+r)))
#define	psc_reg4(r) (*((volatile u_int32_t *)(PSCBase+r)))

void	psc_init __P((void));

int	add_psc_lev3_intr __P((void (*)(void *), void *));
int	add_psc_lev4_intr __P((int, int (*)(void *), void *));
int	add_psc_lev5_intr __P((int, void (*)(void *), void *));
int	add_psc_lev6_intr __P((int, void (*)(void *), void *));

int	remove_psc_lev3_intr __P((void));
int	remove_psc_lev4_intr __P((int));
int	remove_psc_lev5_intr __P((int));
int	remove_psc_lev6_intr __P((int));

/*
 * Reading an interrupt status register returns a mask of the
 * currently interrupting devices (one bit per device). Reading an
 * interrupt enable register returns a mask of the currently enabled
 * devices. Writing an interrupt enable register with the MSB set
 * enables the interrupts in the lower 4 bits, while writing with the
 * MSB clear disables the corresponding interrupts.
 * e.g. write 0x81 to enable device 0, write 0x86 to enable devices 1
 * and 2, write 0x02 to disable device 1.
 *
 * Level 3 device 0 is MACE
 * Level 4 device 0 is 3210 DSP?
 * Level 4 device 1 is SCC channel A (modem port)
 * Level 4 device 2 is SCC channel B (printer port)
 * Level 4 device 3 is MACE DMA completion
 * Level 5 device 0 is 3210 DSP?
 * Level 5 device 1 is 3210 DSP?
 * Level 6 device 0 is ?
 * Level 6 device 1 is ?
 * Level 6 device 2 is ?
 */

/* PSC interrupt registers */
#define	PSC_LEV3_ISR	0x130	/* level 3 interrupt status register */
#define	PSC_LEV3_IER	0x134	/* level 3 interrupt enable register */
#define	  PSCINTR_ENET      0	/*   Ethernet interrupt */

#define	PSC_LEV4_ISR	0x140	/* level 4 interrupt status register */
#define	PSC_LEV4_IER	0x144	/* level 4 interrupt enable register */
#define	  PSCINTR_SCCA      1	/*   SCC channel A interrupt */
#define	  PSCINTR_SCCB      2	/*   SCC channel B interrupt */
#define	  PSCINTR_ENET_DMA  3	/*   Ethernet DMA completion interrupt */

#define	PSC_LEV5_ISR	0x150	/* level 5 interrupt status register */
#define	PSC_LEV5_IER	0x154	/* level 5 interrupt enable register */

#define	PSC_LEV6_ISR	0x160	/* level 6 interrupt status register */
#define	PSC_LEV6_IER	0x164	/* level 6 interrupt enable register */

/* PSC DMA channel control registers */
#define	PSC_ENETRD_CTL	0xc10	/* MACE receive DMA channel control/status */
#define	PSC_ENETWR_CTL	0xc20	/* MACE transmit DMA channel control/status */

/* PSC DMA channels */
#define	PSC_ENETRD_ADDR	0x1020	/* MACE receive DMA address register */
#define	PSC_ENETRD_LEN	0x1024	/* MACE receive DMA buffer count */
#define	PSC_ENETRD_CMD	0x1028	/* MACE receive DMA command register */
#define	PSC_ENETWR_ADDR	0x1040	/* MACE transmit DMA address register */
#define	PSC_ENETWR_LEN	0x1044	/* MACE transmit DMA length */
#define	PSC_ENETWR_CMD	0x1048	/* MACE transmit DMA command register */

/*
 * PSC DMA channels are controlled by two sets of registers (see p.29
 * of the Quadra 840av and Centris 660av Developer Note). Add the
 * following offsets to get the desired register set.
 */
#define	PSC_SET0	0x00
#define	PSC_SET1	0x10
