/*	$OpenBSD: library_mquery.c,v 1.13 2003/09/02 15:17:51 drahn Exp $ */

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

elf_object_t *_dl_tryload_shlib(const char *libname, int type);

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
		if (_dl_strncmp(name, "lib", 3))
			return 0;
		lname += 3;
	}
	if (_dl_strncmp(lname, (char *)sodp->sod_name,
	    _dl_strlen((char *)sodp->sod_name)))
		return 0;

	_dl_build_sod(name, &lsod);

	match = 0;
	if ((_dl_strcmp((char *)lsod.sod_name, (char *)sodp->sod_name) == 0) &&
	    (lsod.sod_library == sodp->sod_library) &&
	    ((sodp->sod_major == -1) || (sodp->sod_major == lsod.sod_major)) &&
	    ((sodp->sod_minor == -1) ||
	    (lsod.sod_minor >= sodp->sod_minor))) {
		match = 1;

		/* return version matched */
		sodp->sod_major = lsod.sod_major;
		sodp->sod_minor = lsod.sod_minor;
	}
	_dl_free((char *)lsod.sod_name);
	return match;
}

char _dl_hint_store[MAXPATHLEN];

char *
_dl_find_shlib(struct sod *sodp, const char *searchpath, int nohints)
{
	char *hint, lp[PATH_MAX + 10], *path;
	struct dirent *dp;
	const char *pp;
	int match, len;
	DIR *dd;
	struct sod tsod, bsod;		/* transient and best sod */

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
				tsod = *sodp;
				if (_dl_match_file(&tsod, dp->d_name,
				    dp->d_namlen)) {
					/*
					 * When a match is found, tsod is
					 * updated with the major+minor found.
					 * This version is compared with the
					 * largest so far (kept in bsod),
					 * and saved if larger.
					 */
					if (!match ||
					    tsod.sod_major == -1 ||
					    tsod.sod_major > bsod.sod_major ||
					    ((tsod.sod_major ==
					    bsod.sod_major) &&
					    tsod.sod_minor > bsod.sod_minor)) {
						bsod = tsod;
						match = 1;
						len = _dl_strlcpy(
						    _dl_hint_store, lp,
						    MAXPATHLEN);
						if (lp[len-1] != '/') {
							_dl_hint_store[len] =
							    '/';
							len++;
						}
						_dl_strlcpy(
						    &_dl_hint_store[len],
						    dp->d_name,
						    MAXPATHLEN-len);
						if (tsod.sod_major == -1)
							break;
					}
				}
			}
			_dl_closedir(dd);
			if (match) {
				*sodp = bsod;
				return (_dl_hint_store);
			}
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
_dl_load_shlib(const char *libname, elf_object_t *parent, int type, int flags)
{
	int try_any_minor, ignore_hints;
	struct sod sod, req_sod;
	elf_object_t *object;
	char *hint;

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
				    "using it anyway\n",
				    sod.sod_name, sod.sod_major,
				    req_sod.sod_minor, sod.sod_minor);
			object = _dl_tryload_shlib(hint, type);
			if (object != NULL) {
				_dl_free((char *)sod.sod_name);
				object->obj_flags = flags;
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
				    "using it anyway\n",
				    sod.sod_name, sod.sod_major,
				    req_sod.sod_minor, sod.sod_minor);
			object = _dl_tryload_shlib(hint, type);
			if (object != NULL) {
				_dl_free((char *)sod.sod_name);
				object->obj_flags = flags;
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
			    "using it anyway\n",
			    sod.sod_name, sod.sod_major,
			    req_sod.sod_minor, sod.sod_minor);
		object = _dl_tryload_shlib(hint, type);
		if (object != NULL) {
			_dl_free((char *)sod.sod_name);
			object->obj_flags = flags;
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
	int align = _dl_pagesz - 1;

	while (load_list != NULL) {
		if (load_list->start != NULL)
			_dl_munmap(load_list->start,
			    ((load_list->size) + align) & ~align);
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
		_dl_remove_object(object);
	}
}


