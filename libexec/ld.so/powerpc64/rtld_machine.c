/*	$OpenBSD: rtld_machine.c,v 1.7 2022/01/08 06:49:42 guenther Exp $ */

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
#include <sys/exec_elf.h>
#include <sys/syscall.h>
#include <sys/unistd.h>

#include <machine/reloc.h>

#include "util.h"
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
	Elf_Addr loff;
	Elf_RelA  *relas;
	/* for jmp table relocations */
	Elf_Addr prev_value = 0, prev_ooff = 0;
	const Elf_Sym *prev_sym = NULL;

	loff = object->obj_base;
	numrela = object->Dyn.info[relasz] / sizeof(Elf_RelA);
	relrel = rel == DT_RELA ? object->relacount : 0;
	relas = (Elf_RelA *)(object->Dyn.info[rel]);

	if (relas == NULL)
		return 0;

	if (relrel > numrela)
		_dl_die("relcount > numrel: %ld > %d", relrel, numrela);

	/* tight loop for leading RELATIVE relocs */
	for (i = 0; i < relrel; i++, relas++) {
		Elf_Addr *r_addr;

		r_addr = (Elf_Addr *)(relas->r_offset + loff);
		*r_addr = loff + relas->r_addend;
	}
	for (; i < numrela; i++, relas++) {
		Elf_Addr *r_addr = (Elf_Addr *)(relas->r_offset + loff);
		const Elf_Sym *sym;
		const char *symn;
		int type;

		if (ELF_R_SYM(relas->r_info) == 0xffffff)
			continue;

		type = ELF_R_TYPE(relas->r_info);

		if (type == R_PPC64_JMP_SLOT && rel != DT_JMPREL)
			continue;

		sym = object->dyn.symtab;
		sym += ELF_R_SYM(relas->r_info);
		symn = object->dyn.strtab + sym->st_name;

		if (ELF_R_SYM(relas->r_info) &&
		    !(ELF_ST_BIND(sym->st_info) == STB_LOCAL &&
		    ELF_ST_TYPE (sym->st_info) == STT_NOTYPE) &&
		    sym != prev_sym) {
			struct sym_res sr;

			sr = _dl_find_symbol(symn,
			    SYM_SEARCH_ALL|SYM_WARNNOTFOUND|
			    ((type == R_PPC64_JMP_SLOT) ?
			    SYM_PLT:SYM_NOTPLT), sym, object);

			if (sr.sym == NULL) {
				if (ELF_ST_BIND(sym->st_info) != STB_WEAK)
					fails++;
				continue;
			}
			prev_sym = sym;
			prev_value = sr.sym->st_value;
			prev_ooff = sr.obj->obj_base;
		}

		switch (type) {
		case R_PPC64_ADDR64:
			if (ELF_ST_BIND(sym->st_info) == STB_LOCAL &&
			    (ELF_ST_TYPE(sym->st_info) == STT_SECTION ||
			    ELF_ST_TYPE(sym->st_info) == STT_NOTYPE) ) {
				*r_addr = prev_ooff + relas->r_addend;
			} else {
				*r_addr = prev_ooff + prev_value +
				    relas->r_addend;
			}
			break;
		case R_PPC64_RELATIVE:
			if (ELF_ST_BIND(sym->st_info) == STB_LOCAL &&
			    (ELF_ST_TYPE(sym->st_info) == STT_SECTION ||
			    ELF_ST_TYPE(sym->st_info) == STT_NOTYPE) ) {
				*r_addr = loff + relas->r_addend;
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
		case R_PPC64_JMP_SLOT:
		case R_PPC64_GLOB_DAT:
			*r_addr = prev_ooff + prev_value + relas->r_addend;
			break;
#if 0
		/* should not be supported ??? */
		case RELOC_REL24:
		    {
			Elf_Addr val = prev_ooff + prev_value +
			    relas->r_addend - (Elf_Addr)r_addr;
			if (!B24_VALID_RANGE(val)) {
				/* invalid offset */
				_dl_die("%s: invalid %s offset %llx at %p",
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
#if 0
		case RELOC_16_LO:
		    {
			Elf_Addr val;

			val = loff + relas->r_addend;
			*(Elf_Half *)r_addr = val;

			_dl_dcbf(r_addr);
		    }
		break;
#endif
#if 0
		case RELOC_16_HI:
		    {
			Elf_Addr val;

			val = loff + relas->r_addend;
			*(Elf_Half *)r_addr = (val >> 16);

			_dl_dcbf(r_addr);
		    }
		break;
#endif
#if 0
		case RELOC_16_HA:
		    {
			Elf_Addr val;

			val = loff + relas->r_addend;
			*(Elf_Half *)r_addr = ((val + 0x8000) >> 16);

			_dl_dcbf(r_addr);
		    }
		break;
#endif
#if 0
		case RELOC_REL14_TAKEN:
			/* val |= 1 << (31-10) XXX? */
		case RELOC_REL14:
		case RELOC_REL14_NTAKEN:
		    {
			Elf_Addr val = prev_ooff + prev_value +
			    relas->r_addend - (Elf_Addr)r_addr;
			if (((val & 0xffff8000) != 0) &&
			    ((val & 0xffff8000) != 0xffff8000)) {
				/* invalid offset */
				_dl_die("%s: invalid %s offset %llx at %p",
				    object->load_name, "REL14", val,
				    (void *)r_addr);
			}
			val &= ~0xffff0003;
			val |= (*r_addr & 0xffff0003);
			*r_addr = val;
			_dl_dcbf(r_addr);
		    }
			break;
#endif
		case R_PPC64_COPY:
		{
			struct sym_res sr;
			/*
			 * we need to find a symbol, that is not in the current
			 * object, start looking at the beginning of the list,
			 * searching all objects but _not_ the current object,
			 * first one found wins.
			 */
			sr = _dl_find_symbol(symn,
			    SYM_SEARCH_OTHER|SYM_WARNNOTFOUND| SYM_NOTPLT,
			    sym, object);
			if (sr.sym != NULL) {
				_dl_bcopy((void *)(sr.obj->obj_base + sr.sym->st_value),
				    r_addr, sym->st_size);
			} else
				fails++;
		}
			break;
		case R_PPC64_NONE:
			break;

		default:
			_dl_die("%s: unsupported relocation '%s' %lld at %p\n",
			    object->load_name, symn,
			    ELF_R_TYPE(relas->r_info), (void *)r_addr );
		}
	}

	return fails;
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
		return 0;

	if (!lazy) {
		fails = _dl_md_reloc(object, DT_JMPREL, DT_PLTRELSZ);
	} else {
		Elf_Addr *plt;
		int numplt, n;

		/* Relocate processor-specific tags. */
		object->Dyn.info[DT_PROC(DT_PPC64_GLINK)] += object->obj_base;

		plt = (Elf_Addr *)
		   (Elf_RelA *)(object->Dyn.info[DT_PLTGOT]);
		numplt = object->Dyn.info[DT_PLTRELSZ] / sizeof(Elf_RelA);
		plt[0] = (uint64_t)_dl_bind_start;
		plt[1] = (uint64_t)object;
		for (n = 0; n < numplt; n++) {
			plt[n + 2] = object->Dyn.info[DT_PROC(DT_PPC64_GLINK)] +
			    n * 4 + 32;
		}
	}

	return fails;
}

Elf_Addr
_dl_bind(elf_object_t *object, int relidx)
{
	const Elf_Sym *sym;
	struct sym_res sr;
	const char *symn;
	Elf_RelA *relas;
	Elf_Addr *plttable;
	int64_t cookie = pcookie;
	struct {
		struct __kbind param;
		Elf_Addr newval;
	} buf;

	relas = ((Elf_RelA *)object->Dyn.info[DT_JMPREL]) + relidx;

	sym = object->dyn.symtab;
	sym += ELF_R_SYM(relas->r_info);
	symn = object->dyn.strtab + sym->st_name;

	sr = _dl_find_symbol(symn, SYM_SEARCH_ALL|SYM_WARNNOTFOUND|SYM_PLT,
	    sym, object);
	if (sr.sym == NULL)
		_dl_die("lazy binding failed!");

	buf.newval = sr.obj->obj_base + sr.sym->st_value;

	if (__predict_false(sr.obj->traced) && _dl_trace_plt(sr.obj, symn))
		return buf.newval;

	plttable = (Elf_Addr *)(Elf_RelA *)(object->Dyn.info[DT_PLTGOT]);
	buf.param.kb_addr = &plttable[relidx + 2];
	buf.param.kb_size = sizeof(Elf_Addr);

	{
		register long syscall_num __asm("r0") = SYS_kbind;
		register void *arg1 __asm("r3") = &buf.param;
		register long  arg2 __asm("r4") = sizeof(struct __kbind) +
		    sizeof(Elf_Addr);
		register long  arg3 __asm("r5") = cookie;

		__asm volatile("sc" : "+r" (syscall_num), "+r" (arg1),
		    "+r" (arg2) : "r" (arg3) : "cc", "memory");
	}

	return buf.newval;
}
