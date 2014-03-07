/*	$OpenBSD: cmd.h,v 1.10 2014/03/07 21:56:13 krw Exp $	*/

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

/* Includes */
#include "disk.h"
#include "mbr.h"


/* Constants (returned by cmd funs) */
#define CMD_EXIT	0x0000
#define CMD_SAVE	0x0001
#define CMD_CONT	0x0002
#define CMD_CLEAN	0x0003
#define CMD_DIRTY	0x0004


/* Data types */
struct cmd_table;
struct cmd {
	struct cmd_table *table;
	char cmd[10];
	char args[100];
};

struct cmd_table {
	char *cmd;
	int (*fcn)(struct cmd *, struct disk *, struct mbr *, struct mbr *,
	    int);
	char *help;
};

/* Prototypes */
int Xreinit(struct cmd *, struct  disk *, struct mbr *, struct mbr *, int);
int Xdisk(struct cmd *, struct disk *, struct mbr *, struct mbr *, int);
int Xmanual(struct cmd *, struct disk *, struct mbr *, struct mbr *, int);
int Xedit(struct cmd *, struct disk *, struct mbr *, struct mbr *, int);
int Xsetpid(struct cmd *, struct disk *, struct mbr *, struct mbr *, int);
int Xselect(struct cmd *, struct disk *, struct mbr *, struct mbr *, int);
int Xswap(struct cmd *, struct disk *, struct mbr *, struct mbr *, int);
int Xprint(struct cmd *, struct disk *, struct mbr *, struct mbr *, int);
int Xwrite(struct cmd *, struct disk *, struct mbr *, struct mbr *, int);
int Xexit(struct cmd *, struct disk *, struct mbr *, struct mbr *, int);
int Xquit(struct cmd *, struct disk *, struct mbr *, struct mbr *, int);
int Xabort(struct cmd *, struct disk *, struct mbr *, struct mbr *, int);
int Xhelp(struct cmd *, struct disk *, struct mbr *, struct mbr *, int);
int Xflag(struct cmd *, struct disk *, struct mbr *, struct mbr *, int);
int Xupdate(struct cmd *, struct disk *, struct mbr *, struct mbr *, int);

#endif /* _CMD_H */


