/*	$OpenBSD: kocell.c,v 1.2 1999/04/30 01:59:11 art Exp $	*/
/*
 * Copyright (c) 1995, 1996, 1997, 1998, 1999 Kungliga Tekniska Högskolan
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
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *      This product includes software developed by the Kungliga Tekniska
 *      Högskolan and its contributors.
 * 
 * 4. Neither the name of the Institute nor the names of its contributors
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
 * Cell information
 */

#include "ko_locl.h"

#ifdef HAVE_ARPA_NAMESER_H
#include <arpa/nameser.h>
#endif
#ifdef HAVE_RESOLV_H
#include <resolv.h>
#endif

#ifdef KERBEROS
#include <krb.h>
#endif

#include "resolve.h"

RCSID("$KTH: kocell.c,v 1.19 1999/01/04 23:11:00 assar Exp $");

#define TRANSARCSYSCONFDIR "/usr/vice/etc"
#define CELLFILENAME "CellServDB"
#define THISCELLFILENAME "ThisCell"
#define SUIDCELLSFILENAME "SuidCells"
#define DEFCELLCACHESIZE 499

/*
 * hash tables by name and by number
 */

static Hashtab *cellnamehtab, *cellnumhtab;

/*
 * name of the current cell
 */

static char *thiscell = NULL;

/*
 * Functions for handling cell entries.
 */

static int
cellnamecmp (void *a, void *b)
{
     cell_entry *c1 = (cell_entry *)a;
     cell_entry *c2 = (cell_entry *)b;

     return strcmp (c1->name, c2->name);
}

static unsigned
cellnamehash (void *a)
{
     cell_entry *c = (cell_entry *)a;

     return hashadd (c->name);
}

static int
cellnumcmp (void *a, void *b)
{
     cell_entry *c1 = (cell_entry *)a;
     cell_entry *c2 = (cell_entry *)b;

     return c1->id != c2->id;
}

static unsigned
cellnumhash (void *a)
{
     cell_entry *c = (cell_entry *)a;

     return c->id;
}

/*
 * New cell from cellserver-database file
 */

static cell_entry *
newcell (char *line)
{
     char *hash;
     cell_entry *c;
     int len;

     len = strcspn (line, " \t#");
     line[len] = '\0';
     c = cell_new (line);
     if (c == NULL)
	 err (1, "malloc failed");

     line += len + 1;
     hash = strchr (line, '#');
     if (hash != NULL) {
	  c->expl = strdup (hash+1);
	  if (c->expl == NULL)
	      err (1, "strdup");
     }
     return c;
}

/*
 * Record this cell in the hashtable
 */

static void
recordcell (cell_entry *c, int dbnum, cell_db_entry *dbservers)
{
     if(dbnum == 0)
	  KODEB (KODEBMISC, ("No db-servers for cell %s\n", c->name));

     c->ndbservers  = dbnum;
     c->dbservers = (cell_db_entry *)malloc (dbnum * sizeof(cell_db_entry));
     if (c->dbservers == NULL)
	 err (1, "malloc %u", dbnum * sizeof(cell_db_entry));
     memcpy (c->dbservers, dbservers, dbnum * sizeof (cell_db_entry));
}

/*
 * try to lookup `cell' in DNS
 */

static void
try_to_find_cell(const char *cell)
{
    struct dns_reply *r;
    struct resource_record *rr;
    cell_entry *c = NULL;
    int dbnum = 0;
    cell_db_entry dbservers[256];

    r = dns_lookup(cell, "AFSDB");
    if (r == NULL)
	return;
    for(rr = r->head; rr;rr=rr->next){
	switch(rr->type){
	case T_AFSDB: {
	    struct mx_record *mx = (struct mx_record*)rr->u.data;

	    if (mx->preference != 1) {
		KODEB (KODEBMISC,
		       ("Ignoring host with cell type %d in cell %s",
			mx->preference, c->name));
		break;
	    }
	    if (c == NULL)
		c = cell_new (cell);
	    if (dbnum >= sizeof (dbservers) / sizeof(*dbservers)) {
		KODEB (KODEBMISC, ("Too many database servers for "
				   "cell %s. Ignoring\n", c->name));
	        break;
	    }
	    dbservers[dbnum].name = strdup (mx->domain);
	    if (dbservers[dbnum].name == NULL)
		err (1, "strdup");
	    dbservers[dbnum].addr.s_addr = inet_addr ("0.0.0.0");
	    ++dbnum;
	    break;
	}
	}
    }
    for(rr = r->head; rr;rr=rr->next){
	int i;

	switch(rr->type){
	case T_A: {
	    for (i = 0; i < dbnum; i++) {
		if (strcmp(dbservers[i].name,rr->domain) == 0) {
		    dbservers[i].addr = *(rr->u.a);
		    break;
		}
	    }
	}
	}
    }
    if (c)
	recordcell (c, dbnum, dbservers);
    dns_free_data(r);
}

/*
 *
 */

