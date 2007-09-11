/*	$OpenBSD: supcmeat.c,v 1.22 2007/09/11 15:47:17 gilles Exp $	*/

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
 * sup "meat" routines
 **********************************************************************
 * HISTORY
 *
 * 7-July-93  Nate Williams at Montana State University
 *	Modified SUP to use gzip based compression when sending files
 *	across the network to save BandWidth
 *
 * Revision 1.16  92/09/09  22:04:51  mrt
 * 	Really added bww's recvone changes this time. 
 * 	Added code to support non-crypting version of sup.
 * 	[92/09/01            mrt]
 * 
 * Revision 1.15  92/08/11  12:07:09  mrt
 * 	Added support to add release to FILEWHEN name.
 * 	Updated variable arguemnt list usage - bww
 * 	Updated recvone() to take a va_list - bww
 * 	Changed conditional for rpausing code from CMUCS to MACH
 * 	[92/07/24            mrt]
 * 
 * Revision 1.14  92/02/08  18:24:12  mja
 * 	Only apply "keep" mode when local file is strictly newer
 * 	otherwise allow update as before if necessary.
 * 	[92/02/08  18:09:00  mja]
 * 
 * 	Added support for -k (keep) option to needone().  Rewrote and
 * 	commented other parts of needone().
 * 	[92/01/17            vdelvecc]
 * 
 * Revision 1.13  91/05/16  14:49:41  ern
 * 	Add timestap to fileserver.
 * 	Drop day of the week from 5 messages.
 * 	[91/05/16  14:47:53  ern]
 * 
 * Revision 1.12  89/08/23  14:55:44  gm0w
 * 	Changed msgf routines to msg routines.
 * 	[89/08/23            gm0w]
 * 
 * Revision 1.11  89/08/03  19:49:10  mja
 * 	Updated to use v*printf() in place of _doprnt().
 * 	[89/04/19            mja]
 * 
 * Revision 1.10  89/06/18  14:41:27  gm0w
 * 	Fixed up some notify messages of errors to use "SUP:" prefix.
 * 	[89/06/18            gm0w]
 * 
 * Revision 1.9  89/06/10  15:12:17  gm0w
 * 	Changed to always use rename to install targets.  This breaks hard
 * 	links and recreates those known to sup, other links will be orphaned.
 * 	[89/06/10            gm0w]
 * 
 * Revision 1.8  89/05/24  15:04:23  gm0w
 * 	Added code to check for EINVAL from FSPARAM ioctl for disk
 * 	space check failures when the ioctl is not implemented.
 * 	[89/05/24            gm0w]
 * 
 * Revision 1.7  89/01/16  18:22:28  gm0w
 * 	Changed needone() to check that mode of files match before
 * 	setting update if times also match.
 * 	[89/01/16            gm0w]
 * 
 * 10-Feb-88  Glenn Marcy (gm0w) at Carnegie-Mellon University
 *	Added timeout to backoff.
 *
 * 27-Dec-87  Glenn Marcy (gm0w) at Carnegie-Mellon University
 *	Added crosspatch support.
 *
 * 09-Sep-87  Glenn Marcy (gm0w) at Carnegie-Mellon University
 *	Added code to be less verbose when updating files that have
 *	already been successfully upgraded.
 *
 * 28-Jun-87  Glenn Marcy (gm0w) at Carnegie-Mellon University
 *	Added code for "release" support.
 *
 * 26-May-87  Doug Philips (dwp) at Carnegie-Mellon University
 *	Converted to end connection with more information.
 *	Added done routine.  Modified goaway routine to free old
 *	goawayreason.
 *
 * 26-May-87  Doug Philips (dwp) at Carnegie-Mellon University
 *	Use computeBackoff from scm instead of doing it ourselves.
 *
 * 25-May-87  Doug Philips (dwp) at Carnegie-Mellon University
 *	Split off from sup.c and reindented goaway calls.
 *
 **********************************************************************
 */

#include "supcdefs.h"
#include "supextern.h"
#include <sys/param.h>
#include <sys/wait.h>

TREE *lastT;				/* last filenames in collection */
jmp_buf sjbuf;				/* jump location for network errors */
int dontjump;				/* flag to void sjbuf */
int cancompress = FALSE;		/* Can we do compression? */
int docompress = FALSE;			/* Do we do compression? */

extern COLLECTION *thisC;		/* collection list pointer */
extern int rpauseflag;			/* don't disable resource pausing */
extern int portdebug;			/* network debugging ports */
extern int noutime;			/* don't set utimes */

/*************************************************
 ***    U P G R A D E   C O L L E C T I O N    ***
 *************************************************/

static int needone(TREE *, void *);
static int recvone(TREE *, va_list);
static int denyone(TREE *, void *);
static int deleteone(TREE *, void *);
static int linkone(TREE *, void *);
static int execone(TREE *, void *);
static int finishone(TREE *, void *);


/*
 * The next two routines define the fsm to support multiple fileservers
 * per collection.
 */
int
getonehost(t, v)
	TREE *t;
	void *v;
{
	long *state = v;

	if (t->Tflags != *state)
		return (SCMOK);
	if (*state != 0 && t->Tmode == SCMEOF) {
		t->Tflags = 0;
		return (SCMOK);
	}
	if (*state == 2)
		t->Tflags--;
	else
		t->Tflags++;
	thisC->Chost = t;
	return (SCMEOF);
}

TREE *
getcollhost(tout, backoff, state, nhostsp)
	int *tout, *backoff, *nhostsp;
	long *state;
{
	static long laststate = 0;
	static int nhosts = 0;

	if (*state != laststate) {
		*nhostsp = nhosts;
		laststate = *state;
		nhosts = 0;
	}
	if (Tprocess(thisC->Chtree, getonehost, state) == SCMEOF) {
		if (*state != 0 && nhosts == 0 && !dobackoff(tout, backoff))
			return (NULL);
		nhosts++;
		return (thisC->Chost);
	}
	if (nhosts == 0)
		return (NULL);
	if (*state == 2)
		(*state)--;
	else
		(*state)++;
	return (getcollhost(tout, backoff, state, nhostsp));
}

/*
 *  Upgrade a collection from the file server on the appropriate
 *  host machine.
 */
