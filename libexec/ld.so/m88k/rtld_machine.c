/*	$OpenBSD: rtld_machine.c,v 1.31 2022/01/08 06:49:42 guenther Exp $	*/

/*
 * Copyright (c) 2013 Miodrag Vallat.
 *
 * Permission to use, copy, modify, and distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 * ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
 * OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */
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

int	_dl_cacheflush(unsigned long, size_t);
Elf_Addr _dl_bind(elf_object_t *object, int reloff);
void	_dl_md_reloc_gotp_ent(Elf_Addr, Elf_Addr, Elf_Addr);

int64_t pcookie __attribute__((section(".openbsd.randomdata"))) __dso_hidden;

int
_dl_md_reloc(elf_object_t *object, int rel, int relasz)
{
	int	i;
	int	numrela;
	int	relrela;
	int	fails = 0;
	Elf_Addr loff;
	Elf_RelA  *relas;
	Elf_Addr prev_value = 0, prev_ooff = 0;
	const Elf_Sym *prev_sym = NULL;

	loff = object->obj_base;
	numrela = object->Dyn.info[relasz] / sizeof(Elf_RelA);
	relrela = rel == DT_RELA ? object->relacount : 0;

	relas = (Elf_RelA *)(object->Dyn.info[rel]);

	if (relas == NULL)
		return 0;

	if (relrela > numrela)
		_dl_die("relacount > numrel: %d > %d", relrela, numrela);

	/* tight loop for leading RELATIVE relocs */
	for (i = 0; i < relrela; i++, relas++) {
		Elf_Addr *r_addr;

		r_addr = (Elf_Addr *)(relas->r_offset + loff);
		*r_addr = relas->r_addend + loff;
	}
	for (; i < numrela; i++, relas++) {
		Elf_Addr *r_addr = (Elf_Addr *)(relas->r_offset + loff);
		Elf_Addr addend, newval;
		const Elf_Sym *sym;
		const char *symn;
		int type;

		type = ELF_R_TYPE(relas->r_info);

		if (type == RELOC_GOTP_ENT && rel != DT_JMPREL)
			continue;

		if (type == RELOC_NONE)
			continue;

		sym = object->dyn.symtab;
		sym += ELF_R_SYM(relas->r_info);
		symn = object->dyn.strtab + sym->st_name;

		if (type == RELOC_COPY) {
			/*
			 * we need to find a symbol, that is not in the current
			 * object, start looking at the beginning of the list,
			 * searching all objects but _not_ the current object,
			 * first one found wins.
			 */
			struct sym_res sr;

			sr = _dl_find_symbol(symn,
			    SYM_SEARCH_OTHER | SYM_WARNNOTFOUND | SYM_NOTPLT,
			    sym, object);
			if (sr.sym != NULL) {
				_dl_bcopy((void *)(sr.obj->obj_base +
				    sr.sym->st_value), r_addr, sym->st_size);
			} else
				fails++;

			continue;
		}

		if (ELF_R_SYM(relas->r_info) &&
		    !(ELF_ST_BIND(sym->st_info) == STB_LOCAL &&
		    ELF_ST_TYPE (sym->st_info) == STT_NOTYPE) &&
		    sym != prev_sym) {
			if (ELF_ST_BIND(sym->st_info) == STB_LOCAL &&
			    ELF_ST_TYPE(sym->st_info) == STT_SECTION) {
				prev_sym = sym;
				prev_value = 0;
				prev_ooff = object->obj_base;
			} else {
				struct sym_res sr;

				sr = _dl_find_symbol(symn,
				    SYM_SEARCH_ALL | SYM_WARNNOTFOUND |
				    ((type == RELOC_GOTP_ENT) ?
				    SYM_PLT : SYM_NOTPLT), sym, object);

				if (sr.sym == NULL) {
					if (ELF_ST_BIND(sym->st_info) !=
					    STB_WEAK)
						fails++;
					continue;
				}
				prev_sym = sym;
				prev_value = sr.sym->st_value;
				prev_ooff = sr.obj->obj_base;
			}
		}

		if (type == RELOC_GOTP_ENT) {
			_dl_md_reloc_gotp_ent((Elf_Addr)r_addr,
			    relas->r_addend + loff,
			    prev_ooff + prev_value);
			continue;
		}

		if (ELF_ST_BIND(sym->st_info) == STB_LOCAL &&
		    (ELF_ST_TYPE(sym->st_info) == STT_SECTION ||
		    ELF_ST_TYPE(sym->st_info) == STT_NOTYPE))
			addend = relas->r_addend;
		else
			addend = prev_value + relas->r_addend;

		switch (type) {
		case RELOC_16L:
			newval = prev_ooff + addend;
			*(unsigned short *)r_addr = newval & 0xffff;
			_dl_cacheflush((unsigned long)r_addr, 2);
			break;
		case RELOC_16H:
			newval = prev_ooff + addend;
			*(unsigned short *)r_addr = newval >> 16;
			_dl_cacheflush((unsigned long)r_addr, 2);
			break;
		case RELOC_DISP26:
			newval = prev_ooff + addend;
			newval -= (Elf_Addr)r_addr;
			if ((newval >> 28) != 0 && (newval >> 28) != 0x0f)
				_dl_die("%s: out of range DISP26"
				    " relocation to '%s' at %p\n",
				    object->load_name, symn, (void *)r_addr);
			*r_addr = (*r_addr & 0xfc000000) |
			    (((int32_t)newval >> 2) & 0x03ffffff);
			_dl_cacheflush((unsigned long)r_addr, 4);
			break;
		case RELOC_32:
			newval = prev_ooff + addend;
			*r_addr = newval;
			break;
		case RELOC_BBASED_32:
			newval = loff + addend;
			*r_addr = newval;
			break;
		default:
			_dl_die("%s: unsupported relocation '%s' %d at %p\n",
			    object->load_name, symn, type, (void *)r_addr);
		}
	}

	return fails;
}

