/*	$OpenBSD: fdisk.c,v 1.103 2018/04/26 15:55:14 guenther Exp $	*/

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
#include <sys/disklabel.h>

#include <err.h>
#include <fcntl.h>
#include <paths.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "disk.h"
#include "part.h"
#include "mbr.h"
#include "misc.h"
#include "cmd.h"
#include "user.h"
#include "gpt.h"

#define _PATH_MBR _PATH_BOOTDIR "mbr"
static unsigned char builtin_mbr[] = {
#include "mbrcode.h"
};

u_int32_t b_arg;
int	y_flag;

static void
usage(void)
{
	extern char * __progname;

	fprintf(stderr, "usage: %s "
	    "[-egvy] [-i|-u] [-b #] [-c # -h # -s #] "
	    "[-f mbrfile] [-l # ] disk\n"
	    "\t-b: specify special boot partition block count; requires -i\n"
	    "\t-chs: specify disk geometry; all three must be specified\n"
	    "\t-e: interactively edit MBR or GPT\n"
	    "\t-f: specify non-standard MBR template\n"
	    "\t-g: initialize disk with GPT; requires -i\n"
	    "\t-i: initialize disk with MBR unless -g is also specified\n"
	    "\t-l: specify LBA block count; cannot be used with -chs\n"
	    "\t-u: update MBR code; preserve partition table\n"
	    "\t-v: print the MBR, the Primary GPT and the Secondary GPT\n"
	    "\t-y: do not ask questions\n"
	    "`disk' may be of the forms: sd0 or /dev/rsd0c.\n",
	    __progname);
	exit(1);
}

int
main(int argc, char *argv[])
{
	ssize_t len;
	int ch, fd, error;
	int e_flag = 0, g_flag = 0, i_flag = 0, u_flag = 0;
	int verbosity = 0;
	int c_arg = 0, h_arg = 0, s_arg = 0;
	u_int32_t l_arg = 0;
	char *query;
#ifdef HAS_MBR
	char *mbrfile = _PATH_MBR;
#else
	char *mbrfile = NULL;
#endif
	struct dos_mbr dos_mbr;
	struct mbr mbr;

	while ((ch = getopt(argc, argv, "iegpuvf:c:h:s:l:b:y")) != -1) {
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
			disk.cylinders = c_arg;
			disk.size = c_arg * h_arg * s_arg;
			break;
		case 'h':
			h_arg = strtonum(optarg, 1, 256, &errstr);
			if (errstr)
				errx(1, "Head argument %s [1..256].", errstr);
			disk.heads = h_arg;
			disk.size = c_arg * h_arg * s_arg;
			break;
		case 's':
			s_arg = strtonum(optarg, 1, 63, &errstr);
			if (errstr)
				errx(1, "Sector argument %s [1..63].", errstr);
			disk.sectors = s_arg;
			disk.size = c_arg * h_arg * s_arg;
			break;
		case 'g':
			g_flag = 1;
			break;
		case 'b':
			b_arg = strtonum(optarg, 64, UINT32_MAX, &errstr);
			if (errstr)
				errx(1, "Block argument %s [64..%u].", errstr,
				    UINT32_MAX);
			break;
		case 'l':
			l_arg = strtonum(optarg, 64, UINT32_MAX, &errstr);
			if (errstr)
				errx(1, "Block argument %s [64..%u].", errstr,
				    UINT32_MAX);
			disk.cylinders = l_arg / 64;
			disk.heads = 1;
			disk.sectors = 64;
			disk.size = l_arg;
			break;
		case 'y':
			y_flag = 1;
			break;
		case 'v':
			verbosity++;
			break;
		default:
			usage();
		}
	}
	argc -= optind;
	argv += optind;

	/* Argument checking */
	if (argc != 1 || (i_flag && u_flag) ||
	    (i_flag == 0 && (b_arg || g_flag)) ||
	    ((c_arg | h_arg | s_arg) && !(c_arg && h_arg && s_arg)) ||
	    ((c_arg | h_arg | s_arg) && l_arg))
		usage();

	disk.name = argv[0];
	DISK_open(i_flag || u_flag || e_flag);

	/* "proc exec" for man page display */
	if (pledge("stdio rpath wpath disklabel proc exec", NULL) == -1)
		err(1, "pledge");

	error = MBR_read(0, &dos_mbr);
	if (error)
		errx(1, "Can't read sector 0!");
	MBR_parse(&dos_mbr, 0, 0, &mbr);

	/* Get the GPT if present. Either primary or secondary is ok. */
	if (MBR_protective_mbr(&mbr) == 0)
		GPT_get_gpt(0);

	if (!(i_flag || u_flag || e_flag)) {
		if (pledge("stdio", NULL) == -1)
			err(1, "pledge");
		USER_print_disk(verbosity);
		goto done;
	}

	/* Create initial/default MBR. */
	if (mbrfile == NULL) {
		memcpy(&dos_mbr, builtin_mbr, sizeof(dos_mbr));
	} else {
		fd = open(mbrfile, O_RDONLY);
		if (fd == -1) {
			warn("%s", mbrfile);
			warnx("using builtin MBR");
			memcpy(&dos_mbr, builtin_mbr, sizeof(dos_mbr));
		} else {
			len = read(fd, &dos_mbr, sizeof(dos_mbr));
			close(fd);
			if (len == -1)
				err(1, "Unable to read MBR from '%s'", mbrfile);
			else if (len != sizeof(dos_mbr))
				errx(1, "Unable to read complete MBR from '%s'",
				    mbrfile);
		}
	}
	MBR_parse(&dos_mbr, 0, 0, &initial_mbr);

	query = NULL;
	if (i_flag) {
		reinited = 1;
		if (g_flag) {
			MBR_init_GPT(&initial_mbr);
			GPT_init();
			query = "Do you wish to write new GPT?";
		} else {
			MBR_init(&initial_mbr);
			query = "Do you wish to write new MBR and "
			    "partition table?";
		}
	} else if (u_flag) {
		memcpy(initial_mbr.part, mbr.part, sizeof(initial_mbr.part));
		query = "Do you wish to write new MBR?";
	}
	if (query && ask_yn(query))
		Xwrite(NULL, &initial_mbr);

	if (e_flag)
		USER_edit(0, 0);

done:
	close(disk.fd);

	return (0);
}
