/*	$OpenBSD: dlfcn.c,v 1.83 2011/05/22 22:43:47 drahn Exp $ */

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
#include "sod.h"

int _dl_errno;
int _dl_tracelib;

int _dl_real_close(void *handle);
void (*_dl_thread_fnc)(int) = NULL;
void (*_dl_bind_lock_f)(int) = NULL;
static elf_object_t *obj_from_addr(const void *addr);

void *
dlopen(const char *libname, int flags)
{
	elf_object_t *object;
	int failed = 0;

	if (libname == NULL)
		return RTLD_DEFAULT;

	if ((flags & RTLD_TRACE) == RTLD_TRACE) {
		_dl_traceld = "true";
		_dl_tracelib = 1;
	}

	DL_DEB(("dlopen: loading: %s\n", libname));

	_dl_thread_kern_stop();

	if (_dl_debug_map->r_brk) {
		_dl_debug_map->r_state = RT_ADD;
		(*((void (*)(void))_dl_debug_map->r_brk))();
	}

	_dl_loading_object = NULL;

	object = _dl_load_shlib(libname, _dl_objects, OBJTYPE_DLO, flags);
	if (object == 0) {
		DL_DEB(("dlopen: failed to open %s\n", libname));
		failed = 1;
		goto loaded;
	}

	_dl_link_dlopen(object);

	if (OBJECT_REF_CNT(object) > 1) {
		/* if opened but grpsym_list has not been created */
		if (OBJECT_DLREF_CNT(object) == 1) {
			/* add first object manually */
			_dl_link_grpsym(object, 1);
			_dl_cache_grpsym_list(object);
		}
		goto loaded;
	}

	/* this add_object should not be here, XXX */
	_dl_add_object(object);

	DL_DEB(("head [%s]\n", object->load_name ));

	if ((failed = _dl_load_dep_libs(object, flags, 0)) == 1) {
		_dl_real_close(object);
		object = NULL;
		_dl_errno = DL_CANT_LOAD_OBJ;
	} else {
		int err;
		DL_DEB(("tail %s\n", object->load_name ));
		if (_dl_traceld) {
			_dl_show_objects();
			_dl_unload_shlib(object);
			_dl_exit(0);
		}
		_dl_search_list_valid = 0;
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
	_dl_loading_object = NULL;

	if (_dl_debug_map->r_brk) {
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
	case DL_SETBINDLCK:
		DL_DEB(("dlctl: _dl_bind_lock_f set to %p\n", data));
		_dl_bind_lock_f = data;
		retval = 0;
		break;
	case 0x20:
		_dl_show_objects();
		retval = 0;
		break;
	case 0x21:
	{
		struct dep_node *n, *m;
		elf_object_t *obj;
		_dl_printf("Load Groups:\n");

		TAILQ_FOREACH(n, &_dlopened_child_list, next_sib) {
			obj = n->data;
			_dl_printf("%s\n", obj->load_name);

			_dl_printf("  children\n");
			TAILQ_FOREACH(m, &obj->child_list, next_sib)
				_dl_printf("\t[%s]\n", m->data->load_name);

			_dl_printf("  grpref\n");
			TAILQ_FOREACH(m, &obj->grpref_list, next_sib)
				_dl_printf("\t[%s]\n", m->data->load_name);
			_dl_printf("\n");
		}
		retval = 0;
		break;
	}
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

	if (handle == RTLD_DEFAULT)
		return 0;

	_dl_thread_kern_stop();

	if (_dl_debug_map->r_brk) {
		_dl_debug_map->r_state = RT_DELETE;
		(*((void (*)(void))_dl_debug_map->r_brk))();
	}

	retval = _dl_real_close(handle);

	_dl_search_list_valid = 0;

	if (_dl_debug_map->r_brk) {
		_dl_debug_map->r_state = RT_CONSISTENT;
		(*((void (*)(void))_dl_debug_map->r_brk))();
	}
	_dl_thread_kern_go();
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

	object->opencount--;
	_dl_notify_unload_shlib(object);
	_dl_run_all_dtors();
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
_dl_tracefmt(int fd, elf_object_t *object, const char *fmt1, const char *fmt2,
    const char *objtypename)
{
	const char *fmt;
	struct sod sd;
	int i;
	char *s;

	s = _dl_strrchr(object->load_name, '/');
	if (s != NULL)
		s++;
	else
		s = object->load_name;
	_dl_build_sod(s, &sd);
	fmt = sd.sod_library ? fmt1 : fmt2;
	
	for (i = 0; fmt[i]; i++) {
		if (fmt[i] != '%' && fmt[i] != '\\') {
			_dl_fdprintf(fd, "%c", fmt[i]);
			continue;
		}
		if (fmt[i] == '%') {
			i++;
			switch (fmt[i]) {
			case '\0':
				return;
			case '%':
				_dl_fdprintf(fd, "%c", '%');
				break;
			case 'A':
				_dl_fdprintf(fd, "%s", _dl_traceprog ?
				    _dl_traceprog : "");
				break;
			case 'a':
				_dl_fdprintf(fd, "%s", _dl_progname);
				break;
			case 'e':
				_dl_fdprintf(fd, "%lX",
				    (void *)(object->load_base +
				    object->load_size));
				break;
			case 'g':
				_dl_fdprintf(fd, "%d", object->grprefcount);
				break;
			case 'm':
				_dl_fdprintf(fd, "%d", sd.sod_major);
				break;
			case 'n':
				_dl_fdprintf(fd, "%d", sd.sod_minor);
				break;
			case 'O':
				_dl_fdprintf(fd, "%d", object->opencount);
				break;
			case 'o':
				_dl_fdprintf(fd, "%s", sd.sod_name);
				break;
			case 'p':
				_dl_fdprintf(fd, "%s", object->load_name);
				break;
			case 'r':
				_dl_fdprintf(fd, "%d", object->refcount);
				break;
			case 't':
				_dl_fdprintf(fd, "%s", objtypename);
				break;
			case 'x':
				_dl_fdprintf(fd, "%lX", object->load_base);
				break;
			}
		}
		if (fmt[i] == '\\') {
			i++;
			switch (fmt[i]) {
			case '\0':
				return;
			case 'n':
				_dl_fdprintf(fd, "%c", '\n');
				break;
			case 'r':
				_dl_fdprintf(fd, "%c", '\r');
				break;
			case 't':
				_dl_fdprintf(fd, "%c", '\t');
				break;
			default:
				_dl_fdprintf(fd, "%c", fmt[i]);
				break;
			}
		}
	}
	_dl_free((void *)sd.sod_name);
}

void
_dl_show_objects(void)
{
	elf_object_t *object;
	char *objtypename;
	int outputfd;
	char *pad;
	const char *fmt1, *fmt2;

	object = _dl_objects;
	if (_dl_traceld)
		outputfd = STDOUT_FILENO;
	else
		outputfd = STDERR_FILENO;

	if (sizeof(long) == 8)
		pad = "        ";
	else
		pad = "";

	fmt1 = _dl_tracefmt1 ? _dl_tracefmt1 :
	    "\t%x %e %t %O    %r   %g      %p\n";
	fmt2 = _dl_tracefmt2 ? _dl_tracefmt2 :
	    "\t%x %e %t %O    %r   %g      %p\n";

	if (_dl_tracefmt1 == NULL && _dl_tracefmt2 == NULL)
		_dl_fdprintf(outputfd, "\tStart   %s End     %s Type Open Ref GrpRef Name\n",
		    pad, pad);

	if (_dl_tracelib) {
		for (; object != NULL; object = object->next)
			if (object->obj_type == OBJTYPE_LDR) {
				object = object->next;
				break;
			}
	}

	for (; object != NULL; object = object->next) {
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
		_dl_tracefmt(outputfd, object, fmt1, fmt2, objtypename);
	}

	if (_dl_symcachestat_lookups != 0)
		DL_DEB(("symcache lookups %d hits %d ratio %d% hits\n",
		    _dl_symcachestat_lookups, _dl_symcachestat_hits,
		    (_dl_symcachestat_hits * 100) /
		    _dl_symcachestat_lookups));
}

void
_dl_thread_bind_lock(int what, sigset_t *omask)
{
	if (! what) {
		sigset_t nmask;

		sigfillset(&nmask);
		_dl_sigprocmask(SIG_BLOCK, &nmask, omask);
	}
	if (_dl_bind_lock_f != NULL)
		(*_dl_bind_lock_f)(what);
	if (what)
		_dl_sigprocmask(SIG_SETMASK, omask, NULL);
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
	struct dl_phdr_info info;
	int retval = -1;

	for (object = _dl_objects; object != NULL; object = object->next) {
		if (object->phdrp == NULL)
			continue;

		info.dlpi_addr = object->obj_base;
		info.dlpi_name = object->load_name;
		info.dlpi_phdr = object->phdrp;
		info.dlpi_phnum = object->phdrc;
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
	Elf_Phdr *phdrp;
	int phdrc;
	Elf_Addr start;
	int i;

	for (dynobj = _dl_objects; dynobj != NULL; dynobj = dynobj->next) {
		if (dynobj->phdrp == NULL)
			continue;

		phdrp = dynobj->phdrp;
		phdrc = dynobj->phdrc;

		for (i = 0; i < phdrc; i++, phdrp++) {
			if (phdrp->p_type == PT_LOAD) {
				start = dynobj->obj_base + phdrp->p_vaddr;
				if ((Elf_Addr)addr >= start &&
				    (Elf_Addr)addr < start + phdrp->p_memsz)
					return dynobj;
			}
		}
	}

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
	info->dli_fbase = (void *)object->load_base;
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
		symbol_addr = (void *)(object->obj_base + sym->st_value);
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
