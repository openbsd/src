/*	$OpenBSD: atactl.c,v 1.2 2000/02/02 02:52:05 deraadt Exp $	*/
/*	$NetBSD: atactl.c,v 1.4 1999/02/24 18:49:14 jwise Exp $	*/

/*-
 * Copyright (c) 1998 The NetBSD Foundation, Inc.
 * All rights reserved.
 *
 * This code is derived from software contributed to The NetBSD Foundation
 * by Ken Hornstein.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *	This product includes software developed by the NetBSD
 *	Foundation, Inc. and its contributors.
 * 4. Neither the name of The NetBSD Foundation nor the names of its
 *    contributors may be used to endorse or promote products derived
 *    from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE NETBSD FOUNDATION, INC. AND CONTRIBUTORS
 * ``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED
 * TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE FOUNDATION OR CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

/*
 * atactl(8) - a program to control ATA devices.
 */

#include <sys/param.h>
#include <sys/ioctl.h>
#include <err.h>
#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <util.h>

#include <dev/ata/atareg.h>
#include <dev/ic/wdcreg.h>
#include <sys/ataio.h>

struct command {
	const char *cmd_name;
	void (*cmd_func) __P((int, char *[]));
};

struct bitinfo {
	u_int bitmask;
	const char *string;
};

int	main __P((int, char *[]));
void	usage __P((void));
void	ata_command __P((struct atareq *));
void	print_bitinfo __P((const char *, u_int, struct bitinfo *));

int	fd;				/* file descriptor for device */
const	char *dvname;			/* device name */
char	dvname_store[MAXPATHLEN];	/* for opendisk(3) */
const	char *cmdname;			/* command user issued */

extern const char *__progname;		/* from crt0.o */

void	device_identify __P((int, char *[]));
void	device_setidle __P((int, char *[]));
void	device_idle __P((int, char *[]));
void	device_checkpower __P((int, char *[]));

struct command commands[] = {
	{ "identify",	device_identify },
	{ "setidle",	device_setidle },
	{ "setstandby",	device_setidle },
	{ "idle",	device_idle },
	{ "standby",	device_idle },
	{ "sleep",	device_idle },
	{ "checkpower",	device_checkpower },
	{ NULL,		NULL },
};

/*
 * Tables containing bitmasks used for error reporting and
 * device identification.
 */

struct bitinfo ata_caps[] = {
	{ ATA_CAP_STBY, "ATA standby timer values" },
	{ WDC_CAP_IORDY, "IORDY operation" },
	{ WDC_CAP_IORDY_DSBL, "IORDY disabling" },
	{ NULL, NULL },
};

struct bitinfo ata_vers[] = {
	{ WDC_VER_ATA1,	"ATA-1" },
	{ WDC_VER_ATA2,	"ATA-2" },
	{ WDC_VER_ATA3,	"ATA-3" },
	{ WDC_VER_ATA4,	"ATA-4" },
	{ NULL, NULL },
};

struct bitinfo ata_cmd_set1[] = {
	{ WDC_CMD1_NOP, "NOP command" },
	{ WDC_CMD1_RB, "READ BUFFER command" },
	{ WDC_CMD1_WB, "WRITE BUFFER command" },
	{ WDC_CMD1_HPA, "Host Protected Area feature set" },
	{ WDC_CMD1_DVRST, "DEVICE RESET command" },
	{ WDC_CMD1_SRV, "SERVICE interrupt" },
	{ WDC_CMD1_RLSE, "release interrupt" },
	{ WDC_CMD1_AHEAD, "look-ahead" },
	{ WDC_CMD1_CACHE, "write cache" },
	{ WDC_CMD1_PKT, "PACKET command feature set" },
	{ WDC_CMD1_PM, "Power Management feature set" },
	{ WDC_CMD1_REMOV, "Removable Media feature set" },
	{ WDC_CMD1_SEC, "Security Mode feature set" },
	{ WDC_CMD1_SMART, "SMART feature set" },
	{ NULL, NULL },
};

