/*
 * Copyright (c) 1996, 1998-2000, 2004, 2007-2009
 *	Todd C. Miller <Todd.Miller@courtesan.com>
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
 */

#ifndef _SUDO_PARSE_H
#define _SUDO_PARSE_H

#undef UNSPEC
#define UNSPEC	-1
#undef DENY
#define DENY	 0
#undef ALLOW
#define ALLOW	 1
#undef IMPLIED
#define IMPLIED	 2

/*
 * A command with args. XXX - merge into struct member.
 */
struct sudo_command {
    char *cmnd;
    char *args;
};

/*
 * Tags associated with a command.
 * Possible valus: TRUE, FALSE, UNSPEC.
 */
struct cmndtag {
    __signed char nopasswd;
    __signed char noexec;
    __signed char setenv;
    __signed char extra;
};

/*
 * SELinux-specific container struct.
 * Currently just contains a role and type.
 */
struct selinux_info {
    char *role;
    char *type;
};

/*
 * The parses sudoers file is stored as a collection of linked lists,
 * modelled after the yacc grammar.
 *
 * Other than the alias struct, which is stored in a red-black tree,
 * the data structure used is basically a doubly-linked tail queue without
 * a separate head struct--the first entry acts as the head where the prev
 * pointer does double duty as the tail pointer.  This makes it possible
 * to trivally append sub-lists.  In addition, the prev pointer is always
 * valid (even if it points to itself).  Unlike a circle queue, the next
 * pointer of the last entry is NULL and does not point back to the head.
 *
 * Note that each list struct must contain a "prev" and "next" pointer as
 * the first two members of the struct (in that order).
 */

/*
 * Tail queue list head structure.
 */
TQ_DECLARE(defaults)
TQ_DECLARE(userspec)
TQ_DECLARE(member)
TQ_DECLARE(privilege)
TQ_DECLARE(cmndspec)

/*
 * Structure describing a user specification and list thereof.
 */
struct userspec {
    struct userspec *prev, *next;
    struct member_list users;		/* list of users */
    struct privilege_list privileges;	/* list of privileges */
};

/*
 * Structure describing a privilege specification.
 */
struct privilege {
    struct privilege *prev, *next;
    struct member_list hostlist;	/* list of hosts */
    struct cmndspec_list cmndlist;	/* list of Cmnd_Specs */
};

/*
 * Structure describing a linked list of Cmnd_Specs.
 */
struct cmndspec {
    struct cmndspec *prev, *next;
    struct member_list runasuserlist;	/* list of runas users */
    struct member_list runasgrouplist;	/* list of runas groups */
    struct member *cmnd;		/* command to allow/deny */
    struct cmndtag tags;		/* tag specificaion */
#ifdef HAVE_SELINUX
    char *role, *type;			/* SELinux role and type */
#endif
};

/*
 * Generic structure to hold users, hosts, commands.
 */
struct member {
    struct member *prev, *next;
    char *name;				/* member name */
    short type;				/* type (see gram.h) */
    short negated;			/* negated via '!'? */
};

struct runascontainer {
    struct member *runasusers;
    struct member *runasgroups;
};

/*
 * Generic structure to hold {User,Host,Runas,Cmnd}_Alias
 * Aliases are stored in a red-black tree, sorted by name and type.
 */
struct alias {
    char *name;				/* alias name */
    unsigned short type;		/* {USER,HOST,RUNAS,CMND}ALIAS */
    unsigned short seqno;		/* sequence number */
    struct member_list members;		/* list of alias members */
};

/*
 * Structure describing a Defaults entry and a list thereof.
 */
struct defaults {
    struct defaults *prev, *next;
    char *var;				/* variable name */
    char *val;				/* variable value */
    struct member_list binding;		/* user/host/runas binding */
    int type;				/* DEFAULTS{,_USER,_RUNAS,_HOST} */
    int op;				/* TRUE, FALSE, '+', '-' */
};

/*
 * Parsed sudoers info.
 */
extern struct userspec_list userspecs;
extern struct defaults_list defaults;

/*
 * Alias sequence number to avoid loops.
 */
extern unsigned int alias_seqno;

/*
 * Prototypes
 */
char *alias_add		__P((char *, int, struct member *));
int addr_matches	__P((char *));
int cmnd_matches	__P((struct member *));
int cmndlist_matches	__P((struct member_list *));
int command_matches	__P((char *, char *));
int hostlist_matches	__P((struct member_list *));
int hostname_matches	__P((char *, char *, char *));
int netgr_matches	__P((char *, char *, char *, char *));
int no_aliases		__P((void));
int runaslist_matches	__P((struct member_list *, struct member_list *));
int userlist_matches	__P((struct passwd *, struct member_list *));
int usergr_matches	__P((char *, char *, struct passwd *));
int userpw_matches	__P((char *, char *, struct passwd *));
int group_matches	__P((char *, struct group *));
struct alias *alias_find __P((char *, int));
struct alias *alias_remove __P((char *, int));
void alias_free		__P((void *));
void alias_apply	__P((int (*)(void *, void *), void *));
void init_aliases	__P((void));
void init_lexer		__P((void));
void init_parser	__P((char *, int));
int alias_compare	__P((const void *, const void *));

#endif /* _SUDO_PARSE_H */
