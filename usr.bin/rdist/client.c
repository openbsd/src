/*	$OpenBSD: client.c,v 1.7 1998/06/26 21:21:00 millert Exp $	*/

/*
 * Copyright (c) 1983 Regents of the University of California.
 * All rights reserved.
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
#if 0
static char RCSid[] = 
"$From: client.c,v 6.80 1996/02/28 20:34:27 mcooper Exp $";
#else
static char RCSid[] = 
"$OpenBSD: client.c,v 1.7 1998/06/26 21:21:00 millert Exp $";
#endif

static char sccsid[] = "@(#)client.c";

static char copyright[] =
"@(#) Copyright (c) 1983 Regents of the University of California.\n\
 All rights reserved.\n";
#endif /* not lint */

/*
 * Routines used in client mode to communicate with remove server.
 */

#include "defs.h"
#include "y.tab.h"

/*
 * Update status
 */
#define US_NOTHING 	0	/* No update needed */
#define US_NOENT	1	/* Entry does not exist */
#define US_OUTDATE	2	/* Entry is out of date */
#define US_DOCOMP	3	/* Do a binary comparison */
#define US_MODE		4	/* Modes of file differ */

struct	linkbuf *ihead = NULL;	/* list of files with more than one link */
char	buf[BUFSIZ];		/* general purpose buffer */
u_char	respbuff[BUFSIZ];	/* Response buffer */
char	target[BUFSIZ];		/* target/source directory name */
char	source[BUFSIZ];		/* source directory name */
char	*ptarget;		/* pointer to end of target name */
char	*Tdest;			/* pointer to last T dest*/
struct namelist	*updfilelist = NULL; /* List of updated files */

static int sendit();

/*
 * return remote file pathname (relative from target)
 */
char *remfilename(src, dest, path, rname, destdir)
	char *src, *dest, *path, *rname;
	int destdir;
{
	extern struct namelist *filelist;
	register char *lname, *cp;
	static char buff[BUFSIZ];
	int srclen, pathlen;
	char *p;


	debugmsg(DM_MISC, 
		 "remfilename: src=%s dest=%s path=%s rname=%s destdir=%d\n",
		A(src), A(dest), A(path), A(rname), destdir);

	if (!dest) {
		debugmsg(DM_MISC, "remfilename: remote filename=%s\n", path);
		return(path);
	}

	if (!destdir) {
		debugmsg(DM_MISC, "remfilename: remote filename=%s\n", dest);
		return(dest);
	}

	buff[0] = CNULL;
	lname = buff;
	if (path && *path) {
		cp = strrchr(path, '/');
 		if (cp == NULL)
			(void) sprintf(buff, "%s/%s", dest, path);
		else {
			srclen = strlen(src);
			pathlen = strlen(path);
			if (srclen >= pathlen)
				cp++; /* xbasename(path) */
			else {
				if (filelist && filelist->n_next == NULL)
					/* path relative to src */
					cp = path + srclen;
				else {
					if ((p = strrchr(src, '/')))
						cp = path + srclen - strlen(p);
					else
						cp = path;
				}
			}
			if ((*cp != '/') && *cp)
				(void) sprintf(buff, "%s/%s", dest, cp);
			else
				(void) sprintf(buff, "%s%s", dest, cp);
		}
	} else
		strcpy(lname, dest);

	debugmsg(DM_MISC, "remfilename: remote filename=%s\n", lname);

	return(lname);
}

/*
 * Return true if name is in the list.
 */
int inlist(list, file)
	struct namelist *list;
	char *file;
{
	register struct namelist *nl;

	for (nl = list; nl != NULL; nl = nl->n_next)
		if (strcmp(file, nl->n_name) == 0)
			return(1);
	return(0);
}

/*
 * Run any special commands for this file
 */
static void runspecial(starget, opts, rname, destdir)
	char *starget;
	opt_t opts;
	char *rname;
	int destdir;
{
	register struct subcmd *sc;
	extern struct subcmd *subcmds;
	char *rfile;

 	rfile = remfilename(source, Tdest, target, rname, destdir);

	for (sc = subcmds; sc != NULL; sc = sc->sc_next) {
		if (sc->sc_type != SPECIAL)
			continue;
		if (sc->sc_args != NULL && !inlist(sc->sc_args, starget))
			continue;
		message(MT_CHANGE, "special \"%s\"", sc->sc_name);
		if (IS_ON(opts, DO_VERIFY))
			continue;
		(void) sendcmd(C_SPECIAL,
			"%s=%s;%s=%s;%s=%s;export %s %s %s;%s",
			E_LOCFILE, starget,
			E_REMFILE, rfile,
			E_BASEFILE, xbasename(rfile),
			E_LOCFILE, E_REMFILE, E_BASEFILE,
			sc->sc_name);
		while (response() > 0)
			;
	}
}

