/* $OpenBSD: bioctl.c,v 1.1 2005/03/29 22:04:21 marco Exp $       */
/*
 * Copyright (c) 2004 Marco Peereboom
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

#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#include <dev/biovar.h>

#define READCAP 0x01
#define ENUM    0x02
#define TUR     0x04
#define INQUIRY 0x08

struct read_cap {
	u_int32_t		maxlba;
	u_int32_t		bsize;
};

struct dev {
	SLIST_ENTRY(dev)	next;
	u_int16_t		id;
	u_int8_t		channel;
	u_int8_t		target;
	u_int64_t		capacity;
};

void		usage(void);
void		cleanup(void);
u_int64_t	parse_passthru(char *);
void		parse_devlist(char *);
void		print_sense(u_int8_t *, u_int8_t);

int		bio_get_capabilities(bioc_capabilities *);
void		bio_alarm(char *);
void		bio_ping(void);
void		bio_startstop(char *, u_int8_t, u_int8_t);
void		bio_status(void);
u_int64_t	bio_pt_readcap(u_int8_t, u_int8_t);
u_int32_t	bio_pt_inquire(u_int8_t, u_int8_t, u_int8_t *);
u_int32_t	bio_pt_tur(u_int8_t, u_int8_t);
void		bio_pt_enum(void);

SLIST_HEAD(dev_list, dev);

/* RAID card device list */
struct dev_list devices = SLIST_HEAD_INITIALIZER(dev);
/* User provided device list*/
struct dev_list ul = SLIST_HEAD_INITIALIZER(dev);

char *bio_device = "/dev/bio";

int devh = -1;
int debug = 0;

struct bio_locate bl;

#define PARSELIST (0x8000000000000000llu)
int
main(int argc, char *argv[])
{
	extern char *optarg;

	bioc_capabilities bc;
	bioc_alarm ba;

	int ch;
	int rv;
	unsigned char *pl;

	char *bioc_dev = NULL;
	char *al_arg = NULL; /* argument to alarm */
	char *ss_arg = NULL; /* argument to start/stop */
	char inq[36];

	struct dev *delm;

	u_int64_t func = 0, subfunc = 0;
	u_int32_t devlist = 0;

	if (argc < 2)
		usage();

	atexit(cleanup);

	while ((ch = getopt(argc, argv, "a:Dd:ehl:pst:u:")) != -1) {
		switch (ch) {
		case 'a': /* alarm */
			func |= BIOC_ALARM;
			al_arg = optarg;
			break;

		case 'D': /* enable debug */
			debug = 1;
			break;

		case 'd': /* device */
			bioc_dev = optarg;
			break;

		case 'e': /* enumerate */
			func |= BIOC_SCSICMD;
			subfunc |= ENUM;
			break;

		case 'l': /* device list, separated for now use one dev only*/
			func |= PARSELIST;
			pl = optarg;
			break;

		case 'p': /* ping */
			func |= BIOC_PING;
			break;

		case 's': /* status */
			func |= BIOC_STATUS;
			break;

		case 't':
			func |= BIOC_SCSICMD;
			subfunc |= parse_passthru(optarg);
			break;

		case 'u': /* start/stop */
			func |= BIOC_STARTSTOP;
			ss_arg = optarg;
			break;

		case 'h': /* help/usage */
			/* FALLTHROUGH */
		default:
			usage();
			/* NOTREACHED */
		}
	}

	devh = open(bio_device, O_RDWR);
	if (devh == -1)
		err(1, "Can't open %s", bio_device);

	bl.name = bioc_dev;
	rv = ioctl(devh, BIOCLOCATE, &bl);
	if (rv == -1)
		errx(1, "Can't locate %s device via %s", bl.name, bio_device);

	if (debug)
		warnx("cookie = %p", bl.cookie);

	if (func & PARSELIST)
		parse_devlist(pl);

	if (!bio_get_capabilities(&bc))
		warnx("could not retrieve capabilities.");
	else {
		/* we should have everything setup by now so get to work */
		if (func & BIOC_ALARM)
			if (bc.ioctls & BIOC_ALARM)
				bio_alarm(al_arg);
			else
				warnx("alarms are not supported.");

		if (func & BIOC_PING)
			if (bc.ioctls & BIOC_PING)
				bio_ping();
			else
				warnx("ping not supported.");

		if (func & BIOC_STATUS) {
			if (bc.ioctls & BIOC_STATUS)
				bio_status();
			else
				warnx("status function not supported.");
		}

		if (func & BIOC_STARTSTOP) {
			if (bc.ioctls & BIOC_STARTSTOP) {
				SLIST_FOREACH(delm, &ul, next) {
					bio_startstop(ss_arg,
					    delm->channel, delm->target);
				}
			} else
				warnx("start/stop unit not supported.");
		}

		if (func & BIOC_SCSICMD) {
			if (bc.ioctls & BIOC_SCSICMD) {
				if (subfunc & READCAP) {
					SLIST_FOREACH(delm, &ul, next) {
						bio_pt_readcap(delm->channel,
						    delm->target);
					}
				}

				if (subfunc & INQUIRY) {
					SLIST_FOREACH(delm, &ul, next) {
						bio_pt_inquire(delm->channel,
						    delm->target, &inq[0]);
					}
				}

				if (subfunc & TUR) {
					SLIST_FOREACH(delm, &ul, next) {
						bio_pt_tur(delm->channel,
						    delm->target);
					}
				}

				if (subfunc & ENUM)
					bio_pt_enum();
			} else
				warnx("passthrough not supported.");
		}
	}

	return (0);
}

