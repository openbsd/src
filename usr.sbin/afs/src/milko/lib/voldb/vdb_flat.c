/*
 * Copyright (c) 1999 - 2000 Kungliga Tekniska Högskolan
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

RCSID("$arla: vdb_flat.c,v 1.19 2002/02/07 17:59:54 lha Exp $");


static int vdbflat_init (int fd, struct voldb *db, int createp);
static int vdbflat_close (struct voldb *db);
static int vdbflat_get_dir (struct voldb *db, const uint32_t num,
			    struct voldb_dir_entry *e);
static int vdbflat_put_dir (struct voldb *db, const uint32_t num,
			    struct voldb_dir_entry *e);
static int vdbflat_put_acl (struct voldb *db, uint32_t num,
			    struct voldb_dir_entry *e);
static int vdbflat_get_file (struct voldb *db, const uint32_t num,
			     struct voldb_file_entry *e);
static int vdbflat_put_file (struct voldb *db, const uint32_t num,
			     struct voldb_file_entry *e);
static int vdbflat_extend_db (struct voldb *db, unsigned int num);
static int vdbflat_flush (struct voldb *db);
static int vdbflat_new_entry (struct voldb *db, uint32_t *num, 
			      uint32_t *unique);
static int vdbflat_del_entry (struct voldb *db, const uint32_t num,
			      onode_opaque *ino);
static int vdbflat_write_header (struct voldb *db, void *d, size_t sz);
    
/*
 * Various internal data definitions
 */

#define VOLDB_FILE_SIZE (19*4+404)
#define VOLDB_DIR_SIZE (VOLDB_FILE_SIZE+(FS_MAX_ACL*2*2*4))
/* (negative + positive) * 2 entries * int32_t */

/*
 * Various internal data structures
 */

typedef struct vdbflat {
    int32_t freeptr;
    void *ptr;
    union {
	struct voldb_file_entry *file;
	struct voldb_dir_entry *dir;
    } u;
} vdbflat;

static void
set_volhdr_union_ptr (struct voldb *db)
{
    vdbflat *vdb = (vdbflat *) db->ptr;

    if (db->hdr.flags & VOLDB_DIR)
	vdb->u.dir = (struct voldb_dir_entry *)
	    ((unsigned char *) ((vdbflat *)db->ptr)->ptr + VOLDB_HEADER_SIZE);
    else if (db->hdr.flags & VOLDB_FILE)
	vdb->u.file = (struct voldb_file_entry *)
	    ((unsigned char *) ((vdbflat *)db->ptr)->ptr + VOLDB_HEADER_SIZE);
    else
	abort();
}

/*
 * boot up the db
 * `fd' is a file descriptor to a db that is returned in `db'.
 * `fd' is saved if everythings works out ok, otherwise its
 * up to the caller to close it.
 */

static int
vdbflat_init (int fd, struct voldb *db, int createp)
{
    vdbflat *vdb;
    uint32_t i;
    int size;
    int ret;
    unsigned char *data;

    assert (db);
    assert (db->size >= VOLDB_HEADER_SIZE);

    vdb = calloc (1, sizeof(*vdb));
    if (vdb == NULL)
	return ENOMEM; /* XXX */

    vdb->ptr = mmap (NULL, db->size, PROT_READ|PROT_WRITE,
		     MAP_SHARED, fd, 0);
    if (vdb->ptr == (void *)MAP_FAILED) {
	free (vdb);
	return (errno);
    }

    /* If we are being created, there is no sane data on disk */
    if (createp) {
	vdb->freeptr = VOLDB_FREELIST_END;
    } else {
	/* Let the above layer parse the header */
	voldb_parse_header (db, vdb->ptr, VOLDB_HEADER_HALF);
	
	/* Now parse the second half */
	data = ((unsigned char *)vdb->ptr) + VOLDB_HEADER_HALF;
	memcpy (&i, data, sizeof(i));
	vdb->freeptr = ntohl (i);
    }

    /* Do some sanity checking */
    if ((db->hdr.flags & (VOLDB_FILE|VOLDB_DIR)) == VOLDB_FILE)
	size = VOLDB_FILE_SIZE;
    else if ((db->hdr.flags & (VOLDB_FILE|VOLDB_DIR)) == VOLDB_DIR)
	size = VOLDB_DIR_SIZE;
    else {
	ret = munmap (vdb->ptr, db->size);
	assert (ret == 0);
	free (vdb);
	return EIO;
    }
    
    if ((db->size - VOLDB_HEADER_SIZE) % size != 0) {
	ret = munmap (vdb->ptr, db->size);
	assert (ret == 0);
	free(vdb);
	return EIO;
    }

    if (db->hdr.num != (db->size - VOLDB_HEADER_SIZE) / size) {
	ret = munmap (vdb->ptr, db->size);
	assert (ret == 0);
	free(vdb);
	return EINVAL;
    }

    /* All done setup pointers */

    db->ptr = vdb;

    set_volhdr_union_ptr (db);

    return 0;
}    