cell_entry *
cell_get_by_name (const char *cellname)
{
    cell_entry key, *data;

    key.name = cellname;
    data = (cell_entry *)hashtabsearch (cellnamehtab, &key);
    if (data == NULL) {
	try_to_find_cell (cellname);
	data = (cell_entry *)hashtabsearch (cellnamehtab, &key);
    }
    return data;
}

/*
 *
 */

cell_entry *
cell_get_by_id (int32_t cell)
{
    cell_entry key;

    key.id = cell;
    return (cell_entry *)hashtabsearch (cellnumhtab, &key);
}

/*
 * cells are assigned monotonically increasing numbers
 */

static long cellno = 0;

/*
 *
 */

cell_entry *
cell_new (const char *name)
{
    cell_entry *c;

    c = (cell_entry *)malloc (sizeof (*c));
    if (c == NULL)
	return NULL;
    c->name = strdup (name);
    if (c->name == NULL) {
	free (c);
	return NULL;
    }
    if (strcmp (c->name, cell_getthiscell ()) == 0)
	c->id = 0;
    else
	c->id = ++cellno;
    c->expl       = NULL;
    c->ndbservers = 0;
    c->dbservers  = NULL;
    c->suid_cell = NOSUID_CELL;
    hashtabadd (cellnamehtab, c);
    hashtabadd (cellnumhtab, c);
    return c;
}

/*
 * Read one line of database information.
 */

static int
readdb (char *line, cell_entry* c, int *dbnum, int maxdbs,
	cell_db_entry *dbservs, int lineno)
{
     struct in_addr numaddr;
     char *hostname;

     if (*dbnum >= maxdbs) {
	  KODEB (KODEBMISC, ("Too many database servers for "
		   "cell %s. Ignoring\n", c->name));
	  return -1;
     }
     if ( (hostname = strchr (line, '#')) == NULL
	 || (numaddr.s_addr = inet_addr (line)) == -1 ) {
	  KODEB (KODEBMISC, ("Syntax error at line %d in %s\n",
		   lineno, CELLFILENAME));
	  return -1;
     } else {
	  ++hostname;
	  dbservs[*dbnum].name = strdup (hostname);
	  if (dbservs[*dbnum].name == NULL)
	      err (1, "strdup");
	  dbservs[*dbnum].addr = numaddr;
	  ++(*dbnum);
     }
     return 0;
}

/*
 * Read the information from the cell-server db.
 */

static int
readcellservdb (const char *filename)
{
     FILE *f;
     char line[256];
     cell_entry *c = NULL;
     int lineno = 0;
     int dbnum;
     cell_db_entry dbservs[256];

     f = fopen (filename, "r");
     if (f == NULL) {
	  KODEB (KODEBMISC, ("Cannot read cell information from %s\n", 
		   filename));
	  return 1;
     }

     while (fgets (line, sizeof (line), f)) {
	  ++lineno;
	  if (line[strlen(line) - 1 ] != '\n')
	       KODEB (KODEBMISC, ("Too long line at line %d in %s\n",
			lineno, filename));
	  else
	       line[strlen(line) - 1] = '\0';
	  if (*line == '#' || *line == '\0')
	       continue;
	  if (*line == '>') {
	       if (c != NULL)
		    recordcell (c, dbnum, dbservs);
	       c = newcell (line + 1);
	       dbnum = 0;
	  } else {
	       if (readdb(line, c, &dbnum, sizeof (dbservs) /
			  sizeof(*dbservs),
			  dbservs, lineno))
		    continue;
	  }
     }
     if (c != NULL)
	  recordcell (c, dbnum, dbservs);
     fclose (f);
     return 0;
}

/*
 * Read name of this cell.
 */

static int
readthiscell (const char *filename)
{
     FILE *f;
     char cell[256];

     f = fopen (filename, "r");
     if (f == NULL) {
	 fprintf(stderr, "Can't open file %s.\n", filename);
	 return 1;
     }

     if (fgets (cell, sizeof cell, f) == NULL) {
	  fprintf(stderr, "Cannot read cellname from %s\n",
		  filename);
	  return 1;
     }
     cell[sizeof(cell)-1] = '\0';
     if (cell[strlen(cell) - 1] == '\n')
	 cell[strlen(cell) - 1] = '\0';
     thiscell = strdup (cell);
     if (thiscell == NULL)
	 err (1, "strdup");
     fclose (f);
     return 0;
}

/*
 * Read suidcells file and set suidcell flag
 */

static int
readsuidcell (const char *filename)
{
     FILE *f;
     char cell[256];

     f = fopen (filename, "r");
     if (f == NULL) {
	 fprintf(stderr, "Can't open file %s.\n", filename);
	 return 1;
     }

     while (fgets (cell, sizeof(cell), f) != NULL) {
	 int i;
	 cell_entry *e;

	 cell[sizeof(cell)-1] = '\0';
	 i = strlen (cell);
	 if (cell[i - 1] == '\n')
	     cell[i - 1] = '\0';

	 e = cell_get_by_name (cell);
	 if (e == NULL) {
	     fprintf (stderr, "cell %s doesn't exist in the db\n", cell);
	 } else {
	     e->suid_cell = SUID_CELL;
	 }
     }
     fclose (f);
     return 0;
}

