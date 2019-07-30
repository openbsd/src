/*	$OpenBSD: stat.c,v 1.21 2015/10/10 20:35:01 deraadt Exp $ */
/*	$NetBSD: stat.c,v 1.19 2004/06/20 22:20:16 jmc Exp $ */

/*
 * Copyright (c) 2002 The NetBSD Foundation, Inc.
 * All rights reserved.
 *
 * This code is derived from software contributed to The NetBSD Foundation
 * by Andrew Brown.
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
#include <sys/stat.h>

#include <ctype.h>
#include <err.h>
#include <errno.h>
#include <grp.h>
#include <limits.h>
#include <pwd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

#define DEF_FORMAT \
	"%d %i %Sp %l %Su %Sg %r %z \"%Sa\" \"%Sm\" \"%Sc\" " \
	"%k %b %#Xf %N"
#define RAW_FORMAT	"%d %i %#p %l %u %g %r %z %a %m %c " \
	"%k %b %f %N"
#define LS_FORMAT	"%Sp %l %Su %Sg %Z %Sm %N%SY"
#define LSF_FORMAT	"%Sp %l %Su %Sg %Z %Sm %N%T%SY"
#define SHELL_FORMAT \
	"st_dev=%d st_ino=%i st_mode=%#p st_nlink=%l " \
	"st_uid=%u st_gid=%g st_rdev=%r st_size=%z " \
	"st_atime=%a st_mtime=%m st_ctime=%c " \
	"st_blksize=%k st_blocks=%b st_flags=%f"
#define LINUX_FORMAT \
	"  File: \"%N\"%n" \
	"  Size: %-11z  FileType: %HT%n" \
	"  Mode: (%01Mp%03OLp/%.10Sp)         Uid: (%5u/%8Su)  Gid: (%5g/%8Sg)%n" \
	"Device: %Hd,%Ld   Inode: %i    Links: %l%n" \
	"Access: %Sa%n" \
	"Modify: %Sm%n" \
	"Change: %Sc"

#define TIME_FORMAT	"%b %e %T %Y"

#define FLAG_POUND	0x01
#define FLAG_SPACE	0x02
#define FLAG_PLUS	0x04
#define FLAG_ZERO	0x08
#define FLAG_MINUS	0x10

/*
 * These format characters must all be unique, except the magic one.
 */
#define FMT_MAGIC	'%'
#define FMT_DOT		'.'

#define SIMPLE_NEWLINE	'n'
#define SIMPLE_TAB	't'
#define SIMPLE_PERCENT	'%'
#define SIMPLE_NUMBER	'@'

#define FMT_POUND	'#'
#define FMT_SPACE	' '
#define FMT_PLUS	'+'
#define FMT_ZERO	'0'
#define FMT_MINUS	'-'

#define FMT_DECIMAL	'D'
#define FMT_OCTAL	'O'
#define FMT_UNSIGNED	'U'
#define FMT_HEX		'X'
#define FMT_FLOAT	'F'
#define FMT_STRING	'S'

#define FMTF_DECIMAL	0x01
#define FMTF_OCTAL	0x02
#define FMTF_UNSIGNED	0x04
#define FMTF_HEX	0x08
#define FMTF_FLOAT	0x10
#define FMTF_STRING	0x20

#define HIGH_PIECE	'H'
#define MIDDLE_PIECE	'M'
#define LOW_PIECE	'L'

#define SHOW_st_dev	'd'
#define SHOW_st_ino	'i'
#define SHOW_st_mode	'p'
#define SHOW_st_nlink	'l'
#define SHOW_st_uid	'u'
#define SHOW_st_gid	'g'
#define SHOW_st_rdev	'r'
#define SHOW_st_atime	'a'
#define SHOW_st_mtime	'm'
#define SHOW_st_ctime	'c'
#define SHOW_st_btime	'B'
#define SHOW_st_size	'z'
#define SHOW_st_blocks	'b'
#define SHOW_st_blksize	'k'
#define SHOW_st_flags	'f'
#define SHOW_st_gen	'v'
#define SHOW_symlink	'Y'
#define SHOW_filetype	'T'
#define SHOW_filename	'N'
#define SHOW_sizerdev	'Z'

void	usage(const char *);
void	output(const struct stat *, const char *,
	    const char *, int, int);
