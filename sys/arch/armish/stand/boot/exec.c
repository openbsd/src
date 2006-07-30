/*	$OpenBSD: exec.c,v 1.3 2006/07/30 21:38:12 drahn Exp $	*/

/*
 * Copyright (c) 2006 Mark Kettenis
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

#include <lib/libsa/loadfile.h>

#ifdef BOOT_ELF
#include <sys/exec_elf.h>
#endif

#include <sys/reboot.h>
#include <stand/boot/cmd.h>
#include <machine/bootconfig.h>

typedef void (*startfuncp)(void) __attribute__ ((noreturn));

void
run_loadfile(u_long *marks, int howto)
{
#ifdef BOOT_ELF
	Elf_Ehdr *elf = (Elf_Ehdr *)marks[MARK_SYM];
	Elf_Shdr *shp = (Elf_Shdr *)(marks[MARK_SYM] + elf->e_shoff);
	u_long esym = marks[MARK_END];
	char *cp;
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
			esym |= shp[i].sh_addr & 0xff000000;
			*(u_long *)(shp[i].sh_addr & 0x00ffffff) = esym;
			break;
		}
	}
#endif
	cp = (char *)0x00200000 - MAX_BOOT_STRING - 1;

#define      BOOT_STRING_MAGIC 0x4f425344

	*(int *)cp = BOOT_STRING_MAGIC;

	cp += sizeof(int);
	snprintf(cp, MAX_BOOT_STRING, "%s:%s -", cmd.bootdev, cmd.image);

	while (*cp != '\0')
		cp++;
	if (howto & RB_ASKNAME)
		*cp++ = 'a';
	if (howto & RB_CONFIG)
		*cp++ = 'c';
	if (howto & RB_KDB)
		*cp++ = 'd';
	if (howto & RB_SINGLE)
		*cp++ = 's';

	*cp = '\0';

	(*(startfuncp)(marks[MARK_ENTRY]))();

	/* NOTREACHED */
}
