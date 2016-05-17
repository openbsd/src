/*	$OpenBSD: exec.c,v 1.5 2016/05/17 21:26:32 kettenis Exp $	*/

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
#include <machine/bootconfig.h>
#include <dev/cons.h>

#include <lib/libsa/loadfile.h>
#include <sys/exec_elf.h>

#include <efi.h>
#include <stand/boot/cmd.h>

#include "efiboot.h"

extern void *fdt;

typedef void (*startfuncp)(void *, void *, void *) __attribute__ ((noreturn));

struct uboot_tag_header {
	uint32_t	size;
	uint32_t	tag;
};
struct uboot_tag_core {
	uint32_t	flags;
	uint32_t	pagesize;
	uint32_t	rootdev;
};
struct uboot_tag_serialnr {
	uint32_t	low;
	uint32_t	high;
};
struct uboot_tag_revision {
	uint32_t	rev;
};
struct uboot_tag_mem32 {
	uint32_t	size;
	uint32_t	start;
};
struct uboot_tag_cmdline {
	char		cmdline[64];
};

#define ATAG_CORE	0x54410001
#define	ATAG_MEM	0x54410002
#define	ATAG_CMDLINE	0x54410009
#define	ATAG_SERIAL	0x54410006
#define	ATAG_REVISION	0x54410007
#define	ATAG_NONE	0x00000000
struct uboot_tag {
	struct uboot_tag_header hdr;
	union {
		struct uboot_tag_core		core;
		struct uboot_tag_mem32		mem;
		struct uboot_tag_revision	rev;
		struct uboot_tag_serialnr	serialnr;
		struct uboot_tag_cmdline	cmdline;
	} u;
};

struct uboot_tag tags[3];

void
run_loadfile(u_long *marks, int howto)
{
	Elf_Ehdr *elf = (Elf_Ehdr *)marks[MARK_SYM];
	Elf_Shdr *shp = (Elf_Shdr *)(marks[MARK_SYM] + elf->e_shoff);
	u_long esym = marks[MARK_END] & 0x0fffffff;
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
			esym |= shp[i].sh_addr & 0xf0000000;
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
	if (fdt == 0) {
		tags[0].hdr.tag = ATAG_MEM;
		tags[0].hdr.size = sizeof(struct uboot_tag) / sizeof(uint32_t);
		tags[0].u.mem.start = 0x10000000;
		tags[0].u.mem.size = 0x80000000;
		tags[1].hdr.tag = ATAG_CMDLINE;
		tags[1].hdr.size = sizeof(struct uboot_tag) / sizeof(uint32_t);
		strlcpy(tags[1].u.cmdline.cmdline, args,
			sizeof(tags[1].u.cmdline.cmdline));

		memcpy((void *)0x10000000, tags, sizeof(tags));
		fdt = (void *)0x10000000;
	}

	efi_cleanup();

	(*(startfuncp)(marks[MARK_ENTRY]))(NULL, (void *)4821, fdt);

	/* NOTREACHED */
}
