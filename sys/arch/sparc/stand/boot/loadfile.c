/*	$OpenBSD: loadfile.c,v 1.1 2002/08/11 23:11:22 art Exp $	*/
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

#define	ELFSIZE		32

#include <lib/libkern/libkern.h>
#include <lib/libsa/stand.h>
#include <sparc/stand/common/promdev.h>

#include <sys/param.h>
#include <sys/exec.h>
#include <sys/exec_elf.h>

#include <ddb/db_aout.h>

#ifdef SPARC_BOOT_AOUT
static int aout_exec(int, struct exec *, vaddr_t *);
#endif
#ifdef SPARC_BOOT_ELF
static int elf_exec(int, Elf_Ehdr *, vaddr_t *);
#endif
int loadfile(int, vaddr_t *);

vaddr_t ssym, esym;

/*
 * Open 'filename', read in program and return the entry point or -1 if error.
 */
int
loadfile(int fd, vaddr_t *entryp)
{
	struct devices *dp;
	union {
#ifdef SPARC_BOOT_AOUT
		struct exec aout;
#endif
#ifdef SPARC_BOOT_ELF
		Elf_Ehdr elf;
#endif
	} hdr;
	int rval;

	/* Read the exec header. */
	if (read(fd, &hdr, sizeof(hdr)) != sizeof(hdr)) {
		printf("read header: %s\n", strerror(errno));
		goto err;
	}

#ifdef SPARC_BOOT_ELF
	if (memcmp(ELFMAG, hdr.elf.e_ident, SELFMAG) == 0) {
		rval = elf_exec(fd, &hdr.elf, entryp);
	} else
#endif
#ifdef SPARC_BOOT_AOUT
	if (!N_BADMAG(hdr.aout)) {
		rval = aout_exec(fd, &hdr.aout, entryp);
	} else
#endif
	{
		printf("unknown executable format\n");
	}

err:
	if (fd >= 0)
		close(fd);
	return (rval);
}

#ifdef SPARC_BOOT_AOUT
static int
aout_exec(int fd, struct exec *aout, vaddr_t *entryp)
{
	caddr_t addr = (caddr_t)LOADADDR;
	int strtablen;
	char *strtab;
	vaddr_t entry = (vaddr_t)LOADADDR;
	int i;

	printf("%d", aout->a_text);
	lseek(fd, sizeof(struct exec), SEEK_SET);
	if (N_GETMAGIC(*aout) == ZMAGIC) {
		entry = (vaddr_t)(addr+sizeof(struct exec));
		addr += sizeof(struct exec);
	}
	if (read(fd, (char *)addr, aout->a_text) != aout->a_text)
		goto shread;
	addr += aout->a_text;
	if (N_GETMAGIC(*aout) == ZMAGIC || N_GETMAGIC(*aout) == NMAGIC)
		while ((int)addr & __LDPGSZ)
			*addr++ = 0;
	printf("+%d", aout->a_data);
	if (read(fd, addr, aout->a_data) != aout->a_data)
		goto shread;
	addr += aout->a_data;
	printf("+%d", aout->a_bss);
	for (i = aout->a_bss; i ; --i)
		*addr++ = 0;
	if (aout->a_syms != 0) {
		bcopy(&aout->a_syms, addr, sizeof(aout->a_syms));
		addr += sizeof(aout->a_syms);
		printf("+[%d", aout->a_syms);
		if (read(fd, addr, aout->a_syms) != aout->a_syms)
			goto shread;
		addr += aout->a_syms;

		if (read(fd, &strtablen, sizeof(int)) != sizeof(int))
			goto shread;

		bcopy(&strtablen, addr, sizeof(int));
		if (i = strtablen) {
			i -= sizeof(int);
			addr += sizeof(int);
			if (read(fd, addr, i) != i)
			    goto shread;
			addr += i;
		}
		printf("+%d]", i);
		esym = ((u_int)aout->a_entry - (u_int)LOADADDR) +
			(((int)addr + sizeof(int) - 1) & ~(sizeof(int) - 1));
	}
	printf("=0x%x\n", addr);
	close(fd);

	*entryp = entry;
	return (0);

shread:
	printf("boot: short read\n");
	return (1);
}
#endif /* SPARC_BOOT_AOUT */

