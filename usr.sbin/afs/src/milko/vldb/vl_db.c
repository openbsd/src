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

RCSID("$KTH: vl_db.c,v 1.3 2000/10/03 00:21:07 lha Exp $");

static void open_db (char *databaseprefix, int flags);

int vl_database;
vlheader vl_header;
off_t file_length;

static char vl_header_ydr[VLHEADER_SIZE];

void
vldb_write_header(void)
{
    off_t pos;
    int length = VLHEADER_SIZE;

    if (ydr_encode_vlheader(&vl_header, vl_header_ydr, &length) == NULL)
        err(1, "write_header");

    pos = lseek(vl_database, 0, SEEK_SET);
    assert(pos == 0);

    length = write(vl_database, vl_header_ydr, VLHEADER_SIZE);
    assert (length == VLHEADER_SIZE);
}

void
vldb_read_header(void)
{
    char vl_header_ydr[VLHEADER_SIZE];
    int length = VLHEADER_SIZE;

    if (lseek(vl_database, 0, SEEK_SET) == -1)
        err(1, "lseek");

    length = read(vl_database, vl_header_ydr, VLHEADER_SIZE);
    if (length == -1)
        err(1, "read");
    if (length != VLHEADER_SIZE)
        errx(1, "read_header read failed");

    if (ydr_decode_vlheader(&vl_header, vl_header_ydr, &length) == NULL)
        err(1, "read_header");
}

void
vldb_get_file_length(void)
{
    file_length = lseek(vl_database, 0, SEEK_END);
    if (file_length == -1) {
        err(1, "lseek");
    }
}

off_t
vldb_find_first_free(void)
{
    off_t pos;

    if (vl_header.vital_header.freePtr == 0) {
	/* if there are no free entries */
	pos = lseek(vl_database, 0, SEEK_END);
	if (pos == -1)
	    err(1, "lseek");
	if (ftruncate(vl_database, pos + DISK_VLENTRY_SIZE) == -1)
	    err(1, "ftruncate");
	return pos;
    } else { /* there are free entries */
	/* Not implemented yet */
	assert(0);
    }
    return 0;
}

int
vldb_write_entry(off_t offset, disk_vlentry *vl_entry)
{
    off_t pos;
    char vl_entry_ydr[DISK_VLENTRY_SIZE];
    int length = DISK_VLENTRY_SIZE;

    if (ydr_encode_disk_vlentry(vl_entry, vl_entry_ydr, &length) == NULL)
	err(1, "write_entry");

    pos = lseek(vl_database, offset, SEEK_SET);
    assert(pos == offset);

    length = write(vl_database, vl_entry_ydr, DISK_VLENTRY_SIZE);
    assert (length == DISK_VLENTRY_SIZE);

    return 0;
}

int
vldb_read_entry(off_t offset, disk_vlentry *vl_entry)
{
    off_t pos;
    char vl_entry_ydr[DISK_VLENTRY_SIZE];
    int length = DISK_VLENTRY_SIZE;

    pos = lseek(vl_database, offset, SEEK_SET);
    assert(pos == offset);

    length = read(vl_database, vl_entry_ydr, DISK_VLENTRY_SIZE);
    assert (length == DISK_VLENTRY_SIZE);

    if (ydr_decode_disk_vlentry((disk_vlentry *) vl_entry, 
				vl_entry_ydr, &length) == NULL)
	err(1, "write_entry");

    return 0;
}

static void
create_database(void)
{
    int i;

    vl_header.vital_header.vldbversion = 0;
    vl_header.vital_header.headersize = VLHEADER_SIZE;
    vl_header.vital_header.freePtr = 0;
    vl_header.vital_header.eofPtr = VLHEADER_SIZE;
    vl_header.vital_header.allocs = 0;
    vl_header.vital_header.frees = 0;
    vl_header.vital_header.MaxVolumeId = 0xA0000000 - 1;
    for (i = 0; i < MAXTYPES; i++)
	vl_header.vital_header.totalEntries[i] = 0;

    for (i = 0; i < MAXSERVERID+1; i++)
	vl_header.IpMappedAddr[i] = 0;

    memset(vl_header.VolnameHash, 0, HASHSIZE * sizeof(int32_t));
    memset(vl_header.VolidHash, 0, HASHSIZE * MAXTYPES * sizeof(int32_t));
    vldb_write_header();
    vldb_get_file_length();
}

