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


#ifndef _VOLDB_INTERNAL_H
#define _VOLDB_INTERNAL_H

/*
 * Various internal data structures
 */

struct voldb_header {
    uint32_t magic;	/* network order */
#define VOLDB_MAGIC_HEADER 0x47111147
    uint32_t num;	/* Number of entries in the db */
    uint32_t flags;	/* flags */
};

struct voldb {
    struct voldb_header hdr;	/* header of voldb */
    int32_t type;		/* type of voldb */
    size_t size;		/* Size of file */
    int fd;			/* fd to volume */
    int32_t volume;		/* volume */
    void *ptr;			/* type owned data */
};

struct voldb_type {
    int (*init) (int fd, struct voldb *db, int createp);
    int (*close) (struct voldb *db);
    int (*get_dir) (struct voldb *db,
		    const uint32_t num, 
		    struct voldb_dir_entry *e);
    int (*put_dir) (struct voldb *db, 
		    const uint32_t num, 
		    struct voldb_dir_entry *e);
    int (*put_acl) (struct voldb *db, 
		    uint32_t num,
		    struct voldb_dir_entry *e);
    int (*get_file) (struct voldb *db,
		     uint32_t num, 
		     struct voldb_file_entry *e);
    int (*put_file) (struct voldb *db, 
		     uint32_t num, 
		     struct voldb_file_entry *e);
    int (*flush) (struct voldb *db);
    int (*new_entry) (struct voldb *db, 
		      uint32_t *num, 
		      uint32_t *unique);
    int (*del_entry) (struct voldb *db, 
		      const uint32_t num,
		      onode_opaque *ino);
    int (*write_header) (struct voldb *db,
			 void *data,
			 size_t sz);
    int (*expand) (struct voldb *db,
		   uint32_t num);
    int (*rebuild) (struct voldb *db);
};


#define VOLDB_FUNC(db,name) (voltypes[(db)->type])->name

extern struct voldb_type *voltypes[];

static inline int __attribute__ ((unused))
voldb_get_dir (struct voldb *db, uint32_t num, struct voldb_dir_entry *e)
{
    return VOLDB_FUNC(db,get_dir)(db, num, e);
}

static inline int __attribute__ ((unused))
voldb_get_file (struct voldb *db, uint32_t num, struct voldb_file_entry *e)
{
    return VOLDB_FUNC(db,get_file)(db, num, e);
}

static inline int __attribute__ ((unused))
voldb_put_dir (struct voldb *db, uint32_t num, struct voldb_dir_entry *e)
{
    return VOLDB_FUNC(db,put_dir)(db, num, e);
}

static inline int __attribute__ ((unused))
voldb_put_file (struct voldb *db, uint32_t num, struct voldb_file_entry *e)
{
    return VOLDB_FUNC(db,put_file)(db, num, e);
}

int
voldb_parse_header (struct voldb *db, void *d, size_t sz);

/*
 * Diffrent types of db's
 */

extern struct voldb_type vdb_flat;

#endif /* _VOLDB_INTERNAL_H */