/*
 * closes the db
 * The saved `fd' is also closed.
 */

static int
vdbflat_close (struct voldb *db)
{
    vdbflat *vdb;
    int ret;

    assert (db);

    assert (db->hdr.magic = VOLDB_MAGIC_HEADER);

    vdb = (vdbflat *) db->ptr;
    db->ptr = NULL;

    ret = msync (vdb->ptr, db->size, MS_SYNC);
    assert (ret == 0);
    ret = munmap (vdb->ptr, db->size);
    
    free (vdb);
    if (ret)
	return errno;

    close (db->fd);

    return 0;
}


/*
 * fetch num'th dir entry (n is 0 based), convert it
 * to hostorder and store it in `e'.
 */

static int
vdbflat_get_dir (struct voldb *db, 
		 const uint32_t num,
		 struct voldb_dir_entry *e)
{
    vdbflat *vdb = (vdbflat *) db->ptr;
    size_t len;

    assert (db && e);

    if ((db->hdr.flags & VOLDB_DIR) == 0)
	return EINVAL; /* XXX */

    if (db->hdr.num < num)
	return EINVAL; /* XXX */

#if defined(BYTE_ORDER) &&  BYTE_ORDER	== BIG_ENDIAN && 0
    memcpy (e, &vdb->u.dir[num], sizeof (*e));
#else
    {
	uint32_t tmp;
	int i;
	unsigned char *ptr = 
	    ((unsigned char *)vdb->u.dir) +
	    VOLDB_DIR_SIZE * num;

	memcpy (&tmp, ptr, sizeof(tmp));
	e->nextptr = ntohl(tmp); ptr += sizeof(tmp);

	memcpy (&tmp, ptr, sizeof(tmp));
	e->unique = ntohl(tmp); ptr += sizeof(tmp);

	len = ONODE_OPAQUE_SIZE;
	ydr_decode_onode_opaque (&e->ino, ptr, &len);
	ptr += ONODE_OPAQUE_SIZE - len;

	memcpy (&tmp, ptr, sizeof(tmp));
	e->spare2 = ntohl(tmp); ptr += sizeof(tmp);

	memcpy (&tmp, ptr, sizeof(tmp));
	e->InterfaceVersion = ntohl(tmp); ptr += sizeof(tmp);

	memcpy (&tmp, ptr, sizeof(tmp));
	e->FileType = ntohl(tmp); ptr += sizeof(tmp);

	memcpy (&tmp, ptr, sizeof(tmp));
	e->LinkCount = ntohl(tmp); ptr += sizeof(tmp);

	memcpy (&tmp, ptr, sizeof(tmp));
	e->Length = ntohl(tmp); ptr += sizeof(tmp);

	memcpy (&tmp, ptr, sizeof(tmp));
	e->spare3 = ntohl(tmp); ptr += sizeof(tmp);

	memcpy (&tmp, ptr, sizeof(tmp));
	e->DataVersion = ntohl(tmp); ptr += sizeof(tmp);

	memcpy (&tmp, ptr, sizeof(tmp));
	e->Author = ntohl(tmp); ptr += sizeof(tmp);

	memcpy (&tmp, ptr, sizeof(tmp));
	e->Owner = ntohl(tmp); ptr += sizeof(tmp);

	memcpy (&tmp, ptr, sizeof(tmp));
	e->UnixModeBits = ntohl(tmp); ptr += sizeof(tmp);

	memcpy (&tmp, ptr, sizeof(tmp));
	e->ParentVnode = ntohl(tmp); ptr += sizeof(tmp);

	memcpy (&tmp, ptr, sizeof(tmp));
	e->ParentUnique = ntohl(tmp); ptr += sizeof(tmp);

	memcpy (&tmp, ptr, sizeof(tmp));
	e->SegSize = ntohl(tmp); ptr += sizeof(tmp);

	memcpy (&tmp, ptr, sizeof(tmp));
	e->ServerModTime = ntohl(tmp); ptr += sizeof(tmp);

	memcpy (&tmp, ptr, sizeof(tmp));
	e->spare4 = ntohl(tmp); ptr += sizeof(tmp);

	memcpy (&tmp, ptr, sizeof(tmp));
	e->Group = ntohl(tmp); ptr += sizeof(tmp);

	memcpy (&tmp, ptr, sizeof(tmp));
	e->spare5 = ntohl(tmp); ptr += sizeof(tmp);

	for (i = 0 ; i < FS_MAX_ACL; i++) {
	    memcpy (&tmp, ptr, sizeof(tmp));
	    e->negacl[i].owner = ntohl(tmp); ptr += sizeof(tmp);
	    memcpy (&tmp, ptr, sizeof(tmp));
	    e->negacl[i].flags = ntohl(tmp); ptr += sizeof(tmp);
	}
	for (i = 0 ; i < FS_MAX_ACL; i++) {
	    memcpy (&tmp, ptr, sizeof(tmp));
	    e->acl[i].owner = ntohl(tmp); ptr += sizeof(tmp);
	    memcpy (&tmp, ptr, sizeof(tmp));
	    e->acl[i].flags = ntohl(tmp); ptr += sizeof(tmp);
	}
    }
#endif
    return 0;
}

