/*
 * Copyright (c) 1999 - 2002 Kungliga Tekniska Högskolan
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

RCSID("$arla: volprocs.c,v 1.19 2002/09/08 04:56:15 ahltorp Exp $");

/*
 * exit debug logging
 */

#define VOLSER_EXIT  mlog_log(MDEBVOLDB, __FUNCTION__ " error: %s(%d)", koerr_gettext(ret), ret)

/*
 * Helper function
 */

static int
volser_fetch_vh (int32_t partition, int32_t volid, 
		 struct dp_part **dp, volume_handle **vh)
{
    int ret;

    ret = dp_create (partition, dp);
    if (ret) {
	fprintf (stderr, "volser_fetch_vh: dp_create: %d\n", ret);
	return VOLSERILLEGAL_PARTITION;
    }

    ret = vld_open_volume_by_num (*dp, volid, vh);
    if (ret) {
	fprintf (stderr, "volser_fetch_vh: vld_open_volume_by_num: %d\n", ret);
	dp_free (*dp);
	if (ret == ENOENT)
	    return VNOVOL;
	return ret;
    }
    
    ret = vld_info_uptodatep (*vh);
    if (ret) {
	printf ("volser_fetch_vh: vld_info_uptodatep: %d\n", ret);
	vld_free (*vh);
	dp_free (*dp);
	return VOLSERFAILEDOP; /* XXX */
    }
    return 0;
}

/*
 *
 */

int
VOLSER_AFSVolCreateVolume(struct rx_call *call,
			  const int32_t partition,
			  const char *name,
			  const int32_t type,
			  const int32_t parent,
			  int32_t *volid,
			  int32_t *trans)
{
    int ret = 0;
    int32_t backstoretype = VLD_SVOL;
    struct dp_part *dp;

    mlog_log(MDEBVOLDB, "VOLSER_AFSVolCreateVolume"
	     " part %d name %s type %d parent %d volid %u",
	     partition, name, type, parent, *volid);

    if (!sec_is_superuser(call)) {
	ret = VOLSERBAD_ACCESS;
	goto out;
    }

    /* XXX parent should be used */

    ret = vld_create_trans (partition, *volid, trans);

    if (ret)
	goto out;

    ret = vld_trans_set_iflags (*trans, ITCreate);
    if (ret)
	goto out;

    ret = dp_create (partition, &dp);
    if (ret) {
	vld_end_trans (*trans, NULL);
	goto out;
    }

    ret = vld_create_volume (dp, *volid, name, backstoretype, type, 0);
    if (ret) {
	vld_end_trans (*trans, NULL);
	goto out;
    }

 out:
    VOLSER_EXIT;
    
    return ret;
}

/*
 *
 */

int
VOLSER_AFSVolDeleteVolume(struct rx_call *call,
			  const int32_t transid)
{
    int ret = 0;
    int32_t backstoretype = VLD_SVOL;
    struct trans *trans;
    struct dp_part *dp;

    mlog_log(MDEBVOLDB, "VOLSER_AFSVolDeleteVolume");

    if (!sec_is_superuser(call)) {
	ret = VOLSERBAD_ACCESS;
	goto out;
    }

    ret = vld_verify_trans(transid);
    if (ret)
	goto out;

    ret = vld_get_trans(transid, &trans);
    if (ret)
	goto out;

    ret = dp_create (trans->partition, &dp);
    if (ret) {
	vld_put_trans (trans);
	goto out;
    }

    ret = vld_delete_volume (dp, trans->volid, backstoretype, 0);

    vld_put_trans(trans);

 out:
    VOLSER_EXIT;
    
    return ret;
}

/*
 *
 */

int
VOLSER_AFSVolNukeVolume(struct rx_call *call,
			const int32_t partID,
			const int32_t volID)
{
    int ret = 0;

    mlog_log(MDEBVOLDB, "VOLSER_AFSVolNukeVolume");

    if (!sec_is_superuser(call)) {
	ret = VOLSERBAD_ACCESS;
	goto out;
    }

    ret = VOLSERFAILEDOP;

 out:
    VOLSER_EXIT;
    
    return ret;
}

/*
 *
 */

