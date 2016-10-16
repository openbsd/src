/*	$NetBSD: msdosfs_conv.c,v 1.17 2016/06/30 09:34:01 nonaka Exp $	*/

/*-
 * Copyright (C) 1995, 1997 Wolfgang Solfrank.
 * Copyright (C) 1995, 1997 TooLs GmbH.
 * All rights reserved.
 * Original code by Paul Popelka (paulp@uts.amdahl.com) (see below).
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
 *	This product includes software developed by TooLs GmbH.
 * 4. The name of TooLs GmbH may not be used to endorse or promote products
 *    derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY TOOLS GMBH ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL TOOLS GMBH BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS;
 * OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
 * WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR
 * OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF
 * ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */
/*
 * Written by Paul Popelka (paulp@uts.amdahl.com)
 *
 * You can do anything you want with this software, just don't say you wrote
 * it, and don't remove this notice.
 *
 * This software is provided "as is".
 *
 * The author supplies this software to be publicly redistributed on the
 * understanding that the author is not responsible for the correct
 * functioning of this software in any circumstances and is not liable for
 * any damages caused by this software.
 *
 * October 1992
 *
 */

#include <assert.h>
#define KASSERT(x)     assert(x)

/*
 * System include files.
 */
#include <sys/param.h>
#include <sys/time.h>
#include <sys/endian.h>
#include <stdio.h>
#include <string.h>
#include <dirent.h>
#include <sys/queue.h>
#include <fs/msdosfs/clock_subr.h>	/* XXX */

/*
 * MSDOSFS include files.
 */
#include <fs/msdosfs/direntry.h>
#include <fs/msdosfs/denode.h>

static int invalidname(const u_int16_t *, int);

static int ucs2utf8(const u_int16_t *, u_int8_t *, int);
static int utf8ucs2(const u_int8_t *, int, u_int16_t *);

static int ucs2utf8str(const u_int16_t *, int, u_int8_t *, int);
static int utf8ucs2str(const u_int8_t *, int, u_int16_t *, int);
static int ucs2char8str(const u_int16_t *, int, u_int8_t *, int);
static int char8ucs2str(const u_int8_t *, int, u_int16_t *, int);

static void ucs2pad(u_int16_t *, int, int);

static u_int16_t ucs2fold(u_int16_t);
static int ucs2match(u_int16_t *, u_int16_t *, int n);
static int char8match(u_int16_t *, u_int16_t *, int n);

/*
 * The number of seconds between Jan 1, 1970 and Jan 1, 1980. In that
 * interval there were 8 regular years and 2 leap years.
 */
#define	DOSBIASYEAR	1980
#define	SECONDSTO1980	(((8 * 365) + (2 * 366)) * (24 * 60 * 60))
/*
 * msdos fs can not store dates beyound the year 2234
 */
#define DOSMAXYEAR	((DD_YEAR_MASK >> DD_YEAR_SHIFT) + DOSBIASYEAR)

/*
 * Convert the unix version of time to dos's idea of time to be used in
 * file timestamps. The passed in unix time is assumed to be in GMT.
 */
void
unix2dostime(const struct timespec *tsp, int gmtoff, u_int16_t *ddp, u_int16_t *dtp, u_int8_t *dhp)
{
	u_long t;
	struct clock_ymdhms ymd;

	t = tsp->tv_sec + gmtoff; /* time zone correction */

	/*
	 * DOS timestamps can not represent dates before 1980.
	 */
	if (t < SECONDSTO1980)
		goto invalid_dos_date;

	/*
	 * DOS granularity is 2 seconds
	 */
	t &= ~1;

	/*
	 * Convert to year/month/day/.. format
	 */
	clock_secs_to_ymdhms(t, &ymd);
	if (ymd.dt_year > DOSMAXYEAR)
		goto invalid_dos_date;

	/*
	 * Now transform to DOS format
	 */
	*ddp = (ymd.dt_day << DD_DAY_SHIFT)
	    + (ymd.dt_mon << DD_MONTH_SHIFT)
	    + ((ymd.dt_year - DOSBIASYEAR) << DD_YEAR_SHIFT);
	if (dhp)
		*dhp = (tsp->tv_sec & 1) * 100 + tsp->tv_nsec / 10000000;
	if (dtp)
		*dtp = (((t / 2) % 30) << DT_2SECONDS_SHIFT)
		    + (((t / 60) % 60) << DT_MINUTES_SHIFT)
		    + (((t / 3600) % 24) << DT_HOURS_SHIFT);
	return;

invalid_dos_date:
	*ddp = 0;
	if (dtp)
		*dtp = 0;
	if (dhp)
		*dhp = 0;
}

