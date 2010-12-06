/*	$OpenBSD: dev_socppc.c,v 1.2 2010/12/06 18:44:49 jasper Exp $	*/

/*
 * Copyright (c) 2008 Mark Kettenis
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
#include <dev/cons.h>

#include "libsa.h"

const char cdevs[][4] = {
	"cn", "", "", "", "", "", "", "",
	"", "", "", "", "com"
};
const int ncdevs = nitems(cdevs);

void
devboot(dev_t dev, char *p)
{
	strlcpy(p, "wd0a", 5);
}

int
cnspeed(dev_t dev, int sp)
{
	return CONSPEED;
}

int pch_pos;

void
putchar(c)
	int c;
{
	switch(c) {
	case '\177':	/* DEL erases */
		cnputc('\b');
		cnputc(' ');
	case '\b':
		cnputc('\b');
		if (pch_pos)
			pch_pos--;
		break;
	case '\t':
		do
			cnputc(' ');
		while(++pch_pos % 8);
		break;
	case '\n':
	case '\r':
		cnputc(c);
		pch_pos=0;
		break;
	default:
		cnputc(c);
		pch_pos++;
		break;
	}
}

int
getchar()
{
	int c = cngetc();

	if (c == '\r')
		c = '\n';

	if ((c < ' ' && c != '\n') || c == '\177')
		return(c);

	putchar(c);

	return(c);
}

char ttyname_buf[8];

char *
ttyname(int fd)
{
	snprintf(ttyname_buf, sizeof ttyname_buf, "%s%d",
	    cdevs[major(cn_tab->cn_dev)], minor(cn_tab->cn_dev));

	return ttyname_buf;
}

dev_t
ttydev(char *name)
{
	int i, unit = -1;
	char *no = name + strlen(name) - 1;

	while (no >= name && *no >= '0' && *no <= '9')
		unit = (unit < 0 ? 0 : (unit * 10)) + *no-- - '0';
	if (no < name || unit < 0)
		return NODEV;
	for (i = 0; i < ncdevs; i++)
		if (strncmp(name, cdevs[i], no - name + 1) == 0)
			return (makedev(i, unit));
	return NODEV;
}
