/*
 * Copyright (c) 1999 - 2003 Kungliga Tekniska Högskolan
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

#include "fsrv_locl.h"

RCSID("$arla: fsprocs.c,v 1.68 2003/04/08 00:13:35 lha Exp $");

#define GETHOST(call) rx_HostOf(rx_PeerOf(rx_ConnectionOf((call))))
#define GETPORT(call) rx_PortOf(rx_PeerOf(rx_ConnectionOf((call))))

int
createentry(struct rx_call *call,
	    const struct AFSFid *DirFid,
	    const char *Name,
	    const char *Contents,
	    const struct AFSStoreStatus *InStatus,
	    const struct AFSFid *ExistingFid,
	    struct AFSFid *EntryFid,
	    struct AFSFetchStatus *OutFidStatus,
	    struct AFSFetchStatus *OutDirStatus,
	    struct AFSCallBack *CallBack,
	    struct AFSVolSync *VolSync);

/*
 * Initlize all fields of `m' except `m->flags'.
 */

static int
fs_init_msec (struct rx_call *call, struct msec *m)
{
    volop_flags flags;
    assert (call && m);
    flags = m->flags;

    memset (m, 0, sizeof (*m));
    m->flags = flags;
    if (call) {
	m->sec = fs_connsec_context_get(call->conn);
    } else {
	m->sec = NULL; 
    }

    m->loop = 0;
    return 0;
}

/*
 * Update FetchStatus `fs' to reflect `n' and `m'.
 */

static void
fs_update_fs (const struct mnode *n, const struct msec *m,
	      AFSFetchStatus *fs)
{
    *fs 		= n->fs;
    fs->CallerAccess	= m->caller_access;
    fs->AnonymousAccess	= m->anonymous_access;
}

/*
 * return a non zero value if the user is the superuser.
 */

static int
super_user (const struct msec *m)
{
    return m->sec->superuser;
}

/*
 * If check if the user have the rights to change `status' the way the
 * user want to with the right `sec'. If the entry is about to be
 * created, set owner if that isn't set.
 */

static int
check_ss_bits (const struct msec *m, const AFSStoreStatus *status,
	       Bool createp)
{
    /* check if member of system:administrators
     */

    if ((status->Mask & SS_OWNER) != 0) {
	if (status->Owner != m->sec->uid && !super_user(m))
	    return EPERM;
    } else if (createp) {
	((AFSStoreStatus *)status)->Mask |= SS_OWNER; /* XXX */
	((AFSStoreStatus *)status)->Owner = m->sec->uid; /* XXX */
    }

    if ((status->Mask & SS_MODEBITS) != 0) {
	if ((07000 & status->UnixModeBits) != 0 && !super_user(m))
	    return EPERM;
    }
    return 0;
}

/*
 * Given `fid', `volh' and `m' open node `n', check rights.
 */

static int
fs_open_node (const AFSFid *fid, struct volume_handle *volh,
              struct msec *m, struct mnode **n)
{
    int ret;

    ret = mnode_find (fid, n);
    if (ret)
        return ret;

    ret = vld_open_vnode (volh, *n, m);
    if (ret == 0)
	ret = vld_check_rights (volh, *n, m);

    if (ret)
	mnode_free (*n, FALSE);

    return ret;
}

/*
 * Given `fid' and `call', init `m', `volh' and `n'.
 *
 * If function returns with 0, `volh' and `n' needs to be free:ed when
 * no longer needed.
 */


static int
fs_init_req (const AFSFid *fid, struct msec *m, struct volume_handle **volh,
	     struct rx_call *call, struct mnode **n)
{
    int ret;
	     
    ret = vld_check_busy(fid->Volume, -1);
    if (ret)
	return ret;

    ret = fs_init_msec (call, m);
    if (ret)
	return ret;

    ret = vld_find_vol (fid->Volume, volh);
    if (ret)
	return ret;

    if (n != NULL && (*volh)->flags.offlinep == TRUE) {
	int ret;
	if ((*volh)->flags.attacherr)
	    ret = VOFFLINE;
	else if ((*volh)->flags.salvaged)
	    ret = VSALVAGE;
	else
	    ret = VOFFLINE;
	vld_free (*volh);
	return ret;
    }

#define VOLOP_MODIFY (VOLOP_ADMIN|VOLOP_DELETE|VOLOP_WRITE|\
			VOLOP_INSERT|VOLOP_LOCK)

    ret = vld_info_uptodatep (*volh);
    if (ret) {
	vld_free (*volh);
	return ret;
    }
    
    if ((*volh)->info.type != RWVOL &&
	(m->flags & VOLOP_MODIFY))
    {
	vld_free (*volh);
	return MILKO_ROFS;
    }

    ret = vld_db_uptodate (*volh);
    if (ret) {
	vld_free (*volh);
	return ret;
    }
    
    if (n != NULL) {
	ret = fs_open_node (fid, *volh, m, n);
	if (ret) {
	    vld_free (*volh);
	    return ret;
	}
    }

    return ret;
}

/*
 *
 */

int
SRXAFS_FetchData(struct rx_call *call,
		 const struct AFSFid *a_fidToFetchP,
		 const int32_t a_offset,
		 const int32_t a_lenInBytes,
		 struct AFSFetchStatus *a_fidStatP,
		 struct AFSCallBack *a_callBackP,
		 struct AFSVolSync *a_volSyncP)
{
    struct volume_handle *volh;

    int32_t len, net_len;
    struct mnode *n;
    struct msec m;
    int ret;
    int haveData = 1;

    mlog_log (MDEBFS, "FetchData: fid: %d.%d.%d, len = %d, offset = %d", 
	      a_fidToFetchP->Volume, a_fidToFetchP->Vnode,
	      a_fidToFetchP->Unique, a_lenInBytes, a_offset);

    m.flags = VOLOP_GETSTATUS | VOLOP_READ;

    ret = fs_init_req (a_fidToFetchP, &m, &volh, call, &n);
    if (ret) {
	mlog_log (MDEBFS, "FetchData: fs_init_req returned %d", ret);
	return ret;
    }

    if (n->fs.Length < a_offset) {
/*	ret = EINVAL; */
	mlog_log (MDEBFS, "FetchData: invalid offset (%d < %d)", 
		  n->fs.Length, a_offset); 
	haveData = 0;
	len = 0;
/*	goto out; */
    } else
	len = min(a_lenInBytes, n->fs.Length - a_offset);

    net_len = ntohl(len);

