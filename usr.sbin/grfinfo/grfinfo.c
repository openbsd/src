/*	$OpenBSD: grfinfo.c,v 1.4 2002/03/14 16:44:24 mpech Exp $	*/

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

#include <stdio.h>
#include <sys/types.h>
#include <sys/ioctl.h>
#include <dev/grfioctl.h>

int aflg = 0;
int tflg = 0;
char *pname;
char *dname, *tname();
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

main(argc, argv)
	char **argv;
{
	extern int optind, optopt;
	extern char *optarg;
	int c;

	pname = argv[0];
	while ((c = getopt(argc, argv, "at")) != -1)
		switch (c) {
		/* everything */
		case 'a':
			aflg++;
			break;
		/* type */
		case 't':
			tflg++;
			break;
		/* bogon */
		case '?':
			usage();
		}
	if (optind == argc)
		usage();
	dname = argv[optind];
	getinfo();
	if (aflg)
		printall();
	else
		printf("%s\n", tname());
	exit(0);
}

getinfo()
{
	int f;

	f = open(dname, 0);
	if (f < 0 || ioctl(f, GRFIOCGINFO, &gi) < 0) {
		if (tflg)
			printf("none\n");
		else
			perror(dname);
		exit(1);
	}
	close(f);
}

printall()
{
	printf("%s: %d x %d ", dname, gi.gd_dwidth, gi.gd_dheight);
	if (gi.gd_colors < 3)
		printf("monochrome");
	else {
		printf("%d color", gi.gd_colors);
		if (gi.gd_planes)
			printf(", %d plane", gi.gd_planes);
	}
	printf(" %s\n", tname());
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

usage()
{
	fprintf(stderr, "usage: %s [-at] device\n", pname);
	exit(1);
}
