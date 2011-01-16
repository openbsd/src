/*	$OpenBSD: usbdevs.c,v 1.20 2011/01/16 00:04:47 jakemsr Exp $	*/
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

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <fcntl.h>
#include <unistd.h>
#include <err.h>
#include <errno.h>
#include <dev/usb/usb.h>

#define USBDEV "/dev/usb"

int verbose = 0;
int showdevs = 0;

void usage(void);
void usbdev(int f, int a, int rec);
int getdevicedesc(int, int, usb_device_descriptor_t *);
void usbdump(int f);
void dumpone(char *name, int f, int addr);
int main(int, char **);

extern char *__progname;

void
usage(void)
{
	fprintf(stderr, "usage: %s [-dv] [-a addr] [-f dev]\n", __progname);
	exit(1);
}

char done[USB_MAX_DEVICES];
int indent;

void
usbdev(int f, int a, int rec)
{
	struct usb_device_info di;
	int e, p, i;

	di.udi_addr = a;
	e = ioctl(f, USB_DEVICEINFO, &di);
	if (e) {
		if (errno != ENXIO)
			printf("addr %d: I/O error\n", a);
		return;
	}

	printf("addr %d: ", a);
	done[a] = 1;
	if (verbose) {
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
	}
	if (verbose) {
		printf("%s(0x%04x), %s(0x%04x), rev %s",
		    di.udi_product, di.udi_productNo,
		    di.udi_vendor, di.udi_vendorNo, di.udi_release);
		if (strlen(di.udi_serial))
			printf(", iSerialNumber %s", di.udi_serial);
	} else
		printf("%s, %s", di.udi_product, di.udi_vendor);
	printf("\n");
	if (showdevs) {
		for (i = 0; i < USB_MAX_DEVNAMES; i++)
			if (di.udi_devnames[i][0])
				printf("%*s  %s\n", indent, "",
				    di.udi_devnames[i]);
	}
	if (!rec)
		return;
	for (p = 0; p < di.udi_nports; p++) {
		int s = di.udi_ports[p];

		if (s >= USB_MAX_DEVICES) {
			if (verbose) {
				printf("%*sport %d %s\n", indent+1, "", p+1,
				    s == USB_PORT_ENABLED ? "enabled" :
				    s == USB_PORT_SUSPENDED ? "suspended" :
				    s == USB_PORT_POWERED ? "powered" :
				    s == USB_PORT_DISABLED ? "disabled" :
				    "???");
			}
			continue;
		}
		indent++;
		printf("%*s", indent, "");
		if (verbose)
			printf("port %d ", p+1);
		if (s == 0)
			printf("addr 0 should never happen!\n");
		else
			usbdev(f, s, 1);
		indent--;
	}
}

int
getdevicedesc(int f, int addr, usb_device_descriptor_t *d)
{
	struct usb_ctl_request req;
	int r;

	req.ucr_addr = addr;
	req.ucr_request.bmRequestType = UT_READ_DEVICE;
	req.ucr_request.bRequest = UR_GET_DESCRIPTOR;
	USETW2(req.ucr_request.wValue, UDESC_DEVICE, 0);
	USETW(req.ucr_request.wIndex, 0);
	USETW(req.ucr_request.wLength, USB_DEVICE_DESCRIPTOR_SIZE);
	req.ucr_data = d;
	req.ucr_flags = 0;
	if ((r = ioctl(f, USB_REQUEST, &req)) == -1)
		perror("getdevicedesc: ioctl");
	return (r != -1);
}

void
usbdump(int f)
{
	int a;

	for (a = 1; a < USB_MAX_DEVICES; a++) {
		if (!done[a])
			usbdev(f, a, 1);
	}
}

void
dumpone(char *name, int f, int addr)
{
	if (verbose)
		printf("Controller %s:\n", name);
	indent = 0;
	memset(done, 0, sizeof done);
	if (addr)
		usbdev(f, addr, 0);
	else
		usbdump(f);
}

int
main(int argc, char **argv)
{
	int ch, i, f;
	char buf[50];
	char *dev = 0;
	const char *errstr;
	int addr = 0;
	int ncont;

	while ((ch = getopt(argc, argv, "a:df:v?")) != -1) {
		switch (ch) {
		case 'a':
			addr = strtonum(optarg, 1, USB_MAX_DEVICES, &errstr);
			if (errstr)
				errx(1, "addr %s", errstr);
			break;
		case 'd':
			showdevs++;
			break;
		case 'f':
			dev = optarg;
			break;
		case 'v':
			verbose = 1;
			break;
		default:
			usage();
		}
	}
	argc -= optind;
	argv += optind;

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
		if (f >= 0)
			dumpone(dev, f, addr);
		else
			err(1, "%s", dev);
	}
	exit(0);
}
