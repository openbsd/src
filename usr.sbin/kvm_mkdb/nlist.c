/*	$OpenBSD: nlist.c,v 1.18 1999/03/24 05:25:57 millert Exp $	*/

/*-
 * Copyright (c) 1990, 1993
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

#ifndef lint
#if 0
static char sccsid[] = "from: @(#)nlist.c	8.1 (Berkeley) 6/6/93";
#else
static char *rcsid = "$OpenBSD: nlist.c,v 1.18 1999/03/24 05:25:57 millert Exp $";
#endif
#endif /* not lint */

#include <sys/param.h>

#include <a.out.h>
#include <db.h>
#include <err.h>
#include <errno.h>
#include <fcntl.h>
#include <kvm.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "extern.h"

#include <sys/mman.h>
#include <sys/stat.h>
#include <sys/file.h>
#include <sys/sysctl.h>

#ifdef _NLIST_DO_ELF
#include <elf_abi.h>
#endif

#ifdef _NLIST_DO_ECOFF
#include <sys/exec_ecoff.h>
#endif

typedef struct nlist NLIST;
#define	_strx	n_un.n_strx
#define	_name	n_un.n_name

static char *kfile;
static char *fmterr;

#if defined(_NLIST_DO_AOUT)

static u_long get_kerntext __P((char *kfn, u_int magic));

