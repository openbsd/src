/*	$OpenBSD: messages.c,v 1.1.1.1 1998/09/14 21:52:54 art Exp $	*/
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

#include "arla_local.h"
RCSID("$KTH: messages.c,v 1.79 1998/08/17 21:03:20 art Exp $");

#include <xfs/xfs_message.h>

#ifdef HAVE_SYS_IOCTL_H
#include <sys/ioctl.h>
#endif
#include <kerberosIV/kafs.h>

#include "messages.h"

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
NULL	                                        /* wakeup_data */
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
};

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
     XA_SET_ATIME(attr, status->ServerModTime);
     XA_SET_MTIME(attr, status->ServerModTime);
     XA_SET_CTIME(attr, status->ServerModTime);
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

static int
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
    storestatus->Mask = mask ;   

    /* SS_SegSize */
    storestatus->SegSize = 0 ;
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

#ifdef notyet
static int
xfs_message_sleep (struct xfs_message_header *h)
{
     listaddtail (sleepers, h);
     LWP_WaitProcess ((char *)h);
     return ((struct xfs_message_wakeup *)h)->error;
}

static int
xfs_message_rpc (int fd, struct xfs_message_header *h, u_int size)
{
     if (size < sizeof (struct xfs_message_wakeup)) {
	  arla_warnx (ADEBMSG, "xfs_message_rpc: Too small packet for rpc");
	  return -1;
     }
     return xfs_message_send (fd, h, size) || xfs_message_sleep (h);
}
#endif

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

     ret = getroot (&root_fid, ce);
     if (ret)
	 goto out;

     result = cm_getattr(root_fid, &status, &real_fid, ce, &ae);
     if (result.res == -1) {
	 ret = result.error;
	 goto out;
     }	 

     fcacheentry2xfsnode (&root_fid, &real_fid,
			  &status, &msg.node, ae);

     msg.header.opcode = XFS_MSG_INSTALLROOT;
     h0 = (struct xfs_message_header *)&msg;
     h0_len = sizeof(msg);

out:
     if (ret == RXKADEXPIRED) {
	 cred_expire (ce);
	 cred_free (ce);
	 return xfs_message_getroot (fd, h, size);
     }

     cred_free (ce);
     xfs_send_message_wakeup_multiple (fd,
				       h->header.sequence_num,
				       ret,
				       h0, h0_len,
				       NULL, 0);
     return ret;
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

     ce = cred_get (dirfid->Cell, h->cred.pag, CRED_ANY);
     assert (ce != NULL);

     res = cm_lookup (*dirfid, h->name, &fid, &ce);
     if (res.res == 0) {
	  res = cm_getattr (fid, &status, &real_fid, ce, &ae);
	  if (res.res == 0) {
	       fcacheentry2xfsnode (&fid, &real_fid,
				    &status, &msg.node, ae);

	       msg.node.tokens = res.tokens & ~XFS_DATA_MASK;
	       msg.parent_handle = h->parent_handle;
	       strcpy (msg.name, h->name);

	       msg.header.opcode = XFS_MSG_INSTALLNODE;
	       h0 = (struct xfs_message_header *)&msg;
	       h0_len = sizeof(msg);
	  }
     }

     if (res.res != 0 && res.error == RXKADEXPIRED) {
	 cred_expire (ce);
	 cred_free (ce);
	 return xfs_message_getnode (fd, h, size);
     }

     cred_free (ce);
     xfs_send_message_wakeup_multiple (fd,
				       h->header.sequence_num,
				       res.res == -1 ? res.error : res.res,
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

     fid = (VenusFid *)&h->handle;

     ce = cred_get (fid->Cell, h->cred.pag, CRED_ANY);
     assert (ce != NULL);

     res = cm_getattr (*fid, &status, &real_fid, ce, &ae);
     if (res.res == 0) {
	 fcacheentry2xfsnode (fid, &real_fid,
			      &status, &msg.node, ae);
	 
	 msg.node.tokens = res.tokens & ~XFS_DATA_MASK;
	 msg.header.opcode = XFS_MSG_INSTALLATTR;
	 h0 = (struct xfs_message_header *)&msg;
	 h0_len = sizeof(msg);
     }

     if (res.res != 0 && res.error == RXKADEXPIRED) {
	 cred_expire (ce);
	 cred_free (ce);
	 return xfs_message_getattr (fd, h, size);
     }

     cred_free (ce);
     xfs_send_message_wakeup_multiple (fd,
				       h->header.sequence_num,
				       res.res ? res.error : res.res,
				       h0, h0_len,
				       NULL, 0);

     return 0;
}


