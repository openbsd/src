/*	$OpenBSD: server.c,v 1.6 1998/05/18 19:12:53 deraadt Exp $	*/

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
static char RCSid[] = 
"$OpenBSD: server.c,v 1.6 1998/05/18 19:12:53 deraadt Exp $";

static char sccsid[] = "@(#)server.c	5.3 (Berkeley) 6/7/86";

static char copyright[] =
"@(#) Copyright (c) 1983 Regents of the University of California.\n\
 All rights reserved.\n";
#endif /* not lint */

/*
 * Server routines
 */

#include "defs.h"

char	tempname[sizeof _RDIST_TMP + 1]; /* Tmp file name */
char	buf[BUFSIZ];		/* general purpose buffer */
char	target[MAXPATHLEN];	/* target/source directory name */
char	*ptarget;		/* pointer to end of target name */
int	catname = 0;		/* cat name to target name */
char	*sptarget[32];		/* stack of saved ptarget's for directories */
char   *fromhost = NULL;	/* Client hostname */
static long min_freespace = 0;	/* Minimium free space on a filesystem */
static long min_freefiles = 0;	/* Minimium free # files on a filesystem */
int	oumask;			/* Old umask */

/*
 * Cat "string" onto the target buffer with error checking.
 */
static int cattarget(string)
	char *string;
{
	if (strlen(string) + strlen(target) + 2 > sizeof(target)) {
		message(MT_INFO, "target buffer is not large enough.");
		return(-1);
	}
	if (!ptarget) {
		message(MT_INFO, "NULL target pointer set.");
		return(-10);
	}

	(void) sprintf(ptarget, "/%s", string);

	return(0);
}
	
/*
 * Set uid and gid ownership of a file.
 */
static int setownership(file, fd, uid, gid)
	char *file;
	int fd;
	UID_T uid;
	GID_T gid;
{
	int status = -1;

	/*
	 * We assume only the Superuser can change uid ownership.
	 */
	if (getuid() == 0) {
#if	defined(HAVE_FCHOWN)
		if (fd != -1)
			status = fchown(fd, (CHOWN_UID_T) uid, 
					(CHOWN_GID_T) gid);
#endif
		if (status < 0)
			status = chown(file, (CHOWN_UID_T) uid, 
				       (CHOWN_GID_T) gid);

		if (status < 0) {
			message(MT_NOTICE, "%s: chown %d.%d failed: %s", 
				target, (UID_T) uid, (GID_T) gid, SYSERR);
			return(-1);
		}
	} else {
#if	defined(HAVE_FCHOWN)
		if (fd != -1)
			status = fchown(fd, (CHOWN_UID_T) -1, 
					(CHOWN_GID_T) gid);
#endif
		if (status < 0)
			status = chown(file, (CHOWN_UID_T) -1, 
				       (CHOWN_GID_T) gid);

		if (status < 0) {
			message(MT_NOTICE, "%s: chgrp %d failed: %s",
				target, (GID_T) gid, SYSERR);
			return(-1);
		}
	}

	return(0);
}

/*
 * Set mode of a file
 */
static int setfilemode(file, fd, mode)
	char *file;
	int fd;
	int mode;
{
	int status = -1;

	if (mode == -1)
		return(0);

#if	defined(HAVE_FCHMOD)
	if (fd != -1)
		status = fchmod(fd, mode);
#endif

	if (status < 0)
		status = chmod(file, mode);

	if (status < 0) {
		message(MT_NOTICE, "%s: chmod failed: %s", target, SYSERR);
		return(-1);
	}

	return(0);
}

/*
 * Get group entry.  This routine takes a string argument (name).
 * If name is of form ":N" a lookup for gid N is done.
 * Otherwise a lookup by name is done.
 */
static struct group *mygetgroup(name)
	char *name;
{
    	struct group *gr;

	if (*name == ':')
	    	gr = getgrgid(atoi(name + 1));
	else
	    	gr = getgrnam(name);

	return(gr);
}

/*
 * Change owner, group and mode of file.
 */
