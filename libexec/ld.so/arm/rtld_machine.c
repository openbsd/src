/*	$OpenBSD: rtld_machine.c,v 1.11 2007/05/05 15:21:21 drahn Exp $ */

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

#include <nlist.h>
#include <link.h>
#include <signal.h>

#include "syscall.h"
#include "archdep.h"
#include "resolve.h"

void _dl_bind_start(void); /* XXX */
Elf_Addr _dl_bind(elf_object_t *object, int reloff);
#define _RF_S		0x80000000		/* Resolve symbol */
#define _RF_A		0x40000000		/* Use addend */
#define _RF_P		0x20000000		/* Location relative */
#define _RF_G		0x10000000		/* GOT offset */
#define _RF_B		0x08000000		/* Load address relative */
#define _RF_U		0x04000000		/* Unaligned */
#define _RF_E		0x02000000		/* ERROR */
#define _RF_SZ(s)	(((s) & 0xff) << 8)	/* memory target size */
#define _RF_RS(s)	((s) & 0xff)		/* right shift */
static int reloc_target_flags[] = {
	0,						/*  0 NONE */
	_RF_S|_RF_P|_RF_A|	_RF_SZ(32) | _RF_RS(0),	/*  1 PC24 */
	_RF_S|_RF_A|		_RF_SZ(32) | _RF_RS(0),	/*  2 ABS32 */
	_RF_S|_RF_P|_RF_A|	_RF_SZ(32) | _RF_RS(0),	/*  3 REL32 */
	_RF_S|_RF_P|_RF_A|	_RF_E,			/*  4 REL13 */
	_RF_S|_RF_A|		_RF_E,			/*  5 ABS16 */
	_RF_S|_RF_A|		_RF_E,			/*  6 ABS12 */
	_RF_S|_RF_A|		_RF_E,			/*  7 T_ABS5 */
	_RF_S|_RF_A|		_RF_E,			/*  8 ABS8 */
	_RF_S|_RF_B|_RF_A|	_RF_E,			/*  9 SBREL32 */
	_RF_S|_RF_P|_RF_A|	_RF_E,			/* 10 T_PC22 */
	_RF_S|_RF_P|_RF_A|	_RF_E,			/* 11 T_PC8 */
	_RF_E,						/* 12 Reserved */
	_RF_S|_RF_A|		_RF_E,			/* 13 SWI24 */
	_RF_S|_RF_A|		_RF_E,			/* 14 T_SWI8 */
	_RF_E,						/* 15 OBSL */
	_RF_E,						/* 16 OBSL */
	_RF_E,						/* 17 UNUSED */
	_RF_E,						/* 18 UNUSED */
	_RF_E,						/* 19 UNUSED */
	_RF_S|			_RF_SZ(32) | _RF_RS(0),	/* 20 COPY */
	_RF_S|_RF_A|		_RF_SZ(32) | _RF_RS(0),	/* 21 GLOB_DAT */
	_RF_S|			_RF_SZ(32) | _RF_RS(0),	/* 22 JUMP_SLOT */
	      _RF_A|	_RF_B|	_RF_SZ(32) | _RF_RS(0),	/* 23 RELATIVE */
	_RF_E,						/* 24 GOTOFF */
	_RF_E,						/* 25 GOTPC */
	_RF_E,						/* 26 GOT32 */
	_RF_E,						/* 27 PLT32 */
	_RF_E,						/* 28 UNUSED */
	_RF_E,						/* 29 UNUSED */
	_RF_E,						/* 30 UNUSED */
	_RF_E,						/* 31 UNUSED */
	_RF_E,						/* 32 A_PCR 0 */
	_RF_E,						/* 33 A_PCR 8 */
	_RF_E,						/* 34 A_PCR 16 */
	_RF_E,						/* 35 B_PCR 0 */
	_RF_E,						/* 36 B_PCR 12 */
	_RF_E,						/* 37 B_PCR 20 */
	_RF_E,						/* 38 RELAB32 */
	_RF_E,						/* 39 ROSGREL32 */
	_RF_E,						/* 40 V4BX */
	_RF_E,						/* 41 STKCHK */
	_RF_E						/* 42 TSTKCHK */
};