void
getcoll(void)
{
	TREE *t;
	int x;
	int tout, backoff, nhosts;
	long state;

	collname = thisC->Cname;
	tout = thisC->Ctimeout;
	lastT = NULL;
	backoff = 2;
	state = 0;
	nhosts = 0;
	for (;;) {
		t = getcollhost(&tout, &backoff, &state, &nhosts);
		if (t == NULL) {
			finishup(SCMEOF);
			notify(NULL);
			return;
		}
		t->Tmode = SCMEOF;
		dontjump = FALSE;
		if (!setjmp(sjbuf) && !signon(t, nhosts, &tout) && !setup(t))
			break;
		(void) requestend();
	}
	dontjump = FALSE;
	if (setjmp(sjbuf))
		x = SCMERR;
	else {
		suplogin();
		listfiles();
		recvfiles();
		x = SCMOK;
	}
	if (thisC->Clockfd >= 0) {
		(void) close(thisC->Clockfd);
		thisC->Clockfd = -1;
	}
	finishup(x);
	notify(NULL);
}

/***  Sign on to file server ***/

int signon (t, nhosts, tout)
	TREE *t;
	int nhosts;
	int *tout;
{
	int x;
	int timeout;
	time_t tloc;

	if ((thisC->Cflags&CFLOCAL) == 0 && thishost(thisC->Chost->Tname)) {
		vnotify("SUP: Skipping local collection %s\n", collname);
		t->Tmode = SCMEOF;
		return (TRUE);
	}
	if (nhosts == 1)
		timeout = *tout;
	else
		timeout = 0;
	x = request(portdebug ? DEBUGFPORT : FILEPORT, thisC->Chost->Tname,
	    &timeout);
	if (nhosts == 1)
		*tout = timeout;
	if (x != SCMOK) {
		if (nhosts) {
			notify("SUP: Can't connect to host %s\n",
			    thisC->Chost->Tname);
			t->Tmode = SCMEOF;
		} else
			t->Tmode = SCMOK;
		return (TRUE);
	}
	xpatch = FALSE;
	x = msgsignon();	/* signon to fileserver */
	if (x != SCMOK)
		goaway("Error sending signon request to fileserver");
	x = msgsignonack();	/* receive signon ack from fileserver */
	if (x != SCMOK)
		goaway("Error reading signon reply from fileserver");
	tloc = time(NULL);
	vnotify("SUP Fileserver %d.%d (%s) %ld on %s at %.8s\n", protver,
	    pgmver, scmver, (long)fspid, remotehost(), ctime(&tloc) + 11);
	free(scmver);
	scmver = NULL;
	if (protver < 4) {
		dontjump = TRUE;
		goaway("Fileserver sup protocol version is obsolete.");
		notify("SUP: This version of sup can only communicate with a fileserver using at least\n");
		notify("SUP: version 4 of the sup network protocol.  You should either run a newer\n");
		notify("SUP: version of the sup fileserver or find an older version of sup.\n");
		t->Tmode = SCMEOF;
		return (TRUE);
	}
	/* If protocol is > 7 then try compression */
	if (protver > 7) {
		cancompress = TRUE;
	}
	return (FALSE);
}

/***  Tell file server what to connect to ***/

int
setup(t)
	TREE *t;
{
	char relsufix[STRINGLENGTH];
	int x;
	struct stat sbuf;

	if (chdir(thisC->Cbase) < 0)
		goaway("Can't change to base directory %s", thisC->Cbase);
	if (stat ("sup", &sbuf) < 0) {
		(void) mkdir("sup", 0755);
		if (stat("sup",&sbuf) < 0)
			goaway("Can't create directory %s/sup", thisC->Cbase);
		vnotify("SUP Created directory %s/sup\n", thisC->Cbase);
	}
	if (thisC->Cprefix && chdir(thisC->Cprefix) < 0)
		goaway("Can't change to %s from base directory %s",
		    thisC->Cprefix, thisC->Cbase);
	if (stat(".", &sbuf) < 0)
		goaway("Can't stat %s directory %s",
			thisC->Cprefix ? "prefix" : "base",
			thisC->Cprefix ? thisC->Cprefix : thisC->Cbase);
	if (thisC->Cprefix)
		(void) chdir(thisC->Cbase);
	/* read time of last upgrade from when file */

	if ((thisC->Cflags&CFURELSUF) && thisC->Crelease)
		(void) snprintf(relsufix, sizeof relsufix, ".%s",
		    thisC->Crelease);
	else
		relsufix[0] = '\0';
	lasttime = getwhen(collname, relsufix);
	/* setup for msgsetup */
	basedir = thisC->Chbase;
	basedev = sbuf.st_dev;
	baseino = sbuf.st_ino;
	listonly = (thisC->Cflags&CFLIST);
	newonly = ((thisC->Cflags&(CFALL|CFDELETE|CFOLD)) == 0);
	release = thisC->Crelease;
	x = msgsetup();
	if (x != SCMOK)
		goaway ("Error sending setup request to file server");
	x = msgsetupack();
	if (x != SCMOK)
		goaway("Error reading setup reply from file server");
	if (setupack == FSETUPOK) {
		/* Test encryption */
		if (netcrypt(thisC->Ccrypt) != SCMOK)
			goaway("Running non-crypting sup");
		crypttest = CRYPTTEST;
		x = msgcrypt();
		if (x != SCMOK)
			goaway("Error sending encryption test request");
		x = msgcryptok();
		if (x == SCMEOF)
			goaway("Data encryption test failed");
		if (x != SCMOK)
			goaway("Error reading encryption test reply");
		return (FALSE);
	}
	switch (setupack) {
	case FSETUPSAME:
		notify("SUP: Attempt to upgrade from same host to same directory\n");
		done(FDONESRVERROR, "Overwrite error");
	case FSETUPHOST:
		notify("SUP: This host has no permission to access %s\n",
		    collname);
		done(FDONESRVERROR, "Permission denied");
	case FSETUPOLD:
		notify("SUP: This version of SUP is too old for the fileserver\n");
		done(FDONESRVERROR, "Obsolete client");
	case FSETUPRELEASE:
		notify("SUP: Invalid release %s for collection %s\n",
			release == NULL ? DEFRELEASE : release, collname);
		done(FDONESRVERROR, "Invalid release");
	case FSETUPBUSY:
		vnotify("SUP Fileserver is currently busy\n");
		t->Tmode = SCMOK;
		doneack = FDONESRVERROR;
		donereason = "Fileserver is busy";
		(void) netcrypt(NULL);
		(void) msgdone();
		return (TRUE);
	default:
		goaway("Unrecognized file server setup status %d", setupack);
	}
	/* NOTREACHED */
	return (FALSE);
}

/***  Tell file server what account to use ***/

