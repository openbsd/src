/*	$OpenBSD: rtld_machine.c,v 1.3 2001/05/31 13:49:27 art Exp $ */

/*
 * Copyright (c) 1999 Dale Rahn
 * Copyright (c) 2001 Niklas Hallqvist
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
 *	This product includes software developed under OpenBSD by
 *	Dale Rahn.
 * 4. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission.
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
#include <sys/cdefs.h>
#include <sys/mman.h>

#include <machine/elf_machdep.h>

#include <nlist.h>
#include <link.h>

#include "syscall.h"
#include "archdep.h"
#include "resolve.h"

void
_dl_bcopy(void *src, void *dest, int size)
{
	unsigned char *psrc, *pdest;
	int i;
	psrc = src;
	pdest = dest;
	for (i = 0; i < size; i++) {
		pdest[i] = psrc[i];
	}
}

int
_dl_md_reloc(elf_object_t *object, int rel, int relasz)
{
	long	i;
	long	numrela;
	long	fails = 0;
	Elf64_Addr loff;
	Elf64_Rela  *relas;
	load_list_t *llist;

	loff   = object->load_offs;
	numrela = object->Dyn.info[relasz] / sizeof(Elf64_Rela);
	relas = (Elf64_Rela *)(object->Dyn.info[rel]);

	if ((object->status & STAT_RELOC_DONE) || !relas) {
		return(0);
	}

	/*
	 * unprotect some segments if we need it.
	 * XXX - we unprotect waay to much. only the text can have cow
	 *       relocations.
	 */
	if ((rel == DT_REL || rel == DT_RELA)) {
		for (llist = object->load_list; llist != NULL; llist = llist->next) {
			if (!(llist->prot & PROT_WRITE)) {
				_dl_mprotect(llist->start, llist->size,
					llist->prot|PROT_WRITE);
			}
		}
	}

	for (i = 0; i < numrela; i++, relas++) {
		Elf64_Addr *r_addr;
		Elf64_Addr ooff;
		const Elf64_Sym *sym, *this;
		const char *symn;

		r_addr = (Elf64_Addr *)(relas->r_offset + loff);

		if (ELF64_R_SYM(relas->r_info) == 0xffffffff) {
			continue;
		}

		sym = object->dyn.symtab;
		sym += ELF64_R_SYM(relas->r_info);
		this = sym;
		symn = object->dyn.strtab + sym->st_name;

		switch (ELF64_R_TYPE(relas->r_info)) {
		case R_TYPE(REFQUAD):
			ooff = _dl_find_symbol(symn, _dl_objects, &this, 0, 1);
			if (this == NULL)
				goto resolve_failed;
			*r_addr += ooff + this->st_value + relas->r_addend;
			break;
		case R_TYPE(RELATIVE):
#if 0
			if ((caddr_t)r_addr < (caddr_t)_GLOBAL_OFFSET_TABLE_ ||
			    (caddr_t)r_addr >= (caddr_t)&_DYNAMIC)
#endif
				*r_addr += loff;
			break;
		case R_TYPE(JMP_SLOT):
			ooff = _dl_find_symbol(symn, _dl_objects, &this, 0, 1);
			if (this == NULL)
				goto resolve_failed;
			*r_addr = ooff + this->st_value;
			break;
		case R_TYPE(GLOB_DAT):
			ooff = _dl_find_symbol(symn, _dl_objects, &this, 0, 1);
			if (this == NULL)
				goto resolve_failed;
			*r_addr = ooff + this->st_value;
			break;
		case R_TYPE(COPY):
_dl_printf("copy relocation, not tested\n");
			ooff = _dl_find_symbol(symn, _dl_objects, &this, 0, 1);
			if (this == NULL)
				goto resolve_failed;
#ifdef DL_PRINTF_DEBUG
			_dl_printf("copy r_addr %lx, sym %x [%s] size %d val %lx\n",
				r_addr, sym, symn, sym->st_size,
				(ooff + this->st_value+
				relas->r_addend)

				);
#endif
{
	/* we need to find a symbol, that is not in the current object,
	 * start looking at the beginning of the list, searching all objects
	 * but _not_ the current object, first one found wins.
	 */
	elf_object_t *cobj;
	const Elf64_Sym *cpysrc = NULL;
	Elf64_Addr src_loff = 0;
	int size;
	for (cobj = _dl_objects;
		cobj != NULL && cpysrc == NULL;
		cobj = cobj->next)
	{
		if (object != cobj) {

			/* only look in this object */
			src_loff = _dl_find_symbol(symn, cobj,
				&cpysrc, 1, 1);
		}
	}
	if (cpysrc == NULL) {
		_dl_printf("symbol not found [%s] \n", symn);
	} else {
		size  = sym->st_size;
		if (sym->st_size != cpysrc->st_size) {
			_dl_printf("symbols size differ [%s] \n", symn);
			size = sym->st_size < cpysrc->st_size ?
				sym->st_size : cpysrc->st_size;
		}
#ifdef DL_PRINTF_DEBUG
_dl_printf(" found other symbol at %x size %d\n", 
		src_loff + cpysrc->st_value,  cpysrc->st_size);
#endif
		_dl_bcopy((void *)(src_loff + cpysrc->st_value),
			(void *)(ooff + this->st_value+ relas->r_addend),
			size);
	}
}
			break;
		case R_TYPE(NONE):
			break;

		default:
			_dl_printf("%s:"
				" %s: unsupported relocation '%s' %d at %lx\n",
					_dl_progname, object->load_name, symn,
					ELF64_R_TYPE(relas->r_info), r_addr );
			_dl_exit(1);
		}
		continue;
resolve_failed:
		_dl_printf("%s: %s :can't resolve reference '%s'\n",
			_dl_progname, object->load_name, symn);
		fails++;
	}
	__asm __volatile("imb" : : : "memory");

	/* reprotect the unprotected segments */
	if ((rel == DT_REL || rel == DT_RELA)) {
		for (llist = object->load_list; llist != NULL; llist = llist->next) {
			if (!(llist->prot & PROT_WRITE)) {
				_dl_mprotect(llist->start, llist->size,
					llist->prot);
			}
		}
	}

	return (fails);
}

/*
 *	Relocate the Global Offset Table (GOT). Currently we don't
 *	do lazy evaluation here because the GNU linker doesn't
 *	follow the ABI spec which says that if an external symbol
 *	is referenced by other relocations than CALL16 and 26 it
 *	should not be given a stub and have a zero value in the
 *	symbol table. By not doing so, we can't use pointers to
 *	external functions and use them in comparitions...
 */
void
_dl_md_reloc_got(elf_object_t *object, int lazy)
{
	/* relocations all done via rela relocations above */
}