/*
 * GOTP_ENT relocations are special in that they define both a .got and a
 * .plt relocation.
 */
void
_dl_md_reloc_gotp_ent(Elf_Addr got_addr, Elf_Addr plt_addr, Elf_Addr val)
{
	uint16_t *plt_entry = (uint16_t *)plt_addr;

	/* .got update */
	*(Elf_Addr *)got_addr = val;
	/* .plt update */
	plt_entry[1] = got_addr >> 16;
	plt_entry[3] = got_addr & 0xffff;
}

/*
 *	Relocate the Global Offset Table (GOT).
 *	This is done by calling _dl_md_reloc on DT_JMPREL for DL_BIND_NOW,
 *	otherwise the lazy binding plt operation is preserved.
 */
int
_dl_md_reloc_got(elf_object_t *object, int lazy)
{
	extern void _dl_bind_start(void);	/* XXX */
	int	fails = 0;
	Elf_Addr *pltgot = (Elf_Addr *)object->Dyn.info[DT_PLTGOT];
	Elf_Addr plt_start, plt_end;

	if (pltgot == NULL)
		return 0;

	pltgot[1] = (Elf_Addr)object;
	pltgot[2] = (Elf_Addr)_dl_bind_start;

	if (object->Dyn.info[DT_PLTREL] != DT_RELA)
		return 0;

	if (!lazy) {
		fails = _dl_md_reloc(object, DT_JMPREL, DT_PLTRELSZ);
	} else {
		if (object->obj_base != 0) {
			int cnt;
			Elf_Addr *addr;
			Elf_RelA *rela;

			cnt = object->Dyn.info[DT_PLTRELSZ] / sizeof(Elf_RelA);
			rela = (Elf_RelA *)object->Dyn.info[DT_JMPREL];

			for (; cnt != 0; cnt--, rela++) {
				addr = (Elf_Addr *)(object->obj_base +
				    rela->r_offset);
				_dl_md_reloc_gotp_ent((Elf_Addr)addr,
				    object->obj_base + rela->r_addend,
				    *addr + object->obj_base);
			}
		}
	}

	/*
	 * Force a cache sync here on the whole PLT if we updated it
	 * (and have the DT entries to find what we need to flush),
	 * otherwise I$ might have stale information.
	 */
	plt_start = object->Dyn.info[DT_88K_PLTSTART - DT_LOPROC + DT_NUM];
	plt_end = object->Dyn.info[DT_88K_PLTEND - DT_LOPROC + DT_NUM];
	if ((!lazy || object->obj_base != 0) && plt_start != 0 &&
	    plt_end != 0) {
		size_t plt_size = plt_end - plt_start;
		if (plt_size != 0)
			_dl_cacheflush(plt_start + object->obj_base, plt_size);
	}

	return fails;
}

Elf_Addr
_dl_bind(elf_object_t *object, int reloff)
{
	Elf_RelA *rel;
	struct sym_res sr;
	const Elf_Sym *sym;
	const char *symn;
	uint64_t cookie = pcookie;
	struct {
		struct __kbind param;
		Elf_Addr newval;
	} buf;

	rel = (Elf_RelA *)(object->Dyn.info[DT_JMPREL] + reloff);

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

	buf.param.kb_addr = (Elf_Addr *)(object->obj_base + rel->r_offset);
	buf.param.kb_size = sizeof(Elf_Addr);

	/* directly code the syscall, so that it's actually inline here */
	{
		register long syscall_num __asm("r13") = SYS_kbind;
		register void *arg1 __asm("r2") = &buf;
		register long  arg2 __asm("r3") = sizeof(buf);
		register long  arg3 __asm("r4") = 0xffffffff & (cookie >> 32);
		register long  arg4 __asm("r5") = 0xffffffff &  cookie;

		__asm volatile("tb0 0, %%r0, 450; or %%r0, %%r0, %%r0"
		    : "+r" (arg1), "+r" (arg2) : "r" (syscall_num),
		    "r" (arg3), "r" (arg4) : "memory");
	}

	return buf.newval;
}
