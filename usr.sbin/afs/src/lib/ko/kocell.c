/*	$OpenBSD: kocell.c,v 1.1.1.1 1998/09/14 21:53:00 art Exp $	*/
/*
 * Copyright (c) 1995, 1996, 1997, 1998 Kungliga Tekniska Högskolan
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

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif
#include <stdio.h>
#ifdef HAVE_SYS_TYPES_H
#include <sys/types.h>
#endif
#ifdef HAVE_NETINET_IN_H
#include <netinet/in.h>
#endif
#ifdef HAVE_ARPA_NAMESER_H
#include <arpa/nameser.h>
#endif
#ifdef HAVE_RESOLV_H
#include <resolv.h>
#endif

#ifdef KERBEROS
#include <kerberosIV/krb.h>
#endif

#include "ko_locl.h"
#include "resolve.h"

RCSID("$KTH: kocell.c,v 1.13 1998/07/28 14:54:08 assar Exp $");

#define TRANSARCSYSCONFDIR "/usr/vice/etc"
#define CELLFILENAME "CellServDB"
#define THISCELLFILENAME "ThisCell"

#define DEFCELLCACHESIZE 499

typedef struct {
     const char *name;
     struct in_addr addr;
} DbServerEntry;

typedef struct {
     int32_t id;		/* Cell-ID */
     const char *name;		/* Domain-style name */
     const char *expl;		/* Longer name */
     unsigned ndbservs;		/* # of database servers */
     DbServerEntry *dbservs;	/* Database servers */
} CellEntry;

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
     CellEntry *c1 = (CellEntry *)a;
     CellEntry *c2 = (CellEntry *)b;

     return strcmp (c1->name, c2->name);
}

static unsigned
cellnamehash (void *a)
{
     CellEntry *c = (CellEntry *)a;

     return hashadd (c->name);
}

static int
cellnumcmp (void *a, void *b)
{
     CellEntry *c1 = (CellEntry *)a;
     CellEntry *c2 = (CellEntry *)b;

     return c1->id != c2->id;
}

static unsigned
cellnumhash (void *a)
{
     CellEntry *c = (CellEntry *)a;

     return c->id;
}

/*
 * Record this cell in the hashtable
 */

static void
recordcell (CellEntry *c, int dbnum, DbServerEntry *dbservs)
{
     if(dbnum == 0)
	  KODEB (KODEBMISC, ("No db-servers for cell %s\n", c->name));

     c->ndbservs = dbnum;
     c->dbservs  = (DbServerEntry*)malloc (dbnum * sizeof(DbServerEntry));
     if (c->dbservs == NULL)
	 err (1, "malloc %u", dbnum * sizeof(DbServerEntry));
     memcpy (c->dbservs, dbservs, dbnum * sizeof (DbServerEntry));
							 
     hashtabadd (cellnamehtab, c);
     hashtabadd (cellnumhtab, c);
}

/*
 * New cell from cellserver-database file
 */

static CellEntry *
newcell (const char *line, int *dbnum)
{
     static long cellno = 0;
     char *hash;
     CellEntry *c;
     int len;
     char *tmp;

     c = (CellEntry *)malloc (sizeof (*c));
     if (c == NULL)
	 err (1, "malloc %u", sizeof(*c));
     len = strcspn (line, " \t#");
     tmp = malloc (len + 1);
     if (tmp == NULL)
	 err (1, "strdup");
     strncpy (tmp, line, len);
     tmp[len] = '\0';
     c->name = tmp;
     if (strcmp (c->name, cell_getthiscell ()) == 0)
	  c->id = 0;
     else
	  c->id = ++cellno;
     line += len + 1;
     hash = strchr (line, '#');
     if (hash != NULL) {
	  c->expl = strdup (hash+1);
	  if (c->expl == NULL)
	      err (1, "strdup");
     } else
	  c->expl = NULL;
     *dbnum = 0;
     return c;
}

/*
 * Read one line of database information.
 */