void
suplogin(void)
{
	char buf[STRINGLENGTH];
	int f, x;

	/* lock collection if desired */
	(void) snprintf(buf, sizeof buf, FILELOCK, collname);
	f = open(buf, O_RDONLY, 0);
	if (f >= 0) {

#if defined(LOCK_EX)
# define TESTLOCK(f)	flock(f, LOCK_EX|LOCK_NB)
# define SHARELOCK(f)	flock(f, LOCK_SH|LOCK_NB)
# define WAITLOCK(f)	flock(f, LOCK_EX)
#elif defined(F_LOCK)
# define TESTLOCK(f)	lockf(f, F_TLOCK, 0)
# define SHARELOCK(f)	1
# define WAITLOCK(f)	lockf(f, F_LOCK, 0)
#else
# define TESTLOCK(f)	(close(f), f = -1, 1)
# define SHARELOCK(f)	1
# define WAITLOCK(f)	1
#endif
		if (TESTLOCK(f) < 0) {
			if (errno != EWOULDBLOCK && errno != EAGAIN) {
				(void) close(f);
				goaway("Can't lock collection %s", collname);
			}
			if (SHARELOCK(f) < 0) {
				(void) close(f);
				if (errno == EWOULDBLOCK && errno != EAGAIN)
					goaway("Collection %s is locked by another sup",collname);
				goaway("Can't lock collection %s", collname);
			}
			vnotify("SUP Waiting for exclusive access lock\n");
			if (WAITLOCK(f) < 0) {
				(void) close(f);
				goaway("Can't lock collection %s", collname);
			}
		}
		thisC->Clockfd = f;
		vnotify("SUP Locked collection %s for exclusive access\n",
		    collname);
	}
	logcrypt = NULL;
	loguser = thisC->Clogin;
	logpswd = thisC->Cpswd;

#ifndef	CRYPTING  /* Define CRYPTING for backwards compatibility with old supfileservers */
	if (thisC->Clogin != NULL) /* othewise we only encrypt if there is a login id */
#endif	/* CRYPTING */
	{
		logcrypt = CRYPTTEST;
		(void) netcrypt(PSWDCRYPT);	/* encrypt password data */
	}
	x = msglogin();
#ifndef CRYPTING
	if (thisC->Clogin != NULL) 
#endif
		(void) netcrypt(NULL);		/* turn off encryption */
	if (x != SCMOK)
		goaway("Error sending login request to file server");
	x = msglogack();
	if (x != SCMOK)
		goaway("Error reading login reply from file server");
	if (logack == FLOGNG) {
		notify("SUP: %s\n", logerror);
		free(logerror);
		logerror = NULL;
		notify("SUP: Improper login to %s account",
			thisC->Clogin ? thisC->Clogin : "default");
		done(FDONESRVERROR, "Improper login");
	}
	if (netcrypt (thisC->Ccrypt) != SCMOK)	/* restore encryption */
		goaway("Running non-crypting sup");
}

/*
 *  Send list of files that we are not interested in.  Receive list of
 *  files that are on the repository that could be upgraded.  Find the
 *  ones that we need.  Receive the list of files that the server could
 *  not access.  Delete any files that have been upgraded in the past
 *  which are no longer on the repository.
 */
void
listfiles()
{
	char buf[STRINGLENGTH];
	char relsufix[STRINGLENGTH];
	char *p,*q;
	FILE *f;
	int x;

	if ((thisC->Cflags&CFURELSUF) && release)
		(void) snprintf(relsufix, sizeof relsufix, ".%s", release);
	else
		relsufix[0] = '\0';
	(void) snprintf(buf, sizeof buf, FILELAST, collname, relsufix);
	f = fopen(buf, "r");
	if (f) {
		while ((p = fgets(buf, sizeof(buf), f)) != NULL) {
			p[strcspn(p, "\n")] = '\0';
			if (strchr("#;:", *p))
				continue;
			(void) Tinsert(&lastT, p, FALSE);
		}
		(void) fclose(f);
	}
	refuseT = NULL;
	(void) snprintf(buf, sizeof buf, FILEREFUSE, collname);
	f = fopen(buf, "r");
	if (f) {
		while ((p = fgets(buf, sizeof(buf), f)) != NULL) {
			p[strcspn(p, "\n")] = '\0';
			if (strchr("#;:", *p))
				continue;
			(void) Tinsert(&refuseT, p, FALSE);
		}
		(void) fclose(f);
	}
	vnotify("SUP Requesting changes since %s", ctime(&lasttime) + 4);
	x = msgrefuse();
	if (x != SCMOK)
		goaway ("Error sending refuse list to file server");
	listT = NULL;
	x = msglist();
	if (x != SCMOK)
		goaway("Error reading file list from file server");
	if (thisC->Cprefix)
		(void) chdir(thisC->Cprefix);
	needT = NULL;
	(void) Tprocess(listT, needone, NULL);
	Tfree(&listT);
	x = msgneed();
	if (x != SCMOK)
		goaway("Error sending needed files list to file server");
	Tfree(&needT);
	denyT = NULL;
	x = msgdeny();
	if (x != SCMOK)
		goaway("Error reading denied files list from file server");
	if (thisC->Cflags&CFVERBOSE)
		(void) Tprocess(denyT, denyone, NULL);
	Tfree(&denyT);
	if (thisC->Cflags&(CFALL|CFDELETE|CFOLD))
		(void) Trprocess(lastT, deleteone, NULL);
	Tfree(&refuseT);
}