    if (rx_Write (call, &net_len, sizeof(net_len)) != sizeof(net_len)) {
	ret = errno;
	mlog_log (MDEBFS, "FetchData: rx_Write returned %d", ret);
	goto out;
    }
    
    if (haveData) {
	ret = copyfd2rx (n->fd, call, a_offset, len);
	if (ret) {
	    mlog_log (MDEBFS, "FetchData: copyfd2rx returned %d", ret);
	    goto out;
	}
    }

    fs_update_fs (n, &m, a_fidStatP);

    ret = vld_info_uptodatep (volh);
    if (ret)
	goto out;

    ropa_getcallback (GETHOST(call), GETPORT(call),
		      a_fidToFetchP, a_callBackP, volh->info.type);
    vld_vld2volsync (volh, a_volSyncP);

 out:
    mnode_free (n, FALSE);
    vld_free (volh);

    mlog_log (MDEBFS, "FetchData: ret = %d (at end), calleraccess = %x\n",
	      ret, a_fidStatP->CallerAccess);

    return ret;
}

/*
 *
 */

static int
i2nlist (idlist *ilist, namelist *nlist)
{
    int i;
    /* XXX convert the number is the ilist to name-as-numbers */
    
    nlist->val = malloc(sizeof(nlist->val[0]) * ilist->len);
    if (nlist->val == NULL)
	return ENOMEM;

    for (i = 0; i < ilist->len; i++)
	snprintf(nlist->val[i], sizeof(nlist->val[i]), "%d", ilist->val[i]);
    
    nlist->len = ilist->len;

    return 0;
}


/*
 *
 */

int
SRXAFS_FetchACL(struct rx_call *call,
		const struct AFSFid *a_dirFidP,
		AFSOpaque *a_ACLP,
		struct AFSFetchStatus *a_dirNewStatP,
		struct AFSVolSync *a_volSyncP)
{
    struct volume_handle *volh;
    struct mnode *n;
    struct msec m;
    int ret;
    int i, j;
    char *tempacl, *tempacl_old;
    int num_negacl, num_posacl;
    namelist nlist;
    idlist ilist;

    mlog_log (MDEBFS, "FetchACL: fid: %d.%d.%d", 
	      a_dirFidP->Volume, a_dirFidP->Vnode,
	      a_dirFidP->Unique);

    m.flags = VOLOP_GETSTATUS;

    ret = fs_init_req (a_dirFidP, &m, &volh, call, &n);
    if (ret)
	return ret;

    fs_update_fs (n, &m, a_dirNewStatP);

    assert (n->flags.ep);
    assert (n->flags.fsp);

    if (n->fs.FileType != TYPE_DIR) {
	mnode_free (n, FALSE);
	return EPERM;
    }

    j = 0;
    num_negacl = 0;
    num_posacl = 0;
    nlist.len = 0;
    nlist.val = NULL;
    ilist.val = malloc(2*FS_MAX_ACL*sizeof(int32_t));

    for (i = 0; i < FS_MAX_ACL && n->e.u.dir.acl[i].owner != 0; i++, j++)
	ilist.val[j] = n->e.u.dir.acl[i].owner;
    num_posacl = j;

    for (i = 0; i < FS_MAX_ACL && n->e.u.dir.negacl[i].owner != 0; i++, j++)
	ilist.val[j] = n->e.u.dir.negacl[i].owner;
    num_negacl = j - num_posacl;

    ilist.len = j;

    ret = fs_connsec_idtoname(&ilist, &nlist);
    switch (ret) {
    case ENETDOWN :
    case RX_CALL_DEAD : 

	ret = i2nlist (&ilist, &nlist);
	if (ret)
	    goto err_out;
	
	break;
    case 0:
	break;
    default:
	goto err_out;
    }

    tempacl = NULL;
    tempacl_old = strdup("");

    /* Make string with all positive ACL:s */ 
    for (i = 0; i < num_posacl; i++) {
	if (asprintf(&tempacl, "%s%s %d\n",
		     tempacl_old,
		     nlist.val[i],
		     n->e.u.dir.acl[i].flags) == -1) {
	    ret = EINVAL /* XXX what is the error code? */;
	    free(tempacl_old);
	    goto err_out;
	}
	free(tempacl_old);
	tempacl_old = tempacl;
	tempacl = NULL;
    }

    /* Add negative ACL:s to string */ 
    for (i = 0; i < num_negacl; i++) {
	if (asprintf(&tempacl, "%s%s %d\n",
		     tempacl_old,
		     nlist.val[i+num_posacl],
		     n->e.u.dir.negacl[i].flags) == -1) {
	    ret = EINVAL /* XXX what is the error code? */;
	    free(tempacl_old);
	    goto err_out;
	}
	free(tempacl_old);
	tempacl_old = tempacl;
	tempacl = NULL;
    }

    asprintf(&tempacl, "%d\n%d\n%s",
	     num_posacl, num_negacl, tempacl_old);
    free(tempacl_old);
    tempacl_old = NULL;

    a_ACLP->len = max(strlen(tempacl), AFSOPAQUEMAX);
    a_ACLP->val = tempacl;
    tempacl[a_ACLP->len - 1] = '\0';

 err_out:
    free(ilist.val);
    free(nlist.val);

    vld_vld2volsync (volh, a_volSyncP);

    mnode_free (n, FALSE);
    vld_free (volh);

    return ret;
}

/*
 *
 */

int
SRXAFS_FetchStatus(struct rx_call *call,
		   const struct AFSFid *a_fidToStatP,
		   struct AFSFetchStatus *a_currStatP,
		   struct AFSCallBack *a_callBackP,
		   struct AFSVolSync *a_volSyncP)
{
    struct volume_handle *volh;
    struct mnode *n;
    struct msec m;
    int ret;

    mlog_log (MDEBFS, "FetchStatus: fid: %u.%u.%u", 
	      (uint32_t)a_fidToStatP->Volume, (uint32_t)a_fidToStatP->Vnode,
	      (uint32_t)a_fidToStatP->Unique);

    m.flags = VOLOP_GETSTATUS;

    ret = fs_init_req (a_fidToStatP, &m, &volh, call, &n);
    if (ret)
	goto out;

    fs_update_fs (n, &m, a_currStatP);

    ret = vld_info_uptodatep (volh);
    if (ret)
	goto out_free;

    ropa_getcallback (GETHOST(call), GETPORT(call),
		      a_fidToStatP, a_callBackP, volh->info.type);
    vld_vld2volsync (volh, a_volSyncP);


