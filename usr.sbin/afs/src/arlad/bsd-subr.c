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

/* XXX need this to get dirent and DIRSIZ */
#ifdef __osf__
#define _OSF_SOURCE
#define _BSD
/* Might want to define _KERNEL for osf and use KDIRSIZ */
#endif

#include "arla_local.h"
RCSID("$arla: bsd-subr.c,v 1.61 2003/02/15 02:24:08 lha Exp $");

#ifdef __linux__
#include <nnpfs/nnpfs_dirent.h>
#else
#define NNPFS_DIRENT_BLOCKSIZE 512
#define nnpfs_dirent dirent
#endif

static long blocksize = NNPFS_DIRENT_BLOCKSIZE;	/* XXX */

/*
 * Write out all remaining data in `args'
 */

static void
flushbuf (void *vargs)
{
    struct write_dirent_args *args = (struct write_dirent_args *)vargs;
    unsigned inc = blocksize - (args->ptr - args->buf);
    struct nnpfs_dirent *last = (struct nnpfs_dirent *)args->last;

    last->d_reclen += inc;
    if (write (args->fd, args->buf, blocksize) != blocksize)
	arla_warn (ADEBWARN, errno, "write");
    args->ptr = args->buf;
    args->last = NULL;
}

/*
 * Write a dirent to the args buf in `arg' containg `fid' and `name'.
 */

static int
write_dirent(VenusFid *fid, const char *name, void *arg)
{
     struct nnpfs_dirent dirent, *real;
     struct write_dirent_args *args = (struct write_dirent_args *)arg;

     dirent.d_namlen = strlen (name);
#ifdef _GENERIC_DIRSIZ
     dirent.d_reclen = _GENERIC_DIRSIZ(&dirent);
#elif defined(DIRENT_SIZE)
     dirent.d_reclen = DIRENT_SIZE(&dirent);
#else
     dirent.d_reclen = DIRSIZ(&dirent);
#endif

     if (args->ptr + dirent.d_reclen > args->buf + blocksize)
	  flushbuf (args);
     real = (struct nnpfs_dirent *)args->ptr;

     real->d_namlen = dirent.d_namlen;
     real->d_reclen = dirent.d_reclen;
#if defined(HAVE_STRUCT_DIRENT_D_TYPE) && !defined(__linux__)
     real->d_type   = DT_UNKNOWN;
#endif
     
     real->d_fileno = dentry2ino (name, fid, args->e);
     strlcpy (real->d_name, name, sizeof(real->d_name));
     args->ptr += real->d_reclen;
     args->off += real->d_reclen;
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

#ifndef DIRBLKSIZ
#define DIRBLKSIZ 1024
#endif

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
    struct nnpfs_dirent *dp;
    struct nnpfs_dirent *last_dp;

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

	dp = (struct nnpfs_dirent *)p;

	assert (dp->d_reclen > 0);

	if (strcmp (filename, dp->d_name) == 0) {
	    if (last_dp != NULL) {
		size_t off1, off2;
		unsigned len;

		/*
		 * d_reclen can be as largest (in worst case)
		 * DIRBLKSIZ, and may not cause the entry to cross a
		 * DIRBLKSIZ boundery.
		 */
		len = last_dp->d_reclen + dp->d_reclen;

                off1 = (char *)last_dp - buf;
                off2 = off1 + len;
		off1 /= DIRBLKSIZ;
		off2 /= DIRBLKSIZ;

		if (len < DIRBLKSIZ && off1 == off2)
		    last_dp->d_reclen = len;
	    }
	    dp->d_fileno = 0;
	    ret = 0;
	    break;
	}
	last_dp = dp;
    }
    fbuf_end (&fb);
    close (fd);
    return ret;
}
