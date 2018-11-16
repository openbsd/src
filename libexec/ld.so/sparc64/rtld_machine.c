/*	$OpenBSD: rtld_machine.c,v 1.63 2018/11/16 21:15:47 guenther Exp $ */

/*
 * Copyright (c) 1999 Dale Rahn
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
#include <machine/trap.h>

#include <nlist.h>
#include <link.h>

#include "syscall.h"
#include "archdep.h"
#include "resolve.h"

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
#define _RF_U		0x04000000		/* Unaligned */
#define _RF_SZ(s)	(((s) & 0xff) << 8)	/* memory target size */
#define _RF_RS(s)	((s) & 0xff)		/* right shift */
static const int reloc_target_flags[] = {
	0,							/* NONE */
	_RF_S|_RF_A|		_RF_SZ(8)  | _RF_RS(0),		/* RELOC_8 */
	_RF_S|_RF_A|		_RF_SZ(16) | _RF_RS(0),		/* RELOC_16 */
	_RF_S|_RF_A|		_RF_SZ(32) | _RF_RS(0),		/* RELOC_32 */
	_RF_S|_RF_A|_RF_P|	_RF_SZ(8)  | _RF_RS(0),		/* DISP_8 */
	_RF_S|_RF_A|_RF_P|	_RF_SZ(16) | _RF_RS(0),		/* DISP_16 */
	_RF_S|_RF_A|_RF_P|	_RF_SZ(32) | _RF_RS(0),		/* DISP_32 */
	_RF_S|_RF_A|_RF_P|	_RF_SZ(32) | _RF_RS(2),		/* WDISP_30 */
	_RF_S|_RF_A|_RF_P|	_RF_SZ(32) | _RF_RS(2),		/* WDISP_22 */
	_RF_S|_RF_A|		_RF_SZ(32) | _RF_RS(10),	/* HI22 */
	_RF_S|_RF_A|		_RF_SZ(32) | _RF_RS(0),		/* 22 */
	_RF_S|_RF_A|		_RF_SZ(32) | _RF_RS(0),		/* 13 */
	_RF_S|_RF_A|		_RF_SZ(32) | _RF_RS(0),		/* LO10 */
	_RF_G|			_RF_SZ(32) | _RF_RS(0),		/* GOT10 */
	_RF_G|			_RF_SZ(32) | _RF_RS(0),		/* GOT13 */
	_RF_G|			_RF_SZ(32) | _RF_RS(10),	/* GOT22 */
	_RF_S|_RF_A|_RF_P|	_RF_SZ(32) | _RF_RS(0),		/* PC10 */
	_RF_S|_RF_A|_RF_P|	_RF_SZ(32) | _RF_RS(10),	/* PC22 */
	      _RF_A|_RF_P|	_RF_SZ(32) | _RF_RS(2),		/* WPLT30 */
	_RF_S|			_RF_SZ(32) | _RF_RS(0),		/* COPY */
	_RF_S|_RF_A|		_RF_SZ(64) | _RF_RS(0),		/* GLOB_DAT */
	_RF_S|			_RF_SZ(32) | _RF_RS(0),		/* JMP_SLOT */
	      _RF_A|	_RF_B|	_RF_SZ(64) | _RF_RS(0),		/* RELATIVE */
	_RF_S|_RF_A|	_RF_U|	_RF_SZ(32) | _RF_RS(0),		/* UA_32 */

	      _RF_A|		_RF_SZ(32) | _RF_RS(0),		/* PLT32 */
	      _RF_A|		_RF_SZ(32) | _RF_RS(10),	/* HIPLT22 */
	      _RF_A|		_RF_SZ(32) | _RF_RS(0),		/* LOPLT10 */
	      _RF_A|_RF_P|	_RF_SZ(32) | _RF_RS(0),		/* PCPLT32 */
	      _RF_A|_RF_P|	_RF_SZ(32) | _RF_RS(10),	/* PCPLT22 */
	      _RF_A|_RF_P|	_RF_SZ(32) | _RF_RS(0),		/* PCPLT10 */
	_RF_S|_RF_A|		_RF_SZ(32) | _RF_RS(0),		/* 10 */
	_RF_S|_RF_A|		_RF_SZ(32) | _RF_RS(0),		/* 11 */
	_RF_S|_RF_A|		_RF_SZ(64) | _RF_RS(0),		/* 64 */
	_RF_S|_RF_A|/*extra*/	_RF_SZ(32) | _RF_RS(0),		/* OLO10 */
	_RF_S|_RF_A|		_RF_SZ(32) | _RF_RS(42),	/* HH22 */
	_RF_S|_RF_A|		_RF_SZ(32) | _RF_RS(32),	/* HM10 */
	_RF_S|_RF_A|		_RF_SZ(32) | _RF_RS(10),	/* LM22 */
	_RF_S|_RF_A|_RF_P|	_RF_SZ(32) | _RF_RS(42),	/* PC_HH22 */
	_RF_S|_RF_A|_RF_P|	_RF_SZ(32) | _RF_RS(32),	/* PC_HM10 */
	_RF_S|_RF_A|_RF_P|	_RF_SZ(32) | _RF_RS(10),	/* PC_LM22 */
	_RF_S|_RF_A|_RF_P|	_RF_SZ(32) | _RF_RS(2),		/* WDISP16 */
	_RF_S|_RF_A|_RF_P|	_RF_SZ(32) | _RF_RS(2),		/* WDISP19 */
	_RF_S|_RF_A|		_RF_SZ(32) | _RF_RS(0),		/* GLOB_JMP */
	_RF_S|_RF_A|		_RF_SZ(32) | _RF_RS(0),		/* 7 */
	_RF_S|_RF_A|		_RF_SZ(32) | _RF_RS(0),		/* 5 */
	_RF_S|_RF_A|		_RF_SZ(32) | _RF_RS(0),		/* 6 */
	_RF_S|_RF_A|_RF_P|	_RF_SZ(64) | _RF_RS(0),		/* DISP64 */
	      _RF_A|		_RF_SZ(64) | _RF_RS(0),		/* PLT64 */
	_RF_S|_RF_A|		_RF_SZ(32) | _RF_RS(10),	/* HIX22 */
	_RF_S|_RF_A|		_RF_SZ(32) | _RF_RS(0),		/* LOX10 */
	_RF_S|_RF_A|		_RF_SZ(32) | _RF_RS(22),	/* H44 */
	_RF_S|_RF_A|		_RF_SZ(32) | _RF_RS(12),	/* M44 */
	_RF_S|_RF_A|		_RF_SZ(32) | _RF_RS(0),		/* L44 */
	_RF_S|_RF_A|		_RF_SZ(64) | _RF_RS(0),		/* REGISTER */
	_RF_S|_RF_A|	_RF_U|	_RF_SZ(64) | _RF_RS(0),		/* UA64 */
	_RF_S|_RF_A|	_RF_U|	_RF_SZ(16) | _RF_RS(0),		/* UA16 */
};