 out_free:
    mnode_free (n, FALSE);
    vld_free (volh);

 out:
    mlog_log (MDEBFS, "FetchStatus: ret = %d (at end), calleraccess = %x\n",
	      ret, a_currStatP->CallerAccess);

    return ret;
}

/*
 *
 */

int
SRXAFS_StoreData(struct rx_call *call,
		 const struct AFSFid *a_fidToStoreP,
		 const struct AFSStoreStatus *a_fidStatusP,
		 const int32_t a_offset,
		 const int32_t a_lenInBytes,
		 const int32_t a_fileLenInBytes,
		 struct AFSFetchStatus *a_fidStatP,
		 struct AFSVolSync *a_volSyncP)
{
    struct volume_handle *volh;
    struct mnode *n;
    struct msec m;
    int32_t len;
    int32_t *len_p;
    int ret;
    
    mlog_log (MDEBFS, "StoreData: fid: %d.%d.%d", 
	      a_fidToStoreP->Volume, a_fidToStoreP->Vnode,
	      a_fidToStoreP->Unique);
    mlog_log (MDEBFS, "StoreData: offset=%d, len=%d, total=%d",
	      a_offset, a_lenInBytes, a_fileLenInBytes);

#if 0
/* XXX what if offset > previous file length? */
    if(a_offset + a_lenInBytes > a_fileLenInBytes) {
	mlog_log (MDEBFS, "StoreData: ret = %d (wrong len)", EINVAL);
	mlog_log (MDEBFS, "StoreData: offset=%d, len=%d, total=%d",
		  a_offset, a_lenInBytes, a_fileLenInBytes);
	return EINVAL;
    }
#endif

    m.flags = VOLOP_WRITE | VOLOP_GETSTATUS;

    ret = fs_init_req (a_fidToStoreP, &m, &volh, call, &n);
    if (ret) {
	mlog_log (MDEBFS, "StoreData: ret = %d (fs_init_req)", ret);
	return ret;
    }
    
    if (n->fs.FileType == TYPE_DIR) {
	mnode_free (n, FALSE);
	mlog_log (MDEBFS, "StoreData: ret = %d", EPERM);
	return EPERM;
    }
    
    /*
     * newlen = max(offset+length, min(afilelength, oldlen))?
     */

    
    len = min(a_fileLenInBytes, n->fs.Length);

    if (a_offset + a_lenInBytes > len)
	len = a_offset + a_lenInBytes;

    if (len != n->fs.Length)
	len_p = &len;
    else
	len_p = NULL;

    ret = check_ss_bits (&m, a_fidStatusP, FALSE);
    if (ret)
	goto out;
    ret = vld_modify_vnode (volh, n, &m, a_fidStatusP, len_p);

    if (ret == 0 && a_lenInBytes != 0)
	ret = copyrx2fd (call, n->fd, a_offset, a_lenInBytes);

    if (ret == 0) {
	fs_update_fs (n, &m, a_fidStatP);
	vld_vld2volsync (volh, a_volSyncP);
    }

    ropa_break_callback (GETHOST(call), GETPORT(call), a_fidToStoreP, FALSE);
    
 out:
    mnode_free (n, FALSE);
    vld_free (volh);

    mlog_log (MDEBFS, "StoreData: ret = %d (at end), len = %d, calleraccess = %x",
	      ret, a_fidStatP->Length, a_fidStatP->CallerAccess);
    return ret;
}

static void
skipline(char **curptr)
{
    while(**curptr!='\n') (*curptr)++;
    (*curptr)++;
}

/*
 *
 */

int
SRXAFS_StoreACL(struct rx_call *call,
		const struct AFSFid *a_dirFidP,
		const AFSOpaque *a_ACLToStoreP,
		struct AFSFetchStatus *a_dirNewStatP,
		struct AFSVolSync *a_volSyncP)
{
    struct volume_handle *volh;
    struct mnode *n;
    struct msec m;
    int ret;
    namelist nlist;
    idlist ilist;
    char *curptr;
    struct acl_entry negacl[FS_MAX_ACL];
    struct acl_entry acl[FS_MAX_ACL];
    int num_posacl;
    int num_negacl;
    int i;
    
    mlog_log (MDEBFS, "StoreACL: fid: %d.%d.%d", 
	      a_dirFidP->Volume, a_dirFidP->Vnode,
	      a_dirFidP->Unique);

    m.flags = VOLOP_GETSTATUS | VOLOP_ADMIN;

    ret = fs_init_req (a_dirFidP, &m, &volh, call, &n);
    if (ret)
	return ret;

    assert (n->flags.ep);
    if (n->e.type != TYPE_DIR) {
	ret = EINVAL /* XXX */;
	goto err_out;
    }

#if 0
    fprintf(stderr, "%.*s",
	    a_ACLToStoreP->len,
	    a_ACLToStoreP->val);
#endif

    /* parse acl into nlist, acl */

    memset(acl, 0, sizeof(acl));
    memset(negacl, 0, sizeof(negacl));
    
    curptr = a_ACLToStoreP->val;
    curptr[a_ACLToStoreP->len - 1] = '\0';

    if (sscanf(curptr, "%d\n%d\n", &num_posacl, &num_negacl) != 2)
	goto err_out;
    skipline(&curptr);
    skipline(&curptr);
    ilist.len = 0;
    ilist.val = NULL;
    nlist.len = num_posacl + num_negacl;
    nlist.val = malloc(PR_MAXNAMELEN * nlist.len);
    for (i = 0; i < num_posacl; i++) {
	sscanf(curptr, "%63s %d", nlist.val[i], &acl[i].flags);
	skipline(&curptr);
    }
    for (i = 0; i < num_negacl; i++) {
	sscanf(curptr, "%63s %d", nlist.val[i+num_posacl], &negacl[i].flags);
	skipline(&curptr);
    }
    
    ret = fs_connsec_nametoid(&nlist, &ilist);
    if (ret)
	goto err_out;

    assert(nlist.len == ilist.len);

    for (i = 0; i < ilist.len; i++) {
	if (ilist.val[i] == PR_ANONYMOUSID) {
	    ret = ENOENT;
	    goto err_out;
	}
	fprintf(stderr, "%d\n", ilist.val[i]);
    }

    for (i = 0; i < num_posacl; i++)
	acl[i].owner = ilist.val[i];

    for (i = 0; i < num_negacl; i++)
	negacl[i].owner = ilist.val[i+num_posacl];

