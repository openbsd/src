/*	$OpenBSD: logging.c,v 1.10 1999/03/29 20:29:04 millert Exp $	*/

/*
 * CU sudo version 1.5.9 (based on Root Group sudo version 1.1)
 * Copyright (c) 1994,1996,1998,1999 Todd C. Miller <Todd.Miller@courtesan.com>
 *
 * This software comes with no waranty whatsoever, use at your own risk.
 *
 * Please send bugs, changes, problems to sudo-bugs@courtesan.com
 *
 */

/*
 *  sudo version 1.1 allows users to execute commands as root
 *  Copyright (C) 1991  The Root Group, Inc.
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 1, or (at your option)
 *  any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 *
 ****************************************************************
 *
 *  logging.c
 *
 *  this file supports the general logging facilities
 *  if you want to change any error messages, this is probably
 *  the place to be...
 *
 *  Jeff Nieusma   Thu Mar 21 23:39:04 MST 1991
 */

#include "config.h"

#include <stdio.h>
#ifdef STDC_HEADERS
#include <stdlib.h>
#endif /* STDC_HEADERS */
#ifdef HAVE_UNISTD_H
#include <unistd.h>
#endif /* HAVE_UNISTD_H */
#ifdef HAVE_STRING_H
#include <string.h>
#endif /* HAVE_STRING_H */
#ifdef HAVE_STRINGS_H
#include <strings.h>
#endif /* HAVE_STRINGS_H */
#include <pwd.h>
#include <signal.h>
#include <time.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/param.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <netinet/in.h>

#include "sudo.h"

#ifndef lint
static const char rcsid[] = "$Sudo: logging.c,v 1.112 1999/03/29 04:05:10 millert Exp $";
#endif /* lint */

/*
 * Prototypes for local functions
 */
static void send_mail		__P((void));
static RETSIGTYPE reapchild	__P((int));
static int appropriate		__P((int));
#ifdef BROKEN_SYSLOG
static void syslog_wrapper	__P((int, char *, char *, char *));
#endif /* BROKEN_SYSLOG */

/*
 * Globals
 */
static char *logline;
extern int errorlineno;

#ifdef BROKEN_SYSLOG
#define MAXSYSLOGTRIES		16	/* num of retries for broken syslogs */
#define SYSLOG(a,b,c,d)		syslog_wrapper(a,b,c,d)

/****************************************************************
 *
 *  syslog_wrapper()
 *
 *  This function logs via syslog w/ a priority and 3 strings args.
 *  It really shouldn't be necesary but some syslog()'s don't
 *  guarantee that the syslog() operation will succeed!
 */

static void syslog_wrapper(pri, fmt, arg1, arg2)
    int pri;
    char *fmt;
    char *arg1;
    char *arg2;
{
    int i;

    for (i = 0; i < MAXSYSLOGTRIES; i++)
	if (syslog(pri, fmt, arg1, arg2) == 0)
	    break;
}
#else
#define SYSLOG(a,b,c,d)		syslog(a,b,c,d)
#endif /* BROKEN_SYSLOG */



/**********************************************************************
 *
 *  log_error()
 *
 *  This function attempts to deliver mail to ALERTMAIL and either
 *  syslogs the error or writes it to the log file
 */