int
VOLSER_AFSVolDump(struct rx_call *call,
		  const int32_t fromTrans,
		  const int32_t fromDate)
{
    int ret = 0;
    struct trans *trans;
    struct dp_part *dp;
    volume_handle *vh;

    mlog_log (MDEBVOLDB, 
	      "VOLSER_AFSVolDump: trans %d fromdate %d", 
	      fromTrans, fromDate);
    
    if (!sec_is_superuser(call)) {
	ret = VOLSERBAD_ACCESS;
	goto out;
    }

    ret = vld_verify_trans (fromTrans);
    if (ret)
	goto out;

    ret = vld_get_trans (fromTrans, &trans);
    if (ret)
	goto out;
    
    ret = volser_fetch_vh (trans->partition, trans->volid, &dp, &vh);
    if (ret) {
	vld_put_trans(trans);
	goto out;
    }

    if (fromDate != 0) {
	vld_put_trans(trans);
	ret = VOLSERFAILEDOP;
	goto out;
    }

    ret = generate_dump(call, vh);

    vld_put_trans (trans);

 out:
    VOLSER_EXIT;
    
    return ret;
}

/*
 *
 */

int
VOLSER_AFSVolSignalRestore(struct rx_call *call,
			   const char *name,
			   const int32_t type,
			   const int32_t pid,
			   const int32_t cloneid)
{
    int ret = 0;

    mlog_log(MDEBVOLDB, "VOLSER_AFSVolSignalRestore");

    if (!sec_is_superuser(call)) {
	ret = VOLSERBAD_ACCESS;
	goto out;
    }

    ret = VOLSERFAILEDOP;

 out:
    VOLSER_EXIT;
    
    return ret;
}

/*
 *
 */

int
VOLSER_AFSVolRestore(struct rx_call *call,
		     const int32_t transid,
		     const int32_t flags,
		     const struct restoreCookie *cookie)
{
    int ret = 0;
    struct trans *trans;
    struct dp_part *dp;
    volume_handle *vh;

    mlog_log(MDEBVOLDB, "VOLSER_AFSVolRestore");

    if (!sec_is_superuser(call)) {
	ret = VOLSERBAD_ACCESS;
	goto out;
    }

    ret = vld_verify_trans(transid);
    if (ret)
	goto out;

    ret = vld_get_trans(transid, &trans);
    if (ret)
	goto out;
    
    ret = volser_fetch_vh (trans->partition, trans->volid, &dp, &vh);
    if (ret) {
	vld_put_trans(trans);
	goto out;
    }

    ret = parse_dump(call, vh);
    if (ret) {
	vld_free (vh);
	dp_free (dp);
	vld_put_trans(trans);
	goto out;
    }

    ret = vld_rebuild(vh);

    vld_info_write(vh);

    vld_free (vh);
    dp_free (dp);

    vld_put_trans(trans);

 out:
    VOLSER_EXIT;
    
    return ret;
}

/*
 *
 */

int
VOLSER_AFSVolForward(struct rx_call *call,
		     const int32_t fromTrans,
		     const int32_t fromData,
		     const struct destServer *destination,
		     const int32_t destTrans,
		     const struct restoreCookie *cookie)
{
    int ret = 0;

    mlog_log(MDEBVOLDB, "VOLSER_AFSVolForward");

    if (!sec_is_superuser(call)) {
	ret = VOLSERBAD_ACCESS;
	goto out;
    }

    ret = VOLSERFAILEDOP;

 out:
    VOLSER_EXIT;
    
    return ret;
}

/*
 *
 */

int
VOLSER_AFSVolClone(struct rx_call *call,
		   const int32_t trans,
		   const int32_t purgeVol,
		   const int32_t newType,
		   const char *newName,
		   int32_t *newVol)
{
    int ret = 0;

    mlog_log(MDEBVOLDB, "VOLSER_AFSVolClone"
	     " trans %d purgevol %d newtype %d newname %d",
	     trans, purgeVol, newType, newName);

    if (!sec_is_superuser(call)) {
	ret = VOLSERBAD_ACCESS;
	goto out;
    }

    ret = VOLSERFAILEDOP;

 out:
    VOLSER_EXIT;
    
    return ret;
}

/*
 *
 */

