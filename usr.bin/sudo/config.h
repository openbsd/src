/*	$OpenBSD: config.h,v 1.3 2004/11/29 17:29:04 millert Exp $	*/

#ifndef _SUDO_CONFIG_H
#define _SUDO_CONFIG_H

/*
 * configure --prefix=/usr --with-insults --with-bsdauth \
 *	     --with-env-editor --disable-path-info --with-logfac=authpriv
 */

#define HAVE_ASPRINTF
#define HAVE_BSD_AUTH_H
#define HAVE_CLOSEFROM
#define HAVE_DIRENT_H
#define HAVE_ERR_H
#define HAVE_FNMATCH
#define HAVE_FREEIFADDRS
#define HAVE_FSTAT
#define HAVE_FUTIMES
#define HAVE_GETCWD
#define HAVE_GETDOMAINNAME
#define HAVE_GETIFADDRS
#define HAVE_GETTIMEOFDAY
#define HAVE_INITGROUPS
#define HAVE_INNETGR
#define HAVE_INTTYPES_H
#define HAVE_ISBLANK
#define HAVE_LOCKF
#define HAVE_LOGIN_CAP_H
#define HAVE_LSEARCH
#define HAVE_MEMCPY
#define HAVE_MEMSET
#define HAVE_NETGROUP_H
#define HAVE_SETRESUID
#define HAVE_SETRLIMIT
#define HAVE_SIGACTION
#define HAVE_SIG_ATOMIC_T
#define HAVE_SNPRINTF
#define HAVE_STDLIB_H
#define HAVE_STRCASECMP
#define HAVE_STRCHR
#define HAVE_STRERROR
#define HAVE_STRFTIME
#define HAVE_STRING_H
#define HAVE_STRLCAT
#define HAVE_STRLCPY
#define HAVE_STRRCHR
#define HAVE_ST_MTIMESPEC
#define HAVE_SYS_SELECT_H
#define HAVE_SYS_STAT_H
#define HAVE_SYS_TYPES_H
#define HAVE_TERMIOS_H
#define HAVE_TIMESPEC
#define HAVE_TZSET
#define HAVE_UNISTD_H
#define HAVE_UTIMES
#define HAVE_VASPRINTF
#define HAVE_VSNPRINTF
#define HAVE___PROGNAME

#define CLASSIC_INSULTS 1
#define CSOPS_INSULTS 1
#define DONT_LEAK_PATH_INFO 1
#define EDITOR _PATH_VI
#define ENV_EDITOR 1
#define INCORRECT_PASSWORD "Sorry, try again."
#define LOGFAC "authpriv"
#define LOGGING SLOG_SYSLOG
#define MAILSUBJECT "*** SECURITY information for %h ***"
#define MAILTO "root"
#define MAXLOGFILELEN 80
#define MAX_UID_T_LEN 10
#define PASSPROMPT "Password:"
#define PASSWORD_TIMEOUT 5
#define PRI_FAILURE "alert"
#define PRI_SUCCESS "notice"
#define RETSIGTYPE void
#define RUNAS_DEFAULT "root"
#define SEND_MAIL_WHEN_NO_USER 1
#define STDC_HEADERS 1
#define SUDO_UMASK 0022
#define	SUDOERS_UID 0
#define	SUDOERS_GID 0
#define	SUDOERS_MODE 0440
#define TIMEOUT 5
#define TRIES_FOR_PASSWORD 3
#define USE_INSULTS 1
#define VOID void
#define WITHOUT_PASSWD 1

#define sudo_waitpid(p, s, o)	waitpid(p, s, o)
#define	stat_sudoers	lstat
#define EXECV	execvp

#define mtim_getsec(_x)		((_x).st_mtimespec.tv_sec)
#define mtim_getnsec(_x)	((_x).st_mtimespec.tv_nsec)

#undef SET
#define	SET(t, f)	((t) |= (f))
#undef CLR
#define	CLR(t, f)	((t) &= ~(f))
#undef ISSET
#define	ISSET(t, f)	((t) & (f))

#endif /* _SUDO_CONFIG_H */
