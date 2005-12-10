/*	$OpenBSD: exec.h,v 1.11 2005/12/10 03:16:16 deraadt Exp $	*/
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

#ifndef _SPARC_EXEC_H_
#define _SPARC_EXEC_H_

#define __LDPGSZ	8192	/* linker page size */

#ifndef __ELF__
enum reloc_type {
	RELOC_8,	RELOC_16, 	RELOC_32,
	RELOC_DISP8,	RELOC_DISP16,	RELOC_DISP32,
	RELOC_WDISP30,	RELOC_WDISP22,
	RELOC_HI22,	RELOC_22,
	RELOC_13,	RELOC_LO10,
	RELOC_UNUSED1,	RELOC_UNUSED2,
	RELOC_BASE10,	RELOC_BASE13,	RELOC_BASE22,
	RELOC_PC10,	RELOC_PC22,
	RELOC_JMP_TBL,
	RELOC_UNUSED3,
	RELOC_GLOB_DAT,	RELOC_JMP_SLOT,	RELOC_RELATIVE
};

/* Relocation format. */
struct relocation_info_sparc {
	int r_address;			/* offset in text or data segment */
	unsigned int r_symbolnum : 24,	/* ordinal number of add symbol */
			r_extern :  1,	/* 1 if need to add symbol to value */
				 :  2;	/* unused bits */
	enum reloc_type r_type   :  5;	/* relocation type time copy */
	long r_addend;			/* relocation addend */
};
#define relocation_info	relocation_info_sparc
#else
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

#define R_TYPE(name)		__CONCAT(R_SPARC_,name)
#endif

#define NATIVE_EXEC_ELF

#define ARCH_ELFSIZE		32

#define	ELF_TARG_CLASS	ELFCLASS32
#define	ELF_TARG_DATA	ELFDATA2MSB
#define	ELF_TARG_MACH	EM_SPARC

#define	_NLIST_DO_AOUT
#define	_NLIST_DO_ELF

#ifdef COMPAT_SUNOS
#define _KERN_DO_AOUT
#endif
#define _KERN_DO_ELF

#endif  /* _SPARC_EXEC_H_ */
