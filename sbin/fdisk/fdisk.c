/*	$OpenBSD: fdisk.c,v 1.114 2021/06/28 19:50:30 krw Exp $	*/

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

#include <sys/param.h>	/* DEV_BSIZE */
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

uint32_t b_sectors, b_offset;
uint8_t b_type;
int	A_flag, y_flag;

static void
usage(void)
{
	extern char * __progname;

	fprintf(stderr, "usage: %s "
	    "[-evy] [-i [-g] | -u | -A ] [-b blocks[@offset[:type]]]\n"
	    "\t[-l blocks | -c cylinders -h heads -s sectors] [-f mbrfile] disk\n",
	    __progname);
	exit(1);
}

int
main(int argc, char *argv[])
{
	ssize_t len;
	int ch, fd, efi, error;
	unsigned int bps;
	int e_flag = 0, g_flag = 0, i_flag = 0, u_flag = 0;
	int verbosity = TERSE;
	int c_arg = 0, h_arg = 0, s_arg = 0;
	uint32_t l_arg = 0;
	char *query;
#ifdef HAS_MBR
	char *mbrfile = _PATH_MBR;
#else
	char *mbrfile = NULL;
#endif
	struct dos_mbr dos_mbr;
	struct mbr mbr;

	while ((ch = getopt(argc, argv, "Aiegpuvf:c:h:s:l:b:y")) != -1) {
		const char *errstr;

		switch(ch) {
		case 'A':
			A_flag = 1;
			break;
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
			parse_b(optarg, &b_sectors, &b_offset, &b_type);
			break;
		case 'l':
			l_arg = strtonum(optarg, BLOCKALIGNMENT, UINT32_MAX, &errstr);
			if (errstr)
				errx(1, "Block argument %s [%u..%u].", errstr,
				    BLOCKALIGNMENT, UINT32_MAX);
			disk.cylinders = l_arg / BLOCKALIGNMENT;
			disk.heads = 1;
			disk.sectors = BLOCKALIGNMENT;
			disk.size = l_arg;
			break;
		case 'y':
			y_flag = 1;
			break;
		case 'v':
			verbosity = VERBOSE;
			break;
		default:
			usage();
		}
	}
	argc -= optind;
	argv += optind;

	/* Argument checking */
	if (argc != 1 || (i_flag && u_flag) ||
	    (i_flag == 0 && g_flag) ||
	    (b_sectors && !(i_flag || A_flag)) ||
	    ((c_arg | h_arg | s_arg) && !(c_arg && h_arg && s_arg)) ||
	    ((c_arg | h_arg | s_arg) && l_arg))
		usage();

	disk.name = argv[0];
	DISK_open(A_flag || i_flag || u_flag || e_flag);
	bps = DL_BLKSPERSEC(&dl);
	if (b_sectors > 0) {
		if (b_sectors % bps != 0)
			b_sectors += bps - b_sectors % bps;
		if (b_offset % bps != 0)
			b_offset += bps - b_offset % bps;
		b_sectors = DL_BLKTOSEC(&dl, b_sectors);
		b_offset = DL_BLKTOSEC(&dl, b_offset);
	}
	if (l_arg > 0) {
		if (l_arg % bps != 0)
			l_arg += bps - l_arg % bps;
		l_arg = DL_BLKTOSEC(&dl, l_arg);
		disk.cylinders = l_arg / BLOCKALIGNMENT;
		disk.heads = 1;
		disk.sectors = BLOCKALIGNMENT;
		disk.size = l_arg;
	}

	/* "proc exec" for man page display */
	if (pledge("stdio rpath wpath disklabel proc exec", NULL) == -1)
		err(1, "pledge");

	error = MBR_read(0, &dos_mbr);
	if (error)
		errx(1, "Can't read sector 0!");
	MBR_parse(&dos_mbr, 0, 0, &mbr);

	/* Get the GPT if present. Either primary or secondary is ok. */
	efi = MBR_protective_mbr(&mbr);
	if (efi != -1)
		GPT_read(ANYGPT);

	if (!(A_flag || i_flag || u_flag || e_flag)) {
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
	if (A_flag) {
		if (letoh64(gh.gh_sig) != GPTSIGNATURE)
			errx(1, "-A requires a valid GPT");
		else {
			initial_mbr = mbr;	/* Keep current MBR. */
			GPT_init(GPONLY, b_sectors);
			query = "Do you wish to write new GPT?";
		}
	} else if (i_flag) {
		if (g_flag) {
			MBR_init_GPT(&initial_mbr);
			GPT_init(GHANDGP, b_sectors);
			query = "Do you wish to write new GPT?";
		} else {
			memset(&gh, 0, sizeof(gh));
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
