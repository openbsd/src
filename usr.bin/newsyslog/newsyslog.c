/*	$OpenBSD: newsyslog.c,v 1.56 2002/09/19 21:22:59 millert Exp $	*/

/*
 * Copyright (c) 1999, 2002 Todd C. Miller <Todd.Miller@courtesan.com>
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
 * 3. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED ``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES,
 * INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY
 * AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL
 * THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS;
 * OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
 * WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR
 * OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF
 * ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

/*
 * Copyright (c) 1997, Jason Downs.  All rights reserved.
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
 *      This product includes software developed by Jason Downs for the
 *      OpenBSD system.
 * 4. Neither the name(s) of the author(s) nor the name OpenBSD
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR(S) ``AS IS'' AND ANY EXPRESS
 * OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR(S) BE LIABLE FOR ANY DIRECT,
 * INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
 * CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

/*
 * This file contains changes from the Open Software Foundation.
 */

/*
 * Copyright 1988, 1989 by the Massachusetts Institute of Technology
 *
 * Permission to use, copy, modify, and distribute this software
 * and its documentation for any purpose and without fee is
 * hereby granted, provided that the above copyright notice
 * appear in all copies and that both that copyright notice and
 * this permission notice appear in supporting documentation,
 * and that the names of M.I.T. and the M.I.T. S.I.P.B. not be
 * used in advertising or publicity pertaining to distribution
 * of the software without specific, written prior permission.
 * M.I.T. and the M.I.T. S.I.P.B. make no representations about
 * the suitability of this software for any purpose.  It is
 * provided "as is" without express or implied warranty.
 */

/*
 *      newsyslog - roll over selected logs at the appropriate time,
 *              keeping the a specified number of backup files around.
 *
 */

#ifndef lint
static const char rcsid[] = "$OpenBSD: newsyslog.c,v 1.56 2002/09/19 21:22:59 millert Exp $";
#endif /* not lint */

#ifndef CONF
#define CONF "/etc/newsyslog.conf" /* Configuration file */
#endif
#ifndef PIDFILE
#define PIDFILE "/etc/syslog.pid"
#endif
#ifndef COMPRESS
#define COMPRESS "/usr/bin/compress" /* File compression program */
#endif
#ifndef COMPRESS_POSTFIX
#define COMPRESS_POSTFIX ".Z"
#endif
#ifndef STATS_DIR
#define STATS_DIR "/etc"
#endif
#ifndef SENDMAIL
#define SENDMAIL "/usr/lib/sendmail"
#endif

#include <sys/param.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <sys/wait.h>

#include <ctype.h>
#include <err.h>
#include <errno.h>
#include <fcntl.h>
#include <grp.h>
#include <limits.h>
#include <pwd.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#define CE_ROTATED	0x01		/* Log file has been rotated */
#define CE_COMPACT	0x02		/* Compact the achived log files */
#define CE_BINARY	0x04		/* Logfile is in binary, don't add */
					/* status messages */
#define CE_MONITOR	0x08		/* Monitory for changes */
#define CE_FOLLOW	0x10		/* Follow symbolic links */

#define	MIN_PID		4		/* Don't touch pids lower than this */
#define	MIN_SIZE	512		/* Don't rotate if smaller than this */

#define	DPRINTF(x)	do { if (verbose) printf x ; } while (0)

struct conf_entry {
	char    *log;		/* Name of the log */
	char    *logbase;	/* Basename of the log */
	char    *backdir;	/* Directory in which to store backups */
	uid_t   uid;		/* Owner of log */
	gid_t   gid;		/* Group of log */
	int     numlogs;	/* Number of logs to keep */
	int     size;		/* Size cutoff to trigger trimming the log */
	int     hours;		/* Hours between log trimming */
	int     permissions;	/* File permissions on the log */
	int	signal;		/* Signal to send (defaults to SIGHUP) */
	int     flags;		/* Flags (CE_COMPACT & CE_BINARY)  */
	char	*whom;		/* Whom to notify if logfile changes */
	char	*pidfile;	/* Path to file containg pid to signal */
	char	*runcmd;	/* Command to run instead of sending a signal */
	struct conf_entry *next; /* Linked list pointer */
};

struct pidinfo {
	char	*file;
	int	signal;
};

