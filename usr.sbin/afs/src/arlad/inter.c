/*
 * Copyright (c) 1995 - 2000 Kungliga Tekniska Högskolan
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
RCSID("$KTH: inter.c,v 1.110.2.3 2001/06/05 01:27:05 ahltorp Exp $") ;

#include <xfs/xfs_message.h>

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
#ifdef USE_MMAPTIME
    mmaptime_gettimeofday (&now, NULL);
#else
    gettimeofday (&now, NULL);
#endif
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
    u_long calc_size;
    u_long real_size;
    char newname[MAXPATHLEN];

    if (cm_consistencyp == FALSE)
	return;
    
    calc_size = fcache_calculate_usage();
    real_size = fcache_usedbytes ();

    if (calc_size != real_size) {
	    log_operation ("consistency check not guaranteed "
			   "(calc: %d, real: %d), aborting\n", 
			   (int) calc_size, (int) real_size);
	    cm_store_state ();
	    abort();
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
 * This means that the file should be in the local cache and that the
 * cache manager should recall that some process has this file opened
 * for reading and/or writing.
 * We do no checking here. Should we?
 */

Result
cm_open (VenusFid *fid,
	 CredCacheEntry **ce,
	 u_int tokens,
	 fcache_cache_handle *cache_handle,
	 char *cache_name,
	 size_t cache_name_sz)
{
     FCacheEntry *entry;
     Result ret;
     u_long mask;
     int error;

     error = fcache_get_data (&entry, fid, ce);
     if (error) {
	 ret.res = -1;
	 ret.error = error;
	 return ret;
     }

     switch(tokens) {
     case XFS_DATA_R:
       /*     case XFS_OPEN_NR:
     case XFS_OPEN_SR: */
	  mask = AREAD;
	  break;
     case XFS_DATA_W:
	  mask = AWRITE;
	  break;
     case XFS_OPEN_NW:
	  mask = AREAD | AWRITE;
	  break;
     default:
	 arla_warnx (ADEBCM, "cm_open(): unknown token: %d, assuming AREAD",
		     tokens);
	 mask = AREAD;
/*	  assert(FALSE); */
     }

     if (checkright (entry, mask, *ce)) {
#if 0
	  assert(entry->flags.attrusedp);
#endif
	  entry->flags.datausedp = TRUE;
	  entry->tokens |= tokens;
	  ret.res    = 0;
	  ret.error  = 0;
	  ret.tokens = entry->tokens;
	  
	  *cache_handle = entry->handle;
	  fcache_file_name (entry, cache_name, cache_name_sz);

	  log_operation ("open (%ld,%lu,%lu,%lu) %u\n",
			 fid->Cell,
			 fid->fid.Volume, fid->fid.Vnode, fid->fid.Unique,
			 mask);
     } else {
	  ret.res = -1;
	  ret.error = EACCES;
     }
     fcache_release(entry);

     cm_check_consistency();
 
     return ret;
}

/*
 * close. Set flags and if we opened the file for writing, write it
 * back to the server.
 */

Result
cm_close (VenusFid fid, int flag, AFSStoreStatus *status, CredCacheEntry* ce)
{
    FCacheEntry *entry;
    Result ret;
    int error;

    error = fcache_get (&entry, fid, ce);
    if (error) {
	ret.res   = -1;
	ret.error = error;
	return ret;
    }

    if (flag & XFS_WRITE) {
	if (flag & XFS_FSYNC)
	    status->Mask |= SS_FSYNC;

	error = write_data (entry, status, ce);

	if (error) {
	    fcache_release(entry);
	    arla_warn (ADEBCM, error, "writing back file");
	    ret.res   = -1;
	    ret.error = error;
	    return ret;
	}
    }

    log_operation ("close (%ld,%lu,%lu,%lu) %d\n",
	     fid.Cell, fid.fid.Volume, fid.fid.Vnode, fid.fid.Unique,
	     flag);
    fcache_release(entry);
    ret.res   = 0;
    ret.error = 0;

    cm_check_consistency();

    return ret;
}

/*
 * getattr - read the attributes from this file.
 */