/*
 * Convert from dos' idea of time to unix'. This will probably only be
 * called from the stat(), and fstat() system calls and so probably need
 * not be too efficient.
 */
void
dos2unixtime(u_int dd, u_int dt, u_int dh, int gmtoff, struct timespec *tsp)
{
	time_t seconds;
	struct clock_ymdhms ymd;

	if (dd == 0) {
		/*
		 * Uninitialized field, return the epoch.
		 */
		tsp->tv_sec = 0;
		tsp->tv_nsec = 0;
		return;
	}

	memset(&ymd, 0, sizeof(ymd));
	ymd.dt_year = ((dd & DD_YEAR_MASK) >> DD_YEAR_SHIFT) + 1980 ;
	ymd.dt_mon = ((dd & DD_MONTH_MASK) >> DD_MONTH_SHIFT);
	ymd.dt_day = ((dd & DD_DAY_MASK) >> DD_DAY_SHIFT);
	ymd.dt_hour = (dt & DT_HOURS_MASK) >> DT_HOURS_SHIFT;
	ymd.dt_min = (dt & DT_MINUTES_MASK) >> DT_MINUTES_SHIFT;
	ymd.dt_sec = ((dt & DT_2SECONDS_MASK) >> DT_2SECONDS_SHIFT) * 2;

	seconds = clock_ymdhms_to_secs(&ymd);

	tsp->tv_sec = seconds;
	tsp->tv_sec -= gmtoff;	/* time zone correction */
	tsp->tv_nsec = (dh % 100) * 10000000;
}

static const u_char
unix2dos[256] = {
	0,    0,    0,    0,    0,    0,    0,    0,	/* 00-07 */
	0,    0,    0,    0,    0,    0,    0,    0,	/* 08-0f */
	0,    0,    0,    0,    0,    0,    0,    0,	/* 10-17 */
	0,    0,    0,    0,    0,    0,    0,    0,	/* 18-1f */
	0,    '!',  0,    '#',  '$',  '%',  '&',  '\'',	/* 20-27 */
	'(',  ')',  0,    '+',  0,    '-',  0,    0,	/* 28-2f */
	'0',  '1',  '2',  '3',  '4',  '5',  '6',  '7',	/* 30-37 */
	'8',  '9',  0,    0,    0,    0,    0,    0,	/* 38-3f */
	'@',  'A',  'B',  'C',  'D',  'E',  'F',  'G',	/* 40-47 */
	'H',  'I',  'J',  'K',  'L',  'M',  'N',  'O',	/* 48-4f */
	'P',  'Q',  'R',  'S',  'T',  'U',  'V',  'W',	/* 50-57 */
	'X',  'Y',  'Z',  0,    0,    0,    '^',  '_',	/* 58-5f */
	'`',  'A',  'B',  'C',  'D',  'E',  'F',  'G',	/* 60-67 */
	'H',  'I',  'J',  'K',  'L',  'M',  'N',  'O',	/* 68-6f */
	'P',  'Q',  'R',  'S',  'T',  'U',  'V',  'W',	/* 70-77 */
	'X',  'Y',  'Z',  '{',  0,    '}',  '~',  0,	/* 78-7f */
	0,    0,    0,    0,    0,    0,    0,    0,	/* 80-87 */
	0,    0,    0,    0,    0,    0,    0,    0,	/* 88-8f */
	0,    0,    0,    0,    0,    0,    0,    0,	/* 90-97 */
	0,    0,    0,    0,    0,    0,    0,    0,	/* 98-9f */
	0,    0xad, 0xbd, 0x9c, 0xcf, 0xbe, 0xdd, 0xf5,	/* a0-a7 */
	0xf9, 0xb8, 0xa6, 0xae, 0xaa, 0xf0, 0xa9, 0xee,	/* a8-af */
	0xf8, 0xf1, 0xfd, 0xfc, 0xef, 0xe6, 0xf4, 0xfa,	/* b0-b7 */
	0xf7, 0xfb, 0xa7, 0xaf, 0xac, 0xab, 0xf3, 0xa8,	/* b8-bf */
	0xb7, 0xb5, 0xb6, 0xc7, 0x8e, 0x8f, 0x92, 0x80,	/* c0-c7 */
	0xd4, 0x90, 0xd2, 0xd3, 0xde, 0xd6, 0xd7, 0xd8,	/* c8-cf */
	0xd1, 0xa5, 0xe3, 0xe0, 0xe2, 0xe5, 0x99, 0x9e,	/* d0-d7 */
	0x9d, 0xeb, 0xe9, 0xea, 0x9a, 0xed, 0xe8, 0xe1,	/* d8-df */
	0xb7, 0xb5, 0xb6, 0xc7, 0x8e, 0x8f, 0x92, 0x80,	/* e0-e7 */
	0xd4, 0x90, 0xd2, 0xd3, 0xde, 0xd6, 0xd7, 0xd8,	/* e8-ef */
	0xd1, 0xa5, 0xe3, 0xe0, 0xe2, 0xe5, 0x99, 0xf6,	/* f0-f7 */
	0x9d, 0xeb, 0xe9, 0xea, 0x9a, 0xed, 0xe8, 0x98,	/* f8-ff */
};

