/*	$OpenBSD: radioctl.c,v 1.2 2001/10/04 22:43:45 gluk Exp $	*/
/* $RuOBSD: radioctl.c,v 1.1 2001/10/03 05:53:35 gluk Exp $ */

/*
 * Copyright (c) 2001 Vladimir Popov <jumbo@narod.ru>
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
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS;
 * OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
 * WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR
 * OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF
 * ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include <sys/ioctl.h>
#include <sys/radioio.h>

#include <err.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#define RADIO_ENV	"RADIODEVICE"
#define RADIODEVICE	"/dev/radio"

const char *varname[] = {
	"search",
#define OPTION_SEARCH		0x00
	"volume",
#define OPTION_VOLUME		0x01
	"frequency",
#define OPTION_FREQUENCY	0x02
	"mute",
#define OPTION_MUTE		0x03
	"reference",
#define OPTION_REFERENCE	0x04
	"mono",
#define OPTION_MONO		0x05
	"stereo",
#define	OPTION_STEREO		0x06
	"sensitivity"
#define	OPTION_SENSITIVITY	0x07
};

#define OPTION_NONE		~0u
#define VALUE_NONE		~0ul

extern char *__progname;
const char *onchar = "on";
#define ONCHAR_LEN	2
const char *offchar = "off";
#define OFFCHAR_LEN	3

u_long caps;

static void     usage(void);
static void     print_vars(int, int);
static void     write_param(int, char *, int);
static u_int    parse_option(const char *);
static u_long   get_value(int, u_int);
static void     set_value(int, u_int, u_long);
static u_long   read_value(char *, u_int);
static void     print_value(int, u_int);
static void     warn_unsupported(u_int);
static void     ext_print(int, u_int, int);

/*
 * Control behavior of a FM tuner - set frequency, volume etc
 */
int
main(int argc, char **argv)
{
	char *radiodev = NULL;
	char optchar;
	char *param = NULL;
	int rd = -1;
	int show_vars = 0;
	int set_param = 0;
	int silent = 0;

	if (argc < 2) {
		usage();
		exit(1);
	}

	radiodev = getenv(RADIO_ENV);
	if (radiodev == NULL)
		radiodev = RADIODEVICE;

	while ((optchar = getopt(argc, argv, "af:nw:")) != -1) {
		switch (optchar) {
		case 'a':
			show_vars = 1;
			optind = 1;
			break;
		case 'f':
			radiodev = optarg;
			optind = 2;
			break;
		case 'n':
			silent = 1;
			optind = 1;
			break;
		case 'w':
			set_param = 1;
			param = optarg;
			optind = 2;
			break;
		default:
			usage();
			/* NOTREACHED */
		}

		argc -= optind;
		argv += optind;
	}

	rd = open(radiodev, O_RDONLY);
	if (rd < 0)
		err(1, "%s open error", radiodev);

	if (ioctl(rd, RIOCGCAPS, &caps) < 0)
		err(1, "RIOCGCAPS");

	if (argc > 1)
		ext_print(rd, parse_option(*(argv + 1)), silent);

	if (set_param)
		write_param(rd, param, silent);

	if (show_vars)
		print_vars(rd, silent);

	if (close(rd) < 0)
		warn("%s close error", radiodev);

	return 0;
}

static void
usage(void)
{
	printf("Usage: %s [-f file] [-a] [-n] [-w name=value] [name]\n",
		__progname);
}

/*
 * Print all available parameters
 */
static void
print_vars(int fd, int silent)
{
	u_long var;

	ext_print(fd, OPTION_VOLUME, silent);
	ext_print(fd, OPTION_FREQUENCY, silent);
	ext_print(fd, OPTION_MUTE, silent);

	if (caps & RADIO_CAPS_REFERENCE_FREQ)
		ext_print(fd, OPTION_REFERENCE, silent);
	if (caps & RADIO_CAPS_LOCK_SENSITIVITY)
		ext_print(fd, OPTION_SENSITIVITY, silent);

	if (ioctl(fd, RIOCGINFO, &var) < 0)
		warn("RIOCGINFO");
	if (caps & RADIO_CAPS_DETECT_SIGNAL)
		if (!silent)
			printf("%s=", "signal");
		printf("%s\n", var & RADIO_INFO_SIGNAL ? onchar : offchar);
	if (caps & RADIO_CAPS_DETECT_STEREO) {
		if (!silent)
			printf("%s=", varname[OPTION_STEREO]);
		printf("%s\n", var & RADIO_INFO_STEREO ? onchar : offchar);
	}

	if (!silent)
		puts("card capabilities:");
	if (caps & RADIO_CAPS_SET_MONO)
		puts("\tmanageable mono/stereo");
	if (caps & RADIO_CAPS_HW_SEARCH)
		puts("\thardware search");
	if (caps & RADIO_CAPS_HW_AFC)
		puts("\thardware AFC");
}

