/*	$OpenBSD: main.c,v 1.19 2002/12/17 16:16:08 millert Exp $	*/

static const char copyright[] =
"@(#) Copyright (c) 1992, 1993\n\
	The Regents of the University of California.  All rights reserved.\n"
"Copyright (c) 1997-2002 Michael Shalayeff\n";

static const char license[] =
"\n"
" Redistribution and use in source and binary forms, with or without\n"
" modification, are permitted provided that the following conditions\n"
" are met:\n"
" 1. Redistributions of source code must retain the above copyright\n"
"    notice, this list of conditions and the following disclaimer.\n"
" 2. Redistributions in binary form must reproduce the above copyright\n"
"    notice, this list of conditions and the following disclaimer in the\n"
"    documentation and/or other materials provided with the distribution.\n"
" 3. All advertising materials mentioning features or use of this software\n"
"    must display the following acknowledgement:\n"
"      This product includes software developed by the University of\n"
"      California, Berkeley and its contributors.\n"
"\n"
" THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR\n"
" IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES\n"
" OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.\n"
" IN NO EVENT SHALL THE AUTHOR OR HIS RELATIVES BE LIABLE FOR ANY DIRECT,\n"
" INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES\n"
" (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR\n"
" SERVICES; LOSS OF MIND, USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)\n"
" HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT,\n"
" STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING\n"
" IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF\n"
" THE POSSIBILITY OF SUCH DAMAGE.\n";

#ifndef lint
#if 0
static char sccsid[] = "@(#)compress.c	8.2 (Berkeley) 1/7/94";
#else
static const char main_rcsid[] = "$OpenBSD: main.c,v 1.19 2002/12/17 16:16:08 millert Exp $";
#endif
#endif /* not lint */

#include <sys/param.h>
#include <sys/time.h>
#include <sys/stat.h>

#include <getopt.h>
#include <err.h>
#include <errno.h>
#include <fts.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <paths.h>
#include "compress.h"

#define min(a,b) ((a) < (b)? (a) : (b))

int pipin, force, verbose, testmode, list, nosave;
int savename, recurse;
int bits, cat, decomp;
extern char *__progname;

const struct compressor {
	char *name;
	char *suffix;
	int (*check_header)(int, struct stat *, const char *);
	void *(*open)(int, const char *, int);
	int (*read)(void *, char *, int);
	int (*write)(void *, const char *, int);
	int (*close)(void *);
} c_table[] = {
#define M_COMPRESS (&c_table[0])
  { "compress", ".Z", z_check_header,  z_open,  zread,   zwrite,   zclose },
#define M_DEFLATE (&c_table[1])
  { "deflate", ".gz", gz_check_header, gz_open, gz_read, gz_write, gz_close },
#if 0
#define M_LZH (&c_table[2])
  { "lzh", ".lzh", lzh_check_header, lzh_open, lzh_read, lzh_write, lzh_close },
#define M_ZIP (&c_table[3])
  { "zip", ".zip", zip_check_header, zip_open, zip_read, zip_write, zip_close },
#define M_PACK (&c_table[4])
  { "pack", ".pak",pak_check_header, pak_open, pak_read, pak_write, pak_close },
#endif
  { NULL }
};

int permission(const char *);
void setfile(const char *, struct stat *);
void usage(void);
int compress(const char *, const char *, const struct compressor *, int, struct stat *);
int decompress(const char *, const char *, const struct compressor *, int, struct stat *);
const struct compressor *check_method(int, struct stat *, const char *);

#define	OPTSTRING	"123456789ab:cdfghlLnNOo:qrS:tvV"
const struct option longopts[] = {
	{ "ascii",	no_argument,		0, 'a' },
	{ "stdout",	no_argument,		0, 'c' },
	{ "to-stdout",	no_argument,		0, 'c' },
	{ "decompress",	no_argument,		0, 'd' },
	{ "uncompress",	no_argument,		0, 'd' },
	{ "force",	no_argument,		0, 'f' },
	{ "help",	no_argument,		0, 'h' },
	{ "list",	no_argument,		0, 'l' },
	{ "license",	no_argument,		0, 'L' },
	{ "no-name",	no_argument,		0, 'n' },
	{ "name",	no_argument,		0, 'N' },
	{ "quiet",	no_argument,		0, 'q' },
	{ "recursive",	no_argument,		0, 'r' },
	{ "suffix",	required_argument,	0, 'S' },
	{ "test",	no_argument,		0, 't' },
	{ "verbose",	no_argument,		0, 'v' },
	{ "version",	no_argument,		0, 'V' },
	{ "fast",	no_argument,		0, '1' },
	{ "best",	no_argument,		0, '9' },
	{ NULL }
};

