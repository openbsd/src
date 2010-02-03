/*	$OpenBSD: xlint.c,v 1.34 2010/02/03 20:46:31 miod Exp $	*/
/*	$NetBSD: xlint.c,v 1.3 1995/10/23 14:29:30 jpo Exp $	*/

/*
 * Copyright (c) 1994, 1995 Jochen Pohl
 * All Rights Reserved.
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
 *      This product includes software developed by Jochen Pohl for
 *	The NetBSD Project.
 * 4. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef lint
static char rcsid[] = "$OpenBSD: xlint.c,v 1.34 2010/02/03 20:46:31 miod Exp $";
#endif

#include <sys/param.h>
#include <sys/wait.h>
#include <sys/stat.h>
#include <sys/utsname.h>
#include <stdio.h>
#include <stdlib.h>
#include <signal.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <err.h>
#include <errno.h>
#include <paths.h>

#include "lint.h"
#include "pathnames.h"

/* directory for temporary files */
static	const	char *tmpdir;

/* path name for cpp output */
static	char	*cppout;

/* files created by 1st pass */
static	char	**p1out;

/* input files for 2nd pass (without libraries) */
static	char	**p2in;

/* library which will be created by 2nd pass */
static	char	*p2out;

/* flags always passed to cpp */
static	char	**cppflags;

/* flags for cpp, controled by sflag/tflag */
static	char	**lcppflgs;

/* flags for lint1 */
static	char	**l1flags;

/* flags for lint2 */
static	char	**l2flags;

/* libraries for lint2 */
static	char	**l2libs;

/* default libraries */
static	char	**deflibs;

/* additional libraries */
static	char	**libs;

/* search path for libraries */
static	char	**libsrchpath;

/* flags */
static	int	iflag, oflag, Cflag, sflag, tflag, Fflag = 1;

/* print the commands executed to run the stages of compilation */
static	int	Vflag;

/* filename for oflag */
static	char	*outputfn;

/* reset after first .c source has been processed */
static	int	first = 1;

/*
 * name of a file which is currently written by a child and should
 * be removed after abnormal termination of the child
 */
static	const	char *currfn;

static	void	appstrg(char ***, char *);
static	void	appcstrg(char ***, const char *);
static	void	applst(char ***, char *const *);
static	void	freelst(char ***);
static	char	*concat2(const char *, const char *);
static	char	*concat3(const char *, const char *, const char *);
static	void	terminate(int);
static	const	char *lbasename(const char *, int);
static	void	appdef(char ***, const char *);
static	void	usage(void);
static	void	fname(const char *);
static	int	runchild(const char *, char *const *, const char *);
static	void	findlibs(char *const *);
static	int	rdok(const char *);
static	void	lint2(void);
static	void	cat(char *const *, const char *);

/*
 * Some functions to deal with lists of strings.
 * Take care that we get no surprises in case of asynchronous signals.
 */
static void
appstrg(char ***lstp, char *s)
{
	char	**lst, **olst;
	int	i;

	olst = *lstp;
	for (i = 0; olst[i] != NULL; i++) ;
	lst = xmalloc((i + 2) * sizeof (char *));
	(void)memcpy(lst, olst, i * sizeof (char *));
	lst[i] = s;
	lst[i + 1] = NULL;
	*lstp = lst;
}

static void
appcstrg(char ***lstp, const char *s)
{
	appstrg(lstp, xstrdup(s));
}

static void
applst(char ***destp, char *const *src)
{
	int	i, k;
	char	**dest, **odest;

	odest = *destp;
	for (i = 0; odest[i] != NULL; i++) ;
	for (k = 0; src[k] != NULL; k++) ;
	dest = xmalloc((i + k + 1) * sizeof (char *));
	(void)memcpy(dest, odest, i * sizeof (char *));
	for (k = 0; src[k] != NULL; k++)
		dest[i + k] = xstrdup(src[k]);
	dest[i + k] = NULL;
	*destp = dest;
	free(odest);
}

