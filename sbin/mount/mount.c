/*	$OpenBSD: mount.c,v 1.36 2004/09/15 15:15:16 otto Exp $	*/
/*	$NetBSD: mount.c,v 1.24 1995/11/18 03:34:29 cgd Exp $	*/

/*
 * Copyright (c) 1980, 1989, 1993, 1994
 *	The Regents of the University of California.  All rights reserved.
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

#ifndef lint
static char copyright[] =
"@(#) Copyright (c) 1980, 1989, 1993, 1994\n\
	The Regents of the University of California.  All rights reserved.\n";
#endif /* not lint */

#ifndef lint
#if 0
static char sccsid[] = "@(#)mount.c	8.19 (Berkeley) 4/19/94";
#else
static char rcsid[] = "$OpenBSD: mount.c,v 1.36 2004/09/15 15:15:16 otto Exp $";
#endif
#endif /* not lint */

#include <sys/param.h>
#include <sys/mount.h>
#include <sys/socket.h>
#include <sys/wait.h>

#include <nfs/rpcv2.h>
#include <nfs/nfsproto.h>
#define _KERNEL
#include <nfs/nfs.h>
#undef _KERNEL

#include <err.h>
#include <errno.h>
#include <fstab.h>
#include <netdb.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <util.h>

#include "pathnames.h"

int	debug, verbose;
char	**typelist = NULL;

int	selected(const char *);
char   *catopt(char *, const char *);
char   *flags2opts(u_int32_t);
struct statfs
       *getmntpt(const char *);
int	hasopt(const char *, const char *);
void	maketypelist(char *);
void	mangle(char *, int *, const char **, int);
int	mountfs(const char *, const char *, const char *, const char *,
	    const char *, int);
void	prmount(struct statfs *);
int	disklabelcheck(struct fstab *);
__dead void	usage(void);

/* Map from mount options to printable formats. */
static struct opt {
	int o_opt;
	int o_silent;
	const char *o_name;
	const char *o_optname;
} optnames[] = {
	{ MNT_ASYNC,		0,	"asynchronous",		"async" },
	{ MNT_DEFEXPORTED,	1,	"exported to the world", "" },
	{ MNT_EXKERB,		1,	"kerberos uid mapping",	"" },
	{ MNT_EXPORTED,		0,	"NFS exported",		"" },
	{ MNT_EXPORTANON,	1,	"anon uid mapping",	"" },
	{ MNT_EXRDONLY,		1,	"exported read-only",	"" },
	{ MNT_LOCAL,		0,	"local",		"" },
	{ MNT_NOATIME,		0,	"noatime",		"noatime" },
	{ MNT_NOATIME,		0,	"noaccesstime",		"" },
	{ MNT_NODEV,		0,	"nodev",		"nodev" },
	{ MNT_NOEXEC,		0,	"noexec",		"noexec" },
	{ MNT_NOSUID,		0,	"nosuid",		"nosuid" },
	{ MNT_QUOTA,		0,	"with quotas",		"" },
	{ MNT_RDONLY,		0,	"read-only",		"ro" },
	{ MNT_ROOTFS,		1,	"root file system",	"" },
	{ MNT_SYNCHRONOUS,	0,	"synchronous",		"sync" },
	{ MNT_SOFTDEP,		0,	"softdep", 		"softdep" },
	{ MNT_UNION,		0,	"union",		"" },
	{ NULL,			0,	"",			"" }
};

