/*	$OpenBSD: pwd_mkdb.c,v 1.18 1998/07/15 00:50:19 millert Exp $	*/

/*-
 * Copyright (c) 1991, 1993, 1994
 *	The Regents of the University of California.  All rights reserved.
 * Portions Copyright (c) 1994, Jason Downs.  All rights reserved.
 * Portions Copyright (c) 1998, Todd C. Miller.  All rights reserved.
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
static char copyright[] =
"@(#) Copyright (c) 1991, 1993, 1994\n\
	The Regents of the University of California.  All rights reserved.\n";
#endif /* not lint */

#ifndef lint
#if 0
static char sccsid[] = "from: @(#)pwd_mkdb.c	8.5 (Berkeley) 4/20/94";
#else
static char *rcsid = "$OpenBSD: pwd_mkdb.c,v 1.18 1998/07/15 00:50:19 millert Exp $";
#endif
#endif /* not lint */

#include <sys/param.h>
#include <sys/stat.h>

#include <db.h>
#include <err.h>
#include <errno.h>
#include <fcntl.h>
#include <limits.h>
#include <pwd.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <util.h>

#define	INSECURE	1
#define	SECURE		2
#define	PERM_INSECURE	(S_IRUSR|S_IWUSR|S_IRGRP|S_IROTH)
#define	PERM_SECURE	(S_IRUSR|S_IWUSR)

HASHINFO openinfo = {
	4096,		/* bsize */
	32,		/* ffactor */
	256,		/* nelem */
	2048 * 1024,	/* cachesize */
	NULL,		/* hash() */
	0		/* lorder */
};

static enum state { FILE_INSECURE, FILE_SECURE, FILE_ORIG } clean;
static struct passwd pwd;			/* password structure */
static char *pname;				/* password file name */
static char *basedir;				/* dir holding master.passwd */
static int hasyp;				/* are we running YP? */

void	cleanup __P((void));
void	error __P((char *));
void	mv __P((char *, char *));
int	scan __P((FILE *, struct passwd *, int *));
void	usage __P((void));
char	*changedir __P((char *path, char *dir));
void	db_store __P((FILE *, FILE *, DB *, struct passwd *, int, int));

