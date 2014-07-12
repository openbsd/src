/*	$OpenBSD: nlist.c,v 1.3 2014/07/12 19:01:49 tedu Exp $ */
/*
 * Copyright (c) 1989, 1993
 *	The Regents of the University of California.  All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. Neither the name of the University nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE REGENTS AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#include <sys/types.h>
#include <sys/param.h>
#include <sys/mman.h>
#include <sys/stat.h>

#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <a.out.h>		/* pulls in nlist.h */

#define	ISLAST(p)	(p->n_un.n_name == 0 || p->n_un.n_name[0] == 0)

int
__aout_fdnlist(int fd, struct nlist *list)
{
	struct nlist *p, *s;
	char *strtab;
	off_t symoff, stroff;
	u_long symsize;
	int nent, cc;
	int strsize, usemalloc = 0;
	struct nlist nbuf[1024];
	struct exec exec;

	if (pread(fd, &exec, sizeof(exec), (off_t)0) != sizeof(exec) ||
	    N_BADMAG(exec) || exec.a_syms == 0)
		return (-1);

	stroff = N_STROFF(exec);
	symoff = N_SYMOFF(exec);
	symsize = exec.a_syms;

	/* Read in the size of the string table. */
	if (pread(fd, (void *)&strsize, sizeof(strsize), stroff) !=
	    sizeof(strsize))
		return (-1);
	else
		stroff += sizeof(strsize);

	/*
	 * Read in the string table.  We try mmap, but that will fail
	 * for /dev/ksyms so fall back on malloc.  Since OpenBSD's malloc(3)
	 * returns memory to the system on free this does not cause bloat.
	 */
	strsize -= sizeof(strsize);
	strtab = mmap(NULL, (size_t)strsize, PROT_READ, MAP_SHARED|MAP_FILE,
	    fd, stroff);
	if (strtab == MAP_FAILED) {
		usemalloc = 1;
		if ((strtab = (char *)malloc(strsize)) == NULL)
			return (-1);
		errno = EIO;
		if (pread(fd, strtab, strsize, stroff) != strsize) {
			nent = -1;
			goto aout_done;
		}
	}

	/*
	 * clean out any left-over information for all valid entries.
	 * Type and value defined to be 0 if not found; historical
	 * versions cleared other and desc as well.  Also figure out
	 * the largest string length so don't read any more of the
	 * string table than we have to.
	 *
	 * XXX clearing anything other than n_type and n_value violates
	 * the semantics given in the man page.
	 */
	nent = 0;
	for (p = list; !ISLAST(p); ++p) {
		p->n_type = 0;
		p->n_other = 0;
		p->n_desc = 0;
		p->n_value = 0;
		++nent;
	}

	while (symsize > 0) {
		cc = MIN(symsize, sizeof(nbuf));
		if (pread(fd, nbuf, cc, symoff) != cc)
			break;
		symsize -= cc;
		symoff += cc;
		for (s = nbuf; cc > 0; ++s, cc -= sizeof(*s)) {
			char *sname = strtab + s->n_un.n_strx - sizeof(int);

			if (s->n_un.n_strx == 0 || (s->n_type & N_STAB) != 0)
				continue;
			for (p = list; !ISLAST(p); p++) {
				char *pname = p->n_un.n_name;

				if (*sname != '_' && *pname == '_')
					pname++;
				if (!strcmp(sname, pname)) {
					p->n_value = s->n_value;
					p->n_type = s->n_type;
					p->n_desc = s->n_desc;
					p->n_other = s->n_other;
					if (--nent <= 0)
						break;
				}
			}
		}
	}
aout_done:
	if (usemalloc)
		free(strtab);
	else
		munmap(strtab, strsize);
	return (nent);
}

int
nlist(const char *name, struct nlist *list)
{
	int fd, n;

	fd = open(name, O_RDONLY, 0);
	if (fd < 0)
		return (-1);
	n = __aout_fdnlist(fd, list);
	(void)close(fd);
	return (n);
}