void log_error(code)
    int code;
{
    char *p;
    int count, header_length;
    time_t now;
#if (LOGGING & SLOG_FILE)
    mode_t oldmask;
    FILE *fp;
#endif /* LOGGING & SLOG_FILE */
#if (LOGGING & SLOG_SYSLOG)
    int pri = Syslog_priority_NO;	/* syslog priority, assume the worst */
    char *tmp, save;
#endif /* LOGGING & SLOG_SYSLOG */

    /*
     * length of syslog-like header info used for mail and file logs
     * is len("DDD MM HH:MM:SS : username : ") with an additional
     * len("HOST=hostname : ") if HOST_IN_LOG is defined.
     */
    header_length = 21 + strlen(user_name);
#ifdef HOST_IN_LOG
    header_length += 8 + strlen(shost);
#endif

    /*
     * Allocate enough memory for logline so we won't overflow it
     */
    count = header_length + 136 + 2 * MAXPATHLEN + strlen(tty) + strlen(cwd) +
	    strlen(runas_user);
    if (cmnd_args)
	count += strlen(cmnd_args);
    logline = (char *) emalloc(count);

    /*
     * we will skip this stuff when using syslog(3) but it is
     * necesary for mail and file logs.
     */
    now = time((time_t) 0);
    p = ctime(&now) + 4;
#ifdef HOST_IN_LOG
    (void) sprintf(logline, "%15.15s : %s : HOST=%s : ", p, user_name, shost);
#else
    (void) sprintf(logline, "%15.15s : %s : ", p, user_name);
#endif

    /*
     * we need a pointer to the end of logline for cheap appends.
     */
    p = logline + header_length;

    switch (code) {

	case ALL_SYSTEMS_GO:
	    (void) sprintf(p, "TTY=%s ; PWD=%s ; USER=%s ; COMMAND=",
	    tty, cwd, runas_user);
#if (LOGGING & SLOG_SYSLOG)
	    pri = Syslog_priority_OK;
#endif /* LOGGING & SLOG_SYSLOG */
	    break;

	case VALIDATE_NO_USER:
	    (void) sprintf(p,
		"user NOT in sudoers ; TTY=%s ; PWD=%s ; USER=%s ; COMMAND=",
		tty, cwd, runas_user);
	    break;

	case VALIDATE_NOT_OK:
	    (void) sprintf(p,
		"command not allowed ; TTY=%s ; PWD=%s ; USER=%s ; COMMAND=",
		tty, cwd, runas_user);
	    break;

	case VALIDATE_ERROR:
	    (void) sprintf(p, "error in %s, line %d ; TTY=%s ; PWD=%s ; USER=%s.  ",
		_PATH_SUDO_SUDOERS, errorlineno, tty, cwd, runas_user);
	    break;

	case GLOBAL_NO_PW_ENT:
	    (void) sprintf(p,
		"There is no passwd entry for uid %ld (TTY=%s).  ",
		(long) user_uid, tty);
	    break;

	case PASSWORD_NOT_CORRECT:
	    (void) sprintf(p,
		"password incorrect ; TTY=%s ; PWD=%s ; USER=%s ; COMMAND=",
		tty, cwd, runas_user);
	    break;

	case PASSWORDS_NOT_CORRECT:
	    (void) sprintf(p,
		"%d incorrect passwords ; TTY=%s ; PWD=%s ; USER=%s ; COMMAND=",
		TRIES_FOR_PASSWORD, tty, cwd, runas_user);
	    break;

	case GLOBAL_NO_HOSTNAME:
	    strcat(p, "This machine does not have a hostname ");
	    break;

	case NO_SUDOERS_FILE:
	    switch (errno) {
		case ENOENT:
		    (void) sprintf(p, "There is no %s file.  ",
			_PATH_SUDO_SUDOERS);
		    break;
		case EACCES:
		    (void) sprintf(p, "Can't read %s.  ", _PATH_SUDO_SUDOERS);
		    break;
		default:
		    (void) sprintf(p, "There is a problem opening %s ",
			_PATH_SUDO_SUDOERS);
		    break;
	    }
	    break;

	case GLOBAL_HOST_UNREGISTERED:
	    (void) sprintf(p, "gethostbyname() cannot find host %s ", host);
	    break;

	case SUDOERS_NOT_FILE:
	    (void) sprintf(p, "%s is not a regular file ", _PATH_SUDO_SUDOERS);
	    break;

	case SUDOERS_WRONG_OWNER:
	    (void) sprintf(p, "%s is not owned by uid %d and gid %d ",
		_PATH_SUDO_SUDOERS, SUDOERS_UID, SUDOERS_GID);
	    break;

	case SUDOERS_WRONG_MODE:
	    (void) sprintf(p, "%s is not mode %o ", _PATH_SUDO_SUDOERS,
		SUDOERS_MODE);
	    break;

	case SPOOF_ATTEMPT:
	    (void) sprintf(p,
		"probable spoofing attempt; TTY=%s ; PWD=%s ; USER=%s ; COMMAND=",
		tty, cwd, runas_user);
	    break;

	case BAD_STAMPDIR:
	    (void) sprintf(p,
	    "%s owned by non-root or not mode 0700; TTY=%s ; PWD=%s ; USER=%s ; COMMAND=",
	    _PATH_SUDO_TIMEDIR, tty, cwd, runas_user);
	    break;

	case BAD_STAMPFILE:
	    (void) sprintf(p,
		"preposterous stampfile date; TTY=%s ; PWD=%s ; USER=%s ; COMMAND=",
		tty, cwd, runas_user);
	    break;

	case BAD_ALLOCATION:
	    (void) sprintf(p,
		"allocation failure; TTY=%s ; PWD=%s ; USER=%s ; COMMAND=",
		tty, cwd, runas_user);
	    break;

#ifdef HAVE_KERB5
	case GLOBAL_KRB5_INIT_ERR:
	    (void) sprintf(p, "Could not initialize Kerberos V");
	    break;
#endif /* HAVE_KERB5 */

	default:
	    strcat(p, "found a weird error : ");
	    break;
    }


    /*
     * If this is a parse error or if the error is from load_globals()
     * don't put  argv in the message.
     */
    if (code != VALIDATE_ERROR && !(code & GLOBAL_PROBLEM)) {

	/* stuff the command into the logline */
	p = logline + strlen(logline);
	strcpy(p, cmnd);

	/* add a trailing space */
	p += strlen(cmnd);
	*p++ = ' ';
	*p = '\0';

	/* cat on command args if they exist */
	if (cmnd_args) {
	    (void) strcpy(p, cmnd_args);
	    p += strlen(cmnd_args);
	    *p++ = ' ';
	    *p = '\0';
	}
    }

#if (LOGGING & SLOG_SYSLOG)
#ifdef Syslog_facility
    openlog(Syslog_ident, Syslog_options, Syslog_facility);
#else
    openlog(Syslog_ident, Syslog_options);
#endif /* Syslog_facility */

    /*
     * Log the full line, breaking into multiple syslog(3) calls if necesary
     */
    p = &logline[header_length];	/* skip past the date, host, and user */
    for (count = 0; count < strlen(logline) / MAXSYSLOGLEN + 1; count++) {
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
		SYSLOG(pri, "%8.8s : %s", user_name, p);
	    else
		SYSLOG(pri, "%8.8s : (command continued) %s", user_name, p);

	    *tmp = save;			/* restore saved character */

	    /* eliminate leading whitespace */
	    for (p=tmp; *p != ' '; p++)
		;
	} else {
	    if (count == 0)
		SYSLOG(pri, "%8.8s : %s", user_name, p);
	    else
		SYSLOG(pri, "%8.8s : (command continued) %s", user_name, p);
	}
    }
    closelog();
