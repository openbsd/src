/*
 * Copyright (c) 1998, 1999, 2000 Kungliga Tekniska Högskolan
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

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <sys/types.h>
#include <rx/rx.h>
#include <rx/rx_null.h>

#include <ports.h>
#include <ko.h>
#include <netinit.h>
#include <msecurity.h>

#ifdef KERBEROS
#ifdef HAVE_OPENSSL
#include <openssl/des.h>
#else
#include <des.h>
#endif
#include <krb.h>
#include <rxkad.h>
#endif

#include <err.h>
#include <assert.h>
#include <ctype.h>
#include <agetarg.h>

#ifndef HAVE_UNISTD_H
#include <unistd.h>
#endif

#include <service.h>

#include <mlog.h>
#include <mdebug.h>
#include <mdb.h>

#include "pts.h"
#include "pts.ss.h"
#include "ptserver.h"
#include "pts.ss.h"

RCSID("$arla: ptserver.c,v 1.46 2003/04/09 02:41:28 lha Exp $");

static struct rx_service *prservice;

prheader_disk pr_header;

#define HEADERID PR_BADID
#define ILLEGAL_ID 0x40000000
#define ILLEGAL_GROUP 0x40000000

#define PRENTRY_DISK_SIZE (sizeof(prentry_disk) + PR_MAXGROUPS * sizeof(int32_t) + 16)
#define PRHEADER_DISK_SIZE (sizeof(prheader_disk) + 4 * PR_MAXGROUPS * sizeof(int32_t) + 16)

static void open_db (char *databaseprefix, int flags);
void prserver_close(void);

MDB *nametoid, *idtodata;


/*
 *
 */

void
write_header(void)
{
    int length = PRHEADER_DISK_SIZE;
    struct mdb_datum key, value;
    char pr_header_ydr[PRHEADER_DISK_SIZE];
    int code;

    char headerid = htonl(HEADERID);

    if (ydr_encode_prheader_disk(&pr_header, pr_header_ydr, &length) == NULL)
        err(1, "write_header");

    key.data = &headerid;
    key.length = sizeof(headerid);

    value.data = pr_header_ydr;
    value.length = PRHEADER_DISK_SIZE - length;

    code = mdb_store(idtodata, &key, &value);
    assert(code == 0);
}

/*
 *
 */

void
read_header(void)
{
    int length = PRHEADER_DISK_SIZE;
    struct mdb_datum key, value;
    char headerid = htonl(HEADERID);
    int code;

    key.data = &headerid;
    key.length = sizeof(headerid);

    code = mdb_fetch(idtodata, &key, &value);
    assert(code == 0);
    
    if (ydr_decode_prheader_disk(&pr_header, value.data, &length) == NULL)
        err(1, "read_header");
}

/*
 *
 */

char *
localize_name(const char *name, Bool *localp)
{
    static prname localname;
    char *tmp;

    *localp = FALSE;

    strlcpy(localname, name, sizeof(localname));
    strlwr(localname);
    
    tmp = strchr(localname, '@');
    if (tmp)
	if (!strcasecmp(tmp + 1, netinit_getrealm())) {
	    *tmp = '\0';
	    *localp = TRUE;
	}

    return localname;
}

/*
 *
 */

static void
create_database(void)
{
    pr_header.version = 0;
    pr_header.headerSize = PRHEADER_DISK_SIZE;
    pr_header.maxGroup = -210; /* XXX */
    pr_header.maxID = 0;
/*XXX    pr_header.orphan = 0;*/
    pr_header.usercount = 0;
    pr_header.groupcount = 0;
    write_header();
}

/*
 * read_prentry(): Fetch data from db, return a classic pr_entry
 */

