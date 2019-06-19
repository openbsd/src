/*	$OpenBSD: rtld_machine.c,v 1.30 2018/11/16 21:15:47 guenther Exp $ */

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
#include <sys/mman.h>
#include <sys/syscall.h>
#include <sys/unistd.h>

#include <nlist.h>
#include <link.h>

#include "syscall.h"
#include "archdep.h"
#include "resolve.h"

int64_t pcookie __attribute__((section(".openbsd.randomdata"))) __dso_hidden;

/*
 * The following table holds for each relocation type:
 *	- the width in bits of the memory location the relocation
 *	  applies to
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
#define _RF_E		0x02000000		/* ERROR */
#define _RF_SZ(s)	(((s) & 0xff) << 8)	/* memory target size */
#define _RF_RS(s)	((s) & 0xff)		/* right shift */
static const int reloc_target_flags[] = {
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
	_RF_E,							/* 16 DTPMOD64*/
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
#define RELOC_USE_ADDEND(t)		((reloc_target_flags[t] & _RF_A) != 0)
#define RELOC_TARGET_SIZE(t)		((reloc_target_flags[t] >> 8) & 0xff)
#define RELOC_VALUE_RIGHTSHIFT(t)	(reloc_target_flags[t] & 0xff)
#define RELOC_ERROR(t)			(reloc_target_flags[t] & _RF_E)

static const Elf_Addr reloc_target_bitmask[] = {
#define _BM(x)  (~(Elf_Addr)0 >> ((8*sizeof(reloc_target_bitmask[0])) - (x)))
	0,			/*  0 NONE */
	_BM(64),		/*  1 _64*/
	_BM(32),		/*  2 PC32 */
	_BM(32),		/*  3 GOT32 */
	_BM(32),		/*  4 PLT32 */
	0,			/*  5 COPY */
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
	0,			/* 16 DTPMOD64*/
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
	long	relrel;
	int	fails = 0;
	Elf_Addr loff;
	Elf_Addr prev_value = 0;
	const Elf_Sym *prev_sym = NULL;
	Elf_RelA *rels;
	struct load_list *llist;

	loff = object->obj_base;
	numrel = object->Dyn.info[relsz] / sizeof(Elf_RelA);
	relrel = rel == DT_RELA ? object->relacount : 0;
	rels = (Elf_RelA *)(object->Dyn.info[rel]);
	if (rels == NULL)
		return(0);

	if (relrel > numrel)
		_dl_die("relacount > numrel: %ld > %ld", relrel, numrel);

	/*
	 * unprotect some segments if we need it.
	 */
	if ((object->dyn.textrel == 1) && (rel == DT_REL || rel == DT_RELA)) {
		for (llist = object->load_list; llist != NULL; llist = llist->next) {
			if (!(llist->prot & PROT_WRITE))
				_dl_mprotect(llist->start, llist->size,
				    PROT_READ | PROT_WRITE);
		}
	}

	/* tight loop for leading RELATIVE relocs */
	for (i = 0; i < relrel; i++, rels++) {
		Elf_Addr *where;

#ifdef DEBUG
		if (ELF_R_TYPE(rels->r_info) != R_TYPE(RELATIVE))
			_dl_die("RELACOUNT wrong");
#endif
		where = (Elf_Addr *)(rels->r_offset + loff);
		*where = rels->r_addend + loff;
	}
	for (; i < numrel; i++, rels++) {
		Elf_Addr *where, value, ooff, mask;
		Elf_Word type;
		const Elf_Sym *sym, *this;
		const char *symn;

		type = ELF_R_TYPE(rels->r_info);

		if (RELOC_ERROR(type))
			_dl_die("relocation error %d idx %ld", type, i);

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
				prev_sym = sym;
				prev_value = (Elf_Addr)(ooff + this->st_value);
				value += prev_value;
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

		if (RELOC_TARGET_SIZE(type) > 32) {
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
	const Elf_Sym *sym, *this;
	const char *symn;
	const elf_object_t *sobj;
	Elf_Addr ooff;
	int64_t cookie = pcookie;
	struct {
		struct __kbind param;
		Elf_Addr newval;
	} buf;

	rel = (Elf_RelA *)(object->Dyn.info[DT_JMPREL]) + index;

	sym = object->dyn.symtab;
	sym += ELF_R_SYM(rel->r_info);
	symn = object->dyn.strtab + sym->st_name;

	this = NULL;
	ooff = _dl_find_symbol(symn, &this,
	    SYM_SEARCH_ALL|SYM_WARNNOTFOUND|SYM_PLT, sym, object, &sobj);
	if (this == NULL)
		_dl_die("lazy binding failed!");

	buf.newval = ooff + this->st_value + rel->r_addend;

	if (__predict_false(sobj->traced) && _dl_trace_plt(sobj, symn))
		return (buf.newval);

	buf.param.kb_addr = (Elf_Word *)(object->obj_base + rel->r_offset);
	buf.param.kb_size = sizeof(Elf_Addr);

	/* directly code the syscall, so that it's actually inline here */
	{
		register long syscall_num __asm("rax") = SYS_kbind;
		register void *arg1 __asm("rdi") = &buf;
		register long  arg2 __asm("rsi") = sizeof(buf);
		register long  arg3 __asm("rdx") = cookie;

		__asm volatile("syscall" : "+r" (syscall_num), "+r" (arg3) :
		    "r" (arg1), "r" (arg2) : "cc", "rcx", "r11", "memory");
	}
	return (buf.newval);
}

int
_dl_md_reloc_got(elf_object_t *object, int lazy)
{
	extern void _dl_bind_start(void);	/* XXX */
	int	fails = 0;
	Elf_Addr *pltgot = (Elf_Addr *)object->Dyn.info[DT_PLTGOT];
	int i, num;
	Elf_RelA *rel;

	if (pltgot == NULL)
		return (0); /* it is possible to have no PLT/GOT relocations */

	if (object->Dyn.info[DT_PLTREL] != DT_RELA)
		return (0);

	if (object->traced)
		lazy = 1;

	if (__predict_false(!lazy)) {
		fails = _dl_md_reloc(object, DT_JMPREL, DT_PLTRELSZ);
	} else {
		pltgot[1] = (Elf_Addr)object;
		pltgot[2] = (Elf_Addr)&_dl_bind_start;

		rel = (Elf_RelA *)(object->Dyn.info[DT_JMPREL]);
		num = (object->Dyn.info[DT_PLTRELSZ]);
		for (i = 0; i < num/sizeof(Elf_RelA); i++, rel++) {
			Elf_Addr *where;
			where = (Elf_Addr *)(rel->r_offset + object->obj_base);
			*where += object->obj_base;
		}
	}

	return (fails);
}
