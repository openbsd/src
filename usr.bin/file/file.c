/*	$OpenBSD: file.c,v 1.11 2003/03/11 21:26:26 ian Exp $	*/

/*
 * file - find type of a file or files - main program.
 *
 * Copyright (c) Ian F. Darwin 1986-1995.
 * Software written by Ian F. Darwin and others;
 * maintained 1995-present by Christos Zoulas and others.
 * 
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice immediately at the beginning of the file, without modification,
 *    this list of conditions, and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *    This product includes software developed by Ian F. Darwin and others.
 * 4. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission.
 *  
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE FOR
 * ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#ifndef	lint
static char *moduleid = "$OpenBSD: file.c,v 1.11 2003/03/11 21:26:26 ian Exp $";
#endif	/* lint */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/param.h>	/* for MAXPATHLEN */
#include <sys/stat.h>
#include <fcntl.h>	/* for open() */
#if (__COHERENT__ >= 0x420)
# include <sys/utime.h>
#else
# ifdef USE_UTIMES
#  include <sys/time.h>
# else
#  include <utime.h>
# endif
#endif
#include <unistd.h>	/* for read() */
#include <err.h>

#include <netinet/in.h>		/* for byte swapping */

#include "patchlevel.h"
#include "file.h"

#ifdef S_IFLNK
# define USAGE  "Usage: %s [-vbczL] [-f namefile] [-m magicfiles] file...\n"
#else
# define USAGE  "Usage: %s [-vbcz] [-f namefile] [-m magicfiles] file...\n"
#endif

#ifndef MAGIC
# define MAGIC "/etc/magic"
#endif

int 			/* Global command-line options 		*/
	debug = 0, 	/* debugging 				*/
	bflag = 0,	/* Don't print filename			*/
	lflag = 0,	/* follow Symlinks (BSD only) 		*/
	zflag = 0;	/* follow (uncompress) compressed files */

int			/* Misc globals				*/
	nmagic = 0;	/* number of valid magic[]s 		*/

struct  magic *magic;	/* array of magic entries		*/

char *magicfile;	/* where magic be found 		*/

int lineno;		/* line number in the magic file	*/


static void	unwrap(char *fn);
#if 0
static int	byteconv4(int, int, int);
static short	byteconv2(int, int, int);
#endif

/*
 * main - parse arguments and handle options
 */
int
main(argc, argv)
int argc;
char *argv[];
{
	int c;
	int check = 0, didsomefiles = 0, errflg = 0, ret = 0, app = 0;
	extern char *__progname;

	if (!(magicfile = getenv("MAGIC")))
		magicfile = MAGIC;

	while ((c = getopt(argc, argv, "bvcdf:Lm:z")) != -1)
		switch (c) {
		case 'v':
			(void) printf("%s-%d.%d\n", __progname,
				       FILE_VERSION_MAJOR, patchlevel);
			return 1;
		case 'b':
			++bflag;
			break;
		case 'c':
			++check;
			break;
		case 'd':
			++debug;
			break;
		case 'f':
			if (!app) {
				ret = apprentice(magicfile, check);
				if (check)
					exit(ret);
				app = 1;
			}
			unwrap(optarg);
			++didsomefiles;
			break;
#ifdef S_IFLNK
		case 'L':
			++lflag;
			break;
#endif
		case 'm':
			magicfile = optarg;
			break;
		case 'z':
			zflag++;
			break;
		case '?':
		default:
			errflg++;
			break;
		}

	if (errflg) {
		(void) fprintf(stderr, USAGE, __progname);
		exit(2);
	}

	if (!app) {
		ret = apprentice(magicfile, check);
		if (check)
			exit(ret);
		app = 1;
	}

	if (optind == argc) {
		if (!didsomefiles) {
			fprintf(stderr, USAGE, __progname);
			exit(2);
		}
	} else {
		int i, wid, nw;
		for (wid = 0, i = optind; i < argc; i++) {
			nw = strlen(argv[i]);
			if (nw > wid)
				wid = nw;
		}
		for (; optind < argc; optind++)
			process(argv[optind], wid);
	}

	return 0;
}


/*
 * unwrap -- read a file of filenames, do each one.
 */
static void
unwrap(fn)
char *fn;
{
	char buf[MAXPATHLEN];
	FILE *f;
	int wid = 0, cwid;

	if (strcmp("-", fn) == 0) {
		f = stdin;
		wid = 1;
	} else {
		if ((f = fopen(fn, "r")) == NULL) {
			err(1, "Cannot open `%s'", fn);
			/*NOTREACHED*/
		}

		while (fgets(buf, sizeof(buf), f) != NULL) {
			cwid = strlen(buf) - 1;
			if (cwid > wid)
				wid = cwid;
		}

		rewind(f);
	}

	while (fgets(buf, sizeof(buf), f) != NULL) {
		buf[strlen(buf)-1] = '\0';
		process(buf, wid);
	}

	(void) fclose(f);
}


