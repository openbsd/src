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
 * Interface to the cache manager.
 */

#include "arla_local.h"
RCSID("$arla: inter.c,v 1.138 2003/01/10 03:05:44 lha Exp $") ;

#include <nnpfs/nnpfs_message.h>

Bool cm_consistencyp = FALSE;

/*
 * Return the rights for user cred and entry e.
 * If the rights are not existant fill in the entry.
 * The locking of e is up to the caller.
 */

static u_long
getrights (FCacheEntry *e, CredCacheEntry *ce)
{
     AccessEntry *ae;
     int error;

     while (findaccess (ce->cred, e->acccache, &ae) == FALSE) {
	 if ((error = read_attr(e, ce)) != 0)
	     return 0; /* XXXX  we want to return errno */
     }
     return ae->access;
}

/*
 * Check to see if the operation(s) mask are allowed to user cred on
 * file e
 */

static Bool
checkright (FCacheEntry *e, u_long mask, CredCacheEntry *ce)
{
    u_long rights;

    if (e->status.FileType == TYPE_LINK &&
	e->anonaccess & ALIST)
	return TRUE;
    if ((e->anonaccess & mask) == mask)
	return TRUE;
    rights = getrights (e, ce);
    if (e->status.FileType == TYPE_LINK &&
	rights & ALIST)
	return TRUE;
    if ((rights & mask) == mask)
	return TRUE;
    return FALSE;
}

static int log_fd;
static FILE *log_fp;

/*
 *
 */

void
cm_init (void)
{
    log_fd = open ("log", O_WRONLY | O_APPEND | O_CREAT | O_BINARY, 0666);
    if (log_fd < 0)
	arla_err (1, ADEBERROR, errno, "open log");
    log_fp = fdopen (log_fd, "a");
    if (log_fp == NULL)
	arla_err (1, ADEBERROR, errno, "fdopen");
}

/*
 *
 */

void
cm_store_state (void)
{
    fclose (log_fp);
}

/*
 *
 */

static void
log_operation (const char *fmt, ...)
{
    va_list args;
    struct timeval now;

    if(connected_mode == CONNECTED && cm_consistencyp == FALSE)
	return;

    va_start (args, fmt);
    gettimeofday (&now, NULL);
    fprintf (log_fp, "%lu.%lu ",
	     (unsigned long)now.tv_sec,
	     (unsigned long)now.tv_usec);
    vfprintf (log_fp, fmt, args);
    va_end (args);
}

/*
 *
 *
 */

void
cm_turn_on_consistency_check(void)
{
    cm_consistencyp = TRUE;
}

/*
 * Check consistency of the fcache.
 * Will break the log-file.
 */

void
cm_check_consistency (void)
{
    static unsigned int log_times = 0;
    static unsigned int file_times = 0;
    int64_t calc_size, real_size;
    char newname[MAXPATHLEN];

    if (cm_consistencyp == FALSE)
	return;
    
    calc_size = fcache_calculate_usage();
    real_size = fcache_usedbytes ();

    if (calc_size != real_size) {
	    log_operation ("consistency check not guaranteed "
			   "(calc: %d, real: %d, diff %d), exiting\n", 
			   (int) calc_size, (int) real_size,
			   (int)(calc_size - real_size));
	    cm_store_state ();
	    exit(-1);
    }
    if (log_times % 100000 == 0) {
	log_operation ("consistency check ok, rotating logs\n");
	cm_store_state ();
	snprintf (newname, sizeof(newname), "log.%d", file_times++);
	rename ("log", newname);
	cm_init ();	
	log_operation ("brave new world\n");
    }
    log_times++;
}

/*
 * These functions often take a FID as an argument to be general, but
 * they are intended to be called from a vnode-type of layer.
 */

/*
 * The interface to the open-routine.
 */

