/*	$OpenBSD: devopen.c,v 1.6 2007/06/27 20:29:38 mk Exp $	*/

/*
 * Copyright (c) 1996-1999 Michael Shalayeff
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR OR HIS RELATIVES BE LIABLE FOR ANY DIRECT,
 * INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF MIND, USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT,
 * STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING
 * IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
 * THE POSSIBILITY OF SUCH DAMAGE.
 */

#include <sys/param.h>
#include <sys/disklabel.h>
#include <dev/cons.h>
#include "libsa.h"
#include <stand/boot/cmd.h>

/* XXX use slot for 'rd' for 'hd' pseudo-device */
const char bdevs[][4] = {
	"wd", "", "fd", "", "sd", "st", "cd", "mcd",
	"", "", "", "", "", "", "", "scd", "", "hd", ""

};
const int nbdevs = NENTS(bdevs);

const char cdevs[][4] = {
	"cn", "", "", "", "", "", "", "",
	"", "", "", "", "com"
};
const int ncdevs = NENTS(cdevs);

/* pass dev_t to the open routines */
int
devopen(struct open_file *f, const char *fname, char **file)
{
	struct devsw *dp = devsw;
	register int i, rc = 1;

	*file = (char *)fname;

#ifdef DEBUG
	if (debug)
		printf("devopen:");
#endif

	for (i = 0; i < ndevs && rc != 0; dp++, i++) {
#ifdef DEBUG
		if (debug)
			printf(" %s: ", dp->dv_name);
#endif
		if ((rc = (*dp->dv_open)(f, file)) == 0) {
			f->f_dev = dp;
			return 0;
		}
#ifdef DEBUG
		else if (debug)
			printf("%d", rc);
#endif

	}
#ifdef DEBUG
	if (debug)
		putchar('\n');
#endif

	if ((f->f_flags & F_NODEV) == 0)
		f->f_dev = dp;

	return rc;
}

void
devboot(dev_t bootdev, char *p)
{
	dev_t unit = 0;		/* XXX */

	*p++ = 'h';
	*p++ = 'd';
	*p++ = '0' + unit;
	*p++ = 'a';
	*p = '\0';
}

int pch_pos = 0;

void
putchar(int c)
{
	switch (c) {
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
		while (++pch_pos % 8);
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
getchar(void)
{
	register int c = cngetc();

	if (c == '\r')
		c = '\n';

	if ((c < ' ' && c != '\n') || c == '\177')
		return c;

	putchar(c);

	return c;
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