/*
 * If we're doing a target with a "cmdspecial" in it, then
 * save the name of the file being updated for use with "cmdspecial".
 */
static void addcmdspecialfile(starget, rname, destdir)
	char *starget;
	char *rname;
	int destdir;
{
	char *rfile;
	struct namelist *new;
	register struct subcmd *sc;
	extern struct subcmd *subcmds;
	int isokay = 0;

 	rfile = remfilename(source, Tdest, target, rname, destdir);

	for (sc = subcmds; sc != NULL && !isokay; sc = sc->sc_next) {
		if (sc->sc_type != CMDSPECIAL)
			continue;
		if (sc->sc_args != NULL && !inlist(sc->sc_args, starget))
			continue;
		isokay = TRUE;
	}

	if (isokay) {
		new = (struct namelist *) xmalloc(sizeof(struct namelist));
		new->n_name = strdup(rfile);
		new->n_next = updfilelist;
		updfilelist = new;
	}
}

/*
 * Free the file list
 */
static void freecmdspecialfiles()
{
	register struct namelist *ptr, *save;

	for (ptr = updfilelist; ptr; ) {
		if (ptr->n_name) (void) free(ptr->n_name);
		save = ptr->n_next;
		(void) free(ptr);
		if (save)
			ptr = save->n_next;
		else
			ptr = NULL;
	}
	updfilelist = NULL;
}

/*
 * Run commands for an entire cmd
 */
extern void runcmdspecial(cmd, filev, opts)
	struct cmd *cmd;
	char **filev;
	opt_t opts;
{
	register struct subcmd *sc;
	register struct namelist *f;
	int first = TRUE;

	for (sc = cmd->c_cmds; sc != NULL; sc = sc->sc_next) {
		if (sc->sc_type != CMDSPECIAL)
			continue;
		message(MT_CHANGE, "cmdspecial \"%s\"", sc->sc_name);
		if (IS_ON(opts, DO_VERIFY))
			continue;
		/* Send all the file names */
		for (f = updfilelist; f != NULL; f = f->n_next) {
			if (first) {
				(void) sendcmd(C_CMDSPECIAL, NULL);
				if (response() < 0)
					return;
				first = FALSE;
			}
			(void) sendcmd(RC_FILE, f->n_name);
			if (response() < 0)
				return;
		}
		if (first) {
			(void) sendcmd(C_CMDSPECIAL, NULL);
			if (response() < 0)
				return;
			first = FALSE;
		}
		/* Send command to run and wait for it to complete */
		(void) sendcmd(RC_COMMAND, sc->sc_name);
		while (response() > 0)
			;
		first = TRUE;	/* Reset in case there are more CMDSPECIAL's */
	}
	freecmdspecialfiles();
}

/*
 * For security, reject filenames that contains a newline
 */
int checkfilename(name)
	char *name;
{
	register char *cp;

	if (strchr(name, '\n')) {
		for (cp = name; *cp; cp++)
			if (*cp == '\n')
				*cp = '?';
		message(MT_NERROR, 
			"Refuse to handle filename containing newline: %s",
			name);
		return(-1);
	}

	return(0);
}

/*
 * Save and retrieve hard link info
 */
static struct linkbuf *linkinfo(statp)
	struct stat *statp;
{
	struct linkbuf *lp;

	for (lp = ihead; lp != NULL; lp = lp->nextp)
		if (lp->inum == statp->st_ino && lp->devnum == statp->st_dev) {
			lp->count--;
			return(lp);
		}

	lp = (struct linkbuf *) xmalloc(sizeof(*lp));
	lp->nextp = ihead;
	ihead = lp;
	lp->inum = statp->st_ino;
	lp->devnum = statp->st_dev;
	lp->count = statp->st_nlink - 1;
	(void) strcpy(lp->pathname, target);
	(void) strcpy(lp->src, source);
	if (Tdest)
		(void) strcpy(lp->target, Tdest);
	else
		*lp->target = CNULL;