void
usage(void)
{
	extern char *__progname;

	fprintf(stderr, "usage: %s [-Dehpt] [-a alarm function] [-s get status]"
	    "[-t passthrough] [-l device list] [-u go/stop function ] "
	    "-d raid device\n", __progname);

	exit(1);
}

void
cleanup(void)
{
	struct dev *delm;

	if (debug)
		printf("atexit\n");

	while (devices.slh_first != NULL) {
		delm = devices.slh_first;
		SLIST_REMOVE_HEAD(&devices, next);
		if (debug)
			printf("free device: %p\n", delm);
		free(delm);
	}

	while (ul.slh_first != NULL) {
		delm = ul.slh_first;
		SLIST_REMOVE_HEAD(&ul, next);
		if (debug)
			printf("free ul: %p\n", delm);
		free(delm);
	}

	if (devh != -1)
		close(devh);
}

u_int64_t
parse_passthru(char *f)
{
	if (debug)
		printf("get_subfunc: %s, ", f);

	switch (f[0]) {
	case 'i': /* INQUIRY */
		if (debug)
			printf("inquiry\n");
		return (INQUIRY);

	case 'e': /* ENUMERATE, not a pass through hmmm */
		if (debug)
			printf("enumerate\n");
		return (ENUM);

	case 'r': /* READ CAPACITY */
		if (debug)
			printf("read cap\n");
		return (READCAP);

	case 't': /* TUR */
		if (debug)
			printf("TUR\n");
		return (TUR);

	default:
		errx(1, "invalid pass through function");
	}
}

void
parse_devlist(char *dl)
{
	u_int8_t c , t, done = 0;
	char *es, *s;
	struct dev *delm;

	es = NULL;
	s = dl;

	if (debug)
		printf("parse: %s\n", dl);

	while (!done) {
		c = strtol(s, &es, 10);
		if (debug)
			printf("%p %p %u %c\n", s, es, c, es[0]);
		s = es;
		if (es[0] == ':') {
			s++;
			t = strtol(s, &es, 10);
			if (debug)
				printf("%p %p %u %c\n", s, es, t, es[0]);
			s = es;

			if (c > 4)
				errx(1, "invalid channel number");
			if (t > 16)
				errx(1, "invalid target number");

			delm = malloc(sizeof(struct dev));
			if (!delm)
				errx(1, "not enough memory");

			delm->target = t;
			delm->channel = c;
			SLIST_INSERT_HEAD(&ul, delm, next);
		}
		if (es[0] == ',') {
			s++;
			continue;
		}
		if (es[0] == '\0') {
			done = 1;
			continue;
		}
		done = 2;
	}

	if (done == 2) {
		/* boink */
		errx(1, "invalid device list.");
	}
}