static int
needone(t, dummy)
	TREE *t;
	void *dummy;
{
	TREE *newt;
	int exists, fetch;
	struct stat sbuf;

	newt = Tinsert (&lastT,t->Tname,TRUE);
	if (!newt)
		return (SCMERR);
	newt->Tflags |= FUPDATE;
	fetch = TRUE;
	if ((thisC->Cflags&CFALL) == 0) {
		if ((t->Tflags&FNEW) == 0 && (thisC->Cflags&CFOLD) == 0)
			return (SCMOK);
		if (S_ISLNK(t->Tmode))
			exists = (lstat(t->Tname,&sbuf) == 0);
		else
			exists = (stat(t->Tname,&sbuf) == 0);
		/*
		 * This is moderately complicated:
		 * If the file is the wrong type or doesn't exist, we need to
		 * fetch the whole file.  If the file is a special file, we
		 * rely solely on the server:  if the file changed, we do an
		 * update; otherwise nothing. If the file is a normal file,
		 * we check timestamps.  If we are in "keep" mode, we fetch if
		 * the file on the server is newer, and do nothing otherwise.
		 * Otherwise, we fetch if the timestamp is wrong; if the file
		 * changed on the server but the timestamp is right, we do an
		 * update.  (Update refers to updating stat information, i.e.
		 * timestamp, owner, mode bits, etc.)
		 */
		if (exists && (sbuf.st_mode&S_IFMT) == (t->Tmode&S_IFMT)) {
			if (!S_ISREG(t->Tmode)) {
				if (t->Tflags&FNEW)
					fetch = FALSE;
				else
					return (SCMOK);
			} else if ((thisC->Cflags&CFKEEP) &&
			    sbuf.st_mtime > t->Tmtime) {
				return (SCMOK);
			} else if (sbuf.st_mtime == t->Tmtime) {
				if (t->Tflags&FNEW)
					fetch = FALSE;
				else
					return (SCMOK);
			}
		}
	}
	/* If we get this far, we're either doing an update or a full fetch. */
	newt = Tinsert(&needT, t->Tname, TRUE);
	if (!newt)
		return (SCMERR);
	if (!fetch && S_ISREG(t->Tmode))
		newt->Tflags |= FUPDATE;
	return (SCMOK);
}

static int
denyone(t, v)
	TREE *t;
	void *v;
{

	vnotify("SUP: Access denied to %s\n", t->Tname);
	return (SCMOK);
}

static int
deleteone(t, v)
	TREE *t;
	void *v;
{
	struct stat sbuf, pbuf;
	int x;
	char *name = t->Tname;
	char pname[MAXPATHLEN];

	if (t->Tflags&FUPDATE)		/* in current upgrade list */
		return (SCMOK);
	if (lstat(name, &sbuf) < 0)	/* doesn't exist */
		return (SCMOK);
	/* is it a symbolic link ? */
	if (S_ISLNK(sbuf.st_mode)) {
		if (Tlookup(refuseT, name)) {
			vnotify("SUP Would not delete symbolic link %s\n",
			    name);
			return (SCMOK);
		}
		if (thisC->Cflags&CFLIST) {
			vnotify("SUP Would delete symbolic link %s\n", name);
			return (SCMOK);
		}
		if ((thisC->Cflags&CFDELETE) == 0) {
			notify("SUP Please delete symbolic link %s\n", name);
			t->Tflags |= FUPDATE;
			return (SCMOK);
		}
		x = unlink(name);
		if (x < 0) {
			notify("SUP: Unable to delete symbolic link %s\n",
			    name);
			t->Tflags |= FUPDATE;
			return (SCMOK);
		}
		vnotify("SUP Deleted symbolic link %s\n", name);
		return (SCMOK);
	}
	/* is it a directory ? */
	if (S_ISDIR(sbuf.st_mode)) {
		if (Tlookup(refuseT, name)) {
			vnotify("SUP Would not delete directory %s\n", name);
			return (SCMOK);
		}
		if (thisC->Cflags&CFLIST) {
			vnotify("SUP Would delete directory %s\n", name);
			return (SCMOK);
		}
		if ((thisC->Cflags&CFDELETE) == 0) {
			notify("SUP Please delete directory %s\n", name);
			t->Tflags |= FUPDATE;
			return (SCMOK);
		}
		if (rmdir(name) < 0) {
			(void) chmod(name, sbuf.st_mode|S_IRWXU);
			if (snprintf(pname, sizeof(pname), "%s/..", name) <
			    sizeof(pname)) {
				if (stat(pname, &pbuf) == 0)
					(void) chmod(pname,
					    pbuf.st_mode|S_IRWXU);
			}
			runp("rm", "rm", "-rf", name, 0);
		}
		if (lstat(name, &sbuf) == 0) {
			notify("SUP: Unable to delete directory %s\n", name);
			t->Tflags |= FUPDATE;
			return (SCMOK);
		}
		vnotify("SUP Deleted directory %s\n", name);
		return (SCMOK);
	}
	/* it is a file */
	if (Tlookup(refuseT, name)) {
		vnotify("SUP Would not delete file %s\n", name);
		return (SCMOK);
	}
	if (thisC->Cflags&CFLIST) {
		vnotify("SUP Would delete file %s\n", name);
		return (SCMOK);
	}
	if ((thisC->Cflags&CFDELETE) == 0) {
		notify("SUP Please delete file %s\n", name);
		t->Tflags |= FUPDATE;
		return (SCMOK);
	}
	x = unlink(name);
	if (x < 0) {
		notify("SUP: Unable to delete file %s\n", name);
		t->Tflags |= FUPDATE;
		return (SCMOK);
	}
	vnotify("SUP Deleted file %s\n", name);
	return (SCMOK);
}

/***************************************
 ***    R E C E I V E   F I L E S    ***
 ***************************************/

/*
 * Note for these routines, return code SCMOK generally means
 * NETWORK communication is OK; it does not mean that the current
 * file was correctly received and stored.  If a file gets messed
 * up, too bad, just print a message and go on to the next one;
 * but if the network gets messed up, the whole sup program loses
 * badly and best just stop the program as soon as possible.
 */
void recvfiles (void)
{
	int x;
	int recvmore;

	/* Does the protocol support compression */
	if (cancompress) {
		/* Check for compression on sending files */
		docompress = (thisC->Cflags&CFCOMPRESS);
		x = msgcompress();
		if ( x != SCMOK) 
			goaway("Error sending compression check to server");
		if (docompress)
			vnotify("SUP Using compressed file transfer\n");
	}
	recvmore = TRUE;
	upgradeT = NULL;
	do {
		x = msgsend();
		if (x != SCMOK)
			goaway("Error sending receive file request to file server");
		(void) Tinsert(&upgradeT, NULL, FALSE);
		x = msgrecv(recvone, &recvmore);
		if (x != SCMOK)
			goaway("Error receiving file from file server");
		Tfree(&upgradeT);
	} while (recvmore);
}

/* prepare the target, if necessary */
int
prepare(name, mode, newp, statp)
	char *name;
	int mode, *newp;
	struct stat *statp;
{
	char *type;
	char pname[MAXPATHLEN];
	struct stat pbuf;

