/*
 * Copyright (c) 1995 - 2000 Kungliga Tekniska Högskolan
 * (Royal Institute of Technology, Stockholm, Sweden).
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
 * 3. Neither the name of the Institute nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 * 
 * THIS SOFTWARE IS PROVIDED BY THE INSTITUTE AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE INSTITUTE OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

/*
 * $KTH: cmd.h,v 1.2 2000/06/01 18:37:26 lha Exp $
 */

#ifndef _ARLA_CMD_H
#define _ARLA_CMD_H 1

/* syncdesc */
typedef enum { CMD_ALIAS = 1,
	       CMD_HIDDEN = 4 } cmd_syncdesc_flags;

/* parmdesc */
typedef enum { CMD_FLAG = 1,
	       CMD_SINGLE,
	       CMD_LIST } cmd_parmdesc_type;

typedef enum { CMD_REQUIRED 	= 0x0,
	       CMD_OPTIONAL 	= 0x1,
	       CMD_EXPANDS	= 0x2,
	       CMD_HIDE		= 0x4,
	       CMD_PROCESSED	= 0x8 } cmd_parmdesc_flags;

enum { CMD_HELPPARM = 63, CMD_MAXPARMS = 64 } ;

struct cmd_syndesc;

typedef int (*cmd_proc) (struct cmd_syndesc *, void *);

struct cmd_item {
    struct cmd_item *next;
    void *data;
};

struct cmd_parmdesc {
    char *name;
    cmd_parmdesc_type type;
    struct cmd_item *items;
    cmd_parmdesc_flags flags;
    char *help;
};

struct cmd_syndesc {
    struct cmd_syndesc *next;
    struct cmd_syndesc *nextAlias;
    struct cmd_syndesc *aliasOf;
    char *name;
    char *a0name;
    char *help;
    cmd_proc proc;
    void *rock;
    int nParams;
    cmd_syncdesc_flags flags;
    struct cmd_parmdesc parms[CMD_MAXPARMS];
};

struct cmd_syndesc *
cmd_CreateSyntax (const char *name, cmd_proc main, void *rock, 
		  const char *help_str);

int
cmd_SetBeforeProc (int (*proc) (void *rock), void *rock);

int
cmd_SetAfterProc (int (*proc) (void *rock), void *rock);

void
cmd_AddParm (struct cmd_syndesc *ts, const char *cmd, 
	     cmd_parmdesc_type type, cmd_parmdesc_flags flags,
	     const char *help_str);

int
cmd_CreateAlias (struct cmd_syndesc *ts, const char *name);

int
cmd_Seek (struct cmd_syndesc *ts, int pos);

void
cmd_FreeArgv (char **argv);

int
cmd_ParseLine (const char *line, char **argv, int *n, int maxn);

int
cmd_Dispatch (int argc, char **argv);

void
cmd_PrintSyntax (const char *commandname);

const char *
cmd_number2str(int error);

#define CMD_EXCESSPARMS			3359744
#define CMD_INTERALERROR		3359745
#define CMD_NOTLIST			3359746
#define CMD_TOOMANY			3359747
#define CMD_USAGE			3359748
#define CMD_UNKNOWNCMD			3359749
#define CMD_UNKNOWNSWITCH		3359750
#define CMD_AMBIG			3359751
#define CMD_TOOFEW			3359752
#define CMD_TOOBIG			3359753

#endif /* _ARLA_CMD_H */
