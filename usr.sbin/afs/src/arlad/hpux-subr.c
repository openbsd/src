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

/* If this isn't defined, we get a constant for DIRSIZ */

#define DIRSIZ_MACRO
#define _INCLUDE_HPUX_SOURCE

#include "arla_local.h"
RCSID("$arla: hpux-subr.c,v 1.28 2002/03/06 21:56:59 tol Exp $");

static long blocksize = 1024;	/* XXX */

static void
flushbuf (void *vargs)
{
    struct write_dirent_args *args = (struct write_dirent_args *)vargs;
    struct dirent *last = (struct dirent *)args->last;
    unsigned inc = blocksize - (args->ptr - args->buf);

    last->d_reclen += inc;
#if 0
    last->d_off    += inc;
#endif
    if (write (args->fd, args->buf, blocksize) != blocksize)
	arla_warn (ADEBWARN, errno, "write");
    args->ptr = args->buf;
    args->last = NULL;
}

#if 0 /* this seems wrong? */
#define DIRSIZ(dp) \
        (((sizeof(struct dirent)+ (strlen((dp)->d_name)+1)) +3) & ~3)
#endif

static int
write_dirent(VenusFid *fid, const char *name, void *arg)
{
     struct dirent dirent, *real;
     struct write_dirent_args *args = (struct write_dirent_args *)arg;

/*     dirent.d_namlen = strlen (name);*/
     dirent.d_reclen = DIRSIZ(&dirent);

     if (args->ptr + dirent.d_reclen > args->buf + blocksize)
	  flushbuf (args);
     real = (struct dirent *)args->ptr;

/*     real->d_namlen = dirent.d_namlen;*/
     real->d_reclen = dirent.d_reclen;
     real->d_ino = dentry2ino (name, fid, args->e);
     strlcpy (real->d_name, name, sizeof(real->d_name));
     args->ptr += real->d_reclen;
     args->off += real->d_reclen;
#if 0
     real->d_off = args->off;
#endif
     args->last = real;
     return 0;
}

int
conv_dir (FCacheEntry *e, CredCacheEntry *ce, u_int tokens,
	  fcache_cache_handle *cache_handle,
	  char *cache_name, size_t cache_name_sz)
{
    return conv_dir_sub (e, ce, tokens, cache_handle, cache_name,
			 cache_name_sz, write_dirent, flushbuf, blocksize);
}
/*
 * remove `filename` from the converted directory for `e'
 */

int
dir_remove_name (FCacheEntry *e, const char *filename,
		 fcache_cache_handle *cache_handle,
		 char *cache_name, size_t cache_name_sz)
{
    int ret;
    int fd;
    fbuf fb;
    struct stat sb;
    char *buf;
    char *p;
    size_t len;
    struct dirent *dp;
    struct dirent *last_dp;

    fcache_extra_file_name (e, cache_name, cache_name_sz);
    fd = open (cache_name, O_RDWR, 0);
    if (fd < 0)
	return errno;
    fcache_fhget (cache_name, cache_handle);
    if (fstat (fd, &sb) < 0) {
	ret = errno;
	close (fd);
	return ret;
    }
    len = sb.st_size;

    ret = fbuf_create (&fb, fd, len, FBUF_READ|FBUF_WRITE|FBUF_SHARED);
    if (ret) {
	close (fd);
	return ret;
    }
    last_dp = NULL;
    ret = ENOENT;
    for (p = buf = fbuf_buf (&fb); p < buf + len; p += dp->d_reclen) {
	dp = (struct dirent *)p;

	if (strcmp (filename, dp->d_name) == 0) {
	    if (last_dp != NULL) {
		struct dirent new_last;

		new_last.d_reclen = last_dp->d_reclen + dp->d_reclen;
		if (new_last.d_reclen >= last_dp->d_reclen)
		    last_dp->d_reclen = new_last.d_reclen;
	    }
	    dp->d_ino = 0;
	    ret = 0;
	    break;
	}
	last_dp = dp;
    }
    fbuf_end (&fb);
    close (fd);
    return ret;
}
