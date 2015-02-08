/*	$OpenBSD: newsyslog.c,v 1.94 2015/02/08 23:40:34 deraadt Exp $	*/

/*
 * Copyright (c) 1999, 2002, 2003 Todd C. Miller <Todd.Miller@courtesan.com>
 *
 * Permission to use, copy, modify, and distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 * ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
 * OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 *
 * Sponsored in part by the Defense Advanced Research Projects
 * Agency (DARPA) and Air Force Research Laboratory, Air Force
 * Materiel Command, USAF, under agreement number F39502-99-1-0512.
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
 *              keeping the specified number of backup files around.
 *
 */

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

#include <sys/param.h>	/* DEV_BSIZE */
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
#include <time.h>
#include <unistd.h>

#define CE_ROTATED	0x01		/* Log file has been rotated */
#define CE_COMPACT	0x02		/* Compact the archived log files */
#define CE_BINARY	0x04		/* Logfile is in binary, don't add */
					/* status messages */
#define CE_MONITOR	0x08		/* Monitor for changes */
#define CE_FOLLOW	0x10		/* Follow symbolic links */
#define CE_TRIMAT	0x20		/* Trim at a specific time */

#define	MIN_PID		4		/* Don't touch pids lower than this */
#define	MIN_SIZE	256		/* Don't rotate if smaller (in bytes) */

#define	DPRINTF(x)	do { if (verbose) printf x ; } while (0)

