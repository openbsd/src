/* $OpenBSD: bioctl.c,v 1.55 2007/05/28 21:54:26 marco Exp $       */

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
#include <sys/types.h>
#include <sys/stat.h>
#include <scsi/scsi_disk.h>
#include <scsi/scsi_all.h>
#include <dev/biovar.h>

#include <errno.h>
#include <err.h>
#include <fcntl.h>
#include <util.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <ctype.h>
#include <util.h>

struct locator {
	int		channel;
	int		target;
	int		lun;
};

void			usage(void);
const char 		*str2locator(const char *, struct locator *);
void			cleanup(void);
int			bio_parse_devlist(char *, dev_t *);

void			bio_inq(char *);
void			bio_alarm(char *);
void			bio_setstate(char *);
void			bio_setblink(char *, char *, int);
void			bio_blink(char *, int, int);
void			bio_createraid(u_int16_t, char *);
u_int32_t		bio_createflags(char *);

int			devh = -1;
int			human;
int			verbose;
u_int32_t		cflags = 0;

struct bio_locate	bl;

int
main(int argc, char *argv[])
{
	extern char		*optarg;
	u_int64_t		func = 0;
	/* u_int64_t subfunc = 0; */
	char			*bioc_dev = NULL, *sd_dev = NULL;
	char			*realname = NULL, *al_arg = NULL;
	char			*bl_arg = NULL, *dev_list = NULL;
	int			ch, rv, blink = 0;
	u_int16_t		cr_level = 0;

	if (argc < 2)
		usage();

	while ((ch = getopt(argc, argv, "b:C:c:l:u:H:ha:iv")) != -1) {
		switch (ch) {
		case 'a': /* alarm */
			func |= BIOC_ALARM;
			al_arg = optarg;
			break;
		case 'b': /* blink */
			func |= BIOC_BLINK;
			blink = BIOC_SBBLINK;
			bl_arg = optarg;
			break;
		case 'C': /* creation flags */
			cflags = bio_createflags(optarg);
			break;
		case 'c': /* create */
			func |= BIOC_CREATERAID;
			cr_level = atoi(optarg);
			break;
		case 'u': /* unblink */
			func |= BIOC_BLINK;
			blink = BIOC_SBUNBLINK;
			bl_arg = optarg;
			break;
		case 'H': /* set hotspare */
			func |= BIOC_SETSTATE;
			al_arg = optarg;
			break;
		case 'h':
			human = 1;
			break;
		case 'i': /* inquiry */
			func |= BIOC_INQ;
			break;
		case 'l': /* device list */
			func |= BIOC_DEVLIST;
			dev_list = optarg;
			break;
		case 'v':
			verbose = 1;
			break;
		default:
			usage();
			/* NOTREACHED */
		}
	}
	argc -= optind;
	argv += optind;

	if (argc != 1)
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
		devh = open("/dev/bio", O_RDWR);
		if (devh == -1)
			err(1, "Can't open %s", "/dev/bio");

		bl.bl_name = bioc_dev;
		rv = ioctl(devh, BIOCLOCATE, &bl);
		if (rv == -1)
			errx(1, "Can't locate %s device via %s",
			    bl.bl_name, "/dev/bio");
	} else if (sd_dev) {
		devh = opendev(sd_dev, O_RDWR, OPENDEV_PART, &realname);
		if (devh == -1)
			err(1, "Can't open %s", sd_dev);
	} else
		errx(1, "need -d or -f parameter");

	if (func & BIOC_INQ) {
		bio_inq(sd_dev);
	} else if (func == BIOC_ALARM) {
		bio_alarm(al_arg);
	} else if (func == BIOC_BLINK) {
		bio_setblink(sd_dev, bl_arg, blink);
	} else if (func == BIOC_SETSTATE) {
		bio_setstate(al_arg);
	} else if (func & BIOC_CREATERAID || func & BIOC_DEVLIST) {
		if (!(func & BIOC_CREATERAID))
			errx(1, "need -c parameter");
		if (!(func & BIOC_DEVLIST))
			errx(1, "need -l parameter");
		if (sd_dev)
			errx(1, "can't use sd device");
		bio_createraid(cr_level, dev_list);
	}

	return (0);
}

void
usage(void)
{
	extern char		*__progname;

	fprintf(stderr,
		"usage: %s [-hiv] [-a alarm-function] "
		"[-b channel:target[.lun]]\n"
		"\t[-c raidlevel] [-H channel:target[.lun]]\n"
		"\t[-l special[,special[,...]]] "
		"[-u channel:target[.lun]] device\n", __progname);
	
	exit(1);
}

