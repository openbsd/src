/*	$OpenBSD: usbdevs.c,v 1.30 2019/01/07 14:22:40 mpi Exp $	*/
/*	$NetBSD: usbdevs.c,v 1.19 2002/02/21 00:34:31 christos Exp $	*/

/*
 * Copyright (c) 1998 The NetBSD Foundation, Inc.
 * All rights reserved.
 *
 * This code is derived from software contributed to The NetBSD Foundation
 * by Lennart Augustsson (augustss@netbsd.org).
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

#include <sys/types.h>
#include <dev/usb/usb.h>

#include <err.h>
#include <errno.h>
#include <fcntl.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#ifndef nitems
#define nitems(_a) (sizeof((_a)) / sizeof((_a)[0]))
#endif

#define MINIMUM(a, b) (((a) < (b)) ? (a) : (b))

#define USBDEV "/dev/usb"

int verbose = 0;

void usage(void);
void usbdev(int f, uint8_t);
void usbdump(int f);
void dumpone(char *name, int f, int addr);
int main(int, char **);

extern char *__progname;

void
usage(void)
{
	fprintf(stderr, "usage: %s [-v] [-a addr] [-d usbdev]\n", __progname);
	exit(1);
}

char done[USB_MAX_DEVICES];

void
usbdev(int f, uint8_t addr)
{
	struct usb_device_info di;
	int e, i, port, nports;
	uint16_t change, status;

	di.udi_addr = addr;
	e = ioctl(f, USB_DEVICEINFO, &di);
	if (e) {
		if (errno != ENXIO)
			printf("addr %d: I/O error\n", addr);
		return;
	}

	printf("addr %02u: ", addr);
	done[addr] = 1;
	printf("%04x:%04x %s, %s", di.udi_vendorNo, di.udi_productNo,
	    di.udi_vendor, di.udi_product);

	if (verbose) {
		printf("\n\t ");
		switch (di.udi_speed) {
		case USB_SPEED_LOW:
			printf("low speed, ");
			break;
		case USB_SPEED_FULL:
			printf("full speed, ");
			break;
		case USB_SPEED_HIGH:
			printf("high speed, ");
			break;
		case USB_SPEED_SUPER:
			printf("super speed, ");
			break;
		default:
			break;
		}

		if (di.udi_power)
			printf("power %d mA, ", di.udi_power);
		else
			printf("self powered, ");
		if (di.udi_config)
			printf("config %d, ", di.udi_config);
		else
			printf("unconfigured, ");

		printf("rev %s", di.udi_release);
		if (strlen(di.udi_serial))
			printf(", iSerial %s", di.udi_serial);
	}
	printf("\n");

	if (verbose) {
		for (i = 0; i < USB_MAX_DEVNAMES; i++)
			if (di.udi_devnames[i][0])
				printf("\t driver: %s\n", di.udi_devnames[i]);
	}

	if (verbose > 1) {
		nports = MINIMUM(di.udi_nports, nitems(di.udi_ports));
		for (port = 0; port < nports; port++) {
			status = di.udi_ports[port] & 0xffff;
			change = di.udi_ports[port] >> 16;
			printf("\t port %02u: %04x.%04x", port+1, change,
			    status);

			if (status & UPS_CURRENT_CONNECT_STATUS)
				printf(" connect");

			if (status & UPS_PORT_ENABLED)
				printf(" enabled");

			if (status & UPS_SUSPEND)
				printf(" supsend");

			if (status & UPS_OVERCURRENT_INDICATOR)
				printf(" overcurrent");

			if (di.udi_speed < USB_SPEED_SUPER) {
				if (status & UPS_PORT_L1)
					printf(" l1");

				if (status & UPS_PORT_POWER)
					printf(" power");
			} else {
				if (status & UPS_PORT_POWER_SS)
					printf(" power");

				switch (UPS_PORT_LS_GET(status)) {
				case UPS_PORT_LS_U0:
					printf(" U0");
					break;
				case UPS_PORT_LS_U1:
					printf(" U1");
					break;
				case UPS_PORT_LS_U2:
					printf(" U2");
					break;
				case UPS_PORT_LS_U3:
					printf(" U3");
					break;
				case UPS_PORT_LS_SS_DISABLED:
					printf(" SS.disabled");
					break;
				case UPS_PORT_LS_RX_DETECT:
					printf(" Rx.detect");
					break;
				case UPS_PORT_LS_SS_INACTIVE:
					printf(" ss.inactive");
					break;
				case UPS_PORT_LS_POLLING:
					printf(" polling");
					break;
				case UPS_PORT_LS_RECOVERY:
					printf(" recovery");
					break;
				case UPS_PORT_LS_HOT_RESET:
					printf(" hot.reset");
					break;
				case UPS_PORT_LS_COMP_MOD:
					printf(" comp.mod");
					break;
				case UPS_PORT_LS_LOOPBACK:
					printf(" loopback");
					break;
				}
			}

			printf("\n");
		}
	}
}

void
usbdump(int f)
{
	uint8_t addr;

	for (addr = 1; addr < USB_MAX_DEVICES; addr++) {
		if (!done[addr])
			usbdev(f, addr);
	}
}

void
dumpone(char *name, int f, int addr)
{
	if (!addr)
		printf("Controller %s:\n", name);
	memset(done, 0, sizeof done);
	if (addr)
		usbdev(f, addr);
	else
		usbdump(f);
}

int
main(int argc, char **argv)
{
	int ch, i, f;
	char buf[50];
	char *dev = NULL;
	const char *errstr;
	int addr = 0;
	int ncont;

	while ((ch = getopt(argc, argv, "a:d:v?")) != -1) {
		switch (ch) {
		case 'a':
			addr = strtonum(optarg, 1, USB_MAX_DEVICES, &errstr);
			if (errstr)
				errx(1, "addr %s", errstr);
			break;
		case 'd':
			dev = optarg;
			break;
		case 'v':
			verbose++;
			break;
		default:
			usage();
		}
	}
	argc -= optind;
	argv += optind;

	if (argc != 0)
		usage();

	if (dev == 0) {
		for (ncont = 0, i = 0; i < 10; i++) {
			snprintf(buf, sizeof buf, "%s%d", USBDEV, i);
			f = open(buf, O_RDONLY);
			if (f >= 0) {
				dumpone(buf, f, addr);
				close(f);
			} else {
				if (errno == ENOENT || errno == ENXIO)
					continue;
				warn("%s", buf);
			}
			ncont++;
		}
		if (verbose && ncont == 0)
			printf("%s: no USB controllers found\n",
			    __progname);
	} else {
		f = open(dev, O_RDONLY);
		if (f >= 0) {
			dumpone(dev, f, addr);
			close(f);
		} else
			err(1, "%s", dev);
	}

	return 0;
}