int
main(argc, argv)
	int argc;
	char *argv[];
{
	FTS *ftsp;
	FTSENT *entry;
	struct stat osb;
	const struct compressor *method;
	char *p, *s, *infile;
	char outfile[MAXPATHLEN], _infile[MAXPATHLEN], suffix[16];
	char *nargv[512];	/* some estimate based on ARG_MAX */
	int exists, oreg, ch, error, i, rc, oflag;

	bits = cat = oflag = decomp = 0;
	p = __progname;
	if (p[0] == 'g') {
		method = M_DEFLATE;
		bits = 6;
		p++;
	} else
		method = M_COMPRESS;

	decomp = 0;
	if (!strcmp(p, "zcat")) {
		decomp++;
		cat++;
	} else {
		if (p[0] == 'u' && p[1] == 'n') {
			p += 2;
			decomp++;
		}

		if (strcmp(p, "zip") &&
		    strcmp(p, "compress"))
			errx(1, "unknown program name");
	}

	strlcpy(suffix, method->suffix, sizeof(suffix));

	nargv[0] = NULL;
	if ((s = getenv("GZIP")) != NULL) {
		char *last;

		nargv[0] = *argv++;
		for (i = 1, (p = strtok_r(s, " ", &last)); p;
		    (p = strtok_r(NULL, " ", &last)), i++)
			if (i < sizeof(nargv)/sizeof(nargv[1]) - argc - 1)
				nargv[i] = p;
			else {
				errx(1, "GZIP is too long");
			}
		argc += i - 1;
		while ((nargv[i++] = *argv++))
			;
		argv = nargv;
	}

	while ((ch = getopt_long(argc, argv, OPTSTRING, longopts, NULL)) != -1)
		switch(ch) {
		case '1':
		case '2':
		case '3':
		case '4':
		case '5':
		case '6':
		case '7':
		case '8':
		case '9':
			method = M_DEFLATE;
			strlcpy(suffix, method->suffix, sizeof(suffix));
			bits = ch - '0';
			break;
		case 'a':
			warnx("option -a is ignored on this system");
			break;
		case 'b':
			bits = strtol(optarg, &p, 10);
			/*
			 * POSIX 1002.3 says 9 <= bits <= 14 for portable
			 * apps, but says the implementation may allow
			 * greater.
			 */
			if (*p)
				errx(1, "illegal bit count -- %s", optarg);
			break;
		case 'c':
			cat++;
			break;
		case 'd':		/* Backward compatible. */
			decomp++;
			break;
		case 'f':
			force++;
			break;
		case 'g':
			method = M_DEFLATE;
			strlcpy(suffix, method->suffix, sizeof(suffix));
			bits = 6;
			break;
		case 'l':
			list++;
			break;
		case 'n':
			nosave++;
			break;
		case 'N':
			nosave = 0;	/* XXX not yet */
			break;
		case 'O':
			method = M_COMPRESS;
			strlcpy(suffix, method->suffix, sizeof(suffix));
			break;
		case 'o':
			if (strlcpy(outfile, optarg,
			    sizeof(outfile)) >= sizeof(outfile))
				errx(1, "-o argument is too long");
			oflag++;
			break;
		case 'q':
			verbose = -1;
			break;
		case 'S':
			p = suffix;
			if (optarg[0] != '.')
				*p++ = '.';
			strlcpy(p, optarg, sizeof(suffix) - (p - suffix));
			p = optarg;
			break;
		case 't':
			testmode++;
			break;
		case 'V':
			printf("%s\n%s\n%s\n", main_rcsid,
			    z_rcsid, gz_rcsid);
			exit (0);
		case 'v':
			verbose++;
			break;
		case 'L':
			fputs(copyright, stderr);
			fputs(license, stderr);
			exit (0);
		case 'r':
			recurse++;
			break;

		case 'h':
		case '?':
		default:
			usage();
		}
	argc -= optind;
	argv += optind;

	if (argc == 0) {
		if (nargv[0] == NULL)
			argv = nargv;
		/* XXX - make sure we don't oflow nargv in $GZIP case (millert) */
		argv[0] = "/dev/stdin";
		argv[1] = NULL;
		pipin++;
		cat++;
	}
	if (oflag && (recurse || argc > 1))
		errx(1, "-o option may only be used with a single input file");
	if (cat + testmode + oflag > 1)
		errx(1, "may not mix -o, -c, or -t options");

	if ((ftsp = fts_open(argv, FTS_PHYSICAL|FTS_NOCHDIR, 0)) == NULL)
		err(1, NULL);
	/* XXX - set rc in cases where we "continue" below? */
	for (rc = 0; (entry = fts_read(ftsp)) != NULL;) {
		infile = entry->fts_path;
		switch (entry->fts_info) {
		case FTS_D:
			if (!recurse) {
				warnx("%s is a directory: ignored",
				    infile);
				fts_set(ftsp, entry, FTS_SKIP);
			}
			continue;
		case FTS_DP:
			continue;
		case FTS_NS:
			/*
			 * If file does not exist and has no suffix,
			 * tack on the default suffix and try that.
			 */
			/* XXX - is overwriting fts_statp legal? (millert) */
			if (entry->fts_errno == ENOENT &&
			    strchr(entry->fts_accpath, '.') == NULL &&
			    snprintf(_infile, sizeof(_infile), "%s%s", infile,
			    suffix) < sizeof(_infile) &&
			    stat(_infile, entry->fts_statp) == 0 &&
			    S_ISREG(entry->fts_statp->st_mode)) {
				infile = _infile;
				break;
			}
		case FTS_ERR:
		case FTS_DNR:
			warnx("%s: %s", infile, strerror(entry->fts_errno));
			error = 1;
			continue;
		default:
			if (!S_ISREG(entry->fts_statp->st_mode)) {
				warnx("%s not a regular file: unchanged",
				    infile);
				continue;
			}
			break;
		}

		if (testmode)
			strcpy(outfile, _PATH_DEVNULL);
		else if (cat)
			strcpy(outfile, "/dev/stdout");
		else if (!oflag) {
			if (decomp) {
				const struct compressor *m = method;

				if ((s = strrchr(infile, '.')) != NULL &&
				    strcmp(s, suffix) != 0) {
					for (m = &c_table[0];
					    m->name && strcmp(s, m->suffix);
					    m++)
						;
				}
				if (s == NULL || m->name == NULL) {
					if (!recurse)
						warnx("%s: unknown suffix: "
						    "ignored", infile);
					continue;
				}
				method = m;
				strlcpy(outfile, infile,
				    min(sizeof(outfile), (s - infile) + 1));
			} else {
				if (snprintf(outfile, sizeof(outfile),
				    "%s%s", infile, suffix) >= sizeof(outfile)) {
					warnx("%s%s: name too long",
					    infile, suffix);
					continue;
				}
			}
		}

		exists = !stat(outfile, &osb);
		if (!force && exists && S_ISREG(osb.st_mode) &&
		    !permission(outfile))
			continue;

		oreg = !exists || S_ISREG(osb.st_mode);

		if (verbose > 0)
			fprintf(stderr, "%s:\t", infile);

		error = (decomp ? decompress : compress)
			(infile, outfile, method, bits, entry->fts_statp);

		if (!error && !cat && !testmode && stat(outfile, &osb) == 0) {
			if (!force && !decomp &&
			    osb.st_size >= entry->fts_statp->st_size) {
				if (verbose > 0)
					fprintf(stderr, "file would grow; "
						     "left unmodified\n");
				error = 1;
				rc = rc ? rc : 2;
			} else {
				setfile(outfile, entry->fts_statp);

				if (unlink(infile) && verbose >= 0)
					warn("input: %s", infile);

				if (verbose > 0) {
					u_int ratio;
					ratio = (1000 * osb.st_size)
					    / entry->fts_statp->st_size;
					fprintf(stderr, "%u", ratio / 10);
					if (ratio % 10)
						fprintf(stderr, ".%u",
						        ratio % 10);
					fputc('%', stderr);
					fputc(' ', stderr);
				}
			}
		}

		if (error && oreg && unlink(outfile) && errno != ENOENT &&
		    verbose >= 0) {
			if (force) {
				warn("output: %s", outfile);
				rc = 1;
			} else
				err(1, "output: %s", outfile);
		} else if (!error && verbose > 0)
			fputs("OK\n", stderr);
	}

	exit(rc);
}