const char *
str2locator(const char *string, struct locator *location)
{
	const char		*errstr;
	char			parse[80], *targ, *lun;

	strlcpy(parse, string, sizeof parse);
	targ = strchr(parse, ':');
	if (targ == NULL)
		return ("target not specified");
	*targ++ = '\0';

	lun = strchr(targ, '.');
	if (lun != NULL) {
		*lun++ = '\0';
		location->lun = strtonum(lun, 0, 256, &errstr);
		if (errstr)
			return (errstr);
	} else
		location->lun = 0;

	location->target = strtonum(targ, 0, 256, &errstr);
	if (errstr)
		return (errstr);
	location->channel = strtonum(parse, 0, 256, &errstr);
	if (errstr)
		return (errstr);
	return (NULL);
}

void
bio_inq(char *name)
{
	char 			*status, size[64], scsiname[16], volname[32];
	char			percent[10], seconds[20];
	int			rv, i, d, volheader, hotspare, unused;
	char			encname[16], serial[32];
	struct bioc_disk	bd;
	struct bioc_inq		bi;
	struct bioc_vol		bv;

	memset(&bi, 0, sizeof(bi));

	bi.bi_cookie = bl.bl_cookie;

	rv = ioctl(devh, BIOCINQ, &bi);
	if (rv == -1) {
		warn("BIOCINQ");
		return;
	}

	volheader = 0;
	for (i = 0; i < bi.bi_novol; i++) {
		memset(&bv, 0, sizeof(bv));
		bv.bv_cookie = bl.bl_cookie;
		bv.bv_volid = i;
		bv.bv_percent = -1;
		bv.bv_seconds = 0;

		rv = ioctl(devh, BIOCVOL, &bv);
		if (rv == -1) {
			warn("BIOCVOL");
			return;
		}

		if (name && strcmp(name, bv.bv_dev) != 0)
			continue;

		if (!volheader) {
			volheader = 1;
			printf("%-7s %-10s %14s %-8s\n",
			    "Volume", "Status", "Size", "Device");
		}

		percent[0] = '\0';
		seconds[0] = '\0';
		if (bv.bv_percent != -1)
			snprintf(percent, sizeof percent,
			    " %d%% done", bv.bv_percent);
		if (bv.bv_seconds)
			snprintf(seconds, sizeof seconds,
			    " %u seconds", bv.bv_seconds);
		switch (bv.bv_status) {
		case BIOC_SVONLINE:
			status = BIOC_SVONLINE_S;
			break;
		case BIOC_SVOFFLINE:
			status = BIOC_SVOFFLINE_S;
			break;
		case BIOC_SVDEGRADED:
			status = BIOC_SVDEGRADED_S;
			break;
		case BIOC_SVBUILDING:
			status = BIOC_SVBUILDING_S;
			break;
		case BIOC_SVREBUILD:
			status = BIOC_SVREBUILD_S;
			break;
		case BIOC_SVSCRUB:
			status = BIOC_SVSCRUB_S;
			break;
		case BIOC_SVINVALID:
		default:
			status = BIOC_SVINVALID_S;
		}

		snprintf(volname, sizeof volname, "%s %u",
		    bi.bi_dev, bv.bv_volid);

		unused = 0;
		hotspare = 0;
		if (bv.bv_level == -1 && bv.bv_nodisk == 1)
			hotspare = 1;
		else if (bv.bv_level == -2 && bv.bv_nodisk == 1)
			unused = 1;
		else {
			if (human)
				fmt_scaled(bv.bv_size, size);
			else
				snprintf(size, sizeof size, "%14llu",
				    bv.bv_size);
			printf("%7s %-10s %14s %-7s RAID%u%s%s\n",
			    volname, status, size, bv.bv_dev,
			    bv.bv_level, percent, seconds);
		}

		for (d = 0; d < bv.bv_nodisk; d++) {
			memset(&bd, 0, sizeof(bd));
			bd.bd_cookie = bl.bl_cookie;
			bd.bd_diskid = d;
			bd.bd_volid = i;

			rv = ioctl(devh, BIOCDISK, &bd);
			if (rv == -1) {
				warn("BIOCDISK");
				return;
			}

			switch (bd.bd_status) {
			case BIOC_SDONLINE:
				status = BIOC_SDONLINE_S;
				break;
			case BIOC_SDOFFLINE:
				status = BIOC_SDOFFLINE_S;
				break;
			case BIOC_SDFAILED:
				status = BIOC_SDFAILED_S;
				break;
			case BIOC_SDREBUILD:
				status = BIOC_SDREBUILD_S;
				break;
			case BIOC_SDHOTSPARE:
				status = BIOC_SDHOTSPARE_S;
				break;
			case BIOC_SDUNUSED:
				status = BIOC_SDUNUSED_S;
				break;
			case BIOC_SDSCRUB:
				status = BIOC_SDSCRUB_S;
				break;
			case BIOC_SDINVALID:
			default:
				status = BIOC_SDINVALID_S;
			}

			if (hotspare || unused)
				;	/* use volname from parent volume */
			else
				snprintf(volname, sizeof volname, "    %3u",
				    bd.bd_diskid);

			if (human)
				fmt_scaled(bd.bd_size, size);
			else
				snprintf(size, sizeof size, "%14llu",
				    bd.bd_size);
			snprintf(scsiname, sizeof scsiname,
			    "%u:%u.%u",
			    bd.bd_channel, bd.bd_target, bd.bd_lun);
			if (bd.bd_procdev[0])
				strlcpy(encname, bd.bd_procdev, sizeof encname);
			else
				strlcpy(encname, "noencl", sizeof encname);
			if (bd.bd_serial[0])
				strlcpy(serial, bd.bd_serial, sizeof serial);
			else
				strlcpy(serial, "unknown serial", sizeof serial);

			printf("%7s %-10s %14s %-7s %-6s <%s>\n",
			    volname, status, size, scsiname, encname,
			    bd.bd_vendor);
			if (verbose)
				printf("%7s %-10s %14s %-7s %-6s '%s'\n",
				    "", "", "", "", "", serial);
		}
	}
}

