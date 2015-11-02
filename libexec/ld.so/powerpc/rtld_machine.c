/*	$OpenBSD: rtld_machine.c,v 1.56 2015/11/02 07:02:53 guenther Exp $ */

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
#include <sys/syscall.h>
#include <sys/unistd.h>

#include <nlist.h>
#include <link.h>

#include "syscall.h"
#include "archdep.h"
#include "resolve.h"

#define	DT_PROC(n)	((n) - DT_LOPROC + DT_NUM)

void _dl_syncicache(char *from, size_t len);

int64_t pcookie __attribute__((section(".openbsd.randomdata"))) __dso_hidden;

/* relocation bits */
#define HA(x) (((Elf_Addr)(x) >> 16) + (((Elf_Addr)(x) & 0x00008000) >> 15))
#define L(x) (((Elf_Addr)x) & 0x0000ffff)
#define ADDIS_R11_R11	0x3d6b0000
#define ADDIS_R11_R0	0x3d600000
#define ADDI_R11_R11	0x396b0000
#define LWZ_R11_R11	0x816b0000
#define LI_R11		0x39600000

#define ADDIS_R12_R0	0x3d800000
#define ADDI_R12_R12	0x398c0000
#define MCTR_R11	0x7d6903a6
#define MCTR_R12	0x7d8903a6
#define BCTR		0x4e800420
#define BRVAL(from, to)					\
	((((Elf32_Addr)(to) - (Elf32_Addr)(&(from)))	\
	    & ~0xfc000000) | 0x48000000)
#define BR(from, to)	((from) = BRVAL(from, to))


#define SLWI_R12_R11_1	0x556c083c
#define ADD_R11_R12_R11 0x7d6c5a14

/* these are structures/functions offset from PLT region */
#define PLT_CALL_OFFSET		8
#define PLT_INFO_OFFSET		12
#define PLT_1STRELA_OFFSET	18
#define B24_VALID_RANGE(x) \
    ((((x) & 0xfe000000) == 0x00000000) || (((x) &  0xfe000000) == 0xfe000000))

void _dl_bind_start(void); /* XXX */
Elf_Addr _dl_bind(elf_object_t *object, int reloff);