static const u_char
dos2unix[256] = {
	 '?',  '?',  '?',  '?',  '?',  '?',  '?',  '?',	/* 00-07 */
	 '?',  '?',  '?',  '?',  '?',  '?',  '?',  '?',	/* 08-0f */
	 '?',  '?',  '?',  '?',  '?',  '?',  '?',  '?',	/* 10-17 */
	 '?',  '?',  '?',  '?',  '?',  '?',  '?',  '?',	/* 18-1f */
	 ' ',  '!',  '"',  '#',  '$',  '%',  '&', '\'',	/* 20-27 */
	 '(',  ')',  '*',  '+',  ',',  '-',  '.',  '/',	/* 28-2f */
	 '0',  '1',  '2',  '3',  '4',  '5',  '6',  '7',	/* 30-37 */
	 '8',  '9',  ':',  ';',  '<',  '=',  '>',  '?',	/* 38-3f */
	 '@',  'A',  'B',  'C',  'D',  'E',  'F',  'G',	/* 40-47 */
	 'H',  'I',  'J',  'K',  'L',  'M',  'N',  'O',	/* 48-4f */
	 'P',  'Q',  'R',  'S',  'T',  'U',  'V',  'W',	/* 50-57 */
	 'X',  'Y',  'Z',  '[', '\\',  ']',  '^',  '_',	/* 58-5f */
	 '`',  'a',  'b',  'c',  'd',  'e',  'f',  'g',	/* 60-67 */
	 'h',  'i',  'j',  'k',  'l',  'm',  'n',  'o',	/* 68-6f */
	 'p',  'q',  'r',  's',  't',  'u',  'v',  'w',	/* 70-77 */
	 'x',  'y',  'z',  '{',  '|',  '}',  '~', 0x7f,	/* 78-7f */
	0xc7, 0xfc, 0xe9, 0xe2, 0xe4, 0xe0, 0xe5, 0xe7,	/* 80-87 */
	0xea, 0xeb, 0xe8, 0xef, 0xee, 0xec, 0xc4, 0xc5,	/* 88-8f */
	0xc9, 0xe6, 0xc6, 0xf4, 0xf6, 0xf2, 0xfb, 0xf9,	/* 90-97 */
	0xff, 0xd6, 0xdc, 0xf8, 0xa3, 0xd8, 0xd7,  '?',	/* 98-9f */
	0xe1, 0xed, 0xf3, 0xfa, 0xf1, 0xd1, 0xaa, 0xba,	/* a0-a7 */
	0xbf, 0xae, 0xac, 0xbd, 0xbc, 0xa1, 0xab, 0xbb,	/* a8-af */
	 '?',  '?',  '?',  '?',  '?', 0xc1, 0xc2, 0xc0,	/* b0-b7 */
	0xa9,  '?',  '?',  '?',  '?', 0xa2, 0xa5,  '?',	/* b8-bf */
	 '?',  '?',  '?',  '?',  '?',  '?', 0xe3, 0xc3,	/* c0-c7 */
	 '?',  '?',  '?',  '?',  '?',  '?',  '?', 0xa4,	/* c8-cf */
	0xf0, 0xd0, 0xca, 0xcb, 0xc8,  '?', 0xcd, 0xce,	/* d0-d7 */
	0xcf,  '?',  '?',  '?',  '?', 0xa6, 0xcc,  '?',	/* d8-df */
	0xd3, 0xdf, 0xd4, 0xd2, 0xf5, 0xd5, 0xb5, 0xfe,	/* e0-e7 */
	0xde, 0xda, 0xdb, 0xd9, 0xfd, 0xdd, 0xaf, 0x3f,	/* e8-ef */
	0xad, 0xb1,  '?', 0xbe, 0xb6, 0xa7, 0xf7, 0xb8,	/* f0-f7 */
	0xb0, 0xa8, 0xb7, 0xb9, 0xb3, 0xb2,  '?',  '?',	/* f8-ff */
};