void
bio_alarm(char *arg)
{
	int			rv;
	struct bioc_alarm	ba;

	ba.ba_cookie = bl.bl_cookie;

	switch (arg[0]) {
	case 'q': /* silence alarm */
		/* FALLTHROUGH */
	case 's':
		ba.ba_opcode = BIOC_SASILENCE;
		break;

	case 'e': /* enable alarm */
		ba.ba_opcode = BIOC_SAENABLE;
		break;

	case 'd': /* disable alarm */
		ba.ba_opcode = BIOC_SADISABLE;
		break;

	case 't': /* test alarm */
		ba.ba_opcode = BIOC_SATEST;
		break;

	case 'g': /* get alarm state */
		ba.ba_opcode = BIOC_GASTATUS;
		break;

	default:
		warnx("invalid alarm function: %s", arg);
		return;
	}

	rv = ioctl(devh, BIOCALARM, &ba);
	if (rv == -1) {
		warn("BIOCALARM");
		return;
	}

	if (arg[0] == 'g') {
		printf("alarm is currently %s\n",
		    ba.ba_status ? "enabled" : "disabled");

	}
}

void
bio_setstate(char *arg)
{
	struct bioc_setstate	bs;
	struct locator		location;
	const char		*errstr;
	int			rv;

	errstr = str2locator(arg, &location);
	if (errstr)
		errx(1, "Target %s: %s", arg, errstr);

	bs.bs_cookie = bl.bl_cookie;
	bs.bs_status = BIOC_SSHOTSPARE;
	bs.bs_channel = location.channel;
	bs.bs_target = location.target;
	bs.bs_lun = location.lun;

	rv = ioctl(devh, BIOCSETSTATE, &bs);
	if (rv == -1) {
		warn("BIOCSETSTATE");
		return;
	}
}

void
bio_setblink(char *name, char *arg, int blink)
{
	struct locator		location;
	struct bioc_inq		bi;
	struct bioc_vol		bv;
	struct bioc_disk	bd;
	struct bioc_blink	bb;
	const char		*errstr;
	int			v, d, rv;

	errstr = str2locator(arg, &location);
	if (errstr)
		errx(1, "Target %s: %s", arg, errstr);

	/* try setting blink on the device directly */
	memset(&bb, 0, sizeof(bb));
	bb.bb_cookie = bl.bl_cookie;
	bb.bb_status = blink;
	bb.bb_target = location.target;
	bb.bb_channel = location.channel;
	rv = ioctl(devh, BIOCBLINK, &bb);
	if (rv == 0)
		return;

	/* if the blink didnt work, try to find something that will */

	memset(&bi, 0, sizeof(bi));
	bi.bi_cookie = bl.bl_cookie;
	rv = ioctl(devh, BIOCINQ, &bi);
	if (rv == -1) {
		warn("BIOCINQ");
		return;
	}

	for (v = 0; v < bi.bi_novol; v++) {
		memset(&bv, 0, sizeof(bv));
		bv.bv_cookie = bl.bl_cookie;
		bv.bv_volid = v;
		rv = ioctl(devh, BIOCVOL, &bv);
		if (rv == -1) {
			warn("BIOCVOL");
			return;
		}

		if (name && strcmp(name, bv.bv_dev) != 0)
			continue;

		for (d = 0; d < bv.bv_nodisk; d++) {
			memset(&bd, 0, sizeof(bd));
			bd.bd_cookie = bl.bl_cookie;
			bd.bd_volid = v;
			bd.bd_diskid = d;

			rv = ioctl(devh, BIOCDISK, &bd);
			if (rv == -1) {
				warn("BIOCDISK");
				return;
			}

			if (bd.bd_channel == location.channel &&
			    bd.bd_target == location.target &&
			    bd.bd_lun == location.lun) {
				if (bd.bd_procdev[0] != '\0') {
					bio_blink(bd.bd_procdev,
					    location.target, blink);
				} else
					warnx("Disk %s is not in an enclosure", arg);
				return;
			}
		}
	}

	warnx("Disk %s does not exist", arg);
	return;
}

