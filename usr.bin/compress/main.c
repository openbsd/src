/*	$OpenBSD: main.c,v 1.10 1998/09/10 06:44:41 deraadt Exp $	*/

/*-
 * Copyright (c) 1992, 1993
 *	The Regents of the University of California.  All rights reserved.
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
 *	This product includes software developed by the University of
 *	California, Berkeley and its contributors.
 * 4. Neither the name of the University nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE REGENTS AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

static char copyright[] =
"@(#) Copyright (c) 1992, 1993\n\
	The Regents of the University of California.  All rights reserved.\n";

#ifndef lint
#if 0
static char sccsid[] = "@(#)compress.c	8.2 (Berkeley) 1/7/94";
#else
static char rcsid[] = "$OpenBSD: main.c,v 1.10 1998/09/10 06:44:41 deraadt Exp $";
#endif
#endif /* not lint */

#include <sys/param.h>
#include <sys/time.h>
#include <sys/stat.h>

#include <err.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <paths.h>
#include "compress.h"

#define min(a,b) ((a) < (b)? (a) : (b))

int pipin = 0, force = 0, verbose = 0, testmode = 0, list = 0, nosave = 0;
extern char *__progname;

struct compressor {
	char *name;
	char *suffix;
	int (*check_header) __P((int, struct stat *, const char *));
	void *(*open) __P((int, const char *, int));
	int (*read) __P((void *, char *, int));
	int (*write) __P((void *, const char *, int));
	int (*close) __P((void *));
} c_table[] = {
#define M_COMPRESS (&c_table[0])
  { "compress", ".Z", z_check_header,  z_open,  zread,   zwrite,   zclose },
#define M_DEFLATE (&c_table[1])
  { "deflate", ".gz", gz_check_header, gz_open, gz_read, gz_write, gz_close },
  { NULL }
};

int permission __P((char *));
void setfile __P((char *, struct stat *));
void usage __P((void));
int compress
	__P((const char *, const char *, register struct compressor *, int));
int decompress
	__P((const char *, const char *, register struct compressor *, int));
struct compressor *check_method __P((int, const char *));

struct stat sb, osb;

