/*	$OpenBSD: loadfile.c,v 1.17 2005/08/10 16:58:42 todd Exp $	*/
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

#ifdef ALPHA_BOOT_ELF
static int elf_exec(int, Elf64_Ehdr *, u_int64_t *);
#endif
int loadfile(char *, u_int64_t *);

paddr_t ffp_save, ptbr_save;
vaddr_t ssym, esym;

#define WARN(...)

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
#ifdef ALPHA_BOOT_ELF
		Elf64_Ehdr elf;
#endif
	} hdr;
	int fd, rval;

	(void)printf("Loading %s...\n", fname);

	/* Open the file. */
	rval = 1;
	if ((fd = open(fname, 0)) < 0) {
		WARN(("open %s: errno %d\n", fname, errno));
		goto err;
	}

	/* Read the exec header. */
	if (read(fd, &hdr, sizeof(hdr)) != sizeof(hdr)) {
		WARN(("read header: %s\n", strerror(errno)));
		goto err;
	}

#ifdef ALPHA_BOOT_ELF
	if (memcmp(ELFMAG, hdr.elf.e_ident, SELFMAG) == 0) {
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

#ifdef ALPHA_BOOT_ELF
static int
elf_exec(fd, elf, entryp)
	int fd;
	Elf64_Ehdr *elf;
	u_int64_t *entryp;
{
	int i;
	int first = 1, havesyms;
	Elf64_Shdr *shp;
	Elf64_Off off;
	size_t sz;

	for (i = 0; i < elf->e_phnum; i++) {
		Elf64_Phdr phdr;
		(void)lseek(fd, elf->e_phoff + sizeof(phdr) * i, SEEK_SET);
		if (read(fd, (void *)&phdr, sizeof(phdr)) != sizeof(phdr)) {
			WARN(("read phdr: %s\n", strerror(errno)));
			return (1);
		}
		if (phdr.p_type != PT_LOAD ||
		    (phdr.p_flags & (PF_W|PF_X)) == 0)
			continue;

		/* Read in segment. */
		(void)printf("%s%lu", first ? "" : "+", phdr.p_filesz);
		(void)lseek(fd, phdr.p_offset, SEEK_SET);
		if (read(fd, (void *)phdr.p_vaddr, phdr.p_filesz) !=
		    phdr.p_filesz) {
			WARN(("read text: %s\n", strerror(errno)));
			return (1);
		}
		if (first || ffp_save < phdr.p_vaddr + phdr.p_memsz)
			ffp_save = phdr.p_vaddr + phdr.p_memsz;

		/* Zero out bss. */
		if (phdr.p_filesz < phdr.p_memsz) {
			(void)printf("+%lu", phdr.p_memsz - phdr.p_filesz);
			bzero((caddr_t)phdr.p_vaddr + phdr.p_filesz,
			    phdr.p_memsz - phdr.p_filesz);
		}
		first = 0;
	}

	ffp_save = roundup(ffp_save, sizeof(long));

	/*
	 * Retrieve symbols.
	 */
	ssym = ffp_save;
	ffp_save += sizeof(Elf64_Ehdr);

	if (lseek(fd, elf->e_shoff, SEEK_SET) == -1)  {
		WARN(("seek to section headers: %s\n", strerror(errno)));
		return (1);
	}

	sz = elf->e_shnum * sizeof(Elf64_Shdr);
	shp = (Elf64_Shdr *)ffp_save;
	ffp_save += roundup(sz, sizeof(long));

	if (read(fd, shp, sz) != sz) {
		WARN(("read section headers: %d\n", strerror(errno)));
		return (1);
	}

	/*
	 * Now load the symbol sections themselves.  Make sure the
	 * sections are aligned. Don't bother with string tables if
	 * there are no symbol sections.
	 */
	off = roundup((sizeof(Elf64_Ehdr) + sz), sizeof(long));

	for (havesyms = i = 0; i < elf->e_shnum; i++)
		if (shp[i].sh_type == SHT_SYMTAB)
			havesyms = 1;

	if (!havesyms)
		goto no_syms;

	for (first = 1, i = 0; i < elf->e_shnum; i++) {
		if (shp[i].sh_type == SHT_SYMTAB ||
		    shp[i].sh_type == SHT_STRTAB) {
			printf("%s%ld", first ? " [" : "+",
			       (u_long)shp[i].sh_size);
			if (lseek(fd, shp[i].sh_offset, SEEK_SET) == -1) {
				WARN(("lseek symbols: %s\n", strerror(errno)));
				return (1);
			}
			if (read(fd, (void *)ffp_save, shp[i].sh_size) !=
			    shp[i].sh_size) {
				WARN(("read symbols: %s\n", strerror(errno)));
				return (1);
			}
			ffp_save += roundup(shp[i].sh_size, sizeof(long));
			shp[i].sh_offset = off;
			off += roundup(shp[i].sh_size, sizeof(long));
			first = 0;
		}
	}
	if (havesyms && first == 0)
		printf("]");

	elf->e_phoff = 0;
	elf->e_shoff = sizeof(Elf64_Ehdr);
	elf->e_phentsize = 0;
	elf->e_phnum = 0;
	bcopy(elf, (void *)ssym, sizeof(*elf));

no_syms:
	esym = ffp_save;
	ffp_save = ALPHA_K0SEG_TO_PHYS((ffp_save + PGOFSET & ~PGOFSET)) >> PGSHIFT;
	ffp_save += 2;		/* XXX OSF/1 does this, no idea why. */

	(void)printf("\n");
	*entryp = elf->e_entry;
	return (0);
}
#endif /* ALPHA_BOOT_ELF */