/*
 * Initialize the cache of cell information.
 */

void
cell_init (int cellcachesize)
{
    char *env;

    if (thiscell != NULL) {
	fprintf(stderr, "cell_init: Already initlized\n");
	return;
    }

    if (cellcachesize == 0)
	cellcachesize = DEFCELLCACHESIZE;

    cellnamehtab = hashtabnew (cellcachesize, cellnamecmp, cellnamehash);
    if (cellnamehash == NULL)
	errx (1, "cell_init: hashtabnew failed");
    cellnumhtab  = hashtabnew (cellcachesize, cellnumcmp,  cellnumhash);
    if (cellnumhtab == NULL)
	errx (1, "cell_init: hashtabnew failed");

    env = getenv ("AFSCELL");
    if (env != NULL) {
	thiscell = strdup (env);
	if (thiscell == NULL)
	    err (1, "strdup");
    } else if (readthiscell (SYSCONFDIR "/" THISCELLFILENAME)) {
	fprintf(stderr, "Falling back on: " TRANSARCSYSCONFDIR "\n"); 
	if (readthiscell(TRANSARCSYSCONFDIR "/" THISCELLFILENAME)) {
	    fprintf(stderr, "Don't know where I am\n");
	    exit(1);
	}
    }

    if (readcellservdb (SYSCONFDIR "/" CELLFILENAME)) {
	fprintf(stderr, "Falling back on: " TRANSARCSYSCONFDIR "\n");
	if (readcellservdb(TRANSARCSYSCONFDIR "/" CELLFILENAME)) {
	    fprintf(stderr, "Can't read the CellServDB file," \
		    "will use DNS AFSDB entries\n");
	}
    }
    if (readsuidcell (SYSCONFDIR "/" SUIDCELLSFILENAME)) {
	fprintf (stderr, "Falling back on: " TRANSARCSYSCONFDIR "\n");
	if (readsuidcell (TRANSARCSYSCONFDIR "/" SUIDCELLSFILENAME))
	    fprintf (stderr, "Cant read suid cells, ignoring\n");
    }
}

/*
 * Return all db servers for `cell' with the count in `num'.
 * NULL on error.
 */

const cell_db_entry *
cell_dbservers (int32_t cell, int *num)
{
    cell_entry *data = cell_get_by_id (cell);

    if (data == NULL) {
	KODEB (KODEBMISC, ("Cannot find cell %d\n", cell));
	return NULL;
    }
    *num = data->ndbservers;
    return data->dbservers;
}

/*
 * Find the name of DB server to talk to for a given cell.
 *
 *   Warning: This is not really good since name could be something
 *     like NULL.
 */

const char *
cell_findnamedbbyname (const char *cell)
{
     cell_entry *data = cell_get_by_name (cell);

     if (data == NULL) {
	  KODEB (KODEBMISC, ("Cannot find cell %d\n", cell));
	  return NULL;
     }
     if (data->ndbservers == 0) {
	  KODEB (KODEBMISC, ("No DB servers for cell %d\n", cell));
	  return NULL;
     }
     return data->dbservers[0].name ;
}

/*
 * Get the cell of the host
 */

const char *   
cell_getcellbyhost(const char *host)
{
    const char *ptr = NULL;
    assert(host);
    
#ifdef KERBEROS
    ptr = krb_realmofhost(host);
#endif
    if (ptr)
	return ptr;

    ptr = host;
    while(*ptr && *ptr != '.') ptr++;
    
    if (*ptr == '\0')
	return NULL;

    return strdup(ptr); 
}

/*
 * Return the ID given the name for a cell.
 * -1 if the cell does not exist.
 */

int32_t
cell_name2num (const char *cell)
{
     cell_entry *data = cell_get_by_name (cell);

     if (data != NULL)
	 return data->id;
     else
	 return -1;
}

/*
 * Return the name given the ID or NULL if the cell doesn't exist.
 */

const char *
cell_num2name (int32_t cell)
{
    cell_entry *data = cell_get_by_id (cell);

    if (data != NULL)
	return data->name;
    else
	return NULL;
}

/*
 * Return name of the cell of the cache manager.
 */

const char *
cell_getthiscell (void)
{
     return thiscell;
}

/*
 * Return if this is a suid cell
 */

Bool
cell_issuid (cell_entry *c)
{
    assert (c);

    return c->suid_cell == SUID_CELL;
}

Bool
cell_issuid_by_num (int32_t cell)
{
    cell_entry *c = cell_get_by_id (cell);
    if (c == NULL)
	return FALSE;
    
    return cell_issuid (c);
}

Bool
cell_issuid_by_name (const char *cell)
{
    cell_entry *c = cell_get_by_name (cell);
    if (c == NULL)
	return FALSE;
    
    return cell_issuid (c);
}

Bool
cell_setsuid_by_num (int32_t cell)
{
    cell_entry *c = cell_get_by_id (cell);
    if (c == NULL)
	return FALSE;
    
    c->suid_cell = SUID_CELL;

    return 0;
}