static const u_char
u2l[256] = {
	0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, /* 00-07 */
	0x08, 0x09, 0x0a, 0x0b, 0x0c, 0x0d, 0x0e, 0x0f, /* 08-0f */
	0x10, 0x11, 0x12, 0x13, 0x14, 0x15, 0x16, 0x17, /* 10-17 */
	0x18, 0x19, 0x1a, 0x1b, 0x1c, 0x1d, 0x1e, 0x1f, /* 18-1f */
	 ' ',  '!',  '"',  '#',  '$',  '%',  '&', '\'', /* 20-27 */
	 '(',  ')',  '*',  '+',  ',',  '-',  '.',  '/', /* 28-2f */
	 '0',  '1',  '2',  '3',  '4',  '5',  '6',  '7', /* 30-37 */
	 '8',  '9',  ':',  ';',  '<',  '=',  '>',  '?', /* 38-3f */
	 '@',  'a',  'b',  'c',  'd',  'e',  'f',  'g', /* 40-47 */
	 'h',  'i',  'j',  'k',  'l',  'm',  'n',  'o', /* 48-4f */
	 'p',  'q',  'r',  's',  't',  'u',  'v',  'w', /* 50-57 */
	 'x',  'y',  'z',  '[', '\\',  ']',  '^',  '_', /* 58-5f */
	 '`',  'a',  'b',  'c',  'd',  'e',  'f',  'g', /* 60-67 */
	 'h',  'i',  'j',  'k',  'l',  'm',  'n',  'o', /* 68-6f */
	 'p',  'q',  'r',  's',  't',  'u',  'v',  'w', /* 70-77 */
	 'x',  'y',  'z',  '{',  '|',  '}',  '~', 0x7f, /* 78-7f */
	0x80, 0x81, 0x82, 0x83, 0x84, 0x85, 0x86, 0x87, /* 80-87 */
	0x88, 0x89, 0x8a, 0x8b, 0x8c, 0x8d, 0x8e, 0x8f, /* 88-8f */
	0x90, 0x91, 0x92, 0x93, 0x94, 0x95, 0x96, 0x97, /* 90-97 */
	0x98, 0x99, 0x9a, 0x9b, 0x9c, 0x9d, 0x9e, 0x9f, /* 98-9f */
	0xa0, 0xa1, 0xa2, 0xa3, 0xa4, 0xa5, 0xa6, 0xa7, /* a0-a7 */
	0xa8, 0xa9, 0xaa, 0xab, 0xac, 0xad, 0xae, 0xaf, /* a8-af */
	0xb0, 0xb1, 0xb2, 0xb3, 0xb4, 0xb5, 0xb6, 0xb7, /* b0-b7 */
	0xb8, 0xb9, 0xba, 0xbb, 0xbc, 0xbd, 0xbe, 0xbf, /* b8-bf */
	0xe0, 0xe1, 0xe2, 0xe3, 0xe4, 0xe5, 0xe6, 0xe7, /* c0-c7 */
	0xe8, 0xe9, 0xea, 0xeb, 0xec, 0xed, 0xee, 0xef, /* c8-cf */
	0xf0, 0xf1, 0xf2, 0xf3, 0xf4, 0xf5, 0xf6, 0xd7, /* d0-d7 */
	0xf8, 0xf9, 0xfa, 0xfb, 0xfc, 0xfd, 0xfe, 0xdf, /* d8-df */
	0xe0, 0xe1, 0xe2, 0xe3, 0xe4, 0xe5, 0xe6, 0xe7, /* e0-e7 */
	0xe8, 0xe9, 0xea, 0xeb, 0xec, 0xed, 0xee, 0xef, /* e8-ef */
	0xf0, 0xf1, 0xf2, 0xf3, 0xf4, 0xf5, 0xf6, 0xf7, /* f0-f7 */
	0xf8, 0xf9, 0xfa, 0xfb, 0xfc, 0xfd, 0xfe, 0xff, /* f8-ff */
};

/*
 * DOS filenames are made of 2 parts, the name part and the extension part.
 * The name part is 8 characters long and the extension part is 3
 * characters long.  They may contain trailing blanks if the name or
 * extension are not long enough to fill their respective fields.
 */

/*
 * Convert a DOS filename to a unix filename. And, return the number of
 * characters in the resulting unix filename excluding the terminating
 * null.
 */