int
bio_get_capabilities(bioc_capabilities *bc)
{
	int rv;

	bc->cookie = bl.cookie;
	rv = ioctl(devh, BIOCCAPABILITIES, bc);
	if (rv == -1) {
		warnx("Error calling bioc_ioctl() via bio_ioctl()");
		return 0;
	}

	if (debug) {
		printf("ioctls = %016llx\n", bc->ioctls);
		printf("raid_types = %08lx\n", bc->raid_types);
	}

	return (1);
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
		ba.opcode = BIOCSALARM_SILENCE;
		break;

	case 'e': /* enable alarm */
		if (debug)
			printf("enable\n");
		ba.opcode = BIOCSALARM_ENABLE;
		break;

	case 'd': /* disable alarm */
		if (debug)
			printf("disable\n");
		ba.opcode = BIOCSALARM_DISABLE;
		break;

	case 't': /* test alarm */
		if (debug)
			printf("test\n");
		ba.opcode = BIOCSALARM_TEST;
		break;

	case 'g': /* get alarm state */
		if (debug)
			printf("get state\n");
		ba.opcode = BIOCGALARM_STATE;
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
		    ba.state ? "enabled" : "disabled");
	}
}

void
bio_ping(void)
{
	int rv;
	bioc_ping bp;

	bp.cookie = bl.cookie;
	bp.x = 0;
	rv = ioctl(devh, BIOCPING, &bp);
	if (rv == -1) {
		warnx("Error calling bioc_ioctl() via bio_ioctl()");
		return;
	}

	printf("x after ioctl() = %i\n", bp.x);
}

void
bio_startstop(char *arg, u_int8_t c, u_int8_t t)
{
	int rv;
	bioc_startstop bs;

	if (debug)
		printf("startstop in: %s, ", arg);

	bs.cookie = bl.cookie;

	switch (arg[0]) {
	case 's': /* stop unit */
		if (debug)
			printf("stop\n");
		bs.opcode = BIOCSUNIT_STOP;
		break;

	case 'g': /* start or go unit */
		if (debug)
			printf("start\n");
		bs.opcode = BIOCSUNIT_START;
		break;

	default:
		warnx("invalid start/stop function: %s", arg);
		return;
	}

	bs.channel = c;
	bs.target = t;

	rv = ioctl(devh, BIOCSTARTSTOP, &bs);
	if (rv == -1) {
		warnx("bioc_ioctl() call failed");
		return;
	}

	if (debug)
		printf("startstop done\n");
}

/* get status, for now only do all */
void
bio_status(void)
{
	int rv;
	bioc_status bs;

	if (debug)
		printf("status()\n");

	bs.cookie = bl.cookie;
	bs.opcode = BIOCGSTAT_ALL;

	rv = ioctl(devh, BIOCSTATUS, &bs);
	if (rv == -1) {
		warnx("bioc_ioctl() call failed");
		return;
	}

	if (debug)
		printf("status done\n");
}

/* read capacity for disk c,t */
u_int64_t
bio_pt_readcap(u_int8_t c, u_int8_t t)
{
	bioc_scsicmd bpt;
	struct read_cap rc;
	int rv;
	u_int64_t size;

	memset(&bpt, 0, sizeof(bpt));
	bpt.cookie = bl.cookie;
	bpt.channel = c;
	bpt.target = t;
	bpt.cdblen = 10;
	bpt.cdb[0] = READ_CAPACITY;
	bpt.data = &rc;    /* set up return data pointer */
	bpt.datalen = sizeof(rc);
	bpt.direction = BIOC_DIRIN;
	bpt.senselen = 32; /* silly since the kernel overrides it */

	rv = ioctl(devh, BIOCSCSICMD, &bpt);
	if (rv == -1) {
		warnx("READ CAPACITY failed %x", bpt.status);
		return (0);
	}
	else if (bpt.status) {
		if (bpt.sensebuf[0] == 0x70 || bpt.sensebuf[0] == 0x71)
			print_sense(&bpt.sensebuf[0], bpt.senselen);
		return (0);
	}

	rc.maxlba = betoh32(rc.maxlba);
	rc.bsize = betoh32(rc.bsize);

	size = (u_int64_t)rc.maxlba * (u_int64_t)rc.bsize;

	if (debug)
		printf("READ CAPACITY: %lu * %lu = %llu\n",
		    rc.maxlba, rc.bsize, size);

	return (size);
}


