/*	$OpenBSD: gpioctl.c,v 1.8 2008/11/24 14:11:58 mbalmer Exp $	*/
/*
 * Copyright (c) 2004 Alexander Yurchenko <grange@openbsd.org>
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

/*
 * Program to control GPIO devices.
 */

#include <sys/types.h>
#include <sys/gpio.h>
#include <sys/ioctl.h>
#include <sys/limits.h>

#include <err.h>
#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#define _PATH_DEV_GPIO	"/dev/gpio0"

char *device = _PATH_DEV_GPIO;
int devfd = -1;
int quiet = 0;

void	getinfo(void);
void	pinread(int);
void	pinwrite(int, int);
void	pinctl(int, char *[], int);
void	devattach(char *, int, u_int32_t);
void	devdetach(char *);

__dead void usage(void);

const struct bitstr {
	unsigned int mask;
	const char *string;
} pinflags[] = {
	{ GPIO_PIN_INPUT, "in" },
	{ GPIO_PIN_OUTPUT, "out" },
	{ GPIO_PIN_INOUT, "inout" },
	{ GPIO_PIN_OPENDRAIN, "od" },
	{ GPIO_PIN_PUSHPULL, "pp" },
	{ GPIO_PIN_TRISTATE, "tri" },
	{ GPIO_PIN_PULLUP, "pu" },
	{ GPIO_PIN_PULLDOWN, "pd" },
	{ GPIO_PIN_INVIN, "iin" },
	{ GPIO_PIN_INVOUT, "iout" },
	{ 0, NULL },
};

int
main(int argc, char *argv[])
{
	int ch;
	const char *errstr;
	char *ga_dvname = NULL, *ep;
	int do_ctl = 0;
	int pin = 0, value = 0, attach = 0, detach = 0;
	u_int32_t ga_mask = 0, ga_offset = -1;
	long lval;

	while ((ch = getopt(argc, argv, "A:cd:D:m:o:q")) != -1)
		switch (ch) {
		case 'A':
			if (detach)
				errx(1, "-A and -D are mutual exclusive");
			ga_dvname = optarg;
			attach = 1;
			break;
		case 'c':
			do_ctl = 1;
			break;
		case 'd':
			device = optarg;
			break;
		case 'D':
			if (attach)
				errx(1, "-D and -A are mutual exclusive");
			ga_dvname = optarg;
			detach = 1;
			break;
		case 'm':
			lval = strtol(optarg, &ep, 0);
			if (*optarg == '\0' || *ep != '\0')
				errx(1, "invalid mask (not a number)");
			if ((errno == ERANGE && (lval == LONG_MAX
			    || lval == LONG_MIN)) || lval > UINT_MAX)
				errx(1, "mask out of range");
			ga_mask = lval;
			break;
		case 'o':
			ga_offset = strtonum(optarg, 0, INT_MAX, &errstr);
			if (errstr)
				errx(1, "offset is %s: %s", errstr, optarg);
			break;
		case 'q':
			quiet = 1;
			break;
		default:
			usage();
			/* NOTREACHED */
		}
	argc -= optind;
	argv += optind;

	if (argc > 0) {
		pin = strtonum(argv[0], 0, INT_MAX, &errstr);
		if (errstr)
			errx(1, "%s: invalid pin", argv[0]);
	}

	if ((devfd = open(device, O_RDWR)) == -1)
		err(1, "%s", device);

	if (attach) {
		if (ga_offset == -1 || ga_mask == 0)
			errx(1, "gpio attach needs an offset and a mask");
		devattach(ga_dvname, ga_offset, ga_mask);
	} else if (detach) {
		devdetach(ga_dvname);
	} else if (argc == 0 && !do_ctl) {
		getinfo();
	} else if (argc == 1) {
		if (do_ctl)
			pinctl(pin, NULL, 0);
		else
			pinread(pin);
	} else if (argc > 1) {
		if (do_ctl) {
			pinctl(pin, argv + 1, argc - 1);
		} else {
			value = strtonum(argv[1], INT_MIN, INT_MAX, &errstr);
			if (errstr)
				errx(1, "%s: invalid value", argv[1]);
			pinwrite(pin, value);
		}
	} else {
		usage();
		/* NOTREACHED */
	}

	return (0);
}