int	verbose = 0;		/* Print out what's going on */
int	needroot = 1;		/* Root privs are necessary */
int	noaction = 0;		/* Don't do anything, just show it */
int	monitormode = 0;	/* Don't do monitoring by default */
int	force = 0;		/* Force the logs to be rotated */
char	*conf = CONF;		/* Configuration file to use */
time_t	timenow;
char	hostname[MAXHOSTNAMELEN]; /* hostname */
char	*daytime;		/* timenow in human readable form */
char	*arcdir;		/* dir to put archives in (if it exists) */

void do_entry(struct conf_entry *);
void parse_args(int, char **);
void usage(void);
struct conf_entry *parse_file(int *);
char *missing_field(char *, char *);
void dotrim(struct conf_entry *);
int log_trim(char *);
void compress_log(struct conf_entry *);
int sizefile(char *);
int age_old_log(struct conf_entry *);
char *sob(char *);
char *son(char *);
int isnumberstr(char *);
void domonitor(char *, char *);
FILE *openmail(void);
void child_killer(int);
void run_command(char *);
void send_signal(char *, int);

int
main(int argc, char **argv)
{
	struct conf_entry *p, *q, *x, *y;
	struct pidinfo *pidlist, *pl;
	char **av;
	int status, listlen;
	
	parse_args(argc, argv);
	argc -= optind;
	argv += optind;

	if (needroot && getuid() && geteuid())
		errx(1, "You must be root.");

	p = parse_file(&listlen);
	if (argc > 0) {
		/* Only rotate specified files. */
		x = y = NULL;
		listlen = 0;
		for (av = argv; *av; av++) {
			for (q = p; q; q = q->next)
				if (strcmp(*av, q->log) == 0) {
					if (x == NULL)
						x = y = q;
					else {
						y->next = q;
						y = q;
					}
					listlen++;
					break;
				}
			if (q == NULL)
				warnx("%s is not listed in %s", *av, conf);
		}
		if (x == NULL)
			errx(1, "no specified log files found in %s", conf);
		y->next = NULL;
		p = x;
	}

	pidlist = (struct pidinfo *)calloc(listlen + 1, sizeof(struct pidinfo));
	if (pidlist == NULL)
		err(1, "calloc");

	signal(SIGCHLD, child_killer);

	/* Step 1, rotate all log files */
	for (q = p; q; q = q->next)
		do_entry(q);

	/* Step 2, make a list of unique pid files */
	for (q = p, pl = pidlist; q; ) {
		if (q->flags & CE_ROTATED) {
			struct pidinfo *pltmp;

			for (pltmp = pidlist; pltmp < pl; pltmp++) {
				if ((q->pidfile &&
				    strcmp(pltmp->file, q->pidfile) == 0 &&
				    pltmp->signal == q->signal) ||
				    (q->runcmd &&
				    strcmp(q->runcmd, pltmp->file) == 0))
					break;
			}
			if (pltmp == pl) {	/* unique entry */
				if (q->runcmd) {
					pl->file = q->runcmd;
					pl->signal = -1;
				} else {
					pl->file = q->pidfile;
					pl->signal = q->signal;
				}
				pl++;
			}
		}
		q = q->next;
	}

	/* Step 3, send a signal or run a command */
	for (pl = pidlist; pl->file; pl++) {
		if (pl->file != NULL) {
			if (pl->signal == -1)
				run_command(pl->file);
			else
				send_signal(pl->file, pl->signal);
		}
	}
	if (!noaction)
		sleep(5);

	/* Step 4, compress the log.0 file if configured to do so and free */
	while (p) {
		if ((p->flags & CE_COMPACT) && (p->flags & CE_ROTATED))
			compress_log(p);
		q = p;
		p = p->next;
		free(q);
	}

	/* Wait for children to finish, then exit */
	while (waitpid(-1, &status, 0) != -1)
		;
	exit(0);
}

