/*	$OpenBSD: library.c,v 1.22 2002/08/08 17:17:12 art Exp $ */

/*
 * Copyright (c) 2002 Dale Rahn
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
#include <sys/syslimits.h>
#include <sys/param.h>
#include <fcntl.h>
#include <nlist.h>
#include <link.h>
#include <sys/mman.h>
#include <dirent.h>

#include "syscall.h"
#include "archdep.h"
#include "resolve.h"
#include "dir.h"
#include "sod.h"

#define DEFAULT_PATH "/usr/lib"

#define PFLAGS(X) ((((X) & PF_R) ? PROT_READ : 0) | \
		   (((X) & PF_W) ? PROT_WRITE : 0) | \
		   (((X) & PF_X) ? PROT_EXEC : 0))

static elf_object_t *_dl_tryload_shlib(const char *libname, int type);

/*
 * _dl_match_file()
 *
 * This fucntion determines if a given name matches what is specified
 * in a struct sod. The major must match exactly, and the minor must
 * be same or larger.
 *
 * sodp is updated with the minor if this matches.
 */

int
_dl_match_file(struct sod *sodp, char *name, int namelen)
{
	int match;
	struct sod lsod;
	char *lname;

	lname = name;
	if (sodp->sod_library) {
		if (_dl_strncmp(name, "lib", 3)) {
			return 0;
		}
		lname += 3;
	}
	if (_dl_strncmp(lname, (char *)sodp->sod_name,
	    _dl_strlen((char *)sodp->sod_name))) {
		return 0;
	}

	_dl_build_sod(name, &lsod);

	match = 0;
	if ((_dl_strcmp((char *)lsod.sod_name, (char *)sodp->sod_name) == 0) &&
	    (lsod.sod_library == sodp->sod_library) &&
	    (sodp->sod_major == lsod.sod_major) &&
	    ((sodp->sod_minor == -1) ||
	    (lsod.sod_minor >= sodp->sod_minor))) {
		match = 1;

		/* return version matched */
		sodp->sod_minor = lsod.sod_minor;
	}
	_dl_free((char *)lsod.sod_name);

	return match;
}

char _dl_hint_store[MAXPATHLEN];

char *
_dl_find_shlib(struct sod *sodp, const char *searchpath, int nohints)
{
	int len;
	char *hint;
	char lp[PATH_MAX + 10];
	char *path;
	const char *pp;
	DIR *dd;
	struct dirent *dp;
	int match;

	/* if we are to search default directories, and hints
	 * are not to be used, search the standard path from ldconfig
	 * (_dl_hint_search_path) or use the default path
	 */
	if (nohints)
		goto nohints;

	if (searchpath == NULL) {
		/* search 'standard' locations, find any match in the hints */
		hint = _dl_findhint((char *)sodp->sod_name, sodp->sod_major,
		    sodp->sod_minor, NULL);
		if (hint)
			return hint;
	} else {
		/* search hints requesting matches for only
		 * the searchpath directories,
		 */
		pp = searchpath;
		while (pp) {
			path = lp;
			while (path < lp + PATH_MAX &&
			    *pp && *pp != ':' && *pp != ';')
				*path++ = *pp++;
			*path = 0;

			hint = _dl_findhint((char *)sodp->sod_name,
			    sodp->sod_major, sodp->sod_minor, lp);
			if (hint != NULL)
				return hint;

			if (*pp)	/* Try curdir if ':' at end */
				pp++;
			else
				pp = 0;
		}
	}
	/*
	 * For each directory in the searchpath, read the directory
	 * entries looking for a match to sod. filename compare is
	 * done by _dl_match_file()
	 */
nohints:
	if (searchpath == NULL) {
		if (_dl_hint_search_path != NULL)
			searchpath = _dl_hint_search_path;
		else
			searchpath = DEFAULT_PATH;
	}
	pp = searchpath;
	while (pp) {
		path = lp;
		while (path < lp + PATH_MAX && *pp && *pp != ':' && *pp != ';')
			*path++ = *pp++;
		*path = 0;

		if ((dd = _dl_opendir(lp)) != NULL) {
			match = 0;
			while ((dp = _dl_readdir(dd)) != NULL) {
				if (_dl_match_file(sodp, dp->d_name,
				    dp->d_namlen)) {
					/* When a match is found, sodp is
					 * updated with the minor found.
					 * We continue looking at this
					 * directory, thus this will find
					 * the largest matching library
					 * in this directory.
					 * we save off the d_name now
					 * so that it doesn't have to be
					 * recreated from the hint.
					 */

					match = 1;
					len = _dl_strlcpy(_dl_hint_store, lp,
					    MAXPATHLEN);
					if (lp[len-1] != '/') {
						_dl_hint_store[len] = '/';
						len++;
					}
					_dl_strlcpy(&_dl_hint_store[len],
						dp->d_name, MAXPATHLEN-len);
				}
			}
			_dl_closedir(dd);
			if (match)
				return (_dl_hint_store);
		}

		if (*pp)	/* Try curdir if ':' at end */
			pp++;
		else
			pp = 0;
	}
	return NULL;
}

