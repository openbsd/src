/*	$OpenBSD: cmd.h,v 1.2 2000/01/08 23:23:37 d Exp $	*/

/*
 * Copyright (c) 1997 Tobias Weingartner
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
 *    This product includes software developed by Tobias Weingartner.
 * 4. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef _CMD_H
#define _CMD_H

/* Constants (returned by cmd funs) */
#define CMD_EXIT	0x0000
#define CMD_SAVE	0x0001
#define CMD_CONT	0x0002

/* Data types */
struct _cmd_table_t;
typedef struct _cmd_t {
	struct _cmd_table_t *table;
	char cmd[10];
	char args[100];
} cmd_t;

typedef struct _cmd_table_t {
	char *cmd;
	int (*fcn)(cmd_t *);
	char *opt;
	char *help;
} cmd_table_t;


#ifndef CMD_NOEXTERN
extern cmd_table_t cmd_table[];
#endif

/* Prototypes */
int Xhelp __P((cmd_t *));
int Xadd __P((cmd_t *));
int Xbase __P((cmd_t *));
int Xchange __P((cmd_t *));
int Xdisable __P((cmd_t *));
int Xenable __P((cmd_t *));
int Xfind __P((cmd_t *));
int Xlines __P((cmd_t *));
int Xlist __P((cmd_t *));
int Xshow __P((cmd_t *));
int Xexit __P((cmd_t *));
int Xquit __P((cmd_t *));
int Xtimezone __P((cmd_t *));

#endif _CMD_H


