/*	$OpenBSD: resolve.c,v 1.17 2003/02/02 16:57:58 deraadt Exp $ */

/*
 * Copyright (c) 1998 Per Fogelstrom, Opsycon AB
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
 *	Per Fogelstrom, Opsycon AB, Sweden.
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

#include <nlist.h>
#include <link.h>
#include "syscall.h"
#include "archdep.h"
#include "resolve.h"

elf_object_t *_dl_objects;
elf_object_t *_dl_last_object;

/*
 * Initialize and add a new dynamic object to the object list.
 * Perform necessary relocations of pointers.
 */
elf_object_t *
_dl_add_object(const char *objname, Elf_Dyn *dynp, const u_long *dl_data,
    const int objtype, const long laddr, const long loff)
{
	elf_object_t *object;
#if 0
	_dl_printf("objname [%s], dynp %p, dl_data %p, objtype %x laddr %lx, loff %lx\n",
	    objname, dynp, dl_data, objtype, laddr, loff);
#endif

	object = _dl_malloc(sizeof(elf_object_t));

	object->next = NULL;
	if (_dl_objects == 0) {			/* First object ? */
		_dl_last_object = _dl_objects = object;
	} else {
		_dl_last_object->next = object;
		object->prev = _dl_last_object;
		_dl_last_object = object;
	}

	object->load_dyn = dynp;
	while (dynp->d_tag != DT_NULL) {
		if (dynp->d_tag < DT_NUM)
			object->Dyn.info[dynp->d_tag] = dynp->d_un.d_val;
		else if (dynp->d_tag >= DT_LOPROC &&
		    dynp->d_tag < DT_LOPROC + DT_NUM)
			object->Dyn.info[dynp->d_tag + DT_NUM - DT_LOPROC] =
			    dynp->d_un.d_val;
		if (dynp->d_tag == DT_TEXTREL)
			object->dyn.textrel = 1;
		if (dynp->d_tag == DT_SYMBOLIC)
			object->dyn.symbolic = 1;
		if (dynp->d_tag == DT_BIND_NOW)
			object->dyn.bind_now = 1;
		dynp++;
	}

	/*
	 *  Now relocate all pointer to dynamic info, but only
	 *  the ones which have pointer values.
	 */
	if (object->Dyn.info[DT_PLTGOT])
		object->Dyn.info[DT_PLTGOT] += loff;
	if (object->Dyn.info[DT_HASH])
		object->Dyn.info[DT_HASH] += loff;
	if (object->Dyn.info[DT_STRTAB])
		object->Dyn.info[DT_STRTAB] += loff;
	if (object->Dyn.info[DT_SYMTAB])
		object->Dyn.info[DT_SYMTAB] += loff;
	if (object->Dyn.info[DT_RELA])
		object->Dyn.info[DT_RELA] += loff;
	if (object->Dyn.info[DT_SONAME])
		object->Dyn.info[DT_SONAME] += loff;
	if (object->Dyn.info[DT_RPATH])
		object->Dyn.info[DT_RPATH] += object->Dyn.info[DT_STRTAB];
	if (object->Dyn.info[DT_REL])
		object->Dyn.info[DT_REL] += loff;
	if (object->Dyn.info[DT_INIT])
		object->Dyn.info[DT_INIT] += loff;
	if (object->Dyn.info[DT_FINI])
		object->Dyn.info[DT_FINI] += loff;
	if (object->Dyn.info[DT_JMPREL])
		object->Dyn.info[DT_JMPREL] += loff;

	if (object->Dyn.info[DT_HASH] != 0) {
		Elf_Word *hashtab = (Elf_Word *)object->Dyn.info[DT_HASH];

		object->nbuckets = hashtab[0];
		object->nchains = hashtab[1];
		object->buckets = hashtab + 2;
		object->chains = object->buckets + object->nbuckets;
	}

	if (dl_data) {
		object->phdrp = (Elf_Phdr *) dl_data[AUX_phdr];
		object->phdrc = dl_data[AUX_phnum];
	}
	object->obj_type = objtype;
	object->load_addr = laddr;
	object->load_offs = loff;
	object->load_name = _dl_strdup(objname);
	object->refcount = 1;
	object->first_child = NULL;
	object->last_child = NULL;
	return(object);
}