/*
 * store e as num'th dir entry (n is 0 based), convert it to
 * network order. DO NOT STORE the ACL.
 */

int
vdbflat_put_dir (struct voldb *db,
		 const uint32_t num,
		 struct voldb_dir_entry *e)
{
    vdbflat *vdb = (vdbflat *) db->ptr;

    assert (db && e);

    if ((db->hdr.flags & VOLDB_DIR) == 0)
	return EINVAL; /* XXX */

    if (db->hdr.num < num)
	return EINVAL; /* XXX */

#if defined(BYTE_ORDER) &&  BYTE_ORDER	== BIG_ENDIAN && 0
    memcpy (&vdb->u.dir[num], e, sizeof (*e));
#else
    {
	uint32_t tmp;
	size_t len;
	unsigned char *ptr =
	    ((unsigned char *)vdb->u.dir) +
	    VOLDB_DIR_SIZE * num;

	tmp = htonl(e->nextptr);
	memcpy (ptr, &tmp, sizeof(tmp)); ptr += sizeof(tmp);

	tmp = htonl(e->unique);
	memcpy (ptr, &tmp, sizeof(tmp)); ptr += sizeof(tmp);

	len = ONODE_OPAQUE_SIZE;
	ydr_encode_onode_opaque (&e->ino, ptr, &len);
	ptr += ONODE_OPAQUE_SIZE - len;

	tmp = htonl(e->spare2);
	memcpy (ptr, &tmp, sizeof(tmp)); ptr += sizeof(tmp);

	tmp = htonl(e->InterfaceVersion);
	memcpy (ptr, &tmp, sizeof(tmp)); ptr += sizeof(tmp);

	tmp = htonl(e->FileType);
	memcpy (ptr, &tmp, sizeof(tmp)); ptr += sizeof(tmp);

	tmp = htonl(e->LinkCount);
	memcpy (ptr, &tmp, sizeof(tmp)); ptr += sizeof(tmp);

	tmp = htonl(e->Length);
	memcpy (ptr, &tmp, sizeof(tmp)); ptr += sizeof(tmp);

	tmp = htonl(e->spare3);
	memcpy (ptr, &tmp, sizeof(tmp)); ptr += sizeof(tmp);

	tmp = htonl(e->DataVersion);
	memcpy (ptr, &tmp, sizeof(tmp)); ptr += sizeof(tmp);

	tmp = htonl(e->Author);
	memcpy (ptr, &tmp, sizeof(tmp)); ptr += sizeof(tmp);

	tmp = htonl(e->Owner);
	memcpy (ptr, &tmp, sizeof(tmp)); ptr += sizeof(tmp);

	tmp = htonl(e->UnixModeBits);
	memcpy (ptr, &tmp, sizeof(tmp)); ptr += sizeof(tmp);

	tmp = htonl(e->ParentVnode);
	memcpy (ptr, &tmp, sizeof(tmp)); ptr += sizeof(tmp);

	tmp = htonl(e->ParentUnique);
	memcpy (ptr, &tmp, sizeof(tmp)); ptr += sizeof(tmp);

	tmp = htonl(e->SegSize);
	memcpy (ptr, &tmp, sizeof(tmp)); ptr += sizeof(tmp);

	tmp = htonl(e->ServerModTime);
	memcpy (ptr, &tmp, sizeof(tmp)); ptr += sizeof(tmp);

	tmp = htonl(e->spare4);
	memcpy (ptr, &tmp, sizeof(tmp)); ptr += sizeof(tmp);

	tmp = htonl(e->Group);
	memcpy (ptr, &tmp, sizeof(tmp)); ptr += sizeof(tmp);

	tmp = htonl(e->spare5);
	memcpy (ptr, &tmp, sizeof(tmp)); ptr += sizeof(tmp);
    }
#endif
    return 0;
}

