/*	$OpenBSD: supcmisc.c,v 1.16 2007/05/17 11:00:37 moritz Exp $	*/

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
 *  Software Distribution Coordinator  or  Software.Distribution@CS.CMU.EDU
 *  School of Computer Science
 *  Carnegie Mellon University
 *  Pittsburgh PA 15213-3890
 *
 * any improvements or extensions that they make and grant Carnegie Mellon
 * the rights to redistribute these changes.
 */
/*
 * sup misc. routines, include list processing.
 **********************************************************************
 * HISTORY
 * Revision 1.5  92/08/11  12:07:22  mrt
 * 	Added release to FILEWHEN name.
 * 	Brad's changes: delinted and updated variable argument usage.
 * 	[92/07/26            mrt]
 * 
 * Revision 1.3  89/08/15  15:31:28  bww
 * 	Updated to use v*printf() in place of _doprnt().
 * 	From "[89/04/19            mja]" at CMU.
 * 	[89/08/15            bww]
 * 
 * 27-Dec-87  Glenn Marcy (gm0w) at Carnegie-Mellon University
 *	Fixed bug in ugconvert() which left pw uninitialized.
 *
 * 25-May-87  Doug Philips (dwp) at Carnegie-Mellon University
 *	Split off from sup.c and changed goaway to use printf
 *	instead of notify if thisC is NULL.
 *
 **********************************************************************
 */

#include "supcdefs.h"
#include "supextern.h"
#include <limits.h>

#ifndef UID_MAX
#define UID_MAX	((uid_t)-1)
#endif

struct liststruct {		/* uid and gid lists */
	char *Lname;		/* name */
	int Lnumber;		/* uid or gid */
	struct liststruct *Lnext;
};
typedef struct liststruct LIST;

#define HASHBITS	4
#define HASHSIZE	(1<<HASHBITS)
#define HASHMASK	(HASHSIZE-1)
#define LISTSIZE	(HASHSIZE*HASHSIZE)

static LIST *uidL[LISTSIZE];		/* uid and gid lists */
static LIST *gidL[LISTSIZE];

extern COLLECTION *thisC;		/* collection list pointer */

static int Lhash(char *);
static void Linsert(LIST **, char *, int);
static LIST *Llookup(LIST **, char *);

/*************************************************
 ***    P R I N T   U P D A T E   T I M E S    ***
 *************************************************/

void
prtime()
{
	char buf[STRINGLENGTH];
	char relsufix[STRINGLENGTH];
	time_t twhen;

	if ((thisC->Cflags&CFURELSUF) && thisC->Crelease)
		(void) snprintf(relsufix, sizeof relsufix, ".%s",
		    thisC->Crelease);
	else
		relsufix[0] = '\0';
	if (chdir(thisC->Cbase) < 0)
		logerr("Can't change to base directory %s for collection %s",
		    thisC->Cbase, thisC->Cname);
	twhen = getwhen(thisC->Cname, relsufix);
	(void) strlcpy(buf, ctime(&twhen), sizeof buf);
	buf[strlen(buf)-1] = '\0';	/* strip newline */
	loginfo("Last update occurred at %s for collection %s%s",
	    buf, thisC->Cname, relsufix);
}

int
establishdir(fname)
	char *fname;
{
	char dpart[STRINGLENGTH], fpart[STRINGLENGTH];

	path(fname, dpart, sizeof dpart, fpart, sizeof fpart);
	return (estabd(fname, dpart));
}

int
makedir(fname, mode, statp)
	char *fname;
	int mode;
	struct stat *statp;
{
	if (lstat(fname, statp) != -1 && !S_ISDIR(statp->st_mode)) {
		if (unlink(fname) == -1) {
			notify("SUP: Can't delete %s\n", fname);
			return (-1);
		}
	}

	(void) mkdir(fname, 0755);

	return (stat(fname, statp));
}

int
estabd(fname, dname)
	char *fname, *dname;
{
	char dpart[STRINGLENGTH], fpart[STRINGLENGTH];
	struct stat sbuf;
	int x;

	if (stat(dname, &sbuf) >= 0)
		return (FALSE); /* exists */
	path(dname, dpart, sizeof dpart, fpart, sizeof fpart);
	if (strcmp(fpart,".") == 0) {		/* dname is / or . */
		notify("SUP: Can't create directory %s for %s\n", dname, fname);
		return (TRUE);
	}
	x = estabd(fname, dpart);
	if (x)
		return (TRUE);
	if (makedir(dname, 0755, &sbuf) < 0) {
		vnotify("SUP: Can't create directory %s for %s\n",
		    dname, fname);
		return TRUE;
	}
	vnotify("SUP Created directory %s for %s\n", dname, fname);
	return (FALSE);
}

/***************************************
 ***    L I S T   R O U T I N E S    ***
 ***************************************/

/*
 * Hash function is:  HASHSIZE * (strlen mod HASHSIZE)
 *		      +          (char   mod HASHSIZE)
 * where "char" is last character of name (if name is non-null).
 */
static int
Lhash(name)
	char *name;
{
	int len;
	char c;

	len = strlen(name);
	if (len > 0)
		c = name[len-1];
	else
		c = 0;
	return (((len&HASHMASK)<<HASHBITS)|(((int)c)&HASHMASK));
}

