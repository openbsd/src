/*	$OpenBSD: supscan.c,v 1.7 1997/09/16 10:43:01 deraadt Exp $	*/

/*
 * Copyright (c) 1992 Carnegie Mellon University
 * All Rights Reserved.
 * 
 * Permission to use, copy, modify and distribute this software and its
 * documentation is hereby granted, provided that both the copyright
 * notice and this permission notice appear in all copies of the
 * software, derivative works or modified versions, and any portions
 * thereof, and that both notices appear in supporting documentation.
 *
 * CARNEGIE MELLON ALLOWS FREE USE OF THIS SOFTWARE IN ITS "AS IS"
 * CONDITION.  CARNEGIE MELLON DISCLAIMS ANY LIABILITY OF ANY KIND FOR
 * ANY DAMAGES WHATSOEVER RESULTING FROM THE USE OF THIS SOFTWARE.
 *
 * Carnegie Mellon requests users of this software to return to
 *
 *  Software Distribution Coordinator  or  Software_Distribution@CS.CMU.EDU
 *  School of Computer Science
 *  Carnegie Mellon University
 *  Pittsburgh PA 15213-3890
 *
 * any improvements or extensions that they make and grant Carnegie Mellon
 * the rights to redistribute these changes.
 */
/*
 * supscan -- SUP Scan File Builder
 *
 * Usage: supscan [ -v ] collection [ basedir ]
 *	  supscan [ -v ] -f dirfile
 *	  supscan [ -v ] -s
 *	-f	"file" -- use dirfile instead of system coll.dir
 *	-s	"system" -- perform scan for system supfile
 *	-v	"verbose" -- print messages as you go
 *	collection	-- name of the desired collection if not -s
 *	basedir		-- name of the base directory, if not
 *			   the default or recorded in coll.dir
 *	dirfile		-- name of replacement for system coll.dir.
 *
 **********************************************************************
 * HISTORY
 * 
 * 13-May-88  Glenn Marcy (gm0w) at Carnegie-Mellon University
 *	Changed goaway to longjmp back to top-level to scan next
 *	collection. [V7.6]
 *
 * 19-Feb-88  Glenn Marcy (gm0w) at Carnegie-Mellon University
 *	Added -f <filename> switch to scan all (or part) of the
 *	collections in a file of collection/base-directory pairs.
 *	[V7.5]
 *
 * 27-Dec-87  Glenn Marcy (gm0w) at Carnegie-Mellon University
 *	Removed nameserver support (which means to use a new
 *	datafile).
 *
 * 09-Sep-87  Glenn Marcy (gm0w) at Carnegie-Mellon University
 *	Use case-insensitive hostname comparison.
 *
 * 28-Jun-87  Glenn Marcy (gm0w) at Carnegie-Mellon University
 *	Added code for "release" support. [V6.4]
 *
 * 05-Jan-86  Glenn Marcy (gm0w) at Carnegie-Mellon University
 *	Changed collection setup errors to be non-fatal. [V5.3]
 *
 * 29-Dec-85  Glenn Marcy (gm0w) at Carnegie-Mellon University
 *	Moved most of the scanning code to scan.c. [V4.2]
 *
 * 02-Nov-85  Glenn Marcy (gm0w) at Carnegie-Mellon University
 *	Added "-s" option.
 *
 * 22-Sep-85  Glenn Marcy (gm0w) at Carnegie-Mellon University
 *	Merged 4.1 and 4.2 versions together.
 *
 * 04-Jun-85  Steven Shafer (sas) at Carnegie-Mellon University
 *	Created for 4.2 BSD.
 *
 **********************************************************************
 */

#include <libc.h>
#include <c.h>
#include <netdb.h>
#include <setjmp.h>
#ifdef __STDC__
#include <stdarg.h>
#else
#include <varargs.h>
#endif
#include <sys/time.h>
#include <sys/types.h>
#include "supcdefs.h"
#include "supextern.h"

#define PGMVERSION 6

/*******************************************
 ***    D A T A   S T R U C T U R E S    ***
 *******************************************/

struct scan_collstruct {		/* one per collection to be upgraded */
	char *Cname;			/* collection name */
	char *Cbase;			/* local base directory */
	char *Cprefix;			/* local collection pathname prefix */
	struct scan_collstruct *Cnext;	/* next collection */
};
typedef struct scan_collstruct SCAN_COLLECTION;

/*********************************************
 ***    G L O B A L   V A R I A B L E S    ***
 *********************************************/

