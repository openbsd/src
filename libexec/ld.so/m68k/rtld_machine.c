/*	$OpenBSD: rtld_machine.c,v 1.3 2013/06/01 09:57:58 miod Exp $ */

/*
 * Copyright (c) 2013 Miodrag Vallat.
 *
 * Permission to use, copy, modify, and distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 * ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
 * OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */
/*
 * Copyright (c) 1999 Dale Rahn
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

#include <nlist.h>
#include <link.h>
#include <signal.h>

#include "syscall.h"
#include "archdep.h"
#include "resolve.h"

Elf_Addr _dl_bind(elf_object_t *object, int reloff);

int
_dl_md_reloc(elf_object_t *object, int rel, int relasz)
{
	int	i;
	int	numrela;
	int	relrel;
	int	fails = 0;
	struct load_list *llist;
	Elf32_Addr loff;
	Elf32_Rela  *relas;

	loff = object->obj_base;
	numrela = object->Dyn.info[relasz] / sizeof(Elf32_Rela);
	relrel = rel == DT_RELA ? object->relacount : 0;
	relas = (Elf32_Rela *)(object->Dyn.info[rel]);

#ifdef DL_PRINTF_DEBUG
	_dl_printf("object relocation size %x, numrela %x\n",
	    object->Dyn.info[relasz], numrela);
#endif

	if (relas == NULL)
		return(0);

	if (relrel > numrela) {
		_dl_printf("relacount > numrela: %ld > %ld\n", relrel, numrela);
		_dl_exit(20);
	}

	/*
	 * Change protection of all write protected segments in the object
	 * so we can do relocations such as PC32. After relocation,
	 * restore protection.
	 */
	if (object->dyn.textrel == 1 && (rel == DT_REL || rel == DT_RELA)) {
		for (llist = object->load_list; llist != NULL;
		    llist = llist->next) {
			if (!(llist->prot & PROT_WRITE)) {
				_dl_mprotect(llist->start, llist->size,
				    llist->prot|PROT_WRITE);
			}
		}
	}

	/* tight loop for leading RELATIVE relocs */
	for (i = 0; i < relrel; i++, relas++) {
		Elf32_Addr *r_addr;

#ifdef DEBUG
		if (ELF_R_TYPE(relas->r_info) != R_68K_RELATIVE) {
			_dl_printf("RELACOUNT wrong\n");
			_dl_exit(20);
		}
#endif
		r_addr = (Elf32_Addr *)(relas->r_offset + loff);
		*r_addr = relas->r_addend + loff;
	}
	for (; i < numrela; i++, relas++) {
		Elf32_Addr *r_addr = (Elf32_Addr *)(relas->r_offset + loff);
		Elf32_Addr ooff, addend, newval;
		const Elf32_Sym *sym, *this;
		const char *symn;
		int type;
		Elf32_Addr prev_value = 0, prev_ooff = 0;
		const Elf32_Sym *prev_sym = NULL;

		type = ELF32_R_TYPE(relas->r_info);

		if (type == R_68K_JMP_SLOT && rel != DT_JMPREL)
			continue;

		if (type == R_68K_NONE)
			continue;

		sym = object->dyn.symtab;
		sym += ELF32_R_SYM(relas->r_info);
		symn = object->dyn.strtab + sym->st_name;

		if (type == R_68K_COPY) {
			/*
			 * we need to find a symbol, that is not in the current
			 * object, start looking at the beginning of the list,
			 * searching all objects but _not_ the current object,
			 * first one found wins.
			 */
			const Elf32_Sym *cpysrc = NULL;
			Elf32_Addr src_loff;
			int size;

			src_loff = 0;
			src_loff = _dl_find_symbol(symn, &cpysrc,
			    SYM_SEARCH_OTHER|SYM_WARNNOTFOUND| SYM_NOTPLT,
			    sym, object, NULL);
			if (cpysrc != NULL) {
				size = sym->st_size;
				if (sym->st_size != cpysrc->st_size) {
					/* _dl_find_symbol() has warned
					   about this already */
					size = sym->st_size < cpysrc->st_size ?
					    sym->st_size : cpysrc->st_size;
				}
				_dl_bcopy((void *)(src_loff + cpysrc->st_value),
				    r_addr, size);
			} else
				fails++;

			continue;
		}

		if (ELF32_R_SYM(relas->r_info) &&
		    !(ELF32_ST_BIND(sym->st_info) == STB_LOCAL &&
		    ELF32_ST_TYPE (sym->st_info) == STT_NOTYPE) &&
		    sym != prev_sym) {
			this = NULL;
			ooff = _dl_find_symbol_bysym(object,
			    ELF32_R_SYM(relas->r_info), &this,
			    SYM_SEARCH_ALL|SYM_WARNNOTFOUND|
			    ((type == R_68K_JMP_SLOT) ? SYM_PLT:SYM_NOTPLT),
			    sym, NULL);

			if (this == NULL) {
				if (ELF_ST_BIND(sym->st_info) != STB_WEAK)
					fails++;
				continue;
			}
			prev_sym = sym;
			prev_value = this->st_value;
			prev_ooff = ooff;
		}

		if (ELF32_ST_BIND(sym->st_info) == STB_LOCAL &&
		    (ELF32_ST_TYPE(sym->st_info) == STT_SECTION ||
		    ELF32_ST_TYPE(sym->st_info) == STT_NOTYPE))
			addend = relas->r_addend;
		else
			addend = prev_value + relas->r_addend;

		switch (type) {
		case R_68K_PC32:
			newval = prev_ooff + addend;
			newval -= (Elf_Addr)r_addr;
			*r_addr = newval;
			break;
		case R_68K_32:
		case R_68K_GLOB_DAT:
		case R_68K_JMP_SLOT:
			newval = prev_ooff + addend;
			*r_addr = newval;
			break;
		case R_68K_RELATIVE:
			newval = loff + addend;
			*r_addr = newval;
			break;
		default:
			_dl_printf("%s:"
			    " %s: unsupported relocation '%s' %d at %x\n",
			    _dl_progname, object->load_name, symn,
			    ELF32_R_TYPE(relas->r_info), r_addr);
			_dl_exit(1);
		}
	}

	/* reprotect the unprotected segments */
	if (object->dyn.textrel == 1 && (rel == DT_REL || rel == DT_RELA)) {
		for (llist = object->load_list; llist != NULL;
		    llist = llist->next) {
			if (!(llist->prot & PROT_WRITE))
				_dl_mprotect(llist->start, llist->size,
				    llist->prot);
		}
	}

	return(fails);
}