int
main(int argc, char * const argv[])
{
	const char *mntonname, *vfstype;
	struct fstab *fs;
	struct statfs *mntbuf;
	FILE *mountdfp;
	pid_t pid;
	int all, ch, forceall, i, mntsize, rval, new;
	char *options, mntpath[MAXPATHLEN];

	all = forceall = 0;
	options = NULL;
	vfstype = "ffs";
	while ((ch = getopt(argc, argv, "Aadfo:rwt:uv")) != -1)
		switch (ch) {
		case 'A':
			all = forceall = 1;
			break;
		case 'a':
			all = 1;
			break;
		case 'd':
			debug = 1;
			break;
		case 'f':
			if (!hasopt(options, "force"))
				options = catopt(options, "force");
			break;
		case 'o':
			if (*optarg)
				options = catopt(options, optarg);
			break;
		case 'r':
			if (!hasopt(options, "ro"))
				options = catopt(options, "ro");
			break;
		case 't':
			if (typelist != NULL)
				errx(1, "only one -t option may be specified.");
			maketypelist(optarg);
			vfstype = optarg;
			break;
		case 'u':
			if (!hasopt(options, "update"))
				options = catopt(options, "update");
			break;
		case 'v':
			verbose = 1;
			break;
		case 'w':
			if (!hasopt(options, "rw"))
				options = catopt(options, "rw");
			break;
		case '?':
		default:
			usage();
			/* NOTREACHED */
		}
	argc -= optind;
	argv += optind;

#define	BADTYPE(type)							\
	(strcmp(type, FSTAB_RO) &&					\
	    strcmp(type, FSTAB_RW) && strcmp(type, FSTAB_RQ))

	rval = 0;
	new = 0;
	switch (argc) {
	case 0:
		if (all)
			while ((fs = getfsent()) != NULL) {
				if (BADTYPE(fs->fs_type))
					continue;
				if (!selected(fs->fs_vfstype))
					continue;
				if (hasopt(fs->fs_mntops, "noauto"))
					continue;
				if (disklabelcheck(fs))
					continue;
				if (mountfs(fs->fs_vfstype, fs->fs_spec,
				    fs->fs_file, options,
				    fs->fs_mntops, !forceall))
					rval = 1;
				else
					++new;
			}
		else {
			if ((mntsize = getmntinfo(&mntbuf, MNT_NOWAIT)) == 0)
				err(1, "getmntinfo");
			for (i = 0; i < mntsize; i++) {
				if (!selected(mntbuf[i].f_fstypename))
					continue;
				prmount(&mntbuf[i]);
			}
			exit(rval);
		}
		break;
	case 1:
		if (typelist != NULL)
			usage();

		if (realpath(*argv, mntpath) == NULL)
			err(1, "realpath %s", mntpath);
		if (hasopt(options, "update")) {
			if ((mntbuf = getmntpt(mntpath)) == NULL)
				errx(1,
				    "unknown special file or file system %s.",
				    *argv);
			if ((mntbuf->f_flags & MNT_ROOTFS) &&
			    !strcmp(mntbuf->f_mntfromname, "root_device")) {
				/* Lookup fstab for name of root device. */
				fs = getfsfile(mntbuf->f_mntonname);
				if (fs == NULL)
					errx(1,
					    "can't find fstab entry for %s.",
					    *argv);
			} else {
				fs = malloc(sizeof(*fs));
				if (fs == NULL)
					err(1, "malloc");
				fs->fs_vfstype = mntbuf->f_fstypename;
				fs->fs_spec = mntbuf->f_mntfromname;
			}
			/*
			 * It's an update, ignore the fstab file options.
			 * Get the current options, so we can change only
			 * the options which given via a command line.
			 */
			fs->fs_mntops = flags2opts(mntbuf->f_flags);
			mntonname = mntbuf->f_mntonname;
		} else {
			if ((fs = getfsfile(mntpath)) == NULL &&
			    (fs = getfsspec(mntpath)) == NULL)
				errx(1, "can't find fstab entry for %s.",
				    *argv);
			if (BADTYPE(fs->fs_type))
				errx(1, "%s has unknown file system type.",
				    *argv);
			mntonname = fs->fs_file;
		}
		rval = mountfs(fs->fs_vfstype, fs->fs_spec,
		    mntonname, options, fs->fs_mntops, 0);
		break;
	case 2:
		/*
		 * If -t flag has not been specified, and spec contains either
		 * a ':' or a '@' then assume that an NFS filesystem is being
		 * specified ala Sun.  If not, check the disklabel for a
		 * known filesystem type.
		 */
		if (typelist == NULL) {
			if (strpbrk(argv[0], ":@") != NULL)
				vfstype = "nfs";
			else {
				char *labelfs = readlabelfs(argv[0], 0);
				if (labelfs != NULL)
					vfstype = labelfs;
			}
		}
		rval = mountfs(vfstype,
		    argv[0], argv[1], options, NULL, 0);
		break;
	default:
		usage();
		/* NOTREACHED */
	}

	/*
	 * If the mount was successfully, and done by root, tell mountd the
	 * good news.  Pid checks are probably unnecessary, but don't hurt.
	 * XXX This should be done from kernel.
	 */
	if ((rval == 0 || new) && getuid() == 0 &&
	    (mountdfp = fopen(_PATH_MOUNTDPID, "r")) != NULL) {
		if (fscanf(mountdfp, "%d", &pid) == 1 &&
		    pid > 0 && kill(pid, SIGHUP) == -1 && errno != ESRCH)
			err(1, "signal mountd");
		(void)fclose(mountdfp);
	}

	exit(rval);
}

