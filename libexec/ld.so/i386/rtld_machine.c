/*	$OpenBSD: rtld_machine.c,v 1.50 2022/05/10 20:23:57 kettenis Exp $ */

/*
 * Copyright (c) 2002 Dale Rahn
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
#include <sys/exec_elf.h>
#include <sys/syscall.h>
#include <sys/unistd.h>

#include <machine/reloc.h>

#include "util.h"
#include "resolve.h"

#define nitems(_a)     (sizeof((_a)) / sizeof((_a)[0]))

int64_t pcookie __attribute__((section(".openbsd.randomdata"))) __dso_hidden;

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
#define _RF_E		0x02000000		/* ERROR */
#define _RF_SZ(s)	(((s) & 0xff) << 8)	/* memory target size */
#define _RF_RS(s)	((s) & 0xff)		/* right shift */
static const int reloc_target_flags[] = {
	0,							/* NONE */
	_RF_S|_RF_A|		_RF_SZ(32) | _RF_RS(0),		/* 32 */
	_RF_S|_RF_A|_RF_P|	_RF_SZ(32) | _RF_RS(0),		/* PC32 */
	_RF_G|			_RF_SZ(32) | _RF_RS(00),	/* GOT32 */
	      _RF_A|		_RF_SZ(32) | _RF_RS(0),		/* PLT32 */
	_RF_S|			_RF_SZ(32) | _RF_RS(0),		/* COPY */
	_RF_S|_RF_A|		_RF_SZ(32) | _RF_RS(0),		/* GLOB_DAT */
	_RF_S|			_RF_SZ(32) | _RF_RS(0),		/* JUMP_SLOT */
	      _RF_A|	_RF_B|	_RF_SZ(32) | _RF_RS(0),		/* RELATIVE */
	_RF_E,							/* GOTOFF */
	_RF_E,							/* GOTPC */
	_RF_E,							/* 32PLT */
	_RF_E,							/* DUMMY 12 */
	_RF_E,							/* DUMMY 13 */
	_RF_E,							/* TLS_TPOFF */
	_RF_E,							/* TLS_IE */
	_RF_E,							/* TLS_GITIE */
	_RF_E,							/* TLS_LE */
	_RF_E,							/* TLS_GD */
	_RF_E,							/* TLS_LDM */
	_RF_S|_RF_A|		_RF_SZ(16) | _RF_RS(0),		/* 16 */
	_RF_S|_RF_A|_RF_P|	_RF_SZ(16) | _RF_RS(0),		/* PC16 */
	_RF_S|_RF_A|		_RF_SZ(8) | _RF_RS(0),		/* 8 */
	_RF_S|_RF_A|_RF_P|	_RF_SZ(8) | _RF_RS(0),		/* PC8 */
};

#define RELOC_RESOLVE_SYMBOL(t)		((reloc_target_flags[t] & _RF_S) != 0)
#define RELOC_PC_RELATIVE(t)		((reloc_target_flags[t] & _RF_P) != 0)
#define RELOC_BASE_RELATIVE(t)		((reloc_target_flags[t] & _RF_B) != 0)
#define RELOC_USE_ADDEND(t)		((reloc_target_flags[t] & _RF_A) != 0)
#define RELOC_TARGET_SIZE(t)		((reloc_target_flags[t] >> 8) & 0xff)
#define RELOC_VALUE_RIGHTSHIFT(t)	(reloc_target_flags[t] & 0xff)
#define RELOC_ERROR(t) \
	((t) >= nitems(reloc_target_flags) || (reloc_target_flags[t] & _RF_E))

