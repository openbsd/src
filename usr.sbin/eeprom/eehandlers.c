/*	$OpenBSD: eehandlers.c,v 1.15 2008/06/26 05:42:21 ray Exp $	*/
/*	$NetBSD: eehandlers.c,v 1.2 1996/02/28 01:13:22 thorpej Exp $	*/

/*-
 * Copyright (c) 1996 The NetBSD Foundation, Inc.
 * All rights reserved.
 *
 * This code is derived from software contributed to The NetBSD Foundation
 * by Jason R. Thorpe.
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
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

#include <sys/types.h>
#include <ctype.h>
#include <err.h>
#include <errno.h>
#include <fcntl.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

#include <machine/eeprom.h>
#include <machine/openpromio.h>

#include "defs.h"

extern	char *path_eeprom;
extern	int eval;
extern	int update_checksums;
extern	int ignore_checksum;
extern	int fix_checksum;
extern	int cksumfail;
extern	u_short writecount;

struct	timeb;
extern	time_t get_date(char *, struct timeb *);

static	char err_str[BUFSIZE];

static	void badval(struct keytabent *, char *);
static	int doio(struct keytabent *, u_char *, ssize_t, int);

#define BARF(kt) {							\
	badval((kt), arg);						\
	++eval;								\
	return;								\
}

#define FAILEDREAD(kt) {						\
	warnx("%s", err_str);						\
	warnx("failed to read field `%s'", (kt)->kt_keyword);		\
	++eval;								\
	return;								\
}

#define FAILEDWRITE(kt) {						\
	warnx("%s", err_str);						\
	warnx("failed to update field `%s'", (kt)->kt_keyword);		\
	++eval;								\
	return;								\
}

void
ee_hwupdate(struct keytabent *ktent, char *arg)
{
	time_t t;
	char *cp, *cp2;

	if (arg) {
		if ((strcmp(arg, "now") == 0) ||
		    (strcmp(arg, "today") == 0)) {
			if ((t = time(NULL)) == (time_t)(-1)) {
				warnx("can't get current time");
				++eval;
				return;
			}
		} else
			if ((t = get_date(arg, NULL)) == (time_t)(-1))
				BARF(ktent);

		if (doio(ktent, (u_char *)&t, sizeof(t), IO_WRITE))
			FAILEDWRITE(ktent);
	} else
		if (doio(ktent, (u_char *)&t, sizeof(t), IO_READ))
			FAILEDREAD(ktent);

	cp = ctime(&t);
	if ((cp2 = strrchr(cp, '\n')) != NULL)
		*cp2 = '\0';

	printf("%s=%d (%s)\n", ktent->kt_keyword, t, cp);
}

void
ee_num8(struct keytabent *ktent, char *arg)
{
	u_char num8 = 0;
	u_int num32;
	int i;

	if (arg) {
		for (i = 0; i < (strlen(arg) - 1); ++i)
			if (!isdigit(arg[i]))
				BARF(ktent);
		num32 = atoi(arg);
		if (num32 > 0xff)
			BARF(ktent);
		num8 += num32;
		if (doio(ktent, &num8, sizeof(num8), IO_WRITE))
			FAILEDWRITE(ktent);
	} else
		if (doio(ktent, &num8, sizeof(num8), IO_READ))
			FAILEDREAD(ktent);

	printf("%s=%d\n", ktent->kt_keyword, num8);
}

void
ee_num16(struct keytabent *ktent, char *arg)
{
	u_int16_t num16 = 0;
	u_int num32;
	int i;

	if (arg) {
		for (i = 0; i < (strlen(arg) - 1); ++i)
			if (!isdigit(arg[i]))
				BARF(ktent);
		num32 = atoi(arg);
		if (num32 > 0xffff)
			BARF(ktent);
		num16 += num32;
		if (doio(ktent, (u_char *)&num16, sizeof(num16), IO_WRITE))
			FAILEDWRITE(ktent);
	} else
		if (doio(ktent, (u_char *)&num16, sizeof(num16), IO_READ))
			FAILEDREAD(ktent);

	printf("%s=%d\n", ktent->kt_keyword, num16);
}

static	struct strvaltabent scrsizetab[] = {
	{ "640x480",		EED_SCR_640X480 },
	{ "1152x900",		EED_SCR_1152X900 },
	{ "1024x1024",		EED_SCR_1024X1024 },
	{ "1600x1280",		EED_SCR_1600X1280 },
	{ "1280x1024",		EED_SCR_1280X1024 },
	{ "1440x1440",		EED_SCR_1440X1440 },
	{ NULL,			0 },
};

void
ee_screensize(struct keytabent *ktent, char *arg)
{
	struct strvaltabent *svp;
	u_char scsize;

	if (arg) {
		for (svp = scrsizetab; svp->sv_str != NULL; ++svp)
			if (strcmp(svp->sv_str, arg) == 0)
				break;
		if (svp->sv_str == NULL)
			BARF(ktent);

		scsize = svp->sv_val;
		if (doio(ktent, &scsize, sizeof(scsize), IO_WRITE))
			FAILEDWRITE(ktent);
	} else {
		if (doio(ktent, &scsize, sizeof(scsize), IO_READ))
			FAILEDREAD(ktent);

		for (svp = scrsizetab; svp->sv_str != NULL; ++svp)
			if (svp->sv_val == scsize)
				break;
		if (svp->sv_str == NULL) {
			warnx("unknown %s value %d", ktent->kt_keyword,
			    scsize);
			return;
		}
	}
	printf("%s=%s\n", ktent->kt_keyword, svp->sv_str);
}

static	struct strvaltabent truthtab[] = {
	{ "true",		EE_TRUE },
	{ "false",		EE_FALSE },
	{ NULL,			0 },
};

void
ee_truefalse(struct keytabent *ktent, char *arg)
{
	struct strvaltabent *svp;
	u_char truth;

	if (arg) {
		for (svp = truthtab; svp->sv_str != NULL; ++svp)
			if (strcmp(svp->sv_str, arg) == 0)
				break;
		if (svp->sv_str == NULL)
			BARF(ktent);

		truth = svp->sv_val;
		if (doio(ktent, &truth, sizeof(truth), IO_WRITE))
			FAILEDWRITE(ktent);
	} else {
		if (doio(ktent, &truth, sizeof(truth), IO_READ))
			FAILEDREAD(ktent);

		for (svp = truthtab; svp->sv_str != NULL; ++svp)
			if (svp->sv_val == truth)
				break;
		if (svp->sv_str == NULL) {
			warnx("unknown truth value 0x%x for %s", truth,
			    ktent->kt_keyword);
			return;
		}
	}
	printf("%s=%s\n", ktent->kt_keyword, svp->sv_str);
}

void
ee_bootdev(struct keytabent *ktent, char *arg)
{
	u_char dev[5];
	int i;
	size_t arglen;
	char *cp;

	if (arg) {
		/*
		 * The format of the string we accept is the following:
		 *	cc(n,n,n)
		 * where:
		 *	c -- an alphabetical character [a-z]
		 *	n -- a number in hexadecimal, between 0 and ff,
		 *	     with no leading `0x'.
		 */
		arglen = strlen(arg);
		if (arglen < 9 || arglen > 12 || arg[2] != '(' ||
		     arg[arglen - 1] != ')')
			BARF(ktent);

		/* Handle the first 2 letters. */
		for (i = 0; i < 2; ++i) {
			if (arg[i] < 'a' || arg[i] > 'z')
				BARF(ktent);
			dev[i] = (u_char)arg[i];
		}

		/* Handle the 3 `0x'-less hex values. */
		cp = &arg[3];
		for (i = 2; i < 5; ++i) {
			if (*cp == '\0')
				BARF(ktent);

			if (*cp >= '0' && *cp <= '9')
				dev[i] = *cp++ - '0';
			else if (*cp >= 'a' && *cp <= 'f')
				dev[i] = 10 + (*cp++ - 'a');
			else
				BARF(ktent);

			/* Deal with a second digit. */
			if (*cp >= '0' && *cp <= '9') {
				dev[i] <<= 4;
				dev[i] &= 0xf0;
				dev[i] += *cp++ - '0';
			} else if (*cp >= 'a' && *cp <= 'f') {
				dev[i] <<= 4;
				dev[i] &= 0xf0;
				dev[i] += 10 + (*cp++ - 'a');
			}

			/* Ensure we have the correct delimiter. */
			if ((*cp == ',' && i < 4) || (*cp == ')' && i == 4)) {
				++cp;
				continue;
			} else
				BARF(ktent);
		}
		if (doio(ktent, (u_char *)&dev[0], sizeof(dev), IO_WRITE))
			FAILEDWRITE(ktent);
	} else
		if (doio(ktent, (u_char *)&dev[0], sizeof(dev), IO_READ))
			FAILEDREAD(ktent);

	printf("%s=%c%c(%x,%x,%x)\n", ktent->kt_keyword, dev[0],
	     dev[1], dev[2], dev[3], dev[4]);
}