static int 
xfs_message_putattr (int fd, struct xfs_message_putattr *h, u_int size)
{
     VenusFid *fid;
     AFSStoreStatus status;
     Result res;
     CredCacheEntry *ce;

     fid = (VenusFid *)&h->handle;

     ce = cred_get (fid->Cell, h->cred.pag, CRED_ANY);
     assert (ce != NULL);

     xfs_attr2afsstorestatus(&h->attr, &status);
     res.res = 0;
     if (XA_VALID_SIZE(&h->attr))
	 res = cm_ftruncate (*fid, h->attr.xa_size, ce);

     if (res.res == 0)
	 res = cm_setattr(*fid, &status, ce);

     if (res.res != 0 && res.error == RXKADEXPIRED) {
	 cred_expire (ce);
	 cred_free (ce);
	 return xfs_message_putattr (fd, h, size);
     }

     cred_free (ce);
     xfs_send_message_wakeup (fd, h->header.sequence_num, 
			      res.res ? res.error : res.res);
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

     parent_fid = (VenusFid *)&h->parent_handle;

     ce = cred_get (parent_fid->Cell, h->cred.pag, CRED_ANY);
     assert (ce != NULL);

     xfs_attr2afsstorestatus(&h->attr, &store_status);
     res = cm_create(*parent_fid, h->name, &store_status,
		     &child_fid, &fetch_status, ce);
     if (res.res == 0) {
	 FCacheEntry *dir_entry;
	 FCacheEntry *child_entry;
	 char tmp[5];
	 VenusFid realfid;

	 ret = fcache_get (&dir_entry, *parent_fid, ce);
	 if (ret)
	     goto out;

	 ret = fcache_get_data (dir_entry, ce);
	 if (ret) {
	     ReleaseWriteLock (&dir_entry->lock);
	     goto out;
	 }
	     
	 res = conv_dir (dir_entry, tmp, sizeof(tmp), ce, 0); /* XXX */
	 if (res.res == -1) {
	     ReleaseWriteLock (&dir_entry->lock);
	     ret = res.error;
	     goto out;
	 }
	 strncpy((char *)&msg1.cache_handle, tmp, 4);
	 msg1.node.tokens = res.tokens;

	 if (dir_entry->flags.mountp)
	     realfid = dir_entry->realfid;
	 else
	     realfid = *parent_fid;

	 fcacheentry2xfsnode (parent_fid, &realfid,
			      &dir_entry->status,
			      &msg1.node, 
			      dir_entry->acccache);
	 ReleaseWriteLock (&dir_entry->lock);
	 
	 ret = fcache_get (&child_entry, child_fid, ce);
	 if (ret)
	     goto out;

	 ret = fcache_get_data (child_entry, ce);
	 if (ret) {
	     ReleaseWriteLock (&child_entry->lock);
	     goto out;
	 }

	 sprintf (tmp, "%04X", (unsigned)child_entry->inode); /* XXX */
	 ReleaseWriteLock (&child_entry->lock);

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
	 strncpy((char *)&msg3.cache_handle, tmp, 4); /* XXX */
	 h2 = (struct xfs_message_header *)&msg3;
	 h2_len = sizeof(msg3);
     }

     if (res.res == -1) {
	 ret = res.error;
	 if (ret == RXKADEXPIRED) {
	     cred_expire (ce);
	     cred_free (ce);
	     return xfs_message_create (fd, h, size);
	 }
     } else
	 ret = 0;
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
     struct xfs_message_header *h0 = NULL;
     size_t h0_len = 0;
     struct xfs_message_header *h1 = NULL;
     size_t h1_len = 0;

     parent_fid = (VenusFid *)&h->parent_handle;

     ce = cred_get (parent_fid->Cell, h->cred.pag, CRED_ANY);
     assert (ce != NULL);

     xfs_attr2afsstorestatus(&h->attr, &store_status);
     res = cm_mkdir(*parent_fid, h->name, &store_status,
		    &child_fid, &fetch_status, ce);
     if (res.res == 0) {
	 FCacheEntry *dir_entry;
	 char tmp[5];
	 VenusFid realfid;

	 ret = fcache_get (&dir_entry, *parent_fid, ce);
	 if (ret)
	     goto out;

	 ret = fcache_get_data (dir_entry, ce);
	 if (ret) {
	     ReleaseWriteLock (&dir_entry->lock);
	     goto out;
	 }
	     
	 res = conv_dir (dir_entry, tmp, sizeof(tmp), ce, 0); /* XXX */
	 if (res.res == -1) {
	     ReleaseWriteLock (&dir_entry->lock);
	     ret = res.error;
	     goto out;
	 }
	 strncpy((char *)&msg1.cache_handle, tmp, 4);
	 msg1.node.tokens = res.tokens;

	 if (dir_entry->flags.mountp)
	     realfid = dir_entry->realfid;
	 else
	     realfid = *parent_fid;

	 fcacheentry2xfsnode (parent_fid, &realfid,
			      &dir_entry->status, &msg1.node, 
			      dir_entry->acccache);
	 
	 msg1.header.opcode = XFS_MSG_INSTALLDATA;
	 h0 = (struct xfs_message_header *)&msg1;
	 h0_len = sizeof(msg1);
	 ReleaseWriteLock (&dir_entry->lock);

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

     if (res.res == -1) {
	 ret = res.error;
	 if (ret == RXKADEXPIRED) {
	     cred_expire (ce);
	     cred_free (ce);
	     return xfs_message_mkdir (fd, h, size);
	 }
     } else
	 ret = 0;
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

     res = cm_link (*parent_fid, h->name, *existing_fid,
		    &fetch_status, ce);
     if (res.res == 0) {
	 FCacheEntry *dir_entry;
	 char tmp[5];
	 VenusFid realfid;

	 ret = fcache_get (&dir_entry, *parent_fid, ce);
	 if (ret)
	     goto out;

	 ret = fcache_get_data (dir_entry, ce);
	 if (ret) {
	     ReleaseWriteLock (&dir_entry->lock);
	     goto out;
	 }
	     
	 res = conv_dir (dir_entry, tmp, sizeof(tmp), ce, 0); /* XXX */
	 if (res.res == -1) {
	     ReleaseWriteLock (&dir_entry->lock);
	     ret = res.error;
	     goto out;
	 }
	 strncpy((char *)&msg1.cache_handle, tmp, 4);
	 msg1.node.tokens = res.tokens;

	 if (dir_entry->flags.mountp)
	     realfid = dir_entry->realfid;
	 else
	     realfid = *parent_fid;

	 fcacheentry2xfsnode (parent_fid, &realfid,
			      &dir_entry->status, &msg1.node,
			      dir_entry->acccache);
	 
	 msg1.header.opcode = XFS_MSG_INSTALLDATA;
	 h0 = (struct xfs_message_header *)&msg1;
	 h0_len = sizeof(msg1);
	 ReleaseWriteLock (&dir_entry->lock);

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

     if (res.res == -1) {
	 ret = res.error;
	 if (ret == RXKADEXPIRED) {
	     cred_expire (ce);
	     cred_free (ce);
	     return xfs_message_link (fd, h, size);
	 }
     } else
	 ret = 0;
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
     res = cm_symlink(*parent_fid, h->name, &store_status,
		      &child_fid, &fetch_status,
		      h->contents, ce);
     if (res.res == 0) {
	 FCacheEntry *dir_entry;
	 char tmp[5];
	 VenusFid realfid;

	 ret = fcache_get (&dir_entry, *parent_fid, ce);
	 if (ret)
	     goto out;

	 ret = fcache_get_data (dir_entry, ce);
	 if (ret) {
	     ReleaseWriteLock (&dir_entry->lock);
	     goto out;
	 }
	     
	 res = conv_dir (dir_entry, tmp, sizeof(tmp), ce, 0); /* XXX */
	 if (res.res == -1) {
	     ReleaseWriteLock (&dir_entry->lock);
	     ret = res.error;
	     goto out;
	 }
	 strncpy((char *)&msg1.cache_handle, tmp, 4);
	 msg1.node.tokens = res.tokens;

	 if (dir_entry->flags.mountp)
	     realfid = dir_entry->realfid;
	 else
	     realfid = *parent_fid;

	 fcacheentry2xfsnode (parent_fid, &realfid,
			      &dir_entry->status, &msg1.node,
			      dir_entry->acccache);
	 
	 msg1.header.opcode = XFS_MSG_INSTALLDATA;
	 h0 = (struct xfs_message_header *)&msg1;
	 h0_len = sizeof(msg1);
	 ReleaseWriteLock (&dir_entry->lock);

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

     if (res.res == -1) {
	 ret = res.error;
	 if (ret == RXKADEXPIRED) {
	     cred_expire (ce);
	     cred_free (ce);
	     return xfs_message_symlink (fd, h, size);
	 }
     } else
	 ret = 0;
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

     res = cm_remove(*parent_fid, h->name, ce);
     if (res.res == 0) {
	 FCacheEntry *dir_entry;
	 char tmp[5];
	 VenusFid realfid;

	 ret = fcache_get (&dir_entry, *parent_fid, ce);
	 if (ret)
	     goto out;

	 ret = fcache_get_data (dir_entry, ce);
	 if (ret) {
	     ReleaseWriteLock (&dir_entry->lock);
	     goto out;
	 }
	     
	 res = conv_dir (dir_entry, tmp, sizeof(tmp), ce, 0); /* XXX */
	 if (res.res == -1) {
	     ReleaseWriteLock (&dir_entry->lock);
	     ret = res.error;
	     goto out;
	 }
	 strncpy((char *)&msg.cache_handle, tmp, 4);
	 msg.node.tokens = res.tokens;

	 if (dir_entry->flags.mountp)
	     realfid = dir_entry->realfid;
	 else
	     realfid = *parent_fid;

	 fcacheentry2xfsnode (parent_fid, &realfid,
			      &dir_entry->status, &msg.node,
			      dir_entry->acccache);
	 
	 msg.header.opcode = XFS_MSG_INSTALLDATA;
	 h0 = (struct xfs_message_header *)&msg;
	 h0_len = sizeof(msg);
	 ReleaseWriteLock (&dir_entry->lock);
     }

     if (res.res == -1) {
	 ret = res.error;
	 if (ret == RXKADEXPIRED) {
	     cred_expire (ce);
	     cred_free (ce);
	     return xfs_message_remove (fd, h, size);
	 }
     } else
	 ret = 0;
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

     res = cm_rmdir(*parent_fid, h->name, ce);
     if (res.res == 0) {
	 FCacheEntry *dir_entry;
	 char tmp[5];
	 VenusFid realfid;

	 ret = fcache_get (&dir_entry, *parent_fid, ce);
	 if (ret)
	     goto out;

	 ret = fcache_get_data (dir_entry, ce);
	 if (ret) {
	     ReleaseWriteLock (&dir_entry->lock);
	     goto out;
	 }
	     
	 res = conv_dir (dir_entry, tmp, sizeof(tmp), ce, 0); /* XXX */
	 if (res.res == -1) {
	     ReleaseWriteLock (&dir_entry->lock);
	     ret = res.error;
	     goto out;
	 }
	 strncpy((char *)&msg.cache_handle, tmp, 4);
	 msg.node.tokens = res.tokens;

	 if (dir_entry->flags.mountp)
	     realfid = dir_entry->realfid;
	 else
	     realfid = *parent_fid;

	 fcacheentry2xfsnode (parent_fid, &realfid,
			      &dir_entry->status, &msg.node,
			      dir_entry->acccache);
	 
	 msg.header.opcode = XFS_MSG_INSTALLDATA;
	 h0 = (struct xfs_message_header *)&msg;
	 h0_len = sizeof(msg);
	 ReleaseWriteLock (&dir_entry->lock);
     }

     if (res.res == -1) {
	 ret = res.error;
	 if (ret == RXKADEXPIRED) {
	     cred_expire (ce);
	     cred_free (ce);
	     return xfs_message_rmdir (fd, h, size);
	 }
     } else
	 ret = 0;
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

     res = cm_rename(*old_parent_fid, h->old_name,
		     *new_parent_fid, h->new_name,
		     ce);

     if (res.res == 0) {
	 FCacheEntry *dir_entry;
	 char tmp[5];
	 VenusFid realfid;

	 ret = fcache_get (&dir_entry, *old_parent_fid, ce);
	 if (ret)
	     goto out;

	 ret = fcache_get_data (dir_entry, ce);
	 if (ret) {
	     ReleaseWriteLock (&dir_entry->lock);
	     goto out;
	 }
	     
	 res = conv_dir (dir_entry, tmp, sizeof(tmp), ce, 0); /* XXX */
	 if (res.res == -1) {
	     ReleaseWriteLock (&dir_entry->lock);
	     ret = res.error;
	     goto out;
	 }
	 strncpy((char *)&msg1.cache_handle, tmp, 4);
	 msg1.node.tokens = res.tokens;

	 if (dir_entry->flags.mountp)
	     realfid = dir_entry->realfid;
	 else
	     realfid = *old_parent_fid;

	 fcacheentry2xfsnode (old_parent_fid, &realfid,
			      &dir_entry->status, &msg1.node,
			      dir_entry->acccache);
	 
	 msg1.header.opcode = XFS_MSG_INSTALLDATA;
	 h0 = (struct xfs_message_header *)&msg1;
	 h0_len = sizeof(msg1);
	 ReleaseWriteLock (&dir_entry->lock);

	 /* new parent */

	 ret = fcache_get (&dir_entry, *new_parent_fid, ce);
	 if (ret)
	     goto out;

	 ret = fcache_get_data (dir_entry, ce);
	 if (ret) {
	     ReleaseWriteLock (&dir_entry->lock);
	     goto out;
	 }
	     
	 res = conv_dir (dir_entry, tmp, sizeof(tmp), ce, 0); /* XXX */
	 if (res.res == -1) {
	     ReleaseWriteLock (&dir_entry->lock);
	     ret = res.error;
	     goto out;
	 }
	 strncpy((char *)&msg2.cache_handle, tmp, 4);
	 msg2.node.tokens = res.tokens;

	 if (dir_entry->flags.mountp)
	     realfid = dir_entry->realfid;
	 else
	     realfid = *new_parent_fid;

	 fcacheentry2xfsnode (new_parent_fid, &realfid,
			      &dir_entry->status, &msg2.node,
			      dir_entry->acccache);
	 
	 msg2.header.opcode = XFS_MSG_INSTALLDATA;
	 h1 = (struct xfs_message_header *)&msg2;
	 h1_len = sizeof(msg2);
	 ReleaseWriteLock (&dir_entry->lock);
     }

     if (res.res == -1) {
	 ret = res.error;
	 if (ret == RXKADEXPIRED) {
	     cred_expire (ce);
	     cred_free (ce);
	     return xfs_message_rename (fd, h, size);
	 }
     } else
	 ret = 0;
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

     fid = (VenusFid *)&h->handle;

     ce = cred_get (fid->Cell, h->cred.pag, CRED_ANY);
     assert (ce != NULL);

     res = cm_close(*fid, h->flag, ce);
     if (res.res != 0)
	 arla_warn (ADEBMSG, res.error, "xfs_message_putdata: cm_close");

     if (res.res != 0 && res.error == RXKADEXPIRED) {
	 cred_expire (ce);
	 cred_free (ce);
	 return xfs_message_putdata (fd, h, size);
     }


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
     char tmp[5];
     CredCacheEntry *ce;
     int ret;
     AccessEntry *ae;
     struct xfs_message_header *h0 = NULL;
     size_t h0_len = 0;

     fid = (VenusFid *)&h->handle;

     ce = cred_get (fid->Cell, h->cred.pag, CRED_ANY);
     assert (ce != NULL);

     res = cm_getattr (*fid, &status, &real_fid, ce, &ae);
     if (res.res == 0) {
	  fcacheentry2xfsnode (fid, &real_fid, &status, &msg.node, ae);
	  if (status.FileType == TYPE_DIR) {
	       FCacheEntry *entry;

	       ret = fcache_get (&entry, *fid, ce);
	       if (ret)
		   goto out;

	       ret = fcache_get_data (entry, ce);
	       if (ret) {
		   ReleaseWriteLock (&entry->lock);
		   goto out;
	       }

	       res = conv_dir (entry, tmp, sizeof(tmp), ce, h->tokens);
	       if (res.res != -1) {
		    strncpy ((char *)&msg.cache_handle, tmp, 4); /* XXX */
		    msg.node.tokens = res.tokens;
	       }
	       ReleaseWriteLock(&entry->lock);
	  } else {
	       res = cm_open (*fid, ce, h->tokens);
	       if (res.res != -1) {
		    sprintf (tmp, "%04X", res.res);         /* XXX */
		    strncpy ((char *)&msg.cache_handle, tmp, 4);  /* XXX */
		    msg.node.tokens = res.tokens;
	       }
	  }
     }

     if (res.res != -1) {
	 msg.header.opcode = XFS_MSG_INSTALLDATA;
	 h0 = (struct xfs_message_header *)&msg;
	 h0_len = sizeof(msg);
     }
     if (res.res == -1) {
	 ret = res.error;
	 if (ret == RXKADEXPIRED) {
	     cred_expire (ce);
	     cred_free (ce);
	     return xfs_message_getdata (fd, h, size);
	 }
     } else
	 ret = 0;

out:
     cred_free (ce);
     xfs_send_message_wakeup_multiple (fd,
				       h->header.sequence_num,
				       ret,
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
     xfs_message_send (kernel_fd , (struct xfs_message_header *)&msg, 
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
	 entry->flags.datausedp = entry->flags.attrusedp = FALSE;
     if (h->flag & XFS_DELETE)
	 entry->flags.kernelp = FALSE;
     ReleaseWriteLock (&entry->lock);
     return 0;
}



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

    error = getacl (fid, ce, &opaque);

    if (error == RXKADEXPIRED) {
	cred_expire (ce);
	cred_free (ce);
	return viocgetacl (fd, h, size);
    } else if (error != 0 && error != EACCES)
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

    error = setacl (fid, ce, &opaque);

    if (error == RXKADEXPIRED) {
	cred_expire (ce);
	cred_free (ce);
	return viocsetacl (fd, h, size);
    } else if (error != 0 && error != EACCES)
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

    error = getvolstat (fid, ce, &volstat, volumename,
			offlinemsg, motd);

    cred_free (ce);

    if (error == RXKADEXPIRED) {
	cred_expire (ce);
	cred_free (ce);
	return viocgetvolstat (fd, h, size);
    } else if (error != 0 && error != EACCES)
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

    error = setvolstat (fid, ce, &outvolstat, volumename,
			offlinemsg, motd);

    if (error == RXKADEXPIRED) {
	cred_expire (ce);
	cred_free (ce);
	return viocsetvolstat (fd, h, size);
    } else if (error != 0 && error != EACCES)
	error = EINVAL;

    cred_free (ce);

    xfs_send_message_wakeup_data (fd, h->header.sequence_num, error,
				  NULL, 0);
    return 0;
}

/*
 * Get info for a mount point.
 */

static int
vioc_afs_stat_mt_pt(int fd, struct xfs_message_pioctl *h, u_int size)
{
    VenusFid fid;
    VenusFid res;
    CredCacheEntry *ce;
    FCacheEntry *e;
    fbuf the_fbuf;
    char *buf;
    int error;
    int symlink_fd;

    if (!h->handle.a && !h->handle.b && !h->handle.c && !h->handle.d)
	return EINVAL;

    fid.Cell = h->handle.a;
    fid.fid.Volume = h->handle.b;
    fid.fid.Vnode = h->handle.c;
    fid.fid.Unique = h->handle.d;

    ce = cred_get (fid.Cell, h->cred.pag, CRED_ANY);
    assert (ce != NULL);

    error = adir_lookup(fid, h->msg, &res, ce);
    if (error) {
	cred_free(ce);
	return error;
    }
    error = fcache_get(&e, res, ce);
    if (error) {
	cred_free(ce);
	return error;
    }
    error = fcache_get_attr (e, ce);
    if (error) {
	ReleaseWriteLock (&e->lock);
	cred_free(ce);
	return error;
    }
    if (e->status.FileType != TYPE_LINK) { /* Is not a mount point */
	ReleaseWriteLock (&e->lock);
	cred_free(ce);
	return EINVAL;
    }
    error = fcache_get_data (e, ce);
    if (error) {
	ReleaseWriteLock (&e->lock);
	cred_free(ce);
	return error;
    }
    symlink_fd = fcache_open_file (e, O_RDONLY, 0);
    if (symlink_fd < 0) {
	ReleaseWriteLock (&e->lock);
	cred_free(ce);
	return errno;
    }
    error = fbuf_create (&the_fbuf, symlink_fd, e->status.Length, FBUF_READ);
    if (error) {
	ReleaseWriteLock (&e->lock);
	cred_free(ce);
	return error;
    }
    buf = (char *)(the_fbuf.buf);
    if (buf[0] != '#' && buf[0] != '%') { /* Is not a mount point */
	ReleaseWriteLock (&e->lock);
	fbuf_end (&the_fbuf);
	cred_free (ce);
	return EINVAL;
    }

    xfs_send_message_wakeup_data (fd, h->header.sequence_num, error,
				  the_fbuf.buf, the_fbuf.len - 1);

    ReleaseWriteLock (&e->lock);
    fbuf_end (&the_fbuf);
    cred_free (ce);

    return 0;
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
	ReleaseWriteLock (&e->lock);
	cred_free(ce);
	return error;
    }
    memset(addresses, 0, sizeof(addresses));
    for (i = 0; (i < e->volume->entry.nServers) && (i < 8); i++)
	addresses[i] = e->volume->entry.serverNumber[i];

    xfs_send_message_wakeup_data (fd, h->header.sequence_num, error,
				  addresses, sizeof(long) * 8);

    ReleaseWriteLock (&e->lock);
    cred_free (ce);

    return 0;
}

static int
viocgetcell(int fd, struct xfs_message_pioctl *h, u_int size)
{
    int i;
    int32_t index;
    const char *cellname;
    int cellname_len;
    int outsize;
    char out[8 * sizeof(int32_t) + MAXPATHLEN]; /* XXX */

    index = *((int32_t *) h->msg);
    cellname = cell_num2name(index);
    if (cellname == NULL)
	return EDOM;
    
    memset(out, 0, sizeof(out));
    cellname_len = strlen(cellname) + 1;
    if (cellname_len > MAXPATHLEN)
	cellname_len = MAXPATHLEN;
    memcpy(out + 8 * sizeof(int32_t), cellname, cellname_len);
    outsize = 8 * sizeof(int32_t) + cellname_len;
    for (i = 0; i < 8; i++) {
	u_long addr = cell_listdbserver(index, i);
	if (addr == 0)
	    break;
	memcpy (&out[i * sizeof(int32_t)], &addr, sizeof(int32_t));
    }

    xfs_send_message_wakeup_data (fd, h->header.sequence_num, 0,
				  out, outsize);

    return 0;
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
    ct.ViceId = h->cred.pag;
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
#endif /* KERBEROS */

static int
viocflush (int fd, struct xfs_message_pioctl *h, u_int size)
{
    VenusFid fid ;
    AFSCallBack broken_callback = {0, 0, CBDROPPED};

    if (!h->handle.a && !h->handle.b && !h->handle.c && !h->handle.d)
	return EINVAL;

    fid.Cell = h->handle.a;
    fid.fid.Volume = h->handle.b;
    fid.fid.Vnode = h->handle.c;
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
	    connected_mode = CONNECTED ;
	    break;
	case CONNMODE_FETCH:
	    connected_mode = FETCH_ONLY ;
	    break;
	case CONNMODE_DISCONN:
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

static void
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

	xfs_send_message_wakeup_data (fd,
				      h->header.sequence_num,
				      0,
				      &n,
				      sizeof(n));
    } else
	xfs_send_message_wakeup (fd, h->header.sequence_num, EINVAL);
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
xfs_message_pioctl (int fd, struct xfs_message_pioctl *h, u_int size)
{
    int32_t sizeof_x;
    char *t;
    int error;

    t = h->msg ;
    switch(h->opcode) {
#ifdef KERBEROS
    case VIOCSETTOK: {
	struct ClearToken ct;
	CREDENTIALS c;
	long cell;

	/* someone probed us */
	if (h->insize == 0) {
	    error = EINVAL ;
	    break;
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

	/* XXX fix ct.ViceId */
	conn_clearcred (cell, h->cred.pag, 2);
	fcache_purge_cred(h->cred.pag, cell);
	cred_add (h->cred.pag, CRED_KRB4, 2, cell, ct.EndTimestamp,
		  &c, sizeof(c));
	

	error = 0 ;
	break;
    }
    case VIOCGETTOK :
	return viocgettok (fd, h, size);
    case VIOCUNPAG:
    case VIOCUNLOG: {
	pag_t cred = h->cred.pag ;

	cred_remove(cred) ;
	fcache_purge_cred(cred, -1);
	error = 0;
	break ;
    }
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
	error = xfs_send_message_wakeup_data(fd, h->header.sequence_num, 0,
					     &h->handle, sizeof(VenusFid));
	break;
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
    case VIOCWHEREIS:
	error = viocwhereis(fd, h, size);
	break;
    case VIOCNOP:
	error = EINVAL;
	break;
    case VIOCGETCELL:
	error = viocgetcell(fd, h, size);
	break;
    case VIOC_VENUSLOG:
	if (h->cred.uid != 0) {
	    error = EACCES;
	    break ;
	}
	    
	conn_status (stderr);
	volcache_status (stderr);
	cred_status (stderr);
	fcache_status (stderr);
	rx_PrintStats(stderr);
	error = 0 ;
	break;
    case VIOC_AFS_SYSNAME: {
	char str[SYSNAMEMAXLEN+sizeof(int32_t)];

	error = 0 ;

	if (*((int32_t *)t)) {
	    t += sizeof(int32_t);
	    arla_warnx (ADEBMSG, "VIOC_AFS_SYSNAME: setting sysname: %s", t);
	    memcpy(arlasysname, t, h->insize);
	    arlasysname[h->insize] = '\0';
	    xfs_send_message_wakeup_data(fd, h->header.sequence_num, error,
					 str, 0);
	} else {
	    t = str;
	    sizeof_x = strlen(arlasysname);
	    memcpy(t, &sizeof_x, sizeof(sizeof_x));
	    t += sizeof(sizeof_x);
	    h->outsize = sizeof_x;
	    strncpy(t, arlasysname, SYSNAMEMAXLEN);
	    xfs_send_message_wakeup_data(fd, h->header.sequence_num, error,
					 str, sizeof_x + sizeof(sizeof_x));
	}
	return 0;
    }

    case VIOC_FILE_CELL_NAME: {
	char *cellname ;

	error = 0 ;
	cellname = (char *) cell_num2name(h->handle.a);

	if (cellname) 
	    xfs_send_message_wakeup_data(fd, h->header.sequence_num, error,
					 cellname, strlen(cellname)+1);
	else 
	    xfs_send_message_wakeup_data(fd, h->header.sequence_num, EINVAL,
					 NULL, 0);
	return 0;
    }
    case VIOC_GET_WS_CELL: {
	char *cellname;

	cellname = (char*) cell_getthiscell();
	xfs_send_message_wakeup_data(fd, h->header.sequence_num, 0 /*error*/,
				     cellname, strlen(cellname));

	return 0;
    }
    case VIOCSETCACHESIZE: {
	u_int32_t *s = (u_int32_t *)t;

	if (h->cred.uid != 0) {
	    error = EPERM;
	    break ;
	}
	
	if (h->insize >= sizeof(int32_t) * 4) 
	    error = fcache_reinit(s[0], s[1], s[2], s[3]);
	else
	    error = fcache_reinit(*s/2, *s, *s*500, *s*1000);
	break;
    }
    case VIOC_GETRXKCRYPT :
	getrxkcrypt(fd, h, size);
	return 0;
    case VIOC_SETRXKCRYPT :
	error = setrxkcrypt(fd, h, size);
	break;
    case VIOC_FPRIOSTATUS:
	error = vioc_fpriostatus(fd, h, size);
	break;
    default:
	arla_warnx (ADEBMSG, "unknown pioctl call %d", h->opcode);
	error = EINVAL ;
    }

    xfs_send_message_wakeup (fd, h->header.sequence_num, error);
    
    return 0;
}