#endif /* LOGGING & SLOG_SYSLOG */
#if (LOGGING & SLOG_FILE)

    /* become root */
    set_perms(PERM_ROOT, 0);

    oldmask = umask(077);
    fp = fopen(_PATH_SUDO_LOGFILE, "a");
    (void) umask(oldmask);
    if (fp == NULL) {
	(void) sprintf(logline, "Can\'t open log file: %s", _PATH_SUDO_LOGFILE);
	send_mail();
    } else {
	char *beg, *oldend, *end;
	int maxlen = MAXLOGFILELEN;

#ifndef WRAP_LOG
       (void) fprintf(fp, "%s\n", logline);
#else
	/*
	 * Print out logline with word wrap
	 */
	beg = end = logline;
	while (beg) {
	    oldend = end;
	    end = strchr(oldend, ' ');

	    if (maxlen > 0 && end) {
		*end = '\0';
		if (strlen(beg) > maxlen) {
		    /* too far, need to back up & print the line */

		    if (beg == (char *)logline)
			maxlen -= 4;		/* don't indent first line */

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

		    /* reset beg to point to the start of the new substring */
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
#endif

	(void) fclose(fp);
    }

    /* relinquish root */
    set_perms(PERM_USER, 0);
#endif /* LOGGING & SLOG_FILE */

    /* send mail if appropriate */
    if (appropriate(code))
	send_mail();
}



#ifdef _PATH_SENDMAIL
/**********************************************************************
 *
 *  send_mail()
 *
 *  This function attempts to mail to ALERTMAIL about the sudo error
 *
 */

static char *mail_argv[] = { "sendmail", "-t", (char *) NULL };

static void send_mail()
{
    char *mailer = _PATH_SENDMAIL;
    char *subject = MAILSUBJECT;
    int fd[2];
    char *p;
#ifdef POSIX_SIGNALS
    struct sigaction action;

    (void) memset((VOID *)&action, 0, sizeof(action));
#endif /* POSIX_SIGNALS */

    /* catch children as they die */
#ifdef POSIX_SIGNALS
    action.sa_handler = reapchild;
    (void) sigaction(SIGCHLD, &action, NULL);
#else
    (void) signal(SIGCHLD, reapchild);
#endif /* POSIX_SIGNALS */

    if (fork())
	return;

    /*
     * we don't want any security problems ...
     */
    set_perms(PERM_FULL_USER, 0);
    
#ifdef POSIX_SIGNALS
    action.sa_handler = SIG_IGN;
    (void) sigaction(SIGHUP, &action, NULL);
    (void) sigaction(SIGINT, &action, NULL);
    (void) sigaction(SIGQUIT, &action, NULL);
#else
    (void) signal(SIGHUP, SIG_IGN);
    (void) signal(SIGINT, SIG_IGN);
    (void) signal(SIGQUIT, SIG_IGN);
#endif /* POSIX_SIGNALS */

    if (pipe(fd)) {
	perror("send_mail: pipe");
	exit(1);
    }
    (void) dup2(fd[0], 0);
    (void) dup2(fd[1], 1);
    (void) close(fd[0]);
    (void) close(fd[1]);

    if (!fork()) {		/* child */
	(void) close(1);
	EXEC(mailer, mail_argv);

	/* this should not happen */
	perror(mailer);
	exit(1);
    } else {			/* parent */
	(void) close(0);

	/* feed the data to sendmail */
	/* XXX - do we need to fdopen this fd #1 to a new stream??? */
	(void) fprintf(stdout, "To: %s\nSubject: ", ALERTMAIL);
	p = subject;
	while (*p) {
	    /* expand %h -> hostname in subject */
	    if (*p == '%' && *(p+1) == 'h') {
		(void) fputs(host, stdout);
		p++;
	    } else
		(void) fputc(*p, stdout);
	    p++;
	}
	(void) fprintf(stdout, "\n\n%s : %s\n\n", host, logline);
	fclose(stdout);

	exit(0);
    }
}
#else
static void send_mail()
{
    /* no mailer defined */
    return;
}
#endif /* _PATH_SENDMAIL */



/****************************************************************
 *
 *  reapchild()
 *
 *  This function gets rid of all the ugly zombies
 */

static RETSIGTYPE reapchild(sig)
    int sig;
{
    int pid, status, save_errno = errno;

#ifdef sudo_waitpid
    do {
	pid = sudo_waitpid(-1, &status, WNOHANG);
    } while (pid == -1);
#else
    (void) wait(NULL);
#endif
#ifndef POSIX_SIGNALS
    (void) signal(SIGCHLD, reapchild);
#endif /* POSIX_SIGNALS */
    errno = save_errno;
}



/**********************************************************************
 *
 *  inform_user ()
 *
 *  This function lets the user know what is happening 
 *  when an error occurs
 */

void inform_user(code)
    int code;
{
    switch (code) {
	case VALIDATE_NO_USER:
	    (void) fprintf(stderr,
		"%s is not in the sudoers file.  This incident will be reported.\n\n",
		user_name);
	    break;

	case VALIDATE_NOT_OK:
	    (void) fprintf(stderr,
		"Sorry, user %s is not allowed to execute \"%s",
		user_name, cmnd);
	
	    /* print command args if they exist */
	    if (cmnd_args) {
		fputc(' ', stderr);
		fputs(cmnd_args, stderr);
	    }

	    (void) fprintf(stderr, "\" as %s on %s.\n\n", runas_user, host);
	    break;

	case VALIDATE_ERROR:
	    (void) fprintf(stderr,
		"Sorry, there is a fatal error in the sudoers file.\n\n");
	    break;

	case GLOBAL_NO_PW_ENT:
	    (void) fprintf(stderr,
		"Intruder Alert!  You don't exist in the passwd file\n\n");
	    break;

	case GLOBAL_NO_SPW_ENT:
	    (void) fprintf(stderr,
		"Intruder Alert!  You don't exist in the shadow passwd file\n\n");
	    break;

	case GLOBAL_NO_HOSTNAME:
	    (void) fprintf(stderr,
		"This machine does not have a hostname\n\n");
	    break;

	case GLOBAL_HOST_UNREGISTERED:
	    (void) fprintf(stderr,
		"This machine is not available via gethostbyname()\n\n");
	    break;

	case PASSWORD_NOT_CORRECT:
	    (void) fprintf(stderr, "Password not entered correctly\n\n");
	    break;

	case PASSWORDS_NOT_CORRECT:
	    (void) fprintf(stderr, "Password not entered correctly after %d tries\n\n",
		TRIES_FOR_PASSWORD);
	    break;

	case NO_SUDOERS_FILE:
	    switch (errno) {
		case ENOENT:
		    (void) fprintf(stderr, "There is no %s file.\n",
			_PATH_SUDO_SUDOERS);
		    break;
		default:
		    (void) fprintf(stderr, "Can't read %s: ",
			_PATH_SUDO_SUDOERS);
		    perror("");
		    break;
	    }
	    break;

	case SUDOERS_NOT_FILE:
	    (void) fprintf(stderr,
		"%s is not a regular file!\n", _PATH_SUDO_SUDOERS);
	    break;

	case SUDOERS_WRONG_OWNER:
	    (void) fprintf(stderr, "%s is not owned by uid %d and gid %d!\n",
		_PATH_SUDO_SUDOERS, SUDOERS_UID, SUDOERS_GID);
	    break;

	case SUDOERS_WRONG_MODE:
	    (void) fprintf(stderr, "%s must be mode %o!\n", _PATH_SUDO_SUDOERS,
		SUDOERS_MODE);
	    break;

	case SPOOF_ATTEMPT:
	    (void) fprintf(stderr,
		"%s is not the same command that was validated, disallowing.\n",
		cmnd);
	    break;

	case BAD_STAMPDIR:
	    (void) fprintf(stderr,
		"Timestamp directory has wrong permissions, ignoring.\n");
	    break;

	case BAD_STAMPFILE:
	    (void) fprintf(stderr,
		"Your timestamp file has a preposterous date, ignoring.\n");
	    break;

	case BAD_ALLOCATION:
	    (void) fprintf(stderr,
		"Resource allocation failure.\n");
	    break;

	default:
	    (void) fprintf(stderr,
		"Something weird happened.\n\n");
	    break;
    }
}



/****************************************************************
 *
 *  appropriate()
 *
 *  This function determines whether to send mail or not...
 */

static int appropriate(code)
    int code;
{

    switch (code) {

    /* 
     * these will NOT send mail
     */
    case VALIDATE_OK:
    case VALIDATE_OK_NOPASS:
    case PASSWORD_NOT_CORRECT:
    case PASSWORDS_NOT_CORRECT:
/*  case ALL_SYSTEMS_GO:               this is the same as OK */
	return (0);
	break;

    case VALIDATE_NO_USER:
#ifdef SEND_MAIL_WHEN_NO_USER
	return (1);
#else
	return (0);
#endif
	break;

    case VALIDATE_NOT_OK:
#ifdef SEND_MAIL_WHEN_NOT_OK
	return (1);
#else
	return (0);
#endif
	break;

    /*
     * these WILL send mail
     */
    case VALIDATE_ERROR:
    case NO_SUDOERS_FILE:
    case SPOOF_ATTEMPT:
    case BAD_STAMPDIR:
    case BAD_STAMPFILE:
    case BAD_ALLOCATION:
    default:
	return (1);
	break;

    }
}
