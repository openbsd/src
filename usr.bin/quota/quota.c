/*	$OpenBSD: quota.c,v 1.33 2015/01/16 06:40:10 deraadt Exp $	*/

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

/*
 * Disk quota reporting program.
 */
#include <sys/param.h>	/* DEV_BSIZE */
#include <sys/types.h>
#include <sys/file.h>
#include <sys/stat.h>
#include <sys/mount.h>
#include <sys/socket.h>

#include <ufs/ufs/quota.h>
#include <ctype.h>
#include <err.h>
#include <errno.h>
#include <fstab.h>
#include <grp.h>
#include <netdb.h>
#include <pwd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

#include <rpc/rpc.h>
#include <rpc/pmap_prot.h>
#include <rpcsvc/rquota.h>

char *qfname = QUOTAFILENAME;
char *qfextension[] = INITQFNAMES;

struct quotause {
	struct	quotause *next;
	long	flags;
	struct	dqblk dqblk;
	char	fsname[PATH_MAX + 1];
};
#define	FOUND	0x01

int	alldigits(char *);
int	callaurpc(char *, int, int, int, xdrproc_t, void *, xdrproc_t, void *);
int	getnfsquota(struct statfs *, struct fstab *, struct quotause *,
	    long, int);
struct quotause
       *getprivs(long id, int quotatype);
int	getufsquota(struct statfs *, struct fstab *, struct quotause *,
	    long, int);
void	heading(int, u_long, const char *, const char *);
void	showgid(gid_t);
void	showgrpname(const char *);
void	showquotas(int, u_long, const char *);
void	showuid(uid_t);
void	showusrname(const char *);
char   *timeprt(time_t seconds);
int	ufshasquota(struct fstab *, int, char **);
void	usage(void);

int	qflag;
int	vflag;

int
main(int argc, char *argv[])
{
	int ngroups; 
	gid_t mygid, gidset[NGROUPS];
	int i, gflag = 0, uflag = 0;
	int ch;
	extern char *optarg;
	extern int optind;

	while ((ch = getopt(argc, argv, "ugvq")) != -1) {
		switch(ch) {
		case 'g':
			gflag++;
			break;
		case 'u':
			uflag++;
			break;
		case 'v':
			vflag++;
			break;
		case 'q':
			qflag++;
			break;
		default:
			usage();
		}
	}
	argc -= optind;
	argv += optind;
	if (!uflag && !gflag)
		uflag++;
	if (argc == 0) {
		if (uflag)
			showuid(getuid());
		if (gflag) {
			mygid = getgid();
			ngroups = getgroups(NGROUPS, gidset);
			if (ngroups < 0)
				err(1, "getgroups");
			showgid(mygid);
			for (i = 0; i < ngroups; i++)
				if (gidset[i] != mygid)
					showgid(gidset[i]);
		}
		exit(0);
	}
	if (uflag && gflag)
		usage();
	if (uflag) {
		for (; argc > 0; argc--, argv++) {
			if (alldigits(*argv))
				showuid(atoi(*argv));
			else
				showusrname(*argv);
		}
		exit(0);
	}
	if (gflag) {
		for (; argc > 0; argc--, argv++) {
			if (alldigits(*argv))
				showgid(atoi(*argv));
			else
				showgrpname(*argv);
		}
		exit(0);
	}
	/* NOTREACHED */

	exit(1);
}

void
usage(void)
{
	fprintf(stderr, "%s\n%s\n%s\n",
	    "usage: quota [-q | -v] [-gu]",
	    "       quota [-q | -v] -g group ...",
	    "       quota [-q | -v] -u user ...");
	exit(1);
}

/*
 * Print out quotas for a specified user identifier.
 */
void
showuid(uid_t uid)
{
	struct passwd *pwd = getpwuid(uid);
	uid_t myuid;
	const char *name;

	if (pwd == NULL)
		name = "(no account)";
	else
		name = pwd->pw_name;
	myuid = getuid();
	if (uid != myuid && myuid != 0) {
		warnx("%s (uid %u): permission denied", name, uid);
		return;
	}
	showquotas(USRQUOTA, uid, name);
}