int	format1(const struct stat *,	/* stat info */
	    const char *,		/* the file name */
	    const char *, int,		/* the format string itself */
	    char *, size_t,		/* a place to put the output */
	    int, int, int, int,		/* the parsed format */
	    int, int);

char *timefmt;

#define addchar(s, c, nl) \
	do { \
		(void)fputc((c), (s)); \
		(*nl) = ((c) == '\n'); \
	} while (0/*CONSTCOND*/)

extern char *__progname;

int
main(int argc, char *argv[])
{
	struct stat st;
	int ch, rc, errs;
	int lsF, fmtchar, usestat, fn, nonl, quiet;
	char *statfmt, *options, *synopsis;

	if (pledge("stdio rpath getpw", NULL) == -1)
		err(1, "pledge");

	lsF = 0;
	fmtchar = '\0';
	usestat = 0;
	nonl = 0;
	quiet = 0;
	statfmt = NULL;
	timefmt = NULL;

	options = "f:FlLnqrst:x";
	synopsis = "[-FLnq] [-f format | -l | -r | -s | -x] "
	    "[-t timefmt] [file ...]";

	while ((ch = getopt(argc, argv, options)) != -1)
		switch (ch) {
		case 'F':
			lsF = 1;
			break;
		case 'L':
			usestat = 1;
			break;
		case 'n':
			nonl = 1;
			break;
		case 'q':
			quiet = 1;
			break;
		case 'f':
			statfmt = optarg;
			/* FALLTHROUGH */
		case 'l':
		case 'r':
		case 's':
		case 'x':
			if (fmtchar != 0)
				errx(1, "can't use format '%c' with '%c'",
				    fmtchar, ch);
			fmtchar = ch;
			break;
		case 't':
			timefmt = optarg;
			break;
		default:
			usage(synopsis);
		}

	argc -= optind;
	argv += optind;
	fn = 1;

	if (fmtchar == '\0') {
		if (lsF)
			fmtchar = 'l';
		else {
			fmtchar = 'f';
			statfmt = DEF_FORMAT;
		}
	}

	if (lsF && fmtchar != 'l')
		errx(1, "can't use format '%c' with -F", fmtchar);

	switch (fmtchar) {
	case 'f':
		/* statfmt already set */
		break;
	case 'l':
		statfmt = lsF ? LSF_FORMAT : LS_FORMAT;
		break;
	case 'r':
		statfmt = RAW_FORMAT;
		break;
	case 's':
		statfmt = SHELL_FORMAT;
		break;
	case 'x':
		statfmt = LINUX_FORMAT;
		if (timefmt == NULL)
			timefmt = "%c";
		break;
	default:
		usage(synopsis);
		/*NOTREACHED*/
	}

	if (timefmt == NULL)
		timefmt = TIME_FORMAT;

	errs = 0;
	do {
		if (argc == 0)
			rc = fstat(STDIN_FILENO, &st);
		else if (usestat) {
			/*
			 * Try stat() and if it fails, fall back to
			 * lstat() just in case we're examining a
			 * broken symlink.
			 */
			if ((rc = stat(argv[0], &st)) == -1 &&
			    errno == ENOENT &&
			    (rc = lstat(argv[0], &st)) == -1)
				errno = ENOENT;
		} else
			rc = lstat(argv[0], &st);

		if (rc == -1) {
			errs = 1;
			if (!quiet)
				warn("%s",
				    argc == 0 ? "(stdin)" : argv[0]);
		} else
			output(&st, argv[0], statfmt, fn, nonl);

		argv++;
		argc--;
		fn++;
	} while (argc > 0);

	return (errs);
}

void
usage(const char *synopsis)
{

	(void)fprintf(stderr, "usage: %s %s\n", __progname, synopsis);
	exit(1);
}

/*
 * Parses a format string.
 */