void
ee_kbdtype(struct keytabent *ktent, char *arg)
{
	u_char kbd = 0;
	u_int kbd2;
	int i;

	if (arg) {
		for (i = 0; i < (strlen(arg) - 1); ++i)
			if (!isdigit(arg[i]))
				BARF(ktent);
		kbd2 = atoi(arg);
		if (kbd2 > 0xff)
			BARF(ktent);
		kbd += kbd2;
		if (doio(ktent, &kbd, sizeof(kbd), IO_WRITE))
			FAILEDWRITE(ktent);
	} else
		if (doio(ktent, &kbd, sizeof(kbd), IO_READ))
			FAILEDREAD(ktent);

	printf("%s=%d (%s)\n", ktent->kt_keyword, kbd, kbd ? "other" : "Sun");
}

static	struct strvaltabent constab[] = {
	{ "b&w",		EED_CONS_BW },
	{ "ttya",		EED_CONS_TTYA },
	{ "ttyb",		EED_CONS_TTYB },
	{ "color",		EED_CONS_COLOR },
	{ "p4opt",		EED_CONS_P4 },
	{ NULL,			0 },
};

void
ee_constype(struct keytabent *ktent, char *arg)
{
	struct strvaltabent *svp;
	u_char cons;

	if (arg) {
		for (svp = constab; svp->sv_str != NULL; ++svp)
			if (strcmp(svp->sv_str, arg) == 0)
				break;
		if (svp->sv_str == NULL)
			BARF(ktent);

		cons = svp->sv_val;
		if (doio(ktent, &cons, sizeof(cons), IO_WRITE))
			FAILEDWRITE(ktent);
	} else {
		if (doio(ktent, &cons, sizeof(cons), IO_READ))
			FAILEDREAD(ktent);

		for (svp = constab; svp->sv_str != NULL; ++svp)
			if (svp->sv_val == cons)
				break;
		if (svp->sv_str == NULL) {
			warnx("unknown type 0x%x for %s", cons,
			    ktent->kt_keyword);
			return;
		}
	}
	printf("%s=%s\n", ktent->kt_keyword, svp->sv_str);

}

