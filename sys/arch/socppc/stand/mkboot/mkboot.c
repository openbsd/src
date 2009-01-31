/*	$OpenBSD: mkboot.c,v 1.2 2009/01/31 21:10:09 grange Exp $	*/

/*
 * Copyright (c) 2008 Mark Kettenis
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
#include <err.h>
#include <fcntl.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>
#include <zlib.h>

#define IH_OS_OPENBSD		1 /* OpenBSD */

#define IH_CPU_PPC		7 /* PowerPC */

#define IH_TYPE_STANDALONE	1 /* Standalone */

#define IH_COMP_NONE		0 /* No compression */

#define IH_MAGIC		0x27051956	/* Image Magic Number */
#define IH_NMLEN		32 		/* Image Name Length */

struct image_header {
	uint32_t	ih_magic;
	uint32_t	ih_hcrc;
	uint32_t	ih_time;
	uint32_t	ih_size;
	uint32_t	ih_load;
	uint32_t	ih_ep;
	uint32_t	ih_dcrc;
	uint8_t		ih_os;
	uint8_t		ih_arch;
	uint8_t		ih_type;
	uint8_t		ih_comp;
	uint8_t		ih_name[IH_NMLEN];
};

extern char *__progname;

void	usage(void);

int
main(int argc, char *argv[])
{
	struct image_header ih;
	const char *iname, *oname;
	int ifd, ofd;
	u_long crc;
	ssize_t nbytes;
	char buf[BUFSIZ];
	int c, ep, load;

	ep = load = 0;
	while ((c = getopt(argc, argv, "e:l:")) != -1) {
		switch (c) {
		case 'e':
			sscanf(optarg, "0x%x", &ep);
			break;
		case 'l':
			sscanf(optarg, "0x%x", &load);
			break;
		default:
			usage();
		}
	}
	if (argc - optind != 2)
		usage();

	iname = argv[optind++];
	oname = argv[optind++];

	/* Initialize U-Boot header. */
	bzero(&ih, sizeof ih);
	ih.ih_magic = IH_MAGIC;
	ih.ih_time = time(NULL);
	ih.ih_load = load;
	ih.ih_ep = ep;
	ih.ih_os = IH_OS_OPENBSD;
	ih.ih_arch = IH_CPU_PPC;
	ih.ih_type = IH_TYPE_STANDALONE;
	ih.ih_comp = IH_COMP_NONE;
	strlcpy(ih.ih_name, "boot", sizeof ih.ih_name);

	ifd = open(iname, O_RDONLY);
	if (ifd < 0)
		err(1, "%s", iname);

	ofd = open(oname, O_RDWR | O_TRUNC | O_CREAT, 0644);
	if (ofd < 0)
		err(1, "%s", oname);

	/* Write initial header. */
	if (write(ofd, &ih, sizeof ih) != sizeof ih)
		err(1, "%s", oname);

	/* Copy data, calculating the data CRC as we go. */
	crc = crc32(0L, Z_NULL, 0);
	while ((nbytes = read(ifd, buf, sizeof buf)) != 0) {
		if (nbytes == -1)
			err(1, "%s", iname);
		if (write(ofd, buf, nbytes) != nbytes)
			err(1, "%s", oname);
		crc = crc32(crc, buf, nbytes);
		ih.ih_size += nbytes;
	}
	ih.ih_dcrc = htonl(crc);

	/* Calculate header CRC. */
	crc = crc32(0, (void *)&ih, sizeof ih);
	ih.ih_hcrc = htonl(crc);

	/* Write finalized header. */
	if (lseek(ofd, 0, SEEK_SET) != 0)
		err(1, "%s", oname);
	if (write(ofd, &ih, sizeof ih) != sizeof ih)
		err(1, "%s", oname);

	return(0);
}

void
usage(void)
{
	(void)fprintf(stderr,
	    "usage: %s [-e entry] [-l loadaddr] infile outfile\n", __progname);
	exit(1);
}