int
__aout_knlist(fd, db)
	int fd;
	DB *db;
{
	register int nsyms;
	struct exec ebuf;
	FILE *fp;
	NLIST nbuf;
	DBT data, key;
	int nr, strsize;
	size_t len;
	u_long kerntextoff;
	size_t snamesize = 0;
	char *strtab, buf[1024], *sname, *p;

	/* Read in exec structure. */
	nr = read(fd, &ebuf, sizeof(struct exec));
	if (nr != sizeof(struct exec)) {
		fmterr = "no exec header";
		return (1);
	}

	/* Check magic number and symbol count. */
	if (N_BADMAG(ebuf)) {
		fmterr = "bad magic number";
		return (1);
	}

	/* Must have a symbol table. */
	if (!ebuf.a_syms) {
		fmterr = "stripped";
		return (-1);
	}

	/* Seek to string table. */
	if (lseek(fd, N_STROFF(ebuf), SEEK_SET) == -1) {
		fmterr = "corrupted string table";
		return (-1);
	}

	/* Read in the size of the string table. */
	nr = read(fd, (char *)&strsize, sizeof(strsize));
	if (nr != sizeof(strsize)) {
		fmterr = "no symbol table";
		return (-1);
	}

	/* Read in the string table. */
	strsize -= sizeof(strsize);
	if (!(strtab = malloc(strsize)))
		errx(1, "cannot allocate %d bytes for string table", strsize);
	if ((nr = read(fd, strtab, strsize)) != strsize) {
		fmterr = "corrupted symbol table";
		return (-1);
	}

	/* Seek to symbol table. */
	if (!(fp = fdopen(fd, "r")))
		err(1, "%s", kfile);
	if (fseek(fp, N_SYMOFF(ebuf), SEEK_SET) == -1) {
		warn("fseek %s", kfile);
		return (-1);
	}
	
	data.data = (u_char *)&nbuf;
	data.size = sizeof(NLIST);

	kerntextoff = get_kerntext(kfile, N_GETMAGIC(ebuf));

	/* Read each symbol and enter it into the database. */
	nsyms = ebuf.a_syms / sizeof(struct nlist);
	sname = NULL;
	while (nsyms--) {
		if (fread((char *)&nbuf, sizeof (NLIST), 1, fp) != 1) {
			if (feof(fp))
				fmterr = "corrupted symbol table";
			else
				warn("%s", kfile);
			return (-1);
		}
		if (!nbuf._strx || (nbuf.n_type & N_STAB))
			continue;

		/* If the symbol does not start with '_', add one */
		p = strtab + nbuf._strx - sizeof(int);
		if (*p != '_') {
			len = strlen(p) + 1;
			if (len >= snamesize)
				sname = realloc(sname, len + 1024);
			if (sname == NULL)
				errx(1, "cannot allocate memory");
			*sname = '_';
			strcpy(sname+1, p);
			key.data = (u_char *)sname;
			key.size = len;
		} else {
			key.data = (u_char *)p;
			key.size = strlen((char *)key.data);
		}
		if (db->put(db, &key, &data, 0))
			err(1, "record enter");

		if (strcmp((char *)key.data, VRS_SYM) == 0) {
			long cur_off = -1;

			if (ebuf.a_data && ebuf.a_text > __LDPGSZ) {
				/*
				 * Calculate offset relative to a normal
				 * (non-kernel) a.out.  Kerntextoff is where the
				 * kernel is really loaded; N_TXTADDR is where
				 * a normal file is loaded.  From there, locate
				 * file offset in text or data.
				 */
				long voff;

				voff = nbuf.n_value-kerntextoff+N_TXTADDR(ebuf);
				if ((nbuf.n_type & N_TYPE) == N_TEXT)
					voff += N_TXTOFF(ebuf)-N_TXTADDR(ebuf);
				else
					voff += N_DATOFF(ebuf)-N_DATADDR(ebuf);
				cur_off = ftell(fp);
				if (fseek(fp, voff, SEEK_SET) == -1) {
					fmterr = "corrupted string table";
					return (-1);
				}

				/*
				 * Read version string up to, and including
				 * newline.  This code assumes that a newline
				 * terminates the version line.
				 */
				if (fgets(buf, sizeof(buf), fp) == NULL) {
					fmterr = "corrupted string table";
					return (-1);
				}
			} else {
				/*
				 * No data segment and text is __LDPGSZ.
				 * This must be /dev/ksyms or a look alike.
				 * Use sysctl() to get version since we
				 * don't have real text or data.
				 */
				int mib[2];

				mib[0] = CTL_KERN;
				mib[1] = KERN_VERSION;
				len = sizeof(buf);
				if (sysctl(mib, 2, buf, &len, NULL, 0) == -1) {
					err(1, "sysctl can't find kernel "
					    "version string");
				}
				if ((p = strchr(buf, '\n')) != NULL)
					*(p+1) = '\0';
			}
			key.data = (u_char *)VRS_KEY;
			key.size = sizeof(VRS_KEY) - 1;
			data.data = (u_char *)buf;
			data.size = strlen(buf);
			if (db->put(db, &key, &data, 0))
				err(1, "record enter");

			/* Restore to original values. */
			data.data = (u_char *)&nbuf;
			data.size = sizeof(NLIST);
			if (cur_off != -1 && fseek(fp, cur_off, SEEK_SET) == -1) {
				fmterr = "corrupted string table";
				return (-1);
			}
		}
	}
	(void)fclose(fp);
	return (0);
}

/*
 * XXX: Using this value from machine/param.h introduces a
 * XXX: machine dependency on this program, so /usr can not
 * XXX: be shared between (i.e.) several m68k machines.
 * Instead of compiling in KERNTEXTOFF or KERNBASE, try to
 * determine the text start address from a standard symbol.
 * For backward compatibility, use the old compiled-in way
 * when the standard symbol name is not found.
 */
#ifndef KERNTEXTOFF
#define KERNTEXTOFF KERNBASE
#endif

static u_long
get_kerntext(name, magic)
	char *name;
	u_int magic;
{
	NLIST nl[2];

	bzero((caddr_t)nl, sizeof(nl));
	nl[0]._name = "_kernel_text";

	if (nlist(name, nl) != 0)
		return (KERNTEXTOFF);

	return (nl[0].n_value);
}

#endif /* _NLIST_DO_AOUT */

