/*
 * Copyright (c) 1995-2002 Kungliga Tekniska Högskolan
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
 * The interface for the file-cache.
 */

/* $arla: fcache.h,v 1.97 2003/01/10 03:20:42 lha Exp $ */

#ifndef _FCACHE_H_
#define _FCACHE_H_

#include <nnpfs/nnpfs_message.h>
#include <fcntl.h>
#include <cred.h>
#include <heap.h>

/*
 * For each entry in the filecache we save the rights of NACCESS users.
 * The value should be the same as MAXRIGHTS from nnpfs_message.h
 * If it isn't you can get some very strange behavior from nnpfs, so don't
 * even try. XXX
 */ 

#define NACCESS MAXRIGHTS

typedef struct {
     nnpfs_pag_t cred;
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
    Bool valid;
    nnpfs_cache_handle nnpfs_handle;
} fcache_cache_handle;

typedef struct {
    struct Lock lock;		/* locking information for this entry */
    VenusFid fid;		/* The fid of the file for this entry */
    unsigned refcount;		/* reference count */
    uint32_t host;		/* the source of this entry */
    size_t length;		/* the cache usage size */
    size_t wanted_length;	/* this much data should be fetched */
    size_t fetched_length;	/* this much data has been fetched */
    AFSFetchStatus status;	/* Removed unused stuff later */
    AFSCallBack callback;	/* Callback to the AFS-server */
    AFSVolSync volsync;		/* Sync info for ro-volumes */
    AccessEntry acccache[NACCESS]; /* cache for the access rights */
    uint32_t anonaccess;	/* the access mask for system:anyuser */
    unsigned index;		/* this is V%u */
    fcache_cache_handle handle;	/* handle */
    struct {
	unsigned usedp : 1;	/* Is this entry used? */
	unsigned attrp : 1;	/* Are the attributes in status valid? */
	unsigned attrusedp : 1;	/* Attr is used in the kernel */
	unsigned datausedp : 1;	/* Data is used in the kernel */
	unsigned extradirp : 1;	/* Has this directory been "converted"? */
	unsigned mountp : 1;	/* Is this an AFS mount point? */
	unsigned kernelp : 1;	/* Does this entry exist in the kernel? */
	unsigned sentenced : 1;	/* This entry should die */
	unsigned silly : 1;	/* Instead of silly-rename */
	unsigned fake_mp : 1;	/* a `fake' mount point */
	unsigned vol_root : 1;	/* root of a volume */
    } flags;
    u_int tokens;		/* read/write tokens for the kernel */
    VenusFid parent;
    Listitem *lru_le;		/* lru */
    heap_ptr invalid_ptr;	/* pointer into the heap */
    VolCacheEntry *volume;	/* pointer to the volume entry */
    Bool priority;		/* is the file worth keeping */
    int hits;			/* number of lookups */
    int cleanergen;		/* generation cleaner */
    PollerEntry *poll;		/* poller entry */
    uint32_t disco_id;		/* id in disconncted log */
} FCacheEntry;

/*
 * The fileservers to ask for a particular volume.
 */

struct fs_server_context {
    int i;			/* current number being probed */
    int num_conns;		/* number in `conns' */
    VolCacheEntry *ve;		/*  */
    struct fs_server_entry {
	ConnCacheEntry *conn;	/* rx connection to server */
	int ve_ent;		/* entry in `ve' */
    } conns[NMAXNSERVERS];
};

typedef struct fs_server_context fs_server_context;

/*
 * How far the cleaner will go went cleaning things up.
 */

extern Bool fprioritylevel;

void
fcache_init (u_long alowvnodes,
	     u_long ahighvnodes,
	     int64_t alowbytes,
	     int64_t ahighbytes,
	     Bool recover);

int
fcache_reinit(u_long alowvnodes,
	      u_long ahighvnodes,
	      int64_t alowbytes,
	      int64_t ahighbytes);

void
fcache_purge_volume (VenusFid fid);

void
fcache_purge_host (u_long host);

void
fcache_purge_cred (nnpfs_pag_t cred, int32_t cell);

void
fcache_stale_entry (VenusFid fid, AFSCallBack callback);

void
fcache_invalidate_mp (void);

