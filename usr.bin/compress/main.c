/*	$OpenBSD: main.c,v 1.77 2011/03/05 20:12:42 millert Exp $	*/

#ifndef SMALL
static const char copyright[] =
"@(#) Copyright (c) 1992, 1993\n\
	The Regents of the University of California.  All rights reserved.\n"
"Copyright (c) 1997-2002 Michael Shalayeff\n";
#endif

#ifndef SMALL
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
" 3. Neither the name of the University nor the names of its contributors\n"
"    may be used to endorse or promote products derived from this software\n"
"    without specific prior written permission.\n"
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
#endif /* SMALL */

#ifndef SMALL
static const char main_rcsid[] = "$OpenBSD: main.c,v 1.77 2011/03/05 20:12:42 millert Exp $";
#endif

#include <sys/param.h>
#include <sys/time.h>
#include <sys/stat.h>

#include <getopt.h>
#include <err.h>
#include <errno.h>
#include <fts.h>
#include <libgen.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <paths.h>
#include "compress.h"

#define min(a,b) ((a) < (b)? (a) : (b))

int cat, decomp, pipin, force, verbose, testmode, list, recurse, storename;
extern char *__progname;

const struct compressor {
	char *name;
	char *suffix;
	u_char *magic;
	void *(*open)(int, const char *, char *, int, u_int32_t, int);
	int (*read)(void *, char *, int);
	int (*write)(void *, const char *, int);
	int (*close)(void *, struct z_info *, const char *, struct stat *);
} c_table[] = {
#define M_DEFLATE (&c_table[0])
  { "deflate", ".gz", "\037\213", gz_open, gz_read, gz_write, gz_close },
#define M_COMPRESS (&c_table[1])
#ifndef SMALL
  { "compress", ".Z", "\037\235", z_open,  zread,   zwrite,   z_close },
#endif /* SMALL */
#if 0
#define M_LZH (&c_table[2])
  { "lzh", ".lzh", "\037\240", lzh_open, lzh_read, lzh_write, lzh_close },
#define M_ZIP (&c_table[3])
  { "zip", ".zip", "PK", zip_open, zip_read, zip_write, zip_close },
#define M_PACK (&c_table[4])
  { "pack", ".pak", "\037\036", pak_open, pak_read, pak_write, pak_close },
#endif
  { NULL }
};

#ifndef SMALL
const struct compressor null_method =
{ "null", ".nul", "XX", null_open, null_read, null_write, null_close };
#endif /* SMALL */

int permission(const char *);
__dead void usage(int);
int docompress(const char *, char *, const struct compressor *,
    int, struct stat *);
int dodecompress(const char *, char *, const struct compressor *,
    int, struct stat *);
const struct compressor *check_method(int);
const char *check_suffix(const char *);
char *set_outfile(const char *, char *, size_t);
void list_stats(const char *, const struct compressor *, struct z_info *);
void verbose_info(const char *, off_t, off_t, u_int32_t);

const struct option longopts[] = {
#ifndef SMALL
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
#endif /* SMALL */
	{ NULL }
};

