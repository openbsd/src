/*	$OpenBSD: messages.c,v 1.2 1999/04/30 01:59:09 art Exp $	*/
/*
 * Copyright (c) 1995, 1996, 1997, 1998, 1999 Kungliga Tekniska Högskolan
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

#include "arla_local.h"
RCSID("$KTH: messages.c,v 1.126 1999/04/14 15:27:36 map Exp $");

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
xfs_message_wakeup (int, struct xfs_message_wakeup*, u_int);

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

typedef int 
(*xfs_message_function) (int, struct xfs_message_header*, u_int);

static xfs_message_function rcvfuncs[] = {
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
NULL						/* advlock */
};

static u_int *seqnums;

static List *sleepers;

/* number of times each type of message has been sent */

static unsigned sent_stat[XFS_MSG_COUNT];

/* number of times each type of message has been received */

static unsigned recv_stat[XFS_MSG_COUNT];

/* count of the number of messages in a write */

static unsigned send_count[9];	/* 8 is the max the multiple stuff handles */

static char *rcvfuncs_name[] = 
{
  "version",
  "wakeup",
  "getroot",
  "installroot",
  "getnode",
  "installnode",
  "getattr",
  "installattr",
  "getdata",
  "installdata",
  "inactivenode",
  "invalidnode",
  "open",
  "put_data",
  "put_attr",
  "create",
  "mkdir",
  "link",
  "symlink",
  "remove",
  "rmdir",
  "rename",
  "pioctl",
  "wakeup_data",
  "updatefid",
  "advlock"
};

/*
 *
 */

long
afsfid2inode (VenusFid *fid)
{
    return ((fid->fid.Volume & 0x7FFF) << 16 | (fid->fid.Vnode & 0xFFFFFFFF));
}

/*
 * AFSFetchStatus -> xfs_attr
 */