static int fchog(fd, file, owner, group, mode)
	int fd;
	char *file, *owner, *group;
	int mode;
{
	struct group *gr = NULL;
	static char last_group[128];
	static char last_owner[128];
	static GID_T last_gid = (GID_T)-2;
	static UID_T last_uid = (UID_T)-2;
	static GID_T last_primegid;
	extern char *locuser;
	register int i;
	UID_T uid;
	GID_T gid;
	GID_T primegid = (GID_T)-2;

	uid = userid;
	if (userid == 0) {	/* running as root; take anything */
		if (*owner == ':') {
			uid = (UID_T) atoi(owner + 1);
		} else if (last_uid == (UID_T)-2 ||
			   strcmp(owner, last_owner) != 0) {
			if ((pw = getpwnam(owner)) == NULL) {
				if (mode != -1 && IS_ON(mode, S_ISUID)) {
					message(MT_NOTICE,
			      "%s: unknown login name \"%s\", clearing setuid",
						target, owner);
					mode &= ~S_ISUID;
					uid = 0;
				} else
					message(MT_NOTICE,
					"%s: unknown login name \"%s\"",
						target, owner);
			} else {
				uid	 = last_uid	 = pw->pw_uid;
				primegid = last_primegid = pw->pw_gid;
				strcpy(last_owner, owner);
			}
		} else {
			uid = last_uid;
			primegid = last_primegid;
		}
		if (*group == ':') {
			gid = (GID_T) atoi(group + 1);
			goto ok;
		}
	} else {	/* not root, setuid only if user==owner */
		struct passwd *lupw;

		if (mode != -1) {
			if (IS_ON(mode, S_ISUID) && 
			    strcmp(locuser, owner) != 0)
				mode &= ~S_ISUID;
			if (mode)
				mode &= ~S_ISVTX; /* and strip sticky too */
		}

		if ((lupw = getpwnam(locuser)) != NULL)
			primegid = lupw->pw_gid;
	}

	gid = (GID_T) -1;
	if (last_gid < (GID_T)0 || strcmp(group, last_group) != 0) {
	        /*
		 * Invalid cached values so we need to do a new lookup.
		 */
		if (gr = mygetgroup(group)) {
			last_gid = gid = gr->gr_gid;
			strcpy(last_group, gr->gr_name);
		} else {
			if (mode != -1 && IS_ON(mode, S_ISGID)) {
				message(MT_NOTICE, 
				"%s: unknown group \"%s\", clearing setgid",
					target, group);
				mode &= ~S_ISGID;
			} else
				message(MT_NOTICE, 
					"%s: unknown group \"%s\"",
					target, group);
		}
	} else {
	    	/*
		 * Use the cached values.
		 */
		gid = last_gid;
	}

	/*
	 * We need to check non-root users to make sure they're a member
	 * of the group.  If they are not, we don't set that gid ownership.
	 */
	if (userid && gid >= 0 && gid != primegid) {
		if (!gr)
		    	gr = mygetgroup(group);
		if (gr)
			for (i = 0; gr->gr_mem[i] != NULL; i++)
			    	if (strcmp(locuser, gr->gr_mem[i]) == 0)
					goto ok;
		if (mode != -1 && IS_ON(mode, S_ISGID)) {
			message(MT_NOTICE, 
				"%s: user %s not in group %s, clearing setgid",
				target, locuser, group);
			mode &= ~S_ISGID;
		}
		gid = (GID_T) -1;
	}
ok:
	/*
	 * Set uid and gid ownership.  If that fails, strip setuid and
	 * setgid bits from mode.  Once ownership is set, successful
	 * or otherwise, set the new file mode.
	 */
	if (setownership(file, fd, uid, gid) < 0) {
		if (mode != -1 && IS_ON(mode, S_ISUID)) {
			message(MT_NOTICE, 
				"%s: chown failed, clearing setuid", target);
			mode &= ~S_ISUID;
		}
		if (mode != -1 && IS_ON(mode, S_ISGID)) {
			message(MT_NOTICE, 
				"%s: chown failed, clearing setgid", target);
			mode &= ~S_ISGID;
		}
	}
	(void) setfilemode(file, fd, mode);


	return(0);
}

/*
 * Remove a file or directory (recursively) and send back an acknowledge
 * or an error message.
 */
static int removefile(statb)
	struct stat *statb;
{
	DIR *d;
	static DIRENTRY *dp;
	register char *cp;
	struct stat stb;
	char *optarget;
	int len, failures = 0;

	switch (statb->st_mode & S_IFMT) {
	case S_IFREG:
	case S_IFLNK:
		if (unlink(target) < 0) {
			if (errno == ETXTBSY) {
				message(MT_REMOTE|MT_NOTICE, 
					"%s: unlink failed: %s",
					target, SYSERR);
				return(0);
			} else {
				error("%s: unlink failed: %s", target, SYSERR);
				return(-1);
			}
		}
		goto removed;

	case S_IFDIR:
		break;

	default:
		error("%s: not a plain file", target);
		return(-1);
	}

	errno = 0;
	if ((d = opendir(target)) == NULL) {
		error("%s: opendir failed: %s", target, SYSERR);
		return(-1);
	}

	optarget = ptarget;
	len = ptarget - target;
	while (dp = readdir(d)) {
		if ((D_NAMLEN(dp) == 1 && dp->d_name[0] == '.') ||
		    (D_NAMLEN(dp) == 2 && dp->d_name[0] == '.' &&
		     dp->d_name[1] == '.'))
			continue;

		if (len + 1 + (int)strlen(dp->d_name) >= MAXPATHLEN - 1) {
			message(MT_REMOTE|MT_WARNING, "%s/%s: Name too long", 
				target, dp->d_name);
			continue;
		}
		ptarget = optarget;
		*ptarget++ = '/';
		cp = dp->d_name;;
		while (*ptarget++ = *cp++)
			;
		ptarget--;
		if (lstat(target, &stb) < 0) {
			message(MT_REMOTE|MT_WARNING, "%s: lstat failed: %s", 
				target, SYSERR);
			continue;
		}
		if (removefile(&stb) < 0)
			++failures;
	}
	(void) closedir(d);
	ptarget = optarget;
	*ptarget = CNULL;

	if (failures)
		return(-1);

	if (rmdir(target) < 0) {
		error("%s: rmdir failed: %s", target, SYSERR);
		return(-1);
	}
removed:
	/*
	 * We use MT_NOTICE instead of MT_CHANGE because this function is
	 * sometimes called by other functions that are suppose to return a
	 * single ack() back to the client (rdist).  This is a kludge until
	 * the Rdist protocol is re-done.  Sigh.
	 */
	message(MT_NOTICE|MT_REMOTE, "%s: removed", target);
	return(0);
}

/*
 * Check the current directory (initialized by the 'T' command to server())
 * for extraneous files and remove them.
 */
