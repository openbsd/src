/*	$OpenBSD: pr.c,v 1.30 2010/08/25 18:20:11 chl Exp $	*/

/*-
 * Copyright (c) 1991 Keith Muller.
 * Copyright (c) 1993
 *	The Regents of the University of California.  All rights reserved.
 *
 * This code is derived from software contributed to Berkeley by
 * Keith Muller of the University of California, San Diego.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. Neither the name of the University nor the names of its contributors
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

#include <sys/types.h>
#include <sys/time.h>
#include <sys/stat.h>

#include <ctype.h>
#include <errno.h>
#include <limits.h>
#include <signal.h>
#include <stdio.h>
#include <stdarg.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "pr.h"
#include "extern.h"

/*
 * pr:	a printing and pagination filter. If multiple input files
 *	are specified, each is read, formatted, and written to standard
 *	output. By default, input is separated into 66-line pages, each
 *	with a header that includes the page number, date, time and the
 *	files pathname.
 *
 *	Complies with posix P1003.2/D11
 */

/*
 * pr: more boundary conditions than a four-legged porcupine
 *
 * the original version didn't support form-feeds, while many of the ad-hoc
 * pr implementations out there do.  Addding this and making it work reasonably
 * in all four output modes required quite a bit of hacking and a few minor
 * bugs were noted and fixed in the process.  Some implementations have this
 * as the as -f, some as -F so we accept either.
 *
 * The implementation of form feeds on top of the existing I/O structure is
 * a bit idiosyncratic.  Basically they are treated as temporary end-of-file
 * conditions and an additional level of "loop on form feed" is added to each
 * of the output modes to continue after such a transient end-of-file's. This
 * has the general benefit of making the existing header/trailer logic work
 * and provides a usable framework for rational behavior in multi-column modes.
 *
 * The original "efficient" implementation of the "skip to page N" option was
 * bogus and I substituted the basic inhibit printing until page N approach.
 * This is still fairly bogus vis-a-vis numbering pages on multiple files
 * restarting at one, but at least lets you consistently reprint some large
 * document starting in the middle, in any of the output modes.
 *
 * Additional support for overprinting via <back-space> or <return> would
 * be nice, but is not trivial across tab interpretation, output formatting
 * and the different operating modes.  Support for line-wrapping, either
 * strict or word-wrapped would be really useful and not all that hard to
 * kludge into the inln() implementation.  The general notion is that -wc n
 * would specify width and wrapping with a marker character c and -Wc n
 * would add word wrapping with a minimum width n and delimiters c, defaulting
 * to tab, blank, and -, and column width.  Word wrapping always involves
 * painful policy questions which are difficult to specify unless you just
 * hardwire in some fixed rules. Think quotes, punctuation and white-space
 * elimination and whether you'd do the same thing with a C program and
 * something like columninated newspaper text.
 *
 *				George Robbins <grr@tharsis.com> 4/22/97.
 */

/*
 * parameter variables
 */
int	pgnm;		/* starting page number */
int	skipping;	/* we're skipping to page pgnum */
int	clcnt;		/* number of columns */
int	colwd;		/* column data width - multiple columns */
int	across;		/* mult col flag; write across page */
int	dspace;		/* double space flag */
char	inchar;		/* expand input char */
int	ingap;		/* expand input gap */
int	formfeed;	/* use formfeed as trailer */
int	inform;		/* grok formfeeds in input */
char	*header;	/* header name instead of file name */
char	ochar;		/* contract output char */
int	ogap;		/* contract output gap */
int	lines;		/* number of lines per page */
int	merge;		/* merge multiple files in output */
char	nmchar;		/* line numbering append char */
int	nmwd;		/* width of line number field */
int	offst;		/* number of page offset spaces */
int	nodiag;		/* do not report file open errors */
char	schar;		/* text column separation character */
int	sflag;		/* -s option for multiple columns */
int	nohead;		/* do not write head and trailer */
int	pgwd;		/* page width with multiple col output */
char	*timefrmt;	/* time conversion string */

/*
 * misc globals
 */
volatile sig_atomic_t	ferr;	/* error message delayed */
int	addone = 0;	/* page length is odd with double space */
int	errcnt = 0;	/* error count on file processing */
int	beheaded = 0;	/* header / trailer link */
char	digs[] = "0123456789";	/* page number translation map */

int
main(int argc, char *argv[])
{
    int ret_val;

    if (signal(SIGINT, SIG_IGN) != SIG_IGN)
	(void)signal(SIGINT, terminate);
    ret_val = setup(argc, argv);
    if (!ret_val) {
	/*
	 * select the output format based on options
	 */
	if (merge)
	    ret_val = mulfile(argc, argv);
	else if (clcnt == 1)
	    ret_val = onecol(argc, argv);
	else if (across)
	    ret_val = horzcol(argc, argv);
	else
	    ret_val = vertcol(argc, argv);
    } else
	usage();
    flsh_errs();
    if (errcnt || ret_val)
	exit(1);
    return(0);
}

/*
 * onecol:    print files with only one column of output.
 *        Line length is unlimited.
 */