Result
cm_getattr (VenusFid fid,
	    AFSFetchStatus *attr,
	    VenusFid *realfid,
	    CredCacheEntry *ce,
	    AccessEntry **ae)
{
     FCacheEntry *entry;
     Result ret;
     int error;

     arla_warnx (ADEBCM, "cm_getattr");

     assert (ae);

     error = fcache_get (&entry, fid, ce);
     if (error) {
	 ret.res   = -1;
	 ret.error = error;
	 return ret;
     }
     
     AssertExclLocked(&entry->lock);

     error = fcache_verify_attr (entry, NULL, NULL, ce);
     if (error) {
	 fcache_release(entry);
	 ret.res   = -1;
	 ret.error = error;
	 return ret;
     }

     arla_warnx (ADEBCM, "cm_getattr: done get attr");

     if (checkright (entry,
		     entry->status.FileType == TYPE_FILE ? AREAD : 0,
		     ce)) {
	 *attr = entry->status;
	 ret.res   = 0;
	 ret.error = 0;
	 entry->flags.attrusedp = TRUE;
	 entry->flags.kernelp   = TRUE;
	 *ae = entry->acccache;
	 
	 log_operation ("getattr (%ld,%lu,%lu,%lu)\n",
			fid.Cell, fid.fid.Volume, fid.fid.Vnode, fid.fid.Unique);
	 
     } else {
	 *ae = NULL;
	 ret.res   = -1;
	 ret.error = EACCES;
     }
     *realfid = *fcache_realfid (entry);
     ret.tokens = entry->tokens;
     if (!entry->flags.datausedp)
	 ret.tokens &= ~(XFS_DATA_MASK | XFS_OPEN_MASK);
     fcache_release(entry);
     
     arla_warnx (ADEBCM, "cm_getattr: return: %d.%d", ret.res, ret.error);
     cm_check_consistency();

     AssertNotExclLocked(&entry->lock);

     return ret;
}

/*
 * setattr - set the attributes of this file. These are immediately
 * sent to the FS.
 */

Result
cm_setattr (VenusFid fid, AFSStoreStatus *attr, CredCacheEntry* ce)
{
     FCacheEntry *entry;
     Result ret;
     int error;

     error = fcache_get (&entry, fid, ce);
     if (error) {
	 ret.res   = -1;
	 ret.error = error;
	 return ret;
     }

     error = fcache_verify_attr (entry, NULL, NULL, ce);
     if (error) {
	 fcache_release(entry);
	 ret.res   = -1;
	 ret.error = error;
	 return ret;
     }
     if (checkright (entry, AWRITE, ce)) {
	  arla_warnx (ADEBCM, "cm_setattr: Writing status");
	  ret.res   = write_attr (entry, attr, ce);
	  ret.error = ret.res;

	  log_operation ("setattr (%ld,%lu,%lu,%lu)\n",
		   fid.Cell, fid.fid.Volume, fid.fid.Vnode, fid.fid.Unique);

     } else {
	  ret.res   = -1;
	  ret.error = EACCES;
     }
     fcache_release(entry);
     cm_check_consistency();
     return ret;
}

/*
 * ftruncate - make the specified file have a specified size
 */

Result
cm_ftruncate (VenusFid fid, off_t size, CredCacheEntry* ce)
{
     FCacheEntry *entry;
     Result ret;
     int error;

     error = fcache_get (&entry, fid, ce);
     if (error) {
	 ret.res   = -1;
	 ret.error = error;
	 return ret;
     }

     error = fcache_verify_attr (entry, NULL, NULL, ce);
     if (error) {
	 fcache_release(entry);
	 ret.res   = -1;
	 ret.error = error;
	 return ret;
     }

     if (size) {
	 error = fcache_verify_data (entry, ce);
	 if (error) {
	     fcache_release (entry);
	     ret.res   = -1;
	     ret.error = error;
	     return ret;
	 }
     }

     if (checkright (entry, AWRITE, ce)) {
	  ret.res   = truncate_file (entry, size, ce);
	  ret.error = ret.res;

	  log_operation ("ftruncate (%ld,%lu,%lu,%lu) %lu\n",
		   fid.Cell, fid.fid.Volume, fid.fid.Vnode, fid.fid.Unique,
		   (unsigned long)size);
     } else {
	  ret.res   = -1;
	  ret.error = EACCES;
     }
     fcache_release(entry);
     cm_check_consistency();
     return ret;
}