void
do_entry(struct conf_entry *ent)
{
	int modtime, size;
	struct stat sb;

	if (lstat(ent->log, &sb) != 0)
		return;
	if (!S_ISREG(sb.st_mode) &&
	    (!S_ISLNK(sb.st_mode) || !(ent->flags & CE_FOLLOW))) {
		DPRINTF(("--> not a regular file, skipping\n"));
		return;
	}

	DPRINTF(("%s <%d%s%s%s>: ", ent->log, ent->numlogs,
	    (ent->flags & CE_COMPACT) ? "Z" : "",
	    (ent->flags & CE_BINARY) ? "B" : "",
	    (ent->flags & CE_FOLLOW) ? "F" : ""));

	size = sizefile(ent->log);
	modtime = age_old_log(ent);
	if (size < 0) {
		DPRINTF(("does not exist.\n"));
	} else {
		if (ent->size > 0)
			DPRINTF(("size (Kb): %d [%d] ", size, ent->size));
		if (ent->hours > 0)
			DPRINTF(("age (hr): %d [%d] ", modtime, ent->hours));
		if (monitormode && ent->flags & CE_MONITOR)
			domonitor(ent->log, ent->whom);
		if (!monitormode && (force ||
		    (ent->size > 0 && size >= ent->size) ||
		    (ent->hours > 0 && (modtime >= ent->hours || modtime < 0)
		    && ((ent->flags & CE_BINARY) || size >= MIN_SIZE)))) {
			DPRINTF(("--> trimming log....\n"));
			if (noaction && !verbose)
				printf("%s <%d%s%s%s>\n", ent->log,
				    ent->numlogs,
				    (ent->flags & CE_COMPACT) ? "Z" : "",
				    (ent->flags & CE_BINARY) ? "B" : "",
				    (ent->flags & CE_FOLLOW) ? "F" : "");
			dotrim(ent);
			ent->flags |= CE_ROTATED;
		} else
			DPRINTF(("--> skipping\n"));
	}
}

/* Run the specified command */
void
run_command(char *cmd)
{
	if (noaction)
		(void)printf("run %s\n", cmd);
	else
		system(cmd);
}

/* Send a signal to the pid specified by pidfile */
void
send_signal(char *pidfile, int signal)
{
	pid_t pid;
	FILE *f;
	char line[BUFSIZ], *ep, *err;
	unsigned long ulval;

	if ((f = fopen(pidfile, "r")) == NULL) {
		warn("can't open %s", pidfile);
		return;
	}

	pid = 0;
	errno = 0;
	err = NULL;
	if (fgets(line, sizeof(line), f)) {
		ulval = strtoul(line, &ep, 10);
		if (line[0] == '\0' || (*ep != '\0' && *ep != '\n'))
			err = "invalid number in";
		else if (errno == ERANGE && ulval == ULONG_MAX)
			err = "out of range number in";
		else if (ulval == 0)
			err = "no number in";
		else if (ulval < MIN_PID)
			err = "preposterous process number in";
		else
			pid = ulval;
	} else {
		if (errno == 0)
			err = "empty";
		else
			err = "error reading";
	}
	(void)fclose(f);

	if (err)
		warnx("%s pid file: %s", err, pidfile);
	else if (noaction)
		(void)printf("kill -%s %ld\n", sys_signame[signal], (long)pid);
	else if (kill(pid, signal))
		warnx("warning - could not send SIG%s to daemon",
		    sys_signame[signal]);
}

void
parse_args(int argc, char **argv)
{
	int ch;
	char *p;

	timenow = time(NULL);
	daytime = ctime(&timenow) + 4;
	daytime[15] = '\0';

	/* Let's get our hostname */
	(void)gethostname(hostname, sizeof(hostname));

	/* Truncate domain */
	if ((p = strchr(hostname, '.')) != NULL)
		*p = '\0';

	while ((ch = getopt(argc, argv, "Fmnrva:f:")) != -1) {
		switch (ch) {
		case 'a':
			arcdir = optarg;
			break;
		case 'n':
			noaction++; /* This implies needroot as off */
			/* fall through */
		case 'r':
			needroot = 0;
			break;
		case 'v':
			verbose++;
			break;
		case 'f':
			conf = optarg;
			break;
		case 'm':
			monitormode++;
			break;
		case 'F':
			force++;
			break;
		default:
			usage();
		}
	}
	if (monitormode && force)
		errx(1, "cannot specify both -m and -F flags");
}

void
usage(void)
{
	extern const char *__progname;

	(void)fprintf(stderr, "usage: %s [-Fmnrv] [-a directory] "
	    "[-f config_file] [log ...]\n", __progname);
	exit(1);
}

/*
 * Parse a configuration file and return a linked list of all the logs
 * to process
 */
