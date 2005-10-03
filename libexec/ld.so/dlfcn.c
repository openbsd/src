/*	$OpenBSD: dlfcn.c,v 1.61 2005/10/03 21:01:37 kurt Exp $ */

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
#include <dlfcn.h>
#include <unistd.h>

#include "syscall.h"
#include "archdep.h"
#include "resolve.h"

int _dl_errno;

int _dl_real_close(void *handle);
void (*_dl_thread_fnc)(int) = NULL;
static elf_object_t *obj_from_addr(const void *addr);

void *
dlopen(const char *libname, int flags)
{
	elf_object_t *object, *dynobj;
	Elf_Dyn	*dynp;
	struct dep_node *n;
	int failed = 0;
	const char *deplibname = NULL;

	if (libname == NULL)
		return _dl_objects;

	DL_DEB(("dlopen: loading: %s\n", libname));

	_dl_thread_kern_stop();

	object = _dl_load_shlib(libname, _dl_objects, OBJTYPE_DLO, flags);
	if (object == 0) {
		DL_DEB(("dlopen: failed to open %s\n", libname));
		failed = 1;
		goto loaded;
	}

	_dl_loading_object = object;

	if (object->opencount == 0)
		_dl_link_dlopen(object);

	if (++object->refcount > 1)
		goto loaded;

	/* this add_object should not be here, XXX */
	_dl_add_object(object);

	DL_DEB(("head [%s]\n", object->load_name ));
	object->load_object = object;

	n = _dl_malloc(sizeof *n);
	if (n == NULL)
		_dl_exit(5);
	n->data = object;
	TAILQ_INSERT_TAIL(&object->dload_list, n, next_sib);


	/*
	 * Check for 'needed' objects. For each 'needed' object we
	 * create a 'shadow' object and add it to a list attached to
	 * the object so we know our dependencies. This list should
	 * also be used to determine the library search order when
	 * resolving undefined symbols. This is not yet done. XXX
	 */
	dynobj = object;
	while (dynobj) {
		elf_object_t *tmpobj = dynobj;

		for (dynp = dynobj->load_dyn; dynp->d_tag; dynp++) {
			elf_object_t *depobj;

			if (dynp->d_tag != DT_NEEDED)
				continue;

			deplibname = dynobj->dyn.strtab + dynp->d_un.d_val;
			DL_DEB(("dlopen: loading: %s required by %s\n",
			    deplibname, libname));
			depobj = _dl_load_shlib(deplibname, dynobj, OBJTYPE_LIB,
				flags);
			if (!depobj) {
				failed = 1;
				break;
			}
			/* this add_object should not be here, XXX */
			_dl_add_object(depobj);
			_dl_link_sub(depobj, dynobj);

			tmpobj->dep_next = _dl_malloc(sizeof(elf_object_t));
			tmpobj->dep_next->next = depobj;
			tmpobj = tmpobj->dep_next;
		}
		dynobj = dynobj->next;
	}

	if (failed == 1) {
		if (deplibname != NULL) {
			DL_DEB(("dlopen: failed to open %s\n", deplibname));
		}
		_dl_real_close(object);
		object = NULL;
		_dl_errno = DL_CANT_LOAD_OBJ;
	} else {
		int err;
		DL_DEB(("tail %s\n", object->load_name ));
		err = _dl_rtld(object);
		if (err != 0) {
			_dl_real_close(object);
			_dl_errno = DL_CANT_LOAD_OBJ;
			object = 0;
			failed = 1;
		} else {
			_dl_call_init(object);
		}
	}

loaded:
	if (failed == 0)
		object->opencount++;

	_dl_loading_object = NULL;

	if (_dl_debug_map->r_brk) {
		_dl_debug_map->r_state = RT_ADD;
		(*((void (*)(void))_dl_debug_map->r_brk))();
		_dl_debug_map->r_state = RT_CONSISTENT;
		(*((void (*)(void))_dl_debug_map->r_brk))();
	}

	_dl_thread_kern_go();

	DL_DEB(("dlopen: %s: done (%s).\n", libname,
	    failed ? "failed" : "success"));

	return((void *)object);
}