int
read_prentry(int id, prentry *pr_entry)
{
    prentry_disk disk_entry;
    int status, i;

    status = get_disk_entry(id, &disk_entry);
    if (status)
	return status;

    memset(pr_entry, 0, sizeof(prentry));

    pr_entry->flags = disk_entry.flags;
    pr_entry->id = disk_entry.id;
    pr_entry->cellid = disk_entry.cellid;
    pr_entry->owner = disk_entry.owner;
    pr_entry->creator = disk_entry.creator;
    pr_entry->ngroups = disk_entry.ngroups;
    strlcpy(pr_entry->name, disk_entry.name, sizeof(pr_entry->name));

    if (disk_entry.owned.len > 0)
	pr_entry->owned = disk_entry.owned.val[0];
      
    for (i = 0; i < PRSIZE && i < disk_entry.entries.len; i++)
	pr_entry->entries[i] = disk_entry.entries.val[i];

    mlog_log (MDEBPRDB, "read_prentry id: %d owner: %d creator: %d name: %s",
	      pr_entry->id, pr_entry->owner, 
	      pr_entry->creator, pr_entry->name);

    return 0;
}

/* 
 * store_disk_entry(): marshal prentry_disk and store in db
 */

int
store_disk_entry(prentry_disk *entry)
{
    char pr_entry_disk_ydr[PRENTRY_DISK_SIZE];
    int length = PRENTRY_DISK_SIZE;
    struct mdb_datum key, value;
    int id;

    mlog_log (MDEBPRDB, "store_disk_entry id: %d owner: %d creator: %d name: %s",
	      entry->id, entry->owner, 
	      entry->creator, entry->name);

    if (ydr_encode_prentry_disk((prentry_disk *) entry, pr_entry_disk_ydr, &length) == NULL)
	err(1, "store_disk_entry");

    id = htonl(entry->id);
    key.data = &id;
    key.length = sizeof(id);

    value.data = pr_entry_disk_ydr;
    value.length = PRENTRY_DISK_SIZE - length;

    return mdb_store(idtodata, &key, &value);
}

/*
 * write_prentry(): update db with classic prentry
 */

int
write_prentry(prentry *pr_entry)
{
    prentry_disk disk_entry;
    int i;

    memset(&disk_entry, 0, sizeof(prentry_disk));
    
    disk_entry.flags = pr_entry->flags;
    disk_entry.id = pr_entry->id;
    disk_entry.cellid = pr_entry->cellid;
    disk_entry.owner = pr_entry->owner;
    disk_entry.creator = pr_entry->creator;
    disk_entry.ngroups = pr_entry->ngroups;
/*    disk_entry.owned = pr_entry->owned;   XXX */
    strlcpy(disk_entry.name, pr_entry->name, sizeof(disk_entry.name));
      
    for (i = 0; i < PRSIZE && i < pr_entry->count; i++)
	disk_entry.entries.val[i] = pr_entry->entries[i];
    
    disk_entry.entries.len = i;

    return store_disk_entry(&disk_entry);
}

/*
 *
 */

static int
write_name(prentry *pr_entry)
{
    struct mdb_datum key, value;
    int32_t id = htonl(pr_entry->id);

    key.data = pr_entry->name;
    key.length = strlen(pr_entry->name);

    value.data = &id;
    value.length = sizeof(id);

    return mdb_store(nametoid, &key, &value);
}

/*
 *
 */

static int
insert_entry(prentry *pr_entry)
{
    char *pr_entry_disk_ydr;
    int status;
    int id;

    char *name = pr_entry->name;

    status = get_ydr_disk_entry(pr_entry->id, &pr_entry_disk_ydr);
    if (status == 0)
	return PREXIST;
    if (status != PRNOENT)
	return status;

    status = conv_name_to_id(name, &id);
    if (status == 0)
	return PREXIST;
    if (status != PRNOENT)
	return status;

    status = write_name(pr_entry);
    if (status)
	return status;

    return write_prentry(pr_entry);

    /* XXX: update owned and nextOwned */
    /* XXX update header */
    /* write_header(); */
}

/*
 *
 */