int
cm_open (FCacheEntry *entry, CredCacheEntry *ce, u_int tokens)
{
     u_long mask;
     int error = 0;

     switch(tokens) {
     case NNPFS_DATA_R:
#if 0
     case NNPFS_OPEN_NR:
     case NNPFS_OPEN_SR:
#endif
	  mask = AREAD;
	  break;
     case NNPFS_DATA_W:
	  mask = AWRITE;
	  break;
     case NNPFS_OPEN_NW:
	  mask = AREAD | AWRITE;
	  tokens |= NNPFS_DATA_R | NNPFS_DATA_W;
	  break;
     default:
	 arla_warnx (ADEBCM, "cm_open(): unknown token: %d, assuming AREAD",
		     tokens);
	 mask = AREAD;
	 tokens |= NNPFS_DATA_R;
     }

     if (checkright (entry, mask, ce)) {
	  assert(entry->flags.attrusedp);
	  entry->flags.datausedp = TRUE;
	  entry->tokens |= tokens;
	  
	  log_operation ("open (%ld,%lu,%lu,%lu) %u\n",
			 entry->fid.Cell,
			 entry->fid.fid.Volume,
			 entry->fid.fid.Vnode,
			 entry->fid.fid.Unique,
			 mask);
     } else
	  error = EACCES;

     cm_check_consistency();
 
     return error;
}

/*
 * close. Set flags and if we opened the file for writing, write it
 * back to the server.
 */

int
cm_close (FCacheEntry *entry, int flag,
	  AFSStoreStatus *status, CredCacheEntry* ce)
{
    int error = 0;

    if (flag & NNPFS_WRITE) {
	if (flag & NNPFS_FSYNC)
	    status->Mask |= SS_FSYNC;

	error = write_data (entry, status, ce);

	if (error) {
	    arla_warn (ADEBCM, error, "writing back file");
	    return error;
	}
    }

    log_operation ("close (%ld,%lu,%lu,%lu) %d\n",
		   entry->fid.Cell,
		   entry->fid.fid.Volume,
		   entry->fid.fid.Vnode,
		   entry->fid.fid.Unique,
		   flag);

    cm_check_consistency();

    return error;
}

/*
 * getattr - read the attributes from this file.
 */

int
cm_getattr (FCacheEntry *entry,
	    CredCacheEntry *ce,
	    AccessEntry **ae)
{
     int error = 0;

     arla_warnx (ADEBCM, "cm_getattr");

     assert (ae);

     AssertExclLocked(&entry->lock);

     error = fcache_verify_attr (entry, NULL, NULL, ce);
     if (error)
	 return error;

     arla_warnx (ADEBCM, "cm_getattr: done get attr");

     if (checkright (entry,
		     entry->status.FileType == TYPE_FILE ? AREAD : 0,
		     ce)) {
	 entry->flags.attrusedp = TRUE;
	 entry->flags.kernelp   = TRUE;
	 *ae = entry->acccache;
	 
	 log_operation ("getattr (%ld,%lu,%lu,%lu)\n",
			entry->fid.Cell,
			entry->fid.fid.Volume,
			entry->fid.fid.Vnode,
			entry->fid.fid.Unique);
	 
     } else {
	 *ae = NULL;
	 error = EACCES;
     }
     if (!entry->flags.datausedp)
	 entry->tokens &= ~(NNPFS_DATA_MASK | NNPFS_OPEN_MASK);
     
     arla_warnx (ADEBCM, "cm_getattr: return: %d", error);
     cm_check_consistency();

     return error;
}

/*
 * setattr - set the attributes of this file. These are immediately
 * sent to the FS.
 */

int
cm_setattr (FCacheEntry *entry, AFSStoreStatus *attr, CredCacheEntry* ce)
{
     int error = 0;

     error = fcache_verify_attr (entry, NULL, NULL, ce);
     if (error)
	 return error;

     if (checkright (entry, AWRITE, ce)) {
	  arla_warnx (ADEBCM, "cm_setattr: Writing status");
	  error = write_attr (entry, attr, ce);

	  log_operation ("setattr (%ld,%lu,%lu,%lu)\n",
			 entry->fid.Cell,
			 entry->fid.fid.Volume,
			 entry->fid.fid.Vnode,
			 entry->fid.fid.Unique);
     } else
	 error = EACCES;

     cm_check_consistency();
     return error;
}

