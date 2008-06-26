/* $NetBSD: loadfile.c,v 1.10 2000/12/03 02:53:04 tsutsui Exp $ */
/* $OpenBSD: loadfile.c,v 1.4 2008/06/26 05:42:14 ray Exp $ */

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

#ifdef _STANDALONE
#include <lib/libkern/libkern.h>
#include <lib/libsa/stand.h>
#else
#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <err.h>
#endif

#include <sys/param.h>
#include <sys/exec.h>

#include "compat_linux.h"
#include "pathnames.h"
#include <lib/libsa/loadfile.h>
#include <stand/boot/cmd.h>

#define BOOT_ZBOOT

#ifdef BOOT_ECOFF
#include <sys/exec_ecoff.h>
static int coff_exec(int, struct ecoff_exechdr *, u_long *, int);
#endif
#ifdef BOOT_ELF
#include <sys/exec_elf.h>
static int elf_exec(int, Elf_Ehdr *, u_long *, int);
#endif
#ifdef BOOT_AOUT
#include <sys/exec_aout.h>
static int aout_exec(int, struct exec *, u_long *, int);
#endif
#ifdef BOOT_ZBOOT
#include <dev/cons.h>		/* XXX */
static int zboot_exec(int, u_long *, int);
#endif

/*
 * Open 'filename', read in program and and return 0 if ok 1 on error.
 * Fill in marks
 */
int
loadfile(const char *fname, u_long *marks, int flags)
{
	union {
#ifdef BOOT_ECOFF
		struct ecoff_exechdr coff;
#endif
#ifdef BOOT_ELF
		Elf_Ehdr elf;
#endif
#ifdef BOOT_AOUT
		struct exec aout;
#endif

	} hdr;
	ssize_t nr;
	int fd, rval;

	/* Open the file. */
	if ((fd = open(fname, 0)) < 0) {
		WARN(("open %s", fname ? fname : "<default>"));
		return -1;
	}

	/* Read the exec header. */
	if ((nr = read(fd, &hdr, sizeof(hdr))) != sizeof(hdr)) {
		WARN(("read header"));
		goto err;
	}

#ifdef BOOT_ECOFF
	if (!ECOFF_BADMAG(&hdr.coff)) {
		rval = coff_exec(fd, &hdr.coff, marks, flags);
	} else
#endif
#ifdef BOOT_ELF
	if (memcmp(hdr.elf.e_ident, ELFMAG, SELFMAG) == 0 &&
	    hdr.elf.e_ident[EI_CLASS] == ELFCLASS) {
#ifdef BOOT_ZBOOT
		rval = zboot_exec(fd, marks, flags);
#else
		rval = elf_exec(fd, &hdr.elf, marks, flags);
#endif
	} else
#endif
#ifdef BOOT_AOUT
	if (OKMAGIC(N_GETMAGIC(hdr.aout))
#ifndef NO_MID_CHECK
	    && N_GETMID(hdr.aout) == MID_MACHINE
#endif
	    ) {
		rval = aout_exec(fd, &hdr.aout, marks, flags);
	} else
#endif
	{
		rval = 1;
		errno = EFTYPE;
		WARN(("%s", fname ? fname : "<default>"));
	}

	if (rval == 0) {
		PROGRESS(("=0x%lx\n", marks[MARK_END] - marks[MARK_START]));
		return fd;
	}
err:
	(void)close(fd);
	return -1;
}

#ifdef BOOT_ECOFF
static int
coff_exec(int fd, struct ecoff_exechdr *coff, u_long *marks, int flags)
{
	paddr_t offset = marks[MARK_START];
	paddr_t minp = ~0, maxp = 0, pos;

	/* Read in text. */
	if (lseek(fd, ECOFF_TXTOFF(coff), SEEK_SET) == -1)  {
		WARN(("lseek text"));
		return 1;
	}

	if (coff->a.tsize != 0) {
		if (flags & LOAD_TEXT) {
			PROGRESS(("%lu", coff->a.tsize));
			if (READ(fd, coff->a.text_start, coff->a.tsize) !=
			    coff->a.tsize) {
				return 1;
			}
		}
		else {
			if (lseek(fd, coff->a.tsize, SEEK_CUR) == -1) {
				WARN(("read text"));
				return 1;
			}
		}
		if (flags & (COUNT_TEXT|LOAD_TEXT)) {
			pos = coff->a.text_start;
			if (minp > pos)
				minp = pos;
			pos += coff->a.tsize;
			if (maxp < pos)
				maxp = pos;
		}
	}

	/* Read in data. */
	if (coff->a.dsize != 0) {
		if (flags & LOAD_DATA) {
			PROGRESS(("+%lu", coff->a.dsize));
			if (READ(fd, coff->a.data_start, coff->a.dsize) !=
			    coff->a.dsize) {
				WARN(("read data"));
				return 1;
			}
		}
		if (flags & (COUNT_DATA|LOAD_DATA)) {
			pos = coff->a.data_start;
			if (minp > pos)
				minp = pos;
			pos += coff->a.dsize;
			if (maxp < pos)
				maxp = pos;
		}
	}

	/* Zero out bss. */
	if (coff->a.bsize != 0) {
		if (flags & LOAD_BSS) {
			PROGRESS(("+%lu", coff->a.bsize));
			BZERO(coff->a.bss_start, coff->a.bsize);
		}
		if (flags & (COUNT_BSS|LOAD_BSS)) {
			pos = coff->a.bss_start;
			if (minp > pos)
				minp = pos;
			pos = coff->a.bsize;
			if (maxp < pos)
				maxp = pos;
		}
	}

	marks[MARK_START] = LOADADDR(minp);
	marks[MARK_ENTRY] = LOADADDR(coff->a.entry);
	marks[MARK_NSYM] = 1;	/* XXX: Kernel needs >= 0 */
	marks[MARK_SYM] = LOADADDR(maxp);
	marks[MARK_END] = LOADADDR(maxp);
	return 0;
}
#endif /* BOOT_ECOFF */

