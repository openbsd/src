/*	$OpenBSD: exec.h,v 1.17 2013/10/17 08:02:17 deraadt Exp $	*/
/*	$NetBSD: exec.h,v 1.7 1994/11/20 20:53:02 deraadt Exp $ */

/*
 * Copyright (c) 1993 Christopher G. Demetriou
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
 */

#ifndef _MACHINE_EXEC_H_
#define _MACHINE_EXEC_H_

#define __LDPGSZ	8192	/* linker page size */

#define R_SPARC_NONE		0
#define R_SPARC_8		1
#define R_SPARC_16		2
#define R_SPARC_32		3
#define R_SPARC_DISP8		4
#define R_SPARC_DISP16		5
#define R_SPARC_DISP32		6
#define R_SPARC_WDISP30		7
#define R_SPARC_WDISP22		8
#define R_SPARC_HI22		9
#define R_SPARC_22		10
#define R_SPARC_13		11
#define R_SPARC_LO10		12
#define R_SPARC_GOT10		13
#define R_SPARC_GOT13		14
#define R_SPARC_GOT22		15
#define R_SPARC_PC10		16
#define R_SPARC_PC22		17
#define R_SPARC_WPLT30		18
#define R_SPARC_COPY		19
#define R_SPARC_GLOB_DAT	20
#define R_SPARC_JMP_SLOT	21
#define R_SPARC_RELATIVE	22
#define R_SPARC_UA32		23
#define R_SPARC_PLT32		24
#define R_SPARC_HIPLT22		25
#define R_SPARC_LOPLT10		26
#define R_SPARC_PCPLT32		27
#define R_SPARC_PCPLT22		28
#define R_SPARC_PCPLT10		29
#define R_SPARC_10		30
#define R_SPARC_11		31
#define R_SPARC_64		32
#define R_SPARC_OLO10		33
#define R_SPARC_HH22		34
#define R_SPARC_HM10		35
#define R_SPARC_LM22		36
#define R_SPARC_PC_HH22		37
#define R_SPARC_PC_HM10		38
#define R_SPARC_PC_LM22		39
#define R_SPARC_WDISP16		40
#define R_SPARC_WDISP19		41
#define R_SPARC_GLOB_JMP	42
#define R_SPARC_7		43
#define R_SPARC_5		44
#define R_SPARC_6		45
#define R_SPARC_TLS_DTPMOD32	74
#define R_SPARC_TLS_DTPMOD64	75
#define R_SPARC_TLS_DTPOFF32	76
#define R_SPARC_TLS_DTPOFF64	77
#define R_SPARC_TLS_TPOFF32	78
#define R_SPARC_TLS_TPOFF64	79

#define R_TYPE(name)		__CONCAT(R_SPARC_,name)

#define ARCH_ELFSIZE		32

#define	ELF_TARG_CLASS	ELFCLASS32
#define	ELF_TARG_DATA	ELFDATA2MSB
#define	ELF_TARG_MACH	EM_SPARC

#define	_NLIST_DO_ELF
#define _KERN_DO_ELF

#endif  /* _MACHINE_EXEC_H_ */
