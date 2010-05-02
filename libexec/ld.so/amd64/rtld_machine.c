/*	$OpenBSD: rtld_machine.c,v 1.15 2010/05/02 04:57:01 guenther Exp $ */

/*
 * Copyright (c) 2002,2004 Dale Rahn
 * Copyright (c) 2001 Niklas Hallqvist
 * Copyright (c) 2001 Artur Grabowski
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
 */
/*-
 * Copyright (c) 2000 Eduardo Horvath.
 * Copyright (c) 1999 The NetBSD Foundation, Inc.
 * All rights reserved.
 *
 * This code is derived from software contributed to The NetBSD Foundation
 * by Paul Kranenburg.
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
 *	This product includes software developed by the NetBSD
 *	Foundation, Inc. and its contributors.
 * 4. Neither the name of The NetBSD Foundation nor the names of its
 *    contributors may be used to endorse or promote products derived
 *    from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE NETBSD FOUNDATION, INC. AND CONTRIBUTORS
 * ``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED
 * TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE FOUNDATION OR CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

#define _DYN_LOADER

#include <sys/types.h>
#include <sys/cdefs.h>
#include <sys/mman.h>

#include <nlist.h>
#include <link.h>
#include <signal.h>

#include "syscall.h"
#include "archdep.h"
#include "resolve.h"

/*
 * The following table holds for each relocation type:
 *	- the width in bits of the memory location the relocation
 *	  applies to (not currently used)
 *	- the number of bits the relocation value must be shifted to the
 *	  right (i.e. discard least significant bits) to fit into
 *	  the appropriate field in the instruction word.
 *	- flags indicating whether
 *		* the relocation involves a symbol
 *		* the relocation is relative to the current position
 *		* the relocation is for a GOT entry
 *		* the relocation is relative to the load address
 *
 */
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
	0,							/*  0 NONE */
	_RF_S|_RF_A|		_RF_SZ(64) | _RF_RS(0),		/*  1 _64*/
	_RF_S|_RF_A|_RF_P|	_RF_SZ(32) | _RF_RS(0),		/*  2 PC32 */
	_RF_G|_RF_A|		_RF_SZ(32) | _RF_RS(0),		/*  3 GOT32 */
	_RF_E|_RF_A|		_RF_SZ(32) | _RF_RS(0),		/*  4 PLT32 */
	_RF_S|			_RF_SZ(32) | _RF_RS(0),		/*  5 COPY */
	_RF_S|			_RF_SZ(64) | _RF_RS(0),		/*  6 GLOB_DAT*/
	_RF_S|			_RF_SZ(64) | _RF_RS(0),		/* 7 JUMP_SLOT*/
	      _RF_A|	_RF_B|	_RF_SZ(64) | _RF_RS(0),		/*  8 RELATIVE*/
	_RF_E,							/*  9 GOTPCREL*/
	_RF_S|_RF_A|		_RF_SZ(32) | _RF_RS(0),		/* 10 32 */
	_RF_S|_RF_A|		_RF_SZ(32) | _RF_RS(0),		/* 11 32S */
	_RF_S|_RF_A|		_RF_SZ(16) | _RF_RS(0),		/* 12 16 */
	_RF_S|_RF_A|_RF_P|	_RF_SZ(16) | _RF_RS(0),		/* 13 PC16 */
	_RF_S|_RF_A|		_RF_SZ(8) | _RF_RS(0),		/* 14 8 */
	_RF_S|_RF_A|_RF_P|	_RF_SZ(8) | _RF_RS(0),		/* 15 PC8 */
	_RF_E,							/* 16 DPTMOD64*/
	_RF_E,							/* 17 DTPOFF64*/
	_RF_E,							/* 18 TPOFF64 */
	_RF_E,							/* 19 TLSGD */
	_RF_E,							/* 20 TLSLD */
	_RF_E,							/* 21 DTPOFF32*/
	_RF_E,							/* 22 GOTTPOFF*/
	_RF_E							/* 23 TPOFF32*/
};

