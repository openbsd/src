/*	$OpenBSD: devopen.c,v 1.5 2011/07/06 18:32:59 miod Exp $ */
/*	$NetBSD: devopen.c,v 1.10 2002/05/24 21:40:59 ragge Exp $ */
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

#include <lib/libsa/stand.h>

#include "machine/rpb.h"
#include "machine/sid.h"
#include "machine/pte.h"

#include "vaxstand.h"

int	atoi(char *);
int nexaddr, csrbase;

int
devopen(f, fname, file)
	struct open_file *f;
	const char *fname;
	char **file;
{
	int dev, unit, ctlr, part, adapt, i, a[4], x;
	int *mapregs;
	struct devsw *dp;
	extern int cnvtab[];
	char *s, *c;

	part = 0;

	/*
	 * Adaptor and controller are normally zero (or uninteresting),
	 * but we need to do some conversion here anyway (if it's a 
	 * manual boot, but that's checked by the device driver).
	 * Set them to -1 to tell if it's a set number or default.
	 */
	dev = bootrpb.devtyp;
	unit = bootrpb.unit;
	adapt = ctlr = -1;

	if (dev == BDEV_KDB)
		dev = BDEV_UDA; /* use the same driver */

	for (i = 0, dp = 0; i < ndevs; i++)
		if (cnvtab[i] == dev)
			dp = devsw + i;

	if ((s = index((char *)fname, '('))) {
		*s++ = 0;

		for (i = 0, dp = devsw; i < ndevs; i++, dp++)
			if (dp->dv_name && strcmp(dp->dv_name, fname) == 0)
				break;

		if (i == ndevs) {
			printf("No such device - Configured devices are:\n");
			for (dp = devsw, i = 0; i < ndevs; i++, dp++)
				if (dp->dv_name && (i == 0 ||
				    strcmp(dp->dv_name, (dp[-1]).dv_name) != 0))
					printf(" %s", dp->dv_name);
			printf("\n");
			return EINVAL;
		}

		dev = cnvtab[i];
		if ((c = index(s, ')')) == 0)
			goto usage;
		*c++ = 0;

		/*
		 * If the user has specified sd(), we need to pick the
		 * correct BDEV_SDx value, depending on the machine type.
		 */
		if (dev == BDEV_SD) {
			switch (vax_boardtype) {
			case VAX_BTYP_46:
			case VAX_BTYP_48:
			case VAX_BTYP_49:
			case VAX_BTYP_1303:
				dev = BDEV_SDN;		/* asc(4) */
				break;
			case VAX_BTYP_60:
				dev = BDEV_SDS;		/* sii(4) */
				break;
			}
		}

		x = 0;
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
	} else {
		*file = (char *)fname;
		c = (char *)fname;
	}

	if (!dp->dv_open) {
		printf("Can't open device type %d\n", dev);
		return ENXIO;
	}
	f->f_dev = dp;
	bootrpb.unit = unit;
	bootrpb.devtyp = dev;

	nexaddr = bootrpb.adpphy;
	switch (vax_boardtype) {
	case VAX_BTYP_610:
		nexaddr = 0; /* No map regs */
		csrbase = 0x20000000;
		break;

	case VAX_BTYP_VXT:
		nexaddr = 0;
		csrbase = bootrpb.csrphy;
		break;
	default:
		nexaddr = 0; /* No map regs */
		csrbase = 0x20000000;
		/* Always map in the lowest 4M on qbus-based machines */
		mapregs = (void *)0x20088000;
		if (bootrpb.adpphy == 0x20087800)
			for (i = 0; i < 8192; i++)
				mapregs[i] = PG_V | i;
		break;
	}

#ifdef DEV_DEBUG
	printf("rpb.type %d rpb.unit %d rpb.csr %lx rpb.adp %lx\n",
	    bootrpb.devtyp, bootrpb.unit, bootrpb.csrphy, bootrpb.adpphy);
	printf("adapter %d ctlr %d unit %d part %d\n", adapt, ctlr, unit, part);
	printf("nexaddr %x csrbase %x\n", nexaddr, csrbase);
#endif

	return (*dp->dv_open)(f, adapt, ctlr, unit, part);

usage:
	printf("usage: dev(adapter,controller,unit,partition)file -asd\n");
	return EINVAL;
}
