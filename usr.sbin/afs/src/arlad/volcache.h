/*	$OpenBSD: volcache.h,v 1.1.1.1 1998/09/14 21:52:57 art Exp $	*/
/*
 * Copyright (c) 1995, 1996, 1997, 1998 Kungliga Tekniska Högskolan
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
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *      This product includes software developed by the Kungliga Tekniska
 *      Högskolan and its contributors.
 * 
 * 4. Neither the name of the Institute nor the names of its contributors
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

/* $KTH: volcache.h,v 1.13 1998/07/29 21:29:10 assar Exp $ */

#ifndef _VOLCACHE_
#define _VOLCACHE_

#include <stdio.h>
#include <cred.h>

#define BACKSUFFIX ".backup"
#define ROSUFFIX   ".readonly"

struct num_ptr {
    int32_t cell;
    u_int32_t vol;
    struct volcacheentry *ptr;
};

struct name_ptr {
    int32_t cell;
    char name[VLDB_MAXNAMELEN];
    struct volcacheentry *ptr;
};

struct volcacheentry {
    vldbentry entry;
    AFSVolSync volsync;
    int32_t cell;
    unsigned refcount;
    struct {
	unsigned validp : 1;
    } flags;
    struct name_ptr name_ptr[MAXTYPES];
    struct num_ptr num_ptr[MAXTYPES];
};

typedef struct volcacheentry VolCacheEntry;

const char *volcache_get_rootvolume (void);

void volcache_set_rootvolume (const char *volname);

void volcache_init (unsigned nentries, Bool recover);

VolCacheEntry *volcache_getbyname (const char *volname,
				   int32_t cell,
				   CredCacheEntry *ce);

VolCacheEntry *volcache_getbyid (u_int32_t id,
				 int32_t cell,
				 CredCacheEntry *ce);

void volcache_update_volsync (VolCacheEntry *e, AFSVolSync volsync);

void volcache_free (VolCacheEntry *e);

void volcache_invalidate (u_int32_t id, int32_t cell);

void volcache_status (FILE *f);

int
volcache_store_state (void);

#endif /* _VOLCACHE_ */
