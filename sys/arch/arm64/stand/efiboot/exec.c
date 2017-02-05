/*	$OpenBSD: exec.c,v 1.4 2017/02/05 13:11:06 patrick Exp $	*/

/*
 * Copyright (c) 2006, 2016 Mark Kettenis
 *
 * Permission to use, copy, modify, and distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 * ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
 * OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */

#include <sys/param.h>
#include <sys/reboot.h>
#include <dev/cons.h>

#include <lib/libkern/libkern.h>
#include <lib/libsa/loadfile.h>
#include <sys/exec_elf.h>

#include <efi.h>
#include <stand/boot/cmd.h>

#include <machine/armreg.h>

#include "efiboot.h"
#include "libsa.h"
#include "fdt.h"

typedef void (*startfuncp)(void *, void *, void *) __attribute__ ((noreturn));

unsigned int cpu_get_dcache_line_size(void);
void cpu_flush_dcache(vaddr_t, vsize_t);
void cpu_inval_icache(void);

unsigned int
cpu_get_dcache_line_size(void)
{
	uint64_t ctr;
	unsigned int dcl_size;

	/* Accessible from all security levels */
	ctr = READ_SPECIALREG(ctr_el0);

	/*
	 * Relevant field [19:16] is LOG2
	 * of the number of words in DCache line
	 */
	dcl_size = CTR_DLINE_SIZE(ctr);

	/* Size of word shifted by cache line size */
	return (sizeof(int) << dcl_size);
}

void
cpu_flush_dcache(vaddr_t addr, vsize_t len)
{
	uint64_t cl_size;
	vaddr_t end;

	cl_size = cpu_get_dcache_line_size();

	/* Calculate end address to clean */
	end = addr + len;
	/* Align start address to cache line */
	addr = addr & ~(cl_size - 1);

	for (; addr < end; addr += cl_size)
		__asm volatile("dc civac, %0" :: "r" (addr) : "memory");

	/* Full system DSB */
	__asm volatile("dsb sy" ::: "memory");
}

void
cpu_inval_icache(void)
{
	__asm volatile(
	    "ic		ialluis	\n"
	    "dsb	ish	\n"
	    : : : "memory");
}

void
run_loadfile(u_long *marks, int howto)
{
	Elf_Ehdr *elf = (Elf_Ehdr *)marks[MARK_SYM];
	Elf_Shdr *shp = (Elf_Shdr *)(marks[MARK_SYM] + elf->e_shoff);
	u_long esym = marks[MARK_END] & 0x000fffffff;
	u_long offset = 0;
	char args[256];
	char *cp;
	void *fdt;
	int i;

	/*
	 * Tell locore.S where the symbol table ends by setting
	 * 'esym', which should be the first word in the .data
	 * section.
	 */
	for (i = 0; i < elf->e_shnum; i++) {
		/* XXX Assume .data is the first writable segment. */
		if (shp[i].sh_flags & SHF_WRITE) {
			/* XXX We have to store the virtual address. */
			esym |= shp[i].sh_addr & 0xffffff8000000000;
			*(u_long *)(LOADADDR(shp[i].sh_addr)) = esym;
			break;
		}
	}

	snprintf(args, sizeof(args) - 8, "%s:%s", cmd.bootdev, cmd.image);
	cp = args + strlen(args);

	*cp++ = ' ';
	*cp = '-';
	if (howto & RB_ASKNAME)
		*++cp = 'a';
	if (howto & RB_CONFIG)
		*++cp = 'c';
	if (howto & RB_SINGLE)
		*++cp = 's';
	if (howto & RB_KDB)
		*++cp = 'd';
	if (*cp == '-')
		*--cp = 0;
	else
		*++cp = 0;

	fdt = efi_makebootargs(args);

	efi_cleanup();

	cpu_flush_dcache(marks[MARK_ENTRY], marks[MARK_END] - marks[MARK_ENTRY]);
	cpu_inval_icache();

	cpu_flush_dcache((vaddr_t)fdt, fdt_get_size(fdt));

	(*(startfuncp)(marks[MARK_ENTRY]))((void *)esym, 0, fdt);

	/* NOTREACHED */
}