/*
 * Set new value of a parameter
 */
static void
write_param(int fd, char *param, int silent)
{
	int paramlen = 0;
	int namelen = 0;
	char *topt = NULL;
	const char *badvalue = "bad value `%s'";
	u_int optval = OPTION_NONE;
	u_long var = VALUE_NONE;
	u_long addvar = VALUE_NONE;
	u_char sign = 0;

	if (param == NULL || *param == '\0')
		return;

	paramlen = strlen(param);
	namelen = strcspn(param, "=");
	if (namelen > paramlen - 2) {
		warnx(badvalue, param);
		return;
	}

	paramlen -= ++namelen;

	if ((topt = (char *)malloc(namelen)) == NULL) {
		warn("memory allocation error");
		return;
	}
	strlcpy(topt, param, namelen);
	optval = parse_option(topt);

	if (optval == OPTION_NONE) {
		free(topt);
		return;
	}

	if (!silent)
		printf("%s: ", topt);

	free(topt);

	topt = &param[namelen];
	switch (*topt) {
	case '+':
	case '-':
		if ((addvar = read_value(topt + 1, optval)) == VALUE_NONE)
			break;
		if ((var = get_value(fd, optval)) == VALUE_NONE)
			break;
		sign++;
		if (*topt == '+')
			var += addvar;
		else
			var -= addvar;
		break;
	case 'o':
		if (strncmp(topt, offchar,
			paramlen > OFFCHAR_LEN ? paramlen : OFFCHAR_LEN) == 0)
			var = 0;
		else
			if (strncmp(topt, onchar,
				paramlen > ONCHAR_LEN ? paramlen : ONCHAR_LEN) == 0)
				var = 1;
		break;
	case 'u':
		if (strncmp(topt, "up", paramlen > 2 ? paramlen : 2) == 0)
			var = 1;
		break;
	case 'd':
		if (strncmp(topt, "down", paramlen > 4 ? paramlen : 4) == 0)
			var = 0;
		break;
	default:
		if (*topt > 47 && *topt < 58)
			var = read_value(topt, optval);
		break;
	}

	if (var == VALUE_NONE || (sign && addvar == VALUE_NONE)) {
		warnx(badvalue, topt);
		return;
	}

	print_value(fd, optval);
	printf(" -> ");

	set_value(fd, optval, var);

	print_value(fd, optval);
	putchar('\n');
}

/*
 * Convert string to integer representation of a parameter
 */
static u_int
parse_option(const char *topt)
{
	u_int res;
	int toptlen, varlen, len, varsize;

	if (topt == NULL || *topt == '\0')
		return OPTION_NONE;

	varsize = sizeof(varname) / sizeof(varname[0]);
	toptlen = strlen(topt);

	for (res = 0; res < varsize; res++) {
		varlen = strlen(varname[res]);
		len = toptlen > varlen ? toptlen : varlen;
		if (strncmp(topt, varname[res], len) == 0)
			return res;
	}

	warnx("bad name `%s'", topt);
	return OPTION_NONE;
}

/*
 * Returns current value of parameter optval
 */
static u_long
get_value(int fd, u_int optval)
{
	u_long var = VALUE_NONE;

	switch (optval) {
	case OPTION_VOLUME:
		if (ioctl(fd, RIOCGVOLU, &var) < 0)
			warn("RIOCGVOLU");
		break;
	case OPTION_FREQUENCY:
		if (ioctl(fd, RIOCGFREQ, &var) < 0)
			warn("RIOCGFREQ");
		break;
	case OPTION_REFERENCE:
		if (caps & RADIO_CAPS_REFERENCE_FREQ)
			if (ioctl(fd, RIOCGREFF, &var) < 0)
				warn("RIOCGREFF");
		break;
	case OPTION_MONO:
		/* FALLTHROUGH */
	case OPTION_STEREO:
		if (caps & RADIO_CAPS_SET_MONO)
			if (ioctl(fd, RIOCGMONO, &var) < 0)
				warn("RIOCGMONO");
		break;
	case OPTION_SENSITIVITY:
		if (caps & RADIO_CAPS_LOCK_SENSITIVITY)
			if (ioctl(fd, RIOCGLOCK, &var) < 0)
				warn("RIOCGLOCK");
		break;
	case OPTION_MUTE:
		if (ioctl(fd, RIOCGMUTE, &var) < 0)
			warn("RIOCGMUTE");
		break;
	}

	if (var == VALUE_NONE)
		warn_unsupported(optval);

	return var;
}

