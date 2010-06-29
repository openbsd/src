/*	$OpenBSD: tip.h,v 1.42 2010/06/29 23:10:56 nicm Exp $	*/
/*	$NetBSD: tip.h,v 1.7 1997/04/20 00:02:46 mellon Exp $	*/

/*
 * Copyright (c) 1989, 1993
 *	The Regents of the University of California.  All rights reserved.
 *
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
 *
 *      @(#)tip.h	8.1 (Berkeley) 6/6/93
 */

/*
 * tip - terminal interface program
 */

#include <sys/types.h>
#include <sys/file.h>
#include <sys/time.h>
#include <sys/wait.h>
#include <sys/ioctl.h>

#include <termios.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pwd.h>
#include <ctype.h>
#include <setjmp.h>
#include <unistd.h>
#include <errno.h>
#include <err.h>
#include <limits.h>

/*
 * Remote host attributes
 */
char	*DV;			/* UNIX device(s) to open */
char	*CM;			/* initial connection message */
char	*PN;			/* phone number(s) */
char	*DI;			/* disconnect string */

char	*ES;			/* escape character */
char	*FO;			/* force (literal next) char*/
char	*RC;			/* raise character */
char	*PR;			/* remote prompt */

/*
 * String value table
 */
typedef	struct {
	char	*v_name;	/* variable name */
	int	 v_flags;	/* type and flags */
	char	*v_abbrev;	/* possible abbreviation */
	char	*v_value;	/* casted to a union later */
}  value_t;

#define V_STRING	01	/* string valued */
#define V_BOOL		02	/* true-false value */
#define V_NUMBER	04	/* numeric value */
#define V_CHAR		010	/* character value */
#define V_TYPEMASK	017

#define V_CHANGED	020	/* to show modification */
#define V_READONLY	040	/* variable is not writable */

#define V_ENVIRON	0100	/* initialize out of the environment */
#define V_INIT		0400	/* static data space used for initialization */

/*
 * variable manipulation stuff --
 *   if we defined the value entry in value_t, then we couldn't
 *   initialize it in vars.c, so we cast it as needed to keep lint
 *   happy.
 */

#define value(v)	vtable[v].v_value
#define lvalue(v)	(long)vtable[v].v_value

#define	number(v)	((long)(v))
#define	boolean(v)      ((short)(long)(v))
#define	character(v)    ((char)(long)(v))
#define	address(v)      ((long *)(v))

#define	setnumber(v,n)		do { (v) = (char *)(long)(n); } while (0)
#define	setboolean(v,n)		do { (v) = (char *)(long)(n); } while (0)
#define	setcharacter(v,n)	do { (v) = (char *)(long)(n); } while (0)
#define	setaddress(v,n)		do { (v) = (char *)(n); } while (0)

/*
 * Escape command table definitions --
 *   lookup in this table is performed when ``escapec'' is recognized
 *   at the begining of a line (as defined by the eolmarks variable).
*/

typedef
	struct {
		char	e_char;			/* char to match on */
		char	*e_help;		/* help string */
		void	(*e_func)(int);		/* command */
	}
	esctable_t;

extern int	vflag;		/* verbose during reading of .tiprc file */
extern int	noesc;		/* no escape `~' char */
extern value_t	vtable[];	/* variable table */

/*
 * Definition of indices into variable table so
 *  value(DEFINE) turns into a static address.
 */
enum {
	BEAUTIFY = 0,
	BAUDRATE,
	EOFREAD,
	EOFWRITE,
	EOL,
	ESCAPE,
	EXCEPTIONS,
	FORCE,
	FRAMESIZE,
	HOST,
	LOG,
	PROMPT,
	RAISE,
	RAISECHAR,
	RECORD,
	REMOTE,
	SCRIPT,
	TABEXPAND,
	VERBOSE,
	SHELL,
	HOME,
	ECHOCHECK,
	DISCONNECT,
	TAND,
	LDELAY,
	CDELAY,
	ETIMEOUT,
	RAWFTP,
	HALFDUPLEX,
	LECHO,
	PARITY,
	HARDWAREFLOW,
	LINEDISC,
	DC
};

struct termios	term;		/* current mode of terminal */
struct termios	defterm;	/* initial mode of terminal */
struct termios	defchars;	/* current mode with initial chars */
int	gotdefterm;

FILE	*fscript;		/* FILE for scripting */

int	FD;			/* open file descriptor to remote host */
int	AC;			/* open file descriptor to dialer (v831 only) */
int	vflag;			/* print .tiprc initialization sequence */
int	noesc;			/* no `~' escape char */
int	sfd;			/* for ~< operation */
pid_t	tipin_pid;		/* pid of tipin */
int	tipin_fd;		/* tipin side of socketpair */
pid_t	tipout_pid;		/* pid of tipout */
int	tipout_fd;		/* tipout side of socketpair */
volatile sig_atomic_t stop;	/* stop transfer session flag */
volatile sig_atomic_t quit;	/* same; but on other end */
volatile sig_atomic_t stoprompt;/* for interrupting a prompt session */
volatile sig_atomic_t timedout;	/* ~> transfer timedout */
int	cumode;			/* simulating the "cu" program */
int	bits8;			/* terminal is 8-bit mode */
#define STRIP_PAR	(bits8 ? 0377 : 0177)

char	fname[PATH_MAX];	/* file name buffer for ~< */
char	copyname[PATH_MAX];	/* file name buffer for ~> */
char	ccc;			/* synchronization character */
char	*uucplock;		/* name of lock file for uucp's */

int	odisc;			/* initial tty line discipline */
extern	int disc;		/* current tty discpline */

extern	char *__progname;	/* program name */

/* cmds.c */
char	*expand(char *);
void	 chdirectory(int);
void	 consh(int);
void	 cu_put(int);
void	 cu_take(int);
void	 finish(int);
void	 genbrk(int);
void	 getfl(int);
void	 listvariables(int);
void	 parwrite(int, char *, size_t);
void	 pipefile(int);
void	 pipeout(int);
void	 sendfile(int);
void	 setscript(void);
void	 shell(int);
void	 suspend(int);
void	 timeout(int);
void	 tipabort(char *);
void	 variable(int);

/* cu.c */
void	 cumain(int, char **);

/* hunt.c */
long	 hunt(char *);

/* log.c */
void	 logent(char *, char *, char *);
void	 loginit(void);

/* remote.c */
char	*getremote(char *);

/* tip.c */
void	 con(void);
char	*ctrl(char);
char	*interp(char *);
int	 any(int, char *);
int	 prompt(char *, char *, size_t);
size_t	 size(char *);
void	 cleanup(int);
void	 help(int);
void	 parwrite(int, char *, size_t);
void	 raw(void);
void	 setparity(char *);
int	 ttysetup(int);
void	 unraw(void);

/* tipout.c */
void	tipout(void);

/* value.c */
void	vinit(void);
void	vlex(char *);
int	vstring(char *, char *);