void
output(const struct stat *st, const char *file,
    const char *statfmt, int fn, int nonl)
{
	int flags, size, prec, ofmt, hilo, what;
	char buf[PATH_MAX + 4 + 1];
	const char *subfmt;
	int nl, t, i;

	nl = 1;
	while (*statfmt != '\0') {

		/*
		 * Non-format characters go straight out.
		 */
		if (*statfmt != FMT_MAGIC) {
			addchar(stdout, *statfmt, &nl);
			statfmt++;
			continue;
		}

		/*
		 * The current format "substring" starts here,
		 * and then we skip the magic.
		 */
		subfmt = statfmt;
		statfmt++;

		/*
		 * Some simple one-character "formats".
		 */
		switch (*statfmt) {
		case SIMPLE_NEWLINE:
			addchar(stdout, '\n', &nl);
			statfmt++;
			continue;
		case SIMPLE_TAB:
			addchar(stdout, '\t', &nl);
			statfmt++;
			continue;
		case SIMPLE_PERCENT:
			addchar(stdout, '%', &nl);
			statfmt++;
			continue;
		case SIMPLE_NUMBER: {
			char num[12], *p;

			snprintf(num, sizeof(num), "%d", fn);
			for (p = &num[0]; *p; p++)
				addchar(stdout, *p, &nl);
			statfmt++;
			continue;
		}
		}

		/*
		 * This must be an actual format string.  Format strings are
		 * similar to printf(3) formats up to a point, and are of
		 * the form:
		 *
		 *	%	required start of format
		 *	[-# +0]	opt. format characters
		 *	size	opt. field width
		 *	.	opt. decimal separator, followed by
		 *	prec	opt. precision
		 *	fmt	opt. output specifier (string, numeric, etc.)
		 *	sub	opt. sub field specifier (high, middle, low)
		 *	datum	required field specifier (size, mode, etc)
		 *
		 * Only the % and the datum selector are required.  All data
		 * have reasonable default output forms.  The "sub" specifier
		 * only applies to certain data (mode, dev, rdev, filetype).
		 * The symlink output defaults to STRING, yet will only emit
		 * the leading " -> " if STRING is explicitly specified.  The
		 * sizerdev datum will generate rdev output for character or
		 * block devices, and size output for all others.
		 */
		flags = 0;
		do {
			if (*statfmt == FMT_POUND)
				flags |= FLAG_POUND;
			else if (*statfmt == FMT_SPACE)
				flags |= FLAG_SPACE;
			else if (*statfmt == FMT_PLUS)
				flags |= FLAG_PLUS;
			else if (*statfmt == FMT_ZERO)
				flags |= FLAG_ZERO;
			else if (*statfmt == FMT_MINUS)
				flags |= FLAG_MINUS;
			else
				break;
			statfmt++;
		} while (1/*CONSTCOND*/);

		size = -1;
		if (isdigit((unsigned char)*statfmt)) {
			size = 0;
			while (isdigit((unsigned char)*statfmt)) {
				size = (size * 10) + (*statfmt - '0');
				statfmt++;
				if (size < 0)
					goto badfmt;
			}
		}

		prec = -1;
		if (*statfmt == FMT_DOT) {
			statfmt++;

			prec = 0;
			while (isdigit((unsigned char)*statfmt)) {
				prec = (prec * 10) + (*statfmt - '0');
				statfmt++;
				if (prec < 0)
					goto badfmt;
			}
		}

#define fmtcase(x, y)		case (y): (x) = (y); statfmt++; break
#define fmtcasef(x, y, z)	case (y): (x) = (z); statfmt++; break
		switch (*statfmt) {
			fmtcasef(ofmt, FMT_DECIMAL,	FMTF_DECIMAL);
			fmtcasef(ofmt, FMT_OCTAL,	FMTF_OCTAL);
			fmtcasef(ofmt, FMT_UNSIGNED,	FMTF_UNSIGNED);
			fmtcasef(ofmt, FMT_HEX,		FMTF_HEX);
			fmtcasef(ofmt, FMT_FLOAT,	FMTF_FLOAT);
			fmtcasef(ofmt, FMT_STRING,	FMTF_STRING);
		default:
			ofmt = 0;
			break;
		}

		switch (*statfmt) {
			fmtcase(hilo, HIGH_PIECE);
			fmtcase(hilo, MIDDLE_PIECE);
			fmtcase(hilo, LOW_PIECE);
		default:
			hilo = 0;
			break;
		}

		switch (*statfmt) {
			fmtcase(what, SHOW_st_dev);
			fmtcase(what, SHOW_st_ino);
			fmtcase(what, SHOW_st_mode);
			fmtcase(what, SHOW_st_nlink);
			fmtcase(what, SHOW_st_uid);
			fmtcase(what, SHOW_st_gid);
			fmtcase(what, SHOW_st_rdev);
			fmtcase(what, SHOW_st_atime);
			fmtcase(what, SHOW_st_mtime);
			fmtcase(what, SHOW_st_ctime);
			fmtcase(what, SHOW_st_btime);
			fmtcase(what, SHOW_st_size);
			fmtcase(what, SHOW_st_blocks);
			fmtcase(what, SHOW_st_blksize);
			fmtcase(what, SHOW_st_flags);
			fmtcase(what, SHOW_st_gen);
			fmtcase(what, SHOW_symlink);
			fmtcase(what, SHOW_filetype);
			fmtcase(what, SHOW_filename);
			fmtcase(what, SHOW_sizerdev);
		default:
			goto badfmt;
		}
#undef fmtcasef
#undef fmtcase

		t = format1(st, file, subfmt, statfmt - subfmt, buf,
		    sizeof(buf), flags, size, prec, ofmt, hilo, what);

		for (i = 0; i < t && i < sizeof(buf) - 1; i++)
			addchar(stdout, buf[i], &nl);

		continue;

	badfmt:
		errx(1, "%.*s: bad format",
		    (int)(statfmt - subfmt + 1), subfmt);
	}

	if (!nl && !nonl)
		(void)fputc('\n', stdout);
	(void)fflush(stdout);
}