#ifdef _NLIST_DO_ELF
int
__elf_knlist(fd, db)
	int fd;
	DB *db;
{
	register caddr_t strtab;
	register off_t symstroff, symoff;
	register u_long symsize;
	register u_long kernvma, kernoffs;
	register int i;
	Elf32_Sym sbuf;
	size_t symstrsize;
	char *shstr, buf[1024];
	Elf32_Ehdr eh;
	Elf32_Shdr *sh = NULL;
	DBT data, key;
	NLIST nbuf;
	FILE *fp;

	if ((fp = fdopen(fd, "r")) < 0)
		err(1, "%s", kfile);

	if (fseek(fp, (off_t)0, SEEK_SET) == -1 ||
	    fread(&eh, sizeof(eh), 1, fp) != 1 ||
	    !IS_ELF(eh))
		return (1);

	sh = (Elf32_Shdr *)malloc(sizeof(Elf32_Shdr) * eh.e_shnum);
	if (sh == NULL)
		errx(1, "cannot allocate %d bytes for symbol header",
		    sizeof(Elf32_Shdr) * eh.e_shnum);

	if (fseek (fp, eh.e_shoff, SEEK_SET) < 0) {
		fmterr = "no exec header";
		return (-1);
	}

	if (fread(sh, sizeof(Elf32_Shdr) * eh.e_shnum, 1, fp) != 1) {
		fmterr = "no exec header";
		return (-1);
	}

	shstr = (char *)malloc(sh[eh.e_shstrndx].sh_size);
	if (shstr == NULL)
		errx(1, "cannot allocate %d bytes for symbol string",
		    sh[eh.e_shstrndx].sh_size);
	if (fseek (fp, sh[eh.e_shstrndx].sh_offset, SEEK_SET) < 0) {
		fmterr = "corrupt file";
		return (-1);
	}
	if (fread(shstr, sh[eh.e_shstrndx].sh_size, 1, fp) != 1) {
		fmterr = "corrupt file";
		return (-1);
	}

	for (i = 0; i < eh.e_shnum; i++) {
		if (strcmp (shstr + sh[i].sh_name, ".strtab") == 0) {
			symstroff = sh[i].sh_offset;
			symstrsize = sh[i].sh_size;
		}
		else if (strcmp (shstr + sh[i].sh_name, ".symtab") == 0) {
			symoff = sh[i].sh_offset;
			symsize = sh[i].sh_size;
		}
		else if (strcmp (shstr + sh[i].sh_name, ".text") == 0) {
			kernvma = sh[i].sh_addr;
			kernoffs = sh[i].sh_offset;
		}
	}

	
	/* Check for files too large to mmap. */
	/* XXX is this really possible? */
	if (symstrsize > SIZE_T_MAX) {
		fmterr = "corrupt file";
		return (-1);
	}
	/*
	 * Map string table into our address space.  This gives us
	 * an easy way to randomly access all the strings, without
	 * making the memory allocation permanent as with malloc/free
	 * (i.e., munmap will return it to the system).
	 */
	strtab = mmap(NULL, (size_t)symstrsize, PROT_READ,
	    MAP_PRIVATE|MAP_FILE, fileno(fp), symstroff);
	if (strtab == (char *)-1) {
		fmterr = "corrupt file";
		return (-1);
	}

	if (fseek(fp, symoff, SEEK_SET) == -1) {
		fmterr = "corrupt file";
		return (-1);
	}

	data.data = (u_char *)&nbuf;
	data.size = sizeof(NLIST);

	/* Read each symbol and enter it into the database. */
	while (symsize > 0) {
		symsize -= sizeof(Elf32_Sym);
		if (fread((char *)&sbuf, sizeof(sbuf), 1, fp) != 1) {
			if (feof(fp))
				fmterr = "corrupted symbol table";
			else
				warn(kfile);
			return (-1);
		}
		if (!sbuf.st_name)
			continue;

		nbuf.n_value = sbuf.st_value;

		/*XXX type conversion is pretty rude... */
		switch(ELF32_ST_TYPE(sbuf.st_info)) {
		case STT_NOTYPE:
			nbuf.n_type = N_UNDF;
			break;
		case STT_FUNC:
			nbuf.n_type = N_TEXT;
			break;
		case STT_OBJECT:
			nbuf.n_type = N_DATA;
			break;
		}
		if(ELF32_ST_BIND(sbuf.st_info) == STB_LOCAL)
			nbuf.n_type = N_EXT;

		if(eh.e_machine == EM_MIPS) {
			*buf = '_';
			strcpy(buf+1,strtab + sbuf.st_name);
			key.data = (u_char *)buf;
		}
		else {
			key.data = (u_char *)(strtab + sbuf.st_name);
		}
		key.size = strlen((char *)key.data);
		if (db->put(db, &key, &data, 0))
			err(1, "record enter");

		if (strcmp((char *)key.data, VRS_SYM) == 0) {
			long cur_off, voff;
			/*
			 * Calculate offset to the version string in the
			 * file.  kernvma is where the kernel is really
			 * loaded; kernoffs is where in the file it starts.
			 */
			voff = nbuf.n_value - kernvma + kernoffs;
			cur_off = ftell(fp);
			if (fseek(fp, voff, SEEK_SET) == -1) {
				fmterr = "corrupted string table";
				return (-1);
			}

			/*
			 * Read version string up to, and including newline.
			 * This code assumes that a newline terminates the
			 * version line.
			 */
			if (fgets(buf, sizeof(buf), fp) == NULL) {
				fmterr = "corrupted string table";
				return (-1);
			}

			key.data = (u_char *)VRS_KEY;
			key.size = sizeof(VRS_KEY) - 1;
			data.data = (u_char *)buf;
			data.size = strlen(buf);
			if (db->put(db, &key, &data, 0))
				err(1, "record enter");

			/* Restore to original values. */
			data.data = (u_char *)&nbuf;
			data.size = sizeof(NLIST);
			if (fseek(fp, cur_off, SEEK_SET) == -1) {
				fmterr = "corrupted string table";
				return (-1);
			}
		}
	}
	munmap(strtab, symstrsize);
	(void)fclose(fp);
	return (0);
}
#endif /* _NLIST_DO_ELF */