    memcpy(&n->e.u.dir.acl, acl, sizeof(acl));
    memcpy(&n->e.u.dir.negacl, negacl, sizeof(negacl));

    ret = vld_put_acl(volh, n, &m);
    if (ret)
	goto err_out;

    fs_update_fs (n, &m, a_dirNewStatP);
    ropa_break_callback (GETHOST(call), GETPORT(call), a_dirFidP, FALSE);

    vld_vld2volsync (volh, a_volSyncP);

 err_out:
    mnode_free (n, FALSE);
    vld_free (volh);
    return ret;
}

/*
 *
 */

int
SRXAFS_StoreStatus(struct rx_call *call,
		   const struct AFSFid *a_fidP,
		   const struct AFSStoreStatus *a_currStatusP,
		   struct AFSFetchStatus *a_srStatusP,
		   struct AFSVolSync *a_volSyncP)
{
    struct volume_handle *volh;
    struct mnode *n;
    struct msec m;
    int ret;
    
    mlog_log (MDEBFS, "StoreStatus: fid: %d.%d.%d", 
	      a_fidP->Volume, a_fidP->Vnode,
	      a_fidP->Unique);

    m.flags = VOLOP_GETSTATUS;

    ret = fs_init_req (a_fidP, &m, &volh, call, &n);
    if (ret)
	return ret;

    ret = check_ss_bits (&m, a_currStatusP, TRUE);
    if (ret)
	goto out;
    ret = vld_modify_vnode (volh, n, &m, a_currStatusP, NULL);

    if (ret == 0) {
	fs_update_fs (n, &m, a_srStatusP);
	vld_vld2volsync (volh, a_volSyncP);
    }
 
    ropa_break_callback (GETHOST(call), GETPORT(call), a_fidP, FALSE);
 out:    
    mnode_free (n, FALSE);
    vld_free (volh);

    return ret;
}


/*
 *
 */

static int
removenode (struct rx_call *call,
	    const struct AFSFid *a_dirFidP,
	    const char *a_name,
	    struct AFSFetchStatus *a_srvStatusP,
	    struct AFSVolSync *a_volSyncP,
	    int dirp)
{    
    struct volume_handle *volh;
    struct mnode *n;
    struct mnode *child_n;
    struct msec m, pm;
    AFSFid fid;
    int ret;
    int32_t new_len;
    unsigned long child_linkcount;

    m.flags = VOLOP_GETSTATUS|VOLOP_DELETE;

    ret = fs_init_req (a_dirFidP, &m, &volh, call, &n);
    if (ret)
	return ret;

    assert (n->flags.fdp);
    assert (n->flags.fsp);

    if (n->fs.FileType != TYPE_DIR) {
	mnode_free (n, FALSE);
	return ENOTDIR;
    }

    pm.flags		= VOLOP_GETSTATUS|VOLOP_READ|VOLOP_NOCHECK;
    fs_init_msec (call, &pm);
    pm.loop			= m.loop + 1;

    ret = mdir_lookup(n, a_name, &fid);
    if (ret) {
        mnode_free (n, FALSE);
        vld_free (volh);
        return ret;
    }

    if (afs_dir_p (fid.Vnode) != dirp) {
        mnode_free (n, FALSE);
        vld_free (volh);
        return dirp ? ENOTDIR : EISDIR;
    }

    ret = fs_open_node (&fid, volh, &pm, &child_n);
    if (ret) {
        mnode_free (n, FALSE);
        vld_free (volh);
        return ret;
    }

    if (dirp) {
	ret = mdir_emptyp (child_n);
	if (!ret) {
	    mnode_free (n, FALSE);
	    mnode_free (child_n, FALSE);
	    vld_free (volh);
	    return EEXIST;
	}
    }

    ret = mdir_remove(n, a_name);
    if (ret) {
	mnode_free (n, FALSE);
	mnode_free (child_n, FALSE);
	vld_free (volh);
	return ret;
    }

    /* removes node if necessary */
    ret = vld_adjust_linkcount (volh, child_n, dirp ? -2 : -1); 
    if (ret) {
	mnode_free (n, FALSE);
	mnode_free (child_n, FALSE);
	vld_free (volh);
	return ret;
    }

    child_linkcount = n->fs.LinkCount;

    mnode_free (child_n, FALSE);

    if (dirp) {
	ret = vld_adjust_linkcount (volh, n, -1);
	if (ret) {
	    mnode_free (n, FALSE);
	    vld_free (volh);
	    return ret;
	}
    }
    
    new_len = n->sb.st_size;
    ret = vld_modify_vnode (volh, n, &m, NULL, &new_len);

    fs_update_fs (n, &m, a_srvStatusP);

    fid.Volume = a_dirFidP->Volume;

    if (ret == 0) {
	if (child_linkcount)
	    ropa_break_callback (GETHOST(call), GETPORT(call), &fid, TRUE);
	else {
	    AFSCBFids cbfids;
	    AFSCBs cbs;
	    
	    cbfids.len = 1;
	    cbfids.val = &fid;
	    cbs.len = 0;

	    ropa_drop_callbacks(GETHOST(call), GETPORT(call), &cbfids, &cbs);
	}
	ropa_break_callback (GETHOST(call), GETPORT(call), a_dirFidP, FALSE);
    }

    mnode_free (n, FALSE);
    vld_vld2volsync (volh, a_volSyncP);
    vld_free (volh);

    return ret;
}

/*
 *
 */

int
SRXAFS_RemoveFile(struct rx_call *call,
		  const struct AFSFid *a_dirFidP,
		  const char *a_name,
		  struct AFSFetchStatus *a_srvStatusP,
		  struct AFSVolSync *a_volSyncP)
{
    mlog_log (MDEBFS, "RemoveFile: fid: %d.%d.%d name: %s", 
	      a_dirFidP->Volume, a_dirFidP->Vnode,
	      a_dirFidP->Unique, a_name);

    return removenode (call, a_dirFidP, a_name, a_srvStatusP, 
		       a_volSyncP, FALSE);
}

/*
 *
 */

int
createentry(struct rx_call *call,
	    const struct AFSFid *DirFid,
	    const char *Name,
	    const char *Contents,
	    const struct AFSStoreStatus *InStatus,
	    const struct AFSFid *ExistingFid,
	    struct AFSFid *EntryFid,
	    struct AFSFetchStatus *OutFidStatus,
	    struct AFSFetchStatus *OutDirStatus,
	    struct AFSCallBack *CallBack,
	    struct AFSVolSync *VolSync)
{
    struct volume_handle *volh;
    struct mnode *n;
    struct mnode *child_n;
    struct msec m;
    struct msec child_m;
    AFSFid child;
    int32_t len;
    int ret;

