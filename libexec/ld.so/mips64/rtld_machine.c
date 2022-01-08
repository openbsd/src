/*	$OpenBSD: rtld_machine.c,v 1.35 2022/01/08 06:49:42 guenther Exp $ */

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
#include <sys/exec_elf.h>
#include <sys/syscall.h>
#include <sys/unistd.h>

#include <machine/reloc.h>

#include "util.h"
#include "resolve.h"

int64_t pcookie __attribute__((section(".openbsd.randomdata"))) __dso_hidden;

static inline void
_dl_reloc_ent(Elf_Addr r_addr, Elf_Addr value)
{
	if ((r_addr & 7) == 0)
		*(u_int64_t *)r_addr += value;
	else {
		/*
		 * XXX Handle non aligned relocs. .eh_frame
		 * XXX in libstdc++ seems to have them...
		 */
		u_int64_t robj;

		_dl_bcopy((char *)r_addr, &robj, sizeof(robj));
		robj += value;
		_dl_bcopy(&robj, (char *)r_addr, sizeof(robj));
	}
}

int
_dl_md_reloc(elf_object_t *object, int rel, int relsz)
{
	int	i;
	int	numrel;
	int	fails = 0;
	Elf_Addr loff;
	Elf_Rel  *relocs;
	const Elf_Sym *sym, *this;
	Elf_Addr prev_value = 0;
	const Elf_Sym *prev_sym = NULL;

	loff = object->obj_base;
	numrel = object->Dyn.info[relsz] / sizeof(Elf_Rel);
	relocs = (Elf_Rel *)(object->Dyn.info[rel]);

	if (relocs == NULL)
		return 0;

	DL_DEB(("relocating %d\n", numrel));
	for (i = 0; i < numrel; i++, relocs++) {
		Elf_Addr r_addr = relocs->r_offset + loff;
		const char *symn;

		if (ELF_R_SYM(relocs->r_info) == 0xffffff)
			continue;

		sym = object->dyn.symtab;
		sym += ELF_R_SYM(relocs->r_info);
		symn = object->dyn.strtab + sym->st_name;

		this = NULL;
		if (ELF_R_SYM(relocs->r_info)) {
			if (sym == prev_sym)
				this = sym;	/* XXX non-NULL */
			else if (!(ELF_ST_BIND(sym->st_info) == STB_LOCAL &&
			    ELF_ST_TYPE (sym->st_info) == STT_NOTYPE)) {
				struct sym_res sr;

				sr = _dl_find_symbol(symn,
				    SYM_SEARCH_ALL | SYM_WARNNOTFOUND | SYM_PLT,
				    sym, object);

				if (sr.sym == NULL) {
					if (ELF_ST_BIND(sym->st_info) !=
					    STB_WEAK)
						fails++;
					continue;
				}
				prev_sym = sym;
				prev_value = sr.obj->obj_base +
				    sr.sym->st_value;
				this = sym;	/* XXX non-NULL */
			}
		}

		switch (ELF_R_TYPE(relocs->r_info)) {
		case R_MIPS_REL32_64:
			if (ELF_ST_BIND(sym->st_info) == STB_LOCAL &&
			    (ELF_ST_TYPE(sym->st_info) == STT_SECTION ||
			    ELF_ST_TYPE(sym->st_info) == STT_NOTYPE) )
				_dl_reloc_ent(r_addr, loff + sym->st_value);
			else if (this)
				_dl_reloc_ent(r_addr, prev_value);
			break;

		case R_MIPS_NONE:
			break;

		default:
			_dl_die("unsupported relocation '%llu'",
			    ELF_R_TYPE(relocs->r_info));
		}
	}

	DL_DEB(("done %d fails\n", fails));
	return fails;
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
	Elf_Addr loff;
	Elf_Addr *gotp;
	const Elf_Sym  *symp;
	const char *strt;

	if (object->status & STAT_GOT_DONE)
		return 0;

	loff = object->obj_base;
	strt = object->dyn.strtab;
	gotp = object->dyn.pltgot;
	n = object->Dyn.info[DT_MIPS_LOCAL_GOTNO - DT_LOPROC + DT_NUM];

	DL_DEB(("loff: 0x%lx\n", (unsigned long)loff));
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

	/*
	 *  Then do all global references according to the ABI.
	 *  Quickstart is not yet implemented.
	 */
	while (n--) {
		const char *symn = strt + symp->st_name;
		struct sym_res sr;

		if (symp->st_shndx == SHN_UNDEF &&
		    ELF_ST_TYPE(symp->st_info) == STT_FUNC) {
			if (symp->st_value == 0 || !lazy) {
				sr = _dl_find_symbol(symn,
				    SYM_SEARCH_ALL|SYM_NOWARNNOTFOUND|SYM_PLT,
				    symp, object);
				if (sr.sym)
					*gotp = sr.sym->st_value +
					    sr.obj->obj_base;
			} else
				*gotp = symp->st_value + loff;
		} else if (symp->st_shndx == SHN_COMMON ||
			symp->st_shndx == SHN_UNDEF) {
			sr = _dl_find_symbol(symn,
			    SYM_SEARCH_ALL|SYM_NOWARNNOTFOUND|SYM_PLT,
			    symp, object);
			if (sr.sym)
				*gotp = sr.sym->st_value + sr.obj->obj_base;
		} else if ((ELF_ST_TYPE(symp->st_info) == STT_FUNC &&
			symp->st_value != *gotp) ||
			ELF_ST_VISIBILITY(symp->st_other) == STV_PROTECTED) {
			*gotp += loff;
		} else {	/* Resolve all others immediately */
			sr = _dl_find_symbol(symn,
			    SYM_SEARCH_ALL|SYM_NOWARNNOTFOUND|SYM_PLT,
			    symp, object);
			if (sr.sym)
				*gotp = sr.sym->st_value + sr.obj->obj_base;
			else
				*gotp = symp->st_value + loff;
		}
		gotp++;
		symp++;
	}
	object->status |= STAT_GOT_DONE;

	return 0;
}

