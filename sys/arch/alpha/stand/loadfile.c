/*	$OpenBSD: loadfile.c,v 1.7 1998/09/04 17:03:24 millert Exp $	*/
/*	$NetBSD: loadfile.c,v 1.3 1997/04/06 08:40:59 cgd Exp $	*/

/*
 * Copyright (c) 1992, 1993
 *	The Regents of the University of California.  All rights reserved.
 *
 * This code is derived from software contributed to Berkeley by
 * Ralph Campbell.
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
 *
 *	@(#)boot.c	8.1 (Berkeley) 6/10/93
 */

#define	ELFSIZE		64

#include <lib/libkern/libkern.h>
#include <lib/libsa/stand.h>

#include <sys/param.h>
#include <sys/exec.h>
#include <sys/exec_ecoff.h>
#include <sys/exec_elf.h>

#include <machine/rpb.h>
#include <machine/prom.h>

#include <ddb/db_aout.h>

#define _KERNEL
#include "include/pte.h"

#ifdef ALPHA_BOOT_ECOFF
static int coff_exec __P((int, struct ecoff_exechdr *, u_int64_t *));
#endif
#ifdef ALPHA_BOOT_ELF
static int elf_exec __P((int, Elf_Ehdr *, u_int64_t *));
#endif
int loadfile __P((char *, u_int64_t *));

vm_offset_t ffp_save, ptbr_save, esym;

/*
 * Open 'filename', read in program and return the entry point or -1 if error.
 */
int
loadfile(fname, entryp)
	char *fname;
	u_int64_t *entryp;
{
	struct devices *dp;
	union {
#ifdef ALPHA_BOOT_ECOFF
		struct ecoff_exechdr coff;
#endif
#ifdef ALPHA_BOOT_ELF
		Elf_Ehdr elf;
#endif
	} hdr;
	int fd, rval;

	(void)printf("\nLoading %s...\n", fname);

	/* Open the file. */
	rval = 1;
	if ((fd = open(fname, 0)) < 0) {
		(void)printf("open %s: %s\n", fname, strerror(errno));
		goto err;
	}

	/* Read the exec header. */
	if (read(fd, &hdr, sizeof(hdr)) != sizeof(hdr)) {
		(void)printf("read header: %s\n", strerror(errno));
		goto err;
	}

#ifdef ALPHA_BOOT_ECOFF
	if (!ECOFF_BADMAG(&hdr.coff)) {
		rval = coff_exec(fd, &hdr.coff, entryp);
	} else
#endif
#ifdef ALPHA_BOOT_ELF
	if (memcmp(Elf_e_ident, hdr.elf.e_ident, Elf_e_siz) == 0) {
		rval = elf_exec(fd, &hdr.elf, entryp);
	} else
#endif
	{
		(void)printf("%s: unknown executable format\n", fname);
	}

err:
	if (fd >= 0)
		(void)close(fd);
	return (rval);
}