    m.flags = VOLOP_GETSTATUS|VOLOP_INSERT;

    ret = fs_init_req (DirFid, &m, &volh, call, &n);
    if (ret)
	return ret;

    assert (n->flags.fdp);
    assert (n->flags.fsp);
    
    if (n->fs.FileType != TYPE_DIR) {
	mnode_free (n, FALSE);
	vld_free (volh);
	return EPERM;
    }

    child.Volume = volh->vol;

    child_m.flags = VOLOP_GETSTATUS;
    fs_init_msec (call, &child_m);
    child_m.caller_access = m.caller_access;
    child_m.anonymous_access = m.anonymous_access;

    if (InStatus) {
	ret = check_ss_bits (&m, InStatus, TRUE);
	if (ret == 0) {
	    AFSFid existing;
	    ret = mdir_lookup (n, Name, &existing);
	    if (ret == 0)
		ret = EEXIST;
	    else if (ret == ENOENT)
		ret = 0;
	}
	
	if (ret == 0)
	    ret = vld_create_entry (volh, n, &child, 
				    Contents ? TYPE_LINK : TYPE_FILE,
				    InStatus, &child_n, &child_m);
	
	if (ret)
	    goto out_parent;
    } else {
	ret = fs_open_node (ExistingFid, volh, &child_m, &child_n);
	if (ret) {
	    mlog_log(MDEBFS, 
		     "createentry: Failed to open existing fid, ret = %d", ret);
	    goto out_parent;
	}
	
	if (child_n->fs.ParentVnode != DirFid->Vnode
	    || child_n->fs.ParentUnique != DirFid->Unique) {
	    ret = EXDEV;
	    mlog_log (MDEBFS, "createentry: ret = %d (EXDEV)", EXDEV);
	    goto out_child;
	}
	ret = vld_adjust_linkcount (volh, child_n, 1);
	if (ret)
	    goto out_child;

	child = *ExistingFid;
    }

    /* XXX check name ! */
    ret = mdir_creat (n, Name, child);

    if (ret == 0) {
	len = n->sb.st_size;
	ret = vld_modify_vnode (volh, n, &m, NULL, &len);
    }

    if (Contents && ret == 0) {
	assert (child_n->flags.fdp);
	
	len = strlen (Contents);
	ret = write (child_n->fd, Contents, len);
	if (ret != len) {
	    ret = errno;
	    mlog_log (MDEBFS, "createentry: ret = %d (write)", ret);
	} else {
	    ret = vld_modify_vnode (volh, child_n, &child_m, NULL, &len);
	}
    }

    if (ret) {
	mdir_remove (n, Name);
	vld_adjust_linkcount (volh, child_n, -1);
	mnode_free (n, TRUE);
	mnode_free (child_n, TRUE);
	vld_free (volh);
	return ret;
    }

    if (EntryFid)
	*EntryFid = child;

    fs_update_fs (child_n, &child_m, OutFidStatus);
    fs_update_fs (n, &m, OutDirStatus);

    ret = vld_info_uptodatep (volh);
    if (ret)
	goto out_child;

    if (CallBack)
	ropa_getcallback (GETHOST(call), GETPORT(call), EntryFid, CallBack,
			  volh->info.type);
    ropa_break_callback (GETHOST(call), GETPORT(call), DirFid, FALSE);

    vld_vld2volsync (volh, VolSync);
 out_child:
    mnode_free (child_n, FALSE);
 out_parent:
    mnode_free (n, FALSE);
    vld_free (volh);

    return ret;
}

/*
 *
 */

int
SRXAFS_CreateFile(struct rx_call *call,
		  const struct AFSFid *DirFid,
		  const char *Name,
		  const struct AFSStoreStatus *InStatus,
		  struct AFSFid *OutFid,
		  struct AFSFetchStatus *OutFidStatus,
		  struct AFSFetchStatus *OutDirStatus,
		  struct AFSCallBack *CallBack,
		  struct AFSVolSync *a_volSyncP)
{
    int ret;
    
    mlog_log (MDEBFS, "CreateFile: fid: %d.%d.%d name: %s", 
	      DirFid->Volume, DirFid->Vnode,
	      DirFid->Unique, Name);

    ret = createentry(call, DirFid, Name, NULL, InStatus, NULL, OutFid,
		      OutFidStatus, OutDirStatus, CallBack, a_volSyncP);

    if (ret)
	mlog_log (MDEBFS, "CreateFile: failed with ret = %d", ret);
    else
	mlog_log (MDEBFS, "CreateFile: created fid: %d.%d.%d calleraccess: %x", 
		  OutFid->Volume, OutFid->Vnode,
		  OutFid->Unique, OutFidStatus->CallerAccess);
    
    return ret;
}

/*
 *
 */

int
SRXAFS_Rename(struct rx_call *call,
	      const struct AFSFid *a_origDirFidP,
	      const char *a_origNameP,
	      const struct AFSFid *a_newDirFidP,
	      const char *a_newNameP,
	      struct AFSFetchStatus *a_origDirStatusP,
	      struct AFSFetchStatus *a_newDirStatusP,
	      struct AFSVolSync *a_volSyncP)
{
    struct volume_handle *volh;
    AFSFid victim, child;
    struct mnode *orig_n, *new_n = NULL;
    struct mnode *child_n = NULL, *victim_n = NULL;
    struct msec m1, m2;
    struct msec *orig_m = &m1, *new_m = &m2;
    int32_t len1, len2;
    int ret;
    int dirp;
    int same_dir = FALSE;
    int delete_dest = FALSE;
    int victim_deleted = FALSE;

    mlog_log (MDEBFS, "Rename: orig_fid: %d.%d.%d orig_name: %s "
	      "new_fid: %d.%d.%d new_name: %s", 
	      a_origDirFidP->Volume, a_origDirFidP->Vnode,
	      a_origDirFidP->Unique, a_origNameP,
	      a_newDirFidP->Volume, a_newDirFidP->Vnode,
	      a_newDirFidP->Unique, a_newNameP);

    if (a_origDirFidP->Volume != a_newDirFidP->Volume)
	return EXDEV;