void
ee_diagpath(struct keytabent *ktent, char *arg)
{
	char path[40];

	bzero(path, sizeof(path));
	if (arg) {
		if (strlcpy(path, arg, sizeof(path)) >= sizeof(path))
			BARF(ktent);
		if (doio(ktent, (u_char *)&path[0], sizeof(path), IO_WRITE))
			FAILEDWRITE(ktent);
	} else
		if (doio(ktent, (u_char *)&path[0], sizeof(path), IO_READ))
			FAILEDREAD(ktent);

	printf("%s=%s\n", ktent->kt_keyword, path);
}

void
ee_banner(struct keytabent *ktent, char *arg)
{
	char string[80];
	u_char enable;
	struct keytabent kt;

	kt.kt_keyword = "enable_banner";
	kt.kt_offset = EE_BANNER_ENABLE_LOC;
	kt.kt_handler = ee_notsupp;

	bzero(string, sizeof(string));
	if (arg) {
		if (*arg != '\0') {
			enable = EE_TRUE;
			if (strlcpy(string, arg, sizeof(string)) >=
			    sizeof(string))
				BARF(ktent);
			if (doio(ktent, (u_char *)string,
			    sizeof(string), IO_WRITE))
				FAILEDWRITE(ktent);
		} else {
			enable = EE_FALSE;
			if (doio(ktent, (u_char *)string,
			    sizeof(string), IO_READ))
				FAILEDREAD(ktent);
		}

		if (doio(&kt, &enable, sizeof(enable), IO_WRITE))
			FAILEDWRITE(&kt);
	} else {
		if (doio(ktent, (u_char *)string, sizeof(string), IO_READ))
			FAILEDREAD(ktent);
		if (doio(&kt, &enable, sizeof(enable), IO_READ))
			FAILEDREAD(&kt);
	}
	printf("%s=%s (%s)\n", ktent->kt_keyword, string,
	    enable == EE_TRUE ? "enabled" : "disabled");
}

