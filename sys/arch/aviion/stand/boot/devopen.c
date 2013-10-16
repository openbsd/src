/*	$OpenBSD: devopen.c,v 1.4 2013/10/16 16:59:34 miod Exp $	*/

/*
 * Copyright (c) 2013 Miodrag Vallat.
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
#include <stand.h>

#include "libsa.h"

/*
 * Parse the boot commandline into a proper device specification and
 * kernel filename.
 */
int
devopen(struct open_file *f, const char *fname, char **file)
{
	struct devsw *dp;
	int error, i;
	const char *po, *pc, *pc2, *p, *comma;
	char ctrl[1 + 4], device[1 + 4];
	uint controller, unit, lun, part;
	size_t devlen;

	/* defaults */
	controller = unit = lun = part = 0;
	device[0] = '\0';
	ctrl[0] = '\0';

	/*
	 * Attempt to parse the name as
	 *     ctrlnam([num[,unit[,lun]]])[partname:]filename
	 * or
	 *     devnam([num|ctrlnam([num]][,initiator])[,unit[,lun]])
	 *						[partname:]filename
	 *
	 * With device names being "sd" or "st" for storage devices,
	 * or a controller name for network devices, and controller names
	 * being four letter long ("dgen", "dgsc", "inen", "ncsc" ...)
	 *
	 * Initiator id is always ignored.
	 */

	po = strchr(fname, '(');
	if (po != NULL) {
		pc = strchr(fname, ')');
		if (pc == NULL || pc < po)
			return EINVAL;
		p = strchr(po + 1, '(');

		if (p != NULL && p < pc) {
			pc2 = strchr(pc + 1, ')');
			if (pc2 == NULL)
				return EINVAL;
		} else
			pc2 = NULL; /* XXX gcc3 -Wuninitialized */

		devlen = po++ - fname;
		if (devlen > 4)
			return EINVAL;
		memcpy(device, fname, devlen);
		device[devlen] = '\0';

		if (p != NULL && p < pc) {
			/* second form. extract controller name */
			devlen = p++ - po;
			if (devlen > 4)
				return EINVAL;
			memcpy(ctrl, po, devlen);
			ctrl[devlen] = '\0';

			controller = strtol(p, NULL, 0);
			po = pc + 1;
			pc = pc2;
		} else {
			/* first form. extract controller number */
			controller = strtol(po, NULL, 0);
		}

		comma = strchr(po, ',');
		if (comma != NULL && comma < pc) {
			comma++;
			unit = strtol(comma, NULL, 0);
			po = comma;
		}

		comma = strchr(po, ',');
		if (comma != NULL && comma < pc) {
			comma++;
			lun = strtol(comma, NULL, 0);
			po = comma;
		}

		pc++;
		fname = pc;
	} else {
		/* no controller, keep defaults */
	}

	p = strchr(fname, ':');
	if (p != NULL) {
		part = strtol(fname, NULL, 0);
		fname = p + 1;
	}

	while (*fname == '/' || *fname == ':')
		fname++;

	*file = (char *)fname;

	for (dp = devsw, i = 0; i < ndevs; dp++, i++)
		if (dp->dv_name != NULL && strcmp(dp->dv_name, device) == 0)
			break;
	if (i == ndevs)
		return ENXIO;

	error = (*dp->dv_open)(f, ctrl, controller, unit, lun, part);
	if (error == 0) {
		f->f_dev = dp;
		/* update global boot_info */
		bi.bootdev = controller;
		bi.bootunit = unit;
		bi.bootlun = lun;
		bi.bootpart = part;
	}

	return error;
}
