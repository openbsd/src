/*	$OpenBSD: nlist.c,v 1.8 1998/08/19 06:47:54 millert Exp $	*/

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
static char *rcsid = "$OpenBSD: nlist.c,v 1.8 1998/08/19 06:47:54 millert Exp $";
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

#ifdef _NLIST_DO_ELF
#include <elf_abi.h>
#endif

typedef struct nlist NLIST;
#define	_strx	n_un.n_strx
#define	_name	n_un.n_name

#define	badfmt(str)	errx(1, "%s: %s: %s", kfile, str, strerror(EFTYPE))
static char *kfile;
static char *fmterr;

#if defined(_NLIST_DO_AOUT)

static void badread __P((int, char *));
static u_long get_kerntext __P((char *kfn, u_int magic));

int
__aout_knlist(name, db)
	char *name;
	DB *db;
{
	register int nsyms;
	struct exec ebuf;
	FILE *fp;
	NLIST nbuf;
	DBT data, key;
	int fd, nr, strsize;
	u_long kerntextoff;
	char *strtab, buf[1024];

	kfile = name;
	if ((fd = open(name, O_RDONLY, 0)) < 0)
		err(1, "can't open %s", name);

	/* Read in exec structure. */
	nr = read(fd, &ebuf, sizeof(struct exec));
	if (nr != sizeof(struct exec)) {
		fmterr = "no exec header";
		return (-1);
	}

	/* Check magic number and symbol count. */
	if (N_BADMAG(ebuf)) {
		fmterr = "bad magic number";
		return (-1);
	}

	/* Must have a symbol table. */
	if (!ebuf.a_syms)
		badfmt("stripped");

	/* Seek to string table. */
	if (lseek(fd, N_STROFF(ebuf), SEEK_SET) == -1)
		badfmt("corrupted string table");

	/* Read in the size of the symbol table. */
	nr = read(fd, (char *)&strsize, sizeof(strsize));
	if (nr != sizeof(strsize))
		badread(nr, "no symbol table");

	/* Read in the string table. */
	strsize -= sizeof(strsize);
	if (!(strtab = malloc(strsize)))
		errx(1, "cannot allocate memory");
	if ((nr = read(fd, strtab, strsize)) != strsize)
		badread(nr, "corrupted symbol table");

	/* Seek to symbol table. */
	if (!(fp = fdopen(fd, "r")))
		err(1, "%s", name);
	if (fseek(fp, N_SYMOFF(ebuf), SEEK_SET) == -1)
		err(1, "%s", name);
	
	data.data = (u_char *)&nbuf;
	data.size = sizeof(NLIST);

	kerntextoff = get_kerntext(name, N_GETMAGIC(ebuf));

	/* Read each symbol and enter it into the database. */
	nsyms = ebuf.a_syms / sizeof(struct nlist);
	while (nsyms--) {
		if (fread((char *)&nbuf, sizeof (NLIST), 1, fp) != 1) {
			if (feof(fp))
				badfmt("corrupted symbol table");
			err(1, "%s", name);
		}
		if (!nbuf._strx || nbuf.n_type&N_STAB)
			continue;

		key.data = (u_char *)strtab + nbuf._strx - sizeof(long);
		key.size = strlen((char *)key.data);
		if (db->put(db, &key, &data, 0))
			err(1, "record enter");

		if (strcmp((char *)key.data, VRS_SYM) == 0) {
			long cur_off, voff;
			/*
			 * Calculate offset relative to a normal (non-kernel)
			 * a.out.  Kerntextoff is where the kernel is really
			 * loaded; N_TXTADDR is where a normal file is loaded.
			 * From there, locate file offset in text or data.
			 */
			voff = nbuf.n_value - kerntextoff + N_TXTADDR(ebuf);
			if ((nbuf.n_type & N_TYPE) == N_TEXT)
				voff += N_TXTOFF(ebuf) - N_TXTADDR(ebuf);
			else
				voff += N_DATOFF(ebuf) - N_DATADDR(ebuf);
			cur_off = ftell(fp);
			if (fseek(fp, voff, SEEK_SET) == -1)
				badfmt("corrupted string table");

			/*
			 * Read version string up to, and including newline.
			 * This code assumes that a newline terminates the
			 * version line.
			 */
			if (fgets(buf, sizeof(buf), fp) == NULL)
				badfmt("corrupted string table");

			key.data = (u_char *)VRS_KEY;
			key.size = sizeof(VRS_KEY) - 1;
			data.data = (u_char *)buf;
			data.size = strlen(buf);
			if (db->put(db, &key, &data, 0))
				err(1, "record enter");

			/* Restore to original values. */
			data.data = (u_char *)&nbuf;
			data.size = sizeof(NLIST);
			if (fseek(fp, cur_off, SEEK_SET) == -1)
				badfmt("corrupted string table");
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

static void
badread(nr, p)
	int nr;
	char *p;
{
	if (nr < 0)
		err(1, "%s", kfile);
	badfmt(p);
}

#endif /* _NLIST_DO_AOUT */

#ifdef _NLIST_DO_ELF
int
__elf_knlist(name, db)
	char *name;
	DB *db;
{
	register struct nlist *p;
	register caddr_t strtab;
	register off_t symstroff, symoff;
	register u_long symsize;
	register u_long kernvma, kernoffs;
	register int cc, i;
	Elf32_Sym sbuf;
	Elf32_Sym *s;
	size_t symstrsize;
	char *shstr, buf[1024];
	Elf32_Ehdr eh;
	Elf32_Shdr *sh = NULL;
	struct stat st;
	DBT data, key;
	NLIST nbuf;
	FILE *fp;

	kfile = name;
	if ((fp = fopen(name, "r")) < 0)
		err(1, "%s", name);

	if (fseek(fp, (off_t)0, SEEK_SET) == -1 ||
	    fread(&eh, sizeof(eh), 1, fp) != 1 ||
	    !IS_ELF(eh))
		return (-1);

	sh = (Elf32_Shdr *)malloc(sizeof(Elf32_Shdr) * eh.e_shnum);
	if (sh == NULL)
		errx(1, "cannot allocate memory");

	if (fseek (fp, eh.e_shoff, SEEK_SET) < 0)
		badfmt("no exec header");

	if (fread(sh, sizeof(Elf32_Shdr) * eh.e_shnum, 1, fp) != 1)
		badfmt("no exec header");

	shstr = (char *)malloc(sh[eh.e_shstrndx].sh_size);
	if (shstr == NULL)
		errx(1, "cannot allocate memory");
	if (fseek (fp, sh[eh.e_shstrndx].sh_offset, SEEK_SET) < 0)
		badfmt("corrupt file");
	if (fread(shstr, sh[eh.e_shstrndx].sh_size, 1, fp) != 1)
		badfmt("corrupt file");

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
		badfmt("corrupt file");
	}
	/*
	 * Map string table into our address space.  This gives us
	 * an easy way to randomly access all the strings, without
	 * making the memory allocation permanent as with malloc/free
	 * (i.e., munmap will return it to the system).
	 */
	strtab = mmap(NULL, (size_t)symstrsize, PROT_READ,
	    MAP_PRIVATE|MAP_FILE, fileno(fp), symstroff);
	if (strtab == (char *)-1)
		badfmt("corrupt file");

	if (fseek(fp, symoff, SEEK_SET) == -1)
		badfmt("corrupt file");

	data.data = (u_char *)&nbuf;
	data.size = sizeof(NLIST);

	/* Read each symbol and enter it into the database. */
	while (symsize > 0) {
		symsize -= sizeof(Elf32_Sym);
		if (fread((char *)&sbuf, sizeof(sbuf), 1, fp) != 1) {
			if (feof(fp))
				badfmt("corrupted symbol table");
			err(1, "%s", name);
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
			if (fseek(fp, voff, SEEK_SET) == -1)
				badfmt("corrupted string table");

			/*
			 * Read version string up to, and including newline.
			 * This code assumes that a newline terminates the
			 * version line.
			 */
			if (fgets(buf, sizeof(buf), fp) == NULL)
				badfmt("corrupted string table");

			key.data = (u_char *)VRS_KEY;
			key.size = sizeof(VRS_KEY) - 1;
			data.data = (u_char *)buf;
			data.size = strlen(buf);
			if (db->put(db, &key, &data, 0))
				err(1, "record enter");

			/* Restore to original values. */
			data.data = (u_char *)&nbuf;
			data.size = sizeof(NLIST);
			if (fseek(fp, cur_off, SEEK_SET) == -1)
				badfmt("corrupted string table");
		}
	}
	munmap(strtab, symstrsize);
	(void)fclose(fp);
	return (0);
}
#endif /* _NLIST_DO_ELF */

#ifdef _NLIST_DO_ECOFF
int
__ecoff_knlist(name, db)
	char *name;
	DB *db;
{
	return (-1);
}
#endif /* _NLIST_DO_ECOFF */

static struct knlist_handlers {
	int	(*fn) __P((char *name, DB *db));
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

void
create_knlist(name, db)
	char *name;
	DB *db;
{
	int i, error;

	for (i = 0; i < sizeof(nlist_fn)/sizeof(nlist_fn[0]); i++) {
		fmterr = NULL;
		if ((error = (nlist_fn[i].fn)(name, db)) == 0)
			break;
	}
	if (fmterr != NULL)
		badfmt(fmterr);
	if (error)
		errx(1, "cannot determine executable type of %s", name);
}