/*
 * store e as num'th acl entry (n is 0 based), convert it to
 * network order.
 */

static int
vdbflat_put_acl (struct voldb *db, uint32_t num, struct voldb_dir_entry *e)
{
    vdbflat *vdb = (vdbflat *) db->ptr;

    assert (db && e);

    if ((db->hdr.flags & VOLDB_DIR) == 0)
	return EINVAL; /* XXX */

    if (db->hdr.num < num)
	return EINVAL; /* XXX */

#if defined(BYTE_ORDER) &&  BYTE_ORDER	== BIG_ENDIAN && 0
    memcpy ((char *)&vdb->u.dir[num] + VOLDB_FILE_SIZE, &e->acl, 
	    sizeof (*e->negacl) + sizeof (*e->acl));
#else
    {
	uint32_t tmp;
	unsigned char *ptr =
	    ((unsigned char *)vdb->u.dir) +
	    VOLDB_DIR_SIZE * num;
	int i;

	ptr += VOLDB_FILE_SIZE;

	for (i = 0 ; i < FS_MAX_ACL; i++) {
	    tmp = htonl(e->negacl[i].owner);
	    memcpy (ptr, &tmp, sizeof(tmp)); ptr += sizeof(tmp);
	    tmp = htonl(e->negacl[i].flags);
	    memcpy (ptr, &tmp, sizeof(tmp)); ptr += sizeof(tmp);
	}
	for (i = 0 ; i < FS_MAX_ACL; i++) {
	    tmp = htonl(e->acl[i].owner);
	    memcpy (ptr, &tmp, sizeof(tmp)); ptr += sizeof(tmp);
	    tmp = htonl(e->acl[i].flags);
	    memcpy (ptr, &tmp, sizeof(tmp)); ptr += sizeof(tmp);
	}
    }
#endif
    return 0;
}


/*
 * fetch num'th file entry (n is 0 based), convert it
 * to hostorder and store it in `e'.
 */

