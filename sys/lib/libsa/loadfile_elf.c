/* $NetBSD: loadfile.c,v 1.10 2000/12/03 02:53:04 tsutsui Exp $ */
/* $OpenBSD: loadfile_elf.c,v 1.1 2007/05/30 01:25:43 tom Exp $ */

/*-
 * Copyright (c) 1997 The NetBSD Foundation, Inc.
 * All rights reserved.
 *
 * This code is derived from software contributed to The NetBSD Foundation
 * by Jason R. Thorpe of the Numerical Aerospace Simulation Facility,
 * NASA Ames Research Center and by Christos Zoulas.
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
 *	This product includes software developed by the NetBSD
 *	Foundation, Inc. and its contributors.
 * 4. Neither the name of The NetBSD Foundation nor the names of its
 *    contributors may be used to endorse or promote products derived
 *    from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE NETBSD FOUNDATION, INC. AND CONTRIBUTORS
 * ``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED
 * TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE FOUNDATION OR CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

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

int ELFNAME(exec)(int, Elf_Ehdr *, u_long *, int);

int
ELFNAME(exec)(int fd, Elf_Ehdr *elf, u_long *marks, int flags)
{
	Elf_Shdr *shp;
	Elf_Phdr *phdr;
	Elf_Off off;
	int i;
	size_t sz;
	int first;
	int havesyms;
	paddr_t minp = ~0, maxp = 0, pos = 0;
	paddr_t offset = marks[MARK_START], shpp, elfp;

	sz = elf->e_phnum * sizeof(Elf_Phdr);
	phdr = ALLOC(sz);

	if (lseek(fd, (off_t)elf->e_phoff, SEEK_SET) == -1)  {
		WARN(("lseek phdr"));
		FREE(phdr, sz);
		return 1;
	}
	if (read(fd, phdr, sz) != sz) {
		WARN(("read program headers"));
		FREE(phdr, sz);
		return 1;
	}

	for (first = 1, i = 0; i < elf->e_phnum; i++) {

		if (phdr[i].p_type != PT_LOAD ||
		    (phdr[i].p_flags & (PF_W|PF_R|PF_X)) == 0)
			continue;

#define IS_TEXT(p)	(p.p_flags & PF_X)
#define IS_DATA(p)	((p.p_flags & PF_X) == 0)
#define IS_BSS(p)	(p.p_filesz < p.p_memsz)
		/*
		 * XXX: Assume first address is lowest
		 */
		if ((IS_TEXT(phdr[i]) && (flags & LOAD_TEXT)) ||
		    (IS_DATA(phdr[i]) && (flags & LOAD_DATA))) {

			/* Read in segment. */
			PROGRESS(("%s%lu", first ? "" : "+",
			    (u_long)phdr[i].p_filesz));

			if (lseek(fd, (off_t)phdr[i].p_offset, SEEK_SET) == -1)  {
				WARN(("lseek text"));
				FREE(phdr, sz);
				return 1;
			}
			if (READ(fd, phdr[i].p_vaddr, phdr[i].p_filesz) !=
			    phdr[i].p_filesz) {
				WARN(("read text"));
				FREE(phdr, sz);
				return 1;
			}
			first = 0;

		}
		if ((IS_TEXT(phdr[i]) && (flags & (LOAD_TEXT|COUNT_TEXT))) ||
		    (IS_DATA(phdr[i]) && (flags & (LOAD_DATA|COUNT_TEXT)))) {
			pos = phdr[i].p_vaddr;
			if (minp > pos)
				minp = pos;
			pos += phdr[i].p_filesz;
			if (maxp < pos)
				maxp = pos;
		}

		/* Zero out bss. */
		if (IS_BSS(phdr[i]) && (flags & LOAD_BSS)) {
			PROGRESS(("+%lu",
			    (u_long)(phdr[i].p_memsz - phdr[i].p_filesz)));
			BZERO((phdr[i].p_vaddr + phdr[i].p_filesz),
			    phdr[i].p_memsz - phdr[i].p_filesz);
		}
		if (IS_BSS(phdr[i]) && (flags & (LOAD_BSS|COUNT_BSS))) {
			pos += phdr[i].p_memsz - phdr[i].p_filesz;
			if (maxp < pos)
				maxp = pos;
		}
	}
	FREE(phdr, sz);

	/*
	 * Copy the ELF and section headers.
	 */
	elfp = maxp = roundup(maxp, sizeof(long));
	if (flags & (LOAD_HDR|COUNT_HDR))
		maxp += sizeof(Elf_Ehdr);

	if (flags & (LOAD_SYM|COUNT_SYM)) {
		if (lseek(fd, (off_t)elf->e_shoff, SEEK_SET) == -1)  {
			WARN(("lseek section headers"));
			return 1;
		}
		sz = elf->e_shnum * sizeof(Elf_Shdr);
		shp = ALLOC(sz);

		if (read(fd, shp, sz) != sz) {
			WARN(("read section headers"));
			FREE(shp, sz);
			return 1;
		}

		shpp = maxp;
		maxp += roundup(sz, sizeof(long));

		/*
		 * Now load the symbol sections themselves.  Make sure the
		 * sections are aligned. Don't bother with string tables if
		 * there are no symbol sections.
		 */
		off = roundup((sizeof(Elf_Ehdr) + sz), sizeof(long));

		for (havesyms = i = 0; i < elf->e_shnum; i++)
			if (shp[i].sh_type == SHT_SYMTAB)
				havesyms = 1;

		for (first = 1, i = 0; i < elf->e_shnum; i++) {
			if (shp[i].sh_type == SHT_SYMTAB ||
			    shp[i].sh_type == SHT_STRTAB) {
				if (havesyms && (flags & LOAD_SYM)) {
					PROGRESS(("%s%ld", first ? " [" : "+",
					    (u_long)shp[i].sh_size));
					if (lseek(fd, (off_t)shp[i].sh_offset,
					    SEEK_SET) == -1) {
						WARN(("lseek symbols"));
						FREE(shp, sz);
						return 1;
					}
					if (READ(fd, maxp, shp[i].sh_size) !=
					    shp[i].sh_size) {
						WARN(("read symbols"));
						FREE(shp, sz);
						return 1;
					}
				}
				maxp += roundup(shp[i].sh_size,
				    sizeof(long));
				shp[i].sh_offset = off;
				off += roundup(shp[i].sh_size, sizeof(long));
				first = 0;
			}
		}
		if (flags & LOAD_SYM) {
			BCOPY(shp, shpp, sz);

			if (havesyms && first == 0)
				PROGRESS(("]"));
		}
		FREE(shp, sz);
	}

	/*
	 * Frob the copied ELF header to give information relative
	 * to elfp.
	 */
	if (flags & LOAD_HDR) {
		elf->e_phoff = 0;
		elf->e_shoff = sizeof(Elf_Ehdr);
		elf->e_phentsize = 0;
		elf->e_phnum = 0;
		BCOPY(elf, elfp, sizeof(*elf));
	}

	marks[MARK_START] = LOADADDR(minp);
	marks[MARK_ENTRY] = LOADADDR(elf->e_entry);
	marks[MARK_NSYM] = 1;	/* XXX: Kernel needs >= 0 */
	marks[MARK_SYM] = LOADADDR(elfp);
	marks[MARK_END] = LOADADDR(maxp);
	return 0;
}
