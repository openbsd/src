/*
 * Copyright (c) 1995-2001 Kungliga Tekniska Högskolan
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
RCSID("$KTH: messages.c,v 1.231.2.12 2001/10/19 04:25:52 ahltorp Exp $");

#include <xfs/xfs_message.h>

#ifdef HAVE_SYS_IOCTL_H
#include <sys/ioctl.h>
#endif
#include <kafs.h>

#include "messages.h"

/* XXX */
int Log_is_open;
DARLA_file log_data;

static int 
xfs_message_getroot (int, struct xfs_message_getroot*, u_int);

static int 
xfs_message_getnode (int, struct xfs_message_getnode*, u_int);

static int 
xfs_message_getattr (int, struct xfs_message_getattr*, u_int);

static int 
xfs_message_getdata (int, struct xfs_message_getdata*, u_int);

static int 
xfs_message_inactivenode (int,struct xfs_message_inactivenode*,u_int);

static int 
xfs_message_putdata (int fd, struct xfs_message_putdata *h, u_int size);

static int
xfs_message_putattr (int fd, struct xfs_message_putattr *h, u_int size);

static int
xfs_message_create (int fd, struct xfs_message_create *h, u_int size);

static int
xfs_message_mkdir (int fd, struct xfs_message_mkdir *h, u_int size);

static int
xfs_message_link (int fd, struct xfs_message_link *h, u_int size);

static int
xfs_message_symlink (int fd, struct xfs_message_symlink *h, u_int size);

static int
xfs_message_remove (int fd, struct xfs_message_remove *h, u_int size);

static int
xfs_message_rmdir (int fd, struct xfs_message_rmdir *h, u_int size);

static int
xfs_message_rename (int fd, struct xfs_message_rename *h, u_int size);

static int
xfs_message_pioctl (int fd, struct xfs_message_pioctl *h, u_int size) ;


xfs_message_function rcvfuncs[] = {
NULL,						/* version */
(xfs_message_function)xfs_message_wakeup,	/* wakeup */
(xfs_message_function)xfs_message_getroot,	/* getroot */
NULL,						/* installroot */
(xfs_message_function)xfs_message_getnode, 	/* getnode */
NULL,						/* installnode */
(xfs_message_function)xfs_message_getattr,	/* getattr */
NULL,						/* installattr */
(xfs_message_function)xfs_message_getdata,	/* getdata */
NULL,						/* installdata */
(xfs_message_function)xfs_message_inactivenode,	/* inactivenode */
NULL,						/* invalidnode */
(xfs_message_function)xfs_message_getdata,	/* open */
(xfs_message_function)xfs_message_putdata,      /* put_data */
(xfs_message_function)xfs_message_putattr,      /* put attr */
(xfs_message_function)xfs_message_create,       /* create */
(xfs_message_function)xfs_message_mkdir,	/* mkdir */
(xfs_message_function)xfs_message_link,		/* link */
(xfs_message_function)xfs_message_symlink,      /* symlink */
(xfs_message_function)xfs_message_remove,	/* remove */
(xfs_message_function)xfs_message_rmdir,	/* rmdir */
(xfs_message_function)xfs_message_rename,	/* rename */
(xfs_message_function)xfs_message_pioctl,	/* pioctl */
NULL,	                                        /* wakeup_data */
NULL,						/* updatefid */
NULL,						/* advlock */
NULL						/* gc nodes */
};


/*
 * Return 0 if ``fid1'' eq ``fid2''.
 */

int
VenusFid_cmp (const VenusFid *fid1, const VenusFid *fid2)
{
    if (fid1->Cell == fid2->Cell &&
	fid1->fid.Volume == fid2->fid.Volume &&
	fid1->fid.Vnode == fid2->fid.Vnode &&
	fid1->fid.Unique == fid2->fid.Unique)
	return 0;
    return 1;
}


/*
 *
 */

long
afsfid2inode (const VenusFid *fid)
{
    return ((fid->fid.Volume & 0x7FFF) << 16 | (fid->fid.Vnode & 0xFFFFFFFF));
}

/*
 * AFSFetchStatus -> xfs_attr
 */

static void
afsstatus2xfs_attr (AFSFetchStatus *status,
		    const VenusFid *fid,
		    struct xfs_attr *attr)
{
     attr->valid = XA_V_NONE;
     switch (status->FileType) {
	  case TYPE_FILE :
	       XA_SET_MODE(attr, S_IFREG);
	       XA_SET_TYPE(attr, XFS_FILE_REG);
	       XA_SET_NLINK(attr, status->LinkCount);
	       break;
	  case TYPE_DIR :
	       XA_SET_MODE(attr, S_IFDIR);
	       XA_SET_TYPE(attr, XFS_FILE_DIR);
	       XA_SET_NLINK(attr, status->LinkCount);
	       break;
	  case TYPE_LINK :
	       XA_SET_MODE(attr, S_IFLNK);
	       XA_SET_TYPE(attr, XFS_FILE_LNK);
	       XA_SET_NLINK(attr, status->LinkCount);
	       break;
	  default :
	       arla_warnx (ADEBMSG, "afsstatus2xfs_attr: default");
	       abort ();
     }
     XA_SET_SIZE(attr, status->Length);
     XA_SET_UID(attr,status->Owner);
     XA_SET_GID(attr, status->Group);
     attr->xa_mode  |= status->UnixModeBits;
     XA_SET_ATIME(attr, status->ClientModTime);
     XA_SET_MTIME(attr, status->ClientModTime);
     XA_SET_CTIME(attr, status->ClientModTime);
     XA_SET_FILEID(attr, afsfid2inode(fid));
}

/*
 * Transform `access', `FileType' and `UnixModeBits' into rights.
 *
 * There are different transformations for directories and files to be
 * compatible with the Transarc client.
 */

static u_char
afsrights2xfsrights(u_long ar, u_int32_t FileType, u_int32_t UnixModeBits)
{
    u_char ret = 0;

    if (FileType == TYPE_DIR) {
	if (ar & ALIST)
	    ret |= XFS_RIGHT_R | XFS_RIGHT_X;
	if (ar & (AINSERT | ADELETE))
	    ret |= XFS_RIGHT_W;
    } else {
	if (FileType == TYPE_LINK && (ar & ALIST))
	    ret |= XFS_RIGHT_R;
	if ((ar & AREAD) && (UnixModeBits & S_IRUSR))
	    ret |= XFS_RIGHT_R;
	if ((ar & AWRITE) && (UnixModeBits & S_IWUSR))
	    ret |= XFS_RIGHT_W;
	if ((ar & AREAD) && (UnixModeBits & S_IXUSR))
	    ret |= XFS_RIGHT_X;
    }

    return ret;
}

void
fcacheentry2xfsnode (const VenusFid *fid,
		     const VenusFid *statfid, 
		     AFSFetchStatus *status,
		     struct xfs_msg_node *node,
                     AccessEntry *ae,
		     int flags)
{
    int i;

    memcpy (&node->handle, fid, sizeof(*fid));
    if (flags & FCACHE2XFSNODE_ATTR)
	afsstatus2xfs_attr (status, statfid, &node->attr);
    if (flags & FCACHE2XFSNODE_RIGHT) {
	node->anonrights = afsrights2xfsrights(status->AnonymousAccess,
					       status->FileType,
					       status->UnixModeBits);
	for (i = 0; i < NACCESS; i++) {
	    node->id[i] = ae[i].cred;
	    node->rights[i] = afsrights2xfsrights(ae[i].access,
						  status->FileType,
						  status->UnixModeBits);
	}
    }
}

/*
 * convert `xa' into `storestatus'
 */

int
xfs_attr2afsstorestatus(struct xfs_attr *xa,
			AFSStoreStatus *storestatus)
{
    int mask = 0;

    if (XA_VALID_MODE(xa)) {
	storestatus->UnixModeBits = xa->xa_mode;
	mask |= SS_MODEBITS;
    }
    if (XA_VALID_UID(xa)) {
	storestatus->Owner = xa->xa_uid;
	mask |= SS_OWNER;
    }
    if (XA_VALID_GID(xa)) {
	storestatus->Group = xa->xa_gid;
	mask |= SS_GROUP;
    }
    if (XA_VALID_MTIME(xa)) {
	storestatus->ClientModTime = xa->xa_mtime;
	mask |= SS_MODTIME;
    }
    storestatus->Mask = mask;

    /* SS_SegSize */
    storestatus->SegSize = 0;
    return 0;
}

/*
 * Return true iff we should retry the operation.
 * Also replace `ce' with anonymous creds in case it has expired.
 *
 * There must not be passed in any NULL pointers.
 */

static int
try_again (int *ret, CredCacheEntry **ce, xfs_cred *cred, const VenusFid *fid)
{
    switch (*ret) {
#ifdef KERBEROS
    case RXKADEXPIRED : 
    case RXKADUNKNOWNKEY: {
	int32_t cell = (*ce)->cell;

	conn_clearcred (CONN_CS_CRED|CONN_CS_SECIDX, 0, cred->pag, 2);
	cred_expire (*ce);
	cred_free (*ce);
	*ce = cred_get (cell, cred->pag, CRED_ANY);
	assert (*ce != NULL);
	return TRUE;
    }
    case RXKADSEALEDINCON :
	arla_warnx_with_fid (ADEBWARN, fid,
			     "seal error");
	*ret = EINVAL;
	return FALSE;
#endif	 
    case ARLA_VSALVAGE :
	*ret = EIO;
	return FALSE;
    case ARLA_VNOVNODE :
	*ret = ENOENT;
	return FALSE;
    case ARLA_VMOVED :
    case ARLA_VNOVOL :
	if (fid && !volcache_reliable (fid->fid.Volume, fid->Cell)) {
	    return TRUE;
	} else {
	    *ret = ENOENT;
	    return FALSE;
	}
    case ARLA_VOFFLINE :
	*ret = ENETDOWN;
	return FALSE;
    case ARLA_VDISKFULL :
	*ret = ENOSPC;
	return FALSE;
    case ARLA_VOVERQUOTA:
#ifdef EDQUOT
	*ret = EDQUOT;
#else
	*ret = ENOSPC;
#endif
	return FALSE;
    case ARLA_VBUSY :
	arla_warnx_with_fid (ADEBWARN, fid,
			     "Waiting for busy volume...");
	IOMGR_Sleep (afs_BusyWaitPeriod);
	return TRUE;
    case ARLA_VRESTARTING:
	arla_warnx_with_fid (ADEBWARN, fid,
			     "Waiting for fileserver to restart...");
	IOMGR_Sleep (afs_BusyWaitPeriod);
	return TRUE;
    case ARLA_VIO :
	*ret = EIO;
	return FALSE;
    default :
	return FALSE;
    }
}

/*
 * Fetch data and retry if failing
 */

static int
message_get_data (FCacheEntry **entry, VenusFid *fid, 
		  struct xfs_cred *cred, CredCacheEntry **ce)
{
    int ret;
    do {
	ret = fcache_get_data (entry, fid, ce);
    } while (try_again (&ret, ce, cred, fid));
    return ret;
}

/*
 *
 */

static int
xfs_message_getroot (int fd, struct xfs_message_getroot *h, u_int size)
{
     struct xfs_message_installroot msg;
     int ret = 0;
     VenusFid root_fid;
     VenusFid real_fid;
     AFSFetchStatus status;
     Result res;
     CredCacheEntry *ce;
     AccessEntry *ae;
     struct xfs_message_header *h0 = NULL;
     size_t h0_len = 0;
     int32_t cell_id = cell_name2num(cell_getthiscell());

     ce = cred_get (cell_id, h->cred.pag, CRED_ANY);
     assert (ce != NULL);
     do {
	 ret = getroot (&root_fid, ce);

	 if (ret == 0) {
	     res = cm_getattr(root_fid, &status, &real_fid, ce, &ae);
	     if (res.res)
		 ret = res.error;
	     else
		 ret = res.res;
	 }
     } while (try_again (&ret, &ce, &h->cred, &root_fid));

     if (ret == 0) {
	 fcacheentry2xfsnode (&root_fid, &real_fid,
			      &status, &msg.node, ae,
			      FCACHE2XFSNODE_ALL);

	 msg.node.tokens = res.tokens & ~XFS_DATA_MASK;
	 msg.header.opcode = XFS_MSG_INSTALLROOT;
	 h0 = (struct xfs_message_header *)&msg;
	 h0_len = sizeof(msg);
     }

     cred_free (ce);
     xfs_send_message_wakeup_multiple (fd,
				       h->header.sequence_num,
				       ret,
				       h0, h0_len,
				       NULL, 0);
     return 0;
}

static int
xfs_message_getnode (int fd, struct xfs_message_getnode *h, u_int size)
{
     struct xfs_message_installnode msg;
     VenusFid *dirfid = (VenusFid *)&h->parent_handle;
     VenusFid fid;
     VenusFid real_fid;
     Result res;
     AFSFetchStatus status;
     CredCacheEntry *ce;
     AccessEntry *ae;
     struct xfs_message_header *h0 = NULL;
     size_t h0_len = 0;
     int ret;
     const VenusFid *report_fid = NULL;


     arla_warnx (ADEBMSG, "getnode (%ld.%lu.%lu.%lu) \"%s\"",
		 (long)dirfid->Cell, (unsigned long)dirfid->fid.Volume,
		 (unsigned long)dirfid->fid.Vnode,
		 (unsigned long)dirfid->fid.Unique, h->name);

     ce = cred_get (dirfid->Cell, h->cred.pag, CRED_ANY);
     assert (ce != NULL);

     report_fid = dirfid;
     do {
  	 res = cm_lookup (dirfid, h->name, &fid, &ce, TRUE);
	 if (res.res == 0) {
	     res = cm_getattr (fid, &status, &real_fid, ce, &ae);
	     report_fid = &fid;
	 }

	 if (res.res)
	     ret = res.error;
	 else
	     ret = res.res;
     } while (try_again (&ret, &ce, &h->cred, report_fid));

     if (ret == 0) {
	 fcacheentry2xfsnode (&fid, &real_fid, &status, &msg.node, ae,
			      FCACHE2XFSNODE_ALL);

	 msg.node.tokens = res.tokens & ~XFS_DATA_MASK;
	 msg.parent_handle = h->parent_handle;
	 strlcpy (msg.name, h->name, sizeof(msg.name));

	 msg.header.opcode = XFS_MSG_INSTALLNODE;
	 h0 = (struct xfs_message_header *)&msg;
	 h0_len = sizeof(msg);
     }

     cred_free (ce);
     xfs_send_message_wakeup_multiple (fd,
				       h->header.sequence_num,
				       ret,
				       h0, h0_len,
				       NULL, 0);
     return 0;
}