	return(NULL);
}

/*
 * Send a hardlink
 */
static int sendhardlink(opts, lp, rname, destdir)
	opt_t opts;
	struct linkbuf *lp;
	char *rname;
	int destdir;
{
	static char buff[MAXPATHLEN];
	char *lname;	/* name of file to link to */

	debugmsg(DM_MISC, 
	       "sendhardlink: rname='%s' pathname='%s' src='%s' target='%s'\n",
		 rname, lp->pathname, lp->src, lp->target);
		 
	if (*lp->target == CNULL)
		(void) sendcmd(C_RECVHARDLINK, "%o %s %s", 
			       opts, lp->pathname, rname);
	else {
		lname = buff;
		strcpy(lname, remfilename(lp->src, lp->target, 
					  lp->pathname, rname, 
					  destdir));
		debugmsg(DM_MISC, "sendhardlink: lname=%s\n", lname);
		(void) sendcmd(C_RECVHARDLINK, "%o %s %s", 
			       opts, lname, rname);
	}

	return(response());
}

/*
 * Send a file
 */
static int sendfile(rname, opts, stb, user, group, destdir)
	char *rname;
	opt_t opts;
	struct stat *stb;
	char *user, *group;
	int destdir;
{
	int goterr, f;
	off_t i;

	if (stb->st_nlink > 1) {
		struct linkbuf *lp;
		
		if ((lp = linkinfo(stb)) != NULL)
			return(sendhardlink(opts, lp, rname, destdir));
	}

	if ((f = open(target, O_RDONLY)) < 0) {
		error("%s: open for read failed: %s", target, SYSERR);
		return(-1);
	}

	/*
	 * Send file info
	 */
	(void) sendcmd(C_RECVREG, "%o %04o %ld %ld %ld %s %s %s", 
		       opts, stb->st_mode & 07777, 
		       (long) stb->st_size, 
		       stb->st_mtime, stb->st_atime,
		       user, group, rname);
	if (response() < 0) {
		(void) close(f);
		return(-1);
	}

	debugmsg(DM_MISC, "Send file '%s' %d bytes\n", 
		 rname, (long) stb->st_size);

	/*
	 * Set remote time out alarm handler.
	 */
	(void) signal(SIGALRM, sighandler);

	/*
	 * Actually transfer the file
	 */
	goterr = 0;
	for (i = 0; i < stb->st_size; i += BUFSIZ) {
		int amt = BUFSIZ;

		(void) alarm(rtimeout);
		if (i + amt > stb->st_size)
			amt = stb->st_size - i;
		if (read(f, buf, amt) != amt) {
			error("%s: File changed size", target);
			err();
			++goterr;
			/*
			 * XXX - We have to keep going because the
			 * server expects to receive a fixed number
			 * of bytes that we specified as the file size.
			 * We need Out Of Band communication to handle
			 * this situation gracefully.
			 */
		}
		if (xwrite(rem_w, buf, amt) < 0) {
		    	error("%s: Error writing to client: %s", 
			      target, SYSERR);
			err();
			++goterr;
			break;
		}
		(void) alarm(0);
	}

	(void) alarm(0);	/* Insure alarm is off */
	(void) close(f);

	debugmsg(DM_MISC, "Send file '%s' %s.\n", 
		 (goterr) ? "failed" : "complete", rname);

	/*
	 * Check for errors and end send
	 */
	if (goterr)
		return(-1);
	else {
		ack();
		f = response();
		if (f < 0)
			return(-1);
		else if (f == 0 && IS_ON(opts, DO_COMPARE))
			return(0);

		runspecial(target, opts, rname, destdir);
		addcmdspecialfile(target, rname, destdir);

		return(0);
	}
}

/*
 * Check for files on the machine being updated that are not on the master
 * machine and remove them.
 *
 * Return < 0 on error.
 * Return 0 if nothing happened.
 * Return > 0 if anything is updated.
 */
