/*
 * Copyright (c) 1998 - 2000 Kungliga Tekniska Högskolan
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
 * Manage the priority of the files
 */

#include "arla_local.h"
#include <kafs.h>
RCSID("$arla: fprio.c,v 1.12 2002/10/02 00:54:21 lha Exp $");

/* Hashtable of entries by name */
static Hashtab *fpriohashtab;

/*
 * fprio - hash help functions
 */

static int
fpriocmp (void *a, void *b)
{
    struct fpriorityentry *n1 = (struct fpriorityentry *)a;
    struct fpriorityentry *n2 = (struct fpriorityentry *)b;

    return n1->fid.Cell != n2->fid.Cell || 
	n1->fid.fid.Volume != n2->fid.fid.Volume ||
	n1->fid.fid.Vnode != n2->fid.fid.Vnode ||
	n1->fid.fid.Unique != n2->fid.fid.Unique;
}

static unsigned
fpriohash (void *a)
{
     struct fpriorityentry *n = (struct fpriorityentry *)a;

     return n->fid.Cell ^ n->fid.fid.Volume ^ 
	 n->fid.fid.Vnode ^ n->fid.fid.Unique;
}

/*
 * fprio_init
 * 
 *  Just create the hashtab. Leave the smartness to the user.
 */

void
fprio_init (char *file)
{
    fpriohashtab = hashtabnew (FPRIOCACHE_SIZE, fpriocmp, fpriohash);
    if (fpriohashtab == NULL)
	arla_errx (1, ADEBERROR, "fprio_init: hashtabnew failed");

    if (file)
	fprio_readin(file);
}

/*
 * Cleanout unwanted enteries
 */

static Bool
cleanupfpriohash(void *ptr, void *arg)
{
    struct fpriorityentry *n = (struct fpriorityentry *)ptr;
    struct fpriorityentry *a = (struct fpriorityentry *)arg;

    /* Clean out if
     *
     *   NULL cleanout argument
     *   cleanout argument is in the same Cell and
     *      Volume == Vnode == 0  (ie, when whole cell), or
     *      Volume == the victim entry's Volume
     *             && Vnode == 0  (ie, whole volume), or
     *      the Vnode and Unique also match (ie the file/direntry)
     *
     *  This means that memset(&myarg, 0, sizeof(stuct fprioentry))
     *  is probably not what you want. (Cleaning out the localcell's
     *  all entries)
     */

    if (a == NULL ||
	(a->fid.Cell == n->fid.Cell && 
	 ((a->fid.fid.Volume == 0 && a->fid.fid.Vnode ==0) ||
	  (a->fid.fid.Volume == n->fid.fid.Volume &&
	   (a->fid.fid.Vnode == 0 ||
	    (a->fid.fid.Vnode == n->fid.fid.Vnode &&
	     a->fid.fid.Unique == n->fid.fid.Unique)))))) {
	
	AFSCallBack broken_callback = {0, 0, CBDROPPED};
	
	fcache_stale_entry (n->fid, broken_callback);
	free(n);
	
	return TRUE;
    }
    
    return FALSE;
}


int
fprio_clear(void)
{
    hashtabcleantab(fpriohashtab, cleanupfpriohash, NULL);
    return 0;
}

/*
 * zapp the `fid'
 */

void
fprio_remove(VenusFid fid)
{
    struct fpriorityentry key; 

    key.fid = fid;
    hashtabfree(fpriohashtab, &key);
    return;
}

/*
 * set a `fid' with `prio' to the hashtab
 */

void
fprio_set(VenusFid fid, Bool prio)
{
    struct fpriorityentry *e; 
    struct fpriorityentry key; 

    key.fid = fid;

    e = hashtabsearch(fpriohashtab, &key);
    if (e) {
	e->priority = prio;
	return;
    }

    e = calloc(1, sizeof(*e));
    if (e == NULL) {
	arla_warn(ADEBFCACHE, 1, "fprio_set: Out of memory");
	return;
    }
    e->fid = fid;
    e->priority = prio;
    
    hashtabadd(fpriohashtab, e);
}

/*
 * Read in new data from the file
 */

#define MAXFPRIOLINE 1024

int
fprio_readin(char *file)
{
    FILE *f;
    char line[MAXFPRIOLINE];
    unsigned prio;
    char cell[MAXFPRIOLINE];
    int32_t cellnum;
    VenusFid fid;
    int lineno = 0 ;

    f = fopen(file, "r");
    if (f == NULL)
	return -1;

    while(fgets(line, sizeof(line), f) != NULL) {
	lineno++;

	line[strcspn(line, "\n")] = '\0';

	if (line[0] == '#')
	    continue;

	if (sscanf(line, "%d:%s:%u:%u:%u", &prio, 
		   cell,
		   &fid.fid.Volume, 
		   &fid.fid.Vnode, 
		   &fid.fid.Unique) != 5) {
	    arla_warn(ADEBFCACHE, 1, 
		      "fprio_readin: %s:%d contain error(s)", 
		      file, lineno);
	    continue;
	}
	
	cellnum = cell_name2num(cell);
	if (cellnum == -1) {
	    arla_warn(ADEBFCACHE, 1, 
		      "fprio_readin: the cell %s does not exist", cell);
	    continue;
	}

	fid.Cell = cellnum;
	fprio_set(fid, prio ? TRUE : FALSE);
    }
    return 0;
}

/*
 * Find the priority of a fid
 */

Bool
fprio_get(VenusFid fid)
{
    struct fpriorityentry a;
    struct fpriorityentry *b;

    a.fid = fid;

    b = hashtabsearch(fpriohashtab, &a);
    if (b)
	return b->priority;
    return FALSE;
}

/*
 * Print the entry `ptr' to the FILE `arg'
 */

static Bool
fprio_print_entry (void *ptr, void *arg)
{
    struct fpriorityentry *n = (struct fpriorityentry *)ptr;
    const char *cell = cell_num2name(n->fid.Cell);
    const char *comment;

    if (cell == NULL)  /* If we cant find the cell comment it out */
	comment = "#";
    else
	comment = "";

    arla_log(ADEBVLOG, "%s%d:%s:%d:%d:%d", 
	     comment, n->priority == TRUE ? 1 : 0, cell?cell:"unknowncell", 
	     n->fid.fid.Volume, n->fid.fid.Vnode, n->fid.fid.Unique);
    return FALSE;
}

/*
 * Print the status of the fprio module in some strange format...
 */

void
fprio_status (void)
{
    time_t the_time = time(NULL);

    arla_log(ADEBVLOG, "#fprio entries\n#\n#  Date: %s\n#"
	     "#Syntax: (# means comment)\n"
	     "#priority:cell:volume:vnode:unique\n",
	     ctime(&the_time));
    hashtabforeach (fpriohashtab, fprio_print_entry, NULL);
}