int
onecol(int argc, char *argv[])
{
    int off;
    int lrgln;
    int linecnt;
    int num;
    int cnt;
    int rc;
    int lncnt;
    int pagecnt;
    int ips;
    int ops;
    int cps;
    char *obuf = NULL;
    char *lbuf;
    char *nbuf;
    char *hbuf = NULL;
    char *ohbuf;
    FILE *inf = NULL;
    char *fname;
    int mor;
    int error = 1;

    if (nmwd)
	num = nmwd + 1;
    else
	num = 0;
    off = num + offst;

    /*
     * allocate line buffer
     */
    if ((obuf = malloc((unsigned)(LBUF + off)*sizeof(char))) == NULL)
	goto oomem;

    /*
     * allocate header buffer
     */
    if ((hbuf = malloc((unsigned)(HDBUF + offst)*sizeof(char))) == NULL)
	goto oomem;

    ohbuf = hbuf + offst;
    nbuf = obuf + offst;
    lbuf = nbuf + num;

    if (num)
	nbuf[--num] = nmchar;

    if (offst) {
	(void)memset(obuf, (int)' ', offst);
	(void)memset(hbuf, (int)' ', offst);
    }

    /*
     * loop by file
     */
    while ((inf = nxtfile(argc, argv, &fname, ohbuf, 0)) != NULL) {
	pagecnt = 0;
	lncnt = 0;

	/*
	 * loop by "form"
	 */
	for(;;) {

	    /*
	     * loop by page
	     */
	    for(;;) {
		linecnt = 0;
		lrgln = 0;
		ops = 0;
		ips = 0;
		cps = 0;

		/*
		 * loop by line
		 */
		while (linecnt < lines) {

		    /*
		     * input next line
		     */
		    rc = inln(inf,lbuf,LBUF,&cnt,&cps,0,&mor);
		    if (cnt >= 0) {
			if (!lrgln)
			    if (!linecnt && prhead(hbuf, fname, ++pagecnt))
			         goto out;

			/*
			 * start new line or continue a long one
			 */
			if (!lrgln) {
			    if (num)
				addnum(nbuf, num, ++lncnt);
			    if (otln(obuf,cnt+off, &ips, &ops, mor))
				goto out;
			} else
			    if (otln(lbuf, cnt, &ips, &ops, mor))
				goto out;

			/*
			 * if line bigger than buffer, get more
			 */
			if (mor) {
			    lrgln = 1;
			} else {
			    /*
			     * whole line rcvd. reset tab proc. state
			     */
			    ++linecnt;
			    lrgln = 0;
			    ops = 0;
			    ips = 0;
			}
		    }

		    if (rc != NORMAL)
			break;
		}

		/*
		 * fill to end of page
		 */
		if (prtail(lines - linecnt, lrgln))
		    goto out;

		/*
		 * unless END continue
		 */
		if (rc == END)
		    break;
	    }

	    /*
	     * On EOF go to next file
	     */
	    if (rc == END)
	    break;
	}

	if (inf != stdin)
	    (void)fclose(inf);
    }
    /*
     * If we didn't process all the files, return error
     */
    if (eoptind < argc) {
	goto out;
    } else {
	error = 0;
	goto out;
    }

oomem:
    mfail();
out:
    free(obuf);
    free(hbuf);
    if (inf != NULL && inf != stdin)
	(void)fclose(inf);
    return error;
}

/*
 * vertcol:	print files with more than one column of output down a page
 *		the general approach is to buffer a page of data, then print
 */
