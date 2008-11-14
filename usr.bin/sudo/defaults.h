/*
 * Copyright (c) 1999-2005, 2008
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
 *
 * Sponsored in part by the Defense Advanced Research Projects
 * Agency (DARPA) and Air Force Research Laboratory, Air Force
 * Materiel Command, USAF, under agreement number F39502-99-1-0512.
 *
 * $Sudo: defaults.h,v 1.33 2008/11/09 14:13:12 millert Exp $
 */

#ifndef _SUDO_DEFAULTS_H
#define _SUDO_DEFAULTS_H

#include <def_data.h>

struct list_member {
    char *value;
    struct list_member *next;
};

struct def_values {
    char *sval;		/* string value */
    int ival;		/* actually an enum */
};

enum list_ops {
    add,
    delete,
    freeall
};

/*
 * Structure describing compile-time and run-time options.
 */
struct sudo_defs_types {
    char *name;
    int type;
    char *desc;
    struct def_values *values;
    int (*callback) __P((char *));
    union {
	int flag;
	int ival;
	enum def_tupple tuple;
	char *str;
	mode_t mode;
	struct list_member *list;
    } sd_un;
};

/*
 * Four types of defaults: strings, integers, and flags.
 * Also, T_INT or T_STR may be ANDed with T_BOOL to indicate that
 * a value is not required.  Flags are boolean by nature...
 */
#undef T_INT
#define T_INT		0x001
#undef T_UINT
#define T_UINT		0x002
#undef T_STR
#define T_STR		0x003
#undef T_FLAG
#define T_FLAG		0x004
#undef T_MODE
#define T_MODE		0x005
#undef T_LIST
#define T_LIST		0x006
#undef T_LOGFAC
#define T_LOGFAC	0x007
#undef T_LOGPRI
#define T_LOGPRI	0x008
#undef T_TUPLE
#define T_TUPLE		0x009
#undef T_MASK
#define T_MASK		0x0FF
#undef T_BOOL
#define T_BOOL		0x100
#undef T_PATH
#define T_PATH		0x200

/*
 * Argument to update_defaults()
 */
#define SETDEF_GENERIC	0x01
#define	SETDEF_HOST	0x02
#define	SETDEF_USER	0x04
#define	SETDEF_RUNAS	0x08
#define	SETDEF_CMND	0x10
#define SETDEF_ALL	(SETDEF_GENERIC|SETDEF_HOST|SETDEF_USER|SETDEF_RUNAS|SETDEF_CMND)

/*
 * Prototypes
 */
void dump_default	__P((void));
int set_default		__P((char *, char *, int));
void init_defaults	__P((void));
int update_defaults	__P((int));
void list_options	__P((void));

extern struct sudo_defs_types sudo_defs_table[];

#endif /* _SUDO_DEFAULTS_H */