static void doclean(cp)
	register char *cp;
{
	DIR *d;
	register DIRENTRY *dp;
	struct stat stb;
	char *optarget, *ep;
	int len;
	opt_t opts;

	opts = strtol(cp, &ep, 8);
	if (*ep != CNULL) {
		error("clean: options not delimited");
		return;
	}
	if ((d = opendir(target)) == NULL) {
		error("%s: opendir failed: %s", target, SYSERR);
		return;
	}
	ack();

	optarget = ptarget;
	len = ptarget - target;
	while (dp = readdir(d)) {
		if ((D_NAMLEN(dp) == 1 && dp->d_name[0] == '.') ||
		    (D_NAMLEN(dp) == 2 && dp->d_name[0] == '.' &&
		     dp->d_name[1] == '.'))
			continue;

		if (len + 1 + (int)strlen(dp->d_name) >= MAXPATHLEN - 1) {
			message(MT_REMOTE|MT_WARNING, "%s/%s: Name too long", 
				target, dp->d_name);
			continue;
		}
		ptarget = optarget;
		*ptarget++ = '/';
		cp = dp->d_name;;
		while (*ptarget++ = *cp++)
			;
		ptarget--;
		if (lstat(target, &stb) < 0) {
			message(MT_REMOTE|MT_WARNING, "%s: lstat failed: %s", 
				target, SYSERR);
			continue;
		}

		(void) sendcmd(CC_QUERY, "%s", dp->d_name);
		(void) remline(cp = buf, sizeof(buf), TRUE);

		if (*cp != CC_YES)
			continue;

		if (IS_ON(opts, DO_VERIFY))
			message(MT_REMOTE|MT_INFO, "%s: need to remove", 
				target);
		else
			(void) removefile(&stb);
	}
	(void) closedir(d);

	ptarget = optarget;
	*ptarget = CNULL;
}

/*
 * Frontend to doclean().
 */
static void clean(cp)
	register char *cp;
{
	doclean(cp);
	(void) sendcmd(CC_END, NULL);
	(void) response();
}

/*
 * Execute a shell command to handle special cases.
 * We can't really set an alarm timeout here since we
 * have no idea how long the command should take.
 */
static void dospecial(cmd)
	char *cmd;
{
	runcommand(cmd);
}

/*
 * Do a special cmd command.  This differs from normal special
 * commands in that it's done after an entire command has been updated.
 * The list of updated target files is sent one at a time with RC_FILE
 * commands.  Each one is added to an environment variable defined by
 * E_FILES.  When an RC_COMMAND is finally received, the E_FILES variable
 * is stuffed into our environment and a normal dospecial() command is run.
 */
static void docmdspecial()
{
	register char *cp;
	char *cmd, *env = NULL;
	int n;
	int len;

	/* We're ready */
	ack();

	for ( ; ; ) {
		n = remline(cp = buf, sizeof(buf), FALSE);
		if (n <= 0) {
			error("cmdspecial: premature end of input.");
			return;
		}

		switch (*cp++) {
		case RC_FILE:
			if (env == NULL) {
				len = (2 * sizeof(E_FILES)) + strlen(cp) + 10;
				env = (char *) xmalloc(len);
				(void) sprintf(env, "export %s;%s=%s", 
					       E_FILES, E_FILES, cp);
			} else {
				len = strlen(env);
				env = (char *) xrealloc(env, 
							len + strlen(cp) + 2);
				env[len] = CNULL;
				(void) strcat(env, ":");
				(void) strcat(env, cp);
			}
			ack();
			break;

		case RC_COMMAND:
			if (env) {
				len = strlen(env);
				env = (char *) xrealloc(env, 
							len + strlen(cp) + 2);
				env[len] = CNULL;
				(void) strcat(env, ";");
				(void) strcat(env, cp);
				cmd = env;
			} else
				cmd = cp;

			dospecial(cmd);
			if (env)
				(void) free(env);
			return;

		default:
			error("Unknown cmdspecial command '%s'.", cp);
			return;
		}
	}
}

/*
 * Query. Check to see if file exists. Return one of the following:
 *
#ifdef NFS_CHECK
 *  QC_ONNFS		- resides on a NFS
#endif NFS_CHECK
#ifdef RO_CHECK
 *  QC_ONRO		- resides on a Read-Only filesystem
#endif RO_CHECK
 *  QC_NO		- doesn't exist
 *  QC_YESsize mtime 	- exists and its a regular file (size & mtime of file)
 *  QC_YES		- exists and its a directory or symbolic link
 *  QC_ERRMSGmessage 	- error message
 */
static void query(name)
	char *name;
{
	static struct stat stb;
	int s = -1, stbvalid = 0;

	if (catname && cattarget(name) < 0)
		return;

#if	defined(NFS_CHECK)
	if (IS_ON(options, DO_CHKNFS)) {
		s = is_nfs_mounted(target, &stb, &stbvalid);
		if (s > 0)
			(void) sendcmd(QC_ONNFS, NULL);

		/* Either the above check was true or an error occured */
		/* and is_nfs_mounted sent the error message */
		if (s != 0) {
			*ptarget = CNULL;
			return;
		}
	}
#endif 	/* NFS_CHECK */

#if	defined(RO_CHECK)
	if (IS_ON(options, DO_CHKREADONLY)) {
		s = is_ro_mounted(target, &stb, &stbvalid);
		if (s > 0)
			(void) sendcmd(QC_ONRO, NULL);

		/* Either the above check was true or an error occured */
		/* and is_ro_mounted sent the error message */
		if (s != 0) {
			*ptarget = CNULL;
			return;
		}
	}
#endif 	/* RO_CHECK */

	if (IS_ON(options, DO_CHKSYM)) {
		if (is_symlinked(target, &stb, &stbvalid) > 0) {
			(void) sendcmd(QC_SYM, NULL);
			return;
		}
	}

	/*
	 * If stbvalid is false, "stb" is not valid because:
	 *	a) RO_CHECK and NFS_CHECK were not defined
	 *	b) The stat by is_*_mounted() either failed or
	 *	   does not match "target".
	 */
	if (!stbvalid && lstat(target, &stb) < 0) {
		if (errno == ENOENT)
			(void) sendcmd(QC_NO, NULL);
		else
			error("%s: lstat failed: %s", target, SYSERR);
		*ptarget = CNULL;
		return;
	}

	switch (stb.st_mode & S_IFMT) {
	case S_IFLNK:
	case S_IFDIR:
	case S_IFREG:
		(void) sendcmd(QC_YES, "%ld %ld %o %s %s",
			       (long) stb.st_size, 
			       stb.st_mtime, 
			       stb.st_mode & 07777,
			       getusername(stb.st_uid, target, options), 
			       getgroupname(stb.st_gid, target, options));
		break;

	default:
		error("%s: not a file or directory", target);
		break;
	}
	*ptarget = CNULL;
}

