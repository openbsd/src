/*	$OpenBSD: pciide_cmd_reg.h,v 1.2 1999/10/04 22:54:18 deraadt Exp $	*/
/*	$NetBSD: pciide_cmd_reg.h,v 1.4 1998/12/02 10:52:25 bouyer Exp $	*/

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
 * Registers definitions for CMD Technologies's PCI 064x IDE controllers.
 * Available from http://www.cmd.com/
 */

/* Configuration (RO) */
#define CMD_CONF 0x50
#define CMD_CONF_REV_MASK	0x03
#define CMD_CONF_DRV0_INTR	0x04
#define CMD_CONF_DEVID		0x18
#define CMD_CONF_VESAPRT	0x20
#define CMD_CONF_DSA1		0x40
#define CMD_CONF_DSA0		0x80

/* Control register (RW) */
#define CMD_CTRL 0x51
#define CMD_CTRL_HR_FIFO		0x01
#define CMD_CTRL_HW_FIFO		0x02
#define CMD_CTRL_DEVSEL			0x04
#define CMD_CTRL_2PORT			0x08
#define CMD_CTRL_PAR			0x10
#define CMD_CTRL_HW_HLD			0x20
#define CMD_CTRL_DRV0_RAHEAD		0x40
#define CMD_CTRL_DRV1_RAHEAD		0x80

/*
 * data read/write timing registers . 0640 uses the same for drive 0 and 1
 * on the secondary channel
 */
#define CMD_DATA_TIM(chan, drive) \
	(((chan) == 0) ? \
		((drive) == 0) ? 0x54: 0x56 \
		: \
		((drive) == 0) ? 0x58 : 0x5b)

/* secondary channel status and addr timings */
#define CMD_ARTTIM23		0x57
#define CMD_ARTTIM23_IRQ	0x10
#define CMD_ARTTIM23_RHAEAD(d)	((0x4) << (d))

/* DMA master read mode select */
#define CMD_DMA_MODE 0x71
#define CMD_DMA			0x00
#define CMD_DMA_MULTIPLE	0x01
#define CMD_DMA_LINE		0x10
		

/*
 * timings values for the 0643 and 0x646
 * for all dma_mode we have to have
 * DMA_timings(dma_mode) >= PIO_timings(dma_mode + 2)
 */
static int8_t cmd0643_6_data_tim_pio[] = {0xA9, 0x57, 0x44, 0x32, 0x3F};
static int8_t cmd0643_6_data_tim_dma[] = {0x87, 0x32, 0x3F};
