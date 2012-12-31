/*	$OpenBSD: bugcrt.c,v 1.6 2012/12/31 21:35:32 miod Exp $	*/

/*
 * Copyright (c) 2012 Miodrag Vallat.
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

#include <sys/types.h>
#include <machine/prom.h>

#include "libbug.h"
#include "stand.h"

struct mvmeprom_args bugargs = { .cputyp = -1 };	/* force out of .bss */

void __main(void);
extern void main(void);

void
bugcrt_start(u_int dev_lun, u_int ctrl_lun, u_int flags, u_int ctrl_addr,
    u_int entry, u_int conf_blk, char *arg_start, char *arg_end,
    char *nbarg_start, char *nbarg_end)
{
	extern int edata, end;
	struct mvmeprom_brdid *id;

#ifdef DEBUG
	printf("BUG parameters: CLUN %x DLUN %x flags %x ctrl_addr %x\n",
	    dev_lun, ctrl_lun, flags, ctrl_addr);
	printf("entry %x conf_blk %x\n", entry, conf_blk);
	printf("arg %p %p\n", arg_start, arg_end);
	printf("nbarg %p %p\n", nbarg_start, nbarg_end);
#endif

	bugargs.dev_lun = dev_lun;
	bugargs.ctrl_lun = ctrl_lun;
	bugargs.flags = flags;
	bugargs.ctrl_addr = ctrl_addr;
	bugargs.entry = entry;
	bugargs.conf_blk = conf_blk;
	bugargs.arg_start = arg_start;
	bugargs.arg_end = arg_end;
	bugargs.nbarg_start = nbarg_start;
	bugargs.nbarg_end = nbarg_end;

	bzero(&edata, (int)&end-(int)&edata);
	*bugargs.arg_end = 0;
	id = mvmeprom_getbrdid();
	bugargs.cputyp = id->model;
	main();
}

extern void bugexec_final(u_int, u_int, u_int, u_int, u_int, u_int, char *,
    char *, char *, char *, u_int, u_int);

void
bugexec(u_int *addr)
{
#ifdef DEBUG
	printf("%s: code %x stack %x\n", __func__, addr[1], addr[0]);
#endif
	bugexec_final(bugargs.dev_lun, bugargs.ctrl_lun, bugargs.flags,
	    bugargs.ctrl_addr, bugargs.entry, bugargs.conf_blk,
	    bugargs.arg_start, bugargs.arg_end, bugargs.nbarg_start,
	    bugargs.nbarg_end, addr[1] /* pc */, addr[0] /* sp */);
}

void
__main()
{
}
