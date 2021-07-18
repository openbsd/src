/*	$OpenBSD: fdisk.c,v 1.126 2021/07/18 15:28:37 krw Exp $	*/

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

#include <ctype.h>
#include <err.h>
#include <fcntl.h>
#include <paths.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "part.h"
#include "disk.h"
#include "mbr.h"
#include "misc.h"
#include "cmd.h"
#include "user.h"
#include "gpt.h"

#define	_PATH_MBR		_PATH_BOOTDIR "mbr"
static unsigned char		builtin_mbr[] = {
#include "mbrcode.h"
};

int			A_flag, y_flag;

void			parse_bootprt(const char *);

static void
usage(void)
{
	extern char		* __progname;

	fprintf(stderr, "usage: %s "
	    "[-evy] [-i [-g] | -u | -A ] [-b blocks[@offset[:type]]]\n"
	    "\t[-l blocks | -c cylinders -h heads -s sectors] [-f mbrfile] disk\n",
	    __progname);
	exit(1);
}

int
main(int argc, char *argv[])
{
	struct dos_mbr		 dos_mbr;
	struct mbr		 mbr;
#ifdef HAS_MBR
	char			*mbrfile = _PATH_MBR;
#else
	char			*mbrfile = NULL;
#endif
	ssize_t			 len;
	int			 ch, fd, efi, error;
	int			 e_flag = 0, g_flag = 0, i_flag = 0, u_flag = 0;
	int			 verbosity = TERSE;
	int			 oflags = O_RDONLY;
	char			*query;

	while ((ch = getopt(argc, argv, "Aiegpuvf:c:h:s:l:b:y")) != -1) {
		const char *errstr;

		switch(ch) {
		case 'A':
			A_flag = 1;
			oflags = O_RDWR;
			break;
		case 'i':
			i_flag = 1;
			oflags = O_RDWR;
			break;
		case 'u':
			u_flag = 1;
			oflags = O_RDWR;
			break;
		case 'e':
			e_flag = 1;
			oflags = O_RDWR;
			break;
		case 'f':
			mbrfile = optarg;
			break;
		case 'c':
			disk.dk_cylinders = strtonum(optarg, 1, 262144, &errstr);
			if (errstr)
				errx(1, "Cylinder argument %s [1..262144].",
				    errstr);
			disk.dk_size = 0;
			break;
		case 'h':
			disk.dk_heads = strtonum(optarg, 1, 256, &errstr);
			if (errstr)
				errx(1, "Head argument %s [1..256].", errstr);
			disk.dk_size = 0;
			break;
		case 's':
			disk.dk_sectors = strtonum(optarg, 1, 63, &errstr);
			if (errstr)
				errx(1, "Sector argument %s [1..63].", errstr);
			disk.dk_size = 0;
			break;
		case 'g':
			g_flag = 1;
			break;
		case 'b':
			parse_bootprt(optarg);
			break;
		case 'l':
			disk.dk_size = strtonum(optarg, BLOCKALIGNMENT, UINT32_MAX, &errstr);
			if (errstr)
				errx(1, "Block argument %s [%u..%u].", errstr,
				    BLOCKALIGNMENT, UINT32_MAX);
			disk.dk_cylinders = disk.dk_heads = disk.dk_sectors = 0;
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

	if (argc != 1 || (i_flag && u_flag) ||
	    (i_flag == 0 && g_flag))
		usage();

	if ((disk.dk_cylinders || disk.dk_heads || disk.dk_sectors) &&
	    (disk.dk_cylinders * disk.dk_heads * disk.dk_sectors == 0))
		usage();

	DISK_open(argv[0], oflags);
	if (oflags == O_RDONLY) {
		if (pledge("stdio", NULL) == -1)
			err(1, "pledge");
		USER_print_disk(verbosity);
		goto done;
	}

	/* "proc exec" for man page display */
	if (pledge("stdio rpath wpath disklabel proc exec", NULL) == -1)
		err(1, "pledge");

	error = MBR_read(0, 0, &mbr);
	if (error)
		errx(1, "Can't read MBR!");

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
		if (GPT_read(ANYGPT))
			errx(1, "-A requires a valid GPT");
		else {
			initial_mbr = mbr;	/* Keep current MBR. */
			GPT_init(GPONLY);
			query = "Do you wish to write new GPT?";
		}
	} else if (i_flag) {
		if (g_flag) {
			MBR_init_GPT(&initial_mbr);
			GPT_init(GHANDGP);
			query = "Do you wish to write new GPT?";
		} else {
			MBR_init(&initial_mbr);
			query = "Do you wish to write new MBR and "
			    "partition table?";
		}
	} else if (u_flag) {
		memcpy(initial_mbr.mbr_prt, mbr.mbr_prt,
		    sizeof(initial_mbr.mbr_prt));
		query = "Do you wish to write new MBR?";
	}
	if (query && ask_yn(query))
		Xwrite(NULL, &initial_mbr);

	if (e_flag)
		USER_edit(0, 0);

done:
	close(disk.dk_fd);

	return 0;
}

void
parse_bootprt(const char *arg)
{
	const char		*errstr;
	char			*poffset, *ptype;
	uint32_t		 blockcount, blockoffset;
	uint8_t			 partitiontype;

	blockoffset = BLOCKALIGNMENT;
	partitiontype = DOSPTYP_EFISYS;
	ptype = NULL;

	/* First number: # of 512-byte blocks in boot partition. */
	poffset = strchr(arg, '@');
	if (poffset != NULL)
		*poffset++ = '\0';
	if (poffset != NULL) {
		ptype = strchr(poffset, ':');
		if (ptype != NULL)
			*ptype++ = '\0';
	}

	blockcount = strtonum(arg, BLOCKALIGNMENT, UINT32_MAX, &errstr);
	if (errstr)
		errx(1, "Block argument %s [%u..%u].", errstr, BLOCKALIGNMENT,
		    UINT32_MAX);

	if (poffset == NULL)
		goto done;

	/* Second number: # of 512-byte blocks to offset partition start. */
	blockoffset = strtonum(poffset, BLOCKALIGNMENT, UINT32_MAX, &errstr);
	if (errstr)
		errx(1, "Block offset argument %s [%u..%u].", errstr,
		    BLOCKALIGNMENT, UINT32_MAX);

	if (ptype == NULL)
		goto done;

	if (strlen(ptype) != 2 || !(isxdigit(*ptype) && isxdigit(*(ptype + 1))))
		errx(1, "Block type is not 2 digit hex value");

	partitiontype = strtol(ptype, NULL, 16);

 done:
	disk.dk_bootprt.prt_ns = blockcount;
	disk.dk_bootprt.prt_bs = blockoffset;
	disk.dk_bootprt.prt_id = partitiontype;
}