static int rmchk(opts)
	opt_t opts;
{
	register u_char *s;
	struct stat stb;
	int didupdate = 0;
	int n;

	debugmsg(DM_CALL, "rmchk()\n");

	/*
	 * Tell the remote to clean the files from the last directory sent.
	 */
	(void) sendcmd(C_CLEAN, "%o", IS_ON(opts, DO_VERIFY));
	if (response() < 0)
		return(-1);

	for ( ; ; ) {
		n = remline(s = respbuff, sizeof(respbuff), TRUE);
		if (n <= 0) {
			error("rmchk: unexpected control record");
			return(didupdate);
		}

		switch (*s++) {
		case CC_QUERY: /* Query if file should be removed */
			/*
			 * Return the following codes to remove query.
			 * CC_NO -- file exists - DON'T remove.
			 * CC_YES -- file doesn't exist - REMOVE.
			 */
			(void) sprintf(ptarget, "%s%s", 
				       (ptarget[-1] == '/' ? "" : "/"), s);
			debugmsg(DM_MISC, "check %s\n", target);
			if (except(target))
				(void) sendcmd(CC_NO, NULL);
			else if (lstat(target, &stb) < 0) {
				if (sendcmd(CC_YES, NULL) == 0)
					didupdate = 1;
			} else
				(void) sendcmd(CC_NO, NULL);
			break;

		case CC_END:
			*ptarget = CNULL;
			ack();
			return(didupdate);

		case C_LOGMSG:
			if (n > 0)
				message(MT_INFO, "%s", s);
			break;

		case C_NOTEMSG:
			if (n > 0)
				message(MT_NOTICE, "%s", s);
			break;
			/* Goto top of loop */

		case C_ERRMSG:
			message(MT_NERROR, "%s", s);
			return(didupdate);

		case C_FERRMSG:
			message(MT_FERROR, "%s", s);
			finish();

		default:
			error("rmchk: unexpected response '%s'", respbuff);
			err();
		}
	}
	/*NOTREACHED*/
}

/*
 * Send a directory
 *
 * Return < 0 on error.
 * Return 0 if nothing happened.
 * Return > 0 if anything is updated.
 */
static int senddir(rname, opts, stb, user, group, destdir)
	char *rname;
	opt_t opts;
	struct stat *stb;
	char *user, *group;
	int destdir;
{
	DIRENTRY *dp;
	DIR *d;
	char *optarget, *cp;
	int len;
	int didupdate = 0;

	/*
	 * Don't descend into directory
	 */
	if (IS_ON(opts, DO_NODESCEND))
		return(0);

	if ((d = opendir(target)) == NULL) {
		error("%s: opendir failed: %s", target, SYSERR);
		return(-1);
	}

	/*
	 * Send recvdir command in recvit() format.
	 */
	(void) sendcmd(C_RECVDIR, "%o %04o 0 0 0 %s %s %s", 
		       opts, stb->st_mode & 07777, user, group, rname);
	if (response() < 0)
		return(-1);

	if (IS_ON(opts, DO_REMOVE))
		if (rmchk(opts) > 0)
			++didupdate;
	
	optarget = ptarget;
	len = ptarget - target;
	while ((dp = readdir(d))) {
		if (!strcmp(dp->d_name, ".") ||
		    !strcmp(dp->d_name, ".."))
			continue;
		if (len + 1 + (int) strlen(dp->d_name) >= MAXPATHLEN - 1) {
			error("%s/%s: Name too long", target,
			      dp->d_name);
			continue;
		}
		ptarget = optarget;
		if (ptarget[-1] != '/')
			*ptarget++ = '/';
		cp = dp->d_name;
		while ((*ptarget++ = *cp++))
			;
		ptarget--;
		if (sendit(dp->d_name, opts, destdir) > 0)
			didupdate = 1;
	}
	(void) closedir(d);

	(void) sendcmd(C_END, NULL);
	(void) response();

	ptarget = optarget;
	*ptarget = CNULL;

	return(didupdate);
}

/*
 * Send a link
 */
