/*
 * Copyright (c) 1999-2000 Todd C. Miller <Todd.Miller@courtesan.com>
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
 * $Sudo: defaults.h,v 1.12 2000/01/17 23:46:25 millert Exp $
 */

#ifndef _SUDO_DEFAULTS_H
#define _SUDO_DEFAULTS_H

/*
 * Structure describing compile-time and run-time options.
 */
struct sudo_defs_types {
    char *name;
    int type;
    char *desc;
    union {
	int flag;
	char *str;
	unsigned int ival;
	mode_t mode;
    } sd_un;
};

/*
 * Four types of defaults: strings, integers, and flags.
 * Also, T_INT or T_STR may be ANDed with T_BOOL to indicate that
 * a value is not required.  Flags are boolean by nature...
 */
#undef T_INT
#define T_INT		0x001
#undef T_STR
#define T_STR		0x002
#undef T_FLAG
#define T_FLAG		0x003
#undef T_MODE
#define T_MODE		0x004
#undef T_LOGFAC
#define T_LOGFAC	0x005
#undef T_LOGPRI
#define T_LOGPRI	0x006
#undef T_PWFLAG
#define T_PWFLAG	0x007
#undef T_MASK
#define T_MASK		0x0FF
#undef T_BOOL
#define T_BOOL		0x100
#undef T_PATH
#define T_PATH		0x200

/*
 * Indexes into sudo_defs_table
 */

/* Integer versions of syslog options.  */
#define	I_LOGFAC	0	/* syslog facility */
#define	I_GOODPRI	1	/* syslog priority for successful auth */
#define	I_BADPRI	2	/* syslog priority for unsuccessful auth */

/* String versions of syslog options.  */
#define	I_LOGFACSTR	3	/* syslog facility */
#define	I_GOODPRISTR	4	/* syslog priority for successful auth */
#define	I_BADPRISTR	5	/* syslog priority for unsuccessful auth */

/* Booleans */
#define I_LONG_OTP_PROMPT	6
#define I_IGNORE_DOT		7
#define I_MAIL_ALWAYS		8
#define I_MAIL_NOUSER		9
#define I_MAIL_NOHOST		10
#define I_MAIL_NOPERMS		11
#define I_TTY_TICKETS		12
#define I_LECTURE		13
#define I_AUTHENTICATE		14
#define I_ROOT_SUDO		15
#define I_LOG_HOST		16
#define I_LOG_YEAR		17
#define I_SHELL_NOARGS		18
#define I_SET_HOME		19
#define I_PATH_INFO		20
#define I_FQDN			21
#define I_INSULTS		22
#define I_REQUIRETTY		23

/* Integer values */
#define	I_LOGLEN	24	/* wrap log file line after N chars */
#define	I_TS_TIMEOUT	25	/* timestamp stale after N minutes */
#define	I_PW_TIMEOUT	26	/* exit if pass not entered in N minutes */
#define	I_PW_TRIES	27	/* exit after N bad password tries */
#define	I_UMASK		28	/* umask to use or 0777 to use user's */

/* Strings */
#define	I_LOGFILE	29	/* path to logfile (or NULL for none) */
#define	I_MAILERPATH	30	/* path to sendmail or other mailer */
#define	I_MAILERFLAGS	31	/* flags to pass to the mailer */
#define	I_MAILTO	32	/* who to send bitch mail to */
#define	I_MAILSUB	33	/* subject line of mail msg */
#define	I_BADPASS_MSG	34	/* what to say when passwd is wrong */
#define	I_TIMESTAMPDIR	35	/* path to timestamp dir */
#define	I_EXEMPT_GRP	36	/* no password or PATH override for these */
#define	I_PASSPROMPT	37	/* password prompt */
#define	I_RUNAS_DEF	38	/* default user to run commands as */
#define	I_SECURE_PATH	39	/* set $PATH to this if not NULL */

/* Integer versions of list/verify options */
#define I_LISTPW	40
#define I_VERIFYPW	41

/* String versions of list/verify options */
#define I_LISTPWSTR	42
#define I_VERIFYPWSTR	43

/*
 * Macros for accessing sudo_defs_table.
 */
#define def_flag(_i)	(sudo_defs_table[(_i)].sd_un.flag)
#define def_ival(_i)	(sudo_defs_table[(_i)].sd_un.ival)
#define def_str(_i)	(sudo_defs_table[(_i)].sd_un.str)
#define def_mode(_i)	(sudo_defs_table[(_i)].sd_un.mode)

/*
 * Prototypes
 */
void dump_default	__P((void));
int set_default		__P((char *, char *, int));
void init_defaults	__P((void));
void list_options	__P((void));

extern struct sudo_defs_types sudo_defs_table[];

#endif /* _SUDO_DEFAULTS_H */