int
main(argc, argv)
	int argc;
	char *argv[];
{
	DB *dp, *edp;
	DBT ypdata, ypkey;
	FILE *fp, *oldfp = NULL;
	struct stat st;
	sigset_t set;
	int ch, tfd, makeold, flags = 0, checkonly = 0;
	char buf[MAXPATHLEN];

	makeold = 0;
	while ((ch = getopt(argc, argv, "cpvd:")) != -1)
		switch(ch) {
		case 'c':			/* verify only */
			checkonly = 1;
			break;
		case 'p':			/* create V7 "file.orig" */
			makeold = 1;
			break;
		case 'v':			/* backward compatible */
			break;
		case 'd':
			basedir = optarg;
			if (strlen(basedir) > MAXPATHLEN - 40)
				error("basedir too long");
			break;
		case '?':
		default:
			usage();
		}
	argc -= optind;
	argv += optind;

	if (argc != 1)
		usage();

	/*
	 * This could be changed to allow the user to interrupt.
	 * Probably not worth the effort.
	 */
	sigemptyset(&set);
	sigaddset(&set, SIGTSTP);
	sigaddset(&set, SIGHUP);
	sigaddset(&set, SIGINT);
	sigaddset(&set, SIGQUIT);
	sigaddset(&set, SIGTERM);
	(void)sigprocmask(SIG_BLOCK, &set, (sigset_t *)NULL);

	/* We don't care what the user wants. */
	(void)umask(0);

	pname = strdup(changedir(*argv, basedir));
	/* Open the original password file */
	if (!(fp = fopen(pname, "r")))
		error(pname);

	/* Check only if password database is valid */
	if (checkonly) {
		u_int cnt;

		for (cnt = 1; scan(fp, &pwd, &flags); ++cnt)
			;
		exit(0);
	}

	if (fstat(fileno(fp), &st) == -1)
		error(pname);

	if (st.st_size > (off_t)100*1024) {
		/*
		 * It is a large file. We are going to crank db's cache size.
		 */
		openinfo.cachesize = st.st_size * 20;
		if (openinfo.cachesize > 12*1024*1024)
			openinfo.cachesize = 12*1024*1024;
	}

	/* Open the temporary insecure password database. */
	(void)snprintf(buf, sizeof(buf), "%s.tmp",
	    changedir(_PATH_MP_DB, basedir));
	dp = dbopen(buf,
	    O_RDWR|O_CREAT|O_EXCL, PERM_INSECURE, DB_HASH, &openinfo);
	if (dp == NULL)
		error(buf);
	clean = FILE_INSECURE;

	/*
	 * Open file for old password file.  Minor trickiness -- don't want to
	 * chance the file already existing, since someone (stupidly) might
	 * still be using this for permission checking.  So, open it first and
	 * fdopen the resulting fd.  The resulting file should be readable by
	 * everyone.
	 */
	if (makeold) {
		(void)snprintf(buf, sizeof(buf), "%s.orig", pname);
		if ((tfd = open(buf,
		    O_WRONLY|O_CREAT|O_EXCL, PERM_INSECURE)) < 0)
			error(buf);
		if ((oldfp = fdopen(tfd, "w")) == NULL)
			error(buf);
		clean = FILE_ORIG;
	}

	/*
	 * The databases actually contain three copies of the original data.
	 * Each password file entry is converted into a rough approximation
	 * of a ``struct passwd'', with the strings placed inline.  This
	 * object is then stored as the data for three separate keys.  The
	 * first key * is the pw_name field prepended by the _PW_KEYBYNAME
	 * character.  The second key is the pw_uid field prepended by the
	 * _PW_KEYBYUID character.  The third key is the line number in the
	 * original file prepended by the _PW_KEYBYNUM character.  (The special
	 * characters are prepended to ensure that the keys do not collide.)
	 *
	 * If we see something go by that looks like YP, we save a special
	 * pointer record, which if YP is enabled in the C lib, will speed
	 * things up.
	 */

	/*
	 * Write "insecure" (no passwd) version of the .db file.
	 * We do this three times, one per key type (for getpw{name,uid,ent}).
	 * The first time through we also check for YP, issue warnings
	 * and save the V7 format passwd file if necessary.
	 */
	db_store(fp, makeold ? oldfp : NULL, dp, &pwd, _PW_KEYBYNAME, 1);
	db_store(fp, makeold ? oldfp : NULL, dp, &pwd, _PW_KEYBYUID, 1);
	db_store(fp, makeold ? oldfp : NULL, dp, &pwd, _PW_KEYBYNUM, 1);

	/* Store YP token, if needed. */
	if (hasyp) {
		ypkey.data = (u_char *)_PW_YPTOKEN;
		ypkey.size = strlen(_PW_YPTOKEN);
		ypdata.data = (u_char *)NULL;
		ypdata.size = 0;

		if ((dp->put)(dp, &ypkey, &ypdata, R_NOOVERWRITE) == -1)
			error("put");
	}

	if ((dp->close)(dp))
		error("close dp");
	if (makeold) {
		(void)fflush(oldfp);
		if (fclose(oldfp) == EOF)
			error("close old");
	}

	/* Open the temporary encrypted password database. */
	(void)snprintf(buf, sizeof(buf), "%s.tmp",
	    changedir(_PATH_SMP_DB, basedir));
	edp = dbopen(buf,
	    O_RDWR|O_CREAT|O_EXCL, PERM_SECURE, DB_HASH, &openinfo);
	if (!edp)
		error(buf);
	clean = FILE_SECURE;

	/*
	 * Write "secure" (including passwd) info to the .db file.
	 * We do this three times, one per key type (for getpw{name,uid,ent}).
	 */
	db_store(fp, makeold ? oldfp : NULL, edp, &pwd, _PW_KEYBYNAME, 0);
	db_store(fp, makeold ? oldfp : NULL, edp, &pwd, _PW_KEYBYUID, 0);
	db_store(fp, makeold ? oldfp : NULL, edp, &pwd, _PW_KEYBYNUM, 0);

	/* Store YP token, if needed. */
	if (hasyp) {
		ypkey.data = (u_char *)_PW_YPTOKEN;
		ypkey.size = strlen(_PW_YPTOKEN);
		ypdata.data = (u_char *)NULL;
		ypdata.size = 0;

		if ((edp->put)(edp, &ypkey, &ypdata, R_NOOVERWRITE) == -1)
			error("put");
	}

	if ((edp->close)(edp))
		error("close edp");

	/* Set master.passwd permissions, in case caller forgot. */
	(void)fchmod(fileno(fp), S_IRUSR|S_IWUSR);
	if (fclose(fp) != 0)
		error("fclose");

	/* Install as the real password files. */
	(void)snprintf(buf, sizeof(buf), "%s.tmp",
	    changedir(_PATH_MP_DB, basedir));
	mv(buf, changedir(_PATH_MP_DB, basedir));
	(void)snprintf(buf, sizeof(buf), "%s.tmp",
	    changedir(_PATH_SMP_DB, basedir));
	mv(buf, changedir(_PATH_SMP_DB, basedir));
	if (makeold) {
		(void)snprintf(buf, sizeof(buf), "%s.orig", pname);
		mv(buf, changedir(_PATH_PASSWD, basedir));
	}

	/*
	 * Move the master password LAST -- chpass(1), passwd(1) and vipw(8)
	 * all use flock(2) on it to block other incarnations of themselves.
	 * The rename means that everything is unlocked, as the original file
	 * can no longer be accessed.
	 */
	mv(pname, changedir(_PATH_MASTERPASSWD, basedir));
	exit(0);
}

