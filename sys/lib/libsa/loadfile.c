/* $NetBSD: loadfile.c,v 1.10 2000/12/03 02:53:04 tsutsui Exp $ */
/* $OpenBSD: loadfile.c,v 1.13 2007/05/30 01:25:43 tom Exp $ */

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

#include "loadfile.h"

#ifdef BOOT_ECOFF
#include <sys/exec_ecoff.h>
static int coff_exec(int, struct ecoff_exechdr *, u_long *, int);
#endif
#ifdef BOOT_AOUT
#include <sys/exec_aout.h>
static int aout_exec(int, struct exec *, u_long *, int);
#endif

#ifdef BOOT_ELF
#include <sys/exec_elf.h>
#if defined(BOOT_ELF32) && defined(BOOT_ELF64)
/*
 * Both defined, so elf32_exec() and elf64_exec() need to be separately
 * created (can't do it by including loadfile_elf.c here).
 */
int elf32_exec(int, Elf32_Ehdr *, u_long *, int);
int elf64_exec(int, Elf64_Ehdr *, u_long *, int);
#else
#include "loadfile_elf.c"
#endif
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
#if defined(BOOT_ELF32) || (defined(BOOT_ELF) && ELFSIZE == 32)
		Elf32_Ehdr elf32;
#endif
#if defined(BOOT_ELF64) || (defined(BOOT_ELF) && ELFSIZE == 64)
		Elf64_Ehdr elf64;
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
#if defined(BOOT_ELF32) || (defined(BOOT_ELF) && ELFSIZE == 32)
	if (memcmp(hdr.elf32.e_ident, ELFMAG, SELFMAG) == 0 &&
	    hdr.elf32.e_ident[EI_CLASS] == ELFCLASS32) {
		rval = elf32_exec(fd, &hdr.elf32, marks, flags);
	} else
#endif
#if defined(BOOT_ELF64) || (defined(BOOT_ELF) && ELFSIZE == 64)
	if (memcmp(hdr.elf64.e_ident, ELFMAG, SELFMAG) == 0 &&
	    hdr.elf64.e_ident[EI_CLASS] == ELFCLASS64) {
		rval = elf64_exec(fd, &hdr.elf64, marks, flags);
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
