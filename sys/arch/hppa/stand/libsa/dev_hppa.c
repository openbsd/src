/*	$OpenBSD: dev_hppa.c,v 1.1.1.1 1998/06/23 18:46:42 mickey Exp $	*/
/*	$NOWHERE: dev_hppa.c,v 2.1 1998/06/17 20:51:54 mickey Exp $	*/

/*
 * Copyright (c) 1998 Michael Shalayeff
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
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *	This product includes software developed by Michael Shalayeff.
 * 4. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR 
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED 
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 */

#include "libsa.h"
#include <sys/param.h>
#include <sys/disklabel.h>
#include <sys/reboot.h>
#include <dev/cons.h>

#include "dev_hppa.h"

struct  pz_device ctdev;	/* cartridge tape (boot) device path */

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
	{ "ct",  0 },
	{ "",   -1 },
	{ "rd", -1 },
	{ "sw", -1 },
	{ "fl",  1 },
	{ "sd",  1 },
};

/* pass dev_t to the open routines */
int
devopen(f, fname, file)
	struct open_file *f;
	const char *fname;
	char **file;
{
	register const struct pdc_devs *dp = pdc_devs;
	register int rc = 1;

	*file = (char *)fname;

#ifdef DEBUG
	if (debug)
		printf("devopen:");
#endif

	for (dp = pdc_devs; dp < &pdc_devs[NENTS(pdc_devs)]; dp++)
		if (strncmp(fname, dp->name, sizeof(dp->name)-1))
			break;

	if (dp >= &pdc_devs[NENTS(pdc_devs)] || dp->dev_type < 0)
		return ENODEV;

	if ((rc = (*devsw[dp->dev_type].dv_open)(f, file)) == 0) {
		f->f_dev = &devsw[dp->dev_type];
		return 0;
	}

	if ((f->f_flags & F_NODEV) == 0)
		f->f_dev = &devsw[dp->dev_type];

	return rc;
}

void
devboot(bootdev, p)
	dev_t bootdev;
	char *p;
{
	register const char *q;
#ifdef _TEST
	*p++ = '/';
	*p++ = 'd';
	*p++ = 'e';
	*p++ = 'v';
	*p++ = '/';
	*p++ = 'r';
#endif
	/* quick copy device name */
	for (q = pdc_devs[B_TYPE(bootdev)].name; (*p++ = *q++););
	*p++ = '0' + B_UNIT(bootdev);
	*p++ = 'a' + B_PARTITION(bootdev);
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
	register int c = cngetc();

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
	sprintf(ttyname_buf, "%s%d", cdevs[major(cn_tab->cn_dev)],
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