/*
 *  Load a shared object. Search order is:
 *	If the name contains a '/' use the name exactly as is. (only)
 *	try the LD_LIBRARY_PATH specification (if present)
 *	   search hints for match in LD_LIBRARY_PATH dirs
 *           this will only match specific libary version.
 *	   search LD_LIBRARY_PATH dirs for match.
 *           this will find largest minor version in first dir found.
 *	check DT_RPATH paths, (if present)
 *	   search hints for match in DT_RPATH dirs
 *           this will only match specific libary version.
 *	   search DT_RPATH dirs for match.
 *           this will find largest minor version in first dir found.
 *	last look in default search directory, either as specified
 *      by ldconfig or default to '/usr/lib'
 */

elf_object_t *
_dl_load_shlib(const char *libname, elf_object_t *parent, int type)
{
	elf_object_t *object;
	struct sod sod;
	struct sod req_sod;
	char *hint;
	int try_any_minor;
	int ignore_hints;

	try_any_minor = 0;
	ignore_hints = 0;

	if (_dl_strchr(libname, '/')) {
		object = _dl_tryload_shlib(libname, type);
		return(object);
	}


	_dl_build_sod(libname, &sod);
	req_sod = sod;

again:
	/*
	 *  No '/' in name. Scan the known places, LD_LIBRARY_PATH first.
	 */
	if (_dl_libpath != NULL) {
		hint = _dl_find_shlib(&req_sod, _dl_libpath, ignore_hints);
		if (hint != NULL) {
			if (req_sod.sod_minor < sod.sod_minor)
				_dl_printf("warning: lib%s.so.%d.%d: "
				    "minor version >= %d expected, "
				    "using it anyway",
				    sod.sod_name, sod.sod_major,
				    sod.sod_minor, req_sod.sod_minor);
			object = _dl_tryload_shlib(hint, type);
			if (object != NULL) {
				_dl_free((char *)sod.sod_name);
				return (object);
			}
		}
	}

	/*
	 *  Check DT_RPATH.
	 */
	if (parent->dyn.rpath != NULL) {
		hint = _dl_find_shlib(&req_sod, parent->dyn.rpath,
		    ignore_hints);
		if (hint != NULL) {
			if (req_sod.sod_minor < sod.sod_minor)
				_dl_printf("warning: lib%s.so.%d.%d: "
				    "minor version >= %d expected, "
				    "using it anyway",
				    sod.sod_name, sod.sod_major,
				    sod.sod_minor, req_sod.sod_minor);
			object = _dl_tryload_shlib(hint, type);
			if (object != NULL) {
				_dl_free((char *)sod.sod_name);
				return (object);
			}
		}
	}

	/* check 'standard' locations */
	hint = _dl_find_shlib(&req_sod, NULL, ignore_hints);
	if (hint != NULL) {
		if (req_sod.sod_minor < sod.sod_minor)
			_dl_printf("warning: lib%s.so.%d.%d: "
			    "minor version >= %d expected, "
			    "using it anyway",
			    sod.sod_name, sod.sod_major,
			    sod.sod_minor, req_sod.sod_minor);
		object = _dl_tryload_shlib(hint, type);
		if (object != NULL) {
			_dl_free((char *)sod.sod_name);
			return(object);
		}
	}

	if (try_any_minor == 0) {
		try_any_minor = 1;
		ignore_hints = 1;
		req_sod.sod_minor = -1;
		goto again;
	}
	_dl_free((char *)sod.sod_name);
	_dl_errno = DL_NOT_FOUND;
	return(0);
}

void
_dl_load_list_free(struct load_list *load_list)
{
	struct load_list *next;

	while (load_list != NULL) {
		next = load_list->next;
		_dl_free(load_list);
		load_list = next;
	}
}

void _dl_run_dtors(elf_object_t *object);

void
_dl_unload_shlib(elf_object_t *object)
{
	if (--object->refcount == 0) {
		_dl_run_dtors(object);
		_dl_load_list_free(object->load_list);
		_dl_munmap((void *)object->load_addr, object->load_size);
		_dl_remove_object(object);
	}
}