struct bitinfo ata_cmd_set2[] = {
	{ WDC_CMD2_RMSN, "Removable Media Status Notification feature set" },
	{ ATA_CMD2_APM, "Advanced Power Management feature set" },
	{ ATA_CMD2_CFA, "CFA feature set" },
	{ ATA_CMD2_RWQ, "READ/WRITE DMS QUEUED commands" },
	{ WDC_CMD2_DM, "DOWNLOAD MICROCODE command" },
	{ NULL, NULL },
};

int
main(argc, argv)
	int argc;
	char *argv[];
{
	int i;

	dvname = argv[1];
	if (argc == 2) {
		cmdname = "identify";
		argv += 2;
		argc -= 2;
	} else if (argc < 3) {
		usage();
	} else {
		/* Skip program name, get and skip device name and command. */

		cmdname = argv[2];
		argv += 3;
		argc -= 3;
	}

	/*
	 * Open the device
	 */
	fd = opendisk(dvname, O_RDWR, dvname_store, sizeof(dvname_store), 0);
	if (fd == -1) {
		if (errno == ENOENT) {
			/*
			 * Device doesn't exist.  Probably trying to open
			 * a device which doesn't use disk semantics for
			 * device name.  Try again, specifying "cooked",
			 * which leaves off the "r" in front of the device's
			 * name.
			 */
			fd = opendisk(dvname, O_RDWR, dvname_store,
			    sizeof(dvname_store), 1);
			if (fd == -1)
				err(1, "%s", dvname);
		} else
			err(1, "%s", dvname);
	}

	/*
	 * Point the dvname at the actual device name that opendisk() opened.
	 */
	dvname = dvname_store;

	/* Look up and call the command. */
	for (i = 0; commands[i].cmd_name != NULL; i++)
		if (strcmp(cmdname, commands[i].cmd_name) == 0)
			break;
	if (commands[i].cmd_name == NULL)
		errx(1, "unknown command: %s\n", cmdname);

	(*commands[i].cmd_func)(argc, argv);
	exit(0);
}

void
usage()
{

	fprintf(stderr, "usage: %s device command [arg [...]]\n",
	    __progname);
	exit(1);
}

/*
 * Wrapper that calls ATAIOCCOMMAND and checks for errors
 */

void
ata_command(req)
	struct atareq *req;
{
	int error;

	error = ioctl(fd, ATAIOCCOMMAND, req);

	if (error == -1)
		err(1, "ATAIOCCOMMAND failed");

	switch (req->retsts) {

	case ATACMD_OK:
		return;
	case ATACMD_TIMEOUT:
		fprintf(stderr, "ATA command timed out\n");
		exit(1);
	case ATACMD_DF:
		fprintf(stderr, "ATA device returned a Device Fault\n");
		exit(1);
	case ATACMD_ERROR:
		if (req->error & WDCE_ABRT)
			fprintf(stderr, "ATA device returned Aborted "
				"Command\n");
		else
			fprintf(stderr, "ATA device returned error register "
				"%0x\n", req->error);
		exit(1);
	default:
		fprintf(stderr, "ATAIOCCOMMAND returned unknown result code "
			"%d\n", req->retsts);
		exit(1);
	}
}

/*
 * Print out strings associated with particular bitmasks
 */

void
print_bitinfo(f, bits, binfo)
	const char *f;
	u_int bits;
	struct bitinfo *binfo;
{

	for (; binfo->bitmask != NULL; binfo++)
		if (bits & binfo->bitmask)
			printf(f, binfo->string);
}

/*
 * DEVICE COMMANDS
 */

/*
 * device_identify:
 *
 *	Display the identity of the device
 */
