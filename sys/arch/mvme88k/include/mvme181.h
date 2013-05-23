/*	$OpenBSD: mvme181.h,v 1.2 2013/05/23 21:20:12 miod Exp $	*/

/*
 * Copyright (c) 2013 Miodrag Vallat.
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

#ifndef _MACHINE_MVME181_H_
#define _MACHINE_MVME181_H_

#define	M181_OBIO_START	0xffe00000

#define	M181_CMMU_I	0xfff7e000	/* Instruction CMMU address */
#define	M181_CMMU_D	0xfff7f000	/* Data CMMU address */

#define	M181_SSR	0xffe10000	/* system status register */
#define	M181_SCR	0xffe20000	/* system control register */
#define	M181_CPEI	0xffe30000	/* clear parity error interrupt */
#define	M181_VMEVEC	0xffe80000	/* VME vector register */
#define	M181_CLRABRT	0xffee0000	/* clear abort interrupt */

#define	M181_DUART	0xffe40000	/* base address of DUART chip */
#define	M181_DSRTC	0xff810000	/* base address of TODclock */

/*
 * Control and Status Register interrupt bits - 180 only has the lower 8 bits
 */

#define	M181_IRQ_VME4_180	0x0010
#define	M181_IRQ_PARITY		0x0020	/* 181 only */
#define	M181_IRQ_DUART		0x0040
#define	M181_IRQ_ABORT		0x0080
#define	M181_IRQ_VME1		0x0200
#define	M181_IRQ_VME2		0x0400
#define	M181_IRQ_VME3		0x0800
#define	M181_IRQ_VME4		0x1000
#define	M181_IRQ_VME5		0x2000
#define	M181_IRQ_VME6		0x4000
#define	M181_IRQ_VME7		0x8000

/*
 * System Status register bits (not interrupt bits)
 */

#define	M181_SYSCON		0x0004	/* S3-1 switch closed */
#define	M181_BOARDMODE		0x0100	/* S3-6 switch closed */

#endif	/* _MACHINE_MVME181_H_ */
