/*	$OpenBSD: xfs_node.h,v 1.2 1998/08/31 05:13:28 art Exp $	*/
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

/* 	$KTH: xfs_node.h,v 1.3 1998/07/13 20:36:37 art Exp $	 */

#ifndef _XFS_XFS_NODE_H_
#define _XFS_XFS_NODE_H_

#include <sys/types.h>
#include <sys/time.h>
#include <sys/vnode.h>

#include <xfs/xfs_attr.h>
#include <xfs/xfs_message.h>

struct xfs_node {
	struct vnode	*vn;
	struct vnode	*data;
	struct vattr	attr;
	u_int		flags;
	u_int		tokens;
	xfs_handle	handle;
	pag_t		id[MAXRIGHTS];
	u_char		rights[MAXRIGHTS];
	u_char		anonrights;
};

#define DATA_FROM_VNODE(vp) DATA_FROM_XNODE(VNODE_TO_XNODE(vp))
#define DATA_FROM_XNODE(xp) ((xp)->data)

#define XNODE_TO_VNODE(xp) ((xp)->vn)
#define VNODE_TO_XNODE(vp) ((struct xfs_node *) (vp)->v_data)

#endif
