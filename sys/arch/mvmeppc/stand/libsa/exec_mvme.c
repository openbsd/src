/*	$OpenBSD: exec_mvme.c,v 1.5 2004/01/24 21:12:22 miod Exp $	*/


/*-
 * Copyright (c) 1982, 1986, 1990, 1993
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
 *
 * 	@(#)boot.c	8.1 (Berkeley) 6/10/93
 */

#include <sys/param.h>
#include <sys/reboot.h>
#include <machine/prom.h>
#include <a.out.h>
#include <sys/exec_elf.h>

#include "stand.h"
#include "libsa.h"

vaddr_t ssym, esym;

int
load_elf(fd, elf, entryp, esymp)
	int fd;
	Elf32_Ehdr *elf;
	u_int32_t *entryp;
	void **esymp;
	
{
	Elf32_Shdr *shpp;
	Elf32_Off off;
	Elf32_Ehdr *elfp;
	void *addr;
	size_t size;
	int n, havesyms, i, first = 1;
	size_t sz;
	
	void *maxp = 0; /*  correct type? */
	
	/*
	 * Don't display load address for ELF; it's encoded in
	 * each section.
	 */

	for (i = 0; i < elf->e_phnum; i++) {
		Elf32_Phdr phdr;
		(void)lseek(fd, elf->e_phoff + sizeof(phdr) * i, SEEK_SET);
		if (read(fd, (void *)&phdr, sizeof(phdr)) != sizeof(phdr)) {
			printf("read phdr: %s\n", strerror(errno));
			return (1);
		}
		if (phdr.p_type != PT_LOAD ||
		    (phdr.p_flags & (PF_W|PF_X)) == 0)
			continue;

		/* Read in segment. */
		printf("%s%lu@0x%lx", first ? "" : "+", phdr.p_filesz,
		    (u_long)phdr.p_vaddr);
		(void)lseek(fd, phdr.p_offset, SEEK_SET);
		maxp = maxp > (void *)(phdr.p_vaddr+ phdr.p_memsz) ?
			maxp : (void *)(phdr.p_vaddr+ phdr.p_memsz);
		if (read(fd, (void *)phdr.p_vaddr, phdr.p_filesz) !=
		    phdr.p_filesz) {
			printf("read segment: %s\n", strerror(errno));
			return (1);
		}
		syncicache((void *)phdr.p_vaddr, phdr.p_filesz);

		/* Zero BSS. */
		if (phdr.p_filesz < phdr.p_memsz) {
			printf("+%lu@0x%lx", phdr.p_memsz - phdr.p_filesz,
			    (u_long)(phdr.p_vaddr + phdr.p_filesz));
			bzero((void *)(phdr.p_vaddr + phdr.p_filesz),
			    phdr.p_memsz - phdr.p_filesz);
		}
		first = 0;
	}

#if 1
	/*
	 * Copy the ELF and section headers.
	 */
	maxp = (void *)roundup((long)maxp, sizeof(long));
	(void *)ssym = elfp = maxp; /* mark the start of the symbol table */
	
	maxp += sizeof(Elf_Ehdr);

	if (lseek(fd, elf->e_shoff, SEEK_SET) == -1)  {
		printf("lseek section headers: %s\n", strerror(errno));
		return 1;
	}
	sz = elf->e_shnum * sizeof(Elf_Shdr);
		
	shpp = maxp;
	maxp += roundup(sz, sizeof(long)); 

	if (read(fd, shpp, sz) != sz) {
		printf("read section headers: %s\n", strerror(errno));
		return 1;
	}
	/*
	 * Now load the symbol sections themselves.  Make sure the
	 * sections are aligned. Don't bother with string tables if
	 * there are no symbol sections.
	 */
	off = roundup((sizeof(Elf_Ehdr) + sz), sizeof(long));
	
	for (havesyms = i = 0; i < elf->e_shnum; i++)
		if (shpp[i].sh_type == SHT_SYMTAB)
			havesyms = 1;
	if (!havesyms)
		goto no_syms;

	for (first = 1, i = 0; i < elf->e_shnum; i++) {
		if (shpp[i].sh_type == SHT_SYMTAB ||
		    shpp[i].sh_type == SHT_STRTAB) {
			printf("%s%ld", first ? " [" : "+",
			    (u_long)shpp[i].sh_size);
			if (lseek(fd, shpp[i].sh_offset, SEEK_SET) == -1) {
				printf("lseek symbols: %s\n", strerror(errno));
				return 1;
			}
			if (read(fd, maxp, shpp[i].sh_size) != shpp[i].sh_size) {
				printf("read symbols: %s\n", strerror(errno));
				return 1;
			}
			maxp += roundup(shpp[i].sh_size, sizeof(long));
			shpp[i].sh_offset = off;
			off += roundup(shpp[i].sh_size, sizeof(long));
			first = 0;
		}
	}
	if (first == 0)
		printf("]");

	/*
	 * Frob the copied ELF header to give information relative
	 * to elfp.
	 */
	elf->e_phoff = 0;
	elf->e_shoff = sizeof(Elf_Ehdr);
	elf->e_phentsize = 0;
	elf->e_phnum = 0;
	bcopy(elf, elfp, sizeof(*elf));

#endif
no_syms:
	*esymp = (void *)esym = maxp; /* mark end of symbol table */
	*entryp = elf->e_entry;
	printf(" \n");
	return (0);
}

/*ARGSUSED*/
void
exec_mvme(file, flag)
char    *file;
int     flag;
{
	char *loadaddr;
	register int io;
	Elf32_Ehdr hdr;
	struct exec x;
	int cc, magic;
	void (*entry)();
	void *esym;
	register char *cp;
	register int *ip;
	int n;
	int bootdev;
	int rval = 1; 
	char dummy[]="\0";

#ifdef DEBUG
	printf("exec_mvme: file=%s flag=0x%x cputyp=%x\n", file, flag, bugargs.cputyp);
#endif

	io = open(file, 0);
	if (io < 0)
		return;

	printf("Booting %s\n", file);
	/*
	 * Read in the exec header, and validate it.
	 */
	if (read(io, &hdr, sizeof(hdr)) != sizeof(hdr)) {
		printf("read header: %s\n", strerror(errno));
		goto shread;
	}
	
	if (IS_ELF(hdr)) {
		rval = load_elf(io, &hdr, &entry, &esym);
	} else {
		printf("unknown executable format\n");
		errno = EFTYPE;
		goto closeout;
	}
	if (rval)
		goto closeout;

	close(io);

	printf("Start @ 0x%x\n", (int)entry);
	printf("Controler Address 0x%x\n", bugargs.ctrl_addr);
	if (flag & RB_HALT) mvmeprom_return();

	bootdev = (bugargs.ctrl_lun << 8) | (bugargs.dev_lun & 0xFF);

/* arguments to start 
 * r1 - stack provided by firmware/bootloader
 * r3 - unused
 * r4 - unused
 * r5 - firmware pointer (NULL for PPC1bug)
 * r6 - arg list
 * r7 - arg list length
 * r8 - end of symbol table
 */
/*               r3                 r4       r5    r6      r7 r8 */
 	(*entry)(bugargs.ctrl_addr, bootdev, NULL, &dummy, 0, esym);
	printf("exec: kernel returned!\n");
	return;

shread:
	printf("exec: short read\n");
	errno = EIO;
closeout:
	close(io);
	return;
}
