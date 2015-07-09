/*	$OpenBSD: fdisk.c,v 1.74 2015/07/09 19:48:36 krw Exp $	*/

/*
 * Copyright (c) 1997 Tobias Weingartner
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

#include <sys/types.h>
#include <sys/fcntl.h>
#include <sys/disklabel.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <paths.h>
#include <stdint.h>
#include <err.h>

#include "disk.h"
#include "part.h"
#include "mbr.h"
#include "misc.h"
#include "cmd.h"
#include "user.h"

#define _PATH_MBR _PATH_BOOTDIR "mbr"
static unsigned char builtin_mbr[] = {
#include "mbrcode.h"
};

int	g_flag;
int	y_flag;

static void
usage(void)
{
	extern char * __progname;

	fprintf(stderr, "usage: %s "
	    "[-i|-u] [-egy] [-c # -h # -s #] [-f mbrfile] "
	    "[-l blocks] disk\n"
	    "\t-i: initialize disk with virgin MBR\n"
	    "\t-u: update MBR code, preserve partition table\n"
	    "\t-e: edit MBRs on disk interactively\n"
	    "\t-f: specify non-standard MBR template\n"
	    "\t-chs: specify disk geometry\n"
	    "\t-l: specify LBA block count\n"
	    "\t-y: do not ask questions\n"
	    "\t-g: initialize disk with EFI/GPT partition, requires -i\n"
	    "`disk' may be of the forms: sd0 or /dev/rsd0c.\n",
	    __progname);
	exit(1);
}


int
main(int argc, char *argv[])
{
	ssize_t len;
	int ch, fd;
	int i_flag = 0, e_flag = 0, u_flag = 0;
	int c_arg = 0, h_arg = 0, s_arg = 0;
	u_int32_t l_arg = 0;
	char *query;
#ifdef HAS_MBR
	char *mbrfile = _PATH_MBR;
#else
	char *mbrfile = NULL;
#endif
	struct dos_mbr dos_mbr;

	while ((ch = getopt(argc, argv, "ieguf:c:h:s:l:y")) != -1) {
		const char *errstr;

		switch(ch) {
		case 'i':
			i_flag = 1;
			break;
		case 'u':
			u_flag = 1;
			break;
		case 'e':
			e_flag = 1;
			break;
		case 'f':
			mbrfile = optarg;
			break;
		case 'c':
			c_arg = strtonum(optarg, 1, 262144, &errstr);
			if (errstr)
				errx(1, "Cylinder argument %s [1..262144].",
				    errstr);
			break;
		case 'h':
			h_arg = strtonum(optarg, 1, 256, &errstr);
			if (errstr)
				errx(1, "Head argument %s [1..256].", errstr);
			break;
		case 's':
			s_arg = strtonum(optarg, 1, 63, &errstr);
			if (errstr)
				errx(1, "Sector argument %s [1..63].", errstr);
			break;
		case 'g':
			g_flag = 1;
			break;
		case 'l':
			l_arg = strtonum(optarg, 64, UINT32_MAX, &errstr);
			if (errstr)
				errx(1, "Block argument %s [64..%u].", errstr,
				    UINT32_MAX);
			break;
		case 'y':
			y_flag = 1;
			break;
		default:
			usage();
		}
	}
	argc -= optind;
	argv += optind;

	memset(&disk, 0, sizeof(disk));

	/* Argument checking */
	if (argc != 1 || (i_flag && u_flag))
		usage();
	else
		disk.name = argv[0];

	if (g_flag != 0 && i_flag == 0) {
		warnx("-g specified without -i");
		usage();
	}

	/* Start with the disklabel geometry and get the sector size. */
	DISK_getlabelgeometry();

	if (c_arg | h_arg | s_arg) {
		/* Use supplied geometry if it is completely specified. */
		if (c_arg && h_arg && s_arg) {
			disk.cylinders = c_arg;
			disk.heads = h_arg;
			disk.sectors = s_arg;
			disk.size = c_arg * h_arg * s_arg;
		} else
			errx(1, "Please specify a full geometry with [-chs].");
	} else if (l_arg) {
		/* Use supplied size to calculate a geometry. */
		disk.cylinders = l_arg / 64;
		disk.heads = 1;
		disk.sectors = 64;
		disk.size = l_arg;
	}

	if (disk.size == 0 || disk.cylinders == 0 || disk.heads == 0 ||
	    disk.sectors == 0 || unit_types[SECTORS].conversion == 0)
		errx(1, "Can't get disk geometry, please use [-chs] "
		    "to specify.");

	/* Print out current MBRs on disk */
	if ((i_flag + u_flag + e_flag) == 0)
		USER_print_disk();

	/* Create initial/default MBR. */
	if (mbrfile != NULL && (fd = open(mbrfile, O_RDONLY)) == -1) {
		warn("%s", mbrfile);
		warnx("using builtin MBR");
		mbrfile = NULL;
	}
	if (mbrfile == NULL) {
		memcpy(&dos_mbr, builtin_mbr, sizeof(dos_mbr));
	} else {
		len = read(fd, &dos_mbr, sizeof(dos_mbr));
		if (len == -1)
			err(1, "Unable to read MBR from '%s'", mbrfile);
		else if (len != sizeof(dos_mbr))
			errx(1, "Unable to read complete MBR from '%s'",
			    mbrfile);
		close(fd);
	}
	MBR_parse(&dos_mbr, 0, 0, &initial_mbr);

	query = NULL;
	if (i_flag) {
		MBR_init(&initial_mbr);
		query = "Do you wish to write new MBR and partition table?";
	} else if (u_flag) {
		MBR_pcopy(&initial_mbr);
		query = "Do you wish to write new MBR?";
	}
	if (query && ask_yn(query))
		Xwrite(NULL, &initial_mbr);

	if (e_flag)
		USER_edit(0, 0);

	return (0);
}