/*
 * Check to see if parent directory exists and create one if not.
 */
static int chkparent(name, opts)
	char *name;
	opt_t opts;
{
	register char *cp;
	struct stat stb;
	int r = -1;

	debugmsg(DM_CALL, "chkparent(%s, %o) start\n", name, opts);

	cp = strrchr(name, '/');
	if (cp == NULL || cp == name)
		return(0);

	*cp = CNULL;

	if (lstat(name, &stb) < 0) {
		if (errno == ENOENT && chkparent(name, opts) >= 0) {
			if (mkdir(name, 0777 & ~oumask) == 0) {
				message(MT_NOTICE, "%s: mkdir", name);
				r = 0;
			} else 
				debugmsg(DM_MISC, 
					 "chkparent(%s, %o) mkdir fail: %s\n",
					 name, opts, SYSERR);
		}
	} else	/* It exists */
		r = 0;

	/* Put back what we took away */
	*cp = '/';

	return(r);
}

/*
 * Save a copy of 'file' by renaming it.
 */
static char *savetarget(file)
	char *file;
{
	static char savefile[MAXPATHLEN];

	if (strlen(file) + sizeof(SAVE_SUFFIX) + 1 > MAXPATHLEN) {
		error("%s: Cannot save: Save name too long", file);
		return((char *) NULL);
	}

	(void) sprintf(savefile, "%s%s", file, SAVE_SUFFIX);

	if (unlink(savefile) != 0 && errno != ENOENT) {
		message(MT_NOTICE, "%s: remove failed: %s", savefile, SYSERR);
		return((char *) NULL);
	}

	if (rename(file, savefile) != 0 && errno != ENOENT) {
		error("%s -> %s: rename failed: %s", 
		      file, savefile, SYSERR);
		return((char *) NULL);
	}

	return(savefile);
}

/*
 * See if buf is all zeros (sparse check)
 */
static int iszeros (buf, size)
	char *buf;
	off_t size;
{
    	while (size > 0) {
	    if (*buf != CNULL)
		return(0);
	    buf++;
	    size--;
	}

	return(1);
}

  
/*
 * Receive a file
 */
