/*
 * Copyright (c) 1980, 1990, 1993
 *	The Regents of the University of California.  All rights reserved.
 *
 * This code is derived from software contributed to Berkeley by
 * Robert Elz at The University of Melbourne.
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
"@(#) Copyright (c) 1980, 1990, 1993\n\
	The Regents of the University of California.  All rights reserved.\n";
#endif /* not lint */

#ifndef lint
/*static char sccsid[] = "from: @(#)edquota.c	8.1 (Berkeley) 6/6/93";*/
static char *rcsid = "$Id: edquota.c,v 1.20 1998/07/13 02:11:47 millert Exp $";
#endif /* not lint */

/*
 * Disk quota editor.
 */
#include <sys/param.h>
#include <sys/stat.h>
#include <sys/file.h>
#include <sys/wait.h>
#include <ufs/ufs/quota.h>
#include <err.h>
#include <errno.h>
#include <fstab.h>
#include <pwd.h>
#include <grp.h>
#include <ctype.h>
#include <stdio.h>
#include <signal.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include "pathnames.h"

char *qfname = QUOTAFILENAME;
char *qfextension[] = INITQFNAMES;
char *quotagroup = QUOTAGROUP;
char tmpfil[] = _PATH_TMPFILE;

struct quotause {
	struct	quotause *next;
	long	flags;
	struct	dqblk dqblk;
	char	fsname[MAXPATHLEN];
	char	qfname[1];	/* actually longer */
} *getprivs __P((u_int, int));
#define	FOUND	0x01

void	putprivs __P((long, int, struct quotause *));
void	freeprivs __P((struct quotause *));
int	writetimes __P((struct quotause *, int, int));
int	editit __P((char *));
int	readtimes __P((struct quotause *, int));
int	writeprivs __P((struct quotause *, int, char *, int));
int	alldigits __P((char *s));
int	readprivs __P((struct quotause *, int));
int	hasquota __P((struct fstab *, int, char **));
int	cvtatos __P((time_t, char *, time_t *));
u_int	getentry __P((char *, int));

void
usage()
{
	(void)fprintf(stderr, "%s%s%s%s",
		"Usage: edquota [-u] [-p username] username ...\n",
		"\tedquota -g [-p groupname] groupname ...\n",
		"\tedquota [-u] -t\n", "\tedquota -g -t\n");
	exit(1);
}

int
main(argc, argv)
	register char **argv;
	int argc;
{
	register struct quotause *qup, *protoprivs, *curprivs;
	register u_int id, protoid;
	register int quotatype, tmpfd;
	char *protoname = NULL;
	int ch;
	int tflag = 0, pflag = 0;

	if (argc < 2)
		usage();
	if (getuid())
		errx(1, strerror(EPERM));
	quotatype = USRQUOTA;
	while ((ch = getopt(argc, argv, "ugtp:")) != -1) {
		switch(ch) {
		case 'p':
			protoname = optarg;
			pflag++;
			break;
		case 'g':
			quotatype = GRPQUOTA;
			break;
		case 'u':
			quotatype = USRQUOTA;
			break;
		case 't':
			tflag++;
			break;
		default:
			usage();
		}
	}
	argc -= optind;
	argv += optind;
	if (pflag) {
		if ((protoid = getentry(protoname, quotatype)) == -1)
			exit(1);
		protoprivs = getprivs(protoid, quotatype);
		for (qup = protoprivs; qup; qup = qup->next) {
			qup->dqblk.dqb_btime = 0;
			qup->dqblk.dqb_itime = 0;
		}
		while (argc-- > 0) {
			if ((id = getentry(*argv++, quotatype)) < 0)
				continue;
			putprivs(id, quotatype, protoprivs);
		}
		exit(0);
	}
	if ((tmpfd = mkstemp(tmpfil)) == -1)
		errx(1, tmpfil);
	if (tflag) {
		protoprivs = getprivs(0, quotatype);
		if (writetimes(protoprivs, tmpfd, quotatype) == 0) {
			unlink(tmpfil);
			exit(1);
		}
		if (editit(tmpfil) && readtimes(protoprivs, tmpfd))
			putprivs(0, quotatype, protoprivs);
		freeprivs(protoprivs);
		unlink(tmpfil);
		exit(0);
	}
	for ( ; argc > 0; argc--, argv++) {
		if ((id = getentry(*argv, quotatype)) == -1)
			continue;
		curprivs = getprivs(id, quotatype);
		if (writeprivs(curprivs, tmpfd, *argv, quotatype) == 0)
			continue;
		if (editit(tmpfil) && readprivs(curprivs, tmpfd))
			putprivs(id, quotatype, curprivs);
		freeprivs(curprivs);
	}
	close(tmpfd);
	unlink(tmpfil);
	exit(0);
}

