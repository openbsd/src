/*	$OpenBSD: misc.c,v 1.69 2021/06/25 19:24:53 krw Exp $	*/

/*
 * Copyright (c) 1997 Tobias Weingartner
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

#include <sys/types.h>
#include <sys/disklabel.h>

#include <ctype.h>
#include <err.h>
#include <errno.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <uuid.h>

#include "disk.h"
#include "misc.h"
#include "part.h"

struct unit_type unit_types[] = {
	{ "b"	, 1LL				, "Bytes"	},
	{ " "	, 0LL				, "Sectors"	},
	{ "K"	, 1024LL			, "Kilobytes"	},
	{ "M"	, 1024LL * 1024			, "Megabytes"	},
	{ "G"	, 1024LL * 1024 *1024		, "Gigabytes"	},
	{ "T"	, 1024LL * 1024 * 1024 * 1024	, "Terabytes"	},
	{ NULL	, 0				, NULL		},
};

int
unit_lookup(char *units)
{
	int i = 0;

	if (units == NULL)
		return (SECTORS);

	while (unit_types[i].abbr != NULL) {
		if (strncasecmp(unit_types[i].abbr, units, 1) == 0)
			break;
		i++;
	}
	/* default */
	if (unit_types[i].abbr == NULL)
		return (SECTORS);

	return (i);
}

int
string_from_line(char *buf, size_t buflen)
{
	static char *line;
	static size_t sz;
	ssize_t len;

	len = getline(&line, &sz, stdin);
	if (len == -1)
		return (1);

	if (line[len - 1] == '\n')
		line[len - 1] = '\0';

	strlcpy(buf, line, buflen);

	return (0);
}

void
ask_cmd(char **cmd, char **arg)
{
	static char lbuf[100];
	size_t cmdstart, cmdend, argstart;

	/* Get NUL terminated string from stdin. */
	if (string_from_line(lbuf, sizeof(lbuf)))
		errx(1, "eof");

	cmdstart = strspn(lbuf, " \t");
	cmdend = cmdstart + strcspn(&lbuf[cmdstart], " \t");
	argstart = cmdend + strspn(&lbuf[cmdend], " \t");

	/* *cmd and *arg may be set to point at final NUL! */
	*cmd = &lbuf[cmdstart];
	lbuf[cmdend] = '\0';
	*arg = &lbuf[argstart];
}

int
ask_num(const char *str, int dflt, int low, int high)
{
	char lbuf[100];
	const char *errstr;
	int num;

	if (dflt < low)
		dflt = low;
	else if (dflt > high)
		dflt = high;

	do {
		printf("%s [%d - %d]: [%d] ", str, low, high, dflt);

		if (string_from_line(lbuf, sizeof(lbuf)))
			errx(1, "eof");

		if (lbuf[0] == '\0') {
			num = dflt;
			errstr = NULL;
		} else {
			num = (int)strtonum(lbuf, low, high, &errstr);
			if (errstr)
				printf("%s is %s: %s.\n", str, errstr, lbuf);
		}
	} while (errstr);

	return (num);
}

int
ask_pid(int dflt, struct uuid *guid)
{
	char lbuf[100], *cp;
	int num = -1, status;

	do {
		printf("Partition id ('0' to disable) [01 - FF]: [%X] ", dflt);
		printf("(? for help) ");

		if (string_from_line(lbuf, sizeof(lbuf)))
			errx(1, "eof");

		if (lbuf[0] == '?') {
			PRT_printall();
			continue;
		}

		if (guid && strlen(lbuf) == UUID_STR_LEN) {
			uuid_from_string(lbuf, guid, &status);
			if (status == uuid_s_ok)
				return (0x100);
		}

		/* Convert */
		cp = lbuf;
		num = strtol(lbuf, &cp, 16);

		/* Make sure only number present */
		if (cp == lbuf)
			num = dflt;
		if (*cp != '\0') {
			printf("'%s' is not a valid number.\n", lbuf);
			num = -1;
		} else if (num == 0) {
			break;
		} else if (num < 0 || num > 0xff) {
			printf("'%x' is out of range.\n", num);
		}
	} while (num < 0 || num > 0xff);

	return (num);
}