	if (mode == S_IFLNK)
		*newp = (lstat(name, statp) < 0);
	else
		*newp = (stat(name, statp) < 0);
	if (*newp) {
		if (thisC->Cflags&CFLIST)
			return (FALSE);
		if (establishdir(name))
			return (TRUE);
		return (FALSE);
	}
	if (mode == (statp->st_mode&S_IFMT))
		return (FALSE);
	*newp = TRUE;
	switch (statp->st_mode&S_IFMT) {
	case S_IFDIR:
		type = "directory";
		break;
	case S_IFLNK:
		type = "symbolic link";
		break;
	case S_IFREG:
		type = "regular file";
		break;
	default:
		type = "unknown file";
		break;
	}
	if (thisC->Cflags&CFLIST) {
		vnotify("SUP Would remove %s %s\n", type, name);
		return (FALSE);
	}
	if (S_ISDIR(statp->st_mode)) {
		if (rmdir (name) < 0) {
			(void) chmod(name, statp->st_mode|S_IRWXU);
			if (snprintf(pname, sizeof(pname), "%s/..", name) <
			    sizeof(pname)) {
				if (stat(pname, &pbuf) == 0)
					(void) chmod(pname,
					    pbuf.st_mode|S_IRWXU);
			}
			runp ("rm", "rm", "-rf", name, 0);
		}
	} else
		(void) unlink(name);
	if (stat(name, statp) < 0) {
		vnotify("SUP Removed %s %s\n", type, name);
		return (FALSE);
	}
	notify("SUP: Couldn't remove %s %s\n", type, name);
	return (TRUE);
}

static int
recvone(t, ap)
	TREE *t;
	va_list ap;
{
	int x = 0;
	int new;
	struct stat sbuf;
	int *recvmore;

	recvmore = va_arg(ap, int *);
	va_end(ap);
	/* check for end of file list */
	if (t == NULL) {
		*recvmore = FALSE;
		return (SCMOK);
	}
	/* check for failed access at fileserver */
	if (t->Tmode == 0) {
		notify("SUP: File server unable to transfer file %s\n",
		    t->Tname);
		thisC->Cnogood = TRUE;
		return (SCMOK);
	}
	if (prepare(t->Tname, t->Tmode&S_IFMT, &new, &sbuf)) {
		notify("SUP: Can't prepare path for %s\n", t->Tname);
		if (S_ISREG(t->Tmode)) {
			x = readskip();		/* skip over file */
			if (x != SCMOK)
				goaway("Can't skip file transfer");
		}
		thisC->Cnogood = TRUE;
		return (SCMOK);
	}
	/* make file mode specific changes */
	switch (t->Tmode&S_IFMT) {
	case S_IFDIR:
		x = recvdir(t, new, &sbuf);
		break;
	case S_IFLNK:
		x = recvsym(t, new, &sbuf);
		break;
	case S_IFREG:
		x = recvreg(t, new, &sbuf);
		break;
	default:
		goaway("Unknown file type %o\n", t->Tmode&S_IFMT);
	}
	if (x) {
		thisC->Cnogood = TRUE;
		return (SCMOK);
	}
	if (S_ISREG(t->Tmode))
		(void) Tprocess(t->Tlink, linkone, t->Tname);
	(void) Tprocess(t->Texec, execone, NULL);
	return (SCMOK);
}

int
recvdir(t, new, statp)		/* receive directory from network */
	TREE *t;
	int new;
	struct stat *statp;
{
	struct timeval tbuf[2];

	if (new) {
		if (thisC->Cflags&CFLIST) {
			vnotify("SUP Would create directory %s\n", t->Tname);
			return (FALSE);
		}
		if (makedir(t->Tname, 0755, statp) == -1) {
			vnotify("SUP: Can't create directory %s\n", t->Tname);
			return TRUE;
		}
	}
	if ((t->Tflags&FNOACCT) == 0) {
		/* convert user and group names to local ids */
		ugconvert(t->Tuser, t->Tgroup, &t->Tuid, &t->Tgid, &t->Tmode);
	}
	if (!new && (t->Tflags&FNEW) == 0 && statp->st_mtime == t->Tmtime) {
		if (t->Tflags&FNOACCT)
			return (FALSE);
		if (statp->st_uid == t->Tuid && statp->st_gid == t->Tgid)
			return (FALSE);
	}
	if (thisC->Cflags&CFLIST) {
		vnotify ("SUP Would update directory %s\n", t->Tname);
		return (FALSE);
	}
	if ((t->Tflags&FNOACCT) == 0) {
		(void) chown(t->Tname, t->Tuid, t->Tgid);
		(void) chmod(t->Tname, t->Tmode&S_IMODE);
	}
	tbuf[0].tv_sec = time(NULL);
	tbuf[0].tv_usec = 0;
	tbuf[1].tv_sec = t->Tmtime;
	tbuf[1].tv_usec = 0;
	if (!noutime)
		(void) utimes(t->Tname, tbuf);
	if (new)
		vnotify("SUP Created directory %s\n", t->Tname);
	else
		v2notify("SUP Updated directory %s\n", t->Tname);
	return (FALSE);
}

int
recvsym(t, new, statp)			/* receive symbolic link */
	TREE *t;
	int new;
	struct stat *statp;
{
	char buf[STRINGLENGTH];
	int n;
	char *linkname;

	if (t->Tlink == NULL || t->Tlink->Tname == NULL) {
		notify("SUP: Missing linkname for symbolic link %s\n",
			t->Tname);
		return (TRUE);
	}
	linkname = t->Tlink->Tname;
	memset(buf, 0, sizeof(buf));
	if (!new && (t->Tflags&FNEW) == 0 &&
	    (n = readlink(t->Tname, buf, sizeof(buf)-1)) >= 0 &&
	    (n == strlen(linkname)) && (strncmp (linkname, buf, n) == 0))
		return (FALSE);
	if (thisC->Cflags&CFLIST) {
		vnotify("SUP Would %s symbolic link %s to %s\n",
			new ? "create" : "update", t->Tname, linkname);
		return (FALSE);
	}
	if (!new)
		(void) unlink(t->Tname);
	if (symlink(linkname, t->Tname) < 0 || lstat(t->Tname, statp) < 0) {
		notify("SUP: Unable to create symbolic link %s\n", t->Tname);
		return (TRUE);
	}
	vnotify("SUP Created symbolic link %s to %s\n", t->Tname, linkname);
	return (FALSE);
}

int
recvreg(t, new, statp)			/* receive file from network */
	TREE *t;
	int new;
	struct stat *statp;
{
	FILE *fin, *fout;
	char dirpart[STRINGLENGTH], filepart[STRINGLENGTH];
	char filename[STRINGLENGTH], buf[STRINGLENGTH];
	struct timeval tbuf[2];
	int x;
	char *p;

