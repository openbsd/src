/*	$OpenBSD: defs.h,v 1.9 1998/07/16 20:40:23 millert Exp $	*/

#ifndef __DEFS_H__
#define __DEFS_H__
/*
 * Copyright (c) 1983 Regents of the University of California.
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
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *	This product includes software developed by the University of
 *	California, Berkeley and its contributors.
 * 4. Neither the name of the University nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE REGENTS AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

/*
 * $From: defs.h,v 6.82 1998/03/23 23:28:25 michaelc Exp $
 * @(#)defs.h      5.2 (Berkeley) 3/20/86
 */

/*
 * POSIX settings
 */
#if	defined(_POSIX_SOURCE) || defined(__OpenBSD__)
#include <unistd.h>
#include <stdlib.h>
#endif	/* _POSIX_SOURCE */
#include <stdio.h>
#include <ctype.h>
#include <errno.h>
#include <pwd.h>
#include <grp.h>
#include <syslog.h>
#include <setjmp.h>
#include <sys/types.h>
#include <sys/param.h>
#include <sys/file.h>
#include <sys/time.h>
#include <sys/stat.h>

#include "version.h"
#include "config-def.h"
#include "config.h"
#include "config-data.h"
#include "pathnames.h"
#include "types.h"

#include <signal.h>

/*
 * This belongs in os-svr4.h but many SVR4 OS's
 * define SVR4 externel to Rdist so we put this
 * check here.
 */
#if	defined(SVR4)
#define NEED_FCNTL_H
#define NEED_UNISTD_H
#define NEED_NETDB_H
#endif	/* defined(SVR4) */

#if	defined(NEED_NETDB_H)
#include <netdb.h>
#endif	/* NEED_NETDB_H */
#if	defined(NEED_FCNTL_H)
#include <fcntl.h>
#endif	/* NEED_FCNTL_H */
#if	defined(NEED_LIMITS_H)
#include <limits.h>
#endif	/* NEED_LIMITS_H */
#if	defined(NEED_UNISTD_H)
#include <unistd.h>
#endif	/* NEED_UNISTD_H */
#if	defined(NEED_STRING_H)
#include <string.h>
#endif	/* NEED_STRING_H */

#if defined(ARG_TYPE)
#if	ARG_TYPE == ARG_STDARG
#include <stdarg.h>
#endif
#if	ARG_TYPE == ARG_VARARGS
#include <varargs.h>
#endif
#endif	/* ARG_TYPE */

	/* boolean truth */
#ifndef TRUE
#define TRUE		1
#endif
#ifndef FALSE
#define FALSE		0
#endif

	/* file modes */
#ifndef S_IXUSR
#define S_IXUSR		0000100
#endif
#ifndef S_IXGRP
#define S_IXGRP		0000010
#endif
#ifndef S_IXOTH
#define S_IXOTH		0000001
#endif

	/* lexical definitions */
#define	QUOTECHAR	160	/* quote next character */

	/* table sizes */
#define HASHSIZE	1021
#define INMAX		3500

	/* expand type definitions */
#define E_VARS		0x1
#define E_SHELL		0x2
#define E_TILDE		0x4
#define E_ALL		0x7

	/* actions for lookup() */
#define LOOKUP		0
#define INSERT		1
#define REPLACE		2

	/* Bit flag test macros */
#define IS_ON(b,f)	(b > 0 && (b & f))
#define IS_OFF(b,f)	!(IS_ON(b,f))
#define FLAG_ON(b,f)	b |= f
#define FLAG_OFF(b,f)	b &= ~(f)

/*
 * POSIX systems should already have S_* defined.
 */
#ifndef S_ISDIR
#define S_ISDIR(m) 	(((m) & S_IFMT) == S_IFDIR)
#endif
#ifndef S_ISREG
#define S_ISREG(m) 	(((m) & S_IFMT) == S_IFREG)
#endif
#ifndef S_ISLNK
#define S_ISLNK(m) 	(((m) & S_IFMT) == S_IFLNK)
#endif

#define ALLOC(x) 	(struct x *) xmalloc(sizeof(struct x))
#define A(s)		((s) ? s : "<null>")

/*
 * Environment variable names
 */