static void
afsstatus2xfs_attr (AFSFetchStatus *status,
		    VenusFid *fid,
		    struct xfs_attr *attr)
{
     attr->valid = XA_V_NONE;
     switch (status->FileType) {
	  case TYPE_FILE :
	       XA_SET_MODE(attr, S_IFREG);
	       XA_SET_TYPE(attr, XFS_FILE_REG);
	       break;
	  case TYPE_DIR :
	       XA_SET_MODE(attr, S_IFDIR);
	       XA_SET_TYPE(attr, XFS_FILE_DIR);
	       break;
	  case TYPE_LINK :
	       XA_SET_MODE(attr, S_IFLNK);
	       XA_SET_TYPE(attr, XFS_FILE_LNK);
	       break;
	  default :
	       arla_warnx (ADEBMSG, "afsstatus2xfs_attr: default");
	       abort ();
     }
     XA_SET_NLINK(attr, status->LinkCount);
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

static void
fcacheentry2xfsnode (VenusFid *fid,
		     VenusFid *statfid, 
		     AFSFetchStatus *status,
		     struct xfs_msg_node *node,
                     AccessEntry *ae)
{
    int i;

    afsstatus2xfs_attr (status, statfid, &node->attr);
    memcpy (&node->handle, fid, sizeof(*fid));
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
 *
 */

void
xfs_message_init (void)
{
     unsigned i;

     seqnums = (u_int *)malloc (sizeof (*seqnums) * getdtablesize ());
     if (seqnums == NULL)
	 arla_err (1, ADEBERROR, errno, "xfs_message_init: malloc");
     for (i = 0; i < getdtablesize (); ++i)
	  seqnums[i] = 0;
     sleepers = listnew ();
     if (sleepers == NULL)
	 arla_err (1, ADEBERROR, errno, "xfs_message_init: listnew");
     assert (sizeof(rcvfuncs) / sizeof(*rcvfuncs) == XFS_MSG_COUNT);
     assert (sizeof(rcvfuncs_name) / sizeof(*rcvfuncs_name) == XFS_MSG_COUNT);
}

/*
 *
 */

int
xfs_message_receive (int fd, struct xfs_message_header *h, u_int size)
{
     unsigned opcode = h->opcode;

     if (opcode >= XFS_MSG_COUNT || rcvfuncs[opcode] == NULL ) {
	  arla_warnx (ADEBMSG, "Bad message opcode = %u", opcode);
	  return -1;
     }

     ++recv_stat[opcode];

     arla_warnx (ADEBMSG, "Rec message: opcode = %u (%s), size = %u",
		 opcode, rcvfuncs_name[opcode], h->size);

     return (*rcvfuncs[opcode])(fd, h, size);
}

/*
 *
 */

static int
xfs_message_send (int fd, struct xfs_message_header *h, u_int size)
{
     int res;
     unsigned opcode = h->opcode;

     h->size = size;
     h->sequence_num = seqnums[fd]++;

     if (opcode >= XFS_MSG_COUNT) {
	  arla_warnx (ADEBMSG, "Bad message opcode = %u", opcode);
	  return -1;
     }

     ++sent_stat[opcode];
     ++send_count[1];

     arla_warnx (ADEBMSG, "Send message: opcode = %u (%s), size = %u",
		 opcode, rcvfuncs_name[opcode], h->size);

     if ((res = write (fd, h, size)) < 0) {
	 arla_warn (ADEBMSG, errno, "xfs_message_send: write");
	 return -1;
     } else
	 return 0;
}

static int
xfs_message_wakeup (int fd, struct xfs_message_wakeup *h, u_int size)
{
     Listitem *i;
     struct xfs_message_header *w;

     for (i = listhead (sleepers); i; i = listnext (sleepers, i)) {
	  w = (struct xfs_message_header *)listdata(i);
	  if (w->sequence_num == h->sleepers_sequence_num) {
	       listdel (sleepers, i);
	       memcpy (w, h, size);
	       LWP_SignalProcess ((char *)w);
	  }
     }
     return 0;
}

static int
xfs_message_sleep (struct xfs_message_header *h)
{
     listaddtail (sleepers, h);
     LWP_WaitProcess ((char *)h);
     return ((struct xfs_message_wakeup *)h)->error;
}

static int __attribute__ ((unused))
xfs_message_rpc (int fd, struct xfs_message_header *h, u_int size)
{
     if (size < sizeof (struct xfs_message_wakeup)) {
	  arla_warnx (ADEBMSG, "xfs_message_rpc: Too small packet for rpc");
	  return -1;
     }
     return xfs_message_send (fd, h, size) || xfs_message_sleep (h);
}

static int
xfs_send_message_wakeup (int fd, u_int seqnum, int error)
{
     struct xfs_message_wakeup msg;
     
     msg.header.opcode = XFS_MSG_WAKEUP;
     msg.sleepers_sequence_num = seqnum;
     msg.error = error;
     arla_warnx (ADEBMSG, "sending wakeup: seq = %u, error = %d",
		 seqnum, error);
     return xfs_message_send (fd, (struct xfs_message_header *)&msg, 
			      sizeof(msg));
}

/*
 *
 */

static int
xfs_send_message_wakeup_vmultiple (int fd,
				   u_int seqnum,
				   int error,
				   va_list args)
{
    struct iovec iovec[8];
    struct xfs_message_header *h;
    struct xfs_message_wakeup msg;
    size_t size;
    int i = 0;
    int ret;

    h = va_arg (args, struct xfs_message_header *);
    size = va_arg (args, size_t);
    while (h != NULL) {
	h->size = size;
	h->sequence_num = seqnums[fd]++;
	assert (h->opcode >= 0 && h->opcode < XFS_MSG_COUNT);
	assert (i < 8);
	iovec[i].iov_base = (char *)h;
	iovec[i].iov_len  = size;

	++sent_stat[h->opcode];

	arla_warnx (ADEBMSG, "Multi-send: opcode = %u (%s), size = %u",
		    h->opcode, rcvfuncs_name[h->opcode], h->size);

	h = va_arg (args, struct xfs_message_header *);
	size = va_arg (args, size_t);
	++i;
    }
    msg.header.opcode = XFS_MSG_WAKEUP;
    msg.header.size  = sizeof(msg);
    msg.header.sequence_num = seqnums[fd]++;
    msg.sleepers_sequence_num = seqnum;
    msg.error = error;
    iovec[i].iov_base = (char *)&msg;
    iovec[i].iov_len  = sizeof(msg);

    ++sent_stat[XFS_MSG_WAKEUP];

    arla_warnx (ADEBMSG, "multi-sending wakeup: seq = %u, error = %d",
		seqnum, error);

    ++i;
    
    ++send_count[i];

    ret = writev (fd, iovec, i);
    if (ret < 0) {
	arla_warn (ADEBMSG, errno,
		   "xfs_send_message_wakeup_vmultiple: writev");
	return -1;
    }
    return 0;
}

static int
xfs_send_message_wakeup_multiple (int fd,
				  u_int seqnum,
				  int error,
				  ...)
{
    va_list args;
    int ret;

    va_start (args, error);
    ret = xfs_send_message_wakeup_vmultiple (fd, seqnum, error, args);
    va_end (args);
    return ret;
}

static int
xfs_send_message_wakeup_data (int fd, u_int seqnum, int error,
			      void *data, int size)
{
     struct xfs_message_wakeup_data msg;
     
     msg.header.opcode = XFS_MSG_WAKEUP_DATA;
     msg.sleepers_sequence_num = seqnum;
     msg.error = error;
     arla_warnx (ADEBMSG,
		 "sending wakeup: seq = %u, error = %d", seqnum, error);

     if (sizeof(msg) >= size && size != 0) {
	 memcpy(msg.msg, data, size);
     }

     msg.len = size;

     return xfs_message_send (fd, (struct xfs_message_header *)&msg, 
			      sizeof(msg));
}

/*
 * Return true iff we should retry the operation.
 * Magic with `ce' has to be done as well.
 */

static int
try_again (int *ret, CredCacheEntry **ce, xfs_cred *cred, VenusFid *fid)
{
    switch (*ret) {
#ifdef KERBEROS
    case RXKADEXPIRED :
	cred_expire (*ce);
	cred_free (*ce);
	*ce = cred_get (0, cred->pag, CRED_ANY);
	assert (*ce != NULL);
	return TRUE;
#endif	 
    case ARLA_VSALVAGE :
	*ret = EIO;
	return FALSE;
    case ARLA_VNOVNODE :
	*ret = ENOENT;
	return FALSE;
    case ARLA_VMOVED :
    case ARLA_VNOVOL :
	if (fid) {
	    volcache_invalidate (fid->fid.Volume, fid->Cell);
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
	arla_warnx (ADEBWARN, "Waiting for busy volume...");
	IOMGR_Sleep (1);
	return TRUE;
    case ARLA_VRESTARTING:
	arla_warnx (ADEBWARN, "Waiting fileserver to restart...");
	IOMGR_Sleep (1);
	return TRUE;
    case ARLA_VIO :
	*ret = EIO;
	return FALSE;
    default :
	return FALSE;
    }
}

static int
xfs_message_getroot (int fd, struct xfs_message_getroot *h, u_int size)
{
     struct xfs_message_installroot msg;
     int ret = 0;
     VenusFid root_fid;
     VenusFid real_fid;
     AFSFetchStatus status;
     Result result;
     CredCacheEntry *ce;
     AccessEntry *ae;
     struct xfs_message_header *h0 = NULL;
     size_t h0_len = 0;

     ce = cred_get (0, h->cred.pag, CRED_ANY);
     assert (ce != NULL);
     do {
	 ret = getroot (&root_fid, ce);

	 if (ret == 0) {
	     result = cm_getattr(root_fid, &status, &real_fid, ce, &ae);
	     if (result.res == -1)
		 ret = result.error;
	 }
     } while (try_again (&ret, &ce, &h->cred, &root_fid));

     if (ret == 0) {
	 fcacheentry2xfsnode (&root_fid, &real_fid,
			      &status, &msg.node, ae);

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

     ce = cred_get (dirfid->Cell, h->cred.pag, CRED_ANY);
     assert (ce != NULL);

     do {
	 res = cm_lookup (*dirfid, h->name, &fid, &ce);
	 if (res.res == 0)
	     res = cm_getattr (fid, &status, &real_fid, ce, &ae);

	 if (res.res)
	     ret = res.error;
	 else
	     ret = res.res;
     } while (try_again (&ret, &ce, &h->cred, &fid));

     if (ret == 0) {
	 fcacheentry2xfsnode (&fid, &real_fid, &status, &msg.node, ae);

	 msg.node.tokens = res.tokens;
	 msg.parent_handle = h->parent_handle;
	 strcpy (msg.name, h->name);

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
	 fcacheentry2xfsnode (fid, &real_fid, &status, &msg.node, ae);
	 
	 msg.node.tokens = res.tokens & ~XFS_DATA_MASK;
	 /* XXX - should not clear data mask if kernel already has data */
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

     fid = (VenusFid *)&h->handle;
     xfs_attr2afsstorestatus(&h->attr, &status);
     ce = cred_get (fid->Cell, h->cred.pag, CRED_ANY);
     assert (ce != NULL);

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
	 res = cm_getattr (*fid, &fetch_status, &real_fid, ce, &ae);
	 if (res.res == 0) {
	     fcacheentry2xfsnode (fid, &real_fid,
				  &fetch_status, &msg.node, ae);
	 
	     msg.node.tokens = res.tokens & ~XFS_DATA_MASK;
	     msg.header.opcode = XFS_MSG_INSTALLATTR;
	     h0 = (struct xfs_message_header *)&msg;
	     h0_len = sizeof(msg);
	 } else {
	     ret = res.error;
	 }
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
     FCacheEntry *dir_entry;

     parent_fid = (VenusFid *)&h->parent_handle;
     xfs_attr2afsstorestatus(&h->attr, &store_status);
     ce = cred_get (parent_fid->Cell, h->cred.pag, CRED_ANY);
     assert (ce != NULL);

     do {
	 res = cm_create(*parent_fid, h->name, &store_status,
			 &child_fid, &fetch_status, ce);

	 if (res.res)
	     ret = res.error;
	 else
	     ret = res.res;
     } while (try_again (&ret, &ce, &h->cred, parent_fid));

     if (res.res == 0) {
	 FCacheEntry *child_entry;

	 ret = fcache_get (&dir_entry, *parent_fid, ce);
	 if (ret)
	     goto out;

	 ret = fcache_get_data (dir_entry, ce);
	 if (ret) {
	     fcache_release(dir_entry);
	     goto out;
	 }
	     
	 res = conv_dir (dir_entry, ce, 0,
			 &msg1.cache_handle,
			 msg1.cache_name,
			 sizeof(msg1.cache_name));
	 if (res.res == -1) {
	     fcache_release(dir_entry);
	     ret = res.error;
	     goto out;
	 }

	 msg1.node.tokens = res.tokens;

	 fcacheentry2xfsnode (parent_fid,
			      &dir_entry->realfid,
			      &dir_entry->status,
			      &msg1.node, 
			      dir_entry->acccache);
	 msg1.flag = 0;
	 fcache_release(dir_entry);
	 
	 ret = fcache_get (&child_entry, child_fid, ce);
	 if (ret)
	     goto out;

	 ret = fcache_get_data (child_entry, ce);
	 if (ret) {
	     fcache_release(child_entry);
	     goto out;
	 }

	 msg3.cache_handle = child_entry->handle;
	 fcache_file_name (child_entry,
			   msg3.cache_name, sizeof(msg3.cache_name));
	 msg3.flag = 0;
	 fcache_release(child_entry);

	 msg1.header.opcode = XFS_MSG_INSTALLDATA;
	 h0 = (struct xfs_message_header *)&msg1;
	 h0_len = sizeof(msg1);

	 fcacheentry2xfsnode (&child_fid, &child_fid,
			      &fetch_status, &msg2.node, dir_entry->acccache);
			      
	 msg2.node.tokens   = XFS_ATTR_R | XFS_OPEN_NW | XFS_OPEN_NR; /* XXX */
	 msg2.parent_handle = h->parent_handle;
	 strcpy (msg2.name, h->name);

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

     struct xfs_message_header *h0 = NULL;
     size_t h0_len = 0;
     struct xfs_message_header *h1 = NULL;
     size_t h1_len = 0;
     struct xfs_message_header *h2 = NULL;
     size_t h2_len = 0;

     parent_fid = (VenusFid *)&h->parent_handle;

     ce = cred_get (parent_fid->Cell, h->cred.pag, CRED_ANY);
     assert (ce != NULL);

     xfs_attr2afsstorestatus(&h->attr, &store_status);

     do {
	 res = cm_mkdir(*parent_fid, h->name, &store_status,
			&child_fid, &fetch_status, ce);
	 if (res.res)
	     ret = res.error;
	 else
	     ret = res.res;
     } while(try_again (&ret, &ce, &h->cred, parent_fid));

     if (res.res == 0) {
	 FCacheEntry *dir_entry;
	 FCacheEntry *child_entry;

	 ret = fcache_get (&dir_entry, *parent_fid, ce);
	 if (ret)
	     goto out;

	 ret = fcache_get_data (dir_entry, ce);
	 if (ret) {
	     fcache_release(dir_entry);
	     goto out;
	 }
	     
	 res = conv_dir (dir_entry, ce, 0,
			 &msg1.cache_handle,
			 msg1.cache_name,
			 sizeof(msg1.cache_name));
	 if (res.res == -1) {
	     fcache_release(dir_entry);
	     ret = res.error;
	     goto out;
	 }
	 msg1.node.tokens = res.tokens;

	 fcacheentry2xfsnode (parent_fid,
			      &dir_entry->realfid,
			      &dir_entry->status, &msg1.node, 
			      dir_entry->acccache);
	 msg1.flag = 0;
	 
	 msg1.header.opcode = XFS_MSG_INSTALLDATA;
	 h0 = (struct xfs_message_header *)&msg1;
	 h0_len = sizeof(msg1);
	 fcache_release(dir_entry);

	 ret = fcache_get (&child_entry, child_fid, ce);
	 if (ret)
	     goto out;

	 ret = fcache_get_data (child_entry, ce);
	 if (ret) {
	     fcache_release(child_entry);
	     goto out;
	 }

	 res = conv_dir (child_entry, ce, 0,
			 &msg3.cache_handle,
			 msg3.cache_name,
			 sizeof(msg3.cache_name));
	 if (res.res == -1) {
	     fcache_release(child_entry);
	     ret = res.error;
	     goto out;
	 }
	 msg3.flag = 0;

	 msg2.node.tokens = res.tokens;

	 fcacheentry2xfsnode (&child_fid, &child_fid,
			      &child_entry->status, &msg2.node,
			      dir_entry->acccache);
			      
	 msg2.parent_handle = h->parent_handle;
	 strcpy (msg2.name, h->name);

	 msg2.header.opcode = XFS_MSG_INSTALLNODE;
	 h1 = (struct xfs_message_header *)&msg2;
	 h1_len = sizeof(msg2);
	 fcache_release(child_entry);

	 msg3.header.opcode = XFS_MSG_INSTALLDATA;
	 msg3.node = msg2.node;
	 h2 = (struct xfs_message_header *)&msg3;
	 h2_len = sizeof(msg3);
     }

out:
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

     parent_fid   = (VenusFid *)&h->parent_handle;
     existing_fid = (VenusFid *)&h->from_handle;

     ce = cred_get (parent_fid->Cell, h->cred.pag, CRED_ANY);
     assert (ce != NULL);

     do {
	 res = cm_link (*parent_fid, h->name, *existing_fid,
			&fetch_status, ce);
	 if (res.res)
	     ret = res.error;
	 else
	     ret = res.res;
     } while (try_again (&ret, &ce, &h->cred, parent_fid));

     if (res.res == 0) {
	 FCacheEntry *dir_entry;

	 ret = fcache_get (&dir_entry, *parent_fid, ce);
	 if (ret)
	     goto out;

	 ret = fcache_get_data (dir_entry, ce);
	 if (ret) {
	     fcache_release(dir_entry);
	     goto out;
	 }
	     
	 res = conv_dir (dir_entry, ce, 0,
			 &msg1.cache_handle,
			 msg1.cache_name,
			 sizeof(msg1.cache_name));
	 if (res.res == -1) {
	     fcache_release(dir_entry);
	     ret = res.error;
	     goto out;
	 }
	 msg1.node.tokens = res.tokens;

	 fcacheentry2xfsnode (parent_fid,
			      &dir_entry->realfid,
			      &dir_entry->status, &msg1.node,
			      dir_entry->acccache);
	 msg1.flag = 0;
	 
	 msg1.header.opcode = XFS_MSG_INSTALLDATA;
	 h0 = (struct xfs_message_header *)&msg1;
	 h0_len = sizeof(msg1);
	 fcache_release(dir_entry);

	 fcacheentry2xfsnode (existing_fid, existing_fid,
			      &fetch_status, &msg2.node,
			      dir_entry->acccache);
			      
	 msg2.node.tokens   = XFS_ATTR_R; /* XXX */
	 msg2.parent_handle = h->parent_handle;
	 strcpy (msg2.name, h->name);

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
     VenusFid *parent_fid, child_fid;
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

     parent_fid = (VenusFid *)&h->parent_handle;

     ce = cred_get (parent_fid->Cell, h->cred.pag, CRED_ANY);
     assert (ce != NULL);

     xfs_attr2afsstorestatus(&h->attr, &store_status);

     do {
	 res = cm_symlink(*parent_fid, h->name, &store_status,
			  &child_fid, &fetch_status,
			  h->contents, ce);
	 ret = res.error;
     } while (try_again (&ret, &ce, &h->cred, parent_fid));
     
     cred_free (ce);
     ce = cred_get (parent_fid->Cell, h->cred.pag, CRED_ANY);
     assert (ce != NULL);

     if (res.res == 0) {
	 FCacheEntry *dir_entry;

	 ret = fcache_get (&dir_entry, *parent_fid, ce);
	 if (ret)
	     goto out;

	 ret = fcache_get_data (dir_entry, ce);
	 if (ret) {
	     fcache_release(dir_entry);
	     goto out;
	 }
	     
	 res = conv_dir (dir_entry, ce, 0,
			 &msg1.cache_handle,
			 msg1.cache_name,
			 sizeof(msg1.cache_name));
	 if (res.res == -1) {
	     fcache_release(dir_entry);
	     ret = res.error;
	     goto out;
	 }
	 msg1.flag = 0;
	 msg1.node.tokens = res.tokens;

	 fcacheentry2xfsnode (parent_fid,
			      &dir_entry->realfid,
			      &dir_entry->status, &msg1.node,
			      dir_entry->acccache);
	 
	 msg1.header.opcode = XFS_MSG_INSTALLDATA;
	 h0 = (struct xfs_message_header *)&msg1;
	 h0_len = sizeof(msg1);
	 fcache_release(dir_entry);

	 fcacheentry2xfsnode (&child_fid, &child_fid,
			      &fetch_status, &msg2.node,
			      dir_entry->acccache);
			      
	 msg2.node.tokens   = XFS_ATTR_R; /* XXX */
	 msg2.parent_handle = h->parent_handle;
	 strcpy (msg2.name, h->name);

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
xfs_message_remove (int fd, struct xfs_message_remove *h, u_int size)
{
     VenusFid *parent_fid;
     Result res;
     CredCacheEntry *ce;
     int ret;
     struct xfs_message_installdata msg;
     struct xfs_message_header *h0 = NULL;
     size_t h0_len = 0;

     parent_fid = (VenusFid *)&h->parent_handle;

     ce = cred_get (parent_fid->Cell, h->cred.pag, CRED_ANY);
     assert (ce != NULL);

     do {
	 res = cm_remove(*parent_fid, h->name, ce);

	 if (res.res)
	     ret = res.error;
	 else
	     ret = res.res;
     } while (try_again (&ret, &ce, &h->cred, parent_fid));

     if (res.res == 0) {
	 FCacheEntry *dir_entry;

	 ret = fcache_get (&dir_entry, *parent_fid, ce);
	 if (ret)
	     goto out;

	 ret = fcache_get_data (dir_entry, ce);
	 if (ret) {
	     fcache_release(dir_entry);
	     goto out;
	 }
	     
	 res = conv_dir (dir_entry, ce, 0,
			 &msg.cache_handle,
			 msg.cache_name,
			 sizeof(msg.cache_name));
	 if (res.res == -1) {
	     fcache_release(dir_entry);
	     ret = res.error;
	     goto out;
	 }

	 msg.flag = XFS_INVALID_DNLC;
	 msg.node.tokens = res.tokens;

	 fcacheentry2xfsnode (parent_fid,
			      &dir_entry->realfid,
			      &dir_entry->status, &msg.node,
			      dir_entry->acccache);
	 
	 msg.header.opcode = XFS_MSG_INSTALLDATA;
	 h0 = (struct xfs_message_header *)&msg;
	 h0_len = sizeof(msg);
	 fcache_release(dir_entry);
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

static int 
xfs_message_rmdir (int fd, struct xfs_message_rmdir *h, u_int size)
{
     VenusFid *parent_fid;
     Result res;
     CredCacheEntry *ce;
     int ret;
     struct xfs_message_installdata msg;
     struct xfs_message_header *h0 = NULL;
     size_t h0_len = 0;

     parent_fid = (VenusFid *)&h->parent_handle;

     ce = cred_get (parent_fid->Cell, h->cred.pag, CRED_ANY);
     assert (ce != NULL);

     do {
	 res = cm_rmdir(*parent_fid, h->name, ce);
	 if (res.res)
	     ret = res.error;
	 else
	     ret = res.res;
     } while (try_again (&ret, &ce, &h->cred, parent_fid));

     if (res.res == 0) {
	 FCacheEntry *dir_entry;

	 ret = fcache_get (&dir_entry, *parent_fid, ce);
	 if (ret)
	     goto out;

	 ret = fcache_get_data (dir_entry, ce);
	 if (ret) {
	     fcache_release(dir_entry);
	     goto out;
	 }
	     
	 res = conv_dir (dir_entry, ce, 0,
			 &msg.cache_handle,
			 msg.cache_name,
			 sizeof(msg.cache_name));
	 if (res.res == -1) {
	     fcache_release(dir_entry);
	     ret = res.error;
	     goto out;
	 }
	 msg.flag = XFS_INVALID_DNLC;
	 msg.node.tokens = res.tokens;

	 fcacheentry2xfsnode (parent_fid,
			      &dir_entry->realfid,
			      &dir_entry->status, &msg.node,
			      dir_entry->acccache);
	 
	 msg.header.opcode = XFS_MSG_INSTALLDATA;
	 h0 = (struct xfs_message_header *)&msg;
	 h0_len = sizeof(msg);
	 fcache_release(dir_entry);
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

static int 
xfs_message_rename (int fd, struct xfs_message_rename *h, u_int size)
{
     VenusFid *old_parent_fid;
     VenusFid *new_parent_fid;
     Result res;
     CredCacheEntry *ce;
     int ret;
     struct xfs_message_installdata msg1;
     struct xfs_message_installdata msg2;
     struct xfs_message_header *h0 = NULL;
     size_t h0_len = 0;
     struct xfs_message_header *h1 = NULL;
     size_t h1_len = 0;

     old_parent_fid = (VenusFid *)&h->old_parent_handle;
     new_parent_fid = (VenusFid *)&h->new_parent_handle;

     ce = cred_get (old_parent_fid->Cell, h->cred.pag, CRED_ANY);
     assert (ce != NULL);

     do {
	 res = cm_rename(*old_parent_fid, h->old_name,
			 *new_parent_fid, h->new_name,
			 ce);
	 if (res.res)
	     ret = res.error;
	 else
	     ret = res.res;
     } while (try_again (&ret, &ce, &h->cred, old_parent_fid));

     if (res.res == 0) {
	 FCacheEntry *dir_entry;

	 ret = fcache_get (&dir_entry, *old_parent_fid, ce);
	 if (ret)
	     goto out;

	 ret = fcache_get_data (dir_entry, ce);
	 if (ret) {
	     fcache_release(dir_entry);
	     goto out;
	 }
	     
	 res = conv_dir (dir_entry, ce, 0,
			 &msg1.cache_handle,
			 msg1.cache_name,
			 sizeof(msg1.cache_name));
	 if (res.res == -1) {
	     fcache_release(dir_entry);
	     ret = res.error;
	     goto out;
	 }
	 msg1.flag = XFS_INVALID_DNLC;
	 msg1.node.tokens = res.tokens;

	 fcacheentry2xfsnode (old_parent_fid,
			      &dir_entry->realfid,
			      &dir_entry->status, &msg1.node,
			      dir_entry->acccache);
	 
	 msg1.header.opcode = XFS_MSG_INSTALLDATA;
	 h0 = (struct xfs_message_header *)&msg1;
	 h0_len = sizeof(msg1);
	 fcache_release(dir_entry);

	 /* new parent */

	 ret = fcache_get (&dir_entry, *new_parent_fid, ce);
	 if (ret)
	     goto out;
	 
	 ret = fcache_get_data (dir_entry, ce);
	 if (ret) {
	     fcache_release(dir_entry);
	     goto out;
	 }
	 
	 res = conv_dir (dir_entry, ce, 0,
			 &msg2.cache_handle,
			 msg2.cache_name,
			     sizeof(msg2.cache_name));
	 if (res.res == -1) {
	     fcache_release(dir_entry);
	     ret = res.error;
	     goto out;
	 }
	 msg2.flag = XFS_INVALID_DNLC;
	 msg2.node.tokens = res.tokens;
	 
	 fcacheentry2xfsnode (new_parent_fid,
			      &dir_entry->realfid,
			      &dir_entry->status, &msg2.node,
			      dir_entry->acccache);
	 
	 msg2.header.opcode = XFS_MSG_INSTALLDATA;
	 h1 = (struct xfs_message_header *)&msg2;
	 h1_len = sizeof(msg2);
	 fcache_release(dir_entry);
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
xfs_message_putdata (int fd, struct xfs_message_putdata *h, u_int size)
{
     VenusFid *fid;
     Result res;
     CredCacheEntry *ce;
     int ret;
     AFSStoreStatus status;

     fid = (VenusFid *)&h->handle;
     xfs_attr2afsstorestatus(&h->attr, &status);

     ce = cred_get (fid->Cell, h->cred.pag, CRED_ANY);
     assert (ce != NULL);

     do {
	 res = cm_close(*fid, h->flag, &status, ce);
	 if (res.res)
	     ret = res.error;
	 else
	     ret = res.res;
     } while (try_again (&ret, &ce, &h->cred, fid));
	 
     if (ret)
	 arla_warn (ADEBMSG, ret, "xfs_message_putdata: cm_close");

     cred_free (ce);
     xfs_send_message_wakeup (fd, h->header.sequence_num,
			      res.res ? res.error : res.res);
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

     fid = (VenusFid *)&h->handle;

     ce = cred_get (fid->Cell, h->cred.pag, CRED_ANY);
     assert (ce != NULL);

     do {
	 res = cm_getattr (*fid, &status, &real_fid, ce, &ae);
	 if (res.res)
	     ret = res.error;
	 else
	     ret = res.res;
     } while (try_again (&ret, &ce, &h->cred, fid));

     if (res.res == 0) {
	  fcacheentry2xfsnode (fid, &real_fid, &status, &msg.node, ae);
	  if (status.FileType == TYPE_DIR) {
	       FCacheEntry *entry;

	       ret = fcache_get (&entry, *fid, ce);
	       if (ret)
		   goto out;

	       ret = fcache_get_data (entry, ce);
	       if (ret) {
		   fcache_release(entry);
		   goto out;
	       }

	       res = conv_dir (entry, ce, h->tokens,
			       &msg.cache_handle,
			       msg.cache_name,
			       sizeof(msg.cache_name));
	       if (res.res != -1) {
		   msg.node.tokens = res.tokens;
		   msg.flag = XFS_INVALID_DNLC;
	       }
	       fcache_release(entry);
	  } else {
	       res = cm_open (*fid, ce, h->tokens, &msg.cache_handle,
			      msg.cache_name, sizeof(msg.cache_name));
	       if (res.res != -1)
		   msg.node.tokens = res.tokens;
	  }
     }

     if (res.res != -1) {
	 msg.header.opcode = XFS_MSG_INSTALLDATA;
	 h0 = (struct xfs_message_header *)&msg;
	 h0_len = sizeof(msg);
     }

out:
     cred_free (ce);
     xfs_send_message_wakeup_multiple (fd,
				       h->header.sequence_num,
				       res.res ? res.error : res.res,
				       h0, h0_len,
				       NULL, 0);

     return ret;
}

void
break_callback (VenusFid fid)
{
     struct xfs_message_invalidnode msg;

     msg.header.opcode = XFS_MSG_INVALIDNODE;
     memcpy (&msg.handle, &fid, sizeof(fid));
     xfs_message_send (kernel_fd, (struct xfs_message_header *)&msg, 
		       sizeof(msg));
}

/*
 * Send an unsolicited install-attr for the node in `e'
 */

void
install_attr (FCacheEntry *e)
{
     struct xfs_message_installattr msg;

     msg.header.opcode = XFS_MSG_INSTALLATTR;
     fcacheentry2xfsnode (&e->fid, &e->realfid, &e->status, &msg.node,
			  e->acccache);
     msg.node.tokens   = e->tokens;
     if (!e->flags.datausedp)
	 msg.node.tokens &= ~XFS_DATA_MASK;

     xfs_message_send (kernel_fd, (struct xfs_message_header *)&msg, 
		       sizeof(msg));
}

void
update_kernelfid(VenusFid oldfid, VenusFid newfid)
{
    struct xfs_message_updatefid msg;

    msg.header.opcode = XFS_MSG_UPDATEFID;
    memcpy (&msg.old_handle, &oldfid, sizeof(oldfid));
    memcpy (&msg.new_handle, &newfid, sizeof(newfid));
    xfs_message_send (kernel_fd, (struct xfs_message_header *)&msg,
		      sizeof(msg));
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
     if (h->flag & XFS_DELETE)
	 entry->flags.kernelp = FALSE;
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
	return EINVAL;

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
    int error;

    if (!h->handle.a && !h->handle.b && !h->handle.c && !h->handle.d)
	return EINVAL;

    if (h->insize > AFSOPAQUEMAX)
	return EINVAL;

    if((opaque.val=malloc(AFSOPAQUEMAX))==NULL)
	return ENOMEM;

    fid.Cell = h->handle.a;
    fid.fid.Volume = h->handle.b;
    fid.fid.Vnode = h->handle.c;
    fid.fid.Unique = h->handle.d;

    ce = cred_get (fid.Cell, h->cred.pag, CRED_ANY);
    assert (ce != NULL);

    opaque.len=h->insize;
    memcpy(opaque.val, h->msg, h->insize);

    do {
	error = setacl (fid, ce, &opaque);
    } while (try_again (&error, &ce, &h->cred, &fid));

    if (error != 0 && error != EACCES)
	error = EINVAL;

    cred_free (ce);
    free (opaque.val);
 
    xfs_send_message_wakeup_data (fd, h->header.sequence_num, error,
				  NULL, 0);
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
    int32_t outsize;
    int error;

    if (!h->handle.a && !h->handle.b && !h->handle.c && !h->handle.d)
	return EINVAL;

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
	error = getvolstat (fid, ce, &volstat, volumename,
			    offlinemsg, motd);
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
 * Get the mount point from (fid, filename) and return in `fbuf'.
 * Return 0 or error.
 */

static int
read_mount_point (VenusFid fid, const char *filename, fbuf *the_fbuf,
		  CredCacheEntry *ce,
		  FCacheEntry **ret_mp_entry)
{
    FCacheEntry *mp_entry;
    VenusFid mp_fid;
    int error;
    int mp_fd;
    char *buf;

    if (fid.fid.Volume == 0 && fid.fid.Vnode == 0 && fid.fid.Unique == 0)
	return EINVAL;

    error = adir_lookup(fid, filename, &mp_fid, ce);
    if (error) {
	return error;
    }

    error = fcache_get(&mp_entry, mp_fid, ce);
    if (error) {
	return error;
    }

    error = fcache_get_attr (mp_entry, ce);
    if (error) {
	fcache_release(mp_entry);
	return error;
    }
    if (mp_entry->status.FileType != TYPE_LINK) { /* Is not a mount point */
	fcache_release(mp_entry);
	return EINVAL;
    }
    error = fcache_get_data (mp_entry, ce);
    if (error) {
	fcache_release(mp_entry);
	return error;
    }
    mp_fd = fcache_open_file (mp_entry, O_RDONLY);
    if (mp_fd < 0) {
	fcache_release(mp_entry);
	return errno;
    }
    error = fbuf_create (the_fbuf, mp_fd, mp_entry->status.Length,
			 FBUF_READ);
    if (error) {
	fcache_release(mp_entry);
	return error;
    }
    buf = (char *)(the_fbuf->buf);
    if (buf[0] != '#' && buf[0] != '%') { /* Is not a mount point */
	fbuf_end (the_fbuf);
	fcache_release(mp_entry);
	return EINVAL;
    }

    if(ret_mp_entry)
	*ret_mp_entry = mp_entry;
    else {
	fbuf_end (the_fbuf);
	fcache_release (mp_entry);
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
    fbuf the_fbuf;
    CredCacheEntry *ce;
    FCacheEntry *e;

    fid.Cell       = h->handle.a;
    fid.fid.Volume = h->handle.b;
    fid.fid.Vnode  = h->handle.c;
    fid.fid.Unique = h->handle.d;

    h->msg[min(h->insize, sizeof(h->msg)-1)] = '\0';

    ce = cred_get (fid.Cell, h->cred.pag, CRED_ANY);
    assert (ce != NULL);

    error = read_mount_point (fid, h->msg, &the_fbuf, ce, &e);
    if (error) {
	cred_free(ce);
	return error;
    }

    xfs_send_message_wakeup_data (fd, h->header.sequence_num, error,
				  the_fbuf.buf, the_fbuf.len - 1);
    fbuf_end (&the_fbuf);
    cred_free (ce);
    fcache_release (e);
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
    fbuf the_fbuf;
    CredCacheEntry *ce;
    struct xfs_message_remove remove_msg;

    h->msg[min(h->insize, sizeof(h->msg)-1)] = '\0';

    fid.Cell       = h->handle.a;
    fid.fid.Volume = h->handle.b;
    fid.fid.Vnode  = h->handle.c;
    fid.fid.Unique = h->handle.d;

    ce = cred_get (fid.Cell, h->cred.pag, CRED_ANY);
    assert (ce != NULL);

    error = read_mount_point (fid, h->msg, &the_fbuf, ce, NULL);
    cred_free (ce);
    if (error)
	return error;

    remove_msg.header        = h->header;
    remove_msg.header.size   = sizeof(remove_msg);
    remove_msg.parent_handle = h->handle;
    strcpy(remove_msg.name, h->msg);
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
    int i;
    int32_t addresses[8];

    if (!h->handle.a && !h->handle.b && !h->handle.c && !h->handle.d)
	return EINVAL;

    fid.Cell = h->handle.a;
    fid.fid.Volume = h->handle.b;
    fid.fid.Vnode = h->handle.c;
    fid.fid.Unique = h->handle.d;

    ce = cred_get (fid.Cell, h->cred.pag, CRED_ANY);
    assert (ce != NULL);

    error = fcache_get(&e, fid, ce);
    if (error) {
	cred_free(ce);
	return error;
    }
    error = fcache_get_attr (e, ce);
    if (error) {
	fcache_release(e);
	cred_free(ce);
	return error;
    }
    memset(addresses, 0, sizeof(addresses));
    for (i = 0; (i < e->volume->entry.nServers) && (i < 8); i++)
	addresses[i] = e->volume->entry.serverNumber[i];

    xfs_send_message_wakeup_data (fd, h->header.sequence_num, error,
				  addresses, sizeof(long) * 8);

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
    
    dbservers = cell_dbservers (index, &num_dbservers);

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
	
    hp = (u_int32_t *)h->msg;
    for (i = 0; i < count; ++i) {
	dbs[i].name = NULL;
	dbs[i].addr.s_addr = hp[i];
    }

    cellname = h->msg + 8 * sizeof(u_int32_t);
    ce = cell_get_by_name (cellname);
    if (ce == NULL) {
	ce = cell_new (cellname);

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
    ct.ViceId = h->cred.uid;
    ct.BeginTimestamp = cred->issue_date + 1;
    ct.EndTimestamp   = ce->expire;

    tmp = 0;
    memcpy (p, &tmp, sizeof(tmp));
    p += sizeof(tmp);
    tmp = sizeof(ct);
    memcpy (p, &tmp, sizeof(tmp));
    p += sizeof(tmp);
    memcpy (p, &ct, sizeof(ct));
    p += sizeof(ct);
    tmp = strlen(cell);
    memcpy (p, &tmp, sizeof(tmp));
    p += sizeof(tmp);
    strcpy (p, cell);
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
	CredCacheEntry *ce = cred_get (0, h->cred.pag, CRED_KRB4);

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

    conn_clearcred (cell, h->cred.pag, 2);
    fcache_purge_cred(h->cred.pag, cell);
    cred_add (h->cred.pag, CRED_KRB4, 2, cell, ct.EndTimestamp,
	      &c, sizeof(c), ct.ViceId);
    return 0;
}

static int
viocunlog (int fd, struct xfs_message_pioctl *h, u_int size)
{
    pag_t cred = h->cred.pag;

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

		do_replay(ARLACACHEDIR"/discon_log",
			  log_data.log_entries, 0);
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
	if (rxkad_min_level == rxkad_crypt)
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
	    rxkad_min_level = rxkad_auth;
	else if(n == 1)
	    rxkad_min_level = rxkad_crypt;
	else
	    error = EINVAL;
    } else
	error = EINVAL;
    return error;
#else
    return EOPNOTSUPP;
#endif
}

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
    fprio_status ();
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
	*((u_int32 *)buf) = sysname_len;
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
    int32_t cell = 0; /* Default local cell */
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
 * Handle a pioctl message in `h'
 */

static int
xfs_message_pioctl (int fd, struct xfs_message_pioctl *h, u_int size)
{
    int error;

    switch(h->opcode) {
#ifdef KERBEROS
    case VIOCSETTOK:
	error = viocsettok (fd, h, size);
	break;
    case VIOCGETTOK :
	return viocgettok (fd, h, size);
    case VIOCUNPAG:
    case VIOCUNLOG:
	error = viocunlog (fd, h, size);
	break;
#endif /* KERBEROS */
    case VIOCCONNECTMODE:
	error = viocconnect(fd, h, size);
	break;
    case VIOCFLUSH:
        error = viocflush(fd, h, size);
	break;
    case VIOC_FLUSHVOLUME:
	error = viocflushvolume(fd, h, size);
	break;
    case VIOCGETFID:
	return viocgetfid (fd, h, size);
    case VIOCGETAL:
	error = viocgetacl(fd, h, size);
	break;
    case VIOCSETAL:
	error = viocsetacl(fd, h, size);
	break;
    case VIOCGETVOLSTAT:
	error = viocgetvolstat(fd, h, size);
	break;
    case VIOCSETVOLSTAT:
	error = viocsetvolstat(fd, h, size);
	break;
    case VIOC_AFS_STAT_MT_PT:
	error = vioc_afs_stat_mt_pt(fd, h, size);
	break;
    case VIOC_AFS_DELETE_MT_PT:
	error = vioc_afs_delete_mt_pt(fd, h, size);
	break;
    case VIOCWHEREIS:
	error = viocwhereis(fd, h, size);
	break;
    case VIOCNOP:
	error = EINVAL;
	break;
    case VIOCGETCELL:
	return vioc_get_cell(fd, h, size);
    case VIOC_GETCELLSTATUS:
	return vioc_get_cellstatus(fd, h, size);
    case VIOC_SETCELLSTATUS:
	return vioc_set_cellstatus(fd, h, size);
    case VIOCNEWCELL:
	return vioc_new_cell(fd, h, size);
    case VIOC_VENUSLOG:
	error = viocvenuslog (fd, h, size);
	break;
    case VIOC_AFS_SYSNAME:
	return vioc_afs_sysname (fd, h, size);
    case VIOC_FILE_CELL_NAME:
	return viocfilecellname (fd, h, size);
    case VIOC_GET_WS_CELL:
	return viocgetwscell (fd, h, size);
    case VIOCSETCACHESIZE:
	error = viocsetcachesize (fd, h, size);
	break;
    case VIOCCKSERV:
	return viocckserv (fd, h, size);
    case VIOCGETCACHEPARAMS:
	return viocgetcacheparms (fd, h, size);
    case VIOC_GETRXKCRYPT :
	return getrxkcrypt(fd, h, size);
    case VIOC_SETRXKCRYPT :
	error = setrxkcrypt(fd, h, size);
	break;
    case VIOC_FPRIOSTATUS:
	error = vioc_fpriostatus(fd, h, size);
	break;
    case VIOC_AVIATOR:
	return viocaviator (fd, h, size);
    case VIOC_ARLADEBUG :
	return vioc_arladebug (fd, h, size);
    case VIOC_GCPAGS :
	error = vioc_gcpags (fd, h, size);
	break;
    default:
	arla_warnx (ADEBMSG, "unknown pioctl call %d", h->opcode);
	error = EINVAL ;
    }

    xfs_send_message_wakeup (fd, h->header.sequence_num, error);
    
    return 0;
}
