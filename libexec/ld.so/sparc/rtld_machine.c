/*	$OpenBSD: rtld_machine.c,v 1.32 2010/05/02 04:57:01 guenther Exp $ */

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

#define _DYN_LOADER

#include <sys/types.h>
#include <sys/cdefs.h>
#include <sys/mman.h>
#include <sys/param.h>
#include <sys/sysctl.h>
#include <machine/cpu.h>

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
#define _RF_SZ(s)	(((s) & 0xff) << 8)	/* memory target size */
#define _RF_RS(s)	((s) & 0xff)		/* right shift */
static int reloc_target_flags[] = {
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
	_RF_S|_RF_A|		_RF_SZ(32) | _RF_RS(0),		/* GLOB_DAT */
	_RF_S|			_RF_SZ(32) | _RF_RS(0),		/* JMP_SLOT */
	      _RF_A|	_RF_B|	_RF_SZ(32) | _RF_RS(0),		/* RELATIVE */
	_RF_S|_RF_A|		_RF_SZ(32) | _RF_RS(0),		/* UA_32 */

	/*unknown*/		_RF_SZ(32) | _RF_RS(0),		/* PLT32 */
	/*unknown*/		_RF_SZ(32) | _RF_RS(0),		/* HIPLT22 */
	/*unknown*/		_RF_SZ(32) | _RF_RS(0),		/* LOPLT10 */
	/*unknown*/		_RF_SZ(32) | _RF_RS(0),		/* LOPLT10 */
	/*unknown*/		_RF_SZ(32) | _RF_RS(0),		/* PCPLT22 */
	/*unknown*/		_RF_SZ(32) | _RF_RS(0),		/* PCPLT32 */
	_RF_S|_RF_A|/*unknown*/	_RF_SZ(32) | _RF_RS(0),		/* 10 */
	_RF_S|_RF_A|/*unknown*/	_RF_SZ(32) | _RF_RS(0),		/* 11 */
	_RF_S|_RF_A|/*unknown*/	_RF_SZ(32) | _RF_RS(0),		/* 64 */
	_RF_S|_RF_A|/*unknown*/	_RF_SZ(32) | _RF_RS(0),		/* OLO10 */
	_RF_S|_RF_A|/*unknown*/	_RF_SZ(32) | _RF_RS(0),		/* HH22 */
	_RF_S|_RF_A|/*unknown*/	_RF_SZ(32) | _RF_RS(0),		/* HM10 */
	_RF_S|_RF_A|/*unknown*/	_RF_SZ(32) | _RF_RS(0),		/* LM22 */
	_RF_S|_RF_A|_RF_P|/*unknown*/	_RF_SZ(32) | _RF_RS(0),	/* WDISP16 */
	_RF_S|_RF_A|_RF_P|/*unknown*/	_RF_SZ(32) | _RF_RS(0),	/* WDISP19 */
	/*unknown*/		_RF_SZ(32) | _RF_RS(0),		/* GLOB_JMP */
	/*unknown*/		_RF_SZ(32) | _RF_RS(0),		/* 7 */
	/*unknown*/		_RF_SZ(32) | _RF_RS(0),		/* 5 */
	/*unknown*/		_RF_SZ(32) | _RF_RS(0),		/* 6 */
};

#define RELOC_RESOLVE_SYMBOL(t)		((reloc_target_flags[t] & _RF_S) != 0)
#define RELOC_PC_RELATIVE(t)		((reloc_target_flags[t] & _RF_P) != 0)
#define RELOC_USE_ADDEND(t)		((reloc_target_flags[t] & _RF_A) != 0)
#define RELOC_TARGET_SIZE(t)		((reloc_target_flags[t] >> 8) & 0xff)
#define RELOC_VALUE_RIGHTSHIFT(t)	(reloc_target_flags[t] & 0xff)

static int reloc_target_bitmask[] = {
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
	-1, -1, -1,			/* _GLOB_DAT, JMP_SLOT, _RELATIVE */
	_BM(32), _BM(32),		/* _UA32, PLT32 */
	_BM(22), _BM(10),		/* _HIPLT22, LOPLT10 */
	_BM(32), _BM(22), _BM(10),	/* _PCPLT32, _PCPLT22, _PCPLT10 */
	_BM(10), _BM(11), -1,		/* _10, _11, _64 */
	_BM(10), _BM(22),		/* _OLO10, _HH22 */
	_BM(10), _BM(22),		/* _HM10, _LM22 */
	_BM(16), _BM(19),		/* _WDISP16, _WDISP19 */
	-1,				/* GLOB_JMP */
	_BM(7), _BM(5), _BM(6)		/* _7, _5, _6 */
#undef _BM
};
#define RELOC_VALUE_BITMASK(t)	(reloc_target_bitmask[t])

