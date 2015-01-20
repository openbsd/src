/*	$OpenBSD: client.h,v 1.1 2015/01/20 09:00:16 guenther Exp $	*/

#ifndef __CLIENT_H__
#define __CLIENT_H__
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

#include <sys/stat.h>
#include <regex.h>
#include <stdio.h>

#include "defs.h"

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

#define ALLOC(x) 	(struct x *) xmalloc(sizeof(struct x))
#define A(s)		((s) ? s : "<null>")


#define COMMENT_CHAR	'#'		/* Config file comment char */


/*
 * Name list
 */
struct namelist {		/* for making lists of strings */
	char	*n_name;
	regex_t	*n_regex;
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
	char	*pathname;
	char	*src;
	char	*target;
	struct	linkbuf *nextp;
};

extern char	       *path_remsh;	/* Remote shell command */
extern char 		host[];		/* Host name of master copy */
extern char 	      **realargv;	/* Real argv */
extern char	       *homedir;	/* User's $HOME */
extern int 		do_fork;	/* Should we do fork()'ing */
extern int 		nflag;		/* NOP flag, don't execute commands */
extern int 		realargc;	/* Real argc */
extern int		setjmp_ok;	/* setjmp/longjmp flag */
extern int		maxchildren;	/* Max active children */
extern int64_t		min_freespace;	/* Min filesys free space */
extern int64_t		min_freefiles;	/* Min filesys free # files */
extern struct linkbuf  *ihead;	/* list of files with more than one link */
extern struct subcmd   *subcmds;/* list of sub-commands for current cmd */
extern struct namelist *filelist;	/* list of source files */
extern struct cmd      *cmds;		/* Initialized by yyparse() */

extern char 		target[BUFSIZ];	/* target/source directory name */
extern char 	       *ptarget;	/* pointer to end of target name */
extern int		activechildren;	/* Number of active children */
extern int		amchild;	/* This PID is a child */
extern char	       *path_rdistd;
extern char	       *remotemsglist;

/*
 * Our own declarations.
 */

/* child.c */
void waitup(void);
int spawn(struct cmd *, struct cmd *);

/* client.c */
char *remfilename(char *, char *, char *, char *, int);
int inlist(struct namelist *, char *);
void runcmdspecial(struct cmd *, opt_t);
int checkfilename(char *);
void freelinkinfo(struct linkbuf *);
int install(char *, char *, int, int , opt_t);

/* distopt.c */
int parsedistopts(char *, opt_t *, int);
char *getdistoptlist(void);
char *getondistoptlist(opt_t);

/* docmd.c */
void markassigned(struct cmd *, struct cmd *);
int okname(char *);
int except(char *);
void docmds(struct namelist *, int, char **);

/* expand.c */
struct namelist *expand(struct namelist *, int);
u_char *xstrchr(u_char *, int);
void expstr(u_char *);
void expsh(u_char *);
void matchdir(char *);
int execbrc(u_char *, u_char *);
int match(char *, char *);
int amatch(char *, u_char *);

/* gram.c */
int yylex(void);
int any(int, char *);
void insert(char *, struct namelist *, struct namelist *, struct subcmd *);
void append(char *, struct namelist *, char *, struct subcmd *);
void yyerror(char *);
struct namelist *makenl(char *);
struct subcmd *makesubcmd(int);
int yyparse(void);

/* isexec.c */
int isexec(char *, struct stat *);

/* lookup.c */
void define(char *);
struct namelist *lookup(char *, int, struct namelist *);

/* rdist.c */
FILE *opendist(char *);
void docmdargs(int, char *[]);
char *getnlstr(struct namelist *);

#endif	/* __CLIENT_H__ */