int
_dl_md_reloc(elf_object_t *object, int rel, int relasz)
{
	int	i;
	int	numrela;
	long	relrel;
	int	fails = 0;
	struct load_list *llist;
	Elf32_Addr loff;
	Elf32_Rela  *relas;
	/* for jmp table relocations */
	Elf32_Addr *pltresolve;
	Elf32_Addr *pltcall;
	Elf32_Addr *plttable;
	Elf32_Addr *pltinfo;
	Elf32_Addr *first_rela;
	Elf32_Addr prev_value = 0, prev_ooff = 0;
	const Elf32_Sym *prev_sym = NULL;

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
		_dl_printf("relcount > numrel: %ld > %ld\n", relrel, numrela);
		_dl_exit(20);
	}

	pltresolve = NULL;
	pltcall = NULL;
	plttable = NULL;

	/* for plt relocation usage */
	if (object->Dyn.info[DT_JMPREL] != 0 &&
	    object->Dyn.info[DT_PROC(DT_PPC_GOT)] == 0) {
		/* resolver stub not set up */
		int nplt;

		/* Need to construct table to do jumps */
		pltresolve = (Elf32_Addr *)(object->Dyn.info[DT_PLTGOT]);
		pltcall = (Elf32_Addr *)(pltresolve) + PLT_CALL_OFFSET;
		pltinfo = (Elf32_Addr *)(pltresolve) + PLT_INFO_OFFSET;
		first_rela =  (Elf32_Addr *)(pltresolve) + PLT_1STRELA_OFFSET;

		nplt = object->Dyn.info[DT_PLTRELSZ]/sizeof(Elf32_Rela);

		if (nplt >= (2<<12)) {
			plttable = (Elf32_Addr *) ((Elf32_Addr)first_rela)
			    + (2 * (2<<12)) + (4 * (nplt - (2<<12)));
		} else {
			plttable = (Elf32_Addr *) ((Elf32_Addr)first_rela)
			    + (2 * nplt);
		}

		pltinfo[0] = (Elf32_Addr)plttable;

#ifdef DL_PRINTF_DEBUG
		_dl_printf("md_reloc:  plttbl size %x\n",
		    (object->Dyn.info[DT_PLTRELSZ]/sizeof(Elf32_Rela)));
		_dl_printf("md_reloc: plttable %x\n", plttable);
#endif
		pltresolve[0] = SLWI_R12_R11_1;
		pltresolve[1] = ADD_R11_R12_R11;
		pltresolve[2] = ADDIS_R12_R0 | HA(_dl_bind_start);
		pltresolve[3] = ADDI_R12_R12 | L(_dl_bind_start);
		pltresolve[4] = MCTR_R12;
		pltresolve[5] = ADDIS_R12_R0 | HA(object);
		pltresolve[6] = ADDI_R12_R12 | L(object);
		pltresolve[7] = BCTR;
		_dl_dcbf(&pltresolve[0]);
		_dl_dcbf(&pltresolve[7]);

		/* addis r11,r11,.PLTtable@ha*/
		pltcall[0] = ADDIS_R11_R11 | HA(plttable);
		/* lwz r11,plttable@l(r11) */
		pltcall[1] = LWZ_R11_R11 | L(plttable);
		pltcall[2] = MCTR_R11;	/* mtctr r11 */
		pltcall[3] = BCTR;	/* bctr */
		_dl_dcbf(&pltcall[0]);
		_dl_dcbf(&pltcall[3]);
	} else {
		first_rela = NULL;
	}

	/*
	 * Change protection of all write protected segments in the object
	 * so we can do relocations such as REL24, REL16 etc. After
	 * relocation restore protection.
	 */
	if ((object->dyn.textrel == 1) && (rel == DT_REL || rel == DT_RELA)) {
		for (llist = object->load_list; llist != NULL; llist = llist->next) {
			if (!(llist->prot & PROT_WRITE)) {
				_dl_mprotect(llist->start, llist->size,
				    llist->prot|PROT_WRITE);
			}
		}
	}

	/* tight loop for leading RELATIVE relocs */
	for (i = 0; i < relrel; i++, relas++) {
		Elf_Addr *r_addr;
#ifdef DEBUG
		const Elf32_Sym *sym;

		if (ELF32_R_TYPE(relas->r_info) != RELOC_RELATIVE) {
			_dl_printf("RELCOUNT wrong\n");
			_dl_exit(20);
		}
		sym = object->dyn.symtab;
		sym += ELF32_R_SYM(relas->r_info);
		if (ELF32_ST_BIND(sym->st_info) != STB_LOCAL ||
		    (ELF32_ST_TYPE(sym->st_info) != STT_SECTION &&
		    ELF32_ST_TYPE(sym->st_info) != STT_NOTYPE)) {
			_dl_printf("RELATIVE relocation against symbol\n");
			_dl_exit(20);
		}
#endif
		r_addr = (Elf_Addr *)(relas->r_offset + loff);
		*r_addr = loff + relas->r_addend;
	}
	for (; i < numrela; i++, relas++) {
		Elf32_Addr *r_addr = (Elf32_Addr *)(relas->r_offset + loff);
		Elf32_Addr ooff;
		const Elf32_Sym *sym, *this;
		const char *symn;
		int type;

		if (ELF32_R_SYM(relas->r_info) == 0xffffff)
			continue;

		type = ELF32_R_TYPE(relas->r_info);

		if (type == RELOC_JMP_SLOT && rel != DT_JMPREL)
			continue;

		sym = object->dyn.symtab;
		sym += ELF32_R_SYM(relas->r_info);
		symn = object->dyn.strtab + sym->st_name;

		ooff = 0;
		this = NULL;
		if (ELF32_R_SYM(relas->r_info) &&
		    !(ELF32_ST_BIND(sym->st_info) == STB_LOCAL &&
		    ELF32_ST_TYPE (sym->st_info) == STT_NOTYPE)) {
			if (sym == prev_sym) {
				this = sym;	/* XXX any non-NULL */
				ooff = prev_ooff;
			} else {
				ooff = _dl_find_symbol_bysym(object,
				    ELF32_R_SYM(relas->r_info), &this,
				    SYM_SEARCH_ALL|SYM_WARNNOTFOUND|
				    ((type == RELOC_JMP_SLOT) ?
				    SYM_PLT:SYM_NOTPLT), sym, NULL);

				if (this == NULL) {
					if (ELF_ST_BIND(sym->st_info) !=
					    STB_WEAK)
						fails++;
					continue;
				}
				prev_sym = sym;
				prev_value = this->st_value;
				prev_ooff = ooff;
			}
		}

		/*
		 * For Secure-PLT, RELOC_JMP_SLOT simply sets PLT
		 * slots similarly to how RELOC_GLOB_DAT updates GOT
		 * slots.
		 */
		if (type == RELOC_JMP_SLOT &&
		    object->Dyn.info[DT_PROC(DT_PPC_GOT)])
			type = RELOC_GLOB_DAT;

		switch (type) {
		case RELOC_32:
			if (ELF32_ST_BIND(sym->st_info) == STB_LOCAL &&
			    (ELF32_ST_TYPE(sym->st_info) == STT_SECTION ||
			    ELF32_ST_TYPE(sym->st_info) == STT_NOTYPE) ) {
				*r_addr = ooff + relas->r_addend;
			} else {
				*r_addr = ooff + prev_value +
				    relas->r_addend;
			}
			break;
		case RELOC_RELATIVE:
			if (ELF32_ST_BIND(sym->st_info) == STB_LOCAL &&
			    (ELF32_ST_TYPE(sym->st_info) == STT_SECTION ||
			    ELF32_ST_TYPE(sym->st_info) == STT_NOTYPE) ) {
				*r_addr = loff + relas->r_addend;

#ifdef DL_PRINTF_DEBUG
_dl_printf("rel1 r_addr %x val %x loff %x ooff %x addend %x\n", r_addr,
    loff + relas->r_addend, loff, ooff, relas->r_addend);
#endif

			} else {
				*r_addr = loff + prev_value +
				    relas->r_addend;
			}
			break;
		case RELOC_JMP_SLOT:
		    {
			Elf32_Addr target = ooff + prev_value +
			    relas->r_addend;
			Elf32_Addr val = target - (Elf32_Addr)r_addr;

			if (!B24_VALID_RANGE(val)){
				int index;
#ifdef DL_PRINTF_DEBUG
_dl_printf(" ooff %x, sym val %x, addend %x"
	" r_addr %x symn [%s] -> %x\n",
	ooff, prev_value, relas->r_addend,
	r_addr, symn, val);
#endif
				/* if offset is > RELOC_24 deal with it */
				index = (r_addr - first_rela) >> 1;

				if (index >= (2 << 12)) {
					/* addis r11,r11,.PLTtable@ha*/
					r_addr[0] = ADDIS_R11_R0 | HA(index*4);
					r_addr[1] = ADDI_R11_R11 | L(index*4);
					BR(r_addr[2], pltcall);
				} else {
					r_addr[0] = LI_R11 | (index * 4);
					BR(r_addr[1], pltcall);

				}
				_dl_dcbf(&r_addr[0]);
				_dl_dcbf(&r_addr[2]);
				val= ooff + prev_value +
				    relas->r_addend;
#ifdef DL_PRINTF_DEBUG
_dl_printf(" symn [%s] val 0x%x\n", symn, val);
#endif
				plttable[index] = val;
			} else {
				/* if the offset is small enough,
				 * branch directly to the dest
				 */
				BR(r_addr[0], target);
				_dl_dcbf(&r_addr[0]);
			}
		    }

			break;
		case RELOC_GLOB_DAT:
			*r_addr = ooff + prev_value + relas->r_addend;
			break;
#if 1
		/* should not be supported ??? */
		case RELOC_REL24:
		    {
			Elf32_Addr val = ooff + prev_value +
			    relas->r_addend - (Elf32_Addr)r_addr;
			if (!B24_VALID_RANGE(val)){
				/* invalid offset */
				_dl_exit(20);
			}
			val &= ~0xfc000003;
			val |= (*r_addr & 0xfc000003);
			*r_addr = val;

			_dl_dcbf(r_addr);
		    }
		break;
#endif
#if 1
		case RELOC_16_LO:
		    {
			Elf32_Addr val;

			val = loff + relas->r_addend;
			*(Elf32_Half *)r_addr = val;

			_dl_dcbf(r_addr);
		    }
		break;
#endif
#if 1
		case RELOC_16_HI:
		    {
			Elf32_Addr val;

			val = loff + relas->r_addend;
			*(Elf32_Half *)r_addr = (val >> 16);

			_dl_dcbf(r_addr);
		    }
		break;
#endif
#if 1
		case RELOC_16_HA:
		    {
			Elf32_Addr val;

			val = loff + relas->r_addend;
			*(Elf32_Half *)r_addr = ((val + 0x8000) >> 16);

			_dl_dcbf(r_addr);
		    }
		break;
#endif
		case RELOC_REL14_TAKEN:
			/* val |= 1 << (31-10) XXX? */
		case RELOC_REL14:
		case RELOC_REL14_NTAKEN:
		    {
			Elf32_Addr val = ooff + prev_value +
			    relas->r_addend - (Elf32_Addr)r_addr;
			if (((val & 0xffff8000) != 0) &&
			    ((val & 0xffff8000) != 0xffff8000)) {
				/* invalid offset */
				_dl_exit(20);
			}
			val &= ~0xffff0003;
			val |= (*r_addr & 0xffff0003);
			*r_addr = val;
#ifdef DL_PRINTF_DEBUG
			_dl_printf("rel 14 %x val %x\n", r_addr, val);
#endif

			_dl_dcbf(r_addr);
		    }
			break;
		case RELOC_COPY:
		{
#ifdef DL_PRINTF_DEBUG
			_dl_printf("copy r_addr %x, sym %x [%s] size %d val %x\n",
			    r_addr, sym, symn, sym->st_size,
			    (ooff + prev_value+
			    relas->r_addend));
#endif
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
					_dl_printf("symbols size differ [%s] \n",
					    symn);
					size = sym->st_size < cpysrc->st_size ?
					    sym->st_size : cpysrc->st_size;
				}
#ifdef DL_PRINTF_DEBUG
_dl_printf(" found other symbol at %x size %d\n",
    src_loff + cpysrc->st_value,  cpysrc->st_size);
#endif
				_dl_bcopy((void *)(src_loff + cpysrc->st_value),
				    r_addr, size);
			} else
				fails++;
		}
			break;
		case RELOC_NONE:
			break;

		default:
			_dl_printf("%s:"
			    " %s: unsupported relocation '%s' %d at %x\n",
			    _dl_progname, object->load_name, symn,
			    ELF32_R_TYPE(relas->r_info), r_addr );
			_dl_exit(1);
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
	return(fails);
}

