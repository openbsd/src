/*	$OpenBSD: inter.c,v 1.1.1.1 1998/09/14 21:52:56 art Exp $	*/
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
 * Interface to the cache manager.
 */

#include "arla_local.h"
RCSID("$KTH: inter.c,v 1.55 1998/07/29 21:31:59 assar Exp $") ;

#include <xfs/xfs_message.h>

/*
 * Return the rights for user cred and entry e.
 * If the rights are not existant fill in the entry.
 * The locking of e is up to the caller.
 */

u_long
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

static void
log_operation (const char *fmt, ...)
{
    va_list args;
    struct timeval now;

    if(connected_mode == CONNECTED)
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
cm_open (VenusFid fid, CredCacheEntry* ce, u_int tokens)
{
     FCacheEntry *entry;
     Result ret;
     u_long mask;
     int error;

     error = fcache_get (&entry, fid, ce);
     if (error) {
	 ret.res = -1;
	 ret.error = error;
	 return ret;
     }

     assert (CheckLock (&entry->lock) == -1);

     error = fcache_get_data (entry, ce);
     if(error) {
	 ReleaseWriteLock (&entry->lock);
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

     if (checkright (entry, mask, ce)) {
	  ret.res = entry->inode;
	  entry->flags.datausedp = TRUE;
	  entry->tokens |= tokens;

	  ret.res = entry->inode;
	  ret.tokens = entry->tokens;

	  log_operation ("open (%ld,%lu,%lu,%lu) %u\n",
		   fid.Cell, fid.fid.Volume, fid.fid.Vnode, fid.fid.Unique,
		   mask);
     } else {
	  ret.res = -1;
	  ret.error = EACCES;
     }
     ReleaseWriteLock (&entry->lock);

     return ret;
}

/*
 * close. Set flags and if we opened the file for writing, write it
 * back to the server.
 */

Result
cm_close (VenusFid fid, int flag, CredCacheEntry* ce)
{
    FCacheEntry *entry;
    Result ret;
    int error;

    error = fcache_get (&entry, fid, ce);
    if (error) {
	ret.res = -1;
	ret.error = error;
	return ret;
    }

    if (flag & XFS_WRITE) {
	error = write_data (entry, ce);

	if (error) {
	    ReleaseWriteLock (&entry->lock);
	    arla_warn (ADEBCM, error, "writing back file");
	    ret.res = -1 ;
	    ret.error = EPERM ;
	    return ret;
	}
    }

    log_operation ("close (%ld,%lu,%lu,%lu) %d\n",
	     fid.Cell, fid.fid.Volume, fid.fid.Vnode, fid.fid.Unique,
	     flag);

    ReleaseWriteLock (&entry->lock);
    ret.res = 0;
    return ret;
}

/*
 * getattr - read the attributes from this file.
 */

Result
cm_getattr (VenusFid fid,
	    AFSFetchStatus *attr,
	    VenusFid *realfid,
	    CredCacheEntry* ce,
	    AccessEntry **ae)
{
     FCacheEntry *entry;
     Result ret;
     int error;

     error = fcache_get (&entry, fid, ce);
     if (error) {
	 ret.res = -1;
	 ret.error = error;
	 return ret;
     }

     error = fcache_get_attr (entry, ce);
     if (error) {
	 ReleaseWriteLock (&entry->lock);
	 ret.res = -1;
	 ret.error = error;
	 return ret;
     }

     if (checkright (entry,
		     entry->status.FileType == TYPE_FILE ? AREAD : ALIST,
		     ce)) {
	  *attr = entry->status;
	  ret.res = 0;
	  entry->flags.attrusedp = TRUE;
	  entry->flags.kernelp = TRUE;
	  entry->tokens |= XFS_ATTR_R;
	  if (ae != NULL)
	      *ae = entry->acccache;

	  log_operation ("getattr (%ld,%lu,%lu,%lu)\n",
		   fid.Cell, fid.fid.Volume, fid.fid.Vnode, fid.fid.Unique);

     } else {
	  ret.res   = -1;
	  ret.error = EACCES;
     }
     if(entry->flags.mountp)
	 *realfid = entry->realfid;
     else
	 *realfid = fid;

     ReleaseWriteLock (&entry->lock);
     ret.tokens = entry->tokens;
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
	 ret.res = -1;
	 ret.error = error;
	 return ret;
     }

     error = fcache_get_attr (entry, ce);
     if (error) {
	 ReleaseWriteLock (&entry->lock);
	 ret.res = -1;
	 ret.error = error;
	 return ret;
     }
     if (checkright (entry, AWRITE, ce)) {
	  arla_warnx (ADEBCM, "cm_setattr: Writing status");
	  ret.res = write_attr (entry, attr, ce);
	  if (ret.res != 0)
	      ret.error = ret.res;

	  log_operation ("setattr (%ld,%lu,%lu,%lu)\n",
		   fid.Cell, fid.fid.Volume, fid.fid.Vnode, fid.fid.Unique);

     } else {
	  ret.res = -1;
	  ret.error = EACCES;
     }
     ReleaseWriteLock (&entry->lock);
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
	 ret.res = -1;
	 ret.error = error;
	 return ret;
     }

     error = fcache_get_attr (entry, ce);
     if (error) {
	 ReleaseWriteLock (&entry->lock);
	 ret.res = -1;
	 ret.error = error;
	 return ret;
     }

     if (checkright (entry, AWRITE, ce)) {
	  ret.res = truncate_file (entry, size, ce);

	  log_operation ("ftruncate (%ld,%lu,%lu,%lu) %lu\n",
		   fid.Cell, fid.fid.Volume, fid.fid.Vnode, fid.fid.Unique,
		   (unsigned long)size);
     } else {
	  ret.res = -1;
	  ret.error = EACCES;
     }
     ReleaseWriteLock (&entry->lock);
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
	 ret.res = -1;
	 ret.error = error;
	 return ret;
     }

     error = fcache_get_attr (entry, ce);
     if (error) {
	 ReleaseWriteLock (&entry->lock);
	 ret.res = -1;
	 ret.error = error;
	 return ret;
     }

     if (checkright (entry, AWRITE, ce)) {
	  ret.res = 0;		/**/
     } else {
	  ret.res = -1;
	  ret.error = EACCES;
     }

     log_operation ("access (%ld,%lu,%lu,%lu)\n",
	      fid.Cell, fid.fid.Volume, fid.fid.Vnode, fid.fid.Unique);

     ReleaseWriteLock (&entry->lock);
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
 * about mount points.
 */

