/*
 * Copyright (c) 1999 Kungliga Tekniska Högskolan
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

/* $arla: voldb.h,v 1.21 2002/02/07 17:59:58 lha Exp $ */

#ifndef FILBUNKE_VOLDB_H
#define FILBUNKE_VOLDB_H 1

#include <volumeserver.h>
#include <fs_def.h>
#include <vstatus.h>

/*
 * Of this space half is shared of the voldb and the underlaying
 * layer.
 */

#define VOLDB_HEADER_SIZE (10*4)
#define VOLDB_HEADER_HALF (VOLDB_HEADER_SIZE/2) 

/*
 * Data structure for a file (link is same)
 */

struct voldb_file_entry {    
    uint32_t nextptr;
    uint32_t unique;
    onode_opaque ino;
    uint32_t spare2;		/* Must be zero */
    uint32_t InterfaceVersion;
    uint32_t FileType;
    uint32_t LinkCount;
    uint32_t Length;
    uint32_t spare3;		/* Must be zero */
    uint32_t DataVersion;
    uint32_t Author;
    uint32_t Owner;
    uint32_t UnixModeBits;
    uint32_t ParentVnode;
    uint32_t ParentUnique;
    uint32_t SegSize;
    uint32_t ServerModTime;
    uint32_t spare4;		/* Must be zero */
    uint32_t Group;
    uint32_t spare5;		/* Must be zero */
};

/*
 * Data structure for an acl_entry
 */

struct acl_entry {
    uint32_t owner;
    uint32_t flags;
};

/*
 * Data structure for a dir
 */

struct voldb_dir_entry {    
    uint32_t nextptr;
    uint32_t unique;
    onode_opaque ino;
    uint32_t spare2;		/* Must be zero */
    uint32_t InterfaceVersion;
    uint32_t FileType;
    uint32_t LinkCount;
    uint32_t Length;
    uint32_t spare3;		/* Must be zero */
    uint32_t DataVersion;
    uint32_t Author;
    uint32_t Owner;
    uint32_t UnixModeBits;
    uint32_t ParentVnode;
    uint32_t ParentUnique;
    uint32_t SegSize;
    uint32_t ServerModTime;
    uint32_t spare4;		/* Must be zero */
    uint32_t Group;
    uint32_t spare5;		/* Must be zero */
    struct acl_entry negacl[FS_MAX_ACL];
    struct acl_entry acl[FS_MAX_ACL];
};

struct voldb_entry {
    int32_t type;
    union {
	struct voldb_dir_entry dir;
	struct voldb_file_entry file;
    } u;
};

/* defines */

#define VOLDB_FILE 0x1
#define VOLDB_DIR 0x2

#define VOLDB_DEFAULT_TYPE 0

#define VOLDB_FREELIST_END 0xffffffff
#define VOLDB_ENTRY_USED 0xfffffffe

/* forward declarations */

struct voldb;

/* prototypes */

int
voldb_init (int fd, int32_t type, int32_t volume, struct voldb **db);

int
voldb_close (struct voldb *db);

int
voldb_put_entry (struct voldb *db, int32_t num, struct voldb_entry *e);

int
voldb_get_size (struct voldb *db, int32_t *total_size, int32_t *num_entries);

int
voldb_get_entry (struct voldb *db, int32_t num, struct voldb_entry *e);

int
voldb_put_acl (struct voldb *db, uint32_t num, struct voldb_dir_entry *e);

int
voldb_pretty_print_file (struct voldb_file_entry *e);

int
voldb_pretty_print_dir (struct voldb_dir_entry *e);

int
voldb_pretty_print_header (struct voldb *db);

int
voldb_create_header (int fd, int type, int flags);

int
voldb_flush (struct voldb *db);

int
voldb_new_entry (struct voldb *db, uint32_t *num, uint32_t *unique);

int
voldb_del_entry (struct voldb *db, uint32_t num, onode_opaque *ino);

int
voldb_header_info (struct voldb *db, 
		   uint32_t *num,
		   uint32_t *flags);

uint32_t
voldb_get_volume (struct voldb *db);

int
voldb_write_hdr (int fd, struct voldb *db);

void
voldb_update_time(struct voldb_entry *e, time_t t);

/* vol.c */

int
vol_getname (uint32_t num, char *str, size_t sz);

int
vol_getfullname (uint32_t part, uint32_t num, char *str, size_t sz);

int
vol_read_header (int fd, volintInfo *info);

int
vol_write_header (int fd, volintInfo *info);

int
vol_create (int fd, uint32_t num, const char *name,
	    uint32_t type, uint32_t parent);

void
vol_pretty_print_info (FILE *out, volintInfo *info);

int
voldb_expand (struct voldb *db, int32_t num);

int
voldb_rebuild (struct voldb *db);

enum voldb_newnum_sizes {  VOLDB_ENTEND_NUM = 10 } ;

const char *
vol_voltype2name (int32_t type);


int
afs_dir_p (int32_t vno);

int32_t
dir_afs2local (int32_t vno);

int32_t
dir_local2afs (int32_t vno);

int32_t
file_afs2local (int32_t vno);

int32_t
file_local2afs (int32_t vno);

#endif /*  FILBUNKE_VOLDB_H 1 */