int
create_group(const char *name,
	     int32_t id,
	     int32_t owner,
	     int32_t creator)
{
    int status;
    prentry pr_entry;
    
    memset(&pr_entry, 0, sizeof(pr_entry));
    pr_entry.flags = PRGRP;
    pr_entry.id = id;
    pr_entry.owner = owner;
    pr_entry.creator = creator;
    strlcpy(pr_entry.name, name, PR_MAXNAMELEN);

    status = insert_entry(&pr_entry);
    if (status)
	return status;

    return 0;
}

/*
 *
 */

int
create_user(const char *name,
	    int32_t id,
	    int32_t owner,
	    int32_t creator)
{
    int status;
    prentry pr_entry;
    
    memset(&pr_entry, 0, sizeof(pr_entry));
    pr_entry.flags = 0;
    pr_entry.id = id;
    pr_entry.owner = owner;
    pr_entry.creator = creator;
    strlcpy(pr_entry.name, name, PR_MAXNAMELEN);

    status = insert_entry(&pr_entry);
    if (status)
	return status;

    return 0;
}

/*
 *
 */

int
addtogroup (int32_t uid, int32_t gid)
{
    prentry_disk uid_entry;
    prentry_disk gid_entry;
    int error, i, tmp1, tmp2;

    mlog_log (MDEBPRDB, "addtogroup");

    error = get_disk_entry(uid, &uid_entry);
    if (error)
	return error;

    error = get_disk_entry(gid, &gid_entry);
    if (error)
	return error;

    if (uid_entry.entries.len >= (PR_MAXGROUPS - 1)
	|| gid_entry.entries.len >= (PR_MAXLIST - 1))
	return PRNOENT;

    i = 0;
    while (uid_entry.entries.val[i] < gid && i < uid_entry.entries.len)
	i++;

    tmp1 = gid;
    for (; i < uid_entry.entries.len; i++) {
	tmp2 = uid_entry.entries.val[i];
	uid_entry.entries.val[i] = tmp1;
	tmp1 = tmp2;
    }
    uid_entry.entries.val[uid_entry.entries.len] = tmp1;
    uid_entry.entries.len++;

    i = 0;
    while (gid_entry.entries.val[i] < uid && i < gid_entry.entries.len)
	i++;

    tmp1 = uid;
    for (; i < gid_entry.entries.len; i++) {
	tmp2 = gid_entry.entries.val[i];
	gid_entry.entries.val[i] = tmp1;
	tmp1 = tmp2;
    }
    gid_entry.entries.val[gid_entry.entries.len] = tmp1;
    gid_entry.entries.len++;

    if ((error = store_disk_entry(&uid_entry)) != 0)
	return error;

    if ((error = store_disk_entry(&gid_entry)) != 0)
	return error;

    return 0;
}

/*
 *
 */

int
removefromgroup (int32_t uid, int32_t gid)
{
    prentry_disk uid_entry;
    prentry_disk gid_entry;
    int error, i;

    mlog_log (MDEBPRDB, "removefromgroup");

    error = get_disk_entry(uid, &uid_entry);
    if (error)
	return error;

    error = get_disk_entry(gid, &gid_entry);
    if (error)
	return error;


    i = 0;
    while (uid_entry.entries.val[i] < gid && i < uid_entry.entries.len)
	i++;

    if (uid_entry.entries.val[i] != gid)
	return PRNOENT;

    for (i++; i < uid_entry.entries.len; i++)
	uid_entry.entries.val[i - 1]  = uid_entry.entries.val[i];

    uid_entry.entries.len--;

    i = 0;
    while (gid_entry.entries.val[i] < uid && i < gid_entry.entries.len)
	i++;

    if (gid_entry.entries.val[i] != uid)
	return PRNOENT;

    for (i++; i < gid_entry.entries.len; i++)
	gid_entry.entries.val[i - 1]  = gid_entry.entries.val[i];

    gid_entry.entries.len--;

    /* XXX may leave database inconsistent ?? */

    if ((error = store_disk_entry(&uid_entry)) != 0)
	return error;
    
    if ((error = store_disk_entry(&gid_entry)) != 0)
	return error;

    return 0;

}

/*
 *
 */