int
vertcol(int argc, char *argv[])
{
    char *ptbf;
    char **lstdat = NULL;
    int i;
    int j;
    int pln;
    int *indy = NULL;
    int cnt;
    int rc;
    int cvc;
    int *lindy = NULL;
    int lncnt;
    int stp;
    int pagecnt;
    int col = colwd + 1;
    int mxlen = pgwd + offst + 1;
    int mclcnt = clcnt - 1;
    struct vcol *vc = NULL;
    int mvc;
    int tvc;
    int cw = nmwd + 1;
    int fullcol;
    char *buf = NULL;
    char *hbuf = NULL;
    char *ohbuf;
    char *fname;
    FILE *inf = NULL;
    int ips = 0;
    int cps = 0;
    int ops = 0;
    int mor = 0;
    int error = 1;

    /*
     * allocate page buffer
     */
    if ((buf = calloc((unsigned)lines, mxlen)) == NULL)
	goto oomem;

    /*
     * allocate page header
     */
    if ((hbuf = malloc((unsigned)HDBUF + offst)) == NULL)
	goto oomem;

    ohbuf = hbuf + offst;
    if (offst)
	(void)memset(hbuf, (int)' ', offst);

    /*
     * col pointers when no headers
     */
    mvc = lines * clcnt;
    if ((vc=(struct vcol *)calloc((unsigned)mvc, sizeof(struct vcol))) == NULL)
	goto oomem;

    /*
     * pointer into page where last data per line is located
     */
    if ((lstdat = (char **)calloc((unsigned)lines, sizeof(char *))) == NULL)
	goto oomem;

    /*
     * fast index lookups to locate start of lines
     */
    if ((indy = (int *)calloc((unsigned)lines, sizeof(int))) == NULL)
	goto oomem;
    if ((lindy = (int *)calloc((unsigned)lines, sizeof(int))) == NULL)
	goto oomem;

    if (nmwd)
	fullcol = col + cw;
    else
	fullcol = col;

    /*
     * initialize buffer lookup indexes and offset area
     */
    for (j = 0; j < lines; ++j) {
	lindy[j] = j * mxlen;
	indy[j] = lindy[j] + offst;
	if (offst) {
	    ptbf = buf + lindy[j];
	    (void)memset(ptbf, (int)' ', offst);
	    ptbf += offst;
	} else
	    ptbf = buf + indy[j];
	lstdat[j] = ptbf;
    }

    /*
     * loop by file
     */
    while ((inf = nxtfile(argc, argv, &fname, ohbuf, 0)) != NULL) {
	pagecnt = 0;
	lncnt = 0;

	/*
	 * loop by "form"
	 */
	 for (;;) {

	    /*
	     * loop by page
	     */
	    for(;;) {

		/*
		 * loop by column
		 */
		cvc = 0;
		for (i = 0; i < clcnt; ++i) {
		    j = 0;
		    /*
		     * if last column, do not pad
		     */
		    if (i == mclcnt)
			stp = 1;
		    else
			stp = 0;

		    /*
		     * loop by line
		     */
		    for(;;) {
			/*
			 * is this first column
			 */
			if (!i) {
			    ptbf = buf + indy[j];
			    lstdat[j] = ptbf;
			} else 
			    ptbf = lstdat[j];
			vc[cvc].pt = ptbf;

			/*
			 * add number
			 */
			if (nmwd) {
			    addnum(ptbf, nmwd, ++lncnt);
			    ptbf += nmwd;
			    *ptbf++ = nmchar;
			}

			/*
			 * input next line
			 */
			rc = inln(inf,ptbf,colwd,&cnt,&cps,1,&mor);
			vc[cvc++].cnt = cnt;
			if (cnt >= 0) {
			    ptbf += cnt;

			    /*
			     * pad all but last column on page
			     */
			    if (!stp) {
				/*
				 * pad to end of column
				 */
				if (sflag)
				    *ptbf++ = schar;
				else if ((pln = col-cnt) > 0) {
				    (void)memset(ptbf,
					(int)' ',pln);
				    ptbf += pln;
				}
			    }

			    /*
			     * remember last char in line
			     */
			    lstdat[j] = ptbf;
			    if (++j >= lines)
				break;
			} /* end of if cnt >= 0 */

			if (rc != NORMAL)
			    break;
		    } /* end of for line */

		    if (rc != NORMAL)
			break;
		} /* end of for column */

		/*
		 * when -t (no header) is specified the spec requires
		 * the min number of lines. The last page may not have
		 * balanced length columns. To fix this we must reorder
		 * the columns. This is a very slow technique so it is
		 * only used under limited conditions. Without -t, the
		 * balancing of text columns is unspecified. To NOT
		 * balance the last page, add the global variable
		 * nohead to the if statement below e.g.
		 */

		/*
		 * print header iff we got anything on the first read
		 */
		if (vc[0].cnt >= 0) {
		    if (prhead(hbuf, fname, ++pagecnt))
		    	goto out;

		    /*
		     * check to see if "last" page needs to be reordered
		     */
		    --cvc;
		    if ((rc != NORMAL) && cvc && ((mvc-cvc) >= clcnt)){
			pln = cvc/clcnt;
			if (cvc % clcnt)
			    ++pln;

			for (i = 0; i < pln; ++i) {
			    ips = 0;
			    ops = 0;
			    if (offst && otln(buf,offst,&ips,&ops,1))
				goto out;
			    tvc = i;

			    for (j = 0; j < clcnt; ++j) {
				/*
				 * determine column length
				 */
				if (j == mclcnt) {
				    /*
				     * last column
				     */
				    cnt = vc[tvc].cnt;
				    if (nmwd)
					cnt += cw;
				} else if (sflag) {
				    /*
				     * single ch between
				     */
				    cnt = vc[tvc].cnt + 1;
				    if (nmwd)
					cnt += cw;
				} else
				    cnt = fullcol;

				if (otln(vc[tvc].pt, cnt, &ips, &ops, 1))
				    goto out;
				tvc += pln;
				if (tvc > cvc)
				    break;
			    }
			    /*
			     * terminate line
			     */
			    if (otln(buf, 0, &ips, &ops, 0))
				goto out;
			}

		    } else {

			/*
			 * just a normal page...
			 * determine how many lines to output
			 */
			if (i > 0)
			    pln = lines;
			else
			    pln = j;

			/*
			 * output each line
			 */
			for (i = 0; i < pln; ++i) {
			    ptbf = buf + lindy[i];
			    if ((j = lstdat[i] - ptbf) <= offst)
				break;
			    else {
				ips = 0;
				ops = 0;
				if (otln(ptbf, j, &ips, &ops, 0))
				    goto out;
			    }
			}
		    }
		}

		/*
		 * pad to end of page
		 */
		if (prtail((lines - pln), 0))
		    goto out;

		/*
		 * if FORM continue
		 */
		if (rc != NORMAL)
		    break;
	    }

	    /*
	     * if EOF go to next file
	     */
	    if (rc == END)
		break;
	}

	if (inf != stdin)
	    (void)fclose(inf);
    }

    if (eoptind < argc){
	goto out;
    } else {
	error = 0;
	goto out;
    }

oomem:
    mfail();
out:
    free(buf);
    free(hbuf);
    free(vc);
    free(lstdat);
    free(lindy);
    if (inf != NULL && inf != stdin)
	(void)fclose(inf);
    return error;

}

/*
 * horzcol:    print files with more than one column of output across a page
 */
int
horzcol(int argc, char *argv[])
{
    char *ptbf;
    int pln;
    char *lstdat;
    int col = colwd + 1;
    int j;
    int i;
    int cnt;
    int rc;
    int lncnt;
    int pagecnt;
    char *buf = NULL;
    char *hbuf = NULL;
    char *ohbuf;
    char *fname;
    FILE *inf = NULL;
    int cps = 0;
    int mor = 0;
    int ips = 0;
    int ops = 0;
    int error = 1;

    if ((buf = malloc((unsigned)pgwd + offst + 1)) == NULL)
	goto oomem;

    /*
     * page header
     */
    if ((hbuf = malloc((unsigned)HDBUF + offst)) == NULL)
	goto oomem;

    ohbuf = hbuf + offst;
    if (offst) {
	(void)memset(buf, (int)' ', offst);
	(void)memset(hbuf, (int)' ', offst);
    }

    /*
     * loop by file
     */
    while ((inf = nxtfile(argc, argv, &fname, ohbuf, 0)) != NULL) {
	pagecnt = 0;
	lncnt = 0;

	/*
	 * loop by form
	 */
	for (;;) {

	    /*
	     * loop by page
	     */
	    for(;;) {

		/*
		 * loop by line
		 */
		for (i = 0; i < lines; ++i) {
		    ptbf = buf + offst;
		    lstdat = ptbf;
		    j = 0;

		    /*
		     * loop by col
		     */
		    for(;;) {
			if (nmwd) {
			    /*
			     * add number to column
			     */
			    addnum(ptbf, nmwd, ++lncnt);
			    ptbf += nmwd;
			    *ptbf++ = nmchar;
			}
			/*
			 * input line
			 */
			rc = inln(inf,ptbf,colwd,&cnt,&cps,1, &mor);
			if (cnt >= 0) {
			    if (!i && !j && prhead(hbuf, fname, ++pagecnt))
			        goto out;

			    ptbf += cnt;
			    lstdat = ptbf;

			    /*
			     * if last line skip padding
			     */
			    if (++j >= clcnt)
				break;

			    /*
			     * pad to end of column
			     */
			    if (sflag)
				*ptbf++ = schar;
			    else if ((pln = col - cnt) > 0) {
				(void)memset(ptbf,(int)' ',pln);
				ptbf += pln;
			    }
			}
			if (rc != NORMAL)
			    break;
		    }

		    /*
		     * output line if any columns on it
		     */
		    if (j) {
			if (otln(buf, lstdat-buf, &ips, &ops, 0))
			    goto out;
		    }

		    if (rc != NORMAL)
			break;
		}

		/*
		 * pad to end of page
		 */
		if (prtail(lines - i, 0))
		    return(1);

		/*
		 * if FORM continue
		 */
		if (rc == END)
		    break;
	    }
	    /*
	     * if EOF go to next file
	     */
	    if (rc == END)
		break;
	}
	if (inf != stdin)
	    (void)fclose(inf);
    }
    if (eoptind < argc){
	goto out;
    } else {
	error = 0;
	goto out;
    }

oomem:
    mfail();
out:
    free(buf);
    free(hbuf);
    if (inf != NULL && inf != stdin)
	(void)fclose(inf);
    return error;
}

