/*	$OpenBSD: devs.c,v 1.7 2011/03/13 00:13:53 deraadt Exp $	*/

/*
 * Copyright (c) 2006 Michael Shalayeff
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
#include <libsa.h>
#include <lib/libsa/loadfile.h>

int sector;

void
machdep(void)
{
	tick_init();

	/* scif_init(9600); */
}

int
devopen(struct open_file *f, const char *fname, char **file)
{  
	if (fname[0] != 'c' || fname[1] != 'f' || fname[2] != ':')
		return EINVAL;

	*file = (char *)fname + 3;
	f->f_flags |= F_NODEV;
	f->f_dev = &devsw[0];
	return (0);
}

void
devboot(dev_t bootdev, char *p)
{
	sector = bootdev;	/* passed from pbr */
	p[0] = 'c';
	p[1] = 'f';
	p[2] = '\0';
}

char *
ttyname(int fd)
{
	return "scif";
}

dev_t
ttydev(char *name)
{
	return NODEV;
}

int
cnspeed(dev_t dev, int sp)
{
	scif_init(sp);
	return sp;
}

void
run_loadfile(u_long *marks, int howto)
{
	u_long entry;

	entry = marks[MARK_ENTRY];
	cache_flush();
	cache_disable();

	(*(void (*)(int,int,int))entry)(howto, marks[MARK_END], 0);
}

int
blkdevopen(struct open_file *f, ...)
{
	return 0;
}

int
blkdevstrategy(void *v, int flag, daddr32_t dblk, size_t size, void *buf, size_t *rsize)
{

	if (flag != F_READ)
		return EROFS;

	if (size & (DEV_BSIZE - 1))
		return EINVAL;

	if (rsize)
		*rsize = size;

	if (size != 0 && readsects(0x40, sector + dblk, buf,
	    size / DEV_BSIZE) != 0)
		return EIO;

	return 0;
}

int
blkdevclose(struct open_file *f)
{
	return 0;
}

int pch_pos = 0;

void
putchar(int c)
{
	switch (c) {
	case '\177':	/* DEL erases */
		scif_putc('\b');
		scif_putc(' ');
	case '\b':
		scif_putc('\b');
		if (pch_pos)
			pch_pos--;
		break;
	case '\t':
		do
			scif_putc(' ');
		while (++pch_pos % 8);
		break;
	case '\n':
		scif_putc(c);
	case '\r':
		scif_putc('\r');
		pch_pos=0;
		break;
	default:
		scif_putc(c);
		pch_pos++;
		break;
	}
}

int
getchar(void)
{
	int c = scif_getc();

	if (c == '\r')
		c = '\n';

	if ((c < ' ' && c != '\n') || c == '\177')
		return c;

	putchar(c);
	return c;
}