#define E_FILES		"FILES"			/* List of files */
#define E_LOCFILE	"FILE"			/* Local Filename  */
#define E_REMFILE	"REMFILE"		/* Remote Filename */
#define E_BASEFILE	"BASEFILE"		/* basename of Remote File */

/*
 * Suffix to use when saving files
 */
#ifndef SAVE_SUFFIX
#define SAVE_SUFFIX	".OLD"
#endif

/*
 * Get system error string
 */
#define SYSERR 		strerror(errno)

#define COMMENT_CHAR	'#'		/* Config file comment char */
#define CNULL		'\0'		/* NULL character */

/*
 * These are the top level protocol commands.
 */
#define C_NONE		'='		/* No command - pass cleanly */
#define C_ERRMSG	'\1'		/* Log an error message */
#define C_FERRMSG	'\2'		/* Log a fatal error message */
#define C_NOTEMSG	'\3'		/* Log a note message */
#define C_LOGMSG	'\4'		/* Log a message */
#define C_ACK		'\5'		/* Acknowledge */
#define C_SETCONFIG	'c'		/* Set configuration parameters */
#define C_DIRTARGET	'T'		/* Set target directory name */
#define C_TARGET	't'		/* Set target file name */
#define C_RECVREG	'R'		/* Receive a regular file */
#define C_RECVDIR	'D'		/* Receive a directory */
#define C_RECVSYMLINK	'K'		/* Receive a symbolic link */
#define C_RECVHARDLINK	'k'		/* Receive a hard link */
#define C_END		'E'		/* Indicate end of recieve/send */
#define C_CLEAN		'C'		/* Clean up */
#define C_QUERY		'Q'		/* Query without checking */
#define C_SPECIAL	'S'		/* Execute special command */
#define C_CMDSPECIAL	's'		/* Execute cmd special command */
#define C_CHMOD		'M'		/* Chmod a file */

#define	ack() 		(void) sendcmd(C_ACK, (char *)NULL)
#define	err() 		(void) sendcmd(C_ERRMSG, (char *)NULL)

/*
 * Session startup commands.
 */
#define S_VERSION	'V'		/* Version number */
#define S_REMOTEUSER	'R'		/* Remote user name */
#define S_LOCALUSER	'L'		/* Local user name */
#define S_END		'E'		/* End of session startup commands */

/*
 * These are the commands for "set config".
 */
#define SC_FREESPACE	's'		/* Set min free space */
#define SC_FREEFILES	'f'		/* Set min free files */
#define SC_HOSTNAME	'H'		/* Set client hostname */
#define SC_LOGGING	'L'		/* Set logging options */

/*
 * Query commands
 */
#define QC_ONNFS	'F'		/* File exists & is on a NFS */
#define QC_ONRO		'O'		/* File exists & is on a readonly fs */
#define QC_NO		'N'		/* File does not exist */
#define QC_SYM		'l'		/* File exists & is a symlink */
#define QC_YES		'Y'		/* File does exist */

/*
 * Clean commands
 */
#define CC_QUERY	'Q'		/* Query if file should be rm'ed */
#define CC_END		'E'		/* End of cleaning */
#define CC_YES		'Y'		/* File doesn't exist - remove */
#define CC_NO		'N'		/* File does exist - don't remove */

/*
 * Run Command commands
 */
#define RC_FILE		'F'		/* Name of a target file */
#define RC_COMMAND	'C'		/* Command to run */

/*
 * Name list
 */
struct namelist {		/* for making lists of strings */
	char	*n_name;
	struct	namelist *n_next;
};

/*
 * Sub command structure
 */
struct subcmd {
	short	sc_type;	/* type - INSTALL,NOTIFY,EXCEPT,SPECIAL */
	opt_t	sc_options;
	char	*sc_name;
	struct	namelist *sc_args;
	struct	subcmd *sc_next;
};

/*
 * Cmd flags
 */
#define CMD_ASSIGNED	0x01	/* This entry has been assigned */
#define CMD_CONNFAILED	0x02	/* Connection failed */
#define CMD_NOCHKNFS	0x04	/* Disable NFS checks */

/*
 * General command structure
 */
struct cmd {
	int	c_type;		/* type - ARROW,DCOLON */
	int	c_flags;	/* flags - CMD_USED,CMD_FAILED */
	char	*c_name;	/* hostname or time stamp file name */
	char	*c_label;	/* label for partial update */
	struct	namelist *c_files;
	struct	subcmd *c_cmds;
	struct	cmd *c_next;
};

