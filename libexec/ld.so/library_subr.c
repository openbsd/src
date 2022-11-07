/*	$OpenBSD: library_subr.c,v 1.53 2022/11/07 10:35:26 deraadt Exp $ */

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
#include <sys/queue.h>
#include <limits.h>
#include <dirent.h>

#include "util.h"
#include "resolve.h"
#include "dir.h"
#include "sod.h"

char * _dl_default_path[2] = { "/usr/lib", NULL };


/* STATIC DATA */
struct dlochld _dlopened_child_list;


/*
 * _dl_match_file()
 *
 * This function determines if a given name matches what is specified
 * in a struct sod. The major must match exactly, and the minor must
 * be same or larger.
 *
 * sodp is updated with the minor if this matches.
 */

int
_dl_match_file(struct sod *sodp, const char *name, int namelen)
{
	int match;
	struct sod lsod;
	const char *lname;

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

/*
 * _dl_cmp_sod()
 *
 * This function compares sod structs. The major must match exactly,
 * and the minor must be same or larger.
 *
 * sodp is updated with the minor if this matches.
 */

static int
_dl_cmp_sod(struct sod *sodp, const struct sod *lsod)
{
	int match;

	match = 1;
	if ((_dl_strcmp((char *)lsod->sod_name, (char *)sodp->sod_name) == 0) &&
	    (lsod->sod_library == sodp->sod_library) &&
	    ((sodp->sod_major == -1) || (sodp->sod_major == lsod->sod_major)) &&
	    ((sodp->sod_minor == -1) ||
	    (lsod->sod_minor >= sodp->sod_minor))) {
		match = 0;

		/* return version matched */
		sodp->sod_major = lsod->sod_major;
		sodp->sod_minor = lsod->sod_minor;
	}
	return match;
}

char _dl_hint_store[PATH_MAX];

char *
_dl_find_shlib(struct sod *sodp, char **searchpath, int nohints)
{
	char *hint, **pp;
	struct dirent *dp;
	int match, len;
	_dl_DIR *dd;
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
		for (pp = searchpath; *pp != NULL; pp++) {
			hint = _dl_findhint((char *)sodp->sod_name,
			    sodp->sod_major, sodp->sod_minor, *pp);
			if (hint != NULL)
				return hint;
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
			searchpath = _dl_default_path;
	}
	_dl_memset(&bsod, 0, sizeof(bsod));
	for (pp = searchpath; *pp != NULL; pp++) {
		if ((dd = _dl_opendir(*pp)) != NULL) {
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
						    _dl_hint_store, *pp,
						    PATH_MAX);
						if (pp[0][len-1] != '/') {
							_dl_hint_store[len] =
							    '/';
							len++;
						}
						_dl_strlcpy(
						    &_dl_hint_store[len],
						    dp->d_name,
						    PATH_MAX-len);
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
	}
	return NULL;
}

static elf_object_t *
_dl_lookup_object(const char *req_name, struct sod *req_sod)
{
	elf_object_t *object = _dl_objects;

	while (object) {
		char *soname;

		if (_dl_cmp_sod(req_sod, &object->sod) == 0)
			return(object);

		soname = (char *)object->Dyn.info[DT_SONAME];
		if (soname != NULL) {
			if (_dl_strcmp(req_name, soname) == 0)
				return(object);
		}

		object = object->next;
	}

	return(NULL);
}

void
_dl_handle_already_loaded(elf_object_t *object, int flags)
{
	object->obj_flags |= flags & DF_1_GLOBAL;
	if (_dl_loading_object == NULL)
		_dl_loading_object = object;
	if (object->load_object != _dl_objects &&
	    object->load_object != _dl_loading_object) {
		_dl_link_grpref(object->load_object, _dl_loading_object);
	}
}

static elf_object_t *
_dl_find_loaded_shlib(const char *req_name, struct sod req_sod, int flags)
{
	elf_object_t *object;

	object = _dl_lookup_object(req_name, &req_sod);

	/* if not found retry with any minor */
	if (object == NULL && req_sod.sod_library && req_sod.sod_minor != -1) {
		short orig_minor = req_sod.sod_minor;
		req_sod.sod_minor = -1;
		object = _dl_lookup_object(req_name, &req_sod);

		if (object != NULL && req_sod.sod_minor < orig_minor)
			_dl_printf("warning: lib%s.so.%d.%d: "
			    "minor version >= %d expected, "
			    "using it anyway\n",
			    req_sod.sod_name, req_sod.sod_major,
			    req_sod.sod_minor, orig_minor);
	}

	if (object)
		_dl_handle_already_loaded(object, flags);

	return (object);
}

/*
 *  Load a shared object. Search order is:
 *      First check loaded objects for a matching shlib, otherwise:
 *
 *	If the name contains a '/' use only the path preceding the
 *	library name and do not continue on to other methods if not
 *	found.
 *	   search hints for match in path preceding library name
 *	     this will only match specific library version.
 *	   search path preceding library name
 *	     this will find largest minor version in path provided
 *
 *	Otherwise, the name doesn't contain a '/':
 *	search hints for the specific library version, trying in turn
 *	paths from the following:
 *	  - the LD_LIBRARY_PATH environment variable (if set)
 *	  - the library's own DT_RUNPATH
 *	  - if DT_RUNPATH wasn't set, then:
 *	    - the library's own DT_RPATH
 *	    - the executable's own DT_RPATH
 *	  - the default search path set by ldconfig, or /usr/lib if unset
 *
 *	If the hints doesn't have an exact match, then we search
 *	that exact same list of directories again, looking for a
 *	lib with the correct major version.  If we find a match on
 *	the major, then we take the match *in that directory* which
 *	has the largest minor version
 */

elf_object_t *
_dl_load_shlib(const char *libname, elf_object_t *parent, int type, int flags,
    int nodelete)
{
	int try_any_minor, ignore_hints;
	struct sod sod, req_sod;
	elf_object_t *object = NULL;
	char *hint;

	try_any_minor = 0;
	ignore_hints = 0;

	if (_dl_strchr(libname, '/')) {
		char *paths[2];
		char *lpath, *lname;
		lpath = _dl_strdup(libname);
		if (lpath == NULL)
			_dl_oom();
		lname = _dl_strrchr(lpath, '/');
		if (lname == NULL) {
			_dl_free(lpath);
			_dl_errno = DL_NOT_FOUND;
			return (object);
		}
		*lname = '\0';
		lname++;
		if (*lname  == '\0') {
			_dl_free(lpath);
			_dl_errno = DL_NOT_FOUND;
			return (object);
		}

		_dl_build_sod(lname, &sod);
		req_sod = sod;

		paths[0] = lpath;
		paths[1] = NULL;
fullpathagain:
		hint = _dl_find_shlib(&req_sod, paths, ignore_hints);
		if (hint != NULL)
			goto fullpathdone;

		if (try_any_minor == 0) {
			try_any_minor = 1;
			ignore_hints = 1;
			req_sod.sod_minor = -1;
			goto fullpathagain;
		}
		_dl_errno = DL_NOT_FOUND;
fullpathdone:
		_dl_free(lpath);
		goto done;
	}

	_dl_build_sod(libname, &sod);
	req_sod = sod;

	object = _dl_find_loaded_shlib(libname, req_sod, flags);
	if (object) {
		_dl_free((char *)sod.sod_name);
		return (object);
	}

again:
	/* No '/' in name. Scan the known places, LD_LIBRARY_PATH first.  */
	if (_dl_libpath != NULL) {
		hint = _dl_find_shlib(&req_sod, _dl_libpath, ignore_hints);
		if (hint != NULL)
			goto done;
	}

	/* Check DT_RUNPATH */
	if (parent->runpath != NULL) {
		hint = _dl_find_shlib(&req_sod, parent->runpath, ignore_hints);
		if (hint != NULL)
			goto done;
	} else {
		/*
		 * If DT_RUNPATH wasn't set then first check DT_RPATH,
		 * followed by the main program's DT_RPATH.
		 */
		if (parent->rpath != NULL) {
			hint = _dl_find_shlib(&req_sod, parent->rpath,
			    ignore_hints);
			if (hint != NULL)
				goto done;
		}
		if (parent != _dl_objects && _dl_objects->rpath != NULL) {
			hint = _dl_find_shlib(&req_sod, _dl_objects->rpath,
			    ignore_hints);
			if (hint != NULL)
				goto done;
		}
	}

	/* check 'standard' locations */
	hint = _dl_find_shlib(&req_sod, NULL, ignore_hints);
	if (hint != NULL)
		goto done;

	if (try_any_minor == 0) {
		try_any_minor = 1;
		ignore_hints = 1;
		req_sod.sod_minor = -1;
		goto again;
	}
	_dl_errno = DL_NOT_FOUND;
done:
	if (hint != NULL) {
		if (req_sod.sod_minor < sod.sod_minor)
			_dl_printf("warning: lib%s.so.%d.%d: "
			    "minor version >= %d expected, "
			    "using it anyway\n",
			    sod.sod_name, sod.sod_major,
			    req_sod.sod_minor, sod.sod_minor);
		object = _dl_tryload_shlib(hint, type, flags, nodelete);
	}
	_dl_free((char *)sod.sod_name);
	return(object);
}


void
_dl_link_dlopen(elf_object_t *dep)
{
	struct dep_node *n;

	dep->opencount++;

	if (OBJECT_DLREF_CNT(dep) > 1)
		return;

	n = _dl_malloc(sizeof *n);
	if (n == NULL)
		_dl_oom();

	n->data = dep;
	TAILQ_INSERT_TAIL(&_dlopened_child_list, n, next_sib);

	DL_DEB(("linking %s as dlopen()ed\n", dep->load_name));
}

static void
_dl_child_refcnt_decrement(elf_object_t *object)
{
	struct object_vector vec;
	int i;

	object->refcount--;
	if (OBJECT_REF_CNT(object) == 0)
		for (vec = object->child_vec, i = 0; i < vec.len; i++)
			_dl_child_refcnt_decrement(vec.vec[i]);
}

void
_dl_notify_unload_shlib(elf_object_t *object)
{
	struct object_vector vec;
	struct dep_node *n;
	int i;

	if (OBJECT_REF_CNT(object) == 0)
		for (vec = object->child_vec, i = 0; i < vec.len; i++)
			_dl_child_refcnt_decrement(vec.vec[i]);

	if (OBJECT_DLREF_CNT(object) == 0) {
		while ((n = TAILQ_FIRST(&object->grpref_list)) != NULL) {
			TAILQ_REMOVE(&object->grpref_list, n, next_sib);
			n->data->grprefcount--;
			_dl_notify_unload_shlib(n->data);
			_dl_free(n);
		}
	}
}

void
_dl_unload_dlopen(void)
{
	struct dep_node *node;

	TAILQ_FOREACH_REVERSE(node, &_dlopened_child_list, dlochld, next_sib) {
		/* dont dlclose the main program */
		if (node->data == _dl_objects)
			continue;

		while (node->data->opencount > 0) {
			node->data->opencount--;
			_dl_notify_unload_shlib(node->data);
			_dl_run_all_dtors();
		}
	}
}

void
_dl_link_grpref(elf_object_t *load_group, elf_object_t *load_object)
{
	struct dep_node *n;

	n = _dl_malloc(sizeof *n);
	if (n == NULL)
		_dl_oom();
	n->data = load_group;
	TAILQ_INSERT_TAIL(&load_object->grpref_list, n, next_sib);
	load_group->grprefcount++;
}

void
_dl_link_child(elf_object_t *dep, elf_object_t *p)
{
	int i;

	i = p->child_vec.len++;
	if (i == p->child_vec.alloc)
		_dl_die("child appeared  %d > %d", p->child_vec.len,
		    p->child_vec.alloc);
	p->child_vec.vec[i] = dep;

	dep->refcount++;

	DL_DEB(("linking dep %s as child of %s\n", dep->load_name,
	    p->load_name));
}

void
object_vec_grow(struct object_vector *vec, int more)
{
	vec->alloc += more;
	vec->vec = _dl_reallocarray(vec->vec, vec->alloc, sizeof(*vec->vec));
	if (vec->vec == NULL)
		_dl_oom();
}

/* Generation number of the current grpsym insertion/caching */
static unsigned int _dl_grpsym_gen = 0;

void
_dl_link_grpsym(elf_object_t *object)
{
	struct object_vector *vec;
	int len;

	if (object->grpsym_gen == _dl_grpsym_gen)
		return;
	object->grpsym_gen = _dl_grpsym_gen;

	vec = &_dl_loading_object->grpsym_vec;
	len = vec->len++;
	if (len >= vec->alloc)
		_dl_die("more grpsym than objects?!  %d > %d", vec->len,
		    vec->alloc);
	vec->vec[len] = object;
}

void
_dl_cache_grpsym_list_setup(elf_object_t *object)
{
	struct object_vector *vec;
	int next;

	_dl_grpsym_gen += 1;

	if (_dl_grpsym_gen == 0) {
		/*
		 * If the count rolls over, reset all counters so
		 * we don't get accidental collision.
		 */
		elf_object_t *walkobj;
		for (walkobj = _dl_objects;
		    walkobj != NULL;
		    walkobj = walkobj->next) {
			walkobj->grpsym_gen = 0;
		}
		_dl_grpsym_gen = 1;
	}

	/*
	 * grpsym_vec is a vector of all child libs of the
	 * _dl_loading_object with no dups. The order is equivalent
	 * to a breadth-first traversal of the child list without dups.
	 */

	vec = &object->grpsym_vec;
	object_vec_grow(vec, object_count);
	next = 0;

	/* add first object manually */
	_dl_link_grpsym(object);

	while (next < vec->len) {
		struct object_vector child_vec;
		int i;

		child_vec = vec->vec[next++]->child_vec;
		for (i = 0; i < child_vec.len; i++)
			_dl_link_grpsym(child_vec.vec[i]);
	}
}