	if (t->Tflags&FUPDATE) {
		if ((t->Tflags&FNOACCT) == 0) {
			/* convert user and group names to local ids */
			ugconvert(t->Tuser, t->Tgroup, &t->Tuid, &t->Tgid,
			    &t->Tmode);
		}
		if (!new && (t->Tflags&FNEW) == 0 &&
		    statp->st_mtime == t->Tmtime) {
			if (t->Tflags&FNOACCT)
				return (FALSE);
			if (statp->st_uid == t->Tuid &&
			    statp->st_gid == t->Tgid)
				return (FALSE);
		}
		if (thisC->Cflags&CFLIST) {
			vnotify("SUP Would update file %s\n", t->Tname);
			return (FALSE);
		}
		vnotify("SUP Updating file %s\n", t->Tname);
		if ((t->Tflags&FNOACCT) == 0) {
			(void) chown(t->Tname, t->Tuid, t->Tgid);
			(void) chmod(t->Tname, t->Tmode&S_IMODE);
		}
		tbuf[0].tv_sec = time(NULL);
		tbuf[0].tv_usec = 0;
		tbuf[1].tv_sec = t->Tmtime;
		tbuf[1].tv_usec = 0;
		if (!noutime)
			(void) utimes(t->Tname, tbuf);
		return (FALSE);
	}
	if (thisC->Cflags&CFLIST) {
		if (new)
			p = "create";
		else if (statp->st_mtime < t->Tmtime)
			p = "receive new";
		else if (statp->st_mtime > t->Tmtime)
			p = "receive old";
		else
			p = "receive";
		vnotify("SUP Would %s file %s\n", p, t->Tname);
		return (FALSE);
	}
	vnotify("SUP Receiving file %s\n", t->Tname);
	if (!new && S_ISREG(t->Tmode) &&
	    (t->Tflags&FBACKUP) && (thisC->Cflags&CFBACKUP)) {
		fin = fopen(t->Tname, "r");	/* create backup */
		if (fin == NULL) {
			x = readskip();		/* skip over file */
			if (x != SCMOK)
				goaway("Can't skip file transfer");
			notify("SUP: Can't open %s to create backup\n",
			    t->Tname);
			return (TRUE);		/* mark upgrade as nogood */
		}
		path(t->Tname, dirpart, sizeof dirpart, filepart, sizeof filepart);
		(void) snprintf(filename, sizeof filename, FILEBACKUP,
		    dirpart, filepart);
		fout = fopen(filename, "w");
		if (fout == NULL) {
			(void) snprintf(buf, sizeof buf, FILEBKDIR, dirpart);
			(void) mkdir(buf, 0755);
			fout = fopen(filename, "w");
		}
		if (fout == NULL) {
			x = readskip();		/* skip over file */
			if (x != SCMOK)
				goaway("Can't skip file transfer");
			notify("SUP: Can't create %s for backup\n", filename);
			(void) fclose(fin);
			return (TRUE);
		}
		ffilecopy(fin, fout);
		(void) fclose(fin);
		(void) fclose(fout);
		vnotify("SUP Backup of %s created\n", t->Tname);
	}
	x = copyfile(t->Tname, NULL);
	if (x)
		return (TRUE);
	if ((t->Tflags&FNOACCT) == 0) {
		/* convert user and group names to local ids */
		ugconvert(t->Tuser, t->Tgroup, &t->Tuid, &t->Tgid, &t->Tmode);
		(void) chown(t->Tname, t->Tuid, t->Tgid);
		(void) chmod(t->Tname, t->Tmode&S_IMODE);
	}
	tbuf[0].tv_sec = time(NULL);
	tbuf[0].tv_usec = 0;
	tbuf[1].tv_sec = t->Tmtime;
	tbuf[1].tv_usec = 0;
	if (!noutime)
		(void) utimes(t->Tname, tbuf);
	return (FALSE);
}

static int
linkone(t, fv)			/* link to file already received */
	TREE *t;
	void *fv;
{
	char *fname = fv;
	struct stat fbuf,sbuf;
	char *name = t->Tname;
	int new,x;
	char *type;

	if (stat(fname, &fbuf) < 0) {	/* source file */
		if (thisC->Cflags&CFLIST) {
			vnotify("SUP Would link %s to %s\n", name, fname);
			return (SCMOK);
		}
		notify("SUP: Can't link %s to missing file %s\n", name, fname);
		thisC->Cnogood = TRUE;
		return (SCMOK);
	}
	if (prepare(name,S_IFREG, &new, &sbuf)) {
		notify("SUP: Can't prepare path for link %s\n", name);
		thisC->Cnogood = TRUE;
		return (SCMOK);
	}
	if (!new && (t->Tflags&FNEW) == 0 &&
	    fbuf.st_dev == sbuf.st_dev && fbuf.st_ino == sbuf.st_ino)
		return (SCMOK);
	if (thisC->Cflags&CFLIST) {
		vnotify("SUP Would link %s to %s\n", name, fname);
		return (SCMOK);
	}
	(void) unlink(name);
	type = "";
	if ((x = link(fname,name)) < 0) {
		type = "symbolic ";
		x = symlink(fname, name);
	}
	if (x < 0 || lstat(name, &sbuf) < 0) {
		notify ("SUP: Unable to create %slink %s\n", type, name);
		return (TRUE);
	}
	vnotify("SUP Created %slink %s to %s\n", type, name, fname);
	return (SCMOK);
}

static int
execone(t, v)			/* execute command for file */
	TREE *t;
	void *v;
{
	int w;

	if (thisC->Cflags&CFLIST) {
		vnotify("SUP Would execute %s\n", t->Tname);
		return (SCMOK);
	}
	if ((thisC->Cflags&CFEXECUTE) == 0) {
		notify("SUP Please execute %s\n", t->Tname);
		return (SCMOK);
	}
	vnotify("SUP Executing %s\n", t->Tname);

	w = system(t->Tname);
	if (WIFEXITED(w) && WEXITSTATUS(w) != 0) {
		notify("SUP: Execute command returned failure status %#o\n",
		    WEXITSTATUS(w));
		thisC->Cnogood = TRUE;
	} else if (WIFSIGNALED(w)) {
		notify("SUP: Execute command killed by signal %d\n",
		    WTERMSIG(w));
		thisC->Cnogood = TRUE;
	} else if (WIFSTOPPED(w)) {
		notify("SUP: Execute command stopped by signal %d\n",
		    WSTOPSIG(w));
		thisC->Cnogood = TRUE;
	}
	return (SCMOK);
}