int
hasopt(const char *mntopts, const char *option)
{
	int found;
	char *opt, *optbuf;

	if (mntopts == NULL)
		return (0);
	optbuf = strdup(mntopts);
	found = 0;
	for (opt = optbuf; !found && opt != NULL; strsep(&opt, ","))
		found = !strncmp(opt, option, strlen(option));
	free(optbuf);
	return (found);
}

/*
 * Convert mount(2) flags to list of mount(8) options.
 */
char*
flags2opts(u_int32_t flags)
{
	char	*optlist;
	struct opt *p;

	optlist = NULL;
	for (p = optnames; p->o_opt; p++) {
		if (flags & p->o_opt && *p->o_optname)
			optlist = catopt(optlist, p->o_optname);
	}

	return(optlist);
}

int
mountfs(const char *vfstype, const char *spec, const char *name,
    const char *options, const char *mntopts, int skipmounted)
{
	/* List of directories containing mount_xxx subcommands. */
	static const char *edirs[] = {
		_PATH_SBIN,
		_PATH_USRSBIN,
		NULL
	};
	const char **argv, **edir;
	struct statfs sf;
	pid_t pid;
	int argc, i, status, argvsize;
	char *optbuf, execname[MAXPATHLEN], mntpath[MAXPATHLEN];

	if (realpath(name, mntpath) == NULL) {
		warn("realpath %s", mntpath);
		return (1);
	}

	name = mntpath;

	if (mntopts == NULL)
		mntopts = "";

	if (options == NULL) {
		if (*mntopts == '\0')
			options = "rw";
		else {
			options = mntopts;
			mntopts = "";
		}
	}

	/* options follows after mntopts, so they get priority over mntopts */
	optbuf = catopt(strdup(mntopts), options);

	if (strcmp(name, "/") == 0) {
		if (!hasopt(optbuf, "update"))
			optbuf = catopt(optbuf, "update");
	} else if (skipmounted) {
		if (statfs(name, &sf) < 0)
			err(1, "statfs %s", name);
		/* XXX can't check f_mntfromname, thanks to mfs, union, etc. */
		if (strncmp(name, sf.f_mntonname, MNAMELEN) == 0 &&
		    strncmp(vfstype, sf.f_fstypename, MFSNAMELEN) == 0) {
			if (verbose)
				(void)printf("%s on %s type %.*s: %s\n",
				    sf.f_mntfromname, sf.f_mntonname,
				    MFSNAMELEN, sf.f_fstypename,
				    "already mounted");
			return (0);
		}
	}

	argvsize = 64;
	if((argv = malloc(argvsize * sizeof(char*))) == NULL)
		err(1, "malloc");
	argc = 0;
	argv[argc++] = NULL;	/* this should be a full path name */
	mangle(optbuf, &argc, argv, argvsize - 4);
	argv[argc++] = spec;
	argv[argc++] = name;
	argv[argc] = NULL;

	if (debug) {
		(void)printf("exec: mount_%s", vfstype);
		for (i = 1; i < argc; i++)
			(void)printf(" %s", argv[i]);
		(void)printf("\n");
		free(optbuf);
		free(argv);
		return (0);
	}

	switch ((pid = fork())) {
	case -1:				/* Error. */
		warn("fork");
		free(optbuf);
		return (1);
	case 0:					/* Child. */
		/* Go find an executable. */
		edir = edirs;
		do {
			(void)snprintf(execname,
			    sizeof(execname), "%s/mount_%s", *edir, vfstype);
			argv[0] = execname;
			execv(execname, (char * const *)argv);
			if (errno != ENOENT)
				warn("exec %s for %s", execname, name);
		} while (*++edir != NULL);

		if (errno == ENOENT)
			warn("no mount helper program found for %s", vfstype);
		exit(1);
		/* NOTREACHED */
	default:				/* Parent. */
		free(optbuf);
		free(argv);

		if (waitpid(pid, &status, 0) < 0) {
			warn("waitpid");
			return (1);
		}

		if (WIFEXITED(status)) {
			if (WEXITSTATUS(status) != 0)
				return (WEXITSTATUS(status));
		} else if (WIFSIGNALED(status)) {
			warnx("%s: %s", name, strsignal(WTERMSIG(status)));
			return (1);
		}

		if (verbose) {
			if (statfs(name, &sf) < 0) {
				warn("statfs %s", name);
				return (1);
			}
			prmount(&sf);
		}
		break;
	}

	return (0);
}

