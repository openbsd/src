/*	$OpenBSD: error_message.c,v 1.4 2003/04/03 12:07:24 hin Exp $	*/

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

#include <stdio.h>
#include <string.h>
#include <errno.h>
#include "error_table.h"

#ifndef lint
static const char rcsid[] =
    "$OpenBSD: error_message.c,v 1.4 2003/04/03 12:07:24 hin Exp $";
static const char copyright[] =
    "Copyright 1986, 1987, 1988 by the Student Information Processing Board\nand the department of Information Systems\nof the Massachusetts Institute of Technology";
#endif

static char buffer[25];

struct et_list * _et_list = (struct et_list *) NULL;

const char *
error_message (code)
	long	code;
{
    int offset;
    struct et_list *et;
    int table_num;
    int started = 0;
    char *cp;

    offset = (int)(code & ((1<<ERRCODE_RANGE)-1));
    table_num = (int)code - offset;
    if (!table_num) {
#ifdef HAS_SYS_ERRLIST
	if (offset < sys_nerr)
	    return(sys_errlist[offset]);
	else
	    goto oops;
#else
	cp = strerror(offset);
	if (cp)
	    return(cp);
	else
	    goto oops;
#endif
    }
    for (et = _et_list; et; et = et->next) {
	if (et->table->base == table_num) {
	    /* This is the right table */
	    if (et->table->n_msgs <= offset)
		goto oops;
	    return(et->table->msgs[offset]);
	}
    }
oops:
    strlcpy (buffer, "Unknown code ", sizeof(buffer));
    if (table_num) {
	strcat (buffer, error_table_name (table_num));
	strcat (buffer, " ");
    }
    for (cp = buffer; *cp; cp++)
	;
    if (offset >= 100) {
	*cp++ = '0' + offset / 100;
	offset %= 100;
	started++;
    }
    if (started || offset >= 10) {
	*cp++ = '0' + offset / 10;
	offset %= 10;
    }
    *cp++ = '0' + offset;
    *cp = '\0';
    return(buffer);
}