static int sendlink(rname, opts, stb, user, group, destdir)
	char *rname;
	opt_t opts;
	struct stat *stb;
	char *user;
	char *group;
	int destdir;
{
	int f, n;
	static char tbuf[BUFSIZ];
	char lbuf[MAXPATHLEN];
	u_char *s;

	debugmsg(DM_CALL, "sendlink(%s, %x, stb, %d)\n", rname, opts, destdir);

	if (stb->st_nlink > 1) {
		struct linkbuf *lp;
		
		if ((lp = linkinfo(stb)) != NULL)
			return(sendhardlink(opts, lp, rname, destdir));
	}

	/*
	 * Gather and send basic link info
	 */
	(void) sendcmd(C_RECVSYMLINK, "%o %04o %ld %ld %ld %s %s %s", 
		       opts, stb->st_mode & 07777, 
		       (long) stb->st_size, 
		       stb->st_mtime, stb->st_atime,
		       user, group, rname);
	if (response() < 0)
		return(-1);

	/*
	 * Gather and send additional link info
	 */
	if ((n = readlink(target, lbuf, sizeof(lbuf)-1)) != -1)
		lbuf[n] = '\0';
	else {
		error("%s: readlink failed", target);
		err();
	}
	(void) sprintf(tbuf, "%.*s", (int) stb->st_size, lbuf);
	(void) sendcmd(C_NONE, "%s\n", tbuf);

	if (n != stb->st_size) {
		error("%s: file changed size", target);
		err();
	} else
		ack();

	/*
	 * Check response
	 */
	f = response();
	if (f < 0)
		return(-1);
	else if (f == 0 && IS_ON(opts, DO_COMPARE))
		return(0);

	/*
	 * Read and process responses from server.
	 * The server may send multiple messages regarding
	 * file deletes if the remote target is a directory.
	 */
	for (;;) {
		n = remline(s = respbuff, sizeof(respbuff), TRUE);
		if (n == -1)	/* normal EOF */
			return(0);
		if (n == 0) {
			error("expected control record");
			continue;
		}
		
		switch (*s++) {
		case C_END:	/* End of send operation */
			*ptarget = CNULL;
			ack();
			runspecial(target, opts, rname, destdir);
			addcmdspecialfile(target, rname, destdir);
			return(0);
			
		case C_LOGMSG:
			if (n > 0)
				message(MT_INFO, "%s", s);
			break;

		case C_NOTEMSG:
			if (n > 0)
				message(MT_NOTICE, "%s", s);
			break;
			/* Goto top of loop */

		case C_ERRMSG:
			message(MT_NERROR, "%s", s);
			return(-1);

		case C_FERRMSG:
			message(MT_FERROR, "%s", s);
			finish();

		default:
			error("install link: unexpected response '%s'", 
			      respbuff);
			err();
		}
	}
	/*NOTREACHED*/
}

/*
 * Check to see if file needs to be updated on the remote machine.
 * Returns:
 * 	US_NOTHING	- no update
 *	US_NOENT	- remote doesn't exist
 *	US_OUTDATE	- out of date
 *	US_DOCOMP	- comparing binaries to determine if out of date
 *	US_MODE		- File modes do not match
 */
static int update(rname, opts, statp)
	char *rname;
	opt_t opts;
	struct stat *statp;
{
	register off_t size;
	register time_t mtime;
	unsigned short lmode;
	unsigned short rmode;
	char *owner = NULL, *group = NULL;
	int done, n;
	u_char *cp;

	debugmsg(DM_CALL, "update(%s, 0x%x, 0x%x)\n", rname, opts, statp);

	if (IS_ON(opts, DO_NOEXEC))
		if (isexec(target, statp)) {
			debugmsg(DM_MISC, "%s is an executable\n", target);
			return(US_NOTHING);
		}

	/*
	 * Check to see if the file exists on the remote machine.
	 */
	(void) sendcmd(C_QUERY, "%s", rname);

	for (done = 0; !done;) {
		n = remline(cp = respbuff, sizeof(respbuff), TRUE);
		if (n <= 0) {
			error("update: unexpected control record in response to query");
			return(US_NOTHING);
		}

		switch (*cp++) {
		case QC_ONNFS:  /* Resides on a NFS */
			debugmsg(DM_MISC,
				 "update: %s is on a NFS.  Skipping...\n", 
				 rname);
			return(US_NOTHING);

		case QC_SYM:  /* Is a symbolic link */
			debugmsg(DM_MISC,
				 "update: %s is a symlink.  Skipping...\n", 
				 rname);
			return(US_NOTHING);

		case QC_ONRO:  /* Resides on a Read-Only fs */
			debugmsg(DM_MISC,
				 "update: %s is on a RO fs.  Skipping...\n", 
				 rname);
			return(US_NOTHING);
			
		case QC_YES:
			done = 1;
			break;

		case QC_NO:  /* file doesn't exist so install it */
			return(US_NOENT);

		case C_ERRMSG:
			if (cp)
				message(MT_NERROR, "%s", cp);
			return(US_NOTHING);

		case C_FERRMSG:
			if (cp)
				message(MT_FERROR, "%s", cp);
			finish();

		case C_NOTEMSG:
			if (cp)
				message(MT_NOTICE, "%s", cp);
			break;
			/* Goto top of loop */

		default:
			error("update: unexpected response to query '%s'", cp);
			return(US_NOTHING);
		}
	}

