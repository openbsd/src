/*	$OpenBSD: config.h,v 1.6 1998/11/21 01:34:51 millert Exp $	*/

/* config.h.  Generated automatically by configure.  */
/*
 *  CU sudo version 1.5.7
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
 *  $From: config.h.in,v 1.109 1998/11/18 20:31:25 millert Exp $
 */

/*
 * config.h -- You shouldn't edit this by hand unless you are
 *             NOT using configure.
 */

#ifndef _SUDO_CONFIG_H
#define _SUDO_CONFIG_H

/* New ANSI-style OS defs.  */
#if defined(hpux) && !defined(__hpux)
#  define __hpux	1
#endif /* hpux */

#if defined(convex) && !defined(__convex__)
#  define __convex__	1
#endif /* convex */

/* Define if on AIX 3.
   System headers sometimes define this.
   We just want to avoid a redefinition error message.  */
#ifndef _ALL_SOURCE
/* #undef _ALL_SOURCE */
#endif

/* Define if on ConvexOs.
   System headers sometimes define this.
   We just want to avoid a redefinition error message.  */
#ifndef _CONVEX_SOURCE
/* #undef _CONVEX_SOURCE */
#endif

/* Define if needed to get POSIX functionality.
   System headers sometimes define this.
   We just want to avoid a redefinition error message.  */
#ifndef _POSIX_SOURCE
/* #undef _POSIX_SOURCE */
#endif

/* Define to `int' if <sys/types.h> doesn't define.  */
/* #undef uid_t */

/* Define to `int' if <sys/types.h> doesn't define.  */
/* #undef gid_t */

/* Define to `int' if <sys/types.h> doesn't define.  */
/* #undef mode_t */

/* Define to `unsigned' if <sys/types.h> doesn't define.  */
/* #undef size_t */

/* Define to `int' if <sys/types.h> doesn't define.  */
/* #undef ssize_t */

/* Define to `int' if <sys/types.h> doesn't define.  */
/* #undef dev_t */

/* Define to `unsigned int' if <sys/types.h> doesn't define.  */
/* #undef ino_t */

/* Define to be nil if C compiler doesn't support "const."  */
/* #undef const */

/* Define as the return type of signal handlers (int or void).  */
#define RETSIGTYPE void

/* Define if you have the ANSI C header files.  */
#define STDC_HEADERS 1

/* Define if you want to use the system getpass().  */
/* #undef USE_GETPASS */

/* Define if you want to use execv() instead of execvp().  */
/* #undef USE_EXECV */

/* Define if you a different ticket file for each tty.  */
/* #undef USE_TTY_TICKETS */

/* Define if you want to insult the user for entering an incorrect password.  */
#define USE_INSULTS 1

/* Define if you want the insults from the "classic" version sudo.  */
#define CLASSIC_INSULTS 1

/* Define if you want 2001-like insults.  */
/* #undef HAL_INSULTS */

/* Define if you want insults from the "Goon Show" */
/* #undef GOONS_INSULTS */

/* Define if you want insults culled from the twisted minds of CSOps.  */
#define CSOPS_INSULTS 1

/* Define to override the user's path with a builtin one.  */
/* #undef SECURE_PATH */

/* Define if you use S/Key.  */
/* #undef HAVE_SKEY */

/* Define if you use NRL OPIE.  */
/* #undef HAVE_OPIE */

/* Define if you want a two line OTP (skey/opie) prompt.  */
/* #undef LONG_OTP_PROMPT */

/* Define if you want to validate users via OTP (skey/opie) only.  */
/* #undef OTP_ONLY */

/* Define if you use SecurID.  */
/* #undef HAVE_SECURID */

/* Define if you use AIX general authentication.  */
/* #undef HAVE_AUTHENTICATE */

/* Define if you use Kerberos.  */
/* #undef HAVE_KERB4 */

/* Define if you use Kerberos.  */
/* #undef HAVE_KERB5 */

/* Keberos v5 has v4 compatibility */
#ifdef HAVE_KERB5
#  define HAVE_KERB4
#endif /* HAVE_KERB5 */

/* Define if you use SIA.  */
/* #undef HAVE_SIA */

/* Define if you use PAM.  */
/* #undef HAVE_PAM */

/* Define if you use AFS.  */
/* #undef HAVE_AFS */

/* Define if you use OSF DCE.  */
/* #undef HAVE_DCE */

/* Define if you have POSIX signals.  */
#define HAVE_SIGACTION 1
#ifdef HAVE_SIGACTION
#  define POSIX_SIGNALS
#endif /* HAVE_SIGACTION */

/* Define if you have tzset(3).  */
#define HAVE_TZSET 1

/* Define if you have getcwd(3).  */
#define HAVE_GETCWD 1

