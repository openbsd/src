/*
 * Emulate the vax instructions for queue insertion and deletion, somewhat.
 * A std_queue structure is defined here and used by these routines.  These
 * routines use caddr_ts so they can operate on any structure.  The std_queue
 * structure is used rather than proc structures so that when the proc struct
 * changes only process management code breaks.  The ideal solution would be
 * to define a std_queue as a global type which is part of all the structures
 * which are manipulated by these routines.  This would involve considerable
 * effort...
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
RCSID("$arla: q.c,v 1.8 1999/12/31 05:39:49 assar Exp $") ;
#endif

#include <stdio.h>
#include <sys/types.h>
#include <sys/time.h>
#ifdef HAVE_SYS_CDEFS_H
#include <sys/cdefs.h>
#endif
#include <utime.h>

#include "q.h"

/* Ansified, made more readable, and mayby fixed /lha */

struct std_queue {
	struct std_queue *q_link;
	struct std_queue *q_rlink;
} ;

void lwp_insque(void *elementp, void *quep)
{
    struct std_queue *que = (struct std_queue *) quep ;
    struct std_queue *element = (struct std_queue *) elementp ;

    element->q_link = que->q_link;
    element->q_rlink = que;
    
    que->q_link->q_rlink = element;
    que->q_link = element;
}

void lwp_remque(void *elementp)
{
    struct std_queue *element = (struct std_queue *) elementp ;

    element->q_link->q_rlink = element->q_rlink;
    element->q_rlink->q_link = element->q_link;
    
    element->q_rlink = element->q_link = (struct std_queue *) 0;
}