int
VOLSER_AFSVolReClone(struct rx_call *call,
		     const int32_t tid,
		     const int32_t cloneID)
{
    int ret = 0;

    mlog_log(MDEBVOLDB, "VOLSER_AFSVolReClone");

    if (!sec_is_superuser(call)) {
	ret = VOLSERBAD_ACCESS;
	goto out;
    }

    ret = VOLSERFAILEDOP;

 out:
    VOLSER_EXIT;
    
    return ret;
}

/*
 *
 */

int
VOLSER_AFSVolSetForwarding(struct rx_call *call,
			   const int32_t tid,
			   const int32_t newsite)
{
    int ret = 0;

    mlog_log(MDEBVOLDB, "VOLSER_AFSVolSetForwarding");

    if (!sec_is_superuser(call)) {
	ret = VOLSERBAD_ACCESS;
	goto out;
    }

    ret = VOLSERFAILEDOP;

 out:
    VOLSER_EXIT;
    
    return ret;
}

/*
 *
 */

int
VOLSER_AFSVolTransCreate(struct rx_call *call,
			 const int32_t volume,
			 const int32_t partition,
			 const int32_t flags,
			 int32_t *trans)
{
    int ret = 0;
    struct dp_part *dp;
    volume_handle *vh;

    mlog_log(MDEBVOLDB, "VOLSER_AFSVolTransCreate");

    if (!sec_is_superuser(call)) {
	ret = VOLSERBAD_ACCESS;
	goto out;
    }

    ret = volser_fetch_vh (partition, volume, &dp, &vh);
    if (ret)
	goto out;

    vld_free (vh);
    dp_free (dp);

    ret = vld_create_trans(partition, volume, trans);
    if (ret)
	goto out;

    ret = vld_trans_set_iflags(*trans, flags);
    if (ret) {
	vld_end_trans (*trans, NULL);
	goto out;
    }

 out:
    VOLSER_EXIT;
    
    return ret;
}

/*
 *
 */

int
VOLSER_AFSVolEndTrans(struct rx_call *call,
		      const int32_t transid,
		      int32_t *rcode)
{
    struct trans *trans;
    int ret = 0;

    mlog_log(MDEBVOLDB, "VOLSER_AFSVolEndTrans trans %d", transid);

    if (!sec_is_superuser(call)) {
	ret = VOLSERBAD_ACCESS;
	goto out;
    }

    ret = vld_verify_trans(transid);
    if (ret) 
	goto out;

    ret = vld_get_trans(transid, &trans);
    if (ret)
	goto out;

    ropa_break_volume_callback(trans->volid); /* XXX */

    vld_put_trans(trans);

    ret = vld_end_trans(transid, rcode);
    if (ret)
	goto out;

 out:
    mlog_log(MDEBVOLDB, "VOLSER_AFSVolEndTrans returns %d", ret);

    return ret;
}

/*
 *
 */

int
VOLSER_AFSVolGetFlags(struct rx_call *call,
		      const int32_t trans,
		      int32_t *flags)
{
    int ret = 0;

    mlog_log(MDEBVOLDB, "VOLSER_AFSVolGetFlags");

    if (!sec_is_superuser(call)) {
	ret = VOLSERBAD_ACCESS;
	goto out;
    }

    ret = vld_verify_trans(trans);
    if (ret)
	goto out;

    ret = vld_trans_get_vflags(trans, flags);
    if (ret)
	goto out;

 out:
    VOLSER_EXIT;
    
    return ret;
}

/*
 *
 */

int
VOLSER_AFSVolSetFlags(struct rx_call *call,
		      const int32_t transid,
		      const int32_t flags)
{
    int ret = 0;
    struct trans *trans;
    struct dp_part *dp;
    volume_handle *vh;

    mlog_log(MDEBVOLDB, "VOLSER_AFSVolSetFlags"
	     " trans %d flags %d", transid, flags);

    if (!sec_is_superuser(call)) {
	ret = VOLSERBAD_ACCESS;
	goto out;
    }

    ret = vld_verify_trans(transid);
    if (ret)
	goto out;

    ret = vld_get_trans(transid, &trans);
    if (ret)
	goto out;
    
    ret = volser_fetch_vh (trans->partition, trans->volid, &dp, &vh);
    if (ret) {
	vld_put_trans(trans);
	goto out;
    }

    if (flags & VTDeleteOnSalvage)
	vh->info.destroyMe = 's';
    else
	vh->info.destroyMe = 0;

#if 0
    assert ((flags & VTOutOfService) == 0); /* XXX */
#endif

    vld_info_write(vh);

    vld_free (vh);
    dp_free (dp);

    vld_put_trans(trans);

    ret = vld_trans_set_vflags(transid, flags);
    if (ret)
	goto out;

 out:
    VOLSER_EXIT;
    
    return ret;
}