static void recvfile(new, opts, mode, owner, group, mtime, atime, size)
	/*ARGSUSED*/
	char *new;
	opt_t opts;
	int mode;
	char *owner, *group;
	time_t mtime;
	time_t atime;
	off_t size;
{
	int f, wrerr, olderrno, lastwashole = 0, wassparse = 0;
	off_t i;
	register char *cp;
	char *savefile = NULL;
	static struct stat statbuff;

	/*
	 * Create temporary file
	 */
	if ((f = open(new, O_CREAT|O_EXCL|O_WRONLY, mode)) < 0) {
		if (errno != ENOENT || chkparent(new, opts) < 0 ||
		    (f = open(new, O_CREAT|O_EXCL|O_WRONLY, mode)) < 0) {
			error("%s: create failed: %s", new, SYSERR);
			(void) unlink(new);
			return;
		}
	}

	/*
	 * Receive the file itself
	 */
	ack();
	wrerr = 0;
	olderrno = 0;
	for (i = 0; i < size; i += BUFSIZ) {
		int amt = BUFSIZ;

		cp = buf;
		if (i + amt > size)
			amt = size - i;
		do {
			int j;

			j = readrem(cp, amt);
			if (j <= 0) {
				(void) close(f);
				(void) unlink(new);
				fatalerr(
				   "Read error occured while receiving file.");
				finish();
			}
			amt -= j;
			cp += j;
		} while (amt > 0);
		amt = BUFSIZ;
		if (i + amt > size)
			amt = size - i;
		if (IS_ON(opts, DO_SPARSE) && iszeros(buf, amt)) {
		    	if (lseek (f, amt, SEEK_CUR) < 0L) {
			    	olderrno = errno;
				wrerr++;
			}
			lastwashole = 1;
			wassparse++;
		} else {
		    	if (wrerr == 0 && xwrite(f, buf, amt) != amt) {
			    	olderrno = errno;
				wrerr++;
			}
			lastwashole = 0;
		}
	}

	if (lastwashole) {
#if	defined(HAVE_FTRUNCATE)
	    	if (write (f, "", 1) != 1 || ftruncate (f, size) < 0)
#else
		/* Seek backwards one character and write a null.  */
		if (lseek (f, (off_t) -1, SEEK_CUR) < 0L
		    || write (f, "", 1) != 1)
#endif
		{
			olderrno = errno;
			wrerr++;
		}
	}

	if (response() < 0) {
		(void) close(f);
		(void) unlink(new);
		return;
	}

	if (wrerr) {
		error("%s: Write error: %s", new, strerror(olderrno));
		(void) close(f);
		(void) unlink(new);
		return;
	}

	/*
	 * Do file comparison if enabled
	 */
	if (IS_ON(opts, DO_COMPARE)) {
		FILE *f1, *f2;
		int c;

		errno = 0;	/* fopen is not a syscall */
		if ((f1 = fopen(target, "r")) == NULL) {
			error("%s: open for read failed: %s", target, SYSERR);
			(void) close(f);
			(void) unlink(new);
			return;
		}
		errno = 0;
		if ((f2 = fopen(new, "r")) == NULL) {
			error("%s: open for read failed: %s", new, SYSERR);
			(void) close(f);
			(void) unlink(new);
			return;
		}
		while ((c = getc(f1)) == getc(f2))
			if (c == EOF) {
				debugmsg(DM_MISC, 
					 "Files are the same '%s' '%s'.",
					 target, new);
				(void) fclose(f1);
				(void) fclose(f2);
				(void) close(f);
				(void) unlink(new);
				/*
				 * This isn't an error per-se, but we
				 * need to indicate to the master that
				 * the file was not updated.
				 */
				error("");
				return;
			}
		debugmsg(DM_MISC, "Files are different '%s' '%s'.",
			 target, new);
		(void) fclose(f1);
		(void) fclose(f2);
		if (IS_ON(opts, DO_VERIFY)) {
			message(MT_REMOTE|MT_INFO, "%s: need to update", 
				target);
			(void) close(f);
			(void) unlink(new);
			return;
		}
	}

	/*
	 * Set owner, group, and file mode
	 */
	if (fchog(f, new, owner, group, mode) < 0) {
		(void) close(f);
		(void) unlink(new);
		return;
	}
	(void) close(f);

	/*
	 * Perform utimes() after file is closed to make
	 * certain OS's, such as NeXT 2.1, happy.
	 */
	if (setfiletime(new, time((time_t *) 0), mtime) < 0)
		message(MT_NOTICE, "%s: utimes failed: %s", new, SYSERR);

	/*
	 * Try to save target file from being over-written
	 */
	if (IS_ON(opts, DO_SAVETARGETS))
		if ((savefile = savetarget(target)) == NULL) {
			(void) unlink(new);
			return;
		}

	/*
	 * If the target is a directory, we need to remove it first
	 * before we can rename the new file.
	 */
	if ((stat(target, &statbuff) == 0) && S_ISDIR(statbuff.st_mode)) {
		char *saveptr = ptarget;

		ptarget = &target[strlen(target)];
		removefile(&statbuff);
		ptarget = saveptr;
	}

	/*
	 * Install new (temporary) file as the actual target
	 */
	if (rename(new, target) < 0) {
		/*
		 * If the rename failed due to "Text file busy", then
		 * try to rename the target file and retry the rename.
		 */
		if (errno == ETXTBSY) {
			/* Save the target */
			if ((savefile = savetarget(target)) != NULL) {
				/* Retry installing new file as target */
				if (rename(new, target) < 0) {
					error("%s -> %s: rename failed: %s",
					      new, target, SYSERR);
					/* Try to put back save file */
					if (rename(savefile, target) < 0)
						error(
					         "%s -> %s: rename failed: %s",
						      savefile, target, 
						      SYSERR);
				} else
					message(MT_NOTICE, "%s: renamed to %s",
						target, savefile);
			}
		} else {
			error("%s -> %s: rename failed: %s", 
			      new, target, SYSERR);
			(void) unlink(new);
		}
	}

	if (wassparse)
	    	message (MT_NOTICE, "%s: was sparse", target);

	if (IS_ON(opts, DO_COMPARE))
		message(MT_REMOTE|MT_CHANGE, "%s: updated", target);
	else
		ack();
}

/*
 * Receive a directory
 */
