/*	$OpenBSD: options.h,v 1.9 1998/03/31 06:41:04 millert Exp $	*/

/*
 *  CU sudo version 1.5.5
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
 *  Please send bugs, changes, problems to sudo-bugs@courtesan.com
 *
 *  Id: options.h,v 1.42 1998/03/31 05:15:42 millert Exp $
 */

#ifndef _SUDO_OPTIONS_H
#define _SUDO_OPTIONS_H

/*
 * DANGER DANGER DANGER!
 * Before you change anything here read through the OPTIONS file
 * for a description of what this stuff does.
 */

/* User-configurable Sudo runtime options */

/*#define FQDN			/* expect fully qualified hosts in sudoers */
#define LOGGING SLOG_SYSLOG	/* log via SLOG_SYSLOG, SLOG_FILE, SLOG_BOTH */
#define LOGFAC LOG_AUTHPRIV	/* syslog facility for sudo to use */
#define MAXLOGFILELEN 80	/* max chars per log line (for line wrapping) */
/*#define NO_ROOT_SUDO		/* root is not allowed to use sudo */
#define ALERTMAIL "root"	/* user that gets sudo mail */
#define SEND_MAIL_WHEN_NO_USER	/* send mail when user not in sudoers file */
/*#define SEND_MAIL_WHEN_NOT_OK	/* send mail if no permissions to run command */
/*#define EXEMPTGROUP "sudo"	/* no passwd needed for users in this group */
#define ENV_EDITOR		/* visudo honors EDITOR and VISUAL envars */
#define SHORT_MESSAGE		/* short sudo message, no copyright printed */
/*#define NO_MESSAGE		/* no sudo "lecture" message */
#define TIMEOUT 5		/* minutes before sudo asks for passwd again */
#define PASSWORD_TIMEOUT 5	/* passwd prompt timeout (in minutes) */
#define TRIES_FOR_PASSWORD 3	/* number of tries to enter passwd correctly */
#define USE_INSULTS		/* insult the user for incorrect passwords */
#define CLASSIC_INSULTS		/* sudo "classic" insults--need USE_INSULTS */
/*#define HAL_INSULTS		/* 2001-like insults--must define USE_INSULTS */
/*#define GOONS_INSULTS		/* Goon Show insults--must define USE_INSULTS */
#define CSOPS_INSULTS		/* CSOps insults--must define USE_INSULTS */
#define EDITOR _PATH_VI		/* default editor to use */
#define MAILER _PATH_SENDMAIL	/* what mailer to use */
#define UMASK 0022		/* umask that the root-run prog should use */
#define INCORRECT_PASSWORD "Sorry, try again." /* message for bad passwd */
#define MAILSUBJECT "*** SECURITY information for %h ***" /* mail subject */
#define PASSPROMPT "Password:"	/* default password prompt */
/*#define IGNORE_DOT_PATH	/* ignore '.' in $PATH if it exists */
/*#define SECURE_PATH	"/bin:/usr/ucb:/usr/bin:/usr/etc:/etc" /* secure path */
/*#define USE_EXECV		/* use execv() instead of execvp() */
/*#define SHELL_IF_NO_ARGS	/* if sudo is given no arguments run a shell */
/*#define SHELL_SETS_HOME	/* -s sets $HOME to runas user's homedir */
/*#define USE_TTY_TICKETS	/* have a different ticket file for each tty */
/*#define OTP_ONLY		/* validate user via OTP (skey/opie) only */
/*#define LONG_OTP_PROMPT	/* use a two line OTP (skey/opie) prompt */
/*#define STUB_LOAD_INTERFACES	/* don't try to read ether interfaces */
#define FAST_MATCH		/* command check fails if basenames not same */
#ifndef SUDOERS_MODE
#define SUDOERS_MODE 0440	/* file mode for sudoers (octal) */
#endif /* SUDOERS_MODE */
#ifndef SUDOERS_UID
#define SUDOERS_UID 0		/* user id that owns sudoers (*not* a name) */
#endif /* SUDOERS_UID */
#ifndef SUDOERS_GID
#define SUDOERS_GID 0		/* group id that owns sudoers (*not* a name) */
#endif /* SUDOERS_GID */

/**********  You probably don't want to modify anything below here  ***********/

#ifdef USE_EXECV
#  define EXEC	execv
#else
#  define EXEC	execvp
#endif /* USE_EXECV */

/*
 * syslog(3) parameters
 */

#if (LOGGING & SLOG_SYSLOG)
#  include <syslog.h>
#  ifndef Syslog_ident
#    define Syslog_ident	"sudo"
#  endif
#  ifndef Syslog_options
#    define Syslog_options	0
#  endif
#  if !defined(Syslog_facility) && defined(LOG_NFACILITIES)
#    define Syslog_facility	LOGFAC
#  endif
#  ifndef Syslog_priority_OK
#    define Syslog_priority_OK	LOG_NOTICE
#  endif
#  ifndef Syslog_priority_NO
#    define Syslog_priority_NO	LOG_ALERT
#  endif
#endif	/* LOGGING & SLOG_SYSLOG */

#endif /* _SUDO_OPTIONS_H */