/*
 * This routine converts a name for a particular quota type to
 * an identifier. This routine must agree with the kernel routine
 * getinoquota as to the interpretation of quota types.
 */
u_int
getentry(name, quotatype)
	char *name;
	int quotatype;
{
	struct passwd *pw;
	struct group *gr;
	u_long id;

	switch(quotatype) {
	case USRQUOTA:
		if ((pw = getpwnam(name)))
			return(pw->pw_uid);
		else if (alldigits(name)) {
			if ((id = strtoul(name, NULL, 10)) <= UID_MAX)
				return((uid_t)id);
		}
		warnx("%s: no such user", name);
		break;
	case GRPQUOTA:
		if ((gr = getgrnam(name)))
			return(gr->gr_gid);
		else if (alldigits(name)) {
			if ((id = strtoul(name, NULL, 10)) <= GID_MAX)
				return((gid_t)id);
		}
		warnx("%s: no such group", name);
		break;
	default:
		warnx("%d: unknown quota type", quotatype);
		break;
	}
	sleep(1);
	return(-1);
}

/*
 * Collect the requested quota information.
 */
struct quotause *
getprivs(id, quotatype)
	register u_int id;
	int quotatype;
{
	register struct fstab *fs;
	register struct quotause *qup, *quptail;
	struct quotause *quphead;
	int qcmd, qupsize, fd;
	char *qfpathname;
	static int warned = 0;

	setfsent();
	quphead = (struct quotause *)0;
	qcmd = QCMD(Q_GETQUOTA, quotatype);
	while ((fs = getfsent())) {
		if (strcmp(fs->fs_vfstype, "ffs") &&
		    strcmp(fs->fs_vfstype, "ufs") &&
		    strcmp(fs->fs_vfstype, "mfs"))
			continue;
		if (!hasquota(fs, quotatype, &qfpathname))
			continue;
		qupsize = sizeof(*qup) + strlen(qfpathname);
		if ((qup = (struct quotause *)malloc(qupsize)) == NULL)
			errx(2, "out of memory");
		if (quotactl(fs->fs_file, qcmd, id, (char *)&qup->dqblk) != 0) {
	    		if (errno == EOPNOTSUPP && !warned) {
				warned++;
				(void)fprintf(stderr, "Warning: %s\n",
				    "Quotas are not compiled into this kernel");
				sleep(3);
			}
			if ((fd = open(qfpathname, O_RDONLY)) < 0) {
				fd = open(qfpathname, O_RDWR|O_CREAT, 0640);
				if (fd < 0 && errno != ENOENT) {
					perror(qfpathname);
					free(qup);
					continue;
				}
				(void)fprintf(stderr, "Creating quota file %s\n",
				    qfpathname);
				sleep(3);
				(void)fchown(fd, getuid(),
				    getentry(quotagroup, GRPQUOTA));
				(void)fchmod(fd, 0640);
			}
			lseek(fd, (off_t)(id * sizeof(struct dqblk)), SEEK_SET);
			switch (read(fd, &qup->dqblk, sizeof(struct dqblk))) {
			case 0:			/* EOF */
				/*
				 * Convert implicit 0 quota (EOF)
				 * into an explicit one (zero'ed dqblk)
				 */
				bzero((caddr_t)&qup->dqblk,
				    sizeof(struct dqblk));
				break;

			case sizeof(struct dqblk):	/* OK */
				break;

			default:		/* ERROR */
				warn("read error in %s", qfpathname);
				close(fd);
				free(qup);
				continue;
			}
			close(fd);
		}
		strcpy(qup->qfname, qfpathname);
		strcpy(qup->fsname, fs->fs_file);
		if (quphead == NULL)
			quphead = qup;
		else
			quptail->next = qup;
		quptail = qup;
		qup->next = 0;
	}
	endfsent();
	return(quphead);
}

