/*	$OpenBSD: dev_hppa64.c,v 1.2 2005/05/29 18:53:54 miod Exp $	*/

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

#include "libsa.h"
#include <sys/param.h>
#include <sys/disklabel.h>
#include <sys/reboot.h>
#include <dev/cons.h>

#include <machine/iomod.h>

#include "dev_hppa64.h"

extern int debug;

const char cdevs[][4] = {
	"ite", "", "", "", "", "", "", "",
	"", "", "", "", ""
};
const int ncdevs = NENTS(cdevs);

const struct pdc_devs {
	char	name[3];
	int	dev_type;
} pdc_devs[] = {
	{ "dk",  0 },
	{ "ct",  1 },
	{ "lf",  2 },
	{ "",   -1 },
	{ "rd", -1 },
	{ "sw", -1 },
	{ "fl", -1 },
};

/* pass dev_t to the open routines */
int
devopen(f, fname, file)
	struct open_file *f;
	const char *fname;
	char **file;
{
	struct hppa_dev *hpd;
	const struct pdc_devs *dp = pdc_devs;
	int rc = 1;

	if (!(*file = strchr(fname, ':')))
		return ENODEV;
	else
		(*file)++;

#ifdef DEBUGBUG
	if (debug)
		printf("devopen: ");
#endif

	for (dp = pdc_devs; dp < &pdc_devs[NENTS(pdc_devs)]; dp++)
		if (!strncmp(fname, dp->name, sizeof(dp->name)-1))
			break;

	if (dp >= &pdc_devs[NENTS(pdc_devs)] || dp->dev_type < 0)
		return ENODEV;
#ifdef DEBUGBUG
	if (debug)
		printf("%s\n", dp->name);
#endif

	if (!(hpd = alloc(sizeof *hpd))) {
#ifdef DEBUG
		printf ("devopen: no mem\n");
#endif
	} else {
		bzero(hpd, sizeof *hpd);
		hpd->bootdev = bootdev;
		hpd->buf = (char *)(((u_long)hpd->ua_buf + IODC_MINIOSIZ-1) &
			~(IODC_MINIOSIZ-1));
		f->f_devdata = hpd;
		if ((rc = (*devsw[dp->dev_type].dv_open)(f, file)) == 0) {
			f->f_dev = &devsw[dp->dev_type];
			return 0;
		}
		free (hpd, 0);
		f->f_devdata = NULL;
	}

	if (!(f->f_flags & F_NODEV))
		f->f_dev = &devsw[dp->dev_type];

	if (!f->f_devdata)
		*file = NULL;

	return rc;
}

void
devboot(dev, p)
	dev_t dev;
	char *p;
{
	const char *q;
	int unit;

	if (!dev) {
		int type;

		switch (PAGE0->mem_boot.pz_class) {
		case PCL_RANDOM:
			type = 0;
			unit = PAGE0->mem_boot.pz_layers[0];
			break;
		case PCL_SEQU:
			type = 1;
			unit = PAGE0->mem_boot.pz_layers[0];
			break;
		case PCL_NET_MASK|PCL_SEQU:
			type = 2;
			unit = 0;
			break;
		default:
			type = 0;
			unit = 0;
			break;
		}
		dev = bootdev = MAKEBOOTDEV(type, 0, 0, unit, B_PARTITION(dev));
	}
#ifdef _TEST
	*p++ = '/';
	*p++ = 'd';
	*p++ = 'e';
	*p++ = 'v';
	*p++ = '/';
	*p++ = 'r';
#endif
	/* quick copy device name */
	for (q = pdc_devs[B_TYPE(dev)].name; (*p++ = *q++););
	unit = B_UNIT(dev);
	if (unit >= 10) {
		p[-1] = '0' + unit / 10;
		*p++ = '0' + (unit % 10);
	} else
		p[-1] = '0' + unit;
	*p++ = 'a' + B_PARTITION(dev);
	*p = '\0';
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
ttyname(fd)
	int fd;
{
	snprintf(ttyname_buf, sizeof ttyname_buf, "%s%d",
	    cdevs[major(cn_tab->cn_dev)],
	    minor(cn_tab->cn_dev));
	return (ttyname_buf);
}

dev_t
ttydev(name)
	char *name;
{
	int i, unit = -1;
	char *no = name + strlen(name) - 1;

	while (no >= name && *no >= '0' && *no <= '9')
		unit = (unit < 0 ? 0 : (unit * 10)) + *no-- - '0';
	if (no < name || unit < 0)
		return (NODEV);
	for (i = 0; i < ncdevs; i++)
		if (strncmp(name, cdevs[i], no - name + 1) == 0)
			return (makedev(i, unit));
	return (NODEV);
}