    if (!(afs_dir_p(a_origDirFidP->Vnode)) 
	|| !(afs_dir_p(a_newDirFidP->Vnode)))
	return EPERM;

    if (a_origDirFidP->Vnode == a_newDirFidP->Vnode &&
	a_origDirFidP->Unique == a_newDirFidP->Unique) {
	same_dir = TRUE;
	orig_m->flags = VOLOP_GETSTATUS|VOLOP_INSERT|VOLOP_DELETE;
    } else {
	orig_m->flags = VOLOP_GETSTATUS|VOLOP_DELETE;
    }

    ret = fs_init_req (a_origDirFidP, orig_m, &volh, call, &orig_n);
    if (ret)
	return ret;

    assert (orig_n->flags.fdp);
    assert (orig_n->flags.fsp);

    ret = mdir_lookup(orig_n, a_origNameP, &child);
    if (ret == 0)
	ret = fs_open_node (&child, volh, orig_m, &child_n);
    if (ret) {
	mnode_free (orig_n, FALSE);
	return ret;
    }

    assert(child_n->flags.fsp);

    dirp = afs_dir_p(child.Vnode);
    if (same_dir == FALSE && child_n->fs.LinkCount != (dirp ? 2 : 1)) {
	ret = EXDEV;
	goto out1;
    }

    if (same_dir == TRUE) {
	new_n = orig_n;
	new_m = orig_m;
    } else {

	new_m->flags = VOLOP_GETSTATUS|VOLOP_INSERT|VOLOP_DELETE;
	
	/* XXX */
	ret = fs_init_msec(call, new_m);
	ret = fs_open_node (a_newDirFidP, volh, new_m, &new_n);
	if (ret) {
	    new_m->flags = VOLOP_GETSTATUS|VOLOP_INSERT;
	    
	    /* XXX */
	    ret = fs_open_node (a_newDirFidP, volh, new_m, &new_n);
	    if (ret) {
		mnode_free (orig_n, FALSE);
		vld_free (volh);
		return ret;
	    }
	}
    }
    
    ret = mdir_lookup(new_n, a_newNameP, &victim);
    if (!ret) {
	if (!(new_m->flags & VOLOP_DELETE)
	    || afs_dir_p(victim.Vnode) != dirp) {
	    ret = EPERM;
	    goto out1;
	}
	delete_dest = TRUE;
	ret = fs_open_node (&victim, volh, new_m, &victim_n);
	if (ret)
	    goto out1;
	
	assert(victim_n->flags.fsp);
	
	if (child_n->fs.LinkCount != (dirp ? 2 : 1)) {
	    ret = EPERM;
	    mnode_free(victim_n, FALSE);
	    goto out1;
	}
    }
    
    ret = mdir_rename(orig_n, a_origNameP, &len1,
		      new_n, a_newNameP, &len2);

    if (ret) {
	if (delete_dest == TRUE)
	    mnode_free(victim_n, FALSE);
	goto out1;
    }

    if (!ret && dirp) {
	ret = mdir_changefid(child_n, "..", *a_newDirFidP);
	if (ret) {
	    /* XXX recover */
	}
    }

    if (delete_dest == TRUE) {
	/* remove the node if necessary */
	ret = vld_adjust_linkcount (volh, victim_n, 
				    -(afs_dir_p(victim.Vnode) ? 2 : 1));
	if (victim_n->fs.LinkCount == 0)
	    victim_deleted = TRUE;

	mnode_free(victim_n, FALSE);
	if (ret) {
	    /* 
	     * XXX Remove failed, try to recover.
	     * Do not check for error, things are bad anyway.
	     * Maybe this should cause a shutdown + salvage?
	     */
	    goto out1;
	}
    }

    /* Update linkcount on parents if directory move */
    if (dirp && !same_dir) {
	ret = vld_adjust_linkcount (volh, orig_n, -1);
	ret = vld_adjust_linkcount (volh, new_n, 1);
    }

    /* XXX update st_ctime and st_mtime of both parents */

    vld_modify_vnode (volh, orig_n, orig_m, NULL, &len1);
    if (!same_dir)
        vld_modify_vnode (volh, new_n, new_m, NULL, &len2);

    fs_update_fs (orig_n, orig_m, a_origDirStatusP);
    fs_update_fs (new_n, new_m, a_newDirStatusP);

    ropa_break_callback (GETHOST(call), GETPORT(call), a_origDirFidP, FALSE);
    if (!same_dir) {
	ropa_break_callback (GETHOST(call),GETPORT(call), a_newDirFidP, FALSE);
	if (dirp)
	    ropa_break_callback (GETHOST(call), GETPORT(call), &child, FALSE);
    }

    if (victim_deleted == TRUE) {
	AFSCBFids cbfids;
	AFSCBs cbs;
	
	cbfids.len = 1;
	cbfids.val = &victim;
	cbs.len = 0;
	
	ropa_drop_callbacks(GETHOST(call), GETPORT(call), &cbfids, &cbs);
    } else
	ropa_break_callback(GETHOST(call), GETPORT(call), &victim, TRUE);
	    
 out1:
    mnode_free (orig_n, FALSE);
    if (child_n)
	mnode_free (child_n, FALSE);
    if (!same_dir && new_n)
	mnode_free (new_n, FALSE);

    if (ret == 0)
	vld_vld2volsync (volh, a_volSyncP);

    vld_free (volh);

    return ret;
}

/*
 *
 */

int
SRXAFS_Symlink(struct rx_call *call,
	       const struct AFSFid *a_dirFidP,
	       const char *a_nameP,
	       const char *a_linkContentsP,
	       const struct AFSStoreStatus *a_origDirStatP,
	       struct AFSFid *a_newFidP,
	       struct AFSFetchStatus *a_newFidStatP,
	       struct AFSFetchStatus *a_newDirStatP,
	       struct AFSVolSync *a_volSyncP)
{
    int ret;

    mlog_log (MDEBFS, "Symlink: fid: %d.%d.%d name: %s content: %s", 
	      a_dirFidP->Volume, a_dirFidP->Vnode,
	      a_dirFidP->Unique, a_nameP, a_linkContentsP);

    ret = createentry(call, a_dirFidP, a_nameP, a_linkContentsP,
		      a_origDirStatP, NULL, a_newFidP, a_newFidStatP, 
		      a_newDirStatP, NULL, a_volSyncP);