/* Define if you have strdup(3).  */
#define HAVE_STRDUP 1

/* Define if you have fnmatch(3).  */
#define HAVE_FNMATCH 1

/* Define if you have lsearch(3).  */
#define HAVE_LSEARCH 1

/* Define if you have strchr(3).  */
#define HAVE_STRCHR 1
#if !defined(HAVE_STRCHR) && !defined(strchr)
#  define strchr	index
#endif

/* Define if you have strrchr(3).  */
#define HAVE_STRRCHR 1
#if !defined(HAVE_STRRCHR) && !defined(strrchr)
#  define strrchr	rindex
#endif

/* Define if you have memcpy(3).  */
#define HAVE_MEMCPY 1
#if !defined(HAVE_MEMCPY) && !defined(memcpy)
#  define memcpy(D, S, L)	(bcopy(S, D, L))
#endif

/* Define if you have memset(3).  */
#define HAVE_MEMSET 1
#if !defined(HAVE_MEMSET) && !defined(memset)
#  define memset(S, X, N)	(bzero(S, N))
#endif

/* Define if you have sysconf(3c).  */
#define HAVE_SYSCONF 1

/* Define if you have putenv(3).  */
/* #undef HAVE_PUTENV */

/* Define if you have setenv(3).  */
#define HAVE_SETENV 1

/* Define if you have strcasecmp(3).  */
#define HAVE_STRCASECMP 1

/* Define if you have tcgetattr(3).  */
#define HAVE_TCGETATTR 1

/* Define if you have innetgr(3).  */
#define HAVE_INNETGR 1

/* Define if you have getdomainname(2).  */
#define HAVE_GETDOMAINNAME 1

/* Define if you have utime(2).  */
#define HAVE_UTIME 1

/* Define if you have a POSIX utime() (uses struct utimbuf) */
#define HAVE_UTIME_POSIX 1

/* Define if utime(file, NULL) sets timestamp to current */
#define HAVE_UTIME_NULL 1

/* Define if you have bigcrypt(3).  */
/* #undef HAVE_BIGCRYPT */

/* Define if you have set_auth_parameters(3).  */
/* #undef HAVE_SET_AUTH_PARAMETERS */

/* Define if you have initprivs(3).  */
/* #undef HAVE_INITPRIVS */

/* Define if you have dispcrypt(3).  */
/* #undef HAVE_DISPCRYPT */

/* Define if you have getspnam(3).  [SVR4-style shadow passwords] */
/* #undef HAVE_GETSPNAM */

/* Define if you have getprpwnam(3).  [SecureWare-style shadow passwords] */
/* #undef HAVE_GETPRPWNAM */

/* Define if you have iscomsec(3).  [HP-UX >= 10.x check for shadow enabled] */
/* #undef HAVE_ISCOMSEC */

/* Define if you have getspwuid(3).  [HP-UX <= 9.X shadow passwords] */
/* #undef HAVE_GETSPWUID */

/* Define if you have getpwanam(3).  [SunOS 4.x shadow passwords] */
/* #undef HAVE_GETPWANAM */

/* Define if you have issecure(3).  [SunOS 4.x check for shadow enabled] */
/* #undef HAVE_ISSECURE */

/* Define if you have getauthuid(3).  [ULTRIX 4.x shadow passwords] */
/* #undef HAVE_GETAUTHUID */

/* Define if you have seteuid(3).  */
#define HAVE_SETEUID 1

/* Define if you have waitpid(2).  */
#define HAVE_WAITPID 1

/* Define if you have wait3(2).  */
/* #undef HAVE_WAIT3 */

/* Define if you have the <malloc.h> header file.  */
/* #undef HAVE_MALLOC_H */

/* Define if you have the <alloca.h> header file.  */
/* #undef HAVE_ALLOCA_H */

/* Define if you have the <paths.h> header file.  */
#define HAVE_PATHS_H 1

/* Define if you have the <string.h> header file.  */
#define HAVE_STRING_H 1

/* Define if you have the <strings.h> header file.  */
#if !defined(__convex__) && !defined(convex)
#define HAVE_STRINGS_H 1
#endif /* convex */

/* Define your flavor of dir entry header file.  */
#define HAVE_DIRENT_H 1
/* #undef HAVE_SYS_NDIR_H */
/* #undef HAVE_SYS_DIR_H */
/* #undef HAVE_NDIR_H */

/* Define if you have the <utime.h> header file.  */
#define HAVE_UTIME_H 1

/* Define if you have the <unistd.h> header file.  */
#define HAVE_UNISTD_H 1

/* Define if you have the <fnmatch.h> header file.  */
#define HAVE_FNMATCH_H 1

/* Define if you have the <netgroup.h> header file.  */
#define HAVE_NETGROUP_H 1