static void recvdir(opts, mode, owner, group)
	opt_t opts;
	int mode;
	char *owner, *group;
{
	static char lowner[100], lgroup[100];
	register char *cp;
	struct stat stb;
	int s;

	s = lstat(target, &stb);
	if (s == 0) {
		/*
		 * If target is not a directory, remove it
		 */
		if (!S_ISDIR(stb.st_mode)) {
			if (IS_ON(opts, DO_VERIFY))
				message(MT_NOTICE, "%s: need to remove",
					target);
			else {
				if (unlink(target) < 0) {
					error("%s: remove failed: %s",
					      target, SYSERR);
					return;
				}
			}
			s = -1;
			errno = ENOENT;
		} else {
			if (!IS_ON(opts, DO_NOCHKMODE) &&
			    (stb.st_mode & 07777) != mode) {
				if (IS_ON(opts, DO_VERIFY))
					message(MT_NOTICE, 
						"%s: need to chmod to %o",
						target, mode);
				else {
					if (chmod(target, mode) != 0)
						message(MT_NOTICE,
					  "%s: chmod from %o to %o failed: %s",
							target, 
							stb.st_mode & 07777, 
							mode,
							SYSERR);
					else
						message(MT_NOTICE,
						"%s: chmod from %o to %o",
							target, 
							stb.st_mode & 07777, 
							mode);
				}
			}

			/*
			 * Check ownership and set if necessary
			 */
			lowner[0] = CNULL;
			lgroup[0] = CNULL;

			if (!IS_ON(opts, DO_NOCHKOWNER) && owner) {
				int o;

				o = (owner[0] == ':') ? opts & DO_NUMCHKOWNER :
					opts;
				if (cp = getusername(stb.st_uid, target, o))
					if (strcmp(owner, cp))
						(void) strcpy(lowner, cp);
			}
			if (!IS_ON(opts, DO_NOCHKGROUP) && group) {
				int o;

				o = (group[0] == ':') ? opts & DO_NUMCHKGROUP :
					opts;
				if (cp = getgroupname(stb.st_gid, target, o))
					if (strcmp(group, cp))
						(void) strcpy(lgroup, cp);
			}

			/*
			 * Need to set owner and/or group
			 */
#define PRN(n) ((n[0] == ':') ? n+1 : n)
			if (lowner[0] != CNULL || lgroup[0] != CNULL) {
				if (lowner[0] == CNULL && 
				    (cp = getusername(stb.st_uid, 
						      target, opts)))
					(void) strcpy(lowner, cp);
				if (lgroup[0] == CNULL && 
				    (cp = getgroupname(stb.st_gid, 
						       target, opts)))
					(void) strcpy(lgroup, cp);

				if (IS_ON(opts, DO_VERIFY))
					message(MT_NOTICE,
				"%s: need to chown from %s.%s to %s.%s",
						target, 
						PRN(lowner), PRN(lgroup),
						PRN(owner), PRN(group));
				else {
					if (fchog(-1, target, owner, 
						  group, -1) == 0)
						message(MT_NOTICE,
					       "%s: chown from %s.%s to %s.%s",
							target,
							PRN(lowner), 
							PRN(lgroup),
							PRN(owner), 
							PRN(group));
				}
			}
#undef PRN
			ack();
			return;
		}
	}

	if (IS_ON(opts, DO_VERIFY)) {
		ack();
		return;
	}

	/*
	 * Create the directory
	 */
	if (s < 0) {
		if (errno == ENOENT) {
			if (mkdir(target, mode) == 0 ||
			    chkparent(target, opts) == 0 && 
			    mkdir(target, mode) == 0) {
				message(MT_NOTICE, "%s: mkdir", target);
				(void) fchog(-1, target, owner, group, mode);
				ack();
			} else {
				error("%s: mkdir failed: %s", target, SYSERR);
				ptarget = sptarget[--catname];
				*ptarget = CNULL;
			}
			return;
		}
	}
	error("%s: lstat failed: %s", target, SYSERR);
	ptarget = sptarget[--catname];
	*ptarget = CNULL;
}

/*
 * Receive a link
 */
static void recvlink(new, opts, mode, size)
	char *new;
	opt_t opts;
	int mode;
	off_t size;
{
	struct stat stb;
	char *optarget;
	off_t i;

	/*
	 * Read basic link info
	 */
	ack();
	(void) remline(buf, sizeof(buf), TRUE);

	if (response() < 0) {
		err();
		return;
	}

	/*
	 * Make new symlink using a temporary name
	 */
	if (symlink(buf, new) < 0) {
		if (errno != ENOENT || chkparent(new, opts) < 0 ||
		    symlink(buf, new) < 0) {
			error("%s -> %s: symlink failed: %s", new, buf,SYSERR);
			(void) unlink(new);
			return;
		}
	}

	/*
	 * Do comparison of what link is pointing to if enabled
	 */
	mode &= 0777;
	if (IS_ON(opts, DO_COMPARE)) {
		char tbuf[MAXPATHLEN];
		
		if ((i = readlink(target, tbuf, sizeof(tbuf)-1)) >= 0 &&
		    i == size && strncmp(buf, tbuf, (int) size) == 0) {
			(void) unlink(new);
			ack();
			return;
		}
		if (IS_ON(opts, DO_VERIFY)) {
			(void) unlink(new);
			message(MT_REMOTE|MT_INFO, "%s: need to update",
				target);
			(void) sendcmd(C_END, NULL);
			(void) response();
			return;
		}
	}

	/*
	 * See if target is a directory and remove it if it is
	 */
	if (lstat(target, &stb) == 0) {
		if (S_ISDIR(stb.st_mode)) {
			optarget = ptarget;
			for (ptarget = target; *ptarget; ptarget++);
			if (removefile(&stb) < 0) {
				ptarget = optarget;
				(void) unlink(new);
				(void) sendcmd(C_END, NULL);
				(void) response();
				return;
			}
			ptarget = optarget;
		}
	}

	/*
	 * Install link as the target
	 */
	if (rename(new, target) < 0) {
		error("%s -> %s: symlink rename failed: %s",
		      new, target, SYSERR);
		(void) unlink(new);
		(void) sendcmd(C_END, NULL);
		(void) response();
		return;
	}

	if (IS_ON(opts, DO_COMPARE))
		message(MT_REMOTE|MT_CHANGE, "%s: updated", target);
	else
	        ack();

	/*
	 * Indicate end of receive operation
	 */
	(void) sendcmd(C_END, NULL);
	(void) response();
}

/*
 * Creat a hard link to existing file.
 */
static void hardlink(cmd)
	char *cmd;
{
	struct stat stb;
	int exists = 0;
	char *oldname, *newname;
	char *cp = cmd;
	static char expbuf[BUFSIZ];

	/* Skip over opts */
	(void) strtol(cp, &cp, 8);
	if (*cp++ != ' ') {
		error("hardlink: options not delimited");
		return;
	}

	oldname = strtok(cp, " ");
	if (oldname == NULL) {
		error("hardlink: oldname name not delimited");
		return;
	}

	newname = strtok((char *)NULL, " ");
	if (newname == NULL) {
		error("hardlink: new name not specified");
		return;
	}

	if (exptilde(expbuf, oldname) == NULL) {
		error("hardlink: tilde expansion failed");
		return;
	}
	oldname = expbuf;

	if (catname && cattarget(newname) < 0) {
		error("Cannot set newname target.");
		return;
	}

	if (lstat(target, &stb) == 0) {
		int mode = stb.st_mode & S_IFMT;

		if (mode != S_IFREG && mode != S_IFLNK) {
			error("%s: not a regular file", target);
			return;
		}
		exists = 1;
	}

	if (chkparent(target, options) < 0 ) {
		error("%s: no parent: %s ", target, SYSERR);
		return;
	}
	if (exists && (unlink(target) < 0)) {
		error("%s: unlink failed: %s", target, SYSERR);
		return;
	}
	if (link(oldname, target) < 0) {
		error("%s: cannot link to %s: %s", target, oldname, SYSERR);
		return;
	}
	ack();
}