int
compress(in, out, method, bits, sb)
	const char *in;
	const char *out;
	const struct compressor *method;
	int bits;
	struct stat *sb;
{
	u_char buf[Z_BUFSIZE];
	int error, ifd, ofd;
	void *cookie;
	ssize_t nr;

	error = 0;
	cookie  = NULL;

	if ((ofd = open(out, O_WRONLY|O_CREAT, S_IWUSR)) < 0) {
		if (verbose >= 0)
			warn("%s", out);
		return (-1);
	}

	if (method != M_COMPRESS && !force && isatty(ofd)) {
		if (verbose >= 0)
			warnx("%s: won't write compressed data to terminal",
			      out);
		return (-1);
	}

	if ((ifd = open(in, O_RDONLY)) < 0) {
		if (verbose >= 0)
			warn("%s", out);
		return (-1);
	}

	if ((cookie = (*method->open)(ofd, "w", bits)) != NULL) {

		while ((nr = read(ifd, buf, sizeof(buf))) > 0)
			if ((method->write)(cookie, buf, nr) != nr) {
				if (verbose >= 0)
					warn("%s", out);
				error++;
				break;
			}
	}

	if (cookie == NULL || nr < 0) {
		if (!error && verbose >= 0)
			warn("%s", in);
		error++;
	}

