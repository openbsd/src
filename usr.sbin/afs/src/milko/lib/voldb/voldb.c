/*
 * Copyright (c) 1999, 2000 Kungliga Tekniska Högskolan
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
 * simple flat db for the vnodes
 */

#include "voldb_locl.h"
#include "voldb_internal.h"

RCSID("$arla: voldb.c,v 1.24 2002/02/07 17:59:56 lha Exp $");

struct voldb_type *voltypes[] = {
    &vdb_flat,
};


/*
 * afs_dir_p: returns true if vno is for a dir
 */

int
afs_dir_p (int32_t vno) {
    return vno & 1;
}

/*
 * Conversion
 */

int32_t
dir_afs2local (int32_t vno)
{
    assert (vno);
    return (vno - 1) >> 1;
}
    
int32_t
dir_local2afs (int32_t vno)
{
    return (vno << 1 ) + 1;
}

int32_t
file_afs2local (int32_t vno)
{
    assert (vno);
    return (vno >> 1) - 1;
}
    
int32_t
file_local2afs (int32_t vno)
{
    return (vno + 1) << 1 ;
}

/*
 *
 */

int
voldb_parse_header (struct voldb *db, void *d, size_t sz)
{
    uint32_t i;
    unsigned char *data = d;
    
    assert (sz >= VOLDB_HEADER_HALF);
    
    memcpy (&i, data, sizeof (i));
    db->hdr.magic = ntohl (i);
    data += sizeof (uint32_t);
    
    memcpy (&i, data, sizeof (i));
    db->hdr.num = ntohl (i);
    data += sizeof (uint32_t);
    
    memcpy (&i, data, sizeof (i));
    db->hdr.flags = ntohl (i);
    data += sizeof (uint32_t);

    return 0;
}

/*
 * boot up the db
 * `fd' is a file descriptor to a db that is returned in `db'.
 * `fd' is saved if everythings works out ok, otherwise its
 * up to the caller to close it.
 */

int
voldb_init (int fd, int32_t type, int32_t volume, struct voldb **db)
{
    struct stat sb;
    int ret;

    assert (db);
    *db = NULL;

    if (type < 0 || 
	type >= sizeof (**voltypes)/ sizeof(*voltypes))
	return EINVAL;

    ret = fstat (fd, &sb);
    if (ret)
	return errno; /* XXX */
    
    if (!S_ISREG(sb.st_mode))
	return EINVAL; /* XXX */
    
    *db = calloc (1, sizeof (**db));
    if (*db == NULL)
	return ENOMEM; /* XXX */

    (*db)->size = sb.st_size;

    (*db)->fd = fd;
    (*db)->volume = volume;
    
    ret = VOLDB_FUNC((*db),init)(fd, *db, 0);
    if (ret) {
	free (*db);
	return ret;
    }

    return 0;
}    

/*
 * closes the db
 * The saved `fd' is also closed.
 */

int
voldb_close (struct voldb *db)
{
    int ret;

    assert (db);

    ret = VOLDB_FUNC(db,close)(db);

    free (db);
    return 0;
}


/*
 * store e as num'th acl entry (n is 0 based), convert it to
 * network order.
 *
 * This one is special, where is where we should add acl's on file.
 * Guess it's will be a configuration option.
 */

int
voldb_put_acl (struct voldb *db, uint32_t num, struct voldb_dir_entry *e)
{
    assert (db && e);
    assert ((db->hdr.flags & VOLDB_DIR) == VOLDB_DIR);

    return VOLDB_FUNC(db,put_acl)(db, num, e);
}

/*
 * store a entry `e' in aproproate database `db'
 */

int
voldb_put_entry (struct voldb *db, int32_t num, struct voldb_entry *e)
{
    assert (db && e);

    switch (e->type) {
    case TYPE_DIR:
	assert ((db->hdr.flags & VOLDB_DIR) == VOLDB_DIR);
	assert (e->u.dir.FileType == e->type);
	return voldb_put_dir (db, num, &e->u.dir);
    case TYPE_FILE:
    case TYPE_LINK:
	assert ((db->hdr.flags & VOLDB_FILE) == VOLDB_FILE);
	assert (e->u.file.FileType == e->type);
	return voldb_put_file (db, num, &e->u.file);
    default:
	abort();
    }
}

