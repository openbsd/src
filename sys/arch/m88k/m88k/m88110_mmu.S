#	$OpenBSD: m88110_mmu.S,v 1.1 2004/04/29 14:33:27 miod Exp $
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

/* set routines */
	text
	align 4096	/* sigh */

ENTRY(set_icmd)
	FLUSH_PIPELINE
	jmp.n	r1
	 stcr	r2, ICMD

ENTRY(set_ictl)
	FLUSH_PIPELINE
	stcr	r2, ICTL
	NOP
	FLUSH_PIPELINE
	jmp	r1

ENTRY(set_isar)
	jmp.n	r1
	 stcr	r2, ISAR

ENTRY(set_isap)
	FLUSH_PIPELINE
	NOP
	stcr	r2, ISAP
	jmp	r1

ENTRY(set_iuap)
	FLUSH_PIPELINE
	NOP
 	stcr	r2, IUAP
	jmp	r1

ENTRY(set_iir)
	jmp.n	r1
	 stcr	r2, IIR 

ENTRY(set_ibp)
	jmp.n	r1
	 stcr	r2, IBP 

ENTRY(set_ippu)
	jmp.n	r1
	 stcr	r2, IPPU

ENTRY(set_ippl)
	jmp.n	r1
	 stcr	r2, IPPL

ENTRY(set_isr)
	jmp.n	r1
	 stcr	r2, ISR 

ENTRY(set_ilar)
	jmp.n	r1
	 stcr	r2, ILAR

ENTRY(set_ipar)
	jmp.n	r1
	 stcr	r2, IPAR

ENTRY(set_dcmd)
	FLUSH_PIPELINE
	jmp.n	r1
	 stcr	r2, DCMD

ENTRY(set_dctl)
	FLUSH_PIPELINE
	stcr	r2, DCTL
	NOP
	FLUSH_PIPELINE
	jmp	r1

ENTRY(set_dsar)
	stcr	r2, DSAR
	NOP
	jmp	r1

ENTRY(set_dsap)
	FLUSH_PIPELINE
	NOP
	stcr	r2, DSAP
	FLUSH_PIPELINE
	NOP
	jmp	r1

ENTRY(set_duap)
	FLUSH_PIPELINE
	NOP
	stcr	r2, DUAP
	FLUSH_PIPELINE
	NOP
	jmp	r1

ENTRY(set_dir)
	jmp.n	r1
	 stcr	r2, DIR 

ENTRY(set_dbp)
	jmp.n	r1
	 stcr	r2, DBP 

ENTRY(set_dppu)
	jmp.n	r1
	 stcr	r2, DPPU

ENTRY(set_dppl)
	jmp.n	r1
	 stcr	r2, DPPL

ENTRY(set_dsr)
	jmp.n	r1
	 stcr	r2, DSR 

ENTRY(set_dlar)
	jmp.n	r1
	 stcr	r2, DLAR

ENTRY(set_dpar)
	jmp.n	r1
	 stcr	r2, DPAR


/* get routines */
ENTRY(get_icmd)
	jmp.n	r1
	 ldcr	r2, ICMD

ENTRY(get_ictl)
	jmp.n	r1
	 ldcr	r2, ICTL

ENTRY(get_isar)
	jmp.n	r1
	 ldcr	r2, ISAR

ENTRY(get_isap)
	jmp.n	r1
	 ldcr	r2, ISAP

ENTRY(get_iuap)
	jmp.n	r1
 	 ldcr	r2, IUAP

ENTRY(get_iir)
	jmp.n	r1
	 ldcr	r2, IIR 

ENTRY(get_ibp)
	jmp.n	r1
	 ldcr	r2, IBP 

ENTRY(get_ippu)
	jmp.n	r1
	 ldcr	r2, IPPU

ENTRY(get_ippl)
	jmp.n	r1
	 ldcr	r2, IPPL

ENTRY(get_isr)
	jmp.n	r1
	 ldcr	r2, ISR 

ENTRY(get_ilar)
	jmp.n	r1
	 ldcr	r2, ILAR

ENTRY(get_ipar)
	jmp.n	r1
	 ldcr	r2, IPAR

ENTRY(get_dcmd)
	jmp.n	r1
	 ldcr	r2, DCMD

ENTRY(get_dctl)
	jmp.n	r1
	 ldcr	r2, DCTL

ENTRY(get_dsar)
	jmp.n	r1
	 ldcr	r2, DSAR

ENTRY(get_dsap)
	jmp.n	r1
	 ldcr	r2, DSAP

ENTRY(get_duap)
	jmp.n	r1
	 ldcr	r2, DUAP

ENTRY(get_dir)
	jmp.n	r1
	 ldcr	r2, DIR 

ENTRY(get_dbp)
	jmp.n	r1
	 ldcr	r2, DBP 

ENTRY(get_dppu)
	jmp.n	r1
	 ldcr	r2, DPPU

ENTRY(get_dppl)
	jmp.n	r1
	 ldcr	r2, DPPL

ENTRY(get_dsr)
	jmp.n	r1
	 ldcr	r2, DSR 

ENTRY(get_dlar)
	jmp.n	r1
	 ldcr	r2, DLAR

ENTRY(get_dpar)
	jmp.n	r1
	 ldcr	r2, DPAR