/*
 *
 */

int
VOLSER_AFSVolGetName(struct rx_call *call,
		     const int32_t tid,
		     char tname[256])
{
    int ret = 0;

    mlog_log(MDEBVOLDB, "VOLSER_AFSVolGetName");

    if (!sec_is_superuser(call)) {
	ret = VOLSERBAD_ACCESS;
	goto out;
    }

    ret = VOLSERFAILEDOP;

 out:
    VOLSER_EXIT;
    
    return ret;
}

/*
 *
 */

int
VOLSER_AFSVolGetStatus(struct rx_call *call,
		       const int32_t transid,
		       struct volser_status *status)
{
    int ret = 0;
    struct trans *trans;
    struct dp_part *dp;
    volume_handle *vh;

    mlog_log(MDEBVOLDB, "VOLSER_AFSVolGetStatus");

    if (!sec_is_superuser(call)) {
	ret = VOLSERBAD_ACCESS;
	goto out;
    }

    ret = vld_verify_trans(transid);
    if (ret)
	goto out;

    ret = vld_get_trans(transid, &trans);
    if (ret)
	goto out;
    
    ret = volser_fetch_vh (trans->partition, trans->volid, &dp, &vh);
    if (ret) {
	vld_put_trans(trans);
	goto out;
    }

    status->volID = vh->info.volid;
    status->nextUnique = 0 /* XXX */;
    status->type = vh->info.type;
    status->parentID = vh->info.parentID;
    status->cloneID = vh->info.cloneID;
    status->backupID = vh->info.backupID;
    status->restoredFromID = 0 /* XXX */;
    status->maxQuota = vh->info.maxquota;
    status->minQuota = 0 /* XXX */;
    status->owner = 0 /* XXX */;
    status->creationDate = vh->info.creationDate;
    status->accessDate = vh->info.accessDate;
    status->updateDate = vh->info.updateDate;
    status->exprirationDate = 0 /* XXX */;
    status->backupDate = vh->info.backupDate;
    status->copyDate = 0 /* XXX */;

    vld_free (vh);
    dp_free (dp);

    vld_put_trans(trans);

 out:
    VOLSER_EXIT;
    
    return ret;
}

/*
 *
 */

int
VOLSER_AFSVolSetIdsTypes(struct rx_call *call,
			 const int32_t transid,
			 const char *name,
			 const int32_t type,
			 const int32_t parentID,
			 const int32_t cloneID,
			 const int32_t backupID)
{
    int ret = 0;
    struct trans *trans;
    struct dp_part *dp;
    volume_handle *vh;

    mlog_log(MDEBVOLDB, "VOLSER_AFSVolSetIdsTypes: type %d parentID %d "
	     "cloneID %d backupID %d", type, parentID, cloneID, backupID);

    if (!sec_is_superuser(call)) {
	ret = VOLSERBAD_ACCESS;
	goto out;
    }

    ret = vld_verify_trans(transid);
    if (ret)
	goto out;

    ret = vld_get_trans(transid, &trans);
    if (ret)
	goto out;
    
    ret = volser_fetch_vh (trans->partition, trans->volid, &dp, &vh);
    if (ret) {
	vld_put_trans(trans);
	goto out;
    }

    strlcpy(vh->info.name, name, VNAMESIZE);
    vh->info.type = type;
    vh->info.parentID = parentID;
    vh->info.cloneID = cloneID;
    vh->info.backupID = backupID;

    vld_info_write(vh);

    vld_free (vh);
    dp_free (dp);

    vld_put_trans(trans);

 out:
    VOLSER_EXIT;
    
    return ret;
}

/*
 *
 */