int
listelements (int32_t id, prlist *elist, Bool default_id_p)
{
    prentry_disk disk_entry;
    int i = 0, error;

    error = get_disk_entry(id, &disk_entry);
    if (error)
        return error;
    
    if (default_id_p)
	elist->len = disk_entry.entries.len + 3;
    else
	elist->len = disk_entry.entries.len;

    elist->val = malloc(sizeof(*elist->val) * elist->len);
    if (elist->val == NULL)
	return ENOMEM; /* XXX */
    

    /* XXX should be sorted */

    for (i = 0; i < disk_entry.entries.len; i++)
	    elist->val[i] = disk_entry.entries.val[i];

    if (default_id_p) {
	elist->val[i] = id;
	elist->val[++i] = PR_ANYUSERID;
	elist->val[++i] = PR_AUTHUSERID;
    }

    return 0;
}

/*
 *
 */

int
get_ydr_disk_entry(int id, char **buf)
{
    struct mdb_datum key, value;
    int status;
    
    id = htonl(id);

    key.data = &id;
    key.length = sizeof(id);

    status = mdb_fetch(idtodata, &key, &value);
    if (status == ENOENT)
	return PRNOENT;

    *buf = value.data;

    return status;
}

/*
 *
 */

int
get_disk_entry(int id, prentry_disk *entry)
{
    char *pr_entry_disk_ydr;
    int length = PRENTRY_DISK_SIZE; /* XXX maxsize in mdb??? */
    int status;

    status = get_ydr_disk_entry(id, &pr_entry_disk_ydr);
    if (status)
	return status;

    if (ydr_decode_prentry_disk(entry, pr_entry_disk_ydr, &length) == NULL)
	err(1, "get_disk_entry");

    return status;
}

/*
 *
 */

int
conv_name_to_id(const char *name, int *id)
{
    struct mdb_datum key, value;
    int status;

    key.data = strdup(name); /*XXX*/
    key.length = strlen(name);

    status = mdb_fetch(nametoid, &key, &value);
    if (status == ENOENT)
	status = PRNOENT;
    else
	*id = ntohl(*((int *)value.data));
    
    free(key.data);

    return status;
}

/*
 *
 */

int
conv_id_to_name(int id, char *name)
{
    prentry pr_entry;
    int status;

    status = read_prentry(id, &pr_entry);
    if (status)
	return status;
    strlcpy(name, pr_entry.name, PR_MAXNAMELEN);
    return 0;
}

/*
 *
 */

int
next_free_group_id(int *id)
{
    pr_header.maxGroup--; /* XXX */
    if (pr_header.maxGroup == ILLEGAL_GROUP) {
	pr_header.maxGroup++;
	return -1;
    }

    write_header();

    *id = pr_header.maxGroup;
    return 0;
}

/*
 *
 */

int
next_free_user_id(int *id)
{
    pr_header.maxID++; /* XXX */
    if (pr_header.maxID == ILLEGAL_ID) {
	pr_header.maxID--;
	return -1;
    }

    write_header();

    *id = pr_header.maxID;
    return 0;
}

/*
 *
 */

static void
open_db (char *databaseprefix, int flags)
{
    char database[MAXPATHLEN];

    if (databaseprefix == NULL)
	databaseprefix = MILKO_SYSCONFDIR;

    snprintf (database, sizeof(database), "%s/pr_idtodata", 
	      databaseprefix);

    mlog_log (MDEBPR, "Loading db from file %s\n", database);

    idtodata = mdb_open(database, flags, 0600);
    if (idtodata == NULL)
        err(1, "failed open (%s)", database);


    snprintf (database, sizeof(database), "%s/pr_nametoid", 
	      databaseprefix);

    mlog_log (MDEBPR, "Loading db from file %s\n", database);

    nametoid = mdb_open(database, flags, 0600);
    if (nametoid == NULL)
        err(1, "failed open (%s)", database);
}

void
prserver_close(void)
{
    mdb_close(idtodata);
    mdb_close(nametoid);
}

