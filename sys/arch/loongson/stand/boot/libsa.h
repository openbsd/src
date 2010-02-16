/*	$OpenBSD: libsa.h,v 1.2 2010/02/16 21:28:39 miod Exp $	*/

/*
 * Copyright (c) 2010 Miodrag Vallat.
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

#include <lib/libsa/stand.h>

#define	DEFAULT_KERNEL_ADDRESS	0

/*
 * MD interfaces for MI boot(9)
 */
void	devboot(dev_t, char *);
void	machdep(void);
void	run_loadfile(u_long *, int);

/*
 * PMON console
 */
void	pmon_cnprobe(struct consdev *);
void	pmon_cninit(struct consdev *);
int	pmon_cngetc(dev_t);
void	pmon_cnputc(dev_t, int);

/*
 * PMON I/O
 */
int	pmon_iostrategy(void *, int, daddr_t, size_t, void *, size_t *);
int	pmon_ioopen(struct open_file *, ...);
int	pmon_ioclose(struct open_file *);

extern int pmon_argc;
extern int32_t *pmon_argv;
extern int32_t *pmon_envp;
extern int32_t pmon_callvec;

extern char pmon_bootdev[];