static const long reloc_target_bitmask[] = {
#define _BM(x)	(~(-(1ULL << (x))))
	0,		/* NONE */
	_BM(32),	/* RELOC_32*/
	_BM(32),	/* PC32 */
	_BM(32),	/* GOT32 */
	_BM(32),	/* PLT32 */
	0,		/* COPY */
	_BM(32),	/* GLOB_DAT */
	_BM(32),	/* JUMP_SLOT */
	_BM(32),	/* RELATIVE */
	0,		/* GOTOFF XXX */
	0,		/* GOTPC XXX */
	0,		/* DUMMY 11 */
	0,		/* DUMMY 12 */
	0,		/* DUMMY 13 */
	0,		/* DUMMY 14 */
	0,		/* DUMMY 15 */
	0,		/* DUMMY 16 */
	0,		/* DUMMY 17 */
	0,		/* DUMMY 18 */
	0,		/* DUMMY 19 */
	_BM(16),	/* RELOC_16 */
	_BM(8),		/* PC_16 */
	_BM(8),		/* RELOC_8 */
	_BM(8),		/* RELOC_PC8 */
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
	Elf_Rel *rels;

	loff = object->obj_base;
	numrel = object->Dyn.info[relsz] / sizeof(Elf_Rel);
	relrel = rel == DT_REL ? object->relcount : 0;
	rels = (Elf_Rel *)(object->Dyn.info[rel]);
	if (rels == NULL)
		return 0;

	if (relrel > numrel)
		_dl_die("relcount > numrel: %ld > %ld", relrel, numrel);

	/* tight loop for leading RELATIVE relocs */
	for (i = 0; i < relrel; i++, rels++) {
		Elf_Addr *where;

		where = (Elf_Addr *)(rels->r_offset + loff);
		*where += loff;
	}
	for (; i < numrel; i++, rels++) {
		Elf_Addr *where, value, mask;
		Elf_Word type;
		const Elf_Sym *sym;
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
			value = *where & RELOC_VALUE_BITMASK(type);
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
				struct sym_res sr;

				sr = _dl_find_symbol(symn,
				    SYM_SEARCH_ALL|SYM_WARNNOTFOUND|
				    ((type == R_TYPE(JUMP_SLOT))?
					SYM_PLT:SYM_NOTPLT), sym, object);
				if (sr.sym == NULL) {
resolve_failed:
					if (ELF_ST_BIND(sym->st_info) !=
					    STB_WEAK)
						fails++;
					continue;
				}
				prev_sym = sym;
				prev_value = (Elf_Addr)(sr.obj->obj_base +
				    sr.sym->st_value);
				value += prev_value;
			}
		}

		if (type == R_TYPE(JUMP_SLOT)) {
			_dl_reloc_plt((Elf_Word *)where, value);
			continue;
		}

		if (type == R_TYPE(COPY)) {
			void *dstaddr = where;
			const void *srcaddr;
			const Elf_Sym *dstsym = sym;
			struct sym_res sr;

			sr = _dl_find_symbol(symn,
			    SYM_SEARCH_OTHER|SYM_WARNNOTFOUND|SYM_NOTPLT,
			    dstsym, object);
			if (sr.sym == NULL)
				goto resolve_failed;

			srcaddr = (void *)(sr.obj->obj_base + sr.sym->st_value);
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

		*where &= ~mask;
		*where |= value;
	}

	return fails;
}

#if 0
struct jmpslot {
	u_short opcode;
	u_short addr[2];
	u_short reloc_index;
#define JMPSLOT_RELOC_MASK	0xffff
};
#define JUMP			0xe990	/* NOP + JMP opcode */
#endif

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
	Elf_Rel *rel;
	const Elf_Sym *sym;
	const char *symn;
	struct sym_res sr;
	uint64_t cookie = pcookie;
	struct {
		struct __kbind param;
		Elf_Addr newval;
	} buf;

	rel = (Elf_Rel *)(object->Dyn.info[DT_JMPREL]);

	rel += index/sizeof(Elf_Rel);

	sym = object->dyn.symtab;
	sym += ELF_R_SYM(rel->r_info);
	symn = object->dyn.strtab + sym->st_name;

	sr = _dl_find_symbol(symn, SYM_SEARCH_ALL|SYM_WARNNOTFOUND|SYM_PLT,
	    sym, object);
	if (sr.sym == NULL)
		_dl_die("lazy binding failed!");

	buf.newval = sr.obj->obj_base + sr.sym->st_value;

	if (__predict_false(sr.obj->traced) && _dl_trace_plt(sr.obj, symn))
		return buf.newval;

	buf.param.kb_addr = (Elf_Word *)(object->obj_base + rel->r_offset);
	buf.param.kb_size = sizeof(Elf_Addr);

	/* directly code the syscall, so that it's actually inline here */
	{
		register long syscall_num __asm("eax") = SYS_kbind;

		__asm volatile("lea %3, %%edx; pushl 4(%%edx);"
		    " pushl (%%edx); pushl %2; pushl %1;"
		    " push %%eax; int $0x80; addl $20, %%esp" :
		    "+a" (syscall_num) : "r" (&buf), "i" (sizeof(buf)),
		    "m" (cookie) : "edx", "cc", "memory");
	}

	return buf.newval;
}

int
_dl_md_reloc_got(elf_object_t *object, int lazy)
{
	extern void _dl_bind_start(void);	/* XXX */
	int	fails = 0;
	Elf_Addr *pltgot = (Elf_Addr *)object->Dyn.info[DT_PLTGOT];
	int i, num;
	Elf_Rel *rel;

	if (pltgot == NULL)
		return 0; /* it is possible to have no PLT/GOT relocations */

	if (object->Dyn.info[DT_PLTREL] != DT_REL)
		return 0;

	if (!lazy) {
		fails = _dl_md_reloc(object, DT_JMPREL, DT_PLTRELSZ);
	} else {
		pltgot[1] = (Elf_Addr)object;
		pltgot[2] = (Elf_Addr)&_dl_bind_start;

		rel = (Elf_Rel *)(object->Dyn.info[DT_JMPREL]);
		num = (object->Dyn.info[DT_PLTRELSZ]);
		for (i = 0; i < num/sizeof(Elf_Rel); i++, rel++) {
			Elf_Addr *where;
			where = (Elf_Addr *)(rel->r_offset + object->obj_base);
			*where += object->obj_base;
		}
	}

	return fails;
}
