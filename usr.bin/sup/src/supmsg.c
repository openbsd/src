/*	$OpenBSD: supmsg.c,v 1.12 2002/08/12 00:42:56 aaron Exp $	*/

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
 **********************************************************************
 * HISTORY
 *
 * 7-July-93  Nate Williams at Montana State University
 *	Modified SUP to use gzip based compression when sending files
 *	across the network to save BandWidth
 *
 * Revision 2.4  92/09/09  22:05:17  mrt
 * 	Moved PFI definition under __STDC__ conditional since it
 * 	is already defined in libc.h in this case.
 * 	[92/09/01            mrt]
 * 
 * Revision 2.3  92/08/11  12:08:12  mrt
 * 	Added copyright
 * 	[92/08/10            mrt]
 * 	Brad's changes: Delinted, Incorporated updated variable 
 * 	argument list usage from old msgxfer.c
 * 	[92/07/24            mrt]
 * 
 * Revision 2.2  89/08/23  15:02:56  gm0w
 * 	Created from separate message modules.
 * 	[89/08/14            gm0w]
 * 
 **********************************************************************
 */
#include <stdio.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <libc.h>
#include <c.h>
#include "supcdefs.h"
#include "supextern.h"
#define MSGSUBR
#define MSGFILE
#include "supmsg.h"

/*
 * signon message
 */
extern int	pgmver;			/* program version of partner */
extern int	pgmversion;		/* my program version */
extern char	*scmver;		/* scm version of partner */
extern pid_t	fspid;			/* process id of fileserver */

static int refuseone(TREE *, void *);
static int listone(TREE *, void *);
static int needone(TREE *, void *);
static int denyone(TREE *, void *);
static int writeone(TREE *, void *);

int
msgsignon()
{
	int x;

	if (server) {
		x = readmsg(MSGSIGNON);
		if (x == SCMOK)
			x = readint(&protver);
		if (x == SCMOK)
			x = readint(&pgmver);
		if (x == SCMOK)
			x = readstring(&scmver);
		if (x == SCMOK)
			x = readmend();
	} else {
		x = writemsg(MSGSIGNON);
		if (x == SCMOK)
			x = writeint(PROTOVERSION);
		if (x == SCMOK)
			x = writeint(pgmversion);
		if (x == SCMOK)
			x = writestring(scmversion);
		if (x == SCMOK)
			x = writemend();
	}
	return (x);
}

int
msgsignonack()
{
	int x;

	if (server) {
		x = writemsg(MSGSIGNONACK);
		if (x == SCMOK)
			x = writeint(PROTOVERSION);
		if (x == SCMOK)
			x = writeint(pgmversion);
		if (x == SCMOK)
			x = writestring(scmversion);
		if (x == SCMOK)
			x = writeint(fspid);
		if (x == SCMOK)
			x = writemend();
	} else {
		x = readmsg(MSGSIGNONACK);
		if (x == SCMOK)
			x = readint(&protver);
		if (x == SCMOK)
			x = readint(&pgmver);
		if (x == SCMOK)
			x = readstring(&scmver);
		if (x == SCMOK)
			x = readint(&fspid);
		if (x == SCMOK)
			x = readmend();
	}
	return (x);
}

/*
 * setup message
 */
extern int	xpatch;			/* setup crosspatch to a new client */
extern char	*xuser;			/* user,group,acct for crosspatch */
extern char	*collname;		/* base directory */
extern char	*basedir;		/* base directory */
extern int	basedev;		/* base directory device */
extern int	baseino;		/* base directory inode */
extern time_t	lasttime;		/* time of last upgrade */
extern int	listonly;		/* only listing files, no data xfer */
extern int	newonly;		/* only send new files */
extern char	*release;		/* release name */
extern int	setupack;		/* ack return value for setup */