#ifdef BOOT_ELF
static int
elf_exec(int fd, Elf_Ehdr *elf, u_long *marks, int flags)
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

	if (lseek(fd, elf->e_phoff, SEEK_SET) == -1)  {
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

			if (lseek(fd, phdr[i].p_offset, SEEK_SET) == -1)  {
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
		if (lseek(fd, elf->e_shoff, SEEK_SET) == -1)  {
			WARN(("lseek section headers"));
			return 1;
		}
		sz = elf->e_shnum * sizeof(Elf_Shdr);
		shp = ALLOC(sz);

		if (read(fd, shp, sz) != sz) {
			WARN(("read section headers"));
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
					if (lseek(fd, shp[i].sh_offset,
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
#endif /* BOOT_ELF */

#ifdef BOOT_AOUT
static int
aout_exec(int fd, struct exec *x, u_long *marks, int flags)
{
	u_long entry = x->a_entry;
	paddr_t aoutp = 0;
	paddr_t minp, maxp;
	int cc;
	paddr_t offset = marks[MARK_START];
	u_long magic = N_GETMAGIC(*x);
	int sub;

	/* In OMAGIC and NMAGIC, exec header isn't part of text segment */
	if (magic == OMAGIC || magic == NMAGIC)
		sub = 0;
	else
		sub = sizeof(*x);

	minp = maxp = ALIGNENTRY(entry);

	if (lseek(fd, sizeof(*x), SEEK_SET) == -1)  {
		WARN(("lseek text"));
		return 1;
	}

	/*
	 * Leave a copy of the exec header before the text.
	 * The kernel may use this to verify that the
	 * symbols were loaded by this boot program.
	 */
	if (magic == OMAGIC || magic == NMAGIC) {
		if (flags & LOAD_HDR && maxp >= sizeof(*x))
			BCOPY(x, maxp - sizeof(*x), sizeof(*x));
	}
	else {
		if (flags & LOAD_HDR)
			BCOPY(x, maxp, sizeof(*x));
		if (flags & (LOAD_HDR|COUNT_HDR))
			maxp += sizeof(*x);
	}

	/*
	 * Read in the text segment.
	 */
	if (flags & LOAD_TEXT) {
		PROGRESS(("%ld", x->a_text));

		if (READ(fd, maxp, x->a_text - sub) != x->a_text - sub) {
			WARN(("read text"));
			return 1;
		}
	} else {
		if (lseek(fd, x->a_text - sub, SEEK_CUR) == -1) {
			WARN(("seek text"));
			return 1;
		}
	}
	if (flags & (LOAD_TEXT|COUNT_TEXT))
		maxp += x->a_text - sub;

	/*
	 * Provide alignment if required
	 */
	if (magic == ZMAGIC || magic == NMAGIC) {
		int size = -(unsigned int)maxp & (__LDPGSZ - 1);

		if (flags & LOAD_TEXTA) {
			PROGRESS(("/%d", size));
			BZERO(maxp, size);
		}

		if (flags & (LOAD_TEXTA|COUNT_TEXTA))
			maxp += size;
	}

	/*
	 * Read in the data segment.
	 */
	if (flags & LOAD_DATA) {
		PROGRESS(("+%ld", x->a_data));

		if (READ(fd, maxp, x->a_data) != x->a_data) {
			WARN(("read data"));
			return 1;
		}
	}
	else {
		if (lseek(fd, x->a_data, SEEK_CUR) == -1) {
			WARN(("seek data"));
			return 1;
		}
	}
	if (flags & (LOAD_DATA|COUNT_DATA))
		maxp += x->a_data;

	/*
	 * Zero out the BSS section.
	 * (Kernel doesn't care, but do it anyway.)
	 */
	if (flags & LOAD_BSS) {
		PROGRESS(("+%ld", x->a_bss));

		BZERO(maxp, x->a_bss);
	}

	if (flags & (LOAD_BSS|COUNT_BSS))
		maxp += x->a_bss;

	/*
	 * Read in the symbol table and strings.
	 * (Always set the symtab size word.)
	 */
	if (flags & LOAD_SYM)
		BCOPY(&x->a_syms, maxp, sizeof(x->a_syms));

	if (flags & (LOAD_SYM|COUNT_SYM)) {
		maxp += sizeof(x->a_syms);
		aoutp = maxp;
	}

	if (x->a_syms > 0) {
		/* Symbol table and string table length word. */

		if (flags & LOAD_SYM) {
			PROGRESS(("+[%ld", x->a_syms));

			if (READ(fd, maxp, x->a_syms) != x->a_syms) {
				WARN(("read symbols"));
				return 1;
			}
		} else  {
			if (lseek(fd, x->a_syms, SEEK_CUR) == -1) {
				WARN(("seek symbols"));
				return 1;
			}
		}
		if (flags & (LOAD_SYM|COUNT_SYM))
			maxp += x->a_syms;

		if (read(fd, &cc, sizeof(cc)) != sizeof(cc)) {
			WARN(("read string table"));
			return 1;
		}

		if (flags & LOAD_SYM) {
			BCOPY(&cc, maxp, sizeof(cc));

			/* String table. Length word includes itself. */

			PROGRESS(("+%d]", cc));
		}
		if (flags & (LOAD_SYM|COUNT_SYM))
			maxp += sizeof(cc);

		cc -= sizeof(int);
		if (cc <= 0) {
			WARN(("symbol table too short"));
			return 1;
		}

		if (flags & LOAD_SYM) {
			if (READ(fd, maxp, cc) != cc) {
				WARN(("read strings"));
				return 1;
			}
		} else {
			if (lseek(fd, cc, SEEK_CUR) == -1) {
				WARN(("seek strings"));
				return 1;
			}
		}
		if (flags & (LOAD_SYM|COUNT_SYM))
			maxp += cc;
	}

	marks[MARK_START] = LOADADDR(minp);
	marks[MARK_ENTRY] = LOADADDR(entry);
	marks[MARK_NSYM] = x->a_syms;
	marks[MARK_SYM] = LOADADDR(aoutp);
	marks[MARK_END] = LOADADDR(maxp);
	return 0;
}
#endif /* BOOT_AOUT */

#ifdef BOOT_ZBOOT
static int
zboot_exec(int fd, u_long *marks, int flags)
{
	char buf[512];
	char *p;
	int tofd;
	int sz;
	int i;

	/* XXX cheating here by assuming that Xboot() was called before. */

	tofd = uopen(_PATH_ZBOOT, O_WRONLY);
	if (tofd == -1) {
		printf("%s: can't open (errno %d)\n", _PATH_ZBOOT, errno);
		return 1;
	}

	p = cmd.path;
	for (; *p != '\0'; p++)
		if (*p == ':') {
			strlcpy(buf, p+1, sizeof(buf));
			break;
		}
	if (*p == '\0')
		strlcpy(buf, cmd.path, sizeof(buf));

	p = buf;
	for (; *p == '/'; p++)
		;

	sz = strlen(p);
	if (uwrite(tofd, p, sz) != sz) {
		printf("zboot_exec: argument write error\n");
		goto err;
	}

	buf[0] = ' ';
	buf[1] = '-';
	if (uwrite(tofd, buf, 2) != 2) {
		printf("zboot_exec: argument write error\n");
		goto err;
	}

	i = (cmd.argc > 1 && cmd.argv[1][0] != '-') ? 2 : 1;
	for (; i < cmd.argc; i++) {
		p = cmd.argv[i];
		if (*p == '-')
			p++;
		sz = strlen(p);
		if (uwrite(tofd, p, sz) != sz) {
			printf("zboot_exec: argument write error\n");
			goto err;
		}
	}

	/* Select UART unit for serial console. */
	if (cn_tab && major(cn_tab->cn_dev) == 12) {
		buf[0] = '0' + minor(cn_tab->cn_dev);
		if (uwrite(tofd, buf, 1) != 1) {
			printf("zboot_exec: argument write error\n");
			goto err;
		}
	}

	/* Commit boot arguments. */
	uclose(tofd);

	tofd = uopen(_PATH_ZBOOT, O_WRONLY);
	if (tofd == -1) {
		printf("%s: can't open (errno %d)\n", _PATH_ZBOOT, errno);
		return 1;
	}

	if (lseek(fd, 0, SEEK_SET) != 0) {
		printf("%s: seek error\n", _PATH_ZBOOT);
		goto err;
	}

	while ((sz = read(fd, buf, sizeof(buf))) == sizeof(buf)) {
		if ((sz = uwrite(tofd, buf, sz)) != sizeof(buf)) {
			printf("%s: write error\n", _PATH_ZBOOT);
			goto err;
		}
	}

	if (sz < 0) {
		printf("zboot_exec: read error\n");
		goto err;
	}

	if (sz >= 0 && uwrite(tofd, buf, sz) != sz) {
		printf("zboot_exec: write error\n");
		goto err;
	}

	uclose(tofd);
	return 0;

err:
	uclose(tofd);
	return 1;
}
#endif /* BOOT_ZBOOT */