	/*
	 * Target exists, but no other info passed
	 */
	if (n <= 1 || !S_ISREG(statp->st_mode))
		return(US_OUTDATE);

	if (IS_ON(opts, DO_COMPARE))
		return(US_DOCOMP);

	/*
	 * Parse size
	 */
	size = strtol(cp, (char **)&cp, 10);
	if (*cp++ != ' ') {
		error("update: size not delimited");
		return(US_NOTHING);
	}

	/*
	 * Parse mtime
	 */
	mtime = strtol(cp, (char **)&cp, 10);
	if (*cp++ != ' ') {
		error("update: mtime not delimited");
		return(US_NOTHING);
	}

	/*
	 * Parse remote file mode
	 */
	rmode = strtol(cp, (char **)&cp, 8);
	if (cp && *cp)
		++cp;

	/*
	 * Be backwards compatible
	 */
	if (cp && *cp != CNULL) {
		/*
		 * Parse remote file owner
		 */
		owner = strtok((char *)cp, " ");
		if (owner == NULL) {
			error("update: owner not delimited");
			return(US_NOTHING);
		}

		/*
		 * Parse remote file group
		 */
		group = strtok(NULL, " ");
		if (group == NULL) {
			error("update: group not delimited");
			return(US_NOTHING);
		}
	}

	/*
	 * File needs to be updated?
	 */
	lmode = statp->st_mode & 07777;

	debugmsg(DM_MISC, "update(%s,) local mode %04o remote mode %04o\n", 
		 rname, lmode, rmode);
	debugmsg(DM_MISC, "update(%s,) size %d mtime %d owner '%s' grp '%s'\n",
		 rname, (int) size, mtime, owner, group);

	if (statp->st_mtime != mtime) {
		if (statp->st_mtime < mtime && IS_ON(opts, DO_YOUNGER)) {
			message(MT_WARNING, 
				"%s: Warning: remote copy is newer",
				target);
			return(US_NOTHING);
		}
		return(US_OUTDATE);
	}

	/*
	 * If the mode of a file does not match the local mode, the
	 * whole file is updated.  This is done both to insure that
	 * a bogus version of the file has not been installed and to
	 * avoid having to handle weird cases of chmod'ing symlinks 
	 * and such.
	 */
	if (!IS_ON(opts, DO_NOCHKMODE) && lmode != rmode) {
		debugmsg(DM_MISC, "modes do not match (%04o != %04o).\n",
			 lmode, rmode);
		return(US_OUTDATE);
	}

	if (statp->st_size != size) {
		debugmsg(DM_MISC, "size does not match (%d != %d).\n",
			 (int) statp->st_size, size);
		return(US_OUTDATE);
	} 

	/*
	 * Check ownership
	 */
	if (!IS_ON(opts, DO_NOCHKOWNER) && owner) {
		if (!IS_ON(opts, DO_NUMCHKOWNER)) {
			/* Check by string compare */
			if (strcmp(owner, getusername(statp->st_uid, 
						      target, opts)) != 0) {
				debugmsg(DM_MISC, 
					 "owner does not match (%s != %s).\n",
					 getusername(statp->st_uid, 
						     target, opts), owner);
				return(US_OUTDATE);
			}
		} else {
			/* 
			 * Check numerically.
			 * Allow negative numbers.
			 */
			while (*owner && !isdigit(*owner) && (*owner != '-'))
				++owner;
			if (owner && atoi(owner) != statp->st_uid) {
				debugmsg(DM_MISC, 
					 "owner does not match (%d != %s).\n",
					 statp->st_uid, owner);
				return(US_OUTDATE);
			}
		}
	} 

