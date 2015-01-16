/*	$OpenBSD: server.c,v 1.34 2015/01/16 06:40:11 deraadt Exp $	*/

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

/*
 * Server routines
 */

char	tempname[sizeof _RDIST_TMP + 1]; /* Tmp file name */
char	buf[BUFSIZ];		/* general purpose buffer */
char	target[PATH_MAX];	/* target/source directory name */
char	*ptarget;		/* pointer to end of target name */
int	catname = 0;		/* cat name to target name */
char	*sptarget[32];		/* stack of saved ptarget's for directories */
char   *fromhost = NULL;	/* Client hostname */
static int64_t min_freespace = 0; /* Minimium free space on a filesystem */
static int64_t min_freefiles = 0; /* Minimium free # files on a filesystem */
int	oumask;			/* Old umask */

static int cattarget(char *);
static int setownership(char *, int, uid_t, gid_t, int);
static int setfilemode(char *, int, int, int);
static int fchog(int, char *, char *, char *, int);
static int removefile(struct stat *, int);
static void doclean(char *);
static void clean(char *);
static void dospecial(char *);
static void docmdspecial(void);
static void query(char *);
static int chkparent(char *, opt_t);
static char *savetarget(char *, opt_t);
static void recvfile(char *, opt_t, int, char *, char *, time_t, time_t, off_t);
static void recvdir(opt_t, int, char *, char *);
static void recvlink(char *, opt_t, int, off_t);
static void hardlink(char *);
static void setconfig(char *);
static void recvit(char *, int);
static void dochmog(char *);
static void settarget(char *, int);

/*
 * Cat "string" onto the target buffer with error checking.
 */
static int
cattarget(char *string)
{
	if (strlen(string) + strlen(target) + 2 > sizeof(target)) {
		message(MT_INFO, "target buffer is not large enough.");
		return(-1);
	}
	if (!ptarget) {
		message(MT_INFO, "NULL target pointer set.");
		return(-10);
	}

	(void) snprintf(ptarget, sizeof(target) - (ptarget - target),
			"/%s", string);

	return(0);
}
	
/*
 * Set uid and gid ownership of a file.
 */
static int
setownership(char *file, int fd, uid_t uid, gid_t gid, int islink)
{
	int status = -1;

	/*
	 * We assume only the Superuser can change uid ownership.
	 */
	if (getuid() != 0) 
		uid = -1;

	if (islink)
		status = lchown(file, uid, gid);

	if (fd != -1 && !islink)
		status = fchown(fd, uid, gid);

	if (status < 0 && !islink)
		status = chown(file, uid, gid);

	if (status < 0) {
		if (uid == (uid_t)-1)
			message(MT_NOTICE, "%s: chgrp %d failed: %s",
				target, gid, SYSERR);
		else
			message(MT_NOTICE, "%s: chown %d.%d failed: %s", 
				target, uid, gid, SYSERR);
		return(-1);
	}

	return(0);
}

/*
 * Set mode of a file
 */
static int
setfilemode(char *file, int fd, int mode, int islink)
{
	int status = -1;

	if (mode == -1)
		return(0);

	if (islink)
		status = fchmodat(AT_FDCWD, file, mode, AT_SYMLINK_NOFOLLOW);

	if (fd != -1 && !islink)
		status = fchmod(fd, mode);

	if (status < 0 && !islink)
		status = chmod(file, mode);

	if (status < 0) {
		message(MT_NOTICE, "%s: chmod failed: %s", target, SYSERR);
		return(-1);
	}

	return(0);
}
/*
 * Change owner, group and mode of file.
 */