/*
 *	Relocate the Global Offset Table (GOT).
 *	This is done by calling _dl_md_reloc on DT_JMPREL for DL_BIND_NOW,
 *	otherwise the lazy binding plt initialization is performed.
 */
int
_dl_md_reloc_got(elf_object_t *object, int lazy)
{
	extern void _dl_bind_start(void);	/* XXX */
	int	fails = 0;
	Elf_Addr *pltgot = (Elf_Addr *)object->Dyn.info[DT_PLTGOT];
	Elf_Addr ooff;
	const Elf_Sym *this;

	if (pltgot == NULL)
		return (0);

	pltgot[1] = (Elf_Addr)object;
	pltgot[2] = (Elf_Addr)_dl_bind_start;

	if (object->Dyn.info[DT_PLTREL] != DT_RELA)
		return (0);

	object->got_addr = 0;
	object->got_size = 0;
	this = NULL;
	ooff = _dl_find_symbol("__got_start", &this,
	    SYM_SEARCH_OBJ|SYM_NOWARNNOTFOUND|SYM_PLT, NULL, object, NULL);
	if (this != NULL)
		object->got_addr = ooff + this->st_value;

	this = NULL;
	ooff = _dl_find_symbol("__got_end", &this,
	    SYM_SEARCH_OBJ|SYM_NOWARNNOTFOUND|SYM_PLT, NULL, object, NULL);
	if (this != NULL)
		object->got_size = ooff + this->st_value  - object->got_addr;

#if 0
	plt_addr = 0;
	object->plt_size = 0;
	this = NULL;
	ooff = _dl_find_symbol("__plt_start", &this,
	    SYM_SEARCH_OBJ|SYM_NOWARNNOTFOUND|SYM_PLT, NULL, object, NULL);
	if (this != NULL)
		plt_addr = ooff + this->st_value;

	this = NULL;
	ooff = _dl_find_symbol("__plt_end", &this,
	    SYM_SEARCH_OBJ|SYM_NOWARNNOTFOUND|SYM_PLT, NULL, object, NULL);
	if (this != NULL)
		object->plt_size = ooff + this->st_value  - plt_addr;
#endif

	if (object->got_addr == 0)
		object->got_start = 0;
	else {
		object->got_start = ELF_TRUNC(object->got_addr, _dl_pagesz);
		object->got_size += object->got_addr - object->got_start;
		object->got_size = ELF_ROUND(object->got_size, _dl_pagesz);
	}
#if 0
	if (plt_addr == 0)
		object->plt_start = 0;
	else {
		object->plt_start = ELF_TRUNC(plt_addr, _dl_pagesz);
		object->plt_size += plt_addr - object->plt_start;
		object->plt_size = ELF_ROUND(object->plt_size, _dl_pagesz);
	}
#endif

	if (object->traced)
		lazy = 1;

	if (!lazy) {
		fails = _dl_md_reloc(object, DT_JMPREL, DT_PLTRELSZ);
	} else {
		if (object->obj_base != 0) {
			int i, size;
			Elf_Addr *addr;
			Elf_RelA *rela;

			size = object->Dyn.info[DT_PLTRELSZ] / sizeof(Elf_RelA);
			rela = (Elf_RelA *)object->Dyn.info[DT_JMPREL];

			for (i = 0; i < size; i++) {
				addr = (Elf_Addr *)(object->obj_base +
				    rela[i].r_offset);
				*addr += object->obj_base;
			}
		}
	}
	if (object->got_size != 0) {
		_dl_mprotect((void*)object->got_start, object->got_size,
		    PROT_READ);
	}
	/* PLT is already RO, no point in mprotecting it, just do GOT */
#if 0
	if (object->plt_size != 0)
		_dl_mprotect((void*)object->plt_start, object->plt_size,
		    PROT_READ|PROT_EXEC);
#endif

	return (fails);
}