#define RELOC_RESOLVE_SYMBOL(t)		((reloc_target_flags[t] & _RF_S) != 0)
#define RELOC_PC_RELATIVE(t)		((reloc_target_flags[t] & _RF_P) != 0)
#define RELOC_BASE_RELATIVE(t)		((reloc_target_flags[t] & _RF_B) != 0)
#define RELOC_UNALIGNED(t)		((reloc_target_flags[t] & _RF_U) != 0)
#define RELOC_USE_ADDEND(t)		((reloc_target_flags[t] & _RF_A) != 0)
#define RELOC_TARGET_SIZE(t)		((reloc_target_flags[t] >> 8) & 0xff)
#define RELOC_VALUE_RIGHTSHIFT(t)	(reloc_target_flags[t] & 0xff)

static const long reloc_target_bitmask[] = {
#define _BM(x)	(~(-(1ULL << (x))))
	0,				/* NONE */
	_BM(8), _BM(16), _BM(32),	/* RELOC_8, _16, _32 */
	_BM(8), _BM(16), _BM(32),	/* DISP8, DISP16, DISP32 */
	_BM(30), _BM(22),		/* WDISP30, WDISP22 */
	_BM(22), _BM(22),		/* HI22, _22 */
	_BM(13), _BM(10),		/* RELOC_13, _LO10 */
	_BM(10), _BM(13), _BM(22),	/* GOT10, GOT13, GOT22 */
	_BM(10), _BM(22),		/* _PC10, _PC22 */
	_BM(30), 0,			/* _WPLT30, _COPY */
	-1, _BM(32), -1,		/* _GLOB_DAT, JMP_SLOT, _RELATIVE */
	_BM(32), _BM(32),		/* _UA32, PLT32 */
	_BM(22), _BM(10),		/* _HIPLT22, LOPLT10 */
	_BM(32), _BM(22), _BM(10),	/* _PCPLT32, _PCPLT22, _PCPLT10 */
	_BM(10), _BM(11), -1,		/* _10, _11, _64 */
	_BM(10), _BM(22),		/* _OLO10, _HH22 */
	_BM(10), _BM(22),		/* _HM10, _LM22 */
	_BM(22), _BM(10), _BM(22),	/* _PC_HH22, _PC_HM10, _PC_LM22 */
	_BM(16), _BM(19),		/* _WDISP16, _WDISP19 */
	-1,				/* GLOB_JMP */
	_BM(7), _BM(5), _BM(6)		/* _7, _5, _6 */
	-1, -1,				/* DISP64, PLT64 */
	_BM(22), _BM(13),		/* HIX22, LOX10 */
	_BM(22), _BM(10), _BM(13),	/* H44, M44, L44 */
	-1, -1, _BM(16),		/* REGISTER, UA64, UA16 */
#undef _BM
};
#define RELOC_VALUE_BITMASK(t)	(reloc_target_bitmask[t])

