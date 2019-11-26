/*	$OpenBSD: rtld_machine.c,v 1.46 2019/11/26 23:38:52 guenther Exp $ */

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
#include <sys/mman.h>
#include <sys/syscall.h>
#include <sys/unistd.h>

#include <nlist.h>
#include <link.h>

#include "syscall.h"
#include "archdep.h"
#include "resolve.h"

int64_t pcookie __attribute__((section(".openbsd.randomdata"))) __dso_hidden;

int
_dl_md_reloc(elf_object_t *object, int rel, int relsz)
{
	long	i;
	long	numrel;
	long	relrel;
	Elf_Addr loff;
	Elf_Addr prev_value = 0;
	const Elf_Sym *prev_sym = NULL;
	Elf_Rel *rels;

	loff = object->obj_base;
	numrel = object->Dyn.info[relsz] / sizeof(Elf_Rel);
	relrel = object->relcount;
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
		Elf_Addr *where, value;
		Elf_Word type;
		const Elf_Sym *sym;
		const char *symn;

		where = (Elf_Addr *)(rels->r_offset + loff);

		sym = object->dyn.symtab;
		sym += ELF_R_SYM(rels->r_info);
		symn = object->dyn.strtab + sym->st_name;

		type = ELF_R_TYPE(rels->r_info);
		switch (type) {
		case R_TYPE(NONE):
		case R_TYPE(JUMP_SLOT):		/* shouldn't happen */
			continue;

		case R_TYPE(RELATIVE):
			*where += loff;
			continue;

		case R_TYPE(32):
			value = *where;
			break;

		case R_TYPE(GLOB_DAT):
			value = 0;
			break;

		case R_TYPE(COPY):
		{
			struct sym_res sr;

			sr = _dl_find_symbol(symn,
			    SYM_SEARCH_OTHER|SYM_WARNNOTFOUND|SYM_NOTPLT,
			    sym, object);
			if (sr.sym == NULL)
				return 1;

			value = sr.obj->obj_base + sr.sym->st_value;
			_dl_bcopy((void *)value, where, sym->st_size);
			continue;
		}

		default:
			_dl_die("unknown relocation %d", type);
		}

		if (sym->st_shndx != SHN_UNDEF &&
		    ELF_ST_BIND(sym->st_info) == STB_LOCAL) {
			value += loff;
		} else if (sym == prev_sym) {
			value += prev_value;
		} else {
			struct sym_res sr;

			sr = _dl_find_symbol(symn,
			    SYM_SEARCH_ALL|SYM_WARNNOTFOUND|SYM_NOTPLT,
			    sym, object);
			if (sr.sym == NULL) {
				if (ELF_ST_BIND(sym->st_info) != STB_WEAK)
					return 1;
				continue;
			}
			prev_sym = sym;
			prev_value = sr.obj->obj_base + sr.sym->st_value;
			value += prev_value;
		}

		*where = value;
	}

	return 0;
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

static int
_dl_md_reloc_all_plt(elf_object_t *object, const Elf_Rel *reloc,
    const Elf_Rel *rend)
{
	for (; reloc < rend; reloc++) {
		const Elf_Sym *sym;
		const char *symn;
		Elf_Addr *where;
		struct sym_res sr;

		sym = object->dyn.symtab;
		sym += ELF_R_SYM(reloc->r_info);
		symn = object->dyn.strtab + sym->st_name;

		sr = _dl_find_symbol(symn,
		    SYM_SEARCH_ALL|SYM_WARNNOTFOUND|SYM_PLT, sym, object);
		if (sr.sym == NULL) {
			if (ELF_ST_BIND(sym->st_info) != STB_WEAK)
				return 1;
			continue;
		}

		where = (Elf_Addr *)(reloc->r_offset + object->obj_base);
		*where = sr.obj->obj_base + sr.sym->st_value; 
	}

	return 0;
}

int
_dl_md_reloc_got(elf_object_t *object, int lazy)
{
	extern void _dl_bind_start(void);	/* XXX */
	Elf_Addr	*pltgot = (Elf_Addr *)object->Dyn.info[DT_PLTGOT];
	const Elf_Rel	*reloc, *rend;

	if (pltgot == NULL)
		return 0; /* it is possible to have no PLT/GOT relocations */

	if (object->Dyn.info[DT_PLTREL] != DT_REL)
		return 0;

	if (object->traced)
		lazy = 1;

	reloc = (const Elf_Rel *)(object->Dyn.info[DT_JMPREL]);
	rend = (const Elf_Rel *)((char *)reloc + object->Dyn.info[DT_PLTRELSZ]);

	if (!lazy)
		return _dl_md_reloc_all_plt(object, reloc, rend);

	/* Lazy */
	pltgot[1] = (Elf_Addr)object;
	pltgot[2] = (Elf_Addr)&_dl_bind_start;

	for (; reloc < rend; reloc++) {
		Elf_Addr *where;
		where = (Elf_Addr *)(reloc->r_offset + object->obj_base);
		*where += object->obj_base;
	}

	return 0;
}