void
getinfo(void)
{
	struct gpio_info info;

	bzero(&info, sizeof(info));
	if (ioctl(devfd, GPIOINFO, &info) == -1)
		err(1, "GPIOINFO");

	if (quiet)
		return;

	printf("%s: %d pins\n", device, info.gpio_npins);
}

void
pinread(int pin)
{
	struct gpio_pin_op op;

	bzero(&op, sizeof(op));
	op.gp_pin = pin;
	if (ioctl(devfd, GPIOPINREAD, &op) == -1)
		err(1, "GPIOPINREAD");

	if (quiet)
		return;

	printf("pin %d: state %d\n", pin, op.gp_value);
}

void
pinwrite(int pin, int value)
{
	struct gpio_pin_op op;

	if (value < 0 || value > 2)
		errx(1, "%d: invalid value", value);

	bzero(&op, sizeof(op));
	op.gp_pin = pin;
	op.gp_value = (value == 0 ? GPIO_PIN_LOW : GPIO_PIN_HIGH);
	if (value < 2) {
		if (ioctl(devfd, GPIOPINWRITE, &op) == -1)
			err(1, "GPIOPINWRITE");
	} else {
		if (ioctl(devfd, GPIOPINTOGGLE, &op) == -1)
			err(1, "GPIOPINTOGGLE");
	}

	if (quiet)
		return;

	printf("pin %d: state %d -> %d\n", pin, op.gp_value,
	    (value < 2 ? value : 1 - op.gp_value));
}

void
pinctl(int pin, char *flags[], int nflags)
{
	struct gpio_pin_ctl ctl;
	int fl = 0;
	const struct bitstr *bs;
	int i;

	bzero(&ctl, sizeof(ctl));
	ctl.gp_pin = pin;
	if (flags != NULL) {
		for (i = 0; i < nflags; i++)
			for (bs = pinflags; bs->string != NULL; bs++)
				if (strcmp(flags[i], bs->string) == 0) {
					fl |= bs->mask;
					break;
				}
	}
	ctl.gp_flags = fl;
	if (ioctl(devfd, GPIOPINCTL, &ctl) == -1)
		err(1, "GPIOPINCTL");

	if (quiet)
		return;

	printf("pin %d: caps:", pin);
	for (bs = pinflags; bs->string != NULL; bs++)
		if (ctl.gp_caps & bs->mask)
			printf(" %s", bs->string);
	printf(", flags:");
	for (bs = pinflags; bs->string != NULL; bs++)
		if (ctl.gp_flags & bs->mask)
			printf(" %s", bs->string);
	if (fl > 0) {
		printf(" ->");
		for (bs = pinflags; bs->string != NULL; bs++)
			if (fl & bs->mask)
				printf(" %s", bs->string);
	}
	printf("\n");
}

void
devattach(char *dvname, int offset, u_int32_t mask)
{
	struct gpio_attach attach;

	bzero(&attach, sizeof(attach));
	strlcpy(attach.ga_dvname, dvname, sizeof(attach.ga_dvname));
	attach.ga_offset = offset;
	attach.ga_mask = mask;
	if (ioctl(devfd, GPIOATTACH, &attach) == -1)
		err(1, "GPIOATTACH");
}

void
devdetach(char *dvname)
{
	struct gpio_attach attach;

	bzero(&attach, sizeof(attach));
	strlcpy(attach.ga_dvname, dvname, sizeof(attach.ga_dvname));
	if (ioctl(devfd, GPIODETACH, &attach) == -1)
		err(1, "GPIODETACH");
}
void
usage(void)
{
	extern char *__progname;

	fprintf(stderr, "usage: %s [-q] [-d device] [pin] [0 | 1 | 2]\n",
	    __progname);
	fprintf(stderr, "       %s [-q] [-d device] -c pin [flags]\n",
	    __progname);
	fprintf(stderr, "       %s [-A device] -o offset -m mask\n",
	    __progname);
	fprintf(stderr, "       %s [-D device]\n", __progname);

	exit(1);
}