Result
cm_lookup (VenusFid dir_fid,
	   const char *name,
	   VenusFid *res,
	   CredCacheEntry** ce)
{
     char tmp_name[MAXPATHLEN];
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

     error = adir_lookup (dir_fid, name, res, *ce);
     if (error) {
	  ret.res   = -1;
	  ret.error = error;
	  return ret;
     }
     error = followmountpoint (res, &dir_fid, ce);
     if (error) {
	  ret.res   = -1;
	  ret.error = error;
     } else {
	  ret.res    = 0;
	  ret.tokens = 0;
     }

     /* Assume that this means a bad .. */
     if (   strcmp("..", name) == 0 
	 && dir_fid.Cell == res->Cell 
	 && dir_fid.fid.Volume == res->fid.Volume
	 && dir_fid.fid.Vnode == res->fid.Vnode 
	 && dir_fid.fid.Unique == res->fid.Unique) {
	  FCacheEntry *e;
	  int error;

	  error = fcache_get (&e, dir_fid, *ce);
	  if (error) {
	      ret.res = -1;
	      ret.error = error;
	      return ret;
	  }

	  error = fcache_get_attr (e, *ce);
	  if (error) {
	      ReleaseWriteLock (&e->lock);
	      ret.res = -1;
	      ret.error = error;
	      return ret;
	  }

	  assert (e->flags.mountp);

	  *res = e->parent;
	  ret.res = 0;
	  ret.tokens = e->tokens;
	  ReleaseWriteLock (&e->lock);
     }
     
     log_operation ("lookup (%ld,%lu,%lu,%lu) %s\n",
	      dir_fid.Cell,
	      dir_fid.fid.Volume, dir_fid.fid.Vnode, dir_fid.fid.Unique,
	      name);

     return ret;
}