unsigned long
vldb_get_id_hash(long id)
{
    return ((unsigned long) id) % HASHSIZE;
}

unsigned long
vldb_get_name_hash(const char *name)
{
    int i;
    unsigned long hash = 0x47114711;

    for (i = 0; name[i] && i < 32; i++)
	hash *= name[i];

    return hash % HASHSIZE;
}

int
vldb_get_first_id_entry(unsigned long hash_id, long type,
			disk_vlentry *vl_entry)
{
    off_t offset = vl_header.VolidHash[type][hash_id];
    int status;

    vldb_debug ("  get_first_id_entry hash_id: %lu type: %ld offset: %d\n", 
		 hash_id, type, (int) offset);

    if (offset == 0)
	return VL_NOENT;

    status = vldb_read_entry(offset, vl_entry);

    return status;
}

int
vldb_get_first_name_entry(unsigned long hash_name, disk_vlentry *vl_entry)
{
    off_t offset = vl_header.VolnameHash[hash_name];
    int status;

    vldb_debug ("  get_first_name_entry hash_name: %lu offset: %d\n", 
		 hash_name, (int) offset);
    if (offset == 0)
	return VL_NOENT;

    status = vldb_read_entry(offset, vl_entry);

    return status;
}

int
vldb_insert_entry(disk_vlentry *vl_entry)
{
    off_t offset;
    int status;
    unsigned long hash_id, hash_name;
    disk_vlentry first_id_entry;
    disk_vlentry first_name_entry;
    
    /* Allokera plats i filen */
    offset = vldb_find_first_free();

    /* Allocate new volume id? */
    /*id = vl_header.vital_header.MaxVolumeId++;*/

    /* Hitta plats i hashtabellerna */
    /* XXX At present, only RW is handled */
    hash_id = vldb_get_id_hash(vl_entry->volumeId[RWVOL]);
    hash_name = vldb_get_name_hash(vl_entry->name);

    status = vldb_get_first_id_entry(hash_id, vl_entry->volumeType,
				     &first_id_entry);

/* XXX    vl_entry->nextIDHash[vldb_entry->type] = status ? 0 : first_id_entry.nextID;*/
    vl_entry->nextIdHash[vl_entry->volumeType] = status ? 0 : vl_header.VolidHash[vl_entry->volumeType][hash_id];

    status = vldb_get_first_name_entry(hash_name, &first_name_entry);
/* XXX    pr_entry->nextName = status ? 0 : first_name_entry.nextName;*/
    vl_entry->nextNameHash = status ? 0 : vl_header.VolnameHash[hash_name];

    /* XXX: uppdatera owned och nextOwned */

    /* Lägg in entryt i filen */
    status = vldb_write_entry(offset, vl_entry);
    if (status)
	return status;
    
    /* Uppdatera hashtabell */
    vl_header.VolidHash[vl_entry->volumeType][hash_id] = offset;
    vl_header.VolnameHash[hash_name] = offset;
    vldb_write_header();
    return 0;
}

int
vldb_print_entry (struct disk_vlentry *entry, int long_print)
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

#if 0
    u_long volumeId[MAXTYPES];
    long flags;
    long LockAfsId;
    long LockTimestamp;
    long cloneId;
    long AssociatedChain;
    long nextIdHash[MAXTYPES];
    long nextNameHash;
    long spares1[2];
    char name[VLDB_MAXNAMELEN];
    u_char volumeType;
    u_char RefCount;
    char spares2[1];
#endif
    return 0;
}

static void
open_db (char *databaseprefix, int flags)
{
    char database[MAXPATHLEN];

    if (databaseprefix == NULL)
	databaseprefix = MILKO_SYSCONFDIR;

    snprintf (database, sizeof(database), "%s/vl_database", 
	      databaseprefix);

    vldb_debug ("Loading db from file %s\n", database);

    vl_database = open(database, flags, S_IRWXU);

    if (vl_database == -1)
        err(1, "failed open (%s)", database);
}

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
    vldb_get_file_length();
}

static int vldbdebug = 0;

int
vldb_setdebug (int debug)
{
    int odebug = vldbdebug;
    vldbdebug = debug;
    return odebug;
}

void
vldb_debug (char *fmt, ...)
{
    va_list args;
    if (!vldbdebug)
	return ;

    va_start (args, fmt);
    vfprintf (stderr, fmt, args);
    va_end(args);
}