int
main(int argc, char *argv[])
{
	FTS *ftsp;
	FTSENT *entry;
	const struct compressor *method;
	const char *s;
	char *p, *infile;
	char outfile[MAXPATHLEN], _infile[MAXPATHLEN], suffix[16];
	int bits, ch, error, rc, cflag, oflag;
	static const char *optstr[3] = {
		"123456789ab:cdfghLlNnOo:qrS:tVv",
		"cfhlNno:qrtVv",
		"fghqr"
	};

	bits = cflag = oflag = 0;
	storename = -1;
	p = __progname;
	if (p[0] == 'g') {
		method = M_DEFLATE;
		bits = 6;
		p++;
	} else
#ifdef SMALL
		method = M_DEFLATE;
#else
		method = M_COMPRESS;
#endif /* SMALL */

	decomp = 0;
	pmode = MODE_COMP;
	if (!strcmp(p, "zcat")) {
		decomp++;
		cflag = 1;
		pmode = MODE_CAT;
	} else {
		if (p[0] == 'u' && p[1] == 'n') {
			p += 2;
			decomp++;
			pmode = MODE_DECOMP;
		}

		if (strcmp(p, "zip") &&
		    strcmp(p, "compress"))
			errx(1, "unknown program name");
	}

	strlcpy(suffix, method->suffix, sizeof(suffix));

	if (method == M_DEFLATE && (p = getenv("GZIP")) != NULL) {
		char *evbuf, *last, **nargv = NULL;
		int argc_extra = 0, nargc = 0;

		if ((evbuf = strdup(p)) == NULL)
			err(1, NULL);
		for ((p = strtok_r(evbuf, " ", &last)); p != NULL;
		    (p = strtok_r(NULL, " ", &last))) {
			if (nargc + 1 >= argc_extra) {
				argc_extra += 1024;
				nargv = realloc(nargv,
				    (argc + argc_extra + 1) * sizeof(char *));
				if (nargv == NULL)
					err(1, NULL);
			}
			nargv[++nargc] = p;
		}
		if (nargv != NULL) {
			nargv[0] = *argv++;
			while ((nargv[++nargc] = *argv++))
				;
			argv = nargv;
			argc = nargc;
		}
	}

	while ((ch = getopt_long(argc, argv, optstr[pmode], longopts, NULL)) != -1)
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
			cflag = 1;
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
			testmode = 1;
			decomp++;
			break;
		case 'n':
			storename = 0;
			break;
		case 'N':
			storename = 1;
			break;
#ifndef SMALL
		case 'O':
			method = M_COMPRESS;
			strlcpy(suffix, method->suffix, sizeof(suffix));
			break;
#endif /* SMALL */
		case 'o':
			if (strlcpy(outfile, optarg,
			    sizeof(outfile)) >= sizeof(outfile))
				errx(1, "-o argument is too long");
			oflag = 1;
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
			testmode = 1;
			decomp++;
			break;
#ifndef SMALL
		case 'V':
			printf("%s\n%s\n", main_rcsid, gz_rcsid);
			printf("%s\n%s\n", z_rcsid, null_rcsid);
#endif
			exit (0);
		case 'v':
			verbose++;
			break;
#ifndef SMALL
		case 'L':
			fputs(copyright, stderr);
			fputs(license, stderr);
#endif
			exit (0);
		case 'r':
			recurse++;
			break;

		case 'h':
			usage(0);
			break;
		default:
			usage(1);
		}
	argc -= optind;
	argv += optind;

	if (argc == 0) {
		argv = calloc(2, sizeof(char *));
		if (argv == NULL)
			err(1, NULL);
		argv[0] = "-";
		argc = 1;
	}
	if (oflag && (recurse || argc > 1))
		errx(1, "-o option may only be used with a single input file");

	if ((cat && argc) + testmode + oflag > 1)
		errx(1, "may not mix -o, -c, or -t options");
	/*
	 * By default, when compressing store the original name and timestamp
	 * in the header.  Do not restore these when decompressing unless
	 * the -N option is given.
	 */
	if (storename == -1)
		storename = !decomp;

	if ((ftsp = fts_open(argv, FTS_PHYSICAL|FTS_NOCHDIR, 0)) == NULL)
		err(1, NULL);
	for (rc = SUCCESS; (entry = fts_read(ftsp)) != NULL;) {
		cat = cflag;
		pipin = 0;
		infile = entry->fts_path;
		if (infile[0] == '-' && infile[1] == '\0') {
			infile = "stdin";
			pipin++;
			if (!oflag)
				cat = 1;
		}
		else
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
				if (entry->fts_errno == ENOENT) {
					p = strrchr(entry->fts_accpath, '.');
					if ((p == NULL ||
					    strcmp(p, suffix) != 0) &&
					    snprintf(_infile, sizeof(_infile),
					    "%s%s", infile, suffix) <
					    sizeof(_infile) &&
					    stat(_infile, entry->fts_statp) ==
					    0 &&
					    S_ISREG(entry->fts_statp->st_mode)) {
						infile = _infile;
						break;
					}
				}
			case FTS_ERR:
			case FTS_DNR:
				warnx("%s: %s", infile,
				    strerror(entry->fts_errno));
				rc = rc ? rc : WARNING;
				continue;
			default:
				if (!S_ISREG(entry->fts_statp->st_mode) &&
				    !(S_ISLNK(entry->fts_statp->st_mode) &&
				    cat)) {
					warnx("%s not a regular file%s",
					    infile, cat ? "" : ": unchanged");
					rc = rc ? rc : WARNING;
					continue;
				}
				break;
			}

		if (!decomp && !pipin && (s = check_suffix(infile)) != NULL) {
			warnx("%s already has %s suffix -- unchanged",
			    infile, s);
			rc = rc ? rc : WARNING;
			continue;
		}

		if (!oflag) {
			if (cat)
				strlcpy(outfile, "stdout", sizeof(outfile));
			else if (decomp) {
				if (set_outfile(infile, outfile,
				    sizeof outfile) == NULL) {
					if (!recurse) {
						warnx("%s: unknown suffix: "
						    "ignored", infile);
						rc = rc ? rc : WARNING;
					}
					continue;
				}
			} else {
				if (snprintf(outfile, sizeof(outfile),
				    "%s%s", infile, suffix) >= sizeof(outfile)) {
					warnx("%s%s: name too long",
					    infile, suffix);
					rc = rc ? rc : WARNING;
					continue;
				}
			}
		}

		if (verbose > 0 && !pipin && !list)
			fprintf(stderr, "%s:\t", infile);

		error = (decomp ? dodecompress : docompress)
		    (infile, outfile, method, bits, entry->fts_statp);

		switch (error) {
		case SUCCESS:
			if (!cat && !testmode) {
				if (!pipin && unlink(infile) && verbose >= 0)
					warn("input: %s", infile);
			}
			break;
		case WARNING:
			rc = rc ? rc : WARNING;
			break;
		default:
			rc = FAILURE;
			break;
		}
	}
	if (list)
		list_stats(NULL, NULL, NULL);

	exit(rc);
}

