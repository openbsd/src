/*	$OpenBSD: sort.c,v 1.8 1998/07/20 19:46:15 deraadt Exp $	*/

/*-
 * Copyright (c) 1993
 *	The Regents of the University of California.  All rights reserved.
 *
 * This code is derived from software contributed to Berkeley by
 * Peter McIlroy.
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

#ifndef lint
static char copyright[] =
"@(#) Copyright (c) 1993\n\
	The Regents of the University of California.  All rights reserved.\n";
#endif /* not lint */

#ifndef lint
#if 0
static char sccsid[] = "@(#)sort.c	8.1 (Berkeley) 6/6/93";
#else
static char rcsid[] = "$OpenBSD: sort.c,v 1.8 1998/07/20 19:46:15 deraadt Exp $";
#endif
#endif /* not lint */

/*
 * Sort sorts a file using an optional user-defined key.
 * Sort uses radix sort for internal sorting, and allows
 * a choice of merge sort and radix sort for external sorting.
 */

#include "sort.h"
#include "fsort.h"
#include "pathnames.h"

#include <paths.h>
#include <signal.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

int REC_D = '\n';
u_char d_mask[NBINS];		/* flags for rec_d, field_d, <blank> */

/*
 * weight tables.  Gweights is one of ascii, Rascii..
 * modified to weight rec_d = 0 (or 255)
 */
extern u_char gweights[NBINS];
u_char ascii[NBINS], Rascii[NBINS], RFtable[NBINS], Ftable[NBINS];

/*
 * masks of ignored characters.  Alltable is 256 ones
 */
u_char dtable[NBINS], itable[NBINS], alltable[NBINS];
int SINGL_FLD = 0, SEP_FLAG = 0, UNIQUE = 0;
struct coldesc clist[(ND+1)*2];
int ncols = 0;
extern struct coldesc clist[(ND+1)*2];
extern int ncols;

char devstdin[] = _PATH_STDIN;
char toutpath[_POSIX_PATH_MAX];
char *tmpdir = _PATH_VARTMP;

static void cleanup __P((void));
static void onsig __P((int));
static void usage __P((char *));