/*
 * Create this file and more.
 */

Result
cm_create (VenusFid dir_fid, const char *name, AFSStoreStatus *store_attr,
	   VenusFid *res, AFSFetchStatus *fetch_attr,
	   CredCacheEntry* ce)
{
     FCacheEntry *dire;
     Result ret;
     int error;

     error = fcache_get (&dire, dir_fid, ce);
     if (error) {
	 ret.res = -1;
	 ret.error = error;
	 return ret;
     }

     error = fcache_get_data (dire, ce);
     if (error) {
	 ret.res = -1;
	 ret.error = error;
	 goto out;
     }

     if (checkright (dire, AINSERT, ce)) {
	  error = create_file (dire, name, store_attr,
			       res, fetch_attr, ce);
	  if (error) {
	      ret.res = -1;
	      ret.error = error;
	  } else {
	      error = adir_creat (dire, name, res->fid);
	      if (error) {
		  ret.res = -1;
		  ret.error = error;
	      } else {
		  ret.res = 0;
	      }
	  }
     } else {
	  ret.res = -1;
	  ret.error = EACCES;
     }
out:
     log_operation ("create (%ld,%lu,%lu,%lu) %s\n",
	      dir_fid.Cell,
	      dir_fid.fid.Volume, dir_fid.fid.Vnode, dir_fid.fid.Unique,
	      name);

     ReleaseWriteLock (&dire->lock);
     return ret;
}

/*
 * Create a new directory
 */

Result
cm_mkdir (VenusFid dir_fid, const char *name,
	  AFSStoreStatus *store_attr,
	  VenusFid *res, AFSFetchStatus *fetch_attr,
	  CredCacheEntry* ce)
{
     FCacheEntry *dire;
     Result ret;
     int error;

     error = fcache_get (&dire, dir_fid, ce);
     if (error) {
	 ret.res = -1;
	 ret.error = error;
	 return ret;
     }

     error = fcache_get_data (dire, ce);
     if (error) {
	 ret.res = -1;
	 ret.error = error;
	 goto out;
     }

     if (checkright (dire, AINSERT, ce)) {
	  error = create_directory (dire, name, store_attr,
				    res, fetch_attr, ce);
	  if (error) {
	      ret.res = -1;
	      ret.error = error;
	  } else {
	      error = adir_creat (dire, name, res->fid);
	      if (error) {
		  ret.res = -1;
		  ret.error = error;
	      } else {
		  ret.res = 0;
	      }
	  }
     } else {
	  ret.res = -1;
	  ret.error = EACCES;
     }

     log_operation ("mkdir (%ld,%lu,%lu,%lu) %s\n",
	      dir_fid.Cell,
	      dir_fid.fid.Volume, dir_fid.fid.Vnode, dir_fid.fid.Unique,
	      name);

out:
     ReleaseSharedLock (&dire->lock);
     return ret;
}

/*
 * Create a symlink
 */

Result
cm_symlink (VenusFid dir_fid,
	    const char *name, AFSStoreStatus *store_attr,
	    VenusFid *res, AFSFetchStatus *fetch_attr,
	    const char *contents,
	    CredCacheEntry* ce)
{
     FCacheEntry *dire;
     Result ret;
     int error;

     error = fcache_get (&dire, dir_fid, ce);
     if (error) {
	 ret.res = -1;
	 ret.error = error;
	 return ret;
     }

     error = fcache_get_data (dire, ce);
     if (error) {
	 ret.res = -1;
	 ret.error = error;
	 goto out;
     }

     if (checkright (dire, AINSERT, ce)) {
	  error = create_symlink (dire, name, store_attr,
				  res, fetch_attr,
				  contents, ce);
	  if (error) {
	      ret.res = -1;
	      ret.error = error;
	  } else {
	      error = adir_creat (dire, name, res->fid);
	      if (error) {
		  ret.res = -1;
		  ret.error = error;
	      } else {
		  ret.res = 0;
	      }
	  }
     } else {
	  ret.res = -1;
	  ret.error = EACCES;
     }
     log_operation ("symlink (%ld,%lu,%lu,%lu) %s %s\n",
	      dir_fid.Cell,
	      dir_fid.fid.Volume, dir_fid.fid.Vnode, dir_fid.fid.Unique,
	      name,
	      contents);

out:
     ReleaseWriteLock (&dire->lock);
     return ret;
}

