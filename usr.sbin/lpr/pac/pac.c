/*	$OpenBSD: pac.c,v 1.24 2015/02/09 23:00:14 deraadt Exp $ */
/*	$NetBSD: pac.c,v 1.14 2000/04/27 13:40:18 msaitoh Exp $	*/

/*
 * Copyright (c) 1983, 1993
 *	The Regents of the University of California.  All rights reserved.
 *
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

/*
 * Do Printer accounting summary.
 * Currently, usage is
 *	pac [-Pprinter] [-pprice] [-s] [-r] [-c] [-m] [user ...]
 * to print the usage information for the named people.
 */

#include <ctype.h>
#include <signal.h>
#include <dirent.h>
#include <err.h>
#include <errno.h>
#include <stdlib.h>
#include <unistd.h>
#include <limits.h>
#include <stdio.h>
#include <string.h>

#include "lp.h"
#include "lp.local.h"

static char	*acctfile;	/* accounting file (input data) */
static int	 allflag = 1;	/* Get stats on everybody */
static int	 errs;
static int	 hcount;	/* Count of hash entries */
static int	 mflag = 0;	/* disregard machine names */
static int	 pflag = 0;	/* 1 if -p on cmd line */
static float	 price = 0.02;	/* cost per page (or what ever) */
static long	 price100;	/* per-page cost in 100th of a cent */
static int	 reverse;	/* Reverse sort order */
static int	 sort;		/* Sort by cost */
static char	*sumfile;	/* summary file */
static int	 summarize;	/* Compress accounting file */

volatile sig_atomic_t gotintr;

/*
 * Grossness follows:
 *  Names to be accumulated are hashed into the following
 *  table.
 */

#define	HSHSIZE	97			/* Number of hash buckets */

struct hent {
	struct	hent *h_link;		/* Forward hash link */
	char	*h_name;		/* Name of this user */
	float	h_feetpages;		/* Feet or pages of paper */
	int	h_count;		/* Number of runs */
};

static struct	hent	*hashtab[HSHSIZE];	/* Hash table proper */

static void	account(FILE *);
static int	chkprinter(const char *);
static void	dumpit(void);
static int	hash(const char *);
static struct	hent *enter(const char *);
static struct	hent *lookup(const char *);
static int	qucmp(const void *, const void *);
static void	rewrite(void);
__dead void	usage(void);

int
main(int argc, char **argv)
{
	FILE *acct;
	int ch;

	/* these aren't actually used in pac(1) */
	effective_uid = geteuid();
	real_uid = getuid();
	effective_gid = getegid();
	real_gid = getgid();

	while ((ch = getopt(argc, argv, "P:p:scmr")) != -1) {
		switch (ch) {
		case 'P':
			/*
			 * Printer name.
			 */
			printer = optarg;
			continue;

		case 'p':
			/*
			 * get the price.
			 */
			price = atof(optarg);
			pflag = 1;
			continue;

		case 's':
			/*
			 * Summarize and compress accounting file.
			 */
			summarize = 1;
			continue;

		case 'c':
			/*
			 * Sort by cost.
			 */
			sort = 1;
			continue;

		case 'm':
			/*
			 * disregard machine names for each user
			 */
			mflag = 1;
			continue;

		case 'r':
			/*
			 * Reverse sorting order.
			 */
			reverse = 1;
			continue;

		default:
			usage();
			/* NOTREACHED */
		}
	}
	argc -= optind;
	argv += optind;

	/*
	 * If there are any arguments left, they're names of users
	 * we want to print info for. In that case, put them in the hash
	 * table and unset allflag.
	 */
	for( ; argc > 0; argc--, argv++) {
		(void)enter(*argv);
		allflag = 0;
	}

	if (printer == NULL && (printer = getenv("PRINTER")) == NULL)
		printer = DEFLP;
	if (!chkprinter(printer))
		errx(2, "unknown printer: %s", printer);

	if ((acct = fopen(acctfile, "r")) == NULL)
		err(1, "%s", acctfile);
	account(acct);
	fclose(acct);
	if ((acct = fopen(sumfile, "r")) != NULL) {
		account(acct);
		fclose(acct);
	}
	if (summarize)
		rewrite();
	else
		dumpit();
	exit(errs);
}

/*
 * Read the entire accounting file, accumulating statistics
 * for the users that we have in the hash table.  If allflag
 * is set, then just gather the facts on everyone.
 * Note that we must accommodate both the active and summary file
 * formats here.
 * The Format of the accounting file is:
 *     feet_per_page   [runs_count] [hostname:]username
 * Some software relies on whitespace between runs_count and hostname:username
 * being optional (such as Ghostscript's unix-lpr.sh).
 *
 * Host names are ignored if the -m flag is present.
 */
static void
account(FILE *acct)
{
	char linebuf[BUFSIZ];
	double t;
	long l;
	char *cp, *cp2, *ep;
	struct hent *hp;
	int ic;

	while (fgets(linebuf, sizeof(linebuf), acct) != NULL) {
		cp = linebuf;
		while (isspace((unsigned char)*cp))
			cp++;

		/* get t, feet_per_page */
		errno = 0;
		t = strtod(cp, &ep);
		if (!isspace((unsigned char)*ep) || errno == ERANGE)
			continue;

		/* get ic, runs_count (optional) */
		for (cp = ep + 1; isspace((unsigned char)*cp); )
			cp++;
		l = strtol(cp, &ep, 10);
		if (cp == ep)
			l = 0;		/* runs_count not specified */
		else if (l < 0 || l >= INT_MAX)
			continue;
		ic = (int)l;

		/* get [hostname:]username */
		for (cp = ep; isspace((unsigned char)*cp); cp++)
			;
		for (cp2 = cp; *cp2 && !isspace((unsigned char)*cp2); cp2++)
			;
		*cp2 = '\0';
		/* if -m was specified, don't use the hostname part */
		if (mflag && (cp2 = strchr(cp, ':')) != NULL)
		    cp = cp2 + 1;

		hp = lookup(cp);
		if (hp == NULL) {
			if (!allflag)
				continue;
			hp = enter(cp);
		}
		hp->h_feetpages += t;
		if (ic)
			hp->h_count += ic;
		else
			hp->h_count++;
	}
}