#define RELOC_RESOLVE_SYMBOL(t)		((reloc_target_flags[t] & _RF_S) != 0)
#define RELOC_PC_RELATIVE(t)		((reloc_target_flags[t] & _RF_P) != 0)
#define RELOC_BASE_RELATIVE(t)		((reloc_target_flags[t] & _RF_B) != 0)
#define RELOC_UNALIGNED(t)		((reloc_target_flags[t] & _RF_U) != 0)
#define RELOC_USE_ADDEND(t)		((reloc_target_flags[t] & _RF_A) != 0)
#define RELOC_TARGET_SIZE(t)		((reloc_target_flags[t] >> 8) & 0xff)
#define RELOC_VALUE_RIGHTSHIFT(t)	(reloc_target_flags[t] & 0xff)
#define RELOC_ERROR(t)			(reloc_target_flags[t] & _RF_E)

static long reloc_target_bitmask[] = {
#define _BM(x)  (x == 64? ~0 : ~(-(1UL << (x))))
	0,			/*  0 NONE */
	_BM(64),		/*  1 _64*/
	_BM(32),		/*  2 PC32 */
	_BM(32),		/*  3 GOT32 */
	_BM(32),		/*  4 PLT32 */
	_BM(0),			/*  5 COPY */
	_BM(64),		/*  6 GLOB_DAT*/
	_BM(64),		/*  7 JUMP_SLOT*/
	_BM(64),		/*  8 RELATIVE*/
	_BM(32),		/*  9 GOTPCREL*/
	_BM(32),		/* 10 32 */
	_BM(32),		/* 11 32S */
	_BM(16),		/* 12 16 */
	_BM(16),		/* 13 PC16 */
	_BM(8),			/* 14 8 */
	_BM(8),			/* 15 PC8 */
	0,			/* 16 DPTMOD64*/
	0,			/* 17 DTPOFF64*/
	0,			/* 18 TPOFF64 */
	0,			/* 19 TLSGD */
	0,			/* 20 TLSLD */
	0,			/* 21 DTPOFF32*/
	0,			/* 22 GOTTPOFF*/
	0			/* 23 TPOFF32*/
#undef _BM
};
#define RELOC_VALUE_BITMASK(t)	(reloc_target_bitmask[t])

void _dl_reloc_plt(Elf_Addr *where, Elf_Addr value);