struct conf_entry *
parse_file(int *nentries)
{
	FILE *f;
	char line[BUFSIZ], *parse, *q, *errline, *group, *tmp;
	struct conf_entry *first = NULL;
	struct conf_entry *working = NULL;
	struct passwd *pwd;
	struct group *grp;
	struct stat sb;

	if (strcmp(conf, "-") == 0)
		f = stdin;
	else if ((f = fopen(conf, "r")) == NULL)
		err(1, "can't open %s", conf);

	*nentries = 0;
	while (fgets(line, sizeof(line), f)) {
		tmp = sob(line);
		if (*tmp == '\0' || *tmp == '#')
			continue;
		errline = strdup(tmp);
		if (errline == NULL)
			err(1, "strdup");
		(*nentries)++;
		if (!first) {
			working = (struct conf_entry *) malloc(sizeof(struct conf_entry));
			if (working == NULL)
				err(1, "malloc");
			first = working;
		} else {
			working->next = (struct conf_entry *) malloc(sizeof(struct conf_entry));
			if (working->next == NULL)
				err(1, "malloc");
			working = working->next;
		}

		q = parse = missing_field(sob(line), errline);
		*(parse = son(line)) = '\0';
		working->log = strdup(q);
		if (working->log == NULL)
			err(1, "strdup");

		if ((working->logbase = strrchr(working->log, '/')) != NULL)
			working->logbase++;

		q = parse = missing_field(sob(++parse), errline);
		*(parse = son(parse)) = '\0';
		if ((group = strchr(q, '.')) != NULL) {
			*group++ = '\0';
			if (*q) {
				if (!(isnumberstr(q))) {
					if ((pwd = getpwnam(q)) == NULL)
						errx(1, "Error in config file; unknown user: %s", q);
					working->uid = pwd->pw_uid;
				} else
					working->uid = atoi(q);
			} else
				working->uid = (uid_t)-1;
			
			q = group;
			if (*q) {
				if (!(isnumberstr(q))) {
					if ((grp = getgrnam(q)) == NULL)
						errx(1, "Error in config file; unknown group: %s", q);
					working->gid = grp->gr_gid;
				} else
					working->gid = atoi(q);
			} else
				working->gid = (gid_t)-1;
			
			q = parse = missing_field(sob(++parse), errline);
			*(parse = son(parse)) = '\0';
		} else {
			working->uid = (uid_t)-1;
			working->gid = (gid_t)-1;
		}

		if (!sscanf(q, "%o", &working->permissions))
			errx(1, "Error in config file; bad permissions: %s", q);

		q = parse = missing_field(sob(++parse), errline);
		*(parse = son(parse)) = '\0';
		if (!sscanf(q, "%d", &working->numlogs) || working->numlogs < 0)
			errx(1, "Error in config file; bad number: %s", q);

		q = parse = missing_field(sob(++parse), errline);
		*(parse = son(parse)) = '\0';
		if (isdigit(*q))
			working->size = atoi(q);
		else
			working->size = -1;
		
		q = parse = missing_field(sob(++parse), errline);
		*(parse = son(parse)) = '\0';
		if (isdigit(*q))
			working->hours = atoi(q);
		else
			working->hours = -1;

		working->flags = 0;
		q = sob(++parse);	/* Optional field */
		if (*q == 'Z' || *q == 'z' || *q == 'B' || *q == 'b' ||
		    *q == 'M' || *q == 'm') {
			*(parse = son(q)) = '\0';
			while (*q) {
				switch (*q) {
				case 'Z':
				case 'z':
					working->flags |= CE_COMPACT;
					break;
				case 'B':
				case 'b':
					working->flags |= CE_BINARY;
					break;
				case 'M':
				case 'm':
					working->flags |= CE_MONITOR;
					break;
				case 'F':
				case 'f':
					working->flags |= CE_FOLLOW;
					break;
				default:
					errx(1, "Illegal flag in config file: %c", *q);
					break;
				}
				q++;
			}
		} else
			parse--;	/* no flags so undo */

		working->whom = NULL;
		if (working->flags & CE_MONITOR) {	/* Optional field */
			q = parse = sob(++parse);
			*(parse = son(parse)) = '\0';

			working->whom = strdup(q);
			if (working->log == NULL)
				err(1, "strdup");
		}

		working->pidfile = PIDFILE;
		working->signal = SIGHUP;
		working->runcmd = NULL;
		for (;;) {
			q = parse = sob(++parse);	/* Optional field */
			if (q == NULL || *q == '\0')
				break;
			if (*q == '/') {
				*(parse = son(parse)) = '\0';
				if (strlen(q) >= MAXPATHLEN)
					errx(1, "%s: pathname too long", q);
				working->pidfile = strdup(q);
				if (working->pidfile == NULL)
					err(1, "strdup");
			} else if (*q == '"' && (tmp = strchr(q + 1, '"'))) {
				*(parse = tmp) = '\0';
				if (*++q != '\0') {
					working->runcmd = strdup(q);
					if (working->runcmd == NULL)
						err(1, "strdup");
				}
				working->pidfile = NULL;
				working->signal = -1;
			} else if (strncmp(q, "SIG", 3) == 0) {
				int i;

				*(parse = son(parse)) = '\0';
				for (i = 1; i < NSIG; i++) {
					if (!strcmp(sys_signame[i], q + 3)) {
						working->signal = i;
						break;
					}
				}
				if (i == NSIG)
					errx(1, "unknown signal: %s", q);
			} else
				errx(1, "unrecognized field: %s", q);
		}
		free(errline);

		/* If there is an arcdir, set working->backdir. */
		if (arcdir != NULL && working->logbase != NULL) {
			if (*arcdir == '/') {
				/* Fully qualified arcdir */
				working->backdir = arcdir;
			} else {
				/* arcdir is relative to log's parent dir */
				*(working->logbase - 1) = '\0';
				if ((asprintf(&working->backdir, "%s/%s",
				    working->log, arcdir)) == -1)
					err(1, "malloc");
				*(working->logbase - 1) = '/';
			}
			/* Ignore arcdir if it doesn't exist. */
			if (stat(working->backdir, &sb) != 0 ||
			    !S_ISDIR(sb.st_mode)) {
				if (working->backdir != arcdir)
					free(working->backdir);
				working->backdir = NULL;
			}
		} else
			working->backdir = NULL;

		/* Make sure we can't oflow MAXPATHLEN */
		if (working->backdir != NULL) {
			if (snprintf(line, sizeof(line), "%s/%s.%d%s",
			    working->backdir, working->logbase,
			    working->numlogs, COMPRESS_POSTFIX) >= MAXPATHLEN)
				errx(1, "%s: pathname too long", working->log);
		} else {
			if (snprintf(line, sizeof(line), "%s.%d%s",
			    working->log, working->numlogs, COMPRESS_POSTFIX)
			    >= MAXPATHLEN)
				errx(1, "%s: pathname too long", working->log);
		}
	}
	if (working)
		working->next = NULL;
	(void)fclose(f);
	return (first);
}

