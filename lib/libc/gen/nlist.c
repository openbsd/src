/*	$NetBSD: nlist.c,v 1.6 1995/09/29 04:19:59 cgd Exp $	*/

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
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *	This product includes software developed by the University of
 *	California, Berkeley and its contributors.
 * 4. Neither the name of the University nor the names of its contributors
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

#if defined(LIBC_SCCS) && !defined(lint)
#if 0
static char sccsid[] = "@(#)nlist.c	8.1 (Berkeley) 6/4/93";
#else
static char rcsid[] = "$NetBSD: nlist.c,v 1.6 1995/09/29 04:19:59 cgd Exp $";
#endif
#endif /* LIBC_SCCS and not lint */

#ifdef __alpha__
#define		DO_ECOFF
#else
#define		DO_AOUT
#endif

#if defined(DO_AOUT) + defined(DO_ECOFF) != 1
	ERROR: NOT PROPERLY CONFIGURED
#endif

#include <sys/param.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <sys/file.h>

#include <errno.h>
#include <a.out.h>
#ifdef DO_ECOFF
#include <sys/exec_ecoff.h>
#endif
#include <stdio.h>
#include <string.h>
#include <unistd.h>

int
nlist(name, list)
	const char *name;
	struct nlist *list;
{
	int fd, n;

	fd = open(name, O_RDONLY, 0);
	if (fd < 0)
		return (-1);
	n = __fdnlist(fd, list);
	(void)close(fd);
	return (n);
}

#define	ISLAST(p)	(p->n_un.n_name == 0 || p->n_un.n_name[0] == 0)

#ifdef DO_AOUT
int
__fdnlist(fd, list)
	register int fd;
	register struct nlist *list;
{
	register struct nlist *p, *s;
	register caddr_t strtab;
	register off_t stroff, symoff;
	register u_long symsize;
	register int nent, cc;
	size_t strsize;
	struct nlist nbuf[1024];
	struct exec exec;
	struct stat st;

	if (lseek(fd, (off_t)0, SEEK_SET) == -1 ||
	    read(fd, &exec, sizeof(exec)) != sizeof(exec) ||
	    N_BADMAG(exec) || fstat(fd, &st) < 0)
		return (-1);

	symoff = N_SYMOFF(exec);
	symsize = exec.a_syms;
	stroff = symoff + symsize;

	/* Check for files too large to mmap. */
	if (st.st_size - stroff > SIZE_T_MAX) {
		errno = EFBIG;
		return (-1);
	}
	/*
	 * Map string table into our address space.  This gives us
	 * an easy way to randomly access all the strings, without
	 * making the memory allocation permanent as with malloc/free
	 * (i.e., munmap will return it to the system).
	 */
	strsize = st.st_size - stroff;
	strtab = mmap(NULL, (size_t)strsize, PROT_READ, 0, fd, stroff);
	if (strtab == (char *)-1)
		return (-1);
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
	if (lseek(fd, symoff, SEEK_SET) == -1)
		return (-1);

	while (symsize > 0) {
		cc = MIN(symsize, sizeof(nbuf));
		if (read(fd, nbuf, cc) != cc)
			break;
		symsize -= cc;
		for (s = nbuf; cc > 0; ++s, cc -= sizeof(*s)) {
			register int soff = s->n_un.n_strx;

			if (soff == 0 || (s->n_type & N_STAB) != 0)
				continue;
			for (p = list; !ISLAST(p); p++)
				if (!strcmp(&strtab[soff], p->n_un.n_name)) {
					p->n_value = s->n_value;
					p->n_type = s->n_type;
					p->n_desc = s->n_desc;
					p->n_other = s->n_other;
					if (--nent <= 0)
						break;
				}
		}
	}
	munmap(strtab, strsize);
	return (nent);
}
#endif /* DO_AOUT */

#ifdef DO_ECOFF
#define check(off, size)	((off < 0) || (off + size > mappedsize))
#define	BAD			do { rv = -1; goto out; } while (0)
#define	BADUNMAP		do { rv = -1; goto unmap; } while (0)

int
__fdnlist(fd, list)
	register int fd;
	register struct nlist *list;
{
	struct nlist *p;
	struct ecoff_filehdr *filehdrp;
	struct ecoff_symhdr *symhdrp;
	struct ecoff_extsym *esyms;
	struct stat st;
	char *mappedfile;
	size_t mappedsize;
	u_long symhdroff, extstroff;
	u_int symhdrsize;
	int rv, nent;
	long i, nesyms;

	rv = -3;

	if (fstat(fd, &st) < 0)
		BAD;
	if (st.st_size > SIZE_T_MAX) {
		errno = EFBIG;
		BAD;
	}
	mappedsize = st.st_size;
	mappedfile = mmap(NULL, mappedsize, PROT_READ, 0, fd, 0);
	if (mappedfile == (char *)-1)
		BAD;

	if (check(0, sizeof *filehdrp))
		BADUNMAP;
	filehdrp = (struct ecoff_filehdr *)&mappedfile[0];

	if (ECOFF_BADMAG(filehdrp))
		BADUNMAP;

	symhdroff = filehdrp->ef_symptr;
	symhdrsize = filehdrp->ef_syms;

	if (check(symhdroff, sizeof *symhdrp) ||
	    sizeof *symhdrp != symhdrsize)
		BADUNMAP;
	symhdrp = (struct ecoff_symhdr *)&mappedfile[symhdroff];

	nesyms = symhdrp->sh_esymmax;
	if (check(symhdrp->sh_esymoff, nesyms * sizeof *esyms))
		BADUNMAP;
	esyms = (struct ecoff_extsym *)&mappedfile[symhdrp->sh_esymoff];
	extstroff = symhdrp->sh_estroff;

	/*
	 * clean out any left-over information for all valid entries.
	 * Type and value defined to be 0 if not found; historical
	 * versions cleared other and desc as well.
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

	for (i = 0; i < nesyms; i++) {
		for (p = list; !ISLAST(p); p++) {
			char *nlistname;
			char *symtabname;

			nlistname = p->n_un.n_name;
			if (*nlistname == '_')
				nlistname++;
			symtabname =
			    &mappedfile[extstroff + esyms[i].es_strindex];

			if (!strcmp(symtabname, nlistname)) {
				p->n_value = esyms[i].es_value;
				p->n_type = N_EXT;		/* XXX */
				p->n_desc = 0;			/* XXX */
				p->n_other = 0;			/* XXX */
				if (--nent <= 0)
					break;
			}
		}
	}
	rv = nent;

unmap:
	munmap(mappedfile, mappedsize);
out:
	return (rv);
}
#endif /* DO_ECOFF */