    if (ret)
	mlog_log (MDEBFS, "Symlink: failed with ret = %d", ret);
    else
	mlog_log (MDEBFS, "Symlink: created fid: %d.%d.%d calleraccess: %x", 
		  a_newFidP->Volume, a_newFidP->Vnode,
		  a_newFidP->Unique, a_newFidStatP->CallerAccess);

    return ret;
}

/*
 *
 */

int
SRXAFS_Link(struct rx_call *call,
	    const struct AFSFid *a_dirFidP,
	    const char *a_nameP,
	    const struct AFSFid *a_existingFidP,
	    struct AFSFetchStatus *a_newFidStatP,
	    struct AFSFetchStatus *a_newDirStatP,
	    struct AFSVolSync *a_volSyncP)
{
    int ret;

    mlog_log (MDEBFS, "Link: fid: %d.%d.%d name: %s existing", 
	      a_dirFidP->Volume, a_dirFidP->Vnode,
	      a_dirFidP->Unique, a_nameP);
    
    if (afs_dir_p(a_existingFidP->Vnode)) 
	return EISDIR;

    ret = createentry(call, a_dirFidP, a_nameP, NULL, NULL, a_existingFidP,
		      NULL, a_newFidStatP, a_newDirStatP, NULL, a_volSyncP);

    if (ret)
	mlog_log (MDEBFS, "Link: failed with ret = %d", ret);
    else
	mlog_log (MDEBFS, "Link: created name: %s calleraccess: %x", 
		  a_nameP, a_newFidStatP->CallerAccess);

    return ret;
}

/*
 *
 */

int
SRXAFS_MakeDir(struct rx_call *call,
	       const struct AFSFid *a_parentDirFidP,
	       const char *a_newDirNameP,
	       const struct AFSStoreStatus *a_currStatP,
	       struct AFSFid *a_newDirFidP,
	       struct AFSFetchStatus *a_dirFidStatP,
	       struct AFSFetchStatus *a_parentDirStatP,
	       struct AFSCallBack *a_newDirCallBackP,
	       struct AFSVolSync *a_volSyncP)
{
    struct volume_handle *volh;
    struct mnode *n;
    struct mnode *child_n;
    struct msec m;
    struct msec child_m;
    int ret;
    AFSFid child;
    
    mlog_log (MDEBFS, "MakeDir: fid: %d.%d.%d name: %s", 
	      a_parentDirFidP->Volume, a_parentDirFidP->Vnode,
	      a_parentDirFidP->Unique, a_newDirNameP);

    m.flags = VOLOP_WRITE | VOLOP_INSERT | VOLOP_GETSTATUS;

    ret = fs_init_req (a_parentDirFidP, &m, &volh, call, &n);
    if (ret)
	return ret;

    if (n->fs.FileType != TYPE_DIR) {
	mnode_free (n, FALSE);
	return EPERM;
    }

    child.Volume = volh->vol;
    
    ret = check_ss_bits (&m, a_currStatP, TRUE);
    if (ret)
	goto out_parent;

    child_m.flags = VOLOP_GETSTATUS;
    fs_init_msec(call, &child_m);
    ret = vld_create_entry (volh, n, &child, TYPE_DIR,
			    a_currStatP, &child_n, &child_m);
    if (ret)
	goto out_parent;

    ret = vld_adjust_linkcount (volh, n, 1);
    if (ret) {
	mnode_free (n, FALSE);
	mnode_free (child_n, FALSE);
	vld_free (volh);
	return ret;
    }

    /* XXX check name ! */
    ret = mdir_creat (n, a_newDirNameP, child);

    if (ret == 0) {
	int32_t len = n->sb.st_size;
	ret = vld_modify_vnode (volh, n, &m, NULL, &len);
    }

    if (ret) {
	/* XXX adjust directory size? */
	vld_adjust_linkcount (volh, n, -1);
	vld_adjust_linkcount (volh, child_n, -1); /* removes node if necessary */
	mnode_free (n, FALSE);
	mnode_free (child_n, TRUE);
	vld_free (volh);
	return ret;
    }

    *a_newDirFidP = child;
    fs_update_fs (n, &m, a_parentDirStatP);
    fs_update_fs (child_n, &m, a_dirFidStatP);
    vld_vld2volsync (volh, a_volSyncP);

    memcpy(&child_n->e.u.dir.acl, &n->e.u.dir.acl,
	   sizeof(n->e.u.dir.acl));
    memcpy(&child_n->e.u.dir.negacl, &n->e.u.dir.negacl,
	   sizeof(n->e.u.dir.negacl));

    ret = vld_put_acl(volh, child_n, &child_m);
    if (ret) {
	/* XXX adjust directory size? */
	vld_adjust_linkcount (volh, n, -1);
	vld_adjust_linkcount (volh, child_n, -1); /* removes node if necessary */
	mnode_free (n, FALSE);
	mnode_free (child_n, TRUE);
	vld_free (volh);
	return ret;
    }
    ropa_break_callback (GETHOST(call), GETPORT(call), a_parentDirFidP, FALSE);

    ret = vld_info_uptodatep (volh);
    if (ret)
	goto out_child;

    ropa_getcallback (GETHOST(call), GETPORT(call),
		      a_newDirFidP, a_newDirCallBackP,
		      volh->info.type);
    
    mlog_log (MDEBFS, "MakeDir: created child fid: %d.%d.%d", 
	      a_newDirFidP->Volume, a_newDirFidP->Vnode,
	      a_newDirFidP->Unique);

 out_child:
    mnode_free (child_n, FALSE);
 out_parent:
    mnode_free (n, FALSE);
    vld_free (volh);

    return ret;
}

/*
 *
 */

int
SRXAFS_RemoveDir(struct rx_call *call,
		 const struct AFSFid *a_parentDirP,
		 const char *a_dirNameP,
		 struct AFSFetchStatus *a_newParentDirStatP,
		 struct AFSVolSync *a_volSyncP)
{
    mlog_log (MDEBFS, "RemoveDir: fid: %d.%d.%d name: %s", 
	      a_parentDirP->Volume, a_parentDirP->Vnode,
	      a_parentDirP->Unique, a_dirNameP);

    return removenode (call, a_parentDirP, a_dirNameP, 
		       a_newParentDirStatP, a_volSyncP, TRUE);
}

/*
 *
 */