static void
Linsert(table, name, number)
	LIST **table;
	char *name;
	int number;
{
	LIST *l;
	int lno;
	lno = Lhash(name);
	l = (LIST *) malloc(sizeof(LIST));
	l->Lname = name;
	l->Lnumber = number;
	l->Lnext = table[lno];
	table[lno] = l;
}

static LIST *
Llookup(table, name)
	LIST **table;
	char *name;
{
	int lno;
	LIST *l;

	lno = Lhash(name);
	for (l = table[lno]; l && strcmp(l->Lname,name) != 0; l = l->Lnext)
		;
	return (l);
}

void
ugconvert(uname, gname, uid, gid, mode)
	char *uname, *gname;
	uid_t *uid;
	gid_t *gid;
	int *mode;
{
	LIST *u, *g;
	struct passwd *pw;
	struct group *gr;
	struct stat sbuf;
	static uid_t defuid = UID_MAX;
	static gid_t defgid;
	static int first = TRUE;

	if (first) {
		memset(uidL, 0, sizeof(uidL));
		memset(gidL, 0, sizeof(gidL));
		first = FALSE;
	}
	pw = NULL;
	if ((u = Llookup(uidL, uname)) != NULL)
		*uid = (uid_t) u->Lnumber;
	else if ((pw = getpwnam(uname)) != NULL) {
		Linsert(uidL, strdup(uname), pw->pw_uid);
		*uid = pw->pw_uid;
	}
	if (u || pw) {
		if ((g = Llookup(gidL, gname)) != NULL) {
			*gid = (gid_t) g->Lnumber;
			return;
		}
		if ((gr = getgrnam(gname)) != NULL) {
			Linsert(gidL, strdup(gname), gr->gr_gid);
			*gid = gr->gr_gid;
			return;
		}
		if (pw == NULL && (pw = getpwnam(uname)) == NULL)
			goto defids;
		*mode &= ~S_ISGID;
		*gid = pw->pw_gid;
		return;
	}
defids:
	*mode &= ~(S_ISUID|S_ISGID);
	if (defuid != UID_MAX) {
		*uid = defuid;
		*gid = defgid;
		return;
	}
	if (stat(".", &sbuf) < 0) {
		*uid = defuid = getuid();
		*gid = defgid = getgid();
		return;
	}
	*uid = defuid = sbuf.st_uid;
	*gid = defgid = sbuf.st_gid;
}


/*********************************************
 ***    U T I L I T Y   R O U T I N E S    ***
 *********************************************/

void
notify (char *fmt,...)		/* record error message */
{
	char buf[STRINGLENGTH];
	char collrelname[STRINGLENGTH];
	time_t tloc;
	static FILE *noteF = NULL;	/* mail program on pipe */
	va_list ap;

	va_start(ap, fmt);
	if (fmt == NULL) {
		if (noteF && noteF != stdout)
			(void) pclose(noteF);
		noteF = NULL;
		return;
	}
	if ((thisC->Cflags&CFURELSUF) && thisC->Crelease) 
		(void) snprintf(collrelname, sizeof collrelname, "%s-%s",
		    collname, thisC->Crelease);
	else
		(void) strlcpy(collrelname, collname, sizeof collrelname);
	
	if (noteF == NULL) {
		/* XXX - it would be nicer to run sendmail directly (millert) */
		if ((thisC->Cflags&CFMAIL) && thisC->Cnotify) {
			(void) snprintf(buf, sizeof buf,
				"mail -s \"SUP Upgrade of %s\" %s >/dev/null",
				collrelname, thisC->Cnotify);
			noteF = popen(buf, "w");
			if (noteF == NULL) {
				logerr ("Can't send mail to %s for %s",
					thisC->Cnotify, collrelname);
				noteF = stdout;
			}
		} else
			noteF = stdout;
		tloc = time(NULL);
		fprintf(noteF, "SUP Upgrade of %s at %s", collrelname,
		    ctime(&tloc));
		(void) fflush(noteF);
	}
	vfprintf(noteF, fmt, ap);
	va_end(ap);
	(void) fflush(noteF);
}

void
lockout(on)		/* lock out interrupts */
	int on;
{
	static sigset_t oset;
	sigset_t nset;

	if (on) {
		sigemptyset(&nset);
		sigaddset(&nset, SIGHUP);
		sigaddset(&nset, SIGINT);
		sigaddset(&nset, SIGTERM);
		sigaddset(&nset, SIGQUIT);
		(void) sigprocmask(SIG_BLOCK, &nset, &oset);
	} else {
		(void) sigprocmask(SIG_SETMASK, &oset, NULL);
	}
}

char *
fmttime(time)
	time_t time;
{
	static char buf[16];
	char *p;

	/*
	 * Copy ctime to buf, stripping day of week, year, and newline.
	 * E.g.: "Thu Nov 24 18:22:48 1986\n" -> "Nov 24 18:22:48"
	 */
	p = ctime(&time) + 4;
	(void) strlcpy(buf, p, sizeof(buf));
	return (buf);
}