void
_dl_remove_object(elf_object_t *object)
{
	elf_object_t *depobj;

	object->prev->next = object->next;
	if (object->next)
		object->next->prev = object->prev;

	if (_dl_last_object == object)
		_dl_last_object = object->prev;

	if (object->load_name)
		_dl_free(object->load_name);

	while ((depobj = object->dep_next)) {
		object->dep_next = object->dep_next->dep_next;
		_dl_free(depobj);
	}
	_dl_free(object);
}


elf_object_t *
_dl_lookup_object(const char *name)
{
	elf_object_t *object;

	object = _dl_objects;
	while (object) {
		if (_dl_strcmp(name, object->load_name) == 0)
			return(object);
		object = object->next;
	}
	return(0);
}


Elf_Addr
_dl_find_symbol(const char *name, elf_object_t *startlook,
    const Elf_Sym **ref, int flags, int req_size, const char *module_name)
{
	const Elf_Sym *weak_sym = NULL;
	const char *weak_symn = NULL; /* remove warning */
	Elf_Addr weak_offs = 0;
	unsigned long h = 0;
	const char *p = name;
	elf_object_t *object, *weak_object = NULL;

	while (*p) {
		unsigned long g;
		h = (h << 4) + *p++;
		if ((g = h & 0xf0000000))
			h ^= g >> 24;
		h &= ~g;
	}

	for (object = startlook; object;
	    object = ((flags & SYM_SEARCH_SELF) ? 0 : object->next)) {
		const Elf_Sym	*symt = object->dyn.symtab;
		const char	*strt = object->dyn.strtab;
		long	si;
		const char *symn;

		for (si = object->buckets[h % object->nbuckets];
		    si != STN_UNDEF; si = object->chains[si]) {
			const Elf_Sym *sym = symt + si;

			if (sym->st_value == 0)
				continue;

			if (ELF_ST_TYPE(sym->st_info) != STT_NOTYPE &&
			    ELF_ST_TYPE(sym->st_info) != STT_OBJECT &&
			    ELF_ST_TYPE(sym->st_info) != STT_FUNC)
				continue;

			symn = strt + sym->st_name;
			if (sym != *ref && _dl_strcmp(symn, name))
				continue;

			/* allow this symbol if we are referring to a function
			 * which has a value, even if section is UNDEF.
			 * this allows &func to refer to PLT as per the
			 * ELF spec. st_value is checked above.
			 * if flags has SYM_PLT set, we must have actual
			 * symbol, so this symbol is skipped.
			 */
			if (sym->st_shndx == SHN_UNDEF) {
				if ((flags & SYM_PLT) || sym->st_value == 0 ||
				    ELF_ST_TYPE(sym->st_info) != STT_FUNC)
					continue;
			}

			if (ELF_ST_BIND(sym->st_info) == STB_GLOBAL) {
				*ref = sym;
				if (req_size != sym->st_size &&
				    req_size != 0 &&
				    (ELF_ST_TYPE(sym->st_info) != STT_FUNC)) {
					_dl_printf("%s: %s : WARNING: "
					    "symbol(%s) size mismatch ",
					    _dl_progname, object->load_name,
					    symn);
					_dl_printf("relink your program\n");
				}
				return(object->load_offs);
			} else if (ELF_ST_BIND(sym->st_info) == STB_WEAK) {
				if (!weak_sym) {
					weak_sym = sym;
					weak_symn = symn;
					weak_offs = object->load_offs;
					weak_object = object;
				}
			}
		}
	}
	if (flags & SYM_WARNNOTFOUND && weak_sym == NULL) {
		_dl_printf("%s:%s: undefined symbol '%s'\n",
		    _dl_progname, module_name, name);
	}
	*ref = weak_sym;
	if (weak_sym != NULL && req_size != weak_sym->st_size &&
	    req_size != 0 && (ELF_ST_TYPE(weak_sym->st_info) != STT_FUNC)) {
		_dl_printf("%s:%s: %s : WARNING: "
		    "symbol(%s) size mismatch ",
		    _dl_progname, module_name, weak_object->load_name,
		    weak_symn);
		_dl_printf("relink your program\n");
	}
	return (weak_offs);
}