static int
xfs_message_getattr (int fd, struct xfs_message_getattr *h, u_int size)
{
     struct xfs_message_installattr msg;
     VenusFid *fid;
     VenusFid real_fid;
     AFSFetchStatus status;
     Result res;
     CredCacheEntry *ce;
     AccessEntry *ae;
     struct xfs_message_header *h0 = NULL;
     size_t h0_len = 0;
     int ret;

     fid = (VenusFid *)&h->handle;
     arla_warnx (ADEBMSG, "getattr (%ld.%lu.%lu.%lu)",
		 (long)fid->Cell, (unsigned long)fid->fid.Volume,
		 (unsigned long)fid->fid.Vnode,
		 (unsigned long)fid->fid.Unique);
     ce = cred_get (fid->Cell, h->cred.pag, CRED_ANY);
     assert (ce != NULL);

     do {
	 res = cm_getattr (*fid, &status, &real_fid, ce, &ae);
	 if (res.res)
	     ret = res.error;
	 else
	     ret = res.res;
     } while (try_again (&ret, &ce, &h->cred, fid));

     if (ret == 0) {
	 fcacheentry2xfsnode (fid, &real_fid, &status, &msg.node, ae,
			      FCACHE2XFSNODE_ALL);
	 
	 msg.node.tokens = res.tokens;
	 msg.header.opcode = XFS_MSG_INSTALLATTR;
	 h0 = (struct xfs_message_header *)&msg;
	 h0_len = sizeof(msg);
     }

     cred_free (ce);
     xfs_send_message_wakeup_multiple (fd,
				       h->header.sequence_num,
				       ret,
				       h0, h0_len,
				       NULL, 0);

     return 0;
}

static int 
xfs_message_putattr (int fd, struct xfs_message_putattr *h, u_int size)
{
     struct xfs_message_installattr msg;
     VenusFid *fid;
     AFSStoreStatus status;
     AFSFetchStatus fetch_status;
     Result res;
     CredCacheEntry *ce;
     AccessEntry *ae;
     VenusFid real_fid;
     struct xfs_message_header *h0 = NULL;
     size_t h0_len = 0;
     int ret;
     struct vcache log_cache;
     FCacheEntry *fce;
     int log_err;

     fid = (VenusFid *)&h->handle;
     arla_warnx (ADEBMSG, "putattr (%ld.%lu.%lu.%lu)",
		 (long)fid->Cell, (unsigned long)fid->fid.Volume,
		 (unsigned long)fid->fid.Vnode,
		 (unsigned long)fid->fid.Unique);
     xfs_attr2afsstorestatus(&h->attr, &status);
     ce = cred_get (fid->Cell, h->cred.pag, CRED_ANY);
     assert (ce != NULL);

     if (connected_mode != CONNECTED) {
	 ret = fcache_find (&fce, *fid);
	 ReleaseWriteLock (&fce->lock);

	 log_cache.fid = *fid;
	 log_cache.DataVersion  = fce->status.DataVersion;
	 log_cache.cred = h->cred;
     }

     do {
	 res.res = 0;
	 if (XA_VALID_SIZE(&h->attr))
	     res = cm_ftruncate (*fid, h->attr.xa_size, ce);

	 if (res.res == 0)
	     res = cm_setattr(*fid, &status, ce);

	 if (res.res)
	     ret = res.error;
	 else
	     ret = res.res;
     } while (try_again (&ret, &ce, &h->cred, fid));

     if (ret == 0) {
	 do {
	     res = cm_getattr (*fid, &fetch_status, &real_fid, ce, &ae);
	     if (res.res)
		 ret = res.error;
	     else
		 ret = res.res;
	 } while (try_again (&ret, &ce, &h->cred, fid));

	 if (ret == 0) {
	     fcacheentry2xfsnode (fid, &real_fid,
				  &fetch_status, &msg.node, ae,
				  FCACHE2XFSNODE_ALL);
	 
	     msg.node.tokens  = res.tokens;
	     msg.header.opcode = XFS_MSG_INSTALLATTR;
	     h0 = (struct xfs_message_header *)&msg;
	     h0_len = sizeof(msg);
	 }
     }

     if (connected_mode != CONNECTED && ret == 0) {
	 log_err = log_dis_setattr (&log_cache, &(h->attr));
     }

     cred_free (ce);
     xfs_send_message_wakeup_multiple (fd,
				       h->header.sequence_num, 
				       ret,
				       h0, h0_len,
				       NULL, 0);
     return 0;
}

static int 
xfs_message_create (int fd, struct xfs_message_create *h, u_int size)
{
     VenusFid *parent_fid, child_fid;
     AFSStoreStatus store_status;
     AFSFetchStatus fetch_status;
     Result res;
     CredCacheEntry *ce;
     int ret;
     struct xfs_message_installdata msg1;
     struct xfs_message_installnode msg2;
     struct xfs_message_installdata msg3;
     struct xfs_message_header *h0 = NULL;
     size_t h0_len = 0;
     struct xfs_message_header *h1 = NULL;
     size_t h1_len = 0;
     struct xfs_message_header *h2 = NULL;
     size_t h2_len = 0;
     FCacheEntry *dir_entry   = NULL;
     FCacheEntry *child_entry = NULL;
     fcache_cache_handle cache_handle;

     parent_fid = (VenusFid *)&h->parent_handle;
     arla_warnx (ADEBMSG, "create (%ld.%lu.%lu.%lu) \"%s\"",
		 (long)parent_fid->Cell,
		 (unsigned long)parent_fid->fid.Volume,
		 (unsigned long)parent_fid->fid.Vnode,
		 (unsigned long)parent_fid->fid.Unique, h->name);

     xfs_attr2afsstorestatus(&h->attr, &store_status);
     if (connected_mode != CONNECTED) {
	 if (!(store_status.Mask & SS_OWNER)) {
	     store_status.Owner = h->cred.uid;
	     store_status.Mask |= SS_OWNER;
	 }
	 if (!(store_status.Mask & SS_MODTIME)) {
	     struct timeval now;

	     gettimeofday (&now, NULL);

	     store_status.ClientModTime = now.tv_sec;
	     store_status.Mask |= SS_MODTIME;
	 }
     }
     ce = cred_get (parent_fid->Cell, h->cred.pag, CRED_ANY);
     assert (ce != NULL);

     do {
	 res = cm_create(parent_fid, h->name, &store_status,
			 &child_fid, &fetch_status, &ce);

	 if (res.res)
	     ret = res.error;
	 else
	     ret = res.res;
     } while (try_again (&ret, &ce, &h->cred, parent_fid));

     if (res.res == 0) {

	 ret = message_get_data (&dir_entry, parent_fid, &h->cred, &ce);
	 if (ret)
	     goto out;
	     
	 res = conv_dir (dir_entry, ce, 0,
			 &cache_handle,
			 msg1.cache_name,
			 sizeof(msg1.cache_name));
	 if (res.res == -1) {
	     ret = res.error;
	     goto out;
	 }
	 msg1.cache_handle = cache_handle.xfs_handle;
	 msg1.flag = 0;
	 if (cache_handle.valid)
	     msg1.flag |= XFS_ID_HANDLE_VALID;

	 msg1.node.tokens = res.tokens;

	 fcacheentry2xfsnode (parent_fid,
			      fcache_realfid(dir_entry),
			      &dir_entry->status,
			      &msg1.node, 
			      dir_entry->acccache,
			      FCACHE2XFSNODE_ALL);

	 ret = message_get_data (&child_entry, &child_fid, &h->cred, &ce);
	 if (ret)
	     goto out;

	 msg3.cache_handle = child_entry->handle.xfs_handle;
	 fcache_file_name (child_entry,
			   msg3.cache_name, sizeof(msg3.cache_name));
	 msg3.flag = 0;
	 if (cache_handle.valid)
	     msg3.flag |= XFS_ID_HANDLE_VALID;

	 child_entry->flags.kernelp = TRUE;
	 child_entry->flags.attrusedp = TRUE;
	 child_entry->flags.datausedp = TRUE;
#if 0
	 assert(dir_entry->flags.attrusedp);
#endif
	 dir_entry->flags.datausedp = TRUE;

	 msg1.header.opcode = XFS_MSG_INSTALLDATA;
	 h0 = (struct xfs_message_header *)&msg1;
	 h0_len = sizeof(msg1);

	 fcacheentry2xfsnode (&child_fid, &child_fid,
			      &fetch_status, &msg2.node, dir_entry->acccache,
			      FCACHE2XFSNODE_ALL);
			      
	 msg2.node.tokens   = XFS_ATTR_R 
	     | XFS_OPEN_NW | XFS_OPEN_NR
	     | XFS_DATA_R | XFS_DATA_W;
	 msg2.parent_handle = h->parent_handle;
	 strlcpy (msg2.name, h->name, sizeof(msg2.name));

	 msg2.header.opcode = XFS_MSG_INSTALLNODE;
	 h1 = (struct xfs_message_header *)&msg2;
	 h1_len = sizeof(msg2);

	 msg3.node          = msg2.node;
	 msg3.header.opcode = XFS_MSG_INSTALLDATA;

	 h2 = (struct xfs_message_header *)&msg3;
	 h2_len = sizeof(msg3);
     }

     if (connected_mode != CONNECTED && res.res == 0) {
	 struct vcache log_ent_parent, log_ent_child;

	 log_ent_parent.fid = *parent_fid;
	 log_ent_parent.DataVersion = dir_entry->status.DataVersion;
	 log_ent_parent.cred = h->cred;

	 log_ent_child.fid = child_fid;
	 log_ent_child.DataVersion = 1;

	 log_dis_create (&log_ent_parent, &log_ent_child, h->name);
     }

out:
     if (dir_entry)
	 fcache_release(dir_entry);
     if (child_entry)
	 fcache_release(child_entry);
     cred_free (ce);
     xfs_send_message_wakeup_multiple (fd,
				       h->header.sequence_num,
				       ret,
				       h0, h0_len,
				       h1, h1_len,
				       h2, h2_len,
				       NULL, 0);

     return ret;
}

static int 
xfs_message_mkdir (int fd, struct xfs_message_mkdir *h, u_int size)
{
     VenusFid *parent_fid, child_fid;
     AFSStoreStatus store_status;
     AFSFetchStatus fetch_status;
     Result res;
     CredCacheEntry *ce;
     int ret;
     struct xfs_message_installdata msg1;
     struct xfs_message_installnode msg2;
     struct xfs_message_installdata msg3;
     FCacheEntry *dir_entry = NULL;
     FCacheEntry *child_entry = NULL;

     struct xfs_message_header *h0 = NULL;
     size_t h0_len = 0;
     struct xfs_message_header *h1 = NULL;
     size_t h1_len = 0;
     struct xfs_message_header *h2 = NULL;
     size_t h2_len = 0;
     fcache_cache_handle cache_handle;

     struct vcache log_ent_parent, log_ent_child;
     FCacheEntry parent_entry;
     AFSStoreStatus log_store_status;

#if 0
     parent_fid = fid_translate((VenusFid *)&h->parent_handle);
#else
     parent_fid = (VenusFid *)&h->parent_handle;
#endif
     arla_warnx (ADEBMSG, "mkdir (%ld.%lu.%lu.%lu) \"%s\"",
		 (long)parent_fid->Cell, (unsigned long)parent_fid->fid.Volume,
		 (unsigned long)parent_fid->fid.Vnode,
		 (unsigned long)parent_fid->fid.Unique, h->name);

     ce = cred_get (parent_fid->Cell, h->cred.pag, CRED_ANY);
     assert (ce != NULL);

     xfs_attr2afsstorestatus(&h->attr, &store_status);
     if (connected_mode != CONNECTED) {
	 if (!(store_status.Mask & SS_OWNER)) {
	     store_status.Owner = h->cred.uid;
	     store_status.Mask |= SS_OWNER;
	 }
	 if (!(store_status.Mask & SS_MODTIME)) {
	     struct timeval now;

	     gettimeofday (&now, NULL);

	     store_status.ClientModTime = now.tv_sec;
	     store_status.Mask |= SS_MODTIME;
	 }
     }

     do {
	 res = cm_mkdir(parent_fid, h->name, &store_status,
			&child_fid, &fetch_status, &ce);
	 if (res.res)
	     ret = res.error;
	 else
	     ret = res.res;
     } while(try_again (&ret, &ce, &h->cred, parent_fid));

     if (res.res == 0) {

	 ret = message_get_data (&dir_entry, parent_fid, &h->cred, &ce);
	 if (ret)
	     goto out;

	 res = conv_dir (dir_entry, ce, 0,
			 &cache_handle,
			 msg1.cache_name,
			 sizeof(msg1.cache_name));
	 if (res.res == -1) {
	     ret = res.error;
	     goto out;
	 }
	 msg1.cache_handle = cache_handle.xfs_handle;
	 msg1.flag = 0;
	 if (cache_handle.valid)
	     msg1.flag |= XFS_ID_HANDLE_VALID;
	 msg1.node.tokens = res.tokens;

	 fcacheentry2xfsnode (parent_fid,
			      fcache_realfid(dir_entry),
			      &dir_entry->status, &msg1.node, 
			      dir_entry->acccache,
			      FCACHE2XFSNODE_ALL);
	 
	 msg1.header.opcode = XFS_MSG_INSTALLDATA;
	 h0 = (struct xfs_message_header *)&msg1;
	 h0_len = sizeof(msg1);

	 ret = message_get_data (&child_entry, &child_fid, &h->cred, &ce);
	 if (ret)
	     goto out;

	 res = conv_dir (child_entry, ce, 0,
			 &cache_handle,
			 msg3.cache_name,
			 sizeof(msg3.cache_name));
	 if (res.res == -1) {
	     ret = res.error;
	     goto out;
	 }
	 msg3.cache_handle = cache_handle.xfs_handle;
	 msg3.flag = 0;
	 if (cache_handle.valid)
	     msg3.flag |= XFS_ID_HANDLE_VALID;

	 child_entry->flags.attrusedp = TRUE;
	 child_entry->flags.datausedp = TRUE;
#if 0
	 assert(dir_entry->flags.attrusedp);
#endif
	 dir_entry->flags.datausedp = TRUE;

	 msg2.node.tokens = res.tokens;

	 fcacheentry2xfsnode (&child_fid, &child_fid,
			      &child_entry->status, &msg2.node,
			      dir_entry->acccache,
			      FCACHE2XFSNODE_ALL);
			      
	 msg2.parent_handle = h->parent_handle;
	 strlcpy (msg2.name, h->name, sizeof(msg2.name));

	 msg2.header.opcode = XFS_MSG_INSTALLNODE;
	 h1 = (struct xfs_message_header *)&msg2;
	 h1_len = sizeof(msg2);

	 msg3.header.opcode = XFS_MSG_INSTALLDATA;
	 msg3.node = msg2.node;
	 h2 = (struct xfs_message_header *)&msg3;
	 h2_len = sizeof(msg3);
	 if (connected_mode != CONNECTED)
	     parent_entry = *dir_entry;
     }

     if (connected_mode != CONNECTED) {
	 log_ent_parent.fid = *parent_fid;
	 log_ent_parent.DataVersion = parent_entry.status.DataVersion;
	 log_ent_parent.cred = h->cred;
	 log_store_status = store_status;
	 log_ent_child.fid = child_fid;
	 log_ent_child.DataVersion = 1;
	 log_dis_mkdir (&log_ent_parent, &log_ent_child, &log_store_status,
			h->name);
     }

out:
     if (child_entry) 
	 fcache_release(child_entry);
     if (dir_entry)
	 fcache_release(dir_entry);
     cred_free (ce);
     xfs_send_message_wakeup_multiple (fd,
				       h->header.sequence_num,
				       ret,
				       h0, h0_len,
				       h1, h1_len,
				       h2, h2_len,
				       NULL, 0);

     return ret;
}

