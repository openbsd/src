/*	$Id: ipwcontrol.c,v 1.5 2004/10/27 21:37:40 damien Exp $	*/

/*-
 * Copyright (c) 2004
 *	Damien Bergamini <damien.bergamini@free.fr>. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice unmodified, this list of conditions, and the following
 *    disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#ifndef lint
static char rcsid[] = "$Id: ipwcontrol.c,v 1.5 2004/10/27 21:37:40 damien Exp $";
#endif /* not lint */

#include <sys/types.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <sys/socket.h>
#include <sys/stat.h>

#include <net/if.h>

#include <err.h>
#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sysexits.h>
#include <unistd.h>

#define SIOCSLOADFW	 _IOW('i', 137, struct ifreq)
#define SIOCSKILLFW	 _IOW('i', 138, struct ifreq)
#define SIOCGRADIO	_IOWR('i', 139, struct ifreq)
#define SIOCGTABLE1	_IOWR('i', 140, struct ifreq)

extern char *optarg;
extern int optind;

static void usage(void);
static int do_req(char *, unsigned long, void *);
static void load_firmware(char *, char *);
static void kill_firmware(char *);
static void get_radio_state(char *);
static void get_statistics(char *);

int
main(int argc, char **argv)
{
	int ch, ifspecified = 0;
	char *iface;

	iface = "ipw0";
	if (argc > 1 && argv[1][0] != '-') {
		iface = argv[1];
		ifspecified = 1;
		optind = 2;
	}

	while ((ch = getopt(argc, argv, "hf:i:kr")) != -1) {
		switch (ch) {
		case 'i':
			if (!ifspecified)
				iface = optarg;
			break;

		case 'f':
			load_firmware(iface, optarg);
			return EX_OK;

		case 'k':
			kill_firmware(iface);
			return EX_OK;

		case 'r':
			get_radio_state(iface);
			return EX_OK;

		case 'h':
		default:
			usage();
		}
	}

	get_statistics(iface);

	return EX_OK;
}

static void
usage(void)
{
	extern char *__progname;

	fprintf(stderr,
	    "usage: %s [interface] [-f firmware] [-kr]\n",
	    __progname);

	exit(EX_USAGE);
}

static int
do_req(char *iface, unsigned long req, void *data)
{
	int s;
	struct ifreq ifr;
	int error;

	if ((s = socket(AF_INET, SOCK_DGRAM, 0)) == -1)
		err(EX_OSERR, "Can't create socket");

	memset(&ifr, 0, sizeof(ifr));
	strncpy(ifr.ifr_name, iface, sizeof(ifr.ifr_name));
	ifr.ifr_data = data;
	error = ioctl(s, req, &ifr);

	(void)close(s);

	return error;
}

static void
load_firmware(char *iface, char *firmware)
{
	int fd;
	struct stat st;
	void *map;

	if ((fd = open(firmware, O_RDONLY)) == -1)
		err(EX_OSERR, "%s", firmware);

	if (fstat(fd, &st) == -1)
		err(EX_OSERR, "Unable to stat %s", firmware);

	if ((map = mmap(NULL, st.st_size, PROT_READ, 0, fd, 0)) == NULL)
		err(EX_OSERR, "Can't map %s into memory", firmware);

	if (do_req(iface, SIOCSLOADFW, map) == -1)
		err(EX_OSERR, "Can't load %s to driver", firmware);

	(void)munmap(map, st.st_size);
	(void)close(fd);
}

static void
kill_firmware(char *iface)
{
	if (do_req(iface, SIOCSKILLFW, NULL) == -1)
		err(EX_OSERR, "Can't kill firmware");
}

static void
get_radio_state(char *iface)
{
	int radio;

	if (do_req(iface, SIOCGRADIO, &radio) == -1)
		err(EX_OSERR, "Can't retrieve radio transmitter state");

	(void)printf("Radio is %s\n", radio ? "ON" : "OFF");
}

struct statistic {
	int index;
	const char *desc;
	int unit;
#define INT		1
#define HEX		2
#define MASK		HEX
#define PERCENTAGE	3
#define BOOL		4
};

/*- 
 * TIM  = Traffic Information Message
 * DTIM = Delivery TIM
 * ATIM = Announcement TIM
 * PSP  = Power Save Poll
 * RTS  = Request To Send
 * CTS  = Clear To Send
 * RSSI = Received Signal Strength Indicator
 */

