/*
 * Copyright (c) 1994-1996,1998-2003 Todd C. Miller <Todd.Miller@courtesan.com>
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 *
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * 3. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission.
 *
 * 4. Products derived from this software may not be called "Sudo" nor
 *    may "Sudo" appear in their names without specific prior written
 *    permission from the author.
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
 *
 * Sponsored in part by the Defense Advanced Research Projects
 * Agency (DARPA) and Air Force Research Laboratory, Air Force
 * Materiel Command, USAF, under agreement number F39502-99-1-0512.
 */

#include "config.h"

#include <sys/types.h>
#include <sys/param.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <stdio.h>
#ifdef STDC_HEADERS
# include <stdlib.h>
# include <stddef.h>
#else
# ifdef HAVE_STDLIB_H
#  include <stdlib.h>
# endif
#endif /* STDC_HEADERS */
#ifdef HAVE_STRING_H
# include <string.h>
#else
# ifdef HAVE_STRINGS_H
#  include <strings.h>
# endif
#endif /* HAVE_STRING_H */
#ifdef HAVE_UNISTD_H
# include <unistd.h>
#endif /* HAVE_UNISTD_H */
#ifdef HAVE_ERR_H
# include <err.h>
#else
# include "emul/err.h"
#endif /* HAVE_ERR_H */
#include <pwd.h>
#include <signal.h>
#include <time.h>
#include <errno.h>

#include "sudo.h"

#ifndef lint
static const char rcsid[] = "$Sudo: logging.c,v 1.161 2003/04/16 00:42:10 millert Exp $";
#endif /* lint */

static void do_syslog		__P((int, char *));
static void do_logfile		__P((char *));
static void send_mail		__P((char *));
static void mail_auth		__P((int, char *));
static char *get_timestr	__P((void));
static void mysyslog		__P((int, const char *, ...));

#define MAXSYSLOGTRIES	16	/* num of retries for broken syslogs */

/*
 * We do an openlog(3)/closelog(3) for each message because some
 * authentication methods (notably PAM) use syslog(3) for their
 * own nefarious purposes and may call openlog(3) and closelog(3).
 * Note that because we don't want to assume that all systems have
 * vsyslog(3) (HP-UX doesn't) "%m" will not be expanded.
 * Sadly this is a maze of #ifdefs.
 */
static void
#ifdef __STDC__
mysyslog(int pri, const char *fmt, ...)
#else
mysyslog(pri, fmt, va_alist)
    int pri;
    const char *fmt;
    va_dcl
#endif
{
#ifdef BROKEN_SYSLOG
    int i;
#endif
    char buf[MAXSYSLOGLEN+1];
    va_list ap;

#ifdef __STDC__
    va_start(ap, fmt);
#else
    va_start(ap);
#endif
#ifdef LOG_NFACILITIES
    openlog("sudo", 0, def_ival(I_LOGFAC));
#else
    openlog("sudo", 0);
#endif
    vsnprintf(buf, sizeof(buf), fmt, ap);
#ifdef BROKEN_SYSLOG
    /*
     * Some versions of syslog(3) don't guarantee success and return
     * an int (notably HP-UX < 10.0).  So, if at first we don't succeed,
     * try, try again...
     */
    for (i = 0; i < MAXSYSLOGTRIES; i++)
	if (syslog(pri, "%s", buf) == 0)
	    break;
#else
    syslog(pri, "%s", buf);
#endif /* BROKEN_SYSLOG */
    va_end(ap);
    closelog();
}

/*
 * Log a message to syslog, pre-pending the username and splitting the
 * message into parts if it is longer than MAXSYSLOGLEN.
 */
static void
do_syslog(pri, msg)
    int pri;
    char *msg;
{
    size_t count;
    char *p;
    char *tmp;
    char save;

    /*
     * Log the full line, breaking into multiple syslog(3) calls if necessary
     */
    for (p = msg, count = 0; *p && count < strlen(msg) / MAXSYSLOGLEN + 1;
	count++) {
	if (strlen(p) > MAXSYSLOGLEN) {
	    /*
	     * Break up the line into what will fit on one syslog(3) line
	     * Try to break on a word boundary if possible.
	     */
	    for (tmp = p + MAXSYSLOGLEN; tmp > p && *tmp != ' '; tmp--)
		;
	    if (tmp <= p)
		tmp = p + MAXSYSLOGLEN;

	    /* NULL terminate line, but save the char to restore later */
	    save = *tmp;
	    *tmp = '\0';

	    if (count == 0)
		mysyslog(pri, "%8.8s : %s", user_name, p);
	    else
		mysyslog(pri, "%8.8s : (command continued) %s", user_name, p);

	    *tmp = save;			/* restore saved character */

	    /* Eliminate leading whitespace */
	    for (p = tmp; *p != ' ' && *p !='\0'; p++)
		;
	} else {
	    if (count == 0)
		mysyslog(pri, "%8.8s : %s", user_name, p);
	    else
		mysyslog(pri, "%8.8s : (command continued) %s", user_name, p);
	}
    }
}