Elf_Addr
_dl_bind(elf_object_t *object, int symidx)
{
	Elf_Addr *gotp = object->dyn.pltgot;
	struct sym_res sr;
	const Elf_Sym *sym;
	const char *symn;
	int64_t cookie = pcookie;
	struct {
		struct __kbind param;
		Elf_Addr newval;
	} buf;
	int n;

	sym = object->dyn.symtab;
	sym += symidx;
	symn = object->dyn.strtab + sym->st_name;
	n = object->Dyn.info[DT_MIPS_LOCAL_GOTNO - DT_LOPROC + DT_NUM] -
	    object->Dyn.info[DT_MIPS_GOTSYM - DT_LOPROC + DT_NUM];

	sr = _dl_find_symbol(symn, SYM_SEARCH_ALL|SYM_WARNNOTFOUND|SYM_PLT,
	    sym, object);
	if (sr.sym == NULL)
		_dl_die("lazy binding failed!");

	buf.newval = sr.obj->obj_base + sr.sym->st_value;

	if (__predict_false(sr.obj->traced) && _dl_trace_plt(sr.obj, symn))
		return buf.newval;

	buf.param.kb_addr = &gotp[n + symidx];
	buf.param.kb_size = sizeof(Elf_Addr);

	/* directly code the syscall, so that it's actually inline here */
	{
		register long syscall_num __asm("v0") = SYS_kbind;
		register void *arg1 __asm("a0") = &buf;
		register long  arg2 __asm("a1") = sizeof(buf);
		register long  arg3 __asm("a2") = cookie;

		__asm volatile("syscall" : "+r" (syscall_num)
		    : "r" (arg1), "r" (arg2), "r" (arg3)
		    : "v1", "a3", "memory");
	}

	return buf.newval;
}