static int 
xfs_message_link (int fd, struct xfs_message_link *h, u_int size)
{
     VenusFid *parent_fid, *existing_fid;
     AFSFetchStatus fetch_status;
     Result res;
     CredCacheEntry *ce;
     int ret;
     struct xfs_message_installdata msg1;
     struct xfs_message_installnode msg2;
     struct xfs_message_header *h0 = NULL;
     size_t h0_len = 0;
     struct xfs_message_header *h1 = NULL;
     size_t h1_len = 0;
     fcache_cache_handle cache_handle;

     parent_fid   = (VenusFid *)&h->parent_handle;
     existing_fid = (VenusFid *)&h->from_handle;
     arla_warnx (ADEBMSG, "link (%ld.%lu.%lu.%lu) (%ld.%lu.%lu.%lu) \"%s\"",
		 (long)parent_fid->Cell, (unsigned long)parent_fid->fid.Volume,
		 (unsigned long)parent_fid->fid.Vnode,
		 (unsigned long)parent_fid->fid.Unique,
		 (long)existing_fid->Cell,
		 (unsigned long)existing_fid->fid.Volume,
		 (unsigned long)existing_fid->fid.Vnode,
		 (unsigned long)existing_fid->fid.Unique,
		 h->name);

     ce = cred_get (parent_fid->Cell, h->cred.pag, CRED_ANY);
     assert (ce != NULL);

     do {
	 res = cm_link (parent_fid, h->name, *existing_fid,
			&fetch_status, &ce);
	 if (res.res)
	     ret = res.error;
	 else
	     ret = res.res;
     } while (try_again (&ret, &ce, &h->cred, parent_fid));

     if (res.res == 0) {
	 FCacheEntry *dir_entry;

	 ret = message_get_data (&dir_entry, parent_fid, &h->cred, &ce);
	 if (ret)
	     goto out;

	 res = conv_dir (dir_entry, ce, 0,
			 &cache_handle,
			 msg1.cache_name,
			 sizeof(msg1.cache_name));
	 if (res.res == -1) {
	     fcache_release(dir_entry);
	     ret = res.error;
	     goto out;
	 }
	 msg1.cache_handle = cache_handle.xfs_handle;
	 msg1.flag = 0;
	 if (cache_handle.valid)
	     msg1.flag |= XFS_ID_HANDLE_VALID;
	 msg1.node.tokens = res.tokens;
#if 0
	 assert(dir_entry->flags.attrusedp);
#endif
	 dir_entry->flags.datausedp = TRUE;

	 fcacheentry2xfsnode (parent_fid,
			      fcache_realfid(dir_entry),
			      &dir_entry->status, &msg1.node,
			      dir_entry->acccache,
			      FCACHE2XFSNODE_ALL);

	 msg1.header.opcode = XFS_MSG_INSTALLDATA;
	 h0 = (struct xfs_message_header *)&msg1;
	 h0_len = sizeof(msg1);

	 fcacheentry2xfsnode (existing_fid, existing_fid,
			      &fetch_status, &msg2.node,
			      dir_entry->acccache,
			      FCACHE2XFSNODE_ALL);
	 fcache_release(dir_entry);

	 msg2.node.tokens   = XFS_ATTR_R; /* XXX */
	 msg2.parent_handle = h->parent_handle;
	 strlcpy (msg2.name, h->name, sizeof(msg2.name));

	 msg2.header.opcode = XFS_MSG_INSTALLNODE;
	 h1 = (struct xfs_message_header *)&msg2;
	 h1_len = sizeof(msg2);
     }

out:
     cred_free (ce);
     xfs_send_message_wakeup_multiple (fd,
				       h->header.sequence_num,
				       ret,
				       h0, h0_len,
				       h1, h1_len,
				       NULL, 0);

     return ret;
}

static int 
xfs_message_symlink (int fd, struct xfs_message_symlink *h, u_int size)
{
     VenusFid *parent_fid, child_fid, real_fid;
     AFSStoreStatus store_status;
     AFSFetchStatus fetch_status;
     Result res;
     CredCacheEntry *ce;
     int ret;
     struct xfs_message_installdata msg1;
     struct xfs_message_installnode msg2;
     struct xfs_message_header *h0 = NULL;
     size_t h0_len = 0;
     struct xfs_message_header *h1 = NULL;
     size_t h1_len = 0;
     fcache_cache_handle cache_handle;

     parent_fid = (VenusFid *)&h->parent_handle;
     arla_warnx (ADEBMSG, "symlink (%ld.%lu.%lu.%lu) \"%s\"",
		 (long)parent_fid->Cell, (unsigned long)parent_fid->fid.Volume,
		 (unsigned long)parent_fid->fid.Vnode,
		 (unsigned long)parent_fid->fid.Unique, h->name);

     ce = cred_get (parent_fid->Cell, h->cred.pag, CRED_ANY);
     assert (ce != NULL);

     xfs_attr2afsstorestatus(&h->attr, &store_status);

     do {
	 res = cm_symlink(parent_fid, h->name, &store_status,
			  &child_fid, &real_fid,
			  &fetch_status,
			  h->contents, &ce);
	 ret = res.error;
     } while (try_again (&ret, &ce, &h->cred, parent_fid));
     
     cred_free (ce);
     ce = cred_get (parent_fid->Cell, h->cred.pag, CRED_ANY);
     assert (ce != NULL);

     if (res.res == 0) {
	 FCacheEntry *dir_entry;

	 ret = message_get_data (&dir_entry, parent_fid, &h->cred, &ce);
	 if (ret)
	     goto out;

	 res = conv_dir (dir_entry, ce, 0,
			 &cache_handle,
			 msg1.cache_name,
			 sizeof(msg1.cache_name));
	 if (res.res == -1) {
	     fcache_release(dir_entry);
	     ret = res.error;
	     goto out;
	 }
	 msg1.cache_handle = cache_handle.xfs_handle;
	 msg1.flag = 0;
	 if (cache_handle.valid)
	     msg1.flag |= XFS_ID_HANDLE_VALID;
	 msg1.node.tokens = res.tokens;
#if 0
	 assert(dir_entry->flags.attrusedp);
#endif
	 dir_entry->flags.datausedp = TRUE;

	 fcacheentry2xfsnode (parent_fid,
			      fcache_realfid(dir_entry),
			      &dir_entry->status, &msg1.node,
			      dir_entry->acccache,
			      FCACHE2XFSNODE_ALL);
	 
	 msg1.header.opcode = XFS_MSG_INSTALLDATA;
	 h0 = (struct xfs_message_header *)&msg1;
	 h0_len = sizeof(msg1);

	 fcacheentry2xfsnode (&child_fid, &real_fid,
			      &fetch_status, &msg2.node,
			      dir_entry->acccache,
			      FCACHE2XFSNODE_ALL);
	 fcache_release(dir_entry);
			      
	 msg2.node.tokens   = XFS_ATTR_R; /* XXX */
	 msg2.parent_handle = h->parent_handle;
	 strlcpy (msg2.name, h->name, sizeof(msg2.name));

	 msg2.header.opcode = XFS_MSG_INSTALLNODE;
	 h1 = (struct xfs_message_header *)&msg2;
	 h1_len = sizeof(msg2);
     }

out:
     cred_free (ce);
     xfs_send_message_wakeup_multiple (fd,
				       h->header.sequence_num,
				       ret,
				       h0, h0_len,
				       h1, h1_len,
				       NULL, 0);
     return ret;
}

/* 
 * Handle the XFS remove message in `h', that is, remove name
 * `h->name' in directory `h->parent' with the creds from `h->cred'.
 */

static int 
xfs_message_remove (int fd, struct xfs_message_remove *h, u_int size)
{
     VenusFid *parent_fid;
     VenusFid fid;
     Result res;
     CredCacheEntry *ce;
     int ret;
     struct xfs_message_installdata msg1;
     struct xfs_message_installattr msg2;
     struct xfs_message_header *h0 = NULL;
     size_t h0_len = 0;
     struct xfs_message_header *h1 = NULL;
     size_t h1_len = 0;
     FCacheEntry *limbo_entry = NULL;
     unsigned link_count;
     FCacheEntry *dir_entry = NULL;
     AFSFetchStatus limbo_status;
     fcache_cache_handle cache_handle;

     parent_fid = (VenusFid *)&h->parent_handle;
     arla_warnx (ADEBMSG, "remove (%ld.%lu.%lu.%lu) \"%s\"",
		 (long)parent_fid->Cell, (unsigned long)parent_fid->fid.Volume,
		 (unsigned long)parent_fid->fid.Vnode,
		 (unsigned long)parent_fid->fid.Unique, h->name);

     ce = cred_get (parent_fid->Cell, h->cred.pag, CRED_ANY);
     assert (ce != NULL);

     do {
	 res = cm_lookup (parent_fid, h->name, &fid, &ce, FALSE);
	 if (res.res)
	     ret = res.error;
	 else
	     ret = res.res;
     } while (try_again (&ret, &ce, &h->cred, parent_fid));

     if (ret)
	 goto out;

     /*
      * Fetch the linkcount of the to be removed node
      */

     ret = fcache_get (&limbo_entry, fid, ce);
     if (ret)
	 goto out;

     ret = fcache_verify_attr (limbo_entry, NULL, NULL, ce);
     if (ret)
	 goto out;
     limbo_status = limbo_entry->status;
     link_count   = limbo_status.LinkCount;

     fcache_release (limbo_entry);
     limbo_entry = NULL;

     /*
      * Do the actual work
      */

     do {
	 res = cm_remove(parent_fid, h->name, &ce);
	 if (res.res)
	     ret = res.error;
	 else
	     ret = res.res;
     } while (try_again (&ret, &ce, &h->cred, parent_fid));

     if (ret == 0) {

	 ret = message_get_data (&dir_entry, parent_fid, &h->cred, &ce);
	 if (ret)
	     goto out;

	 if (!dir_entry->flags.extradirp
	     || dir_remove_name (dir_entry, h->name,
				 &cache_handle,
				 msg1.cache_name,
				 sizeof(msg1.cache_name))) {
	     res = conv_dir (dir_entry, ce, 0,
			     &cache_handle,
			     msg1.cache_name,
			     sizeof(msg1.cache_name));
	     if (res.res == -1) {
		 ret = res.error;
		 goto out;
	     }
	 }
	 msg1.cache_handle = cache_handle.xfs_handle;
	 msg1.flag = XFS_ID_INVALID_DNLC;
	 if (cache_handle.valid)
	     msg1.flag |= XFS_ID_HANDLE_VALID;
	 msg1.node.tokens = res.tokens | XFS_DATA_R;

	 fcacheentry2xfsnode (parent_fid,
			      fcache_realfid(dir_entry),
			      &dir_entry->status, &msg1.node,
			      dir_entry->acccache,
			      FCACHE2XFSNODE_ALL);
	 
	 msg1.header.opcode = XFS_MSG_INSTALLDATA;
	 h0 = (struct xfs_message_header *)&msg1;
	 h0_len = sizeof(msg1);

	 /*
	  * Set datausedp since we push data to kernel in out:
	  */

#if 0
	 assert(dir_entry->flags.attrusedp);
#endif
	 dir_entry->flags.datausedp = TRUE;

	 /*
	  * Make sure that if the removed node is in the
	  * kernel it has the right linkcount since some
	  * might hold a reference to it.
	  */

	 ret = fcache_get (&limbo_entry, fid, ce);
	 if (ret)
	     goto out;

	 /*
	  * Now insert the limbo entry to get right linkcount
	  */

	 ret = fcache_verify_attr (limbo_entry, dir_entry, NULL, ce);
	 if (ret == 0)
	     limbo_status = limbo_entry->status;
	 ret = 0;
	 
	 /* Only a silly rename when this is the last file */
	 if (link_count == 1)
	     limbo_entry->flags.silly = TRUE;

	 msg2.header.opcode = XFS_MSG_INSTALLATTR;
	 msg2.node.tokens   = limbo_entry->tokens;
	 if (!limbo_entry->flags.datausedp)
	     msg2.node.tokens &= ~XFS_DATA_MASK;

	 if (link_count == 1 && limbo_status.LinkCount == 1)
	     --limbo_status.LinkCount;
	 fcacheentry2xfsnode (&fid,
			      fcache_realfid(limbo_entry),
			      &limbo_status,
			      &msg2.node,
			      limbo_entry->acccache,
			      FCACHE2XFSNODE_ALL);
	 
	 h1 = (struct xfs_message_header *)&msg2;
	 h1_len = sizeof(msg2);
	 
     }

out:
     if (dir_entry)
	 fcache_release(dir_entry);
     if (limbo_entry)
	 fcache_release (limbo_entry);
     cred_free (ce);
     xfs_send_message_wakeup_multiple (fd,
				       h->header.sequence_num,
				       ret,
				       h0, h0_len,
				       h1, h1_len,
				       NULL, 0);
     return ret;
}

