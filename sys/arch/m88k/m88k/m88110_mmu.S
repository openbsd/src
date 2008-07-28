#	$OpenBSD: m88110_mmu.S,v 1.5 2008/07/28 17:49:38 miod Exp $
/*
 * Copyright (c) 2000 Steve Murphree, Jr.
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
 *      This product includes software developed by Steve Murphree, Jr.
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

#include <machine/asm.h>

/*
 * 88110 errata #20 (4.2) or #4 (5.1.1):
 * ``If a ldcr, stcr, fldcr, or fstcr instruction is located in the last
 *   word of an instruction page, and the fetch of the next page results
 *   in an mmu exception (invalid descriptor, tablewalk bus error,
 *   software atc miss), the exip and enip value may be incorrectly
 *   incremented to the second word of the page.
 *   Suggested fix: do not allow these instructions to occupy the last
 *   word of a page if this can occur.''
 *
 * This is why the following function are not inlines in asm.h, and are
 * aligned to 32 byte boundaries.
 *
 * Note that fldcr/fstcr are used in several places in userland (libc,
 * libpthread, afs lwp) which will need alignment hints as well to
 * avoid this. Sigh -- miod
 */

	text

/* set routines */
	align 32
ENTRY(set_icmd)
	FLUSH_PIPELINE
	stcr	r2, ICMD
	jmp	r1

	align 32
ENTRY(set_ictl)
	FLUSH_PIPELINE
	stcr	r2, ICTL
	NOP
	FLUSH_PIPELINE
	jmp	r1

	align 32
ENTRY(set_isar)
	stcr	r2, ISAR
	jmp	r1

	align 32
ENTRY(set_isap)
	FLUSH_PIPELINE
	NOP
	stcr	r2, ISAP
	jmp	r1

	align 32
ENTRY(set_iuap)
	FLUSH_PIPELINE
	NOP
 	stcr	r2, IUAP
	jmp	r1

	align 32
ENTRY(set_iir)
	stcr	r2, IIR 
	jmp	r1

	align 32
ENTRY(set_ibp)
	stcr	r2, IBP 
	jmp	r1

	align 32
ENTRY(set_ippu)
	stcr	r2, IPPU
	jmp	r1

	align 32
ENTRY(set_ippl)
	stcr	r2, IPPL
	jmp	r1

	align 32
ENTRY(set_isr)
	stcr	r2, ISR 
	jmp	r1

	align 32
ENTRY(set_dcmd)
	FLUSH_PIPELINE
	stcr	r2, DCMD
	jmp	r1

	align 32
ENTRY(set_dctl)
	FLUSH_PIPELINE
	stcr	r2, DCTL
	NOP
	FLUSH_PIPELINE
	jmp	r1

	align 32
ENTRY(set_dsar)
	stcr	r2, DSAR
	NOP
	jmp	r1

	align 32
ENTRY(set_dsap)
	FLUSH_PIPELINE
	NOP
	stcr	r2, DSAP
	FLUSH_PIPELINE
	NOP
	jmp	r1

	align 32
ENTRY(set_duap)
	FLUSH_PIPELINE
	NOP
	stcr	r2, DUAP
	FLUSH_PIPELINE
	NOP
	jmp	r1

	align 32
ENTRY(set_dir)
	stcr	r2, DIR 
	jmp	r1

	align 32
ENTRY(set_dbp)
	stcr	r2, DBP 
	jmp	r1

	align 32
ENTRY(set_dppu)
	stcr	r2, DPPU
	jmp	r1

	align 32
ENTRY(set_dppl)
	stcr	r2, DPPL
	jmp	r1

	align 32
ENTRY(set_dsr)
	stcr	r2, DSR 
	jmp	r1

/* get routines */
	align 32
ENTRY(get_ictl)
	ldcr	r2, ICTL
	jmp	r1

	align 32
ENTRY(get_dctl)
	ldcr	r2, DCTL
	jmp	r1