int
docompress(const char *in, char *out, const struct compressor *method,
    int bits, struct stat *sb)
{
#ifndef SMALL
	u_char buf[Z_BUFSIZE];
	char *name;
	int error, ifd, ofd, flags, oreg;
	void *cookie;
	ssize_t nr;
	u_int32_t mtime;
	struct z_info info;
	struct stat osb;

	mtime = 0;
	flags = oreg = 0;
	error = SUCCESS;
	name = NULL;
	cookie  = NULL;

	if (pipin)
		ifd = dup(STDIN_FILENO);
	else
		ifd = open(in, O_RDONLY);
	if (ifd < 0) {
		if (verbose >= 0)
			warn("%s", in);
		return (FAILURE);
	}

	if (cat)
		ofd = dup(STDOUT_FILENO);
	else {
		if (stat(out, &osb) == 0) {
			oreg = S_ISREG(osb.st_mode);
			if (!force && oreg && !permission(out)) {
				(void) close(ifd);
				return (WARNING);
			}
		}
		ofd = open(out, O_WRONLY|O_CREAT|O_TRUNC, S_IWUSR);
	}
	if (ofd < 0) {
		if (verbose >= 0)
			warn("%s", out);
		(void) close(ifd);
		return (FAILURE);
	}

	if (method != M_COMPRESS && !force && isatty(ofd)) {
		if (verbose >= 0)
			warnx("%s: won't write compressed data to terminal",
			    out);
		(void) close(ofd);
		(void) close(ifd);
		return (FAILURE);
	}

	if (!pipin && storename) {
		name = basename(in);
		mtime = (u_int32_t)sb->st_mtime;
	}
	if ((cookie = (*method->open)(ofd, "w", name, bits, mtime, flags)) == NULL) {
		if (verbose >= 0)
			warn("%s", out);
		if (oreg)
			(void) unlink(out);
		(void) close(ofd);
		(void) close(ifd);
		return (FAILURE);
	}

	while ((nr = read(ifd, buf, sizeof(buf))) > 0)
		if ((method->write)(cookie, buf, nr) != nr) {
			if (verbose >= 0)
				warn("%s", out);
			error = FAILURE;
			break;
		}

	if (!error && nr < 0) {
		if (verbose >= 0)
			warn("%s", in);
		error = FAILURE;
	}

	if ((method->close)(cookie, &info, out, sb)) {
		if (!error && verbose >= 0)
			warn("%s", out);
		error = FAILURE;
	}

	if (close(ifd)) {
		if (!error && verbose >= 0)
			warn("%s", in);
		error = FAILURE;
	}

	if (!force && info.total_out >= info.total_in) {
		if (verbose > 0)
			fprintf(stderr, "file would grow; left unmodified\n");
		(void) unlink(out);
		error = WARNING;
	}

	if (error) {
		if (oreg)
			(void) unlink(out);
	} else if (verbose > 0)
		verbose_info(out, info.total_out, info.total_in, info.hlen);

	return (error);
#else
	warnx("compression not supported");
	return (FAILURE);
#endif
}