static int
fchog(int fd, char *file, char *owner, char *group, int mode)
{
	static struct group *gr = NULL;
	extern char *locuser;
	int i;
	struct stat st;
	uid_t uid;
	gid_t gid;
	gid_t primegid = (gid_t)-2;

	uid = userid;
	if (userid == 0) {	/* running as root; take anything */
		if (*owner == ':') {
			uid = (uid_t) atoi(owner + 1);
		} else if (pw == NULL || strcmp(owner, pw->pw_name) != 0) {
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
			} else
				uid = pw->pw_uid;
		} else {
			uid = pw->pw_uid;
			primegid = pw->pw_gid;
		}
		if (*group == ':') {
			gid = (gid_t)atoi(group + 1);
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

	gid = (gid_t)-1;
	if (gr == NULL || strcmp(group, gr->gr_name) != 0) {
		if ((*group == ':' && 
		     (getgrgid(gid = atoi(group + 1)) == NULL))
		    || ((gr = (struct group *)getgrnam(group)) == NULL)) {
			if (mode != -1 && IS_ON(mode, S_ISGID)) {
				message(MT_NOTICE, 
				"%s: unknown group \"%s\", clearing setgid",
					target, group);
				mode &= ~S_ISGID;
			} else
				message(MT_NOTICE, 
					"%s: unknown group \"%s\"",
					target, group);
		} else
			gid = gr->gr_gid;
	} else
		gid = gr->gr_gid;

	if (userid && gid >= 0 && gid != primegid) {
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
		gid = (gid_t)-1;
	}
ok:
	if (stat(file, &st) == -1) {
		error("%s: Stat failed %s", file, SYSERR);
		return -1;
	}
	/*
	 * Set uid and gid ownership.  If that fails, strip setuid and
	 * setgid bits from mode.  Once ownership is set, successful
	 * or otherwise, set the new file mode.
	 */
	if (setownership(file, fd, uid, gid, S_ISLNK(st.st_mode)) < 0) {
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
	(void) setfilemode(file, fd, mode, S_ISLNK(st.st_mode));


	return(0);
}

/*
 * Remove a file or directory (recursively) and send back an acknowledge
 * or an error message.
 */
static int
removefile(struct stat *statb, int silent)
{
	DIR *d;
	static struct dirent *dp;
	char *cp;
	struct stat stb;
	char *optarget;
	int len, failures = 0;

	switch (statb->st_mode & S_IFMT) {
	case S_IFREG:
	case S_IFLNK:
	case S_IFCHR:
	case S_IFBLK:
	case S_IFSOCK:
	case S_IFIFO:
		if (unlink(target) < 0) {
			if (errno == ETXTBSY) {
				if (!silent)
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
	while ((dp = readdir(d)) != NULL) {
		if (dp->d_name[0] == '.' && (dp->d_name[1] == '\0' ||
		    (dp->d_name[1] == '.' && dp->d_name[2] == '\0')))
			continue;

		if (len + 1 + (int)strlen(dp->d_name) >= PATH_MAX - 1) {
			if (!silent)
				message(MT_REMOTE|MT_WARNING, 
					"%s/%s: Name too long", 
					target, dp->d_name);
			continue;
		}
		ptarget = optarget;
		*ptarget++ = '/';
		cp = dp->d_name;
		while ((*ptarget++ = *cp++) != '\0')
			continue;
		ptarget--;
		if (lstat(target, &stb) < 0) {
			if (!silent)
				message(MT_REMOTE|MT_WARNING,
					"%s: lstat failed: %s", 
					target, SYSERR);
			continue;
		}
		if (removefile(&stb, 0) < 0)
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
#if NEWWAY
	if (!silent)
		message(MT_CHANGE|MT_REMOTE, "%s: removed", target);
#else
	/*
	 * We use MT_NOTICE instead of MT_CHANGE because this function is
	 * sometimes called by other functions that are suppose to return a
	 * single ack() back to the client (rdist).  This is a kludge until
	 * the Rdist protocol is re-done.  Sigh.
	 */
	message(MT_NOTICE|MT_REMOTE, "%s: removed", target);
#endif
	return(0);
}

/*
 * Check the current directory (initialized by the 'T' command to server())
 * for extraneous files and remove them.
 */
static void
doclean(char *cp)
{
	DIR *d;
	struct dirent *dp;
	struct stat stb;
	char *optarget, *ep;
	int len;
	opt_t opts;
	char targ[PATH_MAX*4];

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
	while ((dp = readdir(d)) != NULL) {
		if (dp->d_name[0] == '.' && (dp->d_name[1] == '\0' ||
		    (dp->d_name[1] == '.' && dp->d_name[2] == '\0')))
			continue;

		if (len + 1 + (int)strlen(dp->d_name) >= PATH_MAX - 1) {
			message(MT_REMOTE|MT_WARNING, "%s/%s: Name too long", 
				target, dp->d_name);
			continue;
		}
		ptarget = optarget;
		*ptarget++ = '/';
		cp = dp->d_name;
		while ((*ptarget++ = *cp++) != '\0')
			continue;
		ptarget--;
		if (lstat(target, &stb) < 0) {
			message(MT_REMOTE|MT_WARNING, "%s: lstat failed: %s", 
				target, SYSERR);
			continue;
		}

		ENCODE(targ, dp->d_name);
		(void) sendcmd(CC_QUERY, "%s", targ);
		(void) remline(cp = buf, sizeof(buf), TRUE);

		if (*cp != CC_YES)
			continue;

		if (IS_ON(opts, DO_VERIFY))
			message(MT_REMOTE|MT_INFO, "%s: need to remove", 
				target);
		else
			(void) removefile(&stb, 0);
	}
	(void) closedir(d);

	ptarget = optarget;
	*ptarget = CNULL;
}

/*
 * Frontend to doclean().
 */
static void
clean(char *cp)
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
static void
dospecial(char *xcmd)
{
	char cmd[BUFSIZ];
	if (DECODE(cmd, xcmd) == -1) {
		error("dospecial: Cannot decode command.");
		return;
	}
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
static void
docmdspecial(void)
{
	char *cp;
	char *cmd, *env = NULL;
	int n;
	size_t len;

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
				(void) snprintf(env, len, "export %s;%s=%s", 
					       E_FILES, E_FILES, cp);
			} else {
				len = strlen(env) + 1 + strlen(cp) + 1;
				env = (char *) xrealloc(env, len);
				(void) strlcat(env, ":", len);
				(void) strlcat(env, cp, len);
			}
			ack();
			break;

		case RC_COMMAND:
			if (env) {
				len = strlen(env) + 1 + strlen(cp) + 1;
				env = (char *) xrealloc(env, len);
				(void) strlcat(env, ";", len);
				(void) strlcat(env, cp, len);
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
static void
query(char *xname)
{
	static struct stat stb;
	int s = -1, stbvalid = 0;
	char name[PATH_MAX];

	if (DECODE(name, xname) == -1) {
		error("query: Cannot decode filename");
		return;
	}

	if (catname && cattarget(name) < 0)
		return;

#if	defined(NFS_CHECK)
	if (IS_ON(options, DO_CHKNFS)) {
		s = is_nfs_mounted(target, &stb, &stbvalid);
		if (s > 0)
			(void) sendcmd(QC_ONNFS, NULL);

		/* Either the above check was true or an error occurred */
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

		/* Either the above check was true or an error occurred */
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
		(void) sendcmd(QC_YES, "%lld %lld %o %s %s",
			       (long long) stb.st_size,
			       (long long) stb.st_mtime,
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
static int
chkparent(char *name, opt_t opts)
{
	char *cp;
	struct stat stb;
	int r = -1;

	debugmsg(DM_CALL, "chkparent(%s, %lo) start\n", name, opts);

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
					 "chkparent(%s, %lo) mkdir fail: %s\n",
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
static char *
savetarget(char *file, opt_t opts)
{
	static char savefile[PATH_MAX];

	if (strlen(file) + sizeof(SAVE_SUFFIX) + 1 > PATH_MAX) {
		error("%s: Cannot save: Save name too long", file);
		return(NULL);
	}

	if (IS_ON(opts, DO_HISTORY)) {
		int i;
		struct stat st;
		/*
		 * There is a race here, but the worst that can happen
		 * is to lose a version of the file
		 */
		for (i = 1; i < 1000; i++) {
			(void) snprintf(savefile, sizeof(savefile),
					"%s;%.3d", file, i);
			if (stat(savefile, &st) == -1 && errno == ENOENT)
				break;

		}
		if (i == 1000) {
			message(MT_NOTICE, 
			    "%s: More than 1000 versions for %s; reusing 1\n",
				savefile, SYSERR);
			i = 1;
			(void) snprintf(savefile, sizeof(savefile),
					"%s;%.3d", file, i);
		}
	}
	else {
		(void) snprintf(savefile, sizeof(savefile), "%s%s",
				file, SAVE_SUFFIX);

		if (unlink(savefile) != 0 && errno != ENOENT) {
			message(MT_NOTICE, "%s: remove failed: %s",
				savefile, SYSERR);
			return(NULL);
		}
	}

	if (rename(file, savefile) != 0 && errno != ENOENT) {
		error("%s -> %s: rename failed: %s", 
		      file, savefile, SYSERR);
		return(NULL);
	}

	return(savefile);
}

/*
 * Receive a file
 */
static void
recvfile(char *new, opt_t opts, int mode, char *owner, char *group,
	 time_t mtime, time_t atime, off_t size)
{
	int f, wrerr, olderrno;
	off_t i;
	char *cp;
	char *savefile = NULL;
	static struct stat statbuff;

	/*
	 * Create temporary file
	 */
	if ((f = mkstemp(new)) < 0) {
		if (errno != ENOENT || chkparent(new, opts) < 0 ||
		    (f = mkstemp(new)) < 0) {
			error("%s: create failed: %s", new, SYSERR);
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
		off_t amt = BUFSIZ;

		cp = buf;
		if (i + amt > size)
			amt = size - i;
		do {
			ssize_t j;

			j = readrem(cp, amt);
			if (j <= 0) {
				(void) close(f);
				(void) unlink(new);
				fatalerr(
				   "Read error occurred while receiving file.");
				finish();
			}
			amt -= j;
			cp += j;
		} while (amt > 0);
		amt = BUFSIZ;
		if (i + amt > size)
			amt = size - i;
		if (wrerr == 0 && xwrite(f, buf, amt) != amt) {
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
			(void) fclose(f1);
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
	if (setfiletime(new, time(NULL), mtime) < 0)
		message(MT_NOTICE, "%s: utimes failed: %s", new, SYSERR);

	/*
	 * Try to save target file from being over-written
	 */
	if (IS_ON(opts, DO_SAVETARGETS))
		if ((savefile = savetarget(target, opts)) == NULL) {
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
		removefile(&statbuff, 0);
		ptarget = saveptr;
	}

	/*
	 * Install new (temporary) file as the actual target
	 */
	if (rename(new, target) < 0) {
		static const char fmt[] = "%s -> %s: rename failed: %s";
		struct stat stb;
		/*
		 * If the rename failed due to "Text file busy", then
		 * try to rename the target file and retry the rename.
		 */
		switch (errno) {
		case ETXTBSY:
			/* Save the target */
			if ((savefile = savetarget(target, opts)) != NULL) {
				/* Retry installing new file as target */
				if (rename(new, target) < 0) {
					error(fmt, new, target, SYSERR);
					/* Try to put back save file */
					if (rename(savefile, target) < 0)
						error(fmt,
						      savefile, target, SYSERR);
					(void) unlink(new);
				} else
					message(MT_NOTICE, "%s: renamed to %s",
						target, savefile);
				/*
				 * XXX: We should remove the savefile here.
				 *	But we are nice to nfs clients and
				 *	we keep it.
				 */
			}
			break;
		case EISDIR:
			/*
			 * See if target is a directory and remove it if it is
			 */
			if (lstat(target, &stb) == 0) {
				if (S_ISDIR(stb.st_mode)) {
					char *optarget = ptarget;
					for (ptarget = target; *ptarget;
						ptarget++);
					/* If we failed to remove, we'll catch
					   it later */
					(void) removefile(&stb, 1);
					ptarget = optarget;
				}
			}
			if (rename(new, target) >= 0)
				break;
			/*FALLTHROUGH*/

		default:
			error(fmt, new, target, SYSERR);
			(void) unlink(new);
			break;
		}
	}

	if (IS_ON(opts, DO_COMPARE))
		message(MT_REMOTE|MT_CHANGE, "%s: updated", target);
	else
		ack();
}

/*
 * Receive a directory
 */
static void
recvdir(opt_t opts, int mode, char *owner, char *group)
{
	static char lowner[100], lgroup[100];
	char *cp;
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
				if ((cp = getusername(stb.st_uid, target, o))
				    != NULL)
					if (strcmp(owner, cp))
						(void) strlcpy(lowner, cp,
						    sizeof(lowner));
			}
			if (!IS_ON(opts, DO_NOCHKGROUP) && group) {
				int o;

				o = (group[0] == ':') ? opts & DO_NUMCHKGROUP :
					opts;
				if ((cp = getgroupname(stb.st_gid, target, o))
				    != NULL)
					if (strcmp(group, cp))
						(void) strlcpy(lgroup, cp,
						    sizeof(lgroup));
			}

			/*
			 * Need to set owner and/or group
			 */
#define PRN(n) ((n[0] == ':') ? n+1 : n)
			if (lowner[0] != CNULL || lgroup[0] != CNULL) {
				if (lowner[0] == CNULL && 
				    (cp = getusername(stb.st_uid, 
						      target, opts)))
					(void) strlcpy(lowner, cp,
					    sizeof(lowner));
				if (lgroup[0] == CNULL && 
				    (cp = getgroupname(stb.st_gid, 
						       target, opts)))
					(void) strlcpy(lgroup, cp,
					    sizeof(lgroup));

				if (IS_ON(opts, DO_VERIFY))
					message(MT_NOTICE,
				"%s: need to chown from %s:%s to %s:%s",
						target, 
						PRN(lowner), PRN(lgroup),
						PRN(owner), PRN(group));
				else {
					if (fchog(-1, target, owner, 
						  group, -1) == 0)
						message(MT_NOTICE,
					       "%s: chown from %s:%s to %s:%s",
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
			    (chkparent(target, opts) == 0 && 
			    mkdir(target, mode) == 0)) {
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
static void
recvlink(char *new, opt_t opts, int mode, off_t size)
{
	char tbuf[PATH_MAX], dbuf[BUFSIZ];
	struct stat stb;
	char *optarget;
	int uptodate;
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

	if (DECODE(dbuf, buf) == -1) {
		error("recvlink: cannot decode symlink target");
		return;
	}

	uptodate = 0;
	if ((i = readlink(target, tbuf, sizeof(tbuf)-1)) != -1) {
		tbuf[i] = '\0';
		if (i == size && strncmp(dbuf, tbuf, (int) size) == 0)
			uptodate = 1;
	}
	mode &= 0777;

	if (IS_ON(opts, DO_VERIFY) || uptodate) {
		if (uptodate)
			message(MT_REMOTE|MT_INFO, "");
		else
			message(MT_REMOTE|MT_INFO, "%s: need to update",
				target);
		if (IS_ON(opts, DO_COMPARE))
			return;
		(void) sendcmd(C_END, NULL);
		(void) response();
		return;
	}

	/*
	 * Make new symlink using a temporary name
	 */
	if (mktemp(new) == NULL || symlink(dbuf, new) < 0) {
		if (errno != ENOENT || chkparent(new, opts) < 0 ||
		    mktemp(new) == NULL || symlink(dbuf, new) < 0) {
			error("%s -> %s: symlink failed: %s", new, dbuf,
			    SYSERR);
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
			if (removefile(&stb, 0) < 0) {
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

	message(MT_REMOTE|MT_CHANGE, "%s: updated", target);

	/*
	 * Indicate end of receive operation
	 */
	(void) sendcmd(C_END, NULL);
	(void) response();
}

/*
 * Creat a hard link to existing file.
 */
static void
hardlink(char *cmd)
{
	struct stat stb;
	int exists = 0;
	char *xoldname, *xnewname;
	char *cp = cmd;
	static char expbuf[BUFSIZ];
	char oldname[BUFSIZ], newname[BUFSIZ];

	/* Skip over opts */
	(void) strtol(cp, &cp, 8);
	if (*cp++ != ' ') {
		error("hardlink: options not delimited");
		return;
	}

	xoldname = strtok(cp, " ");
	if (xoldname == NULL) {
		error("hardlink: oldname name not delimited");
		return;
	}

	if (DECODE(oldname, xoldname) == -1) {
		error("hardlink: Cannot decode oldname");
		return;
	}

	xnewname = strtok(NULL, " ");
	if (xnewname == NULL) {
		error("hardlink: new name not specified");
		return;
	}

	if (DECODE(newname, xnewname) == -1) {
		error("hardlink: Cannot decode newname");
		return;
	}

	if (exptilde(expbuf, oldname, sizeof(expbuf)) == NULL) {
		error("hardlink: tilde expansion failed");
		return;
	}

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
	if (link(expbuf, target) < 0) {
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
static void
setconfig(char *cmd)
{
	char *cp = cmd;
	char *estr;
	const char *errstr;

	switch (*cp++) {
	case SC_HOSTNAME:	/* Set hostname */
		/*
		 * Only use info if we don't know who this is.
		 */
		if (!fromhost) {
			fromhost = xstrdup(cp);
			message(MT_SYSLOG, "startup for %s", fromhost);
			setproctitle("serving %s", cp);
		}
		break;

	case SC_FREESPACE: 	/* Minimium free space */
		min_freespace = (int64_t)strtonum(cp, 0, LLONG_MAX, &errstr);
		if (errstr)
			fatalerr("Minimum free space is %s: '%s'", errstr,
				optarg);
		break;

	case SC_FREEFILES: 	/* Minimium free files */
		min_freefiles = (int64_t)strtonum(cp, 0, LLONG_MAX, &errstr);
		if (errstr)
			fatalerr("Minimum free files is %s: '%s'", errstr,
				optarg);
		break;

	case SC_LOGGING:	/* Logging options */
		if ((estr = msgparseopts(cp, TRUE)) != NULL) {
			fatalerr("Bad message option string (%s): %s", 
				 cp, estr);
			return;
		}
		break;

	case SC_DEFOWNER:
		(void) strlcpy(defowner, cp, sizeof(defowner));
		break;

	case SC_DEFGROUP:
		(void) strlcpy(defgroup, cp, sizeof(defgroup));
		break;

	default:
		message(MT_NOTICE, "Unknown config command \"%s\".", cp-1);
		return;
	}
}

/*
 * Receive something
 */
static void
recvit(char *cmd, int type)
{
	int mode;
	opt_t opts;
	off_t size;
	time_t mtime, atime;
	char *owner, *group, *file;
	char new[PATH_MAX];
	char fileb[PATH_MAX];
	int64_t freespace = -1, freefiles = -1;
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
	size = (off_t) strtoll(cp, &cp, 10);
	if (*cp++ != ' ') {
		error("recvit: size not delimited");
		return;
	}

	/*
	 * Get modification time
	 */
	mtime = (time_t) strtoll(cp, &cp, 10);
	if (*cp++ != ' ') {
		error("recvit: mtime not delimited");
		return;
	}

	/*
	 * Get access time
	 */
	atime = (time_t) strtoll(cp, &cp, 10);
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
	group = strtok(NULL, " ");
	if (group == NULL) {
		error("recvit: group name not delimited");
		return;
	}

	/*
	 * Get file name. Can't use strtok() since there could
	 * be white space in the file name.
	 */
	if (DECODE(fileb, group + strlen(group) + 1) == -1) {
		error("recvit: Cannot decode file name");
		return;
	}

	if (fileb[0] == '\0') {
		error("recvit: no file name");
		return;
	}
	file = fileb;

	debugmsg(DM_MISC,
		 "recvit: opts = %04lo mode = %04o size = %lld mtime = %lld",
		 opts, mode, (long long) size, (long long)mtime);
	debugmsg(DM_MISC,
       "recvit: owner = '%s' group = '%s' file = '%s' catname = %d isdir = %d",
		 owner, group, file, catname, (type == S_IFDIR) ? 1 : 0);

	if (type == S_IFDIR) {
		if ((size_t) catname >= sizeof(sptarget)) {
			error("%s: too many directory levels", target);
			return;
		}
		sptarget[catname] = ptarget;
		if (catname++) {
			*ptarget++ = '/';
			while ((*ptarget++ = *file++) != '\0')
			    continue;
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
			(void) strlcpy(new, tempname, sizeof(new));
		else if (file == target)
			(void) snprintf(new, sizeof(new), "/%s", tempname);
		else {
			*file = CNULL;
			(void) snprintf(new, sizeof(new), "%s/%s", target,
					tempname);
			*file = '/';
		}
	}

	/*
	 * Check to see if there is enough free space and inodes
	 * to install this file.
	 */
	if (min_freespace || min_freefiles) {
		/* Convert file size to kilobytes */
		int64_t fsize = (int64_t)size / 1024;

		if (getfilesysinfo(target, &freespace, &freefiles) != 0)
			return;

		/*
		 * filesystem values < 0 indicate unsupported or unavailable
		 * information.
		 */
		if (min_freespace && (freespace >= 0) && 
		    (freespace - fsize < min_freespace)) {
			error(
		     "%s: Not enough free space on filesystem: min %lld "
		     "free %lld", target, min_freespace, freespace);
			return;
		}
		if (min_freefiles && (freefiles >= 0) &&
		    (freefiles - 1 < min_freefiles)) {
			error(
		     "%s: Not enough free files on filesystem: min %lld free "
		     "%lld", target, min_freefiles, freefiles);
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
 * Chmog something
 */
static void
dochmog(char *cmd)
{
	int mode;
	opt_t opts;
	char *owner, *group, *file;
	char *cp = cmd;
	char fileb[PATH_MAX];

	/*
	 * Get rdist option flags
	 */
	opts = strtol(cp, &cp, 8);
	if (*cp++ != ' ') {
		error("dochmog: options not delimited");
		return;
	}

	/*
	 * Get file mode
	 */
	mode = strtol(cp, &cp, 8);
	if (*cp++ != ' ') {
		error("dochmog: mode not delimited");
		return;
	}

	/*
	 * Get file owner name
	 */
	owner = strtok(cp, " ");
	if (owner == NULL) {
		error("dochmog: owner name not delimited");
		return;
	}

	/*
	 * Get file group name
	 */
	group = strtok(NULL, " ");
	if (group == NULL) {
		error("dochmog: group name not delimited");
		return;
	}

	/*
	 * Get file name. Can't use strtok() since there could
	 * be white space in the file name.
	 */
	if (DECODE(fileb, group + strlen(group) + 1) == -1) {
		error("dochmog: Cannot decode file name");
		return;
	}

	if (fileb[0] == '\0') {
		error("dochmog: no file name");
		return;
	}
	file = fileb;

	debugmsg(DM_MISC,
		 "dochmog: opts = %04lo mode = %04o", opts, mode);
	debugmsg(DM_MISC,
	         "dochmog: owner = '%s' group = '%s' file = '%s' catname = %d",
		 owner, group, file, catname);

	if (catname && cattarget(file) < 0) {
		error("Cannot set newname target.");
		return;
	}

	(void) fchog(-1, target, owner, group, mode);

	ack();
}

/*
 * Set target information
 */
static void
settarget(char *cmd, int isdir)
{
	char *cp = cmd;
	opt_t opts;
	char file[BUFSIZ];

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

	if (DECODE(file, cp) == -1) {
		error("settarget: Cannot decode target name");
		return;
	}

	/*
	 * Handle target
	 */
	if (exptilde(target, cp, sizeof(target)) == NULL)
		return;
	ptarget = target;
	while (*ptarget)
		ptarget++;

	ack();
}

/*
 * Cleanup in preparation for exiting.
 */
void
cleanup(int dummy)
{
	/* We don't need to do anything */
}

/*
 * Server routine to read requests and process them.
 */
void
server(void)
{
	static char cmdbuf[BUFSIZ];
	char *cp;
	int n;
	extern jmp_buf finish_jmpbuf;

	if (setjmp(finish_jmpbuf))
		return;
	(void) signal(SIGHUP, sighandler);
	(void) signal(SIGINT, sighandler);
	(void) signal(SIGQUIT, sighandler);
	(void) signal(SIGTERM, sighandler);
	(void) signal(SIGPIPE, sighandler);
	(void) umask(oumask = umask(0));
	(void) strlcpy(tempname, _RDIST_TMP, sizeof(tempname));
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
		error("server: expected control record");
		return;
	}

	if (cmdbuf[0] != S_VERSION || !isdigit((unsigned char)cmdbuf[1])) {
		error("Expected version command, received: \"%s\".", cmdbuf);
		return;
	}

	proto_version = atoi(&cmdbuf[1]);
	if (proto_version != VERSION) {
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
		if (n == -1)		/* EOF */
			return;
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

	        case C_CHMOG:  		/* Set owner, group, mode */
			dochmog(cp);
			continue;

		case C_ERRMSG:		/* Normal error message */
			if (cp && *cp)
				message(MT_NERROR|MT_NOREMOTE, "%s", cp);
			continue;

		case C_FERRMSG:		/* Fatal error message */
			if (cp && *cp)
				message(MT_FERROR|MT_NOREMOTE, "%s", cp);
			return;

		default:
			error("server: unknown command '%s'", cp - 1);
		case CNULL:
			continue;
		}
	}
}