char *
missing_field(char *p, char *errline)
{
	if (!p || !*p) {
		warnx("Missing field in config file line:");
		fputs(errline, stderr);
		exit(1);
	}
	return (p);
}

void
dotrim(struct conf_entry *ent)
{
	char    file1[MAXPATHLEN], file2[MAXPATHLEN];
	char    zfile1[MAXPATHLEN], zfile2[MAXPATHLEN];
	char    oldlog[MAXPATHLEN];
	int     fd;
	struct  stat sb;
	int	numdays = ent->numlogs;

	/* Is there a separate backup dir? */
	if (ent->backdir != NULL)
		snprintf(oldlog, sizeof(oldlog), "%s/%s", ent->backdir,
		    ent->logbase);
	else
		strlcpy(oldlog, ent->log, sizeof(oldlog));

	/* Remove oldest log (may not exist) */
	(void)snprintf(file1, sizeof(file1), "%s.%d", oldlog, numdays);
	(void)snprintf(zfile1, sizeof(zfile1), "%s.%d%s", oldlog, numdays,
	    COMPRESS_POSTFIX);

	if (noaction) {
		printf("\trm -f %s %s\n", file1, zfile1);
	} else {
		(void)unlink(file1);
		(void)unlink(zfile1);
	}

	/* Move down log files */
	while (numdays--) {
		(void)strlcpy(file2, file1, sizeof(file2));
		(void)snprintf(file1, sizeof(file1), "%s.%d", oldlog, numdays);
		(void)strlcpy(zfile1, file1, sizeof(zfile1));
		(void)strlcpy(zfile2, file2, sizeof(zfile2));
		if (lstat(file1, &sb)) {
			(void)strlcat(zfile1, COMPRESS_POSTFIX, sizeof(zfile1));
			(void)strlcat(zfile2, COMPRESS_POSTFIX, sizeof(zfile2));
			if (lstat(zfile1, &sb))
				continue;
		}
		if (noaction) {
			printf("\tmv %s %s\n", zfile1, zfile2);
			printf("\tchmod %o %s\n", ent->permissions, zfile2);
			if (ent->uid != (uid_t)-1 || ent->gid != (gid_t)-1)
				printf("\tchown %u:%u %s\n",
				    ent->uid, ent->gid, zfile2);
		} else {
			if (rename(zfile1, zfile2))
				warn("can't mv %s to %s", zfile1, zfile2);
			if (chmod(zfile2, ent->permissions))
				warn("can't chmod %s", zfile2);
			if (ent->uid != (uid_t)-1 || ent->gid != (gid_t)-1)
				if (chown(zfile2, ent->uid, ent->gid))
					warn("can't chown %s", zfile2);
		}
	}
	if (!noaction && !(ent->flags & CE_BINARY))
		(void)log_trim(ent->log);  /* Report the trimming to the old log */

	(void)snprintf(file2, sizeof(file2), "%s.XXXXXXXXXX", ent->log);
	if (noaction)  {
		printf("\tmktemp %s\n", file2);
	} else {
		if ((fd = mkstemp(file2)) < 0)
			err(1, "can't start '%s' log", file2);
		if (ent->uid != (uid_t)-1 || ent->gid != (gid_t)-1)
			if (fchown(fd, ent->uid, ent->gid))
			    err(1, "can't chown '%s' log file", file2);
		if (fchmod(fd, ent->permissions))
			err(1, "can't chmod '%s' log file", file2);
		(void)close(fd);
		/* Add status message */
		if (!(ent->flags & CE_BINARY) && log_trim(file2))
			err(1, "can't add status message to log '%s'", file2);
	}

	if (ent->numlogs == 0) {
		if (noaction)
			printf("\trm %s\n", ent->log);
		else if (unlink(ent->log))
			warn("can't rm %s", ent->log);
	} else {
		if (noaction)
			printf("\tmv %s to %s\n", ent->log, file1);
		else if (rename(ent->log, file1))
			warn("can't to mv %s to %s", ent->log, file1);
	}

	/* Now move the new log file into place */
	if (noaction)
		printf("\tmv %s to %s\n", file2, ent->log);
	else if (rename(file2, ent->log))
		warn("can't to mv %s to %s", file2, ent->log);
}