#ifdef SPARC_BOOT_ELF
static int
elf_exec(int fd, Elf_Ehdr *elf, vaddr_t *entryp)
{
	int i;
	int first = 1, havesyms;
	Elf_Shdr *shp;
	Elf_Off off;
	size_t sz;
	vaddr_t addr = 0;
	Elf_Ehdr *fake_elf;

	*entryp = 0;

#define A(x) ((x) - *entryp + (vaddr_t)LOADADDR)

	/* loop through the pheaders and find the entry point. */
	for (i = 0; i < elf->e_phnum; i++) {
		Elf_Phdr phdr;
		lseek(fd, elf->e_phoff + sizeof(phdr) * i, SEEK_SET);
		if (read(fd, (void *)&phdr, sizeof(phdr)) != sizeof(phdr)) {
			(void)printf("read phdr: %s\n", strerror(errno));
			return (1);
		}
		if (phdr.p_type != PT_LOAD ||
		    (phdr.p_flags & (PF_W|PF_X)) == 0 ||
		    (phdr.p_vaddr != elf->e_entry))
			continue;

		*entryp = phdr.p_vaddr;
	}

	if (*entryp == 0) {
		printf("Can't find entry point.\n");
		return (-1);
	}

	for (i = 0; i < elf->e_phnum; i++) {
		Elf_Phdr phdr;
		lseek(fd, elf->e_phoff + sizeof(phdr) * i, SEEK_SET);
		if (read(fd, (void *)&phdr, sizeof(phdr)) != sizeof(phdr)) {
			(void)printf("read phdr: %s\n", strerror(errno));
			return (1);
		}
		if (phdr.p_type != PT_LOAD ||
		    (phdr.p_flags & (PF_W|PF_X)) == 0)
			continue;

		/* Read in segment. */
		printf("%s%lu", first ? "" : "+", phdr.p_filesz);
		lseek(fd, phdr.p_offset, SEEK_SET);
		if (read(fd, (caddr_t)A(phdr.p_vaddr), phdr.p_filesz) !=
		    phdr.p_filesz) {
			(void)printf("read text: %s\n", strerror(errno));
			return (1);
		}

		/* keep track of highest addr we loaded. */
		if (first || addr < (phdr.p_vaddr + phdr.p_memsz))
			addr = (phdr.p_vaddr + phdr.p_memsz);

		/* Zero out bss. */
		if (phdr.p_filesz < phdr.p_memsz) {
			(void)printf("+%lu", phdr.p_memsz - phdr.p_filesz);
			bzero((caddr_t)A(phdr.p_vaddr) + phdr.p_filesz,
			    phdr.p_memsz - phdr.p_filesz);
		}
		first = 0;
	}

	addr = A(addr);
	addr = roundup(addr, sizeof(long));

	ssym = addr;
	/*
	 * Retreive symbols.
	 */
	addr += sizeof(Elf_Ehdr);

	if (lseek(fd, elf->e_shoff, SEEK_SET) == -1)  {
		printf("seek to section headers: %s\n", strerror(errno));
		return (1);
	}

	sz = elf->e_shnum * sizeof(Elf_Shdr);
	shp = (Elf_Shdr *)addr;
	addr += roundup(sz, sizeof(long));

	if (read(fd, shp, sz) != sz) {
		printf("read section headers: %d\n", strerror(errno));
		return (1);
	}

	/*
	 * Now load the symbol sections themselves.  Make sure the
	 * sections are aligned. Don't bother with string tables if
	 * there are no symbol sections.
	 */
	off = roundup((sizeof(Elf_Ehdr) + sz), sizeof(long));

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
				printf("lseek symbols: %s\n", strerror(errno));
				return (1);
			}
			if (read(fd, (void *)addr, shp[i].sh_size) !=
			    shp[i].sh_size) {
				printf("read symbols: %s\n", strerror(errno));
				return (1);
			}
			addr += roundup(shp[i].sh_size, sizeof(long));
			shp[i].sh_offset = off;
			off += roundup(shp[i].sh_size, sizeof(long));
			first = 0;
		}
	}
	if (havesyms && first == 0)
		printf("]");

	elf->e_phoff = 0;
	elf->e_shoff = sizeof(Elf_Ehdr);
	elf->e_phentsize = 0;
	elf->e_phnum = 0;
	bcopy(elf, (void *)ssym, sizeof(*elf));

no_syms:
	esym = (addr - (vaddr_t)LOADADDR) + *entryp;

	*entryp = (vaddr_t)LOADADDR;

	printf("\n");
	return (0);
}
#endif /* ALPHA_BOOT_ELF */