#if 0
/*
 * byteconv4
 * Input:
 *	from		4 byte quantity to convert
 *	same		whether to perform byte swapping
 *	big_endian	whether we are a big endian host
 */
static int
byteconv4(from, same, big_endian)
    int from;
    int same;
    int big_endian;
{
  if (same)
    return from;
  else if (big_endian)		/* lsb -> msb conversion on msb */
  {
    union {
      int i;
      char c[4];
    } retval, tmpval;

    tmpval.i = from;
    retval.c[0] = tmpval.c[3];
    retval.c[1] = tmpval.c[2];
    retval.c[2] = tmpval.c[1];
    retval.c[3] = tmpval.c[0];

    return retval.i;
  }
  else
    return ntohl(from);		/* msb -> lsb conversion on lsb */
}

/*
 * byteconv2
 * Same as byteconv4, but for shorts
 */
static short
byteconv2(from, same, big_endian)
	int from;
	int same;
	int big_endian;
{
  if (same)
    return from;
  else if (big_endian)		/* lsb -> msb conversion on msb */
  {
    union {
      short s;
      char c[2];
    } retval, tmpval;

    tmpval.s = (short) from;
    retval.c[0] = tmpval.c[1];
    retval.c[1] = tmpval.c[0];

    return retval.s;
  }
  else
    return ntohs(from);		/* msb -> lsb conversion on lsb */
}
#endif

/*
 * process - process input file
 */
void
process(inname, wid)
const char	*inname;
int wid;
{
	int	fd = 0;
	static  const char stdname[] = "standard input";
	unsigned char	buf[HOWMANY+1];	/* one extra for terminating '\0' */
	struct stat	sb;
	int nbytes = 0;	/* number of bytes read from a datafile */
	char match = '\0';

	if (strcmp("-", inname) == 0) {
		if (fstat(0, &sb)<0) {
			err(1, "cannot fstat `%s'", stdname);
			/*NOTREACHED*/
		}
		inname = stdname;
	}

	if (wid > 0 && !bflag)
	     (void) printf("%s:%*s ", inname, 
			   (int) (wid - strlen(inname)), "");

	if (inname != stdname) {
	    /*
	     * first try judging the file based on its filesystem status
	     */
	    if (fsmagic(inname, &sb) != 0) {
		    putchar('\n');
		    return;
	    }

	    if ((fd = open(inname, O_RDONLY)) < 0) {
		    /* We can't open it, but we were able to stat it. */
		    if (sb.st_mode & 0002) ckfputs("writable, ", stdout);
		    if (sb.st_mode & 0111) ckfputs("executable, ", stdout);
		    ckfprintf(stdout, "can't read `%s' (%s).\n",
			inname, strerror(errno));
		    return;
	    }
	}


	/*
	 * try looking at the first HOWMANY bytes
	 */
	if ((nbytes = read(fd, (char *)buf, HOWMANY)) == -1) {
		err(1, "read failed");
		/*NOTREACHED*/
	}

	if (nbytes == 0)
		ckfputs("empty", stdout);
	else {
		buf[nbytes++] = '\0';	/* null-terminate it */
		match = tryit(buf, nbytes, zflag);
	}

#ifdef BUILTIN_ELF
	if (match == 's' && nbytes > 5)
		tryelf(fd, buf, nbytes);
#endif

	if (inname != stdname) {
#ifdef RESTORE_TIME
		/*
		 * Try to restore access, modification times if read it.
		 */
# ifdef USE_UTIMES
		struct timeval  utsbuf[2];
		utsbuf[0].tv_sec = sb.st_atime;
		utsbuf[1].tv_sec = sb.st_mtime;

		(void) utimes(inname, utsbuf); /* don't care if loses */
# else
		struct utimbuf  utbuf;

		utbuf.actime = sb.st_atime;
		utbuf.modtime = sb.st_mtime;
		(void) utime(inname, &utbuf); /* don't care if loses */
# endif
#endif
		(void) close(fd);
	}
	(void) putchar('\n');
}


int
tryit(buf, nb, zflag)
unsigned char *buf;
int nb, zflag;
{
	/* try compression stuff */
	if (zflag && zmagic(buf, nb))
		return 'z';

	/* try tests in /etc/magic (or surrogate magic file) */
	if (softmagic(buf, nb))
		return 's';

	/* try known keywords, check whether it is ASCII */
	if (ascmagic(buf, nb))
		return 'a';

	/* see if it's international language text */
	if (internatmagic(buf, nb))
		return 'i';

	/* abandon hope, all ye who remain here */
	ckfputs("data", stdout);
		return '\0';
}
