/*	$OpenBSD: devopen.c,v 1.5 1998/02/03 11:48:26 maja Exp $ */
/*	$NetBSD: devopen.c,v 1.8 1997/06/08 17:49:19 ragge Exp $ */
/*
 * Copyright (c) 1997 Ludd, University of Lule}, Sweden.
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
 *      This product includes software developed at Ludd, University of 
 *      Lule}, Sweden and its contributors.
 * 4. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include <sys/reboot.h>

#include "lib/libsa/stand.h"
#include "samachdep.h"
#include "vaxstand.h"

unsigned int opendev;

int
devopen(f, fname, file)
	struct open_file *f;
	const char *fname;
	char **file;
{
	int dev, ctlr, unit, part, adapt, i, a[4], x;
	struct devsw *dp;
	extern struct fs_ops nfs_system[];
	extern int cnvtab[];
	char *s, *c;

	dev   = B_TYPE(bootdev);
	ctlr  = B_CONTROLLER(bootdev);
	unit  = B_UNIT(bootdev);
	part  = B_PARTITION(bootdev);
	adapt = B_ADAPTOR(bootdev);

	for (i = 0, dp = 0; i < ndevs; i++)
		if (cnvtab[i] == dev)
			dp = devsw + i;

	x = 0;
	if ((s = index(fname, '('))) {
		*s++ = 0;

		for (i = 0, dp = devsw; i < ndevs; i++, dp++)
			if (dp->dv_name && strcmp(dp->dv_name, fname) == 0)
				break;

		if (i == ndevs) {
			printf("No such device - Configured devices are:\n");
			for (dp = devsw, i = 0; i < ndevs; i++, dp++)
				if (dp->dv_name)
					printf(" %s", dp->dv_name);
			printf("\n");
			return -1;
		}
		dev = cnvtab[i];
		if ((c = index(s, ')')) == 0)
			goto usage;

		*c++ = 0;

		if (*s) do {
			a[x++] = atoi(s);
			while (*s >= '0' && *s <= '9')
				s++;

			if (*s != ',' && *s != 0)
				goto usage;
		} while (*s++);

		if (x)
			part = a[x - 1];
		if (x > 1)
			unit = a[x - 2];
		if (x > 2)
			ctlr = a[x - 3];
		if (x > 3)
			adapt = a[0];
		*file = c;
	} else
		*file = (char *)fname;

#ifdef notyet
	if ((u = index(s, ' '))) {
		*u++ = 0;

		if (*u != '-')
			goto usage;

		while (*++u) {
			if (*u == 'a')
				bdev |= RB_ASKNAME;
			else if (*u == 'd')
				bdev |= RB_DEBUG;
			else if (*u == 's')
				bdev |= RB_SINGLE;
			else
				goto usage;
		}

	}
#endif

	if (!dp->dv_open)
		return(ENODEV);
	f->f_dev = dp;

	opendev = MAKEBOOTDEV(dev, adapt, ctlr, unit, part);

	if (dev > 95) { /* MOP boot over network, root & swap over NFS */
		bcopy(nfs_system, file_system, sizeof(struct fs_ops));
		i = (*dp->dv_open)(f, dp->dv_name);
	} else
		i = (*dp->dv_open)(f, adapt, ctlr, unit, part);

	return i;

usage:
	printf("usage: dev(adapter,controller,unit,partition)file -asd\n");
	return -1;
}