static void
do_logfile(msg)
    char *msg;
{
    char *full_line;
    char *beg, *oldend, *end;
    FILE *fp;
    mode_t oldmask;
    size_t maxlen;

    oldmask = umask(077);
    maxlen = def_ival(I_LOGLINELEN) > 0 ? def_ival(I_LOGLINELEN) : 0;
    fp = fopen(def_str(I_LOGFILE), "a");
    (void) umask(oldmask);
    if (fp == NULL) {
	easprintf(&full_line, "Can't open log file: %s: %s",
	    def_str(I_LOGFILE), strerror(errno));
	send_mail(full_line);
	free(full_line);
    } else if (!lock_file(fileno(fp), SUDO_LOCK)) {
	easprintf(&full_line, "Can't lock log file: %s: %s",
	    def_str(I_LOGFILE), strerror(errno));
	send_mail(full_line);
	free(full_line);
    } else {
	if (def_ival(I_LOGLINELEN) == 0) {
	    /* Don't pretty-print long log file lines (hard to grep) */
	    if (def_flag(I_LOG_HOST))
		(void) fprintf(fp, "%s : %s : HOST=%s : %s\n", get_timestr(),
		    user_name, user_shost, msg);
	    else
		(void) fprintf(fp, "%s : %s : %s\n", get_timestr(),
		    user_name, msg);
	} else {
	    if (def_flag(I_LOG_HOST))
		easprintf(&full_line, "%s : %s : HOST=%s : %s", get_timestr(),
		    user_name, user_shost, msg);
	    else
		easprintf(&full_line, "%s : %s : %s", get_timestr(),
		    user_name, msg);

	    /*
	     * Print out full_line with word wrap
	     */
	    beg = end = full_line;
	    while (beg) {
		oldend = end;
		end = strchr(oldend, ' ');

		if (maxlen > 0 && end) {
		    *end = '\0';
		    if (strlen(beg) > maxlen) {
			/* too far, need to back up & print the line */

			if (beg == (char *)full_line)
			    maxlen -= 4;	/* don't indent first line */

			*end = ' ';
			if (oldend != beg) {
			    /* rewind & print */
			    end = oldend-1;
			    while (*end == ' ')
				--end;
			    *(++end) = '\0';
			    (void) fprintf(fp, "%s\n    ", beg);
			    *end = ' ';
			} else {
			    (void) fprintf(fp, "%s\n    ", beg);
			}

			/* reset beg to point to the start of the new substr */
			beg = end;
			while (*beg == ' ')
			    ++beg;
		    } else {
			/* we still have room */
			*end = ' ';
		    }

		    /* remove leading whitespace */
		    while (*end == ' ')
			++end;
		} else {
		    /* final line */
		    (void) fprintf(fp, "%s\n", beg);
		    beg = NULL;			/* exit condition */
		}
	    }
	    free(full_line);
	}
	(void) fflush(fp);
	(void) lock_file(fileno(fp), SUDO_UNLOCK);
	(void) fclose(fp);
    }
}

/*
 * Two main functions, log_error() to log errors and log_auth() to
 * log allow/deny messages.
 */
void
log_auth(status, inform_user)
    int status;
    int inform_user;
{
    char *message;
    char *logline;
    int pri;

    if (status & VALIDATE_OK)
	pri = def_ival(I_GOODPRI);
    else
	pri = def_ival(I_BADPRI);

    /* Set error message, if any. */
    if (status & VALIDATE_OK)
	message = "";
    else if (status & FLAG_NO_USER)
	message = "user NOT in sudoers ; ";
    else if (status & FLAG_NO_HOST)
	message = "user NOT authorized on host ; ";
    else if (status & VALIDATE_NOT_OK)
	message = "command not allowed ; ";
    else
	message = "unknown error ; ";

    easprintf(&logline, "%sTTY=%s ; PWD=%s ; USER=%s ; COMMAND=%s%s%s",
	message, user_tty, user_cwd, *user_runas, user_cmnd,
	user_args ? " " : "", user_args ? user_args : "");

    mail_auth(status, logline);		/* send mail based on status */

    /* Inform the user if they failed to authenticate.  */
    if (inform_user && (status & VALIDATE_NOT_OK)) {
	if (status & FLAG_NO_USER)
	    (void) fprintf(stderr, "%s is not in the sudoers file.  %s",
		user_name, "This incident will be reported.\n");
	else if (status & FLAG_NO_HOST)
	    (void) fprintf(stderr, "%s is not allowed to run sudo on %s.  %s",
		user_name, user_shost, "This incident will be reported.\n");
	else if (status & FLAG_NO_CHECK)
	    (void) fprintf(stderr, "Sorry, user %s may not run sudo on %s.\n",
		user_name, user_shost);
	else
	    (void) fprintf(stderr,
		"Sorry, user %s is not allowed to execute '%s%s%s' as %s on %s.\n",
		user_name, user_cmnd, user_args ? " " : "",
		user_args ? user_args : "", *user_runas, user_host);
    }

    /*
     * Log via syslog and/or a file.
     */
    if (def_str(I_SYSLOG))
	do_syslog(pri, logline);
    if (def_str(I_LOGFILE))
	do_logfile(logline);

    free(logline);
}

