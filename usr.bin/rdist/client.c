/*	$OpenBSD: client.c,v 1.32 2015/01/16 06:40:11 deraadt Exp $	*/

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

#include <dirent.h>

#include "defs.h"
#include "y.tab.h"

/*
 * Routines used in client mode to communicate with remove server.
 */


/*
 * Update status
 */
#define US_NOTHING 	0	/* No update needed */
#define US_NOENT	1	/* Entry does not exist */
#define US_OUTDATE	2	/* Entry is out of date */
#define US_DOCOMP	3	/* Do a binary comparison */
#define US_CHMOG	4	/* Modes or ownership of file differ */

struct	linkbuf *ihead = NULL;	/* list of files with more than one link */
char	buf[BUFSIZ];		/* general purpose buffer */
u_char	respbuff[BUFSIZ];	/* Response buffer */
char	target[BUFSIZ];		/* target/source directory name */
char	source[BUFSIZ];		/* source directory name */
char	*ptarget;		/* pointer to end of target name */
char	*Tdest;			/* pointer to last T dest*/
struct namelist	*updfilelist = NULL; /* List of updated files */

static void runspecial(char *, opt_t, char *, int);
static void addcmdspecialfile(char *, char *, int);
static void freecmdspecialfiles(void);
static struct linkbuf *linkinfo(struct stat *);
static int sendhardlink(opt_t, struct linkbuf *, char *, int);
static int sendfile(char *, opt_t, struct stat *, char *, char *, int);
static int rmchk(opt_t);
static int senddir(char *, opt_t, struct stat *, char *, char *, int);
static int sendlink(char *, opt_t, struct stat *, char *, char *, int);
static int update(char *, opt_t, struct stat *);
static int dostat(char *, struct stat *, opt_t);
static int statupdate(int, char *, opt_t, char *, int, struct stat *, char *, char *);
static int fullupdate(int, char *, opt_t, char *, int, struct stat *, char *, char *);
static int sendit(char *, opt_t, int);

/*
 * return remote file pathname (relative from target)
 */
char *
remfilename(char *src, char *dest, char *path, char *rname, int destdir)
{
	extern struct namelist *filelist;
	char *lname, *cp;
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
			(void) snprintf(buff, sizeof(buff), "%s/%s", dest, path);
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
				(void) snprintf(buff, sizeof(buff), "%s/%s",
						dest, cp);
			else
				(void) snprintf(buff, sizeof(buff), "%s%s",
						dest, cp);
		}
	} else
		(void) strlcpy(lname, dest, buf + sizeof buff - lname);

	debugmsg(DM_MISC, "remfilename: remote filename=%s\n", lname);

	return(lname);
}

/*
 * Return true if name is in the list.
 */
int
inlist(struct namelist *list, char *file)
{
	struct namelist *nl;

	for (nl = list; nl != NULL; nl = nl->n_next)
		if (strcmp(file, nl->n_name) == 0)
			return(1);
	return(0);
}

/*
 * Run any special commands for this file
 */
static void
runspecial(char *starget, opt_t opts, char *rname, int destdir)
{
	struct subcmd *sc;
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
static void
addcmdspecialfile(char *starget, char *rname, int destdir)
{
	char *rfile;
	struct namelist *new;
	struct subcmd *sc;
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
		new = xmalloc(sizeof *new);
		new->n_name = xstrdup(rfile);
		new->n_regex = NULL;
		new->n_next = updfilelist;
		updfilelist = new;
	}
}

/*
 * Free the file list
 */
static void
freecmdspecialfiles(void)
{
	struct namelist *ptr, *save;

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
void
runcmdspecial(struct cmd *cmd, opt_t opts)
{
	struct subcmd *sc;
	struct namelist *f;
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
			(void) sendcmd(RC_FILE, "%s", f->n_name);
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
		(void) sendcmd(RC_COMMAND, "%s", sc->sc_name);
		while (response() > 0)
			;
		first = TRUE;	/* Reset in case there are more CMDSPECIAL's */
	}
	freecmdspecialfiles();
}

/*
 * For security, reject filenames that contains a newline
 */