int
msgsetup()
{
	int x;

	if (server) {
		x = readmsg(MSGSETUP);
		if (x != SCMOK)
			return (x);
		if (protver >= 7) {
			x = readint(&xpatch);
			if (x != SCMOK)
				return (x);
		} else
			xpatch = FALSE;
		if (xpatch) {
			x = readstring(&xuser);
			if (x != SCMOK)
				return (x);
			return (readmend());
		}
		x = readstring(&collname);
		if (x == SCMOK)
			x = readint((int *)&lasttime);
		if (x == SCMOK)
			x = readstring(&basedir);
		if (x == SCMOK)
			x = readint(&basedev);
		if (x == SCMOK)
			x = readint(&baseino);
		if (x == SCMOK)
			x = readint(&listonly);
		if (x == SCMOK)
			x = readint(&newonly);
		if (x == SCMOK) {
			if (protver < 6)
				release = NULL;
			else
				x = readstring(&release);
		}
		if (x == SCMOK)
			x = readmend();
	} else {
		x = writemsg(MSGSETUP);
		if (x != SCMOK)
			return (x);
		if (protver >= 7) {
			x = writeint(xpatch);
			if (x != SCMOK)
				return (x);
		}
		if (xpatch) {
			x = writestring(xuser);
			if (x != SCMOK)
				return (x);
			return (writemend());
		}
		if (x == SCMOK)
			x = writestring(collname);
		if (x == SCMOK)
			x = writeint((int)lasttime);
		if (x == SCMOK)
			x = writestring(basedir);
		if (x == SCMOK)
			x = writeint(basedev);
		if (x == SCMOK)
			x = writeint(baseino);
		if (x == SCMOK)
			x = writeint(listonly);
		if (x == SCMOK)
			x = writeint(newonly);
		if (x == SCMOK && protver >= 6)
			x = writestring(release);
		if (x == SCMOK)
			x = writemend();
	}
	return (x);
}

int
msgsetupack()
{

	if (server)
		return (writemint(MSGSETUPACK, setupack));
	return (readmint(MSGSETUPACK, &setupack));
}

/*
 * crypt test message
 */
extern char	*crypttest;		/* encryption test string */

int
msgcrypt()
{

	if (server)
		return (readmstr(MSGCRYPT, &crypttest));
	return (writemstr(MSGCRYPT, crypttest));
}

int
msgcryptok()
{

	if (server)
		return (writemnull(MSGCRYPTOK));
	return (readmnull(MSGCRYPTOK));
}

/*
 * login message
 */
extern char	*logcrypt;		/* login encryption test */
extern char	*loguser;		/* login username */
extern char	*logpswd;		/* password for login */
extern int	logack;			/* login ack status */
extern char	*logerror;		/* error from login */

int
msglogin()
{
	int x;

	if (server) {
		x = readmsg(MSGLOGIN);
		if (x == SCMOK)
			x = readstring(&logcrypt);
		if (x == SCMOK)
			x = readstring(&loguser);
		if (x == SCMOK)
			x = readstring(&logpswd);
		if (x == SCMOK)
			x = readmend();
	} else {
		x = writemsg(MSGLOGIN);
		if (x == SCMOK)
			x = writestring(logcrypt);
		if (x == SCMOK)
			x = writestring(loguser);
		if (x == SCMOK)
			x = writestring(logpswd);
		if (x == SCMOK)
			x = writemend();
	}
	return (x);
}

int
msglogack()
{
	int x;

	if (server) {
		x = writemsg(MSGLOGACK);
		if (x == SCMOK)
			x = writeint(logack);
		if (x == SCMOK)
			x = writestring(logerror);
		if (x == SCMOK)
			x = writemend();
	} else {
		x = readmsg(MSGLOGACK);
		if (x == SCMOK)
			x = readint(&logack);
		if (x == SCMOK)
			x = readstring(&logerror);
		if (x == SCMOK)
			x = readmend();
	}
	return (x);
}

/*
 * refuse list message
 */
extern TREE	*refuseT;		/* tree of files to refuse */

static int
refuseone(t, v)
	TREE *t;
	void *v;
{
	return (writestring(t->Tname));
}

int
msgrefuse()
{
	int x;

	if (server) {
		char *name;

		x = readmsg(MSGREFUSE);
		if (x == SCMOK)
			x = readstring(&name);
		while (x == SCMOK) {
			if (name == NULL)
				break;
			(void) Tinsert(&refuseT, name, FALSE);
			free(name);
			x = readstring(&name);
		}
		if (x == SCMOK)
			x = readmend();
	} else {
		x = writemsg(MSGREFUSE);
		if (x == SCMOK)
			x = Tprocess(refuseT, refuseone, NULL);
		if (x == SCMOK)
			x = writestring(NULL);
		if (x == SCMOK)
			x = writemend();
	}
	return (x);
}

/*
 * list files message
 */
extern TREE	*listT;			/* tree of files to list */
extern time_t	scantime;		/* time that collection was scanned */