int
fcache_file_name (FCacheEntry *entry, char *s, size_t len);

int
fcache_conv_file_name (FCacheEntry *entry, char *s, size_t len);

int
fcache_dir_name (FCacheEntry *entry, char *s, size_t len);

int
fcache_extra_file_name (FCacheEntry *entry, char *s, size_t len);

int
fcache_open_file (FCacheEntry *entry, int flag);

int
fcache_open_extra_dir (FCacheEntry *entry, int flag, mode_t mode);

int
fcache_fhget (char *filename, fcache_cache_handle *handle);

int
write_data (FCacheEntry *entry, AFSStoreStatus *status, CredCacheEntry *ce);

int
truncate_file (FCacheEntry *entry, off_t size,
	       AFSStoreStatus *status, CredCacheEntry *ce);

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

void
fcache_release (FCacheEntry *e);

int
fcache_find (FCacheEntry **res, VenusFid fid);

int
fcache_get_data (FCacheEntry **e, CredCacheEntry **ce,
		 size_t wanted_length);

int
fcache_verify_attr (FCacheEntry *entry, FCacheEntry *parent_entry,
		    const char *prefered_name, CredCacheEntry* ce);

int
fcache_verify_data (FCacheEntry *e, CredCacheEntry *ce);

int
followmountpoint (VenusFid *fid, const VenusFid *parent, FCacheEntry *parent_e,
		  CredCacheEntry **ce);

void
fcache_status (void);

int
fcache_store_state (void);

int
getacl(VenusFid fid, CredCacheEntry *ce,
       AFSOpaque *opaque);

int
setacl(VenusFid fid, CredCacheEntry *ce,
       AFSOpaque *opaque, FCacheEntry **ret);

int
getvolstat(VenusFid fid, CredCacheEntry *ce,
	   AFSFetchVolumeStatus *volstat,
	   char *volumename, size_t volumenamesz,
	   char *offlinemsg,
	   char *motd);

int
setvolstat(VenusFid fid, CredCacheEntry *ce,
	   AFSStoreVolumeStatus *volstat,
	   char *volumename,
	   char *offlinemsg,
	   char *motd);

int64_t
fcache_highbytes(void);

int64_t
fcache_usedbytes(void);

int64_t
fcache_lowbytes(void);

u_long
fcache_highvnodes(void);

u_long
fcache_usedvnodes(void);

u_long
fcache_lowvnodes(void);

int
fcache_need_bytes(u_long needed);

Bool
fcache_need_nodes (void);

int
fcache_giveup_all_callbacks (void);

int
fcache_reobtain_callbacks (struct nnpfs_cred *cred);

/* XXX - this shouldn't be public, but getrights in inter.c needs it */
int
read_attr (FCacheEntry *, CredCacheEntry *);

Bool
findaccess (nnpfs_pag_t cred, AccessEntry *ae, AccessEntry **pos);

void
fcache_unused(FCacheEntry *entry);

void
fcache_update_length (FCacheEntry *entry, size_t len, size_t have_len);

int
init_fs_context (FCacheEntry *e,
		 CredCacheEntry *ce,
		 fs_server_context *context);

ConnCacheEntry *
find_first_fs (fs_server_context *context);

ConnCacheEntry *
find_next_fs (fs_server_context *context,
	      ConnCacheEntry *prev_conn,
	      int mark_as_dead);

void
free_fs_server_context (fs_server_context *context);

void
recon_hashtabadd(FCacheEntry *entry);
 
void
recon_hashtabdel(FCacheEntry *entry);

int
fcache_get_fbuf (FCacheEntry *centry, int *fd, fbuf *fbuf,
		 int open_flags, int fbuf_flags);

int64_t
fcache_calculate_usage (void);

const VenusFid *
fcache_realfid (const FCacheEntry *entry);

void
fcache_mark_as_mountpoint (FCacheEntry *entry);

const char *
fcache_getdefsysname (void);

int
fcache_addsysname (const char *sysname);

int
fcache_removesysname (const char *sysname);

int
fcache_setdefsysname (const char *sysname);

int
fs_probe (struct rx_connection *conn);

#endif /* _FCACHE_H_ */