const struct compressor *
check_method(int fd)
{
	const struct compressor *method;
	u_char magic[2];

	if (read(fd, magic, sizeof(magic)) != 2)
		return (NULL);
	for (method = &c_table[0]; method->name != NULL; method++) {
		if (magic[0] == method->magic[0] &&
		    magic[1] == method->magic[1])
			return (method);
	}
#ifndef SMALL
	if (force && cat) {
		null_magic[0] = magic[0];
		null_magic[1] = magic[1];
		return (&null_method);
	}
#endif /* SMALL */
	return (NULL);
}

int
dodecompress(const char *in, char *out, const struct compressor *method,
    int bits, struct stat *sb)
{
	u_char buf[Z_BUFSIZE];
	char oldname[MAXPATHLEN];
	int error, oreg, ifd, ofd;
	void *cookie;
	ssize_t nr;
	struct z_info info;
	struct stat osb;

	oreg = 0;
	error = SUCCESS;
	cookie = NULL;

	if (pipin)
		ifd = dup(STDIN_FILENO);
	else
		ifd = open(in, O_RDONLY);
	if (ifd < 0) {
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

	if ((method = check_method(ifd)) == NULL) {
		if (verbose >= 0)
			warnx("%s: unrecognized file format", in);
		close (ifd);
		return -1;
	}

	/* XXX - open constrains outfile to MAXPATHLEN so this is safe */
	oldname[0] = '\0';
	if ((cookie = (*method->open)(ifd, "r", oldname, bits, 0, 1)) == NULL) {
		if (verbose >= 0)
			warn("%s", in);
		close (ifd);
		return (FAILURE);
	}
	if (storename && oldname[0] != '\0') {
		char *cp = strrchr(out, '/');
		if (cp != NULL) {
			*(cp + 1) = '\0';
			strlcat(out, oldname, MAXPATHLEN);
		} else
			strlcpy(out, oldname, MAXPATHLEN);
		cat = 0;			/* XXX should -c override? */
	}

	if (testmode)
		ofd = -1;
	else {
		if (cat)
			ofd = dup(STDOUT_FILENO);
		else {
			if (stat(out, &osb) == 0) {
				oreg = S_ISREG(osb.st_mode);
				if (!force && oreg && !permission(out)) {
					(void) close(ifd);
					return (WARNING);
				}
			}
			ofd = open(out, O_WRONLY|O_CREAT|O_TRUNC, S_IWUSR);
		}
		if (ofd < 0) {
			if (verbose >= 0)
				warn("%s", in);
			(method->close)(cookie, NULL, NULL, NULL);
			return (FAILURE);
		}
	}

	while ((nr = (method->read)(cookie, buf, sizeof(buf))) > 0) {
		if (ofd != -1 && write(ofd, buf, nr) != nr) {
			if (verbose >= 0)
				warn("%s", out);
			error = FAILURE;
			break;
		}
	}

	if (!error && nr < 0) {
		if (verbose >= 0)
			warnx("%s: %s", in,
			    errno == EINVAL ? "crc error" : strerror(errno));
		error = errno == EINVAL ? WARNING : FAILURE;
	}

	if ((method->close)(cookie, &info, NULL, NULL)) {
		if (!error && verbose >= 0)
			warnx("%s", in);
		error = FAILURE;
	}
	if (storename && !cat) {
		if (info.mtime != 0) {
			sb->st_mtimespec.tv_sec =
			    sb->st_atimespec.tv_sec = info.mtime;
			sb->st_mtimespec.tv_nsec =
			    sb->st_atimespec.tv_nsec = 0;
		} else
			storename = 0;		/* no timestamp to restore */
	}
	if (error == SUCCESS)
		setfile(out, ofd, sb);

	if (ofd != -1 && close(ofd)) {
		if (!error && verbose >= 0)
			warn("%s", out);
		error = FAILURE;
	}

	if (!error) {
		if (list) {
			if (info.mtime == 0)
				info.mtime = (u_int32_t)sb->st_mtime;
			list_stats(out, method, &info);
		} else if (verbose > 0) {
			verbose_info(out, info.total_in, info.total_out,
			    info.hlen);
		}
	}

	/* On error, clean up the file we created but preserve errno. */
	if (error && oreg)
		unlink(out);

	return (error);
}

void
setfile(const char *name, int fd, struct stat *fs)
{
	struct timeval tv[2];

	if (name == NULL || cat || testmode)
		return;

	/*
	 * If input was a pipe we don't have any info to restore but we
	 * must set the mode since the current mode on the file is 0200.
	 */
	if (pipin) {
		mode_t mask = umask(022);
		fchmod(fd, DEFFILEMODE & ~mask);
		umask(mask);
		return;
	}

	/*
	 * Changing the ownership probably won't succeed, unless we're root
	 * or POSIX_CHOWN_RESTRICTED is not set.  Set uid/gid before setting
	 * the mode; current BSD behavior is to remove all setuid bits on
	 * chown.  If chown fails, lose setuid/setgid bits.
	 */
	fs->st_mode &= S_ISUID|S_ISGID|S_IRWXU|S_IRWXG|S_IRWXO;
	if (fchown(fd, fs->st_uid, fs->st_gid)) {
		if (errno != EPERM)
			warn("fchown: %s", name);
		fs->st_mode &= ~(S_ISUID|S_ISGID);
	}
	if (fchmod(fd, fs->st_mode))
		warn("fchmod: %s", name);

	if (fs->st_flags && fchflags(fd, fs->st_flags))
		warn("fchflags: %s", name);

	TIMESPEC_TO_TIMEVAL(&tv[0], &fs->st_atimespec);
	TIMESPEC_TO_TIMEVAL(&tv[1], &fs->st_mtimespec);
	if (futimes(fd, tv))
		warn("futimes: %s", name);
}

int
permission(const char *fname)
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

/*
 * Check infile for a known suffix and return the suffix portion or NULL.
 */
const char *
check_suffix(const char *infile)
{
	int i;
	char *suf, *sep, *separators = ".-_";
	static char *suffixes[] = { "Z", "gz", "z", "tgz", "taz", NULL };

	for (sep = separators; *sep != '\0'; sep++) {
		if ((suf = strrchr(infile, *sep)) == NULL)
			continue;
		suf++;

		for (i = 0; suffixes[i] != NULL; i++) {
			if (strcmp(suf, suffixes[i]) == 0)
				return (suf - 1);
		}
	}
	return (NULL);
}

/*
 * Set outfile based on the suffix.  In most cases we just strip
 * off the suffix but things like .tgz and .taz are special.
 */
char *
set_outfile(const char *infile, char *outfile, size_t osize)
{
	const char *s;
	char *cp;

	if ((s = check_suffix(infile)) == NULL)
		return (NULL);

	(void)strlcpy(outfile, infile, osize);
	cp = outfile + (s - infile) + 1;
	/*
	 * Convert tgz and taz -> tar, else drop the suffix.
	 */
	if (strcmp(cp, "tgz") == 0) {
		cp[1] = 'a';
		cp[2] = 'r';
	} else if (strcmp(cp, "taz") == 0)
		cp[2] = 'r';
	else
		cp[-1] = '\0';
	return (outfile);
}

/*
 * Print output for the -l option.
 */
void
list_stats(const char *name, const struct compressor *method,
    struct z_info *info)
{
	static off_t compressed_total, uncompressed_total, header_total;
	static u_int nruns;
	char *timestr;

	if (nruns == 0) {
		if (verbose >= 0) {
			if (verbose > 0)
				fputs("method  crc      date   time  ", stdout);
			puts("compressed  uncompressed  ratio  uncompressed_name");
		}
	}
	nruns++;

	if (name != NULL) {
		if (verbose > 0) {
			timestr = ctime(&info->mtime) + 4;
			timestr[12] = '\0';
			if (timestr[4] == ' ')
				timestr[4] = '0';
			printf("%-7.7s %08x %s ", method->name, info->crc,
			    timestr);
		}
		printf("%10lld    %10lld  %4.1f%%  %s\n",
		    (long long)(info->total_in + info->hlen),
		    (long long)info->total_out,
		    ((long long)info->total_out - (long long)info->total_in) *
		    100.0 / info->total_out, name);
		compressed_total += info->total_in;
		uncompressed_total += info->total_out;
		header_total += info->hlen;
	} else if (verbose >= 0) {
		if (nruns < 3)		/* only do totals for > 1 files */
			return;
		if (verbose > 0)
			fputs("                              ", stdout);
		printf("%10lld    %10lld  %4.1f%%  (totals)\n",
		    (long long)(compressed_total + header_total),
		    (long long)uncompressed_total,
		    (uncompressed_total - compressed_total) *
		    100.0 / uncompressed_total);
	}
}

void
verbose_info(const char *file, off_t compressed, off_t uncompressed,
    u_int32_t hlen)
{
	if (testmode) {
		fputs("OK\n", stderr);
		return;
	}
	if (!pipin) {
		fprintf(stderr, "\t%4.1f%% -- replaced with %s\n",
		    (uncompressed - compressed) * 100.0 / uncompressed, file);
	}
	compressed += hlen;
	fprintf(stderr, "%lld bytes in, %lld bytes out\n",
	    (long long)(decomp ? compressed : uncompressed),
	    (long long)(decomp ? uncompressed : compressed));
}

__dead void
usage(int status)
{
	switch (pmode) {
	case MODE_COMP:
		fprintf(stderr, "usage: %s [-123456789cdfghLlNnOqrtVv] "
		    "[-b bits] [-o filename] [-S suffix]\n"
		    "       %*s [file ...]\n",
		    __progname, (int)strlen(__progname), "");
		break;
	case MODE_DECOMP:
		fprintf(stderr, "usage: %s [-cfhlNnqrtVv] [-o filename] "
		    "[file ...]\n", __progname);
		break;
	case MODE_CAT:
		fprintf(stderr, "usage: %s [-fghqr] [file ...]\n", __progname);
		break;
	}
	exit(status);
}