static int
vdbflat_get_file (struct voldb *db, uint32_t num, struct voldb_file_entry *e)
{
    vdbflat *vdb = (vdbflat *) db->ptr;

    assert (db && e);

    if ((db->hdr.flags & VOLDB_FILE) == 0)
	return EINVAL; /* XXX */

    if (db->hdr.num < num)
	return EINVAL; /* XXX */

#if defined(BYTE_ORDER) &&  BYTE_ORDER	== BIG_ENDIAN && 0
    memcpy (e, &vdb->u.file[num], sizeof (*e));
#else
    {
	uint32_t tmp;
	size_t len;
	unsigned char *ptr =
	    ((unsigned char *)vdb->u.file) +
	    VOLDB_FILE_SIZE * num;

	memcpy (&tmp, ptr, sizeof(tmp));
	e->nextptr = ntohl(tmp); ptr += sizeof(tmp);

	memcpy (&tmp, ptr, sizeof(tmp));
	e->unique = ntohl(tmp); ptr += sizeof(tmp);

	len = ONODE_OPAQUE_SIZE;
	ydr_decode_onode_opaque (&e->ino, ptr, &len);
	ptr += ONODE_OPAQUE_SIZE - len;

	memcpy (&tmp, ptr, sizeof(tmp));
	e->spare2 = ntohl(tmp); ptr += sizeof(tmp);

	memcpy (&tmp, ptr, sizeof(tmp));
	e->InterfaceVersion = ntohl(tmp); ptr += sizeof(tmp);

	memcpy (&tmp, ptr, sizeof(tmp));
	e->FileType = ntohl(tmp); ptr += sizeof(tmp);

	memcpy (&tmp, ptr, sizeof(tmp));
	e->LinkCount = ntohl(tmp); ptr += sizeof(tmp);

	memcpy (&tmp, ptr, sizeof(tmp));
	e->Length = ntohl(tmp); ptr += sizeof(tmp);

	memcpy (&tmp, ptr, sizeof(tmp));
	e->spare3 = ntohl(tmp); ptr += sizeof(tmp);

	memcpy (&tmp, ptr, sizeof(tmp));
	e->DataVersion = ntohl(tmp); ptr += sizeof(tmp);

	memcpy (&tmp, ptr, sizeof(tmp));
	e->Author = ntohl(tmp); ptr += sizeof(tmp);

	memcpy (&tmp, ptr, sizeof(tmp));
	e->Owner = ntohl(tmp); ptr += sizeof(tmp);

	memcpy (&tmp, ptr, sizeof(tmp));
	e->UnixModeBits = ntohl(tmp); ptr += sizeof(tmp);

	memcpy (&tmp, ptr, sizeof(tmp));
	e->ParentVnode = ntohl(tmp); ptr += sizeof(tmp);

	memcpy (&tmp, ptr, sizeof(tmp));
	e->ParentUnique = ntohl(tmp); ptr += sizeof(tmp);

	memcpy (&tmp, ptr, sizeof(tmp));
	e->SegSize = ntohl(tmp); ptr += sizeof(tmp);

	memcpy (&tmp, ptr, sizeof(tmp));
	e->ServerModTime = ntohl(tmp); ptr += sizeof(tmp);

	memcpy (&tmp, ptr, sizeof(tmp));
	e->spare4 = ntohl(tmp); ptr += sizeof(tmp);

	memcpy (&tmp, ptr, sizeof(tmp));
	e->Group = ntohl(tmp); ptr += sizeof(tmp);

	memcpy (&tmp, ptr, sizeof(tmp));
	e->spare5 = ntohl(tmp); ptr += sizeof(tmp);
    }
#endif
    return 0;
}

/*
 * store e as num'th file entry (n is 0 based), convert it to
 * network order.
 */

