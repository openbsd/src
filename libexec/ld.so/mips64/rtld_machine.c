/*	$OpenBSD: rtld_machine.c,v 1.12 2010/03/27 20:16:15 kettenis Exp $ */

/*
 * Copyright (c) 1998-2004 Opsycon AB, Sweden.
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

#define _DYN_LOADER

#include <sys/types.h>
#include <sys/mman.h>

#include <link.h>
#include <signal.h>

#include "resolve.h"
#include "syscall.h"
#include "archdep.h"

int
_dl_md_reloc(elf_object_t *object, int rel, int relsz)
{
	int	i;
	int	numrel;
	int	fails = 0;
	struct load_list *load_list;
	Elf64_Addr loff;
	Elf64_Addr ooff;
	Elf64_Addr got_start, got_end;
	Elf64_Rel  *relocs;
	const Elf64_Sym *sym, *this;

	loff = object->obj_base;
	numrel = object->Dyn.info[relsz] / sizeof(Elf64_Rel);
	relocs = (Elf64_Rel *)(object->Dyn.info[rel]);

	if (relocs == NULL)
		return(0);

	/*
	 * Change protection of all write protected segments in the
	 * object so we can do relocations in the .rodata section.
	 * After relocation restore protection.
	 */
	load_list = object->load_list;
	while (load_list != NULL) {
		if ((load_list->prot & PROT_WRITE) == 0)
			_dl_mprotect(load_list->start, load_list->size,
			    load_list->prot|PROT_WRITE);
		load_list = load_list->next;
	}

	/* XXX We need the got limits to know if reloc is in got. */
	/* XXX Relocs against the got should not include the STUB address! */
	this = NULL;
	got_start = 0;
	got_end = 0;
	ooff = _dl_find_symbol("__got_start", &this,
	    SYM_SEARCH_OBJ|SYM_NOWARNNOTFOUND|SYM_PLT, NULL, object, NULL);
	if (this != NULL)
		got_start = ooff + this->st_value;
	this = NULL;
	ooff = _dl_find_symbol("__got_end", &this,
	    SYM_SEARCH_OBJ|SYM_NOWARNNOTFOUND|SYM_PLT, NULL, object, NULL);
	if (this != NULL)
		got_end = ooff + this->st_value;

	DL_DEB(("relocating %d\n", numrel));
	for (i = 0; i < numrel; i++, relocs++) {
		Elf64_Addr r_addr = relocs->r_offset + loff;
		const char *symn;
		int type;

		if (ELF64_R_SYM(relocs->r_info) == 0xffffff)
			continue;

		ooff = 0;
		sym = object->dyn.symtab;
		sym += ELF64_R_SYM(relocs->r_info);
		symn = object->dyn.strtab + sym->st_name;
		type = ELF64_R_TYPE(relocs->r_info);

		this = NULL;
		if (ELF64_R_SYM(relocs->r_info) &&
		    !(ELF64_ST_BIND(sym->st_info) == STB_LOCAL &&
		    ELF64_ST_TYPE (sym->st_info) == STT_NOTYPE)) {
			ooff = _dl_find_symbol(symn, &this,
			SYM_SEARCH_ALL | SYM_WARNNOTFOUND | SYM_PLT,
			sym, object, NULL);

			if (this == NULL) {
				if (ELF_ST_BIND(sym->st_info) != STB_WEAK)
					fails++;
				continue;
			}
		}

		switch (ELF64_R_TYPE(relocs->r_info)) {
			/* XXX Handle non aligned relocs. .eh_frame
			 * XXX in libstdc++ seems to have them... */
			u_int64_t robj;

		case R_MIPS_REL32_64:
			if (ELF64_ST_BIND(sym->st_info) == STB_LOCAL &&
			    (ELF64_ST_TYPE(sym->st_info) == STT_SECTION ||
			    ELF64_ST_TYPE(sym->st_info) == STT_NOTYPE) ) {
				if ((long)r_addr & 7) {
					_dl_bcopy((char *)r_addr, &robj, sizeof(robj));
					robj += loff + sym->st_value;
					_dl_bcopy(&robj, (char *)r_addr, sizeof(robj));
				} else {
					*(u_int64_t *)r_addr += loff + sym->st_value;
				}
			} else if (this && ((long)r_addr & 7)) {
				_dl_bcopy((char *)r_addr, &robj, sizeof(robj));
				robj += this->st_value + ooff;
				_dl_bcopy(&robj, (char *)r_addr, sizeof(robj));
			} else if (this) {
				*(u_int64_t *)r_addr += this->st_value + ooff;
			}
			break;

		case R_MIPS_NONE:
			break;

		default:
			_dl_printf("%s: unsupported relocation '%d'\n",
			    _dl_progname, ELF64_R_TYPE(relocs->r_info));
			_dl_exit(1);
		}
	}
	DL_DEB(("done %d fails\n", fails));
	load_list = object->load_list;
	while (load_list != NULL) {
		if ((load_list->prot & PROT_WRITE) == 0)
			_dl_mprotect(load_list->start, load_list->size,
			    load_list->prot);
		load_list = load_list->next;
	}
	return(fails);
}

extern void _dl_bind_start(void);

/*
 *	Relocate the Global Offset Table (GOT). Currently we don't
 *	do lazy evaluation here because the GNU linker doesn't
 *	follow the ABI spec which says that if an external symbol
 *	is referenced by other relocations than CALL16 and 26 it
 *	should not be given a stub and have a zero value in the
 *	symbol table. By not doing so, we can't use pointers to
 *	external functions and use them in comparisons...
 */