void
bio_blink(char *enclosure, int target, int blinktype)
{
	int			bioh;
	struct bio_locate	bio;
	struct bioc_blink	blink;
	int			rv;

	bioh = open("/dev/bio", O_RDWR);
	if (bioh == -1)
		err(1, "Can't open %s", "/dev/bio");

	bio.bl_name = enclosure;
	rv = ioctl(bioh, BIOCLOCATE, &bio);
	if (rv == -1)
		errx(1, "Can't locate %s device via %s", enclosure, "/dev/bio");

	memset(&blink, 0, sizeof(blink));
	blink.bb_cookie = bio.bl_cookie;
	blink.bb_status = blinktype;
	blink.bb_target = target;

	rv = ioctl(bioh, BIOCBLINK, &blink);
	if (rv == -1)
		warn("BIOCBLINK");

	close(bioh);
}

void
bio_createraid(u_int16_t level, char *dev_list)
{
	struct bioc_createraid	create;
	int			rv, no_dev;
	dev_t			*dt;
	u_int16_t		min_disks = 0;

	if (!dev_list)
		errx(1, "no devices specified");

	dt = (dev_t *)malloc(BIOC_CRMAXLEN);
	if (!dt)
		err(1, "not enough memory for dev_t list");
	memset(dt, 0, BIOC_CRMAXLEN);

	no_dev = bio_parse_devlist(dev_list, dt);

	switch (level) {
	case 0:
		min_disks = 1;
		break;
	case 1:
		min_disks = 2;
		break;
	default:
		errx(1, "unsuported raid level");
	}

	memset(&create, 0, sizeof(create));
	create.bc_cookie = bl.bl_cookie;
	create.bc_level = level;
	create.bc_dev_list_len = no_dev * sizeof(dev_t);
	create.bc_dev_list = dt;
	create.bc_flags = BIOC_SCDEVT | cflags;

	rv = ioctl(devh, BIOCCREATERAID, &create);
	if (rv == -1) {
		warn("BIOCCREATERAID");
		goto done;
	}

done:
	free(dt);
}

int
bio_parse_devlist(char *lst, dev_t *dt)
{
	char			*s, *e;
	u_int32_t		sz = 0;
	int			no_dev = 0, i, x;
	struct stat		sb;
	char			devname[MAXPATHLEN];

	if (!lst)
		errx(1, "invalid device list");

	s = e = lst;
	/* make sure we have a valid device list like /dev/sdNa,/dev/sdNNa */
	while (*e != '\0') {
		if (*e == ',')
			s = e + 1;
		else if (*(e + 1) == '\0' || *(e + 1) == ',') {
			/* got one */
			sz = e - s + 1;
			strlcpy(devname, s, sz + 1);
			if (stat(devname, &sb) == -1)
				err(1, "could not stat %s", devname);
			dt[no_dev] = sb.st_rdev;
			no_dev++;
			if (no_dev > (BIOC_CRMAXLEN / sizeof(dev_t)))
				errx(1, "too many devices on device list");
		}
		e++;
	}

	for (i = 0; i < no_dev; i++)
		for (x = 0; x < no_dev; x++)
			if (dt[i] == dt[x] && x != i)
				errx(1, "duplicate device in list");

	return (no_dev);
}

u_int32_t
bio_createflags(char *lst)
{
	char			*s, *e, fs[32];
	u_int32_t		sz = 0;
	u_int32_t		flags = 0;

	if (!lst)
		errx(1, "invalid flags list");

	s = e = lst;
	/* make sure we have a valid flags list like force,noassemeble */
	while (*e != '\0') {
		if (*e == ',')
			s = e + 1;
		else if (*(e + 1) == '\0' || *(e + 1) == ',') {
			/* got one */
			sz = e - s + 1;
			switch (s[0]) {
			case 'f':
				flags |= BIOC_SCFORCE;
				break;
			case 'n':
				flags |= BIOC_SCNOAUTOASSEMBLE;
				break;
			default:
				strlcpy(fs, s, sz + 1);
				errx(1, "invalid flag %s", fs);
			}
		}
		e++;
	}

	return (flags);
}
