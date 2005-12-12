/*	$OpenBSD: m88410.c,v 1.1 2005/12/12 20:36:33 miod Exp $	*/
/*
 * Copyright (c) 2001 Steve Murphree, Jr.
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
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *      This product includes software developed by Steve Murphree.
 * 4. The name of the author may not be used to endorse or promote products
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

#include <sys/param.h>
#include <sys/systm.h>

#include <machine/asm_macro.h>
#include <machine/m88410.h>

#include <mvme88k/dev/busswreg.h>

#define XCC_NOP		"0x00"
#define XCC_FLUSH_PAGE	"0x01"
#define XCC_FLUSH_ALL	"0x02"
#define XCC_INVAL_ALL	"0x03"
#define XCC_ADDR	0xff800000

void
mc88410_flush_page(paddr_t physaddr)
{
	paddr_t xccaddr = XCC_ADDR | (physaddr >> PGSHIFT);
	u_int psr;
	u_int16_t bs_gcsr, bs_romcr;

	bs_gcsr = *(volatile u_int16_t *)(BS_BASE + BS_GCSR);
	bs_romcr = *(volatile u_int16_t *)(BS_BASE + BS_ROMCR);

	/* mask misaligned exceptions */
	set_psr((psr = get_psr()) | PSR_MXM);

	/* clear WEN0 and WEN1 in ROMCR (disables writes to FLASH) */
	*(volatile u_int16_t *)(BS_BASE + BS_ROMCR) =
	    bs_romcr & ~(BS_ROMCR_WEN0 | BS_ROMCR_WEN1);

	/* set XCC bit in GCSR (0xff8xxxxx now decodes to mc88410) */
	*(volatile u_int16_t *)(BS_BASE + BS_GCSR) = bs_gcsr | BS_GCSR_XCC;

	/* send command */
	__asm__ __volatile__ (
	    "or   r2, r0, " XCC_FLUSH_PAGE ";"
	    "or   r3, r0, r0;"
	    "st.d r2, %0, 0" : : "r" (xccaddr) : "r2", "r3");

	/* spin until the operation starts */
	while ((*(volatile u_int32_t *)(BS_BASE + BS_XCCR) & BS_XCC_FBSY) != 0)
		;

	/* restore PSR and friends */
        set_psr(psr);
	flush_pipeline();
	*(volatile u_int16_t *)(BS_BASE + BS_GCSR) = bs_gcsr;
	*(volatile u_int16_t *)(BS_BASE + BS_ROMCR) = bs_romcr;
}

void
mc88410_flush(void)
{
	u_int psr;
	u_int16_t bs_gcsr, bs_romcr;

	bs_gcsr = *(volatile u_int16_t *)(BS_BASE + BS_GCSR);
	bs_romcr = *(volatile u_int16_t *)(BS_BASE + BS_ROMCR);

	/* mask misaligned exceptions */
	set_psr((psr = get_psr()) | PSR_MXM);

	/* clear WEN0 and WEN1 in ROMCR (disables writes to FLASH) */
	*(volatile u_int16_t *)(BS_BASE + BS_ROMCR) =
	    bs_romcr & ~(BS_ROMCR_WEN0 | BS_ROMCR_WEN1);

	/* set XCC bit in GCSR (0xFF8xxxxx now decodes to mc88410) */
	*(volatile u_int16_t *)(BS_BASE + BS_GCSR) = bs_gcsr | BS_GCSR_XCC;

	/* send command */
	__asm__ __volatile__ (
	    "or   r2, r0, " XCC_FLUSH_ALL ";"
	    "or   r3, r0, r0;"
	    "st.d r2, %0, 0" : : "r" (XCC_ADDR) : "r2", "r3");

	/* spin until the operation starts */
	while ((*(volatile u_int32_t *)(BS_BASE + BS_XCCR) & BS_XCC_FBSY) != 0)
		;

	/* restore PSR and friends */
        set_psr(psr);
	flush_pipeline();
	*(volatile u_int16_t *)(BS_BASE + BS_GCSR) = bs_gcsr;
	*(volatile u_int16_t *)(BS_BASE + BS_ROMCR) = bs_romcr;
}

void
mc88410_inval(void)
{
	u_int psr;
	u_int16_t bs_gcsr, bs_romcr;

	bs_gcsr = *(volatile u_int16_t *)(BS_BASE + BS_GCSR);
	bs_romcr = *(volatile u_int16_t *)(BS_BASE + BS_ROMCR);

	/* mask misaligned exceptions */
	set_psr((psr = get_psr()) | PSR_MXM);

	/* clear WEN0 and WEN1 in ROMCR (disables writes to FLASH) */
	*(volatile u_int16_t *)(BS_BASE + BS_ROMCR) =
	    bs_romcr & ~(BS_ROMCR_WEN0 | BS_ROMCR_WEN1);

	/* set XCC bit in GCSR (0xFF8xxxxx now decodes to mc88410) */
	*(volatile u_int16_t *)(BS_BASE + BS_GCSR) = bs_gcsr | BS_GCSR_XCC;

	/* send command */
	__asm__ __volatile__ (
	    "or   r2, r0, " XCC_INVAL_ALL ";"
	    "or   r3, r0, r0;"
	    "st.d r2, %0, 0" : : "r" (XCC_ADDR) : "r2", "r3");

	/* wait for the operation to be completed */
	while (*(volatile u_int32_t *)(BS_BASE + BS_XCCR) != 0)
		;

	/* restore PSR and friends */
        set_psr(psr);
	flush_pipeline();
	*(volatile u_int16_t *)(BS_BASE + BS_GCSR) = bs_gcsr;
	*(volatile u_int16_t *)(BS_BASE + BS_ROMCR) = bs_romcr;
}
