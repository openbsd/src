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

/*
 *
 */

/* $KTH: messages.h,v 1.17 2001/01/07 18:32:49 lha Exp $ */

#ifndef _MESSAGES_H_
#define _MESSAGES_H_

void xfs_message_init (void);
int xfs_message_receive (int fd, struct xfs_message_header *h, u_int size);
void break_callback (FCacheEntry *e);
void install_attr (FCacheEntry *e, int flags);

long afsfid2inode(const VenusFid *fid);

int
xfs_attr2afsstorestatus(struct xfs_attr *xa,
			AFSStoreStatus *storestatus);

void
update_fid(VenusFid oldfid, FCacheEntry *old_entry,
	   VenusFid newfid, FCacheEntry *new_entry);

enum { FCACHE2XFSNODE_ATTR = 1,
       FCACHE2XFSNODE_RIGHT = 2 } ;

#define FCACHE2XFSNODE_ALL (FCACHE2XFSNODE_ATTR|FCACHE2XFSNODE_RIGHT)

void
fcacheentry2xfsnode (const VenusFid *fid,
		     const VenusFid *statfid, 
		     AFSFetchStatus *status,
		     struct xfs_msg_node *node,
                     AccessEntry *ae,
		     int flags);

int
VenusFid_cmp (const VenusFid *fid1, const VenusFid *fid2);

#endif /* _MESSAGES_H_ */