/*
 * ftruncate - make the specified file have a specified size
 */

int
cm_ftruncate (FCacheEntry *entry, off_t size,
	      AFSStoreStatus *storestatus, CredCacheEntry* ce)
{
     int error = 0;

     error = fcache_verify_attr (entry, NULL, NULL, ce);
     if (error)
	 return error;

     if (size) {
	 error = fcache_verify_data (entry, ce);
	 if (error)
	     return error;
     }

     if (checkright (entry, AWRITE, ce)) {
	  error = truncate_file (entry, size, storestatus, ce);

	  log_operation ("ftruncate (%ld,%lu,%lu,%lu) %lu\n",
			 entry->fid.Cell,
			 entry->fid.fid.Volume,
			 entry->fid.fid.Vnode,
			 entry->fid.fid.Unique,
			 (unsigned long)size);
     } else
	 error = EACCES;

     cm_check_consistency();
     return error;
}

/*
 * access - check if user is allowed to perform operation.
 */

int
cm_access (FCacheEntry *entry, int mode, CredCacheEntry* ce)
{
     int error = 0;

     error = fcache_verify_attr (entry, NULL, NULL, ce);
     if (error)
	 return error;

     if (checkright (entry, AWRITE, ce))
	 error = 0;
     else
	 error = EACCES;

     log_operation ("access (%ld,%lu,%lu,%lu)\n",
		    entry->fid.Cell,
		    entry->fid.fid.Volume,
		    entry->fid.fid.Vnode,
		    entry->fid.fid.Unique);

     cm_check_consistency();
     return error;
}

/*
 * Expand `src' into `dest' (of size `dst_sz'), expanding `str' to
 * `replacement'. Return number of characters written to `dest'
 * (excluding terminating zero) or `dst_sz' if there's not enough
 * room.
 */

static int
expand_sys (char *dest, size_t dst_sz, const char *src,
	    const char *str, const char *rep)
{
    char *destp = dest;
    const char *srcp = src;
    char *s;
    int n = 0;
    int len;
    size_t str_len = strlen(str);
    size_t rep_len = strlen(rep);
    size_t src_len = strlen(src);
    
    while ((s = strstr (srcp, str)) != NULL) {
	len = s - srcp;

	if (dst_sz <= n + len + rep_len)
	    return dst_sz;

	memcpy (destp, srcp, len);
	memcpy (destp + len, rep, rep_len);
	n += len + rep_len;
	destp += len + rep_len;
	srcp = s + str_len;
    }
    len = src_len - (srcp - src);
    if (dst_sz <= n + len)
	return dst_sz;
    memcpy (destp, srcp, len);
    n += len;
    destp[len] = '\0';
    return n;
}

/*
 * Find this entry in the directory. If the entry happens to point to
 * a mount point, then we follow that and return the root directory of
 * the volume. Hopefully this is the only place where we need to think
 * about mount points (which are followed iff follow_mount_point).
 */

int
cm_lookup (FCacheEntry **entry,
	   const char *name,
	   VenusFid *res,
	   CredCacheEntry** ce,
	   int follow_mount_point)
{
     char tmp_name[MAXPATHLEN];
     int error = 0;

     error = fcache_get_data(entry, ce, 0);
     if (error)
	 return error;

     if (strstr (name, "@sys") != NULL) {
	 int i;

	 for (i = 0; i < sysnamenum; i++) {
	     int size = expand_sys (tmp_name, sizeof(tmp_name), name,
				    "@sys", sysnamelist[i]);
	     if (size >= sizeof(tmp_name))
		 continue;
	     error = adir_lookup (*entry, tmp_name, res);
	     if (error == 0)
		 break;
	 }
	 if (i == sysnamenum)
	     error = ENOENT;

     } else
	 error = adir_lookup (*entry, name, res);

     if (error) 
	 return error;

     /* 
      * The ".." at the top of a volume just points to the volume root,
      * so get the real ".." from the volume cache instead.
      *
      * Or if we are looking up "." we don't want to follow the
      * mountpoint
      */

     if (strcmp(".", name) == 0) {

	 error = fcache_verify_attr (*entry, NULL, NULL, *ce);
	 if (error)
	     goto out;

	 *res = (*entry)->fid;
     } else if (strcmp("..", name) == 0
		&& VenusFid_cmp(&(*entry)->fid, res) == 0) {

	 error = fcache_verify_attr (*entry, NULL, NULL, *ce);
	 if (error)
	     goto out;

	 *res = (*entry)->volume->parent_fid; /* entry->parent */
     } else if (follow_mount_point) {
	 error = followmountpoint (res, &(*entry)->fid, *entry, ce);
	 if (error)
	     goto out;
     }
out:
     log_operation ("lookup (%ld,%lu,%lu,%lu) %s\n",
		    (*entry)->fid.Cell,
		    (*entry)->fid.fid.Volume,
		    (*entry)->fid.fid.Vnode,
		    (*entry)->fid.fid.Unique,
		    name);

     cm_check_consistency();
     return error;
}