static elf_object_t *
_dl_tryload_shlib(const char *libname, int type)
{
	int	libfile, i, align = _dl_pagesz - 1;
	struct load_list *next_load, *load_list = NULL;
	Elf_Addr maxva = 0, minva = 0x7fffffff;	/* XXX Correct for 64bit? */
	Elf_Addr libaddr, loff;
	elf_object_t *object;
	char	hbuf[4096];
	Elf_Dyn *dynp = 0;
	Elf_Ehdr *ehdr;
	Elf_Phdr *phdp;

#define ROUND_PG(x) (((x) + align) & ~(align))
#define TRUNC_PG(x) ((x) & ~(align))

	object = _dl_lookup_object(libname);
	if (object) {
		object->refcount++;
		return(object);		/* Already loaded */
	}

	libfile = _dl_open(libname, O_RDONLY);
	if (libfile < 0) {
		_dl_errno = DL_CANT_OPEN;
		return(0);
	}

	_dl_read(libfile, hbuf, sizeof(hbuf));
	ehdr = (Elf_Ehdr *)hbuf;
	if (_dl_strncmp(ehdr->e_ident, ELFMAG, SELFMAG) ||
	    ehdr->e_type != ET_DYN || ehdr->e_machine != MACHID) {
		_dl_close(libfile);
		_dl_errno = DL_NOT_ELF;
		return(0);
	}

	/*
	 *  Alright, we might have a winner!
	 *  Figure out how much VM space we need.
	 */
	phdp = (Elf_Phdr *)(hbuf + ehdr->e_phoff);
	for (i = 0; i < ehdr->e_phnum; i++, phdp++) {
		switch (phdp->p_type) {
		case PT_LOAD:
			if (phdp->p_vaddr < minva)
				minva = phdp->p_vaddr;
			if (phdp->p_vaddr + phdp->p_memsz > maxva)
				maxva = phdp->p_vaddr + phdp->p_memsz;
			break;
		case PT_DYNAMIC:
			dynp = (Elf_Dyn *)phdp->p_vaddr;
			break;
		default:
			break;
		}
	}
	minva = TRUNC_PG(minva);
	maxva = ROUND_PG(maxva);

	/*
	 * We map the entire area to see that we can get the VM
	 * space required. Map it unaccessible to start with.
	 *
	 * We must map the file we'll map later otherwise the VM
	 * system won't be able to align the mapping properly
	 * on VAC architectures.
	 */
	libaddr = (Elf_Addr)_dl_mmap(0, maxva - minva, PROT_NONE,
	    MAP_PRIVATE|MAP_FILE, libfile, 0);
	if (_dl_check_error(libaddr)) {
		_dl_printf("%s: rtld mmap failed mapping %s.\n",
		    _dl_progname, libname);
		_dl_close(libfile);
		_dl_errno = DL_CANT_MMAP;
		return(0);
	}

	loff = libaddr - minva;
	phdp = (Elf_Phdr *)(hbuf + ehdr->e_phoff);

	for (i = 0; i < ehdr->e_phnum; i++, phdp++) {
		if (phdp->p_type == PT_LOAD) {
			char *start = (char *)(TRUNC_PG(phdp->p_vaddr)) + loff;
			int off = (phdp->p_vaddr & align);
			int size = off + phdp->p_filesz;
			void *res;

			res = _dl_mmap(start, ROUND_PG(size),
			    PFLAGS(phdp->p_flags),
			    MAP_FIXED|MAP_PRIVATE, libfile,
			    TRUNC_PG(phdp->p_offset));
			next_load = _dl_malloc(sizeof(struct load_list));
			next_load->next = load_list;
			load_list = next_load;
			next_load->start = start;
			next_load->size = size;
			next_load->prot = PFLAGS(phdp->p_flags);
			if (_dl_check_error((long)res)) {
				_dl_printf("%s: rtld mmap failed mapping %s.\n",
				    _dl_progname, libname);
				_dl_close(libfile);
				_dl_errno = DL_CANT_MMAP;
				_dl_munmap((void *)libaddr, maxva - minva);
				_dl_load_list_free(load_list);
				return(0);
			}
			if (phdp->p_flags & PF_W) {
				/* Zero out everything past the EOF */
				if ((size & align) != 0)
					_dl_memset(start + size, 0,
					    _dl_pagesz - (size & align));
				if (ROUND_PG(size) ==
				    ROUND_PG(off + phdp->p_memsz))
					continue;
				start = start + ROUND_PG(size);
				size = ROUND_PG(off + phdp->p_memsz) -
				    ROUND_PG(size);
				res = _dl_mmap(start, size,
				    PFLAGS(phdp->p_flags),
				    MAP_FIXED|MAP_PRIVATE|MAP_ANON, -1, 0);
				if (_dl_check_error((long)res)) {
					_dl_printf("%s: rtld mmap failed mapping %s.\n",
					    _dl_progname, libname);
					_dl_close(libfile);
					_dl_errno = DL_CANT_MMAP;
					_dl_munmap((void *)libaddr, maxva - minva);
					_dl_load_list_free(load_list);
					return(0);
				}
			}
		}
	}
	_dl_close(libfile);

	dynp = (Elf_Dyn *)((unsigned long)dynp + loff);
	object = _dl_add_object(libname, dynp, 0, type, libaddr, loff);
	if (object) {
		object->load_size = maxva - minva;	/*XXX*/
		object->load_list = load_list;
	} else {
		_dl_munmap((void *)libaddr, maxva - minva);
		_dl_load_list_free(load_list);
	}
	return(object);
}