void
#ifdef __STDC__
log_error(int flags, const char *fmt, ...)
#else
log_error(va_alist)
    va_dcl
#endif
{
    int serrno = errno;
    char *message;
    char *logline;
    va_list ap;
#ifdef __STDC__
    va_start(ap, fmt);
#else
    int flags;
    const char *fmt;

    va_start(ap);
    flags = va_arg(ap, int);
    fmt = va_arg(ap, const char *);
#endif

    /* Become root if we are not already to avoid user control */
    if (geteuid() != 0)
	set_perms(PERM_ROOT);

    /* Expand printf-style format + args. */
    evasprintf(&message, fmt, ap);
    va_end(ap);

    if (flags & MSG_ONLY)
	logline = message;
    else if (flags & USE_ERRNO) {
	if (user_args) {
	    easprintf(&logline,
		"%s: %s ; TTY=%s ; PWD=%s ; USER=%s ; COMMAND=%s %s",
		message, strerror(serrno), user_tty, user_cwd, *user_runas,
		user_cmnd, user_args);
	} else {
	    easprintf(&logline,
		"%s: %s ; TTY=%s ; PWD=%s ; USER=%s ; COMMAND=%s", message,
		strerror(serrno), user_tty, user_cwd, *user_runas, user_cmnd);
	}
    } else {
	if (user_args) {
	    easprintf(&logline,
		"%s ; TTY=%s ; PWD=%s ; USER=%s ; COMMAND=%s %s", message,
		user_tty, user_cwd, *user_runas, user_cmnd, user_args);
	} else {
	    easprintf(&logline,
		"%s ; TTY=%s ; PWD=%s ; USER=%s ; COMMAND=%s", message,
		user_tty, user_cwd, *user_runas, user_cmnd);
	}
    }

    /*
     * Tell the user.
     */
    if (flags & USE_ERRNO)
	warn("%s", message);
    else
	warnx("%s", message);

    /*
     * Send a copy of the error via mail.
     */
    if (!(flags & NO_MAIL))
	send_mail(logline);

    /*
     * Log to syslog and/or a file.
     */
    if (def_str(I_SYSLOG))
	do_syslog(def_ival(I_BADPRI), logline);
    if (def_str(I_LOGFILE))
	do_logfile(logline);

    free(message);
    if (logline != message)
	free(logline);

    if (!(flags & NO_EXIT))
	exit(1);
}

#define MAX_MAILFLAGS	63

/*
 * Send a message to MAILTO user
 */