int
dos2unixfn(u_char dn[11], u_char *un, int lower)
{
	int i, j;
	int thislong = 1;
	u_char c;

	/*
	 * If first char of the filename is SLOT_E5 (0x05), then the real
	 * first char of the filename should be 0xe5. But, they couldn't
	 * just have a 0xe5 mean 0xe5 because that is used to mean a freed
	 * directory slot. Another dos quirk.
	 */
	if (*dn == SLOT_E5)
		c = dos2unix[0xe5];
	else
		c = dos2unix[*dn];
	*un++ = lower ? u2l[c] : c;

	/*
	 * Copy the rest into the unix filename string, ignoring
	 * trailing blanks.
	 */

	for (j=7; (j >= 0) && (dn[j] == ' '); j--)
		;

	for (i = 1; i <= j; i++) {
		c = dos2unix[dn[i]];
		*un++ = lower ? u2l[c] : c;
		thislong++;
	}
	dn += 8;

	/*
	 * Now, if there is an extension then put in a period and copy in
	 * the extension.
	 */
	if (*dn != ' ') {
		*un++ = '.';
		thislong++;
		for (i = 0; i < 3 && *dn != ' '; i++) {
			c = dos2unix[*dn++];
			*un++ = lower ? u2l[c] : c;
			thislong++;
		}
	}
	*un++ = 0;

	return (thislong);
}

/*
 * Convert a unix filename to a DOS filename according to Win95 rules.
 * If applicable and gen is not 0, it is inserted into the converted
 * filename as a generation number.
 * Returns
 *	0 if name couldn't be converted
 *	1 if the converted name is the same as the original
 *	  (no long filename entry necessary for Win95)
 *	2 if conversion was successful
 *	3 if conversion was successful and generation number was inserted
 */
int
unix2dosfn(const u_char *un, u_char dn[12], int unlen, u_int gen)
{
	int i, j, l;
	int conv = 1;
	const u_char *cp, *dp, *dp1;
	u_char gentext[6], *wcp;
	int shortlen;

	/*
	 * Fill the dos filename string with blanks. These are DOS's pad
	 * characters.
	 */
	for (i = 0; i < 11; i++)
		dn[i] = ' ';
	dn[11] = 0;

	/*
	 * The filenames "." and ".." are handled specially, since they
	 * don't follow dos filename rules.
	 */
	if (un[0] == '.' && unlen == 1) {
		dn[0] = '.';
		return gen <= 1;
	}
	if (un[0] == '.' && un[1] == '.' && unlen == 2) {
		dn[0] = '.';
		dn[1] = '.';
		return gen <= 1;
	}

	/*
	 * Filenames with only blanks and dots are not allowed!
	 */
	for (cp = un, i = unlen; --i >= 0; cp++)
		if (*cp != ' ' && *cp != '.')
			break;
	if (i < 0)
		return 0;

	/*
	 * Now find the extension
	 * Note: dot as first char doesn't start extension
	 *	 and trailing dots and blanks are ignored
	 */
	dp = dp1 = 0;
	for (cp = un + 1, i = unlen - 1; --i >= 0;) {
		switch (*cp++) {
		case '.':
			if (!dp1)
				dp1 = cp;
			break;
		case ' ':
			break;
		default:
			if (dp1)
				dp = dp1;
			dp1 = 0;
			break;
		}
	}

	/*
	 * Now convert it
	 */
	if (dp) {
		if (dp1)
			l = dp1 - dp;
		else
			l = unlen - (dp - un);
		for (i = 0, j = 8; i < l && j < 11; i++, j++) {
			if (dp[i] != (dn[j] = unix2dos[dp[i]])
			    && conv != 3)
				conv = 2;
			if (!dn[j]) {
				conv = 3;
				dn[j--] = ' ';
			}
		}
		if (i < l)
			conv = 3;
		dp--;
	} else {
		for (dp = cp; *--dp == ' ' || *dp == '.';);
		dp++;
	}

	shortlen = (dp - un) <= 8;

	/*
	 * Now convert the rest of the name
	 */
	for (i = j = 0; un < dp && j < 8; i++, j++, un++) {
		if ((*un == ' ') && shortlen)
			dn[j] = ' ';
		else
			dn[j] = unix2dos[*un];
		if ((*un != dn[j])
		    && conv != 3)
			conv = 2;
		if (!dn[j]) {
			conv = 3;
			dn[j--] = ' ';
		}
	}
	if (un < dp)
		conv = 3;
	/*
	 * If we didn't have any chars in filename,
	 * generate a default
	 */
	if (!j)
		dn[0] = '_';

	/*
	 * The first character cannot be E5,
	 * because that means a deleted entry
	 */
	if (dn[0] == 0xe5)
		dn[0] = SLOT_E5;

	/*
	 * If there wasn't any char dropped,
	 * there is no place for generation numbers
	 */
	if (conv != 3) {
		if (gen > 1)
			return 0;
		return conv;
	}

	/*
	 * Now insert the generation number into the filename part
	 */
	for (wcp = gentext + sizeof(gentext); wcp > gentext && gen; gen /= 10)
		*--wcp = gen % 10 + '0';
	if (gen)
		return 0;
	for (i = 8; dn[--i] == ' ';);
	i++;
	if (gentext + sizeof(gentext) - wcp + 1 > 8 - i)
		i = 8 - (gentext + sizeof(gentext) - wcp + 1);
	dn[i++] = '~';
	while (wcp < gentext + sizeof(gentext))
		dn[i++] = *wcp++;
	return 3;
}

