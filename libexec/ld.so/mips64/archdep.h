/*	$OpenBSD: archdep.h,v 1.1 2004/08/11 17:11:45 pefo Exp $ */

/*
 * Copyright (c) 1998-2002 Opsycon AB, Sweden.
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

#ifndef _MIPS_ARCHDEP_H_
#define _MIPS_ARCHDEP_H_

#include <link.h>

#include "syscall.h"
#include "resolve.h"
#include "util.h"

#define RTLD_PROTECT_PLT

#define	DL_MALLOC_ALIGN	4	/* Arch constraint or otherwise */

#define	MACHID	EM_MIPS		/* ELF e_machine ID value checked */

#define	RELTYPE	Elf32_Rel
#define	RELSIZE	sizeof(Elf32_Rel)

static inline void
RELOC_REL(Elf_Rel *r, const Elf_Sym *s, Elf_Addr *p, unsigned long v)
{
}

static inline void
RELOC_RELA(Elf32_Rela *r, const Elf32_Sym *s, Elf32_Addr *p, unsigned long v)
{
	_dl_exit(20);
}

struct elf_object;

static inline void
RELOC_GOT(struct elf_object *dynld, long loff)
{
	Elf32_Addr *gotp;
	int i, n;
	const Elf_Sym *sp;

	/* Do all local gots */
	gotp = dynld->dyn.pltgot;
	n = dynld->Dyn.info[DT_MIPS_LOCAL_GOTNO - DT_LOPROC + DT_NUM];

	for (i = ((gotp[1] & 0x80000000) ? 2 : 1); i < n; i++) {
		gotp[i] += loff;
	}
	gotp += n;

	/* Do symbol referencing gots. There should be no global... */
	n =  dynld->Dyn.info[DT_MIPS_SYMTABNO - DT_LOPROC + DT_NUM] -
	  dynld->Dyn.info[DT_MIPS_GOTSYM - DT_LOPROC + DT_NUM];
	sp = dynld->dyn.symtab;
	sp += dynld->Dyn.info[DT_MIPS_GOTSYM - DT_LOPROC + DT_NUM];

	while (n--) {
		if (sp->st_shndx == SHN_UNDEF ||
		    sp->st_shndx == SHN_COMMON) {
			_dl_exit(6);
		} else if (ELF32_ST_TYPE(sp->st_info) == STT_FUNC) {
			*gotp += loff;
		} else {
			*gotp = sp->st_value + loff;
		}
		gotp++;
		sp++;
	}
	dynld->status |= STAT_GOT_DONE;
}

#define GOT_PERMS PROT_READ

#endif /* _MIPS_ARCHDEP_H_ */