/* inquire device */
u_int32_t
bio_pt_inquire(u_int8_t c, u_int8_t t, u_int8_t *inq)
{
	bioc_scsicmd bpt;
	int rv, i;

	memset(&bpt, 0, sizeof(bpt));
	bpt.cookie = bl.cookie;
	bpt.channel = c;
	bpt.target = t;
	bpt.cdblen = 6;
	bpt.cdb[0] = INQUIRY;
	bpt.cdb[4] = 36;   /* LENGTH  */
	bpt.data = inq;    /* set up return data pointer */
	bpt.datalen = 36;  /* minimum INQ size */
	bpt.direction = BIOC_DIRIN;
	bpt.senselen = 32; /* silly since the kernel overrides it */

	rv = ioctl(devh, BIOCSCSICMD, &bpt);
	if (rv == -1) {
		warnx("INQUIRY failed %x", bpt.status);
		return 0;
	}
	else if (bpt.status) {
		if (bpt.sensebuf[0] == 0x70 || bpt.sensebuf[0] == 0x71)
			print_sense(&bpt.sensebuf[0], bpt.senselen);

		return 0;
	}

	if (debug) {
		printf("INQUIRY: ");
		printf("c: %u t: %u INQUIRY:", c, t);
		for (i = 0;  i < bpt.datalen; i++) {
			if (i < 8)
				printf("%0x ", inq[i]);
			else
				printf("%c", inq[i] < ' ' ? ' ' : inq[i]);
		}
		printf("\n");
	}

	return 1;
}

/* TUR for disk c,t */
u_int32_t
bio_pt_tur(u_int8_t c, u_int8_t t)
{
	bioc_scsicmd bpt;
	int rv;

	if (debug)
		printf("tur\n");

	memset(&bpt, 0, sizeof(bpt));
	bpt.cookie = bl.cookie;
	bpt.channel = c;
	bpt.target = t;
	bpt.cdblen = 6;
	bpt.direction = BIOC_DIRNONE;
	rv = ioctl(devh, BIOCSCSICMD, &bpt);
	if (rv == -1) {
		warnx("passthrough failed");
		return (0);
	}

	if (bpt.status) {
		if (bpt.sensebuf[0] == 0x70 || bpt.sensebuf[0] == 0x71)
			print_sense(&bpt.sensebuf[0], bpt.senselen);

		return (0);
	}

	if (debug)
		printf("tur completed\n");

	return (1);
}


/* enumerate all disks */
void
bio_pt_enum(void)
{
	bioc_scsicmd bpt;
	u_int32_t c, t, i, d;
	int rv;
	unsigned char inq[36];

	struct dev *delm;

	d = 0;
	for (c = 0; c < 2 /* FIXME */; c++) {
		for (t = 0; t < 16 /* FIXME */; t++) {
			if (bio_pt_inquire(c, t, &inq[0])) {
				printf("disk %u: c: %u t: %u\n", d, c, t);
				delm = malloc(sizeof(struct dev));
				if (delm == NULL)
					errx(1, "not enough memory");
				delm->id = d++;
				delm->target = t;
				delm->channel = c;
				SLIST_INSERT_HEAD(&devices, delm, next);
			}
		}
	}

	/* silly to do this here instead of in the for loop */
	SLIST_FOREACH(delm, &devices, next) {
		/* FIXME check the return value */
		delm->capacity = bio_pt_readcap(delm->channel, delm->target);
		if (debug)
			printf("%p: %u %u %u %llu\n", delm, delm->id,
			    delm->channel, delm->target, delm->capacity);
	}
}

/* printf sense data */
void
print_sense(u_int8_t *sensebuf, u_int8_t sensebuflen)
{
	u_int8_t i;

	if (debug)
		printf("print_sense() %p, %u\n", sensebuf, sensebuflen);

	for (i = 0; i < sensebuflen; i++) {
		printf("%02x ", sensebuf[i]);
	}
	printf("\n");

	/* FIXME add some pretty decoding here */
}
