/*	$OpenBSD: newsyslog.c,v 1.27 1999/11/07 05:31:53 millert Exp $	*/

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

Copyright 1988, 1989 by the Massachusetts Institute of Technology

Permission to use, copy, modify, and distribute this software
and its documentation for any purpose and without fee is
hereby granted, provided that the above copyright notice
appear in all copies and that both that copyright notice and
this permission notice appear in supporting documentation,
and that the names of M.I.T. and the M.I.T. S.I.P.B. not be
used in advertising or publicity pertaining to distribution
of the software without specific, written prior permission.
M.I.T. and the M.I.T. S.I.P.B. make no representations about
the suitability of this software for any purpose.  It is
provided "as is" without express or implied warranty.

*/

/*
 *      newsyslog - roll over selected logs at the appropriate time,
 *              keeping the a specified number of backup files around.
 *
 */

#ifndef lint
static char rcsid[] = "$OpenBSD: newsyslog.c,v 1.27 1999/11/07 05:31:53 millert Exp $";
#endif /* not lint */

#ifndef CONF
#define CONF "/etc/athena/newsyslog.conf" /* Configuration file */
#endif
#ifndef PIDFILE
#define PIDFILE "/etc/syslog.pid"
#endif
#ifndef COMPRESS
#define COMPRESS "/usr/ucb/compress" /* File compression program */
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

#include <stdio.h>
#include <sys/types.h>
#include <sys/time.h>
#include <sys/stat.h>
#include <sys/param.h>
#include <sys/wait.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <signal.h>
#include <fcntl.h>
#include <pwd.h>
#include <grp.h>
#include <unistd.h>
#include <err.h>

#define CE_ROTATED	0x01		/* Log file has been rotated */
#define CE_COMPACT	0x02		/* Compact the achived log files */
#define CE_BINARY	0x04		/* Logfile is in binary, don't add */
					/* status messages */
#define CE_MONITOR	0x08		/* Monitory for changes */
#define NONE -1
        
struct conf_entry {
        char    *log;           /* Name of the log */
        uid_t   uid;            /* Owner of log */
        gid_t   gid;            /* Group of log */
        int     numlogs;        /* Number of logs to keep */
        int     size;           /* Size cutoff to trigger trimming the log */
        int     hours;          /* Hours between log trimming */
        int     permissions;    /* File permissions on the log */
        int     flags;          /* Flags (CE_COMPACT & CE_BINARY)  */
	char	*whom;		/* Whom to notify if logfile changes */
	char	*pidfile;	/* Path to file containg pid to HUP */
        struct conf_entry       *next; /* Linked list pointer */
};

int     verbose = 0;            /* Print out what's going on */
int     needroot = 1;           /* Root privs are necessary */
int     noaction = 0;           /* Don't do anything, just show it */
int	monitor = 0;		/* Don't do monitoring by default */
char    *conf = CONF;           /* Configuration file to use */
time_t  timenow;
#define MIN_PID		4
char    hostname[MAXHOSTNAMELEN]; /* hostname */
char    *daytime;               /* timenow in human readable form */


void do_entry __P((struct conf_entry *));
void PRS __P((int, char **));
void usage __P((void));
struct conf_entry *parse_file __P((void));
char *missing_field __P((char *, char *));
void dotrim __P((char *, int, int, int, uid_t, gid_t));
int log_trim __P((char *));
void compress_log __P((char *));
int sizefile __P((char *));
int age_old_log __P((char *));
char *sob __P((char *));
char *son __P((char *));
int isnumberstr __P((char *));
void domonitor __P((char *, char *));
FILE *openmail __P((void));
void closemail __P((FILE *));
void child_killer __P((int));
void send_hup __P((char *));