int
SRXAFS_GiveUpCallBacks(struct rx_call *call,
		       const AFSCBFids *a_fidArrayP,
		       const AFSCBs *a_callBackArrayP)
{
    int ret;

    mlog_log (MDEBFS, "GiveUpCallBacks");

    ret = ropa_drop_callbacks (GETHOST(call), GETPORT(call), 
			       a_fidArrayP, a_callBackArrayP);
    if (ret)
	mlog_log (MDEBFS, "GiveUpCallBacks: returning %d", ret);

    return ret;
}

/*
 *
 */

int
SRXAFS_GetVolumeStatus(struct rx_call *call,
		       const int32_t a_volIDP,
		       struct AFSFetchVolumeStatus *a_volFetchStatP,
		       char *a_volNameP,
		       char *a_offLineMsgP,
		       char *a_motdP)
{
    struct volume_handle *volh;
    AFSFid fid;
    struct msec m;
    int ret;

    mlog_log (MDEBFS, "GetVolumeStats: vol: %d", a_volIDP);

    m.flags = VOLOP_GETSTATUS;
    fid.Volume = a_volIDP;

    ret = fs_init_req (&fid, &m, &volh, call, NULL);
    if (ret) {
	mlog_log (MDEBFS, "GetVolumeStatus: fs_init_req returned %d", ret);
	return ret;
    }

    ret = vld_get_volstats (volh, a_volFetchStatP, a_volNameP,
			    a_offLineMsgP, a_motdP);
    return ret;
}

/*
 *
 */

int
SRXAFS_SetVolumeStatus(struct rx_call *call,
		       const int32_t a_volIDP,
		       const struct AFSStoreVolumeStatus *a_volStoreStatP,
		       const char *a_volNameP,
		       const char *a_offLineMsgP,
		       const char *a_motdP)
{
    struct volume_handle *volh;
    AFSFid fid;
    struct msec m;
    int ret;

    mlog_log (MDEBFS, "SRXAFS_SetVolumeStatus: vol: %d", a_volIDP);

    m.flags = 0;
    fid.Volume = a_volIDP;
    fid.Vnode = 1;
    fid.Unique = 1;

    ret = fs_init_req (&fid, &m, &volh, call, NULL);
    if (ret) {
	goto out;
    }

    if (!super_user(&m)) {
	ret = EPERM;
	goto out;
    }

    ret = vld_set_volstats (volh, a_volStoreStatP, a_volNameP,
			    a_offLineMsgP, a_motdP);

 out:
    vld_free (volh);
    mlog_log (MDEBFS, "SRXAFS_SetVolumeStatus: fs_init_req returned %d", ret);
    return ret;
}

/*
 *
 */

int
SRXAFS_GetRootVolume(struct rx_call *call,
		     char *a_rootVolNameP)
{
    mlog_log (MDEBFS, "GetRootVolume");

    strlcpy (a_rootVolNameP, "root.cell", AFSNAMEMAX);
    a_rootVolNameP[AFSNAMEMAX-1] = '\0';
    return 0;
}

/*
 * Get time, the poor mans ntp, used as probe by some clients
 */

int
SRXAFS_GetTime(struct rx_call *call,
	       uint32_t *a_secondsP,
	       uint32_t *a_uSecondsP)
{
    struct timeval tv;

    mlog_log (MDEBFS, "GetTime");

    gettimeofday (&tv, NULL);

    *a_secondsP = tv.tv_sec;
    *a_uSecondsP = tv.tv_usec;

    return 0;
}

/*
 *
 */

int
SRXAFS_NGetVolumeInfo(struct rx_call *call,
		      const char *VolumeName,
		      struct AFSVolumeInfo *stuff)
{
    return EPERM;
}

/*
 *
 */

int
SRXAFS_BulkStatus(struct rx_call *call,
		  const AFSCBFids *FidsArray,
		  AFSBulkStats *StatArray,
		  AFSCBs *CBArray,
		  struct AFSVolSync *Sync)
{
    struct volume_handle *volh = NULL;
    struct mnode *n;
    struct msec m;
    int ret, i = 0;
    int32_t oldvolume = -1;

    mlog_log (MDEBFS, "BulkStatus");

    CBArray->val = NULL;
    CBArray->len = 0;
    StatArray->val = NULL;
    StatArray->len = 0;

    if (FidsArray->len == 0)
	return 0;

    m.flags = VOLOP_GETSTATUS;

    ret = fs_init_msec (call, &m);
    if (ret)
	return ret;

    StatArray->len = FidsArray->len;
    StatArray->val = malloc(StatArray->len * sizeof(StatArray->val[0]));
    if (StatArray->val == NULL)
	return ENOMEM;

    CBArray->len   = FidsArray->len;
    CBArray->val   = malloc(CBArray->len * sizeof(CBArray->val[0]));
    if(CBArray->val == NULL) {
	free(StatArray->val);
	return ENOMEM;
    }

    for (i = 0; FidsArray->len > i ; i++) {
	
	if (FidsArray->val[i].Volume != oldvolume) {
	    if (volh)
		vld_free (volh);
	    
	    ret = vld_find_vol (FidsArray->val[i].Volume, &volh);
	    if (ret)
		return ret;

	    if (volh->flags.offlinep) {
		vld_free (volh);
		return VOFFLINE;
	    }
	    
	    ret = vld_db_uptodate (volh);
	    if (ret) {
		vld_free (volh);
		return ret;
	    }
	    oldvolume = FidsArray->val[i].Volume;
	}
	
	ret = fs_open_node (&FidsArray->val[i], volh, &m, &n);
	if (ret) {
	    vld_free (volh);
	    return ret;
	}
	
	fs_update_fs (n, &m, &StatArray->val[i]);
    
	mnode_free (n, FALSE);
	n = NULL;
	
	ret = vld_info_uptodatep (volh);
	if (ret)
	    goto out;

	ropa_getcallback (GETHOST(call), GETPORT(call), 
			  &FidsArray->val[i], &CBArray->val[i],
			  volh->info.type);
    }

 out:
    vld_free (volh);

    return 0;
}

/*
 *
 */

int
SRXAFS_SetLock(struct rx_call *call,
	       const struct AFSFid *Fid,
	       const ViceLockType Type,
	       struct AFSVolSync *Sync)
{
    return EPERM;
}

/*
 *
 */

int
SRXAFS_ExtendLock(struct rx_call *call,
		  const struct AFSFid *Fid,
		  struct AFSVolSync *Sync)
{
    return EPERM;
}


/*
 *
 */

int
SRXAFS_ReleaseLock(struct rx_call *call,
		   const struct AFSFid *Fid,
		   struct AFSVolSync *Sync)
{
    return EPERM;
}
