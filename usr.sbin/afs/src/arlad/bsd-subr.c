/*	$OpenBSD: bsd-subr.c,v 1.2 1999/04/30 01:59:07 art Exp $	*/
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

/* XXX need this to get dirent and DIRSIZ */
#ifdef __osf__
#define _OSF_SOURCE
#define _BSD
#endif
#include "arla_local.h"
RCSID("$KTH: bsd-subr.c,v 1.32 1998/12/22 13:15:54 lha Exp $");

#ifdef __linux__
#include <xfs/xfs_dirent.h>
#else
#define XFS_DIRENT_BLOCKSIZE 1024
#define xfs_dirent dirent
#endif
#if defined(GENERIC_DIRSIZ_IN_SYS_DIRENT_H)
#include <sys/dirent.h>
#elif defined(DIRSIZ_IN_DIRENT_H)
#include <dirent.h>
#elif defined(DIRSIZ_IN_SYS_DIR_H)
#include <sys/dir.h>
#endif

static long blocksize = XFS_DIRENT_BLOCKSIZE;	/* XXX */

struct args {
    int fd;
    off_t off;
    char *buf;
    char *ptr;
    struct xfs_dirent *last;
    FCacheEntry *e; 
};

/*
 * Write out all remaining data in `args'
 */

static void
flushbuf (struct args *args)
{
     unsigned inc = blocksize - (args->ptr - args->buf);

     args->last->d_reclen += inc;
     if (write (args->fd, args->buf, blocksize) != blocksize)
	  arla_warn (ADEBWARN, errno, "write");
     args->ptr = args->buf;
     args->last = NULL;
}

/*
 * Write a dirent to the args buf in `arg' containg `fid' and `name'.
 */

static void
write_dirent(VenusFid *fid, const char *name, void *arg)
{
     struct xfs_dirent dirent, *real;
     struct args *args = (struct args *)arg;

     dirent.d_namlen = strlen (name);
#ifdef _GENERIC_DIRSIZ
     dirent.d_reclen = _GENERIC_DIRSIZ(&dirent);
#else
     dirent.d_reclen = DIRSIZ(&dirent);
#endif

     if (args->ptr + dirent.d_reclen > args->buf + blocksize)
	  flushbuf (args);
     real = (struct xfs_dirent *)args->ptr;

     real->d_namlen = dirent.d_namlen;
     real->d_reclen = dirent.d_reclen;
#if defined(HAVE_STRUCT_DIRENT_D_TYPE) && !defined(__linux__)
     real->d_type   = DT_UNKNOWN;
#endif
     
     if (dirent.d_namlen == 2
	 && strcmp(name, "..") == 0
	 && args->e->flags.mountp) {
	 real->d_fileno = afsfid2inode (&args->e->parent);
     } else if (dirent.d_namlen == 1 && 
		strcmp(name, ".") == 0 &&
		args->e->flags.mountp) {
	 real->d_fileno = afsfid2inode (&args->e->realfid); 
     } else
	 real->d_fileno = afsfid2inode (fid);
     strcpy (real->d_name, name);
     args->ptr += real->d_reclen;
     args->off += real->d_reclen;
#if 0
     real->d_off = args->off;
#endif
     args->last = real;
}

/*
 *
 */

Result
conv_dir (FCacheEntry *e, CredCacheEntry *ce, u_int tokens,
	  xfs_cache_handle *cache_handle,
	  char *cache_name, size_t cache_name_sz)
{
     struct args args;
     Result res;
     int ret;

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
     if(ret) {
	 res.res = -1;
	 res.error = errno;
	 close (args.fd);
	 arla_warn (ADEBWARN, res.error, "fcache_fhget(%s)", cache_name);
	 return res;
     }

     args.off  = 0;
     args.buf  = (char *)malloc (blocksize);
     if (args.buf == NULL) {
	 arla_warn (ADEBWARN, errno, "malloc %u", (unsigned)blocksize);
	 res.res = -1;
	 res.error = errno;
	 close (args.fd);
	 return res;
     }
     args.ptr  = args.buf;
     args.last = NULL;
     args.e = e;
     ReleaseWriteLock (&e->lock); /* XXX */
     adir_readdir (e->fid, write_dirent, (void *)&args, ce);
     ObtainWriteLock (&e->lock); /* XXX */
     if (args.last)
	  flushbuf (&args);
     free (args.buf);
     res.res = close (args.fd);
     if (res.res)
	  res.error = errno;
     return res;
}
