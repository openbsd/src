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

/* $KTH: voldb.h,v 1.17 2000/10/03 00:20:19 lha Exp $ */

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
    u_int32_t nextptr;
    u_int32_t unique;
    onode_opaque ino;
    u_int32_t spare2;		/* Must be zero */
    u_int32_t InterfaceVersion;
    u_int32_t FileType;
    u_int32_t LinkCount;
    u_int32_t Length;
    u_int32_t spare3;		/* Must be zero */
    u_int32_t DataVersion;
    u_int32_t Author;
    u_int32_t Owner;
    u_int32_t UnixModeBits;
    u_int32_t ParentVnode;
    u_int32_t ParentUnique;
    u_int32_t SegSize;
    u_int32_t ServerModTime;
    u_int32_t spare4;		/* Must be zero */
    u_int32_t Group;
    u_int32_t spare5;		/* Must be zero */
};

/*
 * Data structure for an acl_entry
 */

struct acl_entry {
    u_int32_t owner;
    u_int32_t flags;
};

/*
 * Data structure for a dir
 */

struct voldb_dir_entry {    
    u_int32_t nextptr;
    u_int32_t unique;
    onode_opaque ino;
    u_int32_t spare2;		/* Must be zero */
    u_int32_t InterfaceVersion;
    u_int32_t FileType;
    u_int32_t LinkCount;
    u_int32_t Length;
    u_int32_t spare3;		/* Must be zero */
    u_int32_t DataVersion;
    u_int32_t Author;
    u_int32_t Owner;
    u_int32_t UnixModeBits;
    u_int32_t ParentVnode;
    u_int32_t ParentUnique;
    u_int32_t SegSize;
    u_int32_t ServerModTime;
    u_int32_t spare4;		/* Must be zero */
    u_int32_t Group;
    u_int32_t spare5;		/* Must be zero */
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
voldb_put_acl (struct voldb *db, u_int32_t num, struct voldb_dir_entry *e);

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
voldb_new_entry (struct voldb *db, u_int32_t *num, u_int32_t *unique);

int
voldb_del_entry (struct voldb *db, u_int32_t num, onode_opaque *ino);

int
voldb_header_info (struct voldb *db, 
		   u_int32_t *num,
		   u_int32_t *flags);

u_int32_t
voldb_get_volume (struct voldb *db);

int
voldb_write_hdr (int fd, struct voldb *db);

void
voldb_update_time(struct voldb_entry *e, time_t t);

/* vol.c */

int
vol_getname (u_int32_t num, char *str, size_t sz);

int
vol_getfullname (u_int32_t part, u_int32_t num, char *str, size_t sz);

int
vol_read_header (int fd, volintInfo *info);

int
vol_write_header (int fd, volintInfo *info);

int
vol_create (int fd, u_int32_t num, const char *name,
	    u_int32_t type, u_int32_t parent);

void
vol_pretty_print_info (FILE *out, volintInfo *info);

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
