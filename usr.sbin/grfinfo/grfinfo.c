/*	$OpenBSD: grfinfo.c,v 1.5 2002/09/06 22:08:36 miod Exp $	*/

/* 
 * Copyright (c) 1987-1993, The University of Utah and
 * the Center for Software Science at the University of Utah (CSS).
 * All rights reserved.
 *
 * Permission to use, copy, modify and distribute this software is hereby
 * granted provided that (1) source code retains these copyright, permission,
 * and disclaimer notices, and (2) redistributions including binaries
 * reproduce the notices in supporting documentation, and (3) all advertising
 * materials mentioning features or use of this software display the following
 * acknowledgement: ``This product includes software developed by the Center
 * for Software Science at the University of Utah.''
 *
 * THE UNIVERSITY OF UTAH AND CSS ALLOW FREE USE OF THIS SOFTWARE IN ITS "AS
 * IS" CONDITION.  THE UNIVERSITY OF UTAH AND CSS DISCLAIM ANY LIABILITY OF
 * ANY KIND FOR ANY DAMAGES WHATSOEVER RESULTING FROM THE USE OF THIS SOFTWARE.
 *
 * CSS requests users of this software to return to css-dist@cs.utah.edu any
 * improvements that they make and grant CSS redistribution rights.
 *
 * 	from: Utah $Hdr: grfinfo.c 1.3 94/04/04$
 */

#include <errno.h>
#include <stdio.h>
#include <unistd.h>
#include <util.h>
#include <sys/types.h>
#include <sys/ioctl.h>
#include <dev/grfioctl.h>

int getinfo(char *);
void printall(void);
char *tname(void);
void usage(void);

struct grfinfo gi;

struct grf_info {
	int	grf_id;
	char	*grf_name;
} info[] = {
	GRFGATOR,	"gatorbox",
	GRFBOBCAT,	"topcat",
	GRFRBOX,	"renaissance",
	GRFFIREEYE,	"fireeye",
	GRFHYPERION,	"hyperion",
	GRFDAVINCI,	"davinci",
	-1,		"unknown",
};

int
main(argc, argv)
	char **argv;
{
	int aflg, tflg;
	int c;
	char *dname;

	aflg = tflg = 0;
	while ((c = getopt(argc, argv, "at")) != -1) {
		switch (c) {
		case 'a':
			if (tflg != 0)
				usage();
			aflg++;
			break;
		case 't':
			if (aflg != 0)
				usage();
			tflg++;
			break;
		case '?':
		default:
			usage();
		}
	}
	argc -= optind;
	argv += optind;

	if (argc == 0)
		usage();
	while (argc-- != 0) {
		dname = *argv++;
		if (getinfo(dname)) {
			printf("%s: ", dname);
			if (aflg)
				printall();
			else
				printf("%s\n", tname());
		}
	}
	exit(0);
}

int
getinfo(dname)
	char *dname;
{
	int f;

	f = opendev(dname, 0, OPENDEV_BLCK, NULL);
	if (f < 0) {
		warn("open(%s)", dname);
		return 0;
	}
	if (ioctl(f, GRFIOCGINFO, &gi) < 0) {
		warn("ioctl(%s)", dname);
		close(f);
		return 0;
	}

	close(f);
	return 1;
}

void
printall()
{
	printf("%d x %d, ", gi.gd_dwidth, gi.gd_dheight);
	if (gi.gd_colors < 3)
		printf("monochrome, ");
	else {
		printf("%d colors, ", gi.gd_colors);
		if (gi.gd_planes)
			printf("%d planes, ", gi.gd_planes);
	}
	printf("%s\n", tname());
	printf("registers: 0x%x bytes at 0x%x\n",
	       gi.gd_regsize, gi.gd_regaddr);
	printf("framebuf:  0x%x bytes at 0x%x (%d x %d)\n",
	       gi.gd_fbsize, gi.gd_fbaddr, gi.gd_fbwidth, gi.gd_fbheight);
}

char *
tname()
{
	struct grf_info *gp;

	for (gp = info; gp->grf_id >= 0; gp++)
		if (gi.gd_id == gp->grf_id)
			break;
	/*
	 * Heuristics to differentiate catseye from topcat:
	 *	low-res color catseye has 1k x 1k framebuffer and 64 colors
	 *	hi-res mono and color catseye have 1280 wide display
	 */
	if (gi.gd_id == GRFBOBCAT &&
	    (gi.gd_dwidth == 1280 ||
	     gi.gd_fbsize == 0x100000 && gi.gd_colors == 64))
		return("catseye");
	return(gp->grf_name);
}

void
usage()
{
	extern char *__progname;

	fprintf(stderr, "usage: %s [-at] device\n", __progname);
	exit(1);
}
