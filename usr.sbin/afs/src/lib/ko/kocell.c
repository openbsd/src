/*
 * Copyright (c) 1995 - 2000 Kungliga Tekniska Högskolan
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

RCSID("$Id: kocell.c,v 1.3 2000/09/11 14:40:57 art Exp $");

#define TRANSARCSYSCONFDIR "/usr/vice/etc"
#define CELLFILENAME "CellServDB"
#define THISCELLFILENAME "ThisCell"
#define SUIDCELLSFILENAME "SuidCells"
#define THESECELLFILENAME "TheseCells"
#define DEFCELLCACHESIZE 499

/*
 * hash tables by name and by number
 */

static Hashtab *cellnamehtab, *cellnumhtab;

/*
 * name of the current cell
 */

static char *thiscell = NULL;
static char **thesecells = NULL;
static int numthesecells = 1;


/*
 *
 */

static unsigned long celldb_version = 0;

static int suid_read = 0;

static int add_special_dynroot_cell (void);

/*
 * Functions for handling cell entries.
 */

static int
cellnamecmp (void *a, void *b)
{
     cell_entry *c1 = (cell_entry *)a;
     cell_entry *c2 = (cell_entry *)b;

     return strcasecmp (c1->name, c2->name);
}

static unsigned
cellnamehash (void *a)
{
     cell_entry *c = (cell_entry *)a;

     return hashcaseadd (c->name);
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
     c->ndbservers = dbnum;
     c->dbservers  = malloc (dbnum * sizeof(cell_db_entry));
     if (c->dbservers == NULL)
	 err (1, "malloc %lu", (unsigned long)dbnum * sizeof(cell_db_entry));
     memcpy (c->dbservers, dbservers, dbnum * sizeof (cell_db_entry));
}

/*
 * try to lookup `cell' in DNS
 * if c == NULL, a new cell will be allocated
 */