int
ask_yn(const char *str)
{
	int ch, first;
	extern int y_flag;

	if (y_flag)
		return (1);

	printf("%s [n] ", str);
	fflush(stdout);

	first = ch = getchar();
	while (ch != '\n' && ch != EOF)
		ch = getchar();

	if (ch == EOF || first == EOF)
		errx(1, "eof");

	return (first == 'y' || first == 'Y');
}

/*
 * adapted from sbin/disklabel/editor.c
 */
uint64_t
getuint64(char *prompt, uint64_t oval, uint64_t minval, uint64_t maxval)
{
	const int secsize = unit_types[SECTORS].conversion;
	char buf[BUFSIZ], *endptr, *p, operator = '\0';
	size_t n;
	int64_t mult = 1;
	double d, d2;
	int rslt, secpercyl, saveerr;
	char unit;

	if (oval > maxval)
		oval = maxval;
	if (oval < minval)
		oval = minval;

	secpercyl = disk.sectors * disk.heads;

	do {
		printf("%s [%llu - %llu]: [%llu] ", prompt, minval, maxval,
		    oval);

		if (string_from_line(buf, sizeof(buf)))
			errx(1, "eof");

		if (buf[0] == '\0') {
			rslt = snprintf(buf, sizeof(buf), "%llu", oval);
			if (rslt < 0 || rslt >= sizeof(buf))
				errx(1, "default value too long");
		} else if (buf[0] == '*' && buf[1] == '\0') {
			return (maxval);
		}

		/* deal with units */
		n = strlen(buf);
		switch (tolower((unsigned char)buf[n-1])) {
		case 'c':
			unit = 'c';
			mult = secpercyl;
			buf[--n] = '\0';
			break;
		case 'b':
			unit = 'b';
			mult = -(int64_t)secsize;
			buf[--n] = '\0';
			break;
		case 's':
			unit = 's';
			mult = 1;
			buf[--n] = '\0';
			break;
		case 'k':
			unit = 'k';
			if (secsize > 1024)
				mult = -(int64_t)secsize / 1024LL;
			else
				mult = 1024LL / secsize;
			buf[--n] = '\0';
			break;
		case 'm':
			unit = 'm';
			mult = (1024LL * 1024) / secsize;
			buf[--n] = '\0';
			break;
		case 'g':
			unit = 'g';
			mult = (1024LL * 1024 * 1024) / secsize;
			buf[--n] = '\0';
			break;
		case 't':
			unit = 't';
			mult = (1024LL * 1024 * 1024 * 1024) / secsize;
			buf[--n] = '\0';
			break;
		default:
			unit = ' ';
			mult = 1;
			break;
		}

		/* deal with the operator */
		p = &buf[0];
		if (*p == '+' || *p == '-')
			operator = *p++;
		else
			operator = ' ';

		endptr = p;
		errno = 0;
		d = strtod(p, &endptr);
		saveerr = errno;
		d2 = d;
		if (mult > 0)
			d *= mult;
		else {
			d /= (-mult);
			d2 = d;
		}

		/* Apply the operator */
		if (operator == '+')
			d = oval + d;
		else if (operator == '-') {
			d = oval - d;
			d2 = d;
		}

		if (saveerr == ERANGE || d > maxval || d < minval || d < d2) {
			printf("%s is out of range: %c%s%c\n", prompt, operator,
			    p, unit);
		} else if (*endptr != '\0') {
			printf("%s is invalid: %c%s%c\n", prompt, operator,
			    p, unit);
		} else {
			break;
		}
	} while (1);

	return((uint64_t)d);
}