/*
 * Create a Win95 long name directory entry
 * Note: assumes that the filename is valid,
 *	 i.e. doesn't consist solely of blanks and dots
 */
int
unix2winfn(const u_char *un, int unlen, struct winentry *wep, int cnt, int chksum, int utf8)
{
	u_int16_t wn[WIN_MAXLEN], *p;
	int i, len;
	const u_char *cp;

	/*
	 * Drop trailing blanks and dots
	 */
	for (cp = un + unlen; unlen > 0; unlen--)
		if (*--cp != ' ' && *cp != '.')
			break;

	/*
	 * Offset of this entry
	 */
	i = (cnt - 1) * WIN_CHARS;

	/*
	 * Translate UNIX name to ucs-2
	 */
	len = utf8 ? utf8ucs2str(un, unlen, wn, WIN_MAXLEN) : char8ucs2str(un, unlen, wn, WIN_MAXLEN);
	ucs2pad(wn, len, WIN_MAXLEN);

	/*
	 * Initialize winentry to some useful default
	 */
	memset(wep, 0xff, sizeof(*wep));
	wep->weCnt = cnt;
	wep->weAttributes = ATTR_WIN95;
	wep->weReserved1 = 0;
	wep->weChksum = chksum;
	wep->weReserved2 = 0;

	/*
	 * Store name segment into directory entry
	 */
	p = &wn[i];
	memcpy(wep->wePart1, p, sizeof(wep->wePart1));
	p += sizeof(wep->wePart1) / sizeof(*p);
	memcpy(wep->wePart2, p, sizeof(wep->wePart2));
	p += sizeof(wep->wePart2) / sizeof(*p);
	memcpy(wep->wePart3, p, sizeof(wep->wePart3));
	
	if (len > i + WIN_CHARS)
		return 1;

	wep->weCnt |= WIN_LAST;
	return 0;
}

/*
 * Compare our filename to the one in the Win95 entry
 * Returns the checksum or -1 if no match
 */
int
winChkName(const u_char *un, int unlen, struct winentry *wep, int chksum, int utf8)
{
	u_int16_t wn[WIN_MAXLEN], *p;
	u_int16_t buf[WIN_CHARS];
	int i, len;

	/*
	 * First compare checksums
	 */
	if (wep->weCnt & WIN_LAST)
		chksum = wep->weChksum;
	else if (chksum != wep->weChksum)
		chksum = -1;
	if (chksum == -1)
		return -1;

	/*
	 * Offset of this entry
	 */
	i = ((wep->weCnt & WIN_CNT) - 1) * WIN_CHARS;

	/*
	 * Translate UNIX name to ucs-2
	 */
	len = utf8 ? utf8ucs2str(un, unlen, wn, WIN_MAXLEN) : char8ucs2str(un, unlen, wn, WIN_MAXLEN);
	ucs2pad(wn, len, WIN_MAXLEN);

	if (i >= len + 1)
		return -1;
	if ((wep->weCnt & WIN_LAST) && (len - i > WIN_CHARS))
		return -1;

	/*
	 * Fetch name segment from directory entry
	 */
	p = &buf[0];
	memcpy(p, wep->wePart1, sizeof(wep->wePart1));
	p += sizeof(wep->wePart1) / sizeof(*p);
	memcpy(p, wep->wePart2, sizeof(wep->wePart2));
	p += sizeof(wep->wePart2) / sizeof(*p);
	memcpy(p, wep->wePart3, sizeof(wep->wePart3));

	/*
	 * And compare name segment
	 */
	if (! (utf8 ? ucs2match(&wn[i], buf, WIN_CHARS) : char8match(&wn[i], buf, WIN_CHARS)))
		return -1;

	return chksum;
}

/*
 * Convert Win95 filename to dirbuf.
 * Returns the checksum or -1 if impossible
 */
