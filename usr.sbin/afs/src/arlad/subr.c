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

#include "arla_local.h"
RCSID("$KTH: subr.c,v 1.7 2000/10/14 19:58:12 map Exp $");

/*
 * come up with a good inode number for `name', `fid' in `parent'
 */

ino_t
dentry2ino (const char *name, const VenusFid *fid, const FCacheEntry *parent)
{
    if (strcmp (name, ".") == 0
	&& (parent->flags.vol_root
	    || (fid->fid.Vnode == 1 && fid->fid.Unique == 1))
	&& parent->volume != NULL)
	return afsfid2inode (&parent->volume->mp_fid);
    else if (strcmp (name, "..") == 0
	     && (parent->flags.vol_root
		 || (parent->fid.fid.Vnode == 1
		     && parent->fid.fid.Unique == 1))
	     && parent->volume != NULL)
	    return afsfid2inode (&parent->volume->parent_fid);
    else if (strcmp (name, "..") == 0
	     && fid->fid.Vnode == 1 && fid->fid.Unique == 1
	     && parent->volume != NULL)
	return afsfid2inode (&parent->volume->mp_fid);
    else
	return afsfid2inode (fid);
}

/*
 * Assume `e' has valid data.
 */

Result
conv_dir_sub (FCacheEntry *e, CredCacheEntry *ce, u_int tokens,
	      fcache_cache_handle *cache_handle,
	      char *cache_name, size_t cache_name_sz,
	      fdir_readdir_func func,
	      void (*flush_func)(void *),
	      size_t blocksize)
{
     struct write_dirent_args args;
     Result res;
     int ret;
     int fd;
     fbuf the_fbuf;

     e->flags.extradirp = TRUE;
     fcache_extra_file_name (e, cache_name, cache_name_sz);
     res.tokens = e->tokens |= XFS_DATA_R | XFS_OPEN_NR;

     args.fd = open (cache_name, O_WRONLY | O_CREAT | O_TRUNC | O_BINARY, 0666);
     if (args.fd == -1) {
	  res.res = -1;
	  res.error = errno;
	  arla_warn (ADEBWARN, errno, "open %s", cache_name);
	  return res;
     }
     ret = fcache_fhget (cache_name, cache_handle);

     args.off  = 0;
     args.buf  = (char *)malloc (blocksize);
     if (args.buf == NULL) {
	 arla_warn (ADEBWARN, errno, "malloc %u", (unsigned)blocksize);
	 res.res = -1;
	 res.error = errno;
	 close (args.fd);
	 return res;
     }

     ret = fcache_get_fbuf (e, &fd, &the_fbuf,
			    O_RDONLY, FBUF_READ|FBUF_PRIVATE);
     if (ret) {
	 res.res = -1;
	 res.error = ret;
	 close (args.fd);
	 free (args.buf);
	 return res;
     }
     
     args.ptr  = args.buf;
     args.last = NULL;
     args.e    = e;
     args.ce   = ce;
     
     fdir_readdir (&the_fbuf, func, (void *)&args, &e->fid);

     fbuf_end (&the_fbuf);
     close (fd);

     if (args.last)
	  (*flush_func) (&args);
     free (args.buf);
     res.res = close (args.fd);
     if (res.res)
	  res.error = errno;
     return res;
}
