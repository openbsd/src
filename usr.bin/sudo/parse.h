/*
 * Copyright (c) 1996,1998-2000,2004 Todd C. Miller <Todd.Miller@courtesan.com>
 *
 * Permission to use, copy, modify, and distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 * ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
 * OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 *
 * $Sudo: parse.h,v 1.14 2004/08/02 18:44:58 millert Exp $
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
	int noexec;
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
#define no_execve	(match[top-1].noexec)

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
    int noexecve;
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
int command_matches	__P((char *, char *));
int hostname_matches	__P((char *, char *, char *));
int netgr_matches	__P((char *, char *, char *, char *));
int userpw_matches	__P((char *, char *, struct passwd *));
int usergr_matches	__P((char *, char *, struct passwd *));

#endif /* _SUDO_PARSE_H */