/*
 * Print out quotas for a specified user name.
 */
void
showusrname(const char *name)
{
	struct passwd *pwd = getpwnam(name);
	uid_t myuid;

	if (pwd == NULL) {
		warnx("%s: unknown user", name);
		return;
	}
	myuid = getuid();
	if (pwd->pw_uid != myuid && myuid != 0) {
		warnx("%s (uid %u): permission denied", pwd->pw_name,
		    pwd->pw_uid);
		return;
	}
	showquotas(USRQUOTA, pwd->pw_uid, pwd->pw_name);
}

/*
 * Print out quotas for a specified group identifier.
 */
void
showgid(gid_t gid)
{
	struct group *grp = getgrgid(gid);
	int ngroups;
	gid_t mygid, gidset[NGROUPS];
	int i;
	const char *name;

	if (grp == NULL)
		name = "(no entry)";
	else
		name = grp->gr_name;
	mygid = getgid();
	ngroups = getgroups(NGROUPS, gidset);
	if (ngroups < 0) {
		warn("getgroups");
		return;
	}
	if (gid != mygid) {
		for (i = 0; i < ngroups; i++)
			if (gid == gidset[i])
				break;
		if (i >= ngroups && getuid() != 0) {
			warnx("%s (gid %u): permission denied", name, gid);
			return;
		}
	}
	showquotas(GRPQUOTA, gid, name);
}

/*
 * Print out quotas for a specified group name.
 */
void
showgrpname(const char *name)
{
	struct group *grp = getgrnam(name);
	int ngroups;
	gid_t mygid, gidset[NGROUPS];
	int i;

	if (grp == NULL) {
		warnx("%s: unknown group", name);
		return;
	}
	mygid = getgid();
	ngroups = getgroups(NGROUPS, gidset);
	if (ngroups < 0) {
		warn("getgroups");
		return;
	}
	if (grp->gr_gid != mygid) {
		for (i = 0; i < ngroups; i++)
			if (grp->gr_gid == gidset[i])
				break;
		if (i >= ngroups && getuid() != 0) {
			warnx("%s (gid %u): permission denied",
			    grp->gr_name, grp->gr_gid);
			return;
		}
	}
	showquotas(GRPQUOTA, grp->gr_gid, grp->gr_name);
}

void
showquotas(int type, u_long id, const char *name)
{
	struct quotause *qup;
	struct quotause *quplist;
	char *msgi, *msgb, *nam;
	uid_t lines = 0;
	static time_t now;

	if (now == 0)
		time(&now);
	quplist = getprivs(id, type);
	for (qup = quplist; qup; qup = qup->next) {
		if (!vflag &&
		    qup->dqblk.dqb_isoftlimit == 0 &&
		    qup->dqblk.dqb_ihardlimit == 0 &&
		    qup->dqblk.dqb_bsoftlimit == 0 &&
		    qup->dqblk.dqb_bhardlimit == 0)
			continue;
		msgi = (char *)0;
		if (qup->dqblk.dqb_ihardlimit &&
		    qup->dqblk.dqb_curinodes >= qup->dqblk.dqb_ihardlimit)
			msgi = "File limit reached on";
		else if (qup->dqblk.dqb_isoftlimit &&
		    qup->dqblk.dqb_curinodes >= qup->dqblk.dqb_isoftlimit) {
			if (qup->dqblk.dqb_itime > now)
				msgi = "In file grace period on";
			else
				msgi = "Over file quota on";
		}
		msgb = (char *)0;
		if (qup->dqblk.dqb_bhardlimit &&
		    qup->dqblk.dqb_curblocks >= qup->dqblk.dqb_bhardlimit)
			msgb = "Block limit reached on";
		else if (qup->dqblk.dqb_bsoftlimit &&
		    qup->dqblk.dqb_curblocks >= qup->dqblk.dqb_bsoftlimit) {
			if (qup->dqblk.dqb_btime > now)
				msgb = "In block grace period on";
			else
				msgb = "Over block quota on";
		}
		if (qflag) {
			if ((msgi != (char *)0 || msgb != (char *)0) &&
			    lines++ == 0)
				heading(type, id, name, "");
			if (msgi != (char *)0)
				printf("\t%s %s\n", msgi, qup->fsname);
			if (msgb != (char *)0)
				printf("\t%s %s\n", msgb, qup->fsname);
			continue;
		}
		if (vflag ||
		    qup->dqblk.dqb_curblocks ||
		    qup->dqblk.dqb_curinodes) {
			if (lines++ == 0)
				heading(type, id, name, "");
			nam = qup->fsname;
			if (strlen(qup->fsname) > 15) {
				printf("%s\n", qup->fsname);
				nam = "";
			} 
			printf("%12s %7d%c %7d %7d %7s",
			    nam,
			    (int)(dbtob((u_quad_t)qup->dqblk.dqb_curblocks)
				/ 1024),
			    (msgb == (char *)0) ? ' ' : '*',
			    (int)(dbtob((u_quad_t)qup->dqblk.dqb_bsoftlimit)
				/ 1024),
			    (int)(dbtob((u_quad_t)qup->dqblk.dqb_bhardlimit)
				/ 1024),
			    (msgb == (char *)0) ? ""
			        : timeprt(qup->dqblk.dqb_btime));
			printf(" %7d%c %7d %7d %7s\n",
			    qup->dqblk.dqb_curinodes,
			    (msgi == (char *)0) ? ' ' : '*',
			    qup->dqblk.dqb_isoftlimit,
			    qup->dqblk.dqb_ihardlimit,
			    (msgi == (char *)0) ? ""
			        : timeprt(qup->dqblk.dqb_itime)
			);
			continue;
		}
	}
	if (!qflag && lines == 0)
		heading(type, id, name, "none");
}

