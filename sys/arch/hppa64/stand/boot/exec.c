/*	$OpenBSD: exec.c,v 1.1 2005/04/01 10:40:48 mickey Exp $	*/

/*
 * Copyright (c) 2005 Michael Shalayeff
 * All rights reserved.
 *
 * Permission to use, copy, modify, and distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF MIND, USE, DATA OR PROFITS, WHETHER IN
 * AN ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT
 * OF OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */

#include <sys/param.h>
#include <machine/pdc.h>
#include "libsa.h"
#include <lib/libsa/loadfile.h>
#include <stand/boot/bootarg.h>
#include "dev_hppa64.h"

typedef void (*startfuncp)(int, int, int, int, int, int, caddr_t)
    __attribute__ ((noreturn));

void
run_loadfile(u_long *marks, int howto)
{
	fcacheall();

	__asm("mtctl %r0, %cr17");
	__asm("mtctl %r0, %cr17");
	/* stack and the gung is ok at this point, so, no need for asm setup */
	(*(startfuncp)(marks[MARK_ENTRY]))((int)(long)(long)pdc, howto, bootdev,
	    marks[MARK_END], BOOTARG_APIVER, BOOTARG_LEN, (caddr_t)BOOTARG_OFF);

	/* not reached */
}