int
checkfilename(char *name)
{
	char *cp;

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

void
freelinkinfo(struct linkbuf *lp)
{
	if (lp->pathname)
		free(lp->pathname);
	if (lp->src)
		free(lp->src);
	if (lp->target)
		free(lp->target);
	free(lp);
}

/*
 * Save and retrieve hard link info
 */
static struct linkbuf *
linkinfo(struct stat *statp)
{
	struct linkbuf *lp;

	/* XXX - linear search doesn't scale with many links */
	for (lp = ihead; lp != NULL; lp = lp->nextp)
		if (lp->inum == statp->st_ino && lp->devnum == statp->st_dev) {
			lp->count--;
			return(lp);
		}

	lp = xmalloc(sizeof(*lp));
	lp->nextp = ihead;
	ihead = lp;
	lp->inum = statp->st_ino;
	lp->devnum = statp->st_dev;
	lp->count = statp->st_nlink - 1;
	lp->pathname = xstrdup(target);
	lp->src = xstrdup(source);
	if (Tdest)
		lp->target = xstrdup(Tdest);
	else
		lp->target = NULL;

	return(NULL);
}

/*
 * Send a hardlink
 */
static int
sendhardlink(opt_t opts, struct linkbuf *lp, char *rname, int destdir)
{
	static char buff[PATH_MAX];
	char *lname;	/* name of file to link to */
	char ername[PATH_MAX*4], elname[PATH_MAX*4];

	debugmsg(DM_MISC, 
	       "sendhardlink: rname='%s' pathname='%s' src='%s' target='%s'\n",
		rname, lp->pathname ? lp->pathname : "",
		lp->src ? lp->src : "", lp->target ? lp->target : "");
		 
	if (lp->target == NULL)
		lname = lp->pathname;
	else {
		lname = buff;
		strlcpy(lname, remfilename(lp->src, lp->target, 
					  lp->pathname, rname, 
					  destdir), sizeof(buff));
		debugmsg(DM_MISC, "sendhardlink: lname=%s\n", lname);
	}
	ENCODE(elname, lname);
	ENCODE(ername, rname);
	(void) sendcmd(C_RECVHARDLINK, "%lo %s %s", 
		       opts, elname, ername);

	return(response());
}

/*
 * Send a file
 */
static int
sendfile(char *rname, opt_t opts, struct stat *stb, char *user,
	 char *group, int destdir)
{
	int goterr, f;
	off_t i;
	char ername[PATH_MAX*4];

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
	ENCODE(ername, rname);

	(void) sendcmd(C_RECVREG, "%lo %04o %lld %lld %lld %s %s %s", 
		       opts, stb->st_mode & 07777, (long long) stb->st_size, 
		       (long long)stb->st_mtime, (long long)stb->st_atime,
		       user, group, ername);
	if (response() < 0) {
		(void) close(f);
		return(-1);
	}


	debugmsg(DM_MISC, "Send file '%s' %lld bytes\n", rname,
		 (long long) stb->st_size);

	/*
	 * Set remote time out alarm handler.
	 */
	(void) signal(SIGALRM, sighandler);

	/*
	 * Actually transfer the file
	 */
	goterr = 0;
	for (i = 0; i < stb->st_size; i += BUFSIZ) {
		off_t amt = BUFSIZ;

		(void) alarm(rtimeout);
		if (i + amt > stb->st_size)
			amt = stb->st_size - i;
		if (read(f, buf, (size_t) amt) != (ssize_t) amt) {
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
		if (xwrite(rem_w, buf, (size_t) amt) < 0) {
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
static int
rmchk(opt_t opts)
{
	u_char *s;
	struct stat stb;
	int didupdate = 0;
	int n;
	char targ[PATH_MAX*4];

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
			if (DECODE(targ, (char *) s) == -1) {
				error("rmchk: cannot decode file");
				return(-1);
			}
			(void) snprintf(ptarget,
					sizeof(target) - (ptarget - target),
					"%s%s", 
				        (ptarget[-1] == '/' ? "" : "/"),
				        targ);
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
static int
senddir(char *rname, opt_t opts, struct stat *stb, char *user,
	char *group, int destdir)
{
	struct dirent *dp;
	DIR *d;
	char *optarget, *cp;
	int len;
	int didupdate = 0;
	char ername[PATH_MAX*4];

	/*
	 * Send recvdir command in recvit() format.
	 */
	ENCODE(ername, rname);
	(void) sendcmd(C_RECVDIR, "%lo %04o 0 0 0 %s %s %s", 
		       opts, stb->st_mode & 07777, user, group, ername);
	if (response() < 0)
		return(-1);

	optarget = ptarget;

	/*
	 * Don't descend into directory
	 */
	if (IS_ON(opts, DO_NODESCEND)) {
		didupdate = 0;
		goto out;
	}

	if (IS_ON(opts, DO_REMOVE))
		if (rmchk(opts) > 0)
			++didupdate;
	
	if ((d = opendir(target)) == NULL) {
		error("%s: opendir failed: %s", target, SYSERR);
		didupdate = -1;
		goto out;
	}

	len = ptarget - target;
	while ((dp = readdir(d)) != NULL) {
		if (!strcmp(dp->d_name, ".") ||
		    !strcmp(dp->d_name, ".."))
			continue;
		if (len + 1 + (int) strlen(dp->d_name) >= PATH_MAX - 1) {
			error("%s/%s: Name too long", target,
			      dp->d_name);
			continue;
		}
		ptarget = optarget;
		if (ptarget[-1] != '/')
			*ptarget++ = '/';
		cp = dp->d_name;
		while ((*ptarget++ = *cp++) != '\0')
			continue;
		ptarget--;
		if (sendit(dp->d_name, opts, destdir) > 0)
			didupdate = 1;
	}
	(void) closedir(d);

out:
	(void) sendcmd(C_END, NULL);
	(void) response();

	ptarget = optarget;
	*ptarget = CNULL;

	return(didupdate);
}

/*
 * Send a link
 */
static int
sendlink(char *rname, opt_t opts, struct stat *stb, char *user,
	 char *group, int destdir)
{
	int f, n;
	static char tbuf[BUFSIZ];
	char lbuf[PATH_MAX];
	u_char *s;
	char ername[PATH_MAX*4];

	debugmsg(DM_CALL, "sendlink(%s, %lx, stb, %d)\n", rname, opts, destdir);

	if (stb->st_nlink > 1) {
		struct linkbuf *lp;
		
		if ((lp = linkinfo(stb)) != NULL)
			return(sendhardlink(opts, lp, rname, destdir));
	}

	/*
	 * Gather and send basic link info
	 */
	ENCODE(ername, rname);
	(void) sendcmd(C_RECVSYMLINK, "%lo %04o %lld %lld %lld %s %s %s", 
		       opts, stb->st_mode & 07777, (long long) stb->st_size, 
		       (long long)stb->st_mtime, (long long)stb->st_atime,
		       user, group, ername);
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
	(void) snprintf(tbuf, sizeof(tbuf), "%.*s", (int) stb->st_size, lbuf);
	ENCODE(ername, tbuf);
	(void) sendcmd(C_NONE, "%s\n", ername);

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
 *	US_CHMOG	- File modes or ownership do not match
 */
static int
update(char *rname, opt_t opts, struct stat *statp)
{
	off_t size;
	time_t mtime;
	unsigned short lmode;
	unsigned short rmode;
	char *owner = NULL, *group = NULL;
	int done, n;
	u_char *cp;
	char ername[PATH_MAX*4];

	debugmsg(DM_CALL, "update(%s, 0x%lx, %p)\n", rname, opts, statp);

	switch (statp->st_mode & S_IFMT) {
	case S_IFBLK:
		debugmsg(DM_MISC, "%s is a block special; skipping\n", target);
		return(US_NOTHING);
	case S_IFCHR:
		debugmsg(DM_MISC, "%s is a character special; skipping\n",
		    target);
		return(US_NOTHING);
	case S_IFIFO:
		debugmsg(DM_MISC, "%s is a fifo; skipping\n", target);
		return(US_NOTHING);
	case S_IFSOCK:
		debugmsg(DM_MISC, "%s is a socket; skipping\n", target);
		return(US_NOTHING);
	}

	if (IS_ON(opts, DO_NOEXEC))
		if (isexec(target, statp)) {
			debugmsg(DM_MISC, "%s is an executable\n", target);
			return(US_NOTHING);
		}

	/*
	 * Check to see if the file exists on the remote machine.
	 */
	ENCODE(ername, rname);
	(void) sendcmd(C_QUERY, "%s", ername);

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
			error("update: unexpected response to query '%s'", respbuff);
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
	size = (off_t) strtoll(cp, (char **)&cp, 10);
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
	debugmsg(DM_MISC, "update(%s,) size %lld mtime %lld owner '%s' grp '%s'"
		 "\n", rname, (long long) size, (long long)mtime, owner, group);

	if (statp->st_mtime != mtime) {
		if (statp->st_mtime < mtime && IS_ON(opts, DO_YOUNGER)) {
			message(MT_WARNING, 
				"%s: Warning: remote copy is newer",
				target);
			return(US_NOTHING);
		}
		return(US_OUTDATE);
	}

	if (statp->st_size != size) {
		debugmsg(DM_MISC, "size does not match (%lld != %lld).\n",
			 (long long) statp->st_size, (long long) size);
		return(US_OUTDATE);
	} 

	if (!IS_ON(opts, DO_NOCHKMODE) && lmode != rmode) {
		debugmsg(DM_MISC, "modes do not match (%04o != %04o).\n",
			 lmode, rmode);
		return(US_CHMOG);
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
				return(US_CHMOG);
			}
		} else {
			/* 
			 * Check numerically.
			 * Allow negative numbers.
			 */
			while (*owner && !isdigit((unsigned char)*owner) &&
			    (*owner != '-'))
				++owner;
			if (owner && (uid_t)atoi(owner) != statp->st_uid) {
				debugmsg(DM_MISC, 
					 "owner does not match (%d != %s).\n",
					 statp->st_uid, owner);
				return(US_CHMOG);
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
				return(US_CHMOG);
			}
		} else {	
			/* Check numerically */
			/* Allow negative gid */
			while (*group && !isdigit((unsigned char) *group) &&
			    (*group != '-'))
				++group;
			if (group && (gid_t)atoi(group) != statp->st_gid) {
				debugmsg(DM_MISC,
					 "group does not match (%d != %s).\n",
					 statp->st_gid, group);
				return(US_CHMOG);
			}
		}
	}

	return(US_NOTHING);
}

/*
 * Stat a file
 */
static int
dostat(char *file, struct stat *statbuf, opt_t opts)
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
 * We need to just change file info.
 */
static int
statupdate(int u, char *starget, opt_t opts, char *rname, int destdir,
	   struct stat *st, char *user, char *group)
{
	int rv = 0;
	char ername[PATH_MAX*4];
	int lmode = st->st_mode & 07777;

	if (u == US_CHMOG) {
		if (IS_ON(opts, DO_VERIFY)) {
			message(MT_INFO,
				"%s: need to change to perm %04o, owner %s, group %s",
				starget, lmode, user, group);
			runspecial(starget, opts, rname, destdir);
		}
		else {
			message(MT_CHANGE, "%s: change to perm %04o, owner %s, group %s", 
				starget, lmode, user, group);
			ENCODE(ername, rname);
			(void) sendcmd(C_CHMOG, "%lo %04o %s %s %s",
				       opts, lmode, user, group, ername);
			(void) response();
		}
		rv = 1;
	}
	return(rv);
}


/*
 * We need to install/update:
 */
static int
fullupdate(int u, char *starget, opt_t opts, char *rname, int destdir,
	   struct stat *st, char *user, char *group)
{
	/*
	 * No entry - need to install
	 */
	if (u == US_NOENT) {
		if (IS_ON(opts, DO_VERIFY)) {
			message(MT_INFO, "%s: need to install", starget);
			runspecial(starget, opts, rname, destdir);
			return(1);
		}
		if (!IS_ON(opts, DO_QUIET))
			message(MT_CHANGE, "%s: installing", starget);
		FLAG_OFF(opts, (DO_COMPARE|DO_REMOVE));
	}

	/*
	 * Handle special file types, including directories and symlinks
	 */
	if (S_ISDIR(st->st_mode)) {
		if (senddir(rname, opts, st, user, group, destdir) > 0)
			return(1);
		return(0);
	} else if (S_ISLNK(st->st_mode)) {
		if (u == US_NOENT)
			FLAG_ON(opts, DO_COMPARE);
		/*
		 * Since we always send link info to the server
		 * so the server can determine if the remote link
		 * is correct, we never get any acknowledgement
		 * from the server whether the link was really
		 * updated or not.
		 */
		(void) sendlink(rname, opts, st, user, group, destdir);
		return(0);
	} else if (S_ISREG(st->st_mode)) {		
		if (u == US_OUTDATE) {
			if (IS_ON(opts, DO_VERIFY)) {
				message(MT_INFO, "%s: need to update", starget);
				runspecial(starget, opts, rname, destdir);
				return(1);
			}
			if (!IS_ON(opts, DO_QUIET))
				message(MT_CHANGE, "%s: updating", starget);
		}
		return (sendfile(rname, opts, st, user, group, destdir) == 0);
	} else {
		message(MT_INFO, "%s: unknown file type 0%o", starget,
			st->st_mode);
		return(0);
	}
}

/*
 * Transfer the file or directory in target[].
 * rname is the name of the file on the remote host.
 *
 * Return < 0 on error.
 * Return 0 if nothing happened.
 * Return > 0 if anything is updated.
 */
static int
sendit(char *rname, opt_t opts, int destdir)
{
	static struct stat stb;
	char *user, *group;
	int u, len;

	/*
	 * Remove possible accidental newline
	 */
	len = strlen(rname);
	if (len > 0 && rname[len-1] == '\n')
		rname[len-1] = CNULL;

	if (checkfilename(rname) != 0)
		return(-1);

	debugmsg(DM_CALL, "sendit(%s, 0x%lx) called\n", rname, opts);

	if (except(target))
		return(0);

	if (dostat(target, &stb, opts) < 0)
		return(-1);

	/*
	 * Does rname need updating?
	 */
	u = update(rname, opts, &stb);
	debugmsg(DM_MISC, "sendit(%s, 0x%lx): update status of %s is %d\n", 
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

	user = getusername(stb.st_uid, target, opts);
	group = getgroupname(stb.st_gid, target, opts);

	if (u == US_CHMOG && IS_OFF(opts, DO_UPDATEPERM))
		u = US_OUTDATE;

	if (u == US_NOENT || u == US_OUTDATE || u == US_DOCOMP)
		return(fullupdate(u, target, opts, rname, destdir, &stb,
				  user, group));

	if (u == US_CHMOG)
		return(statupdate(u, target, opts, rname, destdir, &stb,
				  user, group));

	return(0);
}
	
/*
 * Remove temporary files and do any cleanup operations before exiting.
 */
void
cleanup(int dummy)
{
	char *file;

	if ((file = getnotifyfile()) != NULL)
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
int
install(char *src, char *dest, int ddir, int destdir, opt_t opts)
{
	static char destcopy[PATH_MAX];
	char *rname;
	int didupdate = 0;
	char ername[PATH_MAX*4];

	debugmsg(DM_CALL,
		"install(src=%s,dest=%s,ddir=%d,destdir=%d,opts=%ld) start\n",
		(src?src:"NULL"), (dest?dest:"NULL"), ddir, destdir, opts);
	/*
	 * Save source name
	 */
	if (IS_ON(opts, DO_WHOLE))
		source[0] = CNULL;
	else
		(void) strlcpy(source, src, sizeof(source));

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
		(void) snprintf(buff, sizeof(buff), "%s%s%s %s %s", 
			       IS_ON(opts, DO_VERIFY) ? "verify" : "install",
			       (cp) ? " -o" : "", (cp) ? cp : "", 
			       src, dest);
		if (nflag) {
			printf("%s\n", buff);
			return(0);
		} else
			debugmsg(DM_MISC, "%s\n", buff);
	}

	rname = exptilde(target, src, sizeof(target));
	if (rname == NULL)
		return(-1);
	ptarget = target;
	while (*ptarget)
		ptarget++;
	/*
	 * If we are renaming a directory and we want to preserve
	 * the directory hierarchy (-w), we must strip off the leading
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
	ENCODE(ername, dest);
 	if (ddir)
		(void) sendcmd(C_DIRTARGET, "%lo %s", opts, ername);
	else
		(void) sendcmd(C_TARGET, "%lo %s", opts, ername);
	if (response() < 0)
		return(-1);

	/*
	 * Save the name of the remote target destination if we are
	 * in WHOLE mode (destdir > 0) or if the source and destination
	 * are not the same.  This info will be used later for maintaining
	 * hardlink info.
	 */
	if (destdir || (src && dest && strcmp(src, dest))) {
		(void) strlcpy(destcopy, dest, sizeof(destcopy));
		Tdest = destcopy;
	}

	didupdate = sendit(rname, opts, destdir);
	Tdest = 0;

	return(didupdate);
}