/*
 * Create a hard link.
 */

Result
cm_link (VenusFid dir_fid,
	 const char *name,
	 VenusFid existing_fid,
	 AFSFetchStatus *existing_status,
	 CredCacheEntry* ce)
{
     FCacheEntry *dire, *file;
     Result ret;
     int error;

     error = fcache_get (&dire, dir_fid, ce);
     if (error) {
	 ret.res = -1;
	 ret.error = error;
	 return ret;
     }

     error = fcache_get_data (dire, ce);
     if (error) {
	 ReleaseWriteLock (&dire->lock);
	 ret.res = -1;
	 ret.error = error;
	 return ret;
     }

     error = fcache_get (&file, existing_fid, ce);
     if (error) {
	 ReleaseWriteLock (&dire->lock);
	 ret.res = -1;
	 ret.error = error;
	 return ret;
     }

     error = fcache_get_attr (file, ce);
     if (error) {
	 ret.res = -1;
	 ret.error = error;
	 goto out;
     }

     if (checkright (dire, AINSERT, ce)) {
	  error = create_link (dire, name, 
			       file, ce);
	  if (error) {
	      ret.res = -1;
	      ret.error = error;
	  } else {
	      error = adir_creat (dire, name, existing_fid.fid);
	      if (error) {
		  ret.res = -1;
		  ret.error = error;
	      } else {
		  *existing_status = file->status;
		  ret.res = 0;
	      }
	  }
     } else {
	  ret.res = -1;
	  ret.error = EACCES;
     }
     log_operation ("link (%ld,%lu,%lu,%lu) (%ld,%lu,%lu,%lu) %s\n",
	      dir_fid.Cell,
	      dir_fid.fid.Volume, dir_fid.fid.Vnode, dir_fid.fid.Unique,
	      existing_fid.Cell,
	      existing_fid.fid.Volume,
	      existing_fid.fid.Vnode,
	      existing_fid.fid.Unique,
	      name);

out:
     ReleaseWriteLock (&dire->lock);
     ReleaseWriteLock (&file->lock);
     return ret;
}
/*
 *
 */

Result
cm_remove(VenusFid dir_fid,
	  const char *name, CredCacheEntry* ce)
{
     FCacheEntry *dire;
     Result ret;
     int error;

     error = fcache_get (&dire, dir_fid, ce);
     if (error) {
	 ret.res = -1;
	 ret.error = error;
	 return ret;
     }

     error = fcache_get_data (dire, ce);
     if (error) {
	 ret.res = -1;
	 ret.error = error;
	 goto out;
     }

     if (checkright (dire, ADELETE, ce)) {
	  error = remove_file (dire, name, ce);
	  if (error) {
	      ret.res = -1;
	      ret.error = error;
	  } else {
	      error = adir_remove (dire, name);
	      if (error) {
		  ret.res = -1;
		  ret.error = error;
	      } else {
		  ret.res = 0;
	      }
	  }
     } else {
	  ret.res = -1;
	  ret.error = EACCES;
     }
     log_operation ("remove (%ld,%lu,%lu,%lu) %s\n",
	      dir_fid.Cell,
	      dir_fid.fid.Volume, dir_fid.fid.Vnode, dir_fid.fid.Unique,
	      name);

out:
     ReleaseWriteLock (&dire->lock);
     return ret;
}

/*
 *
 */