#ifndef O_BINARY
#define O_BINARY 0
#endif

/*
 *
 */

static int
prserver_create (char *databaseprefix)
{
    int status;

    open_db (databaseprefix, O_RDWR|O_CREAT|O_EXCL|O_BINARY);

    printf ("Creating a new pr-database.\n");
    create_database();

#define M(N,I,O,G) \
    do { \
	status = create_group((N), (I), (O), (G)); \
	if (status)  \
            errx (1, "failed when creating %s with error %d", \
	          (N), status); \
    } while (0)

    M("system:administrators", PR_SYSADMINID, PR_SYSADMINID, PR_SYSADMINID);
    M("system:anyuser", PR_ANYUSERID, PR_SYSADMINID, PR_SYSADMINID);
    M("system:authuser", PR_AUTHUSERID, PR_SYSADMINID, PR_SYSADMINID);
    M("anonymous", PR_ANONYMOUSID, PR_SYSADMINID, PR_SYSADMINID);

#undef M

    prserver_close();

    return 0;
}

/*
 *
 */

static int
prserver_init(char *databaseprefix)
{
    open_db(databaseprefix, O_RDWR|O_BINARY);
    read_header();

    return 0;
}

/*
 *
 */

static char *cell = NULL;
static char *realm = NULL;
static char *srvtab_file = NULL;
static char *log_file = "syslog";
static char *debug_levels = NULL;
static int no_auth = 0;
static int do_help = 0;
static char *databasedir = NULL;
static int do_create = 0;

static struct agetargs args[] = {
    {"cell",	0, aarg_string,    &cell, "what cell to use"},
    {"realm",	0, aarg_string,	  &realm, "what realm to use"},
    {"debug",  'd', aarg_string,  &debug_levels, "debug level"},
    {"log",	'l',	aarg_string,	&log_file,
     "where to write log (stderr, syslog (default), or path to file)"},
    {"srvtab", 0, aarg_string,    &srvtab_file, "what srvtab to use"},
    {"noauth", 0,  aarg_flag,	  &no_auth, "disable authentication checks"},
    {"help",  'h', aarg_flag,      &do_help, "help"},
    {"dbdir",  0, aarg_string,    &databasedir, "where to store the db"},
    {"create",  0, aarg_flag,      &do_create, "create new database"},
    { NULL, 0, aarg_end, NULL }
};

/*
 *
 */

static void
usage(int exit_code)
{
    aarg_printusage (args, NULL, "", AARG_GNUSTYLE);
    exit (exit_code);
}

/*
 *
 */

int
main(int argc, char **argv) 
{
    int optind = 0;
    int ret;
    Log_method *method;

    set_progname (argv[0]);

    if (agetarg (args, argc, argv, &optind, AARG_GNUSTYLE)) {
	usage (1);
    }

    argc -= optind;
    argv += optind;

    if (argc) {
	printf("unknown option %s", *argv);
	return 1;
    }

    if (do_help)
	usage(0);

    if (no_auth)
	sec_disable_superuser_check ();

    method = log_open (getprogname(), log_file);
    if (method == NULL)
	errx (1, "log_open failed");
    cell_init(0, method);
    ports_init();

    printf ("ptserver booting");

    mlog_loginit (method, milko_deb_units, MDEFAULT_LOG);

    if (debug_levels)
	mlog_log_set_level (debug_levels);

    if (cell)
	cell_setthiscell (cell);

    network_kerberos_init (srvtab_file);
    
    if (do_create) {
	prserver_create (databasedir);
	return 0;
    }

    ret = prserver_init(databasedir);
    if (ret)
	errx (1, "prserver_init: error %d", ret);

    ret = network_init(htons(afsprport), "pr", PR_SERVICE_ID, 
		       PR_ExecuteRequest, &prservice, realm);
    if (ret)
	errx (1, "network_init returned %d", ret);

    mlog_log (MDEBWARN, "started");

    rx_SetMaxProcs(prservice,5) ;
    rx_StartServer(1) ;

    abort();
    return 0;
}