int
VOLSER_AFSVolSetDate(struct rx_call *call,
		     const int32_t transid,
		     const int32_t newDate)
{
    int ret = 0;
    struct trans *trans;
    struct dp_part *dp;
    volume_handle *vh;

    mlog_log(MDEBVOLDB, "VOLSER_AFSVolSetDate");

    if (!sec_is_superuser(call)) {
	ret = VOLSERBAD_ACCESS;
	goto out;
    }

    ret = vld_verify_trans(transid);
    if (ret)
	goto out;

    ret = vld_get_trans(transid, &trans);
    if (ret)
	goto out;
    
    ret = volser_fetch_vh (trans->partition, trans->volid, &dp, &vh);
    if (ret) {
	vld_put_trans(trans);
	goto out;
    }

    vh->info.creationDate = newDate;

    vld_info_write(vh);

    vld_free (vh);
    dp_free (dp);

    vld_put_trans(trans);

 out:
    VOLSER_EXIT;
    
    return ret;
}

/*
 *
 */

int
VOLSER_AFSVolListPartitions(struct rx_call *call,
			    struct pIDs *partIDs)
{
    int i;
    struct dp_part *dp = NULL;
    int ret = 0;

    mlog_log(MDEBVOLDB, "VOLSER_AFSVolListPartitions");

    i = 0;
    do {
	ret = dp_find(&dp);
	if (dp == NULL)
	    break;
	if (ret)
	    goto out;
	partIDs->partIds[i] = dp->num;
	i++;
    } while (i < 26);

    for (; i < 26; i++)
	partIDs->partIds[i] = -1;
    
    ret = 0;
 out:
    VOLSER_EXIT;
    
    return ret;
}

/*
 *
 */

int
VOLSER_AFSVolPartitionInfo(struct rx_call *call,
			   const char *name,
			   struct diskPartition *partition)
{
    int num;
    struct dp_part *dp;
    int ret = 0;

    mlog_log(MDEBVOLDB, "VOLSER_AFSVolPartitionInfo");

    num = partition_name2num (name);
    if (num == -1) {
	ret = VOLSERILLEGAL_PARTITION;
	goto out;
    }
    
    ret = dp_create (num, &dp);
    if (ret)
	goto out;

    memset(partition, 0, sizeof(*partition));
    strlcpy(partition->name, dp->part, 32);
    partition->free = 1000;
    
    dp_free(dp);

 out:
    VOLSER_EXIT;
    
    return ret;
}


static void
copy_volumeinfo(struct volintInfo *volinfo,
		volume_handle *vh,
		const int32_t partID)
{
    strlcpy(volinfo->name, vh->info.name, VNAMESIZE);
    volinfo->volid = vh->info.volid;
    volinfo->type = vh->info.type;
    volinfo->backupID = vh->info.backupID;
    volinfo->parentID = vh->info.parentID;
    volinfo->cloneID = vh->info.cloneID;
    if (vld_check_busy(volinfo->volid, partID))
	volinfo->status = VBUSY;
    else
	volinfo->status = VOK;
    volinfo->copyDate = vh->info.copyDate;
    volinfo->inUse = vh->info.inUse;
    volinfo->creationDate = vh->info.creationDate;
    volinfo->accessDate = vh->info.accessDate;
    volinfo->updateDate = vh->info.updateDate;
    volinfo->backupDate = vh->info.backupDate;
    volinfo->dayUse = vh->info.dayUse;
    volinfo->filecount = vh->info.filecount;
    volinfo->maxquota = vh->info.maxquota;
    volinfo->size = vh->info.size;
    volinfo->needsSalvaged = 0;
    volinfo->destroyMe = 0;
}

/*
 *
 */

int
VOLSER_AFSVolListVolumes(struct rx_call *call,
			 const int32_t partID,
			 const int32_t flags,
			 volEntries *resultEntries)
{
    int ret = 0;
    List *vollist;
    Listitem *item;
    volume_handle *vh;
    struct dp_part *dp;
    int numvol;
    int i;

    mlog_log(MDEBVOLDB, "VOLSER_AFSVolListVolumes");

    ret = dp_create (partID, &dp);
    if (ret)
	goto out;

    ret = vld_list_volumes(dp, &vollist);
    if (ret)
	goto free_part;

    numvol = 0;

    item = listhead(vollist);
    while (item) {
	numvol++;
	item = listnext(vollist, item);
    }

    resultEntries->len = numvol;
    
    resultEntries->val = calloc(sizeof(struct volintInfo) * resultEntries->len, 1);

    i = 0;
    while (!listemptyp(vollist)) {
	vh = (volume_handle *) listdelhead(vollist);
	assert(vh);
	ret = vld_info_uptodatep (vh);
	assert(ret == 0);
	copy_volumeinfo(&resultEntries->val[i], vh, partID);
	vld_free (vh);
	i++;
    }

    free(vollist);

 free_part:
    dp_free (dp);

 out:
    VOLSER_EXIT;
    
    return ret;
}