static void
send_mail(line)
    char *line;
{
    FILE *mail;
    char *p;
    int pfd[2];
    pid_t pid;
    sigset_t set, oset;
#ifndef NO_ROOT_MAILER
    static char *root_envp[] = {
	"HOME=/",
	"PATH=/usr/bin:/bin",
	"LOGNAME=root",
	"USER=root",
	NULL
    };
#endif

    /* Just return if mailer is disabled. */
    if (!def_str(I_MAILERPATH) || !def_str(I_MAILTO))
	return;

    (void) sigemptyset(&set);
    (void) sigaddset(&set, SIGCHLD);
    (void) sigprocmask(SIG_BLOCK, &set, &oset);

    if (pipe(pfd) == -1)
	err(1, "cannot open pipe");

    switch (pid = fork()) {
	case -1:
	    /* Error. */
	    err(1, "cannot fork");
	    break;
	case 0:
	    {
		char *argv[MAX_MAILFLAGS + 1];
		char *mpath, *mflags;
		int i;

		/* Child, set stdin to output side of the pipe */
		if (pfd[0] != STDIN_FILENO) {
		    (void) dup2(pfd[0], STDIN_FILENO);
		    (void) close(pfd[0]);
		}
		(void) close(pfd[1]);

		/* Build up an argv based the mailer path and flags */
		mflags = estrdup(def_str(I_MAILERFLAGS));
		mpath = estrdup(def_str(I_MAILERPATH));
		if ((argv[0] = strrchr(mpath, ' ')))
		    argv[0]++;
		else
		    argv[0] = mpath;

		i = 1;
		if ((p = strtok(mflags, " \t"))) {
		    do {
			argv[i] = p;
		    } while (++i < MAX_MAILFLAGS && (p = strtok(NULL, " \t")));
		}
		argv[i] = NULL;

		/* Close password file so we don't leak the fd. */
		endpwent();

		/*
		 * Depending on the config, either run the mailer as root
		 * (so user cannot kill it) or as the user (for the paranoid).
		 */
#ifndef NO_ROOT_MAILER
		set_perms(PERM_FULL_ROOT);
		execve(mpath, argv, root_envp);
#else
		set_perms(PERM_FULL_USER);
		execv(mpath, argv);
#endif /* NO_ROOT_MAILER */
		_exit(127);
	    }
	    break;
    }

    (void) close(pfd[0]);
    mail = fdopen(pfd[1], "w");

    /* Pipes are all setup, send message via sendmail. */
    (void) fprintf(mail, "To: %s\nFrom: %s\nSubject: ",
	def_str(I_MAILTO), user_name);
    for (p = def_str(I_MAILSUB); *p; p++) {
	/* Expand escapes in the subject */
	if (*p == '%' && *(p+1) != '%') {
	    switch (*(++p)) {
		case 'h':
		    (void) fputs(user_host, mail);
		    break;
		case 'u':
		    (void) fputs(user_name, mail);
		    break;
		default:
		    p--;
		    break;
	    }
	} else
	    (void) fputc(*p, mail);
    }
    (void) fprintf(mail, "\n\n%s : %s : %s : %s\n\n", user_host,
	get_timestr(), user_name, line);
    fclose(mail);

    /* If mailer is done, wait for it now.  If not, we'll get it later.  */
    reapchild(SIGCHLD);
    (void) sigprocmask(SIG_SETMASK, &oset, NULL);
}

/*
 * Send mail based on the value of "status" and compile-time options.
 */
static void
mail_auth(status, line)
    int status;
    char *line;
{
    int mail_mask;

    /* If any of these bits are set in status, we send mail. */
    if (def_flag(I_MAIL_ALWAYS))
	mail_mask =
	    VALIDATE_ERROR|VALIDATE_OK|FLAG_NO_USER|FLAG_NO_HOST|VALIDATE_NOT_OK;
    else {
	mail_mask = VALIDATE_ERROR;
	if (def_flag(I_MAIL_NO_USER))
	    mail_mask |= FLAG_NO_USER;
	if (def_flag(I_MAIL_NO_HOST))
	    mail_mask |= FLAG_NO_HOST;
	if (def_flag(I_MAIL_NO_PERMS))
	    mail_mask |= VALIDATE_NOT_OK;
    }

    if ((status & mail_mask) != 0)
	send_mail(line);
}

/*
 * SIGCHLD sig handler--wait for children as they die.
 */
RETSIGTYPE
reapchild(sig)
    int sig;
{
    int status, serrno = errno;
#ifdef sudo_waitpid
    pid_t pid;

    do {
	pid = sudo_waitpid(-1, &status, WNOHANG);
    } while (pid != 0 && (pid != -1 || errno == EINTR));
#else
    (void) wait(&status);
#endif
    errno = serrno;
}

/*
 * Return an ascii string with the current date + time
 * Uses strftime() if available, else falls back to ctime().
 */
static char *
get_timestr()
{
    char *s;
    time_t now = time((time_t) 0);
#ifdef HAVE_STRFTIME
    static char buf[128];
    struct tm *timeptr;

    timeptr = localtime(&now);
    if (def_flag(I_LOG_YEAR))
	s = "%h %e %T %Y";
    else
	s = "%h %e %T";

    /* strftime() does not guarantee to NUL-terminate so we must check. */
    buf[sizeof(buf) - 1] = '\0';
    if (strftime(buf, sizeof(buf), s, timeptr) && buf[sizeof(buf) - 1] == '\0')
	return(buf);

#endif /* HAVE_STRFTIME */

    s = ctime(&now) + 4;		/* skip day of the week */
    if (def_flag(I_LOG_YEAR))
	s[20] = '\0';			/* avoid the newline */
    else
	s[15] = '\0';			/* don't care about year */

    return(s);
}