struct ferrlist {
	struct ferrlist *next;
	char *buf;
};
struct ferrlist *ferrhead, *ferrtail;

/*
 * flsh_errs():    output saved up diagnostic messages after all normal
 *        processing has completed
 */
void
flsh_errs(void)
{
    struct ferrlist *f;

    if (ferr) {
	for (f = ferrhead; f; f = f->next)
	    (void)write(STDERR_FILENO, f->buf, strlen(f->buf));
    }
}

static void
ferrout(char *fmt, ...)
{
    sigset_t block, oblock;
    struct ferrlist *f;
    va_list ap;
    char *p;

    va_start(ap, fmt);
    if (ferr == 0)
        vfprintf(stderr, fmt, ap);
    else {
	sigemptyset(&block);
	sigaddset(&block, SIGINT);
	sigprocmask(SIG_BLOCK, &block, &oblock);

	if (vasprintf(&p, fmt, ap) == -1 || (f = malloc(sizeof(*f))) == NULL) {
		flsh_errs();
		fprintf(stderr, fmt, ap);
		fputs("pr: memory allocation failed\n", stderr);
		exit(1);
	}

	f->next = NULL;
	f->buf = p;
	if (ferrhead == NULL)
	    ferrhead = f;
	if (ferrtail)
		ferrtail->next = f;
	ferrtail = f;
	sigprocmask(SIG_SETMASK, &oblock, NULL);
    }
    va_end(ap);
}

/*
 * mulfile:    print files with more than one column of output and
 *        more than one file concurrently
 */
int
mulfile(int argc, char *argv[])
{
    char *ptbf;
    int j;
    int pln;
    int *rc;
    int cnt;
    char *lstdat;
    int i;
    FILE **fbuf = NULL;
    int actf;
    int lncnt;
    int col;
    int pagecnt;
    int fproc;
    char *buf = NULL;
    char *hbuf = NULL;
    char *ohbuf;
    char *fname;
    int ips = 0;
    int cps = 0;
    int ops = 0;
    int mor = 0;
    int error = 1;

    /*
     * array of FILE *, one for each operand
     */
    if ((fbuf = (FILE **)calloc((unsigned)clcnt, sizeof(FILE *))) == NULL)
	goto oomem;

    /*
     * array of int *, one for each operand
     */
    if ((rc = (int *)calloc((unsigned)clcnt, sizeof(int))) == NULL)
	goto oomem;

    /*
     * page header
     */
    if ((hbuf = malloc((unsigned)HDBUF + offst)) == NULL)
	goto oomem;

    ohbuf = hbuf + offst;

    /*
     * do not know how many columns yet. The number of operands provide an
     * upper bound on the number of columns. We use the number of files
     * we can open successfully to set the number of columns. The operation
     * of the merge operation (-m) in relation to unsuccessful file opens
     * is unspecified by posix.
     *
     * XXX - this seems moderately bogus, you'd think that specifying
     * "pr -2 a b c d" would run though all the files in pairs, but
     * the existing code says up two files, or fewer if one is bogus.
     * fixing it would require modifying the looping structure, so be it.
     */
    j = 0;
    while (j < clcnt) {
	if ((fbuf[j] = nxtfile(argc, argv, &fname, ohbuf, 1)) != NULL) {
	    rc[j] = NORMAL;
	    j++;
	}
    }

    /*
     * if no files, exit
     */
    if (j)
	clcnt = j;
    else
	goto out;

    /*
     * calculate page boundaries based on open file count
     */
    if (nmwd) {
	colwd = (pgwd - clcnt - nmwd)/clcnt;
	pgwd = ((colwd + 1) * clcnt) - nmwd - 2;
    } else {
	colwd = (pgwd + 1 - clcnt)/clcnt;
	pgwd = ((colwd + 1) * clcnt) - 1;
    }
    if (colwd < 1) {
	ferrout("pr: page width too small for %d columns\n", clcnt);
	goto out;
    }
    col = colwd + 1;

    /*
     * line buffer
     */
    if ((buf = malloc((unsigned)pgwd + offst + 1)) == NULL)
	goto oomem;

    if (offst) {
	(void)memset(buf, (int)' ', offst);
	(void)memset(hbuf, (int)' ', offst);
    }

    pagecnt = 0;
    lncnt = 0;
    actf = clcnt;

    /*
     * continue to loop while any file still has data
     */
    while (actf > 0) {

	/*
	 * loop on "form"
	 */
	for (;;) {

	    /*
	     * loop by line
	     */
	    for (i = 0; i < lines; ++i) {
		ptbf = buf + offst;
		lstdat = ptbf;
		if (nmwd) {
		    /*
		     * add line number to line
		     */
		    addnum(ptbf, nmwd, ++lncnt);
		    ptbf += nmwd;
		    *ptbf++ = nmchar;
		}

		fproc = 0;
		/*
		 * loop by column
		 */
		for (j = 0; j < clcnt; ++j) {
		    if (rc[j] == NORMAL ) {
			rc[j] = inln(fbuf[j], ptbf, colwd, &cnt, &cps, 1, &mor);
			if (cnt >= 0) {
			    /*
			     * process file data
			     */
			    ptbf += cnt;
			    lstdat = ptbf;
			    fproc++;
			} else
			    cnt = 0;
			
			if (rc[j] == END) {
			    /*
			     * EOF close file
			     */
			    if (fbuf[j] != stdin)
				(void)fclose(fbuf[j]);
			    --actf;
			}
		    } else
			cnt = 0;

		    /*
		     * if last ACTIVE column, done with line
		     */
		    if (fproc >= actf)
			break;

		    /*
		     * pad to end of column
		     */
		    if (sflag) {
			*ptbf++ = schar;
		    } else {
			if (cnt >= 0)
			    pln = col - cnt;
			else
			    pln = col;
			if (pln > 0) {
			    (void)memset(ptbf, (int)' ', pln);
			    ptbf += pln;
			}
		    }
		}

		/*
		 * if there was anything to do, print it
		 */
		if (fproc != 0) {
		    if (!i && prhead(hbuf, fname, ++pagecnt))
			goto out;

		    /*
		     * output line
		     */
		    if (otln(buf, lstdat-buf, &ips, &ops, 0))
			goto out;
		} else
		    break;
	    }

	    /*
	     * pad to end of page
	     */
	    if (prtail(lines - i, 0))
		return(1);

	    for (j = 0; j < clcnt; ++j)
		if (rc[j] != END)
		    rc[j] = NORMAL;

	    if (actf <= 0)
		break;
	}
	if (actf <= 0)
	break;
    }
    if (eoptind < argc){
	goto out;
    } else {
	error = 0;
	goto out;
    }

oomem:
	mfail();
out:
    if (fbuf) {
	for (j = 0; j < clcnt; j++) {
	    if (fbuf[j] && fbuf[j] != stdin)
		(void)fclose(fbuf[j]);
	}
	free(fbuf);
    }
    free(hbuf);
    free(buf);
    return error;
}

