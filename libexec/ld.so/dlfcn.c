/*	$OpenBSD: dlfcn.c,v 1.37 2004/06/07 15:00:38 mickey Exp $ */

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

static int _dl_real_close(void *handle);
static void _dl_unload_deps(elf_object_t *object);
static void _dl_thread_kern_stop(void);
static void _dl_thread_kern_go(void);
static void (*_dl_thread_fnc)(int) = NULL;

void *
dlopen(const char *libname, int flags)
{
	elf_object_t *object, *dynobj;
	Elf_Dyn	*dynp;

	if (libname == NULL)
		return _dl_objects;

	DL_DEB(("dlopen: loading: %s\n", libname));

	_dl_thread_kern_stop();
	object = _dl_load_shlib(libname, _dl_objects, OBJTYPE_DLO, flags);
	/* this add_object should not be here, XXX */
	if (object == 0) {
		_dl_thread_kern_go();
		return((void *)0);
	}
	_dl_add_object(object);
	_dl_link_sub(object, _dl_objects);
	_dl_thread_kern_go();

	if (object->refcount > 1)
		return((void *)object);	/* Already loaded */

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
			const char *libname;
			elf_object_t *depobj;

			if (dynp->d_tag != DT_NEEDED)
				continue;

			libname = dynobj->dyn.strtab + dynp->d_un.d_val;
			_dl_thread_kern_stop();
			depobj = _dl_load_shlib(libname, dynobj, OBJTYPE_LIB,
				flags|RTLD_GLOBAL);
			if (!depobj)
				_dl_exit(4);
			/* this add_object should not be here, XXX */
			_dl_add_object(depobj);
			_dl_link_sub(depobj, dynobj);
			_dl_thread_kern_go();

			tmpobj->dep_next = _dl_malloc(sizeof(elf_object_t));
			tmpobj->dep_next->next = depobj;
			tmpobj = tmpobj->dep_next;
		}
		dynobj = dynobj->next;
	}

	_dl_rtld(object);
	_dl_call_init(object);

	if (_dl_debug_map->r_brk) {
		_dl_debug_map->r_state = RT_ADD;
		(*((void (*)(void))_dl_debug_map->r_brk))();
		_dl_debug_map->r_state = RT_CONSISTENT;
		(*((void (*)(void))_dl_debug_map->r_brk))();
	}

	DL_DEB(("dlopen: %s: done.\n", libname));

	return((void *)object);
}

void *
dlsym(void *handle, const char *name)
{
	elf_object_t	*object;
	elf_object_t	*dynobj;
	void		*retval;
	const Elf_Sym	*sym = NULL;

	object = (elf_object_t *)handle;
	dynobj = _dl_objects;
	while (dynobj && dynobj != object)
		dynobj = dynobj->next;

	if (!dynobj || object != dynobj) {
		_dl_errno = DL_INVALID_HANDLE;
		return(0);
	}

	retval = (void *)_dl_find_symbol(name, object, &sym, NULL,
	    SYM_SEARCH_SELF|SYM_NOWARNNOTFOUND|SYM_NOTPLT, 0, object);
	if (sym != NULL)
		retval += sym->st_value;
	else
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

	retval = _dl_real_close(handle);

	if (_dl_debug_map->r_brk) {
		_dl_debug_map->r_state = RT_DELETE;
		(*((void (*)(void))_dl_debug_map->r_brk))();
		_dl_debug_map->r_state = RT_CONSISTENT;
		(*((void (*)(void))_dl_debug_map->r_brk))();
	}
	return (retval);
}

static int
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

	if (object->refcount == 1) {
		if (dynobj->dep_next)
			_dl_unload_deps(dynobj);
	}

	_dl_unload_shlib(object);
	return (0);
}

/*
 * Scan through the shadow dep list and 'unload' every library
 * we depend upon. Shadow objects are removed when removing ourself.
 */
static void
_dl_unload_deps(elf_object_t *object)
{
	elf_object_t *depobj;

	depobj = object->dep_next;
	while (depobj) {
		if (depobj->next->refcount == 1) { /* This object will go away */
			if (depobj->next->dep_next)
				_dl_unload_deps(depobj->next);
			_dl_unload_shlib(depobj->next);
		}
		depobj = depobj->dep_next;
	}
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
	default:
		errmsg = "Unknown error";
	}

	_dl_errno = 0;
	return (errmsg);
}

void
_dl_show_objects(void)
{
	extern int _dl_symcachestat_hits;
	extern int _dl_symcachestat_lookups;
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

static void
_dl_thread_kern_stop(void)
{
	if (_dl_thread_fnc != NULL)
		(*_dl_thread_fnc)(0);
}

static void
_dl_thread_kern_go(void)
{
	if (_dl_thread_fnc != NULL)
		(*_dl_thread_fnc)(1);
}

