/*
 * Copyright (c) 1995 - 2001 Kungliga Tekniska Högskolan
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

#include "vldb_locl.h"

RCSID("$arla: vl_db.c,v 1.10 2002/06/02 21:12:20 lha Exp $");

#define DISK_VLENTRY_MAX 10000

static void open_db (char *databaseprefix, int flags);

int vl_database;
vital_vlheader vl_header;

MDB *idtoname, *nametodata;

void
vldb_write_header(void)
{
    int length = VITAL_VLHEADER_SIZE;
    struct mdb_datum key, value;
    char vl_header_ydr[VITAL_VLHEADER_SIZE];
    int code;
    char headerid[] = "\0\0\0\0";

    if (ydr_encode_vital_vlheader(&vl_header, vl_header_ydr, &length) == NULL)
        err(1, "write_header");

    assert (length == 0);

    key.data = headerid;
    key.length = sizeof(headerid);

    value.data = vl_header_ydr;
    value.length = VITAL_VLHEADER_SIZE;

    code = mdb_store(nametodata, &key, &value);
    assert(code == 0);

}

void
vldb_read_header(void)
{
    int length = VITAL_VLHEADER_SIZE;
    struct mdb_datum key, value;
    char headerid[] = "\0\0\0\0";
    int ret;

    key.data = headerid;
    key.length = sizeof(headerid);

    value.data = malloc(length);
    value.length = length;

    ret = mdb_fetch(nametodata, &key, &value);

    if (ret) {
	errx(1, "read_header: mdb_fetch failed");
    }

    assert(value.length == VITAL_VLHEADER_SIZE);
    
    if (ydr_decode_vital_vlheader(&vl_header, value.data, &length) == NULL)
        err(1, "read_header");
}

static void
create_database(void)
{
    int i;

    vl_header.vldbversion = 0;
    vl_header.headersize = VITAL_VLHEADER_SIZE;
    vl_header.freePtr = 0;
    vl_header.eofPtr = VITAL_VLHEADER_SIZE;
    vl_header.allocs = 0;
    vl_header.frees = 0;
    vl_header.MaxVolumeId = 0x20000000;
    for (i = 0; i < MAXTYPES; i++)
	vl_header.totalEntries[i] = 0;

    vldb_write_header();
}

int
vldb_write_entry(const disk_vlentry *vldb_entry)
{
    struct mdb_datum datum;
    struct mdb_datum key;
    char *disk_vlentry_ydr;
    int length = DISK_VLENTRY_MAX;
    int ret;

    disk_vlentry_ydr = malloc(length);

    if (ydr_encode_disk_vlentry(vldb_entry, disk_vlentry_ydr, &length) == NULL)
        err(1, "write_entry");

    datum.data = disk_vlentry_ydr;
    datum.length = DISK_VLENTRY_MAX - length;

    length = datum.length;

    key.data = (char *) vldb_entry->name;
    key.length = strlen(vldb_entry->name);

    ret = mdb_store(nametodata, &key, &datum);
    free(disk_vlentry_ydr);
    return ret;
}

int
vldb_read_entry(const char *name, disk_vlentry *entry)
{
    struct mdb_datum key, value;
    int length;
    int ret;

    key.data = (char *) name;
    key.length = strlen(name);

    ret = mdb_fetch(nametodata, &key, &value);

    if (ret)
	return ret;

    length = value.length;

    if (ydr_decode_disk_vlentry(entry, value.data, &length) == NULL)
        err(1, "read_entry");

    return 0;
}

int
vldb_delete_entry(const char *name)
{
    struct mdb_datum key;
    int ret;

    key.data = (char *) name;
    key.length = strlen(name);

    ret = mdb_delete(nametodata, &key);

    if (ret)
	return ret;

    return 0;
}

int
vldb_id_to_name(const int32_t volid, char **name)
{
    struct mdb_datum key, value;
    int ret;

    key.data = (char *)&volid;
    key.length = sizeof(volid);

    ret = mdb_fetch(idtoname, &key, &value);

    if (ret)
	return ret;

    *name = malloc(value.length+1);

    if (*name == 0)
	return ENOMEM;

    memcpy(*name, value.data, value.length);
    (*name)[value.length] = '\0';

    return 0;
}

int
vldb_write_id (const char *name, const uint32_t volid)
{
    struct mdb_datum datum, key;
    char *name_copy;
    uint32_t volid_copy = volid;

    if (volid == 0)
	return 0;

    name_copy = strdup(name);

    datum.data = name_copy;
    datum.length = strlen(name_copy);

    key.data=&volid_copy;
    key.length = sizeof(volid);

    mdb_store(idtoname, &key, &datum);
    
    free(name_copy);
    return 0;
}

int
vldb_delete_id (const char *name, const uint32_t volid)
{
    struct mdb_datum named, vold;
    char *name_copy;
    uint32_t volid_copy = volid;
    int ret;

    if (volid == 0)
	return 0;

    name_copy = strdup(name);

    named.data = name_copy;
    named.length = strlen(name_copy);

    vold.data=&volid_copy;
    vold.length = sizeof(volid);

    ret = mdb_delete(idtoname, &vold);
    if (ret)
	return ret;
    
    free(name_copy);
    return 0;
}

int
vldb_print_entry (vldbentry *entry, int long_print)
{
    int i;
    printf ("name: %s\n"
	    "\tid: rw: %u ro: %u bu: %u\n",
	    entry->name,
	    entry->volumeId[RWVOL], 
	    entry->volumeId[ROVOL],
	    entry->volumeId[BACKVOL]);
    if (!long_print)
	return 0;
    printf ("\tservers:\n");
    for (i = 0; i < MAXNSERVERS; i++) {
	struct in_addr in;
	in.s_addr = ntohl(entry->serverNumber[i]);
	if (entry->serverFlags[i])
	    printf ("\t\t%2d: %-16s %d %s %s %s %s\n",
		    i,
		    inet_ntoa (in),
		    entry->serverPartition[i],
		    entry->serverFlags[i] & VLSF_RWVOL   ? "RW" : "  ",
		    entry->serverFlags[i] & VLSF_ROVOL   ? "RO" : "  ",
		    entry->serverFlags[i] & VLSF_BACKVOL ? "BU" : "  ",
		    entry->serverFlags[i] & VLSF_NEWREPSITE ? "New site" : "");
    }

    return 0;
}

static void
open_db (char *databaseprefix, int flags)
{

    char database[MAXPATHLEN];

    if (databaseprefix == NULL)
	databaseprefix = MILKO_SYSCONFDIR;

    snprintf (database, sizeof(database), "%s/vl_idtoname", 
	      databaseprefix);

    mlog_log (MDEBVL, "Loading db from file %s\n", database);

    idtoname = mdb_open(database, flags, 0600);

    if (idtoname == NULL)
        err(1, "failed open (%s)", database);

    snprintf (database, sizeof(database), "%s/vl_nametodata", 
	      databaseprefix);

    mlog_log (MDEBVL, "Loading db from file %s\n", database);

    nametodata = mdb_open(database, flags, 0600);

    if (nametodata == NULL)
        err(1, "failed open (%s)", database);
}

#ifndef O_BINARY
#define O_BINARY 0
#endif

void
vldb_create (char *databaseprefix)
{
    open_db (databaseprefix, O_RDWR|O_CREAT|O_EXCL|O_BINARY);

    printf ("Creating a new vl-database.\n");
    create_database();
}

void
vldb_init(char *databaseprefix)
{
    open_db (databaseprefix, O_RDWR|O_BINARY);
    vldb_read_header();
}

void
vldb_close(void)
{
    mdb_close(nametodata);
    mdb_close(idtoname);
}

void
vldb_flush(void)
{
    mdb_flush(nametodata);
    mdb_flush(idtoname);
}