int
vdbflat_put_file (struct voldb *db, uint32_t num, struct voldb_file_entry *e)
{
    vdbflat *vdb = (vdbflat *) db->ptr;

    assert (db && e);

    if ((db->hdr.flags & VOLDB_FILE) == 0)
	return EINVAL; /* XXX */

    if (db->hdr.num < num)
	return EINVAL; /* XXX */

#if defined(BYTE_ORDER) &&  BYTE_ORDER	== BIG_ENDIAN && 0
    memcpy (&vdb->u.file[num], e, sizeof (*e));
#else
    {
	uint32_t tmp;
	size_t len;
	unsigned char *ptr =
	    ((unsigned char *)vdb->u.file) +
	    VOLDB_FILE_SIZE * num;

	tmp = htonl(e->nextptr);
	memcpy (ptr, &tmp, sizeof(tmp)); ptr += sizeof(tmp);

	tmp = htonl(e->unique);
	memcpy (ptr, &tmp, sizeof(tmp)); ptr += sizeof(tmp);

	len = ONODE_OPAQUE_SIZE;
	ydr_encode_onode_opaque (&e->ino, ptr, &len);
	ptr += ONODE_OPAQUE_SIZE - len;

	tmp = htonl(e->spare2);
	memcpy (ptr, &tmp, sizeof(tmp)); ptr += sizeof(tmp);

	tmp = htonl(e->InterfaceVersion);
	memcpy (ptr, &tmp, sizeof(tmp)); ptr += sizeof(tmp);

	tmp = htonl(e->FileType);
	memcpy (ptr, &tmp, sizeof(tmp)); ptr += sizeof(tmp);

	tmp = htonl(e->LinkCount);
	memcpy (ptr, &tmp, sizeof(tmp)); ptr += sizeof(tmp);

	tmp = htonl(e->Length);
	memcpy (ptr, &tmp, sizeof(tmp)); ptr += sizeof(tmp);

	tmp = htonl(e->spare3);
	memcpy (ptr, &tmp, sizeof(tmp)); ptr += sizeof(tmp);

	tmp = htonl(e->DataVersion);
	memcpy (ptr, &tmp, sizeof(tmp)); ptr += sizeof(tmp);

	tmp = htonl(e->Author);
	memcpy (ptr, &tmp, sizeof(tmp)); ptr += sizeof(tmp);

	tmp = htonl(e->Owner);
	memcpy (ptr, &tmp, sizeof(tmp)); ptr += sizeof(tmp);

	tmp = htonl(e->UnixModeBits);
	memcpy (ptr, &tmp, sizeof(tmp)); ptr += sizeof(tmp);

	tmp = htonl(e->ParentVnode);
	memcpy (ptr, &tmp, sizeof(tmp)); ptr += sizeof(tmp);

	tmp = htonl(e->ParentUnique);
	memcpy (ptr, &tmp, sizeof(tmp)); ptr += sizeof(tmp);

	tmp = htonl(e->SegSize);
	memcpy (ptr, &tmp, sizeof(tmp)); ptr += sizeof(tmp);

	tmp = htonl(e->ServerModTime);
	memcpy (ptr, &tmp, sizeof(tmp)); ptr += sizeof(tmp);

	tmp = htonl(e->spare4);
	memcpy (ptr, &tmp, sizeof(tmp)); ptr += sizeof(tmp);

	tmp = htonl(e->Group);
	memcpy (ptr, &tmp, sizeof(tmp)); ptr += sizeof(tmp);

	tmp = htonl(e->spare5);
	memcpy (ptr, &tmp, sizeof(tmp)); ptr += sizeof(tmp);
    }
#endif
    return 0;
}

/*
 * Extend the db and add more entries.
 */

static int
vdbflat_extend_db (struct voldb *db, unsigned int num)
{
    int dirp = 0, ret;
    size_t newsize;
    uint32_t oldnum = db->hdr.num;
    vdbflat *vdb = db->ptr;

    if (db->hdr.magic != VOLDB_MAGIC_HEADER)
	return EINVAL; /* XXX */

    if ((db->hdr.flags & (VOLDB_FILE|VOLDB_DIR)) == VOLDB_DIR) {
	dirp = 1;
	newsize = db->size + num * VOLDB_DIR_SIZE;
    } else if ((db->hdr.flags & (VOLDB_FILE|VOLDB_DIR)) == VOLDB_FILE)
	newsize = db->size + num * VOLDB_FILE_SIZE;
    else
	return EINVAL; /* XXX */

    ret = munmap (vdb->ptr, db->size);
    if (ret)
	return errno;

    ret = ftruncate (db->fd, newsize);
    if (ret)
	return errno;
    
    db->size = newsize;
    db->hdr.num += num;

    vdb->ptr = mmap (NULL, db->size, PROT_READ|PROT_WRITE,
		       MAP_SHARED, db->fd, 0);
    if (vdb->ptr == (void *)MAP_FAILED) 
	return errno; /* XXX */

    set_volhdr_union_ptr (db);

    if (dirp) {
	uint32_t i = oldnum;
	struct voldb_dir_entry e;

	memset (&e, 0, sizeof (e));
	e.FileType = 0;

	while (i < db->hdr.num) {
	    if (i < db->hdr.num - 1)
		e.nextptr = i + 1;
	    else
		e.nextptr = vdb->freeptr;
	    ret = voldb_put_dir (db, i, &e);
	    i++;
	    if (ret) {
		abort();  /* XXX need ftruncate and remap */
		return ret;
	    }
	}
    } else {
	uint32_t i = oldnum;
	struct voldb_file_entry e;

	memset (&e, 0, sizeof (e));
	e.FileType = 0;

	while (i < db->hdr.num) {
	    if (i < db->hdr.num - 1)
		e.nextptr = i + 1;
	    else
		e.nextptr = vdb->freeptr;
	    ret = voldb_put_file (db, i, &e);
	    i++;
	    if (ret) {
		abort();  /* XXX need ftruncate and remap */
		return ret;
	    }
	}
    }

    vdb->freeptr = oldnum;
    
    ret = voldb_write_hdr (db->fd, db);
    if (ret)
	return errno;

    return 0;
}