#ifdef ALPHA_BOOT_ECOFF
static int
coff_exec(fd, coff, entryp)
	int fd;
	struct ecoff_exechdr *coff;
	u_int64_t *entryp;
{
	struct nlist *symtab;
	struct ecoff_symhdr symhdr;
	struct ecoff_extsym sym;
	int symsize, nesyms;

	/* Read in text. */
	(void)printf("%lu", coff->a.tsize);
	if (lseek(fd, ECOFF_TXTOFF(coff), SEEK_SET) == -1) {
		(void)printf("seek to text: %s\n", strerror(errno));
		return (1);
	}
	if (read(fd, (void *)coff->a.text_start, coff->a.tsize) !=
	    coff->a.tsize) {
		(void)printf("read text: %s\n", strerror(errno));
		return (1);
	}

	/* Read in data. */
	if (coff->a.dsize != 0) {
		(void)printf("+%lu", coff->a.dsize);
		if (read(fd, (void *)coff->a.data_start, coff->a.dsize) !=
		    coff->a.dsize) {
			(void)printf("read data: %s\n", strerror(errno));
			return (1);
		}
	}


	/* Zero out bss. */
	if (coff->a.bsize != 0) {
		(void)printf("+%lu", coff->a.bsize);
		bzero((void *)coff->a.bss_start, coff->a.bsize);
	}

	ffp_save = coff->a.text_start + coff->a.tsize;
	if (ffp_save < coff->a.data_start + coff->a.dsize)
		ffp_save = coff->a.data_start + coff->a.dsize;
	if (ffp_save < coff->a.bss_start + coff->a.bsize)
		ffp_save = coff->a.bss_start + coff->a.bsize;

	/* Get symbols if there for DDB's sake.  */
	if (coff->f.f_symptr && coff->f.f_nsyms) {
		if (lseek(fd, coff->f.f_symptr, SEEK_SET) == -1) {
			printf("seek to symbol table header: %s\n",
			    strerror(errno));
			return (1);
		}
		if (read(fd, &symhdr, coff->f.f_nsyms) != coff->f.f_nsyms) {
			printf("read symbol table header: %s\n",
			    strerror(errno));
			return (1);
		}
		*(long *)ffp_save = symsize =
		    symhdr.esymMax * sizeof(struct nlist);
		ffp_save += sizeof(long);
		printf("+[%d", symsize);
		symtab = (struct nlist *)ffp_save;
		bzero(symtab, symsize);
		if (lseek(fd, symhdr.cbExtOffset, SEEK_SET) == -1) {
			printf("lseek to symbol table: %s\n", strerror(errno));
			return (1);
		}
		nesyms = symhdr.esymMax;
		while (nesyms--) {
			if (read(fd, &sym, sizeof(sym)) != sizeof(sym)) {
				printf("read symbols: %s\n", strerror(errno));
				return (1);
			}
			symtab->n_un.n_strx = sym.es_strindex + sizeof(int);
			symtab->n_value = sym.es_value;
			symtab->n_type = N_EXT;
			if (sym.es_class == 1)		/* scText */
				symtab->n_type != N_TEXT;
			symtab++;
		}
		ffp_save += symsize;
		*(int *)ffp_save = symhdr.estrMax + sizeof(int);
		ffp_save += sizeof(int);
		if (lseek(fd, symhdr.cbSsExtOffset, SEEK_SET) == -1) {
			printf("seek to string table: %s\n", strerror(errno));
			return (1);
		}
		if (read(fd, (char *)ffp_save, symhdr.estrMax) !=
		    symhdr.estrMax) {
			printf("read string table: %s\n", strerror(errno));
			return (1);
		}
		ffp_save += symhdr.estrMax;
		printf("+%d]", symhdr.estrMax);
		esym = ((ffp_save + sizeof(int) - 1) & ~(sizeof(int) - 1));
	}

	ffp_save = ALPHA_K0SEG_TO_PHYS((ffp_save + PGOFSET & ~PGOFSET)) >>
	    PGSHIFT;
	ffp_save += 2;		/* XXX OSF/1 does this, no idea why. */

	(void)printf("\n");
	*entryp = coff->a.entry;
	return (0);
}
#endif /* ALPHA_BOOT_ECOFF */

#ifdef ALPHA_BOOT_ELF
static int
elf_exec(fd, elf, entryp)
	int fd;
	Elf_Ehdr *elf;
	u_int64_t *entryp;
{
	int i;
	int first = 1;

	for (i = 0; i < elf->e_phnum; i++) {
		Elf_Phdr phdr;
		(void)lseek(fd, elf->e_phoff + sizeof(phdr) * i, SEEK_SET);
		if (read(fd, (void *)&phdr, sizeof(phdr)) != sizeof(phdr)) {
			(void)printf("read phdr: %s\n", strerror(errno));
			return (1);
		}
		if (phdr.p_type != Elf_pt_load ||
		    (phdr.p_flags & (Elf_pf_w|Elf_pf_x)) == 0)
			continue;

		/* Read in segment. */
		(void)printf("%s%lu", first ? "" : "+", phdr.p_filesz);
		(void)lseek(fd, phdr.p_offset, SEEK_SET);
		if (read(fd, (void *)phdr.p_vaddr, phdr.p_filesz) !=
		    phdr.p_filesz) {
			(void)printf("read text: %s\n", strerror(errno));
			return (1);
		}
		if (first || ffp_save < phdr.p_vaddr + phdr.p_memsz)
			ffp_save = phdr.p_vaddr + phdr.p_memsz;

		/* Zero out bss. */
		if (phdr.p_filesz < phdr.p_memsz) {
			(void)printf("+%lu", phdr.p_memsz - phdr.p_filesz);
			bzero(phdr.p_vaddr + phdr.p_filesz,
			    phdr.p_memsz - phdr.p_filesz);
		}
		first = 0;
	}

	ffp_save = ALPHA_K0SEG_TO_PHYS((ffp_save + PGOFSET & ~PGOFSET)) >> PGSHIFT;
	ffp_save += 2;		/* XXX OSF/1 does this, no idea why. */

	(void)printf("\n");
	*entryp = elf->e_entry;
	return (0);
}
#endif /* ALPHA_BOOT_ELF */
