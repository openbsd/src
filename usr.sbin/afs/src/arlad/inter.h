/*
 * Copyright (c) 1995 - 2002 Kungliga Tekniska Högskolan
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
 * The interface to the cache manager.
 */

/* $arla: inter.h,v 1.36 2003/01/09 16:40:10 lha Exp $ */

#ifndef _INTER_H_
#define _INTER_H_

#include <cred.h>

void
cm_init (void);

void
cm_store_state (void);

int
cm_open (FCacheEntry *entry, CredCacheEntry *ce, u_int tokens);

int
cm_close (FCacheEntry *entry, int flag, AFSStoreStatus *status,
	  CredCacheEntry* ce);

int
cm_getattr (FCacheEntry *entry,
	    CredCacheEntry *ce,
	    AccessEntry **ae);

int
cm_setattr (FCacheEntry *entry, AFSStoreStatus *attr, CredCacheEntry *ce);

int
cm_ftruncate (FCacheEntry *entry, off_t size,
	      AFSStoreStatus *storeattr, CredCacheEntry *ce);

int
cm_access (FCacheEntry *entry, int mode, CredCacheEntry *ce);

int
cm_lookup (FCacheEntry **entry, const char *name, VenusFid *res,
	   CredCacheEntry **ce, int follow_mount_point);
int
cm_create (FCacheEntry **dir, const char *name, AFSStoreStatus *store_attr,
	   VenusFid *res, AFSFetchStatus *fetch_attr,
	   CredCacheEntry **ce);
int
cm_mkdir (FCacheEntry **dir, const char *name, AFSStoreStatus *store_attr,
	  VenusFid *res, AFSFetchStatus *fetch_attr,
	  CredCacheEntry **ce);

int
cm_remove (FCacheEntry **dir, const char *name, CredCacheEntry **ce);

int
cm_rmdir (FCacheEntry **dir, const char *name, CredCacheEntry **ce);

int
cm_link (FCacheEntry **dir, const char *name,
	 VenusFid existing_fid,
	 AFSFetchStatus *existing_status,
	 CredCacheEntry **ce);

int
cm_symlink (FCacheEntry **dir, const char *name,
	    AFSStoreStatus *store_attr,
	    VenusFid *res, VenusFid *real_fid,
	    AFSFetchStatus *fetch_attr,
	    const char *contents,
	    CredCacheEntry **ce);

int
cm_rename(FCacheEntry **old_dir, const char *old_name,
	  FCacheEntry **new_dir, const char *new_name,
	  VenusFid *child_fid,
	  int *update_child,
	  CredCacheEntry **ce);

int
cm_walk (VenusFid fid,
	 const char *name,
	 VenusFid *res);

void
cm_check_consistency (void);

void
cm_turn_on_consistency_check(void);

#endif /* _INTER_H_ */