void
_dl_setup_secure_plt(elf_object_t *object)
{
	Elf32_Addr *got;
	Elf32_Addr *plt;
	int numplt, i;

	/* Relocate processor-specific tags. */
	object->Dyn.info[DT_PROC(DT_PPC_GOT)] += object->obj_base;

	got = (Elf32_Addr *)
	    (Elf32_Rela *)(object->Dyn.info[DT_PROC(DT_PPC_GOT)]);
	got[1] = (Elf32_Addr)_dl_bind_start;
	got[2] = (Elf32_Addr)object;

	plt = (Elf32_Addr *)
	   (Elf32_Rela *)(object->Dyn.info[DT_PLTGOT]);
	numplt = object->Dyn.info[DT_PLTRELSZ] / sizeof(Elf32_Rela);
	for (i = 0; i < numplt; i++)
		plt[i] += object->obj_base;
}

void
_dl_setup_bss_plt(elf_object_t *object)
{
	Elf_Addr *pltresolve;
	Elf_Addr *first_rela;
	Elf_RelA *relas;
	Elf32_Addr *r_addr;
	int numrela, i;
	int index;

	first_rela = (Elf32_Addr *)
	    (((Elf32_Rela *)(object->Dyn.info[DT_JMPREL]))->r_offset +
	    object->obj_base);
	pltresolve = (Elf32_Addr *)(first_rela) - 18;

	relas = (Elf32_Rela *)(object->Dyn.info[DT_JMPREL]);
	numrela = object->Dyn.info[DT_PLTRELSZ] / sizeof(Elf32_Rela);
	r_addr = (Elf32_Addr *)(relas->r_offset + object->obj_base);

	for (i = 0, index = 0; i < numrela; i++, r_addr+=2, index++) {
		if (index >= (2 << 12)) {
			/* addis r11,r0,.PLTtable@ha*/
			r_addr[0] = ADDIS_R11_R0 | HA(index*4);
			r_addr[1] = ADDI_R11_R11 | L(index*4);
			BR(r_addr[2], pltresolve);
			/* only every other slot is used after
			 * index == 2^14
			 */
			r_addr += 2;
		} else {
			r_addr[0] = LI_R11 | (index * 4);
			BR(r_addr[1], pltresolve);
		}
		_dl_dcbf(&r_addr[0]);
		_dl_dcbf(&r_addr[2]);
	}
}