/*
 * Hard link buffer information
 */
struct linkbuf {
	ino_t	inum;
	dev_t	devnum;
	int	count;
	char	pathname[BUFSIZ];
	char	src[BUFSIZ];
	char	target[BUFSIZ];
	struct	linkbuf *nextp;
};

extern char	       *optarg;		/* Option argument */
extern char	       *path_remsh;	/* Remote shell command */
extern char 		host[];		/* Host name of master copy */
extern char 	       *currenthost;	/* Name of current host */
extern char 	       *progname;	/* Name of this program */
extern char 	      **realargv;	/* Real argv */
extern int		optind;		/* Option index into argv */
extern int 		contimedout;	/* Connection timed out */
extern int 		debug;		/* Debugging flag */
extern opt_t 		defoptions;	/* Default install options */
extern int 		do_fork;	/* Should we do fork()'ing */
extern int 		errno;		/* System error number */
extern int 		isserver;	/* Acting as remote server */
extern int 		nerrs;		/* Number of errors seen */
extern int 		nflag;		/* NOP flag, don't execute commands */
extern opt_t 		options;	/* Global options */
extern int 		proto_version;	/* Protocol version number */
extern int 		realargc;	/* Real argc */
extern int		rem_r;		/* Remote file descriptor, reading */
extern int 		rem_w;		/* Remote file descriptor, writing */
extern int 		rtimeout;	/* Response time out in seconds */
extern int		setjmp_ok;	/* setjmp/longjmp flag */
extern void		mysetlinebuf();	/* set line buffering */
extern UID_T 		userid;		/* User ID of rdist user */
extern jmp_buf 		finish_jmpbuf;	/* Setjmp buffer for finish() */
extern struct group    *gr;	/* pointer to static area used by getgrent */
extern struct linkbuf  *ihead;	/* list of files with more than one link */
extern struct passwd   *pw;	/* pointer to static area used by getpwent */
#ifdef USE_STATDB
extern int 		dostatdb;
extern int 		juststatdb;
#endif /* USE_STATDB */

/*
 * System function declarations
 */
char 			       *hasmntopt();
char			       *strchr();
char		 	       *strdup();
char		 	       *strrchr();
char 			       *strtok();

/*
 * Our own declarations.
 */
char			       *exptilde();
char			       *makestr();
char	       		       *xcalloc();
char	       		       *xmalloc();
char	       		       *xrealloc();
extern char		       *xbasename();
extern char		       *getdistoptlist();
extern char		       *getgroupname();
extern char		       *getnlstr();
extern char		       *getnotifyfile();
extern char		       *getondistoptlist();
extern char		       *getusername();
extern char		       *getversion();
extern char		       *msgparseopts();
extern char		       *searchpath();
extern int			any();
extern int			init();
extern int			install();
extern int			isexec();
extern int			parsedistopts();
extern int			remline();
extern int			setfiletime();
extern int			spawn();
extern struct subcmd 	       *makesubcmd();
extern void			checkhostname();
extern void			cleanup();
extern void			complain();
extern void			docmds();
extern void			finish();
extern void			log();
extern void			logmsg();
extern void			lostconn();
extern void			markassigned();
extern void			msgprusage();
extern void			note();
extern void			runcmdspecial();
extern void			runcommand();
extern void			server();
extern void			setprogname();
extern void			sighandler();
extern void			waitup();
struct namelist		       *expand();
struct namelist		       *lookup();
struct namelist		       *makenl();
extern WRITE_RETURN_T		xwrite();

#if	defined(ARG_TYPE) && ARG_TYPE == ARG_STDARG
extern void			debugmsg(int, char *, ...);
extern void			error(char *, ...);
extern void			fatalerr(char *, ...);
extern void			message(int, char *, ...);
#ifndef HAVE_SETPROCTITLE
extern void			setproctitle(char *fmt, ...);
#endif
extern void			yyerror(char *);
#else
extern void			debugmsg();
extern void			error();
extern void			fatalerr();
extern void			message();
#ifndef HAVE_SETPROCTITLE
extern void			setproctitle();
#endif
extern void			yyerror();
#endif

#endif	/* __DEFS_H__ */
