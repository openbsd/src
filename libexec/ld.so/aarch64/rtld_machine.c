/*	$OpenBSD: rtld_machine.c,v 1.7 2018/11/16 21:15:47 guenther Exp $ */

/*
 * Copyright (c) 2004 Dale Rahn
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
#include <sys/syscall.h>
#include <sys/unistd.h>

#include <nlist.h>
#include <link.h>

#include "syscall.h"
#include "archdep.h"
#include "resolve.h"

int64_t pcookie __attribute__((section(".openbsd.randomdata"))) __dso_hidden;

void _dl_bind_start(void); /* XXX */
Elf_Addr _dl_bind(elf_object_t *object, int index);
#define _RF_S		0x80000000		/* Resolve symbol */
#define _RF_A		0x40000000		/* Use addend */
#define _RF_P		0x20000000		/* Location relative */
#define _RF_G		0x10000000		/* GOT offset */
#define _RF_B		0x08000000		/* Load address relative */
#define _RF_U		0x04000000		/* Unaligned */
#define _RF_V		0x02000000		/* ERROR */
#define _RF_SZ(s)	(((s) & 0xff) << 8)	/* memory target size */
#define _RF_RS(s)	((s) & 0xff)		/* right shift */
static const int reloc_target_flags[] = {
	[ R_AARCH64_NONE ] = 0,
	[ R_AARCH64_ABS64 ] =
	  _RF_V|_RF_S|_RF_A|		_RF_SZ(64) | _RF_RS(0),	/* ABS64 */
	[ R_AARCH64_GLOB_DAT ] =
	  _RF_V|_RF_S|_RF_A|		_RF_SZ(64) | _RF_RS(0),	/* GLOB_DAT */
	[ R_AARCH64_JUMP_SLOT ] =
	  _RF_V|_RF_S|			_RF_SZ(64) | _RF_RS(0),	/* JUMP_SLOT */
	[ R_AARCH64_RELATIVE ] =
	  _RF_V|_RF_B|_RF_A|		_RF_SZ(64) | _RF_RS(0),	/* REL64 */
	[ R_AARCH64_TLSDESC ] =
	  _RF_V|_RF_S,
	[ R_AARCH64_TLS_TPREL64 ] =
	  _RF_V|_RF_S,
	[ R_AARCH64_COPY ] =
	  _RF_V|_RF_S|			_RF_SZ(32) | _RF_RS(0),	/* 20 COPY */

};

#define RELOC_RESOLVE_SYMBOL(t)		((reloc_target_flags[t] & _RF_S) != 0)
#define RELOC_PC_RELATIVE(t)		((reloc_target_flags[t] & _RF_P) != 0)
#define RELOC_BASE_RELATIVE(t)		((reloc_target_flags[t] & _RF_B) != 0)
#define RELOC_UNALIGNED(t)		((reloc_target_flags[t] & _RF_U) != 0)
#define RELOC_USE_ADDEND(t)		((reloc_target_flags[t] & _RF_A) != 0)
#define RELOC_TARGET_SIZE(t)		((reloc_target_flags[t] >> 8) & 0xff)
#define RELOC_VALUE_RIGHTSHIFT(t)	(reloc_target_flags[t] & 0xff)
static const Elf_Addr reloc_target_bitmask[] = {
#define _BM(x)  (~(Elf_Addr)0 >> ((8*sizeof(reloc_target_bitmask[0])) - (x)))
	[ R_AARCH64_NONE ] = 0,
	[ R_AARCH64_ABS64 ] = _BM(64),
	[ R_AARCH64_GLOB_DAT ] = _BM(64),
	[ R_AARCH64_JUMP_SLOT ] = _BM(64),
	[ R_AARCH64_RELATIVE ] = _BM(64),
	[ R_AARCH64_TLSDESC ] = _BM(64),
	[ R_AARCH64_TLS_TPREL64 ] = _BM(64),
	[ R_AARCH64_COPY ] = _BM(64),
#undef _BM
};
#define RELOC_VALUE_BITMASK(t)	(reloc_target_bitmask[t])

#define R_TYPE(x) R_AARCH64_ ## x

void _dl_reloc_plt(Elf_Word *where, Elf_Addr value, Elf_RelA *rel);

#define nitems(_a)     (sizeof((_a)) / sizeof((_a)[0]))

