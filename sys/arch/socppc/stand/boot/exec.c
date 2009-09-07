/*	$OpenBSD: exec.c,v 1.3 2009/09/07 21:16:57 dms Exp $	*/

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

#include <lib/libsa/stand.h>
#include <lib/libsa/loadfile.h>

#include <sys/reboot.h>
#include <stand/boot/cmd.h>

typedef void (*startfuncp)(int, int, u_int32_t, char *, int) __dead;

void
run_loadfile(u_long *marks, int howto)
{
	char args[512];			/* Should check size? */
	u_int32_t entry;
	char *cp;
	void *ssym, *esym;
	int l;

	snprintf(args, sizeof(args), "%s:%s -", cmd.bootdev, cmd.image);
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

	entry = marks[MARK_ENTRY];
	ssym = (void *)marks[MARK_SYM];
	esym = (void *)marks[MARK_END];

	/*
	 * Stash pointer to end of symbol table after the argument
	 * strings.
	 */
	l = strlen(args) + 1;
	bcopy(&ssym, args + l, sizeof(ssym));
	l += sizeof(ssym);
	bcopy(&esym, args + l, sizeof(esym));
	l += sizeof(esym);
	extern int fdtaddrsave;

	(*(startfuncp)(marks[MARK_ENTRY]))(fdtaddrsave, 0, entry, args, l);

	/* NOTREACHED */
}