/*
 * Set configuration information.
 *
 * A key letter is followed immediately by the value
 * to set.  The keys are:
 *	SC_FREESPACE	- Set minimium free space of filesystem
 *	SC_FREEFILES	- Set minimium free number of files of filesystem
 */
static void setconfig(cmd)
	char *cmd;
{
	register char *cp = cmd;
	char *estr;

	switch (*cp++) {
	case SC_HOSTNAME:	/* Set hostname */
		/*
		 * Only use info if we don't know who this is.
		 */
		if (!fromhost) {
			fromhost = strdup(cp);
			message(MT_SYSLOG, "startup for %s",  fromhost);
#if defined(SETARGS)
			setproctitle("serving %s", cp);
#endif /* SETARGS */
		}
		break;

	case SC_FREESPACE: 	/* Minimium free space */
		if (!isdigit(*cp)) {
			fatalerr("Expected digit, got '%s'.", cp);
			return;
		}
		min_freespace = (unsigned long) atoi(cp);
		break;

	case SC_FREEFILES: 	/* Minimium free files */
		if (!isdigit(*cp)) {
			fatalerr("Expected digit, got '%s'.", cp);
			return;
		}
		min_freefiles = (unsigned long) atoi(cp);
		break;

	case SC_LOGGING:	/* Logging options */
		if (estr = msgparseopts(cp, TRUE)) {
			fatalerr("Bad message option string (%s): %s", 
				 cp, estr);
			return;
		}
		break;

	default:
		message(MT_NOTICE, "Unknown config command \"%s\".", cp-1);
		return;
	}
}

/*
 * Receive something
 */
static void recvit(cmd, type)
	char *cmd;
	int type;
{
	int mode;
	opt_t opts;
	off_t size;
	time_t mtime, atime;
	char *owner, *group, *file;
	char new[MAXPATHLEN];
	long freespace = -1, freefiles = -1;
	char *cp = cmd;

	/*
	 * Get rdist option flags
	 */
	opts = strtol(cp, &cp, 8);
	if (*cp++ != ' ') {
		error("recvit: options not delimited");
		return;
	}

	/*
	 * Get file mode
	 */
	mode = strtol(cp, &cp, 8);
	if (*cp++ != ' ') {
		error("recvit: mode not delimited");
		return;
	}

	/*
	 * Get file size
	 */
	size = strtol(cp, &cp, 10);
	if (*cp++ != ' ') {
		error("recvit: size not delimited");
		return;
	}

	/*
	 * Get modification time
	 */
	mtime = strtol(cp, &cp, 10);
	if (*cp++ != ' ') {
		error("recvit: mtime not delimited");
		return;
	}

	/*
	 * Get access time
	 */
	atime = strtol(cp, &cp, 10);
	if (*cp++ != ' ') {
		error("recvit: atime not delimited");
		return;
	}

	/*
	 * Get file owner name
	 */
	owner = strtok(cp, " ");
	if (owner == NULL) {
		error("recvit: owner name not delimited");
		return;
	}

	/*
	 * Get file group name
	 */
	group = strtok((char *)NULL, " ");
	if (group == NULL) {
		error("recvit: group name not delimited");
		return;
	}

	/*
	 * Get file name.  Can't use strtok() since there could
	 * be white space in the file name.
	 */
	file = group + strlen(group) + 1;
	if (file == NULL) {
		error("recvit: no file name");
		return;
	}

	debugmsg(DM_MISC,
		 "recvit: opts = %04o mode = %04o size = %d mtime = %d",
		 opts, mode, size, mtime);
	debugmsg(DM_MISC,
       "recvit: owner = '%s' group = '%s' file = '%s' catname = %d isdir = %d",
		 owner, group, file, catname, (type == S_IFDIR) ? 1 : 0);

	if (type == S_IFDIR) {
		if (catname >= sizeof(sptarget)) {
			error("%s: too many directory levels", target);
			return;
		}
		sptarget[catname] = ptarget;
		if (catname++) {
			*ptarget++ = '/';
			while (*ptarget++ = *file++)
			    ;
			ptarget--;
		}
	} else {
		/*
		 * Create name of temporary file
		 */
		if (catname && cattarget(file) < 0) {
			error("Cannot set file name.");
			return;
		}
		file = strrchr(target, '/');
		if (file == NULL)
			(void) strcpy(new, tempname);
		else if (file == target)
			(void) sprintf(new, "/%s", tempname);
		else {
			*file = CNULL;
			(void) sprintf(new, "%s/%s", target, tempname);
			*file = '/';
		}
		(void) mktemp(new);
	}

	/*
	 * Check to see if there is enough free space and inodes
	 * to install this file.
	 */
	if (min_freespace || min_freefiles) {
		/* Convert file size to kilobytes */
		long fsize = (long) (size / 1024);

		if (getfilesysinfo(target, &freespace, &freefiles) != 0)
			return;

		/*
		 * filesystem values < 0 indicate unsupported or unavailable
		 * information.
		 */
		if (min_freespace && (freespace >= 0) && 
		    (freespace - fsize < min_freespace)) {
			error(
		     "%s: Not enough free space on filesystem: min %d free %d",
			      target, min_freespace, freespace);
			return;
		}
		if (min_freefiles && (freefiles >= 0) &&
		    (freefiles - 1 < min_freefiles)) {
			error(
		     "%s: Not enough free files on filesystem: min %d free %d",
			      target, min_freefiles, freefiles);
			return;
		}
	}

	/*
	 * Call appropriate receive function to receive file
	 */
	switch (type) {
	case S_IFDIR:
		recvdir(opts, mode, owner, group);
		break;

	case S_IFLNK:
		recvlink(new, opts, mode, size);
		break;

	case S_IFREG:
		recvfile(new, opts, mode, owner, group, mtime, atime, size);
		break;

	default:
		error("%d: unknown file type", type);
		break;
	}
}