/*
 * Store the requested quota information.
 */
void
putprivs(id, quotatype, quplist)
	long id;
	int quotatype;
	struct quotause *quplist;
{
	register struct quotause *qup;
	int qcmd, fd;

	qcmd = QCMD(Q_SETQUOTA, quotatype);
	for (qup = quplist; qup; qup = qup->next) {
		if (quotactl(qup->fsname, qcmd, id, (char *)&qup->dqblk) == 0)
			continue;
		if ((fd = open(qup->qfname, O_WRONLY)) < 0) {
			perror(qup->qfname);
		} else {
			lseek(fd, (off_t)(id * sizeof (struct dqblk)), 0);
			if (write(fd, &qup->dqblk, sizeof (struct dqblk)) !=
			    sizeof (struct dqblk))
				warn(qup->qfname);
			close(fd);
		}
	}
}

/*
 * Take a list of priviledges and get it edited.
 */
int
editit(tmpfile)
	char *tmpfile;
{
	int omask;
	int pid, stat, xpid;
	char *argp[] = {"sh", "-c", NULL, NULL};
	register char *ed;
	char *p;

	if ((ed = getenv("EDITOR")) == (char *)0)
		ed = _PATH_VI;
	p = (char *)malloc(strlen(ed) + 1 + strlen(tmpfile) + 1);
	if (!p)
		return(0);
	(void)sprintf(p, "%s %s", ed, tmpfile);
	argp[2] = p;

	omask = sigblock(sigmask(SIGINT)|sigmask(SIGQUIT)|sigmask(SIGHUP));
 top:
	if ((pid = fork()) < 0) {
		if (errno == EPROCLIM) {
			warnx("you have too many processes");
			free(p);
			return(0);
		}
		if (errno == EAGAIN) {
			sleep(1);
			goto top;
		}
		perror("fork");
		free(p);
		return(0);
	}
	if (pid == 0) {
		sigsetmask(omask);
		setgid(getgid());
		setuid(getuid());
		execv(_PATH_BSHELL, argp);
		_exit(127);
	}
	free(p);
	for (;;) {
		xpid = waitpid(pid, (int *)&stat, WUNTRACED);
		if (WIFSTOPPED(stat))
			raise(WSTOPSIG(stat));
		else if (WIFEXITED(stat))
			break;
	}
	sigsetmask(omask);
	if (!WIFEXITED(stat) || WEXITSTATUS(stat) != 0)
		return(0);
	return(1);
}

/*
 * Convert a quotause list to an ASCII file.
 */
int
writeprivs(quplist, outfd, name, quotatype)
	struct quotause *quplist;
	int outfd;
	char *name;
	int quotatype;
{
	register struct quotause *qup;
	FILE *fd;

	ftruncate(outfd, 0);
	lseek(outfd, 0, SEEK_SET);
	if ((fd = fdopen(dup(outfd), "w")) == NULL)
		err(1, tmpfil);
	(void)fprintf(fd, "Quotas for %s %s:\n", qfextension[quotatype], name);
	for (qup = quplist; qup; qup = qup->next) {
		(void)fprintf(fd, "%s: %s %d, limits (soft = %d, hard = %d)\n",
		    qup->fsname, "blocks in use:",
		    dbtob(qup->dqblk.dqb_curblocks) / 1024,
		    dbtob(qup->dqblk.dqb_bsoftlimit) / 1024,
		    dbtob(qup->dqblk.dqb_bhardlimit) / 1024);
		(void)fprintf(fd, "%s %d, limits (soft = %d, hard = %d)\n",
		    "\tinodes in use:", qup->dqblk.dqb_curinodes,
		    qup->dqblk.dqb_isoftlimit, qup->dqblk.dqb_ihardlimit);
	}
	fclose(fd);
	return(1);
}

