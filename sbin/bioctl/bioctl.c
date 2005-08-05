/* $OpenBSD: bioctl.c,v 1.23 2005/08/05 02:40:36 deraadt Exp $       */

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
#include <dev/biovar.h>

#include <errno.h>
#include <err.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <ctype.h>
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
	char *bioc_dev = NULL, *sd_dev = NULL;
	char *realname = NULL, *al_arg = NULL;
	int ch, rv;

	if (argc < 2)
		usage();

	while ((ch = getopt(argc, argv, "a:Di")) != -1) {
		switch (ch) {
		case 'a': /* alarm */
			func |= BIOC_ALARM;
			al_arg = optarg;
			break;
		case 'D': /* debug */
			debug = 1;
			break;
		case 'i': /* inquiry */
			func |= BIOC_INQ;
			break;
		default:
			usage();
			/* NOTREACHED */
		}
	}
	argc -= optind;
	argv += optind;

	if (argc < 0 || argc > 1)
		usage();

	if (func == 0)
		func |= BIOC_INQ;

	/* if at least glob sd[0-9]*, it is a drive identifier */
	if (strncmp(argv[0], "sd", 2) == 0 && strlen(argv[0]) > 2 &&
	    isdigit(argv[0][2]))
		sd_dev = argv[0];
	else
		bioc_dev = argv[0];

	if (bioc_dev) {
		devh = open(bio_device, O_RDWR);
		if (devh == -1)
			err(1, "Can't open %s", bio_device);

		bl.name = bioc_dev;
		rv = ioctl(devh, BIOCLOCATE, &bl);
		if (rv == -1)
			errx(1, "Can't locate %s device via %s",
			    bl.name, bio_device);
	} else if (sd_dev) {
		devh = opendev(sd_dev, O_RDWR, OPENDEV_PART, &realname);
		if (devh == -1)
			err(1, "Can't open %s", sd_dev);
	} else
		errx(1, "need -d or -f parameter");

	if (debug)
		warnx("cookie = %p", bl.cookie);

	if (func & BIOC_INQ) {
		bio_inq();
	} else if (func == BIOC_ALARM) {
		bio_alarm(al_arg);
	}

	return (0);
}

void
usage(void)
{
	extern char *__progname;

	fprintf(stderr,
	    "usage: %s [-Di] [-a alarm-function] device\n", __progname);
	exit(1);
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
		printf("\tstatus   : ");
		switch (bv.status) {
		case BIOC_SVONLINE:
			printf("%s\n", BIOC_SVONLINE_S);
			break;

		case BIOC_SVOFFLINE:
			printf("%s\n", BIOC_SVOFFLINE_S);
			break;

		case BIOC_SVDEGRADED:
			printf("%s\n", BIOC_SVDEGRADED_S);
			break;

		case BIOC_SVINVALID:
		default:
			printf("%s\n", BIOC_SVINVALID_S);
		}
		printf("\tsize     : %lld\n", bv.size);
		printf("\traid     : %d\n", bv.level);
		printf("\tnr disks : %d\n", bv.nodisk);

		for (d = 0; d < bv.nodisk; d++) {
			memset(&bd, 0, sizeof(bd));
			bd.cookie = bl.cookie;
			bd.diskid = d;
			bd.volid = i;

			rv = ioctl(devh, BIOCDISK, &bd);
			if (rv == -1) {
				warnx("bioc_ioctl() call failed");
				return;
			}

			printf("\t\tdisk id  : %d\n", bd.diskid);
			printf("\t\tstatus   : ");
			switch (bd.status) {
			case BIOC_SDONLINE:
				printf("%s\n", BIOC_SDONLINE_S);
				break;

			case BIOC_SDOFFLINE:
				printf("%s\n", BIOC_SDOFFLINE_S);
				break;

			case BIOC_SDFAILED:
				printf("%s\n", BIOC_SDFAILED_S);
				break;

			case BIOC_SDREBUILD:
				printf("%s\n", BIOC_SDREBUILD_S);
				break;

			case BIOC_SDHOTSPARE:
				printf("%s\n", BIOC_SDHOTSPARE_S);
				break;

			case BIOC_SDUNUSED:
				printf("%s\n", BIOC_SDUNUSED_S);
				break;

			case BIOC_SDINVALID:
			default:
				printf("%s\n", BIOC_SDINVALID_S);
			}
			printf("\t\tvolume id: %d\n", bd.volid);
			printf("\t\tsize     : %lld\n", bd.size);
			printf("\t\tvendor   : %s\n", bd.vendor);
		}
		printf("\n");
	}
}

void
bio_alarm(char *arg)
{
	int rv;
	bioc_alarm ba;

	if (debug)
		printf("alarm in: %s, ", arg);

	ba.cookie = bl.cookie;

	switch (arg[0]) {
	case 'q': /* silence alarm */
		/* FALLTHROUGH */
	case 's':
		if (debug)
			printf("silence\n");
		ba.opcode = BIOC_SASILENCE;
		break;

	case 'e': /* enable alarm */
		if (debug)
			printf("enable\n");
		ba.opcode = BIOC_SAENABLE;
		break;

	case 'd': /* disable alarm */
		if (debug)
			printf("disable\n");
		ba.opcode = BIOC_SADISABLE;
		break;

	case 't': /* test alarm */
		if (debug)
			printf("test\n");
		ba.opcode = BIOC_SATEST;
		break;

	case 'g': /* get alarm state */
		if (debug)
			printf("get state\n");
		ba.opcode = BIOC_GASTATUS;
		break;

	default:
		warnx("invalid alarm function: %s", arg);
		return;
	}

	rv = ioctl(devh, BIOCALARM, &ba);
	if (rv == -1) {
		warnx("bioc_ioctl() call failed");
		return;
	}

	if (arg[0] == 'g') {
		printf("alarm is currently %s\n",
		    ba.status ? "enabled" : "disabled");
	}
}