#ifdef _NLIST_DO_ECOFF

#define check(off, size)	((off < 0) || (off + size > mappedsize))
#define BAD			do { rv = -1; goto out; } while (0)
#define BADUNMAP		do { rv = -1; goto unmap; } while (0)
#define ECOFF_INTXT(p, e)	((p) > (e)->a.text_start && \
				 (p) < (e)->a.text_start + (e)->a.tsize)
#define ECOFF_INDAT(p, e)	((p) > (e)->a.data_start && \
				 (p) < (e)->a.data_start + (e)->a.dsize)

int
__ecoff_knlist(fd, db)
	int fd;
	DB *db;
{
	struct ecoff_exechdr *exechdrp;
	struct ecoff_symhdr *symhdrp;
	struct ecoff_extsym *esyms;
	struct stat st;
	char *mappedfile, *cp;
	size_t mappedsize;
	u_long symhdroff, extstroff, off;
	u_int symhdrsize;
	int rv = 0;
	long i, nesyms;
	DBT data, key;
	NLIST nbuf;
	char *sname = NULL;
	size_t len, snamesize = 0;

	if (fstat(fd, &st) < 0)
		err(1, "can't stat %s", kfile);
	if (st.st_size > SIZE_T_MAX) {
		fmterr = "file too large";
		BAD;
	}

	mappedsize = st.st_size;
	mappedfile = mmap(NULL, mappedsize, PROT_READ, MAP_COPY|MAP_FILE, fd, 0);
	if (mappedfile == MAP_FAILED) {
		fmterr = "unable to mmap";
		BAD;
	}

	if (check(0, sizeof *exechdrp))
		BADUNMAP;
	exechdrp = (struct ecoff_exechdr *)&mappedfile[0];

	if (ECOFF_BADMAG(exechdrp))
		BADUNMAP;

	symhdroff = exechdrp->f.f_symptr;
	symhdrsize = exechdrp->f.f_nsyms;
	if (symhdrsize == 0) {
		fmterr = "stripped";
		return (-1);
	}

	if (check(symhdroff, sizeof *symhdrp) ||
	    sizeof *symhdrp != symhdrsize)
		BADUNMAP;
	symhdrp = (struct ecoff_symhdr *)&mappedfile[symhdroff];

	nesyms = symhdrp->esymMax;
	if (check(symhdrp->cbExtOffset, nesyms * sizeof *esyms))
		BADUNMAP;
	esyms = (struct ecoff_extsym *)&mappedfile[symhdrp->cbExtOffset];
	extstroff = symhdrp->cbSsExtOffset;

	data.data = (u_char *)&nbuf;
	data.size = sizeof(NLIST);

	for (sname = NULL, i = 0; i < nesyms; i++) {
		/* Need to prepend a '_' */
		len = strlen(&mappedfile[extstroff + esyms[i].es_strindex]) + 1;
		if (len >= snamesize)
			sname = realloc(sname, len + 1024);
		if (sname == NULL)
			errx(1, "cannot allocate memory");
		*sname = '_';
		strcpy(sname+1, &mappedfile[extstroff + esyms[i].es_strindex]);

		/* Fill in NLIST */
		bzero(&nbuf, sizeof(nbuf));
		nbuf.n_value = esyms[i].es_value;
		nbuf.n_type = N_EXT;		/* XXX */

		/* Store entry in db */
		key.data = (u_char *)sname;
		key.size = strlen(sname);
		if (db->put(db, &key, &data, 0))
			err(1, "record enter");

		if (strcmp(sname, VRS_SYM) == 0) {
			key.data = (u_char *)VRS_KEY;
			key.size = sizeof(VRS_KEY) - 1;

			/* Version string may be in either text or data segs */
			if (ECOFF_INTXT(nbuf.n_value, exechdrp))
				off = nbuf.n_value - exechdrp->a.text_start +
				    ECOFF_TXTOFF(exechdrp);
			else if (ECOFF_INDAT(nbuf.n_value, exechdrp))
				off = nbuf.n_value - exechdrp->a.data_start +
				    ECOFF_DATOFF(exechdrp);
			else
				err(1, "unable to find version string");

			/* Version string should end in newline but... */
			data.data = &mappedfile[off];
			if ((cp = strchr(data.data, '\n')) != NULL)
				data.size = cp - (char *)data.data;
			else
				data.size = strlen((char *)data.data);

			if (db->put(db, &key, &data, 0))
				err(1, "record enter");

			/* Restore to original values */
			data.data = (u_char *)&nbuf;
			data.size = sizeof(nbuf);
		}
		
	}

unmap:
	munmap(mappedfile, mappedsize);
out:
	return (rv);
}
#endif /* _NLIST_DO_ECOFF */

static struct knlist_handlers {
	int	(*fn) __P((int fd, DB *db));
} nlist_fn[] = {
#ifdef _NLIST_DO_AOUT
	{ __aout_knlist },
#endif
#ifdef _NLIST_DO_ELF
	{ __elf_knlist },
#endif
#ifdef _NLIST_DO_ECOFF
	{ __ecoff_knlist },
#endif
};

int
create_knlist(name, fd, db)
	char *name;
	int fd;
	DB *db;
{
	int i, error;

	for (i = 0; i < sizeof(nlist_fn)/sizeof(nlist_fn[0]); i++) {
		fmterr = NULL;
		kfile = name;
		/* rval of 1 means wrong executable type */
		if ((error = (nlist_fn[i].fn)(fd, db)) != 1)
			break;
	}
	if (fmterr != NULL)
		warnx("%s: %s: %s", kfile, fmterr, strerror(EFTYPE));

	return(error);
}