static int 
xfs_message_rmdir (int fd, struct xfs_message_rmdir *h, u_int size)
{
     VenusFid *parent_fid, fid;
     Result res;
     CredCacheEntry *ce;
     int ret;
     struct xfs_message_installdata msg0;
     struct xfs_message_header *h0 = NULL;
     size_t h0_len = 0;
     struct xfs_message_installattr msg1;
     struct xfs_message_header *h1 = NULL;
     size_t h1_len = 0;
     FCacheEntry *limbo_entry = NULL;
     FCacheEntry *dir_entry = NULL;
     unsigned link_count = 0;
     fcache_cache_handle cache_handle;

     parent_fid = (VenusFid *)&h->parent_handle;
     arla_warnx (ADEBMSG, "rmdir (%ld.%lu.%lu.%lu) \"%s\"",
		 (long)parent_fid->Cell, (unsigned long)parent_fid->fid.Volume,
		 (unsigned long)parent_fid->fid.Vnode,
		 (unsigned long)parent_fid->fid.Unique, h->name);

     ce = cred_get (parent_fid->Cell, h->cred.pag, CRED_ANY);
     assert (ce != NULL);

     /*
      * Fetch the child-entry fid.
      */

     do {
	 res = cm_lookup (parent_fid, h->name, &fid, &ce, FALSE);
	 if (res.res)
	     ret = res.error;
	 else
	     ret = res.res;
     } while (try_again (&ret, &ce, &h->cred, parent_fid));

     if (ret)
	 goto out;

     /*
      * Need to get linkcount for silly rename.
      */

     ret = fcache_get (&limbo_entry, fid, ce);
     if (ret)
	 goto out;

     ret = fcache_verify_attr (limbo_entry, NULL, NULL, ce);
     if (ret)
	 goto out;
     link_count = limbo_entry->status.LinkCount;

     fcache_release (limbo_entry);
     limbo_entry = NULL;

     /*
      * Do the actual work
      */

     do {
	 res = cm_rmdir(parent_fid, h->name, &ce);
	 if (res.res)
	     ret = res.error;
	 else
	     ret = res.res;
     } while (try_again (&ret, &ce, &h->cred, parent_fid));

     if (res.res == 0) {

	 ret = message_get_data (&dir_entry, parent_fid, &h->cred, &ce);
	 if (ret)
	     goto out;

	 if (!dir_entry->flags.extradirp
	     || dir_remove_name (dir_entry, h->name,
				 &cache_handle,
				 msg0.cache_name,
				 sizeof(msg0.cache_name))) {
	     res = conv_dir (dir_entry, ce, 0,
			     &cache_handle,
			     msg0.cache_name,
			     sizeof(msg0.cache_name));
	     if (res.res == -1) {
		 ret = res.error;
		 goto out;
	     }
	 }
	 msg0.cache_handle = cache_handle.xfs_handle;
	 msg0.flag = XFS_ID_INVALID_DNLC;
	 if (cache_handle.valid)
	     msg0.flag |= XFS_ID_HANDLE_VALID;

	 msg0.node.tokens = res.tokens;

	 fcacheentry2xfsnode (parent_fid,
			      fcache_realfid(dir_entry),
			      &dir_entry->status, &msg0.node,
			      dir_entry->acccache,
			      FCACHE2XFSNODE_ALL);
	 
	 msg0.header.opcode = XFS_MSG_INSTALLDATA;
	 h0 = (struct xfs_message_header *)&msg0;
	 h0_len = sizeof(msg0);

	 ret = fcache_get (&limbo_entry, fid, ce);
	 if (ret)
	     goto out;

	 /* Only silly rename when this is the last reference. */

	 if (link_count == 2)
	     limbo_entry->flags.silly = TRUE;

	 if (limbo_entry->flags.kernelp) {

	     ret = fcache_verify_attr (limbo_entry, NULL, NULL, ce);
	     if (ret)
		 goto out;

	     msg1.header.opcode = XFS_MSG_INSTALLATTR;
	     msg1.node.tokens   = limbo_entry->tokens;
	     if (!limbo_entry->flags.datausedp)
		 msg1.node.tokens &= ~XFS_DATA_MASK;

	     if (link_count == 2 && limbo_entry->status.LinkCount == 2)
		 limbo_entry->status.LinkCount = 0;
	     fcacheentry2xfsnode (&fid,
				  fcache_realfid(limbo_entry),
				  &limbo_entry->status,
				  &msg1.node,
				  limbo_entry->acccache,
				  FCACHE2XFSNODE_ALL);
	     
	     h1 = (struct xfs_message_header *)&msg1;
	     h1_len = sizeof(msg1);
	 }
#if 0
	 assert(dir_entry->flags.attrusedp);
#endif
	 dir_entry->flags.datausedp = TRUE;
     }

out:
     if (dir_entry)
	 fcache_release(dir_entry);
     if (limbo_entry)
	 fcache_release (limbo_entry);

     cred_free (ce);
     xfs_send_message_wakeup_multiple (fd,
				       h->header.sequence_num,
				       ret,
				       h0, h0_len,
				       h1, h1_len,
				       NULL, 0);
     return ret;
}

static int 
xfs_message_rename (int fd, struct xfs_message_rename *h, u_int size)
{
     VenusFid *old_parent_fid;
     VenusFid *new_parent_fid;
     VenusFid child_fid;
     Result res;
     CredCacheEntry *ce;
     int ret;
     struct xfs_message_installdata msg1;
     struct xfs_message_installdata msg2;
     struct xfs_message_installdata msg3;
     struct xfs_message_header *h0 = NULL;
     size_t h0_len = 0;
     struct xfs_message_header *h1 = NULL;
     size_t h1_len = 0;
     struct xfs_message_header *h2 = NULL;
     size_t h2_len = 0;
     FCacheEntry *old_entry   = NULL;
     FCacheEntry *new_entry   = NULL;
     FCacheEntry *child_entry = NULL;
     int update_child = 0;
     fcache_cache_handle cache_handle;

     old_parent_fid = (VenusFid *)&h->old_parent_handle;
     new_parent_fid = (VenusFid *)&h->new_parent_handle;
     arla_warnx (ADEBMSG,
		 "rename (%ld.%lu.%lu.%lu) (%ld.%lu.%lu.%lu) \"%s\" \"%s\"",
		 (long)old_parent_fid->Cell,
		 (unsigned long)old_parent_fid->fid.Volume,
		 (unsigned long)old_parent_fid->fid.Vnode,
		 (unsigned long)old_parent_fid->fid.Unique,
		 (long)new_parent_fid->Cell,
		 (unsigned long)new_parent_fid->fid.Volume,
		 (unsigned long)new_parent_fid->fid.Vnode,
		 (unsigned long)new_parent_fid->fid.Unique,
		 h->old_name,
		 h->new_name);

     ce = cred_get (old_parent_fid->Cell, h->cred.pag, CRED_ANY);
     assert (ce != NULL);

     do {
	 res = cm_rename(old_parent_fid, h->old_name,
			 new_parent_fid, h->new_name,
			 &child_fid, &update_child, &ce);
	 if (res.res)
	     ret = res.error;
	 else
	     ret = res.res;
     } while (try_again (&ret, &ce, &h->cred, old_parent_fid));

     if (res.res == 0) {

	 ret = message_get_data (&old_entry, old_parent_fid, &h->cred, &ce);
	 if (ret)
	     goto out;

	 if (!old_entry->flags.extradirp
	     || dir_remove_name (old_entry, h->old_name,
				 &cache_handle,
				 msg1.cache_name,
				 sizeof(msg1.cache_name))) {
	     res = conv_dir (old_entry, ce, 0,
			     &cache_handle,
			     msg1.cache_name,
			     sizeof(msg1.cache_name));
	     if (res.res == -1) {
		 ret = res.error;
		 goto out;
	     }
	 }
	 msg1.cache_handle = cache_handle.xfs_handle;
	 msg1.flag = XFS_ID_INVALID_DNLC;
	 if (cache_handle.valid)
	     msg1.flag |= XFS_ID_HANDLE_VALID;

	 msg1.node.tokens = res.tokens;

	 fcacheentry2xfsnode (old_parent_fid,
			      fcache_realfid(old_entry),
			      &old_entry->status, &msg1.node,
			      old_entry->acccache,
			      FCACHE2XFSNODE_ALL);
	 
	 msg1.header.opcode = XFS_MSG_INSTALLDATA;
	 h0 = (struct xfs_message_header *)&msg1;
	 h0_len = sizeof(msg1);


	 /*
	  * If the new parent is the same as the old parent, reuse
	  */

	 if (VenusFid_cmp(new_parent_fid, old_parent_fid) == 0) {
	     new_entry = old_entry;
	     old_entry = NULL;
	 } else {
	     ret = fcache_get (&new_entry, *new_parent_fid, ce);
	     if (ret)
		 goto out;
	 }
	 
	 ret = fcache_verify_data (new_entry, ce); /* XXX - fake_mp? */
	 if (ret)
	     goto out;
	 
	 res = conv_dir (new_entry, ce, 0,
			 &cache_handle,
			 msg2.cache_name,
			 sizeof(msg2.cache_name));
	 if (res.res == -1) {
	     ret = res.error;
	     goto out;
	 }
	 msg2.cache_handle = cache_handle.xfs_handle;
	 msg2.flag = XFS_ID_INVALID_DNLC;
	 if (cache_handle.valid)
	     msg2.flag |= XFS_ID_HANDLE_VALID;

	 msg2.node.tokens = res.tokens;
	 
	 fcacheentry2xfsnode (new_parent_fid,
			      fcache_realfid(new_entry),
			      &new_entry->status, &msg2.node,
			      new_entry->acccache,
			      FCACHE2XFSNODE_ALL);
	 
	 msg2.header.opcode = XFS_MSG_INSTALLDATA;
	 h1 = (struct xfs_message_header *)&msg2;
	 h1_len = sizeof(msg2);

	 if (old_entry) {
#if 0
	     assert(old_entry->flags.attrusedp);
#endif
	     old_entry->flags.datausedp = TRUE;
	 }
#if 0
	 assert(new_entry->flags.attrusedp);
#endif
	 new_entry->flags.datausedp = TRUE;

	 if (update_child) {
	     ret = message_get_data (&child_entry, &child_fid, &h->cred, &ce);
	     if (ret)
		 goto out;

	     res = conv_dir (child_entry, ce, 0,
			     &cache_handle,
			     msg3.cache_name,
			     sizeof(msg3.cache_name));
	     if (res.res == -1) {
		 ret = res.error;
		 goto out;
	     }
	     msg3.cache_handle = cache_handle.xfs_handle;
	     msg3.flag = XFS_ID_INVALID_DNLC;
	     if (cache_handle.valid)
		 msg3.flag |= XFS_ID_HANDLE_VALID;

	     msg3.node.tokens = res.tokens;

	     fcacheentry2xfsnode (&child_fid,
				  fcache_realfid(child_entry),
				  &child_entry->status, &msg3.node,
				  child_entry->acccache,
				  FCACHE2XFSNODE_ALL);

	     msg3.header.opcode = XFS_MSG_INSTALLDATA;
	     h2 = (struct xfs_message_header *)&msg3;
	     h2_len = sizeof(msg3);
	 }
     }

out:
     if (old_entry) fcache_release(old_entry);
     if (new_entry) fcache_release(new_entry);
     if (child_entry) fcache_release(child_entry);
     
     cred_free (ce);
     xfs_send_message_wakeup_multiple (fd,
				       h->header.sequence_num,
				       ret,
				       h0, h0_len,
				       h1, h1_len,
				       NULL, 0);

     return ret;
}

static int 
xfs_message_putdata (int fd, struct xfs_message_putdata *h, u_int size)
{
     VenusFid *fid;
     Result res;
     CredCacheEntry *ce;
     int ret;
     AFSStoreStatus status;
     struct vcache log_cache;
     FCacheEntry *fce;
     int log_err;

     fid = (VenusFid *)&h->handle;
     arla_warnx (ADEBMSG, "putdata (%ld.%lu.%lu.%lu)",
		 (long)fid->Cell, (unsigned long)fid->fid.Volume,
		 (unsigned long)fid->fid.Vnode,
		 (unsigned long)fid->fid.Unique);

     xfs_attr2afsstorestatus(&h->attr, &status);

     ce = cred_get (fid->Cell, h->cred.pag, CRED_ANY);
     assert (ce != NULL);

     if (connected_mode != CONNECTED) {
	 ret = fcache_find (&fce, *fid);

	 ReleaseWriteLock (&fce->lock);

	 log_cache.fid  = *fid;
	 log_cache.cred = h->cred;
     }

     do {
	 res = cm_close(*fid, h->flag, &status, ce);
	 if (res.res)
	     ret = res.error;
	 else
	     ret = res.res;
     } while (try_again (&ret, &ce, &h->cred, fid));
	 
     if (ret == 0) {
	 if (connected_mode != CONNECTED) {
	     log_cache.DataVersion = ++fce->status.DataVersion;
	     log_err = log_dis_store (&log_cache);
	 }
     } else {
	 arla_warn (ADEBMSG, ret, "xfs_message_putdata: cm_close");
     }

     cred_free (ce);
     xfs_send_message_wakeup (fd, h->header.sequence_num, ret);
     return 0;
}

static int
xfs_message_getdata (int fd, struct xfs_message_getdata *h, u_int size)
{
     struct xfs_message_installdata msg;
     VenusFid *fid;
     VenusFid real_fid;
     Result res;
     AFSFetchStatus status;
     CredCacheEntry *ce;
     int ret;
     AccessEntry *ae;
     struct xfs_message_header *h0 = NULL;
     size_t h0_len = 0;
     fcache_cache_handle cache_handle;

     fid = (VenusFid *)&h->handle;
     arla_warnx (ADEBMSG, "getdata (%ld.%lu.%lu.%lu)",
		 (long)fid->Cell, (unsigned long)fid->fid.Volume,
		 (unsigned long)fid->fid.Vnode,
		 (unsigned long)fid->fid.Unique);

     ce = cred_get (fid->Cell, h->cred.pag, CRED_ANY);
     assert (ce != NULL);

     do {
	 res = cm_getattr (*fid, &status, &real_fid, ce, &ae);
	 if (res.res)
	     ret = res.error;
	 else
	     ret = res.res;
     } while (try_again (&ret, &ce, &h->cred, fid));

     if (ret == 0) {
	  if (status.FileType == TYPE_DIR) {
	       FCacheEntry *entry;

	       ret = message_get_data (&entry, fid, &h->cred, &ce);
	       if (ret)
		   goto out;

	       fcacheentry2xfsnode (fid, fcache_realfid(entry),
				    &entry->status, &msg.node, ae,
				    FCACHE2XFSNODE_ALL);

	       res = conv_dir (entry, ce, h->tokens,
			       &cache_handle,
			       msg.cache_name,
			       sizeof(msg.cache_name));
	       if (res.res)
		   ret = res.error;

	       if (ret == 0) {
		   msg.node.tokens = res.tokens;
		   msg.cache_handle = cache_handle.xfs_handle;
		   msg.flag = XFS_ID_INVALID_DNLC;
		   if (cache_handle.valid)
		       msg.flag |= XFS_ID_HANDLE_VALID;

		   entry->flags.attrusedp = TRUE;
		   entry->flags.datausedp = TRUE;
	       }
	       fcache_release(entry);
	  } else {
	      do {
		  res = cm_open (fid, &ce, h->tokens, &cache_handle,
				 msg.cache_name, sizeof(msg.cache_name));
		  if (res.res)
		      ret = res.error;
		  else
		      ret = res.res;
	      } while (try_again (&ret, &ce, &h->cred, fid));
	      if (ret == 0) {
		  msg.cache_handle = cache_handle.xfs_handle;
		  msg.flag = 0;
		  if (cache_handle.valid)
		      msg.flag |= XFS_ID_HANDLE_VALID;
		  msg.node.tokens = res.tokens;
		  fcacheentry2xfsnode (fid, &real_fid,
				       &status, &msg.node, ae,
				       FCACHE2XFSNODE_ALL);
	      }
	  }
     }

     if (ret == 0) {
	 msg.header.opcode = XFS_MSG_INSTALLDATA;
	 h0 = (struct xfs_message_header *)&msg;
	 h0_len = sizeof(msg);
     }

out:
     cred_free (ce);
     xfs_send_message_wakeup_multiple (fd,
				       h->header.sequence_num,
				       ret,
				       h0, h0_len,
				       NULL, 0);

     return ret;
}

