/* $NetBSD: iomd.h,v 1.3 1996/03/28 21:26:05 mark Exp $ */

/*
 * Copyright (c) 1994 Mark Brinicombe.
 * Copyright (c) 1994 Brini.
 * All rights reserved.
 *
 * This code is derived from software written for Brini by Mark Brinicombe
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
 *	This product includes software developed by Brini.
 * 4. The name of the company nor the name of the author may be used to
 *    endorse or promote products derived from this software without specific
 *    prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY BRINI ``AS IS'' AND ANY EXPRESS OR IMPLIED
 * WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL BRINI OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT,
 * INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 * RiscBSD kernel project
 *
 * iomd.h
 *
 * IOMD registers
 *
 * Created      : 18/09/94
 *
 * Based on kate/display/iomd.h
 */

#define IOMD_HW_BASE	0x03200000

#define IOMD_BASE	0xf6000000

#define IOMD_IOCR	(IOMD_BASE + 0x00000000)
#define IOMD_KBDDAT	(IOMD_BASE + 0x00000004)
#define IOMD_KBDCR	(IOMD_BASE + 0x00000008)

#define IOMD_IRQSTA	(IOMD_BASE + 0x00000010)
#define IOMD_IRQRQA	(IOMD_BASE + 0x00000014)
#define IOMD_IRQMSKA	(IOMD_BASE + 0x00000018)
#define IOMD_SUSPEND	(IOMD_BASE + 0x0000001C) /* ARM7500 */

#define IOMD_IRQSTB	(IOMD_BASE + 0x00000020)
#define IOMD_IRQRQB	(IOMD_BASE + 0x00000024)
#define IOMD_IRQMSKB	(IOMD_BASE + 0x00000028)

#define IOMD_FIQST	(IOMD_BASE + 0x00000030)
#define IOMD_FIQRQ	(IOMD_BASE + 0x00000034)
#define IOMD_FIQMSK	(IOMD_BASE + 0x00000038)

#define IOMD_T0LOW	(IOMD_BASE + 0x00000040)
#define IOMD_T0HIGH	(IOMD_BASE + 0x00000044)
#define IOMD_T0GO	(IOMD_BASE + 0x00000048)
#define IOMD_T0LATCH	(IOMD_BASE + 0x0000004c)

#define IOMD_T1LOW	(IOMD_BASE + 0x00000050)
#define IOMD_T1HIGH	(IOMD_BASE + 0x00000054)
#define IOMD_T1GO	(IOMD_BASE + 0x00000058)
#define IOMD_T1LATCH	(IOMD_BASE + 0x0000005c)

/*
 * For RC7500, it's not really a IOMD device.
 */
#define IOMD_IRQSTC	(IOMD_BASE + 0x00000060)	/* ARM7500 */
#define IOMD_IRQRQC	(IOMD_BASE + 0x00000064)	/* ARM7500 */
#define IOMD_IRQMSKC	(IOMD_BASE + 0x00000068)	/* ARM7500 */
#define IOMD_VIDMUX	(IOMD_BASE + 0x0000006C)	/* ARM7500 */
 
#define IOMD_IRQSTD	(IOMD_BASE + 0x00000070)	/* ARM7500 */
#define IOMD_IRQRQD	(IOMD_BASE + 0x00000074)	/* ARM7500 */
#define IOMD_IRQMSKD	(IOMD_BASE + 0x00000078)	/* ARM7500 */

#define IOMD_ROMCR0	(IOMD_BASE + 0x00000080)
#define IOMD_ROMCR1	(IOMD_BASE + 0x00000084)
#define IOMD_DRAMCR	(IOMD_BASE + 0x00000088)
#define IOMD_VREFCR	(IOMD_BASE + 0x0000008c)
#define IOMD_FSIZE	(IOMD_BASE + 0x00000090)
#define IOMD_ID0	(IOMD_BASE + 0x00000094)
#define IOMD_ID1	(IOMD_BASE + 0x00000098)
#define IOMD_VERSION	(IOMD_BASE + 0x0000009c)