void
device_identify(argc, argv)
	int argc;
	char *argv[];
{
	struct ataparams *inqbuf;
	struct atareq req;
	unsigned char inbuf[DEV_BSIZE];
#if BYTE_ORDER == LITTLE_ENDIAN
	int i;
	u_int16_t *p;
#endif

	/* No arguments. */
	if (argc != 0)
		goto usage;

	memset(&inbuf, 0, sizeof(inbuf));
	memset(&req, 0, sizeof(req));

	inqbuf = (struct ataparams *) inbuf;

	req.flags = ATACMD_READ;
	req.command = WDCC_IDENTIFY;
	req.databuf = (caddr_t) inbuf;
	req.datalen = sizeof(inbuf);
	req.timeout = 1000;

	ata_command(&req);

#if BYTE_ORDER == LITTLE_ENDIAN
	/*
	 * On little endian machines, we need to shuffle the string
	 * byte order.  However, we don't have to do this for NEC or
	 * Mitsumi ATAPI devices
	 */

	if (!((inqbuf->atap_config & WDC_CFG_ATAPI_MASK) == WDC_CFG_ATAPI &&
	      ((inqbuf->atap_model[0] == 'N' &&
		  inqbuf->atap_model[1] == 'E') ||
	       (inqbuf->atap_model[0] == 'F' &&
		  inqbuf->atap_model[1] == 'X')))) {
		for (i = 0 ; i < sizeof(inqbuf->atap_model); i += 2) {
			p = (u_short *) (inqbuf->atap_model + i);
			*p = ntohs(*p);
		}
		for (i = 0 ; i < sizeof(inqbuf->atap_serial); i += 2) {
			p = (u_short *) (inqbuf->atap_serial + i);
			*p = ntohs(*p);
		}
		for (i = 0 ; i < sizeof(inqbuf->atap_revision); i += 2) {
			p = (u_short *) (inqbuf->atap_revision + i);
			*p = ntohs(*p);
		}
	}
#endif

	/*
	 * Strip blanks off of the info strings.  Yuck, I wish this was
	 * cleaner.
	 */

	if (inqbuf->atap_model[sizeof(inqbuf->atap_model) - 1] == ' ') {
		inqbuf->atap_model[sizeof(inqbuf->atap_model) - 1] = '\0';
		while (inqbuf->atap_model[strlen(inqbuf->atap_model) - 1] == ' ')
			inqbuf->atap_model[strlen(inqbuf->atap_model) - 1] = '\0';
	}

	if (inqbuf->atap_revision[sizeof(inqbuf->atap_revision) - 1] == ' ') {
		inqbuf->atap_revision[sizeof(inqbuf->atap_revision) - 1] = '\0';
		while (inqbuf->atap_revision[strlen(inqbuf->atap_revision) - 1] == ' ')
			inqbuf->atap_revision[strlen(inqbuf->atap_revision) - 1] = '\0';
	}

	if (inqbuf->atap_serial[sizeof(inqbuf->atap_serial) - 1] == ' ') {
		inqbuf->atap_serial[sizeof(inqbuf->atap_serial) - 1] = '\0';
		while (inqbuf->atap_serial[strlen(inqbuf->atap_serial) - 1] == ' ')
			inqbuf->atap_serial[strlen(inqbuf->atap_serial) - 1] = '\0';
	}

	printf("Model: %.*s, Rev: %.*s, Serial #: %.*s\n",
	       (int) sizeof(inqbuf->atap_model), inqbuf->atap_model,
	       (int) sizeof(inqbuf->atap_revision), inqbuf->atap_revision,
	       (int) sizeof(inqbuf->atap_serial), inqbuf->atap_serial);

	printf("Device type: %s, %s\n", inqbuf->atap_config & WDC_CFG_ATAPI ?
	       "ATAPI" : "ATA", inqbuf->atap_config & ATA_CFG_FIXED ? "fixed" :
	       "removable");

	if ((inqbuf->atap_config & WDC_CFG_ATAPI_MASK) == 0)
		printf("Cylinders: %d, heads: %d, sec/track: %d, total "
		       "sectors: %d\n", inqbuf->atap_cylinders,
		       inqbuf->atap_heads, inqbuf->atap_sectors,
		       (inqbuf->atap_capacity[1] << 16) |
		       inqbuf->atap_capacity[0]);

	if (inqbuf->atap_queuedepth & WDC_QUEUE_DEPTH_MASK)
		printf("Device supports command queue depth of %d\n",
		       inqbuf->atap_queuedepth & 0xf);

	printf("Device capabilities:\n");
	print_bitinfo("\t%s\n", inqbuf->atap_capabilities1, ata_caps);

	if (inqbuf->atap_ata_major != 0 && inqbuf->atap_ata_major != 0xffff) {
		printf("Device supports following standards:\n");
		print_bitinfo("%s ", inqbuf->atap_ata_major, ata_vers);
		printf("\n");
	}

	if (inqbuf->atap_cmd_set1 != 0 && inqbuf->atap_cmd_set1 != 0xffff &&
	    inqbuf->atap_cmd_set2 != 0 && inqbuf->atap_cmd_set2 != 0xffff) {
		printf("Command set support:\n");
		print_bitinfo("\t%s\n", inqbuf->atap_cmd_set1, ata_cmd_set1);
		print_bitinfo("\t%s\n", inqbuf->atap_cmd_set2, ata_cmd_set2);
	}

	if (inqbuf->atap_cmd_def != 0 && inqbuf->atap_cmd_def != 0xffff) {
		printf("Command sets/features enabled:\n");
		print_bitinfo("\t%s\n", inqbuf->atap_cmd_set1 &
			      (WDC_CMD1_SRV | WDC_CMD1_RLSE | WDC_CMD1_AHEAD |
			       WDC_CMD1_CACHE | WDC_CMD1_SEC | WDC_CMD1_SMART),
			       ata_cmd_set1);
		print_bitinfo("\t%s\n", inqbuf->atap_cmd_set2 &
			      (WDC_CMD2_RMSN | ATA_CMD2_APM), ata_cmd_set2);
	}

	return;

usage:
	fprintf(stderr, "usage: %s device %s\n", __progname, cmdname);
	exit(1);
}

