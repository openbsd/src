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

RCSID("$KTH: kocell.c,v 1.47.2.3 2001/07/09 03:27:48 lha Exp $");

#define TRANSARCSYSCONFDIR "/usr/vice/etc"
#define CELLFILENAME "CellServDB"
#define THISCELLFILENAME "ThisCell"
#define SUIDCELLSFILENAME "SuidCells"
#define THESECELLFILENAME "TheseCells"
#define DYNROOTDBFILENAME "DynRootDB"
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
static int dynrootdb_in_use;

enum { CELL_INVALID_HOST = 1 };

/*
 * Logging
 */

#define CDEBERR		0x800
#define CDEBWARN	0x400
#define CDEBDNS		0x200

#define CDEBDEFAULT	(CDEBWARN|CDEBERR)
#define cdeball		(CDEBWARN|CDEBERR)

static struct units celldebug_units[]  = {
    { "all", 		cdeball },
    { "almost-all",	cdeball },
    { "warn",		CDEBWARN },
    { "error", 		CDEBERR },
    { "dns",		CDEBDNS },
    { NULL, 0 },
};

#undef cdeball

static Log_unit *cell_log = NULL;

/*
 *
 */

static unsigned long celldb_version = 0;

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
 *
 */

static void
fetch_host (cell_entry *c, cell_db_entry *host)
{
    struct dns_reply *r;
    struct resource_record *rr;
    struct timeval tv;

    gettimeofday (&tv, NULL);
    
    r = dns_lookup(host->name, "A");
    if (r == NULL) {
	log_log (cell_log, CDEBDNS, 
		 "fetch_host: failed to resolve host %s in cell %s",
		 host->name, c->name);
	host->timeout = CELL_INVALID_HOST;
	return;
    }
    for(rr = r->head; rr;rr=rr->next){
	if (rr->type == T_A) {
	    if (strcmp(host->name,rr->domain) == 0) {
		host->addr = *(rr->u.a);
		host->timeout = tv.tv_sec + rr->ttl;
		break;
	    }
	}
    }
    dns_free_data(r);
}


/*
 * Help function for updatehosts
 */

static int 
host_sort (const void *p1, const void *p2)
{
    const cell_db_entry *a = (const cell_db_entry *)p1;
    const cell_db_entry *b = (const cell_db_entry *)p2;
    if (a->timeout == CELL_INVALID_HOST)
	return -1;
    if (b->timeout == CELL_INVALID_HOST)
	return 1;
    return a->addr.s_addr - b->addr.s_addr;
}

/*
 * Update the hosts for this cell
 */

static void
updatehosts (cell_entry *c, int dbnum, cell_db_entry *dbservers)
{
    cell_db_entry *old_servers = c->dbservers;
    struct timeval tv;
    int i;

    gettimeofday(&tv, NULL);

    for (i = 0; i < dbnum; i++) {
	if (dbservers[i].timeout == CELL_INVALID_HOST
	    || (dbservers[i].timeout && dbservers[i].timeout < tv.tv_sec)) {
	    fetch_host (c, &dbservers[i]);
	}
    }

    c->ndbservers = dbnum;
    c->dbservers  = malloc (dbnum * sizeof(cell_db_entry));
    if (c->dbservers == NULL && dbnum != 0)
	err (1, "malloc %lu", (unsigned long)dbnum * sizeof(cell_db_entry));
    memcpy (c->dbservers, dbservers, dbnum * sizeof (cell_db_entry));
    free (old_servers);

    if (c->ndbservers)
	qsort (c->dbservers, c->ndbservers, sizeof(c->dbservers[0]),
	       host_sort);

    c->active_hosts = 0;
    for (i = 0; i < c->ndbservers; i++) {
	if (c->dbservers[i].timeout != CELL_INVALID_HOST)
	    c->active_hosts++;
    }
}