void *
dlsym(void *handle, const char *name)
{
	elf_object_t	*object;
	elf_object_t	*dynobj;
	const elf_object_t	*pobj;
	void		*retval;
	const Elf_Sym	*sym = NULL;
	int flags;

	if (handle == NULL || handle == RTLD_NEXT ||
	    handle == RTLD_SELF || handle == RTLD_DEFAULT) {
		void *retaddr;

		retaddr = __builtin_return_address(0);	/* __GNUC__ only */

		if ((object = obj_from_addr(retaddr)) == NULL) {
			_dl_errno = DL_CANT_FIND_OBJ;
			return(0);
		}

		if (handle == RTLD_NEXT)
			flags = SYM_SEARCH_NEXT|SYM_PLT;
		else if (handle == RTLD_SELF)
			flags = SYM_SEARCH_SELF|SYM_PLT;
		else if (handle == RTLD_DEFAULT)
			flags = SYM_SEARCH_ALL|SYM_PLT;
		else
			flags = SYM_DLSYM|SYM_PLT;

	} else {
		object = (elf_object_t *)handle;
		flags = SYM_DLSYM|SYM_PLT;

		dynobj = _dl_objects;
		while (dynobj && dynobj != object)
			dynobj = dynobj->next;

		if (!dynobj || object != dynobj) {
			_dl_errno = DL_INVALID_HANDLE;
			return(0);
		}
	}

	retval = (void *)_dl_find_symbol(name, &sym,
	    flags|SYM_NOWARNNOTFOUND, NULL, object, &pobj);

	if (sym != NULL) {
		retval += sym->st_value;
#ifdef __hppa__
		if (ELF_ST_TYPE(sym->st_info) == STT_FUNC)
			retval = (void *)_dl_md_plabel((Elf_Addr)retval,
			    pobj->dyn.pltgot);
#endif
		DL_DEB(("dlsym: %s in %s: %p\n",
		    name, object->load_name, retval));
	} else
		_dl_errno = DL_NO_SYMBOL;
	return (retval);
}

int
dlctl(void *handle, int command, void *data)
{
	int retval;

	switch (command) {
	case DL_SETTHREADLCK:
		DL_DEB(("dlctl: _dl_thread_fnc set to %p\n", data));
		_dl_thread_fnc = data;
		retval = 0;
		break;
	default:
		_dl_errno = DL_INVALID_CTL;
		retval = -1;
		break;
	}
	return (retval);
}

int
dlclose(void *handle)
{
	int retval;

	if (handle == _dl_objects)
		return 0;

	_dl_thread_kern_stop();

	retval = _dl_real_close(handle);

	_dl_thread_kern_go();

	if (_dl_debug_map->r_brk) {
		_dl_debug_map->r_state = RT_DELETE;
		(*((void (*)(void))_dl_debug_map->r_brk))();
		_dl_debug_map->r_state = RT_CONSISTENT;
		(*((void (*)(void))_dl_debug_map->r_brk))();
	}
	return (retval);
}

int
_dl_real_close(void *handle)
{
	elf_object_t	*object;
	elf_object_t	*dynobj;

	object = (elf_object_t *)handle;

	dynobj = _dl_objects;
	while (dynobj && dynobj != object)
		dynobj = dynobj->next;

	if (!dynobj || object != dynobj) {
		_dl_errno = DL_INVALID_HANDLE;
		return (1);
	}

	if (object->opencount == 0) {
		_dl_errno = DL_INVALID_HANDLE;
		return (1);
	}

	_dl_notify_unload_shlib(object);
	_dl_run_all_dtors();
	object->opencount--;
	_dl_unload_shlib(object);
	_dl_cleanup_objects();
	return (0);
}


/*
 * Return a character string describing the last dl... error occurred.
 */
const char *
dlerror(void)
{
	const char *errmsg;

	switch (_dl_errno) {
	case 0:	/* NO ERROR */
		errmsg = NULL;
		break;
	case DL_NOT_FOUND:
		errmsg = "File not found";
		break;
	case DL_CANT_OPEN:
		errmsg = "Can't open file";
		break;
	case DL_NOT_ELF:
		errmsg = "File not an ELF object";
		break;
	case DL_CANT_OPEN_REF:
		errmsg = "Can't open referenced object";
		break;
	case DL_CANT_MMAP:
		errmsg = "Can't map ELF object";
		break;
	case DL_INVALID_HANDLE:
		errmsg = "Invalid handle";
		break;
	case DL_NO_SYMBOL:
		errmsg = "Unable to resolve symbol";
		break;
	case DL_INVALID_CTL:
		errmsg = "Invalid dlctl() command";
		break;
	case DL_NO_OBJECT:
		errmsg = "No shared object contains address";
		break;
	case DL_CANT_FIND_OBJ:
		errmsg = "Cannot determine caller's shared object";
		break;
	case DL_CANT_LOAD_OBJ:
		errmsg = "Cannot load specified object";
		break;
	default:
		errmsg = "Unknown error";
	}

	_dl_errno = 0;
	return (errmsg);
}

void
_dl_show_objects(void)
{
	elf_object_t *object;
	char *objtypename;
	int outputfd;
	char *pad;

	object = _dl_objects;
	if (_dl_traceld)
		outputfd = STDOUT_FILENO;
	else
		outputfd = STDERR_FILENO;

	if (sizeof(long) == 8)
		pad = "        ";
	else
		pad = "";
	_dl_fdprintf(outputfd, "\tStart   %s End     %s Type Ref Name\n",
	    pad, pad);

	while (object) {
		switch (object->obj_type) {
		case OBJTYPE_LDR:
			objtypename = "rtld";
			break;
		case OBJTYPE_EXE:
			objtypename = "exe ";
			break;
		case OBJTYPE_LIB:
			objtypename = "rlib";
			break;
		case OBJTYPE_DLO:
			objtypename = "dlib";
			break;
		default:
			objtypename = "????";
			break;
		}
		_dl_fdprintf(outputfd, "\t%lX %lX %s  %d  %s\n",
		    (void *)object->load_addr,
		    (void *)(object->load_addr + object->load_size),
		    objtypename, object->refcount, object->load_name);
		object = object->next;
	}

	if (_dl_symcachestat_lookups != 0)
		DL_DEB(("symcache lookups %d hits %d ratio %d% hits\n",
		    _dl_symcachestat_lookups, _dl_symcachestat_hits,
		    (_dl_symcachestat_hits * 100) /
		    _dl_symcachestat_lookups));
}