/*
 *	Relocate the Global Offset Table (GOT).
 *	This is done by calling _dl_md_reloc on DT_JMPREL for DL_BIND_NOW,
 *	otherwise the lazy binding plt initialization is performed.
 */
int
_dl_md_reloc_got(elf_object_t *object, int lazy)
{
	void *got_addr;
	int fails = 0;
	int prot_exec = 0;

	if (object->Dyn.info[DT_PLTREL] != DT_RELA)
		return (0);

	/*
	 * For BSS-PLT, both the GOT and the PLT need to be
	 * executable.  Yuck!
	 */
	if (object->Dyn.info[DT_PROC(DT_PPC_GOT)] == 0)
		prot_exec = PROT_EXEC;

	if (object->traced)
		lazy = 1;

	if (!lazy) {
		fails = _dl_md_reloc(object, DT_JMPREL, DT_PLTRELSZ);
	} else {
		if (object->Dyn.info[DT_PROC(DT_PPC_GOT)])
			_dl_setup_secure_plt(object);
		else
			_dl_setup_bss_plt(object);
	}

	/* mprotect the GOT */
	got_addr = _dl_protect_segment(object, 0, "__got_start", "__got_end",
	    PROT_READ|prot_exec);
	if (got_addr != NULL)
		_dl_syncicache(got_addr, 4);

	/* mprotect the PLT */
	_dl_protect_segment(object, 0, "__plt_start", "__plt_end",
	    PROT_READ|prot_exec);

	return (fails);
}