int
main(argc, argv)
	int argc;
	char *argv[];
{
	int (*get)();
	int ch, i, stdinflag = 0, tmp = 0;
	char cflag = 0, mflag = 0;
	char *outfile, *outpath = 0;
	struct field fldtab[ND+2], *ftpos;
	union f_handle filelist;
	FILE *outfp = NULL;

	memset(fldtab, 0, (ND+2)*sizeof(struct field));
	memset(d_mask, 0, NBINS);
	d_mask[REC_D = '\n'] = REC_D_F;
	SINGL_FLD = SEP_FLAG = 0;
	d_mask['\t'] = d_mask[' '] = BLANK | FLD_D;
	ftpos = fldtab;
	fixit(&argc, argv);
	if (!issetugid() && (outfile = getenv("TMPDIR")))
		tmpdir = outfile;
	while ((ch = getopt(argc, argv, "bcdfik:mHno:rR:t:T:uy:")) != -1) {
		switch (ch) {
		case 'b': fldtab->flags |= BI | BT;
			break;
		case 'd':
		case 'f':
		case 'i':
		case 'n':
		case 'r': tmp |= optval(ch, 0);
			if (tmp & R && tmp & F)
				fldtab->weights = RFtable;
			else if (tmp & F)
				fldtab->weights = Ftable;
			else if (tmp & R)
				fldtab->weights = Rascii;
			fldtab->flags |= tmp;
			break;
		case 'o':
			outpath = optarg;
			break;
		case 'k':
			setfield(optarg, ++ftpos, fldtab->flags);
			break;
		case 't':
			if (SEP_FLAG)
				usage("multiple field delimiters");
			SEP_FLAG = 1;
			d_mask[' '] &= ~FLD_D;
			d_mask['\t'] &= ~FLD_D;
			d_mask[(int)*optarg] |= FLD_D;
			if (d_mask[(int)*optarg] & REC_D_F)
				err(2, "record/field delimiter clash");
			break;
		case 'R':
			if (REC_D != '\n')
				usage("multiple record delimiters");
			if ('\n' == (REC_D = *optarg))
				break;
			d_mask['\n'] = d_mask[' '];
			d_mask[REC_D] = REC_D_F;
			break;
		case 'T':
			tmpdir = optarg;
			break;
		case 'u':
			UNIQUE = 1;
			break;
		case 'c':
			cflag = 1;
			break;
		case 'm':
			mflag = 1;
			break;
		case 'H':
			PANIC = 0;
			break;
		case 'y':
			/* accept -y for backwards compat. */
			break;
		case '?':
		default:
			usage(NULL);
		}
	}

	if (cflag && argc > optind+1)
		errx(2, "too many input files for -c option");

	if (argc - 2 > optind && !strcmp(argv[argc-2], "-o")) {
		outpath = argv[argc-1];
		argc -= 2;
	}

	if (mflag && argc - optind > (MAXFCT - (16+1))*16)
		errx(2, "too many input files for -m option");

	for (i = optind; i < argc; i++) {
		/* allow one occurrence of /dev/stdin */
		if (!strcmp(argv[i], "-") || !strcmp(argv[i], devstdin)) {
			if (stdinflag)
				warnx("ignoring extra \"%s\" in file list",
				    argv[i]);
			else {
				stdinflag = 1;
				argv[i] = devstdin;
			}
		} else if ((ch = access(argv[i], R_OK)))
			err(2, argv[i]);
	}

	if (!(fldtab->flags & (I|D|N) || fldtab[1].icol.num)) {
		SINGL_FLD = 1;
		fldtab[0].icol.num = 1;
	} else {
		if (!fldtab[1].icol.num) {
			fldtab[0].flags &= ~(BI|BT);
			setfield("1", ++ftpos, fldtab->flags);
		}
		fldreset(fldtab);
		fldtab[0].flags &= ~F;
	}
	settables(fldtab[0].flags);
	num_init();
	fldtab->weights = gweights;

	if (optind == argc) {
		static char *names[2];

		names[0] = devstdin;
		names[1] = NULL;
		filelist.names = names;
		optind--;
	} else
		filelist.names = argv+optind;

	if (SINGL_FLD)
		get = makeline;
	else
		get = makekey;

	if (cflag) {
		order(filelist, get, fldtab);
		/* NOT REACHED */
	}

	if (!outpath) {
		(void)snprintf(toutpath,
		    sizeof(toutpath), "%sstdout", _PATH_DEV);
		outfile = outpath = toutpath;
	} else if (!(ch = access(outpath, 0)) &&
	    strncmp(_PATH_DEV, outpath, 5)) {
		struct sigaction act = {0, SIG_BLOCK, 6};
		int sigtable[] = {SIGHUP, SIGINT, SIGPIPE, SIGXCPU, SIGXFSZ,
		    SIGVTALRM, SIGPROF, 0};
		int outfd;

		errno = 0;

		if (access(outpath, W_OK))
			err(2, outpath);
		act.sa_handler = onsig;
		(void)snprintf(toutpath, sizeof(toutpath), "%sXXXXXXXXXX",
		    outpath);
		if ((outfd = mkstemp(toutpath)) < 0 ||
		    (outfp = fdopen(outfd, "w")) == 0)
			err(2, toutpath);
		outfile = toutpath;

		(void)atexit(cleanup);
		for (i = 0; sigtable[i]; ++i)	/* always unlink toutpath */
			sigaction(sigtable[i], &act, 0);
	} else
		outfile = outpath;
	if (outfp == NULL && (outfp = fopen(outfile, "w")) == NULL)
		err(2, outfile);
	if (mflag)
		fmerge(-1, filelist, argc-optind, get, outfp, putline, fldtab);
	else
		fsort(-1, 0, filelist, argc-optind, outfp, fldtab);
	if (outfile != outpath) {
		if (access(outfile, 0))
			err(2, outfile);
		(void)unlink(outpath);
		if (link(outfile, outpath))
			err(2, "cannot link %s: output left in %s",
			    outpath, outfile);
		(void)unlink(outfile);
	}
	exit(0);
}

static void
onsig(s)
	int s;
{

	cleanup();
	exit(2);			/* return 2 on error/interrupt */
}

static void
cleanup()
{

	if (toutpath[0])
		(void)unlink(toutpath);
}

static void
usage(msg)
	char *msg;
{
	extern char *__progname;

	if (msg)
		(void)fprintf(stderr, "sort: %s\n", msg);
	(void)fprintf(stderr, "usage: %s [-T dir] [-o output] [-cmubdfinr] "
	"[-t char] [-R char] [-k keydef] ... [files]\n", __progname);
	exit(2);
}
