/*	$OpenBSD: sod.c,v 1.16 2003/02/02 16:57:58 deraadt Exp $	*/

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
#include <sys/syslimits.h>
#include <stdio.h>
#include <fcntl.h>
#include <nlist.h>
#include <link.h>
#include <machine/exec.h>
#include <sys/mman.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <syscall.h>

#include "archdep.h"
#include "util.h"
#include "sod.h"

#define PAGSIZ	__LDPGSZ
int _dl_hinthash(char *cp, int vmajor, int vminor);

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
	sodp->sod_library = 0;
	sodp->sod_major = sodp->sod_minor = 0;

	/* does it look like /^lib/ ? */
	if (_dl_strncmp((char *)sodp->sod_name, "lib", 3) != 0)
		return;

	/* is this a filename? */
	if (_dl_strchr((char *)sodp->sod_name, '/'))
		return;

	/* skip over 'lib' */
	cp = (char *)sodp->sod_name + 3;

	/* dot guardian */
	if ((_dl_strchr(cp, '.') == NULL) || (*(cp+_dl_strlen(cp)-1) == '.'))
		return;

	/* default */
	major = minor = -1;

	realname = NULL;
	/* loop through name - parse skipping name */
	for (tuplet = 0; (tok = strsep(&cp, ".")) != NULL; tuplet++) {
		switch (tuplet) {
		case 0:
			/* removed 'lib' and extensions from name */
			realname = tok;
			break;
		case 1:
			/* 'so' extension */
			if (_dl_strcmp(tok, "so") != 0)
				goto backout;
			break;
		case 2:
			/* major version extension */
			major = strtol(tok, &etok, 10);
			if (*tok == '\0' || *etok != '\0')
				goto backout;
			break;
		case 3:
			/* minor version extension */
			minor = strtol(tok, &etok, 10);
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
	_dl_free(cp);
	sodp->sod_library = 1;
	sodp->sod_major = major;
	sodp->sod_minor = minor;
	return;

backout:
	_dl_free((char *)sodp->sod_name);
	sodp->sod_name = (long)_dl_strdup(name);
}

static int			hfd;
static long			hsize;
static struct hints_header	*hheader = NULL;
static struct hints_bucket	*hbuckets;
static char			*hstrtab;
char				*_dl_hint_search_path = NULL;

#define HINTS_VALID (hheader != NULL && hheader != (struct hints_header *)-1)

void
_dl_maphints(void)
{
	caddr_t		addr;

	if ((hfd = _dl_open(_PATH_LD_HINTS, O_RDONLY)) < 0) {
		hheader = (struct hints_header *)-1;
		return;
	}

	hsize = PAGSIZ;
	addr = (void *) _dl_mmap(0, hsize, PROT_READ, MAP_PRIVATE, hfd, 0);

	if (addr == MAP_FAILED) {
		_dl_close(hfd);
		hheader = (struct hints_header *)-1;
		return;
	}

	hheader = (struct hints_header *)addr;
	if (HH_BADMAG(*hheader)) {
		_dl_munmap(addr, hsize);
		_dl_close(hfd);
		hheader = (struct hints_header *)-1;
		return;
	}

	if (hheader->hh_version != LD_HINTS_VERSION_1 &&
	    hheader->hh_version != LD_HINTS_VERSION_2) {
		_dl_munmap(addr, hsize);
		_dl_close(hfd);
		hheader = (struct hints_header *)-1;
		return;
	}

	if (hheader->hh_ehints > hsize) {
		if ((caddr_t)_dl_mmap(addr+hsize, hheader->hh_ehints - hsize,
		    PROT_READ, MAP_PRIVATE|MAP_FIXED,
		    hfd, hsize) != (caddr_t)(addr+hsize)) {
			_dl_munmap((caddr_t)hheader, hsize);
			_dl_close(hfd);
			hheader = (struct hints_header *)-1;
			return;
		}
	}

	hbuckets = (struct hints_bucket *)(addr + hheader->hh_hashtab);
	hstrtab = (char *)(addr + hheader->hh_strtab);
	if (hheader->hh_version >= LD_HINTS_VERSION_2)
		_dl_hint_search_path = hstrtab + hheader->hh_dirlist;

	/* close the file descriptor, leaving the hints mapped */
	_dl_close(hfd);
}

char *
_dl_findhint(char *name, int major, int minor, char *prefered_path)
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
				if (prefered_path == NULL ||
				    _dl_strncmp(prefered_path,
				    hstrtab + bp->hi_pathx,
				    _dl_strlen(prefered_path)) == 0)
					return hstrtab + bp->hi_pathx;
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