/*
 * Set card parameter optval to value var
 */
static void
set_value(int fd, u_int optval, u_long var)
{
	int unsupported = 0;

	if (var == VALUE_NONE)
		return;

	switch (optval) {
	case OPTION_VOLUME:
		if (ioctl(fd, RIOCSVOLU, &var) < 0)
			warn("RIOCSVOLU");
		break;
	case OPTION_FREQUENCY:
		if (ioctl(fd, RIOCSFREQ, &var) < 0)
			warn("RIOCSFREQ");
		break;
	case OPTION_REFERENCE:
		if (caps & RADIO_CAPS_REFERENCE_FREQ) {
			if (ioctl(fd, RIOCSREFF, &var) < 0)
				warn("RIOCSREFF");
		} else unsupported++;
		break;
	case OPTION_STEREO:
		var = !var;
		/* FALLTHROUGH */
	case OPTION_MONO:
		if (caps & RADIO_CAPS_SET_MONO) {
			if (ioctl(fd, RIOCSMONO, &var) < 0)
				warn("RIOCSMONO");
		} else unsupported++;
		break;
	case OPTION_SENSITIVITY:
		if (caps & RADIO_CAPS_LOCK_SENSITIVITY) {
			if (ioctl(fd, RIOCSLOCK, &var) < 0)
				warn("RIOCSLOCK");
		} else unsupported++;
		break;
	case OPTION_SEARCH:
		if (caps & RADIO_CAPS_HW_SEARCH) {
			if (ioctl(fd, RIOCSSRCH, &var) < 0)
				warn("RIOCSSRCH");
		} else unsupported++;
		break;
	case OPTION_MUTE:
		if (ioctl(fd, RIOCSMUTE, &var) < 0)
			warn("RIOCSMUTE");
		break;
	}

	if ( unsupported )
		warn_unsupported(optval);
}

/*
 * Convert string to float or unsigned integer
 */
static u_long
read_value(char *str, u_int optval)
{
	u_long val;

	if (str == NULL || *str == '\0')
		return VALUE_NONE;

	if (optval == OPTION_FREQUENCY)
		val = (u_long)1000 * atof(str);
	else
		val = (u_long)strtol(str, (char **)NULL, 10);

	return val;
}

/*
 * Print current value of the parameter.
 */
static void
print_value(int fd, u_int optval)
{
	u_long var, mhz;

	if (optval == OPTION_NONE)
		return;

	if ( optval == OPTION_SEARCH)
		var = get_value(fd, OPTION_FREQUENCY);
	else
		var = get_value(fd, optval);

	if (var == VALUE_NONE)
		return;

	switch (optval) {
	case OPTION_SEARCH:
		/* FALLTHROUGH */
	case OPTION_FREQUENCY:
		mhz = var / 1000;
		printf("%u.%uMHz", (u_int)mhz,
			(u_int)var / 10 - (u_int)mhz * 100);
		break;
	case OPTION_REFERENCE:
		printf("%ukHz", (u_int)var);
		break;
	case OPTION_SENSITIVITY:
		printf("%umkV", (u_int)var);
		break;
	case OPTION_MUTE:
		/* FALLTHROUGH */
	case OPTION_MONO:
		printf("%s", var ? onchar : offchar);
		break;
	case OPTION_STEREO:
		printf("%s", var ? offchar : onchar);
		break;
	default:
		printf("%u", (u_int)var);
		break;
	}
}

static void
warn_unsupported(u_int optval)
{
	warnx("driver does not support `%s'", varname[optval]);
}

static void
ext_print(int fd, u_int optval, int silent)
{
	if (optval == OPTION_NONE)
		return;

	if (!silent)
		printf("%s=", varname[optval]);
	print_value(fd, optval);
	putchar('\n');
}