int
_dl_md_reloc(elf_object_t *object, int rel, int relsz)
{
	long	i;
	long	numrel;
	long	relrel;
	int	fails = 0;
	Elf_Addr loff;
	Elf_Addr prev_value = 0;
	const Elf_Sym *prev_sym = NULL;
	Elf_RelA *rels;
	struct load_list *llist;

	loff = object->obj_base;
	numrel = object->Dyn.info[relsz] / sizeof(Elf_RelA);
	relrel = rel == DT_RELA ? object->relcount : 0;
	rels = (Elf_RelA *)(object->Dyn.info[rel]);

	if (rels == NULL)
		return(0);

	if (relrel > numrel)
		_dl_die("relcount > numrel: %ld > %ld", relrel, numrel);

	/*
	 * unprotect some segments if we need it.
	 */
	if ((object->dyn.textrel == 1) && (rel == DT_REL || rel == DT_RELA)) {
		for (llist = object->load_list;
		    llist != NULL;
		    llist = llist->next) {
			if (!(llist->prot & PROT_WRITE))
				_dl_mprotect(llist->start, llist->size,
				    llist->prot|PROT_WRITE);
		}
	}

	/* tight loop for leading RELATIVE relocs */
	for (i = 0; i < relrel; i++, rels++) {
		Elf_Addr *where;

#ifdef DEBUG
		if (ELF_R_TYPE(rels->r_info) != R_TYPE(RELATIVE))
			_dl_die("RELCOUNT wrong");
#endif
		where = (Elf_Addr *)(rels->r_offset + loff);
		*where += loff;
	}
	for (; i < numrel; i++, rels++) {
		Elf_Addr *where, value, ooff, mask;
		Elf_Word type;
		const Elf_Sym *sym, *this;
		const char *symn;

		type = ELF_R_TYPE(rels->r_info);

		if (type >= nitems(reloc_target_flags) ||
		    (reloc_target_flags[type] & _RF_V) == 0)
			_dl_die("bad relocation %ld %d", i, type);

		if (type == R_TYPE(NONE))
			continue;

		if (type == R_TYPE(JUMP_SLOT) && rel != DT_JMPREL)
			continue;

		where = (Elf_Addr *)(rels->r_offset + loff);

		if (RELOC_USE_ADDEND(type))
			value = rels->r_addend;
		else
			value = 0;

		sym = NULL;
		symn = NULL;
		if (RELOC_RESOLVE_SYMBOL(type)) {
			sym = object->dyn.symtab;
			sym += ELF_R_SYM(rels->r_info);
			symn = object->dyn.strtab + sym->st_name;

			if (sym->st_shndx != SHN_UNDEF &&
			    ELF_ST_BIND(sym->st_info) == STB_LOCAL) {
				value += loff;
			} else if (sym == prev_sym) {
				value += prev_value;
			} else {
				this = NULL;
				ooff = _dl_find_symbol_bysym(object,
				    ELF_R_SYM(rels->r_info), &this,
				    SYM_SEARCH_ALL|SYM_WARNNOTFOUND|
				    ((type == R_TYPE(JUMP_SLOT)) ?
					SYM_PLT : SYM_NOTPLT),
				    sym, NULL);
				if (this == NULL) {
resolve_failed:
					if (ELF_ST_BIND(sym->st_info) !=
					    STB_WEAK)
						fails++;
					continue;
				}
				prev_sym = sym;
				prev_value = (Elf_Addr)(ooff + this->st_value);
				value += prev_value;
			}
		}

		if (type == R_TYPE(JUMP_SLOT)) {
			/*
			_dl_reloc_plt((Elf_Word *)where, value, rels);
			*/
			*where = value;
			continue;
		}

		if (type == R_TYPE(COPY)) {
			void *dstaddr = where;
			const void *srcaddr;
			const Elf_Sym *dstsym = sym, *srcsym = NULL;
			Elf_Addr soff;

			soff = _dl_find_symbol(symn, &srcsym,
			    SYM_SEARCH_OTHER|SYM_WARNNOTFOUND|SYM_NOTPLT,
			    dstsym, object, NULL);
			if (srcsym == NULL)
				goto resolve_failed;

			srcaddr = (void *)(soff + srcsym->st_value);
			_dl_bcopy(srcaddr, dstaddr, dstsym->st_size);
			continue;
		}

		if (RELOC_PC_RELATIVE(type))
			value -= (Elf_Addr)where;
		if (RELOC_BASE_RELATIVE(type))
			value += loff;

		mask = RELOC_VALUE_BITMASK(type);
		value >>= RELOC_VALUE_RIGHTSHIFT(type);
		value &= mask;

		if (RELOC_UNALIGNED(type)) {
			/* Handle unaligned relocations. */
			Elf_Addr tmp = 0;
			char *ptr = (char *)where;
			int i, size = RELOC_TARGET_SIZE(type)/8;

			/* Read it in one byte at a time. */
			for (i=0; i<size; i++)
				tmp = (tmp << 8) | ptr[i];

			tmp &= ~mask;
			tmp |= value;

			/* Write it back out. */
			for (i=0; i<size; i++)
				ptr[i] = ((tmp >> (8*i)) & 0xff);
		} else {
			*where &= ~mask;
			*where |= value;
		}
	}

	/* reprotect the unprotected segments */
	if ((object->dyn.textrel == 1) && (rel == DT_REL || rel == DT_RELA)) {
		for (llist = object->load_list;
		    llist != NULL;
		    llist = llist->next) {
			if (!(llist->prot & PROT_WRITE))
				_dl_mprotect(llist->start, llist->size,
				    llist->prot);
		}
	}

	return (fails);
}

