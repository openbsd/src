/*	$OpenBSD: sudo.h,v 1.6 1998/09/15 02:42:45 millert Exp $	*/

/*
 * CU sudo version 1.5.6 (based on Root Group sudo version 1.1)
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
 *  $From: sudo.h,v 1.126 1998/09/07 02:51:05 millert Exp $
 */

#ifndef _SUDO_SUDO_H
#define _SUDO_SUDO_H

#include <pathnames.h>
#include "compat.h"

/*
 * IP address and netmask pairs for checking against local interfaces.
 */
struct interface {
    struct in_addr addr;
    struct in_addr netmask;
};

/*
 * Data structure used in parsing sudoers;
 * top of stack values are the ones that
 * apply when parsing is done & can be
 * accessed by *_matches macros
 */
#define STACKINCREMENT (32)
struct matchstack {
	int user;
	int cmnd;
	int host;
	int runas;
	int nopass;
};

/*
 * Data structure describing a command in the
 * sudoers file.
 */
struct sudo_command {
    char *cmnd;
    char *args;
};


extern struct matchstack *match;
extern int top;

#define user_matches	(match[top-1].user)
#define cmnd_matches	(match[top-1].cmnd)
#define host_matches	(match[top-1].host)
#define runas_matches	(match[top-1].runas)
#define no_passwd	(match[top-1].nopass)

/*
 * Structure containing command matches if "sudo -l" is used.
 */
struct command_match {
    char *runas;
    size_t runas_len;
    size_t runas_size;
    char *cmnd;
    size_t cmnd_len;
    size_t cmnd_size;
    int nopasswd;
};

/*
 * Structure containing Cmnd_Alias's if "sudo -l" is used.
 */
struct generic_alias {
    char *alias;
    char *entries;
    size_t entries_size;
    size_t entries_len;
};

/*
 * Maximum number of characters to log per entry.  The syslogger
 * will log this much, after that, it truncates the log line.
 * We need this here to make sure that we continue with another
 * syslog(3) call if the internal buffer is moe than 1023 characters.
 */
#ifndef MAXSYSLOGLEN
#  define MAXSYSLOGLEN		960
#endif

#define SLOG_SYSLOG              0x01
#define SLOG_FILE                0x02
#define SLOG_BOTH                0x03

#define VALIDATE_OK              0x00
#define VALIDATE_NO_USER         0x01
#define VALIDATE_NOT_OK          0x02
#define VALIDATE_OK_NOPASS       0x03
#define VALIDATE_ERROR          -1

/*
 *  the arguments passed to log_error() are ANDed with GLOBAL_PROBLEM
 *  If the result is TRUE, the argv is NOT logged with the error message
 */
#define GLOBAL_PROBLEM           0x20
#define ALL_SYSTEMS_GO           0x00
#define GLOBAL_NO_PW_ENT         ( 0x01 | GLOBAL_PROBLEM )
#define GLOBAL_NO_SPW_ENT        ( 0x02 | GLOBAL_PROBLEM )
#define GLOBAL_NO_HOSTNAME       ( 0x03 | GLOBAL_PROBLEM )
#define GLOBAL_HOST_UNREGISTERED ( 0x04 | GLOBAL_PROBLEM )
#define PASSWORD_NOT_CORRECT     0x05
#define PASSWORDS_NOT_CORRECT    0x06
#define NO_SUDOERS_FILE          ( 0x07 | GLOBAL_PROBLEM )
#define BAD_SUDOERS_FILE         ( 0x08 | GLOBAL_PROBLEM )
#define SUDOERS_WRONG_OWNER      ( 0x09 | GLOBAL_PROBLEM )
#define SUDOERS_WRONG_MODE       ( 0x0A | GLOBAL_PROBLEM )
#define SUDOERS_NOT_FILE         ( 0x0B | GLOBAL_PROBLEM )
#define SPOOF_ATTEMPT            0x0D
#define BAD_STAMPDIR             0x0E
#define BAD_STAMPFILE            0x0F

/*
 * Boolean values
 */
#undef TRUE
#define TRUE                     0x01
#undef FALSE
#define FALSE                    0x00

/*
 * Various modes sudo can be in (based on arguments) in octal
 */
#define MODE_RUN                 00001
#define MODE_VALIDATE            00002
#define MODE_KILL                00004
#define MODE_VERSION             00010
#define MODE_HELP                00020
#define MODE_LIST                00040
#define MODE_BACKGROUND          00100
#define MODE_SHELL               00200
#define MODE_RESET_HOME          00400

/*
 * Used with set_perms()
 */
#define PERM_ROOT                0x00
#define PERM_USER                0x01
#define PERM_FULL_USER           0x02
#define PERM_SUDOERS             0x03
#define PERM_RUNAS	         0x04

/*
 * Shortcuts for user_pw_ent
 */
#define user_name		(user_pw_ent -> pw_name)
#define user_passwd		(user_pw_ent -> pw_passwd)
#define user_uid		(user_pw_ent -> pw_uid)
#define user_gid		(user_pw_ent -> pw_gid)
#define user_shell		(user_pw_ent -> pw_shell)
#define user_dir		(user_pw_ent -> pw_dir)

/*
 * Function prototypes
 */
#define YY_DECL int yylex __P((void))

#ifndef HAVE_STRDUP
char *strdup		__P((const char *));
#endif
#ifndef HAVE_GETCWD
char *getcwd		__P((char *, size_t size));
#endif
#if !defined(HAVE_PUTENV) && !defined(HAVE_SETENV)
int putenv		__P((const char *));
#endif
char *sudo_goodpath	__P((const char *));
int sudo_setenv		__P((char *, char *));
char *tgetpass		__P((char *, int, char *, char *));
char * find_path	__P((char *));
void log_error		__P((int));
void inform_user	__P((int));
void check_user		__P((void));
int validate		__P((int));
void set_perms		__P((int, int));
void remove_timestamp	__P((void));
void load_interfaces	__P((void));
int yyparse		__P((void));
YY_DECL;


/*
 * Most of these variables are declared in main() so they don't need
 * to be extern'ed here if this is main...
 */
#ifndef MAIN
extern char host[];
extern char *shost;
extern char cwd[];
extern struct interface *interfaces;
extern int num_interfaces;
extern struct passwd *user_pw_ent;
extern char *runas_user;
extern char *tty;
extern char *cmnd;
extern char *cmnd_args;
extern char *prompt;
extern struct stat cmnd_st;
extern int Argc;
extern char **Argv;
extern int NewArgc;
extern char **NewArgv;
extern FILE *sudoers_fp;
#endif
extern int errno;

#endif /* _SUDO_SUDO_H */
