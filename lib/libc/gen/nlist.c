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
static char rcsid[] = "$OpenBSD: nlist.c,v 1.28 1998/09/05 16:30:07 millert Exp $";
#endif /* LIBC_SCCS and not lint */

#include <sys/types.h>
#include <sys/param.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <sys/file.h>

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
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

#ifdef _NLIST_DO_GZIP
#include <zlib.h>
#define Read	gzread
#define Seek	gzseek
typedef gzFile	File;
#else
#define Read	read
#define Seek	lseek
typedef int	File;
#endif /* _NLIST_DO_GZIP */

#define	ISLAST(p)	(p->n_un.n_name == 0 || p->n_un.n_name[0] == 0)

#ifdef _NLIST_DO_AOUT
int
__aout_fdnlist(fd, list)
	register File fd;
	register struct nlist *list;
{
	register struct nlist *p, *s;
	register char *strtab;
	register off_t stroff, symoff;
	register u_long symsize;
	register int nent, cc;
	int strsize;
	struct nlist nbuf[1024];
	struct exec exec;

	if (Seek(fd, 0, SEEK_SET) == -1 ||
	    Read(fd, &exec, sizeof(exec)) != sizeof(exec) ||
	    N_BADMAG(exec) || exec.a_syms == NULL)
		return (-1);

	symoff = N_SYMOFF(exec);
	symsize = exec.a_syms;
	stroff = symoff + symsize;

	/* Read in the size of the string table. */
	if (Seek(fd, N_STROFF(exec), SEEK_SET) == -1)
		return (-1);
	if (Read(fd, (char *)&strsize, sizeof(strsize)) != sizeof(strsize))
		return (-1);

	/*
	 * Read in the string table.  Since OpenBSD's malloc(3) returns
	 * memory to the system on free this does not cause bloat.
	 */
	strsize -= sizeof(strsize);
	if ((strtab = (char *)malloc(strsize)) == NULL)
		return (-1);
	if (Read(fd, strtab, strsize) != strsize)
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
	if (Seek(fd, symoff, SEEK_SET) == -1)
		return (-1);

	while (symsize > 0) {
		cc = MIN(symsize, sizeof(nbuf));
		if (Read(fd, nbuf, cc) != cc)
			break;
		symsize -= cc;
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
	free(strtab);
	return (nent);
}
#endif /* _NLIST_DO_AOUT */

#ifdef _NLIST_DO_ECOFF
#define check(off, size)	((off < 0) || (off + size > mappedsize))
#define	BAD			do { rv = -1; goto out; } while (0)
#define	BADFREE			do { rv = -1; goto freestr; } while (0)

int
__ecoff_fdnlist(fd, list)
	register File fd;
	register struct nlist *list;
{
	struct nlist *p;
	struct ecoff_exechdr exechdr;
	struct ecoff_symhdr symhdr;
	struct ecoff_extsym esym;
	char *strtab, *nlistname, *symtabname;
	int rv, nent, strsize, nesyms;

	rv = -3;

	if (Seek(fd, 0, SEEK_SET) == -1)
		BAD;

	/* Read in exec header and check magic nummber. */
	if (Read(fd, &exechdr, sizeof(exechdr)) != sizeof(exechdr))
		BAD;
	if (ECOFF_BADMAG(&exechdr))  
		BAD;

	/* Can't operate on stripped executables. *.
	if (exechdr.f.f_nsyms == 0)
		BAD;

	/* Read in symbol table header and check magic nummber. */
	if (Seek(fd, exechdr.f.f_symptr, SEEK_SET) == -1)
		BAD;
	if (Read(fd, &symhdr, sizeof(symhdr)) != sizeof(symhdr))
		BAD;
	if (ECOFF_BADMAG(&exechdr))
		BAD;

	/* Read in the string table. */
	if (Seek(fd, symhdr.cbSsExtOffset, SEEK_SET) == -1)
		BAD;
	strsize = symhdr.estrMax;
	if (!(strtab = (char *)malloc(strsize)))
		BAD;
	if (Read(fd, strtab, strsize) != strsize)
		BADFREE;

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

	/* Seek to symbol table. */
	if (Seek(fd, symhdr.cbExtOffset, SEEK_SET) == -1)
		BADFREE;

	/* Check each symbol against the list */
	nesyms = symhdr.esymMax;
	while (nesyms--) {
		if (Read(fd, &esym, sizeof (esym)) != sizeof (esym))
			BADFREE;
		symtabname = strtab + esym.es_strindex;
		for (p = list; !ISLAST(p); p++) {
			nlistname = p->n_un.n_name;
			if (*nlistname == '_')
				nlistname++;

			if (strcmp(symtabname, nlistname) == 0) {
				p->n_value = esym.es_value;
				p->n_type = N_EXT;		/* XXX */
				if (--nent <= 0)
					break;
			}
		}
	}
	rv = nent;

freestr:
	free(strtab);
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
	register File fd;
	register struct nlist *list;
{
	register struct nlist *p;
	register char *strtab = NULL;
	register Elf32_Off symoff = 0, symstroff = 0;
	register Elf32_Word symsize = 0, symstrsize = 0;
	register Elf32_Sword nent, cc, i;
	Elf32_Sym sbuf[1024];
	Elf32_Sym *s;
	Elf32_Ehdr ehdr;
	Elf32_Shdr *shdr = NULL;
	Elf32_Word shdr_size;
	int serrno;
	struct stat st;

	/* Make sure obj is OK */
	if (Seek(fd, 0, SEEK_SET) == -1 ||
	    Read(fd, &ehdr, sizeof(Elf32_Ehdr)) != sizeof(Elf32_Ehdr) ||
	    !__elf_is_okay__(&ehdr))
		return (-1);

	/*
	 * Clean out any left-over information for all valid entries.
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

	/* Calculate section header table size */
	shdr_size = ehdr.e_shentsize * ehdr.e_shnum;

	/* Alloc and read section header table */
	shdr = (Elf32_Shdr *)malloc((size_t)shdr_size);
	if (shdr == NULL || Seek(fd, ehdr.e_shoff, SEEK_SET) == -1 ||
	    Read(fd, shdr, shdr_size) != shdr_size)
		goto done;

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

	/* Alloc and read in string table */
	strtab = (char *)malloc(symstrsize);
	if (strtab == NULL || Seek(fd, symstroff, SEEK_SET) == -1 ||
	    Read(fd, strtab, symstrsize) != symstrsize)
		goto done;

	/*
	 * Don't process any further if object is stripped.
	 * ELFism -- dunno if stripped by looking at header
	 */
	if (symoff == 0 || Seek(fd, symoff, SEEK_SET) == -1)
		goto done;

	while (symsize > 0) {
		cc = MIN(symsize, sizeof(sbuf));
		if (Read(fd, sbuf, cc) != cc)
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
					if (--nent <= 0)
						break;
				}
			}
		}
	}