/* ARGSUSED */
void
ee_notsupp(struct keytabent *ktent, char *arg)
{

	warnx("field `%s' not yet supported", ktent->kt_keyword);
}

static void
badval(struct keytabent *ktent, char *arg)
{

	warnx("inappropriate value `%s' for field `%s'", arg,
	    ktent->kt_keyword);
}

static int
doio(struct keytabent *ktent, u_char *buf, ssize_t len, int wr)
{
	int fd, rval = 0;
	u_char *buf2;

	buf2 = (u_char *)calloc(1, len);
	if (buf2 == NULL) {
		snprintf(err_str, sizeof err_str, "memory allocation failed");
		return (1);
	}

	fd = open(path_eeprom, wr == IO_WRITE ? O_RDWR : O_RDONLY, 0640);
	if (fd < 0) {
		snprintf(err_str, sizeof err_str, "open: %s: %s", path_eeprom,
		    strerror(errno));
		free(buf2);
		return (1);
	}

	if (lseek(fd, (off_t)ktent->kt_offset, SEEK_SET) < (off_t)0) {
		snprintf(err_str, sizeof err_str, "lseek: %s: %s", path_eeprom,
		    strerror(errno));
		rval = 1;
		goto done;
	}

	if (read(fd, buf2, len) != len) {
		snprintf(err_str, sizeof err_str, "read: %s: %s", path_eeprom,
		    strerror(errno));
		return (1);
	}

	if (wr == IO_WRITE) {
		if (bcmp(buf, buf2, len) == 0)
			goto done;

		if (lseek(fd, (off_t)ktent->kt_offset, SEEK_SET) < (off_t)0) {
			snprintf(err_str, sizeof err_str, "lseek: %s: %s",
			    path_eeprom, strerror(errno));
			rval = 1;
			goto done;
		}

		++update_checksums;
		if (write(fd, buf, len) < 0) {
			snprintf(err_str, sizeof err_str, "write: %s: %s",
			    path_eeprom, strerror(errno));
			rval = 1;
			goto done;
		}
	} else
		bcopy(buf2, buf, len);

 done:
	free(buf2);
	(void)close(fd);
	return (rval);
}

/*
 * Read from eeLastHwUpdate to just before eeReserved.  Calculate
 * a checksum, and deposit 3 copies of it sequentially starting at
 * eeChecksum[0].  Increment the write count, and deposit 3 copies
 * of it sequentially starting at eeWriteCount[0].
 */