struct conf_entry {
	char    *log;		/* Name of the log */
	char    *logbase;	/* Basename of the log */
	char    *backdir;	/* Directory in which to store backups */
	uid_t   uid;		/* Owner of log */
	gid_t   gid;		/* Group of log */
	int     numlogs;	/* Number of logs to keep */
	off_t   size;		/* Size cutoff to trigger trimming the log */
	int     hours;		/* Hours between log trimming */
	time_t  trim_at;	/* Specific time at which to do trimming */
	mode_t  permissions;	/* File permissions on the log */
	int	signal;		/* Signal to send (defaults to SIGHUP) */
	int     flags;		/* Flags (CE_COMPACT & CE_BINARY)  */
	char	*whom;		/* Whom to notify if logfile changes */
	char	*pidfile;	/* Path to file containing pid to signal */
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
char	hostname[HOST_NAME_MAX+1]; /* Hostname */
char	*daytime;		/* timenow in human readable form */
char	*arcdir;		/* Dir to put archives in (if it exists) */

FILE   *openmail(void);
char   *lstat_log(char *, size_t, int);
char   *missing_field(char *, char *, int);
char   *sob(char *);
char   *son(char *);
int	age_old_log(struct conf_entry *);
int	domonitor(struct conf_entry *);
int	isnumberstr(char *);
int	log_trim(char *);
int	movefile(char *, char *, uid_t, gid_t, mode_t);
int	stat_suffix(char *, size_t, char *, struct stat *,
	    int (*)(const char *, struct stat *));
off_t	sizefile(struct stat *);
struct conf_entry *
	parse_file(int *);
time_t	parse8601(char *);
time_t	parseDWM(char *);
void	child_killer(int);
void	compress_log(struct conf_entry *);
void	do_entry(struct conf_entry *);
void	dotrim(struct conf_entry *);
void	rotate(struct conf_entry *, const char *);
void	parse_args(int, char **);
void	run_command(char *);
void	send_signal(char *, int);
void	usage(void);

int
main(int argc, char **argv)
{
	struct conf_entry *p, *q, *x, *y;
	struct pidinfo *pidlist, *pl;
	int status, listlen;
	char **av;
	
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
				warnx("%s: %s not found", conf, *av);
		}
		if (x == NULL)
			errx(1, "%s: no specified log files", conf);
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
				if ((q->pidfile && pltmp->file &&
				    strcmp(pltmp->file, q->pidfile) == 0 &&
				    pltmp->signal == q->signal) ||
				    (q->runcmd && pltmp->file &&
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
	for (pl--; pl >= pidlist; pl--) {
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
		if ((p->flags & CE_COMPACT) && (p->flags & CE_ROTATED) &&
		    p->numlogs > 0)
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
	struct stat sb;
	int modhours;
	off_t size;

	if (lstat(ent->log, &sb) != 0)
		return;
	if (!S_ISREG(sb.st_mode) &&
	    (!S_ISLNK(sb.st_mode) || !(ent->flags & CE_FOLLOW))) {
		DPRINTF(("--> not a regular file, skipping\n"));
		return;
	}
	if (S_ISLNK(sb.st_mode) && stat(ent->log, &sb) != 0) {
		DPRINTF(("--> link target does not exist, skipping\n"));
		return;
	}
	if (ent->uid == (uid_t)-1)
		ent->uid = sb.st_uid;
	if (ent->gid == (gid_t)-1)
		ent->gid = sb.st_gid;

	DPRINTF(("%s <%d%s%s%s%s>: ", ent->log, ent->numlogs,
	    (ent->flags & CE_COMPACT) ? "Z" : "",
	    (ent->flags & CE_BINARY) ? "B" : "",
	    (ent->flags & CE_FOLLOW) ? "F" : "",
	    (ent->flags & CE_MONITOR) && monitormode ? "M" : ""));
	size = sizefile(&sb);
	modhours = age_old_log(ent);
	if (ent->flags & CE_TRIMAT && !force) {
		if (timenow < ent->trim_at ||
		    difftime(timenow, ent->trim_at) >= 60 * 60) {
			DPRINTF(("--> will trim at %s",
			    ctime(&ent->trim_at)));
			return;
		} else if (ent->hours <= 0) {
			DPRINTF(("--> time is up\n"));
		}
	}
	if (ent->size > 0)
		DPRINTF(("size (KB): %.2f [%d] ", size / 1024.0,
		    (int)(ent->size / 1024)));
	if (ent->hours > 0)
		DPRINTF(("age (hr): %d [%d] ", modhours, ent->hours));
	if (monitormode && (ent->flags & CE_MONITOR) && domonitor(ent))
		DPRINTF(("--> monitored\n"));
	else if (!monitormode &&
	    (force || (ent->size > 0 && size >= ent->size) ||
	    (ent->hours <= 0 && (ent->flags & CE_TRIMAT)) ||
	    (ent->hours > 0 && (modhours >= ent->hours || modhours < 0)
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
	char line[BUFSIZ], *ep, *err;
	pid_t pid;
	long lval;
	FILE *f;

	if ((f = fopen(pidfile, "r")) == NULL) {
		warn("can't open %s", pidfile);
		return;
	}

	pid = 0;
	errno = 0;
	err = NULL;
	if (fgets(line, sizeof(line), f)) {
		lval = strtol(line, &ep, 10);
		if (line[0] == '\0' || (*ep != '\0' && *ep != '\n'))
			err = "invalid number in";
		else if (lval < 0 || (errno == ERANGE && lval == LONG_MAX))
			err = "out of range number in";
		else if (lval == 0)
			err = "no number in";
		else if (lval < MIN_PID)
			err = "preposterous process number in";
		else
			pid = (pid_t)lval;
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
		warnx("warning - could not send SIG%s to PID from pid file %s",
		    sys_signame[signal], pidfile);
}

void
parse_args(int argc, char **argv)
{
	char *p;
	int ch;

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
			noaction = 1;	/* This implies needroot as off */
			/* fall through */
		case 'r':
			needroot = 0;
			break;
		case 'v':
			verbose = 1;
			break;
		case 'f':
			conf = optarg;
			break;
		case 'm':
			monitormode = 1;
			break;
		case 'F':
			force = 1;
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
	char line[BUFSIZ], *parse, *q, *errline, *group, *tmp, *ep;
	struct conf_entry *working = NULL, *first = NULL;
	struct passwd *pwd;
	struct group *grp;
	struct stat sb;
	int lineno;
	FILE *f;
	long l;

	if (strcmp(conf, "-") == 0)
		f = stdin;
	else if ((f = fopen(conf, "r")) == NULL)
		err(1, "can't open %s", conf);

	*nentries = 0;
	for (lineno = 1; fgets(line, sizeof(line), f); lineno++) {
		tmp = sob(line);
		if (*tmp == '\0' || *tmp == '#')
			continue;
		errline = strdup(tmp);
		if (errline == NULL)
			err(1, "strdup");
		(*nentries)++;
		if (!first) {
			working = malloc(sizeof(struct conf_entry));
			if (working == NULL)
				err(1, "malloc");
			first = working;
		} else {
			working->next = malloc(sizeof(struct conf_entry));
			if (working->next == NULL)
				err(1, "malloc");
			working = working->next;
		}

		q = parse = missing_field(sob(line), errline, lineno);
		*(parse = son(line)) = '\0';
		working->log = strdup(q);
		if (working->log == NULL)
			err(1, "strdup");

		if ((working->logbase = strrchr(working->log, '/')) != NULL)
			working->logbase++;

		q = parse = missing_field(sob(++parse), errline, lineno);
		*(parse = son(parse)) = '\0';
		if ((group = strchr(q, ':')) != NULL ||
		    (group = strrchr(q, '.')) != NULL)  {
			*group++ = '\0';
			if (*q) {
				if (!(isnumberstr(q))) {
					if ((pwd = getpwnam(q)) == NULL)
						errx(1, "%s:%d: unknown user: %s",
						    conf, lineno, q);
					working->uid = pwd->pw_uid;
				} else
					working->uid = atoi(q);
			} else
				working->uid = (uid_t)-1;
			
			q = group;
			if (*q) {
				if (!(isnumberstr(q))) {
					if ((grp = getgrnam(q)) == NULL)
						errx(1, "%s:%d: unknown group: %s",
						    conf, lineno, q);
					working->gid = grp->gr_gid;
				} else
					working->gid = atoi(q);
			} else
				working->gid = (gid_t)-1;
			
			q = parse = missing_field(sob(++parse), errline, lineno);
			*(parse = son(parse)) = '\0';
		} else {
			working->uid = (uid_t)-1;
			working->gid = (gid_t)-1;
		}

		l = strtol(q, &ep, 8);
		if (*ep != '\0' || l < 0 || l > ALLPERMS)
			errx(1, "%s:%d: bad permissions: %s", conf, lineno, q);
		working->permissions = (mode_t)l;

		q = parse = missing_field(sob(++parse), errline, lineno);
		*(parse = son(parse)) = '\0';
		l = strtol(q, &ep, 10);
		if (*ep != '\0' || l < 0 || l >= INT_MAX)
			errx(1, "%s:%d: bad number: %s", conf, lineno, q);
		working->numlogs = (int)l;

		q = parse = missing_field(sob(++parse), errline, lineno);
		*(parse = son(parse)) = '\0';
		if (isdigit((unsigned char)*q))
			working->size = atoi(q) * 1024;
		else
			working->size = -1;
		
		working->flags = 0;
		q = parse = missing_field(sob(++parse), errline, lineno);
		*(parse = son(parse)) = '\0';
		l = strtol(q, &ep, 10);
		if (l < 0 || l >= INT_MAX)
			errx(1, "%s:%d: interval out of range: %s", conf,
			    lineno, q);
		working->hours = (int)l;
		switch (*ep) {
		case '\0':
			break;
		case '@':
			working->trim_at = parse8601(ep + 1);
			if (working->trim_at == (time_t) - 1)
				errx(1, "%s:%d: bad time: %s", conf, lineno, q);
			working->flags |= CE_TRIMAT;
			break;
		case '$':
			working->trim_at = parseDWM(ep + 1);
			if (working->trim_at == (time_t) - 1)
				errx(1, "%s:%d: bad time: %s", conf, lineno, q);
			working->flags |= CE_TRIMAT;
			break;
		case '*':
			if (q == ep)
				break;
			/* FALLTHROUGH */
		default:
			errx(1, "%s:%d: bad interval/at: %s", conf, lineno, q);
			break;
		}

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
					errx(1, "%s:%d: illegal flag: `%c'",
					    conf, lineno, *q);
					break;
				}
				q++;
			}
		} else
			parse--;	/* no flags so undo */

		working->pidfile = PIDFILE;
		working->signal = SIGHUP;
		working->runcmd = NULL;
		working->whom = NULL;
		for (;;) {
			q = parse = sob(++parse);	/* Optional field */
			if (q == NULL || *q == '\0')
				break;
			if (*q == '/') {
				*(parse = son(parse)) = '\0';
				if (strlen(q) >= PATH_MAX)
					errx(1, "%s:%d: pathname too long: %s",
					    conf, lineno, q);
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
					errx(1, "%s:%d: unknown signal: %s",
					    conf, lineno, q);
			} else if (working->flags & CE_MONITOR) {
				*(parse = son(parse)) = '\0';
				working->whom = strdup(q);
				if (working->whom == NULL)
					err(1, "strdup");
			} else
				errx(1, "%s:%d: unrecognized field: %s",
				    conf, lineno, q);
		}
		free(errline);

		if ((working->flags & CE_MONITOR) && working->whom == NULL)
			errx(1, "%s:%d: missing monitor notification field",
			    conf, lineno);

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

		/* Make sure we can't oflow PATH_MAX */
		if (working->backdir != NULL) {
			if (snprintf(line, sizeof(line), "%s/%s.%d%s",
			    working->backdir, working->logbase,
			    working->numlogs, COMPRESS_POSTFIX) >= PATH_MAX)
				errx(1, "%s:%d: pathname too long: %s",
				    conf, lineno, q);
		} else {
			if (snprintf(line, sizeof(line), "%s.%d%s",
			    working->log, working->numlogs, COMPRESS_POSTFIX)
			    >= PATH_MAX)
				errx(1, "%s:%d: pathname too long: %s",
				    conf, lineno, working->log);
		}
	}
	if (working)
		working->next = NULL;
	(void)fclose(f);
	return (first);
}

char *
missing_field(char *p, char *errline, int lineno)
{
	if (p == NULL || *p == '\0') {
		warnx("%s:%d: missing field", conf, lineno);
		fputs(errline, stderr);
		exit(1);
	}
	return (p);
}

void
rotate(struct conf_entry *ent, const char *oldlog)
{
	char file1[PATH_MAX], file2[PATH_MAX], *suffix;
	int numdays = ent->numlogs - 1;
	int done = 0;

	/* Remove old logs */
	do {
		(void)snprintf(file1, sizeof(file1), "%s.%d", oldlog, numdays);
		(void)snprintf(file2, sizeof(file2), "%s.%d%s", oldlog,
		    numdays, COMPRESS_POSTFIX);
		if (noaction) {
			printf("\trm -f %s %s\n", file1, file2);
			done = access(file1, 0) && access(file2, 0);
		} else {
			done = unlink(file1) && unlink(file2);
		}
		numdays++;
	} while (done == 0);

	/* Move down log files */
	for (numdays = ent->numlogs - 2; numdays >= 0; numdays--) {
		/*
		 * If both the compressed archive and the non-compressed archive
		 * exist, we decide which to rotate based on the CE_COMPACT flag
		 */
		(void)snprintf(file1, sizeof(file1), "%s.%d", oldlog, numdays);
		suffix = lstat_log(file1, sizeof(file1), ent->flags);
		if (suffix == NULL)
			continue;
		(void)snprintf(file2, sizeof(file2), "%s.%d%s", oldlog,
		    numdays + 1, suffix);

		if (noaction) {
			printf("\tmv %s %s\n", file1, file2);
			printf("\tchmod %o %s\n", ent->permissions, file2);
			printf("\tchown %u:%u %s\n", ent->uid, ent->gid, file2);
		} else {
			if (rename(file1, file2))
				warn("can't mv %s to %s", file1, file2);
			if (chmod(file2, ent->permissions))
				warn("can't chmod %s", file2);
			if (chown(file2, ent->uid, ent->gid))
				warn("can't chown %s", file2);
		}
	}
}

void
dotrim(struct conf_entry *ent)
{
	char file1[PATH_MAX], file2[PATH_MAX], oldlog[PATH_MAX];
	int fd;

	/* Is there a separate backup dir? */
	if (ent->backdir != NULL)
		snprintf(oldlog, sizeof(oldlog), "%s/%s", ent->backdir,
		    ent->logbase);
	else
		strlcpy(oldlog, ent->log, sizeof(oldlog));

	if (ent->numlogs > 0)
		rotate(ent, oldlog);
	if (!noaction && !(ent->flags & CE_BINARY))
		(void)log_trim(ent->log);

	(void)snprintf(file2, sizeof(file2), "%s.XXXXXXXXXX", ent->log);
	if (noaction)  {
		printf("\tmktemp %s\n", file2);
	} else {
		if ((fd = mkstemp(file2)) < 0)
			err(1, "can't start '%s' log", file2);
		if (fchmod(fd, ent->permissions))
			err(1, "can't chmod '%s' log file", file2);
		if (fchown(fd, ent->uid, ent->gid))
			err(1, "can't chown '%s' log file", file2);
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
		(void)snprintf(file1, sizeof(file1), "%s.0", oldlog);
		if (noaction) {
			printf("\tmv %s to %s\n", ent->log, file1);
			printf("\tchmod %o %s\n", ent->permissions, file1);
			printf("\tchown %u:%u %s\n", ent->uid, ent->gid, file1);
		} else if (movefile(ent->log, file1, ent->uid, ent->gid,
		    ent->permissions))
			warn("can't mv %s to %s", ent->log, file1);
	}

	/* Now move the new log file into place */
	if (noaction)
		printf("\tmv %s to %s\n", file2, ent->log);
	else if (rename(file2, ent->log))
		warn("can't mv %s to %s", file2, ent->log);
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
	char *base, tmp[PATH_MAX];
	pid_t pid;

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

/* Return size in bytes of a file */
off_t
sizefile(struct stat *sb)
{
	/* For sparse files, return the size based on number of blocks used. */
	if (sb->st_size / DEV_BSIZE > sb->st_blocks)
		return (sb->st_blocks * DEV_BSIZE);
	else
		return (sb->st_size);
}

/* Return the age (in hours) of old log file (file.0), or -1 if none */
int
age_old_log(struct conf_entry *ent)
{
	char file[PATH_MAX];
	struct stat sb;

	if (ent->backdir != NULL)
		(void)snprintf(file, sizeof(file), "%s/%s.0", ent->backdir,
		    ent->logbase);
	else
		(void)snprintf(file, sizeof(file), "%s.0", ent->log);
	if (ent->flags & CE_COMPACT) {
		if (stat_suffix(file, sizeof(file), COMPRESS_POSTFIX, &sb,
		    stat) < 0 && stat(file, &sb) < 0)
			return (-1);
	} else {
		if (stat(file, &sb) < 0 && stat_suffix(file, sizeof(file),
		    COMPRESS_POSTFIX, &sb, stat) < 0)
			return (-1);
	}
	return ((int)(timenow - sb.st_mtime + 1800) / 3600);
}

/* Skip Over Blanks */
char *
sob(char *p)
{
	if (p == NULL)
		return(p);
	while (isspace((unsigned char)*p))
		p++;
	return (p);
}

/* Skip Over Non-Blanks */
char *
son(char *p)
{
	while (p && *p && !isspace((unsigned char)*p))
		p++;
	return (p);
}

/* Check if string is actually a number */
int
isnumberstr(char *string)
{
	while (*string) {
		if (!isdigit((unsigned char)*string++))
			return (0);
	}
	return (1);
}

int
domonitor(struct conf_entry *ent)
{
	char fname[PATH_MAX], *flog, *p, *rb = NULL;
	struct stat sb, tsb;
	off_t osize;
	FILE *fp;
	int rd;

	if (stat(ent->log, &sb) < 0)
		return (0);

	if (noaction) {
		if (!verbose)
			printf("%s: monitored\n", ent->log);
		return (1);
	}

	flog = strdup(ent->log);
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
	if (fscanf(fp, "%lld\n", &osize) != 1) {
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
		fp = fopen(ent->log, "r");
		if (fp == NULL) {
			warn("%s", ent->log);
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
		fprintf(fp, "Auto-Submitted: auto-generated\n");
		fprintf(fp, "To: %s\nSubject: LOGFILE NOTIFICATION: %s\n\n\n",
		    ent->whom, ent->log);
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
	fprintf(fp, "%lld\n", (long long)sb.st_size);
#else
	fprintf(fp, "%ld\n", (long)sb.st_size);
#endif	/* QUAD_OFF_T */
	fclose(fp);

cleanup:
	free(flog);
	if (rb != NULL)
		free(rb);
	return (1);
}

FILE *
openmail(void)
{
	char *cmdbuf = NULL;
	FILE *ret;

	if (asprintf(&cmdbuf, "%s -t", SENDMAIL) != -1) {
		ret = popen(cmdbuf, "w");
		free(cmdbuf);
		return (ret);
	}
	return (NULL);
}

/* ARGSUSED */
void
child_killer(int signo)
{
	int save_errno = errno;
	int status;

	while (waitpid(-1, &status, WNOHANG) > 0)
		;
	errno = save_errno;
}

int
stat_suffix(char *file, size_t size, char *suffix, struct stat *sp,
    int (*func)(const char *, struct stat *))
{
	size_t n;

	n = strlcat(file, suffix, size);
	if (n < size && func(file, sp) == 0)
		return (0);
	file[n - strlen(suffix)] = '\0';
	return (-1);
}

/*
 * lstat() a log, possibly appending a suffix; order is based on flags.
 * Returns the suffix appended (may be empty string) or NULL if no file.
 */
char *
lstat_log(char *file, size_t size, int flags)
{
	struct stat sb;

	if (flags & CE_COMPACT) {
		if (stat_suffix(file, size, COMPRESS_POSTFIX, &sb, lstat) == 0)
			return (COMPRESS_POSTFIX);
		if (lstat(file, &sb) == 0)
			return ("");
	} else {
		if (lstat(file, &sb) == 0)
			return ("");
		if (stat_suffix(file, size, COMPRESS_POSTFIX, &sb, lstat) == 0)
			return (COMPRESS_POSTFIX);

	}
	return (NULL);
}

/*
 * Parse a limited subset of ISO 8601. The specific format is as follows:
 *
 * [CC[YY[MM[DD]]]][THH[MM[SS]]]	(where `T' is the literal letter)
 *
 * We don't accept a timezone specification; missing fields (including timezone)
 * are defaulted to the current date but time zero.
 */
time_t
parse8601(char *s)
{
	struct tm tm, *tmp;
	char *t;
	long l;

	tmp = localtime(&timenow);
	tm = *tmp;

	tm.tm_hour = tm.tm_min = tm.tm_sec = 0;

	l = strtol(s, &t, 10);
	if (l < 0 || l >= INT_MAX || (*t != '\0' && *t != 'T'))
		return (-1);

	/*
	 * Now t points either to the end of the string (if no time was
	 * provided) or to the letter `T' which separates date and time in
	 * ISO 8601.  The pointer arithmetic is the same for either case.
	 */
	switch (t - s) {
	case 8:
		tm.tm_year = ((l / 1000000) - 19) * 100;
		l = l % 1000000;
	case 6:
		tm.tm_year -= tm.tm_year % 100;
		tm.tm_year += l / 10000;
		l = l % 10000;
	case 4:
		tm.tm_mon = (l / 100) - 1;
		l = l % 100;
	case 2:
		tm.tm_mday = l;
	case 0:
		break;
	default:
		return (-1);
	}

	/* sanity check */
	if (tm.tm_year < 70 || tm.tm_mon < 0 || tm.tm_mon > 12 ||
	    tm.tm_mday < 1 || tm.tm_mday > 31)
		return (-1);

	if (*t != '\0') {
		s = ++t;
		l = strtol(s, &t, 10);
		if (l < 0 || l >= INT_MAX ||
		    (*t != '\0' && !isspace((unsigned char)*t)))
			return (-1);

		switch (t - s) {
		case 6:
			tm.tm_sec = l % 100;
			l /= 100;
		case 4:
			tm.tm_min = l % 100;
			l /= 100;
		case 2:
			tm.tm_hour = l;
		case 0:
			break;
		default:
			return (-1);
		}

		/* sanity check */
		if (tm.tm_sec < 0 || tm.tm_sec > 60 || tm.tm_min < 0 ||
		    tm.tm_min > 59 || tm.tm_hour < 0 || tm.tm_hour > 23)
			return (-1);
	}
	return (mktime(&tm));
}

/*-
 * Parse a cyclic time specification, the format is as follows:
 *
 *	[Dhh] or [Wd[Dhh]] or [Mdd[Dhh]]
 *
 * to rotate a logfile cyclic at
 *
 *	- every day (D) within a specific hour (hh)	(hh = 0...23)
 *	- once a week (W) at a specific day (d)     OR	(d = 0..6, 0 = Sunday)
 *	- once a month (M) at a specific day (d)	(d = 1..31,l|L)
 *
 * We don't accept a timezone specification; missing fields
 * are defaulted to the current date but time zero.
 */
time_t
parseDWM(char *s)
{
	static int mtab[] = {31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};
	int WMseen = 0, Dseen = 0, nd;
	struct tm tm, *tmp;
	char *t;
	long l;

	tmp = localtime(&timenow);
	tm = *tmp;

	/* set no. of days per month */

	nd = mtab[tm.tm_mon];

	if (tm.tm_mon == 1) {
		if (((tm.tm_year + 1900) % 4 == 0) &&
		    ((tm.tm_year + 1900) % 100 != 0) &&
		    ((tm.tm_year + 1900) % 400 == 0)) {
			nd++;	/* leap year, 29 days in february */
		}
	}
	tm.tm_hour = tm.tm_min = tm.tm_sec = 0;

	for (;;) {
		switch (*s) {
		case 'D':
			if (Dseen)
				return (-1);
			Dseen++;
			s++;
			l = strtol(s, &t, 10);
			if (l < 0 || l > 23)
				return (-1);
			tm.tm_hour = l;
			break;

		case 'W':
			if (WMseen)
				return (-1);
			WMseen++;
			s++;
			l = strtol(s, &t, 10);
			if (l < 0 || l > 6)
				return (-1);
			if (l != tm.tm_wday) {
				int save;

				if (l < tm.tm_wday) {
					save = 6 - tm.tm_wday;
					save += (l + 1);
				} else {
					save = l - tm.tm_wday;
				}

				tm.tm_mday += save;

				if (tm.tm_mday > nd) {
					tm.tm_mon++;
					tm.tm_mday = tm.tm_mday - nd;
				}
			}
			break;

		case 'M':
			if (WMseen)
				return (-1);
			WMseen++;
			s++;
			if (tolower((unsigned char)*s) == 'l') {
				tm.tm_mday = nd;
				s++;
				t = s;
			} else {
				l = strtol(s, &t, 10);
				if (l < 1 || l > 31)
					return (-1);

				if (l > nd)
					return (-1);
				if (l < tm.tm_mday)
					tm.tm_mon++;
				tm.tm_mday = l;
			}
			break;

		default:
			return (-1);
			break;
		}

		if (*t == '\0' || isspace((unsigned char)*t))
			break;
		else
			s = t;
	}
	return (mktime(&tm));
}

/*
 * Move a file using rename(2) if possible and copying if not.
 */
int
movefile(char *from, char *to, uid_t owner_uid, gid_t group_gid, mode_t perm)
{
	FILE *src, *dst;
	int i;

	/* try rename(2) first */
	if (rename(from, to) == 0) {
		if (chmod(to, perm))
			warn("can't chmod %s", to);
		if (chown(to, owner_uid, group_gid))
			warn("can't chown %s", to);
		return (0);
	} else if (errno != EXDEV)
		return (-1);

	/* different filesystem, have to copy the file */
	if ((src = fopen(from, "r")) == NULL)
		err(1, "can't fopen %s for reading", from);
	if ((dst = fopen(to, "w")) == NULL)
		err(1, "can't fopen %s for writing", to);
	if (fchmod(fileno(dst), perm))
		err(1, "can't fchmod %s", to);
	if (fchown(fileno(dst), owner_uid, group_gid))
		err(1, "can't fchown %s", to);

	while ((i = getc(src)) != EOF) {
		if ((putc(i, dst)) == EOF)
			err(1, "error writing to %s", to);
	}

	if (ferror(src))
		err(1, "error reading from %s", from);
	if ((fclose(src)) != 0)
		err(1, "can't fclose %s", from);
	if ((fclose(dst)) != 0)
		err(1, "can't fclose %s", to);
	if ((unlink(from)) != 0)
		err(1, "can't unlink %s", from);

	return (0);
}