static void
freelst(char ***lstp)
{
	char	*s;
	int	i;

	for (i = 0; (*lstp)[i] != NULL; i++) ;
	while (i-- > 0) {
		s = (*lstp)[i];
		(*lstp)[i] = NULL;
		free(s);
	}
}

static char *
concat2(const char *s1, const char *s2)
{
	char	*s;
	size_t len = strlen(s1) + strlen(s2) + 1;

	s = xmalloc(len);
	(void)strlcpy(s, s1, len);
	(void)strlcat(s, s2, len);

	return (s);
}

static char *
concat3(const char *s1, const char *s2, const char *s3)
{
	char	*s;
	size_t len = strlen(s1) + strlen(s2) + strlen(s3) + 1;

	s = xmalloc(len);
	(void)strlcpy(s, s1, len);
	(void)strlcat(s, s2, len);
	(void)strlcat(s, s3, len);

	return (s);
}

/*
 * Clean up after a signal.
 */
static void
terminate(int signo)
{
	int	i;

	if (cppout != NULL)
		(void)remove(cppout);

	if (p1out != NULL) {
		for (i = 0; p1out[i] != NULL; i++)
			(void)remove(p1out[i]);
	}

	if (p2out != NULL)
		(void)remove(p2out);

	if (currfn != NULL)
		(void)remove(currfn);

	_exit(signo != 0 ? 1 : 0);
}

/*
 * Returns a pointer to the last component of strg after delim.
 * Returns strg if the string does not contain delim.
 */
static const char *
lbasename(const char *strg, int delim)
{
	const	char *cp, *cp1, *cp2;

	cp = cp1 = cp2 = strg;
	while (*cp != '\0') {
		if (*cp++ == delim) {
			cp2 = cp1;
			cp1 = cp;
		}
	}
	return (*cp1 == '\0' ? cp2 : cp1);
}

static void
appdef(char ***lstp, const char *def)
{
	appstrg(lstp, concat2("-D__", def));
	appstrg(lstp, concat3("-D__", def, "__"));
}

static void
usage()
{
	(void)printf("usage: lint [-ceFfgHhprsVvxz] [-i | -nu] [-Dname[=def]] [-Idirectory]\n");
	(void)printf("\t[-Ldirectory] [-llibrary] [-ooutputfile] [-Uname] file ...\n");
	(void)printf("       lint [-ceFfgHhprsVvz] -Clibrary [-Dname[=def]]\n");
	(void)printf("\t[-Idirectory] [-Uname] file ...\n");
	terminate(-1);
}