int _dl_reloc_plt(Elf_Word *where1, Elf_Word *where2, Elf_Word *pltaddr,
	Elf_Addr value);
void _dl_install_plt(Elf_Word *pltgot, Elf_Addr proc);

int
_dl_md_reloc(elf_object_t *object, int rel, int relasz)
{
	long	i;
	long	numrela;
	long	relrel;
	int	fails = 0;
	Elf_Addr loff;
	Elf_Addr prev_value = 0;
	const Elf_Sym *prev_sym = NULL;
	Elf_RelA *relas;
	struct load_list *llist;

	loff = object->obj_base;
	numrela = object->Dyn.info[relasz] / sizeof(Elf64_Rela);
	relrel = rel == DT_RELA ? object->relacount : 0;
	relas = (Elf64_Rela *)(object->Dyn.info[rel]);

	if (relas == NULL)
		return(0);

	if (relrel > numrela)
		_dl_die("relacount > numrel: %ld > %ld", relrel, numrela);

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
	for (i = 0; i < relrel; i++, relas++) {
		Elf_Addr *where;

#ifdef DEBUG
		if (ELF_R_TYPE(relas->r_info) != R_TYPE(RELATIVE))
			_dl_die("RELACOUNT wrong");
#endif
		where = (Elf_Addr *)(relas->r_offset + loff);
		*where = relas->r_addend + loff;
	}
	for (; i < numrela; i++, relas++) {
		Elf_Addr *where, value, ooff, mask;
		Elf_Word type;
		const Elf_Sym *sym, *this;
		const char *symn;

		type = ELF_R_TYPE(relas->r_info);

		if (type == R_TYPE(NONE) || type == R_TYPE(JMP_SLOT))
			continue;

		where = (Elf_Addr *)(relas->r_offset + loff);

		if (RELOC_USE_ADDEND(type))
			value = relas->r_addend;
		else
			value = 0;

		sym = NULL;
		symn = NULL;
		if (RELOC_RESOLVE_SYMBOL(type)) {
			sym = object->dyn.symtab;
			sym += ELF_R_SYM(relas->r_info);
			symn = object->dyn.strtab + sym->st_name;

			if (sym->st_shndx != SHN_UNDEF &&
			    ELF_ST_BIND(sym->st_info) == STB_LOCAL) {
				value += loff;
			} else if (sym == prev_sym) {
				value += prev_value;
			} else {
				this = NULL;
				ooff = _dl_find_symbol_bysym(object,
				    ELF_R_SYM(relas->r_info), &this,
				    SYM_SEARCH_ALL|SYM_WARNNOTFOUND|
				    ((type == R_TYPE(JMP_SLOT)) ?
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

		if (type == R_TYPE(COPY)) {
			void *dstaddr = where;
			const void *srcaddr;
			const Elf_Sym *dstsym = sym, *srcsym = NULL;
			size_t size = dstsym->st_size;
			Elf_Addr soff;

			soff = _dl_find_symbol(symn, &srcsym,
			    SYM_SEARCH_OTHER|SYM_WARNNOTFOUND|SYM_NOTPLT,
			    dstsym, object, NULL);
			if (srcsym == NULL)
				goto resolve_failed;

			srcaddr = (void *)(soff + srcsym->st_value);
			_dl_bcopy(srcaddr, dstaddr, size);
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

/*
 * Instruction templates:
 */

#define	BAA	0x30680000	/*	ba,a	%xcc, 0 */
#define	SETHI	0x03000000	/*	sethi	%hi(0), %g1 */
#define	JMP	0x81c06000	/*	jmpl	%g1+%lo(0), %g0	  <-- simm13 */
#define	NOP	0x01000000	/*	sethi	%hi(0), %g0 */
#define	OR	0x82106000	/*	or	%g1, 0, %g1 */
#define	ORG5	0x8a116000	/*	or	%g5, 0, %g5 */
#define	XOR	0x82186000	/*	xor	%g1, 0, %g1 */
#define	MOV71	0x8210000f	/*	or	%o7, 0, %g1 */
#define	MOV17	0x9e100001	/*	or	%g1, 0, %o7 */
#define	CALL	0x40000000	/*	call	0	  <-- disp30 */
#define	SLLX	0x83287000	/*	sllx	%g1, 0, %g1 */
#define	SLLXG5	0x8b297000	/*	sllx	%g5, 0, %g5 */
#define	SRAX	0x83387000	/*	srax	%g1, 0, %g1 */
#define	SETHIG5	0x0b000000	/*	sethi	%hi(0), %g5 */
#define	ORG15	0x82804005	/*	or	%g1, %g5, %g1 */


/* %hi(v) with variable shift */
#define	HIVAL(v, s)	(((v) >> (s)) &  0x003fffff)
#define LOVAL(v)	((v) & 0x000003ff)

int
_dl_reloc_plt(Elf_Word *where1, Elf_Word *where2, Elf_Word *pltaddr,
    Elf_Addr value)
{
	Elf_Addr offset;

	/*
	 * At the PLT entry pointed at by `where', we now construct
	 * a direct transfer to the now fully resolved function
	 * address.
	 *
	 * A PLT entry is supposed to start by looking like this:
	 *
	 *	sethi	%hi(. - .PLT0), %g1
	 *	ba,a,pt	%xcc, .PLT1
	 *	nop
	 *	nop
	 *	nop
	 *	nop
	 *	nop
	 *	nop
	 *
	 * When we replace these entries we either (a) only replace
	 * the second word (the ba,a,pt), or (b) replace multiple
	 * words: one or more nops, then finally the ba,a,pt.  By
	 * replacing the ba,a,pt last, we guarantee that the PLT can
	 * be used by other threads even while it's being updated.
	 * This is made slightly more complicated by kbind, for which
	 * we need to pass them to the kernel in the order they get
	 * written.  To that end, we store the word to overwrite the
	 * ba,a,pt at *where1, and the words to overwrite the nops at
	 * where2[0], where2[1], ...
	 *
	 * We now need to find out how far we need to jump.  We
	 * have a choice of several different relocation techniques
	 * which are increasingly expensive.
	 */

	offset = value - ((Elf_Addr)pltaddr);
	if ((int64_t)(offset-4) <= (1L<<20) &&
	    (int64_t)(offset-4) >= -(1L<<20)) {
		/*
		 * We're within 1MB -- we can use a direct branch insn.
		 *
		 * We can generate this pattern:
		 *
		 *	sethi	%hi(. - .PLT0), %g1
		 *	ba,a,pt	%xcc, addr
		 *	nop
		 *	nop
		 *	nop
		 *	nop
		 *	nop
		 *	nop
		 *
		 */
		*where1 = BAA | (((offset-4) >> 2) &0x7ffff);
		return (0);
	} else if (value < (1UL<<32)) {
		/*
		 * We're within 32-bits of address zero.
		 *
		 * The resulting code in the jump slot is:
		 *
		 *	sethi	%hi(. - .PLT0), %g1
		 *	sethi	%hi(addr), %g1
		 *	jmp	%g1+%lo(addr)
		 *	nop
		 *	nop
		 *	nop
		 *	nop
		 *	nop
		 *
		 */
		*where1 = SETHI | HIVAL(value, 10);
		where2[0] = JMP   | LOVAL(value);
		return (1);
	} else if (value > -(1UL<<32)) {
		/*
		 * We're within 32-bits of address -1.
		 *
		 * The resulting code in the jump slot is:
		 *
		 *	sethi	%hi(. - .PLT0), %g1
		 *	sethi	%hix(~addr), %g1
		 *	xor	%g1, %lox(~addr), %g1
		 *	jmp	%g1
		 *	nop
		 *	nop
		 *	nop
		 *	nop
		 *
		 */
		*where1 = SETHI | HIVAL(~value, 10);
		where2[0] = XOR | ((~value) & 0x00001fff);
		where2[1] = JMP;
		return (2);
	} else if ((int64_t)(offset-8) <= (1L<<31) &&
	    (int64_t)(offset-8) >= -((1L<<31) - 4)) {
		/*
		 * We're within 32-bits -- we can use a direct call insn
		 *
		 * The resulting code in the jump slot is:
		 *
		 *	sethi	%hi(. - .PLT0), %g1
		 *	mov	%o7, %g1
		 *	call	(.+offset)
		 *	 mov	%g1, %o7
		 *	nop
		 *	nop
		 *	nop
		 *	nop
		 *
		 */
		*where1 = MOV71;
		where2[0] = CALL | (((offset-8) >> 2) & 0x3fffffff);
		where2[1] = MOV17;
		return (2);
	} else if (value < (1L<<42)) {
		/*
		 * Target 42bits or smaller.
		 * We can generate this pattern:
		 *
		 * The resulting code in the jump slot is:
		 *
		 *	sethi	%hi(. - .PLT0), %g1
		 *	sethi	%hi(addr >> 20), %g1
		 *	or	%g1, %lo(addr >> 10), %g1
		 *	sllx	%g1, 10, %g1
		 *	jmp	%g1+%lo(addr)
		 *	nop
		 *	nop
		 *	nop
		 *
		 * this can handle addresses 0 - 0x3fffffffffc
		 */
		*where1 = SETHI | HIVAL(value, 20);
		where2[0] = OR    | LOVAL(value >> 10);
		where2[1] = SLLX  | 10;
		where2[2] = JMP   | LOVAL(value);
		return (3);
	} else if (value > -(1UL<<41)) {
		/*
		 * Large target >= 0xfffffe0000000000UL
		 * We can generate this pattern:
		 *
		 * The resulting code in the jump slot is:
		 *
		 *	sethi	%hi(. - .PLT0), %g1
		 *	sethi	%hi(addr >> 20), %g1
		 *	or	%g1, %lo(addr >> 10), %g1
		 *	sllx	%g1, 32, %g1
		 *	srax	%g1, 22, %g1
		 *	jmp	%g1+%lo(addr)
		 *	nop
		 *	nop
		 *	nop
		 *
		 */
		*where1 = SETHI | HIVAL(value, 20);
		where2[0] = OR   | LOVAL(value >> 10);
		where2[1] = SLLX  | 32;
		where2[2] = SRAX  | 22;
		where2[3] = JMP   | LOVAL(value);
		return (4);
	} else {
		/*
		 * We need to load all 64-bits
		 *
		 * The resulting code in the jump slot is:
		 *
		 *	sethi	%hi(. - .PLT0), %g1
		 *	sethi	%hi(addr >> 42), %g5
		 *	sethi	%hi(addr >> 10), %g1
		 *	or	%g1, %lo(addr >> 32), %g5
		 *	sllx	%g5, 32, %g5
		 *	or	%g1, %g5, %g1
		 *	jmp	%g1+%lo(addr)
		 *	nop
		 *
		 */
		*where1 = SETHIG5 | HIVAL(value, 42);
		where2[0] = SETHI | HIVAL(value, 10);
		where2[1] = ORG5 | LOVAL(value >> 32);
		where2[2] = SLLXG5 | 32;
		where2[3] = ORG15;
		where2[4] = JMP | LOVAL(value);
		return (5);
	}
}

/*
 * Resolve a symbol at run-time.
 */
Elf_Addr
_dl_bind(elf_object_t *object, int index)
{
	Elf_RelA *rela;
	Elf_Word *addr;
	Elf_Addr ooff, newvalue;
	const Elf_Sym *sym, *this;
	const char *symn;
	const elf_object_t *sobj;
	int64_t cookie = pcookie;
	struct {
		struct __kbind param[2];
		Elf_Word newval[6];
	} buf;
	struct __kbind *param;
	size_t psize;
	int i;

	rela = (Elf_RelA *)(object->Dyn.info[DT_JMPREL]);
	if (ELF_R_TYPE(rela->r_info) == R_TYPE(JMP_SLOT)) {
		/*
		 * XXXX
		 *
		 * The first four PLT entries are reserved.  There
		 * is some disagreement whether they should have
		 * associated relocation entries.  Both the SPARC
		 * 32-bit and 64-bit ELF specifications say that
		 * they should have relocation entries, but the
		 * 32-bit SPARC binutils do not generate them,
		 * and now the 64-bit SPARC binutils have stopped
		 * generating them too.
		 *
		 * So, to provide binary compatibility, we will
		 * check the first entry, if it is reserved it
		 * should not be of the type JMP_SLOT.  If it
		 * is JMP_SLOT, then the 4 reserved entries were
		 * not generated and our index is 4 entries too far.
		 */
		rela += index - 4;
	} else
		rela += index;

	sym = object->dyn.symtab;
	sym += ELF64_R_SYM(rela->r_info);
	symn = object->dyn.strtab + sym->st_name;

	this = NULL;
	ooff = _dl_find_symbol(symn, &this,
	    SYM_SEARCH_ALL|SYM_WARNNOTFOUND|SYM_PLT, sym, object, &sobj);
	if (this == NULL)
		_dl_die("lazy binding failed!");

	newvalue = ooff + this->st_value;

	if (__predict_false(sobj->traced) && _dl_trace_plt(sobj, symn))
		return (newvalue);

	/*
	 * While some relocations just need to write one word and
	 * can do that with kbind() with just one block, many
	 * require two blocks to be written: all but first word,
	 * then the first word.  So, if we want to write 5 words
	 * in total, then the layout of the buffer we pass to
	 * kbind() needs to be one of these:
	 *   +------------+
	 *   | kbind.addr |
	 *   |     """    |
	 *   | kbind.size |
	 *   |     """    |		+------------+
	 *   | kbind.addr |		| kbind.addr |
	 *   |     """    |		|     """    |
	 *   | kbind.size |		| kbind.size |
	 *   |     """    |		|     """    |
	 *   |   word 2   |		|    word    |
	 *   |   word 3   |		+------------+
	 *   |   word 4   |
	 *   |   word 5   |
	 *   |   word 1   |
	 *   +------------+
	 *
	 * We first handle the special case of relocations with a
	 * non-zero r_addend, which have one block to update whose
	 * address is the relocation address itself.  This is only
	 * used for PLT entries after the 2^15th, i.e., truly monstrous
	 * programs, thus the __predict_false().
	 */
	addr = (Elf_Word *)(object->obj_base + rela->r_offset);
	_dl_memset(&buf, 0, sizeof(buf));
	if (__predict_false(rela->r_addend)) {
		/*
		 * This entry is >32768.  The relocation points to a
		 * PC-relative pointer to the _dl_bind_start_0 stub at
		 * the top of the PLT section.  Update it to point to
		 * the target function.
		 */
		buf.newval[0] = rela->r_addend + newvalue
		    - object->Dyn.info[DT_PLTGOT];
		buf.param[1].kb_addr = addr;
		buf.param[1].kb_size = sizeof(buf.newval[0]);
		param = &buf.param[1];
		psize = sizeof(struct __kbind) + sizeof(buf.newval[0]);
	} else {
		Elf_Word first;

		/*
		 * For the other relocations, the word at the relocation
		 * address will be left unchanged.  Assume _dl_reloc_plt()
		 * will tell us to update multiple words, so save the first
		 * word to the side.
		 */
		i = _dl_reloc_plt(&first, &buf.newval[0], addr, newvalue);

		/*
		 * _dl_reloc_plt() returns the number of words that must be
		 * written after the first word in location, but before it
		 * in time.  If it returns zero, then only a single block
		 * with one word is needed, so we just put it in place per
		 * the right-hand diagram and just use param[1] and newval[0]
		 */
		if (i == 0) {
			/* fill in the __kbind structure */
			buf.param[1].kb_addr = &addr[1];
			buf.param[1].kb_size = sizeof(Elf_Word);
			buf.newval[0] = first;
			param = &buf.param[1];
			psize = sizeof(struct __kbind) + sizeof(buf.newval[0]);
		} else {
			/*
			 * Two blocks are necessary.  Save the first word
			 * after the other words.
			 */
			buf.param[0].kb_addr = &addr[2];
			buf.param[0].kb_size = i * sizeof(Elf_Word);
			buf.param[1].kb_addr = &addr[1];
			buf.param[1].kb_size = sizeof(Elf_Word);
			buf.newval[i] = first;
			param = &buf.param[0];
			psize = 2 * sizeof(struct __kbind) +
			    (i + 1) * sizeof(buf.newval[0]);
		}
	}

	/* directly code the syscall, so that it's actually inline here */
	{
		register long syscall_num __asm("g1") = SYS_kbind;
		register void *arg1 __asm("o0") = param;
		register long  arg2 __asm("o1") = psize;
		register long  arg3 __asm("o2") = cookie;

		__asm volatile("t %2" : "+r" (arg1), "+r" (arg2)
		    : "i" (ST_SYSCALL), "r" (syscall_num), "r" (arg3)
		    : "cc", "memory");
	}

	return (newvalue);
}

/*
 * Install rtld function call into this PLT slot.
 */
#define SAVE		0x9de3bf50
#define SETHI_l0	0x21000000
#define SETHI_l1	0x23000000
#define OR_l0_l0	0xa0142000
#define SLLX_l0_32_l0	0xa12c3020
#define OR_l0_l1_l0	0xa0140011
#define JMPL_l0_o1	0x93c42000
#define MOV_g1_o0	0x90100001

void
_dl_install_plt(Elf_Word *pltgot, Elf_Addr proc)
{
	pltgot[0] = SAVE;
	pltgot[1] = SETHI_l0  | HIVAL(proc, 42);
	pltgot[2] = SETHI_l1  | HIVAL(proc, 10);
	pltgot[3] = OR_l0_l0  | LOVAL((proc) >> 32);
	pltgot[4] = SLLX_l0_32_l0;
	pltgot[5] = OR_l0_l1_l0;
	pltgot[6] = JMPL_l0_o1 | LOVAL(proc);
	pltgot[7] = MOV_g1_o0;
}

void _dl_bind_start_0(long, long);
void _dl_bind_start_1(long, long);

static int
_dl_md_reloc_all_plt(elf_object_t *object)
{
	long	i;
	long	numrela;
	int	fails = 0;
	Elf_Addr loff;
	Elf_RelA *relas;

	loff = object->obj_base;
	numrela = object->Dyn.info[DT_PLTRELSZ] / sizeof(Elf64_Rela);
	relas = (Elf64_Rela *)(object->Dyn.info[DT_JMPREL]);

	if (relas == NULL)
		return(0);

	for (i = 0; i < numrela; i++, relas++) {
		Elf_Addr value;
		Elf_Word *where;
		const Elf_Sym *sym, *this;

		if (ELF_R_TYPE(relas->r_info) != R_TYPE(JMP_SLOT))
			continue;

		sym = object->dyn.symtab + ELF_R_SYM(relas->r_info);

		this = NULL;
		value = _dl_find_symbol_bysym(object, ELF_R_SYM(relas->r_info),
		    &this, SYM_SEARCH_ALL|SYM_WARNNOTFOUND|SYM_PLT, sym, NULL);
		if (this == NULL) {
			if (ELF_ST_BIND(sym->st_info) != STB_WEAK)
				fails++;
			continue;
		}

		where = (Elf_Word *)(relas->r_offset + loff);
		value += this->st_value;

		if (__predict_false(relas->r_addend)) {
			/*
			 * This entry is >32768.  The relocation points to a
			 * PC-relative pointer to the _dl_bind_start_0 stub at
			 * the top of the PLT section.  Update it to point to
			 * the target function.
			 */
			*(Elf_Addr *)where = relas->r_addend + value -
			    object->Dyn.info[DT_PLTGOT];
		} else
			_dl_reloc_plt(&where[1], &where[2], where, value);
	}

	return (fails);
}

/*
 *	Relocate the Global Offset Table (GOT).
 */
int
_dl_md_reloc_got(elf_object_t *object, int lazy)
{
	int	fails = 0;
	Elf_Addr *pltgot = (Elf_Addr *)object->Dyn.info[DT_PLTGOT];
	Elf_Word *entry = (Elf_Word *)pltgot;

	if (object->Dyn.info[DT_PLTREL] != DT_RELA)
		return (0);

	if (object->traced)
		lazy = 1;

	if (!lazy) {
		fails = _dl_md_reloc_all_plt(object);
	} else {
		_dl_install_plt(&entry[0], (Elf_Addr)&_dl_bind_start_0);
		_dl_install_plt(&entry[8], (Elf_Addr)&_dl_bind_start_1);

		pltgot[8] = (Elf_Addr)object;
	}

	return (fails);
}
