/*
 * Copyright (c) 1996, 1998-2000 Todd C. Miller <Todd.Miller@courtesan.com>
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
 * $Sudo: parse.h,v 1.9 2000/03/23 04:38:20 millert Exp $
 */

#ifndef _SUDO_PARSE_H
#define _SUDO_PARSE_H

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
 * Structure describing an alias match in parser.
 */
typedef struct {
    int type;
    char *name;
    int val;
} aliasinfo;

/*
 * Structure containing Cmnd_Alias's if "sudo -l" is used.
 */
struct generic_alias {
    int type;
    char *alias;
    char *entries;
    size_t entries_size;
    size_t entries_len;
};

/* The matching stack and number of entries on it. */
extern struct matchstack *match;
extern int top;

/*
 * Prototypes
 */
int addr_matches	__P((char *));
int command_matches	__P((char *, char *, char *, char *));
int hostname_matches	__P((char *, char *, char *));
int netgr_matches	__P((char *, char *, char *, char *));
int usergr_matches	__P((char *, char *));

#endif /* _SUDO_PARSE_H */