#define RELOC_RESOLVE_SYMBOL(t)		((reloc_target_flags[t] & _RF_S) != 0)
#define RELOC_PC_RELATIVE(t)		((reloc_target_flags[t] & _RF_P) != 0)
#define RELOC_BASE_RELATIVE(t)		((reloc_target_flags[t] & _RF_B) != 0)
#define RELOC_UNALIGNED(t)		((reloc_target_flags[t] & _RF_U) != 0)
#define RELOC_USE_ADDEND(t)		((reloc_target_flags[t] & _RF_A) != 0)
#define RELOC_TARGET_SIZE(t)		((reloc_target_flags[t] >> 8) & 0xff)
#define RELOC_VALUE_RIGHTSHIFT(t)	(reloc_target_flags[t] & 0xff)
static int reloc_target_bitmask[] = {
#define _BM(x)  (x == 32? ~0 : ~(-(1UL << (x))))
	_BM(0),		/*  0 NONE */
	_BM(24),	/*  1 PC24 */
	_BM(32),	/*  2 ABS32 */
	_BM(32),	/*  3 REL32 */
	_BM(0),		/*  4 REL13 */
	_BM(0),		/*  5 ABS16 */
	_BM(0),		/*  6 ABS12 */
	_BM(0),		/*  7 T_ABS5 */
	_BM(0),		/*  8 ABS8 */
	_BM(32),	/*  9 SBREL32 */
	_BM(0),		/* 10 T_PC22 */
	_BM(0),		/* 11 T_PC8 */
	_BM(0),		/* 12 Reserved */
	_BM(0),		/* 13 SWI24 */
	_BM(0),		/* 14 T_SWI8 */
	_BM(0),		/* 15 OBSL */
	_BM(0),		/* 16 OBSL */
	_BM(0),		/* 17 UNUSED */
	_BM(0),		/* 18 UNUSED */
	_BM(0),		/* 19 UNUSED */
	_BM(32),	/* 20 COPY */
	_BM(32),	/* 21 GLOB_DAT */
	_BM(32),	/* 22 JUMP_SLOT */
	_BM(32),	/* 23 RELATIVE */
	_BM(0),		/* 24 GOTOFF */
	_BM(0),		/* 25 GOTPC */
	_BM(0),		/* 26 GOT32 */
	_BM(0),		/* 27 PLT32 */
	_BM(0),		/* 28 UNUSED */
	_BM(0),		/* 29 UNUSED */
	_BM(0),		/* 30 UNUSED */
	_BM(0),		/* 31 UNUSED */
	_BM(0),		/* 32 A_PCR 0 */
	_BM(0),		/* 33 A_PCR 8 */
	_BM(0),		/* 34 A_PCR 16 */
	_BM(0),		/* 35 B_PCR 0 */
	_BM(0),		/* 36 B_PCR 12 */
	_BM(0),		/* 37 B_PCR 20 */
	_BM(0),		/* 38 RELAB32 */
	_BM(0),		/* 39 ROSGREL32 */
	_BM(0),		/* 40 V4BX */
	_BM(0),		/* 41 STKCHK */
	_BM(0)		/* 42 TSTKCHK */
#undef _BM
};
#define RELOC_VALUE_BITMASK(t)	(reloc_target_bitmask[t])

#define R_TYPE(x) R_ARM_ ## x

void _dl_reloc_plt(Elf_Word *where, Elf_Addr value, Elf_Rel *rel);

