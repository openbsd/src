/*	$OpenBSD: nlist.c,v 1.10 2011/07/03 21:37:40 krw Exp $	*/
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

#ifdef _NLIST_DO_ELF
#include <elf_abi.h>
#endif

#ifdef _NLIST_DO_ECOFF
#include <sys/exec_ecoff.h>
#endif

int	__fdnlist(int, struct nlist *);
int	__aout_fdnlist(int, struct nlist *);
int	__ecoff_fdnlist(int, struct nlist *);
int	__elf_fdnlist(int, struct nlist *);
#ifdef _NLIST_DO_ELF
int	__elf_is_okay__(Elf_Ehdr *ehdr);
#endif

#define	ISLAST(p)	(p->n_un.n_name == 0 || p->n_un.n_name[0] == 0)

#ifdef _NLIST_DO_AOUT
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
#endif /* _NLIST_DO_AOUT */

#ifdef _NLIST_DO_ECOFF
#define check(off, size)	((off < 0) || (off + size > mappedsize))
#define	BAD			do { rv = -1; goto out; } while (0)
#define	BADUNMAP		do { rv = -1; goto unmap; } while (0)

int
__ecoff_fdnlist(int fd, struct nlist *list)
{
	struct nlist *p;
	struct ecoff_exechdr *exechdrp;
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
	mappedfile = mmap(NULL, mappedsize, PROT_READ, MAP_SHARED|MAP_FILE,
	    fd, 0);
	if (mappedfile == MAP_FAILED)
		BAD;

	if (check(0, sizeof *exechdrp))
		BADUNMAP;
	exechdrp = (struct ecoff_exechdr *)&mappedfile[0];

	if (ECOFF_BADMAG(exechdrp))
		BADUNMAP;

	symhdroff = exechdrp->f.f_symptr;
	symhdrsize = exechdrp->f.f_nsyms;

	if (check(symhdroff, sizeof *symhdrp) ||
	    sizeof *symhdrp != symhdrsize)
		BADUNMAP;
	symhdrp = (struct ecoff_symhdr *)&mappedfile[symhdroff];

	nesyms = symhdrp->esymMax;
	if (check(symhdrp->cbExtOffset, nesyms * sizeof *esyms))
		BADUNMAP;
	esyms = (struct ecoff_extsym *)&mappedfile[symhdrp->cbExtOffset];
	extstroff = symhdrp->cbSsExtOffset;

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
#endif /* _NLIST_DO_ECOFF */

#ifdef _NLIST_DO_ELF
/*
 * __elf_is_okay__ - Determine if ehdr really
 * is ELF and valid for the target platform.
 *
 * WARNING:  This is NOT a ELF ABI function and
 * as such its use should be restricted.
 */
int
__elf_is_okay__(Elf_Ehdr *ehdr)
{
	int retval = 0;
	/*
	 * We need to check magic, class size, endianess,
	 * and version before we look at the rest of the
	 * Elf_Ehdr structure.  These few elements are
	 * represented in a machine independent fashion.
	 */
	if (IS_ELF(*ehdr) &&
	    ehdr->e_ident[EI_CLASS] == ELF_TARG_CLASS &&
	    ehdr->e_ident[EI_DATA] == ELF_TARG_DATA &&
	    ehdr->e_ident[EI_VERSION] == ELF_TARG_VER) {

		/* Now check the machine dependant header */
		if (ehdr->e_machine == ELF_TARG_MACH &&
		    ehdr->e_version == ELF_TARG_VER)
			retval = 1;
	}

	return retval;
}

int
__elf_fdnlist(int fd, struct nlist *list)
{
	struct nlist *p;
	caddr_t strtab;
	Elf_Off symoff = 0, symstroff = 0;
	Elf_Word symsize = 0, symstrsize = 0;
	Elf_Sword nent, cc, i;
	Elf_Sym sbuf[1024];
	Elf_Sym *s;
	Elf_Ehdr ehdr;
	Elf_Shdr *shdr = NULL;
	Elf_Word shdr_size;
	struct stat st;
	int usemalloc = 0;

	/* Make sure obj is OK */
	if (pread(fd, &ehdr, sizeof(Elf_Ehdr), (off_t)0) != sizeof(Elf_Ehdr) ||
	    /* !__elf_is_okay__(&ehdr) || */ fstat(fd, &st) < 0)
		return (-1);

	/* calculate section header table size */
	shdr_size = ehdr.e_shentsize * ehdr.e_shnum;

	/* mmap section header table */
	shdr = (Elf_Shdr *)mmap(NULL, (size_t)shdr_size, PROT_READ,
	    MAP_SHARED|MAP_FILE, fd, (off_t) ehdr.e_shoff);
	if (shdr == MAP_FAILED) {
		usemalloc = 1;
		if ((shdr = malloc(shdr_size)) == NULL)
			return (-1);

		if (pread(fd, shdr, shdr_size, (off_t)ehdr.e_shoff) != shdr_size) {
			free(shdr);
			return (-1);
		}
	}

	/*
	 * Find the symbol table entry and its corresponding
	 * string table entry.	Version 1.1 of the ABI states
	 * that there is only one symbol table but that this
	 * could change in the future.
	 */
	for (i = 0; i < ehdr.e_shnum; i++) {
		if (shdr[i].sh_type == SHT_SYMTAB) {
			symoff = shdr[i].sh_offset;
			symsize = shdr[i].sh_size;
			symstroff = shdr[shdr[i].sh_link].sh_offset;
			symstrsize = shdr[shdr[i].sh_link].sh_size;
			break;
		}
	}

	/* Flush the section header table */
	if (usemalloc)
		free(shdr);
	else
		munmap((caddr_t)shdr, shdr_size);

	/*
	 * Map string table into our address space.  This gives us
	 * an easy way to randomly access all the strings, without
	 * making the memory allocation permanent as with malloc/free
	 * (i.e., munmap will return it to the system).
	 */
	if (usemalloc) {
		if ((strtab = malloc(symstrsize)) == NULL)
			return (-1);
		if (pread(fd, strtab, symstrsize, (off_t)symstroff) != symstrsize) {
			free(strtab);
			return (-1);
		}
	} else {
		strtab = mmap(NULL, (size_t)symstrsize, PROT_READ,
		    MAP_SHARED|MAP_FILE, fd, (off_t) symstroff);
		if (strtab == MAP_FAILED)
			return (-1);
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

	/* Don't process any further if object is stripped. */
	/* ELFism - dunno if stripped by looking at header */
	if (symoff == 0)
		goto elf_done;

	while (symsize > 0) {
		cc = MIN(symsize, sizeof(sbuf));
		if (pread(fd, sbuf, cc, (off_t)symoff) != cc)
			break;
		symsize -= cc;
		symoff += cc;
		for (s = sbuf; cc > 0; ++s, cc -= sizeof(*s)) {
			int soff = s->st_name;

			if (soff == 0)
				continue;
			for (p = list; !ISLAST(p); p++) {
				char *sym;

				/*
				 * First we check for the symbol as it was
				 * provided by the user. If that fails
				 * and the first char is an '_', skip over
				 * the '_' and try again.
				 * XXX - What do we do when the user really
				 *       wants '_foo' and the are symbols
				 *       for both 'foo' and '_foo' in the
				 *	 table and 'foo' is first?
				 */
				sym = p->n_un.n_name;
				if (strcmp(&strtab[soff], sym) != 0 &&
				    (sym[0] != '_' ||
				     strcmp(&strtab[soff], sym + 1) != 0))
					continue;

				p->n_value = s->st_value;

				/* XXX - type conversion */
				/*	 is pretty rude. */
				switch(ELF_ST_TYPE(s->st_info)) {
				case STT_NOTYPE:
					switch (s->st_shndx) {
					case SHN_UNDEF:
						p->n_type = N_UNDF;
						break;
					case SHN_ABS:
						p->n_type = N_ABS;
						break;
					case SHN_COMMON:
						p->n_type = N_COMM;
						break;
					default:
						p->n_type = N_COMM | N_EXT;
						break;
					}
					break;
				case STT_OBJECT:
					p->n_type = N_DATA;
					break;
				case STT_FUNC:
					p->n_type = N_TEXT;
					break;
				case STT_FILE:
					p->n_type = N_FN;
					break;
				}
				if (ELF_ST_BIND(s->st_info) ==
				    STB_LOCAL)
					p->n_type = N_EXT;
				p->n_desc = 0;
				p->n_other = 0;
				if (--nent <= 0)
					break;
			}
		}
	}
elf_done:
	if (usemalloc)
		free(strtab);
	else
		munmap(strtab, symstrsize);
	return (nent);
}
#endif /* _NLIST_DO_ELF */


static struct nlist_handlers {
	int	(*fn)(int fd, struct nlist *list);
} nlist_fn[] = {
#ifdef _NLIST_DO_AOUT
	{ __aout_fdnlist },
#endif
#ifdef _NLIST_DO_ELF
	{ __elf_fdnlist },
#endif
#ifdef _NLIST_DO_ECOFF
	{ __ecoff_fdnlist },
#endif
};

int
__fdnlist(int fd, struct nlist *list)
{
	int n = -1, i;

	for (i = 0; i < sizeof(nlist_fn)/sizeof(nlist_fn[0]); i++) {
		n = (nlist_fn[i].fn)(fd, list);
		if (n != -1)
			break;
	}
	return (n);
}


int
nlist(const char *name, struct nlist *list)
{
	int fd, n;

	fd = open(name, O_RDONLY, 0);
	if (fd < 0)
		return (-1);
	n = __fdnlist(fd, list);
	(void)close(fd);
	return (n);
}