int
scan(fp, pw, flags)
	FILE *fp;
	struct passwd *pw;
	int *flags;
{
	static int lcnt;
	static char line[LINE_MAX];
	char *p;

	if (fgets(line, sizeof(line), fp) == NULL)
		return (0);
	++lcnt;
	/*
	 * ``... if I swallow anything evil, put your fingers down my
	 * throat...''
	 *	-- The Who
	 */
	p = line;
	if (*p != '\0' && *(p += strlen(line) - 1) != '\n') {
		warnx("line too long");
		goto fmt;
	}
	*p = '\0';
	*flags = 0;
	if (!pw_scan(line, pw, flags)) {
		warnx("at line #%d", lcnt);
fmt:		errno = EFTYPE;	/* XXX */
		error(pname);
	}

	return (1);
}

void
mv(from, to)
	char *from, *to;
{
	char buf[MAXPATHLEN];

	if (rename(from, to)) {
		int sverrno = errno;
		(void)snprintf(buf, sizeof(buf), "%s to %s", from, to);
		errno = sverrno;
		error(buf);
	}
}

void
error(name)
	char *name;
{

	warn(name);
	cleanup();
	exit(1);
}

void
cleanup()
{
	char buf[MAXPATHLEN];

	switch(clean) {
	case FILE_ORIG:
		(void)snprintf(buf, sizeof(buf), "%s.orig", pname);
		(void)unlink(buf);
		/* FALLTHROUGH */
	case FILE_SECURE:
		(void)snprintf(buf, sizeof(buf), "%s.tmp",
		    changedir(_PATH_SMP_DB, basedir));
		(void)unlink(buf);
		/* FALLTHROUGH */
	case FILE_INSECURE:
		(void)snprintf(buf, sizeof(buf), "%s.tmp",
		    changedir(_PATH_MP_DB, basedir));
		(void)unlink(buf);
	}
}

