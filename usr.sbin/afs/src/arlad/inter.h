/*	$OpenBSD: inter.h,v 1.1.1.1 1998/09/14 21:52:57 art Exp $	*/
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
 * The interface to the cache manager.
 */

/* $KTH: inter.h,v 1.14 1998/07/22 07:04:06 assar Exp $ */

#ifndef _INTER_H_
#define _INTER_H_

#include <cred.h>

/*
 * This is the return value of all these operations.
 * Iff res == -1, the error reason should be errno.
 */

typedef struct {
     int res;
     int error;
     u_int tokens;
} Result;

void
cm_init (void);

void
cm_store_state (void);

Result
cm_open (VenusFid fid, CredCacheEntry *ce, u_int tokens);

Result
cm_close (VenusFid fid, int flag, CredCacheEntry *ce);

Result
cm_getattr (VenusFid fid,
	    AFSFetchStatus *attr,
	    VenusFid *real_fid,
	    CredCacheEntry *ce,
	    AccessEntry **ae);

Result
cm_setattr (VenusFid fid, AFSStoreStatus *attr, CredCacheEntry *ce);

Result
cm_ftruncate (VenusFid fid, off_t size, CredCacheEntry *ce);

Result
cm_access (VenusFid fid, int mode, CredCacheEntry *ce);

Result
cm_lookup (VenusFid dir_fid, const char *name, VenusFid *res,
	   CredCacheEntry **ce);
Result
cm_create (VenusFid dir_fid, const char *name, AFSStoreStatus *store_attr,
	   VenusFid *res, AFSFetchStatus *fetch_attr,
	   CredCacheEntry* ce);
Result
cm_mkdir (VenusFid dir_fid, const char *name, AFSStoreStatus *store_attr,
	  VenusFid *res, AFSFetchStatus *fetch_attr,
	  CredCacheEntry* ce);

Result
cm_remove (VenusFid dir_fid, const char *name, CredCacheEntry *ce);

Result
cm_rmdir (VenusFid dir_fid, const char *name, CredCacheEntry *ce);

Result
cm_readdir (VenusFid fid);

Result
cm_link (VenusFid dir_fid, const char *name,
	 VenusFid existing_fid,
	 AFSFetchStatus *existing_status,
	 CredCacheEntry* ce);

Result
cm_symlink (VenusFid dir_fid, const char *name,
	    AFSStoreStatus *store_attr,
	    VenusFid *res, AFSFetchStatus *fetch_attr,
	    const char *contents,
	    CredCacheEntry* ce);

Result
cm_rename(VenusFid old_parent_fid, const char *old_name,
	  VenusFid new_parent_fid, const char *new_name,
	  CredCacheEntry* ce);

#endif /* _INTER_H_ */