/*
 *
 */

int
voldb_get_size (struct voldb *db, int32_t *total_size, int32_t *num_entries)
{
    assert (db);

    *total_size	   = db->size;
    *num_entries   = db->hdr.num;
    
    return 0;
}

/*
 * Retrive a entry `e' from the database `db'
 *
 * We use the db to figure out what type e might have.  In the case of
 * a FILE/LINK we have to look at the content of `e' after the fetch. 
 */

int
voldb_get_entry (struct voldb *db, int32_t num, struct voldb_entry *e)
{
    assert (db && e);

    if ((db->hdr.flags & VOLDB_DIR) == VOLDB_DIR) {
	e->type = TYPE_DIR;
	return voldb_get_dir (db, num, &e->u.dir);
    }
    if ((db->hdr.flags & VOLDB_FILE) == VOLDB_FILE) {
	int ret;
	ret = voldb_get_file (db, num, &e->u.file);
	if (ret == 0)
	    e->type = e->u.file.FileType;
	return ret;
    } else
	abort();
}

/* Expand the table to num size */

int
voldb_expand (struct voldb *db, int32_t num)
{
    assert (db);

    return VOLDB_FUNC(db, expand)(db, num);
}

/* Rebuild the table */

int
voldb_rebuild (struct voldb *db)
{
    assert (db);

    return VOLDB_FUNC(db, rebuild)(db);
}

/*
 * Fill out the data we need to have stored and the
 * let the lower layer store it.
 */

int
voldb_write_hdr (int fd, struct voldb *db)
{
    int ret;
    uint32_t i;
    unsigned char data[VOLDB_HEADER_SIZE], *ptr;

    ptr = data;

    i = htonl (db->hdr.magic);
    memcpy (ptr, &i, sizeof (i));
    ptr += sizeof (uint32_t);

    i = htonl (db->hdr.num);
    memcpy (ptr, &i, sizeof (i));
    ptr += sizeof (uint32_t);

    i = htonl (db->hdr.flags);
    memcpy (ptr, &i, sizeof (i));
    ptr += sizeof (uint32_t);
    
    /* Here is space for yet other 2 int32_t */

    ret = VOLDB_FUNC(db,write_header)(db, data, VOLDB_HEADER_SIZE);

    return ret;
}


/*
 * create a db header in a empty existing file pointed by `fd'
 * with flags set to `flags'.
 */

int
voldb_create_header (int fd, int type, int flags)
{
    struct stat sb;
    struct voldb db;
    int ret;

    ret = fstat (fd, &sb);
    if (ret)
	return errno; /* XXX */

    if (sb.st_size != 0)
	return EINVAL; /* XXX */

    ret = ftruncate (fd, VOLDB_HEADER_SIZE);
    if (ret)
	return errno; /* XXX */

    db.type		= type;
    db.hdr.magic 	= VOLDB_MAGIC_HEADER;
    db.hdr.num 		= 0;
    db.hdr.flags 	= flags;
    db.ptr 		= NULL;
    db.size		= VOLDB_HEADER_SIZE;
    
    ret = VOLDB_FUNC(&db,init)(fd, &db, 1);
    if (ret) abort();

    ret = voldb_write_hdr (fd, &db);
    if (ret) abort();

    ret = VOLDB_FUNC(&db,close)(&db);
    if (ret) abort();
    return ret;
}

/*
 * Assure `db' is stored to disk.
 */

int
voldb_flush (struct voldb *db)
{
    int ret;
    assert (db);
    ret = voldb_write_hdr (db->fd, db);
    if (ret) return ret;
    return VOLDB_FUNC(db,flush)(db);
}


int
voldb_new_entry (struct voldb *db, uint32_t *num, uint32_t *unique)
{
    assert (db && unique);

    assert ((db->hdr.flags & (VOLDB_FILE|VOLDB_DIR)) != 
	    (VOLDB_FILE|VOLDB_DIR));
    assert ((db->hdr.flags & (VOLDB_FILE|VOLDB_DIR)) != 0);

    return VOLDB_FUNC(db,new_entry)(db, num, unique);
}

/*
 *
 */

int
voldb_del_entry (struct voldb *db, uint32_t num, onode_opaque *ino)
{
    assert (db);

    return VOLDB_FUNC(db,del_entry)(db, num, ino);
}