int
vdbflat_new_entry (struct voldb *db, uint32_t *num, uint32_t *unique)
{
    uint32_t oldfreeptr, newfreeptr, newunique;
    vdbflat *vdb = db->ptr;
    int ret;

    if (vdb->freeptr == VOLDB_FREELIST_END) {
	ret = vdbflat_extend_db (db, VOLDB_ENTEND_NUM);
	if (ret)
	    return ret;
    }

    oldfreeptr = vdb->freeptr;

    if ((db->hdr.flags & VOLDB_DIR) == VOLDB_DIR) {
	struct voldb_dir_entry e;

	ret = voldb_get_dir (db, oldfreeptr, &e);
	if (ret)
	    return ret;

	assert(e.nextptr != VOLDB_ENTRY_USED);
	newfreeptr = e.nextptr;
	newunique = ++e.unique;
	e.nextptr = VOLDB_ENTRY_USED;
	e.FileType = TYPE_DIR;

	ret = voldb_put_dir (db, oldfreeptr, &e);
	if (ret)
	    return ret;

    } else if ((db->hdr.flags & VOLDB_FILE) == VOLDB_FILE) {
	struct voldb_file_entry e;

	ret = voldb_get_file (db, oldfreeptr, &e);
	if (ret)
	    return ret;

	assert(e.nextptr != VOLDB_ENTRY_USED);
	newfreeptr = e.nextptr;
	newunique = ++e.unique;
	e.nextptr = VOLDB_ENTRY_USED;
	e.FileType = TYPE_FILE;

	ret = voldb_put_file (db, oldfreeptr, &e);
	if (ret)
	    return ret;
    } else
	abort();
    
    vdb->freeptr = newfreeptr;

    ret = voldb_write_hdr (db->fd, db);
    if (ret)
	return ret;

    *num = oldfreeptr;
    *unique = newunique;
   
    return 0;
}

/*
 * In database `db' add entry `num' to the freelist,
 * return the allocated inode `ino' that was associated
 * with `num'.
 */

static int
vdbflat_del_entry (struct voldb *db,
		   const uint32_t num,
		   onode_opaque *ino)
{
    vdbflat *vdb = db->ptr;
    int ret;

    if ((db->hdr.flags & (VOLDB_DIR|VOLDB_FILE)) == VOLDB_DIR) {
	struct voldb_dir_entry e;

	ret = voldb_get_dir (db, num, &e);
	if (ret)
	    return ret;

	e.FileType = 0;
	e.ParentVnode = e.ParentUnique = 0;
	e.LinkCount = 0;
	assert (e.nextptr == VOLDB_ENTRY_USED);
	e.nextptr = vdb->freeptr;
	if (ino)
	    memcpy (ino, &e.ino, sizeof (e.ino));
	memset (&e.ino, 0, sizeof(e.ino));

	ret = voldb_put_dir (db, num, &e);
	if (ret)
	    return ret;

    } else if ((db->hdr.flags & (VOLDB_DIR|VOLDB_FILE)) == VOLDB_FILE) {
	struct voldb_file_entry e;

	ret = voldb_get_file (db, num, &e);
	if (ret)
	    return ret;

	e.FileType = 0;
	e.ParentVnode = e.ParentUnique = 0;
	e.LinkCount = 0;
	assert (e.nextptr == VOLDB_ENTRY_USED);
	e.nextptr = vdb->freeptr;
	if (ino)
	    memcpy (ino, &e.ino, sizeof (e.ino));
	memset (&e.ino, 0, sizeof(e.ino));

	ret = voldb_put_file (db, num, &e);
	if (ret)
	    return ret;
    } else
	return EINVAL; /* XXX */

    vdb->freeptr = num;

    ret = voldb_write_hdr (db->fd, db);
    if (ret)
	return ret;

    return 0;
}