Elf_Addr
_dl_bind(elf_object_t *object, int reloff)
{
	Elf_RelA *rel;
	Elf_Addr *r_addr, ooff, value;
	const Elf_Sym *sym, *this;
	const char *symn;
	const elf_object_t *sobj;
	sigset_t savedmask;

	rel = (Elf_RelA *)(object->Dyn.info[DT_JMPREL] + reloff);

	sym = object->dyn.symtab;
	sym += ELF_R_SYM(rel->r_info);
	symn = object->dyn.strtab + sym->st_name;

	r_addr = (Elf_Addr *)(object->obj_base + rel->r_offset);
	this = NULL;
	ooff = _dl_find_symbol(symn, &this,
	    SYM_SEARCH_ALL|SYM_WARNNOTFOUND|SYM_PLT, sym, object, &sobj);
	if (this == NULL) {
		_dl_printf("lazy binding failed!\n");
		*((int *)0) = 0;	/* XXX */
	}

	value = ooff + this->st_value + rel->r_addend;

	if (sobj->traced && _dl_trace_plt(sobj, symn))
		return value;

	/* if GOT is protected, allow the write */
	if (object->got_size != 0)  {
		_dl_thread_bind_lock(0, &savedmask);
		_dl_mprotect((void*)object->got_start, object->got_size,
		    PROT_READ|PROT_WRITE);
	}

	*r_addr = value;

	/* put the GOT back to RO */
	if (object->got_size != 0) {
		_dl_mprotect((void*)object->got_start, object->got_size,
		    PROT_READ);
		_dl_thread_bind_lock(1, &savedmask);
	}

	return (value);
}
