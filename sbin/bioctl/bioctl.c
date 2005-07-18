/* $OpenBSD: bioctl.c,v 1.14 2005/07/18 14:34:41 jmc Exp $       */
/*
 * Copyright (c) 2004, 2005 Marco Peereboom
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
 * THIS SOFTWARE IS PROVIDED BY THE AUTHORS AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL THE AUTHORS OR CONTRIBUTORS BE LIABLE FOR
 * ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 */

#include <sys/ioctl.h>
#include <sys/param.h>
#include <sys/queue.h>
#include <scsi/scsi_disk.h>
#include <scsi/scsi_all.h>
#include <scsi/scsi_ses.h>
#include <dev/biovar.h>

#include <errno.h>
#include <err.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <util.h>

#include "bioctl.h"

/* globals */
const char *bio_device = "/dev/bio";

int devh = -1;
int debug = 0;

struct bio_locate bl;

int
main(int argc, char *argv[])
{
	extern char *optarg;

	u_int64_t func = 0;
	/* u_int64_t subfunc = 0; */

	int ch;
	int rv;

	char *bioc_dev = NULL;
	char *sd_dev = NULL;
	char *realname = NULL;

	if (argc < 2)
		usage();

	atexit(cleanup);

	while ((ch = getopt(argc, argv, "Dd:f:hi")) != -1) {
		switch (ch) {
		case 'D': /* debug */
			debug = 1;
			break;

		case 'd': /* bio device */
			bioc_dev = optarg;
			break;

		case 'f': /* scsi device */
			sd_dev = optarg;
			break;

		case 'i': /* inquiry */
			func |= BIOC_INQ;
			break;

		case 'h': /* help/usage */
			/* FALLTHROUGH */
		default:
			usage();
			/* NOTREACHED */
		}
	}

	if (sd_dev && bioc_dev)
		err(1, "-d and -f are mutually exclusive");

	if (bioc_dev) {
		devh = open(bio_device, O_RDWR);
		if (devh == -1)
			err(1, "Can't open %s", bio_device);

		bl.name = bioc_dev;
		rv = ioctl(devh, BIOCLOCATE, &bl);
		if (rv == -1)
			errx(1, "Can't locate %s device via %s",
			    bl.name, bio_device);
	}
	else if (sd_dev) {
	        devh = opendev(sd_dev, O_RDWR, OPENDEV_PART, &realname);
		if (devh == -1)
			err(1, "Can't open %s", sd_dev);
	}
	else
		errx(1, "need -d or -f parameter");

	if (debug)
		warnx("cookie = %p", bl.cookie);

	if (func & BIOC_INQ) {
		bio_inq();
	}

	return (0);
}

void
usage(void)
{
	extern char *__progname;

	fprintf(stderr, "usage: %s [-Dhi] -d device | -f device\n", __progname);

	exit(1);
}

void
cleanup(void)
{
	if (debug)
		printf("atexit\n");

	if (devh != -1)
		close(devh);
}

void
bio_inq(void)
{
	bioc_inq bi;
	bioc_vol bv;
	bioc_disk bd;

	int rv, i, d;

	memset(&bi, 0, sizeof(bi));

	if (debug)
		printf("bio_inq\n");

	bi.cookie = bl.cookie;

	rv = ioctl(devh, BIOCINQ, &bi);
	if (rv == -1) {
		warnx("bioc_ioctl() call failed");
		return;
	}

	printf("RAID volumes   : %d\n", bi.novol);
	printf("Physical disks : %d\n\n", bi.nodisk);

	for (i = 0; i < bi.novol; i++) {
		memset(&bv, 0, sizeof(bv));
		bv.cookie = bl.cookie;
		bv.volid = i;

		rv = ioctl(devh, BIOCVOL, &bv);
		if (rv == -1) {
			warnx("bioc_ioctl() call failed");
			return;
		}

		printf("\tvolume id: %d\n", bv.volid);
		printf("\tstatus   : %d\n", bv.status);
		printf("\tsize     : %lld\n", bv.size);
		printf("\traid     : %d\n", bv.level);
		printf("\tnr disks : %d\n", bv.nodisk);

		for (d = 0; d < bv.nodisk; d++) {
			memset(&bd, 0, sizeof(bd));
			bd.cookie = bl.cookie;
			bd.diskid = 0;

			rv = ioctl(devh, BIOCDISK, &bd);
			if (rv == -1) {
				warnx("bioc_ioctl() call failed");
				return;
			}

			printf("\t\tdisk id  : %d\n", bd.diskid);
			printf("\t\tstatus   : %d\n", bd.status);
			printf("\t\tvolume id: %d\n", bd.volid);
			printf("\t\tsize     : %lld\n", bd.size);
			printf("\t\tvendor   : %s\n", bd.vendor);
		}
		printf("\n");
	}
}