/*
 *
 */

static int
vdbflat_flush (struct voldb *db)
{
    vdbflat *vdb = db->ptr;

    if (msync (vdb->ptr, db->size, MS_SYNC) < 0)
	return errno;
    return 0;
}

/*
 *
 */

int
vdbflat_write_header (struct voldb *db,
		      void *d,
		      size_t sz)
{
    vdbflat *vdb = (vdbflat *) db->ptr;
    unsigned char *data = d;
    size_t size = sz;
    uint32_t i;
    int ret;

    assert (sz == VOLDB_HEADER_SIZE);

    /*
     * Make sure we don't overwrite the voldb header
     */

    data += VOLDB_HEADER_HALF;
    size -= VOLDB_HEADER_HALF;

    assert (size >= sizeof (i));

    if (vdb == NULL) {
	i = htonl (VOLDB_FREELIST_END);
    } else {
	i = htonl(vdb->freeptr);
    }
    memcpy (data, &i, sizeof (i));
	
    /* Now store it to the mmap file */
    memcpy (vdb->ptr, d, sz);

    ret = msync (vdb->ptr, sz, MS_SYNC);
    if (ret)
	return errno;

    return 0;
}

static int
vdbflat_expand (struct voldb *db,
		uint32_t num)
{
    int entriesneeded;
    int ret = 0;

    entriesneeded = num - db->hdr.num + 1;
    if (entriesneeded > 0)
	ret = vdbflat_extend_db(db, entriesneeded + VOLDB_ENTEND_NUM);

    if (ret)
	return ret;

    return 0;
}

static int
vdbflat_rebuild (struct voldb *db)
{
    uint32_t freeptr;
    vdbflat *vdb = db->ptr;
    int ret;
    int i;

    freeptr = VOLDB_FREELIST_END;

    for (i = db->hdr.num - 1; i >= 0; i--) {
	if ((db->hdr.flags & VOLDB_DIR) == VOLDB_DIR) {
	    struct voldb_dir_entry e;

	    ret = voldb_get_dir(db, i, &e);
	    if (ret)
		return ret;

	    if (e.nextptr == VOLDB_ENTRY_USED) {
		assert (e.FileType == TYPE_DIR);
	    } else {
		e.nextptr = VOLDB_FREELIST_END;
		freeptr = i;
		ret = voldb_put_dir(db, i, &e);
		if (ret)
		    return ret;
	    }
	} else {
	    struct voldb_file_entry e;

	    ret = voldb_get_file(db, i, &e);
	    if (ret)
		return ret;
	    
	    if (e.nextptr == VOLDB_ENTRY_USED) {
		assert (e.FileType == TYPE_FILE);
	    } else {
		e.nextptr = VOLDB_FREELIST_END;
		freeptr = i;		
		ret = voldb_put_file(db, i, &e);
		if (ret)
		    return ret;
	    }
	}
    }

    vdb->freeptr = freeptr;
    return 0;
}

/*
 *
 */

struct voldb_type vdb_flat = {
    vdbflat_init,
    vdbflat_close,
    vdbflat_get_dir,
    vdbflat_put_dir,
    vdbflat_put_acl,
    vdbflat_get_file,
    vdbflat_put_file,
    vdbflat_flush,
    vdbflat_new_entry,
    vdbflat_del_entry,
    vdbflat_write_header,
    vdbflat_expand,
    vdbflat_rebuild,
};