/*
 * Send a invalid node to the kernel to invalidate `entry'
 * and record that it's not being used in the kernel.
 */

void
break_callback (FCacheEntry *entry)
{
     struct xfs_message_invalidnode msg;
     enum { CALLBACK_BREAK_WARN = 100 };
     static int failed_callbacks_break = 0;
     int ret;

     assert (entry->flags.kernelp);

     msg.header.opcode = XFS_MSG_INVALIDNODE;
     memcpy (&msg.handle, &entry->fid, sizeof(entry->fid));
     ret = xfs_message_send (kernel_fd, (struct xfs_message_header *)&msg, 
			     sizeof(msg));
     if (ret) {
	 arla_warnx (ADEBMSG, "break_callback: (%ld.%lu.%lu.%lu) failed",
		     (long)entry->fid.Cell, 
		     (unsigned long)entry->fid.fid.Volume,
		     (unsigned long)entry->fid.fid.Vnode,
		     (unsigned long)entry->fid.fid.Unique);
	 ++failed_callbacks_break;
	 if (failed_callbacks_break > CALLBACK_BREAK_WARN) {
	     arla_warnx (ADEBWARN, "break_callback: have failed %d times",
			 failed_callbacks_break);
	     failed_callbacks_break = 0;
	 }
     }
}

/*
 * Send an unsolicited install-attr for the node in `e'
 */

void
install_attr (FCacheEntry *e, int flags)
{
     struct xfs_message_installattr msg;

     memset (&msg, 0, sizeof(msg));
     msg.header.opcode = XFS_MSG_INSTALLATTR;
     fcacheentry2xfsnode (&e->fid, fcache_realfid(e), &e->status, &msg.node,
			  e->acccache, flags);
     msg.node.tokens   = e->tokens;
     if (!e->flags.datausedp)
	 msg.node.tokens &= ~XFS_DATA_MASK;

     xfs_message_send (kernel_fd, (struct xfs_message_header *)&msg, 
		       sizeof(msg));
}

void
update_fid(VenusFid oldfid, FCacheEntry *old_entry,
	   VenusFid newfid, FCacheEntry *new_entry)
{
    struct xfs_message_updatefid msg;

    msg.header.opcode = XFS_MSG_UPDATEFID;
    memcpy (&msg.old_handle, &oldfid, sizeof(oldfid));
    memcpy (&msg.new_handle, &newfid, sizeof(newfid));
    xfs_message_send (kernel_fd, (struct xfs_message_header *)&msg,
		      sizeof(msg));
    if (new_entry != NULL) {
	new_entry->flags.kernelp   = TRUE;
	new_entry->flags.attrusedp = TRUE;
    }
    if (old_entry != NULL) {
	old_entry->flags.kernelp   = FALSE;
	old_entry->flags.attrusedp = FALSE;
	old_entry->flags.datausedp = FALSE;
    }
}

static int
xfs_message_inactivenode (int fd, struct xfs_message_inactivenode *h, 
			  u_int size)
{
     FCacheEntry *entry;
     VenusFid *fid;
     int ret;
     CredCacheEntry *ce;

     fid = (VenusFid *)&h->handle;
     arla_warnx (ADEBMSG, "inactivenode (%ld.%lu.%lu.%lu)",
		 (long)fid->Cell, (unsigned long)fid->fid.Volume,
		 (unsigned long)fid->fid.Vnode,
		 (unsigned long)fid->fid.Unique);

     ce = cred_get (fid->Cell, 0, CRED_NONE);
     assert (ce != NULL);

     ret = fcache_get (&entry, *fid, ce);
     cred_free (ce);

     if (ret) {
	 arla_warnx (ADEBMSG, "xfs_message_inactivenode: node not found");
	 return 0;
     }
     if (h->flag & XFS_NOREFS)
	 fcache_unused (entry);
     if (h->flag & XFS_DELETE) {
	 entry->flags.kernelp   = FALSE;
	 entry->flags.datausedp = FALSE;
	 entry->flags.attrusedp = FALSE;
     }
     fcache_release(entry);
     return 0;
}

/*
 * Do we have powers for changing stuff?
 */

static Bool
all_powerful_p (const xfs_cred *cred)
{
    return cred->uid == 0;
}

/*
 * Flush the contents of a volume
 */

static int
viocflushvolume (int fd, struct xfs_message_pioctl *h, u_int size)
{
    VenusFid fid ;

    if (!h->handle.a && !h->handle.b && !h->handle.c && !h->handle.d)
	return EINVAL;

    fid.Cell = h->handle.a;
    fid.fid.Volume = h->handle.b;
    fid.fid.Vnode = 0;
    fid.fid.Unique = 0;

    arla_warnx(ADEBMSG,
	       "flushing volume (%d, %u)",
	       fid.Cell, fid.fid.Volume);

    fcache_purge_volume(fid);
    volcache_invalidate (fid.fid.Volume, fid.Cell);
    return 0 ;
}

/*
 * Get an ACL for a directory
 */

static int
viocgetacl(int fd, struct xfs_message_pioctl *h, u_int size)
{
    VenusFid fid;
    AFSOpaque opaque;
    CredCacheEntry *ce;
    int error;

    if (!h->handle.a && !h->handle.b && !h->handle.c && !h->handle.d)
	return xfs_send_message_wakeup (fd, h->header.sequence_num, EINVAL);

    fid.Cell = h->handle.a;
    fid.fid.Volume = h->handle.b;
    fid.fid.Vnode = h->handle.c;
    fid.fid.Unique = h->handle.d;

    ce = cred_get (fid.Cell, h->cred.pag, CRED_ANY);
    assert (ce != NULL);

    do {
	error = getacl (fid, ce, &opaque);
    } while (try_again (&error, &ce, &h->cred, &fid));

    if (error != 0 && error != EACCES)
	error = EINVAL;

    cred_free (ce);
 
    xfs_send_message_wakeup_data (fd, h->header.sequence_num, error,
				  opaque.val, opaque.len);
    if (error == 0)
	free (opaque.val);
    return 0;
}

/*
 * Set an ACL for a directory
 */

static int
viocsetacl(int fd, struct xfs_message_pioctl *h, u_int size)
{
    VenusFid fid;
    AFSOpaque opaque;
    CredCacheEntry *ce;
    FCacheEntry *e;
    int error;

    if (!h->handle.a && !h->handle.b && !h->handle.c && !h->handle.d)
	return xfs_send_message_wakeup (fd, h->header.sequence_num, EINVAL);

    if (h->insize > AFSOPAQUEMAX || h->insize == 0)
	return xfs_send_message_wakeup (fd, h->header.sequence_num, EINVAL);

    opaque.val = malloc(h->insize);
    if(opaque.val == NULL)
	return xfs_send_message_wakeup (fd, h->header.sequence_num, ENOMEM);

    fid.Cell       = h->handle.a;
    fid.fid.Volume = h->handle.b;
    fid.fid.Vnode  = h->handle.c;
    fid.fid.Unique = h->handle.d;

    ce = cred_get (fid.Cell, h->cred.pag, CRED_ANY);
    assert (ce != NULL);

    opaque.len = h->insize;
    memcpy(opaque.val, h->msg, h->insize);

    do {
	error = setacl (fid, ce, &opaque, &e);
    } while (try_again (&error, &ce, &h->cred, &fid));

    if (error == 0) {
	install_attr (e, FCACHE2XFSNODE_ALL);
	fcache_release (e);
    } else if (error != EACCES)
	error = EINVAL;

    cred_free (ce);
    free (opaque.val);
 
    xfs_send_message_wakeup_data (fd, h->header.sequence_num, error, NULL, 0);
    return 0;
}

/*
 * Get volume status
 */

static int
viocgetvolstat(int fd, struct xfs_message_pioctl *h, u_int size)
{
    VenusFid fid;
    CredCacheEntry *ce;
    AFSFetchVolumeStatus volstat;
    char volumename[AFSNAMEMAX];
    char offlinemsg[AFSOPAQUEMAX];
    char motd[AFSOPAQUEMAX];
    char out[SYSNAMEMAXLEN];
    int32_t outsize = 0;
    int error;

    if (!h->handle.a && !h->handle.b && !h->handle.c && !h->handle.d)
	return xfs_send_message_wakeup (fd, h->header.sequence_num, EINVAL);

    fid.Cell = h->handle.a;
    fid.fid.Volume = h->handle.b;
    fid.fid.Vnode = 0;
    fid.fid.Unique = 0;

    ce = cred_get (fid.Cell, h->cred.pag, CRED_ANY);
    assert (ce != NULL);

    memset (volumename, 0, AFSNAMEMAX);
    memset (offlinemsg, 0, AFSOPAQUEMAX);
    memset (motd, 0, AFSOPAQUEMAX);
    memset (out, 0, SYSNAMEMAXLEN);

    do {
	error = getvolstat (fid, ce, &volstat,
			    volumename,
			    offlinemsg,
			    motd);
    } while (try_again (&error, &ce, &h->cred, &fid));

    cred_free (ce);

    if (error != 0 && error != EACCES)
	error = EINVAL;

    memcpy (out, (char *) &volstat, sizeof (AFSFetchVolumeStatus));
    outsize = sizeof (AFSFetchVolumeStatus);

    if (volumename[0]) {
	strncpy (out+outsize, volumename, AFSNAMEMAX);
	outsize += strlen (volumename);
    }
    else {
	out[outsize] = 0;
	outsize++;
    }

    if (offlinemsg[0]) {
	strncpy (out+outsize, offlinemsg, AFSOPAQUEMAX);
	outsize += strlen (offlinemsg);
    }
    else {
	out[outsize] = 0;
	outsize++;
    }

    if (motd[0]) {
	strncpy (out+outsize, motd, AFSOPAQUEMAX);
	outsize += strlen (motd);
    }
    else {
	out[outsize] = 0;
	outsize++;
    }

    xfs_send_message_wakeup_data (fd, h->header.sequence_num, error,
				  out, outsize);
    return 0;
}

/*
 * Set volume status
 */

static int
viocsetvolstat(int fd, struct xfs_message_pioctl *h, u_int size)
{
    VenusFid fid;
    CredCacheEntry *ce;
    AFSFetchVolumeStatus *involstat;
    AFSStoreVolumeStatus outvolstat;
    char volumename[AFSNAMEMAX];
    char offlinemsg[AFSOPAQUEMAX];
    char motd[AFSOPAQUEMAX];
    int error;
    char *ptr;

    if (!h->handle.a && !h->handle.b && !h->handle.c && !h->handle.d)
	return EINVAL;

    fid.Cell = h->handle.a;
    fid.fid.Volume = h->handle.b;
    fid.fid.Vnode = 0;
    fid.fid.Unique = 0;

    ce = cred_get (fid.Cell, h->cred.pag, CRED_ANY);
    assert (ce != NULL);

    involstat = (AFSFetchVolumeStatus *) h->msg;
    outvolstat.Mask = 0x3; /* Store both the next fields */
    outvolstat.MinQuota = involstat->MinQuota;
    outvolstat.MaxQuota = involstat->MaxQuota;

    ptr = h->msg + sizeof (AFSFetchVolumeStatus);

#if 0
    if (*ptr) {
	strncpy (volumename, ptr, AFSNAMEMAX);
	ptr += strlen (ptr);
    }
    else {
	memset (volumename, 0, AFSNAMEMAX);
	ptr++; /* skip 0 character */
    }

    if (*ptr) {
	strncpy (offlinemsg, ptr, AFSOPAQUEMAX);
	ptr += strlen (ptr);
    }
    else {
	memset (offlinemsg, 0, AFSOPAQUEMAX);
	ptr++;
    }

    strncpy (motd, ptr, AFSOPAQUEMAX);
#else
    volumename[0] = '\0';
    offlinemsg[0] = '\0';
    motd[0] = '\0';
#endif

    do {
	error = setvolstat (fid, ce, &outvolstat, volumename,
			    offlinemsg, motd);
    } while (try_again (&error, &ce, &h->cred, &fid));

    if (error != 0 && error != EACCES)
	error = EINVAL;

    cred_free (ce);

    xfs_send_message_wakeup_data (fd, h->header.sequence_num, error,
				  NULL, 0);
    return 0;
}

/*
 * Get the mount point at (`fid', `filename') using the cred in `ce'
 * and returning the fcache entry in `ret_mp_entry'
 * Return 0 or an error.
 */

static int
get_mount_point (VenusFid fid,
		 const char *filename,
		 CredCacheEntry **ce,
		 FCacheEntry **ret_mp_entry)
{
    FCacheEntry *mp_entry;
    FCacheEntry *dentry;
    VenusFid mp_fid;
    int error;

    if (fid.fid.Volume == 0 && fid.fid.Vnode == 0 && fid.fid.Unique == 0)
	return EINVAL;

    error = fcache_get_data(&dentry, &fid, ce);
    if (error)
	return error;

    error = adir_lookup(dentry, filename, &mp_fid);
    fcache_release(dentry);
    if (error)
	return error;

    error = fcache_get(&mp_entry, mp_fid, *ce);
    if (error)
	return error;

    error = fcache_verify_attr (mp_entry, NULL, NULL, *ce);
    if (error) {
	fcache_release(mp_entry);
	return error;
    }

    if ((mp_entry->status.FileType != TYPE_LINK
	 && !mp_entry->flags.fake_mp)
	|| mp_entry->status.Length == 0) { 	/* Is not a mount point */
	fcache_release(mp_entry);
	return EINVAL;
    }
    *ret_mp_entry = mp_entry;
    return 0;
}