/*
 * device idle:
 *
 * issue the IDLE IMMEDIATE command to the drive
 */

void
device_idle(argc, argv)
	int argc;
	char *argv[];
{
	struct atareq req;

	/* No arguments. */
	if (argc != 0)
		goto usage;

	memset(&req, 0, sizeof(req));

	if (strcmp(cmdname, "idle") == 0)
		req.command = WDCC_IDLE_IMMED;
	else if (strcmp(cmdname, "standby") == 0)
		req.command = WDCC_STANDBY_IMMED;
	else
		req.command = WDCC_SLEEP;

	req.timeout = 1000;

	ata_command(&req);

	return;
usage:
	fprintf(stderr, "usage: %s device %s\n", __progname, cmdname);
	exit(1);
}

/*
 * Set the idle timer on the disk.  Set it for either idle mode or
 * standby mode, depending on how we were invoked.
 */

void
device_setidle(argc, argv)
	int argc;
	char *argv[];
{
	unsigned long idle;
	struct atareq req;
	char *end;

	/* Only one argument */
	if (argc != 1)
		goto usage;

	idle = strtoul(argv[0], &end, 0);

	if (*end != '\0') {
		fprintf(stderr, "Invalid idle time: \"%s\"\n", argv[0]);
		exit(1);
	}

	if (idle > 19800) {
		fprintf(stderr, "Idle time has a maximum value of 5.5 "
			"hours\n");
		exit(1);
	}

	if (idle != 0 && idle < 5) {
		fprintf(stderr, "Idle timer must be at least 5 seconds\n");
		exit(1);
	}

	memset(&req, 0, sizeof(req));

	if (idle <= 240*5)
		req.sec_count = idle / 5;
	else
		req.sec_count = idle / (30*60) + 240;

	req.command = cmdname[3] == 's' ? WDCC_STANDBY : WDCC_IDLE;
	req.timeout = 1000;

	ata_command(&req);

	return;

usage:
	fprintf(stderr, "usage; %s device %s idle-time\n", __progname,
		cmdname);
	exit(1);
}

/*
 * Query the device for the current power mode
 */

void
device_checkpower(argc, argv)
	int argc;
	char *argv[];
{
	struct atareq req;

	/* No arguments. */
	if (argc != 0)
		goto usage;

	memset(&req, 0, sizeof(req));

	req.command = WDCC_CHECK_PWR;
	req.timeout = 1000;
	req.flags = ATACMD_READREG;

	ata_command(&req);

	printf("Current power status: ");

	switch (req.sec_count) {
	case 0x00:
		printf("Standby mode\n");
		break;
	case 0x80:
		printf("Idle mode\n");
		break;
	case 0xff:
		printf("Active mode\n");
		break;
	default:
		printf("Unknown power code (%02x)\n", req.sec_count);
	}

	return;
usage:
	fprintf(stderr, "usage: %s device %s\n", __progname, cmdname);
	exit(1);
}
