/*	$OpenBSD: fcache.h,v 1.1.1.1 1998/09/14 21:52:56 art Exp $	*/
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
 * The interface for the file-cache.
 */

/* $KTH: fcache.h,v 1.37 1998/07/22 07:02:04 assar Exp $ */

#ifndef _FCACHE_H_
#define _FCACHE_H_

#include <xfs/xfs_message.h>
#include <fcntl.h>
#include <cred.h>

/*
 * For each entry in the filecache we save the rights of NACCESS users.
 * The value should be the same as MAXRIGHTS from xfs_message.h
 * If it isn't you can get some very strange behavior from xfs, so don't
 * even try. XXX
 */ 

#define NACCESS MAXRIGHTS

typedef struct {
     pag_t cred;
     u_long access;
} AccessEntry;

enum Access { ANONE   = 0x0,
              AREAD   = 0x01,
	      AWRITE  = 0x02,
	      AINSERT = 0x04,
	      ALIST   = 0x08,
	      ADELETE = 0x10,
	      ALOCK   = 0x20,
	      AADMIN  = 0x40 };

typedef struct {
    struct Lock lock;		/* locking information for this entry */
    VenusFid fid;		/* The fid of the file for this entry */
    unsigned refcount;		/* reference count */
    u_long host;		/* the source of this entry */
    AFSFetchStatus status;	/* Removed unused stuff later */
    AFSCallBack callback;	/* Callback to the AFS-server */
    AFSVolSync volsync;		/* Sync info for ro-volumes */
    AccessEntry acccache[NACCESS]; /* cache for the access rights */
    u_long anonaccess;		/* the access mask for system:anyuser */
    ino_t inode;		/* right now only an index */
    struct {
	unsigned usedp : 1;	/* Is this entry used? */
	unsigned attrp : 1;	/* Are the attributes in status valid? */
	unsigned datap : 1;	/* Is the cache-file valid? */
	unsigned attrusedp : 1;	/* Attr is used in the kernel */
	unsigned datausedp : 1;	/* Data is used in the kernel */
	unsigned extradirp : 1;	/* Has this directory been "converted"? */
	unsigned mountp : 1;	/* Is this an AFS mount point? */
	unsigned kernelp : 1;	/* Does this entry exist in the kernel? */
    } flags;
    u_int tokens;		/* read/write tokens for the kernel */
    VenusFid parent;		/* fid of the parent */
    VenusFid realfid;		/* Real fid (mountpoints) */
    Listitem *lru_le;		/* lru */
    Listitem *invalid_le;	/* invalidation list */
    VolCacheEntry *volume;	/* pointer to the volume entry */
    unsigned priority;		/* the priority of keeping the file 0-100 */
} FCacheEntry;


/*
 * How far the cleaner will go went cleaning things up.
 */

extern unsigned fprioritylevel;

void
fcache_init (u_long alowvnodes,
	     u_long ahighvnodes,
	     u_long alowbytes,
	     u_long ahighbytes,
	     Bool recover);

int
fcache_reinit(u_long alowvnodes,
	      u_long ahighvnodes,
	      u_long alowbytes,
	      u_long ahighbytes);

void
fcache_purge_volume (VenusFid fid);

void
fcache_purge_host (u_long host);

void
fcache_purge_cred (pag_t cred, int32_t cell);

void
fcache_stale_entry (VenusFid fid, AFSCallBack callback);

int
fcache_file_name (FCacheEntry *entry, char *s, size_t len);

int
fcache_extra_file_name (FCacheEntry *entry, char *s, size_t len);

int
fcache_open_file (FCacheEntry *entry, int flag, mode_t mode);

int
fcache_open_extra_dir (FCacheEntry *entry, int flag, mode_t mode);

int
write_data (FCacheEntry *entry, CredCacheEntry *ce);

int
truncate_file (FCacheEntry *entry, off_t size, CredCacheEntry *ce);

int
write_attr (FCacheEntry *entry, const AFSStoreStatus *status,
	    CredCacheEntry *ce);

int
create_file (FCacheEntry *dir_entry,
	     const char *name, AFSStoreStatus *store_attr,
	     VenusFid *child_fid, AFSFetchStatus *fetch_attr,
	     CredCacheEntry *ce);

int
create_directory (FCacheEntry *dir_entry,
		  const char *name, AFSStoreStatus *store_attr,
		  VenusFid *child_fid, AFSFetchStatus *fetch_attr,
		  CredCacheEntry *ce);

int
create_symlink (FCacheEntry *dir_entry,
		const char *name, AFSStoreStatus *store_attr,
		VenusFid *child_fid, AFSFetchStatus *fetch_attr,
		const char *contents,
		CredCacheEntry *ce);

int
create_link (FCacheEntry *dir_entry,
	     const char *name,
	     FCacheEntry *existing_entry,
	     CredCacheEntry *ce);

int
remove_file (FCacheEntry *dire, const char *name, CredCacheEntry *ce);

int
remove_directory (FCacheEntry *dire, const char *name, CredCacheEntry *ce);

int
rename_file (FCacheEntry *old_dir,
	     const char *old_name,
	     FCacheEntry *new_dir,
	     const char *new_name,
	     CredCacheEntry *ce);

int
getroot (VenusFid *res, CredCacheEntry *ce);

int
fcache_get (FCacheEntry **res, VenusFid fid, CredCacheEntry *ce);

int
fcache_find (FCacheEntry **res, VenusFid fid);

int
fcache_get_attr (FCacheEntry *e, CredCacheEntry *ce);

int
fcache_get_data (FCacheEntry *e, CredCacheEntry *ce);

int
followmountpoint (VenusFid *fid, VenusFid *parent, CredCacheEntry **ce);

void
fcache_status (FILE *f);

int
fcache_store_state (void);

int
getacl(VenusFid fid, CredCacheEntry *ce,
       AFSOpaque *opaque);

int
setacl(VenusFid fid, CredCacheEntry *ce,
       AFSOpaque *opaque);

int
getvolstat(VenusFid fid, CredCacheEntry *ce,
	   AFSFetchVolumeStatus *volstat,
	   char *volumename,
	   char *offlinemsg,
	   char *motd);

int
setvolstat(VenusFid fid, CredCacheEntry *ce,
	   AFSStoreVolumeStatus *volstat,
	   char *volumename,
	   char *offlinemsg,
	   char *motd);


/* XXX - this shouldn't be public, but getrights in inter.c needs it */
int
read_attr (FCacheEntry *, CredCacheEntry *);

Bool
findaccess (pag_t cred, AccessEntry *ae, AccessEntry **pos);

#endif /* _FCACHE_H_ */