void
prmount(struct statfs *sf)
{
	int flags;
	struct opt *o;
	int f = 0;

	(void)printf("%s on %s type %.*s", sf->f_mntfromname, sf->f_mntonname,
	    MFSNAMELEN, sf->f_fstypename);

	flags = sf->f_flags & MNT_VISFLAGMASK;
	if (verbose && !(flags & MNT_RDONLY))
		(void)printf("%s%s", !f++ ? " (" : ", ", "rw");
	for (o = optnames; flags && o->o_opt; o++)
		if (flags & o->o_opt) {
			if (!o->o_silent)
				(void)printf("%s%s", !f++ ? " (" : ", ",
				    o->o_name);
			flags &= ~o->o_opt;
		}
	if (flags)
		(void)printf("%sunknown flag%s %#x", !f++ ? " (" : ", ",
		    flags & (flags - 1) ? "s" : "", flags);


	if (verbose) {
		char buf[26];
		time_t time = sf->f_ctime;

		ctime_r(&time, buf);
		buf[24] = '\0';
		printf(", ctime=%s", buf);
	}

	/*
	 * Filesystem-specific options
	 * We only print the "interesting" values unless in verboser
	 * mode in order to keep the signal/noise ratio high.
	 */
	if (strcmp(sf->f_fstypename, MOUNT_NFS) == 0) {
		struct protoent *pr;
		struct nfs_args *nfs_args = &sf->mount_info.nfs_args;

		(void)printf("%s%s", !f++ ? " (" : ", ",
		    (nfs_args->flags & NFSMNT_NFSV3) ? "v3" : "v2");
		if (nfs_args->proto && (pr = getprotobynumber(nfs_args->proto)))
			(void)printf("%s%s", !f++ ? " (" : ", ", pr->p_name);
		else
			(void)printf("%s%s", !f++ ? " (" : ", ",
			    (nfs_args->sotype == SOCK_DGRAM) ? "udp" : "tcp");
		if (nfs_args->flags & NFSMNT_SOFT)
			(void)printf("%s%s", !f++ ? " (" : ", ", "soft");
		else if (verbose)
			(void)printf("%s%s", !f++ ? " (" : ", ", "hard");
		if (nfs_args->flags & NFSMNT_INT)
			(void)printf("%s%s", !f++ ? " (" : ", ", "intr");
		if (nfs_args->flags & NFSMNT_NOCONN)
			(void)printf("%s%s", !f++ ? " (" : ", ", "noconn");
		if (verbose || nfs_args->wsize != NFS_WSIZE)
			(void)printf("%s%s=%d", !f++ ? " (" : ", ",
			    "wsize", nfs_args->wsize);
		if (verbose || nfs_args->rsize != NFS_RSIZE)
			(void)printf("%s%s=%d", !f++ ? " (" : ", ",
			    "rsize", nfs_args->rsize);
		if (verbose || nfs_args->readdirsize != NFS_READDIRSIZE)
			(void)printf("%s%s=%d", !f++ ? " (" : ", ",
			    "rdirsize", nfs_args->readdirsize);
		if (verbose || nfs_args->timeo != 10) /* XXX */
			(void)printf("%s%s=%d", !f++ ? " (" : ", ",
			    "timeo", nfs_args->timeo);
		if (verbose || nfs_args->retrans != NFS_RETRANS)
			(void)printf("%s%s=%d", !f++ ? " (" : ", ",
			    "retrans", nfs_args->retrans);
		if (verbose || nfs_args->maxgrouplist != NFS_MAXGRPS)
			(void)printf("%s%s=%d", !f++ ? " (" : ", ",
			    "maxgrouplist", nfs_args->maxgrouplist);
		if (verbose || nfs_args->readahead != NFS_DEFRAHEAD)
			(void)printf("%s%s=%d", !f++ ? " (" : ", ",
			    "readahead", nfs_args->readahead);
		if (verbose) {
			(void)printf("%s%s=%d", !f++ ? " (" : ", ",
			    "acregmin", nfs_args->acregmin);
			(void)printf(", %s=%d",
			    "acregmax", nfs_args->acregmax);
			(void)printf(", %s=%d",
			    "acdirmin", nfs_args->acdirmin);
			(void)printf(", %s=%d",
			    "acdirmax", nfs_args->acdirmax);
		}
	} else if (strcmp(sf->f_fstypename, MOUNT_MFS) == 0) {
		int headerlen;
		long blocksize;
		char *header;

		header = getbsize(&headerlen, &blocksize);
		(void)printf("%s%s=%lu %s", !f++ ? " (" : ", ",
		    "size", sf->mount_info.mfs_args.size / blocksize, header);
	} else if (strcmp(sf->f_fstypename, MOUNT_ADOSFS) == 0) {
		struct adosfs_args *adosfs_args = &sf->mount_info.adosfs_args;

		if (verbose || adosfs_args->uid || adosfs_args->gid)
			(void)printf("%s%s=%u, %s=%u", !f++ ? " (" : ", ",
			    "uid", adosfs_args->uid, "gid", adosfs_args->gid);
		if (verbose || adosfs_args->mask != 0755)
			(void)printf("%s%s=0%o", !f++ ? " (" : ", ",
			    "mask", adosfs_args->mask);
	} else if (strcmp(sf->f_fstypename, MOUNT_MSDOS) == 0) {
		struct msdosfs_args *msdosfs_args = &sf->mount_info.msdosfs_args;

		if (verbose || msdosfs_args->uid || msdosfs_args->gid)
			(void)printf("%s%s=%u, %s=%u", !f++ ? " (" : ", ",
			    "uid", msdosfs_args->uid, "gid", msdosfs_args->gid);
		if (verbose || msdosfs_args->mask != 0755)
			(void)printf("%s%s=0%o", !f++ ? " (" : ", ",
			    "mask", msdosfs_args->mask);
		if (msdosfs_args->flags & MSDOSFSMNT_SHORTNAME)
			(void)printf("%s%s", !f++ ? " (" : ", ", "short");
		if (msdosfs_args->flags & MSDOSFSMNT_LONGNAME)
			(void)printf("%s%s", !f++ ? " (" : ", ", "long");
		if (msdosfs_args->flags & MSDOSFSMNT_NOWIN95)
			(void)printf("%s%s", !f++ ? " (" : ", ", "nowin95");
		if (msdosfs_args->flags & MSDOSFSMNT_GEMDOSFS)
			(void)printf("%s%s", !f++ ? " (" : ", ", "gem");
		if (msdosfs_args->flags & MSDOSFSMNT_ALLOWDIRX)
			(void)printf("%s%s", !f++ ? " (" : ", ", "direxec");
	} else if (strcmp(sf->f_fstypename, MOUNT_CD9660) == 0) {
		struct iso_args *iso_args = &sf->mount_info.iso_args;

		if (iso_args->flags & ISOFSMNT_NORRIP)
			(void)printf("%s%s", !f++ ? " (" : ", ", "norrip");
		if (iso_args->flags & ISOFSMNT_GENS)
			(void)printf("%s%s", !f++ ? " (" : ", ", "gens");
		if (iso_args->flags & ISOFSMNT_EXTATT)
			(void)printf("%s%s", !f++ ? " (" : ", ", "extatt");
	} else if (strcmp(sf->f_fstypename, MOUNT_PROCFS) == 0) {
		struct procfs_args *procfs_args = &sf->mount_info.procfs_args;

		if (verbose)
			(void)printf("version %d", procfs_args->version);
		if (procfs_args->flags & PROCFSMNT_LINUXCOMPAT)
			(void)printf("%s%s", !f++ ? " (" : ", ", "linux");
	}
	(void)printf(f ? ")\n" : "\n");
}