done:
	serrno = errno;
	if (shdr)
		free(shdr);
	if (strtab)
		free(strtab);
	errno = serrno;
	return (nent);
}
#endif /* _NLIST_DO_ELF */


static struct nlist_handlers {
	int	(*fn) __P((File fd, struct nlist *list));
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
	int n = -1, i, serrno;
	File f;
#ifdef _NLIST_DO_GZIP
	int nfd;

	if ((nfd = dup(fd)) == -1)
		return (-1);

	if ((f = gzdopen(nfd, "r")) == NULL)
		return (-1);
#else
	f = fd;
#endif /* _NLIST_DO_GZIP */

	for (i = 0; i < sizeof(nlist_fn)/sizeof(nlist_fn[0]); i++) {
		n = (nlist_fn[i].fn)(f, list);
		serrno = errno;
		if (n != -1)
			break;
	}

#ifdef _NLIST_DO_GZIP
	(void)gzclose(f);
#endif /* _NLIST_DO_GZIP */

	errno = serrno;
	return (n);
}

int
nlist(name, list)
	const char *name;
	struct nlist *list;
{
	int n, fd;

	fd = open(name, O_RDONLY);
	if (fd == -1)
		return (-1);
	n = __fdnlist(fd, list);
	(void)close(fd);
	return (n);
}
