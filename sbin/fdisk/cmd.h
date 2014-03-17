/*	$OpenBSD: cmd.h,v 1.12 2014/03/17 16:40:00 krw Exp $	*/

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

#define CMD_EXIT	0x0000
#define CMD_SAVE	0x0001
#define CMD_CONT	0x0002
#define CMD_CLEAN	0x0003
#define CMD_DIRTY	0x0004

struct cmd {
	char *cmd;
	int (*fcn)(char *, struct disk *, struct mbr *, struct mbr *,
	    int);
	char *help;
};
extern struct cmd cmd_table[];

int Xreinit(char *, struct  disk *, struct mbr *, struct mbr *, int);
int Xdisk(char *, struct disk *, struct mbr *, struct mbr *, int);
int Xmanual(char *, struct disk *, struct mbr *, struct mbr *, int);
int Xedit(char *, struct disk *, struct mbr *, struct mbr *, int);
int Xsetpid(char *, struct disk *, struct mbr *, struct mbr *, int);
int Xselect(char *, struct disk *, struct mbr *, struct mbr *, int);
int Xswap(char *, struct disk *, struct mbr *, struct mbr *, int);
int Xprint(char *, struct disk *, struct mbr *, struct mbr *, int);
int Xwrite(char *, struct disk *, struct mbr *, struct mbr *, int);
int Xexit(char *, struct disk *, struct mbr *, struct mbr *, int);
int Xquit(char *, struct disk *, struct mbr *, struct mbr *, int);
int Xabort(char *, struct disk *, struct mbr *, struct mbr *, int);
int Xhelp(char *, struct disk *, struct mbr *, struct mbr *, int);
int Xflag(char *, struct disk *, struct mbr *, struct mbr *, int);
int Xupdate(char *, struct disk *, struct mbr *, struct mbr *, int);

#endif /* _CMD_H */