char *
ask_string(const char *prompt, const char *oval)
{
	static char buf[UUID_STR_LEN + 1];

	buf[0] = '\0';
	printf("%s: [%s] ", prompt, oval ? oval : "");
	if (string_from_line(buf, sizeof(buf)))
		errx(1, "eof");

	if (buf[0] == '\0' && oval)
		strlcpy(buf, oval, sizeof(buf));

	return(buf);
}

/*
 * Adapted from Hacker's Delight crc32b().
 *
 * To quote http://www.hackersdelight.org/permissions.htm :
 *
 * "You are free to use, copy, and distribute any of the code on
 *  this web site, whether modified by you or not. You need not give
 *  attribution. This includes the algorithms (some of which appear
 *  in Hacker's Delight), the Hacker's Assistant, and any code submitted
 *  by readers. Submitters implicitly agree to this."
 */
uint32_t
crc32(const u_char *buf, const uint32_t size)
{
	int j;
	uint32_t i, byte, crc, mask;

	crc = 0xFFFFFFFF;

	for (i = 0; i < size; i++) {
		byte = buf[i];			/* Get next byte. */
		crc = crc ^ byte;
		for (j = 7; j >= 0; j--) {	/* Do eight times. */
			mask = -(crc & 1);
			crc = (crc >> 1) ^ (0xEDB88320 & mask);
		}
	}

	return ~crc;
}

char *
utf16le_to_string(const uint16_t *utf)
{
	static char name[GPTPARTNAMESIZE];
	int i;

	for (i = 0; i < GPTPARTNAMESIZE; i++) {
		name[i] = letoh16(utf[i]) & 0x7F;
		if (name[i] == '\0')
			break;
	}
	if (i == GPTPARTNAMESIZE)
		name[i - 1] = '\0';

	return (name);
}

uint16_t *
string_to_utf16le(const char *ch)
{
	static uint16_t utf[GPTPARTNAMESIZE];
	int i;

	for (i = 0; i < GPTPARTNAMESIZE; i++) {
		utf[i] = htole16((unsigned int)ch[i]);
		if (utf[i] == 0)
			break;
	}
	if (i == GPTPARTNAMESIZE)
		utf[i - 1] = 0;

	return (utf);
}

void
parse_b(const char *arg, uint32_t *blocks, uint32_t *offset, uint8_t *type)
{
	const char *errstr;
	char *poffset, *ptype;
	uint32_t blockcount, blockoffset;
	uint8_t partitiontype;

	blockoffset = BLOCKALIGNMENT;
	partitiontype = DOSPTYP_EFISYS;
	ptype = NULL;

	/* First number: # of 512-byte blocks in boot partition. */
	poffset = strchr(arg, '@');
	if (poffset != NULL)
		*poffset++ = '\0';
	if (poffset != NULL) {
		ptype = strchr(poffset, ':');
		if (ptype != NULL)
			*ptype++ = '\0';
	}

	blockcount = strtonum(arg, BLOCKALIGNMENT, UINT32_MAX, &errstr);
	if (errstr)
		errx(1, "Block argument %s [%u..%u].", errstr, BLOCKALIGNMENT,
		    UINT32_MAX);

	if (poffset == NULL)
		goto done;

	/* Second number: # of 512-byte blocks to offset partition start. */
	blockoffset = strtonum(poffset, BLOCKALIGNMENT, UINT32_MAX, &errstr);
	if (errstr)
		errx(1, "Block offset argument %s [%u..%u].", errstr,
		    BLOCKALIGNMENT, UINT32_MAX);

	if (ptype == NULL)
		goto done;

	if (strlen(ptype) != 2 || !(isxdigit(*ptype) && isxdigit(*(ptype + 1))))
		errx(1, "Block type is not 2 digit hex value");

	partitiontype = strtol(ptype, NULL, 16);

 done:
	*blocks = blockcount;
	*offset = blockoffset;
	*type = partitiontype;
}
