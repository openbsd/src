/*	$OpenBSD: hilinfo.c,v 1.6 2004/08/01 18:32:18 deraadt Exp $	*/
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
 * 	from: Utah $Hdr: hilinfo.c 1.3 94/04/04$
 */

#include <errno.h>
#include <stdio.h>
#include <unistd.h>
#include <util.h>
#include <sys/types.h>
#include <sys/ioctl.h>
#include <dev/hilioctl.h>

int getinfo(char *);
void printall(void);
char *tname(void);
void usage(void);

struct _hilbuf11 hi;
struct _hilbuf16 sc;

struct hil_info {
	u_char	hil_lo;
	u_char	hil_hi;
	char	*hil_name;
} info[] = {
	0xA0,	0xFF,	"keyboard",
	0x60,	0x6B,	"mouse",
	0x90,	0x97,	"tablet",
	0x34,	0x34,	"id-module",
	0x30,	0x30,	"button-box",
	0x00,	0x00,	"unknown",
};

int
main(int argc, char *argv[])
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
getinfo(char *dname)
{
	int f;

	f = opendev(dname, 0, OPENDEV_BLCK, NULL);
	if (f < 0) {
		warn("open(%s)", dname);
		return 0;
	}
	if (ioctl(f, HILIOCID, &hi) < 0 || ioctl(f, HILIOCSC, &sc) < 0) {
		warn("ioctl(%s)", dname);
		close(f);
		return 0;
	}

	close(f);
	return 1;
}

void
printall(void)
{
	int i;

	printf("%s, info: ", tname());
	for (i = 0; i < 11; i++)
		printf("%2.2x", hi.string[i]);
	if (strcmp(tname(), "id-module") == 0) {
		printf(", sc: ");
		for (i = 0; i < 16; i++)
			printf("%2.2x", sc.string[i]);
	}
	printf("\n");
}

char *
tname(void)
{
	struct hil_info *hp;

	for (hp = info; hp->hil_lo; hp++)
		if (hi.string[0] >= hp->hil_lo && hi.string[0] <= hp->hil_hi)
			break;
	if (hi.string[0] == 0x61 && hi.string[1] == 0x13)
		return("knobs");
	return(hp->hil_name);
}

void
usage(void)
{
	extern char *__progname;

	fprintf(stderr, "usage: %s [-at] device\n", __progname);
	exit(1);
}