/*
 * Sort the hashed entries by name or footage
 * and print it all out.
 */
static void
dumpit(void)
{
	struct hent **base;
	struct hent *hp, **ap;
	int hno, c, runs;
	float feet;

	hp = hashtab[0];
	hno = 1;
	base = (struct hent **) calloc(hcount, sizeof(hp));
	if (base == NULL)
		err(1, NULL);
	for (ap = base, c = hcount; c--; ap++) {
		while (hp == NULL)
			hp = hashtab[hno++];
		*ap = hp;
		hp = hp->h_link;
	}
	qsort(base, hcount, sizeof hp, qucmp);
	printf("  Login               pages/feet   runs    price\n");
	feet = 0.0;
	runs = 0;
	for (ap = base, c = hcount; c--; ap++) {
		hp = *ap;
		runs += hp->h_count;
		feet += hp->h_feetpages;
		printf("%-24s %7.2f %4d   $%6.2f\n", hp->h_name,
		    hp->h_feetpages, hp->h_count, hp->h_feetpages * price);
	}
	if (allflag) {
		printf("\n");
		printf("%-24s %7.2f %4d   $%6.2f\n", "total", feet, 
		    runs, feet * price);
	}

	free(base);
}

/*
 * Rewrite the summary file with the summary information we have accumulated.
 */
static void
rewrite(void)
{
	struct hent *hp;
	int i;
	FILE *acctf;

	if ((acctf = fopen(sumfile, "w")) == NULL) {
		warn("%s", sumfile);
		errs++;
		return;
	}
	for (i = 0; i < HSHSIZE; i++) {
		hp = hashtab[i];
		while (hp != NULL) {
			fprintf(acctf, "%7.2f\t%s\t%d\n", hp->h_feetpages,
			    hp->h_name, hp->h_count);
			hp = hp->h_link;
		}
	}
	fflush(acctf);
	if (ferror(acctf)) {
		warn("%s", sumfile);
		errs++;
	}
	fclose(acctf);
	if ((acctf = fopen(acctfile, "w")) == NULL)
		warn("%s", acctfile);
	else
		fclose(acctf);
}

/*
 * Hashing routines.
 */

/*
 * Enter the name into the hash table and return the pointer allocated.
 */

static struct hent *
enter(const char *name)
{
	struct hent *hp;
	int h;

	if ((hp = lookup(name)) != NULL)
		return(hp);
	h = hash(name);
	hcount++;
	hp = (struct hent *) malloc(sizeof *hp);
	if (hp == NULL)
		err(1, NULL);
	hp->h_name = strdup(name);
	if (hp->h_name == NULL)
		err(1, NULL);
	hp->h_feetpages = 0.0;
	hp->h_count = 0;
	hp->h_link = hashtab[h];
	hashtab[h] = hp;
	return(hp);
}

/*
 * Lookup a name in the hash table and return a pointer
 * to it.
 */

static struct hent *
lookup(const char *name)
{
	int h;
	struct hent *hp;

	h = hash(name);
	for (hp = hashtab[h]; hp != NULL; hp = hp->h_link)
		if (strcmp(hp->h_name, name) == 0)
			return(hp);
	return(NULL);
}

/*
 * Hash the passed name and return the index in
 * the hash table to begin the search.
 */
static int
hash(const char *name)
{
	int h;
	const char *cp;

	for (cp = name, h = 0; *cp; h = (h << 2) + *cp++)
		;
	return((h & 0x7fffffff) % HSHSIZE);
}

/*
 * The qsort comparison routine.
 * The comparison is ascii collating order
 * or by feet of typesetter film, according to sort.
 */
static int
qucmp(const void *a, const void *b)
{
	struct hent *h1, *h2;
	int r;

	h1 = *(struct hent **)a;
	h2 = *(struct hent **)b;
	if (sort)
		r = h1->h_feetpages < h2->h_feetpages ?
		    -1 : h1->h_feetpages > h2->h_feetpages;
	else
		r = strcmp(h1->h_name, h2->h_name);
	return(reverse ? -r : r);
}

/*
 * Perform lookup for printer name or abbreviation --
 */
static int
chkprinter(const char *s)
{
	int stat;
	int len;

	if ((stat = cgetent(&bp, printcapdb, s)) == -2) {
		printf("pac: can't open printer description file\n");
		exit(3);
	} else if (stat == -1)
		return(0);
	else if (stat == -3)
		fatal("potential reference loop detected in printcap file");

	if (cgetstr(bp, "af", &acctfile) == -1) {
		printf("accounting not enabled for printer %s\n", printer);
		exit(2);
	}
	if (!pflag && (cgetnum(bp, "pc", &price100) == 0))
		price = price100/10000.0;
	len = strlen(acctfile) + 5;
	sumfile = (char *) malloc(len);
	if (sumfile == NULL)
		err(1, "pac");
	strlcpy(sumfile, acctfile, len);
	strlcat(sumfile, "_sum", len);
	return(1);
}

__dead void
usage(void)
{
	extern char *__progname;

	fprintf(stderr, "usage: %s [-cmrs] [-Pprinter] [-pprice] [user ...]\n",
	    __progname);
	exit(1);
}
