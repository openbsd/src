/*
 * Copyright (c) 1999 Kungliga Tekniska Högskolan
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
 * Interface to fdir directory handling routines
 */

#include <config.h>

RCSID("$KTH: mdir.c,v 1.10 2000/10/03 00:19:06 lha Exp $");

#include <sys/types.h>
#include <sys/stat.h>
#include <sys/errno.h>
#include <assert.h>
#include <unistd.h>

#include <fs.h>
#include <rx/rx.h>
#include <fbuf.h>
#include <fdir.h>

#include <mdir.h>


int
mdir_lookup (struct mnode *node, VenusFid *dir, const char *name, VenusFid *file)
{
    fbuf the_fbuf;
    int ret, saved_ret;

    assert (node->flags.sbp);
    assert (node->flags.fdp);

    ret = fbuf_create (&the_fbuf, node->fd, node->sb.st_size, 
		       FBUF_READ|FBUF_PRIVATE);
    if (ret)
	return ret;

    saved_ret = fdir_lookup (&the_fbuf, dir, name, file);

    ret = fbuf_end (&the_fbuf);
    if (ret)
	return ret;
    
    return saved_ret;
}

int
mdir_emptyp (struct mnode *node)
{
    fbuf the_fbuf;
    int ret, saved_ret;

    assert (node->flags.sbp);
    assert (node->flags.fdp);

    ret = fbuf_create (&the_fbuf, node->fd, node->sb.st_size,
		       FBUF_READ|FBUF_PRIVATE);
    if (ret)
	return ret;

    saved_ret = fdir_emptyp (&the_fbuf);

    ret = fbuf_end (&the_fbuf);
    if (ret)
	return ret;
    
    return saved_ret;
}

int
mdir_readdir (struct mnode *node,
	      void (*func)(VenusFid *, const char *, void *), 
	      void *arg,
	      VenusFid *dir)
{
    fbuf the_fbuf;
    int ret, saved_ret;

    assert (node->flags.sbp);
    assert (node->flags.fdp);

    ret = fbuf_create (&the_fbuf, node->fd, node->sb.st_size,
		       FBUF_READ|FBUF_PRIVATE);
    if (ret)
	return ret;

    saved_ret = fdir_readdir (&the_fbuf, func, arg, dir);

    ret = fbuf_end (&the_fbuf);
    if (ret)
	return ret;
    
    return saved_ret;
}


int
mdir_creat (struct mnode *node,
	    const char *filename,
	    AFSFid fid)
{
    fbuf the_fbuf;
    int ret, saved_ret;
    int32_t len;

    assert (node->flags.sbp);
    assert (node->flags.fdp);

    ret = fbuf_create (&the_fbuf, node->fd, node->sb.st_size, 
		       FBUF_READ|FBUF_WRITE|FBUF_SHARED);
    if (ret)
	return ret;

    saved_ret = fdir_creat (&the_fbuf, filename, fid);

    if (ret == 0) {
	len = fbuf_len (&the_fbuf);
	mnode_update_size (node, &len);
    }
    
    ret = fbuf_end (&the_fbuf);
    if (ret)
	return ret;
    
    return saved_ret;
}


int
mdir_remove (struct mnode *node,
	     const char *name)
{
    fbuf the_fbuf;
    int ret, saved_ret;
    int32_t len;

    assert (node->flags.sbp);
    assert (node->flags.fdp);

    ret = fbuf_create (&the_fbuf, node->fd, node->sb.st_size,
		       FBUF_READ|FBUF_WRITE|FBUF_SHARED);
    if (ret)
	return ret;

    saved_ret = fdir_remove (&the_fbuf, name, NULL);

    if (ret == 0) {
	len = fbuf_len (&the_fbuf);
	mnode_update_size (node, &len);
    }
    
    ret = fbuf_end (&the_fbuf);
    if (ret)
	return ret;
    
    return saved_ret;
}

int
mdir_mkdir (struct mnode *node,
	    AFSFid dot,
	    AFSFid dot_dot)
{
    fbuf the_fbuf;
    int ret, saved_ret;
    int32_t len;

    assert (node->flags.sbp);
    assert (node->flags.fdp);

    ret = fbuf_create (&the_fbuf, node->fd, node->sb.st_size,
		       FBUF_READ|FBUF_WRITE|FBUF_SHARED);
    if (ret)
	return ret;

    saved_ret = fdir_mkdir (&the_fbuf, dot, dot_dot);

    if (ret == 0) {
	len = fbuf_len (&the_fbuf);
	mnode_update_size (node, &len);
    }

    ret = fbuf_end (&the_fbuf);
    if (ret)
	return ret;
    
    return saved_ret;
}