void
usage()
{

	(void)fprintf(stderr, "usage: pwd_mkdb [-cp] [-d basedir] file\n");
	exit(1);
}

char *
changedir(path, dir)
	char *path, *dir;
{
	static char fixed[MAXPATHLEN];
	char *p;

	if (!dir)
		return (path);

	p = strrchr(path, '/');
	strcpy(fixed, dir);
	if (p) {
		strcat(fixed, "/");
		strcat(fixed, p + 1);
	}
	return (fixed);
}

void
db_store(fp, oldfp, dp, pw, keytype, insecure)
	FILE *fp;
	FILE *oldfp;
	DB *dp;
	struct passwd *pw;
	int keytype;
	int insecure;
{
	int flags = 0;
	u_int cnt;
	char *p, *t, buf[LINE_MAX * 2], tbuf[1024];
	DBT data, key;
	size_t len;
	static int firsttime = 1;

	rewind(fp);
	data.data = (u_char *)buf;
	key.data = (u_char *)tbuf;
	for (cnt = 1; scan(fp, pw, &flags); ++cnt) {

		if (firsttime) {
			/* Look like YP? */
			if ((pw->pw_name[0] == '+') || (pw->pw_name[0] == '-'))
				hasyp++;

			/* Warn about potentially unsafe uid/gid overrides. */
			if (pw->pw_name[0] == '+') {
				if (!(flags & _PASSWORD_NOUID) && !pw->pw_uid)
					warnx("line %d: superuser override in "
					    "YP inclusion", cnt);
				if (!(flags & _PASSWORD_NOGID) && !pw->pw_gid)
					warnx("line %d: wheel override in "
					    "YP inclusion", cnt);
			}

			/* Create V7 format password file entry. */
			if (oldfp != NULL)
				if (fprintf(oldfp, "%s:*:%u:%u:%s:%s:%s\n",
				    pw->pw_name, pw->pw_uid, pw->pw_gid,
				    pw->pw_gecos, pw->pw_dir, pw->pw_shell)
				    == EOF)
					error("write old");
		}

		/* Create data (a packed struct passwd). */
#define	COMPACT(e)	t = e; while ((*p++ = *t++));
		p = buf;
		COMPACT(pw->pw_name);
		COMPACT(insecure ? "*" : pw->pw_passwd);
		memmove(p, &pw->pw_uid, sizeof(uid_t));
		p += sizeof(uid_t);
		memmove(p, &pw->pw_gid, sizeof(gid_t));
		p += sizeof(gid_t);
		memmove(p, &pw->pw_change, sizeof(time_t));
		p += sizeof(time_t);
		COMPACT(pw->pw_class);
		COMPACT(pw->pw_gecos);
		COMPACT(pw->pw_dir);
		COMPACT(pw->pw_shell);
		memmove(p, &pw->pw_expire, sizeof(time_t));
		p += sizeof(time_t);
		memmove(p, &flags, sizeof(int));
		p += sizeof(int);
		data.size = p - buf;

		/* Build the key. */
		switch (keytype) {
		case _PW_KEYBYNUM:
			memmove(tbuf + 1, &cnt, sizeof(cnt));
			key.size = sizeof(cnt) + 1;
			break;

		case _PW_KEYBYNAME:
			len = strlen(pw->pw_name);
			memmove(tbuf + 1, pw->pw_name, len);
			key.size = len + 1;
			break;

		case _PW_KEYBYUID:
			memmove(tbuf + 1, &pw->pw_uid, sizeof(pw->pw_uid));
			key.size = sizeof(pw->pw_uid) + 1;
			break;
		}
		tbuf[0] = keytype;

		/* Write the record. */
		if ((dp->put)(dp, &key, &data, R_NOOVERWRITE) == -1)
			error("put");
	}
	if (firsttime)
		firsttime = 0;
}
