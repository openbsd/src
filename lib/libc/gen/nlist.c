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
static char rcsid[] = "$OpenBSD: nlist.c,v 1.25 1998/08/21 19:25:36 millert Exp $";
#endif /* LIBC_SCCS and not lint */

#include <sys/types.h>
#include <sys/param.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <sys/file.h>

#include <errno.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <a.out.h>		/* pulls in nlist.h */

#ifdef _NLIST_DO_ELF
#include <elf_abi.h>
#include <olf_abi.h>
#endif

#ifdef _NLIST_DO_ECOFF
#include <sys/exec_ecoff.h>
#endif

#define	ISLAST(p)	(p->n_un.n_name == 0 || p->n_un.n_name[0] == 0)

#ifdef _NLIST_DO_AOUT
int
__aout_fdnlist(fd, list)
	register int fd;
	register struct nlist *list;
{
	register struct nlist *p, *s;
	register void *strtab;
	register off_t stroff, symoff;
	register u_long symsize;
	register int nent, cc;
	int strsize;
	struct nlist nbuf[1024];
	struct exec exec;

	if (lseek(fd, (off_t)0, SEEK_SET) == -1 ||
	    read(fd, &exec, sizeof(exec)) != sizeof(exec) ||
	    N_BADMAG(exec) || exec.a_syms == NULL)
		return (-1);

	symoff = N_SYMOFF(exec);
	symsize = exec.a_syms;
	stroff = symoff + symsize;

	/* Read in the size of the string table. */
	if (read(fd, (char *)&strsize, sizeof(strsize)) != sizeof(strsize))
		return (-1);

	/*
	 * Map string table into our address space.  This gives us
	 * an easy way to randomly access all the strings, without
	 * making the memory allocation permanent as with malloc/free
	 * (i.e., munmap will return it to the system).  We try to
	 * get a clean snapshot via MAP_COPY but that does not work
	 * for cdevs (like /dev/ksyms) so we try without if that fails.
	 */
	if ((strtab = mmap(NULL, (size_t)strsize, PROT_READ, MAP_COPY|MAP_FILE,
	    fd, stroff)) == MAP_FAILED)
		strtab = mmap(NULL, (size_t)strsize, PROT_READ, 0, fd,
		    stroff);
	if (strtab == MAP_FAILED)
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
				if (!strcmp(&((char *)strtab)[soff], p->n_un.n_name)) {
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
#endif /* _NLIST_DO_AOUT */

#ifdef _NLIST_DO_ECOFF
#define check(off, size)	((off < 0) || (off + size > mappedsize))
#define	BAD			do { rv = -1; goto out; } while (0)
#define	BADUNMAP		do { rv = -1; goto unmap; } while (0)

int
__ecoff_fdnlist(fd, list)
	register int fd;
	register struct nlist *list;
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
	mappedfile = mmap(NULL, mappedsize, PROT_READ, MAP_COPY|MAP_FILE,
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
 * as such it's use should be restricted.
 */
int
__elf_is_okay__(ehdr)
	register Elf32_Ehdr *ehdr;
{
	register int retval = 0;
	/*
	 * We need to check magic, class size, endianess,
	 * and version before we look at the rest of the
	 * Elf32_Ehdr structure.  These few elements are
	 * represented in a machine independant fashion.
	 */
	if ((IS_ELF(*ehdr) || IS_OLF(*ehdr)) &&
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
__elf_fdnlist(fd, list)
	register int fd;
	register struct nlist *list;
{
	register struct nlist *p;
	register caddr_t strtab;
	register Elf32_Off symoff = 0, symstroff = 0;
	register Elf32_Word symsize = 0, symstrsize = 0;
	register Elf32_Sword nent, cc, i;
	Elf32_Sym sbuf[1024];
	Elf32_Sym *s;
	Elf32_Ehdr ehdr;
	Elf32_Shdr *shdr = NULL;
	Elf32_Word shdr_size;
	struct stat st;

	/* Make sure obj is OK */
	if (lseek(fd, (off_t)0, SEEK_SET) == -1 ||
	    read(fd, &ehdr, sizeof(Elf32_Ehdr)) != sizeof(Elf32_Ehdr) ||
	    !__elf_is_okay__(&ehdr) ||
	    fstat(fd, &st) < 0)
		return (-1);

	/* calculate section header table size */
	shdr_size = ehdr.e_shentsize * ehdr.e_shnum;

	/* Make sure it's not too big to mmap */
	if (shdr_size > SIZE_T_MAX) {
		errno = EFBIG;
		return (-1);
	}

	/* mmap section header table */
	shdr = (Elf32_Shdr *)mmap(NULL, (size_t)shdr_size, PROT_READ,
	    MAP_COPY|MAP_FILE, fd, (off_t) ehdr.e_shoff);
	if (shdr == (Elf32_Shdr *)-1)
		return (-1);

	/*
	 * Find the symbol table entry and it's corresponding
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
	munmap((caddr_t)shdr, shdr_size);

	/* Check for files too large to mmap. */
	/* XXX is this really possible? */
	if (symstrsize > SIZE_T_MAX) {
		errno = EFBIG;
		return (-1);
	}
	/*
	 * Map string table into our address space.  This gives us
	 * an easy way to randomly access all the strings, without
	 * making the memory allocation permanent as with malloc/free
	 * (i.e., munmap will return it to the system).
	 */
	strtab = mmap(NULL, (size_t)symstrsize, PROT_READ, MAP_COPY|MAP_FILE,
	    fd, (off_t) symstroff);
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

	/* Don't process any further if object is stripped. */
	/* ELFism - dunno if stripped by looking at header */
	if (symoff == 0)
		goto done;

	if (lseek(fd, (off_t) symoff, SEEK_SET) == -1) {
		nent = -1;
		goto done;
	}

	while (symsize > 0) {
		cc = MIN(symsize, sizeof(sbuf));
		if (read(fd, sbuf, cc) != cc)
			break;
		symsize -= cc;
		for (s = sbuf; cc > 0; ++s, cc -= sizeof(*s)) {
			register int soff = s->st_name;

			if (soff == 0)
				continue;
			for (p = list; !ISLAST(p); p++) {
				/*
				 * XXX - ABI crap, they
				 * really fucked this up
				 * for MIPS and PowerPC
				 */
				if (!strcmp(&strtab[soff],
				    ((ehdr.e_machine == EM_MIPS) ||
				     (ehdr.e_machine == EM_PPC)) ?
				    p->n_un.n_name+1 :
				    p->n_un.n_name)) {
					p->n_value = s->st_value;

					/* XXX - type conversion */
					/*	 is pretty rude. */
					switch(ELF32_ST_TYPE(s->st_info)) {
					case STT_NOTYPE:
						p->n_type = N_UNDF;
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
					if (ELF32_ST_BIND(s->st_info) ==
					    STB_LOCAL)
						p->n_type = N_EXT;
					p->n_desc = 0;
					p->n_other = 0;
					if (--nent <= 0)
						break;
				}
			}
		}
	}
done:
	munmap(strtab, symstrsize);
	return (nent);
}
#endif /* _NLIST_DO_ELF */


static struct nlist_handlers {
	int	(*fn) __P((int fd, struct nlist *list));
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
__fdnlist(fd, list)
	register int fd;
	register struct nlist *list;
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
