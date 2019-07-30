/*	$OpenBSD: archdep.h,v 1.21 2017/10/27 16:47:08 mpi Exp $ */

/*
 * Copyright (c) 1998 Per Fogelstrom, Opsycon AB
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS
 * OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 */

#ifndef _ALPHA_ARCHDEP_H_
#define _ALPHA_ARCHDEP_H_

#define	RELOC_TAG	DT_RELA

#define	MACHID	EM_ALPHA_EXP	/* ELF e_machine ID value checked */

#include <elf.h>
#include <machine/reloc.h>
#include "syscall.h"
#include "util.h"

static inline void
RELOC_DYN(Elf64_Rela *r, const Elf64_Sym *s, Elf64_Addr *p, unsigned long v)
{
	if (ELF64_R_TYPE(r->r_info) == RELOC_RELATIVE) {
		/* handled by _reloc_alpha_got */
	} else if (ELF64_R_TYPE(r->r_info) == RELOC_NONE) {
		/* nothing to do */
	} else if (ELF64_R_TYPE(r->r_info) == RELOC_JMP_SLOT) {
		Elf64_Addr val = v + s->st_value + r->r_addend -
			(Elf64_Addr)(p);
		*p = val;
		__asm volatile("imb" : : : "memory");
	} else if (ELF64_R_TYPE(r->r_info) == RELOC_GLOB_DAT) {
		*p = v + s->st_value + r->r_addend;
	} else {
		_dl_exit(6);
	}
}

#define RELOC_GOT(obj, offs)

#endif /* _ALPHA_ARCHDEP_H_ */