/*
 * Read the contents of the mount point in `e' and return a fbuf in
 * `the_fbuf' mapped READ|WRITE|PRIVATE.
 * Return 0 or an error
 */

static int
read_mount_point (FCacheEntry *mp_entry, CredCacheEntry *ce,
		  int *fd, fbuf *the_fbuf)
{
    int error;
    char *buf;

    error = fcache_verify_data (mp_entry, ce);
    if (error)
	return error;

    *fd = fcache_open_file (mp_entry, O_RDONLY);
    if (*fd < 0)
	return errno;

    error = fbuf_create (the_fbuf, *fd, mp_entry->status.Length,
			 FBUF_READ|FBUF_WRITE|FBUF_PRIVATE);
    if (error) {
	close (*fd);
	return error;
    }

    buf = (char *)(the_fbuf->buf);
    if (buf[0] != '#' && buf[0] != '%') { /* Is not a mount point */
	fbuf_end (the_fbuf);
	close (*fd);
	return EINVAL;
    }

    return 0;
}

/*
 * Get info for a mount point.
 */

static int
vioc_afs_stat_mt_pt(int fd, struct xfs_message_pioctl *h, u_int size)
{
    VenusFid fid;
    int error;
    int mp_fd;
    fbuf the_fbuf;
    CredCacheEntry *ce;
    FCacheEntry *e;
    unsigned char *buf;

    fid.Cell       = h->handle.a;
    fid.fid.Volume = h->handle.b;
    fid.fid.Vnode  = h->handle.c;
    fid.fid.Unique = h->handle.d;

    h->msg[min(h->insize, sizeof(h->msg)-1)] = '\0';

    ce = cred_get (fid.Cell, h->cred.pag, CRED_ANY);
    assert (ce != NULL);

    error = get_mount_point (fid, h->msg, &ce, &e);
    if (error) {
	cred_free(ce);
	return xfs_send_message_wakeup (fd, h->header.sequence_num, error);
    }

    error = read_mount_point (e, ce, &mp_fd, &the_fbuf);
    if (error) {
	fcache_release (e);
	cred_free(ce);
	return xfs_send_message_wakeup (fd, h->header.sequence_num, error);
    }

    /*
     * To confuse us, the volume is passed up w/o the ending
     * dot. It's not even mentioned in the ``VIOC_AFS_STAT_MT_PT''
     * documentation.
     */

    buf = (unsigned char *)the_fbuf.buf;
    buf[the_fbuf.len-1] = '\0';

    xfs_send_message_wakeup_data (fd, h->header.sequence_num, error,
				  buf, the_fbuf.len);
    fbuf_end (&the_fbuf);
    close (mp_fd);
    fcache_release (e);
    cred_free (ce);

    return 0;
}

/*
 * Handle the VIOC_AFS_DELETE_MT_PT message in `h' by deleting the
 * mountpoint.  
 */

static int
vioc_afs_delete_mt_pt(int fd, struct xfs_message_pioctl *h, u_int size)
{
    VenusFid fid;
    int error = 0;
    CredCacheEntry *ce;
    struct xfs_message_remove remove_msg;
    FCacheEntry *entry;

    h->msg[min(h->insize, sizeof(h->msg)-1)] = '\0';

    fid.Cell       = h->handle.a;
    fid.fid.Volume = h->handle.b;
    fid.fid.Vnode  = h->handle.c;
    fid.fid.Unique = h->handle.d;

    ce = cred_get (fid.Cell, h->cred.pag, CRED_ANY);
    assert (ce != NULL);

    error = get_mount_point (fid, h->msg, &ce, &entry);
    cred_free (ce);
    if (error)
	return xfs_send_message_wakeup (fd, h->header.sequence_num, error);
    fcache_release(entry);

    remove_msg.header        = h->header;
    remove_msg.header.size   = sizeof(remove_msg);
    remove_msg.parent_handle = h->handle;
    strlcpy(remove_msg.name, h->msg, sizeof(remove_msg.name));
    remove_msg.cred          = h->cred;

    return xfs_message_remove (fd, &remove_msg, sizeof(remove_msg));
}

static int
viocwhereis(int fd, struct xfs_message_pioctl *h, u_int size)
{
    VenusFid fid;
    CredCacheEntry *ce;
    FCacheEntry *e;
    int error;
    int i, j;
    int32_t addresses[8];
    int bit;

    if (!h->handle.a && !h->handle.b && !h->handle.c && !h->handle.d)
	return xfs_send_message_wakeup (fd, h->header.sequence_num, EINVAL);

    fid.Cell       = h->handle.a;
    fid.fid.Volume = h->handle.b;
    fid.fid.Vnode  = h->handle.c;
    fid.fid.Unique = h->handle.d;

    ce = cred_get (fid.Cell, h->cred.pag, CRED_ANY);
    assert (ce != NULL);

    error = fcache_get(&e, fid, ce);
    if (error) {
	cred_free(ce);
	return xfs_send_message_wakeup (fd, h->header.sequence_num, error);
    }
    error = fcache_verify_attr (e, NULL, NULL, ce);
    if (error) {
	fcache_release(e);
	cred_free(ce);
	return xfs_send_message_wakeup (fd, h->header.sequence_num, error);
    }

    bit = volcache_volid2bit (e->volume, fid.fid.Volume);

    if (bit == -1) {
	fcache_release(e);
	cred_free(ce);
	return xfs_send_message_wakeup (fd, h->header.sequence_num, EINVAL);
    }

    memset(addresses, 0, sizeof(addresses));
    for (i = 0, j = 0; i < min(e->volume->entry.nServers, MAXNSERVERS); i++) {
	u_long addr = htonl(e->volume->entry.serverNumber[i]);

	if ((e->volume->entry.serverFlags[i] & bit) && addr != 0)
	    addresses[j++] = addr;
    }
    xfs_send_message_wakeup_data (fd, h->header.sequence_num, error,
				  addresses, sizeof(long) * j);

    fcache_release(e);
    cred_free (ce);

    return 0;
}

/*
 * Return all db servers for a particular cell.
 */ 

static int
vioc_get_cell(int fd, struct xfs_message_pioctl *h, u_int size)
{
    int i;
    int32_t index;
    const char *cellname;
    int cellname_len;
    int outsize;
    char out[8 * sizeof(int32_t) + MAXPATHLEN]; /* XXX */
    const cell_db_entry *dbservers;
    int num_dbservers;

    index = *((int32_t *) h->msg);
    cellname = cell_num2name(index);
    if (cellname == NULL)
	return xfs_send_message_wakeup (fd, h->header.sequence_num, EDOM);
    
    dbservers = cell_dbservers_by_id (index, &num_dbservers);

    if (dbservers == NULL)
	return xfs_send_message_wakeup (fd, h->header.sequence_num, EDOM);

    memset(out, 0, sizeof(out));
    cellname_len = min(strlen(cellname), MAXPATHLEN - 1);
    memcpy(out + 8 * sizeof(int32_t), cellname, cellname_len);
    out[8 * sizeof(int32_t) + cellname_len] = '\0';
    outsize = 8 * sizeof(int32_t) + cellname_len + 1;
    for (i = 0; i < min(num_dbservers, 8); ++i) {
	u_int32_t addr = dbservers[i].addr.s_addr;
	memcpy (&out[i * sizeof(int32_t)], &addr, sizeof(int32_t));
    }

    xfs_send_message_wakeup_data (fd, h->header.sequence_num, 0,
				  out, outsize);

    return 0;
}

/*
 * Return status information about a cell.
 */

static int
vioc_get_cellstatus(int fd, struct xfs_message_pioctl *h, u_int size)
{
    char *cellname;
    int32_t cellid;
    u_int32_t out = 0;

    cellname = h->msg;
    cellname[h->insize-1]  = '\0';

    cellid = cell_name2num (cellname);
    if (cellid == -1)
	return xfs_send_message_wakeup (fd, h->header.sequence_num, ENOENT);

    if (cellid == 0)
	out |= CELLSTATUS_PRIMARY;
    if (cell_issuid_by_num (cellid))
	out |= CELLSTATUS_SETUID;

    xfs_send_message_wakeup_data (fd, h->header.sequence_num, 0,
				  &out, sizeof(out));

    return 0;
}

/*
 * Set status information about a cell.
 */

static int
vioc_set_cellstatus(int fd, struct xfs_message_pioctl *h, u_int size)
{
    int32_t cellid;
    char *cellname;
    u_int32_t in = 0;
    int ret;

    if (!all_powerful_p (&h->cred))
	return xfs_send_message_wakeup (fd, h->header.sequence_num, EACCES);

    if (h->insize < sizeof (in) + 2) /* terminating NUL and one char */
	return xfs_send_message_wakeup (fd, h->header.sequence_num, EINVAL);

    cellname = h->msg + sizeof (in);
    cellname[h->insize-1-sizeof(in)]  = '\0';

    cellid = cell_name2num (cellname);
    if (cellid == -1)
	return xfs_send_message_wakeup (fd, h->header.sequence_num, ENOENT);

    if (in & CELLSTATUS_SETUID) { 
	ret = cell_setsuid_by_num (cellid);
	if (ret)
	    return xfs_send_message_wakeup (fd, h->header.sequence_num,EINVAL);
    }

    xfs_send_message_wakeup (fd, h->header.sequence_num, 0);

    return 0;
}

/*
 * Set information about a cell or add a new one.
 */

static int
vioc_new_cell(int fd, struct xfs_message_pioctl *h, u_int size)
{
    const char *cellname;
    cell_entry *ce;
    int count, i;
    u_int32_t *hp;
    cell_db_entry *dbs;

    if (!all_powerful_p (&h->cred))
	return xfs_send_message_wakeup (fd, h->header.sequence_num, EPERM);
	    
    if (h->insize < 9)
	return xfs_send_message_wakeup (fd, h->header.sequence_num, EINVAL);

    hp = (u_int32_t *)h->msg;
    for (count = 0; *hp != 0; ++hp)
	++count;

    dbs = malloc (count * sizeof(*dbs));
    if (dbs == NULL)
	return xfs_send_message_wakeup (fd, h->header.sequence_num, ENOMEM);
	
    memset(dbs, 0, count * sizeof(*dbs));

    hp = (u_int32_t *)h->msg;
    for (i = 0; i < count; ++i) {
	dbs[i].name = NULL;
	dbs[i].addr.s_addr = hp[i];
	dbs[i].timeout = 0;
    }

    cellname = h->msg + 8 * sizeof(u_int32_t);
    ce = cell_get_by_name (cellname);
    if (ce == NULL) {
	ce = cell_new_dynamic (cellname);

	if (ce == NULL) {
	    free (dbs);
	    return xfs_send_message_wakeup (fd, h->header.sequence_num,
					    ENOMEM);
	}
    } else {
	free (ce->dbservers);
    }

    ce->ndbservers = count;
    ce->dbservers  = dbs;

    return xfs_send_message_wakeup (fd, h->header.sequence_num, 0);
}

#ifdef KERBEROS

/*
 * Return the token for the cell in `ce'
 */

static int
token_for_cell (int fd, struct xfs_message_pioctl *h, u_int size,
		CredCacheEntry *ce)
{
    struct ClearToken ct;
    char buf[2048];
    size_t len;
    char *p = buf;
    u_int32_t tmp;
    krbstruct *kstruct = (krbstruct *)ce->cred_data;
    CREDENTIALS *cred  = &kstruct->c;
    const char *cell = cell_num2name (ce->cell);

    ct.AuthHandle = cred->kvno;
    memcpy (ct.HandShakeKey, cred->session, sizeof(cred->session));
    ct.ViceId         = ce->uid;
    ct.BeginTimestamp = cred->issue_date + 1;
    ct.EndTimestamp   = ce->expire;

    tmp = cred->ticket_st.length;
    memcpy (p, &tmp, sizeof(tmp));
    p += sizeof(tmp);
    memcpy (p, cred->ticket_st.dat, tmp);
    p += tmp;
    tmp = sizeof(ct);
    memcpy (p, &tmp, sizeof(tmp));
    p += sizeof(tmp);
    memcpy (p, &ct, sizeof(ct));
    p += sizeof(ct);
    tmp = strlen(cell);
    memcpy (p, &tmp, sizeof(tmp));
    p += sizeof(tmp);
    strlcpy (p, cell, buf + sizeof buf - cell);
    p += strlen(cell) + 1;

    len = p - buf;

    memset (&ct, 0, sizeof(ct));

    cred_free (ce);

    xfs_send_message_wakeup_data (fd, h->header.sequence_num, 0,
				  buf, len);
    return 0;
}

/*
 * Handle the GETTOK message in `h'
 */

static int
viocgettok (int fd, struct xfs_message_pioctl *h, u_int size)
{
    if (h->insize == 0) {
	int32_t cell_id = cell_name2num(cell_getthiscell());
	CredCacheEntry *ce = cred_get (cell_id, h->cred.pag, CRED_KRB4);

	if (ce == NULL) {
	    xfs_send_message_wakeup (fd, h->header.sequence_num, ENOTCONN);
	    return 0;
	}
	return token_for_cell (fd, h, size, ce);
    } else if (h->insize == sizeof(u_int32_t)) {
	u_int32_t n;
	int i, c;
	int found;
	CredCacheEntry *ce = NULL;

	memcpy (&n, h->msg, sizeof(n));

	i = 0;
	c = 0;
	found = 0;
	while (!found && i <= n) {
	    if (cell_num2name(c) == NULL)
		break;

	    ce = cred_get (c++, h->cred.pag, CRED_KRB4);
	    if (ce != NULL) {
		if (i == n) {
		    found = 1;
		} else {
		    cred_free (ce);
		    ++i;
		}
	    }
	}
	if (!found) {
	    xfs_send_message_wakeup (fd, h->header.sequence_num, EDOM);
	    return 0;
	}
	return token_for_cell (fd, h, size, ce);
    } else {
	xfs_send_message_wakeup (fd, h->header.sequence_num, EINVAL);
    }
    return 0;
}

/*
 * Handle the SETTOK message in `h'
 */