/*
 * try to lookup `cell' in DNS
 * if c == NULL, a new cell will be allocated
 */

static int
dns_lookup_cell (const char *cell, cell_entry *c)
{
    struct dns_reply *r;
    struct resource_record *rr;
    int dbnum = 0;
    cell_db_entry dbservers[256];
    int lowest_ttl = INT_MAX;
    int i;
    struct timeval tv;

    memset (dbservers, 0, sizeof(dbservers));
    gettimeofday(&tv, NULL);

    r = dns_lookup(cell, "AFSDB");
    if (r == NULL) {
	log_log (cell_log, CDEBDNS, 
		 "dns_lookup_cell: failed to resolve cell %s", cell);
	return 1;
    }
    if (c == NULL)
	c = cell_new_dynamic (cell);

    for(rr = r->head; rr;rr=rr->next){
	if(rr->type == T_AFSDB) {
	    struct mx_record *mx = (struct mx_record*)rr->u.data;

	    if (mx->preference != 1) {
		break;
	    }
	    if (dbnum >= sizeof (dbservers) / sizeof(*dbservers)) {
	        break;
	    }
	    if (lowest_ttl > rr->ttl)
		lowest_ttl = rr->ttl;
	    dbservers[dbnum].name = strdup (mx->domain);
	    if (dbservers[dbnum].name == NULL)
		err (1, "strdup");
	    dbservers[dbnum].timeout = CELL_INVALID_HOST;
	    dbnum++;
	}
    }
    for(rr = r->head; rr;rr=rr->next){
	if (rr->type == T_A) {
	    for (i = 0; i < dbnum; i++) {
		if (strcmp(dbservers[i].name,rr->domain) == 0) {
		    dbservers[i].addr = *(rr->u.a);
		    dbservers[i].timeout = tv.tv_sec + rr->ttl;
		    break;
		}
	    }
	}
    }
    dns_free_data(r);

    if (lowest_ttl == INT_MAX)
	lowest_ttl = 0;
    
    /* catch the hosts that didn't fit in additional rr */
    c->timeout = lowest_ttl + tv.tv_sec;
    updatehosts (c, dbnum, dbservers);
    return 0;
}

/*
 * If the cell-information comes from a source that have a time-limit,
 * make sure the data is uptodate.
 */

static void
update_cell (cell_entry *c)
{
    struct timeval tv;

    gettimeofday(&tv, NULL);

    if ((c->timeout && c->timeout < tv.tv_sec)
	|| c->ndbservers == 0) {
	dns_lookup_cell (c->name, c);
    }
    updatehosts (c, c->ndbservers, c->dbservers);
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
	dns_lookup_cell (cellname, NULL);
	data = (cell_entry *)hashtabsearch (cellnamehtab, &key);
    }
    if (data)
	update_cell (data);
    return data;
}

/*
 * cell id -> cell_entry *
 */