int
main(int argc, char *argv[])
{
	int	c;
	int	fd;
	char	flgbuf[3], *tmp, *s;
	size_t	len;
	struct	utsname un;

	if ((tmp = getenv("TMPDIR")) == NULL || (len = strlen(tmp)) == 0) {
		tmpdir = xstrdup(_PATH_TMP);
	} else {
		s = xmalloc(len + 2);
		(void)snprintf(s, len + 2, "%s%s",
			tmp, tmp[len - 1] == '/' ? "" : "/");
		tmpdir = s;
	}

	if (asprintf(&cppout, "%slint0.XXXXXXXXXX", tmpdir) == -1)
		err(1, NULL);
	if ((fd = mkstemp(cppout)) == -1) {
		warn("can't make temp");
		terminate(-1);
	}
	close(fd);

	p1out = xcalloc(1, sizeof (char *));
	p2in = xcalloc(1, sizeof (char *));
	cppflags = xcalloc(1, sizeof (char *));
	lcppflgs = xcalloc(1, sizeof (char *));
	l1flags = xcalloc(1, sizeof (char *));
	l2flags = xcalloc(1, sizeof (char *));
	l2libs = xcalloc(1, sizeof (char *));
	deflibs = xcalloc(1, sizeof (char *));
	libs = xcalloc(1, sizeof (char *));
	libsrchpath = xcalloc(1, sizeof (char *));

	appcstrg(&cppflags, "-x");
	appcstrg(&cppflags, "c");
	appcstrg(&cppflags, "-undef");
	/* even with -undef cpp still identifies as GNUC */
	appcstrg(&cppflags, "-U__GNUC__");
#if defined(__GNUC__)
#if __GNUC__ < 3
	appcstrg(&cppflags, "-$");
	appcstrg(&cppflags, "-C");
#else
	appcstrg(&cppflags, "-CC");
#endif
#endif
	appcstrg(&cppflags, "-Wcomment");
	appcstrg(&cppflags, "-D__OpenBSD__");
	appcstrg(&cppflags, "-Dlint");		/* XXX don't def. with -s */
	appdef(&cppflags, "lint");
	appdef(&cppflags, "unix");

	if (uname(&un) == -1)
		err(1, "uname");
	appdef(&cppflags, un.machine);
	appstrg(&lcppflgs, concat2("-D", un.machine));

#ifdef MACHINE_ARCH
#ifdef MACHINE_CPU
	if (strcmp(MACHINE_ARCH, MACHINE_CPU) != 0) {
		appdef(&cppflags, MACHINE_CPU);
		appstrg(&lcppflgs, concat2("-D", MACHINE_CPU));
	}
#endif
	if (strcmp(un.machine, MACHINE_ARCH) != 0) {
		appdef(&cppflags, MACHINE_ARCH);
		appstrg(&lcppflgs, concat2("-D", MACHINE_ARCH));
	}
#endif

	appcstrg(&deflibs, "c");

	if (signal(SIGHUP, terminate) == SIG_IGN)
		(void)signal(SIGHUP, SIG_IGN);
	(void)signal(SIGINT, terminate);
	(void)signal(SIGQUIT, terminate);
	(void)signal(SIGTERM, terminate);

	while (argc > optind) {
		c = getopt(argc, argv, "abcefghil:no:prstuvxyzC:D:FHI:L:U:V");

		switch (c) {

		case 'a':
		case 'b':
		case 'c':
		case 'e':
		case 'f':
		case 'g':
		case 'r':
		case 'v':
		case 'y':
		case 'z':
			(void)snprintf(flgbuf, sizeof flgbuf, "-%c", c);
			appcstrg(&l1flags, flgbuf);
			break;

		case 'F':
			Fflag = 1;
			/* FALLTHROUGH */
		case 'u':
		case 'h':
			(void)snprintf(flgbuf, sizeof flgbuf, "-%c", c);
			appcstrg(&l1flags, flgbuf);
			appcstrg(&l2flags, flgbuf);
			break;

		case 'i':
			if (Cflag)
				usage();
			iflag = 1;
			break;

		case 'n':
			freelst(&deflibs);
			break;

		case 'p':
			appcstrg(&l1flags, "-p");
			appcstrg(&l2flags, "-p");
			if (*deflibs != NULL) {
				freelst(&deflibs);
				appcstrg(&deflibs, "c");
			}
			break;

		case 's':
			if (tflag)
				usage();
			freelst(&lcppflgs);
			appcstrg(&lcppflgs, "-trigraphs");
			appcstrg(&lcppflgs, "-Wtrigraphs");
			appcstrg(&lcppflgs, "-pedantic");
			appcstrg(&lcppflgs, "-D__STRICT_ANSI__");
			appcstrg(&l1flags, "-s");
			appcstrg(&l2flags, "-s");
			sflag = 1;
			break;

		case 't':
			if (sflag)
				usage();
			freelst(&lcppflgs);
			appcstrg(&lcppflgs, "-traditional");
			appstrg(&lcppflgs, concat2("-D", MACHINE));
			appstrg(&lcppflgs, concat2("-D", MACHINE_ARCH));
			appcstrg(&l1flags, "-t");
			appcstrg(&l2flags, "-t");
			tflag = 1;
			break;

		case 'x':
			appcstrg(&l2flags, "-x");
			break;

		case 'C':
			if (Cflag || oflag || iflag)
				usage();
			Cflag = 1;
			appstrg(&l2flags, concat2("-C", optarg));
			if (asprintf(&p2out, "llib-l%s.ln", optarg) == -1)
				err(1, NULL);
			freelst(&deflibs);
			break;

		case 'D':
		case 'I':
		case 'U':
			(void)snprintf(flgbuf, sizeof flgbuf, "-%c", c);
			appstrg(&cppflags, concat2(flgbuf, optarg));
			break;

		case 'l':
			appcstrg(&libs, optarg);
			break;

		case 'o':
			if (Cflag || oflag)
				usage();
			oflag = 1;
			outputfn = xstrdup(optarg);
			break;

		case 'L':
			appcstrg(&libsrchpath, optarg);
			break;

		case 'H':
			appcstrg(&l2flags, "-H");
			break;

		case 'V':
			Vflag = 1;
			break;

		case '?':
			usage();
			/* NOTREACHED */

		case -1:
			/* filename */
			if (argv[optind] == NULL)
				break;
			fname(argv[optind++]);
			first = 0;
		}

	}
	argc -= optind;
	argv += optind;

	if (first)
		usage();

	if (iflag)
		terminate(0);

	if (!oflag) {
		if ((s = getenv("LIBDIR")) == NULL || strlen(s) == 0)
			s = PATH_LINTLIB;
		appcstrg(&libsrchpath, s);
		findlibs(libs);
		findlibs(deflibs);
	}

	(void)printf("Lint pass2:\n");
	lint2();

	if (oflag)
		cat(p2in, outputfn);

	if (Cflag)
		p2out = NULL;

	terminate(0);
	/* NOTREACHED */
}