/*
 * Arranges output according to a single parsed format substring.
 */
int
format1(const struct stat *st,
    const char *file,
    const char *fmt, int flen,
    char *buf, size_t blen,
    int flags, int size, int prec, int ofmt,
    int hilo, int what)
{
	u_int64_t data;
	char *sdata, lfmt[24], tmp[20];
	char smode[12], sid[12], path[PATH_MAX + 4];
	struct passwd *pw;
	struct group *gr;
	struct tm *tm;
	time_t secs;
	long nsecs;
	int l, small, formats, gottime, n;

	formats = 0;
	small = 0;
	gottime = 0;
	secs = 0;
	nsecs = 0;

	/*
	 * First, pick out the data and tweak it based on hilo or
	 * specified output format (symlink output only).
	 */
	switch (what) {
	case SHOW_st_dev:
	case SHOW_st_rdev:
		small = (sizeof(st->st_dev) == 4);
		data = (what == SHOW_st_dev) ? st->st_dev : st->st_rdev;
		sdata = (what == SHOW_st_dev) ?
		    devname(st->st_dev, S_IFBLK) :
		    devname(st->st_rdev,
		    S_ISCHR(st->st_mode) ? S_IFCHR :
		    S_ISBLK(st->st_mode) ? S_IFBLK :
		    0U);
		if (sdata == NULL)
			sdata = "???";
		if (hilo == HIGH_PIECE) {
			data = major(data);
			hilo = 0;
		} else if (hilo == LOW_PIECE) {
			data = minor((unsigned)data);
			hilo = 0;
		}
		formats = FMTF_DECIMAL | FMTF_OCTAL | FMTF_UNSIGNED | FMTF_HEX |
		    FMTF_STRING;
		if (ofmt == 0)
			ofmt = FMTF_UNSIGNED;
		break;
	case SHOW_st_ino:
		small = (sizeof(st->st_ino) == 4);
		data = st->st_ino;
		sdata = NULL;
		formats = FMTF_DECIMAL | FMTF_OCTAL | FMTF_UNSIGNED | FMTF_HEX;
		if (ofmt == 0)
			ofmt = FMTF_UNSIGNED;
		break;
	case SHOW_st_mode:
		small = (sizeof(st->st_mode) == 4);
		data = st->st_mode;
		strmode(st->st_mode, smode);
		sdata = smode;
		l = strlen(sdata);
		if (sdata[l - 1] == ' ')
			sdata[--l] = '\0';
		if (hilo == HIGH_PIECE) {
			data >>= 12;
			sdata += 1;
			sdata[3] = '\0';
			hilo = 0;
		} else if (hilo == MIDDLE_PIECE) {
			data = (data >> 9) & 07;
			sdata += 4;
			sdata[3] = '\0';
			hilo = 0;
		} else if (hilo == LOW_PIECE) {
			data &= 0777;
			sdata += 7;
			sdata[3] = '\0';
			hilo = 0;
		}
		formats = FMTF_DECIMAL | FMTF_OCTAL | FMTF_UNSIGNED | FMTF_HEX |
		    FMTF_STRING;
		if (ofmt == 0)
			ofmt = FMTF_OCTAL;
		break;
	case SHOW_st_nlink:
		small = (sizeof(st->st_dev) == 4);
		data = st->st_nlink;
		sdata = NULL;
		formats = FMTF_DECIMAL | FMTF_OCTAL | FMTF_UNSIGNED | FMTF_HEX;
		if (ofmt == 0)
			ofmt = FMTF_UNSIGNED;
		break;
	case SHOW_st_uid:
		small = (sizeof(st->st_uid) == 4);
		data = st->st_uid;
		if ((pw = getpwuid(st->st_uid)) != NULL)
			sdata = pw->pw_name;
		else {
			snprintf(sid, sizeof(sid), "(%ld)", (long)st->st_uid);
			sdata = sid;
		}
		formats = FMTF_DECIMAL | FMTF_OCTAL | FMTF_UNSIGNED | FMTF_HEX |
		    FMTF_STRING;
		if (ofmt == 0)
			ofmt = FMTF_UNSIGNED;
		break;
	case SHOW_st_gid:
		small = (sizeof(st->st_gid) == 4);
		data = st->st_gid;
		if ((gr = getgrgid(st->st_gid)) != NULL)
			sdata = gr->gr_name;
		else {
			snprintf(sid, sizeof(sid), "(%ld)", (long)st->st_gid);
			sdata = sid;
		}
		formats = FMTF_DECIMAL | FMTF_OCTAL | FMTF_UNSIGNED | FMTF_HEX |
		    FMTF_STRING;
		if (ofmt == 0)
			ofmt = FMTF_UNSIGNED;
		break;
	case SHOW_st_atime:
		gottime = 1;
		secs = st->st_atime;
		nsecs = st->st_atimensec;
		/* FALLTHROUGH */
	case SHOW_st_mtime:
		if (!gottime) {
			gottime = 1;
			secs = st->st_mtime;
			nsecs = st->st_mtimensec;
		}
		/* FALLTHROUGH */
	case SHOW_st_ctime:
		if (!gottime) {
			gottime = 1;
			secs = st->st_ctime;
			nsecs = st->st_ctimensec;
		}
		/* FALLTHROUGH */
	case SHOW_st_btime:
		if (!gottime) {
			gottime = 1;
			secs = st->__st_birthtimespec.tv_sec;
			nsecs = st->__st_birthtimespec.tv_nsec;
		}
		small = (sizeof(secs) == 4);
		data = secs;
		small = 1;
		tm = localtime(&secs);
		(void)strftime(path, sizeof(path), timefmt, tm);
		sdata = path;
		formats = FMTF_DECIMAL | FMTF_OCTAL | FMTF_UNSIGNED | FMTF_HEX |
		    FMTF_FLOAT | FMTF_STRING;
		if (ofmt == 0)
			ofmt = FMTF_DECIMAL;
		break;
	case SHOW_st_size:
		small = (sizeof(st->st_size) == 4);
		data = st->st_size;
		sdata = NULL;
		formats = FMTF_DECIMAL | FMTF_OCTAL | FMTF_UNSIGNED | FMTF_HEX;
		if (ofmt == 0)
			ofmt = FMTF_UNSIGNED;
		break;
	case SHOW_st_blocks:
		small = (sizeof(st->st_blocks) == 4);
		data = st->st_blocks;
		sdata = NULL;
		formats = FMTF_DECIMAL | FMTF_OCTAL | FMTF_UNSIGNED | FMTF_HEX;
		if (ofmt == 0)
			ofmt = FMTF_UNSIGNED;
		break;
	case SHOW_st_blksize:
		small = (sizeof(st->st_blksize) == 4);
		data = st->st_blksize;
		sdata = NULL;
		formats = FMTF_DECIMAL | FMTF_OCTAL | FMTF_UNSIGNED | FMTF_HEX;
		if (ofmt == 0)
			ofmt = FMTF_UNSIGNED;
		break;
	case SHOW_st_flags:
		small = (sizeof(st->st_flags) == 4);
		data = st->st_flags;
		sdata = NULL;
		formats = FMTF_DECIMAL | FMTF_OCTAL | FMTF_UNSIGNED | FMTF_HEX;
		if (ofmt == 0)
			ofmt = FMTF_UNSIGNED;
		break;
	case SHOW_st_gen:
		small = (sizeof(st->st_gen) == 4);
		data = st->st_gen;
		sdata = NULL;
		formats = FMTF_DECIMAL | FMTF_OCTAL | FMTF_UNSIGNED | FMTF_HEX;
		if (ofmt == 0)
			ofmt = FMTF_UNSIGNED;
		break;
	case SHOW_symlink:
		small = 0;
		data = 0;
		if (S_ISLNK(st->st_mode)) {
			snprintf(path, sizeof(path), " -> ");
			l = readlink(file, path + 4, sizeof(path) - 4 - 1);
			if (l == -1) {
				l = 0;
				path[0] = '\0';
			}
			path[l + 4] = '\0';
			sdata = path + (ofmt == FMTF_STRING ? 0 : 4);
		} else
			sdata = "";

		formats = FMTF_STRING;
		if (ofmt == 0)
			ofmt = FMTF_STRING;
		break;
	case SHOW_filetype:
		small = 0;
		data = 0;
		sdata = smode;
		sdata[0] = '\0';
		if (hilo == 0 || hilo == LOW_PIECE) {
			switch (st->st_mode & S_IFMT) {
			case S_IFIFO:
				(void)strlcat(sdata, "|", sizeof(smode));
				break;
			case S_IFDIR:
				(void)strlcat(sdata, "/", sizeof(smode));
				break;
			case S_IFREG:
				if (st->st_mode & (S_IXUSR | S_IXGRP | S_IXOTH))
					(void)strlcat(sdata, "*",
					    sizeof(smode));
				break;
			case S_IFLNK:
				(void)strlcat(sdata, "@", sizeof(smode));
				break;
			case S_IFSOCK:
				(void)strlcat(sdata, "=", sizeof(smode));
				break;
			}
			hilo = 0;
		} else if (hilo == HIGH_PIECE) {
			switch (st->st_mode & S_IFMT) {
			case S_IFIFO:	sdata = "Fifo File";		break;
			case S_IFCHR:	sdata = "Character Device";	break;
			case S_IFDIR:	sdata = "Directory";		break;
			case S_IFBLK:	sdata = "Block Device";		break;
			case S_IFREG:	sdata = "Regular File";		break;
			case S_IFLNK:	sdata = "Symbolic Link";	break;
			case S_IFSOCK:	sdata = "Socket";		break;
			default:	sdata = "???";			break;
			}
			hilo = 0;
		}
		formats = FMTF_STRING;
		if (ofmt == 0)
			ofmt = FMTF_STRING;
		break;
	case SHOW_filename:
		small = 0;
		data = 0;
		if (file == NULL)
			(void)strlcpy(path, "(stdin)", sizeof(path));
		else
			(void)strlcpy(path, file, sizeof(path));
		sdata = path;
		formats = FMTF_STRING;
		if (ofmt == 0)
			ofmt = FMTF_STRING;
		break;
	case SHOW_sizerdev:
		if (S_ISCHR(st->st_mode) || S_ISBLK(st->st_mode)) {
			char majdev[20], mindev[20];
			int l1, l2;

			l1 = format1(st, file, fmt, flen,
			    majdev, sizeof(majdev), flags, size, prec,
			    ofmt, HIGH_PIECE, SHOW_st_rdev);
			l2 = format1(st, file, fmt, flen,
			    mindev, sizeof(mindev), flags, size, prec,
			    ofmt, LOW_PIECE, SHOW_st_rdev);
			n = snprintf(buf, blen, "%.*s,%.*s",
			    l1, majdev, l2, mindev);
			return (n >= blen ? blen : n);
		} else {
			return (format1(st, file, fmt, flen, buf, blen,
			    flags, size, prec, ofmt, 0, SHOW_st_size));
		}
		/*NOTREACHED*/
	default:
		errx(1, "%.*s: bad format", (int)flen, fmt);
	}

	/*
	 * If a subdatum was specified but not supported, or an output
	 * format was selected that is not supported, that's an error.
	 */
	if (hilo != 0 || (ofmt & formats) == 0)
		errx(1, "%.*s: bad format", (int)flen, fmt);

	/*
	 * Assemble the format string for passing to printf(3).
	 */
	lfmt[0] = '\0';
	(void)strlcat(lfmt, "%", sizeof(lfmt));
	if (flags & FLAG_POUND)
		(void)strlcat(lfmt, "#", sizeof(lfmt));
	if (flags & FLAG_SPACE)
		(void)strlcat(lfmt, " ", sizeof(lfmt));
	if (flags & FLAG_PLUS)
		(void)strlcat(lfmt, "+", sizeof(lfmt));
	if (flags & FLAG_MINUS)
		(void)strlcat(lfmt, "-", sizeof(lfmt));
	if (flags & FLAG_ZERO)
		(void)strlcat(lfmt, "0", sizeof(lfmt));

	/*
	 * Only the timespecs support the FLOAT output format, and that
	 * requires work that differs from the other formats.
	 */
	if (ofmt == FMTF_FLOAT) {
		/*
		 * Nothing after the decimal point, so just print seconds.
		 */
		if (prec == 0) {
			if (size != -1) {
				(void)snprintf(tmp, sizeof(tmp), "%d", size);
				(void)strlcat(lfmt, tmp, sizeof(lfmt));
			}
			(void)strlcat(lfmt, "d", sizeof(lfmt));
			n = snprintf(buf, blen, lfmt, secs);
			return (n >= blen ? blen : n);
		}

		/*
		 * Unspecified precision gets all the precision we have:
		 * 9 digits.
		 */
		if (prec == -1)
			prec = 9;

		/*
		 * Adjust the size for the decimal point and the digits
		 * that will follow.
		 */
		size -= prec + 1;

		/*
		 * Any leftover size that's legitimate will be used.
		 */
		if (size > 0) {
			(void)snprintf(tmp, sizeof(tmp), "%d", size);
			(void)strlcat(lfmt, tmp, sizeof(lfmt));
		}
		(void)strlcat(lfmt, "d", sizeof(lfmt));

		/*
		 * The stuff after the decimal point always needs zero
		 * filling.
		 */
		(void)strlcat(lfmt, ".%0", sizeof(lfmt));

		/*
		 * We can "print" at most nine digits of precision.  The
		 * rest we will pad on at the end.
		 */
		(void)snprintf(tmp, sizeof(tmp), "%dd", prec > 9 ? 9 : prec);
		(void)strlcat(lfmt, tmp, sizeof(lfmt));

		/*
		 * For precision of less that nine digits, trim off the
		 * less significant figures.
		 */
		for (; prec < 9; prec++)
			nsecs /= 10;

		/*
		 * Use the format, and then tack on any zeroes that
		 * might be required to make up the requested precision.
		 */
		l = snprintf(buf, blen, lfmt, secs, nsecs);
		if (l >= blen)
			return (l);
		for (; prec > 9 && l < blen; prec--, l++)
			(void)strlcat(buf, "0", blen);
		return (l);
	}

	/*
	 * Add on size and precision, if specified, to the format.
	 */
	if (size != -1) {
		(void)snprintf(tmp, sizeof(tmp), "%d", size);
		(void)strlcat(lfmt, tmp, sizeof(lfmt));
	}
	if (prec != -1) {
		(void)snprintf(tmp, sizeof(tmp), ".%d", prec);
		(void)strlcat(lfmt, tmp, sizeof(lfmt));
	}

	/*
	 * String output uses the temporary sdata.
	 */
	if (ofmt == FMTF_STRING) {
		if (sdata == NULL)
			errx(1, "%.*s: bad format", (int)flen, fmt);
		(void)strlcat(lfmt, "s", sizeof(lfmt));
		n = snprintf(buf, blen, lfmt, sdata);
		return (n >= blen ? blen : n);
	}

	/*
	 * Ensure that sign extension does not cause bad looking output
	 * for some forms.
	 */
	if (small && ofmt != FMTF_DECIMAL)
		data = (u_int32_t)data;

	/*
	 * The four "numeric" output forms.
	 */
	(void)strlcat(lfmt, "ll", sizeof(lfmt));
	switch (ofmt) {
	case FMTF_DECIMAL:	(void)strlcat(lfmt, "d", sizeof(lfmt));	break;
	case FMTF_OCTAL:	(void)strlcat(lfmt, "o", sizeof(lfmt));	break;
	case FMTF_UNSIGNED:	(void)strlcat(lfmt, "u", sizeof(lfmt));	break;
	case FMTF_HEX:		(void)strlcat(lfmt, "x", sizeof(lfmt));	break;
	}

	n = snprintf(buf, blen, lfmt, data);
	return (n >= blen ? blen : n);
}