/* Log the fact that the logs were turned over */
int
log_trim(char *log)
{
	FILE    *f;

	if ((f = fopen(log, "a")) == NULL)
		return (-1);
	(void)fprintf(f, "%s %s newsyslog[%ld]: logfile turned over\n",
	    daytime, hostname, (long)getpid());
	if (fclose(f) == EOF)
		err(1, "log_trim: fclose");
	return (0);
}

/* Fork off compress or gzip to compress the old log file */
void
compress_log(struct conf_entry *ent)
{
	pid_t pid;
	char *base, tmp[MAXPATHLEN];

	if (ent->backdir != NULL)
		snprintf(tmp, sizeof(tmp), "%s/%s.0", ent->backdir,
		    ent->logbase);
	else
		snprintf(tmp, sizeof(tmp), "%s.0", ent->log);

	if ((base = strrchr(COMPRESS, '/')) == NULL)
		base = COMPRESS;
	else
		base++;
	if (noaction) {
		printf("%s %s\n", base, tmp);
		return;
	}
	pid = fork();
	if (pid < 0) {
		err(1, "fork");
	} else if (pid == 0) {
		(void)execl(COMPRESS, base, "-f", tmp, (char *)NULL);
		warn(COMPRESS);
		_exit(1);
	}
}

/* Return size in kilobytes of a file */
int
sizefile(char *file)
{
	struct stat sb;

	if (stat(file, &sb) < 0)
		return (-1);
	return (sb.st_blocks / (1024.0 / DEV_BSIZE));
}

