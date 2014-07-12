/*	$OpenBSD: libsa.h,v 1.2 2014/07/12 20:36:52 jasper Exp $	*/

/*
 * Copyright (c) 2013 Jasper Lievisse Adriaanse <jasper@openbsd.org>
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

#define DEFAULT_KERNEL_ADDRESS	0
#define INITRD_BASE		PHYS_TO_CKSEG0(0x04000000)
#define CONSPEED		115200

extern char *kernelfile;

/*
 * MD interfaces for MI boot(9)
 */
void    devboot(dev_t, char *);
void    machdep(void);
void    run_loadfile(u_long *, int);

/*
 * CN30XX UART
 */
void	cn30xxuartcnprobe(struct consdev *);
void	cn30xxuartcninit(struct consdev *);
void	cn30xxuartcnputc(dev_t, int);
int	cn30xxuartcngetc(dev_t);

/*
 * clock
 */
void	delay(int);
u_int	cp0_get_count(void);

