/*	$Id: ss.h,v 1.1.1.1 1995/12/14 06:52:35 tholo Exp $	*/

/*-
 * Copyright 1987, 1988 by the Student Information Processing Board
 *	of the Massachusetts Institute of Technology
 *
 * Permission to use, copy, modify, and distribute this software
 * and its documentation for any purpose and without fee is
 * hereby granted, provided that the above copyright notice
 * appear in all copies and that both that copyright notice and
 * this permission notice appear in supporting documentation,
 * and that the names of M.I.T. and the M.I.T. S.I.P.B. not be
 * used in advertising or publicity pertaining to distribution
 * of the software without specific, written prior permission.
 * M.I.T. and the M.I.T. S.I.P.B. make no representations about
 * the suitability of this software for any purpose.  It is
 * provided "as is" without express or implied warranty.
 */

#ifndef _SS_H
#define _SS_H

#include <sys/cdefs.h>

#ifndef NO_SS_ERR_H
#include <ss/ss_err.h>
#endif

typedef const struct _ss_request_entry {
    const char * const *command_names; /* whatever */
    void (* const function) __P((int, const char * const *, int, void *));
    const char * const info_string;	/* NULL */
    int flags;			/* 0 */
} ss_request_entry;

typedef const struct _ss_request_table {
    int version;
    ss_request_entry *requests;
} ss_request_table;

#define SS_RQT_TBL_V2	2

typedef struct _ss_rp_options {	/* DEFAULT VALUES */
    int version;		/* SS_RP_V1 */
    void (*unknown) __P((int, const char * const *, int, void *));	/* call for unknown command */
    int allow_suspend;
    int catch_int;
} ss_rp_options;

#define SS_RP_V1 1

#define SS_OPT_DONT_LIST	0x0001
#define SS_OPT_DONT_SUMMARIZE	0x0002

void ss_help __P((int, const char * const *, int, void *));
char *ss_current_request();
char *ss_name();
void ss_error __P((int, long, char const *, ...));
void ss_perror __P((int, long, char const *));
int ss_create_invocation __P((char *, char *, char *, ss_request_table *, int *));
int ss_listen(int);
void ss_abort_subsystem();

extern ss_request_table ss_std_requests;
#endif /* _SS_H */
