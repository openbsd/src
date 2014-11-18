/*	$OpenBSD: sod.c,v 1.32 2014/11/18 20:51:00 krw Exp $	*/

/*
 * Copyright (c) 1993 Paul Kranenburg
 * All rights reserved.
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
 *	This product includes software developed by Paul Kranenburg.
 * 4. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 */

#include <sys/types.h>
#include <sys/mman.h>
#include <machine/exec.h>
#include <limits.h>
#include <stdio.h>
#include <fcntl.h>
#include <nlist.h>
#include <link.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>

#include "syscall.h"
#include "archdep.h"
#include "path.h"
#include "util.h"
#include "sod.h"

int _dl_hinthash(char *cp, int vmajor, int vminor);
void _dl_maphints(void);

/*
 * Populate sod struct for dlopen's call to map_object
 */
void
_dl_build_sod(const char *name, struct sod *sodp)
{
	unsigned int	tuplet;
	int		major, minor;
	char		*realname, *tok, *etok, *cp;

	/* default is an absolute or relative path */
	sodp->sod_name = (long)_dl_strdup(name);    /* strtok is destructive */
	if (sodp->sod_name == 0)
		_dl_exit(7);
	sodp->sod_library = 0;
	sodp->sod_major = sodp->sod_minor = 0;

	/* does it look like /^lib/ ? */
	if (_dl_strncmp((char *)sodp->sod_name, "lib", 3) != 0)
		goto backout;

	/* is this a filename? */
	if (_dl_strchr((char *)sodp->sod_name, '/'))
		goto backout;

	/* skip over 'lib' */
	cp = (char *)sodp->sod_name + 3;

	realname = cp;

	/* dot guardian */
	if ((_dl_strchr(cp, '.') == NULL) || (*(cp+_dl_strlen(cp)-1) == '.'))
		goto backout;

	cp = _dl_strstr(cp, ".so");
	if (cp == NULL)
		goto backout;

	/* default */
	major = minor = -1;

	/* loop through name - parse skipping name */
	for (tuplet = 0; (tok = _dl_strsep(&cp, ".")) != NULL; tuplet++) {
		switch (tuplet) {
		case 0:
			/* empty tok, we already skipped to "\.so.*" */
			break;
		case 1:
			/* 'so' extension */
			break;
		case 2:
			/* major version extension */
			major = _dl_strtol(tok, &etok, 10);
			if (*tok == '\0' || *etok != '\0')
				goto backout;
			break;
		case 3:
			/* minor version extension */
			minor = _dl_strtol(tok, &etok, 10);
			if (*tok == '\0' || *etok != '\0')
				goto backout;
			break;
		/* if we get here, it must be weird */
		default:
			goto backout;
		}
	}
	if (realname == NULL)
		goto backout;
	cp = (char *)sodp->sod_name;
	sodp->sod_name = (long)_dl_strdup(realname);
	if (sodp->sod_name == 0)
		_dl_exit(7);
	_dl_free(cp);
	sodp->sod_library = 1;
	sodp->sod_major = major;
	sodp->sod_minor = minor;
	return;

backout:
	_dl_free((char *)sodp->sod_name);
	sodp->sod_name = (long)_dl_strdup(name);
	if (sodp->sod_name == 0)
		_dl_exit(7);
}

void
_dl_set_sod(const char *path, struct sod *sod)
{
	char *fname = _dl_strrchr(path, '/');

	if (fname != NULL)
		_dl_build_sod(++fname, sod);
	else
		_dl_build_sod(path, sod);
}

static struct hints_header	*hheader = NULL;
static struct hints_bucket	*hbuckets;
static char			*hstrtab;
char				**_dl_hint_search_path = NULL;

#define HINTS_VALID (hheader != NULL && hheader != (struct hints_header *)-1)