/*
 * Merge changes to an ASCII file into a quotause list.
 */
int
readprivs(quplist, infd)
	struct quotause *quplist;
	int infd;
{
	register struct quotause *qup;
	FILE *fd;
	int cnt;
	register char *cp;
	struct dqblk dqblk;
	char *fsp, line1[BUFSIZ], line2[BUFSIZ];

	lseek(infd, 0, SEEK_SET);
	fd = fdopen(dup(infd), "r");
	if (fd == NULL) {
		warnx("can't re-read temp file!!");
		return(0);
	}
	/*
	 * Discard title line, then read pairs of lines to process.
	 */
	(void)fgets(line1, sizeof (line1), fd);
	while (fgets(line1, sizeof (line1), fd) != NULL &&
	       fgets(line2, sizeof (line2), fd) != NULL) {
		if ((fsp = strtok(line1, " \t:")) == NULL) {
			warnx("%s: bad format", line1);
			return(0);
		}
		if ((cp = strtok((char *)0, "\n")) == NULL) {
			warnx("%s: %s: bad format", fsp, &fsp[strlen(fsp) + 1]);
			return(0);
		}
		cnt = sscanf(cp,
		    " blocks in use: %d, limits (soft = %d, hard = %d)",
		    &dqblk.dqb_curblocks, &dqblk.dqb_bsoftlimit,
		    &dqblk.dqb_bhardlimit);
		if (cnt != 3) {
			warnx("%s:%s: bad format", fsp, cp);
			return(0);
		}
		dqblk.dqb_curblocks = btodb(dqblk.dqb_curblocks * 1024);
		dqblk.dqb_bsoftlimit = btodb(dqblk.dqb_bsoftlimit * 1024);
		dqblk.dqb_bhardlimit = btodb(dqblk.dqb_bhardlimit * 1024);
		if ((cp = strtok(line2, "\n")) == NULL) {
			warnx("%s: %s: bad format", fsp, line2);
			return(0);
		}
		cnt = sscanf(cp,
		    "\tinodes in use: %d, limits (soft = %d, hard = %d)",
		    &dqblk.dqb_curinodes, &dqblk.dqb_isoftlimit,
		    &dqblk.dqb_ihardlimit);
		if (cnt != 3) {
			warnx("%s: %s: bad format", fsp, line2);
			return(0);
		}
		for (qup = quplist; qup; qup = qup->next) {
			if (strcmp(fsp, qup->fsname))
				continue;
			/*
			 * Cause time limit to be reset when the quota
			 * is next used if previously had no soft limit
			 * or were under it, but now have a soft limit
			 * and are over it.
			 */
			if (dqblk.dqb_bsoftlimit &&
			    qup->dqblk.dqb_curblocks >= dqblk.dqb_bsoftlimit &&
			    (qup->dqblk.dqb_bsoftlimit == 0 ||
			     qup->dqblk.dqb_curblocks <
			     qup->dqblk.dqb_bsoftlimit))
				qup->dqblk.dqb_btime = 0;
			if (dqblk.dqb_isoftlimit &&
			    qup->dqblk.dqb_curinodes >= dqblk.dqb_isoftlimit &&
			    (qup->dqblk.dqb_isoftlimit == 0 ||
			     qup->dqblk.dqb_curinodes <
			     qup->dqblk.dqb_isoftlimit))
				qup->dqblk.dqb_itime = 0;
			qup->dqblk.dqb_bsoftlimit = dqblk.dqb_bsoftlimit;
			qup->dqblk.dqb_bhardlimit = dqblk.dqb_bhardlimit;
			qup->dqblk.dqb_isoftlimit = dqblk.dqb_isoftlimit;
			qup->dqblk.dqb_ihardlimit = dqblk.dqb_ihardlimit;
			qup->flags |= FOUND;
			if (dqblk.dqb_curblocks == qup->dqblk.dqb_curblocks &&
			    dqblk.dqb_curinodes == qup->dqblk.dqb_curinodes)
				break;
			warnx("%s: cannot change current allocationn", fsp);
			break;
		}
	}
	fclose(fd);
	/*
	 * Disable quotas for any filesystems that have not been found.
	 */
	for (qup = quplist; qup; qup = qup->next) {
		if (qup->flags & FOUND) {
			qup->flags &= ~FOUND;
			continue;
		}
		qup->dqblk.dqb_bsoftlimit = 0;
		qup->dqblk.dqb_bhardlimit = 0;
		qup->dqblk.dqb_isoftlimit = 0;
		qup->dqblk.dqb_ihardlimit = 0;
	}
	return(1);
}