/*
 * Read a file name from the command line
 * and pass it through lint1 if it is a C source.
 */
static void
fname(const char *name)
{
	const	char *bn, *suff;
	char	**args, *ofn, *path;
	size_t	len;
	int	error;
	int	fd;

	bn = lbasename(name, '/');
	suff = lbasename(bn, '.');

	if (strcmp(suff, "ln") == 0) {
		/* only for lint2 */
		if (!iflag)
			appcstrg(&p2in, name);
		return;
	}

	if (strcmp(suff, "c") != 0 &&
	    (strncmp(bn, "llib-l", 6) != 0 || bn != suff)) {
		warnx("unknown file type: %s", name);
		return;
	}

	/* build the name of the output file of lint1 */
	if (oflag) {
		ofn = outputfn;
		outputfn = NULL;
		oflag = 0;
	} else if (iflag) {
		len = strlen(bn) + (bn == suff ? 4 : 2);
		ofn = xmalloc(len);
		(void)snprintf(ofn, len, "%.*s",
		    bn == suff ? strlen(bn) : (suff - 1) - bn, bn);
		(void)strlcat(ofn, ".ln", len);
	} else {
		if (asprintf(&ofn, "%slint1.XXXXXXXXXX", tmpdir) == -1)
			err(1, NULL);
		if ((fd  = mkstemp(ofn)) == -1) {
			warn("can't make temp");
			terminate(-1);
		}
		close(fd);
	}
	if (!iflag)
		appcstrg(&p1out, ofn);

	args = xcalloc(1, sizeof (char *));

	/* run cpp */

	if (asprintf(&path, "%s/cpp", PATH_LIBEXEC) == -1)
		err(1, NULL);

	appcstrg(&args, path);
	applst(&args, cppflags);
	applst(&args, lcppflgs);
	appcstrg(&args, name);
	appcstrg(&args, cppout);

	error = runchild(path, args, cppout);
	free(path);
	freelst(&args);
	if (error)
		return;

	/* run lint1 */

	if (asprintf(&path, "%s/lint1", PATH_LIBEXEC) == -1)
		err(1, NULL);

	appcstrg(&args, path);
	applst(&args, l1flags);
	appcstrg(&args, cppout);
	appcstrg(&args, ofn);

	error = runchild(path, args, ofn);
	free(path);
	freelst(&args);
	if (error)
		return;

	appcstrg(&p2in, ofn);
	free(ofn);

	free(args);
}

