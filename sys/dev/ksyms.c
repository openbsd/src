/*	$OpenBSD: ksyms.c,v 1.18 2007/09/08 17:59:23 gilles Exp $	*/
/*
 * Copyright (c) 1998 Todd C. Miller <Todd.Miller@courtesan.com>
 * Copyright (c) 2001 Artur Grabowski <art@openbsd.org>
 * All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED ``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES,
 * INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY
 * AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL
 * THE AUTHORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS;
 * OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
 * WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR
 * OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF
 * ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include <sys/param.h>
#include <sys/buf.h>
#include <sys/exec.h>
#include <sys/systm.h>
#include <sys/uio.h>
#include <sys/malloc.h>
#include <sys/fcntl.h>
#include <sys/conf.h>

#include <uvm/uvm_extern.h>

#ifdef _NLIST_DO_ELF
#include <sys/exec_elf.h>
#endif

#include <machine/cpu.h>

extern char *esym;				/* end of symbol table */
#if defined(__sparc64__) || defined(__mips__)
extern char *ssym;				/* end of kernel */
#else
extern long end;				/* end of kernel */
#endif

static caddr_t ksym_head;
static caddr_t ksym_syms;
static size_t ksym_head_size;
static size_t ksym_syms_size;

void	ksymsattach(int);

/*
 * We assume __LDPGSZ is a multiple of PAGE_SIZE (it is)
 */

/*ARGSUSED*/
void
ksymsattach(num)
	int num;
{

#if defined(__sparc64__) || defined(__mips__)
	if (esym <= ssym) {
		printf("/dev/ksyms: Symbol table not valid.\n");
		return;
	}
#else
	if (esym <= (char *)&end) {
		printf("/dev/ksyms: Symbol table not valid.\n");
		return;
	}
#endif

#ifdef _NLIST_DO_ELF
	do {
#if defined(__sparc64__) || defined(__mips__)
		caddr_t symtab = ssym;
#else
		caddr_t symtab = (caddr_t)&end;
#endif
		Elf_Ehdr *elf;
		Elf_Shdr *shdr;
		int i;

		elf = (Elf_Ehdr *)symtab;
		if (memcmp(elf->e_ident, ELFMAG, SELFMAG) != 0 ||
		    elf->e_ident[EI_CLASS] != ELFCLASS ||
		    elf->e_machine != ELF_TARG_MACH)
			break;

		shdr = (Elf_Shdr *)&symtab[elf->e_shoff];
		for (i = 0; i < elf->e_shnum; i++) {
			if (shdr[i].sh_type == SHT_SYMTAB) {
				break;
			}
		}

		/*
		 * No symbol table found.
		 */
		if (i == elf->e_shnum)
			break;

		/*
		 * No additional header.
		 */
		ksym_head_size = 0;
		ksym_syms = symtab;
		ksym_syms_size = (size_t)(esym - symtab);

		return;
	} while (0);
#endif

#ifdef _NLIST_DO_AOUT
	{
		/*
		 * a.out header.
		 * Fake up a struct exec.
		 * We only fill in the following non-zero entries:
		 *	a_text - fake text segment (struct exec only)
		 *	a_syms - size of symbol table
		 */
		caddr_t symtab = (char *)(&end + 1);
		struct exec *k1;

		ksym_head_size = __LDPGSZ;
		ksym_head = malloc(ksym_head_size, M_DEVBUF, M_NOWAIT|M_ZERO);
		if (ksym_head == NULL) {
			printf("failed to allocate memory for /dev/ksyms\n");
			return;
		}

		k1 = (struct exec *)ksym_head;

		N_SETMAGIC(*k1, ZMAGIC, MID_MACHINE, 0);
		k1->a_text = __LDPGSZ;
		k1->a_syms = end;

		ksym_syms = symtab;
		ksym_syms_size = (size_t)(esym - symtab);
	}
#endif
}

/*ARGSUSED*/
int
ksymsopen(dev, flag, mode, p)
	dev_t dev;
	int flag, mode;
	struct proc *p;
{

	/* There are no non-zero minor devices */
	if (minor(dev) != 0)
		return (ENXIO);

	/* This device is read-only */
	if ((flag & FWRITE))
		return (EPERM);

	/* ksym_syms must be initialized */
	if (ksym_syms == NULL)
		return (ENXIO);

	return (0);
}

/*ARGSUSED*/
int
ksymsclose(dev, flag, mode, p)
	dev_t dev;
	int flag, mode;
	struct proc *p;
{

	return (0);
}

/*ARGSUSED*/
int
ksymsread(dev, uio, flags)
	dev_t dev;
	struct uio *uio;
	int flags;
{
	int error;
	size_t len;
	caddr_t v;
	size_t off;

	while (uio->uio_resid > 0) {
		if (uio->uio_offset >= ksym_head_size + ksym_syms_size)
			break;

		if (uio->uio_offset < ksym_head_size) {
			v = ksym_head + uio->uio_offset;
			len = ksym_head_size - uio->uio_offset;
		} else {
			off = uio->uio_offset - ksym_head_size;
			v = ksym_syms + off;
			len = ksym_syms_size - off;
		}

		if (len > uio->uio_resid)
			len = uio->uio_resid;

		if ((error = uiomove(v, len, uio)) != 0)
			return (error);
	}

	return (0);
}

/* XXX - not yet */
#if 0
paddr_t
ksymsmmap(dev, off, prot)
	dev_t dev;
	off_t off;
	int prot;
{
	vaddr_t va;
	paddr_t pa;

	if (off < 0)
		return (-1);
	if (off >= ksym_head_size + ksym_syms_size)
		return (-1);

	if ((vaddr_t)off < ksym_head_size) {
		va = (vaddr_t)ksym_head + off;
	} else {
		va = (vaddr_t)ksym_syms + off;
	}

	if (pmap_extract(pmap_kernel, va, &pa) == FALSE)
		panic("ksymsmmap: unmapped page");

	return atop(pa);
}
#endif