int
main(argc, argv)
	int argc;
	char *argv[];
{
	int ch, bits, cat, decomp, error;
	struct compressor *method;
	int exists, isreg, oreg;
	char *infile, outfile[MAXPATHLEN+4], suffix[16];
	char *p;
	int rc = 0;

	bits = cat = decomp = 0;
	p = __progname;
	if (p[0] == 'g') {
		method = M_DEFLATE;
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

	outfile[0] = '\0';
	while ((ch = getopt(argc, argv, "0123456789b:cdfghlnOo:qStv")) != -1)
		switch(ch) {
		case '0':
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
			bits = ch - '0';
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
			break;
		case 'l':
			list++;
			break;
		case 'L':
			fputs(copyright, stderr);
		case 'n':
			nosave++;
			break;
		case 'N':
			nosave = 0;
			break;
		case 'O':
			method = M_COMPRESS;
			break;
		case 'o':
			strncpy(outfile, optarg, sizeof(outfile)-1);
			outfile[sizeof(outfile)-1] = '\0';
			break;
		case 'q':
			verbose = -1;
			break;
		case 'S':
			p = suffix;
			if (optarg[0] != '.')
				*p++ = '.';
			strncpy(p, optarg, sizeof(suffix) - (p - suffix) - 1);
			break;
		case 't':
			testmode++;
			break;
		case 'v':
			verbose++;
			break;
		case 'h':
		case '?':
		default:
			usage();
		}
	argc -= optind;
	argv += optind;

	do {
		if (*argv != NULL) {
			infile = *argv;
			if (outfile[0] == '\0') {
				if (!decomp && !cat && outfile[0] == '\0') {
					int len;
					char *p;

					snprintf(outfile, sizeof(outfile),
						"%s%s", infile,
						method->suffix);

					len = strlen(outfile);
					if (len > MAXPATHLEN) {
						errx(1, "pathname%s too long",
							method->suffix);
					}
					
					p = strrchr(outfile, '/');
					if (p == NULL) p = outfile;
					len = strlen(p);
					if (len > NAME_MAX) {
						errx(1, "filename%s too long",
							method->suffix);
					}
				} else if (decomp && !cat) {
					char *p = strrchr(infile, '.');
					if (p != NULL)
						for (method = &c_table[0];
						     method->name != NULL &&
							!strcmp(p, method->suffix);
						     method++)
							;
					if (method->name != NULL) {
						int l =	min(sizeof(outfile),
							    (p - infile));
						strncpy(outfile, infile, l);
						outfile[l] = '\0';
					}
				}
			}			
		} else {
			infile = "/dev/stdin";
			pipin++;
		}

		if (testmode)
			strcpy(outfile, _PATH_DEVNULL);
		else if (cat || outfile[0] == '\0') {
			strcpy(outfile, "/dev/stdout");
			cat++;
		}

		exists = !stat(outfile, &sb);
		if (!force && exists && S_ISREG(sb.st_mode) &&
		    !permission(outfile)) {
		    	argv++;
			continue;
		}
		isreg = oreg = !exists || S_ISREG(sb.st_mode);

		if (stat(infile, &sb) != 0 && verbose >= 0)
			err(1, infile);

		if (!S_ISREG(sb.st_mode))
			isreg = 0;

		if (verbose > 0)
			fprintf(stderr, "%s:\t", infile);

		error = (decomp? decompress: compress)
			(infile, outfile, method, bits);

		if (!error && isreg && stat(outfile, &osb) == 0) {

			if (!force && !decomp && osb.st_size >= sb.st_size) {
				if (verbose > 0)
					fprintf(stderr, "file would grow; "
						     "left unmodified\n");
				error = 1;
				rc = 2;
			} else {

				setfile(outfile, &sb);

				if (unlink(infile) && verbose >= 0)
					warn("%s", infile);

				if (verbose > 0) {
					u_int ratio;
					ratio = (1000*osb.st_size)/sb.st_size;
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
		    verbose >= 0)
			warn("%s", outfile);
		else if (!error && verbose > 0)
			fputs("OK\n", stderr);

		outfile[0] = '\0';
		if (*argv != NULL)
			argv++;

	} while (*argv != NULL);

	return (rc);
}

int
compress(in, out, method, bits)
	const char *in;
	const char *out;
	register struct compressor *method;
	int bits;
{
	register int ifd;
	int ofd;
	register void *cookie;
	register size_t nr;
	u_char buf[Z_BUFSIZE];
	int error;

	error = 0;
	cookie  = NULL;

	if ((ofd = open(out, O_WRONLY|O_CREAT, S_IWUSR)) < 0) {
		if (verbose >= 0)
			warn("%s", out);
		return -1;
	}

	if (method != M_COMPRESS && !force && isatty(ofd)) {
		if (verbose >= 0)
			warnx("%s: won't write compressed data to terminal",
			      out);
		return -1;
	}

	if ((ifd = open(in, O_RDONLY)) >= 0 &&
	    (cookie = (*method->open)(ofd, "w", bits)) != NULL) {

		while ((nr = read(ifd, buf, sizeof(buf))) > 0)
			if ((method->write)(cookie, buf, nr) != nr) {
				if (verbose >= 0)
					warn("%s", out);
				error++;
				break;
			}
	}

	if (ifd < 0 || close(ifd) || nr < 0) {
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

	return error? -1 : 0;
}

struct compressor *
check_method(fd, out)
	int fd;
	const char *out;
{
	register struct compressor *method;

	for (method = &c_table[0];
	     method->name != NULL &&
		     !(*method->check_header)(fd, &sb, out);
	     method++)
		;

	if (method->name == NULL)
		method = NULL;

	return method;
}

int
decompress(in, out, method, bits)
	const char *in;
	const char *out;
	register struct compressor *method;
	int bits;
{
	int ifd;
	register int ofd;
	register void *cookie;
	register size_t nr;
	u_char buf[Z_BUFSIZE];
	int error;

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

	if (!pipin && (method = check_method(ifd, out)) == NULL) {
		if (verbose >= 0)
			warnx("%s: unrecognized file format", in);
		return -1;
	}

	if ((ofd = open(out, O_WRONLY|O_CREAT, S_IWUSR)) >= 0 &&
	    (cookie = (*method->open)(ifd, "r", bits)) != NULL) {

		while ((nr = (method->read)(cookie, buf, sizeof(buf))) > 0)
			if (write(ofd, buf, nr) != nr) {
				if (verbose >= 0)
					warn("%s", out);
				error++;
				break;
			}
	}

	if (ofd < 0 || close(ofd)) {
		if (!error && verbose >= 0)
			warn("%s", out);
		error++;
	}

	if (cookie == NULL || (method->close)(cookie) || nr < 0) {
		if (!error && verbose >= 0)
			warn("%s", in);
		error++;
		(void) close (ifd);
	}

	return error;
}

void
setfile(name, fs)
	char *name;
	register struct stat *fs;
{
	static struct timeval tv[2];

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
	char *fname;
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
		"usage: %s [-cdfghlnOtqv] [-b <bits>] [-[0-9]] [file ...]\n",
		__progname);
	exit(1);
}

