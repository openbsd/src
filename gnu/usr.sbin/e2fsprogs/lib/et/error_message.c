/*
 * $Header: /home/cvs/src/gnu/usr.sbin/e2fsprogs/lib/et/Attic/error_message.c,v 1.1.1.1 1997/06/14 07:18:44 downsj Exp $
 * $Source: /home/cvs/src/gnu/usr.sbin/e2fsprogs/lib/et/Attic/error_message.c,v $
 * $Locker:  $
 *
 * Copyright 1987 by the Student Information Processing Board
 * of the Massachusetts Institute of Technology
 *
 * For copyright info, see "mit-sipb-copyright.h".
 */

#include <stdio.h>
#include <string.h>
#include <errno.h>
#include "com_err.h"
#include "error_table.h"
#include "mit-sipb-copyright.h"
#include "internal.h"

static char buffer[25];

struct et_list * _et_list = (struct et_list *) NULL;


#ifdef __STDC__
const char * error_message (errcode_t code)
#else
const char * error_message (code)
	errcode_t	code;
#endif
{
    int offset;
    struct et_list *et;
    int table_num;
    int started = 0;
    char *cp;

    offset = code & ((1<<ERRCODE_RANGE)-1);
    table_num = code - offset;
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
    strcpy (buffer, "Unknown code ");
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