void
_dl_thread_kern_stop(void)
{
	if (_dl_thread_fnc != NULL)
		(*_dl_thread_fnc)(0);
}

void
_dl_thread_kern_go(void)
{
	if (_dl_thread_fnc != NULL)
		(*_dl_thread_fnc)(1);
}

int
dl_iterate_phdr(int (*callback)(struct dl_phdr_info *, size_t, void *data),
	void *data)
{
	elf_object_t *object;
	Elf_Ehdr *ehdr;
	struct dl_phdr_info info;
	int retval = -1;

	for (object = _dl_objects; object != NULL; object = object->next) {
		ehdr = (Elf_Ehdr *)object->load_addr;
		if (object->phdrp == NULL && ehdr == NULL)
			continue;

		info.dlpi_addr = object->load_addr;
		info.dlpi_name = object->load_name;
		info.dlpi_phdr = object->phdrp;
		info.dlpi_phnum = object->phdrc;
		if (info.dlpi_phdr == NULL) {
		    info.dlpi_phdr = (Elf_Phdr *)
			((char *)object->load_addr + ehdr->e_phoff);
		    info.dlpi_phnum = ehdr->e_phnum;
		}
		retval = callback(&info, sizeof (struct dl_phdr_info), data);
		if (retval)
			break;
	}

	return retval;
}

static elf_object_t *
obj_from_addr(const void *addr)
{
	elf_object_t *dynobj;
	Elf_Ehdr *ehdr;
	Elf_Phdr *phdr;
	Elf_Addr start;
	Elf_Addr end;
	u_int32_t symoffset;
	const Elf_Sym *sym;
	int i;

	for (dynobj = _dl_objects; dynobj != NULL; dynobj = dynobj->next) {
		ehdr = (Elf_Ehdr *)dynobj->load_addr;
		if (ehdr == NULL)
			continue;

		phdr = (Elf_Phdr *)((char *)dynobj->load_addr + ehdr->e_phoff);

		for (i = 0; i < ehdr->e_phnum; i++) {
			switch (phdr[i].p_type) {
			case PT_LOAD:
				start = phdr[i].p_vaddr + dynobj->load_addr;
				if ((Elf_Addr)addr >= start &&
				    (Elf_Addr)addr < start + phdr[i].p_memsz)
					return dynobj;
				break;
			default:
				break;
			}
		}
	}

	/* find the lowest & highest symbol address in the main exe */
	start = -1;
	end = 0;

	for (symoffset = 0; symoffset < _dl_objects->nchains; symoffset++) {
		sym = _dl_objects->dyn.symtab + symoffset;

		/*
		 * For skip the symbol if st_shndx is either SHN_UNDEF or
		 * SHN_COMMON.
		 */
		if (sym->st_shndx == SHN_UNDEF || sym->st_shndx == SHN_COMMON)
			continue;

		if (sym->st_value < start)
			start = sym->st_value;

		if (sym->st_value > end)
			end = sym->st_value;
	}

	if (end && (Elf_Addr) addr >= start && (Elf_Addr) addr <= end)
		return _dl_objects;
	else
		return NULL;
}

int
dladdr(const void *addr, Dl_info *info)
{
	const elf_object_t *object;
	const Elf_Sym *sym;
	void *symbol_addr;
	u_int32_t symoffset;

	object = obj_from_addr(addr);

	if (object == NULL) {
		_dl_errno = DL_NO_OBJECT;
		return 0;
	}

	info->dli_fname = (char *)object->load_name;
	info->dli_fbase = (void *)object->load_addr;
	info->dli_sname = NULL;
	info->dli_saddr = (void *)0;

	/*
	 * Walk the symbol list looking for the symbol whose address is
	 * closest to the address sent in.
	 */
	for (symoffset = 0; symoffset < object->nchains; symoffset++) {
		sym = object->dyn.symtab + symoffset;

		/*
		 * For skip the symbol if st_shndx is either SHN_UNDEF or
		 * SHN_COMMON.
		 */
		if (sym->st_shndx == SHN_UNDEF || sym->st_shndx == SHN_COMMON)
			continue;

		/*
		 * If the symbol is greater than the specified address, or if
		 * it is further away from addr than the current nearest
		 * symbol, then reject it.
		 */
		symbol_addr = (void *)(object->load_addr + sym->st_value);
		if (symbol_addr > addr || symbol_addr < info->dli_saddr)
			continue;

		/* Update our idea of the nearest symbol. */
		info->dli_sname = object->dyn.strtab + sym->st_name;
		info->dli_saddr = symbol_addr;

		/* Exact match? */
		if (info->dli_saddr == addr)
			break;
	}

	return 1;
}