/*
 *	Relocate the Global Offset Table (GOT).
 *	This is done by calling _dl_md_reloc on DT_JMPREL for DL_BIND_NOW,
 *	otherwise the lazy binding plt initialization is performed.
 */
int
_dl_md_reloc_got(elf_object_t *object, int lazy)
{
	int	fails = 0;
	Elf_Addr *pltgot = (Elf_Addr *)object->Dyn.info[DT_PLTGOT];
	int i, num;
	Elf_RelA *rel;

	if (object->Dyn.info[DT_PLTREL] != DT_RELA)
		return (0);

	if (object->traced)
		lazy = 1;

	if (!lazy) {
		fails = _dl_md_reloc(object, DT_JMPREL, DT_PLTRELSZ);
	} else {
		rel = (Elf_RelA *)(object->Dyn.info[DT_JMPREL]);
		num = (object->Dyn.info[DT_PLTRELSZ]);

		for (i = 0; i < num/sizeof(Elf_RelA); i++, rel++) {
			Elf_Addr *where;
			where = (Elf_Addr *)(rel->r_offset + object->obj_base);
			*where += object->obj_base;
		}

		pltgot[1] = (Elf_Addr)object;
		pltgot[2] = (Elf_Addr)_dl_bind_start;
	}

	return (fails);
}

Elf_Addr
_dl_bind(elf_object_t *object, int relidx)
{
	Elf_RelA *rel;
	const Elf_Sym *sym, *this;
	const char *symn;
	const elf_object_t *sobj;
	Elf_Addr ooff;
	int64_t cookie = pcookie;
	struct {
		struct __kbind param;
		Elf_Addr newval;
	} buf;

	rel = ((Elf_RelA *)object->Dyn.info[DT_JMPREL]) + (relidx);

	sym = object->dyn.symtab;
	sym += ELF_R_SYM(rel->r_info);
	symn = object->dyn.strtab + sym->st_name;

	this = NULL;
	ooff = _dl_find_symbol(symn,  &this,
	    SYM_SEARCH_ALL|SYM_WARNNOTFOUND|SYM_PLT, sym, object, &sobj);
	if (this == NULL)
		_dl_die("lazy binding failed!");

	buf.newval = ooff + this->st_value;

	if (sobj->traced && _dl_trace_plt(sobj, symn))
		return buf.newval;

	buf.param.kb_addr = (Elf_Word *)(object->obj_base + rel->r_offset);
	buf.param.kb_size = sizeof(Elf_Addr);

	/* directly code the syscall, so that it's actually inline here */
	{
		register long syscall_num __asm("x8") = SYS_kbind;
		register void *arg1 __asm("x0") = &buf;
		register long  arg2 __asm("x1") = sizeof(buf);
		register long  arg3 __asm("x2") = cookie;

		__asm volatile("svc 0" : "+r" (arg1), "+r" (arg2)
		    : "r" (syscall_num), "r" (arg3)
		    : "cc", "memory");
	}

	return buf.newval;
}