struct statfs *
getmntpt(const char *name)
{
	struct statfs *mntbuf;
	int i, mntsize;

	mntsize = getmntinfo(&mntbuf, MNT_NOWAIT);
	for (i = 0; i < mntsize; i++)
		if (strcmp(mntbuf[i].f_mntfromname, name) == 0 ||
		    strcmp(mntbuf[i].f_mntonname, name) == 0)
			return (&mntbuf[i]);
	return (NULL);
}

static enum { IN_LIST, NOT_IN_LIST } which;

int
selected(const char *type)
{
	char **av;

	/* If no type specified, it's always selected. */
	if (typelist == NULL)
		return (1);
	for (av = typelist; *av != NULL; ++av)
		if (!strncmp(type, *av, MFSNAMELEN))
			return (which == IN_LIST ? 1 : 0);
	return (which == IN_LIST ? 0 : 1);
}

void
maketypelist(char *fslist)
{
	int i;
	char *nextcp, **av;

	if ((fslist == NULL) || (fslist[0] == '\0'))
		errx(1, "empty type list");

	/*
	 * XXX
	 * Note: the syntax is "noxxx,yyy" for no xxx's and
	 * no yyy's, not the more intuitive "noxxx,noyyy".
	 */
	if (fslist[0] == 'n' && fslist[1] == 'o') {
		fslist += 2;
		which = NOT_IN_LIST;
	} else
		which = IN_LIST;

	/* Count the number of types. */
	for (i = 1, nextcp = fslist; (nextcp = strchr(nextcp, ',')); i++)
		++nextcp;

	/* Build an array of that many types. */
	if ((av = typelist = malloc((i + 1) * sizeof(char *))) == NULL)
		err(1, NULL);
	av[0] = fslist;
	for (i = 1, nextcp = fslist; (nextcp = strchr(nextcp, ',')); i++) {
		*nextcp = '\0';
		av[i] = ++nextcp;
	}
	/* Terminate the array. */
	av[i] = NULL;
}

