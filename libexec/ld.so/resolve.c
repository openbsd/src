/*	$OpenBSD: resolve.c,v 1.1.1.1 2000/06/13 03:34:07 rahnds Exp $ */

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

#include <link.h>
#include "resolve.h"
#include "syscall.h"
#include "archdep.h"

elf_object_t *_dl_objects;
elf_object_t *_dl_last_object;


/*
 *	Initialize and add a new dynamic object to the object list.
 *	Perform necessary relocations of pointers.
 */

elf_object_t *
_dl_add_object(const char *objname, Elf32_Dyn *dynp,
			     const u_int32_t *dl_data, const int objtype,
			     const int laddr, const int loff)
{
	elf_object_t *object;

	object = (elf_object_t *)_dl_malloc(sizeof(elf_object_t));

	if(_dl_objects == 0) {	/* First object ? */
		_dl_last_object = _dl_objects = object;
	}
	else {
		_dl_last_object->next = object;
		object->prev = _dl_last_object;
		_dl_last_object = object;
	}

	object->load_dyn = dynp;
	while(dynp->d_tag != DT_NULL) {
		if(dynp->d_tag < DT_PROCNUM) {
			object->Dyn.info[dynp->d_tag] = dynp->d_un.d_val;
		}
		else if(dynp->d_tag >= DT_LOPROC && dynp->d_tag < DT_LOPROC + DT_PROCNUM) {
			object->Dyn.info[dynp->d_tag + DT_NUM - DT_LOPROC] = dynp->
d_un.d_val;
		}
		if(dynp->d_tag == DT_TEXTREL)
			object->dyn.textrel = 1;
		if(dynp->d_tag == DT_SYMBOLIC)
			object->dyn.symbolic = 1;
		if(dynp->d_tag == DT_BIND_NOW)
			object->dyn.bind_now = 1;
		dynp++;
	}

	/*
	 *  Now relocate all pointer to dynamic info, but only
	 *  the ones which has pointer values.
	 */
	if(object->Dyn.info[DT_PLTGOT])
		object->Dyn.info[DT_PLTGOT] += loff;
	if(object->Dyn.info[DT_HASH])
		object->Dyn.info[DT_HASH] += loff;
	if(object->Dyn.info[DT_STRTAB])
		object->Dyn.info[DT_STRTAB] += loff;
	if(object->Dyn.info[DT_SYMTAB])
		object->Dyn.info[DT_SYMTAB] += loff;
	if(object->Dyn.info[DT_RELA])
		object->Dyn.info[DT_RELA] += loff;
	if(object->Dyn.info[DT_SONAME])
		object->Dyn.info[DT_SONAME] += loff;
	if(object->Dyn.info[DT_RPATH])
		object->Dyn.info[DT_RPATH] += loff;
	if(object->Dyn.info[DT_REL])
		object->Dyn.info[DT_REL] += loff;
	if(object->Dyn.info[DT_INIT])
		object->Dyn.info[DT_INIT] += loff;
	if(object->Dyn.info[DT_FINI])
		object->Dyn.info[DT_FINI] += loff;

	object->buckets = object->dyn.hash;
	if(object->buckets != 0) {
		object->nbuckets = *object->buckets++;
		object->nchains  = *object->buckets++;
		object->chains   = object->buckets + object->nbuckets;
	}

	if(dl_data) {
		object->phdrp = (Elf32_Phdr *) dl_data[AUX_phdr];
		object->phdrc = dl_data[AUX_phnum];
	}
	object->obj_type = objtype;
	object->load_addr = laddr;
	object->load_offs = loff;
	object->load_name = (char *)_dl_malloc(_dl_strlen(objname) + 1);
	_dl_strcpy(object->load_name, objname);
	object->refcount = 1;

	return(object);
}


void
_dl_remove_object(elf_object_t *object)
{
	elf_object_t *depobj;

	object->prev->next = object->next;
	if(object->next) {
		object->next->prev = object->prev;
	}
	if(object->load_name) {
		_dl_free(object->load_name);
	}
	while((depobj = object->dep_next)) {
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
	while(object) {
		if(_dl_strcmp(name, object->load_name) == 0) {
			return(object);
		}
		object = object->next;
	}
	return(0);
}


Elf32_Addr
_dl_find_symbol(const char *name, elf_object_t *startlook,
			const Elf32_Sym **ref, int myself)
{
	u_int32_t h = 0;
	const char *p = name;
	elf_object_t *object;
	const Elf32_Sym *weak_sym = 0;
	Elf32_Addr weak_offs = 0;

	while(*p) {
		u_int32_t g;
		h = (h << 4) + *p++;
		if((g = h & 0xf0000000)) {
			h ^= g >> 24;
		}
		h &= ~g;
	}

	for(object = startlook; object; object = (myself ? 0 : object->next)) {
		const Elf32_Sym *symt;
		const char	*strt;
		u_int32_t	si;

		symt = object->dyn.symtab;
		strt = object->dyn.strtab;

		for(si = object->buckets[h % object->nbuckets];
			si != STN_UNDEF; si = object->chains[si]) {
			const Elf32_Sym *sym = symt + si;

			if(sym->st_value == 0 ||
			   sym->st_shndx == SHN_UNDEF) {
				continue;
			}

			if(ELF32_ST_TYPE(sym->st_info) != STT_NOTYPE &&
			   ELF32_ST_TYPE(sym->st_info) != STT_OBJECT &&
			   ELF32_ST_TYPE(sym->st_info) != STT_FUNC) {
				continue;
			}

			if(sym != *ref && _dl_strcmp(strt+sym->st_name, name)) {
				continue;
			}

			if(ELF32_ST_BIND(sym->st_info) == STB_GLOBAL) {
				*ref = sym;
				return(object->load_offs);
			}
			else if(ELF32_ST_BIND(sym->st_info) == STB_WEAK) {
				if(!weak_sym) {
					weak_sym = sym;
					weak_offs = object->load_offs;
				}
			}
		}
	}
	if(!weak_sym && *ref && ELF32_ST_BIND((*ref)->st_info) != STB_WEAK) {
		_dl_printf("%s: undefined symbol '%s'\n", _dl_progname, name);
	}
	*ref = weak_sym;
	return(weak_offs);
}

