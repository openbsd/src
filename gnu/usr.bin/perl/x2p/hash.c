/*    hash.c
 *
 *    Copyright (C) 1991, 1992, 1993, 1994, 1995, 1999, 2000, 2001, 2002,
 *    2005 by Larry Wall and others
 *
 *    You may distribute under the terms of either the GNU General Public
 *    License or the Artistic License, as specified in the README file.
 */

#include <stdio.h>
#include "EXTERN.h"
#include "a2p.h"
#include "util.h"

#ifdef NETWARE
char *savestr(char *str);
#endif

STR *
hfetch(HASH *tb, char *key)
{
    char *s;
    int i;
    int hash;
    HENT *entry;

    if (!tb)
	return NULL;
    for (s=key,		i=0,	hash = 0;
      /* while */ *s;
	 s++,		i++,	hash *= 5) {
	hash += *s * coeff[i];
    }
    entry = tb->tbl_array[hash & tb->tbl_max];
    for (; entry; entry = entry->hent_next) {
	if (entry->hent_hash != hash)		/* strings can't be equal */
	    continue;
	if (strNE(entry->hent_key,key))	/* is this it? */
	    continue;
	return entry->hent_val;
    }
    return NULL;
}

bool
hstore(HASH *tb, char *key, STR *val)
{
    char *s;
    int i;
    int hash;
    HENT *entry;
    HENT **oentry;

    if (!tb)
	return FALSE;
    for (s=key,		i=0,	hash = 0;
      /* while */ *s;
	 s++,		i++,	hash *= 5) {
	hash += *s * coeff[i];
    }

    oentry = &(tb->tbl_array[hash & tb->tbl_max]);
    i = 1;

    for (entry = *oentry; entry; i=0, entry = entry->hent_next) {
	if (entry->hent_hash != hash)		/* strings can't be equal */
	    continue;
	if (strNE(entry->hent_key,key))	/* is this it? */
	    continue;
	/*NOSTRICT*/
	safefree(entry->hent_val);
	entry->hent_val = val;
	return TRUE;
    }
    /*NOSTRICT*/
    entry = (HENT*) safemalloc(sizeof(HENT));

    entry->hent_key = savestr(key);
    entry->hent_val = val;
    entry->hent_hash = hash;
    entry->hent_next = *oentry;
    *oentry = entry;

    if (i) {				/* initial entry? */
	tb->tbl_fill++;
	if ((tb->tbl_fill * 100 / (tb->tbl_max + 1)) > FILLPCT)
	    hsplit(tb);
    }

    return FALSE;
}

void
hsplit(HASH *tb)
{
    const int oldsize = tb->tbl_max + 1;
    int newsize = oldsize * 2;
    int i;
    HENT **a;
    HENT **b;
    HENT *entry;
    HENT **oentry;

    a = (HENT**) saferealloc((char*)tb->tbl_array, newsize * sizeof(HENT*));
    memset(&a[oldsize], 0, oldsize * sizeof(HENT*)); /* zero second half */
    tb->tbl_max = --newsize;
    tb->tbl_array = a;

    for (i=0; i<oldsize; i++,a++) {
	if (!*a)				/* non-existent */
	    continue;
	b = a+oldsize;
	for (oentry = a, entry = *a; entry; entry = *oentry) {
	    if ((entry->hent_hash & newsize) != i) {
		*oentry = entry->hent_next;
		entry->hent_next = *b;
		if (!*b)
		    tb->tbl_fill++;
		*b = entry;
		continue;
	    }
	    else
		oentry = &entry->hent_next;
	}
	if (!*a)				/* everything moved */
	    tb->tbl_fill--;
    }
}

HASH *
hnew(void)
{
    HASH *tb = (HASH*)safemalloc(sizeof(HASH));

    tb->tbl_array = (HENT**) safemalloc(8 * sizeof(HENT*));
    tb->tbl_fill = 0;
    tb->tbl_max = 7;
    hiterinit(tb);	/* so each() will start off right */
    memset(tb->tbl_array, 0, 8 * sizeof(HENT*));
    return tb;
}

int
hiterinit(HASH *tb)
{
    tb->tbl_riter = -1;
    tb->tbl_eiter = (HENT*)NULL;
    return tb->tbl_fill;
}