/*
 *
 */

int
VOLSER_AFSVolListOneVolume(struct rx_call *call,
			   const int32_t partID,
			   const int32_t volid,
			   volEntries *resultEntries)
{
    volume_handle *vh;
    int ret = 0;
    struct dp_part *dp;
    struct volintInfo *volinfo;

    mlog_log(MDEBVOLDB, "VOLSER_AFSVolListOneVolume partid: %d volid: %u\n",
	     partID, volid);

    ret = volser_fetch_vh (partID, volid, &dp, &vh);
    if (ret)
	goto out;

    resultEntries->len = 1;
    
    volinfo = calloc(sizeof(struct volintInfo) * resultEntries->len, 1);
    resultEntries->val = volinfo;
    copy_volumeinfo(volinfo, vh, partID);

    vld_free (vh);
    dp_free (dp);

 out:
    VOLSER_EXIT;
    
    return ret;
}

/*
 *
 */

int
VOLSER_AFSVolGetNthVolume(struct rx_call *call,
			  const int32_t index,
			  int32_t *volume,
			  int32_t *partition)
{
    int ret = 0;

    mlog_log(MDEBVOLDB, "VOLSER_AFSVolGetNthVolume");

    ret = VOLSERFAILEDOP;

 out:
    VOLSER_EXIT;
    
    return ret;
}

/*
 *
 */

int
VOLSER_AFSVolMonitor(struct rx_call *call,
		     transDebugEntries *result)
{
    int ret = 0;

    mlog_log(MDEBVOLDB, "VOLSER_AFSVolMonitor");

    ret = VOLSERFAILEDOP;

 out:
    VOLSER_EXIT;
    
    return ret;
}

/*
 *
 */

int
VOLSER_AFSVolXListVolumes(struct rx_call *call,
			  const int32_t partID,
			  const int32_t flags,
			  xvolEntries *resultEntries)
{
    int ret = 0;

    mlog_log(MDEBVOLDB, "VOLSER_AFSVolXListVolumes");

    ret = VOLSERFAILEDOP;

 out:
    VOLSER_EXIT;
    
    return ret;
}

/*
 *
 */

int
VOLSER_AFSVolXListOneVolume(struct rx_call *call,
			    const int32_t partID,
			    const int32_t volid,
			    xvolEntries *resultEntries)
{
    volume_handle *vh;
    struct dp_part *dp;
    struct xvolintInfo *volinfo;
    int ret = 0;

    mlog_log(MDEBVOLDB, "VOLSER_AFSVolXListOneVolume partid: %d volid: %u\n",
	     partID, volid);

    ret = volser_fetch_vh (partID, volid, &dp, &vh);
    if (ret)
	return ret;

    resultEntries->len = 1;
    
    volinfo = calloc(sizeof(struct xvolintInfo) * resultEntries->len, 1);
    resultEntries->val = volinfo;
    strlcpy(volinfo->name, vh->info.name, VNAMESIZE);
    volinfo->volid = vh->info.volid;
    volinfo->type = vh->info.type;
    volinfo->backupID = vh->info.backupID;
    volinfo->parentID = vh->info.parentID;
    volinfo->cloneID = vh->info.cloneID;
    if (vld_check_busy(volid, partID))
	volinfo->status = VBUSY;
    else
	volinfo->status = VOK;
    volinfo->copyDate = vh->info.copyDate;
    volinfo->inUse = vh->info.inUse;
    volinfo->creationDate = vh->info.creationDate;
    volinfo->accessDate = vh->info.accessDate;
    volinfo->updateDate = vh->info.updateDate;
    volinfo->backupDate = vh->info.backupDate;
    volinfo->dayUse = vh->info.dayUse;
    volinfo->filecount = vh->info.filecount;
    volinfo->maxquota = vh->info.maxquota;
    volinfo->size = vh->info.size;

    vld_free (vh);
    dp_free (dp);

 out:
    VOLSER_EXIT;
    
    return ret;
}