/*
 * Create this file and more.
 */

int
cm_create (FCacheEntry **dir, const char *name, AFSStoreStatus *store_attr,
	   VenusFid *res, AFSFetchStatus *fetch_attr,
	   CredCacheEntry **ce)
{
     int error = 0;

     error = fcache_get_data (dir, ce, 0);
     if (error)
	 return error;

     if (checkright (*dir, AINSERT, *ce)) {
	 error = create_file (*dir, name, store_attr,
			      res, fetch_attr, *ce);
	 if (error == 0)
	     error = adir_creat (*dir, name, res->fid);
     } else
	 error = EACCES;

     log_operation ("create (%ld,%lu,%lu,%lu) %s\n",
		    (*dir)->fid.Cell,
		    (*dir)->fid.fid.Volume,
		    (*dir)->fid.fid.Vnode,
		    (*dir)->fid.fid.Unique,
		    name);

     cm_check_consistency();
     return error;
}

/*
 * Create a new directory
 */

int
cm_mkdir (FCacheEntry **dir, const char *name,
	  AFSStoreStatus *store_attr,
	  VenusFid *res, AFSFetchStatus *fetch_attr,
	  CredCacheEntry **ce)
{
     int error = 0;

     error = fcache_get_data (dir, ce, 0);
     if (error)
	 return error;

     if (checkright (*dir, AINSERT, *ce)) {
	 error = create_directory (*dir, name, store_attr,
				   res, fetch_attr, *ce);
	 if (error == 0)
	     error = adir_creat (*dir, name, res->fid);
	 
     } else
	 error = EACCES;

     log_operation ("mkdir (%ld,%lu,%lu,%lu) %s\n",
		    (*dir)->fid.Cell,
		    (*dir)->fid.fid.Volume,
		    (*dir)->fid.fid.Vnode,
		    (*dir)->fid.fid.Unique,
	      name);

     cm_check_consistency();
     return error;
}

/*
 * Create a symlink
 */

