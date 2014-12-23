/*	$OpenBSD: boot.h,v 1.3 2014/12/23 16:45:04 kettenis Exp $ */

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

#include <sys/types.h>
#include <sys/mman.h>
#include <sys/exec.h>
#include <sys/param.h>
#include <sys/sysctl.h>
#include <nlist.h>
#include <link.h>
#include <dlfcn.h>

#include "syscall.h"
#include "archdep.h"
#include "path.h"
#include "resolve.h"
#include "sod.h"
#include "stdlib.h"
#include "dl_prebind.h"

#include "../../lib/csu/os-note-elf.h"

#ifdef RCRT0
/*
 * Local decls.
 */
void _dl_boot_bind(const long, long *, Elf_Dyn *);

extern char __plt_start[];
extern char __plt_end[];
extern char __got_start[];
extern char __got_end[];

void
_dl_boot_bind(const long sp, long *dl_data, Elf_Dyn *dynamicp)
{
	struct elf_object  dynld;	/* Resolver data for the loader */
	AuxInfo		*auxstack;
	long		*stack;
	Elf_Dyn		*dynp;
	Elf_Addr	start;
	size_t		size;
	int		n, argc;
	char **argv, **envp;
	long loff;

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
	 * We need to do 'selfreloc' in case the code weren't
	 * loaded at the address it was linked to.
	 *
	 * Scan the DYNAMIC section for the loader.
	 * Cache the data for easier access.
	 */

#if defined(__alpha__)
	dynp = (Elf_Dyn *)((long)_DYNAMIC);
#elif defined(__sparc__) || defined(__sparc64__) || defined(__powerpc__) || \
    defined(__hppa__) || defined(__sh__)
	dynp = dynamicp;
#else
	dynp = (Elf_Dyn *)((long)_DYNAMIC + loff);
#endif
	_dl_memset(dynld.Dyn.info, 0, sizeof(dynld.Dyn.info));
	while (dynp != NULL && dynp->d_tag != DT_NULL) {
		if (dynp->d_tag < DT_NUM)
			dynld.Dyn.info[dynp->d_tag] = dynp->d_un.d_val;
		else if (dynp->d_tag >= DT_LOPROC &&
		    dynp->d_tag < DT_LOPROC + DT_PROCNUM)
			dynld.Dyn.info[dynp->d_tag - DT_LOPROC + DT_NUM] =
			    dynp->d_un.d_val;
		if (dynp->d_tag == DT_TEXTREL)
			dynld.dyn.textrel = 1;
		dynp++;
	}

	/*
	 * Do the 'bootstrap relocation'. This is really only needed if
	 * the code was loaded at another location than it was linked to.
	 * We don't do undefined symbols resolving (to difficult..)
	 */

	/* "relocate" dyn.X values if they represent addresses */
	{
		int i, val;
		/* must be code, not pic data */
		int table[20];

		i = 0;
		table[i++] = DT_PLTGOT;
		table[i++] = DT_HASH;
		table[i++] = DT_STRTAB;
		table[i++] = DT_SYMTAB;
		table[i++] = DT_RELA;
		table[i++] = DT_INIT;
		table[i++] = DT_FINI;
		table[i++] = DT_REL;
		table[i++] = DT_JMPREL;
		/* other processors insert their extras here */
		table[i++] = DT_NULL;
		for (i = 0; table[i] != DT_NULL; i++) {
			val = table[i];
			if (val >= DT_LOPROC && val < DT_LOPROC + DT_PROCNUM)
				val = val - DT_LOPROC + DT_NUM;
			else if (val >= DT_NUM)
				continue;
			if (dynld.Dyn.info[val] != 0)
				dynld.Dyn.info[val] += loff;
		}
	}

	{
		u_int32_t rs;
		Elf_Rel *rp;
		int	i;

		rp = (Elf_Rel *)(dynld.Dyn.info[DT_REL]);
		rs = dynld.dyn.relsz;

		for (i = 0; i < rs; i += sizeof (Elf_Rel), rp++) {
			Elf_Addr *ra;
			const Elf_Sym *sp;

			sp = dynld.dyn.symtab;
			sp += ELF_R_SYM(rp->r_info);

			if (ELF_R_SYM(rp->r_info) && sp->st_value == 0) {
#if 0
/* cannot printf in this function */
				_dl_wrstderr("Dynamic loader failure: self bootstrapping impossible.\n");
				_dl_wrstderr("Undefined symbol: ");
				_dl_wrstderr((char *)dynld.dyn.strtab +
				    sp->st_name);
#endif
#ifdef RCRT0
				continue;
#else
				_dl_exit(5);
#endif
			}

			ra = (Elf_Addr *)(rp->r_offset + loff);
			RELOC_REL(rp, sp, ra, loff);
		}
	}

	for (n = 0; n < 2; n++) {
		unsigned long rs;
		Elf_RelA *rp;
		int	i;

		switch (n) {
		case 0:
			rp = (Elf_RelA *)(dynld.Dyn.info[DT_JMPREL]);
			rs = dynld.dyn.pltrelsz;
			break;
		case 1:
			rp = (Elf_RelA *)(dynld.Dyn.info[DT_RELA]);
			rs = dynld.dyn.relasz;
			break;
		default:
			rp = NULL;
			rs = 0;
		}
		for (i = 0; i < rs; i += sizeof (Elf_RelA), rp++) {
			Elf_Addr *ra;
			const Elf_Sym *sp;

			sp = dynld.dyn.symtab;
			sp += ELF_R_SYM(rp->r_info);
			if (ELF_R_SYM(rp->r_info) && sp->st_value == 0) {
#if 0
				_dl_wrstderr("Dynamic loader failure: self bootstrapping impossible.\n");
				_dl_wrstderr("Undefined symbol: ");
				_dl_wrstderr((char *)dynld.dyn.strtab +
				    sp->st_name);
#endif
#ifdef RCRT0
				continue;
#else
				_dl_exit(6);
#endif
			}

			ra = (Elf_Addr *)(rp->r_offset + loff);
			RELOC_RELA(rp, sp, ra, loff, dynld.dyn.pltgot);
		}
	}

	RELOC_GOT(&dynld, loff);

	/*
	 * we have been fully relocated here, so most things no longer
	 * need the loff adjustment
	 */

	/*
	 * No further changes to the PLT and/or GOT are needed so make
	 * them read-only.
	 */

#if defined(__sparc64__)
	start = ELF_TRUNC((Elf_Addr)__plt_start, PAGE_SIZE);
	size = ELF_ROUND((Elf_Addr)__plt_end - start, PAGE_SIZE);
	mprotect((void *)start, size, PROT_READ);
#endif

	start = ELF_TRUNC((Elf_Addr)__got_start, PAGE_SIZE);
	size = ELF_ROUND((Elf_Addr)__got_end - start, PAGE_SIZE);
	mprotect((void *)start, size, PROT_READ);
}
#endif /* RCRT0 */