int
_dl_md_reloc(elf_object_t *object, int rel, int relsz)
{
	long	i;
	long	numrel;
	long	fails = 0;
	Elf_Addr loff;
	Elf_Rel *rels;
	struct load_list *llist;

	loff = object->load_offs;
	numrel = object->Dyn.info[relsz] / sizeof(Elf_Rel);
	rels = (Elf_Rel *)(object->Dyn.info[rel]);

	if (rels == NULL)
		return(0);

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

	for (i = 0; i < numrel; i++, rels++) {
		Elf_Addr *where, value, ooff, mask;
		Elf_Word type;
		const Elf_Sym *sym, *this;
		const char *symn;

		type = ELF_R_TYPE(rels->r_info);

		if (reloc_target_flags[type] & _RF_E) {
			_dl_printf(" bad relocation %d %d\n", i, type);
			_dl_exit(1);
		}
		if (type == R_TYPE(NONE))
			continue;

		if (type == R_TYPE(JUMP_SLOT) && rel != DT_JMPREL)
			continue;

		where = (Elf_Addr *)(rels->r_offset + loff);

		if (RELOC_USE_ADDEND(type))
#ifdef LDSO_ARCH_IS_RELA_
			value = rels->r_addend;
#else
			value = *where & RELOC_VALUE_BITMASK(type);
#endif
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
				value += (Elf_Addr)(ooff + this->st_value);
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
 *	This is done by calling _dl_md_reloc on DT_JUMPREL for DL_BIND_NOW,
 *	otherwise the lazy binding plt initialization is performed.
 */
void
_dl_md_reloc_got(elf_object_t *object, int lazy)
{
	Elf_Addr *pltgot = (Elf_Addr *)object->Dyn.info[DT_PLTGOT];
	Elf_Addr ooff;
	const Elf_Sym *this;
	int i, num;
	Elf_Rel *rel;

	if (object->Dyn.info[DT_PLTREL] != DT_REL)
		return;

	object->got_addr = NULL;
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

	object->plt_size = 0;	/* Text PLT on ARM */

	if (object->got_addr == NULL)
		object->got_start = NULL;
	else {
		object->got_start = ELF_TRUNC(object->got_addr, _dl_pagesz);
		object->got_size += object->got_addr - object->got_start;
		object->got_size = ELF_ROUND(object->got_size, _dl_pagesz);
	}
	object->plt_start = NULL;

	if (!lazy) {
		_dl_md_reloc(object, DT_JMPREL, DT_PLTRELSZ);
	} else {
		rel = (Elf_Rel *)(object->Dyn.info[DT_JMPREL]);
		num = (object->Dyn.info[DT_PLTRELSZ]);

		for (i = 0; i < num/sizeof(Elf_Rel); i++, rel++) {
			Elf_Addr *where;
			where = (Elf_Addr *)(rel->r_offset + object->load_offs);
			*where += object->load_offs;
		}

		pltgot[1] = (Elf_Addr)object;
		pltgot[2] = (Elf_Addr)_dl_bind_start;
	}
	if (object->got_size != 0)
		_dl_mprotect((void*)object->got_addr, object->got_size,
		    PROT_READ);
	if (object->plt_size != 0)
		_dl_mprotect((void*)object->plt_start, object->plt_size,
		    PROT_READ|PROT_EXEC);
}

Elf_Addr
_dl_bind(elf_object_t *object, int relidx)
{
	Elf_Rel *rel;
	Elf_Word *addr;
	const Elf_Sym *sym, *this;
	const char *symn;
	Elf_Addr ooff, newval;
	sigset_t omask, nmask;

	rel = ((Elf_Rel *)object->Dyn.info[DT_JMPREL]) + (relidx);

	sym = object->dyn.symtab;
	sym += ELF_R_SYM(rel->r_info);
	symn = object->dyn.strtab + sym->st_name;

	this = NULL;
	ooff = _dl_find_symbol(symn,  &this,
	    SYM_SEARCH_ALL|SYM_WARNNOTFOUND|SYM_PLT, sym,
	    object, NULL);
	if (this == NULL) {
		_dl_printf("lazy binding failed!\n");
		*((int *)0) = 0;	/* XXX */
	}

	addr = (Elf_Addr *)(object->load_offs + rel->r_offset);
	newval = ooff + this->st_value;

	/* if GOT is protected, allow the write */
	if (object->got_size != 0) {
		sigfillset(&nmask);
		_dl_sigprocmask(SIG_BLOCK, &nmask, &omask);
		_dl_thread_bind_lock(0);
		_dl_mprotect((void*)object->got_start, object->got_size,
		    PROT_READ|PROT_WRITE);
	}

	if (*addr != newval)
		*addr = newval;

	/* put the GOT back to RO */
	if (object->got_size != 0) {
		_dl_mprotect((void*)object->got_start, object->got_size,
		    PROT_READ);
		_dl_thread_bind_lock(1);
		_dl_sigprocmask(SIG_SETMASK, &omask, NULL);
	}
	return newval;
}
