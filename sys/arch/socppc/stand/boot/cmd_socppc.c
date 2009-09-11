/*	$OpenBSD: cmd_socppc.c,v 1.1 2009/09/11 17:45:01 dms Exp $	*/

/*
 * Copyright (c) 2009 Dariusz Swiderski <sfires@sfires.net>
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
#include "fdt.h"
#include "stand/boot/cmd.h"

int Xfdt(void);
int Xbats(void);

const struct cmd_table cmd_machine[] = {
	{ "fdt",	CMDT_CMD, Xfdt },
	{ NULL, 0 }
};

int
Xfdt(void)
{
	extern int fdtaddrsave;
	if (fdtaddrsave)
		fdt_print_tree();
	else
		printf("FDT blob not available\n");
	return 0;
}