int trace;				/* -v flag */

SCAN_COLLECTION *firstC;		/* collection list pointer */
char *collname;				/* collection name */
char *basedir;				/* base directory name */
char *prefix;				/* collection pathname prefix */
time_t lasttime = 0;			/* time of last upgrade */
time_t scantime;			/* time of this scan */
int newonly = FALSE;			/* new files only */
jmp_buf sjbuf;				/* jump location for errors */

TREELIST *listTL;	/* list of all files specified by <coll>.list */
TREE *listT;		/* final list of files in collection */
TREE *refuseT = NULL;	/* list of all files specified by <coll>.list */


void usage __P((void));
void init __P((int, char **));
static SCAN_COLLECTION *getscancoll __P((char *, char *, char *));
int localhost __P((char *));
int main __P((int, char **));

/*************************************
 ***    M A I N   R O U T I N E    ***
 *************************************/

int
main (argc,argv)
int argc;
char **argv;
{
	SCAN_COLLECTION *c;
#if __GNUC__
	/* Avoid longjmp clobbering */
	(void) &c;
#endif

	init (argc,argv);		/* process arguments */
	for (c = firstC; c; c = c->Cnext) {
		collname = c->Cname;
		basedir = c->Cbase;
		prefix = c->Cprefix;
		(void) chdir (basedir);
		scantime = time ((time_t *)NULL);
		printf ("SUP Scan for %s starting at %s",collname,
			ctime (&scantime));
		(void) fflush (stdout);
		if (!setjmp (sjbuf)) {
			makescanlists (); /* record names in scan files */
			scantime = time ((time_t *)NULL);
			printf ("SUP Scan for %s completed at %s",collname,
				ctime (&scantime));
		} else
			printf ("SUP: Scan for %s aborted at %s",collname,
				ctime (&scantime));
		(void) fflush (stdout);
	}
	while ((c = firstC) != NULL) {
		firstC = firstC->Cnext;
		free (c->Cname);
		free (c->Cbase);
		if (c->Cprefix)  free (c->Cprefix);
		free ((char *)c);
	}
	exit (0);
}

/*****************************************
 ***    I N I T I A L I Z A T I O N    ***
 *****************************************/

void
usage ()
{
	fprintf (stderr,"Usage: supscan [ -v ] collection [ basedir ]\n");
	fprintf (stderr,"       supscan [ -v ] -f dirfile\n");
	fprintf (stderr,"       supscan [ -v ] -s\n");
	exit (1);
}

void
init (argc,argv)
int argc;
char **argv;
{
	char buf[STRINGLENGTH],fbuf[STRINGLENGTH],*p,*q;
	FILE *f;
	SCAN_COLLECTION **c;
	int fflag,sflag;
	char *filename = NULL;

	trace = FALSE;
	fflag = FALSE;
	sflag = FALSE;
	while (argc > 1 && argv[1][0] == '-') {
		switch (argv[1][1]) {
		case 'f':
			fflag = TRUE;
			if (argc == 2)
				usage ();
			--argc;
			argv++;
			filename = argv[1];
			break;
		case 'v':
			trace = TRUE;
			break;
		case 's':
			sflag = TRUE;
			break;
		default:
			fprintf (stderr,"supscan: Invalid flag %s ignored\n",argv[1]);
			(void) fflush (stderr);
		}
		--argc;
		argv++;
	}
	if (!fflag) {
		(void) snprintf (fbuf,sizeof fbuf,FILEDIRS,DEFDIR);
		filename = fbuf;
	}
	if (sflag) {
		if (argc != 1)
			usage ();
		firstC = NULL;
		c = &firstC;
		(void) snprintf (buf,sizeof buf,sizeof buf,FILEHOSTS,DEFDIR);
		if ((f = fopen (buf,"r")) == NULL)
			quit (1,"supscan: Unable to open %s\n",buf);
		while ((p = fgets (buf,STRINGLENGTH,f)) != NULL) {
			q = strchr (p,'\n');
			if (q)  *q = 0;
			if (strchr ("#;:",*p))  continue;
			collname = nxtarg (&p," \t=");
			p = skipover (p," \t=");
			if (!localhost (p))  continue;
			*c = getscancoll(filename,salloc (collname),
					(char *)NULL);
			if (*c)  c = &((*c)->Cnext);
		}
		(void) fclose (f);
		return;
	}
	if (argc < 2 && fflag) {
		firstC = NULL;
		c = &firstC;
		if ((f = fopen (filename,"r")) == NULL)
			quit (1,"supscan: Unable to open %s\n",filename);
		while ((p = fgets (buf,STRINGLENGTH,f)) != NULL) {
			q = strchr (p,'\n');
			if (q)  *q = 0;
			if (strchr ("#;:",*p))  continue;
			q = nxtarg (&p," \t=");
			p = skipover (p," \t=");
			*c = getscancoll(filename,salloc (q),salloc (p));
			if (*c)  c = &((*c)->Cnext);
		}
		(void) fclose (f);
		return;
	}
	if (argc < 2 || argc > 3)
		usage ();
	firstC = getscancoll(filename,salloc (argv[1]),
			argc > 2 ? salloc (argv[2]) : (char *)NULL);
}