#define IOMD_MOUSEX	(IOMD_BASE + 0x000000a0)
#define IOMD_MOUSEY	(IOMD_BASE + 0x000000a4)

#define IOMD_MSDATA	(IOMD_BASE + 0x000000a8)	/* ARM7500 */
#define IOMD_MSCR	(IOMD_BASE + 0x000000ac)	/* ARM7500 */

#define IOMD_IOTCR	(IOMD_BASE + 0x000000C4)	/* ARM7500 */
#define IOMD_ECTCR	(IOMD_BASE + 0x000000C8)	/* ARM7500 */
#define IOMD_ASTCR	(IOMD_BASE + 0x000000CC)	/* ARM7500 */

#define IOMD_DRAMWID	(IOMD_BASE + 0x000000D0)	/* ARM7500 */
#define IOMD_SELFREF	(IOMD_BASE + 0x000000D4)	/* ARM7500 */

#define IOMD_ATODICR	(IOMD_BASE + 0x000000E0)	/* ARM7500 */
#define IOMD_ATODSR	(IOMD_BASE + 0x000000E4)	/* ARM7500 */
#define IOMD_ATODCR	(IOMD_BASE + 0x000000E8)	/* ARM7500 */
#define IOMD_ATODCNT1	(IOMD_BASE + 0x000000EC)	/* ARM7500 */
#define IOMD_ATODCNT2	(IOMD_BASE + 0x000000F0)	/* ARM7500 */
#define IOMD_ATODCNT3	(IOMD_BASE + 0x000000F4)	/* ARM7500 */
#define IOMD_ATODCNT4	(IOMD_BASE + 0x000000F8)	/* ARM7500 */

#define IOMD_SD0CURA	(IOMD_BASE + 0x00000180)
#define IOMD_SD0ENDA	(IOMD_BASE + 0x00000184)
#define IOMD_SD0CURB	(IOMD_BASE + 0x00000188)
#define IOMD_SD0ENDB	(IOMD_BASE + 0x0000018c)
#define IOMD_SD0CR	(IOMD_BASE + 0x00000190)
#define IOMD_SD0ST	(IOMD_BASE + 0x00000194)

#define IOMD_SD1CURA	(IOMD_BASE + 0x000001a0)
#define IOMD_SD1ENDA	(IOMD_BASE + 0x000001a4)
#define IOMD_SD1CURB	(IOMD_BASE + 0x000001a8)
#define IOMD_SD1ENDB	(IOMD_BASE + 0x000001ac)
#define IOMD_SD1CR	(IOMD_BASE + 0x000001b0)
#define IOMD_SD1ST	(IOMD_BASE + 0x000001b4)

#define IOMD_CURSCUR	(IOMD_BASE + 0x000001c0)
#define IOMD_CURSINIT	(IOMD_BASE + 0x000001c4)

#define IOMD_VIDCUR	(IOMD_BASE + 0x000001d0)
#define IOMD_VIDEND	(IOMD_BASE + 0x000001d4)
#define IOMD_VIDSTART	(IOMD_BASE + 0x000001d8)
#define IOMD_VIDINIT	(IOMD_BASE + 0x000001dc)
#define IOMD_VIDCR	(IOMD_BASE + 0x000001e0)

#define IOMD_DMAST	(IOMD_BASE + 0x000001f0)
#define IOMD_DMARQ	(IOMD_BASE + 0x000001f4)
#define IOMD_DMAMSK	(IOMD_BASE + 0x000001f8)

#define IO_MOUSE_BUTTONS 0xf6010000

#define MOUSE_BUTTON_RIGHT  0x10
#define MOUSE_BUTTON_MIDDLE 0x20
#define MOUSE_BUTTON_LEFT   0x40

#define RPC600_IOMD_ID	0xd4e7
#define RC7500_IOC_ID	0x5B98

#define FREQCON	(IOMD_BASE + 0x40000)

/* End of iomd.h */