int
_dl_md_reloc(elf_object_t *object, int rel, int relsz)
{
	long	i;
	long	numrel;
	int	fails = 0;
	Elf_Addr loff;
	Elf_RelA *rels;
	struct load_list *llist;

	loff = object->obj_base;
	numrel = object->Dyn.info[relsz] / sizeof(Elf_RelA);
	rels = (Elf_RelA *)(object->Dyn.info[rel]);
	if (rels == NULL)
		return(0);

	/*
	 * unprotect some segments if we need it.
	 */
	if ((object->dyn.textrel == 1) && (rel == DT_REL || rel == DT_RELA)) {
		for (llist = object->load_list; llist != NULL; llist = llist->next) {
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

		if (RELOC_ERROR(type)) {
			_dl_printf("relocation error %d idx %d\n", type, i);
			_dl_exit(20);
		}

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
			} else {
				this = NULL;
				ooff = _dl_find_symbol_bysym(object,
				    ELF_R_SYM(rels->r_info), &this,
				    SYM_SEARCH_ALL|SYM_WARNNOTFOUND|
				    ((type == R_TYPE(JUMP_SLOT))?
					SYM_PLT:SYM_NOTPLT),
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
			_dl_reloc_plt(where, value);
			continue;
		}

		if (type == R_TYPE(COPY)) {
			void *dstaddr = where;
			const void *srcaddr;
			const Elf_Sym *dstsym = sym, *srcsym = NULL;
			Elf_Addr soff;

			soff = _dl_find_symbol(symn, &srcsym,
			    SYM_SEARCH_OTHER|SYM_WARNNOTFOUND|
			    ((type == R_TYPE(JUMP_SLOT)) ? SYM_PLT:SYM_NOTPLT),
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
		} else if (RELOC_TARGET_SIZE(type) > 32) {
			*where &= ~mask;
			*where |= value;
		} else {
			Elf32_Addr *where32 = (Elf32_Addr *)where;

			*where32 &= ~mask;
			*where32 |= value;
		}
	}

	/* reprotect the unprotected segments */
	if ((object->dyn.textrel == 1) && (rel == DT_REL || rel == DT_RELA)) {
		for (llist = object->load_list; llist != NULL; llist = llist->next) {
			if (!(llist->prot & PROT_WRITE))
				_dl_mprotect(llist->start, llist->size,
				    llist->prot);
		}
	}

	return (fails);
}

void
_dl_reloc_plt(Elf_Addr *where, Elf_Addr value)
{
	*where = value;
}

/*
 * Resolve a symbol at run-time.
 */
Elf_Addr
_dl_bind(elf_object_t *object, int index)
{
	Elf_RelA *rel;
	Elf_Word *addr;
	const Elf_Sym *sym, *this;
	const char *symn;
	Elf_Addr ooff, newval;
	sigset_t savedmask;

	rel = (Elf_RelA *)(object->Dyn.info[DT_JMPREL]);

	rel += index;

	sym = object->dyn.symtab;
	sym += ELF_R_SYM(rel->r_info);
	symn = object->dyn.strtab + sym->st_name;

	addr = (Elf_Word *)(object->obj_base + rel->r_offset);
	this = NULL;
	ooff = _dl_find_symbol(symn, &this,
	    SYM_SEARCH_ALL|SYM_WARNNOTFOUND|SYM_PLT, sym,
	    object, NULL);
	if (this == NULL) {
		_dl_printf("lazy binding failed!\n");
		*((int *)0) = 0;		/* XXX */
	}

	newval = ooff + this->st_value + rel->r_addend;

	/* if GOT is protected, allow the write */
	if (object->got_size != 0) {
		_dl_thread_bind_lock(0, &savedmask);
		_dl_mprotect((void*)object->got_start, object->got_size,
		    PROT_READ|PROT_WRITE);
	}

	_dl_reloc_plt((Elf_Addr *)addr, newval);

	/* put the GOT back to RO */
	if (object->got_size != 0) {
		_dl_mprotect((void*)object->got_start, object->got_size,
		    PROT_READ);
		_dl_thread_bind_lock(1, &savedmask);
	}

	return(newval);
}

int
_dl_md_reloc_got(elf_object_t *object, int lazy)
{
	extern void _dl_bind_start(void);	/* XXX */
	int	fails = 0;
	Elf_Addr *pltgot = (Elf_Addr *)object->Dyn.info[DT_PLTGOT];
	int i, num;
	Elf_RelA *rel;
	Elf_Addr ooff;
	const Elf_Sym *this;

	if (pltgot == NULL)
		return (0); /* it is possible to have no PLT/GOT relocations */

	pltgot[1] = (Elf_Addr)object;
	pltgot[2] = (Elf_Addr)&_dl_bind_start;

	if (object->Dyn.info[DT_PLTREL] != DT_RELA)
		return (0);

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

	if (object->got_addr == NULL)
		object->got_start = NULL;
	else {
		object->got_start = ELF_TRUNC(object->got_addr, _dl_pagesz);
		object->got_size += object->got_addr - object->got_start;
		object->got_size = ELF_ROUND(object->got_size, _dl_pagesz);
	}

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

	}

	/* PLT is already RO on i386, no point in mprotecting it, just GOT */
	if (object->got_size != 0)
		_dl_mprotect((void*)object->got_start, object->got_size,
		    PROT_READ);

	return (fails);
}