Elf_Addr
_dl_bind(elf_object_t *object, int reloff)
{
	const Elf_Sym *sym, *this;
	Elf_Addr *r_addr, ooff;
	const char *symn;
	const elf_object_t *sobj;
	Elf_Addr value;
	Elf_RelA *relas;
	Elf32_Addr val;
	Elf32_Addr *pltresolve;
	Elf32_Addr *pltcall;
	Elf32_Addr *pltinfo;
	Elf32_Addr *plttable;
	int64_t cookie = pcookie;
	struct {
		struct __kbind param[2];
		Elf_Addr newval[2];
	} buf;
	struct __kbind *param;
	size_t psize;

	relas = (Elf_RelA *)(object->Dyn.info[DT_JMPREL] + reloff);

	sym = object->dyn.symtab;
	sym += ELF_R_SYM(relas->r_info);
	symn = object->dyn.strtab + sym->st_name;

	this = NULL;
	ooff = _dl_find_symbol(symn, &this,
	    SYM_SEARCH_ALL|SYM_WARNNOTFOUND|SYM_PLT, sym, object, &sobj);
	if (this == NULL) {
		_dl_printf("lazy binding failed!\n");
		*(volatile int *)0 = 0;		/* XXX */
	}

	value = ooff + this->st_value;

	if (__predict_false(sobj->traced) && _dl_trace_plt(sobj, symn))
		return (value);

	r_addr = (Elf_Addr *)(object->obj_base + relas->r_offset);
	val = value - (Elf32_Addr)r_addr;

	if (object->Dyn.info[DT_PROC(DT_PPC_GOT)] == 0) {
		if (!B24_VALID_RANGE(val)) {
			int index, addr_off;

			/* if offset is > RELOC_24 deal with it */
			index = reloff / sizeof(Elf32_Rela);

			pltresolve = (Elf32_Addr *)
			    (Elf32_Rela *)(object->Dyn.info[DT_PLTGOT]);
			pltcall = (Elf32_Addr *)(pltresolve) + PLT_CALL_OFFSET;

			/*
			 * Early plt entries can make short jumps; later ones
			 * use a 3 word sequence.  c.f. _dl_md_reloc_got()
			 */
			addr_off = (index >= (2 << 12)) ? 2 : 1;

			/*
			 * Update plttable before pltcall branch, to make
			 * this a safe race for threads
			 */
			pltinfo = (Elf32_Addr *)(pltresolve) + PLT_INFO_OFFSET;
			plttable = (Elf32_Addr *)pltinfo[0];

			buf.param[0].kb_addr = &plttable[index];
			buf.param[0].kb_size = sizeof(Elf_Addr);
			buf.param[1].kb_addr = &r_addr[addr_off];
			buf.param[1].kb_size = sizeof(Elf_Addr);
			buf.newval[0] = value + relas->r_addend;
			buf.newval[1] = BRVAL(r_addr[addr_off], pltcall);
			param = &buf.param[0];
			psize = sizeof(buf);
		} else {
			/*
			 * If the offset is small enough, branch directly to
			 * the dest.  We use the _second_ kbind params only.
			 */
			buf.param[1].kb_addr = &r_addr[0];
			buf.param[1].kb_size = sizeof(Elf_Addr);
			buf.newval[0] = BRVAL(r_addr[0], value);
			param = &buf.param[1];
			psize = sizeof(struct __kbind) + sizeof(Elf_Addr);
		}
	} else {
		int index = reloff / sizeof(Elf32_Rela);

		/*
		 * Secure PLT; only needs one update so use the
		 * second kbind params.
		 */
		plttable = (Elf32_Addr *)
		    (Elf32_Rela *)(object->Dyn.info[DT_PLTGOT]);
		buf.param[1].kb_addr = &plttable[index];
		buf.param[1].kb_size = sizeof(Elf_Addr);
		buf.newval[0] = value;
		param = &buf.param[1];
		psize = sizeof(struct __kbind) + sizeof(Elf_Addr);
	}

	{
		register long syscall_num __asm("r0") = SYS_kbind;
		register void *arg1 __asm("r3") = param;
		register long  arg2 __asm("r4") = psize;
		register long  arg3 __asm("r5") = 0xffffffff & (cookie >> 32);
		register long  arg4 __asm("r6") = 0xffffffff &  cookie;

		__asm volatile("sc" : "+r" (syscall_num), "+r" (arg1),
		    "+r" (arg2) : "r" (arg3), "r" (arg4) : "cc", "memory");
	}

	return (value);
}

/* should not be defined here, but it is 32 for all powerpc 603-G4 */
#define CACHELINESIZE 32
void
_dl_syncicache(char *from, size_t len)
{
	unsigned int off = 0;
	int l = len + ((int)from & (CACHELINESIZE-1));

	while (off < l) {
		asm volatile ("dcbst %1,%0" :: "r"(from), "r"(off));
		asm volatile ("sync");
		asm volatile ("icbi %1, %0" :: "r"(from), "r"(off));
		asm volatile ("sync");
		asm volatile ("isync");

		off += CACHELINESIZE;
	}
}
