/*	$OpenBSD: rtld_machine.c,v 1.9 2002/11/14 15:15:54 drahn Exp $ */

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
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *	This product includes software developed by Opsycon AB, Sweden.
 * 4. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission.
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

#define _DYN_LOADER

#include <sys/types.h>

#include <link.h>
#include "resolve.h"
#include "syscall.h"
#include "archdep.h"

int
_dl_md_reloc(elf_object_t *object, int rel, int relsz)
{
	int	i;
	int	numrel;
	int	fails = 0;
	Elf32_Addr loff;
	Elf32_Rel  *relocs;

	loff = object->load_offs;
	numrel = object->Dyn.info[relsz] / sizeof(Elf32_Rel);
	relocs = (Elf32_Rel *)(object->Dyn.info[rel]);

	if (relocs == NULL)
		return(0);

	for (i = 0; i < numrel; i++, relocs++) {
		Elf32_Addr r_addr = relocs->r_offset + loff;
		Elf32_Addr ooff = 0;
		const Elf32_Sym *sym, *this;
		const char *symn;
		int type;

		if (ELF32_R_SYM(relocs->r_info) == 0xffffff)
			continue;

		sym = object->dyn.symtab;
		sym += ELF32_R_SYM(relocs->r_info);
		this = sym;
		symn = object->dyn.strtab + sym->st_name;
		type = ELF32_R_TYPE(relocs->r_info);

		if (ELF32_R_SYM(relocs->r_info) &&
		    !(ELF32_ST_BIND(sym->st_info) == STB_LOCAL &&
		    ELF32_ST_TYPE (sym->st_info) == STT_NOTYPE)) {
			ooff = _dl_find_symbol(symn, _dl_objects, &this,
			SYM_SEARCH_ALL | SYM_NOWARNNOTFOUND | SYM_PLT,
			sym->st_size, object->load_name);
			if (!this && ELF32_ST_BIND(sym->st_info) == STB_GLOBAL) {
				_dl_printf("%s: can't resolve reference '%s'\n",
				    _dl_progname, symn);
				fails++;
			}

		}

		switch (ELF32_R_TYPE(relocs->r_info)) {
		case R_MIPS_REL32:
			if (ELF32_ST_BIND(sym->st_info) == STB_LOCAL &&
			    (ELF32_ST_TYPE(sym->st_info) == STT_SECTION ||
			    ELF32_ST_TYPE(sym->st_info) == STT_NOTYPE) ) {
				*(u_int32_t *)r_addr += loff + sym->st_value;
			} else if (this) {
				*(u_int32_t *)r_addr += this->st_value + ooff;
			}
			break;

		case R_MIPS_NONE:
			break;

		default:
			_dl_printf("%s: unsupported relocation '%s'\n",
			    _dl_progname, symn);
			_dl_exit(1);
		}
	}

	return(fails);
}

/*
 *	Relocate the Global Offset Table (GOT). Currently we don't
 *	do lazy evaluation here because the GNU linker doesn't
 *	follow the ABI spec which says that if an external symbol
 *	is referenced by other relocations than CALL16 and 26 it
 *	should not be given a stub and have a zero value in the
 *	symbol table. By not doing so, we can't use pointers to
 *	external functions and use them in comparisons...
 */
void
_dl_md_reloc_got(elf_object_t *object, int lazy)
{
	int	i, n;
	Elf32_Addr loff;
	Elf32_Addr ooff;
	Elf32_Addr *gotp;
	const Elf32_Sym  *symp;
	const Elf32_Sym  *this;
	const char *strt;

	if (object->status & STAT_GOT_DONE)
		return;

	lazy = 0;	/* XXX Fix ld before enabling lazy */
	loff = object->load_offs;
	strt = object->dyn.strtab;
	gotp = object->dyn.pltgot;
	n = object->Dyn.info[DT_MIPS_LOCAL_GOTNO - DT_LOPROC + DT_NUM];

	/*
	 *  Set up pointers for run time (lazy) resolving.
	 */
	gotp[0] = (int)_dl_rt_resolve;
	if (gotp[1] & 0x80000000) {
		gotp[1] = (int)object | 0x80000000;
	}

	/*  First do all local references. */
	for (i = ((gotp[1] & 0x80000000) ? 2 : 1); i < n; i++) {
		gotp[i] += loff;
	}

	gotp += n;

	symp =  object->dyn.symtab;
	symp += object->Dyn.info[DT_MIPS_GOTSYM - DT_LOPROC + DT_NUM];
	n =  object->Dyn.info[DT_MIPS_SYMTABNO - DT_LOPROC + DT_NUM] -
	    object->Dyn.info[DT_MIPS_GOTSYM - DT_LOPROC + DT_NUM];

	/*
	 *  Then do all global references according to the ABI.
	 *  Quickstart is not yet implemented.
	 */
	while (n--) {
		if (symp->st_shndx == SHN_UNDEF &&
		    ELF32_ST_TYPE(symp->st_info) == STT_FUNC) {
DL_DEB(("got: '%s' = %x\n", strt + symp->st_name, symp->st_value));
			if (symp->st_value == 0 || !lazy) {
				this = 0;
				ooff = _dl_find_symbol(strt + symp->st_name,
				    _dl_objects, &this,
				    SYM_SEARCH_ALL|SYM_NOWARNNOTFOUND|SYM_PLT,
				    symp->st_size, object->load_name);
				if (this)
					*gotp = this->st_value + ooff;
			} else
				*gotp = symp->st_value + ooff;
		} else if (symp->st_shndx == SHN_COMMON ||
			symp->st_shndx == SHN_UNDEF) {
			this = 0;
			ooff = _dl_find_symbol(strt + symp->st_name,
			    _dl_objects, &this,
			    SYM_SEARCH_ALL|SYM_NOWARNNOTFOUND|SYM_PLT,
			    symp->st_size, object->load_name);
			if (this)
				*gotp = this->st_value + ooff;
		} else if (ELF32_ST_TYPE(symp->st_info) == STT_FUNC) {
			*gotp += loff;
		} else {	/* XXX ??? */	/* Resolve all others immediatly */
			*gotp = symp->st_value + loff;
		}
		gotp++;
		symp++;
	}
	object->status |= STAT_GOT_DONE;
}