static int
viocsettok (int fd, struct xfs_message_pioctl *h, u_int size)
{
    struct ClearToken ct;
    CREDENTIALS c;
    long cell;
    int32_t sizeof_x;
    char *t = h->msg;

    /* someone probed us */
    if (h->insize == 0) {
	return EINVAL;
    }

    /* Get ticket_st */
    memcpy(&sizeof_x, t, sizeof(sizeof_x)) ;
    c.ticket_st.length = sizeof_x ;
    arla_warnx (ADEBMSG, "ticket_st has size %d", sizeof_x);
    t += sizeof(sizeof_x) ;

    memcpy(c.ticket_st.dat, t, sizeof_x) ;
    t += sizeof_x ;

    /* Get ClearToken */
    memcpy(&sizeof_x, t, sizeof(sizeof_x)) ;
    t += sizeof(sizeof_x) ;

    memcpy(&ct, t, sizeof_x) ;
    t += sizeof_x ;

    /* Get primary cell ? */
    memcpy(&sizeof_x, t, sizeof(sizeof_x)) ;
    t += sizeof(sizeof_x) ;

    /* Get Cellname */ 
    strncpy(c.realm, t, REALM_SZ) ;
    c.realm[REALM_SZ-1] = '\0' ;

    /* Make this a sane world again */
    c.kvno = ct.AuthHandle;
    memcpy (c.session, ct.HandShakeKey, sizeof(c.session));
    c.issue_date = ct.BeginTimestamp - 1;
	
    cell = cell_name2num(strlwr(c.realm));

    if (cell == -1)
	return ENOENT;

    conn_clearcred (CONN_CS_ALL, cell, h->cred.pag, 2);
    fcache_purge_cred(h->cred.pag, cell);
    cred_add (h->cred.pag, CRED_KRB4, 2, cell, ct.EndTimestamp,
	      &c, sizeof(c), ct.ViceId);
    return 0;
}

static int
viocunlog (int fd, struct xfs_message_pioctl *h, u_int size)
{
    xfs_pag_t cred = h->cred.pag;

    cred_remove(cred);
    fcache_purge_cred(cred, -1);
    return 0;
}

#endif /* KERBEROS */

/*
 * Flush the fid in `h->handle' from the cache.
 */

static int
viocflush (int fd, struct xfs_message_pioctl *h, u_int size)
{
    VenusFid fid ;
    AFSCallBack broken_callback = {0, 0, CBDROPPED};

    if (!h->handle.a && !h->handle.b && !h->handle.c && !h->handle.d)
	return EINVAL;

    fid.Cell       = h->handle.a;
    fid.fid.Volume = h->handle.b;
    fid.fid.Vnode  = h->handle.c;
    fid.fid.Unique = h->handle.d;

    arla_warnx(ADEBMSG,
	       "flushing (%d, %u, %u, %u)",
	       fid.Cell, fid.fid.Volume, fid.fid.Vnode, fid.fid.Unique);

    fcache_stale_entry(fid, broken_callback);
    return 0 ;
}

static int
viocconnect(int fd, struct xfs_message_pioctl *h, u_int size)
{
    char *p = h->msg;
    int32_t tmp;
    int32_t ret;
    int error = 0;

    if (h->insize != sizeof(int32_t) ||
	h->outsize != sizeof(int32_t)) {

	ret = -EINVAL;
    } else {
    
	memcpy(&tmp, h->msg, sizeof(tmp));
	p += sizeof(tmp);

	ret = tmp;

	switch(tmp) {
	case CONNMODE_PROBE:
	    switch(connected_mode) {
	    case CONNECTED: ret = CONNMODE_CONN; break;
	    case FETCH_ONLY: ret = CONNMODE_FETCH; break;
	    case DISCONNECTED: ret = CONNMODE_DISCONN; break;
	    default:
		error = EINVAL;
		ret = 0;
		break;
	    }
	    break;
	case CONNMODE_CONN:
	    if (Log_is_open) {
		DARLA_Close(&log_data);
		Log_is_open = 0;

#if 0
		do_replay("discon_log", log_data.log_entries, 0);
#endif
	    }
	    if (connected_mode == DISCONNECTED) {
		connected_mode = CONNECTED ;
		fcache_reobtain_callbacks ();
	    }
	    connected_mode = CONNECTED ;
	    break;
	case CONNMODE_FETCH:
	    connected_mode = FETCH_ONLY ;
	    break;
	case CONNMODE_DISCONN:
	    ret = DARLA_Open(&log_data, ARLACACHEDIR"/discon_log",
			     O_WRONLY | O_CREAT | O_BINARY);
	    if (ret < 0) {
		arla_warn (ADEBERROR, errno, "DARLA_Open");
	    } else {
		Log_is_open = 1;
	    }
	    connected_mode = DISCONNECTED;
	    break;
	default:
	    error = EINVAL;
	    break;
	}
    }

    xfs_send_message_wakeup_data (fd, h->header.sequence_num, error,
				  &ret, sizeof(ret));
    return 0;
}

static int
getrxkcrypt(int fd, struct xfs_message_pioctl *h, u_int size)
{
    if (h->outsize == sizeof(u_int32_t)) {
	u_int32_t n;

#ifdef KERBEROS
	if (conn_rxkad_level == rxkad_crypt)
	    n = 1;
	else
#endif
	    n = 0;

	return xfs_send_message_wakeup_data (fd,
					     h->header.sequence_num,
					     0,
					     &n,
					     sizeof(n));
    } else
	return xfs_send_message_wakeup (fd, h->header.sequence_num, EINVAL);
}

static int
setrxkcrypt(int fd, struct xfs_message_pioctl *h, u_int size)
{
#ifdef KERBEROS
    int error = 0;

    if (h->insize == sizeof(u_int32_t)) {
	u_int32_t n;

	memcpy (&n, h->msg, sizeof(n));

	if (n == 0)
	    conn_rxkad_level = rxkad_auth;
	else if(n == 1)
	    conn_rxkad_level = rxkad_crypt;
	else
	    error = EINVAL;
	if (error == 0)
	    conn_clearcred (CONN_CS_NONE, 0, -1, -1);
    } else
	error = EINVAL;
    return error;
#else
    return EOPNOTSUPP;
#endif
}

/*
 * XXX - this function sometimes does a wakeup_data and then an ordinary wakeup is sent in xfs_message_pioctl
 */

static int
vioc_fpriostatus (int fd, struct xfs_message_pioctl *h, u_int size)
{
    struct vioc_fprio *fprio;
    int error = 0;
    VenusFid fid;

    if (h->insize != sizeof(struct vioc_fprio))
	return EINVAL;

    fprio = (struct vioc_fprio *) h->msg;

    fid.Cell = fprio->Cell ;
    fid.fid.Volume = fprio->Volume ;
    fid.fid.Vnode = fprio->Vnode ;
    fid.fid.Unique = fprio->Unique ;


#if 0
    switch(fprio->cmd) {
    case FPRIO_GET: {
	unsigned prio;

	if (h->outsize != sizeof(unsigned)) {
	    error = EINVAL;
	    break;
	}
	
	prio = fprio_get(fid);
	xfs_send_message_wakeup_data (fd,
				      h->header.sequence_num,
				      0,
				      &prio,
				      sizeof(prio));

	break;
    }
    case FPRIO_SET:
	if (fprio->prio == 0) {
	    fprio_remove(fid);
	    error = 0;
	} else if (fprio->prio < FPRIO_MIN ||
	    fprio->prio > FPRIO_MAX)
	    error = EINVAL;
	else {
	    fprio_set(fid, fprio->prio);
	    error = 0;
	}
	break;
    case FPRIO_GETMAX: 
	if (h->outsize != sizeof(unsigned)) {
	    error = EINVAL;
	    break;
	}

	xfs_send_message_wakeup_data (fd,
				      h->header.sequence_num,
				      0,
				      &fprioritylevel,
				      sizeof(fprioritylevel));
	error = 0;
	break;
    case FPRIO_SETMAX: 
	if (fprio->prio < FPRIO_MIN ||
	    fprio->prio > FPRIO_MAX)
	    error = EINVAL;
	else {
	    fprioritylevel = fprio->prio;
	    error = 0;
	}
	break;
    default:
	error = EINVAL;
	break;
    }
#endif
    return error;
}

static int
viocgetfid (int fd, struct xfs_message_pioctl *h, u_int size)
{
    return xfs_send_message_wakeup_data(fd, h->header.sequence_num, 0,
					&h->handle, sizeof(VenusFid));
}

static int
viocvenuslog (int fd, struct xfs_message_pioctl *h, u_int size)
{
    if (!all_powerful_p(&h->cred))
	return EPERM;
	    
    conn_status ();
    volcache_status ();
    cred_status ();
    fcache_status ();
#if 0
    fprio_status ();
#endif
#ifdef RXDEBUG
    rx_PrintStats(stderr);
#endif
    return 0;
}

/*
 * Set or get the sysname
 */

static int
vioc_afs_sysname (int fd, struct xfs_message_pioctl *h, u_int size)
{
    char *t = h->msg;
    int32_t parm = *((int32_t *)t);

    if (parm) {
	if (!all_powerful_p (&h->cred))
	    return xfs_send_message_wakeup (fd,
					    h->header.sequence_num,
					    EPERM);
	t += sizeof(int32_t);
	arla_warnx (ADEBMSG, "VIOC_AFS_SYSNAME: setting sysname: %s", t);
	memcpy(arlasysname, t, h->insize);
	arlasysname[h->insize] = '\0';
	return xfs_send_message_wakeup(fd, h->header.sequence_num, 0);
    } else {
	char *buf;
	size_t sysname_len = strlen (arlasysname);
	int ret;

	buf = malloc (sysname_len + 4 + 1);
	if (buf == NULL)
	    return xfs_send_message_wakeup (fd, h->header.sequence_num, ENOMEM);
	*((u_int32_t *)buf) = sysname_len;
	memcpy (buf + 4, arlasysname, sysname_len);
	buf[sysname_len + 4] = '\0';

	ret = xfs_send_message_wakeup_data (fd, h->header.sequence_num, 0,
					    buf, sysname_len + 5);
	free (buf);
	return ret;
    }
}

static int
viocfilecellname (int fd, struct xfs_message_pioctl *h, u_int size)
{
    char *cellname;

    cellname = (char *) cell_num2name(h->handle.a);

    if (cellname) 
	return xfs_send_message_wakeup_data(fd, h->header.sequence_num, 0,
					    cellname, strlen(cellname)+1);
    else 
	return xfs_send_message_wakeup_data(fd, h->header.sequence_num, EINVAL,
					    NULL, 0);
}

static int
viocgetwscell (int fd, struct xfs_message_pioctl *h, u_int size)
{
    char *cellname;

    cellname = (char*) cell_getthiscell();
    return xfs_send_message_wakeup_data(fd, h->header.sequence_num, 0,
					cellname, strlen(cellname)+1);
}

static int
viocsetcachesize (int fd, struct xfs_message_pioctl *h, u_int size)
{
    u_int32_t *s = (u_int32_t *)h->msg;

    if (!all_powerful_p (&h->cred))
	return EPERM;
	
    if (h->insize >= sizeof(int32_t) * 4) 
	return fcache_reinit(s[0], s[1], s[2], s[3]);
    else
	return fcache_reinit(*s/2, *s, *s*500, *s*1000);
}

/*
 * VIOCCKSERV
 *
 *  in:  flags	- bitmask (1 - dont ping, use cached data, 2 - check fsservers only)
 *       cell	- string (optional)
 *  out: hosts  - u_int32_t number of hosts, followed by list of hosts being down.
 */

static int
viocckserv (int fd, struct xfs_message_pioctl *h, u_int size)
{
    int32_t cell = cell_name2num (cell_getthiscell());
    int flags = 0;
    int num_entries;
    u_int32_t hosts[CKSERV_MAXSERVERS + 1];
    int msg_size;

    if (h->insize < sizeof(int32_t))
	return xfs_send_message_wakeup (fd, h->header.sequence_num, EINVAL);

    memset (hosts, 0, sizeof(hosts));

    flags = *(u_int32_t *)h->msg;
    flags &= CKSERV_DONTPING|CKSERV_FSONLY;

    if (h->insize > sizeof(int32_t)) {
	h->msg[min(h->insize, sizeof(h->msg)-1)] = '\0';

	cell = cell_name2num (((char *)h->msg) + sizeof(int32_t));
	if (cell == -1)
	    return xfs_send_message_wakeup (fd, h->header.sequence_num, ENOENT);
    }
    
    num_entries = CKSERV_MAXSERVERS;
    
    conn_downhosts(cell, hosts + 1, &num_entries, flags);
    
    hosts[0] = num_entries;
    msg_size = sizeof(hosts[0]) * (num_entries + 1);
    return xfs_send_message_wakeup_data (fd, h->header.sequence_num, 0,
					 hosts, msg_size);
}


/*
 * Return the number of used KBs and reserved KBs
 */

static int
viocgetcacheparms (int fd, struct xfs_message_pioctl *h, u_int size)
{
    u_int32_t parms[16];
    
    memset(parms, 0, sizeof(parms));
    parms[0] = fcache_highbytes() / 1024;
    parms[1] = fcache_usedbytes() / 1024;
    parms[2] = fcache_highvnodes();
    parms[3] = fcache_usedvnodes();
    parms[4] = fcache_highbytes();
    parms[5] = fcache_usedbytes();
    parms[6] = fcache_lowbytes();
    parms[7] = fcache_lowvnodes();

    h->outsize = sizeof(parms);
    return xfs_send_message_wakeup_data(fd, h->header.sequence_num, 0,
					parms, sizeof(parms));
}

/*
 * debugging interface to give out statistics of the cache
 */

static int
viocaviator (int fd, struct xfs_message_pioctl *h, u_int size)
{
    u_int32_t parms[16];
    
    memset(parms, 0, sizeof(parms));
    parms[0] = kernel_highworkers();
    parms[1] = kernel_usedworkers();

    h->outsize = sizeof(parms);
    return xfs_send_message_wakeup_data(fd, h->header.sequence_num, 0,
					parms, sizeof(parms));
}

/*
 * Get/set arla debug level
 */

static int
vioc_arladebug (int fd, struct xfs_message_pioctl *h, u_int size)
{
    if (h->insize != 0) {
	if (h->insize < sizeof(int32_t))
	    return xfs_send_message_wakeup (fd, h->header.sequence_num,
					    EINVAL);
	if (!all_powerful_p (&h->cred))
	    return xfs_send_message_wakeup (fd, h->header.sequence_num,
					    EPERM);
	arla_log_set_level_num (*((int32_t *)h->msg));
    }
    if (h->outsize != 0) {
	int32_t debug_level;

	if (h->outsize < sizeof(int32_t))
	    return xfs_send_message_wakeup (fd, h->header.sequence_num,
					    EINVAL);

	debug_level = arla_log_get_level_num ();
	return xfs_send_message_wakeup_data (fd, h->header.sequence_num,
					     0, &debug_level,
					     sizeof(debug_level));
    }
    return xfs_send_message_wakeup (fd, h->header.sequence_num, 0);
}

