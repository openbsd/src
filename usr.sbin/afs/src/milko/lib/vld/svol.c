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

#include <sfvol_private.h>

RCSID("$arla: svol.c,v 1.3 2000/10/03 00:19:44 lha Exp $");

/*
 * Description:
 *  svol is a simple (and dumb (and slow)) implementation of volume-
 *  operations. It store the files in a tree-structure where
 *  inode-number is used as ``key'' to where the node is stored in 
 *  the tree.
 */

/*
 * Create a svol and return it in `vol' that is generic volume.
 * ignore the flags.
 */

static int
svol_open (struct dp_part *part, int32_t volid, int flags,
	   void **data)
{
    *data = NULL;

    return 0;
}

/*
 * free all svol related to the volume `vol'.
 */

static void
svol_free (volume_handle *vol)
{
    assert (vol->data == NULL);
}

/*
 * create a inode on `vol', return onode_opaque in `o'.
 */

static int
svol_icreate (volume_handle *vol, onode_opaque *o, node_type type,
	      struct mnode *n)
{
    return local_create_file (vol->dp, o, n);
}

/*
 * open `o' in `vol' with open(2) `flags', return filedescriptor in `fd'
 */

static int
svol_iopen (volume_handle *vol, onode_opaque *o, int flags, int *fd)
{
    return local_open_file (vol->dp, o, flags, fd);
}

static int
svol_unlink (volume_handle *vol, onode_opaque *o)
{
    return local_unlink_file (vol->dp, o);
}

/*
 *
 */

static int
svol_remove (volume_handle *vol)
{
    /* 
     * We don't need to remove anything since we never created anything.
     * There might be reson to try and keed track of all nodes to be able
     * to remove lost nodes. But then, this wouldn't be the simple volume.
     */

    svol_free (vol);
    return 0;
}

/*
 *
 */

vol_op svol_volume_ops = {
    "svol",
    svol_open,
    svol_free,
    svol_icreate,
    svol_iopen,
    svol_unlink,
    svol_remove
};    