elf_object_t *
_dl_tryload_shlib(const char *libname, int type)
{
	int	libfile, i, align = _dl_pagesz - 1;
	struct load_list *ld, *lowld = NULL;
	elf_object_t *object;
	char	hbuf[4096];
	Elf_Dyn *dynp = 0;
	Elf_Ehdr *ehdr;
	Elf_Phdr *phdp;
	int off;
	int size;
	Elf_Addr load_end = 0;

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
	if (ehdr->e_ident[0] != ELFMAG0  || ehdr->e_ident[1] != ELFMAG1 ||
	    ehdr->e_ident[2] != ELFMAG2 || ehdr->e_ident[3] != ELFMAG3 ||
	    ehdr->e_type != ET_DYN || ehdr->e_machine != MACHID) {
		_dl_close(libfile);
		_dl_errno = DL_NOT_ELF;
		return(0);
	}

	/* Insertion sort */
#define LDLIST_INSERT(ld) do { \
	struct load_list **_ld; \
	for (_ld = &lowld; *_ld != NULL; _ld = &(*_ld)->next) \
		if ((*_ld)->moff > ld->moff) \
			break; \
	ld->next = *_ld; \
	*_ld = ld; \
} while (0)
	/*
	 *  Alright, we might have a winner!
	 *  Figure out how much VM space we need and set up the load
	 *  list that we'll use to find free VM space.
	 */
	phdp = (Elf_Phdr *)(hbuf + ehdr->e_phoff);
	for (i = 0; i < ehdr->e_phnum; i++, phdp++) {
		switch (phdp->p_type) {
		case PT_LOAD:
			off = (phdp->p_vaddr & align);
			size = off + phdp->p_filesz;

			ld = _dl_malloc(sizeof(struct load_list));
			ld->start = NULL;
			ld->size = size;
			ld->moff = TRUNC_PG(phdp->p_vaddr);
			ld->foff = TRUNC_PG(phdp->p_offset);
			ld->prot = PFLAGS(phdp->p_flags);
			LDLIST_INSERT(ld);

			if ((ld->prot & PROT_WRITE) == 0 ||
			    ROUND_PG(size) == ROUND_PG(off + phdp->p_memsz))
				break;
			/* This phdr has a zfod section */
			ld = _dl_malloc(sizeof(struct load_list));
			ld->start = NULL;
			ld->size = ROUND_PG(off + phdp->p_memsz) -
			    ROUND_PG(size);
			ld->moff = TRUNC_PG(phdp->p_vaddr) +
			    ROUND_PG(size);
			ld->foff = -1;
			ld->prot = PFLAGS(phdp->p_flags);
			LDLIST_INSERT(ld);
			break;
		case PT_DYNAMIC:
			dynp = (Elf_Dyn *)phdp->p_vaddr;
			break;
		default:
			break;
		}
	}

#define LOFF ((Elf_Addr)lowld->start - lowld->moff)

retry:
	for (ld = lowld; ld != NULL; ld = ld->next) {
		off_t foff;
		int fd, flags;

		/*
		 * We don't want to provide the fd/off hint for anything
		 * but the first mapping, all other might have
		 * cache-incoherent aliases and will cause this code to
		 * loop forever.
		 */
		if (ld == lowld) {
			fd = libfile;
			foff = ld->foff;
			flags = 0;
		} else {
			fd = -1;
			foff = 0;
			flags = MAP_FIXED;
		}

		ld->start = (void *)(LOFF + ld->moff);

		/*
		 * Magic here.
		 * The first mquery is done with MAP_FIXED to see if
		 * the mapping we want is free. If it's not, we redo the
		 * mquery without MAP_FIXED to get the next free mapping,
		 * adjust the base mapping address to match this free mapping
		 * and restart the process again.
		 */
		ld->start = _dl_mquery(ld->start, ROUND_PG(ld->size), ld->prot,
		    flags, fd, foff);
		if (_dl_check_error(ld->start)) {
			ld->start = (void *)(LOFF + ld->moff);
			ld->start = _dl_mquery(ld->start, ROUND_PG(ld->size),
			    ld->prot, flags & ~MAP_FIXED, fd, foff);
			if (_dl_check_error(ld->start))
				goto fail;
		}

		if (ld->start != (void *)(LOFF + ld->moff)) {
			lowld->start = ld->start - ld->moff + lowld->moff;
			goto retry;
		}
		/*
		 * XXX - we need some kind of boundary condition here,
		 * or fix mquery to not run into the stack
		 */
	}

	for (ld = lowld; ld != NULL; ld = ld->next) {
		off_t foff;
		int fd, flags;
		void *res;

		if (ld->foff < 0) {
			fd = -1;
			foff = 0;
			flags = MAP_FIXED|MAP_PRIVATE|MAP_ANON;
		} else {
			fd = libfile;
			foff = ld->foff;
			flags = MAP_FIXED|MAP_PRIVATE;
		}
		res = _dl_mmap(ld->start, ROUND_PG(ld->size), ld->prot, flags,
		    fd, foff);
		if (_dl_check_error((long)res))
			goto fail;
		/* Zero out everything past the EOF */
		if ((ld->prot & PROT_WRITE) != 0 && (ld->size & align) != 0)
			_dl_memset((char *)ld->start + ld->size, 0,
			    _dl_pagesz - (ld->size & align));
		load_end = (Elf_Addr)ld->start + ROUND_PG(ld->size);
	}
	_dl_close(libfile);

	dynp = (Elf_Dyn *)((unsigned long)dynp + LOFF);
	object = _dl_finalize_object(libname, dynp, 0, type,
	    (Elf_Addr)lowld->start, LOFF);
	if (object) {
		object->load_size = (Elf_Addr)load_end - (Elf_Addr)lowld->start;
		object->load_list = lowld;
	} else {
		_dl_load_list_free(lowld);
	}
	return(object);
fail:
	_dl_printf("%s: rtld mmap failed mapping %s.\n",
	    _dl_progname, libname);
	_dl_close(libfile);
	_dl_errno = DL_CANT_MMAP;
	_dl_load_list_free(lowld);
	return(0);
}

void
_dl_link_sub(elf_object_t *dep, elf_object_t *p)
{
	struct dep_node *n;

	n = _dl_malloc(sizeof *n);
	if (n == NULL)
		_dl_exit(5);
	n->data = dep;
	n->next_sibling = NULL;
	if (p->first_child) {
		p->last_child->next_sibling = n;
		p->last_child = n;
	} else
		p->first_child = p->last_child = n;

	DL_DEB(("linking dep %s as child of %s\n", dep->load_name,
	    p->load_name));
}