	if (!IS_ON(opts, DO_NOCHKGROUP) && group) {
		if (!IS_ON(opts, DO_NUMCHKGROUP)) {
			/* Check by string compare */
			if (strcmp(group, getgroupname(statp->st_gid, 
						       target, opts)) != 0) {
				debugmsg(DM_MISC, 
					 "group does not match (%s != %s).\n",
					 getgroupname(statp->st_gid, 
						      target, opts), group);
				return(US_OUTDATE);
			}
		} else {	
			/* Check numerically */
			/* Allow negative gid */
			while (*group && !isdigit(*group) && (*group != '-'))
				++group;
			if (group && atoi(group) != statp->st_gid) {
				debugmsg(DM_MISC,
					 "group does not match (%d != %s).\n",
					 statp->st_gid, group);
				return(US_OUTDATE);
			}
		}
	}

	return(US_NOTHING);
}

/*
 * Stat a file
 */
static int dostat(file, statbuf, opts)
	char *file;
	struct stat *statbuf;
	opt_t opts;
{
	int s;

	if (IS_ON(opts, DO_FOLLOW))
		s = stat(file, statbuf);
	else
		s = lstat(file, statbuf);

	if (s < 0)
		error("%s: %s failed: %s", file,
		      IS_ON(opts, DO_FOLLOW) ? "stat" : "lstat", SYSERR);
	return(s);
}

/*
 * Transfer the file or directory in target[].
 * rname is the name of the file on the remote host.
 *
 * Return < 0 on error.
 * Return 0 if nothing happened.
 * Return > 0 if anything is updated.
 */
static int sendit(rname, opts, destdir)
	char *rname;
	opt_t opts;
	int destdir;
{
	static struct stat stb;
	extern struct subcmd *subcmds;
	char *user, *group;
	int u, len;
	int didupdate = 0;

	/*
	 * Remove possible accidental newline
	 */
	len = strlen(rname);
	if (len > 0 && rname[len-1] == '\n')
		rname[len-1] = CNULL;

	if (checkfilename(rname) != 0)
		return(-1);

	debugmsg(DM_CALL, "sendit(%s, 0x%x) called\n", rname, opts);

	if (except(target))
		return(0);

	if (dostat(target, &stb, opts) < 0)
		return(-1);

	/*
	 * Does rname need updating?
	 */
	u = update(rname, opts, &stb);
	debugmsg(DM_MISC, "sendit(%s, 0x%x): update status of %s is %d\n", 
		 rname, opts, target, u);

	/*
	 * Don't need to update the file, but we may need to save hardlink
	 * info.
	 */
	if (u == US_NOTHING) {
		if (S_ISREG(stb.st_mode) && stb.st_nlink > 1)
			(void) linkinfo(&stb);
		return(0);
	}

	/*
	 * File mode needs changing
	 */
	if (u == US_MODE) {
		if (IS_ON(opts, DO_VERIFY)) {
			message(MT_INFO, "%s: need to chmod to %04o",
				target, stb.st_mode & 07777);
			runspecial(target, opts, rname, destdir);
			return(1);
		}
		message(MT_CHANGE, "%s: chmod to %04o", 
			target, stb.st_mode & 07777);
		(void) sendcmd(C_CHMOD, "%o %04o %s",
			       opts, stb.st_mode & 07777, rname);
		(void) response();
		return(1);
	}

	user = getusername(stb.st_uid, target, opts);
	group = getgroupname(stb.st_gid, target, opts);

	/*
	 * No entry - need to install
	 */
	if (u == US_NOENT) {
		if (IS_ON(opts, DO_VERIFY)) {
			message(MT_INFO, "%s: need to install", target);
			runspecial(target, opts, rname, destdir);
			return(1);
		}
		if (!IS_ON(opts, DO_QUIET))
			message(MT_CHANGE, "%s: installing", target);
		FLAG_OFF(opts, (DO_COMPARE|DO_REMOVE));
	}

	/*
	 * Handle special file types, including directories and symlinks
	 */
	if (S_ISDIR(stb.st_mode)) {
		if (senddir(rname, opts, &stb, user, group, destdir) > 0)
			didupdate = 1;
	} else if (S_ISLNK(stb.st_mode)) {
		if (u != US_NOENT)
			FLAG_ON(opts, DO_COMPARE);
		/*
		 * Since we always send link info to the server
		 * so the server can determine if the remote link
		 * is correct, we never get any acknowledge meant
		 * from the server whether the link was really
		 * updated or not.
		 */
		(void) sendlink(rname, opts, &stb, user, group, destdir);
	} else if (S_ISREG(stb.st_mode)) {		
		if (u == US_OUTDATE) {
			if (IS_ON(opts, DO_VERIFY)) {
				message(MT_INFO, "%s: need to update", target);
				runspecial(target, opts, rname, destdir);
				return(1);
			}
			if (!IS_ON(opts, DO_QUIET))
				message(MT_CHANGE, "%s: updating", target);
		}
		if (sendfile(rname, opts, &stb, user, group, destdir) == 0)
			didupdate = 1;
	} else
		error("%s: unknown file type", target);

	return(didupdate);
}
	