/*
 * inln():    input a line of data (unlimited length lines supported)
 *        Input is optionally expanded to spaces
 *        Returns 0 if normal LF, FORM on Formfeed, and END on EOF
 *
 *    inf:    file
 *    buf:    buffer
 *    lim:    buffer length
 *    cnt:    line length or -1 if no line (EOF for example)
 *    cps:    column position 1st char in buffer (large line support)
 *    trnc:    throw away data more than lim up to \n 
 *    mor:    set if more data in line (not truncated)
 */
int
inln(FILE *inf, char *buf, int lim, int *cnt, int *cps, int trnc, int *mor)
{
    int col;
    int gap = ingap;
    int ch = -1;
    char *ptbuf;
    int chk = (int)inchar;

    ptbuf = buf;

    if (gap) {
	/*
	 * expanding input option
	 */
	while ((--lim >= 0) && ((ch = getc(inf)) != EOF)) {
	    /*
	     * is this the input "tab" char
	     */
	    if (ch == chk) {
		/*
		 * expand to number of spaces
		 */
		col = (ptbuf - buf) + *cps;
		col = gap - (col % gap);

		/*
		 * if more than this line, push back
		 */
		if ((col > lim) && (ungetc(ch, inf) == EOF)) {
		    *cnt = -1;
		    return(END);    /* shouldn't happen */
		}

		/*
		 * expand to spaces
		 */
		while ((--col >= 0) && (--lim >= 0))
		    *ptbuf++ = ' ';
		continue;
	    }
	    if (ch == '\n' || (inform && ch == INFF))
		break;
	    *ptbuf++ = ch;
	}
    } else {
	/*
	 * no expansion
	 */
	while ((--lim >= 0) && ((ch = getc(inf)) != EOF)) {
	    if (ch == '\n' || (inform && ch == INFF))
		break;
	    *ptbuf++ = ch;
	}
    }
    col = ptbuf - buf;
    if (ch == EOF) {
	*mor = 0;
	*cps = 0;
	*cnt = col ? col : -1;
	return(END);
    }
    if (inform && ch == INFF) {
	*mor = 0;
	*cps = 0;
	*cnt = col;
	return(FORM);
    }
    if (ch == '\n') {
	/*
	 * entire line processed
	 */
	*mor = 0;
	*cps = 0;
	*cnt = col;
	return(NORMAL);
    }

    /*
     * line was larger than limit
     */
    if (trnc) {
	/*
	 * throw away rest of line
	 */
	while ((ch = getc(inf)) != EOF) {
	    if (ch == '\n')
		break;
	}
	*cps = 0;
	*mor = 0;
    } else {
	/*
	 * save column offset if not truncated
	 */
	*cps += col;
	*mor = 1;
    }

    *cnt = col;
    return(NORMAL);
}

/*
 * otln():    output a line of data. (Supports unlimited length lines)
 *        output is optionally contracted to tabs
 *
 *    buf:    output buffer with data
 *    cnt:    number of chars of valid data in buf
 *    svips:    buffer input column position (for large lines)
 *    svops:    buffer output column position (for large lines)
 *    mor:    output line not complete in this buf; more data to come.    
 *        1 is more, 0 is complete, -1 is no \n's
 */
