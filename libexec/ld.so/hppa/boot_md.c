/*	$OpenBSD: boot_md.c,v 1.6 2022/01/31 05:43:22 guenther Exp $ */

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

/*
 * IMPORTANT: any functions below are NOT protected by SSP.  Please
 * do not add anything except what is required to reach GOT with
 * an adjustment.
 */

#define	_DYN_LOADER

#include <sys/exec_elf.h>

#include "util.h"
#include "archdep.h"		/* for RELOC_TAG */

#include "../../lib/csu/os-note-elf.h"

typedef	Elf_RelA	RELOC_TYPE;

/*
 * Local decls.
 */
void _dl_boot_bind(const long, long *, Elf_Dyn *) __boot;

void
_dl_boot_bind(const long sp, long *dl_data, Elf_Dyn *dynp)
{
	AuxInfo			*auxstack;
	long			*stack;
	int			n, argc;
	char			**argv, **envp;
	long			loff;
	const RELOC_TYPE	*rend;
	const RELOC_TYPE	*dt_reloc;	/* DT_RELA */
	Elf_Addr		dt_relocsz;	/* DT_RELASZ */
	Elf_Addr		dt_pltgot;
	Elf_Addr		dt_pltrelsz;
	const Elf_Sym		*dt_symtab;
	const RELOC_TYPE	*dt_jmprel;

	/*
	 * Scan argument and environment vectors. Find dynamic
	 * data vector put after them.
	 */
	stack = (long *)sp;
	argc = *stack++;
	argv = (char **)stack;
	envp = &argv[argc + 1];
	stack = (long *)envp;
	while (*stack++ != 0L)
		;

	/*
	 * Zero out dl_data.
	 */
	for (n = 0; n <= AUX_entry; n++)
		dl_data[n] = 0;

	/*
	 * Dig out auxiliary data set up by exec call. Move all known
	 * tags to an indexed local table for easy access.
	 */
	for (auxstack = (AuxInfo *)stack; auxstack->au_id != AUX_null;
	    auxstack++) {
		if (auxstack->au_id > AUX_entry)
			continue;
		dl_data[auxstack->au_id] = auxstack->au_v;
	}
	loff = dl_data[AUX_base];	/* XXX assumes ld.so is linked at 0x0 */

	/*
	 * Scan the DYNAMIC section for the loader for the two items we need
	 */
	dt_pltrelsz = dt_relocsz = dt_pltgot = 0;
	dt_jmprel = dt_reloc = NULL;
	dt_symtab = NULL;
	while (dynp->d_tag != DT_NULL) {
		/* first the tags that are pointers to be relocated */
		if (dynp->d_tag == DT_PLTGOT)
			dt_pltgot = dynp->d_un.d_ptr + loff;
		else if (dynp->d_tag == DT_SYMTAB)
			dt_symtab = (void *)(dynp->d_un.d_ptr + loff);
		else if (dynp->d_tag == RELOC_TAG)	/* DT_{RELA,REL} */
			dt_reloc = (void *)(dynp->d_un.d_ptr + loff);
		else if (dynp->d_tag == DT_JMPREL)
			dt_jmprel = (void *)(dynp->d_un.d_ptr + loff);

		/* Now for the tags that are just sizes or counts */
		else if (dynp->d_tag == DT_PLTRELSZ)
			dt_pltrelsz = dynp->d_un.d_val;
		else if (dynp->d_tag == RELOC_TAG+1)	/* DT_{RELA,REL}SZ */
			dt_relocsz = dynp->d_un.d_val;
		dynp++;
	}

	rend = (RELOC_TYPE *)((char *)dt_jmprel + dt_pltrelsz);
	for (; dt_jmprel < rend; dt_jmprel++) {
		Elf_Addr *ra;
		const Elf_Sym *sp;

		sp = dt_symtab + ELF_R_SYM(dt_jmprel->r_info);
		ra = (Elf_Addr *)(dt_jmprel->r_offset + loff);
		ra[0] = loff + sp->st_value + dt_jmprel->r_addend;
		ra[1] = dt_pltgot;
	}

	rend = (RELOC_TYPE *)((char *)dt_reloc + dt_relocsz);
	for (; dt_reloc < rend; dt_reloc++) {
		Elf_Addr *ra;
		const Elf_Sym *sp;

		sp = dt_symtab + ELF_R_SYM(dt_reloc->r_info);
		ra = (Elf_Addr *)(dt_reloc->r_offset + loff);
		*ra = loff + sp->st_value + dt_reloc->r_addend;
	}
}