void
_dl_maphints(void)
{
	struct stat	sb;
	caddr_t		addr = MAP_FAILED;
	long		hsize = 0;
	int		hfd;

	if ((hfd = _dl_open(_PATH_LD_HINTS, O_RDONLY | O_CLOEXEC)) < 0)
		goto bad_hints;

	if (_dl_fstat(hfd, &sb) != 0 || !S_ISREG(sb.st_mode) ||
	    sb.st_size < sizeof(struct hints_header) || sb.st_size > LONG_MAX)
		goto bad_hints;

	hsize = (long)sb.st_size;
	addr = (void *)_dl_mmap(0, hsize, PROT_READ, MAP_PRIVATE, hfd, 0);
	if (_dl_mmap_error(addr))
		goto bad_hints;

	hheader = (struct hints_header *)addr;
	if (HH_BADMAG(*hheader) || hheader->hh_ehints > hsize)
		goto bad_hints;

	if (hheader->hh_version != LD_HINTS_VERSION_1 &&
	    hheader->hh_version != LD_HINTS_VERSION_2)
		goto bad_hints;

	hbuckets = (struct hints_bucket *)(addr + hheader->hh_hashtab);
	hstrtab = (char *)(addr + hheader->hh_strtab);
	if (hheader->hh_version >= LD_HINTS_VERSION_2)
		_dl_hint_search_path = _dl_split_path(hstrtab + hheader->hh_dirlist);

	/* close the file descriptor, leaving the hints mapped */
	_dl_close(hfd);

	return;

bad_hints:
	if (!_dl_mmap_error(addr))
		_dl_munmap(addr, hsize);
	if (hfd != -1)
		_dl_close(hfd);
	hheader = (struct hints_header *)-1;
}

char *
_dl_findhint(char *name, int major, int minor, char *preferred_path)
{
	struct hints_bucket	*bp;

	/*
	 * If not mapped, and we have not tried before, try to map the
	 * hints, if previous attempts failed hheader is -1 and we
	 * do not wish to retry it.
	 */
	if (hheader == NULL)
		_dl_maphints();

	/* if it failed to map, return failure */
	if (!(HINTS_VALID))
		return NULL;

	if (hheader->hh_nbucket == 0)
		return NULL;

	bp = hbuckets + (_dl_hinthash(name, major, minor) % hheader->hh_nbucket);

	while (1) {
		/* Sanity check */
		if (bp->hi_namex >= hheader->hh_strtab_sz) {
			_dl_printf("Bad name index: %#x\n", bp->hi_namex);
			_dl_exit(7);
			break;
		}
		if (bp->hi_pathx >= hheader->hh_strtab_sz) {
			_dl_printf("Bad path index: %#x\n", bp->hi_pathx);
			_dl_exit(7);
			break;
		}

		if (_dl_strcmp(name, hstrtab + bp->hi_namex) == 0) {
			/* It's `name', check version numbers */
			if (bp->hi_major == major &&
			    (bp->hi_ndewey < 2 || bp->hi_minor >= minor)) {
				if (preferred_path == NULL) {
					return hstrtab + bp->hi_pathx;
				} else {
					char *path = hstrtab + bp->hi_pathx;
					char *edir = _dl_strrchr(path, '/');

					if ((_dl_strncmp(preferred_path, path,
					    (edir - path)) == 0) &&
					    (preferred_path[edir - path] == '\0'))
						return path;
				}
			}
		}

		if (bp->hi_next == -1)
			break;

		/* Move on to next in bucket */
		bp = &hbuckets[bp->hi_next];
	}

	/* No hints available for name */
	return NULL;
}

int
_dl_hinthash(char *cp, int vmajor, int vminor)
{
	int	k = 0;

	while (*cp)
		k = (((k << 1) + (k >> 14)) ^ (*cp++)) & 0x3fff;

	k = (((k << 1) + (k >> 14)) ^ (vmajor*257)) & 0x3fff;
	if (hheader->hh_version == LD_HINTS_VERSION_1)
		k = (((k << 1) + (k >> 14)) ^ (vminor*167)) & 0x3fff;

	return k;
}
