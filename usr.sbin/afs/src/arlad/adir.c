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
 * Routines for reading an AFS directory
 */

#include "arla_local.h"

RCSID("$arla: adir.c,v 1.73 2002/07/24 05:56:27 lha Exp $") ;

/*
 *
 */

static int
get_fbuf_from_centry (CredCacheEntry **ce,
		      int *fd, fbuf *fbuf,
		      FCacheEntry **centry,
		      int open_flags, int fbuf_flags)
{
    int ret;

    ret = fcache_get_data (centry, ce, 0);
    if (ret)
	return ret;

    ret = fcache_get_fbuf (*centry, fd, fbuf, open_flags, fbuf_flags);
    if (ret) {
	return ret;
    }
    return 0;
}

/*
 * Lookup `name' in the AFS directory identified by `centry' and return
 * the Fid in `file'.  All operations are done as `cred' and return
 * value is 0 or error code.
 *
 *
 * Locking:
 *            In        Out       Fail
 *    centry: Locked    Locked    Locked
 *  
 */

int
adir_lookup (FCacheEntry *centry, const char *name, VenusFid *file)
{
     int ret;
     int fd;
     fbuf the_fbuf;

     ret = fcache_get_fbuf (centry, &fd, &the_fbuf, O_RDONLY,
			    FBUF_READ|FBUF_PRIVATE);
     if (ret)
	 return ret;

     ret = fdir_lookup (&the_fbuf, &centry->fid, name, file);
     fbuf_end (&the_fbuf);
     close (fd);
     return ret;
}

/*
 * Lookup `name' in the AFS directory identified by `dir' and change the
 * fid to `fid'.
 */

int
adir_changefid (FCacheEntry **centry,
		const char *name,
		VenusFid *file,
		CredCacheEntry **ce)
{
    int ret;
    int fd;
    fbuf the_fbuf;

    ret = get_fbuf_from_centry (ce, &fd, &the_fbuf, centry, O_RDWR, 
				FBUF_READ|FBUF_WRITE|FBUF_SHARED);
    if (ret)
	return ret;

    ret = fdir_changefid (&the_fbuf, name, file);
    fbuf_end (&the_fbuf);
    close (fd);
    return ret;
}

/*
 * Return TRUE if dir is empty.
 */

int
adir_emptyp (FCacheEntry **centry,
	     CredCacheEntry **ce)
{
     int ret;
     int fd;
     fbuf the_fbuf;

     ret = get_fbuf_from_centry (ce, &fd, &the_fbuf, centry,
				 O_RDONLY, FBUF_READ|FBUF_PRIVATE);
     if (ret)
	 return ret;

     ret = fdir_emptyp (&the_fbuf);
     fbuf_end (&the_fbuf);
     close (fd);
     return ret;
}

/*
 * Read all entries in the AFS directory identified by `dir' and call
 * `func' on each entry with the fid, the name, and `arg'.
 */

int
adir_readdir (FCacheEntry **centry,
	      fdir_readdir_func func,
	      void *arg,
	      CredCacheEntry **ce)
{
     int fd;
     fbuf the_fbuf;
     int ret;

     ret = get_fbuf_from_centry (ce, &fd, &the_fbuf, centry,
				 O_RDONLY, FBUF_READ|FBUF_PRIVATE);
     if (ret)
	 return ret;

     ret = fdir_readdir (&the_fbuf, func, arg, (*centry)->fid, NULL);
     fbuf_end (&the_fbuf);
     close (fd);
     return ret;
}

/*
 * Create a new directory with only . and ..
 */

int
adir_mkdir (FCacheEntry *dir,
	    AFSFid dot,
	    AFSFid dot_dot)
{
    int fd;
    fbuf the_fbuf;
    int ret;

    AssertExclLocked(&dir->lock);

    ret = fcache_get_fbuf (dir, &fd, &the_fbuf, O_RDWR,
			   FBUF_READ|FBUF_WRITE|FBUF_SHARED);
    if (ret)
	return ret;

    ret = fdir_mkdir (&the_fbuf, dot, dot_dot);
    fcache_update_length (dir, fbuf_len(&the_fbuf), fbuf_len(&the_fbuf));
    fbuf_end (&the_fbuf);
    close (fd);
    return ret;
}

/*
 * Create a new entry with name `filename' and contents `fid' in `dir'.
 */

int
adir_creat (FCacheEntry *dir,
	    const char *name,
	    AFSFid fid)
{
    int fd;
    fbuf the_fbuf;
    int ret;

    ret = fcache_get_fbuf (dir, &fd, &the_fbuf, O_RDWR, 
			   FBUF_READ|FBUF_WRITE|FBUF_SHARED);
    if (ret)
	return ret;

    ret = fdir_creat (&the_fbuf, name, fid);
    fcache_update_length (dir, fbuf_len(&the_fbuf), fbuf_len(&the_fbuf));
    fbuf_end (&the_fbuf);
    close (fd);
    return ret;
}

/*
 * Remove the entry named `name' in dir.
 */

int
adir_remove (FCacheEntry *dir,
	     const char *name)
{
    int fd;
    fbuf the_fbuf;
    int ret;

    ret = fcache_get_fbuf (dir, &fd, &the_fbuf, O_RDWR, 
			   FBUF_READ|FBUF_WRITE|FBUF_SHARED);
    if (ret)
	return ret;

    ret = fdir_remove(&the_fbuf, name, NULL);
    fcache_update_length (dir, fbuf_len(&the_fbuf), fbuf_len(&the_fbuf));
    fbuf_end (&the_fbuf);
    close (fd);
    return ret;
}