/*
 * Convert a quotause list to an ASCII file of grace times.
 */
int
writetimes(quplist, outfd, quotatype)
	struct quotause *quplist;
	int outfd;
	int quotatype;
{
	register struct quotause *qup;
	char *cvtstoa();
	FILE *fd;

	ftruncate(outfd, 0);
	lseek(outfd, 0, SEEK_SET);
	if ((fd = fdopen(dup(outfd), "w")) == NULL)
		err(1, tmpfil);
	(void)fprintf(fd,
	    "Time units may be: days, hours, minutes, or seconds\n");
	(void)fprintf(fd,
	    "Grace period before enforcing soft limits for %ss:\n",
	    qfextension[quotatype]);
	for (qup = quplist; qup; qup = qup->next) {
		(void)fprintf(fd, "%s: block grace period: %s, ",
		    qup->fsname, cvtstoa(qup->dqblk.dqb_btime));
		(void)fprintf(fd, "file grace period: %s\n",
		    cvtstoa(qup->dqblk.dqb_itime));
	}
	fclose(fd);
	return(1);
}

/*
 * Merge changes of grace times in an ASCII file into a quotause list.
 */
int
readtimes(quplist, infd)
	struct quotause *quplist;
	int infd;
{
	register struct quotause *qup;
	FILE *fd;
	int cnt;
	register char *cp;
	time_t itime, btime, iseconds, bseconds;
	char *fsp, bunits[10], iunits[10], line1[BUFSIZ];

	lseek(infd, 0, SEEK_SET);
	fd = fdopen(dup(infd), "r");
	if (fd == NULL) {
		warnx("can't re-read temp file!!");
		return(0);
	}
	/*
	 * Discard two title lines, then read lines to process.
	 */
	(void)fgets(line1, sizeof (line1), fd);
	(void)fgets(line1, sizeof (line1), fd);
	while (fgets(line1, sizeof (line1), fd) != NULL) {
		if ((fsp = strtok(line1, " \t:")) == NULL) {
			warnx("%s: bad format", line1);
			return(0);
		}
		if ((cp = strtok((char *)0, "\n")) == NULL) {
			warnx("%s: %s: bad format", fsp,
			    &fsp[strlen(fsp) + 1]);
			return(0);
		}
		cnt = sscanf(cp,
		    " block grace period: %d %s file grace period: %d %s",
		    (int *)&btime, bunits, (int *)&itime, iunits);
		if (cnt != 4) {
			warnx("%s:%s: bad format", fsp, cp);
			return(0);
		}
		if (cvtatos(btime, bunits, &bseconds) == 0)
			return(0);
		if (cvtatos(itime, iunits, &iseconds) == 0)
			return(0);
		for (qup = quplist; qup; qup = qup->next) {
			if (strcmp(fsp, qup->fsname))
				continue;
			qup->dqblk.dqb_btime = bseconds;
			qup->dqblk.dqb_itime = iseconds;
			qup->flags |= FOUND;
			break;
		}
	}
	fclose(fd);
	/*
	 * reset default grace periods for any filesystems
	 * that have not been found.
	 */
	for (qup = quplist; qup; qup = qup->next) {
		if (qup->flags & FOUND) {
			qup->flags &= ~FOUND;
			continue;
		}
		qup->dqblk.dqb_btime = 0;
		qup->dqblk.dqb_itime = 0;
	}
	return(1);
}