int
copyfile(to, from)
	char *to;
	char *from;		/* 0 if reading from network */
{
	int fromf, tof, istemp, x;
	char dpart[STRINGLENGTH], fpart[STRINGLENGTH];
	char tname[MAXPATHLEN];
	static int true = 1;
	static pid_t thispid = 0;	/* process id # */

	if (from) {			/* reading file */
		fromf = open(from, O_RDONLY, 0);
		if (fromf < 0) {
			notify("SUP: Can't open %s to copy to %s: %s\n",
			    from, to, errmsg(-1));
			return (TRUE);
		}
	} else				/* reading network */
		fromf = -1;
	istemp = TRUE;			/* try to create temp file */
	lockout(TRUE);			/* block interrupts */
	if (thispid == 0)
		thispid = getpid();
	/* Now try hard to find a temp file name.  Try VERY hard. */
	for (;;) {
	/* try destination directory */
		path(to, dpart, sizeof dpart, fpart, sizeof fpart);
		(void) snprintf(tname, sizeof tname, "%s/#%ld.sup.XXXXXXXXXX",
		    dpart, (long)thispid);
		tof = mkstemp(tname);
		if (tof >= 0)
			break;
	/* try sup directory */
		if (thisC->Cprefix)
			(void) chdir (thisC->Cbase);
		(void) snprintf(tname, sizeof tname, "sup/#%ld.sup.XXXXXXXXXX",
		    (long)thispid);
		tof = mkstemp(tname);
		if (tof >= 0) {
			if (thisC->Cprefix)
				(void) chdir(thisC->Cprefix);
			break;
		}
	/* try base directory */
		(void) snprintf(tname, sizeof tname, "#%ld.sup.XXXXXXXXXX",
		    (long)thispid);
		tof = mkstemp(tname);
		if (thisC->Cprefix)
			(void) chdir(thisC->Cprefix);
		if (tof >= 0)
			break;
#ifdef	VAR_TMP
	/* try /var/tmp */
		(void) snprintf(tname, sizeof tname,
		    "/var/tmp/#%ld.sup.XXXXXXXXXX", (long)thispid);
		tof = mkstemp(tname);
		if (tof >= 0)
			break;
#else
	/* try /usr/tmp */
		(void) snprintf(tname, sizeof tname,
		    "/usr/tmp/#%ld.sup.XXXXXXXXXX", (long)thispid);
		tof = mkstemp(tname);
		if (tof >= 0)
			break;
#endif
	/* try /tmp */
		(void) snprintf(tname, sizeof tname,
		    "/tmp/#%ld.sup.XXXXXXXXXX", (long)thispid);
		tof = mkstemp(tname);
		if (tof >= 0)
			break;
		istemp = FALSE;
	/* give up: try to create output file */
		if (!docompress)
			tof = open(to, (O_WRONLY|O_CREAT|O_TRUNC), 0600);
		if (tof >= 0)
			break;
	/* no luck */
		notify("SUP: Can't create %s or temp file for it\n", to);
		lockout (FALSE);
		if (fromf >= 0)
			(void) close (fromf);
		else {
			x = readskip();
			if (x != SCMOK)
				goaway("Can't skip file transfer");
		}
		if (true)
			return (TRUE);
	}
	if (fromf >= 0) {		/* read file */
		x = filecopy(fromf, tof);
		(void) close(fromf);
		(void) close(tof);
		if (x < 0) {
			notify("SUP: Error in copying %s to %s\n", from, to);
			if (istemp)
				(void) unlink(tname);
			lockout(FALSE);
			return (TRUE);
		}
	} else {			/* read network */
#if	MACH
		if (!rpauseflag) {
			int fsize;
			struct fsparam fsp;

			x = prereadcount(&fsize);
			if (x != SCMOK) {
				if (istemp)
					(void) unlink(tname);
				lockout(FALSE);
				x = readskip();
				if (x != SCMOK)
					goaway("Can't skip file transfer");
				goaway("Error in server space check");
				logquit(1, "Error in server space check");
			}
			errno = 0;
			if (ioctl(tof, FIOCFSPARAM, (char *)&fsp) < 0 &&
			    errno != EINVAL) {
				if (istemp)
					(void) unlink(tname);
				lockout(FALSE);
				x = readskip();
				if (x != SCMOK)
					goaway("Can't skip file transfer");
				goaway("Error in disk space check");
				logquit(1, "Error in disk space check");
			}
			if (errno == 0) {
				fsize = (fsize + 1023) / 1024;
				x = fsp.fsp_size * MAX (fsp.fsp_minfree, 1) / 100;
				fsp.fsp_free -= x;
				if (fsize > MAX (fsp.fsp_free, 0)) {
					if (istemp)
						(void) unlink(tname);
					lockout(FALSE);
					x = readskip();
					if (x != SCMOK)
						goaway("Can't skip file transfer");
					goaway("No disk space for file %s", to);
					logquit (1, "No disk space for file %s",
					    to);
				}
			}
		}
#endif	/* MACH */
		x = readfile(tof);
		(void) close(tof);
		if (x != SCMOK) {
			if (istemp)
				(void) unlink(tname);
			lockout(FALSE);
			goaway("Error in receiving %s\n", to);
		}
	}
	if (!istemp) {			/* no temp file used */
		lockout(FALSE);
		return (FALSE);
	}
	/*
	 * If the file is compressed, uncompress it in place.  We open the
	 * temp file for reading, unlink the file, and then open the same
	 * file again for writing.  Then we pipe through gzip.  When 
	 * finished the temp file contains the uncompressed version and we
	 * can continue as before.
	 *
	 * Since sup prefers to write close to the original file the
	 * benefits of atomic updates probably outweigh the cost of the
	 * extra filecopy which occurs when the temp file is on a different
	 * filesystem from the original.
	 */
	if (docompress) {
		char *av[4];
		int   ac = 0;
		int   infd = -1;
		int   outfd = -1;
		av[ac++] = "gzip";
		av[ac++] = "-d";
		av[ac++] = NULL;
		/* XXX - race between unlink and re-open */
		if ((infd = open(tname, O_RDONLY, 0)) == -1 ||
		     unlink(tname) == -1 ||
		     (outfd = open(tname, O_WRONLY|O_CREAT|O_EXCL, 0600)) == -1 ||
		     runiofd(av, infd, outfd, 2) != 0 ) {
			notify("SUP: Error in uncompressing file %s (%s)\n",
				to, tname);
			(void) unlink(tname);
			if (infd != -1)
				(void) close(infd);
			if (outfd != -1)
				(void) close(outfd);
			lockout(FALSE);
			return(TRUE);
		}
		(void) close(infd);
		(void) close(outfd);
	}
	/* move to destination */
	if (rename(tname, to) == 0) {
		(void) unlink(tname);
		lockout(FALSE);
		return(FALSE);
	}
	fromf = open(tname, O_RDONLY, 0);
	if (fromf < 0) {
		notify ("SUP: Error in moving temp file to %s: %s\n",
			to, errmsg (-1));
		(void) unlink(tname);
		lockout(FALSE);
		return (TRUE);
	}
	tof = open(to, O_WRONLY|O_CREAT|O_TRUNC, 0600);
	if (tof < 0) {
		(void) close(fromf);
		notify("SUP: Can't create %s from temp file: %s\n",
			to, errmsg(-1));
		(void) unlink(tname);
		lockout(FALSE);
		return (TRUE);
	}
	x = filecopy(fromf, tof);
	(void) close(fromf);
	(void) close(tof);
	(void) unlink(tname);
	lockout(FALSE);
	if (x < 0) {
		notify("SUP: Error in storing data in %s\n", to);
		return (TRUE);
	}
	return (FALSE);
}

