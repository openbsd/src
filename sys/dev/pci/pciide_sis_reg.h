/*	$OpenBSD: pciide_sis_reg.h,v 1.1 1999/07/18 21:25:20 csapuntz Exp $	*/
/*	$NetBSD: pciide_sis_reg.h,v 1.5 1998/12/04 17:30:55 drochner Exp $	*/

/*
 * Copyright (c) 1998 Manuel Bouyer.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *	This product includes software developed by the University of
 *	California, Berkeley and its contributors.
 * 4. Neither the name of the University nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE REGENTS AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 */

/*
 * Registers definitions for SiS SiS5597/98 PCI IDE controller.
 * Available from http://www.sis.com.tw/html/databook.html
 */

/* IDE timing control registers (32 bits) */
#define SIS_TIM(channel) (0x40 + (channel * 4))
#define SIS_TIM_REC_OFF(drive) (16 * (drive))
#define SIS_TIM_ACT_OFF(drive) (8 + 16 * (drive))
#define SIS_TIM_UDMA_TIME_OFF(drive) (13 + 16 * (drive))
#define SIS_TIM_UDMA_EN(drive) (1 << (15 + 16 * (drive)))

/* IDE general control register 0 (8 bits) */
#define SIS_CTRL0 0x4a
#define SIS_CTRL0_PCIBURST	0x80
#define SIS_CTRL0_FAST_PW	0x20
#define SIS_CTRL0_BO		0x08
#define SIS_CTRL0_CHAN0_EN	0x02 /* manual (v2.0) is wrong!!! */
#define SIS_CTRL0_CHAN1_EN	0x04 /* manual (v2.0) is wrong!!! */

/* IDE general control register 1 (8 bits) */
#define SIS_CTRL1 0x4b
#define SIS_CTRL1_POSTW_EN(chan, drv) (0x10 << ((drv) + 2 * (chan)))
#define SIS_CTRL1_PREFETCH_EN(chan, drv) (0x01 << ((drv) + 2 * (chan)))

/* IDE misc control register (8 bit) */
#define SIS_MISC 0x52
#define SIS_MISC_TIM_SEL	0x08
#define SIS_MISC_GTC		0x04
#define SIS_MISC_FIFO_SIZE	0x01

static int8_t sis_pio_act[] = {7, 5, 4, 3, 3};
static int8_t sis_pio_rec[] = {7, 0, 5, 3, 1};
#ifdef unused
static int8_t sis_dma_act[] = {0, 3, 3};
static int8_t sis_dma_rec[] = {0, 2, 1};
#endif
static int8_t sis_udma_tim[] = {3, 2, 1};