static int
readdb (char *line, CellEntry* c, int *dbnum, int maxdbs,
	DbServerEntry *dbservs, int lineno)
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
     CellEntry *c = NULL;
     int lineno = 0;
     int dbnum;
     DbServerEntry dbservs[256];

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
	       c = newcell (line + 1, &dbnum);
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
     if (cell[strlen(cell) - 1] == '\n')
	 cell[strlen(cell) - 1] = '\0';
     thiscell = strdup (cell);
     if (thiscell == NULL)
	 err (1, "strdup");
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
}

/*
 * Find a DB server to talk to for a given cell.
 */

struct in_addr
cell_finddbserver (int32_t cell)
{
     CellEntry key;
     CellEntry *data;
     struct in_addr addr;

     key.id = cell;
     data = hashtabsearch (cellnumhtab, &key);
     if (data == NULL) {
	  KODEB (KODEBMISC, ("Cannot find cell %d\n", cell));
	  return addr;
     }
     if (data->ndbservs == 0) {
	  KODEB (KODEBMISC, ("No DB servers for cell %d\n", cell));
	  return addr;
     }
     if (ipgetaddr (data->dbservs[0].name, &addr))
	  return addr;
     else
	  return data->dbservs[0].addr;
}

/*
 * Return DB server number "index" for a given cell.
 */

u_long
cell_listdbserver (int32_t cell, int index)
{
    CellEntry key;
    CellEntry *data;
    struct in_addr addr;
    
    key.id = cell;
    data = hashtabsearch (cellnumhtab, &key);
    if (data == NULL) {
	KODEB (KODEBMISC, ("Cannot find cell %d\n", cell));
	return 0;
    }
    if (data->ndbservs <= index) {
	return 0;
    }
    if (ipgetaddr (data->dbservs[index].name, &addr))
	return addr.s_addr;
    else
	return data->dbservs[index].addr.s_addr;
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
     CellEntry key;
     CellEntry *data;

     key.name = cell;
     data = hashtabsearch (cellnamehtab, &key);
     if (data == NULL) {
	  KODEB (KODEBMISC, ("Cannot find cell %d\n", cell));
	  return NULL;
     }
     if (data->ndbservs == 0) {
	  KODEB (KODEBMISC, ("No DB servers for cell %d\n", cell));
	  return NULL;
     }
     return data->dbservs[0].name ;
}

static void
try_to_find_cell(const char *cell)
{
    struct dns_reply *r;
    struct resource_record *rr;
    CellEntry *c = NULL;
    int dbnum;
    DbServerEntry dbservs[256];

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
		c = newcell (cell, &dbnum);
	    if (dbnum >= sizeof (dbservs) / sizeof(*dbservs)) {
		KODEB (KODEBMISC, ("Too many database servers for "
				   "cell %s. Ignoring\n", c->name));
	        break;
	    }
	    dbservs[dbnum].name = strdup (mx->domain);
	    if (dbservs[dbnum].name == NULL)
		err (1, "strdup");
	    dbservs[dbnum].addr.s_addr = inet_addr ("0.0.0.0");
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
		if (strcmp(dbservs[i].name,rr->domain) == 0) {
		    dbservs[i].addr = *(rr->u.a);
		    break;
		}
	    }
	}
	}
    }
    if (c)
	recordcell (c, dbnum, dbservs);
    dns_free_data(r);
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
     CellEntry key, *data;

     key.name = cell;
     data = hashtabsearch (cellnamehtab, (void *)&key);
     if (data)
	  return data->id;
     else {
	 try_to_find_cell(cell);
	 data = hashtabsearch (cellnamehtab, (void *)&key);
	 if (data)
	     return data->id;
	 else
	     return -1;
     }
}

/*
 * Return the name given the ID or NULL if the cell doesn't exist.
 */

const char *
cell_num2name (int32_t cell)
{
    CellEntry key, *data;

    key.id = cell;
    data = hashtabsearch (cellnumhtab, (void *)&key);
    if (data)
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