void
heading(int type, u_long id, const char *name, const char *tag)
{

	printf("Disk quotas for %s %s (%cid %ld): %s\n", qfextension[type],
	    name, *qfextension[type], id, tag);
	if (!qflag && tag[0] == '\0') {
		printf("%12s%8s%9s%8s%8s%9s%8s%8s%8s\n",
		    "Filesystem",
		    "KBytes",
		    "quota",
		    "limit",
		    "grace",
		    "files",
		    "quota",
		    "limit",
		    "grace");
	}
}

/*
 * Calculate the grace period and return a printable string for it.
 */
char *
timeprt(time_t seconds)
{
	time_t hours, minutes;
	static char buf[20];
	static time_t now;

	if (now == 0)
		time(&now);
	if (now > seconds)
		return ("none");
	seconds -= now;
	minutes = (seconds + 30) / 60;
	hours = (minutes + 30) / 60;
	if (hours >= 36) {
		(void)snprintf(buf, sizeof buf, "%ddays",
		    (int)((hours + 12) / 24));
		return (buf);
	}
	if (minutes >= 60) {
		(void)snprintf(buf, sizeof buf, "%2d:%d",
		    (int)(minutes / 60), (int)(minutes % 60));
		return (buf);
	}
	(void)snprintf(buf, sizeof buf, "%2d", (int)minutes);
	return (buf);
}

/*
 * Collect the requested quota information.
 */
struct quotause *
getprivs(long id, int quotatype)
{
	struct quotause *qup, *quptail;
	struct fstab *fs;
	struct quotause *quphead;
	struct statfs *fst;
	int nfst, i;

	qup = quphead = NULL;

	nfst = getmntinfo(&fst, MNT_WAIT);
	if (nfst == 0)
		errx(2, "no filesystems mounted!");
	setfsent();
	for (i = 0; i < nfst; i++) {
		if (qup == NULL) {
			if ((qup =
			    (struct quotause *)malloc(sizeof *qup)) == NULL)
				errx(2, "out of memory");
		}
		if (strncmp(fst[i].f_fstypename, "nfs", MFSNAMELEN) == 0) {
			if (getnfsquota(&fst[i], NULL, qup, id, quotatype) == 0)
				continue;
		} else if (!strncmp(fst[i].f_fstypename, "ffs", MFSNAMELEN) ||
		    !strncmp(fst[i].f_fstypename, "ufs", MFSNAMELEN) ||
		    !strncmp(fst[i].f_fstypename, "mfs", MFSNAMELEN)) {
			/*
			 * XXX
			 * UFS filesystems must be in /etc/fstab, and must
			 * indicate that they have quotas on (?!) This is quite
			 * unlike SunOS where quotas can be enabled/disabled
			 * on a filesystem independent of /etc/fstab, and it
			 * will still print quotas for them.
			 */
			if ((fs = getfsspec(fst[i].f_mntfromspec)) == NULL)
				continue;
			if (getufsquota(&fst[i], fs, qup, id, quotatype) == 0)
				continue;
		} else
			continue;
		strncpy(qup->fsname, fst[i].f_mntonname, sizeof qup->fsname-1);
		qup->fsname[sizeof qup->fsname-1] = '\0';
		if (quphead == NULL)
			quphead = qup;
		else
			quptail->next = qup;
		quptail = qup;
		quptail->next = 0;
		qup = NULL;
	}
	if (qup)
		free(qup);
	endfsent();
	return (quphead);
}

