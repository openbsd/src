/*
 * Copyright (c) 1999 - 2000 Kungliga Tekniska Högskolan
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
 * A module for caching of mnodes.
 *
 * Mnodes contain unix and afs structs to to aid in the conversion
 * inbeteen. It also has the feature that data is never allocated.
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

RCSID("$arla: mnode.c,v 1.11 2001/11/06 16:03:19 tol Exp $");

#include <sys/types.h>
#include <stdio.h>

#include <assert.h>

#include <roken.h>
#include <err.h>

#include <list.h>
#include <hash.h>

#include <mnode.h>

#include <mdebug.h>
#include <mlog.h>


List *mnode_lru;	/* least recently used on tail, ref == 0 */

Hashtab *mnode_htab;	/* hash of all valid nodes */

unsigned long mnode_nodes; /* number of nodes in system */
unsigned long mnode_numfree;  /* number of free nodes in system */

/*
 * find out if two nodes `n1' and `n2' are the same
 */

static int
mnode_cmp (void *n1, void *n2)
{
    struct mnode *rn1 = (struct mnode *)n1;
    struct mnode *rn2 = (struct mnode *)n2;
    
    return rn1->fid.Volume != rn1->fid.Volume
	|| rn1->fid.Vnode != rn2->fid.Vnode
	|| rn1->fid.Unique != rn2->fid.Unique;
}

/*
 * calculate a uniq hashvalue for `node'
 */

static unsigned
mnode_hash(void *node)
{
    struct mnode *n = (struct mnode *)node;
    return n->fid.Volume + n->fid.Vnode;
}

/*
 * The init function for the mnode cache
 */

void
mnode_init (unsigned num)
{
    struct mnode *nodes = calloc (sizeof(struct mnode), num);
    int i;

    mnode_numfree = mnode_nodes = num;
    
    if (nodes == NULL)
	errx (1, "mnode_init: calloc failed");

    mnode_lru = listnew();
    if (mnode_lru == NULL)
	errx (1, "mnode_init: listnew returned NULL");
    
    for (i = 0; i < num ;i++) {
	nodes[i].li = listaddhead (mnode_lru, &nodes[i]);
	assert(nodes[i].li);
    }
    mnode_htab = hashtabnew (num * 2, /* XXX */
			     mnode_cmp,
			     mnode_hash);
    if (mnode_htab == NULL)
	errx (1, "mnode_init: hashtabnew returned NULL");
}

/*
 * reset a mnode `res' to a new `fid'
 */

static void
reset_node (struct mnode *n, const AFSFid *fid)
{
    assert (n->ref == 0);
    if (n->flags.fdp) {
	close (n->fd);
	n->flags.fdp = FALSE;
    }
    memset (n, 0, sizeof (*n));
    n->fid = *fid;
}

/*
 * find the `fid' the the hashtable, if it isn't
 * there use an entry on the lru.
 * 
 *
 * XXX fix end of nodes problem.
 */

int
mnode_find (const AFSFid *fid, struct mnode **node)
{
    struct mnode ptr, *res = NULL;
    
    ptr.fid = *fid;

    while (res == NULL) {
	res = hashtabsearch (mnode_htab, &ptr);
	
	if (res) {
	    if (res->flags.removedp == TRUE)
		return ENOENT;

	    if (res->li)
		listdel (mnode_lru, res->li);
	    if (res->ref == 0)
		mnode_numfree--;
	    res->ref++;
	} else if (mnode_numfree != 0) {
	    res = listdeltail (mnode_lru); assert (res);
	    assert (res->ref == 0);
	    hashtabdel (mnode_htab, res);
	    reset_node (res, fid);
	    hashtabadd (mnode_htab, res);
	    res->ref++;
	} else {
	    /* XXX */
	    mlog_log (MDEBWARN,
		      "mnode_find: no free nodes, had to malloc()");

	    res = malloc(sizeof(struct mnode));
	    if (res == NULL) {
		mlog_log (MDEBWARN,
			  "mnode_find: malloc() failed");
		LWP_DispatchProcess(); /* Yield */
		continue;
	    }

	    reset_node (res, fid);
	    hashtabadd (mnode_htab, res);
	    res->ref++;
	}
    }

    assert(res->flags.removedp == FALSE);

    *node = res;
    res->li = listaddhead (mnode_lru, *node);
    return 0;
}

/*
 * Free the `node'. If the node has gone bad (something has failed
 * that shouldn't) its marked bad and things are uncached.
 */

void
mnode_free (struct mnode *node, Bool bad)
{
    if (node->li)
	listdel (mnode_lru, node->li);
    /* 
       bad -> reread
       0 -> close
       */
    
    if (bad) {
	if (node->flags.fdp) 
	    close (node->fd);
	memset (&node->flags, 0, sizeof (node->flags));
    }
    if (--node->ref == 0) {
	if (node->flags.fdp) {
	    close (node->fd);
	    node->flags.fdp = FALSE;
	}

	if (node->flags.removedp == TRUE) {
	    hashtabdel (mnode_htab, node);
	    node->flags.removedp = FALSE;
	}
	mnode_numfree++;
	node->li = listaddtail (mnode_lru, node);
    } else
	node->li = listaddhead (mnode_lru, node);
}

/*
 *
 */

void
mnode_remove (const AFSFid *fid)
{
    struct mnode ptr, *res;
    
    ptr.fid = *fid;

    res = hashtabsearch (mnode_htab, &ptr);
    if (res) {
	if (res->ref == 0 && res->flags.fdp) {
	    close (res->fd);
	    res->flags.fdp = FALSE;
	}
	if (res->li)
	    listdel (mnode_lru, res->li);
	res->li = listaddhead (mnode_lru, res);
	res->flags.removedp = TRUE;
    }
}

/*
 * always update the node `n' to reflect the size `len' or
 * if `len' isn't given use the fd in `n'.
 */

int
mnode_update_size (struct mnode *n, int32_t *len)
{
    int ret;

    if (len) {
	n->fs.Length = n->sb.st_size = *len;
    } else {
	assert (n->flags.fdp);
    	
	ret = fstat (n->fd, &n->sb);
	if (ret)
	    return errno;
	n->flags.sbp = TRUE;
	n->fs.Length = n->sb.st_size;
    }

    return 0;
}

/*
 * Update the size in `n' if the stat information in `n->sb' has
 * changed. If there is no stat information call ``vld_update_size''.
 */

int
mnode_update_size_cached (struct mnode *n)
{
    if (n->flags.sbp == FALSE) {
	assert (n->flags.fdp);
	return mnode_update_size (n, NULL);
    }

    n->fs.Length = n->sb.st_size;
    return 0;
}
 