/*
 * access - check if user is allowed to perform operation.
 */

Result
cm_access (VenusFid fid, int mode, CredCacheEntry* ce)
{
     FCacheEntry *entry;
     Result ret;
     int error;

     error = fcache_get (&entry, fid, ce);
     if (error) {
	 ret.res   = -1;
	 ret.error = error;
	 return ret;
     }

     error = fcache_verify_attr (entry, NULL, NULL, ce);
     if (error) {
	 fcache_release(entry);
	 ret.res   = -1;
	 ret.error = error;
	 return ret;
     }

     if (checkright (entry, AWRITE, ce)) {
	  ret.res   = 0;		/**/
	  ret.error = 0;
     } else {
	  ret.res   = -1;
	  ret.error = EACCES;
     }

     log_operation ("access (%ld,%lu,%lu,%lu)\n",
	      fid.Cell, fid.fid.Volume, fid.fid.Vnode, fid.fid.Unique);

     fcache_release(entry);
     cm_check_consistency();
     return ret;
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

Result
cm_lookup (VenusFid *dir_fid,
	   const char *name,
	   VenusFid *res,
	   CredCacheEntry** ce,
	   int follow_mount_point)
{
     char tmp_name[MAXPATHLEN];
     FCacheEntry *entry;
     Result ret;
     int error;

     if (strstr (name, "@sys") != NULL) {
	 if (expand_sys (tmp_name, sizeof(tmp_name), name,
			 "@sys", arlasysname) >= sizeof(tmp_name)) {
	     ret.res   = -1;
	     ret.error = ENAMETOOLONG;
	     return ret;
	 }
	 name = tmp_name;
     }

     error = fcache_get_data(&entry, dir_fid, ce);
     if (error) {
	 ret.res   = -1;
	 ret.error = error;
	 return ret;
     }

     error = adir_lookup (entry, name, res);
     if (error) {
	 fcache_release(entry);
	 ret.res   = -1;
	 ret.error = error;
	 return ret;
     }

     ret.res    = 0;
     ret.error  = 0; 
     ret.tokens = 0;

     /* 
      * The ".." at the top of a volume just points to the volume root,
      * so get the real ".." from the volume cache instead.
      *
      * Or if we are looking up "." we don't want to follow the
      * mountpoint
      */

     if (strcmp(".", name) == 0) {

	 error = fcache_verify_attr (entry, NULL, NULL, *ce);
	 if (error) {
	     ret.res   = -1;
	     ret.error = error;
	     goto out;
	 }

	 *res = *dir_fid;
	 ret.tokens = entry->tokens;
     } else if ((strcmp("..", name) == 0 && VenusFid_cmp(dir_fid, res) == 0)) {

	 error = fcache_verify_attr (entry, NULL, NULL, *ce);
	 if (error) {
	     ret.res   = -1;
	     ret.error = error;
	     goto out;
	 }

	 *res = entry->volume->parent_fid; /* entry->parent */
	 ret.tokens = entry->tokens;
     } else if (follow_mount_point) {
	 error = followmountpoint (res, dir_fid, entry, ce);
	 if (error) {
	     ret.res   = -1;
	     ret.error = error;
	     goto out;
	 }
     }
out:
     fcache_release(entry);

     log_operation ("lookup (%ld,%lu,%lu,%lu) %s\n",
		    dir_fid->Cell,
		    dir_fid->fid.Volume,
		    dir_fid->fid.Vnode,
		    dir_fid->fid.Unique,
		    name);

     cm_check_consistency();
     return ret;
}

/*
 * Create this file and more.
 */

Result
cm_create (VenusFid *dir_fid, const char *name, AFSStoreStatus *store_attr,
	   VenusFid *res, AFSFetchStatus *fetch_attr,
	   CredCacheEntry **ce)
{
     FCacheEntry *dire;
     Result ret;
     int error;

     error = fcache_get_data (&dire, dir_fid, ce);
     if (error) {
	 ret.res   = -1;
	 ret.error = error;
	 return ret;
     }

     if (checkright (dire, AINSERT, *ce)) {
	  error = create_file (dire, name, store_attr,
			       res, fetch_attr, *ce);
	  if (error) {
	      ret.res   = -1;
	      ret.error = error;
	  } else {
	      error = adir_creat (dire, name, res->fid);
	      if (error) {
		  ret.res   = -1;
		  ret.error = error;
	      } else {
		  ret.res   = 0;
		  ret.error = 0;
	      }
	  }
     } else {
	  ret.res   = -1;
	  ret.error = EACCES;
     }
     log_operation ("create (%ld,%lu,%lu,%lu) %s\n",
	      dir_fid->Cell,
	      dir_fid->fid.Volume, dir_fid->fid.Vnode, dir_fid->fid.Unique,
	      name);

     fcache_release(dire);
     cm_check_consistency();
     return ret;
}

/*
 * Create a new directory
 */

Result
cm_mkdir (VenusFid *dir_fid, const char *name,
	  AFSStoreStatus *store_attr,
	  VenusFid *res, AFSFetchStatus *fetch_attr,
	  CredCacheEntry **ce)
{
     FCacheEntry *dire;
     Result ret;
     int error;

     error = fcache_get_data (&dire, dir_fid, ce);
     if (error) {
	 ret.res   = -1;
	 ret.error = error;
	 return ret;
     }

     if (checkright (dire, AINSERT, *ce)) {
	  error = create_directory (dire, name, store_attr,
				    res, fetch_attr, *ce);
	  if (error) {
	      ret.res   = -1;
	      ret.error = error;
	  } else {
	      error = adir_creat (dire, name, res->fid);
	      if (error) {
		  ret.res   = -1;
		  ret.error = error;
	      } else {
		  ret.res   = 0;
		  ret.error = 0;
	      }
	  }
     } else {
	  ret.res   = -1;
	  ret.error = EACCES;
     }

     log_operation ("mkdir (%ld,%lu,%lu,%lu) %s\n",
		    dir_fid->Cell,
		    dir_fid->fid.Volume,
		    dir_fid->fid.Vnode,
		    dir_fid->fid.Unique,
	      name);

     ReleaseWriteLock (&dire->lock);
     cm_check_consistency();
     return ret;
}

/*
 * Create a symlink
 */

Result
cm_symlink (VenusFid *dir_fid,
	    const char *name, AFSStoreStatus *store_attr,
	    VenusFid *res, VenusFid *realfid,
	    AFSFetchStatus *fetch_attr,
	    const char *contents,
	    CredCacheEntry **ce)
{
     FCacheEntry *dire, *symlink_entry;
     Result ret;
     int error;

     error = fcache_get_data (&dire, dir_fid, ce);
     if (error) {
	 ret.res   = -1;
	 ret.error = error;
	 return ret;
     }

     if (!checkright (dire, AINSERT, *ce)) {
	 ret.res   = -1;
	 ret.error = EACCES;
	 goto out;
     }

     /* It seems Transarc insists on mount points having mode bits 0644 */

     if (contents[0] == '%' || contents[0] == '#') {
	 store_attr->UnixModeBits = 0644;
	 store_attr->Mask |= SS_MODEBITS;
     } else if (store_attr->Mask & SS_MODEBITS
		&& store_attr->UnixModeBits == 0644)
	 store_attr->UnixModeBits = 0755;

     error = create_symlink (dire, name, store_attr,
			     res, fetch_attr,
			     contents, *ce);
     if (error) {
	 ret.res   = -1;
	 ret.error = error;
	 goto out;
     }

     error = adir_creat (dire, name, res->fid);
     if (error) {
	 ret.res   = -1;
	 ret.error = error;
	 goto out;
     }

     error = followmountpoint(res, dir_fid, NULL, ce);
     if (error) {
	 ret.res   = -1;
	 ret.error = error;
	 goto out;
     }
     
     /*
      * If the new symlink is a mountpoint and it points
      * to dir_fid we will deadlock if we look it up.
      */

     if (VenusFid_cmp (res, dir_fid) != 0) {

	 error = fcache_get (&symlink_entry, *res, *ce);
	 if (error) {
	     ret.res   = -1;
	     ret.error = error;
	     goto out;
	 }
	 
	 error = fcache_verify_attr (symlink_entry, NULL, NULL, *ce);
	 if (error) {
	     fcache_release (symlink_entry);
	     ret.res   = -1;
	     ret.error = error;
	     goto out;
	 }
	 
	 symlink_entry->flags.kernelp = TRUE;

	 *fetch_attr = symlink_entry->status;
	 *realfid = *fcache_realfid (symlink_entry);

	 fcache_release (symlink_entry);
     } else {
	 *fetch_attr = dire->status;
	 *realfid = *fcache_realfid (dire);
     }
     
     ret.res   = 0;
     ret.error = 0;
     
     log_operation ("symlink (%ld,%lu,%lu,%lu) %s %s\n",
		    dir_fid->Cell,
		    dir_fid->fid.Volume,
		    dir_fid->fid.Vnode,
		    dir_fid->fid.Unique,
		    name,
		    contents);
     
 out:
     fcache_release(dire);
     cm_check_consistency();
     return ret;
}

/*
 * Create a hard link.
 */

Result
cm_link (VenusFid *dir_fid,
	 const char *name,
	 VenusFid existing_fid,
	 AFSFetchStatus *existing_status,
	 CredCacheEntry **ce)
{
     FCacheEntry *dire, *file;
     Result ret;
     int error;

     error = fcache_get_data (&dire, dir_fid, ce);
     if (error) {
	 ret.res   = -1;
	 ret.error = error;
	 return ret;
     }

     error = fcache_get (&file, existing_fid, *ce);
     if (error) {
	 fcache_release(dire);
	 ret.res   = -1;
	 ret.error = error;
	 return ret;
     }

     error = fcache_verify_attr (file, NULL, NULL, *ce);
     if (error) {
	 ret.res   = -1;
	 ret.error = error;
	 goto out;
     }

     if (checkright (dire, AINSERT, *ce)) {
	  error = create_link (dire, name, file, *ce);
	  if (error) {
	      ret.res   = -1;
	      ret.error = error;
	  } else {
	      error = adir_creat (dire, name, existing_fid.fid);
	      if (error) {
		  ret.res   = -1;
		  ret.error = error;
	      } else {
		  *existing_status = file->status;
		  ret.res   = 0;
		  ret.error = 0;
	      }
	  }
     } else {
	  ret.res   = -1;
	  ret.error = EACCES;
     }
     log_operation ("link (%ld,%lu,%lu,%lu) (%ld,%lu,%lu,%lu) %s\n",
	      dir_fid->Cell,
	      dir_fid->fid.Volume, dir_fid->fid.Vnode, dir_fid->fid.Unique,
	      existing_fid.Cell,
	      existing_fid.fid.Volume,
	      existing_fid.fid.Vnode,
	      existing_fid.fid.Unique,
	      name);

out:
     fcache_release(dire);
     fcache_release(file);
     cm_check_consistency();
     return ret;
}

/*
 * generic function for both remove and rmdir
 */

static Result
sub_remove (VenusFid *dir_fid, const char *name, CredCacheEntry **ce,
	    const char *operation,
	    int (*func)(FCacheEntry *fe,
			const char *name,
			CredCacheEntry *ce))
{
     FCacheEntry *dire;
     Result ret;
     int error;

     error = fcache_get_data (&dire, dir_fid, ce);
     if (error) {
	 ret.res   = -1;
	 ret.error = error;
	 return ret;
     }

     if (checkright (dire, ADELETE, *ce)) {
	  error = (*func) (dire, name, *ce);
	  if (error) {
	      ret.res   = -1;
	      ret.error = error;
	  } else {
	      error = adir_remove (dire, name);
	      if (error) {
		  ret.res   = -1;
		  ret.error = error;
	      } else {
		  ret.res   = 0;
		  ret.error = 0;
	      }
	  }
     } else {
	  ret.res   = -1;
	  ret.error = EACCES;
     }
     log_operation ("%s (%ld,%lu,%lu,%lu) %s\n",
		    operation,
		    dir_fid->Cell,
		    dir_fid->fid.Volume,
		    dir_fid->fid.Vnode,
		    dir_fid->fid.Unique,
		    name);

     fcache_release(dire);
     cm_check_consistency();
     return ret;
}

/*
 * Remove the file named `name' in the directory `dir_fid'.
 */

Result
cm_remove(VenusFid *dir_fid,
	  const char *name, CredCacheEntry **ce)
{
    return sub_remove (dir_fid, name, ce, "remove", remove_file);
}

/*
 * Remove the directory named `name' in the directory `dir_fid'.
 */

Result
cm_rmdir(VenusFid *dir_fid,
	 const char *name, CredCacheEntry **ce)
{
    return sub_remove (dir_fid, name, ce, "rmdir", remove_directory);
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
		     CredCacheEntry *ce)
{
    int error;

    error = fcache_verify_attr (child_entry, parent_entry, NULL, ce);
    if (error) 
	return error;

    /*
     * if we're moving a directory.
     */

    if (child_entry->status.FileType == TYPE_DIR) {
	int fd;
	fbuf the_fbuf;

	error = fcache_verify_data (child_entry, ce); /* XXX - check fake_mp */
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

Result
cm_rename(VenusFid *old_parent_fid, const char *old_name,
	  VenusFid *new_parent_fid, const char *new_name,
	  VenusFid *child_fid,
	  int *update_child,
	  CredCacheEntry **ce)
{
    FCacheEntry *old_dir;
    FCacheEntry *new_dir;
    Result ret;
    int error;
    int diff_dir;

    *update_child = 0;

    /* old parent dir */

    error = fcache_get_data (&old_dir, old_parent_fid, ce);
    if (error) {
	ret.res   = -1;
	ret.error = error;
	return ret;
    }

    diff_dir = VenusFid_cmp (old_parent_fid, new_parent_fid);

     /* new parent dir */

    if (diff_dir) {
	error = fcache_get_data (&new_dir, new_parent_fid, ce);
	if (error) {
	    fcache_release(old_dir);
	    ret.res   = -1;
	    ret.error = error;
	    return ret;
	}
    } else {
	new_dir = old_dir;
    }

    if (checkright (old_dir, ADELETE, *ce)
	&& checkright (new_dir, AINSERT, *ce)) {
	
	error = rename_file (old_dir, old_name, new_dir, new_name, *ce);
	if (error) {
	    ret.res   = -1;
	    ret.error = error;
	} else {
	    VenusFid new_fid, old_fid;
	    
	    /*
	     * Lookup the old name (to get the fid of the new name)
	     */
	    
	    error = adir_lookup (old_dir, old_name, &new_fid);
	    
	    if (error) {
		ret.res   = -1;
		ret.error = error;
		goto out;
	    }
	    
	    *child_fid = new_fid;
	    
	    if (diff_dir) {
		FCacheEntry *child_entry;
		
		error = fcache_get (&child_entry, *child_fid, *ce);
		if (error) {
		    ret.res   = -1;
		    ret.error = error;
		    goto out;
		}
		
		child_entry->parent = *new_parent_fid;

		error = potential_update_dir (child_entry, new_parent_fid,
					      new_dir, update_child, *ce);
		fcache_release (child_entry);
		if (error) {
		    ret.res   = -1;
		    ret.error = error;
		    goto out;
		}
	    }
	    
	    /*
	     * Lookup the new name, if it exists we need to clear it out.
	     * XXX Should we check the lnkcount and clear it from fcache ?
	     */
	    
	    error = adir_lookup (new_dir, new_name, &old_fid);
	    if (error == 0)
		adir_remove (new_dir, new_name);
	    
	    /*
	     * Now do the rename, ie create the new name and remove
	     * the old name.
	     */
	    
	    error = adir_creat (new_dir, new_name,  new_fid.fid)
		|| adir_remove (old_dir, old_name);
	    
	    if (error) {
		ret.res   = -1;
		ret.error = error;
	    } else {
		ret.res   = 0;
		ret.error = 0;
	    }
	}
    } else {
	ret.res   = -1;
	ret.error = EACCES;
    }
    
    log_operation ("rename (%ld,%lu,%lu,%lu) (%ld,%lu,%lu,%lu) %s %s\n",
		   old_parent_fid->Cell,
		   old_parent_fid->fid.Volume,
		   old_parent_fid->fid.Vnode,
		   old_parent_fid->fid.Unique,
		   new_parent_fid->Cell,
		   new_parent_fid->fid.Volume,
		   new_parent_fid->fid.Vnode,
		   new_parent_fid->fid.Unique,
		   old_name, new_name);
    
 out:
    fcache_release(old_dir);
    if (diff_dir)
	fcache_release(new_dir);
    cm_check_consistency();
    return ret;
}