/*
 *
 */

int
voldb_header_info (struct voldb *db, 
		   uint32_t *num,
		   uint32_t *flags)
{
    if (num)
	*num = db->hdr.num;
    if (flags)
	*flags = db->hdr.flags;

    return 0;
}

/*
 *
 */

uint32_t
voldb_get_volume (struct voldb *db)
{
    return db->volume;
}

/*
 *
 */

int
voldb_pretty_print_file (struct voldb_file_entry *e)
{
    printf ("  nextptr		= %d\n", e->nextptr);
    printf ("  unique		= %d\n", e->unique);
    printf ("  ino		= ");
    vstatus_print_onode (stdout, &e->ino);
    printf ("  InterfaceVersion = %d\n", e->InterfaceVersion);
    printf ("  FileType		= %d\n", e->FileType);
    printf ("  LinkCount	= %d\n", e->LinkCount);
    printf ("  Length		= %d\n", e->Length);
    printf ("  DataVersion	= %d\n", e->DataVersion);
    printf ("  Author		= %d\n", e->Author);
    printf ("  Owner		= %d\n", e->Owner);
    printf ("  UnixModeBits	= 0%o\n", e->UnixModeBits);
    printf ("  ParentVnode	= %d\n", e->ParentVnode);
    printf ("  ParentUnique	= %d\n", e->ParentUnique);
    printf ("  SegSize		= %d\n", e->SegSize);
    printf ("  ServerModTime	= %d\n", e->ServerModTime);
    printf ("  Group		= %d\n", e->Group);

    return 0;
}

int
voldb_pretty_print_dir (struct voldb_dir_entry *e)
{
    int i;

    printf ("  nextptr		= %d\n", e->nextptr);
    printf ("  unique		= %d\n", e->unique);
    printf ("  ino		= ");
    vstatus_print_onode (stdout, &e->ino);
    printf ("  InterfaceVersion = %d\n", e->InterfaceVersion);
    printf ("  FileType		= %d\n", e->FileType);
    printf ("  LinkCount	= %d\n", e->LinkCount);
    printf ("  Length		= %d\n", e->Length);
    printf ("  DataVersion	= %d\n", e->DataVersion);
    printf ("  Author		= %d\n", e->Author);
    printf ("  Owner		= %d\n", e->Owner);
    printf ("  UnixModeBits	= 0%o\n", e->UnixModeBits);
    printf ("  ParentVnode	= %d\n", e->ParentVnode);
    printf ("  ParentUnique	= %d\n", e->ParentUnique);
    printf ("  SegSize		= %d\n", e->SegSize);
    printf ("  ServerModTime	= %d\n", e->ServerModTime);
    printf ("  Group		= %d\n", e->Group);

    printf ("  ACL:");
    printf ("  negative:\n");
    for (i = 0; i < FS_MAX_ACL; i++)
	printf ("    owner %d flags 0x%x\n", 
		e->negacl[i].owner, e->negacl[i].flags);
    printf ("  positive:\n");
    for (i = 0; i < FS_MAX_ACL; i++)
	printf ("    owner %d flags 0x%x\n", 
		e->acl[i].owner, e->acl[i].flags);

    return 0;
}

int
voldb_pretty_print_header (struct voldb *db)
{
    printf ("type:\t%d", db->type);
    printf ("size:\t%d", db->size);
    printf ("magic:\t0x%x (should be 0x%x)\n",
	    db->hdr.magic, VOLDB_MAGIC_HEADER);
    printf ("num:\t%d\n", db->hdr.num);
    printf ("flags:\t0x%x", db->hdr.flags);
    return 0;
}

/*
 * voldb_update_time
 *
 * Should be called upon creation and when data is modified.
 * Should be called before any call to vld_storestatus_to_ent().
 * Should _not_ be called when metadata is changed.
 */

void
voldb_update_time(struct voldb_entry *e, time_t t)
{
    switch (e->type) {
    case TYPE_DIR:
	e->u.dir.ServerModTime = t; /* XXX update ClientModTime also */
	break;
    case TYPE_FILE:
    case TYPE_LINK:
	e->u.file.ServerModTime = t; /* XXX update ClientModTime also */
	break;
    default:
	abort();
	break;
    }
}