/*
 * Remove temporary files and do any cleanup operations before exiting.
 */
extern void cleanup()
{
	char *file;
#ifdef USE_STATDB
	extern char statfile[];

	(void) unlink(statfile);
#endif

	if ((file = getnotifyfile()))
		(void) unlink(file);
}

/*
 * Update the file(s) if they are different.
 * destdir = 1 if destination should be a directory
 * (i.e., more than one source is being copied to the same destination).
 *
 * Return < 0 on error.
 * Return 0 if nothing updated.
 * Return > 0 if something was updated.
 */
extern int install(src, dest, ddir, destdir, opts)
	char *src, *dest;
 	int ddir, destdir;
	opt_t opts;
{
	static char destcopy[MAXPATHLEN];
	char *rname;
	int didupdate = 0;

	debugmsg(DM_CALL,
		"install(src=%s,dest=%s,ddir=%d,destdir=%d,opts=%d) start\n",
		(src?src:"NULL"), (dest?dest:"NULL"), ddir, destdir, opts);
	/*
	 * Save source name
	 */
	if (IS_ON(opts, DO_WHOLE))
		source[0] = CNULL;
	else
		(void) strcpy(source, src);

	if (dest == NULL) {
		FLAG_OFF(opts, DO_WHOLE); /* WHOLE only useful if renaming */
		dest = src;
	}

	if (checkfilename(dest) != 0)
		return(-1);

	if (nflag || debug) {
		static char buff[BUFSIZ];
		char *cp;

		cp = getondistoptlist(opts);
		(void) sprintf(buff, "%s%s%s %s %s", 
			       IS_ON(opts, DO_VERIFY) ? "verify" : "install",
			       (cp) ? " -o" : "", (cp) ? cp : "", 
			       src, dest);
		if (nflag) {
			printf("%s\n", buff);
			return(0);
		} else
			debugmsg(DM_MISC, "%s\n", buff);
	}

	rname = exptilde(target, src);
	if (rname == NULL)
		return(-1);
	ptarget = target;
	while (*ptarget)
		ptarget++;
	/*
	 * If we are renaming a directory and we want to preserve
	 * the directory heirarchy (-w), we must strip off the leading
	 * directory name and preserve the rest.
	 */
	if (IS_ON(opts, DO_WHOLE)) {
		while (*rname == '/')
			rname++;
		ddir = 1;
		destdir = 1;
	} else {
		rname = strrchr(target, '/');
		/* Check if no '/' or target ends in '/' */
		if (rname == NULL || 
		    rname+1 == NULL || 
		    *(rname+1) == CNULL)
			rname = target;
		else
			rname++;
	}

	debugmsg(DM_MISC, 
 	"install: target=%s src=%s rname=%s dest='%s' destdir=%d, ddir=%d\n", 
 		 target, source, rname, dest, destdir, ddir);

	/*
	 * Pass the destination file/directory name to remote.
	 */
 	if (ddir)
		(void) sendcmd(C_DIRTARGET, "%o %s", opts, dest);
	else
		(void) sendcmd(C_TARGET, "%o %s", opts, dest);
	if (response() < 0)
		return(-1);

	/*
	 * Save the name of the remote target destination if we are
	 * in WHOLE mode (destdir > 0) or if the source and destination
	 * are not the same.  This info will be used later for maintaining
	 * hardlink info.
	 */
	if (destdir || (src && dest && strcmp(src, dest))) {
		(void) strcpy(destcopy, dest);
		Tdest = destcopy;
	}

	didupdate = sendit(rname, opts, destdir);
	Tdest = 0;

	return(didupdate);
}