static SCAN_COLLECTION *
getscancoll(filename, collname, basedir)
register char *filename,*collname,*basedir;
{
	char buf[STRINGLENGTH],*p,*q;
	FILE *f;
	SCAN_COLLECTION *c;

	if (basedir == NULL) {
		if ((f = fopen (filename,"r")) != NULL) {
			while ((p = fgets (buf,STRINGLENGTH,f)) != NULL) {
				q = strchr (p,'\n');
				if (q)  *q = 0;
				if (strchr ("#;:",*p))  continue;
				q = nxtarg (&p," \t=");
				if (strcmp (q,collname) == 0) {
					p = skipover (p," \t=");
					basedir = salloc (p);
					break;
				}
			}
			(void) fclose (f);
		}
		if (basedir == NULL) {
			(void) snprintf (buf,sizeof buf,sizeof buf,
				FILEBASEDEFAULT,collname);
			basedir = salloc (buf);
		}
	}
	if (chdir(basedir) < 0) {
		fprintf (stderr,"supscan:  Can't chdir to base directory %s for %s\n",
			basedir,collname);
		return (NULL);
	}
	prefix = NULL;
	(void) snprintf (buf,sizeof buf,FILEPREFIX,collname);
	if ((f = fopen (buf,"r")) != NULL) {
		while ((p = fgets (buf,STRINGLENGTH,f)) != NULL) {
			q = strchr (p,'\n');
			if (q) *q = 0;
			if (strchr ("#;:",*p))  continue;
			prefix = salloc (p);
			if (chdir(prefix) < 0) {
				fprintf (stderr,"supscan: can't chdir to %s from base directory %s for %s\n",
					prefix,basedir,collname);
				return (NULL);
			}
			break;
		}
		(void) fclose (f);
	}
	if ((c = (SCAN_COLLECTION *) malloc (sizeof(SCAN_COLLECTION))) == NULL)
		quit (1,"supscan: can't malloc collection structure\n");
	c->Cname = collname;
	c->Cbase = basedir;
	c->Cprefix = prefix;
	c->Cnext = NULL;
	return (c);
}

void
#ifdef __STDC__
goaway (char *fmt,...)
#else
/*VARARGS*//*ARGSUSED*/
goaway (va_alist)
va_dcl
#endif
{
	va_list ap;

#ifdef __STDC__
	va_start(ap,fmt);
#else
	char *fmt;

	va_start(ap);
	fmt = va_arg(ap,char *);
#endif

	vfprintf(stderr, fmt, ap);
	va_end(ap);
	(void) putc ('\n',stderr);
	(void) fflush (stderr);
	longjmp (sjbuf,TRUE);
}

int localhost (host)
register char *host;
{
	static char myhost[STRINGLENGTH];
	static int myhostlen;
	register int hostlen;

	if (*myhost == '\0') {
		/*
		 * We assume that the host name in the kernel is the
		 * fully qualified form.
		 */
		if (gethostname (myhost,sizeof (myhost)) < 0) {
		    quit (1,"supscan: can't get kernel host name\n");
		}
		myhostlen = strlen(myhost);
	}

	/*
	 * Here, we assume that the 'host' parameter from the
	 * coll.host file is at least a prefix of the fully qualified
	 * host name of some machine.  This will be true when modcoll(8)
	 * (and related scripts) maintain the relevant files, but if
	 * a person makes a manual change, problems could result.  In
	 * particular, if a nicname, such as "Y" for "GANDALF.CS.CMU.EDU"
	 * is present in the coll.host file, things will not work as
	 * expected.
	 */

	hostlen = strlen(host);

	return(strncasecmp (myhost,
			    host,
			    hostlen < myhostlen ? hostlen : myhostlen) == 0);
}