int
win2unixfn(struct winentry *wep, struct dirent *dp, int chksum,
    uint16_t *namlen, int utf8)
{
	u_int16_t wn[WIN_CHARS], *p;
	u_int8_t buf[WIN_CHARS*3];
	int len;

	if ((wep->weCnt & WIN_CNT) > howmany(WIN_MAXLEN, WIN_CHARS)
	    || !(wep->weCnt & WIN_CNT))
		return -1;

	/*
	 * First compare checksums
	 */
	if (wep->weCnt & WIN_LAST) {
		chksum = wep->weChksum;
		*namlen = 0;
	} else if (chksum != wep->weChksum)
		chksum = -1;
	if (chksum == -1)
		return -1;

	/*
	 * Fetch name segment from directory entry
	 */
	p = &wn[0];
	memcpy(p, wep->wePart1, sizeof(wep->wePart1));
	p += sizeof(wep->wePart1) / sizeof(*p);
	memcpy(p, wep->wePart2, sizeof(wep->wePart2));
	p += sizeof(wep->wePart2) / sizeof(*p);
	memcpy(p, wep->wePart3, sizeof(wep->wePart3));

	/*
	 * Don't allow slashes in UNIX names. Discard that entry.
	 */
	if (invalidname(wn, WIN_CHARS))
		return -1;

	/*
	 * Translate ucs-2 to UNIX name
	 */
	len = utf8 ? ucs2utf8str(wn, WIN_CHARS, buf, sizeof(buf))
	    : ucs2char8str(wn, WIN_CHARS, buf, sizeof(buf));

	KASSERT(len >= 0);
	KASSERT((size_t)len <= MIN(sizeof(buf), sizeof(dp->d_name)-1));

	/*
	 * Prepend name segment to directory entry
	 *
	 * This ignores the slot number from the windows entry but
	 * assumes that segments are read in reverse order.
	 *
	 * The UCS-2 name (up to 255 chars) can overflow the UNIX
	 * directory entry (up to 511 bytes). Trailing characters
	 * are silently discarded. This could also end in multiple
	 * files using the same (truncated) name.
	 */
	*namlen += len;
	if (*namlen > sizeof(dp->d_name) - 1)
		*namlen = sizeof(dp->d_name) - 1;

	KASSERT(*namlen >= len);

	memmove(&dp->d_name[len], &dp->d_name[0], *namlen - len);
	memcpy(dp->d_name, buf, len);

	return chksum;
}

/*
 * Compute the checksum of a DOS filename for Win95 use
 */
u_int8_t
winChksum(u_int8_t *name)
{
	int i;
	u_int8_t s;

	for (s = 0, i = 11; --i >= 0; s += *name++)
		s = (s << 7) | (s >> 1);
	return s;
}

/*
 * Determine the number of slots necessary for Win95 names
 */
int
winSlotCnt(const u_char *un, int unlen, int utf8)
{
	const u_char *cp;
	int len;

	/*
	 * Drop trailing blanks and dots
	 */
	for (cp = un + unlen; unlen > 0; unlen--)
		if (*--cp != ' ' && *cp != '.')
			break;

	len = utf8 ? utf8ucs2str(un, unlen, NULL, WIN_MAXLEN) : unlen;

	return howmany(len, WIN_CHARS);
}

/*
 * Scan windows name for characters that must not
 * appear in a UNIX filename
 */
static int
invalidname(const u_int16_t *in, int n)
{
	while (n-- > 0) {
		if (*in++ == '/')
			return 1;
	}

	return 0;
}

/*
 * Convert UCS-2 character into UTF-8
 * return number of output bytes or 0 if output
 * buffer is too short
 */
static int
ucs2utf8(const u_int16_t *in, u_int8_t *out, int n)
{
	uint16_t inch = le16toh(in[0]);

	if (inch <= 0x007f) {
		if (n < 1) return 0;
		if (out)
			*out++ = inch;
		return 1;
	} else if (inch <= 0x07ff) {
		if (n < 2) return 0;
		if (out) {
			*out++ = 0xc0 | (inch >> 6);
			*out++ = 0x80 | (inch & 0x3f);
		}
		return 2;
	} else {
		if (n < 3) return 0;
		if (out) {
			*out++ = 0xe0 | (inch >> 12);
			*out++ = 0x80 | ((inch >> 6) & 0x3f);
			*out++ = 0x80 | (inch & 0x3f);
		}
		return 3;
	}
}


/*
 * Convert UTF-8 bytes into UCS-2 character
 * return number of input bytes, 0 if input
 * is too short and -1 if input is invalid
 */
static int
utf8ucs2(const u_int8_t *in, int n, u_int16_t *out)
{
	uint16_t outch;

	if (n < 1) return 0;

	if (in[0] <= 0x7f) {
		outch = in[0];
		if (out)
			*out = htole16(outch);
		return 1;
	} else if (in[0] <= 0xdf) {
		if (n < 2) return 0;
		outch = (in[0] & 0x1f) << 6 | (in[1] & 0x3f);
		if (out)
			*out = htole16(outch);
		return 2;
	} else if (in[0] <= 0xef) {
		if (n < 3) return 0;
		outch = (in[0] & 0x1f) << 12 | (in[1] & 0x3f) << 6 | (in[2] & 0x3f);
		if (out)
			*out = htole16(outch);
		return 3;
	}

	return -1;
}