/* Define if you have the <termio.h> header file.  */
/* #undef HAVE_TERMIO_H */

/* Define if you have the <termios.h> header file and tcgetattr(3).  */
#ifdef HAVE_TCGETATTR
#define HAVE_TERMIOS_H 1
#endif /* HAVE_TCGETATTR */

/* Define if you have the <sys/sockio.h> header file.  */
#define HAVE_SYS_SOCKIO_H 1

/* Define if you have the <sys/bsdtypes.h> header file.  */
/* #undef HAVE_SYS_BSDTYPES_H */

/* Define if you have the <sys/select.h> header file.  */
#define HAVE_SYS_SELECT_H 1

/* Define if your struct sockadr has an sa_len field.  */
#define HAVE_SA_LEN 1

/* Define to void if your C compiler fully groks void, else char */
#define VOID void

/* Define to the max length of a uid_t in string context (excluding the NUL) */
#define MAX_UID_T_LEN 10

/* Define if your syslog(3) does not guarantee the message will be logged */
/* and syslog(3) returns non-zero to denote failure */
/* #undef BROKEN_SYSLOG */

/* The umask that the root-run prog should use */
#define SUDO_UMASK 0022

/* Define if you want the hostname to be entered into the log file */
/* #undef HOST_IN_LOG */

/* Define if you want the log file line to be wrapped */
#define WRAP_LOG 1

/* Define to be the number of minutes before sudo asks for passwd again.  */
#define TIMEOUT 5

/* Define to be the passwd prompt timeout (in minutes).  */
#define PASSWORD_TIMEOUT 5

/* Define to be the number of tries the user gets to enter the passwd.  */
#define TRIES_FOR_PASSWORD 3

/* Define to be the user sudo should run commands as by default.  */
#define RUNAS_DEFAULT "root"

/* Define if you want to require fully qualified hosts in sudoers.  */
/* #undef FQDN */

/* If defined, users in this group need not enter a passwd (ie "sudo").  */
/* #undef EXEMPTGROUP */

/* Define to the path of the editor visudo should use. */
#define EDITOR _PATH_VI

/* Define to be the user that gets sudo mail.  */
#define ALERTMAIL "root"

/* Define to be the subject of the mail sent to ALERTMAIL by sudo.  */
#define MAILSUBJECT "*** SECURITY information for %h ***"

/* Define to be the message given for a bad password.  */
#define INCORRECT_PASSWORD "Sorry, try again."

/* Define to be the password prompt.  */
#define PASSPROMPT "Password:"

/* Define if you want visudo to honor EDITOR and VISUAL env variables.  */
#define ENV_EDITOR 1

/* Define to SLOG_SYSLOG, SLOG_FILE, or SLOG_BOTH */
#define LOGGING SLOG_SYSLOG

/* Define to be the syslog facility to use.  */
#define LOGFAC LOG_AUTHPRIV

/* Define to be the max chars per log line (for line wrapping).  */
#define MAXLOGFILELEN 80

/* Define if you want to ignore '.' and '' in $PATH */
/* #undef IGNORE_DOT_PATH */

/* Define if you want "command not allowed" instead of "command not found" */
#define DONT_LEAK_PATH_INFO 1

/* Define SHORT_MESSAGE for a short lecture or NO_MESSAGE for none.  */
#define SHORT_MESSAGE 1
/* #undef NO_MESSAGE */

/* Define SEND_MAIL_WHEN_NO_USER to send mail when user not in sudoers file */
#define SEND_MAIL_WHEN_NO_USER 1

/* Define SEND_MAIL_WHEN_NOT_OK to send mail when not allowed to run command */
/* #undef SEND_MAIL_WHEN_NOT_OK */

/* Define if you want sudo to start a shell if given no arguments.  */
/* #undef SHELL_IF_NO_ARGS */

/* Define if you want sudo to set $HOME in shell mode.  */
/* #undef SHELL_SETS_HOME */

/* Define if the code in interfaces.c does not compile for you.  */
/* #undef STUB_LOAD_INTERFACES */

/**********  You probably don't want to modify anything below here  ***********/

/*
 * Emulate a subset of waitpid() if we don't have it.
 */
#ifdef HAVE_WAITPID
#  define sudo_waitpid(p, s, o)		waitpid(p, s, o)
#else
#  ifdef HAVE_WAIT3
#    define sudo_waitpid(p, s, o)	wait3(s, o, NULL)
#  endif
#endif

#ifdef USE_EXECV
#  define EXEC	execv
#else
#  define EXEC	execvp
#endif /* USE_EXECV */

#ifdef __svr4__
#  define BSD_COMP
#endif /* __svr4__ */

#endif /* _SUDO_CONFIG_H */
