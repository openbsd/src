/*	$OpenBSD: fdisk.c,v 1.135 2021/08/28 11:55:17 krw Exp $	*/

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
#include "cmd.h"
#include "misc.h"
#include "user.h"
#include "gpt.h"

#define	INIT_GPT		1
#define	INIT_GPTPARTITIONS	2
#define	INIT_MBR		3
#define	INIT_MBRBOOTCODE	4

#define	_PATH_MBR		_PATH_BOOTDIR "mbr"
static unsigned char		builtin_mbr[] = {
#include "mbrcode.h"
};

int			y_flag;

void			parse_bootprt(const char *);
void			get_default_dmbr(const char *, struct dos_mbr *);

static void
usage(void)
{
	extern char		* __progname;

	fprintf(stderr, "usage: %s "
	    "[-evy] [-A | -g | -i | -u] [-b blocks[@offset[:type]]]\n"
	    "\t[-l blocks | -c cylinders -h heads -s sectors] [-f mbrfile] disk\n",
	    __progname);
	exit(1);
}

int
main(int argc, char *argv[])
{
	struct mbr		 mbr;
#ifdef HAS_MBR
	const char		*mbrfile = _PATH_MBR;
#else
	const char		*mbrfile = NULL;
#endif
	const char		*errstr;
	int			 ch;
	int			 e_flag = 0, init = 0;
	int			 verbosity = TERSE;
	int			 oflags = O_RDONLY;

	while ((ch = getopt(argc, argv, "Ab:c:ef:gh:il:s:uvy")) != -1) {
		switch(ch) {
		case 'A':
			init = INIT_GPTPARTITIONS;
			break;
		case 'b':
			parse_bootprt(optarg);
			break;
		case 'c':
			disk.dk_cylinders = strtonum(optarg, 1, 262144, &errstr);
			if (errstr)
				errx(1, "Cylinder argument %s [1..262144].",
				    errstr);
			disk.dk_size = 0;
			break;
		case 'e':
			e_flag = 1;
			break;
		case 'f':
			mbrfile = optarg;
			break;
		case 'g':
			init = INIT_GPT;
			break;
		case 'h':
			disk.dk_heads = strtonum(optarg, 1, 256, &errstr);
			if (errstr)
				errx(1, "Head argument %s [1..256].", errstr);
			disk.dk_size = 0;
			break;
		case 'i':
			init = INIT_MBR;
			break;
		case 'l':
			disk.dk_size = strtonum(optarg, BLOCKALIGNMENT, UINT32_MAX, &errstr);
			if (errstr)
				errx(1, "Block argument %s [%u..%u].", errstr,
				    BLOCKALIGNMENT, UINT32_MAX);
			disk.dk_cylinders = disk.dk_heads = disk.dk_sectors = 0;
			break;
		case 's':
			disk.dk_sectors = strtonum(optarg, 1, 63, &errstr);
			if (errstr)
				errx(1, "Sector argument %s [1..63].", errstr);
			disk.dk_size = 0;
			break;
		case 'u':
			init = INIT_MBRBOOTCODE;
			break;
		case 'v':
			verbosity = VERBOSE;
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

	if (argc != 1)
		usage();

	if ((disk.dk_cylinders || disk.dk_heads || disk.dk_sectors) &&
	    (disk.dk_cylinders * disk.dk_heads * disk.dk_sectors == 0))
		usage();

	if (init || e_flag)
		oflags = O_RDWR;

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

	get_default_dmbr(mbrfile, &default_dmbr);

	switch (init) {
	case INIT_GPT:
		GPT_init(GHANDGP);
		if (ask_yn("Do you wish to write new GPT?"))
			Xwrite(NULL, &gmbr);
		break;
	case INIT_GPTPARTITIONS:
		if (GPT_read(ANYGPT))
			errx(1, "-A requires a valid GPT");
		GPT_init(GPONLY);
		if (ask_yn("Do you wish to write new GPT?"))
			Xwrite(NULL, &gmbr);
		break;
	case INIT_MBR:
		mbr.mbr_lba_self = mbr.mbr_lba_firstembr = 0;
		MBR_init(&mbr);
		if (ask_yn("Do you wish to write new MBR?"))
			Xwrite(NULL, &mbr);
		break;
	case INIT_MBRBOOTCODE:
		if (MBR_read(0, 0, &mbr))
			errx(1, "Can't read MBR!");
		memcpy(mbr.mbr_code, default_dmbr.dmbr_boot,
		    sizeof(mbr.mbr_code));
		if (ask_yn("Do you wish to write new MBR?"))
			Xwrite(NULL, &mbr);
		break;
	default:
		break;
	}

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
	int			 partitiontype;
	uint32_t		 blockcount, blockoffset;

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

	partitiontype = hex_octet(ptype);
	if (partitiontype == -1)
		errx(1, "Block type is not a 1-2 digit hex value");

 done:
	disk.dk_bootprt.prt_ns = blockcount;
	disk.dk_bootprt.prt_bs = blockoffset;
	disk.dk_bootprt.prt_id = partitiontype;
}

void
get_default_dmbr(const char *mbrfile, struct dos_mbr *dmbr)
{
	ssize_t			len;
	int			fd;

	if (mbrfile == NULL) {
		memcpy(dmbr, builtin_mbr, sizeof(*dmbr));
	} else {
		fd = open(mbrfile, O_RDONLY);
		if (fd == -1) {
			warn("%s", mbrfile);
			warnx("using builtin MBR");
			memcpy(dmbr, builtin_mbr, sizeof(*dmbr));
		} else {
			len = read(fd, dmbr, sizeof(*dmbr));
			close(fd);
			if (len == -1)
				err(1, "Unable to read MBR from '%s'", mbrfile);
			else if (len != sizeof(*dmbr))
				errx(1, "Unable to read complete MBR from '%s'",
				    mbrfile);
		}
	}
}