int
cm_symlink (FCacheEntry **dir,
	    const char *name, AFSStoreStatus *store_attr,
	    VenusFid *res, VenusFid *realfid,
	    AFSFetchStatus *fetch_attr,
	    const char *contents,
	    CredCacheEntry **ce)
{
     FCacheEntry *symlink_entry;
     int error = 0;

     error = fcache_get_data (dir, ce, 0);
     if (error)
	 return error;

     if (!checkright (*dir, AINSERT, *ce)) {
	 error = EACCES;
	 goto out;
     }

     /* It seems Transarc insists on mount points having mode bits 0644 */

     if (contents[0] == '%' || contents[0] == '#') {
	 store_attr->UnixModeBits = 0644;
	 store_attr->Mask |= SS_MODEBITS;
     } else if (store_attr->Mask & SS_MODEBITS
		&& store_attr->UnixModeBits == 0644)
	 store_attr->UnixModeBits = 0755;

     error = create_symlink (*dir, name, store_attr,
			     res, fetch_attr,
			     contents, *ce);
     if (error)
	 goto out;

     error = adir_creat (*dir, name, res->fid);
     if (error)
	 goto out;

     error = followmountpoint(res, &(*dir)->fid, NULL, ce);
     if (error)
	 goto out;
     
     /*
      * If the new symlink is a mountpoint and it points
      * to dir_fid we will deadlock if we look it up.
      */

     if (VenusFid_cmp (res, &(*dir)->fid) != 0) {

	 error = fcache_get (&symlink_entry, *res, *ce);
	 if (error)
	     goto out;
	 
	 error = fcache_verify_attr (symlink_entry, *dir, name, *ce);
	 if (error) {
	     fcache_release (symlink_entry);
	     goto out;
	 }
	 
	 symlink_entry->flags.kernelp = TRUE;

	 *fetch_attr = symlink_entry->status;
	 *realfid = *fcache_realfid (symlink_entry);

	 fcache_release (symlink_entry);
     } else {
	 *fetch_attr = (*dir)->status;
	 *realfid = *fcache_realfid (*dir);
     }
     
     log_operation ("symlink (%ld,%lu,%lu,%lu) %s %s\n",
		    (*dir)->fid.Cell,
		    (*dir)->fid.fid.Volume,
		    (*dir)->fid.fid.Vnode,
		    (*dir)->fid.fid.Unique,
		    name,
		    contents);
     
 out:
     cm_check_consistency();
     return error;
}

/*
 * Create a hard link.
 */

int
cm_link (FCacheEntry **dir,
	 const char *name,
	 VenusFid existing_fid,
	 AFSFetchStatus *existing_status,
	 CredCacheEntry **ce)
{
     FCacheEntry *file;
     int error = 0;

     error = fcache_get_data (dir, ce, 0);
     if (error)
	 return error;

     error = fcache_get (&file, existing_fid, *ce);
     if (error)
	 return error;

     error = fcache_verify_attr (file, *dir, NULL, *ce);
     if (error)
	 goto out;

     if (checkright (*dir, AINSERT, *ce)) {
	 error = create_link (*dir, name, file, *ce);
	 if (error == 0) {
	     error = adir_creat (*dir, name, existing_fid.fid);
	     if (error == 0)
		 *existing_status = file->status;
	 }
     } else 
	 error = EACCES;

     log_operation ("link (%ld,%lu,%lu,%lu) (%ld,%lu,%lu,%lu) %s\n",
		    (*dir)->fid.Cell,
		    (*dir)->fid.fid.Volume,
		    (*dir)->fid.fid.Vnode,
		    (*dir)->fid.fid.Unique,
		    existing_fid.Cell,
		    existing_fid.fid.Volume,
		    existing_fid.fid.Vnode,
		    existing_fid.fid.Unique,
		    name);

out:
     fcache_release(file);
     cm_check_consistency();
     return error;
}

/*
 * generic function for both remove and rmdir
 */

static int
sub_remove (FCacheEntry **dir, const char *name, CredCacheEntry **ce,
	    const char *operation,
	    int (*func)(FCacheEntry *fe,
			const char *name,
			CredCacheEntry *ce))
{
     int error = 0;

     error = fcache_get_data (dir, ce, 0);
     if (error)
	 return error;

     if (checkright (*dir, ADELETE, *ce)) {
	 error = (*func) (*dir, name, *ce);
	 if (error == 0)
	     error = adir_remove (*dir, name);
     } else 
	 error = EACCES;
     
     log_operation ("%s (%ld,%lu,%lu,%lu) %s\n",
		    operation,
		    (*dir)->fid.Cell,
		    (*dir)->fid.fid.Volume,
		    (*dir)->fid.fid.Vnode,
		    (*dir)->fid.fid.Unique,
		    name);

     cm_check_consistency();
     return error;
}

/*
 * Remove the file named `name' in the directory `dir'.
 */

int
cm_remove(FCacheEntry **dir,
	  const char *name, CredCacheEntry **ce)
{
    return sub_remove (dir, name, ce, "remove", remove_file);
}

/*
 * Remove the directory named `name' in the directory `dir'.
 */

int
cm_rmdir(FCacheEntry **dir,
	 const char *name, CredCacheEntry **ce)
{
    return sub_remove (dir, name, ce, "rmdir", remove_directory);
}