/*
 * GC pags --- there shouldn't be any need to do anything here.
 */

static int
vioc_gcpags (int fd, struct xfs_message_pioctl *h, u_int size)
{
    return 0;
}

/*
 *
 */

static int
vioc_calculate_cache (int fd, struct xfs_message_pioctl *h, u_int size)
{
    u_int32_t parms[16];
    
    memset(parms, 0, sizeof(parms));
    
    if (!all_powerful_p(&h->cred))
	return EPERM;

    h->outsize = sizeof(parms);

    parms[0] = fcache_calculate_usage();
    parms[1] = fcache_usedbytes();

    arla_warnx (ADEBMISC, 
		"diskusage = %d, usedbytes = %d", 
		parms[0], parms[1]);
    
    return xfs_send_message_wakeup_data (fd, h->header.sequence_num, 0,
					 &parms, sizeof(parms));
}

/*
 *
 */

static int
vioc_breakcallback(int fd, struct xfs_message_pioctl *h, u_int size)
{
    int error;
    VenusFid fid;
    FCacheEntry *e;
    CredCacheEntry *ce;

    if (!all_powerful_p(&h->cred))
	return EPERM;

    if (!h->handle.a && !h->handle.b && !h->handle.c && !h->handle.d)
	return EINVAL;

    fid.Cell = h->handle.a;
    fid.fid.Volume = h->handle.b;
    fid.fid.Vnode = h->handle.c;
    fid.fid.Unique = h->handle.d;

    ce = cred_get (fid.Cell, h->cred.pag, CRED_ANY);
    assert (ce != NULL);

    error = fcache_get(&e, fid, ce);
    if (error)
	return error;

    if (!e->flags.kernelp) {
	cred_free (ce);
	return -ENOENT;
    }
	
    break_callback (e);
    
    fcache_release (e);
    cred_free (ce);

    return 0;
}

/*
 * check volume mappings
 */

static int
statistics_hostpart(int fd, struct xfs_message_pioctl *h, u_int size)
{
    u_int32_t host[100];
    u_int32_t part[100];
    u_int32_t outparms[512];
    int n;
    int outsize;
    int maxslots;
    int i;

    if (h->outsize < sizeof(u_int32_t))
	return xfs_send_message_wakeup (fd, h->header.sequence_num,
					EINVAL);
    
    n = 100;
    collectstats_hostpart(host, part, &n);
    maxslots = (h->outsize / sizeof(u_int32_t) - 1) / 2;
    if (n > maxslots)
	n = maxslots;
    
    outsize = (n * 2 + 1) * sizeof(u_int32_t);
    
    outparms[0] = n;
    for (i = 0; i < n; i++) {
	outparms[i*2 + 1] = host[i];
	outparms[i*2 + 2] = part[i];
    }
    
    return xfs_send_message_wakeup_data (fd, h->header.sequence_num, 0,
					 (char *) &outparms, outsize);
}

static int
statistics_entry(int fd, struct xfs_message_pioctl *h, u_int size)
{
    u_int32_t *request = (u_int32_t *) h->msg;
    u_int32_t host;
    u_int32_t part;
    u_int32_t type;
    u_int32_t items_slot;
    u_int32_t count[32];
    int64_t items_total[32];
    int64_t total_time[32];
    u_int32_t outparms[160];
    int i;
    int j;

    if (h->insize < sizeof(u_int32_t) * 5) {
	return xfs_send_message_wakeup (fd, h->header.sequence_num,
					EINVAL);
    }

    if (h->outsize < sizeof(u_int32_t) * 160) {
	return xfs_send_message_wakeup (fd, h->header.sequence_num,
					EINVAL);
    }

    host = request[1];
    part = request[2];
    type = request[3];
    items_slot = request[4];

    collectstats_getentry(host, part, type, items_slot,
			  count, items_total, total_time);

    j = 0;
    for (i = 0; i < 32; i++) {
	outparms[j++] = count[i];
    }
    for (i = 0; i < 32; i++) {
	memcpy(&outparms[j], &items_total[i], 8);
	j+=2;
    }
    for (i = 0; i < 32; i++) {
	memcpy(&outparms[j], &total_time[i], 8);
	j+=2;
    }
    return xfs_send_message_wakeup_data (fd, h->header.sequence_num, 0,
					 (char *) &outparms, sizeof(outparms));
}

static int
aioc_statistics(int fd, struct xfs_message_pioctl *h, u_int size)
{
    u_int32_t opcode;

    if (!all_powerful_p (&h->cred))
	return xfs_send_message_wakeup (fd, h->header.sequence_num,
					EPERM);

    if (h->insize < sizeof(u_int32_t))
	return xfs_send_message_wakeup (fd, h->header.sequence_num,
					EPERM);

    opcode = *((int32_t *)h->msg);

    if (opcode == 0) {
	return statistics_hostpart(fd, h, size);
    } else if (opcode == 1) {
	return statistics_entry(fd, h, size);
    } else {
	return xfs_send_message_wakeup (fd, h->header.sequence_num,
					EINVAL);
    }
}


/*
 * Handle a pioctl message in `h'
 */

static int
xfs_message_pioctl (int fd, struct xfs_message_pioctl *h, u_int size)
{
    int error;

    switch(h->opcode) {
#ifdef KERBEROS
#ifdef VIOCSETTOK_32
    case VIOCSETTOK_32:
    case VIOCSETTOK_64:
#else
    case VIOCSETTOK:
#endif
	error = viocsettok (fd, h, size);
	break;
#ifdef VIOCGETTOK_32
    case VIOCGETTOK_32:
    case VIOCGETTOK_64:
#else
    case VIOCGETTOK :
#endif
	return viocgettok (fd, h, size);
#ifdef VIOCUNPAG_32
    case VIOCUNPAG_32:
    case VIOCUNPAG_64:
#else
    case VIOCUNPAG:
#endif
#ifdef VIOCUNLOG_32
    case VIOCUNLOG_32:
    case VIOCUNLOG_64:
#else
    case VIOCUNLOG:
#endif
	error = viocunlog (fd, h, size);
	break;
#endif /* KERBEROS */
#ifdef VIOCCONNECTMODE_32
    case VIOCCONNECTMODE_32:
    case VIOCCONNECTMODE_64:
#else
    case VIOCCONNECTMODE:
#endif
	return viocconnect(fd, h, size);
#ifdef VIOCFLUSH_32
    case VIOCFLUSH_32:
    case VIOCFLUSH_64:
#else
    case VIOCFLUSH:
#endif
        error = viocflush(fd, h, size);
	break;
#ifdef VIOC_FLUSHVOLUME_32
    case VIOC_FLUSHVOLUME_32:
    case VIOC_FLUSHVOLUME_64:
#else
    case VIOC_FLUSHVOLUME:
#endif
	error = viocflushvolume(fd, h, size);
	break;
#ifdef VIOCGETFID_32
    case VIOCGETFID_32:
    case VIOCGETFID_64:
#else
    case VIOCGETFID:
#endif
	return viocgetfid (fd, h, size);
#ifdef VIOCGETAL_32
    case VIOCGETAL_32:
    case VIOCGETAL_64:
#else
    case VIOCGETAL:
#endif
	return viocgetacl(fd, h, size);
#ifdef VIOCSETAL_32
    case VIOCSETAL_32:
    case VIOCSETAL_64:
#else
    case VIOCSETAL:
#endif
	return viocsetacl(fd, h, size);
#ifdef VIOCGETVOLSTAT_32
    case VIOCGETVOLSTAT_32:
    case VIOCGETVOLSTAT_64:
#else
    case VIOCGETVOLSTAT:
#endif
	return viocgetvolstat(fd, h, size);
#ifdef VIOCSETVOLSTAT_32
    case VIOCSETVOLSTAT_32:
    case VIOCSETVOLSTAT_64:
#else
    case VIOCSETVOLSTAT:
#endif
	error = viocsetvolstat(fd, h, size);
	break;
#ifdef VIOC_AFS_STAT_MT_PT_32
    case VIOC_AFS_STAT_MT_PT_32:
    case VIOC_AFS_STAT_MT_PT_64:
#else
    case VIOC_AFS_STAT_MT_PT:
#endif
	return vioc_afs_stat_mt_pt(fd, h, size);
#ifdef VIOC_AFS_DELETE_MT_PT_32
    case VIOC_AFS_DELETE_MT_PT_32:
    case VIOC_AFS_DELETE_MT_PT_64:
#else
    case VIOC_AFS_DELETE_MT_PT:
#endif
	return vioc_afs_delete_mt_pt(fd, h, size);
#ifdef VIOCWHEREIS_32
    case VIOCWHEREIS_32:
    case VIOCWHEREIS_64:
#else
    case VIOCWHEREIS:
#endif
	return viocwhereis(fd, h, size);
#ifdef VIOCNOP_32
    case VIOCNOP_32:
    case VIOCNOP_64:
#else
    case VIOCNOP:
#endif
	error = EINVAL;
	break;
#ifdef VIOCGETCELL_32
    case VIOCGETCELL_32:
    case VIOCGETCELL_64:
#else
    case VIOCGETCELL:
#endif
	return vioc_get_cell(fd, h, size);
#ifdef VIOC_GETCELLSTATUS_32
    case VIOC_GETCELLSTATUS_32:
    case VIOC_GETCELLSTATUS_64:
#else
    case VIOC_GETCELLSTATUS:
#endif
	return vioc_get_cellstatus(fd, h, size);
#ifdef VIOC_SETCELLSTATUS_32
    case VIOC_SETCELLSTATUS_32:
    case VIOC_SETCELLSTATUS_64:
#else
    case VIOC_SETCELLSTATUS:
#endif
	return vioc_set_cellstatus(fd, h, size);
#ifdef VIOCNEWCELL_32
    case VIOCNEWCELL_32:
    case VIOCNEWCELL_64:
#else
    case VIOCNEWCELL:
#endif
	return vioc_new_cell(fd, h, size);
#ifdef VIOC_VENUSLOG_32
    case VIOC_VENUSLOG_32:
    case VIOC_VENUSLOG_64:
#else
    case VIOC_VENUSLOG:
#endif
	error = viocvenuslog (fd, h, size);
	break;
#ifdef VIOC_AFS_SYSNAME_32
    case VIOC_AFS_SYSNAME_32:
    case VIOC_AFS_SYSNAME_64:
#else
    case VIOC_AFS_SYSNAME:
#endif
	return vioc_afs_sysname (fd, h, size);
#ifdef VIOC_FILE_CELL_NAME_32
    case VIOC_FILE_CELL_NAME_32:
    case VIOC_FILE_CELL_NAME_64:
#else
    case VIOC_FILE_CELL_NAME:
#endif
	return viocfilecellname (fd, h, size);
#ifdef VIOC_GET_WS_CELL_32
    case VIOC_GET_WS_CELL_32:
    case VIOC_GET_WS_CELL_64:
#else
    case VIOC_GET_WS_CELL:
#endif
	return viocgetwscell (fd, h, size);
#ifdef VIOCSETCACHESIZE_32
    case VIOCSETCACHESIZE_32:
    case VIOCSETCACHESIZE_64:
#else
    case VIOCSETCACHESIZE:
#endif
	error = viocsetcachesize (fd, h, size);
	break;
#ifdef VIOCCKSERV_32
    case VIOCCKSERV_32:
    case VIOCCKSERV_64:
#else
    case VIOCCKSERV:
#endif
	return viocckserv (fd, h, size);
#ifdef VIOCGETCACHEPARAMS_32
    case VIOCGETCACHEPARAMS_32:
    case VIOCGETCACHEPARAMS_64:
#else
    case VIOCGETCACHEPARAMS:
#endif
	return viocgetcacheparms (fd, h, size);
#ifdef VIOC_GETRXKCRYPT_32
    case VIOC_GETRXKCRYPT_32:
    case VIOC_GETRXKCRYPT_64:
#else
    case VIOC_GETRXKCRYPT:
#endif
	return getrxkcrypt(fd, h, size);
#ifdef VIOC_SETRXKCRYPT_32
    case VIOC_SETRXKCRYPT_32:
    case VIOC_SETRXKCRYPT_64:
#else
    case VIOC_SETRXKCRYPT:
#endif
	error = setrxkcrypt(fd, h, size);
	break;
#ifdef VIOC_FPRIOSTATUS_32
    case VIOC_FPRIOSTATUS_32:
    case VIOC_FPRIOSTATUS_64:
#else
    case VIOC_FPRIOSTATUS:
#endif
	error = vioc_fpriostatus(fd, h, size);
	break;
#ifdef VIOC_AVIATOR_32
    case VIOC_AVIATOR_32:
    case VIOC_AVIATOR_64:
#else
    case VIOC_AVIATOR:
#endif
	return viocaviator (fd, h, size);
#ifdef VIOC_ARLADEBUG_32
    case VIOC_ARLADEBUG_32:
    case VIOC_ARLADEBUG_64:
#else
    case VIOC_ARLADEBUG:
#endif
	return vioc_arladebug (fd, h, size);
#ifdef VIOC_GCPAGS_32
    case VIOC_GCPAGS_32:
    case VIOC_GCPAGS_64:
#else
    case VIOC_GCPAGS:
#endif
	error = vioc_gcpags (fd, h, size);
	break;
#ifdef VIOC_CALCULATE_CACHE_32
    case VIOC_CALCULATE_CACHE_32:
    case VIOC_CALCULATE_CACHE_64:
#else
    case VIOC_CALCULATE_CACHE:
#endif
	return vioc_calculate_cache (fd, h, size);
#ifdef VIOC_BREAKCALLBACK_32
    case VIOC_BREAKCALLBACK_32:
    case VIOC_BREAKCALLBACK_64:
#else
    case VIOC_BREAKCALLBACK:
#endif	
	error = vioc_breakcallback (fd, h, size);
	break;
#ifdef VIOCCKBACK_32
    case VIOCCKBACK_32 :
    case VIOCCKBACK_64 :
#else
    case VIOCCKBACK :
#endif
	break;

#ifdef VIOCCKBACK_32
    case AIOC_STATISTICS_32:
    case AIOC_STATISTICS_64:
#else
    case AIOC_STATISTICS:
#endif
	return aioc_statistics (fd, h, size);

    default:
	arla_warnx (ADEBMSG, "unknown pioctl call %d", h->opcode);
	error = EINVAL ;
    }

    xfs_send_message_wakeup (fd, h->header.sequence_num, error);
    
    return 0;
}