int
otln(char *buf, int cnt, int *svips, int *svops, int mor)
{
    int ops;        /* last col output */
    int ips;        /* last col in buf examined */
    int gap = ogap;
    int tbps;
    char *endbuf;

    /* skipping is only changed at header time not mid-line! */
    if (skipping)
	return (0);

    if (ogap) {
	/*
	 * contracting on output
	 */
	endbuf = buf + cnt;
	ops = *svops;
	ips = *svips;
	while (buf < endbuf) {
	    /*
	     * count number of spaces and ochar in buffer
	     */
	    if (*buf == ' ') {
		++ips;
		++buf;
		continue;
	    }

	    /*
	     * simulate ochar processing
	     */
	    if (*buf == ochar) {
		ips += gap - (ips % gap);
		++buf;
		continue;
	    }

	    /*
	     * got a non space char; contract out spaces
	     */
	    while (ops < ips) {
		/*
		 * use one space if necessary
		 */
		if (ips - ops == 1) {
			putchar(' ');
			break;
		}
		/*
		 * use as many ochar as will fit
		 */
		if ((tbps = ops + gap - (ops % gap)) > ips)
		    break;
		if (putchar(ochar) == EOF) {
		    pfail();
		    return(1);
		}
		ops = tbps;
	    }

	    while (ops < ips) {
		/*
		 * finish off with spaces
		 */
		if (putchar(' ') == EOF) {
		    pfail();
		    return(1);
		}
		++ops;
	    }

	    /*
	     * output non space char
	     */
	    if (putchar(*buf++) == EOF) {
		pfail();
		return(1);
	    }
	    ++ips;
	    ++ops;
	}

	if (mor > 0) {
	    /*
	     * if incomplete line, save position counts
	     */
	    *svops = ops;
	    *svips = ips;
	    return(0);
	}

	if (mor < 0) {
	    while (ops < ips) {
		/*
		 * use one space if necessary
		 */
		if (ips - ops == 1) {
			putchar(' ');
			break;
		}
		/*
		 * use as many ochar as will fit
		 */
		if ((tbps = ops + gap - (ops % gap)) > ips)
		    break;
		if (putchar(ochar) == EOF) {
		    pfail();
		    return(1);
		}
		ops = tbps;
	    }

	    while (ops < ips) {
		/*
		 * finish off with spaces
		 */
		if (putchar(' ') == EOF) {
		    pfail();
		    return(1);
		}
		++ops;
	    }
	    return(0);
	}
    } else {
	/*
	 * output is not contracted
	 */
	if (cnt && (fwrite(buf, sizeof(char), cnt, stdout) < cnt)) {
	    pfail();
	    return(1);
	}
	if (mor != 0)
	    return(0);
    }

    /*
     * process line end and double space as required
     */
    if ((putchar('\n') == EOF) || (dspace && (putchar('\n') == EOF))) {
	pfail();
	return(1);
    }
    return(0);
}

#ifdef notused
/*
 * inskip():    skip over pgcnt pages with lncnt lines per page
 *        file is closed at EOF (if not stdin).
 *
 *    inf    FILE * to read from
 *    pgcnt    number of pages to skip
 *    lncnt    number of lines per page
 */
int
inskip(FILE *inf, int pgcnt, int lncnt)
{
    int c;
    int cnt;

    while(--pgcnt > 0) {
	cnt = lncnt;
	while ((c = getc(inf)) != EOF) {
	    if ((c == '\n') && (--cnt == 0))
		break;
	}
	if (c == EOF) {
	    if (inf != stdin)
		(void)fclose(inf);
	    return(1);
	}
    }
    return(0);
}
#endif

/*
 * nxtfile:    returns a FILE * to next file in arg list and sets the
 *        time field for this file (or current date).
 *
 *    buf    array to store proper date for the header.
 *    dt    if set skips the date processing (used with -m)
 */
FILE *
nxtfile(int argc, char *argv[], char **fname, char *buf, int dt)
{
    FILE *inf = NULL;
    struct timeval tv;
    struct timezone tz;
    struct tm *timeptr = NULL;
    struct stat statbuf;
    time_t curtime;
    static int twice = -1;

    ++twice;
    if (eoptind >= argc) {
	/*
	 * no file listed; default, use standard input
	 */
	if (twice)
	    return(NULL);
	clearerr(stdin);
	inf = stdin;
	if (header != NULL)
	    *fname = header;
	else
	    *fname = FNAME;
	if (nohead)
	    return(inf);
	if (gettimeofday(&tv, &tz) < 0) {
	    ++errcnt;
	    ferrout("pr: cannot get time of day, %s\n",
		strerror(errno));
	    eoptind = argc - 1;
	    return(NULL);
	}
	curtime = tv.tv_sec;
	timeptr = localtime(&curtime);
    }
    for (; eoptind < argc; ++eoptind) {
	if (strcmp(argv[eoptind], "-") == 0) {
	    /*
	     * process a "-" for filename
	     */
	    clearerr(stdin);
	    inf = stdin;
	    if (header != NULL)
		*fname = header;
	    else
		*fname = FNAME;
	    ++eoptind;
	    if (nohead || (dt && twice))
		return(inf);
	    if (gettimeofday(&tv, &tz) < 0) {
		++errcnt;
		ferrout("pr: cannot get time of day, %s\n",
		    strerror(errno));
		return(NULL);
	    }
	    curtime = tv.tv_sec;
	    timeptr = localtime(&curtime);
	} else {
	    /*
	     * normal file processing
	     */
	    if ((inf = fopen(argv[eoptind], "r")) == NULL) {
		++errcnt;
		if (nodiag)
		    continue;
		ferrout("pr: Cannot open %s, %s\n",
		    argv[eoptind], strerror(errno));
		continue;
	    }
	    if (header != NULL)
		*fname = header;
	    else if (dt)
		*fname = FNAME;
	    else
		*fname = argv[eoptind];
	    ++eoptind;
	    if (nohead || (dt && twice))
		return(inf);

	    if (dt) {
		if (gettimeofday(&tv, &tz) < 0) {
		    ++errcnt;
		    ferrout("pr: cannot get time of day, %s\n",
			 strerror(errno));
		    return(NULL);
		}
		curtime = tv.tv_sec;
		timeptr = localtime(&curtime);
	    } else {
		if (fstat(fileno(inf), &statbuf) < 0) {
		    ++errcnt;
		    (void)fclose(inf);
		    ferrout("pr: Cannot stat %s, %s\n",
			argv[eoptind], strerror(errno));
		    return(NULL);
		}
		timeptr = localtime(&(statbuf.st_mtime));
	    }
	}
	break;
    }
    if (inf == NULL)
	return(NULL);

    /*
     * set up time field used in header
     */
    if (strftime(buf, HDBUF, timefrmt, timeptr) == 0) {
	++errcnt;
	if (inf != stdin)
	    (void)fclose(inf);
	ferrout("pr: time conversion failed\n");
	return(NULL);
    }
    return(inf);
}