static inline void
_dl_reloc_plt(Elf_Addr *where, Elf_Addr value)
{
	/*
	 * At the PLT entry pointed at by `where', we now construct
	 * a direct transfer to the now fully resolved function
	 * address.  The resulting code in the jump slot is:
	 *
	 *	sethi	%hi(roffset), %g1
	 *	sethi	%hi(addr), %g1
	 *	jmp	%g1+%lo(addr)
	 *
	 * We write the third instruction first, since that leaves the
	 * previous `b,a' at the second word in place. Hence the whole
	 * PLT slot can be atomically change to the new sequence by
	 * writing the `sethi' instruction at word 2.
	 */
#define SETHI	0x03000000
#define JMP	0x81c06000
#define NOP	0x01000000
	where[2] = JMP   | (value & 0x000003ff);
	where[1] = SETHI | ((value >> 10) & 0x003fffff);
	__asm __volatile("iflush %0+8" : : "r" (where));
	__asm __volatile("iflush %0+4" : : "r" (where));
	/*
	 * iflush requires 5 subsequent cycles to be sure all copies
	 * are flushed from the CPU and the icache.
	 */
	__asm __volatile("nop;nop;nop;nop;nop");
}

int
_dl_md_reloc(elf_object_t *object, int rel, int relasz)
{
	long	i;
	long	numrela;
	int	fails = 0;
	Elf_Addr loff;
	Elf_RelA *relas;
	struct load_list *llist;

	loff = object->obj_base;
	numrela = object->Dyn.info[relasz] / sizeof(Elf_RelA);
	relas = (Elf_RelA *)(object->Dyn.info[rel]);

	if (relas == NULL)
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

	for (i = 0; i < numrela; i++, relas++) {
		Elf_Addr *where, ooff;
		Elf_Word type, value, mask;
		const Elf_Sym *sym, *this;
		const char *symn;

		type = ELF_R_TYPE(relas->r_info);

		if (type == R_TYPE(NONE))
			continue;

		if (type == R_TYPE(JMP_SLOT) && rel != DT_JMPREL)
			continue;

		where = (Elf_Addr *)(relas->r_offset + loff);

		if (type == R_TYPE(RELATIVE)) {
			*where += (Elf_Addr)(loff + relas->r_addend);
			continue;
		}

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
				value += (Elf_Addr)(ooff + this->st_value);
			}
		}

		if (type == R_TYPE(COPY)) {
			void *dstaddr = where;
			const void *srcaddr;
			const Elf_Sym *dstsym = sym, *srcsym = NULL;
			size_t size = dstsym->st_size;
			Elf_Addr soff;

			soff = _dl_find_symbol(symn, &srcsym,
			    SYM_SEARCH_OTHER|SYM_WARNNOTFOUND|
			    ((type == R_TYPE(JMP_SLOT)) ? SYM_PLT : SYM_NOTPLT),
			    dstsym, object, NULL);
			if (srcsym == NULL)
				goto resolve_failed;

			srcaddr = (void *)(soff + srcsym->st_value);
			_dl_bcopy(srcaddr, dstaddr, size);
			continue;
		}

		if (type == R_TYPE(JMP_SLOT)) {
			_dl_reloc_plt(where, value);
			continue;
		}

		if (RELOC_PC_RELATIVE(type))
			value -= (Elf_Addr)where;

		mask = RELOC_VALUE_BITMASK(type);
		value >>= RELOC_VALUE_RIGHTSHIFT(type);
		value &= mask;

		/* We ignore alignment restrictions here */
		*where &= ~mask;
		*where |= value;
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
 * Resolve a symbol at run-time.
 */