/*
 * Called when the object is being moved to a new directory, to be
 * able to update .. when required.
 */

static int
potential_update_dir(FCacheEntry *child_entry,
		     const VenusFid *new_parent_fid,
		     FCacheEntry *parent_entry,
		     int *update_child,
		     CredCacheEntry **ce)
{
    int error;

    error = fcache_verify_attr (child_entry, parent_entry, NULL, *ce);
    if (error) 
	return error;

    /*
     * if we're moving a directory.
     */

    if (child_entry->status.FileType == TYPE_DIR) {
	int fd;
	fbuf the_fbuf;

	error = fcache_get_data(&child_entry, ce, 0); /* XXX - check fake_mp */
	if (error)
	    return error;

	error = fcache_get_fbuf (child_entry, &fd, &the_fbuf, O_RDWR,
				 FBUF_READ|FBUF_WRITE|FBUF_SHARED);
	if (error)
	    return error;

	error = fdir_changefid (&the_fbuf, "..", new_parent_fid);
	fbuf_end (&the_fbuf);
	close (fd);
	if (error)
	    return error;

	*update_child = 1;
    }
    return 0;
}

/*
 * Rename (old_parent_fid, old_name) -> (new_parent_fid, new_name)
 * update the `child' in the new directory if update_child.
 * set child_fid to the fid of the moved object.
 */

int
cm_rename(FCacheEntry **old_dir, const char *old_name,
	  FCacheEntry **new_dir, const char *new_name,
	  VenusFid *child_fid,
	  int *update_child,
	  CredCacheEntry **ce)
{
    int error = 0;
    VenusFid new_fid, old_fid;
    
    *update_child = 0;

    /* old parent dir */

    error = fcache_get_data (old_dir, ce, 0);
    if (error)
	return error;

    /* new parent dir */

    error = fcache_get_data (new_dir, ce, 0);
    if (error)
	return error;

    if (!checkright (*old_dir, ADELETE, *ce)
	|| !checkright (*new_dir, AINSERT, *ce)) {
	error = EACCES;
	goto out;
    }
	
    error = rename_file (*old_dir, old_name, *new_dir, new_name, *ce);
    if (error)
	goto out;

    /*
     * Lookup the old name (to get the fid of the new name)
     */
    
    error = adir_lookup (*old_dir, old_name, &new_fid);
    
    if (error)
	goto out;
    
    *child_fid = new_fid;
    
    if (VenusFid_cmp (&(*old_dir)->fid, &(*new_dir)->fid)) {
	FCacheEntry *child_entry;
	
	error = fcache_get (&child_entry, *child_fid, *ce);
	if (error)
	    goto out;
	
	child_entry->parent = (*new_dir)->fid;
	
	error = potential_update_dir (child_entry, &(*new_dir)->fid,
				      *new_dir, update_child, ce);
	fcache_release (child_entry);
	if (error)
	    goto out;
    }
    
    /*
     * Lookup the new name, if it exists we need to silly
     * rename it was just killed on the fileserver.
     * XXXDISCO remember mark this node as dead
     */

    error = adir_lookup (*new_dir, new_name, &old_fid);
    if (error == 0) {
	FCacheEntry *old_entry;

	error = fcache_find (&old_entry, old_fid);
	if (error == 0) {
	    old_entry->flags.silly = TRUE;
	    fcache_release (old_entry);
	}
	adir_remove (*new_dir, new_name);
    }
    
    /*
     * Now do the rename, ie create the new name and remove
     * the old name.
     */
    
    error = adir_creat (*new_dir, new_name,  new_fid.fid)
	|| adir_remove (*old_dir, old_name);
    
    log_operation ("rename (%ld,%lu,%lu,%lu) (%ld,%lu,%lu,%lu) %s %s\n",
		   (*old_dir)->fid.Cell,
		   (*old_dir)->fid.fid.Volume,
		   (*old_dir)->fid.fid.Vnode,
		   (*old_dir)->fid.fid.Unique,
		   (*new_dir)->fid.Cell,
		   (*new_dir)->fid.fid.Volume,
		   (*new_dir)->fid.fid.Vnode,
		   (*new_dir)->fid.fid.Unique,
		   old_name, new_name);
    
 out:
    cm_check_consistency();
    return error;
}