/*
 * addnum():    adds the line number to the column
 *        Truncates from the front or pads with spaces as required.
 *        Numbers are right justified.
 *
 *    buf    buffer to store the number
 *    wdth    width of buffer to fill
 *    line    line number
 *
 *        NOTE: numbers occupy part of the column. The posix
 *        spec does not specify if -i processing should or should not
 *        occur on number padding. The spec does say it occupies
 *        part of the column. The usage of addnum    currently treats
 *        numbers as part of the column so spaces may be replaced.
 */
void
addnum(char *buf, int wdth, int line)
{
    char *pt = buf + wdth;

    do {
	*--pt = digs[line % 10];
	line /= 10;
    } while (line && (pt > buf));

    /*
     * pad with space as required
     */
    while (pt > buf)
	*--pt = ' ';
}

/*
 * prhead():    prints the top of page header
 *
 *    buf    buffer with time field (and offset)
 *    cnt    number of chars in buf
 *    fname    fname field for header
 *    pagcnt    page number
 *
 * prhead() should be used carefully, we don't want to print out headers
 * for null input files or orphan headers at the end of files, and also
 * trailer processing is typically conditional on whether you've called
 * prhead() at least once for a file and incremented pagecnt.  Exactly
 * how to determine whether to print a header is a little different in
 * the context each output mode, but we let the caller figure that out.
 */
int
prhead(char *buf, char *fname, int pagcnt)
{
    int ips = 0;
    int ops = 0;

    beheaded = 1;

    if (skipping && pagcnt >= pgnm)
	skipping = 0;

    if (nohead || skipping)
	return (0);

    if ((putchar('\n') == EOF) || (putchar('\n') == EOF)) {
	pfail();
	return(1);
    }
    /*
     * posix is not clear if the header is subject to line length
     * restrictions. The specification for header line format
     * in the spec clearly does not limit length. No pr currently
     * restricts header length. However if we need to truncate in
     * an reasonable way, adjust the length of the printf by
     * changing HDFMT to allow a length max as an argument printf.
     * buf (which contains the offset spaces and time field could
     * also be trimmed
     *
     * note only the offset (if any) is processed for tab expansion
     */
    if (offst && otln(buf, offst, &ips, &ops, -1))
	return(1);
    (void)printf(HDFMT,buf+offst, fname, pagcnt);
    return(0);
}

/*
 * prtail():    pad page with empty lines (if required) and print page trailer
 *        if requested
 *
 *    cnt    	number of lines of padding needed
 *    incomp    was a '\n' missing from last line output
 *
 * prtail() can now be invoked unconditionally, with the notion that if
 * we haven't printed a header, there is no need for a trailer
 */
int
prtail(int cnt, int incomp)
{
    /*
     * if were's skipping to page N or haven't put out anything yet just exit
     */
    if (skipping || beheaded == 0)
	return (0);
    beheaded = 0;

    /*
     * if noheaders, only terminate an incomplete last line
     */
    if (nohead) {

	if (incomp) {
	    if (dspace)
		if (putchar('\n') == EOF) {
		    pfail();
		    return(1);
		}
	    if (putchar('\n') == EOF) {
		pfail();
		return(1);
	     }
	}
	/*
	 * but honor the formfeed request
	 */
	if (formfeed)
	    if (putchar(OUTFF) == EOF) {
		pfail();
		return(1);
	    }

    } else {

	/*
	 * if double space output two \n
	 *
  	 * XXX this all seems bogus, why are we doing it here???
	 * page length is in terms of output lines and only the input is
	 * supposed to be double spaced...  otln() users should be doing
	 * something like linect+=(dspace ? 2:1).
	 */
	if (dspace)
	    cnt *= 2;

	/*
	 * if an odd number of lines per page, add an extra \n
	 */
	if (addone)
	    ++cnt;

	/*
	 * either put out a form-feed or pad page with blanks
	 */
	if (formfeed) {
	    if (incomp)
		if (putchar('\n') == EOF) {
		    pfail();
		    return(1);
		}
	    if (putchar(OUTFF) == EOF) {
		    pfail();
		    return(1);
	    }

	} else {

	    if (incomp)
		cnt++;

	    cnt += TAILLEN;
	    while (--cnt >= 0) {
		if (putchar('\n') == EOF) {
		    pfail();
		    return(1);
		}
	    }
	}
    }

    return(0);
}

/*
 * terminate():    when a SIGINT is recvd
 */
/*ARGSUSED*/
void
terminate(int which_sig)
{
    flsh_errs();
    _exit(1);
}

void
mfail(void)
{
    ferrout("pr: memory allocation failed\n");
}

void
pfail(void)
{
    ferrout("pr: write failure, %s\n", strerror(errno));
}

void
usage(void)
{
    ferrout(
     "usage: pr [+page] [-column] [-adFfmrt] [-e [char] [gap]] [-h header]\n");
    ferrout(
     "\t[-i [char] [gap]] [-l lines] [-n [char] [width]] [-o offset]\n");
    ferrout(
     "\t[-s [char]] [-w width] [-] [file ...]\n");
}

/*
 * setup:    Validate command args, initialize and perform sanity 
 *        checks on options
 */
