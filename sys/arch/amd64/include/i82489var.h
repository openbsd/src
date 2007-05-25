/*	$NetBSD: i82489var.h,v 1.1 2003/02/26 21:26:10 fvdl Exp $	*/

/*-
 * Copyright (c) 1998 The NetBSD Foundation, Inc.
 * All rights reserved.
 *
 * This code is derived from software contributed to The NetBSD Foundation
 * by Frank van der Linden.
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
 *	This product includes software developed by the NetBSD
 *	Foundation, Inc. and its contributors.
 * 4. Neither the name of The NetBSD Foundation nor the names of its
 *    contributors may be used to endorse or promote products derived
 *    from this software without specific prior written permission.
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

#ifndef _X86_I82489REG_H_
#define _X86_I82489REG_H_

/*
 * Software definitions belonging to Local APIC driver.
 */

static __inline__ u_int32_t i82489_readreg(int);
static __inline__ void i82489_writereg(int, u_int32_t);

#ifdef _KERNEL
extern volatile u_int32_t local_apic[];
extern volatile u_int32_t lapic_tpr;
#endif

static __inline__ u_int32_t
i82489_readreg(reg)
	int reg;
{
	return *((volatile u_int32_t *)(((volatile u_int8_t *)local_apic)
	    + reg));
}

static __inline__ void
i82489_writereg(reg, val)
	int reg;
	u_int32_t val;
{
	*((volatile u_int32_t *)(((volatile u_int8_t *)local_apic) + reg)) = val;
}

#define lapic_cpu_number() 	(i82489_readreg(LAPIC_ID)>>LAPIC_ID_SHIFT)

/*
 * "spurious interrupt vector"; vector used by interrupt which was
 * aborted because the CPU masked it after it happened but before it
 * was delivered.. "Oh, sorry, i caught you at a bad time".
 * Low-order 4 bits must be all ones.
 */
extern void Xintrspurious(void);
#define LAPIC_SPURIOUS_VECTOR		0xef

/*
 * Vector used for inter-processor interrupts.
 */
extern void Xintr_lapic_ipi(void);
extern void Xrecurse_lapic_ipi(void);
extern void Xresume_lapic_ipi(void);
#define LAPIC_IPI_VECTOR			0xe0

/*
 * We take 0xf0-0xfe for fast IPI handlers.
 */
#define LAPIC_IPI_OFFSET			0xf0
#define LAPIC_IPI_INVLTLB			(LAPIC_IPI_OFFSET + 0)
#define LAPIC_IPI_INVLPG			(LAPIC_IPI_OFFSET + 1)
#define LAPIC_IPI_INVLRANGE			(LAPIC_IPI_OFFSET + 2)

extern void Xipi_invltlb(void);
extern void Xipi_invlpg(void);
extern void Xipi_invlrange(void);

/*
 * Vector used for local apic timer interrupts.
 */

extern void Xintr_lapic_ltimer(void);
extern void Xresume_lapic_ltimer(void);
extern void Xrecurse_lapic_ltimer(void);
#define LAPIC_TIMER_VECTOR		0xc0

/*
 * 'pin numbers' for local APIC
 */
#define LAPIC_PIN_TIMER		0
#define LAPIC_PIN_PCINT		2
#define LAPIC_PIN_LVINT0	3
#define LAPIC_PIN_LVINT1	4
#define LAPIC_PIN_LVERR		5

extern void Xintr_lapic0(void);
extern void Xintr_lapic2(void);
extern void Xintr_lapic3(void);
extern void Xintr_lapic4(void);
extern void Xintr_lapic5(void);


struct cpu_info;

extern void lapic_boot_init(paddr_t);
extern void lapic_set_lvt(void);
extern void lapic_enable(void);
extern void lapic_calibrate_timer(struct cpu_info *ci);
extern void lapic_initclocks(void);

#endif
