/*	$OpenBSD: exec.c,v 1.1 2008/05/10 20:06:26 kettenis Exp $	*/

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
	char *cp;

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
