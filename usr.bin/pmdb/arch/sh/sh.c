/*	$OpenBSD: sh.c,v 1.1 2007/03/03 14:20:12 miod Exp $	*/

/*
 * Copyright (c) 2007 Miodrag Vallat.
 *
 * Permission to use, copy, modify, and distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice, this permission notice, and the disclaimer below
 * appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 * ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
 * OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */

#include <string.h>

#include <sys/param.h>
#include <sys/ptrace.h>

#include <machine/reg.h>

#include "pmdb.h"

static const char *md_reg_names[] = {
	/* integer registers */
	"pc",	"sr",	"pr",	"mach",	"macl",
	"r15",	"r14",	"r13",	"r12",	"r11",	"r10",	"r9",	"r8",
	"r7",	"r6",	"r5",	"r4",	"r3",	"r2",	"r1",	"r0",
#ifdef notyet
	/* floating point registers */
	"fr0",	"fr1",	"fr2",	"fr3",	"fr4",	"fr5",	"fr6",	"fr7",
	"fr8",	"fr9",	"fr10",	"fr11",	"fr12",	"fr13",	"fr14",	"fr15",
	"xf0",	"xf1",	"xf2",	"xf3",	"xf4",	"xf5",	"xf6",	"xf7",
	"xf8",	"xf9",	"xf10",	"xf11",	"xf12",	"xf13",	"xf14",	"xf15",
	"fpul",	"fpscr"
#endif
};

struct md_def md_def = { md_reg_names, 21 /* + 34 */, 0 };

void
md_def_init()
{
}

int
md_getframe(struct pstate *ps, int frame, struct md_frame *f)
{
	struct reg regs;

	if (process_getregs(ps, &regs) != 0)
		return (-1);

	f->pc = regs.r_spc;
	f->fp = regs.r_r14;	/* frame */
	if (frame == 0) {
		f->nargs = 0;
	} else {
		f->nargs = 0;	/* XXX for now */
	}

	return (0);
}

int
md_getregs(struct pstate *ps, reg *r)
{
	struct reg regs;
#ifdef notyet
	struct fpreg fpregs;
	int rc;
#endif

	if (process_getregs(ps, &regs) != 0)
		return (-1);

	bcopy(&regs, r, sizeof(regs));

#ifdef notyet
	switch (rc = process_getfpregs(ps, &fpregs)) {
	case 0:
		bcopy(&fpregs, r + sizeof(regs) / sizeof(reg), sizeof(fpregs));
		break;
	case EINVAL:
		bzero(r + sizeof(regs) / sizeof(reg), sizeof(fpregs));
		break;
	default:
		return (-1);
	}
#endif

	return (0);
}