Result
cm_rmdir(VenusFid dir_fid,
	 const char *name, CredCacheEntry* ce)
{
     FCacheEntry *dire;
     Result ret;
     int error;

     error = fcache_get (&dire, dir_fid, ce);
     if (error) {
	 ret.res = -1;
	 ret.error = error;
	 return ret;
     }

     error = fcache_get_data (dire, ce);
     if (error) {
	 ret.res = -1;
	 ret.error = error;
	 goto out;
     }

     if (checkright (dire, ADELETE, ce)) {
	  error = remove_directory (dire, name, ce);
	  if (error) {
	      ret.res = -1;
	      ret.error = error;
	  } else {
	      error = adir_remove (dire, name);
	      if (error) {
		  ret.res = -1;
		  ret.error = error;
	      } else {
		  ret.res = 0;
	      }
	  }
     } else {
	  ret.res = -1;
	  ret.error = EACCES;
     }

     log_operation ("rmdir (%ld,%lu,%lu,%lu) %s\n",
	      dir_fid.Cell,
	      dir_fid.fid.Volume, dir_fid.fid.Vnode, dir_fid.fid.Unique,
	      name);

out:
     ReleaseWriteLock (&dire->lock);
     return ret;
}

/*
 *
 */

Result
cm_rename(VenusFid old_parent_fid, const char *old_name,
	  VenusFid new_parent_fid, const char *new_name,
	  CredCacheEntry* ce)
{
     FCacheEntry *old_dir;
     FCacheEntry *new_dir;
     Result ret;
     int error;

     /* old parent dir */

     error = fcache_get (&old_dir, old_parent_fid, ce);
     if (error) {
	 ret.res = -1;
	 ret.error = error;
	 return ret;
     }

     error = fcache_get_data (old_dir, ce);
     if (error) {
	 ReleaseWriteLock (&old_dir->lock);
	 ret.res = -1;
	 ret.error = error;
	 return ret;
     }

     /* new parent dir */

     if (old_parent_fid.fid.Vnode != new_parent_fid.fid.Vnode
	 || old_parent_fid.fid.Unique != new_parent_fid.fid.Unique) {

	 error = fcache_get (&new_dir, new_parent_fid, ce);
	 if (error) {
	     ReleaseWriteLock (&old_dir->lock);
	     ret.res = -1;
	     ret.error = error;
	     return ret;
	 }

	 error = fcache_get_data (new_dir, ce);
	 if (error) {
	     ret.res = -1;
	     ret.error = error;
	     goto out;
	 }

     } else {
	 new_dir = old_dir;
     }

     if (checkright (old_dir, ADELETE, ce)
	 && checkright (new_dir, AINSERT, ce)) {

	 error = rename_file (old_dir, old_name,
			      new_dir, new_name,
			      ce);
	  if (error) {
	      ret.res = -1;
	      ret.error = error;
	  } else {
	      VenusFid foo_fid;

	      ReleaseWriteLock (&old_dir->lock);
	      error = adir_lookup (old_dir->fid, old_name, &foo_fid, ce);
	      ObtainWriteLock (&old_dir->lock);

	      if (error) {
		  ret.res = -1;
		  ret.error = error;
		  goto out;
	      }
	      error = adir_remove (old_dir, old_name)
		  || adir_creat (new_dir, new_name, foo_fid.fid);

	      if (error) {
		  ret.res = -1;
		  ret.error = error;
	      } else {
		  ret.res = 0;
	      }
	  }
     } else {
	  ret.res = -1;
	  ret.error = EACCES;
     }

     log_operation ("rename (%ld,%lu,%lu,%lu) (%ld,%lu,%lu,%lu) %s %s\n",
	      old_parent_fid.Cell,
	      old_parent_fid.fid.Volume,
	      old_parent_fid.fid.Vnode,
	      old_parent_fid.fid.Unique,
	      new_parent_fid.Cell,
	      new_parent_fid.fid.Volume,
	      new_parent_fid.fid.Vnode,
	      new_parent_fid.fid.Unique,
	      old_name, new_name);

out:
     ReleaseWriteLock (&old_dir->lock);
     if (old_parent_fid.fid.Vnode != new_parent_fid.fid.Vnode
	 || old_parent_fid.fid.Unique != new_parent_fid.fid.Unique)
	 ReleaseWriteLock (&new_dir->lock);
     return ret;
}