int
setup(int argc, char *argv[])
{
    int c;
    int eflag = 0;
    int iflag = 0;
    int wflag = 0;
    int cflag = 0;
    const char *errstr;

    if (isatty(fileno(stdout)))
	ferr = 1;

    while ((c = egetopt(argc, argv, "#adfFmrte?h:i?l:n?o:s?w:")) != -1) {
	switch (c) {
	case '+':
	    pgnm = strtonum(eoptarg, 1, INT_MAX, &errstr);
	    if (errstr) {
		ferrout("pr: +page number is %s: %s\n", errstr, eoptarg);
		return(1);
	    }
	    ++skipping;
	    break;
	case '-':
	    clcnt = strtonum(eoptarg, 1, INT_MAX, &errstr);
	    if (errstr) {
		ferrout("pr: -columns number is %s: %s\n", errstr, eoptarg);
		return(1);
	    }
	    if (clcnt > 1)
		++cflag;
	    break;
	case 'a':
	    ++across;
	    break;
	case 'd':
	    ++dspace;
	    break;
	case 'e':
	    ++eflag;
	    if ((eoptarg != NULL) && !isdigit(*eoptarg))
		inchar = *eoptarg++;
	    else
		inchar = INCHAR;
	    if ((eoptarg != NULL) && isdigit(*eoptarg)) {
		ingap = strtonum(eoptarg, 0, INT_MAX, &errstr);
		if (errstr) {
		    ferrout("pr: -e gap is %s: %s\n", errstr, eoptarg);
		    return(1);
		}
		if (ingap == 0)
		    ingap = INGAP;
	    } else if ((eoptarg != NULL) && (*eoptarg != '\0')) {
		ferrout("pr: invalid value for -e %s\n", eoptarg);
		return(1);
	    } else
		ingap = INGAP;
	    break;
	case 'f':
	case 'F':
	    ++formfeed;
	    break;
	case 'h':
	    header = eoptarg;
	    break;
	case 'i':
	    ++iflag;
	    if ((eoptarg != NULL) && !isdigit(*eoptarg))
		ochar = *eoptarg++;
	    else
		ochar = OCHAR;
	    if ((eoptarg != NULL) && isdigit(*eoptarg)) {
		ogap = strtonum(eoptarg, 0, INT_MAX, &errstr);
		if (errstr) {
		    ferrout("pr: -i gap is %s: %s\n", errstr, eoptarg);
		    return(1);
		}
		if (ogap == 0)
		    ogap = OGAP;
	    } else if ((eoptarg != NULL) && (*eoptarg != '\0')) {
		ferrout("pr: invalid value for -i %s\n", eoptarg);
		return(1);
	    } else
		ogap = OGAP;
	    break;
	case 'l':
	    lines = strtonum(eoptarg, 1, INT_MAX, &errstr);
	    if (errstr) {
		ferrout("pr: number of lines is %s: %s\n", errstr, eoptarg);
		return(1);
	    }
	    break;
	case 'm':
	    ++merge;
	    break;
	case 'n':
	    if ((eoptarg != NULL) && !isdigit(*eoptarg))
		nmchar = *eoptarg++;
	    else
		nmchar = NMCHAR;
	    if ((eoptarg != NULL) && isdigit(*eoptarg)) {
		nmwd = strtonum(eoptarg, 1, INT_MAX, &errstr);
		if (errstr) {
		    ferrout("pr: -n width is %s: %s\n", errstr, eoptarg);
		    return(1);
		}
	    } else if ((eoptarg != NULL) && (*eoptarg != '\0')) {
		ferrout("pr: invalid value for -n %s\n", eoptarg);
		return(1);
	    } else
		nmwd = NMWD;
	    break;
	case 'o':
	    offst = strtonum(eoptarg, 1, INT_MAX, &errstr);
	    if (errstr) {
		ferrout("pr: -o offset is %s: %s\n", errstr, eoptarg);
		return(1);
	    }
	    break;
	case 'r':
	    ++nodiag;
	    break;
	case 's':
	    ++sflag;
	    if (eoptarg == NULL)
		schar = SCHAR;
	    else {
		schar = *eoptarg++;
		if (*eoptarg != '\0') {
		    ferrout("pr: invalid value for -s %s\n", eoptarg);
		    return(1);
		}
	    }
	    break;
	case 't':
	    ++nohead;
	    break;
	case 'w':
	    ++wflag;
	    pgwd = strtonum(eoptarg, 1, INT_MAX, &errstr);
	    if (errstr) {
		ferrout("pr: -w width is %s: %s\n", errstr, eoptarg);
		return(1);
	    }
	    break;
	default:
	    return(1);
	}
    }

    /*
     * default and sanity checks
     */
    inform++;

    if (!clcnt) {
	if (merge) {
	    if ((clcnt = argc - eoptind) <= 1) {
		clcnt = CLCNT;
#ifdef stupid
		merge = 0;
#endif
	    }
	} else
	    clcnt = CLCNT;
    }
    if (across) {
	if (clcnt == 1) {
	    ferrout("pr: -a flag requires multiple columns\n");
	    return(1);
	}
	if (merge) {
	    ferrout("pr: -m cannot be used with -a\n");
	    return(1);
	}
    }
    if (!wflag) {
	if (sflag)
	    pgwd = SPGWD;
	else
	    pgwd = PGWD;
    }
    if (cflag || merge) {
	if (!eflag) {
	    inchar = INCHAR;
	    ingap = INGAP;
	}
	if (!iflag) {
	    ochar = OCHAR;
	    ogap = OGAP;
	}
    }
    if (cflag) {
	if (merge) {
	    ferrout("pr: -m cannot be used with multiple columns\n");
	    return(1);
	}
	if (nmwd) {
	    colwd = (pgwd + 1 - (clcnt * (nmwd + 2)))/clcnt;
	    pgwd = ((colwd + nmwd + 2) * clcnt) - 1;
	} else {
	    colwd = (pgwd + 1 - clcnt)/clcnt;
	    pgwd = ((colwd + 1) * clcnt) - 1;
	}
	if (colwd < 1) {
	    ferrout("pr: page width is too small for %d columns\n",clcnt);
	    return(1);
	}
    }
    if (!lines)
	lines = LINES;

    /*
     * make sure long enough for headers. if not disable
     */
    if (lines <= HEADLEN + TAILLEN)
	++nohead;    
    else if (!nohead)
	lines -= HEADLEN + TAILLEN;

    /*
     * adjust for double space on odd length pages
     */
    if (dspace) {
	if (lines == 1)
	    dspace = 0;
	else {
	    if (lines & 1)
		++addone;
	    lines /= 2;
	}
    }

    if ((timefrmt = getenv("LC_TIME")) == NULL)
	timefrmt = TIMEFMT;
    return(0);
}