static int
listone(t, v)
	TREE *t;
	void *v;
{
	int x;

	x = writestring(t->Tname);
	if (x == SCMOK)
		x = writeint((int)t->Tmode);
	if (x == SCMOK)
		x = writeint((int)t->Tflags);
	if (x == SCMOK)
		x = writeint(t->Tmtime);
	return (x);
}

int
msglist()
{
	int x;

	if (server) {
		x = writemsg(MSGLIST);
		if (x == SCMOK)
			x = Tprocess(listT, listone, NULL);
		if (x == SCMOK)
			x = writestring(NULL);
		if (x == SCMOK)
			x = writeint((int)scantime);
		if (x == SCMOK)
			x = writemend();
	} else {
		char *name;
		int mode, flags, mtime;
		TREE *t;

		x = readmsg(MSGLIST);
		if (x == SCMOK)
			x = readstring(&name);
		while (x == SCMOK) {
			if (name == NULL)
				break;
			x = readint(&mode);
			if (x == SCMOK)
				x = readint(&flags);
			if (x == SCMOK)
				x = readint(&mtime);
			if (x != SCMOK)
				break;
			t = Tinsert(&listT, name, TRUE);
			free(name);
			t->Tmode = mode;
			t->Tflags = flags;
			t->Tmtime = mtime;
			x = readstring(&name);
		}
		if (x == SCMOK)
			x = readint((int *)&scantime);
		if (x == SCMOK)
			x = readmend();
	}
	return (x);
}

/*
 * files needed message
 */
extern TREE	*needT;			/* tree of files to need */

static int
needone(t, v)
	TREE *t;
	void *v;
{
	int x;

	x = writestring(t->Tname);
	if (x == SCMOK)
		x = writeint((t->Tflags&FUPDATE) != 0);
	return (x);
}

int msgneed()
{
	int x;

	if (server) {
		char *name;
		int update;
		TREE *t;

		x = readmsg(MSGNEED);
		if (x == SCMOK)
			x = readstring(&name);
		while (x == SCMOK) {
			if (name == NULL)
				break;
			x = readint(&update);
			if (x != SCMOK)
				break;
			t = Tinsert(&needT, name, TRUE);
			free(name);
			if (update)
				t->Tflags |= FUPDATE;
			x = readstring(&name);
		}
		if (x == SCMOK)
			x = readmend();
	} else {
		x = writemsg(MSGNEED);
		if (x == SCMOK)
			x = Tprocess(needT, needone, NULL);
		if (x == SCMOK)
			x = writestring(NULL);
		if (x == SCMOK)
			x = writemend();
	}
	return (x);
}

/*
 * files denied message
 */
extern TREE	*denyT;			/* tree of files to deny */

static int
denyone(t, v)
	TREE *t;
	void *v;
{

	return (writestring(t->Tname));
}

int
msgdeny()
{
	int x;

	if (server) {
		x = writemsg(MSGDENY);
		if (x == SCMOK)
			x = Tprocess(denyT,denyone, NULL);
		if (x == SCMOK)
			x = writestring(NULL);
		if (x == SCMOK)
			x = writemend();
	} else {
		char *name;

		x = readmsg(MSGDENY);
		if (x == SCMOK)
			x = readstring(&name);
		while (x == SCMOK) {
			if (name == NULL)
				break;
			(void) Tinsert(&denyT, name, FALSE);
			free(name);
			x = readstring(&name);
		}
		if (x == SCMOK)
			x = readmend();
	}
	return (x);
}

/*
 * send file message
 */
int
msgsend()
{

	if (server)
		return (readmnull(MSGSEND));
	return (writemnull(MSGSEND));
}

/*
 * receive file message
 */
extern TREE	*upgradeT;		/* pointer to file being upgraded */

static int
writeone(t, v)
	TREE *t;
	void *v;
{

	return (writestring(t->Tname));
}