/* 
 * An emulation of kernel lookup, convert (fid, name) into
 * (res).  Strips away leading /afs, removes double slashes,
 * and resolves symlinks.
 * Return 0 for success, otherwise -1.
 */

int
cm_walk (VenusFid fid,
	 const char *name,
	 VenusFid *res)
{
    VenusFid cwd = fid;
    char *base;
    VenusFid file;
    FCacheEntry *entry;
    FCacheEntry *dentry;
    int error;
    char symlink[MAXPATHLEN];
    char store_name[MAXPATHLEN];
    char *fname;
    CredCacheEntry *ce;

    ce = cred_get (fid.Cell, getuid(), CRED_ANY);
    
    strlcpy(store_name, name, sizeof(store_name));
    fname = store_name;
    
    do {
        /* set things up so that fname points to the remainder of the path,
         * whereas base points to the whatever precedes the first /
         */
        base = fname;
        fname = strchr(fname, '/');
        if (fname) {
            /* deal with repeated adjacent / chars by eliminating the
             * duplicates. 
             */
            while (*fname == '/') {
                *fname = '\0';
                fname++;
            }
        }
	
        /* deal with absolute pathnames first. */
        if (*base == '\0') {
	    error = getroot(&cwd, ce);
	    if (error) {
		arla_warn(ADEBWARN, error, "getroot");
		cred_free(ce);
		return -1;
	    }
	    
	    if (fname) {
		if (strncmp("afs",fname,3) == 0) {
		    fname += 3;
		    }
		continue;
	    } else {
		break;
	    }
	}
	error = fcache_get(&dentry, cwd, ce);
	if (error) {
	    arla_warn (ADEBWARN, error, "fcache_get");
	    cred_free(ce);
	    return -1;
	}
	error = cm_lookup (&dentry, base, &file, &ce, TRUE);
	if (error) {
	    fcache_release(dentry);
	    arla_warn (ADEBWARN, error, "lookup(%s)", base);
	    cred_free(ce);
	    return -1;
	}
	fcache_release(dentry);
	error = fcache_get(&entry, file, ce);
	if (error) {
	    arla_warn (ADEBWARN, error, "fcache_get");
	    cred_free(ce);
	    return -1;
	}
	
	error = fcache_get_data (&entry, &ce, 0);
	if (error) {
	    fcache_release(entry);
	    arla_warn (ADEBWARN, error, "fcache_get_data");
	    cred_free(ce);
	    return -1;
	}
	
	/* handle symlinks here */
	if (entry->status.FileType == TYPE_LINK) {
	    int len;
	    int fd;
	    
	    fd = fcache_open_file (entry, O_RDONLY);
	    /* read the symlink and null-terminate it */
	    if (fd < 0) {
		fcache_release(entry);
		arla_warn (ADEBWARN, errno, "fcache_open_file");
		cred_free(ce);
		return -1;
	    }
	    len = read (fd, symlink, sizeof(symlink));
	    close (fd);
	    if (len <= 0) {
		fcache_release(entry);
		arla_warnx (ADEBWARN, "cannot read symlink");
		cred_free(ce);
		return -1;
	    }
	    symlink[len] = '\0';
	    /* if we're not at the end (i.e. fname is not null), take
	     * the expansion of the symlink and append fname to it.
	     */
	    if (fname != NULL) {
		    
		strlcat (symlink, "/", sizeof(symlink));
		strlcat (symlink, fname, sizeof(symlink));
	    }
	    strlcpy(store_name, symlink, sizeof(store_name));
	    fname = store_name;
	} else {
	    /* if not a symlink, just update cwd */
	    cwd = entry->fid;
	}
	fcache_release(entry);
	
	/* the *fname condition below deals with a trailing / in a
	 * path-name */
    } while (fname != NULL && *fname);
    *res = cwd;
    cred_free(ce);
    return 0;
}
