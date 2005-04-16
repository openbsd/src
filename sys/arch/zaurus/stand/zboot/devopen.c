/*	$OpenBSD: devopen.c,v 1.2 2005/04/16 17:27:58 uwe Exp $	*/

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

#include "libsa.h"
#include <sys/param.h>
#include <sys/disklabel.h>
#include <dev/cons.h>

extern int debug;

/* XXX use slot for 'rd' for 'hd' pseudo-device */
const char bdevs[][4] = {
	"wd", "", "fd", "wt", "sd", "st", "cd", "mcd",
	"", "", "", "", "", "", "", "scd", "", "hd", ""

};
const int nbdevs = NENTS(bdevs);

const char cdevs[][4] = {
	"cn", "", "", "", "", "", "", "",
	"com", "", "", "", "pc"
};
const int ncdevs = NENTS(cdevs);

int getbootdev(dev_t, char *);

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

int
getbootdev(dev_t bootdev, char *p)
{
	char buf[DEV_BSIZE];
	struct dos_partition *dp;
	struct disklabel label;
	static int timeout = 10;
	char *s;
	int fd;
	int n;
	char *msg = "";

	s = p;
	*p++ = '/';
	*p++ = 'd';
	*p++ = 'e';
	*p++ = 'v';
	*p++ = '/';
	*p++ = 'h';
	*p++ = 'd';
	*p++ = 'a' + (bootdev & 0xf); /* a - h */
	*p = '\0';

	/*
	 * Give disk devices some time to become ready when the first open
	 * fails.  Even when open succeeds the disk is sometimes not ready.
	 */
	if ((fd = uopen(s, O_RDONLY)) == -1 && errno == ENXIO) {
		int t;
		while (fd == -1 && timeout > 0) {
			timeout--;
			t = getsecs() + 1;
			while (getsecs() < t);
			fd = uopen(s, O_RDONLY);
		}
		if (fd != -1) {
			t = getsecs() + 2;
			while (getsecs() < t);
		}
	}
	if (fd == -1)
		return 0;

	/* Read the disk's MBR. */
	if (unixstrategy((void *)fd, F_READ, DOSBBSECTOR, DEV_BSIZE, buf,
	    &n) != 0 || n != DEV_BSIZE) {
		uclose(fd);
		return 0;
	}

	/* Find OpenBSD primary partition in the disk's MBR. */
	dp = (struct dos_partition *)&buf[DOSPARTOFF];
	for (n = 0; n < NDOSPART; n++)
		if (dp[n].dp_typ == DOSPTYP_OPENBSD)
			break;
	if (n == NDOSPART) {
		uclose(fd);
		return 0;
	}
	*p++ = '1' + n;
	*p = '\0';
	uclose(fd);

	/* Test if the OpenBSD partition has a valid disklabel. */
	if ((fd = uopen(s, O_RDONLY)) != -1) {
		if (unixstrategy((void *)fd, F_READ, LABELSECTOR,
		    DEV_BSIZE, buf, &n) == 0 && n == DEV_BSIZE)
			msg = getdisklabel(buf, &label);
		uclose(fd);
	}
	return msg == NULL;
}

void
devboot(dev_t bootdev, char *p)
{

	if (bootdev != 0 && getbootdev(bootdev, p))
		return;

	for (bootdev = 0; bootdev < 8; bootdev++)
		if (getbootdev(bootdev, p))
			return;

	/* fall-back to the previous default device */
	strlcpy(p, "/dev/hda4", 16);
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

#if 0
	putchar(c);
#endif

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
