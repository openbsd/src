/*
 * Copyright (c) 2002 - 2003, Stockholms Universitet
 * (Stockholm University, Stockholm Sweden)
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
 * 3. Neither the name of the university nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

#include <nnpfs/nnpfs_locl.h>
#include <nnpfs/nnpfs_common.h>
#include <nnpfs/nnpfs_fs.h>
#include <nnpfs/nnpfs_deb.h>
#include <nnpfs/nnpfs_node.h>
#include <nnpfs/nnpfs_vnodeops.h>
#include <nnpfs/nnpfs_queue.h>

RCSID("$arla: nnpfs_node.c,v 1.3 2003/02/06 12:56:09 lha Exp $");

#define nnpfs_hash(node) \
  (((node)->a+(node)->b+(node)->c+(node)->d) % XN_HASHSIZE)

/*
 * Init the nnp node storage system
 */

void
nnfs_init_head(struct nnpfs_nodelist_head *head)
{
    int i;

    for (i = 0; i < XN_HASHSIZE; i++)
	NNPQUEUE_INIT(&head->nh_nodelist[i]);
}

/*
 * Tries to purge all nodes from the hashtable. Nodes that unpurgeable
 * (still used nodes) are given to proc for special termination
 * (conversion to dead node).
 */

void
nnpfs_node_purge(struct nnpfs_nodelist_head *head, 
		 void (*func)(struct nnpfs_node *))
{
    panic("nnpfs_node_purge");
}

/*
 * nnpfs_node_find returns the node with the handle `handlep'.
 */

struct nnpfs_node *
nnpfs_node_find(struct nnpfs_nodelist_head *head, nnpfs_handle *handlep)
{
    struct nh_node_list *h;
    struct nnpfs_node *nn;

    h = &head->nh_nodelist[nnpfs_hash(handlep)];

    NNPQUEUE_FOREACH(nn, h, nn_hash) {
	if (nnpfs_handle_eq(handlep, &nn->handle))
	    break;
    }

    return nn;
}

/*
 * Remove the node `node' from the node storage system.
 */

void
nnpfs_remove_node(struct nnpfs_nodelist_head *head, struct nnpfs_node *node)
{
    struct nh_node_list *h;

    h = &head->nh_nodelist[nnpfs_hash(&node->handle)];
    NNPQUEUE_REMOVE(node, h, nn_hash);
}

/*
 * Add the node `node' from the node storage system.
 */

void
nnpfs_insert(struct nnpfs_nodelist_head *head, struct nnpfs_node *node)
{
    struct nh_node_list *h;

    h = &head->nh_nodelist[nnpfs_hash(&node->handle)];
    NNPQUEUE_INSERT_HEAD(h, node, nn_hash);
}

/*
 * Update `old_handlep' in the node list `head' to `new_handlep'.
 */

int
nnpfs_update_handle(struct nnpfs_nodelist_head *head,
		    nnpfs_handle *old_handlep, nnpfs_handle *new_handlep)
{
    struct nnpfs_node *node;

    node = nnpfs_node_find(head, new_handlep);
    if (node)
	return EEXIST;
    node = nnpfs_node_find(head, old_handlep);
    if (node == NULL)
	return ENOENT;
    nnpfs_remove_node(head, node);
    node->handle = *new_handlep;
    nnpfs_insert(head, node);

    return 0;
}