/***  Finish connection with file server ***/

void
finishup(x)
	int x;
{
	char tname[STRINGLENGTH], fname[STRINGLENGTH];
	char relsufix[STRINGLENGTH];
	char collrelname[STRINGLENGTH];
	time_t tloc;
	FILE *finishfile;		/* record of all filenames */

	if ((thisC->Cflags&CFURELSUF) && release) {
		(void) snprintf(relsufix, sizeof relsufix, ".%s", release);
		(void) snprintf(collrelname, sizeof collrelname,
		    "%s-%s", collname, release);
	} else {
		relsufix[0] = '\0';
		(void) strlcpy(collrelname, collname, sizeof collrelname);
	}
	dontjump = TRUE;		/* once here, no more longjmp */
	(void) netcrypt(NULL);
	if (protver < 6) {
		/* done with server */
		if (x == SCMOK)
			goaway(NULL);
		(void) requestend();
	}
	tloc = time(NULL);
	if (x != SCMOK) {
		notify("SUP: Upgrade of %s aborted at %s", collrelname,
		    ctime(&tloc) + 4);
		Tfree(&lastT);
		if (protver < 6)
			return;
		/* if we've not been blown off, make sure he is! */
		if (x != SCMEOF)
			goaway("Aborted");
		(void) requestend();
		return;
	}
	if (thisC->Cnogood) {
		notify("SUP: Upgrade of %s completed with errors at %s",
		    collrelname, ctime(&tloc) + 4);
		notify("SUP: Upgrade time will not be updated\n");
		Tfree(&lastT);
		if (protver < 6)
			return;
		done(FDONEUSRERROR, "Completed with errors");
		(void) requestend();
		return;
	}
	if (thisC->Cprefix)
		(void) chdir(thisC->Cbase);
	vnotify("SUP Upgrade of %s completed at %s", collrelname,
	    ctime(&tloc) + 4);
	if (thisC->Cflags&CFLIST) {
		Tfree(&lastT);
		if (protver < 6)
			return;
		done(FDONEDONTLOG, "List only");
		(void) requestend();
		return;
	}
	(void) snprintf(fname, sizeof fname, FILEWHEN, collname, relsufix);
	if (establishdir(fname)) {
		notify("SUP: Can't create directory for upgrade timestamp\n");
		Tfree(&lastT);
		if (protver < 6)
			return;
		done(FDONEUSRERROR, "Couldn't timestamp");
		(void) requestend();
		return;
	}
	if (!putwhen(fname, scantime)) {
		notify("SUP: Can't record current time in %s: %s\n",
		    fname,errmsg (-1));
		Tfree(&lastT);
		if (protver < 6)
			return;
		done(FDONEUSRERROR,"Couldn't timestamp");
		(void) requestend();
		return;
	}
	if (protver >= 6) {
		/*
		 * At this point we have let the server go
		 * "I'm sorry, we've had to let you go"
		 */
		done(FDONESUCCESS, "Success");
		(void) requestend();
	}
	(void) snprintf(tname, sizeof tname, FILELASTTEMP, collname, relsufix);
	finishfile = fopen(tname, "w");
	if (finishfile == NULL) {
		notify("SUP: Can't record list of all files in %s\n", tname);
		Tfree(&lastT);
		return;
	}
	(void) Tprocess(lastT, finishone, finishfile);
	(void) fclose(finishfile);
	(void) snprintf(fname, sizeof fname, FILELAST, collname, relsufix);
	if (rename(tname, fname) < 0)
		notify("SUP: Can't change %s to %s\n", tname, fname);
	(void) unlink(tname);
	Tfree(&lastT);
}

int
finishone(t, fv)
	TREE *t;
	void *fv;
{
	FILE *finishfile = fv;

	if ((thisC->Cflags&CFDELETE) == 0 || (t->Tflags&FUPDATE))
		fprintf(finishfile, "%s\n", t->Tname);
	return (SCMOK);
}

void
done (int value,char *fmt,...)
{
	char buf[STRINGLENGTH];
	va_list ap;

	va_start(ap, fmt);
	(void) netcrypt(NULL);

	if (fmt) 
		vsnprintf(buf, sizeof(buf), fmt, ap);
	va_end(ap);
	if (protver < 6) {
		if (goawayreason)
			free(goawayreason);
		goawayreason = (fmt) ? strdup(buf) : NULL;
		(void) msggoaway();
	}
	else {
		doneack = value;
		donereason = (fmt) ? buf : NULL;
		(void) msgdone();
	}
	if (!dontjump)
		longjmp(sjbuf, TRUE);
}

void
goaway (char *fmt,...)
{
	char buf[STRINGLENGTH];
	va_list ap;

	va_start(ap, fmt);
	(void) netcrypt (NULL);
	if (fmt) {
		vsnprintf(buf, sizeof(buf), fmt, ap);
		goawayreason = buf;
	} else
		goawayreason = NULL;
	va_end(ap);
	(void) msggoaway();
	if (fmt) {
		if (thisC)
			notify("SUP: %s\n", buf);
		else
			printf("SUP: %s\n", buf);
	}
	if (!dontjump)
		longjmp(sjbuf,TRUE);
}