cell_entry *
cell_get_by_id (int32_t cell)
{
    cell_entry key, *data;

    key.id = cell;
    data = (cell_entry *)hashtabsearch (cellnumhtab, &key);
    if (data)
	update_cell (data);
    return data;
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
    c->id           = cellno++;
    c->expl         = NULL;
    c->ndbservers   = 0;
    c->active_hosts = 0;
    c->dbservers    = NULL;
    c->flags	    = 0;
    hashtabadd (cellnamehtab, c);
    hashtabadd (cellnumhtab, c);
    c->timeout      = 0;
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
    c->expl = "dynamically added cell";
    f = fopen (SYSCONFDIR "/" CELLFILENAME, "a");
    if (f == NULL)
	f = fopen (TRANSARCSYSCONFDIR "/" CELLFILENAME, "a");
    if (f == NULL) {
	log_log (cell_log, CDEBWARN,"Cannot open CellServDB for writing");
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
     
     while (*line && isspace((unsigned char)*line))
	    ++line;

     if (inet_aton (line, &numaddr) == 0) {
	  return -1;
     }

     while (*line && !isspace((unsigned char)*line) && *line != '#')
	 ++line;

     hostname = line;

     while (isspace ((unsigned char)*hostname) || *hostname == '#')
	 ++hostname;

     eh = hostname;

     while (*eh && !isspace((unsigned char)*eh) && *eh != '#')
	 ++eh;

     *eh = '\0';

     if (*hostname == '\0')
	 hostname = inet_ntoa (numaddr);
     
     dbservs[*dbnum].name = strdup (hostname);
     if (dbservs[*dbnum].name == NULL)
	 err (1, "strdup");
     dbservs[*dbnum].addr = numaddr;
     dbservs[*dbnum].timeout = 0;
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
     int i;

     f = fopen (filename, "r");
     if (f == NULL) {
	  return 1;
     }

     while (fgets (line, sizeof (line), f)) {
	  ++lineno;
	  i = 0;
	  line[strlen(line) - 1] = '\0';
	  while (line[0] && isspace((unsigned char)line[i]))
		 i++;
	  if (line[i] == '#' || line[i] == '\0')
	       continue;
	  if (line[i] == '>') {
	       if (c != NULL)
		    updatehosts (c, dbnum, dbservs);
	       c = newcell (&line[i] + 1);
	       memset (dbservs, 0, sizeof(dbservs));
	       dbnum = 0;
	  } else {
	      if (readdb(&line[i], c, &dbnum, sizeof (dbservs) /
			 sizeof(*dbservs),
			 dbservs, lineno))
		  continue;
	  }
     }
     if (c != NULL)
	  updatehosts (c, dbnum, dbservs);
     fclose (f);
     return 0;
}

/*
 * Read single line confilefiles in `filename' and send the 
 * to `func'.
 */

static int
parse_simple_file (const char *filename, void (*func)(const char *))
{
    FILE *f;
    char line[256];
    
    f = fopen (filename, "r");
    if (f == NULL)
	return 1;
    
    while ((fgets (line, sizeof(line), f) != NULL)) {
	line[strlen(line) - 1] = '\0';
	(*func) (line);
    }
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
 * Read cells in TheseCells
 */

static int
readthesecells (const char *filename)
{
     parse_simple_file (filename, addthesecell);
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
	 log_log (cell_log, CDEBERR, "Cannot read cellname from %s\n",
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

static void
addsuidcell (const char *cellname)
{
    cell_entry *e;

    e = cell_get_by_name (cellname);
    if (e == NULL) {
	log_log (cell_log, CDEBWARN,
		 "suidcell: cell %s doesn't exist in the db\n", cellname);
    } else {
	e->flags |= SUID_CELL;
    }
}

static int
readsuidcell (const char *filename)
{
    return parse_simple_file (filename, addsuidcell);
}

/*
 *
 */

static void
add_dynroot(const char *cellname)
{
    cell_entry *e; 

    e = cell_get_by_name (cellname);
    if (e == NULL) {
	log_log (cell_log, CDEBWARN,
		 "dynroot: cell %s doesn't exist in the db\n", cellname);
    } else {
	e->flags |= DYNROOT_CELL;
	dynrootdb_in_use = 1;
    }
}

static int
readdynrootdb (const char *filename)
{
    return parse_simple_file (filename, add_dynroot);
}

/*
 * Initialize the cache of cell information.
 */

static int cell_inited = 0;

void
cell_init (int cellcachesize, Log_method *log)
{
    char *env;
    int ret;

    assert (log);

    if (cell_inited) {
	log_log (cell_log, CDEBWARN, "cell_init: Already initlized");
	return;
    }
    cell_inited = 1;

#ifdef HAVE_RES_INIT
    res_init();
#endif

    cell_log = log_unit_init (log, "cell", celldebug_units, CDEBDEFAULT);
    if (cell_log == NULL)
	errx (1, "cell_init: log_unit_init failed");

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
	    log_log (cell_log, CDEBWARN,
		     "Can't read the CellServDB file, "
		     "will use DNS AFSDB entries");
	}
    }
    ret = add_special_dynroot_cell();
    if (ret)
	log_log (cell_log, CDEBWARN, "adding dynroot cell failed with %d", ret);
    
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
    if (readsuidcell (SYSCONFDIR "/" SUIDCELLSFILENAME))
	readsuidcell (TRANSARCSYSCONFDIR "/" SUIDCELLSFILENAME);
    if (readdynrootdb (SYSCONFDIR "/" DYNROOTDBFILENAME))
	readdynrootdb (TRANSARCSYSCONFDIR "/" DYNROOTDBFILENAME);

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
	dns_lookup_cell (data->name, data);
     if (data->ndbservers == 0 || data->active_hosts == 0)
	 return NULL;

    *num = data->active_hosts;
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
	 dns_lookup_cell (cell, data);
     if (data->ndbservers == 0 || data->active_hosts == 0)
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
    return ptr + 1;
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

    data = cell_get_by_name(cell);
    if (data == NULL) {
	log_log (cell_log, CDEBWARN, "this cell doesn't exist: %s", cell);
	return 1;
    }

    free (thiscell);
    thiscell = strdup (cell);
    if (thiscell == NULL)
	abort();
    
    return 0;
}

/*
 * Return if this should be in dynroot
 */

Bool
cell_dynroot (const cell_entry *c)
{
    assert (c);
    if (!dynrootdb_in_use)
	return TRUE;
    return (c->flags & DYNROOT_CELL) != 0;
}

/*
 * Return if this is a suid cell
 */

Bool
cell_issuid (const cell_entry *c)
{
    assert (c);
    return (c->flags & SUID_CELL) != 0;
}

Bool
cell_issuid_by_num (int32_t cell)
{
    cell_entry *c;

    c = cell_get_by_id (cell);
    if (c == NULL)
	return FALSE;
    
    return cell_issuid (c);
}

Bool
cell_issuid_by_name (const char *cell)
{
    cell_entry *c;

    c = cell_get_by_name (cell);
    if (c == NULL)
	return FALSE;
    
    return cell_issuid (c);
}

Bool
cell_setsuid_by_num (int32_t cell)
{
    cell_entry *c;

    c = cell_get_by_id (cell);
    if (c == NULL)
	return FALSE;
    
    c->flags |= SUID_CELL;

    return 0;
}

/*
 *
 */

void
cell_print_cell (cell_entry *c, FILE *out)
{
    int i;
    char timestr[30];
    struct tm tm;
    time_t t;

    fprintf (out, "name: %s id: %d type: %s\n",
	     c->name, c->id, c->timeout ? "dynamic" : "static");
    fprintf (out, "comment: %s\n", c->expl);
    if (c->timeout) {
	t = c->timeout;
	strftime(timestr, sizeof(timestr),
		 "%Y-%m-%d %H:%M:%S", localtime_r(&t, &tm));
	fprintf (out, "timeout: %s\n", timestr);
    }
    fprintf (out, "num hosts: %d active hosts: %d\n",
	     c->ndbservers, c->active_hosts);
    for (i = 0; i < c->ndbservers; i++) {
	char *buf;
	if (c->dbservers[i].timeout == CELL_INVALID_HOST)
	    buf = "invalid";
	else if (c->dbservers[i].timeout == 0)
	    buf = "no timeout";
	else {
	    t = c->timeout;
	    strftime(timestr, sizeof(timestr),
		     "%Y-%m-%d %H:%M:%S", localtime_r(&t, &tm));
	    buf = timestr;
	}
	fprintf (out, " host: %s %s - %s\n", c->dbservers[i].name,
		 inet_ntoa (c->dbservers[i].addr), buf);
    }
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
    c->flags	  = 0;
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
