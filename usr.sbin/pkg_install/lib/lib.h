/* $OpenBSD: lib.h,v 1.4 1998/11/19 04:12:55 espie Exp $ */

/*
 * FreeBSD install - a package for the installation and maintainance
 * of non-core utilities.
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
 * Jordan K. Hubbard
 * 18 July 1993
 *
 * Include and define various things wanted by the library routines.
 *
 */

#ifndef _INST_LIB_LIB_H_
#define _INST_LIB_LIB_H_

/* Includes */
#include <sys/param.h>
#include <sys/stat.h>
#include <sys/file.h>

#include <ctype.h>
#include <dirent.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <string.h>
#include <unistd.h>

/* Macros */
#define SUCCESS	(0)
#define	FAIL	(-1)

#ifndef TRUE
#define TRUE	(1)
#endif

#ifndef FALSE
#define FALSE	(0)
#endif

#define YES		2
#define NO		1

/* Usually "rm", but often "echo" during debugging! */
#define REMOVE_CMD	"rm"

/* Usually "rm", but often "echo" during debugging! */
#define RMDIR_CMD	"rmdir"

/* Where we put logging information by default, else ${PKG_DBDIR} if set */
#define DEF_LOG_DIR		"/var/db/pkg"
/* just in case we change the environment variable name */
#define PKG_DBDIR		"PKG_DBDIR"

/* The names of our "special" files */
#define CONTENTS_FNAME		"+CONTENTS"
#define COMMENT_FNAME		"+COMMENT"
#define DESC_FNAME		"+DESC"
#define INSTALL_FNAME		"+INSTALL"
#define DEINSTALL_FNAME		"+DEINSTALL"
#define REQUIRE_FNAME		"+REQUIRE"
#define REQUIRED_BY_FNAME	"+REQUIRED_BY"
#define DISPLAY_FNAME		"+DISPLAY"
#define MTREE_FNAME		"+MTREE_DIRS"

#define CMD_CHAR		'@'	/* prefix for extended PLIST cmd */

/* The name of the "prefix" environment variable given to scripts */
#define PKG_PREFIX_VNAME	"PKG_PREFIX"

/* maximum size of comment that will fit on one line */
#ifndef MAXINDEXSIZE
#define MAXINDEXSIZE 60
#endif

/* enumerated constants for plist entry types */
typedef enum pl_ent_t {
	PLIST_SHOW_ALL = -1,
	PLIST_FILE,
	PLIST_CWD,
	PLIST_CMD,
	PLIST_CHMOD,
	PLIST_CHOWN,
	PLIST_CHGRP,
	PLIST_COMMENT,
	PLIST_IGNORE,
	PLIST_NAME,
	PLIST_UNEXEC,
	PLIST_SRC,
	PLIST_DISPLAY,
	PLIST_PKGDEP,
	PLIST_MTREE,
	PLIST_DIR_RM,
	PLIST_IGNORE_INST,
	PLIST_OPTION,
	PLIST_PKGCFL
} pl_ent_t;

/* Types */
typedef unsigned int Boolean;

/* this structure describes a packing list entry */
typedef struct plist_t {
	struct plist_t	*prev;		/* previous entry */
	struct plist_t	*next;		/* next entry */
	char		*name;		/* name of entry */
	Boolean		marked;		/* whether entry has been marked */
	pl_ent_t	type;		/* type of entry */
} plist_t;

/* this structure describes a package's complete packing list */
typedef struct package_t {
	plist_t		*head;		/* head of list */
	plist_t		*tail;		/* tail of list */
} package_t;

enum {
	ChecksumLen = 16,
	LegibleChecksumLen = 33
};

/* type of function to be handed to findmatchingname; return value of this
 * is currently ignored */
typedef int (*matchfn)(const char *found, char *data);

/* Prototypes */
/* Misc */
int		vsystem(const char *, ...);
void		cleanup(int);
char		*make_playpen(char *, size_t, size_t);
char		*where_playpen(void);
void		leave_playpen(char *);
off_t		min_free(char *);
void            save_dirs(char **c, char **p);
void            restore_dirs(char *c, char *p);

/* String */
char 		*get_dash_string(char **);
char		*copy_string(char *);
Boolean		suffix(char *, char *);
void		nuke_suffix(char *);
void		str_lowercase(char *);
char		*basename_of(char *);
char		*dirname_of(const char *);
char		*strconcat(char *, char *);
int		pmatch(const char *, const char *);
int		findmatchingname(const char *, const char *, matchfn, char *); /* doesn't really belong here */
char		*findbestmatchingname(const char *, const char *); /* neither */
int		ispkgpattern(const char *);
char		*strnncpy(char *to, size_t tosize, char *from, size_t cc);

/* File */
Boolean		fexists(char *);
Boolean		isdir(char *);
Boolean		islinktodir(char *);
Boolean		isemptydir(char *fname);
Boolean		isemptyfile(char *fname);
Boolean         isfile(char *);
Boolean		isempty(char *);
Boolean		isURL(char *);
char		*fileGetURL(char *, char *);
char		*fileURLFilename(char *, char *, int);
char		*fileURLHost(char *, char *, int);
char		*fileFindByPath(char *, char *);
char		*fileGetContents(char *);
Boolean		make_preserve_name(char *, size_t, char *, char *);
void		write_file(char *, char *);
void		copy_file(char *, char *, char *);
void		move_file(char *, char *, char *);
void		copy_hierarchy(char *, char *, Boolean);
int		delete_hierarchy(char *, Boolean, Boolean);
int		unpack(char *, char *);
void		format_cmd(char *, size_t , char *, char *, char *);

/* Packing list */
plist_t		*new_plist_entry(void);
plist_t		*last_plist(package_t *);
plist_t		*find_plist(package_t *, pl_ent_t);
char		*find_plist_option(package_t *, char *name);
void		plist_delete(package_t *, Boolean, pl_ent_t, char *);
void		free_plist(package_t *);
void		mark_plist(package_t *);
void		csum_plist_entry(char *, plist_t *);
void		add_plist(package_t *, pl_ent_t, char *);
void		add_plist_top(package_t *, pl_ent_t, char *);
void		delete_plist(package_t *pkg, Boolean all, pl_ent_t type, char *name);
void		write_plist(package_t *, FILE *);
void		read_plist(package_t *, FILE *);
int		plist_cmd(char *, char **);
int		delete_package(Boolean, Boolean, package_t *);

/* For all */
int		pkg_perform(char **);

/* Externs */
extern Boolean	Verbose;
extern Boolean	Fake;
extern Boolean  Force;

#endif /* _INST_LIB_LIB_H_ */