/*
 * Set target information
 */
static void settarget(cmd, isdir)
	char *cmd;
	int isdir;
{
	char *cp = cmd;
	opt_t opts;

	catname = isdir;

	/*
	 * Parse options for this target
	 */
	opts = strtol(cp, &cp, 8);
	if (*cp++ != ' ') {
		error("settarget: options not delimited");
		return;
	}
	options = opts;

	/*
	 * Handle target
	 */
	if (exptilde(target, cp) == NULL)
		return;
	ptarget = target;
	while (*ptarget)
		ptarget++;

	ack();
}

/*
 * Cleanup in preparation for exiting.
 */
extern void cleanup()
{
	/* We don't need to do anything */
}

/*
 * Server routine to read requests and process them.
 */
extern void server()
{
	static char cmdbuf[BUFSIZ];
	register char *cp;
	register int n;
	extern jmp_buf finish_jmpbuf;

	if (setjmp(finish_jmpbuf)) {
		setjmp_ok = FALSE;
		return;
	}
        setjmp_ok = TRUE;
	(void) signal(SIGHUP, sighandler);
	(void) signal(SIGINT, sighandler);
	(void) signal(SIGQUIT, sighandler);
	(void) signal(SIGTERM, sighandler);
	(void) signal(SIGPIPE, sighandler);
	(void) umask(oumask = umask(0));
	(void) strcpy(tempname, _RDIST_TMP);
	if (fromhost) {
		message(MT_SYSLOG, "Startup for %s", fromhost);
#if 	defined(SETARGS)
		setproctitle("Serving %s", fromhost);
#endif	/* SETARGS */
	}

	/* 
	 * Let client know we want it to send it's version number
	 */
	(void) sendcmd(S_VERSION, NULL);

	if (remline(cmdbuf, sizeof(cmdbuf), TRUE) < 0) {
		setjmp_ok = FALSE;
		error("server: expected control record");
		return;
	}

	if (cmdbuf[0] != S_VERSION || !isdigit(cmdbuf[1])) {
		setjmp_ok = FALSE;
		error("Expected version command, received: \"%s\".", cmdbuf);
		return;
	}

	proto_version = atoi(&cmdbuf[1]);
	if (proto_version != VERSION) {
		setjmp_ok = FALSE;
		error("Protocol version %d is not supported.", proto_version);
		return;
	}

	/* Version number is okay */
	ack();

	/*
	 * Main command loop
	 */
	for ( ; ; ) {
		n = remline(cp = cmdbuf, sizeof(cmdbuf), TRUE);
		if (n == -1) {		/* EOF */
			setjmp_ok = FALSE;
			return;
		}
		if (n == 0) {
			error("server: expected control record");
			continue;
		}

		switch (*cp++) {
		case C_SETCONFIG:  	/* Configuration info */
		        setconfig(cp);
			ack();
			continue;

		case C_DIRTARGET:  	/* init target file/directory name */
			settarget(cp, TRUE);
			continue;

		case C_TARGET:  	/* init target file/directory name */
			settarget(cp, FALSE);
			continue;

		case C_RECVREG:  	/* Transfer a regular file. */
			recvit(cp, S_IFREG);
			continue;

		case C_RECVDIR:  	/* Transfer a directory. */
			recvit(cp, S_IFDIR);
			continue;

		case C_RECVSYMLINK:  	/* Transfer symbolic link. */
			recvit(cp, S_IFLNK);
			continue;

		case C_RECVHARDLINK:  	/* Transfer hard link. */
			hardlink(cp);
			continue;

		case C_END:  		/* End of transfer */
			*ptarget = CNULL;
			if (catname <= 0) {
				error("server: too many '%c's", C_END);
				continue;
			}
			ptarget = sptarget[--catname];
			*ptarget = CNULL;
			ack();
			continue;

		case C_CLEAN:  		/* Clean. Cleanup a directory */
			clean(cp);
			continue;

		case C_QUERY:  		/* Query file/directory */
			query(cp);
			continue;

		case C_SPECIAL:  	/* Special. Execute commands */
			dospecial(cp);
			continue;

		case C_CMDSPECIAL:  	/* Cmd Special. Execute commands */
			docmdspecial();
			continue;

#ifdef DOCHMOD
	        case C_CHMOD:  		/* Set mode */
			dochmod(cp);
			continue;
#endif /* DOCHMOD */

		case C_ERRMSG:		/* Normal error message */
			if (cp && *cp)
				message(MT_NERROR|MT_NOREMOTE, "%s", cp);
			continue;

		case C_FERRMSG:		/* Fatal error message */
			if (cp && *cp)
				message(MT_FERROR|MT_NOREMOTE, "%s", cp);
			setjmp_ok = FALSE;
			return;

		default:
			error("server: unknown command '%s'", cp - 1);
		case CNULL:
			continue;
		}
	}
}