static int
runchild(const char *path, char *const *args, const char *crfn)
{
	int	status, signo, i;
	pid_t	rv;

	if (Vflag) {
		for (i = 0; args[i] != NULL; i++)
			(void)printf("%s ", args[i]);
		(void)printf("\n");
	}

	currfn = crfn;

	(void)fflush(stdout);

	switch (fork()) {
	case -1:
		warn("cannot fork");
		terminate(-1);
		/* NOTREACHED */
	default:
		/* parent */
		break;
	case 0:
		/* child */
		(void)execv(path, args);
		warn("cannot exec %s", path);
		exit(1);
		/* NOTREACHED */
	}
	currfn = NULL;

	while ((rv = wait(&status)) == -1 && errno == EINTR) ;
	if (rv == -1) {
		warn("wait");
		return(-1);
	}
	if (WIFSIGNALED(status)) {
		signo = WTERMSIG(status);
		warnx("%s got SIG%s", path, sys_signame[signo]);
		return(-1);
	}
	if (WEXITSTATUS(status) != 0)
		return(-1);
	return(0);
}

static void
findlibs(char *const *liblst)
{
	int	i, k;
	const	char *lib, *path;
	char	*lfn;
	size_t	len, l;

	lfn = NULL;

	for (i = 0; (lib = liblst[i]) != NULL; i++) {
		for (k = 0; (path = libsrchpath[k]) != NULL; k++) {
			len = strlen(path) + strlen(lib);
			l = len + sizeof ("/llib-l.ln");
			lfn = xrealloc(lfn, l);
			(void)snprintf(lfn, l, "%s/llib-l%s.ln", path, lib);
			if (rdok(lfn))
				break;
			l = len + sizeof ("/lint/llib-l.ln");
			lfn = xrealloc(lfn, l);
			(void)snprintf(lfn, l, "%s/lint/llib-l%s.ln", path, lib);
			if (rdok(lfn))
				break;
		}
		if (path != NULL) {
			appstrg(&l2libs, concat2("-l", lfn));
		} else {
			warnx("cannot find llib-l%s.ln", lib);
		}
	}

	free(lfn);
}

static int
rdok(const char *path)
{
	struct	stat sbuf;

	if (stat(path, &sbuf) == -1)
		return (0);
	if ((sbuf.st_mode & S_IFMT) != S_IFREG)
		return (0);
	if (access(path, R_OK) == -1)
		return (0);
	return (1);
}

static void
lint2()
{
	char	*path, **args;

	args = xcalloc(1, sizeof (char *));

	if (asprintf(&path, "%s/lint2", PATH_LIBEXEC) == -1)
		err(1, NULL);

	appcstrg(&args, path);
	applst(&args, l2flags);
	applst(&args, l2libs);
	applst(&args, p2in);

	(void)runchild(path, args, p2out);
	free(path);
	freelst(&args);
	free(args);
}

static void
cat(char *const *srcs, const char *dest)
{
	int	ifd, ofd, i;
	char	*src, *buf;
	ssize_t	rlen;

	if ((ofd = open(dest, O_WRONLY | O_CREAT | O_TRUNC, 0666)) == -1) {
		warn("cannot open %s", dest);
		terminate(-1);
	}

	buf = xmalloc(MBLKSIZ);

	for (i = 0; (src = srcs[i]) != NULL; i++) {
		if ((ifd = open(src, O_RDONLY)) == -1) {
			free(buf);
			warn("cannot open %s", src);
			terminate(-1);
		}
		do {
			if ((rlen = read(ifd, buf, MBLKSIZ)) == -1) {
				free(buf);
				warn("read error on %s", src);
				terminate(-1);
			}
			if (write(ofd, buf, (size_t)rlen) == -1) {
				free(buf);
				warn("write error on %s", dest);
				terminate(-1);
			}
		} while (rlen == MBLKSIZ);
		(void)close(ifd);
	}
	(void)close(ofd);
	free(buf);
}