/*
 * Check to see if a particular quota is to be enabled.
 */
int
ufshasquota(struct fstab *fs, int type, char **qfnamep)
{
	static char initname, usrname[100], grpname[100];
	static char buf[BUFSIZ];
	char *opt, *cp;

	cp = NULL;
	if (!initname) {
		(void)snprintf(usrname, sizeof usrname, "%s%s",
		    qfextension[USRQUOTA], qfname);
		(void)snprintf(grpname, sizeof grpname, "%s%s",
		    qfextension[GRPQUOTA], qfname);
		initname = 1;
	}
	strncpy(buf, fs->fs_mntops, sizeof buf);
	buf[sizeof(buf) - 1] = '\0';
	for (opt = strtok(buf, ","); opt; opt = strtok(NULL, ",")) {
		if ((cp = strchr(opt, '=')))
			*cp++ = '\0';
		if (type == USRQUOTA && strcmp(opt, usrname) == 0)
			break;
		if (type == GRPQUOTA && strcmp(opt, grpname) == 0)
			break;
	}
	if (!opt)
		return (0);
	if (cp) {
		*qfnamep = cp;
		return (1);
	}
	(void)snprintf(buf, sizeof buf, "%s/%s.%s",
	    fs->fs_file, qfname, qfextension[type]);
	*qfnamep = buf;
	return (1);
}

int
getufsquota(struct statfs *fst, struct fstab *fs, struct quotause *qup,
    long id, int quotatype)
{
	char *qfpathname;
	int fd, qcmd;

	qcmd = QCMD(Q_GETQUOTA, quotatype);
	if (!ufshasquota(fs, quotatype, &qfpathname))
		return (0);

	if (quotactl(fs->fs_file, qcmd, id, (char *)&qup->dqblk) != 0) {
		if ((fd = open(qfpathname, O_RDONLY)) < 0) {
			warn("%s", qfpathname);
			return (0);
		}
		(void)lseek(fd, (off_t)(id * sizeof(struct dqblk)), SEEK_SET);
		switch (read(fd, &qup->dqblk, sizeof(struct dqblk))) {
		case 0:				/* EOF */
			/*
			 * Convert implicit 0 quota (EOF)
			 * into an explicit one (zero'ed dqblk)
			 */
			memset((caddr_t)&qup->dqblk, 0, sizeof(struct dqblk));
			break;
		case sizeof(struct dqblk):	/* OK */
			break;
		default:		/* ERROR */
			warn("%s", qfpathname);
			close(fd);
			return (0);
		}
		close(fd);
	}
	return (1);
}

