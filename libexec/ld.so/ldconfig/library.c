/* $OpenBSD: library.c,v 1.3 2009/12/30 04:30:01 drahn Exp $ */
/*
 * Copyright (c) 2006 Dale Rahn <drahn@dalerahn.com>
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

#include <sys/types.h>
#include <sys/stat.h>
#include <sys/syslimits.h>
#include <sys/param.h>
#include <sys/mman.h>
#include <fcntl.h>
#include <nlist.h>
#include <elf_abi.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <dirent.h>
#include "link.h"
#include "sod.h"
#include "resolve.h"
#include "prebind.h"
#include "prebind_struct.h"

/* TODO - library path from ldconfig */
#define DEFAULT_PATH "/usr/lib"

elf_object_t * elf_load_shlib_hint(struct sod *sod, struct sod *req_sod,
    int ignore_hints, const char *libpath);
char * elf_find_shlib(struct sod *sodp, const char *searchpath, int nohints);
elf_object_t * elf_tryload_shlib(const char *libname);
int elf_match_file(struct sod *sodp, char *name, int namelen);

int
load_lib(const char *name, struct elf_object *parent)
{
	struct sod sod, req_sod;
	int ignore_hints, try_any_minor = 0;
	struct elf_object *object = NULL;

#if 0
	printf("load_lib %s\n", name);
#endif
	ignore_hints = 0;

	if(strchr(name, '/')) {
		char *lpath, *lname;

		lpath = strdup(name);
		lname = strrchr(lpath, '/');
		if (lname == NULL || lname[1] == '\0') {
			free(lpath);
			return (1); /* failed */
		}
		*lname = '\0';
		lname++;

		_dl_build_sod(lname, &sod);
		req_sod = sod;

		/* this code does not allow lower minors */
fullpathagain:
		object = elf_load_shlib_hint(&sod, &req_sod,
			ignore_hints, lpath);
		if (object != NULL)
			goto fullpathdone;

		if (try_any_minor == 0) {
			try_any_minor = 1;
			ignore_hints = 1;
			req_sod.sod_minor = -1;
			goto fullpathagain;
		}
		/* ERR */
fullpathdone:
		free(lpath);
		free((char *)sod.sod_name);
		return (object == NULL); /* failed */
	}
	_dl_build_sod(name, &sod);
	req_sod = sod;

	/* ignore LD_LIBRARY_PATH */

again:
	if (parent->dyn.rpath != NULL) {
		object = elf_load_shlib_hint(&sod, &req_sod,
		    ignore_hints, parent->dyn.rpath);
		if (object != NULL)
			goto done;
	}
	if (parent != load_object && load_object->dyn.rpath != NULL) {
		object = elf_load_shlib_hint(&sod, &req_sod,
			ignore_hints, load_object->dyn.rpath);
		if (object != NULL)
			goto done;
	}
	object = elf_load_shlib_hint(&sod, &req_sod, ignore_hints, NULL);

	if (try_any_minor == 0) {
		try_any_minor = 1;
		ignore_hints = 1;
		req_sod.sod_minor = -1;
		goto again;
	}
	if (object == NULL)
		printf ("unable to load %s\n", name);

done:
	free((char *)sod.sod_name);

	return (object == NULL);
}

/*
 * attempt to locate and load a library based on libpath, sod info and
 * if it needs to respect hints, passing type and flags to perform open
 */
elf_object_t *
elf_load_shlib_hint(struct sod *sod, struct sod *req_sod,
    int ignore_hints, const char *libpath)
{
	elf_object_t *object = NULL;
	char *hint;

	hint = elf_find_shlib(req_sod, libpath, ignore_hints);
	if (hint != NULL) {
		if (req_sod->sod_minor < sod->sod_minor)
			printf("warning: lib%s.so.%d.%d: "
			    "minor version >= %d expected, "
			    "using it anyway\n",
			    (char *)sod->sod_name, sod->sod_major,
			    req_sod->sod_minor, sod->sod_minor);
		object = elf_tryload_shlib(hint);
	}
	return object;
}

char elf_hint_store[MAXPATHLEN];

char *
elf_find_shlib(struct sod *sodp, const char *searchpath, int nohints)
{
	char *hint, lp[PATH_MAX + 10], *path;
	struct sod tsod, bsod;		/* transient and best sod */
	struct dirent *dp;
	const char *pp;
	int match, len;
	DIR *dd;

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

			/* interpret "" as curdir "." */
			if (lp[0] == '\0') {
				lp[0] = '.';
				lp[1] = '\0';
			}

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

		/* interpret "" as curdir "." */
		if (lp[0] == '\0') {
			lp[0] = '.';
			lp[1] = '\0';
		}

		if ((dd = opendir(lp)) != NULL) {
			match = 0;
			while ((dp = readdir(dd)) != NULL) {
				tsod = *sodp;
				if (elf_match_file(&tsod, dp->d_name,
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
						len = strlcpy(
						    elf_hint_store, lp,
						    MAXPATHLEN);
						if (lp[len-1] != '/') {
							elf_hint_store[len] =
							    '/';
							len++;
						}
						strlcpy(
						    &elf_hint_store[len],
						    dp->d_name,
						    MAXPATHLEN-len);
						if (tsod.sod_major == -1)
							break;
					}
				}
			}
			closedir(dd);
			if (match) {
				*sodp = bsod;
				return (elf_hint_store);
			}
		}

		if (*pp)	/* Try curdir if ':' at end */
			pp++;
		else
			pp = 0;
	}
	return NULL;
}

elf_object_t *
elf_tryload_shlib(const char *libname)
{
	struct elf_object *object;
	object = elf_lookup_object(libname);

	if (object == NULL)
		object = load_file(libname, OBJTYPE_LIB);
	if (object == NULL)
		printf("tryload_shlib %s\n", libname);
	return object;
}

/*
 * elf_match_file()
 *
 * This fucntion determines if a given name matches what is specified
 * in a struct sod. The major must match exactly, and the minor must
 * be same or larger.
 *
 * sodp is updated with the minor if this matches.
 */

int
elf_match_file(struct sod *sodp, char *name, int namelen)
{
	int match;
	struct sod lsod;
	char *lname;

	lname = name;
	if (sodp->sod_library) {
		if (strncmp(name, "lib", 3))
			return 0;
		lname += 3;
	}
	if (strncmp(lname, (char *)sodp->sod_name,
	    strlen((char *)sodp->sod_name)))
		return 0;

	_dl_build_sod(name, &lsod);

	match = 0;
	if (strcmp((char *)lsod.sod_name, (char *)sodp->sod_name) == 0 &&
	    lsod.sod_library == sodp->sod_library &&
	    (sodp->sod_major == -1 || sodp->sod_major == lsod.sod_major) &&
	    (sodp->sod_minor == -1 || lsod.sod_minor >= sodp->sod_minor)) {
		match = 1;

		/* return version matched */
		sodp->sod_major = lsod.sod_major;
		sodp->sod_minor = lsod.sod_minor;
	}
	free((char *)lsod.sod_name);
	return match;
}