/*
 *
 */

int
VOLSER_AFSVolSetInfo(struct rx_call *call,
		     const int32_t transid,
		     const struct volintInfo *volinfo)
{
    volume_handle *vh;
    int ret = 0;
    struct dp_part *dp;
    struct trans *trans;

    mlog_log(MDEBVOLDB, "VOLSER_AFSVolSetInfo"
	     " trans %d name %s type %d parent %d volid %d backup %d",
	     transid, volinfo->name, volinfo->type, volinfo->parentID,
	     volinfo->volid, volinfo->backupID);

    if (!sec_is_superuser(call)) {
	ret = VOLSERBAD_ACCESS;
	goto out;
    }

    ret = vld_verify_trans(transid);
    if (ret)
	goto out;

    ret = vld_get_trans(transid, &trans);
    if (ret)
	goto out;
    
    ret = volser_fetch_vh (trans->partition, trans->volid, &dp, &vh);
    if (ret) {
	vld_put_trans(trans);
	goto out;
    }

    if (volinfo->name[0])
	strlcpy(vh->info.name, volinfo->name, VNAMESIZE);

    if (volinfo->volid)
	vh->info.volid = volinfo->volid;
    if (volinfo->type)
	vh->info.type = volinfo->type;
    if (volinfo->backupID)
	vh->info.backupID = volinfo->backupID;
    if (volinfo->parentID)
	vh->info.parentID = volinfo->parentID;
    if (volinfo->cloneID)
	vh->info.cloneID = volinfo->cloneID;
    if (volinfo->status)
	vh->info.status = volinfo->status;
    if (volinfo->copyDate)
	vh->info.copyDate = volinfo->copyDate;
    if (volinfo->inUse)
	vh->info.inUse = volinfo->inUse;
    if (volinfo->creationDate)
	vh->info.creationDate = volinfo->creationDate;
    if (volinfo->backupDate)
	vh->info.backupDate = volinfo->backupDate;
    if (volinfo->dayUse != -1)
	vh->info.dayUse = volinfo->dayUse;
    if (volinfo->filecount)
	vh->info.filecount = volinfo->filecount;
    if (volinfo->maxquota != -1)
	vh->info.maxquota = volinfo->maxquota;
    if (volinfo->size)
	vh->info.size = volinfo->size;

    vld_info_write(vh);

    vld_free (vh);
    dp_free (dp);
    vld_put_trans(trans);

 out:
    VOLSER_EXIT;
    
    return ret;
}

/*
 *
 */

int
VOLSER_AFSVolXListPartitions(struct rx_call *call,
			     part_entries *ent)
{
    int i;
    struct dp_part *dp = NULL;
    int ret = 0;
    int32_t partIDs[26]; /* XXX */

    mlog_log(MDEBVOLDB, "VOLSER_AFSVolXListPartitions");

    i = 0;
    do {
	ret = dp_find(&dp);
	if (dp == NULL)
	    break;
	if (ret)
	    goto out;
	partIDs[i] = dp->num;
	i++;
    } while (i < 26);
    
    ent->len = i;
    ent->val = malloc(sizeof(int32_t) * ent->len);
    memcpy(ent->val, partIDs, sizeof(int32_t) * ent->len);

    ret = 0;
 out:
    VOLSER_EXIT;
    
    return ret;
}

/*
 *
 */

int
VOLSER_AFSVolForwardMultiple(struct rx_call *call,
			     const int32_t fromTrans,
			     const int32_t fromData,
			     const manyDests *destinations,
			     const int32_t spare0,
			     const struct restoreCookie *cookie,
			     multi_results *results)
{
    int ret = 0;

    mlog_log(MDEBVOLDB, "VOLSER_AFSVolForwardMultiple");

    if (!sec_is_superuser(call)) {
	ret = VOLSERBAD_ACCESS;
	goto out;
    }
    
    ret = VOLSERFAILEDOP;

 out:
    VOLSER_EXIT;
    
    return ret;
}