static const struct statistic tbl[] = {
	{ 1, "Number of frames submitted for transfer", INT },
	{ 2, "Number of frames transmitted", INT },
	{ 3, "Number of unicast frames transmitted", INT },
	{ 4, "Number of unicast frames transmitted at 1Mb/s", INT },
	{ 5, "Number of unicast frames transmitted at 2Mb/s", INT },
	{ 6, "Number of unicast frames transmitted at 5.5Mb/s", INT },
	{ 7, "Number of unicast frames transmitted at 11Mb/s", INT },

	{ 13, "Number of multicast frames transmitted at 1Mb/s", INT },
	{ 14, "Number of multicast frames transmitted at 2Mb/s", INT },
	{ 15, "Number of multicast frames transmitted at 5.5Mb/s", INT },
	{ 16, "Number of multicast frames transmitted at 11Mb/s", INT },

	{ 21, "Number of null frames transmitted", INT },
	{ 22, "Number of RTS frames transmitted", INT },
	{ 23, "Number of CTS frames transmitted", INT },
	{ 24, "Number of ACK frames transmitted", INT },
	{ 25, "Number of association requests transmitted", INT },
	{ 26, "Number of association responses transmitted", INT },
	{ 27, "Number of reassociation requests transmitted", INT },
	{ 28, "Number of reassociation responses transmitted", INT },
	{ 29, "Number of probe requests transmitted", INT },
	{ 30, "Number of probe reponses transmitted", INT },
	{ 31, "Number of beacons transmitted", INT },
	{ 32, "Number of ATIM frames transmitted", INT },
	{ 33, "Number of disassociation requests transmitted", INT },
	{ 34, "Number of authentification requests transmitted", INT },
	{ 35, "Number of deauthentification requests transmitted", INT },

	{ 41, "Number of bytes transmitted", INT },
	{ 42, "Number of transmission retries", INT },
	{ 43, "Number of transmission retries at 1Mb/s", INT },
	{ 44, "Number of transmission retries at 2Mb/s", INT },
	{ 45, "Number of transmission retries at 5.5Mb/s", INT },
	{ 46, "Number of transmission retries at 11Mb/s", INT },

	{ 51, "Number of transmission failures", INT },

	{ 54, "Number of transmission aborted due to DMA", INT },

	{ 56, "Number of disassociation failures", INT },

	{ 58, "Number of spanning tree frames transmitted", INT },
	{ 59, "Number of transmission errors due to missing ACK", INT },

	{ 61, "Number of frames received", INT },
	{ 62, "Number of unicast frames received", INT },
	{ 63, "Number of unicast frames received at 1Mb/s", INT },
	{ 64, "Number of unicast frames received at 2Mb/s", INT },
	{ 65, "Number of unicast frames received at 5.5Mb/s", INT },
	{ 66, "Number of unicast frames received at 11Mb/s", INT },

	{ 71, "Number of multicast frames received", INT },
	{ 72, "Number of multicast frames received at 1Mb/s", INT },
	{ 73, "Number of multicast frames received at 2Mb/s", INT },
	{ 74, "Number of multicast frames received at 5.5Mb/s", INT },
	{ 75, "Number of multicast frames received at 11Mb/s", INT },

	{ 80, "Number of null frames received", INT },
	{ 81, "Number of poll frames received", INT },
	{ 82, "Number of RTS frames received", INT },
	{ 83, "Number of CTS frames received", INT },
	{ 84, "Number of ACK frames received", INT },
	{ 85, "Number of CF-End frames received", INT },
	{ 86, "Number of CF-End + CF-Ack frames received", INT },
	{ 87, "Number of association requests received", INT },
	{ 88, "Number of association responses received", INT },
	{ 89, "Number of reassociation requests received", INT },
	{ 90, "Number of reassociation responses received", INT },
	{ 91, "Number of probe requests received", INT },
	{ 92, "Number of probe reponses received", INT },
	{ 93, "Number of beacons received", INT },
	{ 94, "Number of ATIM frames received", INT },
	{ 95, "Number of disassociation requests received", INT },
	{ 96, "Number of authentification requests received", INT },
	{ 97, "Number of deauthentification requests received", INT },

	{ 101, "Number of bytes received", INT },
	{ 102, "Number of frames with a bad CRC received", INT },
	{ 103, "Number of frames with a bad CRC received at 1Mb/s", INT },
	{ 104, "Number of frames with a bad CRC received at 2Mb/s", INT },
	{ 105, "Number of frames with a bad CRC received at 5.5Mb/s", INT },
	{ 106, "Number of frames with a bad CRC received at 11Mb/s", INT },

	{ 112, "Number of duplicated frames received at 1Mb/s", INT },
	{ 113, "Number of duplicated frames received at 2Mb/s", INT },
	{ 114, "Number of duplicated frames received at 5.5Mb/s", INT },
	{ 115, "Number of duplicated frames received at 11Mb/s", INT },

	{ 119, "Number of duplicated frames received", INT },

	{ 123, "Number of frames with a bad protocol received", INT },
	{ 124, "Boot time", INT },
	{ 125, "Number of frames dropped due to missing buffer", INT },
	{ 126, "Number of frames dropped due to DMA", INT },

	{ 128, "Number of frames dropped due to missing fragment", INT },
	{ 129, "Number of frames dropped due to non-seq fragment", INT },
	{ 130, "Number of frames dropped due to missing first frame", INT },
	{ 131, "Number of frames dropped due to uncompleted frame", INT },

	{ 137, "Number of times adapter suspended", INT },
	{ 138, "Beacon timeout", INT },
	{ 139, "Number of poll response timeouts", INT },

	{ 141, "Number of PSP DTIM frames received", INT },
	{ 142, "Number of PSP TIM frames received", INT },
	{ 143, "PSP station Id", INT },

	{ 147, "RTC time of last association", INT },
	{ 148, "Percentage of missed beacons", PERCENTAGE },
	{ 149, "Percentage of missed transmission retries", PERCENTAGE },

	{ 151, "Number of access points in access points table", INT },

	{ 153, "Number of associations", INT },
	{ 154, "Number of association failures", INT },
	{ 156, "Number of full scans", INT },
	{ 157, "Card disabled", BOOL },

	{ 160, "RSSI at time of association", INT },
	{ 161, "Number of reassociations due to no probe response", INT },
	{ 162, "Number of reassociations due to poor line quality", INT },
	{ 163, "Number of reassociations due to load", INT },
	{ 164, "Number of reassociations due to access point RSSI level", INT },
	{ 165, "Number of reassociations due to load leveling", INT },

	{ 170, "Number of times authentification failed", INT },
	{ 171, "Number of times authentification response failed", INT },
	{ 172, "Number of entries in association table", INT },
	{ 173, "Average RSSI", INT },

	{ 176, "Self test status", INT },
	{ 177, "Power mode", INT },
	{ 178, "Power index", INT },
	{ 179, "IEEE country code", HEX },
	{ 180, "Channels supported for this country", MASK },
	{ 181, "Number of adapter warm resets", INT },
	{ 182, "Beacon interval", INT },

	{ 184, "Princeton version", INT },
	{ 185, "Antenna diversity disabled", BOOL },
	{ 186, "CCA RSSI", INT },
	{ 187, "Number of times EEPROM updated", INT },
	{ 188, "Beacon intervals between DTIM", INT },
	{ 189, "Current channel", INT },
	{ 190, "RTC time", INT },
	{ 191, "Operating mode", INT },
	{ 192, "Transmission rate", HEX },
	{ 193, "Supported transmission rates", MASK },
	{ 194, "ATIM window", INT },
	{ 195, "Supported basic transmission rates", MASK },
	{ 196, "Adapter highest rate", HEX },
	{ 197, "Access point highest rate", HEX },
	{ 198, "Management frame capability", BOOL },
	{ 199, "Type of authentification", INT },
	{ 200, "Adapter card platform type", INT },
	{ 201, "RTS threshold", INT },
	{ 202, "International mode", BOOL },
	{ 203, "Fragmentation threshold", INT },

	{ 213, "Microcode version", INT },

	{ 0, NULL, 0 }
};

static void
get_statistics(char *iface)
{
	static unsigned long stats[256]; /* XXX */
	const struct statistic *stat;

	if (do_req(iface, SIOCGTABLE1, stats) == -1)
		err(EX_OSERR, "Can't retrieve statistics");

	for (stat = tbl; stat->index != 0; stat++) {
		(void)printf("%-60s[", stat->desc);
		switch (stat->unit) {
		case INT:
			(void)printf("%lu", stats[stat->index]);
			break;
		
		case BOOL:
			(void)printf(stats[stat->index] ? "true" : "false");
			break;

		case PERCENTAGE:
			(void)printf("%lu%%", stats[stat->index]);
			break;

		case HEX:
		default:
			(void)printf("0x%08lX", stats[stat->index]);
		}
		(void)printf("]\n");
	}
}