int
msgrecv(int (*xferfile)(TREE *, va_list),...)
{
	va_list args;
	int x;
	TREE *t = upgradeT;
	va_start(args,xferfile);
	if (server) {
		x = writemsg (MSGRECV);
		if (t == NULL) {
			if (x == SCMOK)
				x = writestring(NULL);
			if (x == SCMOK)
				x = writemend();
			goto done;
		}
		if (x == SCMOK)
			x = writestring(t->Tname);
		if (x == SCMOK)
			x = writeint(t->Tmode);
		if (t->Tmode == 0) {
			if (x == SCMOK)
				x = writemend();
			goto done;
		}
		if (x == SCMOK)
			x = writeint(t->Tflags);
		if (x == SCMOK)
			x = writestring(t->Tuser);
		if (x == SCMOK)
			x = writestring(t->Tgroup);
		if (x == SCMOK)
			x = writeint(t->Tmtime);
		if (x == SCMOK)
			x = Tprocess(t->Tlink, writeone, NULL);
		if (x == SCMOK)
			x = writestring(NULL);
		if (x == SCMOK)
			x = Tprocess(t->Texec,writeone, NULL);
		if (x == SCMOK)
			x = writestring(NULL);
		if (x == SCMOK)
			x = (*xferfile)(t, args);
		if (x == SCMOK)
			x = writemend();
	} else {
		char *linkname, *execcmd;

		if (t == NULL) {
			x = SCMERR;
			goto done;
		}
		x = readmsg (MSGRECV);
		if (x == SCMOK)
			x = readstring(&t->Tname);
		if (x == SCMOK && t->Tname == NULL) {
			x = readmend();
			if (x == SCMOK)
				x = (*xferfile)(NULL, args);
			goto done;
		}
		if (x == SCMOK)
			x = readint(&t->Tmode);
		if (t->Tmode == 0) {
			x = readmend();
			if (x == SCMOK)
				x = (*xferfile)(t, args);
			goto done;
		}
		if (x == SCMOK)
			x = readint(&t->Tflags);
		if (x == SCMOK)
			x = readstring(&t->Tuser);
		if (x == SCMOK)
			x = readstring(&t->Tgroup);
		if (x == SCMOK)
			x = readint(&t->Tmtime);
		t->Tlink = NULL;
		if (x == SCMOK)
			x = readstring(&linkname);
		while (x == SCMOK) {
			if (linkname == NULL)
				break;
			(void) Tinsert(&t->Tlink, linkname, FALSE);
			free(linkname);
			x = readstring(&linkname);
		}
		t->Texec = NULL;
		if (x == SCMOK)
			x = readstring(&execcmd);
		while (x == SCMOK) {
			if (execcmd == NULL)
				break;
			(void) Tinsert(&t->Texec, execcmd, FALSE);
			free(execcmd);
			x = readstring(&execcmd);
		}
		if (x == SCMOK)
			x = (*xferfile)(t, args);
		if (x == SCMOK)
			x = readmend();
	}

done:
	va_end(args);
	return (x);
}

/*
 * protocol done message
 */
extern int	doneack;
extern char	*donereason;

int
msgdone()
{
	int x;

	if (protver < 6) {
		printf("Error, msgdone should not have been called.");
		return (SCMERR);
	}
	if (server) {
		x = readmsg(MSGDONE);
		if (x == SCMOK)
			x = readint(&doneack);
		if (x == SCMOK)
			x = readstring(&donereason);
		if (x == SCMOK)
			x = readmend();
	} else {
		x = writemsg(MSGDONE);
		if (x == SCMOK)
			x = writeint(doneack);
		if (x == SCMOK)
			x = writestring(donereason);
		if (x == SCMOK)
			x = writemend();
	}
	return (x);
}

/*
 * go away message
 */
extern char	*goawayreason;		/* reason for goaway */

int
msggoaway()
{

	return (writemstr(MSGGOAWAY, goawayreason));
}

/*
 * cross-patch protocol message
 */
extern int	xargc;			/* arg count for crosspatch */
extern char	**xargv;		/* arg array for crosspatch */

int
msgxpatch()
{
	int x, i;

	if (server) {
		x = readmsg(MSGXPATCH);
		if (x != SCMOK)
			return (x);
		x = readint(&xargc);
		if (x != SCMOK)
			return (x);
		xargc += 2;
		xargv = (char **)calloc(xargc+1, sizeof (char *));
		if (xargv == NULL)
			return (SCMERR);
		for (i = 2; i < xargc; i++) {
			x = readstring(&xargv[i]);
			if (x != SCMOK)
				return (x);
		}
		x = readmend();
	} else {
		x = writemsg(MSGXPATCH);
		if (x != SCMOK)
			return (x);
		x = writeint(xargc);
		if (x != SCMOK)
			return (x);
		for (i = 0; i < xargc; i++) {
			x = writestring(xargv[i]);
			if (x != SCMOK)
				return (x);
		}
		x = writemend();
	}
	return (x);
}

/*
 * Compression check protocol message
 */
extern int	docompress;		/* Compress file before sending? */

int
msgcompress()
{

	if (server)
		return (readmint(MSGCOMPRESS, &docompress));
	return (writemint(MSGCOMPRESS, docompress));
}
