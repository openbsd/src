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

/* $arla: mnode.h,v 1.9 2002/01/23 00:50:08 d95_mah Exp $ */

#ifndef MILKO_MNODE_H
#define MILKO_MNODE_H 1

#include <sys/types.h>

#include <fs.h>
#include <pts.h>
#include <voldb.h>

#include <list.h>

typedef enum { VOLOP_READ	= 0x001,	/* read file */
	       VOLOP_WRITE	= 0x002,	/* write file */
	       VOLOP_INSERT	= 0x004,	/* insert file */
	       VOLOP_LOOKUP	= 0x008,	/* lookup fileentry */
	       VOLOP_DELETE	= 0x010,	/* delete entry */
	       VOLOP_LOCK	= 0x020,	/* lock file */
	       VOLOP_ADMIN	= 0x040,	/* modify bits */
	       VOLOP_GETSTATUS	= 0x080,	/* get status */
	       VOLOP_NOCHECK	= 0x100		/* do no check */
} volop_flags ;

struct mnode {
    int fd;
    int ref;			/* reference counter */
    struct stat sb;		/* the status of the node */ 
    AFSFetchStatus fs;		/* fetchstatus */
    AFSFid fid;			/* only valid if on hashtable */
    struct voldb_entry e;	/* entry information */
    struct {
	unsigned usedp:1;	/* if node is used */
	unsigned removedp:1;	/* if node has been removed */
	unsigned fdp:1;		/* if fd is open */
	unsigned sbp:1;		/* if stat sb is valid */
	unsigned fsp:1;		/* if afsfetchstatus fs is valid */
	unsigned ep:1;		/* if voldb entry is valid */
    } flags;
    Listitem *li;		/* where we are placed in the mnode_lru */
};

struct msec {
    struct fs_security_context	*sec; /* security context */
    int32_t	caller_access;	/* access of caller */
    int32_t	anonymous_access; /* anonymous access */
    volop_flags	flags;		/* what we want to do with the node */
    int loop;			/* to detect loop */
};

struct fs_security_context {
    prlist *cps;	/* current proctection set */
    int32_t uid;	/* user id of caller */
    int superuser;	/* is super user */
    int ref;		/* reference counter */
};

void
mnode_init (unsigned num);

int
mnode_find (const AFSFid *fid, struct mnode **node);

void
mnode_free (struct mnode *node, Bool bad);

void
mnode_remove (const AFSFid *fid);

int
mnode_update_size_cached (struct mnode *);

int
mnode_update_size (struct mnode *, int32_t *len);

#endif /* MILKO_MNODE_H */
 
