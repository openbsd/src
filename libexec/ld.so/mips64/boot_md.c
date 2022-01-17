/*	$OpenBSD: boot_md.c,v 1.4 2022/01/17 19:45:34 guenther Exp $ */

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

#include <machine/reloc.h>

#include "util.h"
#include "archdep.h"

#include "../../lib/csu/os-note-elf.h"

typedef	Elf_Rel		RELOC_TYPE;

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
	unsigned		i;
	const RELOC_TYPE	*rend;
	const RELOC_TYPE	*dt_reloc;	/* DT_REL */
	unsigned		dt_relocsz;	/* DT_RELSZ */
	const Elf_Sym		*dt_symtab;
	Elf_Addr		*dt_pltgot;
	unsigned		dt_local_gotno, dt_gotsym, dt_symtabno;

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
	 * Scan the DYNAMIC section for the loader for the items we need
	 */
	dt_reloc = NULL;
	dt_local_gotno = dt_gotsym = dt_symtabno = dt_relocsz = 0;
	while (dynp->d_tag != DT_NULL) {
		/* first the tags that are pointers to be relocated */
		if (dynp->d_tag == DT_SYMTAB)
			dt_symtab = (void *)(dynp->d_un.d_ptr + loff);
		else if (dynp->d_tag == RELOC_TAG)	/* DT_REL */
			dt_reloc = (void *)(dynp->d_un.d_ptr + loff);
		else if (dynp->d_tag == DT_PLTGOT)
			dt_pltgot = (void *)(dynp->d_un.d_ptr + loff);

		/* Now for the tags that are just sizes or counts */
		else if (dynp->d_tag == RELOC_TAG+1)	/* DT_RELSZ */
			dt_relocsz = dynp->d_un.d_val;
		else if (dynp->d_tag == DT_MIPS_LOCAL_GOTNO)
			dt_local_gotno = dynp->d_un.d_val;
		else if (dynp->d_tag == DT_MIPS_GOTSYM)
			dt_gotsym = dynp->d_un.d_val;
		else if (dynp->d_tag == DT_MIPS_SYMTABNO)
			dt_symtabno = dynp->d_un.d_val;
		dynp++;
	}

	rend = (RELOC_TYPE *)((char *)dt_reloc + dt_relocsz);
	for (; dt_reloc < rend; dt_reloc++) {
		if (ELF64_R_TYPE(dt_reloc->r_info) == R_MIPS_REL32_64) {
			Elf_Addr *ra;
			ra = (Elf_Addr *)(dt_reloc->r_offset + loff);
			*ra += loff;
		}
	}

	/* Do all local gots */
	for (i = 2; i < dt_local_gotno; i++)
		dt_pltgot[i] += loff;
	dt_pltgot += dt_local_gotno;

	/* Do symbol referencing gots. There should be no global... */
	i = dt_symtabno - dt_gotsym;
	dt_symtab += dt_gotsym;

	while (i--) {
		if (ELF64_ST_TYPE(dt_symtab->st_info) == STT_FUNC)
			*dt_pltgot += loff;
		else
			*dt_pltgot = dt_symtab->st_value + loff;
		dt_pltgot++;
		dt_symtab++;
	}
}