/* Return the age (in hours) of old log file (file.0), or -1 if none */
int
age_old_log(struct conf_entry *ent)
{
	struct stat sb;
	char tmp[MAXPATHLEN];

	if (ent->backdir != NULL)
		snprintf(tmp, sizeof(tmp), "%s/%s.0", ent->backdir, ent->logbase);
	else {
		strlcpy(tmp, ent->log, sizeof(tmp));
		strlcat(tmp, ".0", sizeof(tmp));
	}
	if (ent->flags & CE_COMPACT)
		strlcat(tmp, COMPRESS_POSTFIX, sizeof(tmp));
	if (stat(tmp, &sb) < 0)
		return (-1);
	return ((int)(timenow - sb.st_mtime + 1800) / 3600);
}

/* Skip Over Blanks */
char *
sob(char *p)
{
	while (p && *p && isspace(*p))
		p++;
	return (p);
}

/* Skip Over Non-Blanks */
char *
son(char *p)
{
	while (p && *p && !isspace(*p))
		p++;
	return (p);
}

/* Check if string is actually a number */
int
isnumberstr(char *string)
{
	while (*string) {
		if (!isdigit(*string++))
			return (0);
	}
	return (1);
}

void
domonitor(char *log, char *whom)
{
	struct stat sb, tsb;
	char fname[MAXPATHLEN], *flog, *p, *rb = NULL;
	FILE *fp;
	off_t osize;
	int rd;

	if (stat(log, &sb) < 0)
		return;

	flog = strdup(log);
	if (flog == NULL)
		err(1, "strdup");

	for (p = flog; *p != '\0'; p++) {
		if (*p == '/')
			*p = '_';
	}
	snprintf(fname, sizeof(fname), "%s/newsyslog.%s.size",
	    STATS_DIR, flog);

	/* ..if it doesn't exist, simply record the current size. */
	if ((sb.st_size == 0) || stat(fname, &tsb) < 0)
		goto update;

	fp = fopen(fname, "r");
	if (fp == NULL) {
		warn("%s", fname);
		goto cleanup;
	}
#ifdef QUAD_OFF_T
	if (fscanf(fp, "%qd\n", &osize) != 1) {
#else
	if (fscanf(fp, "%ld\n", &osize) != 1) {
#endif	/* QUAD_OFF_T */
		fclose(fp);
		goto update;
	}

	fclose(fp);

	/* If the file is smaller, mark the entire thing as changed. */
	if (sb.st_size < osize)
		osize = 0;

	/* Now see if current size is larger. */
	if (sb.st_size > osize) {
		rb = (char *) malloc(sb.st_size - osize);
		if (rb == NULL)
			err(1, "malloc");

		/* Open logfile, seek. */
		fp = fopen(log, "r");
		if (fp == NULL) {
			warn("%s", log);
			goto cleanup;
		}
		fseek(fp, osize, SEEK_SET);
		rd = fread(rb, 1, sb.st_size - osize, fp);
		if (rd < 1) {
			warn("fread");
			fclose(fp);
			goto cleanup;
		}
		
		/* Send message. */
		fclose(fp);

		fp = openmail();
		if (fp == NULL) {
			warn("openmail");
			goto cleanup;
		}
		fprintf(fp, "To: %s\nSubject: LOGFILE NOTIFICATION: %s\n\n\n",
		    whom, log);
		fwrite(rb, 1, rd, fp);
		fputs("\n\n", fp);

		pclose(fp);
	}
update:
	/* Reopen for writing and update file. */
	fp = fopen(fname, "w");
	if (fp == NULL) {
		warn("%s", fname);
		goto cleanup;
	}
#ifdef QUAD_OFF_T
	fprintf(fp, "%qd\n", sb.st_size);
#else
	fprintf(fp, "%ld\n", sb.st_size);
#endif	/* QUAD_OFF_T */
	fclose(fp);

cleanup:
	free(flog);
	if (rb != NULL)
		free(rb);
}

FILE *
openmail(void)
{
	FILE *ret;
	char *cmdbuf = NULL;

	asprintf(&cmdbuf, "%s -t", SENDMAIL);
	if (cmdbuf) {
		ret = popen(cmdbuf, "w");
		free(cmdbuf);
		return (ret);
	}
	return (NULL);
}

void
child_killer(int signo)
{
	int save_errno = errno;
	int status;

	while (waitpid(-1, &status, WNOHANG) > 0)
		;
	errno = save_errno;
}
