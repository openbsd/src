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

/*
 * Our cache of volume information.
 */

/* $KTH: volcache.h,v 1.28.2.2 2001/02/12 12:53:02 assar Exp $ */

#ifndef _VOLCACHE_
#define _VOLCACHE_

#include <stdio.h>
#include <cred.h>
#include <list.h>
#include "vldb.h"

/*
 * index for number into a VolCacheEntry
 */

struct num_ptr {
    int32_t cell;
    u_int32_t vol;
    struct volcacheentry *ptr;
    int32_t type;
};

/*
 * index for name into a VolCacheEntry
 */

struct name_ptr {
    int32_t cell;
    char name[VLDB_MAXNAMELEN];
    struct volcacheentry *ptr;
};

struct volcacheentry {
    nvldbentry entry;
    AFSVolSync volsync;
    int32_t cell;
    unsigned refcount;		/* number of files refererring this */
    unsigned vol_refs;		/* number of volumes refing this */
    time_t last_fetch;
    Listitem *li;
    VenusFid mp_fid;		/* pointing to this volume */
    VenusFid parent_fid;	/* .. of this volume */
    struct volcacheentry *parent; /* parent volume */
    struct {
	unsigned validp  : 1;
	unsigned stablep : 1;
    } flags;
    struct name_ptr name_ptr;
    struct num_ptr num_ptr[MAXTYPES];
};

typedef struct volcacheentry VolCacheEntry;

/*
 * This is magic cookie for the dump of the volcache.
 * It's supposed not to be able to be confused with an old-style
 * dump (with no header)
 */

#define VOLCACHE_MAGIC_COOKIE	0x00120100

/*
 * current version number of the dump file
 */

#define VOLCACHE_VERSION	0x3

const char *volcache_get_rootvolume (void);

void volcache_set_rootvolume (const char *volname);

void volcache_init (unsigned nentries, Bool recover);

int volcache_getbyname (const char *volname,
			int32_t cell,
			CredCacheEntry *ce,
			VolCacheEntry **e,
			int *type);

int volcache_getbyid (u_int32_t id,
		      int32_t cell,
		      CredCacheEntry *ce,
		      VolCacheEntry **e,
		      int *type);

void volcache_update_volsync (VolCacheEntry *e, AFSVolSync volsync);

void volcache_free (VolCacheEntry *e);

void volcache_ref (VolCacheEntry *e);

void volcache_volref (VolCacheEntry *e, VolCacheEntry *parent);

void volcache_volfree (VolCacheEntry *e);

void volcache_invalidate (u_int32_t id, int32_t cell);

void volcache_invalidate_ve (VolCacheEntry *ve);

int volume_make_uptodate (VolCacheEntry *e, CredCacheEntry *ce);

Bool volcache_reliable (u_int32_t id, int32_t cell);

int volcache_getname (u_int32_t id, int32_t cell, char *, size_t);

void volcache_status (void);

int
volcache_store_state (void);

int
volcache_volid2bit (const VolCacheEntry *ve, u_int32_t volid);

enum { VOLCACHE_OLD = 120 };

#endif /* _VOLCACHE_ */
