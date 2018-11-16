/*	$OpenBSD: rtld_machine.c,v 1.65 2018/11/16 21:15:47 guenther Exp $ */

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

int64_t pcookie __attribute__((section(".openbsd.randomdata"))) __dso_hidden;

/* relocation bits */
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

	if (relrel > numrela)
		_dl_die("relcount > numrel: %ld > %d", relrel, numrela);

	if (object->Dyn.info[DT_PROC(DT_PPC_GOT)] == 0)
		_dl_die("unsupported insecure BSS PLT object");

	/*
	 * Change protection of all write protected segments in the object
	 * so we can do relocations such as REL24, REL16 etc. After
	 * relocation restore protection.
	 */
	if ((object->dyn.textrel == 1) && (rel == DT_REL || rel == DT_RELA)) {
		for (llist = object->load_list; llist != NULL; llist = llist->next) {
			if (!(llist->prot & PROT_WRITE)) {
				_dl_mprotect(llist->start, llist->size,
				    PROT_READ | PROT_WRITE);
			}
		}
	}

	/* tight loop for leading RELATIVE relocs */
	for (i = 0; i < relrel; i++, relas++) {
		Elf_Addr *r_addr;
#ifdef DEBUG
		const Elf32_Sym *sym;

		if (ELF32_R_TYPE(relas->r_info) != RELOC_RELATIVE)
			_dl_die("RELCOUNT wrong");
		sym = object->dyn.symtab;
		sym += ELF32_R_SYM(relas->r_info);
		if (ELF32_ST_BIND(sym->st_info) != STB_LOCAL ||
		    (ELF32_ST_TYPE(sym->st_info) != STT_SECTION &&
		    ELF32_ST_TYPE(sym->st_info) != STT_NOTYPE))
			_dl_die("RELATIVE relocation against symbol");
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
		/*
		 * For Secure-PLT, RELOC_JMP_SLOT simply sets PLT
		 * slots similarly to how RELOC_GLOB_DAT updates GOT
		 * slots.
		 */
		case RELOC_JMP_SLOT:
		case RELOC_GLOB_DAT:
			*r_addr = ooff + prev_value + relas->r_addend;
			break;
#if 1
		/* should not be supported ??? */
		case RELOC_REL24:
		    {
			Elf32_Addr val = ooff + prev_value +
			    relas->r_addend - (Elf32_Addr)r_addr;
			if (!B24_VALID_RANGE(val)) {
				/* invalid offset */
				_dl_die("%s: invalid %s offset %x at %p",
				    object->load_name, "REL24", val,
				    (void *)r_addr);
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
				_dl_die("%s: invalid %s offset %x at %p",
				    object->load_name, "REL14", val,
				    (void *)r_addr);
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
			_dl_die("%s: unsupported relocation '%s' %d at %p\n",
			    object->load_name, symn,
			    ELF32_R_TYPE(relas->r_info), (void *)r_addr );
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

/*
 *	Relocate the Global Offset Table (GOT).
 *	This is done by calling _dl_md_reloc on DT_JMPREL for DL_BIND_NOW,
 *	otherwise the lazy binding plt initialization is performed.
 */
int
_dl_md_reloc_got(elf_object_t *object, int lazy)
{
	int fails = 0;

	if (object->Dyn.info[DT_PLTREL] != DT_RELA)
		return (0);

	if (object->traced)
		lazy = 1;

	if (!lazy) {
		fails = _dl_md_reloc(object, DT_JMPREL, DT_PLTRELSZ);
	} else {
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

	return (fails);
}

Elf_Addr
_dl_bind(elf_object_t *object, int reloff)
{
	const Elf_Sym *sym, *this;
	Elf_Addr ooff;
	const char *symn;
	const elf_object_t *sobj;
	Elf_RelA *relas;
	Elf32_Addr *plttable;
	int64_t cookie = pcookie;
	struct {
		struct __kbind param;
		Elf_Addr newval;
	} buf;

	relas = (Elf_RelA *)(object->Dyn.info[DT_JMPREL] + reloff);

	sym = object->dyn.symtab;
	sym += ELF_R_SYM(relas->r_info);
	symn = object->dyn.strtab + sym->st_name;

	this = NULL;
	ooff = _dl_find_symbol(symn, &this,
	    SYM_SEARCH_ALL|SYM_WARNNOTFOUND|SYM_PLT, sym, object, &sobj);
	if (this == NULL)
		_dl_die("lazy binding failed!");

	buf.newval = ooff + this->st_value;

	if (__predict_false(sobj->traced) && _dl_trace_plt(sobj, symn))
		return buf.newval;

	plttable = (Elf32_Addr *)(Elf32_Rela *)(object->Dyn.info[DT_PLTGOT]);
	buf.param.kb_addr = &plttable[ reloff / sizeof(Elf32_Rela) ];
	buf.param.kb_size = sizeof(Elf_Addr);

	{
		register long syscall_num __asm("r0") = SYS_kbind;
		register void *arg1 __asm("r3") = &buf.param;
		register long  arg2 __asm("r4") = sizeof(struct __kbind) +
		    sizeof(Elf_Addr);
		register long  arg3 __asm("r5") = 0xffffffff & (cookie >> 32);
		register long  arg4 __asm("r6") = 0xffffffff &  cookie;

		__asm volatile("sc" : "+r" (syscall_num), "+r" (arg1),
		    "+r" (arg2) : "r" (arg3), "r" (arg4) : "cc", "memory");
	}

	return buf.newval;
}
