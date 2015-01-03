/*	$OpenBSD: fdisk.c,v 1.64 2015/01/03 15:50:50 jsing Exp $	*/

/*
 * Copyright (c) 1997 Tobias Weingartner
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
 * IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
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
	    "[-egiuy] [-c cylinders -h heads -s sectors] [-f mbrfile] "
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
	int ch, fd, error;
	int i_flag = 0, m_flag = 0, u_flag = 0;
	int c_arg = 0, h_arg = 0, s_arg = 0;
	struct disk disk;
	u_int32_t l_arg = 0;
#ifdef HAS_MBR
	char *mbrfile = _PATH_MBR;
#else
	char *mbrfile = NULL;
#endif
	struct mbr mbr;
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
			m_flag = 1;
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
	if (argc != 1)
		usage();
	else
		disk.name = argv[0];

	if (g_flag != 0 && i_flag == 0) {
		warnx("-g specified without -i");
		usage();
	}

	/* Start with the disklabel geometry and get the sector size. */
	DISK_getlabelgeometry(&disk);

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
	if ((i_flag + u_flag + m_flag) == 0)
		exit(USER_print_disk(&disk));

	/* Parse mbr template, to pass on later */
	if (mbrfile != NULL && (fd = open(mbrfile, O_RDONLY)) == -1) {
		warn("%s", mbrfile);
		warnx("using builtin MBR");
		mbrfile = NULL;
	}
	if (mbrfile == NULL) {
		memcpy(&dos_mbr, builtin_mbr, sizeof(dos_mbr));
	} else {
		error = MBR_read(fd, 0, &dos_mbr);
		if (error == -1)
			err(1, "Unable to read MBR");
		close(fd);
	}
	MBR_parse(&disk, &dos_mbr, 0, 0, &mbr);

	/* Now do what we are supposed to */
	if (i_flag || u_flag)
		if (USER_init(&disk, &mbr, u_flag) == -1)
			err(1, "error initializing MBR");

	if (m_flag)
		USER_modify(&disk, &mbr, 0, 0);

	return (0);
}