void
ee_updatechecksums(void)
{
	struct keytabent kt;
	u_char checkme[EE_SIZE - EE_HWUPDATE_LOC];
	u_char checksum;
	int i;

	kt.kt_keyword = "eeprom contents";
	kt.kt_offset = EE_HWUPDATE_LOC;
	kt.kt_handler = ee_notsupp;

	if (doio(&kt, checkme, sizeof(checkme), IO_READ)) {
		cksumfail = 1;
		FAILEDREAD(&kt);
	}

	checksum = ee_checksum(checkme, sizeof(checkme));

	kt.kt_keyword = "eeprom checksum";
	for (i = 0; i < 4; ++i) {
		kt.kt_offset = EE_CKSUM_LOC + (i * sizeof(checksum));
		if (doio(&kt, &checksum, sizeof(checksum), IO_WRITE)) {
			cksumfail = 1;
			FAILEDWRITE(&kt);
		}
	}

	kt.kt_keyword = "eeprom writecount";
	for (i = 0; i < 4; ++i) {
		kt.kt_offset = EE_WC_LOC + (i * sizeof(writecount));
		if (doio(&kt, (u_char *)&writecount, sizeof(writecount),
		    IO_WRITE)) {
			cksumfail = 1;
			FAILEDWRITE(&kt);
		}
	}
}

void
ee_verifychecksums(void)
{
	struct keytabent kt;
	u_char checkme[EE_SIZE - EE_HWUPDATE_LOC];
	u_char checksum, ochecksum[3];
	u_short owritecount[3];

	/*
	 * Verify that the EEPROM's write counts match, and update the
	 * global copy for use later.
	 */
	kt.kt_keyword = "eeprom writecount";
	kt.kt_offset = EE_WC_LOC;
	kt.kt_handler = ee_notsupp;

	if (doio(&kt, (u_char *)&owritecount, sizeof(owritecount), IO_READ)) {
		cksumfail = 1;
		FAILEDREAD(&kt);
	}

	if (owritecount[0] != owritecount[1] ||
	    owritecount[0] != owritecount[2]) {
		warnx("eeprom writecount mismatch %s",
		    ignore_checksum ? "(ignoring)" :
		    (fix_checksum ? "(fixing)" : ""));

		if (!ignore_checksum && !fix_checksum) {
			cksumfail = 1;
			return;
		}

		writecount = MAXIMUM(owritecount[0], owritecount[1]);
		writecount = MAXIMUM(writecount, owritecount[2]);
	} else
		writecount = owritecount[0];

	/*
	 * Verify that the EEPROM's checksums match and are correct.
	 */
	kt.kt_keyword = "eeprom checksum";
	kt.kt_offset = EE_CKSUM_LOC;

	if (doio(&kt, ochecksum, sizeof(ochecksum), IO_READ)) {
		cksumfail = 1;
		FAILEDREAD(&kt);
	}

	if (ochecksum[0] != ochecksum[1] ||
	    ochecksum[0] != ochecksum[2]) {
		warnx("eeprom checksum mismatch %s",
		    ignore_checksum ? "(ignoring)" :
		    (fix_checksum ? "(fixing)" : ""));

		if (!ignore_checksum && !fix_checksum) {
			cksumfail = 1;
			return;
		}
	}

	kt.kt_keyword = "eeprom contents";
	kt.kt_offset = EE_HWUPDATE_LOC;

	if (doio(&kt, checkme, sizeof(checkme), IO_READ)) {
		cksumfail = 1;
		FAILEDREAD(&kt);
	}

	checksum = ee_checksum(checkme, sizeof(checkme));

	if (ochecksum[0] != checksum) {
		warnx("eeprom checksum incorrect %s",
		    ignore_checksum ? "(ignoring)" :
		    (fix_checksum ? "(fixing)" : ""));

		if (!ignore_checksum && !fix_checksum) {
			cksumfail = 1;
			return;
		}
	}

	if (fix_checksum)
		ee_updatechecksums();
}

u_char
ee_checksum(u_char *area, size_t len)
{
	u_char sum = 0;

	while (len--)
		sum += *area++;

	return (0x100 - sum);
}