	if (cookie == NULL || (method->close)(cookie)) {
		if (!error && verbose >= 0)
			warn("%s", out);
		error++;
		(void) close(ofd);
	}

	if (close(ifd)) {
		if (!error && verbose >= 0)
			warn("%s", out);
		error++;
	}

	return (error ? -1 : 0);
}

const struct compressor *
check_method(fd, sb, out)
	int fd;
	struct stat *sb;
	const char *out;
{
	const struct compressor *method;

	for (method = &c_table[0];
	     method->name != NULL && !(*method->check_header)(fd, sb, out);
	     method++)
		;

	if (method->name == NULL)
		method = NULL;

	return (method);
}

int
decompress(in, out, method, bits, sb)
	const char *in;
	const char *out;
	const struct compressor *method;
	int bits;
	struct stat *sb;
{
	u_char buf[Z_BUFSIZE];
	int error, ifd, ofd;
	void *cookie;
	ssize_t nr;

	error = 0;
	cookie = NULL;

	if ((ifd = open(in, O_RDONLY)) < 0) {
		if (verbose >= 0)
			warn("%s", in);
		return -1;
	}

	if (!force && isatty(ifd)) {
		if (verbose >= 0)
			warnx("%s: won't read compressed data from terminal",
			      in);
		close (ifd);
		return -1;
	}

	if (!pipin && (method = check_method(ifd, sb, out)) == NULL) {
		if (verbose >= 0)
			warnx("%s: unrecognized file format", in);
		close (ifd);
		return -1;
	}

	if ((ofd = open(out, O_WRONLY|O_CREAT|O_TRUNC, S_IWUSR)) < 0) {
		if (verbose >= 0)
			warn("%s", in);
		return -1;
	}

	if ((cookie = (*method->open)(ifd, "r", bits)) != NULL) {

		while ((nr = (method->read)(cookie, buf, sizeof(buf))) > 0)
			if (write(ofd, buf, nr) != nr) {
				if (verbose >= 0)
					warn("%s", out);
				error++;
				break;
			}
	}

	if (cookie == NULL || (method->close)(cookie) || nr < 0) {
		if (!error && verbose >= 0)
			warn("%s", in);
		error++;
		close (ifd);
	}

	if (close(ofd)) {
		if (!error && verbose >= 0)
			warn("%s", out);
		error++;
	}

	return error;
}

void
setfile(name, fs)
	const char *name;
	struct stat *fs;
{
	struct timeval tv[2];

	fs->st_mode &= S_ISUID|S_ISGID|S_IRWXU|S_IRWXG|S_IRWXO;

	TIMESPEC_TO_TIMEVAL(&tv[0], &fs->st_atimespec);
	TIMESPEC_TO_TIMEVAL(&tv[1], &fs->st_mtimespec);
	if (utimes(name, tv))
		warn("utimes: %s", name);

	/*
	 * Changing the ownership probably won't succeed, unless we're root
	 * or POSIX_CHOWN_RESTRICTED is not set.  Set uid/gid before setting
	 * the mode; current BSD behavior is to remove all setuid bits on
	 * chown.  If chown fails, lose setuid/setgid bits.
	 */
	if (chown(name, fs->st_uid, fs->st_gid)) {
		if (errno != EPERM)
			warn("chown: %s", name);
		fs->st_mode &= ~(S_ISUID|S_ISGID);
	}
	if (chmod(name, fs->st_mode))
		warn("chown: %s", name);

	if (fs->st_flags && chflags(name, fs->st_flags))
		warn("chflags: %s", name);
}

int
permission(fname)
	const char *fname;
{
	int ch, first;

	if (!isatty(fileno(stderr)))
		return (0);
	(void)fprintf(stderr, "overwrite %s? ", fname);
	first = ch = getchar();
	while (ch != '\n' && ch != EOF)
		ch = getchar();
	return (first == 'y');
}

void
usage()
{
	fprintf(stderr,
	    "usage: %s [-cdfghlnLOqrStvV] [-b <bits>] [-[0-9]] [file ...]\n",
	    __progname);
	exit(1);
}