/*
 * Convert UCS-2 string into UTF-8 string
 * return total number of output bytes
 */
static int
ucs2utf8str(const u_int16_t *in, int n, u_int8_t *out, int m)
{
	u_int8_t *p;
	int outlen;

	p = out;
	while (n > 0 && *in != 0) {
		outlen = ucs2utf8(in, out ? p : out, m);
		if (outlen == 0)
			break;
		p += outlen;
		m -= outlen;
		in += 1;
		n -= 1;
	}

	return p - out;
}

/*
 * Convert UTF8 string into UCS-2 string
 * return total number of output chacters
 */
static int
utf8ucs2str(const u_int8_t *in, int n, u_int16_t *out, int m)
{
	u_int16_t *p;
	int inlen;

	p = out;
	while (n > 0 && *in != 0) {
		if (m < 1)
			break;
		inlen = utf8ucs2(in, n, out ? p : out);
		if (inlen <= 0)
			break;
		in += inlen;
		n -= inlen;
		p += 1;
		m -= 1;
	}

	return p - out;
}

/*
 * Convert UCS-2 string into 8bit character string
 * return total number of output bytes
 */
static int
ucs2char8str(const u_int16_t *in, int n, u_int8_t *out, int m)
{
	u_int8_t *p;
	u_int16_t inch;

	p = out;
	while (n > 0 && in[0] != 0) {
		if (m < 1)
			break;
		inch = le16toh(in[0]);
		if (inch > 255)
			break;
		if (p)
			p[0] = inch;
		p += 1;
		m -= 1;
		in += 1;
		n -= 1;
	}

	return p - out;
}

/*
 * Convert 8bit character string into UCS-2 string
 * return total number of output chacters
 */
static int
char8ucs2str(const u_int8_t *in, int n, u_int16_t *out, int m)
{
	u_int16_t *p;

	p = out;
	while (n > 0 && in[0] != 0) {
		if (m < 1)
			break;
		if (p)
			p[0] = htole16(in[0]);
		p += 1;
		m -= 1;
		in += 1;
		n -= 1;
	}

	return p - out;
}

static void
ucs2pad(u_int16_t *buf, int len, int size)
{

	if (len < size-1)
		buf[len++] = 0x0000;
	while (len < size)
		buf[len++] = 0xffff;
}

/*
 * Fold UCS-2 character to uppercase
 */
static u_int16_t
ucs2fold(u_int16_t w)
{
	int low,high,mid;
	u_int16_t check;
	extern const u_int16_t msdosfs_unicode_foldmap[];
	extern size_t msdosfs_unicode_foldmap_entries;

	w = le16toh(w);

	low = 0;
	high = msdosfs_unicode_foldmap_entries / 2;
	while (low < high) {
		mid = (low + high)/2;
		check = msdosfs_unicode_foldmap[2*mid+0];

		if (w == check) {
			w = msdosfs_unicode_foldmap[2*mid+1];
			break;
		}

		if (w < check)
			high = mid;
		else
			low = mid+1;
	}

	w = le16toh(w);

	return w;
}

/*
 * Compare two UCS-2 strings case-insensitive
 *
 * uses the Unicode case folding table
 */
static int
ucs2match(u_int16_t *w1, u_int16_t *w2, int n)
{
	u_int16_t u1, u2;

	while (n > 0) {
		if (*w1 == 0 || *w2 == 0)
			return *w1 == *w2;
		u1 = ucs2fold(*w1);
		u2 = ucs2fold(*w2);
		if (u1 != u2)
			return 0;
		++w1;
		++w2;
		--n;
	}

	return 1;
}

/*
 * Compare two 8bit char conversions case-insensitive
 *
 * uses the DOS case folding table
 */
static int
char8match(u_int16_t *w1, u_int16_t *w2, int n)
{
	u_int16_t u1, u2;

	while (n > 0) {
		u1 = le16toh(*w1);
		u2 = le16toh(*w2);
		if (u1 == 0 || u2 == 0)
			return u1 == u2;
		if (u1 > 255 || u2 > 255)
			return 0;
		u1 = u2l[u1 & 0xff];
		u2 = u2l[u2 & 0xff];
		if (u1 != u2)
			return 0;
		++w1;
		++w2;
		--n;
	}

	return 1;
}