Elf_Addr
_dl_bind(elf_object_t *object, int reloff)
{
	const Elf_Sym *sym, *this;
	Elf_Addr *addr, ooff;
	const char *symn;
	Elf_Addr value;
	Elf_RelA *rela;
	sigset_t savedmask;

	rela = (Elf_RelA *)(object->Dyn.info[DT_JMPREL] + reloff);

	sym = object->dyn.symtab;
	sym += ELF_R_SYM(rela->r_info);
	symn = object->dyn.strtab + sym->st_name;

	addr = (Elf_Addr *)(object->obj_base + rela->r_offset);
	this = NULL;
	ooff = _dl_find_symbol(symn, &this,
	    SYM_SEARCH_ALL|SYM_WARNNOTFOUND|SYM_PLT, sym,
	    object, NULL);
	if (this == NULL) {
		_dl_printf("lazy binding failed!\n");
		*((int *)0) = 0;	/* XXX */
	}

	value = ooff + this->st_value;

	/* if PLT is protected, allow the write */
	if (object->plt_size != 0) {
		_dl_thread_bind_lock(0, &savedmask);
		/* mprotect the actual modified region, not the whole plt */
		_dl_mprotect((void*)addr, sizeof (Elf_Addr) * 3,
		    PROT_READ|PROT_WRITE|PROT_EXEC);
	}

	_dl_reloc_plt(addr, value);

	/* if PLT is (to be protected, change back to RO/X */
	if (object->plt_size != 0) {
		/* mprotect the actual modified region, not the whole plt */
		_dl_mprotect((void*)addr, sizeof (Elf_Addr) * 3,
		    PROT_READ|PROT_EXEC);
		_dl_thread_bind_lock(1, &savedmask);
	}

	return (value);
}

int
_dl_md_reloc_got(elf_object_t *object, int lazy)
{
	int	fails = 0;
	Elf_Addr *pltgot;
	extern void _dl_bind_start(void);	/* XXX */
	Elf_Addr ooff;
	const Elf_Sym *this;
	Elf_Addr plt_addr;

	pltgot = (Elf_Addr *)object->Dyn.info[DT_PLTGOT];

	if (pltgot != NULL) {
		/*
		 * PLTGOT is the PLT on the sparc.
		 * The first entry holds the call the dynamic linker.
		 * We construct a `call' sequence that transfers
		 * to `_dl_bind_start()'.
		 * The second entry holds the object identification.
		 * Note: each PLT entry is three words long.
		 */
#define SAVE	0x9de3bfc0	/* i.e. `save %sp,-64,%sp' */
#define CALL	0x40000000
#define NOP	0x01000000
		pltgot[0] = SAVE;
		pltgot[1] = CALL |
		    ((Elf_Addr)&_dl_bind_start - (Elf_Addr)&pltgot[1]) >> 2;
		pltgot[2] = NOP;
		pltgot[3] = (Elf_Addr) object;
		__asm __volatile("iflush %0+8"  : : "r" (pltgot));
		__asm __volatile("iflush %0+4"  : : "r" (pltgot));
		__asm __volatile("iflush %0+0"  : : "r" (pltgot));
		/*
		 * iflush requires 5 subsequent cycles to be sure all copies
		 * are flushed from the CPU and the icache.
		 */
		__asm __volatile("nop;nop;nop;nop;nop");
	}

	object->got_addr = NULL;
	object->got_size = 0;
	this = NULL;
	ooff = _dl_find_symbol("__got_start", &this,
	    SYM_SEARCH_OBJ|SYM_NOWARNNOTFOUND|SYM_PLT, NULL,
	    object, NULL);
	if (this != NULL)
		object->got_addr = ooff + this->st_value;

	this = NULL;
	ooff = _dl_find_symbol("__got_end", &this,
	    SYM_SEARCH_OBJ|SYM_NOWARNNOTFOUND|SYM_PLT, NULL,
	    object, NULL);
	if (this != NULL)
		object->got_size = ooff + this->st_value  - object->got_addr;

	plt_addr = 0;
	object->plt_size = 0;
	this = NULL;
	ooff = _dl_find_symbol("__plt_start", &this,
	    SYM_SEARCH_OBJ|SYM_NOWARNNOTFOUND|SYM_PLT, NULL,
	    object, NULL);
	if (this != NULL)
		plt_addr = ooff + this->st_value;

	this = NULL;
	ooff = _dl_find_symbol("__plt_end", &this,
	    SYM_SEARCH_OBJ|SYM_NOWARNNOTFOUND|SYM_PLT, NULL,
	    object, NULL);
	if (this != NULL)
		object->plt_size = ooff + this->st_value  - plt_addr;

	if (object->got_addr == NULL)
		object->got_start = NULL;
	else {
		object->got_start = ELF_TRUNC(object->got_addr, _dl_pagesz);
		object->got_size += object->got_addr - object->got_start;
		object->got_size = ELF_ROUND(object->got_size, _dl_pagesz);
	}
	if (plt_addr == NULL)
		object->plt_start = NULL;
	else {
		object->plt_start = ELF_TRUNC(plt_addr, _dl_pagesz);
		object->plt_size += plt_addr - object->plt_start;
		object->plt_size = ELF_ROUND(object->plt_size, _dl_pagesz);
	}

	if (object->obj_type == OBJTYPE_LDR || !lazy || pltgot == NULL) {
		fails = _dl_md_reloc(object, DT_JMPREL, DT_PLTRELSZ);
	}

	if (object->got_size != 0)
		_dl_mprotect((void*)object->got_start, object->got_size,
		    PROT_READ);
	if (object->plt_size != 0)
		_dl_mprotect((void*)object->plt_start, object->plt_size,
		    PROT_READ|PROT_EXEC);

	return (fails);
}