int
main(argc, argv)
        int argc;
        char **argv;
{
        struct conf_entry *p, *q;
	int status;
        
        PRS(argc, argv);
        if (needroot && getuid() && geteuid())
		errx(1, "You must be root.");
        p = q = parse_file();
	signal(SIGCHLD, child_killer);

	/* Step 1, rotate all log files */
        while (q) {
                do_entry(q);
                q = q->next;
        }

	/* Step 2, send a HUP to relevant processes */
	/* XXX - should avoid HUP'ing the same process multiple times */
	q = p;
        while (q) {
		if (q->flags & CE_ROTATED)
			send_hup(q->pidfile);
                q = q->next;
        }

	/* Step 3, compress the log.0 file if configured to do so and free */
        while (p) {
		if ((p->flags & CE_COMPACT) && (p->flags & CE_ROTATED))
			compress_log(p->log);
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
do_entry(ent)
        struct conf_entry       *ent;
        
{
	int	modtime, size;

	if (verbose)
		printf("%s <%d%s>: ", ent->log, ent->numlogs,
			(ent->flags & CE_COMPACT) ? "Z" : "");
        size = sizefile(ent->log);
        modtime = age_old_log(ent->log);
        if (size < 0) {
                if (verbose)
                        printf("does not exist.\n");
        } else {
                if (verbose && (ent->size > 0))
                        printf("size (Kb): %d [%d] ", size, ent->size);
                if (verbose && (ent->hours > 0))
                        printf(" age (hr): %d [%d] ", modtime, ent->hours);
		if (monitor && ent->flags & CE_MONITOR)
			domonitor(ent->log, ent->whom);
                if (!monitor && (((ent->size > 0) && (size >= ent->size)) ||
                    ((ent->hours > 0) && ((modtime >= ent->hours)
                                        || (modtime < 0))))) {
                        if (verbose)
                                printf("--> trimming log....\n");
			if (noaction && !verbose)
				printf("%s <%d%s>: ", ent->log, ent->numlogs,
					(ent->flags & CE_COMPACT) ? "Z" : "");
                        dotrim(ent->log, ent->numlogs, ent->flags,
                               ent->permissions, ent->uid, ent->gid);
			ent->flags |= CE_ROTATED;
                } else if (verbose)
			printf("--> skipping\n");
        }
}

/* Send a HUP to the pid specified by pidfile */
void
send_hup(pidfile)
	char	*pidfile;
{
	FILE	*f;
        char    line[BUFSIZ];
	pid_t	pid = 0;

        if ((f = fopen(pidfile, "r")) == NULL) {
		warn("can't open %s", pidfile);
		return;
	}

	if (fgets(line, sizeof(line), f))
		pid = atoi(line);
	(void)fclose(f);

        if (noaction)
                (void)printf("kill -HUP %d\n", pid);
	else if (pid == 0)
		warnx("empty pid file: %s", pidfile);
	else if (pid < MIN_PID)
		warnx("preposterous process number: %d", pid);
	else if (kill(pid, SIGHUP))
		warnx("warning - could not HUP daemon");
}

void
PRS(argc, argv)
        int argc;
        char **argv;
{
        int     c;
	char	*p;

        timenow = time(NULL);
        daytime = ctime(&timenow) + 4;
        daytime[15] = '\0';

        /* Let's get our hostname */
        (void)gethostname(hostname, sizeof(hostname));

	/* Truncate domain */
	p = strchr(hostname, '.');
	if (p)
		*p = '\0';

        optind = 1;             /* Start options parsing */
        while ((c = getopt(argc, argv, "nrvmf:t:")) != -1) {
                switch (c) {
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
			monitor++;
			break;
                default:
                        usage();
                }
        }
}

void
usage()
{
	extern const char *__progname;

	(void)fprintf(stderr, "usage: %s [-nrvm] [-f config-file]\n",
	    __progname);
	exit(1);
}

/* Parse a configuration file and return a linked list of all the logs
 * to process
 */
struct conf_entry *
parse_file()
{
        FILE    *f;
        char    line[BUFSIZ], *parse, *q;
        char    *errline, *group, *tmp;
        struct conf_entry *first = NULL;
        struct conf_entry *working;
        struct passwd *pass;
        struct group *grp;

        if (strcmp(conf, "-") == 0)
                f = stdin;
	else {
                if ((f = fopen(conf, "r")) == NULL)
			err(1, "can't open %s", conf);
	}

        while (fgets(line, sizeof(line), f)) {
                if ((line[0] == '\n') || (line[0] == '#'))
                        continue;
                errline = strdup(line);
		if (errline == NULL)
			err(1, "strdup");
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

                q = parse = missing_field(sob(++parse), errline);
                *(parse = son(parse)) = '\0';
		if ((group = strchr(q, '.')) != NULL) {
			*group++ = '\0';
			if (*q) {
				if (!(isnumberstr(q))) {
					if ((pass = getpwnam(q)) == NULL)
						errx(1, "Error in config file; unknown user: %s", q);
					working->uid = pass->pw_uid;
				} else
					working->uid = atoi(q);
			} else
				working->uid = NONE;
			
			q = group;
			if (*q) {
				if (!(isnumberstr(q))) {
					if ((grp = getgrnam(q)) == NULL)
						errx(1, "Error in config file; unknown group: %s", q);
					working->gid = grp->gr_gid;
				} else
					working->gid = atoi(q);
			} else
				working->gid = NONE;
			
			q = parse = missing_field(sob(++parse), errline);
			*(parse = son(parse)) = '\0';
		} else 
			working->uid = working->gid = NONE;

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

                q = parse = sob(++parse); /* Optional field */
                *(parse = son(parse)) = '\0';
                working->flags = 0;
                while (q && *q && !isspace(*q)) {
                        if ((*q == 'Z') || (*q == 'z'))
                                working->flags |= CE_COMPACT;
                        else if ((*q == 'B') || (*q == 'b'))
                                working->flags |= CE_BINARY;
			else if ((*q == 'M') || (*q == 'm'))
				working->flags |= CE_MONITOR;
                        else
				errx(1, "Illegal flag in config file: %c", *q);
                        q++;
                }

		working->whom = NULL;
		if (working->flags & CE_MONITOR) {	/* Optional field */
			q = parse = sob(++parse);
			*(parse = son(parse)) = '\0';

			working->whom = strdup(q);
			if (working->log == NULL)
				err(1, "strdup");
		}

		working->pidfile = PIDFILE;
                q = parse = sob(++parse); /* Optional field */
                *(parse = son(parse)) = '\0';
		if (q && *q != '\0') {
			if (strlen(q) >= MAXPATHLEN)
				errx(1, "%s: pathname too long", q);
			working->pidfile = strdup(q);
			if (working->pidfile == NULL)
				err(1, "strdup");
		}

		/* Make sure we can't oflow MAXPATHLEN */
		if (asprintf(&tmp, "%s.%d%s", working->log, working->numlogs,
		    COMPRESS_POSTFIX) >= MAXPATHLEN)
			errx(1, "%s: pathname too long", working->log);
                
                free(tmp);
                free(errline);
        }
        if (working)
                working->next = NULL;
        (void)fclose(f);
        return(first);
}

char *
missing_field(p, errline)
        char    *p;
	char    *errline;
{
        if (!p || !*p) {
		warnx("Missing field in config file line:");
                fputs(errline, stderr);
                exit(1);
        }
        return(p);
}

void
dotrim(log, numdays, flags, perm, owner_uid, group_gid)
        char    *log;
        int     numdays;
        int     flags;
        int     perm;
        uid_t   owner_uid;
        gid_t   group_gid;
{
        char    file1[MAXPATHLEN], file2[MAXPATHLEN];
        char    zfile1[MAXPATHLEN], zfile2[MAXPATHLEN];
        int     fd;
        struct  stat st;
	int	days = numdays;

        /* Remove oldest log (may not exist) */
        (void)sprintf(file1, "%s.%d", log, numdays);
        (void)strcpy(zfile1, file1);
        (void)strcat(zfile1, COMPRESS_POSTFIX);
        if (noaction) {
                printf("rm -f %s %s\n", file1, zfile1);
        } else {
                (void)unlink(file1);
                (void)unlink(zfile1);
        }

        /* Move down log files */
        while (numdays--) {
                (void)strcpy(file2, file1);
                (void)sprintf(file1, "%s.%d", log, numdays);
                (void)strcpy(zfile1, file1);
                (void)strcpy(zfile2, file2);
                if (lstat(file1, &st)) {
                        (void)strcat(zfile1, COMPRESS_POSTFIX);
                        (void)strcat(zfile2, COMPRESS_POSTFIX);
                        if (lstat(zfile1, &st))
                        	continue;
                }
                if (noaction) {
                        printf("mv %s %s\n", zfile1, zfile2);
                        printf("chmod %o %s\n", perm, zfile2);
                        printf("chown %d:%d %s\n",
                               owner_uid, group_gid, zfile2);
                } else {
                        if (rename(zfile1, zfile2))
				warn("can't mv %s to %s", zfile1, zfile2);
                        if (chmod(zfile2, perm))
				warn("can't chmod %s", zfile2);
                        if (chown(zfile2, owner_uid, group_gid))
				warn("can't chown %s", zfile2);
                }
        }
        if (!noaction && !(flags & CE_BINARY))
                (void)log_trim(log);  /* Report the trimming to the old log */

	(void)snprintf(file2, sizeof(file2), "%s.XXXXXXXXXX", log);
        if (noaction)  {
                printf("Create new log file...\n");
        } else {
                if ((fd = mkstemp(file2)) < 0)
			err(1, "can't start '%s' log", file2);
                if (fchown(fd, owner_uid, group_gid))
			err(1, "can't chown '%s' log file", file2);
                if (fchmod(fd, perm))
			err(1, "can't chmod '%s' log file", file2);
                (void)close(fd);
		/* Add status message */
                if (!(flags & CE_BINARY) && log_trim(file2))
			err(1, "can't add status message to log '%s'", file2);
        }

	if (days == 0) {
		if (noaction)
			printf("rm %s\n", log);
		else if (unlink(log))
			warn("can't rm %s", log);
	} else {
		if (noaction) 
	                printf("mv %s to %s\n", log, file1);
	        else if (rename(log, file1))
			warn("can't to mv %s to %s", log, file1);
	}

	/* Now move the new log file into place */
	if (noaction)
		printf("mv %s to %s\n", file2, log);
	else if (rename(file2, log))
		warn("can't to mv %s to %s", file2, log);
}

/* Log the fact that the logs were turned over */
int
log_trim(log)
	char    *log;
{
        FILE    *f;

        if ((f = fopen(log, "a")) == NULL)
                return(-1);
        (void)fprintf(f, "%s %s newsyslog[%d]: logfile turned over\n",
	    daytime, hostname, getpid());
        if (fclose(f) == EOF)
                err(1, "log_trim: fclose");
        return(0);
}

/* Fork off compress or gzip to compress the old log file */
void
compress_log(log)
        char    *log;
{
        pid_t   pid;
	char	*base;
        char    tmp[MAXPATHLEN];
        
	if ((base = strrchr(COMPRESS, '/')) == NULL)
		base = COMPRESS;
	else
		base++;
	if (noaction) {
		printf("%s %s.0\n", base, log);
		return;
	}
        pid = fork();
        (void)sprintf(tmp, "%s.0", log);
        if (pid < 0) {
		err(1, "fork");
        } else if (!pid) {
                (void)execl(COMPRESS, base, "-f", tmp, 0);
		warn(COMPRESS);
		_exit(1);
        }
}

/* Return size in kilobytes of a file */
int
sizefile(file)
        char    *file;
{
        struct stat sb;

        if (stat(file, &sb) < 0)
                return(-1);
        return(sb.st_blocks / (1024.0 / DEV_BSIZE));
}

/* Return the age (in hours) of old log file (file.0), or -1 if none */
int
age_old_log(file)
        char    *file;
{
        struct stat sb;
        char tmp[MAXPATHLEN];

        (void)strcpy(tmp, file);
        if (stat(strcat(tmp, ".0"), &sb) < 0)
            if (stat(strcat(tmp, COMPRESS_POSTFIX), &sb) < 0)
                return(-1);
        return( (int) (timenow - sb.st_mtime + 1800) / 3600);
}

/* Skip Over Blanks */
char *
sob(p)
        register char   *p;
{
        while (p && *p && isspace(*p))
                p++;
        return(p);
}

/* Skip Over Non-Blanks */
char *
son(p)
        register char   *p;
{
        while (p && *p && !isspace(*p))
                p++;
        return(p);
}

/* Check if string is actually a number */
int
isnumberstr(string)
	char *string;
{
        while (*string) {
            if (!isdigit(*string++))
		return(0);
        }
        return(1);
}

void
domonitor(log, whom)
	char *log, *whom;
{
	struct stat sb, tsb;
	char *fname, *flog, *p, *rb = NULL;
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
	fname = (char *) malloc(sizeof(STATS_DIR) + strlen(flog) + 16);
	if (fname == NULL)
		err(1, "malloc");

	sprintf(fname, "%s/newsyslog.%s.size", STATS_DIR, flog);

	/* ..if it doesn't exist, simply record the current size. */
	if ((sb.st_size == 0) || stat(fname, &tsb) < 0)
		goto update;

	fp = fopen(fname, "r");
	if (fp == NULL) {
		warn(fname);
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
			warn(log);
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

		closemail(fp);
	}
update:
	/* Reopen for writing and update file. */
	fp = fopen(fname, "w");
	if (fp == NULL) {
		warn(fname);
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
	free(fname);
	if (rb != NULL)
		free(rb);
}

FILE *
openmail()
{
	char *cmdbuf;
	FILE *ret;

	cmdbuf = (char *) malloc(sizeof(SENDMAIL) + 3);
	if (cmdbuf == NULL)
		return(NULL);

	sprintf(cmdbuf, "%s -t", SENDMAIL);
	ret = popen(cmdbuf, "w");

	free(cmdbuf);
	return(ret);
}

void
closemail(pfp)
	FILE *pfp;
{
	pclose(pfp);
}

void
child_killer(signum)
	int signum;
{
	int status;

	while (waitpid(-1, &status, WNOHANG) > 0)
		;
}
