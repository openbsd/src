/*	$OpenBSD: defs.h,v 1.38 2018/09/21 19:00:45 millert Exp $	*/

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
 * 3. Neither the name of the University nor the names of its contributors
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
 * $From: defs.h,v 1.6 2001/03/12 18:16:30 kim Exp $
 * @(#)defs.h      5.2 (Berkeley) 3/20/86
 */

#include <pwd.h>
#include <setjmp.h>
#include <signal.h>

#ifndef __GNUC__
# ifndef __attribute__
#  define __attribute__(a)
# endif
#endif

#include "version.h"
#include "config.h"
#include "pathnames.h"
#include "types.h"

/*
 * Define the read and write values for the file descriptor array
 * used by pipe().
 */
#define PIPE_READ		0
#define PIPE_WRITE		1

	/* boolean truth */
#ifndef TRUE
#define TRUE		1
#endif
#ifndef FALSE
#define FALSE		0
#endif

	/* Bit flag test macros */
#define IS_ON(b,f)	(b & f)
#define IS_OFF(b,f)	!(IS_ON(b,f))
#define FLAG_ON(b,f)	b |= f
#define FLAG_OFF(b,f)	b &= ~(f)

/*
 * Environment variable names
 */
#define E_FILES		"FILES"			/* List of files */
#define E_LOCFILE	"FILE"			/* Local Filename  */
#define E_REMFILE	"REMFILE"		/* Remote Filename */
#define E_BASEFILE	"BASEFILE"		/* basename of Remote File */

/*
 * Get system error string
 */
#define SYSERR		strerror(errno)

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
#define C_END		'E'		/* Indicate end of receive/send */
#define C_CLEAN		'C'		/* Clean up */
#define C_QUERY		'Q'		/* Query without checking */
#define C_SPECIAL	'S'		/* Execute special command */
#define C_CMDSPECIAL	's'		/* Execute cmd special command */
#define C_CHMOG		'M'		/* Chown,Chgrp,Chmod a file */

#define	ack()		(void) sendcmd(C_ACK, NULL)
#define	err()		(void) sendcmd(C_ERRMSG, NULL)

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
#define SC_DEFOWNER	'o'		/* Set default owner */
#define SC_DEFGROUP	'g'		/* Set default group */

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


extern char	       *currenthost;	/* Name of current host */
extern char	       *progname;	/* Name of this program */
extern char	       *locuser;	/* Local User's name */
extern int		debug;		/* Debugging flag */
extern int		isserver;	/* Acting as remote server */
extern int		nerrs;		/* Number of errors seen */
extern opt_t		options;	/* Global options */
extern int		rem_r;		/* Remote file descriptor, reading */
extern int		rem_w;		/* Remote file descriptor, writing */
extern int		rtimeout;	/* Response time out in seconds */
extern uid_t		userid;		/* User ID of rdist user */
extern gid_t		groupid;	/* Group ID of rdist user */
extern gid_t	        gidset[];	/* List of group IDs of rdist user */
extern int		gidsetlen;	/* Number of group IDs in list */
extern jmp_buf		finish_jmpbuf;	/* Setjmp buffer for finish() */
extern char defowner[64];		/* Default owner */
extern char defgroup[64];		/* Default group */
extern volatile sig_atomic_t contimedout; /* Connection timed out */


/*
 * Declarations for files shared between rdist and rdistd
 */

/* common.c */
ssize_t xwrite(int, void *, size_t);
int init(int, char **, char **);
void finish(void);
void lostconn(void);
void coredump(void);
void sighandler(int);
int sendcmd(char, const char *, ...) __attribute__((__format__ (printf, 2, 3)));
int remline(u_char *, int, int);
ssize_t readrem(char *, ssize_t);
char *getusername(uid_t, char *, opt_t);
char *getgroupname(gid_t, char *, opt_t);
int response(void);
char *exptilde(char *, char *, size_t);
int becomeuser(void);
int becomeroot(void);
int setfiletime(char *, time_t, time_t);
char *getversion(void);
void runcommand(char *);
void *xmalloc(size_t);
void *xrealloc(void *, size_t);
void *xcalloc(size_t, size_t);
char *xstrdup(const char *);
char *xbasename(char *);
char *searchpath(char *);

/* message.c */
void msgprusage(void);
void msgprconfig(void);
char *msgparseopts(char *, int);
void checkhostname(void);
void message(int, const char *, ...) __attribute__((format (printf, 2, 3)));
void debugmsg(int, const char *, ...) __attribute__((format (printf, 2, 3)));
void error(const char *, ...) __attribute__((format (printf, 1, 2)));
void fatalerr(const char *, ...) __attribute__((format (printf, 1, 2)));
char *getnotifyfile(void);

/* client.c or server.c */
void cleanup(int);

#include <vis.h>
#define DECODE(a, b)	strunvis(a, b)
#define ENCODE(a, b)	strvis(a, b, VIS_WHITE)

#endif	/* __DEFS_H__ */