void __mul(void);
void _mulreplace_end(void);
void _mulreplace(void);
void __umul(void);
void _umulreplace_end(void);
void _umulreplace(void);

void __div(void);
void _divreplace_end(void);
void _divreplace(void);
void __udiv(void);
void _udivreplace_end(void);
void _udivreplace(void);

void __rem(void);
void _remreplace_end(void);
void _remreplace(void);
void __urem(void);
void _uremreplace_end(void);
void _uremreplace(void);

void
_dl_mul_fixup()
{
	int mib[2], v8mul;
	size_t len;


	mib[0] = CTL_MACHDEP;
	mib[1] = CPU_V8MUL;
	len = sizeof(v8mul);
	_dl_sysctl(mib, 2, &v8mul, &len, NULL, 0);


	if (!v8mul)
		return;

	_dl_mprotect(&__mul, _mulreplace_end-_mulreplace,
	    PROT_READ|PROT_WRITE|PROT_EXEC);
	_dl_bcopy(_mulreplace, __mul, _mulreplace_end-_mulreplace);
	_dl_mprotect(&__mul, _mulreplace_end-_mulreplace,
	    PROT_READ|PROT_EXEC);

	_dl_mprotect(&__umul, _umulreplace_end-_umulreplace,
	    PROT_READ|PROT_WRITE|PROT_EXEC);
	_dl_bcopy(_umulreplace, __umul, _umulreplace_end-_umulreplace);
	_dl_mprotect(&__umul, _umulreplace_end-_umulreplace,
	    PROT_READ|PROT_EXEC);


	_dl_mprotect(&__div, _divreplace_end-_divreplace,
	    PROT_READ|PROT_WRITE|PROT_EXEC);
	_dl_bcopy(_divreplace, __div, _divreplace_end-_divreplace);
	_dl_mprotect(&__div, _divreplace_end-_divreplace,
	    PROT_READ|PROT_EXEC);

	_dl_mprotect(&__udiv, _udivreplace_end-_udivreplace,
	    PROT_READ|PROT_WRITE|PROT_EXEC);
	_dl_bcopy(_udivreplace, __udiv, _udivreplace_end-_udivreplace);
	_dl_mprotect(&__udiv, _udivreplace_end-_udivreplace,
	    PROT_READ|PROT_EXEC);


	_dl_mprotect(&__rem, _remreplace_end-_remreplace,
	    PROT_READ|PROT_WRITE|PROT_EXEC);
	_dl_bcopy(_remreplace, __rem, _remreplace_end-_remreplace);
	_dl_mprotect(&__rem, _remreplace_end-_remreplace,
	    PROT_READ|PROT_EXEC);

	_dl_mprotect(&__urem, _uremreplace_end-_uremreplace,
	    PROT_READ|PROT_WRITE|PROT_EXEC);
	_dl_bcopy(_uremreplace, __urem, _uremreplace_end-_uremreplace);
	_dl_mprotect(&__urem, _uremreplace_end-_uremreplace,
	    PROT_READ|PROT_EXEC);
}