static void
try_to_find_cell(const char *cell, cell_entry *c)
{
    struct dns_reply *r;
    struct resource_record *rr;
    int dbnum = 0;
    cell_db_entry dbservers[256];

    r = dns_lookup(cell, "AFSDB");
    if (r == NULL)
	return;
    for(rr = r->head; rr;rr=rr->next){
	if(rr->type == T_AFSDB) {
	    struct mx_record *mx = (struct mx_record*)rr->u.data;

	    if (mx->preference != 1) {
		break;
	    }
	    if (c == NULL)
		c = cell_new_dynamic (cell);
	    if (dbnum >= sizeof (dbservers) / sizeof(*dbservers)) {
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
    for(rr = r->head; rr;rr=rr->next){
	if (rr->type == T_A) {
	    int i;

	    for (i = 0; i < dbnum; i++) {
		if (strcmp(dbservers[i].name,rr->domain) == 0) {
		    dbservers[i].addr = *(rr->u.a);
		    break;
		}
	    }
	}
    }
    if (c)
	recordcell (c, dbnum, dbservers);
    dns_free_data(r);
}

/*
 * cell name -> cell_entry *
 */

cell_entry *
cell_get_by_name (const char *cellname)
{
    cell_entry key, *data;

    key.name = cellname;
    data = (cell_entry *)hashtabsearch (cellnamehtab, &key);
    if (data == NULL) {
	try_to_find_cell (cellname, NULL);
	data = (cell_entry *)hashtabsearch (cellnamehtab, &key);
    }
    return data;
}

/*
 * cell id -> cell_entry *
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
 * that dont use 0.
 */

static int32_t cellno = 1;

/*
 * Add the cell `name' to the cell cache.
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
    assert (cellno != 0);
    c->id         = cellno++;
    c->expl       = NULL;
    c->ndbservers = 0;
    c->dbservers  = NULL;
    c->suid_cell  = NOSUID_CELL;
    hashtabadd (cellnamehtab, c);
    hashtabadd (cellnumhtab, c);
    celldb_version++;
    return c;
}

/*
 * add a new `dynamic' cell
 */

cell_entry *
cell_new_dynamic (const char *name)
{
    cell_entry *c;
    FILE *f;

    c = cell_new (name);
    if (c == NULL)
	return NULL;
    f = fopen (SYSCONFDIR "/" CELLFILENAME, "a");
    if (f == NULL)
	f = fopen (TRANSARCSYSCONFDIR "/" CELLFILENAME, "a");
    if (f == NULL) {
	fprintf (stderr, "Cannot open CellServDB for writing\n");
	return c;
    }
    fprintf (f, ">%s	#dynamically added cell\n", name);
    fclose (f);
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
     char *hostname, *eh;

     if (*dbnum >= maxdbs) {
	  return -1;
     }
     if (inet_aton (line, &numaddr) == 0) {
	  return -1;
     }

     while (*line && !isspace(*line) && *line != '#')
	 ++line;

     hostname = line;

     while (isspace (*hostname) || *hostname == '#')
	 ++hostname;

     eh = hostname;

     while (*eh && !isspace(*eh) && *eh != '#')
	 ++eh;

     *eh = '\0';

     if (*hostname == '\0')
	 hostname = inet_ntoa (numaddr);
     
     dbservs[*dbnum].name = strdup (hostname);
     if (dbservs[*dbnum].name == NULL)
	 err (1, "strdup");
     dbservs[*dbnum].addr = numaddr;
     ++(*dbnum);
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
	  return 1;
     }

     while (fgets (line, sizeof (line), f)) {
	  ++lineno;
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
 * Add expanded `cellname' to Thesecells list, filter out duplicates
 */

static void
addthesecell (const char *cellname)
{
    int i;
    cellname = cell_expand_cell (cellname);
    for (i = 0; thesecells && thesecells[i]; i++)
	if (strcasecmp (thesecells[i], cellname) == 0)
	    return;
    thesecells = erealloc (thesecells, 
			   (numthesecells + 1) * sizeof (char *));
    thesecells[numthesecells - 1] = estrdup (cellname);
    thesecells[numthesecells] = NULL;
    ++numthesecells;
}

/*
 * Read cells in these cell's
 */

static int
readthesecells (const char *filename)
{
     FILE *f;
     char cell[256];

     f = fopen (filename, "r");
     if (f == NULL)
	 return 1;

     while ((fgets (cell, sizeof cell, f) != NULL)) {
	 cell[strlen(cell) - 1] = '\0';
	 addthesecell (cell);
     }
     fclose (f);
     if (numthesecells == 1)
	 return 1;
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
     if (f == NULL)
	 return 1;

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
 * Read suidcells file and set suidcell flag
 */

static int
readsuidcell (const char *filename)
{
     FILE *f;
     char cell[256];

     f = fopen (filename, "r");
     if (f == NULL)
	 return 1;

     while (fgets (cell, sizeof(cell), f) != NULL) {
	 int i;
	 cell_entry *e;

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
 * initialize suid information, if hasn't already been done
 */

static void
cond_readsuidcell (void)
{
    if (suid_read)
	return;

    if (readsuidcell (SYSCONFDIR "/" SUIDCELLSFILENAME))
	readsuidcell (TRANSARCSYSCONFDIR "/" SUIDCELLSFILENAME);
    suid_read = 1;
}

/*
 * Initialize the cache of cell information.
 */

static int cell_inited = 0;

void
cell_init (int cellcachesize)
{
    char *env;
    int ret;

    if (cell_inited) {
	fprintf(stderr, "cell_init: Already initlized\n");
	return;
    }
    cell_inited = 1;

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
	thiscell = estrdup (env);
    } else if (readthiscell (SYSCONFDIR "/" THISCELLFILENAME)) {
	if (readthiscell(TRANSARCSYSCONFDIR "/" THISCELLFILENAME))
	    errx (1, "could not open "
		  SYSCONFDIR "/" THISCELLFILENAME
		  " nor "
		  TRANSARCSYSCONFDIR "/" THISCELLFILENAME);
    }

    if (readcellservdb (SYSCONFDIR "/" CELLFILENAME)) {
	if (readcellservdb(TRANSARCSYSCONFDIR "/" CELLFILENAME)) {
	    fprintf(stderr, "Can't read the CellServDB file," \
		    "will use DNS AFSDB entries\n");
	}
    }
    ret = add_special_dynroot_cell();
    if (ret)
	fprintf (stderr, "adding dynroot cell failed with %d", ret);

    if (readthesecells (SYSCONFDIR "/" THESECELLFILENAME))
	readthesecells (TRANSARCSYSCONFDIR "/" THESECELLFILENAME);
    if (getenv("HOME") != NULL) {
	char homedir[MAXPATHLEN];
	snprintf (homedir, sizeof(homedir), 
		  "%s/." THESECELLFILENAME,
		  getenv("HOME"));
	readthesecells (homedir);
    }
    addthesecell (thiscell);
}

/*
 *
 */

const char **
cell_thesecells (void)
{
    return (const char **)thesecells;
}

/*
 * Return all db servers for `cell' with the count in `num'.
 * NULL on error.
 */

const cell_db_entry *
cell_dbservers_by_id (int32_t id, int *num)
{
    cell_entry *data = cell_get_by_id (id);

    if (data == NULL)
	return NULL;

    if (data->ndbservers == 0)
	try_to_find_cell (data->name, data);

    *num = data->ndbservers;
    return data->dbservers;
}

/*
 * Return the name of the first database server in `cell' or NULL (if
 * the cell does not exist or it has no db servers).
 */

const char *
cell_findnamedbbyname (const char *cell)
{
     cell_entry *data = cell_get_by_name (cell);

     if (data == NULL)
	  return NULL;
     if (data->ndbservers == 0)
	 try_to_find_cell (cell, data);
     if (data->ndbservers == 0)
	 return NULL;

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

    ptr = strchr (host, '.');
    if (ptr == NULL)
	return NULL;
    return strdup (ptr + 1);
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
    assert (thiscell != NULL);

    return thiscell;
}

/*
 *
 */

int
cell_setthiscell (const char *cell)
{
    cell_entry *data;

    if (!cell_inited)
	cell_init (0);

    data = cell_get_by_name(cell);
    if (data == NULL) {
	fprintf (stderr, "this cell doesn't exist: %s", cell);
	return 1;
    }

    free (thiscell);
    thiscell = strdup (cell);
    if (thiscell == NULL)
	abort();
    
    return 0;
}

/*
 * Return if this is a suid cell
 */

Bool
cell_issuid (cell_entry *c)
{
    assert (c);

    cond_readsuidcell ();
    return c->suid_cell == SUID_CELL;
}

Bool
cell_issuid_by_num (int32_t cell)
{
    cell_entry *c;

    cond_readsuidcell ();
    c = cell_get_by_id (cell);
    if (c == NULL)
	return FALSE;
    
    return cell_issuid (c);
}

Bool
cell_issuid_by_name (const char *cell)
{
    cell_entry *c;

    cond_readsuidcell ();
    c = cell_get_by_name (cell);
    if (c == NULL)
	return FALSE;
    
    return cell_issuid (c);
}

Bool
cell_setsuid_by_num (int32_t cell)
{
    cell_entry *c;

    cond_readsuidcell ();
    c = cell_get_by_id (cell);
    if (c == NULL)
	return FALSE;
    
    c->suid_cell = SUID_CELL;

    return 0;
}

/*
 * Iterate over all entries in the cell-db with `func'
 * (that takes the cellname and `arg' as arguments)
 */

struct cell_iterate_arg {
    void *arg;
    int ret;
    int (*func) (const cell_entry *, void *);
};

static int
cell_iterate_func (void *ptr, void *arg)
{
    struct cell_iterate_arg *cia = (struct cell_iterate_arg *) arg;
    cell_entry *cell = (cell_entry *) ptr;
    int ret;

    ret = (cia->func) (cell, cia->arg);
    if (ret)
	cia->ret = ret;
    return ret;
}

int
cell_foreach (int (*func) (const cell_entry *, void *), void *arg)
{
    struct cell_iterate_arg cia;

    cia.arg = arg;
    cia.ret = 0;
    cia.func = func;

    hashtabforeach (cellnamehtab, cell_iterate_func, &cia);

    return cia.ret;
}

/*
 *
 */

struct cell_expand_arg {
    const char *cell;
    const char *fullcell;
    int32_t last_found;
};

static int
cell_expand_cell_name (const cell_entry *cell, void *data)
{
    struct cell_expand_arg *cea = (struct cell_expand_arg *) data;
    if (strcasecmp (cell->name, cea->cell) == 0) {
	cea->fullcell = cell->name;
	return 1;
    }
    if (strstr (cell->name, cea->cell) && cea->last_found > cell->id) {
	cea->last_found = cell->id;
	cea->fullcell = cell->name;
    }
    return 0;
}

const char *
cell_expand_cell (const char *cell)
{
    struct cell_expand_arg cea;
    cea.cell = cell;
    cea.fullcell = NULL;
    cea.last_found = cellno + 1;

    cell_foreach (cell_expand_cell_name, &cea);
    if (cea.fullcell)
	return cea.fullcell;
    return cell;
}

/*
 *
 */

unsigned long
cell_get_version(void)
{
    return celldb_version;
}

/*
 *
 */

static int
add_special_dynroot_cell (void)
{
    cell_entry * c;

    c = cell_get_by_id (0);
    if (c != NULL)
	abort();

    c = (cell_entry *)malloc (sizeof (*c));
    if (c == NULL)
	return errno;
    c->name = strdup ("#dynrootcell#");
    if (c->name == NULL) {
	free (c);
	return errno;
    }
    c->id         = 0; /* XXX */
    c->expl       = "The special dynroot cell";
    c->ndbservers = 0;
    c->dbservers  = NULL;
    c->suid_cell  = NOSUID_CELL;
    hashtabadd (cellnamehtab, c);
    hashtabadd (cellnumhtab, c);
    return 0;
}

/*
 * Return TRUE if ``cell'' has a sane cellnumber.
 */

Bool
cell_is_sanep (int cell)
{
    return cell < cellno;
} 