/*
 * Convert seconds to ASCII times.
 */
char *
cvtstoa(time)
	time_t time;
{
	static char buf[20];

	if (time % (24 * 60 * 60) == 0) {
		time /= 24 * 60 * 60;
		(void)sprintf(buf, "%d day%s", (int)time,
		    time == 1 ? "" : "s");
	} else if (time % (60 * 60) == 0) {
		time /= 60 * 60;
		(void)sprintf(buf, "%d hour%s", (int)time,
		    time == 1 ? "" : "s");
	} else if (time % 60 == 0) {
		time /= 60;
		(void)sprintf(buf, "%d minute%s", (int)time,
		    time == 1 ? "" : "s");
	} else
		(void)sprintf(buf, "%d second%s", (int)time,
		    time == 1 ? "" : "s");
	return(buf);
}

/*
 * Convert ASCII input times to seconds.
 */
int
cvtatos(time, units, seconds)
	time_t time;
	char *units;
	time_t *seconds;
{

	if (bcmp(units, "second", 6) == 0)
		*seconds = time;
	else if (bcmp(units, "minute", 6) == 0)
		*seconds = time * 60;
	else if (bcmp(units, "hour", 4) == 0)
		*seconds = time * 60 * 60;
	else if (bcmp(units, "day", 3) == 0)
		*seconds = time * 24 * 60 * 60;
	else {
		(void)printf("%s: bad units, specify %s\n", units,
		    "days, hours, minutes, or seconds");
		return(0);
	}
	return(1);
}

/*
 * Free a list of quotause structures.
 */
void
freeprivs(quplist)
	struct quotause *quplist;
{
	register struct quotause *qup, *nextqup;

	for (qup = quplist; qup; qup = nextqup) {
		nextqup = qup->next;
		free(qup);
	}
}

/*
 * Check whether a string is completely composed of digits.
 */
int
alldigits(s)
	register char *s;
{
	register int c;

	c = *s++;
	do {
		if (!isdigit(c))
			return(0);
	} while ((c = *s++));
	return(1);
}

/*
 * Check to see if a particular quota is to be enabled.
 */
int
hasquota(fs, type, qfnamep)
	register struct fstab *fs;
	int type;
	char **qfnamep;
{
	register char *opt;
	char *cp;
	static char initname, usrname[100], grpname[100];
	static char buf[BUFSIZ];

	if (!initname) {
		(void)sprintf(usrname, "%s%s", qfextension[USRQUOTA], qfname);
		(void)sprintf(grpname, "%s%s", qfextension[GRPQUOTA], qfname);
		initname = 1;
	}
	strcpy(buf, fs->fs_mntops);
	for (opt = strtok(buf, ","); opt; opt = strtok(NULL, ",")) {
		if ((cp = strchr(opt, '=')))
			*cp++ = '\0';
		if (type == USRQUOTA && strcmp(opt, usrname) == 0)
			break;
		if (type == GRPQUOTA && strcmp(opt, grpname) == 0)
			break;
	}
	if (!opt)
		return(0);
	if (cp) {
		*qfnamep = cp;
		return(1);
	}
	(void)sprintf(buf, "%s/%s.%s", fs->fs_file, qfname, qfextension[type]);
	*qfnamep = buf;
	return(1);
}