int
_dl_md_reloc_got(elf_object_t *object, int lazy)
{
	int	i, n;
	Elf64_Addr loff;
	Elf64_Addr ooff;
	Elf64_Addr *gotp;
	const Elf64_Sym  *symp;
	const Elf64_Sym  *this;
	const char *strt;

	if (object->status & STAT_GOT_DONE)
		return (0);

	loff = object->obj_base;
	strt = object->dyn.strtab;
	gotp = object->dyn.pltgot;
	n = object->Dyn.info[DT_MIPS_LOCAL_GOTNO - DT_LOPROC + DT_NUM];

	DL_DEB(("loff: '%p'\n", loff));
	/*
	 *  Set up pointers for run time (lazy) resolving.
	 */
	gotp[0] = (long)_dl_bind_start;
	gotp[1] = (long)object;

	/*  First do all local references. */
	for (i = 2; i < n; i++) {
		gotp[i] += loff;
	}

	gotp += n;

	symp =  object->dyn.symtab;
	symp += object->Dyn.info[DT_MIPS_GOTSYM - DT_LOPROC + DT_NUM];
	n =  object->Dyn.info[DT_MIPS_SYMTABNO - DT_LOPROC + DT_NUM] -
	    object->Dyn.info[DT_MIPS_GOTSYM - DT_LOPROC + DT_NUM];

	this = NULL;
	object->plt_size = 0;
	object->got_size = 0;
	ooff = _dl_find_symbol("__got_start", &this,
	    SYM_SEARCH_OBJ|SYM_NOWARNNOTFOUND|SYM_PLT, NULL, object, NULL);
	if (this != NULL)
		object->got_start = ooff + this->st_value;

	this = NULL;
	ooff = _dl_find_symbol("__got_end", &this,
	    SYM_SEARCH_OBJ|SYM_NOWARNNOTFOUND|SYM_PLT, NULL, object, NULL);
	if (this != NULL)
		object->got_size = ooff + this->st_value  - object->got_start;

	/*
	 *  Then do all global references according to the ABI.
	 *  Quickstart is not yet implemented.
	 */
	while (n--) {
		if (symp->st_shndx == SHN_UNDEF &&
		    ELF64_ST_TYPE(symp->st_info) == STT_FUNC) {
			if (symp->st_value == 0 || !lazy) {
				this = 0;
				ooff = _dl_find_symbol(strt + symp->st_name,
				    &this,
				    SYM_SEARCH_ALL|SYM_NOWARNNOTFOUND|SYM_PLT,
				    symp, object, NULL);
				if (this)
					*gotp = this->st_value + ooff;
			} else
				*gotp = symp->st_value + loff;
		} else if (symp->st_shndx == SHN_COMMON ||
			symp->st_shndx == SHN_UNDEF) {
			this = 0;
			ooff = _dl_find_symbol(strt + symp->st_name, &this,
			    SYM_SEARCH_ALL|SYM_NOWARNNOTFOUND|SYM_PLT,
			    symp, object, NULL);
			if (this)
				*gotp = this->st_value + ooff;
		} else if (ELF64_ST_TYPE(symp->st_info) == STT_FUNC &&
			symp->st_value != *gotp) {
			*gotp += loff;
		} else {	/* Resolve all others immediately */
			this = 0;
			ooff = _dl_find_symbol(strt + symp->st_name, &this,
			    SYM_SEARCH_ALL|SYM_NOWARNNOTFOUND|SYM_PLT,
			    symp, object, NULL);
			if (this)
				*gotp = this->st_value + ooff;
			else
				*gotp = symp->st_value + loff;
		}
		gotp++;
		symp++;
	}
	object->status |= STAT_GOT_DONE;

	DL_DEB(("got: %x, %x\n", object->got_start, object->got_size));
	if (object->got_size != 0)
		_dl_mprotect((void*)object->got_start, object->got_size,
		    PROT_READ);

	return (0);
}

Elf_Addr
_dl_bind(elf_object_t *object, int symidx)
{
	Elf_Addr *gotp = object->dyn.pltgot;
	Elf_Addr *addr, ooff;
	const Elf_Sym *sym, *this;
	const char *symn;
	sigset_t omask, nmask;
	int n;

	sym = object->dyn.symtab;
	sym += symidx;
	symn = object->dyn.strtab + sym->st_name;
	n = object->Dyn.info[DT_MIPS_LOCAL_GOTNO - DT_LOPROC + DT_NUM] -
	    object->Dyn.info[DT_MIPS_GOTSYM - DT_LOPROC + DT_NUM];

	ooff = _dl_find_symbol(symn, &this,
	    SYM_SEARCH_ALL|SYM_WARNNOTFOUND|SYM_PLT, sym, object, NULL);
	if (this == NULL) {
		_dl_printf("lazy binding failed\n");
		*((int *)0) = 0;	/* XXX */
	}

	addr = &gotp[n + symidx];

	/* if GOT is protected, allow the write */
	if (object->got_size != 0) {
		sigfillset(&nmask);
		_dl_sigprocmask(SIG_BLOCK, &nmask, &omask);
		_dl_thread_bind_lock(0);
		_dl_mprotect(addr, sizeof(Elf_Addr), PROT_READ|PROT_WRITE);
	}

	*addr = ooff + this->st_value;

	/* if GOT is (to be protected, change back to RO */
	if (object->got_size != 0) {
		_dl_mprotect(addr, sizeof (Elf_Addr), PROT_READ);
		_dl_thread_bind_lock(1);
		_dl_sigprocmask(SIG_SETMASK, &omask, NULL);
	}

	return *addr;
}