int
getnfsquota(struct statfs *fst, struct fstab *fs, struct quotause *qup,
    long id, int quotatype)
{
	struct getquota_args gq_args;
	struct getquota_rslt gq_rslt;
	struct dqblk *dqp = &qup->dqblk;
	struct timeval tv;
	char *cp;

	if (fst->f_flags & MNT_LOCAL)
		return (0);

	/*
	 * rpc.rquotad does not support group quotas
	 */
	if (quotatype != USRQUOTA)
		return (0);

	/*
	 * must be some form of "hostname:/path"
	 */
	cp = strchr(fst->f_mntfromname, ':');
	if (cp == NULL) {
		warnx("cannot find hostname for %s", fst->f_mntfromname);
		return (0);
	}
 
	*cp = '\0';
	if (cp[1] != '/') {
		*cp = ':';
		return (0);
	}

	gq_args.gqa_pathp = &cp[1];
	gq_args.gqa_uid = id;
	if (callaurpc(fst->f_mntfromname, RQUOTAPROG, RQUOTAVERS,
	    RQUOTAPROC_GETQUOTA, xdr_getquota_args, &gq_args,
	    xdr_getquota_rslt, &gq_rslt) != 0) {
		*cp = ':';
		return (0);
	}

	switch (gq_rslt.status) {
	case Q_NOQUOTA:
		break;
	case Q_EPERM:
		warnx("permission error, host: %s", fst->f_mntfromname);
		break;
	case Q_OK:
		gettimeofday(&tv, NULL);
			/* blocks*/
		dqp->dqb_bhardlimit =
		    gq_rslt.getquota_rslt_u.gqr_rquota.rq_bhardlimit *
		    (gq_rslt.getquota_rslt_u.gqr_rquota.rq_bsize / DEV_BSIZE);
		dqp->dqb_bsoftlimit =
		    gq_rslt.getquota_rslt_u.gqr_rquota.rq_bsoftlimit *
		    (gq_rslt.getquota_rslt_u.gqr_rquota.rq_bsize / DEV_BSIZE);
		dqp->dqb_curblocks =
		    gq_rslt.getquota_rslt_u.gqr_rquota.rq_curblocks *
		    (gq_rslt.getquota_rslt_u.gqr_rquota.rq_bsize / DEV_BSIZE);
			/* inodes */
		dqp->dqb_ihardlimit =
			gq_rslt.getquota_rslt_u.gqr_rquota.rq_fhardlimit;
		dqp->dqb_isoftlimit =
			gq_rslt.getquota_rslt_u.gqr_rquota.rq_fsoftlimit;
		dqp->dqb_curinodes =
			gq_rslt.getquota_rslt_u.gqr_rquota.rq_curfiles;
			/* grace times */
		dqp->dqb_btime =
		    tv.tv_sec + gq_rslt.getquota_rslt_u.gqr_rquota.rq_btimeleft;
		dqp->dqb_itime =
		    tv.tv_sec + gq_rslt.getquota_rslt_u.gqr_rquota.rq_ftimeleft;
		*cp = ':';
		return (1);
	default:
		warnx("bad rpc result, host: %s", fst->f_mntfromname);
		break;
	}
	*cp = ':';
	return (0);
}
 
int
callaurpc(char *host, int prognum, int versnum, int procnum,
    xdrproc_t inproc, void *in, xdrproc_t outproc, void *out)
{
	struct sockaddr_in server_addr;
	enum clnt_stat clnt_stat;
	struct hostent *hp;
	struct timeval timeout, tottimeout;
 
	CLIENT *client = NULL;
	int socket = RPC_ANYSOCK;
 
	if ((hp = gethostbyname(host)) == NULL)
		return ((int) RPC_UNKNOWNHOST);
	timeout.tv_usec = 0;
	timeout.tv_sec = 6;

	memset(&server_addr, 0, sizeof server_addr);
	memcpy(&server_addr.sin_addr, hp->h_addr, hp->h_length);
	server_addr.sin_family = AF_INET;
	server_addr.sin_port =  0;

	if ((client = clntudp_create(&server_addr, prognum,
	    versnum, timeout, &socket)) == NULL)
		return ((int) rpc_createerr.cf_stat);

	client->cl_auth = authunix_create_default();
	tottimeout.tv_sec = 25;
	tottimeout.tv_usec = 0;
	clnt_stat = clnt_call(client, procnum, inproc, in,
	    outproc, out, tottimeout);
 
	return ((int) clnt_stat);
}

int
alldigits(char *s)
{
	int c;

	c = (unsigned char)*s++;
	do {
		if (!isdigit(c))
			return (0);
	} while ((c = (unsigned char)*s++));
	return (1);
}
