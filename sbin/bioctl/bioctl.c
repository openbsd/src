/* $OpenBSD: bioctl.c,v 1.7 2005/04/06 02:36:34 marco Exp $       */
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

#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#include <sys/ioctl.h>
#include <sys/param.h>
#include <sys/queue.h>
#include <scsi/scsi_disk.h>
#include <scsi/scsi_all.h>
#include <dev/biovar.h>

#include "bioctl.h"

/* globals */
const char *bio_device = "/dev/bio";

SLIST_HEAD(dev_list, dev);

/* RAID card device list */
struct dev_list devices = SLIST_HEAD_INITIALIZER(dev);
/* User provided device list*/
struct dev_list ul = SLIST_HEAD_INITIALIZER(dev);

int devh = -1;
int debug = 0;

struct bio_locate bl;

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
	char inq[INQSIZE];

	struct dev *delm;

	u_int64_t func = 0, subfunc = 0;
	u_int32_t devlist = 0;

	if (argc < 2)
		usage();

	atexit(cleanup);

	while ((ch = getopt(argc, argv, "a:b:Dd:ehl:pst:u:")) != -1) {
		switch (ch) {
		case 'a': /* alarm */
			func |= BIOC_ALARM;
			al_arg = optarg;
			break;
		case 'b': /* LED blink/unblink */
			func |= BIOC_BLINK;
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
			subfunc |= F_ENUM;
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

		if (func & BIOC_BLINK)
			if (bc.ioctls & BIOC_BLINK)
				SLIST_FOREACH(delm, &ul, next) {
					bio_blink(al_arg, delm->channel,
					    delm->target);
				}
			else
				warnx("blink is not supported.");

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
				if (subfunc & F_READCAP) {
					SLIST_FOREACH(delm, &ul, next) {
						bio_pt_readcap(delm->channel,
						    delm->target, F_NOISY);
					}
				}

				if (subfunc & F_INQUIRY) {
					SLIST_FOREACH(delm, &ul, next) {
						bio_pt_inquire(delm->channel,
						    delm->target, F_NOISY,
						    &inq[0]);
					}
				}

				if (subfunc & F_TUR) {
					SLIST_FOREACH(delm, &ul, next) {
						bio_pt_tur(delm->channel,
						    delm->target);
					}
				}

				if (subfunc & F_ENUM)
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

	fprintf(stderr, "usage: %s [-Dehpt] "
	    "[-a alarm function] "
	    "[-b blink function] "
	    "[-s get status] "
	    "[-t passthrough] "
	    "[-l device list] "
	    "[-u go/stop function ] "
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
		return (F_INQUIRY);

	case 'e': /* ENUMERATE, not a pass through hmmm */
		if (debug)
			printf("enumerate\n");
		return (F_ENUM);

	case 'r': /* READ CAPACITY */
		if (debug)
			printf("read cap\n");
		return (F_READCAP);

	case 't': /* TUR */
		if (debug)
			printf("TUR\n");
		return (F_TUR);

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
bio_blink_userland(u_int8_t opc, u_int8_t c, u_int8_t t)
{
	struct dev *delm;
	struct scsi_enc_ctrl_diag_page *cdp;

	u_int8_t rc[SESSIZE];

	/* FIXME if the raid controllers are clustered we might have more
	 * than one proc device. */

	bio_pt_enum();

	SLIST_FOREACH(delm, &devices, next) {
		if (delm->channel != c)
			continue;

		if (delm->type != T_PROCESSOR)
			continue;

		if (debug)
			printf("proc at channel: %d target: %2d\n",
			    delm->channel, delm->target);

		/* get ses page so that we can modify bits for blink */
		if (!get_ses_page(delm->channel, delm->target,
		    &rc[0], sizeof(rc))) {
			return;
		}

		cdp = (struct scsi_enc_ctrl_diag_page *)rc;

		cdp->elmts[0].common_ctrl = 0x80;
		switch (opc) {
		case BIOCSBLINK_ALERT:
			cdp->elmts[0].byte4 = SDECD_RQST_FAULT;
			break;

		case BIOCSBLINK_BLINK:
			cdp->elmts[0].byte3 = SDECD_RQST_IDENT;
			break;

		case BIOCSBLINK_UNBLINK:
			cdp->elmts[0].byte3 = 0x00;
			cdp->elmts[0].byte4 = 0x00;
			break;

		default:
			return;
		}

		if (!set_ses_page(delm->channel, delm->target,
		    &rc[0], sizeof(rc))) {
			return;
		}

		return; /* done */
	}
}

void
bio_blink(char * arg, u_int8_t c, u_int8_t t)
{
	int rv;
	bioc_blink bb;

	if (debug)
		printf("blink in: %s, ", arg);

	bb.cookie = bl.cookie;

	switch (arg[0]) {
	case 'a': /* blink amber or alert led */
		if (debug)
			printf("blink alert\n");
		bb.opcode = BIOCSBLINK_ALERT;
		break;

	case 'b': /* blink hdd */
		if (debug)
			printf("blink\n");
		bb.opcode = BIOCSBLINK_BLINK;
		break;

	case 'u': /* unblink hdd */
		if (debug)
			printf("unblink\n");
		bb.opcode = BIOCSBLINK_UNBLINK;
		break;

	default:
		warnx("invalid blink function: %s", arg);
		return;
	}

	rv = ioctl(devh, BIOCBLINK, &bb);
	if (rv == -1) {
		if (errno == EOPNOTSUPP) {
			/* operation is not supported in kernel, do it here */
			if (debug)
				printf("doing blink in userland\n");
			bio_blink_userland(bb.opcode, c, t);
		}
		else
			warnx("bioc_ioctl() call failed");
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
bio_pt_readcap(u_int8_t c, u_int8_t t, u_int8_t flags)
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
		else
			printf("channel: %d target: %2d READ CAPACITY failed "
			    "without sense data\n", c, t);

		return (0);
	}

	rc.maxlba = betoh32(rc.maxlba);
	rc.bsize = betoh32(rc.bsize);

	size = (u_int64_t)rc.maxlba * (u_int64_t)rc.bsize;

	if (debug)
		printf("\nREAD CAPACITY: %lu * %lu = %llu\n",
		    rc.maxlba, rc.bsize, size);

	if (flags & F_NOISY) {
		printf("channel: %d target: %2d READ CAPACITY  %llu", c, t,
		    size);
		print_cap(size);
		printf("\n");
	}

	return (size);
}


/* inquire device */
u_int32_t
bio_pt_inquire(u_int8_t c, u_int8_t t, u_int8_t flags, u_int8_t *inq)
{
	bioc_scsicmd bpt;
	int rv, i;

	memset(&bpt, 0, sizeof(bpt));
	bpt.cookie = bl.cookie;
	bpt.channel = c;
	bpt.target = t;
	bpt.cdblen = 6;
	bpt.cdb[0] = INQUIRY;
	bpt.cdb[4] = INQSIZE;   /* LENGTH  */
	bpt.data = inq;    /* set up return data pointer */
	bpt.datalen = INQSIZE;  /* minimum INQ size */
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
		else
			if (flags & F_NOISY)
				printf("device %d:%d did not respond to "
				    "INQUIRY command\n", c, t);

		return 0;
	}

	printf("channel: %u target: %2u ", c, t);
	print_inquiry(flags, inq, bpt.datalen);

	if (flags & F_NOISY)
		printf("\n");

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
	bpt.cdb[0] = TEST_UNIT_READY;
	bpt.direction = BIOC_DIRNONE;
	rv = ioctl(devh, BIOCSCSICMD, &bpt);
	if (rv == -1) {
		warnx("passthrough failed");
		return (0);
	}

	if (bpt.status) {
		if (bpt.sensebuf[0] == 0x70 || bpt.sensebuf[0] == 0x71)
			print_sense(&bpt.sensebuf[0], bpt.senselen);
		else
			printf("channel: %d target: %2d: TUR failed without "
			    "sense data\n", c, t);

		return (0);
	}

	printf("channel: %d target: %2d: TUR completed\n", c, t);

	return (1);
}


/* enumerate all disks */
void
bio_pt_enum(void)
{
	bioc_scsicmd bpt;
	u_int32_t c, t, i, d;
	int rv;
	u_int8_t inq[INQSIZE];

	struct dev *delm;

	d = 0;
	for (c = 0; c < 4 /* FIXME */; c++) {
		for (t = 0; t < 16 /* FIXME */; t++) {
			if (bio_pt_inquire(c, t, F_SILENCE, &inq[0])) {
				if (inq[0] & SID_QUAL)
					continue; /* invalid device */

				delm = malloc(sizeof(struct dev));
				if (delm == NULL)
					errx(1, "not enough memory");
				delm->id = d++;
				delm->target = t;
				delm->channel = c;
				delm->type = inq[0];
				if (delm->type == T_DIRECT) {
					/* FIXME check the return value */
					delm->capacity = bio_pt_readcap(
					    delm->channel, delm->target,
					    F_SILENCE);
					print_cap(delm->capacity);
				}
				printf("\n");

				SLIST_INSERT_HEAD(&devices, delm, next);
			}
		} /* for t */
	} /* for c */
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

void
print_inquiry(u_int8_t flags, u_int8_t *inq, u_int8_t inqlen)
{
	u_int8_t i;

	if (inqlen < INQSIZE) {
		/* INQUIRY shall return at least 36 bytes */
		printf("invalid INQUIRY buffer size\n");
		return;
	}

	if (SID_QUAL & inq[0]) {
		printf("invalid device\n");
		return;
	}

	switch (SID_TYPE & inq[0]) {
	case T_DIRECT:
		printf("disk ");
		break;

	case T_PROCESSOR:
		printf("proc ");
		break;

	default:
		printf("unsuported device type\n");
		return;
	}

	for (i = 0;  i < inqlen; i++) {
		if (i < 8) {
			if ((flags & F_NOISY) || debug)
				printf("%02x ", inq[i]);
		}
		else
			printf("%c", inq[i] < ' ' ? ' ' : inq[i]);
	}
}

void print_cap(u_int64_t cap)
{
	if (cap / S_TERA > 1) {
		printf(" %3llu TB", cap / S_TERA);
		return;
	}

	if (cap / S_GIGA > 1) {
		printf(" %3llu GB", cap / S_GIGA);
		return;
	}

	if (cap / S_MEGA > 1) {
		printf(" %3llu MB", cap / S_MEGA);
		return;
	}

	if (cap / S_KILO > 1) {
		printf(" %3llu MB", cap / S_KILO);
		return;
	}

	printf(" %llu B", cap);
}

#if 0
	/* in case we want to do SAFTE this is the format */
	/* SAF-TE */
	memset(&bpt, 0, sizeof(bpt));
	bpt.cookie = bl.cookie;
	bpt.channel = delm->channel;
	bpt.target = delm->target;
	bpt.cdblen = 10;
	bpt.cdb[0] = 0x3c; /* READ BUFFER */
	bpt.cdb[1] = 0x01; /* SAF-TE command */
	bpt.cdb[8] = sizeof(rc); /* LSB size, FIXME */
	bpt.data = &rc[0];    /* set up return data pointer */
	bpt.datalen = sizeof(rc);
	bpt.direction = BIOC_DIRIN;
	bpt.senselen = 32; /* silly since the kernel overrides it */
#endif

int
get_ses_page(u_int8_t c, u_int8_t t, u_int8_t *buf, u_int8_t buflen)
{
	bioc_scsicmd bpt;
	int rv;

	memset(&bpt, 0, sizeof(bpt));
	bpt.cookie = bl.cookie;
	bpt.channel = c;
	bpt.target = t;
	bpt.cdblen = 6;
	bpt.cdb[0] = RECEIVE_DIAGNOSTIC;
	/* FIXME add this cdb struct + #defines to scsi_all.h */
	bpt.cdb[1] = 0x01; /* set PCV bit for SES commands */
	bpt.cdb[2] = 0x02; /* SES page nr */
	bpt.cdb[4] = buflen;
	bpt.data = buf;    /* set up return data pointer */
	bpt.datalen = buflen;
	bpt.direction = BIOC_DIRIN;
	bpt.senselen = 32; /* silly since the kernel overrides it */

	rv = ioctl(devh, BIOCSCSICMD, &bpt);
	if (rv == -1) {
		warnx("RECEIVE_DIAGNOSTIC failed %x", bpt.status);
		return (0);
	}
	else if (bpt.status) {
		if (bpt.sensebuf[0] == 0x70 || bpt.sensebuf[0] == 0x71)
			print_sense(&bpt.sensebuf[0], bpt.senselen);
		else
			printf("channel: %d target: %2d RECEIVE_DIAGNOSTIC "
			    "failed without sense data\n", c, t);

		return (0);
	}

	if (debug) {
		/* abuse print sense a little */
		print_sense(buf, bpt.datalen);
	}

	return (1);
}

int
set_ses_page(u_int8_t c, u_int8_t t, u_int8_t *buf, u_int8_t buflen)
{
	bioc_scsicmd bpt;
	int rv;

	memset(&bpt, 0, sizeof(bpt));
	bpt.cookie = bl.cookie;
	bpt.channel = c;
	bpt.target = t;
	bpt.cdblen = 6;
	bpt.cdb[0] = SEND_DIAGNOSTIC;
	bpt.cdb[1] = SSD_PF;
	bpt.cdb[4] = buflen;
	bpt.data = buf;    /* set up return data pointer */
	/*
	buf[12] = 0x80;
	buf[14] = 0x00;
	buf[15] = 0x20;
	*/
	bpt.datalen = buflen;
	bpt.direction = BIOC_DIROUT;
	bpt.senselen = 32; /* silly since the kernel overrides it */

	rv = ioctl(devh, BIOCSCSICMD, &bpt);
	if (rv == -1) {
		warnx("SEND_DIAGNOSTIC failed %x", bpt.status);
		return (0);
	}
	else if (bpt.status) {
		if (bpt.sensebuf[0] == 0x70 || bpt.sensebuf[0] == 0x71)
			print_sense(&bpt.sensebuf[0], bpt.senselen);
		else
			printf("channel: %d target: %2d SEND_DIAGNOSTIC "
			    "failed without sense data\n", c, t);

		return (0);
	}

	if (debug) {
		/* abuse print sense a little */
		print_sense(buf, bpt.datalen);
	}

	return (1);
}