char *
catopt(char *s0, const char *s1)
{
	size_t i;
	char *cp;

	if (s0 && *s0) {
		i = strlen(s0) + strlen(s1) + 1 + 1;
		if ((cp = malloc(i)) == NULL)
			err(1, NULL);
		(void)snprintf(cp, i, "%s,%s", s0, s1);
	} else
		cp = strdup(s1);

	free(s0);
	return (cp);
}

void
mangle(char *options, int *argcp, const char **argv, int argcmax)
{
	char *p, *s;
	int argc;

	argcmax -= 2;
	argc = *argcp;
	for (s = options; argc <= argcmax && (p = strsep(&s, ",")) != NULL;)
		if (*p != '\0') {
			if (*p == '-') {
				argv[argc++] = p;
				p = strchr(p, '=');
				if (p) {
					*p = '\0';
					argv[argc++] = p + 1;
				}
			} else {
				argv[argc++] = "-o";
				argv[argc++] = p;
			}
		}

	*argcp = argc;
}

void
usage(void)
{

	(void)fprintf(stderr,
	    "usage: mount %s %s\n       mount %s\n       mount %s\n",
	    "[-dfruvw] [-o options] [-t ffs | external_type]",
	    "special node",
	    "[-Aadfruvw] [-t ffs | external_type]",
	    "[-dfruvw] special | node");
	exit(1);
}

int
disklabelcheck(struct fstab *fs)
{
	char *labelfs;

	if (strcmp(fs->fs_vfstype, "nfs") != 0 ||
	    strpbrk(fs->fs_spec, ":@") == NULL) {
		labelfs = readlabelfs(fs->fs_spec, 0);
		if (labelfs == NULL ||
		    strcmp(labelfs, fs->fs_vfstype) == 0)
			return (0);
		if (strcmp(fs->fs_vfstype, "ufs") == 0 &&
		    strcmp(labelfs, "ffs") == 0) {
			warnx("%s: fstab uses outdated type 'ufs' -- fix please",
			    fs->fs_spec);
			return (0);
		}
		warnx("%s: fstab type %s != disklabel type %s",
		    fs->fs_spec, fs->fs_vfstype, labelfs);
		return (1);
	}
	return (0);
}

