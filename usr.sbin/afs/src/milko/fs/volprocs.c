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

#include "fsrv_locl.h"

RCSID("$KTH: volprocs.c,v 1.9 2000/10/03 00:17:36 lha Exp $");

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
	return VOLSERFAILEDOP; /* XXX */
    }

    ret = vld_open_volume_by_num (*dp, volid, vh);
    if (ret) {
	fprintf (stderr, "volser_fetch_vh: vld_open_volume_by_num: %d\n", ret);
	dp_free (*dp);
	return VOLSERFAILEDOP; /* XXX */
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
    int ret;
    int32_t backstoretype = VLD_SVOL;
    struct dp_part *dp;

    mlog_log(MDEBVOLDB, "VOLSER_AFSVolCreateVolume "
	     "part %d name %s type %d parent %d volid %u",
	     partition, name, type, parent, *volid);

    /* XXX parent should be used */

    ret = vld_create_trans (partition, *volid, trans);

    if (ret)
	return ret;

    ret = vld_trans_set_iflags (*trans, ITOffline);
    if (ret)
	return ret;

    ret = dp_create (partition, &dp);
    if (ret) {
	vld_end_trans (*trans, NULL);
	return ret;
    }

    ret = vld_create_volume (dp, *volid, name, backstoretype, type, 0);
    if (ret) {
	vld_end_trans (*trans, NULL);
	return ret;
    }

    return 0;
}

/*
 *
 */

int
VOLSER_AFSVolDeleteVolume(struct rx_call *call,
			  const int32_t trans)
{
    int ret;

    ret = vld_verify_trans(trans);
    if (ret)
	return ret;

    return VOLSERFAILEDOP;
}

/*
 *
 */

int
VOLSER_AFSVolNukeVolume(struct rx_call *call,
			const int32_t partID,
			const int32_t volID)
{
    return VOLSERFAILEDOP;
}

/*
 *
 */

int
VOLSER_AFSVolDump(struct rx_call *call,
		  const int32_t fromTrans,
		  const int32_t fromDate)
{
    int ret;
    struct trans *trans;
    struct dp_part *dp;
    volume_handle *vh;

    mlog_log (MDEBVOLDB, 
	      "VOLSER_AFSVolDump trans %d fromdate %d", 
	      fromTrans, fromDate);
    
    ret = vld_verify_trans (fromTrans);
    if (ret)
	return ret;

    ret = vld_get_trans (fromTrans, &trans);
    if (ret)
	return ret;
    
    ret = volser_fetch_vh (trans->partition, trans->volid, &dp, &vh);
    if (ret) {
	vld_put_trans(trans);
	return ret;
    }

    if (fromDate != 0) {
	vld_put_trans(trans);
	return VOLSERFAILEDOP;
    }



    vld_put_trans (trans);
    return VOLSERFAILEDOP;
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
    return VOLSERFAILEDOP;
}

/*
 *
 */

int
VOLSER_AFSVolRestore(struct rx_call *call,
		     const int32_t toTrans,
		     const int32_t flags,
		     const struct restoreCookie *cookie)
{
    return VOLSERFAILEDOP;
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
    return VOLSERFAILEDOP;
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
    return VOLSERFAILEDOP;
}

/*
 *
 */

int
VOLSER_AFSVolReClone(struct rx_call *call,
		     const int32_t tid,
		     const int32_t cloneID)
{
    return VOLSERFAILEDOP;
}

/*
 *
 */

int
VOLSER_AFSVolSetForwarding(struct rx_call *call,
			   const int32_t tid,
			   const int32_t newsite)
{
    return VOLSERFAILEDOP;
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
    int ret;

    ret = vld_create_trans(partition, volume, trans);
    if (ret)
	return ret;

    ret = vld_trans_set_iflags(*trans, flags);
    if (ret) {
	vld_end_trans (*trans, NULL);
	return ret;
    }

    return 0;
}

/*
 *
 */

int
VOLSER_AFSVolEndTrans(struct rx_call *call,
		      const int32_t trans,
		      int32_t *rcode)
{
    int ret;

    ret = vld_verify_trans(trans);
    if (ret)
	return ret;

    ret = vld_end_trans(trans, rcode);
    if (ret)
	return ret;

    return 0;
}

/*
 *
 */

int
VOLSER_AFSVolGetFlags(struct rx_call *call,
		      const int32_t trans,
		      int32_t *flags)
{
    int ret;

    ret = vld_verify_trans(trans);
    if (ret)
	return ret;

    ret = vld_trans_get_vflags(trans, flags);
    if (ret)
	return ret;

    return 0;
}

/*
 *
 */

int
VOLSER_AFSVolSetFlags(struct rx_call *call,
		      const int32_t transid,
		      const int32_t flags)
{
    int ret;
    struct trans *trans;
    struct dp_part *dp;
    volume_handle *vh;

    mlog_log(MDEBVOLDB, "VOLSER_AFSVolSetFlags "
	     "trans %d flags %d", transid, flags);

    ret = vld_verify_trans(transid);
    if (ret)
	return ret;

    ret = vld_get_trans(transid, &trans);
    if (ret)
	return ret;
    
    ret = volser_fetch_vh (trans->partition, trans->volid, &dp, &vh);
    if (ret) {
	vld_put_trans(trans);
	return ret;
    }

    if (flags & VTDeleteOnSalvage)
	vh->info.destroyMe = 's';
    else
	vh->info.destroyMe = 0;

    assert ((flags & VTOutOfService) == 0); /* XXX */

    vld_info_write(vh);

    vld_free (vh);
    dp_free (dp);

    vld_put_trans(trans);

    ret = vld_trans_set_vflags(transid, flags);
    if (ret)
	return ret;

    return 0;
}

/*
 *
 */

int
VOLSER_AFSVolGetName(struct rx_call *call,
		     const int32_t tid,
		     char tname[256])
{
    return VOLSERFAILEDOP;
}

/*
 *
 */

int
VOLSER_AFSVolGetStatus(struct rx_call *call,
		       const int32_t tid,
		       struct volser_status *status)
{
    return VOLSERFAILEDOP;
}

/*
 *
 */

int
VOLSER_AFSVolSetIdsTypes(struct rx_call *call,
			 const int32_t tId,
			 const char *name,
			 const int32_t type,
			 const int32_t pId,
			 const int32_t cloneId,
			 const int32_t backupId)
{
    return VOLSERFAILEDOP;
}

/*
 *
 */

int
VOLSER_AFSVolSetDate(struct rx_call *call,
		     const int32_t tid,
		     const int32_t newDate)
{
    return VOLSERFAILEDOP;
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
    int ret;

    i = 0;
    do {
	ret = dp_find(&dp);
	if (dp == NULL)
	    break;
	if (ret)
	    return ret;
	partIDs->partIds[i] = dp->num;
	i++;
    } while (i < 26);

    for (; i < 26; i++)
	partIDs->partIds[i] = -1;
    
    return 0;
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
    int ret;

    num = partition_name2num (name);
    if (num == -1)
	return 0; /* XXX */
    
    ret = dp_create (num, &dp);
    if (ret)
	return ret;

    memset(partition, 0, sizeof(*partition));
    strlcpy(partition->name, dp->part, 32);    
    
    dp_free(dp);
    
    return 0;
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
    return VOLSERFAILEDOP;
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
    int ret;
    struct dp_part *dp;
    struct volintInfo *volinfo;

    fprintf(stderr, 
	    "VOLSER_AFSVolListOneVolume partid: %d volid: %u\n",
	    partID, volid);

    ret = volser_fetch_vh (partID, volid, &dp, &vh);
    if (ret)
	return ret;

    resultEntries->len = 1;
    
    volinfo = calloc(sizeof(struct volintInfo) * resultEntries->len, 1);
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
    volinfo->needsSalvaged = 0;
    volinfo->destroyMe = 0;

    vld_free (vh);
    dp_free (dp);


    return 0;
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
    return VOLSERFAILEDOP;
}

/*
 *
 */

int
VOLSER_AFSVolMonitor(struct rx_call *call,
		     transDebugEntries *result)
{
    return VOLSERFAILEDOP;
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
    return VOLSERFAILEDOP;
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
    int ret;
    struct dp_part *dp;
    struct xvolintInfo *volinfo;

    fprintf(stderr, 
	    "VOLSER_AFSVolXListOneVolume partid: %d volid: %u\n",
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


    return 0;
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
    int ret;
    struct dp_part *dp;
    struct trans *trans;

    mlog_log(MDEBVOLDB, "VOLSER_AFSVolSetInfo "
	     "trans %d name %s type %d parent %d volid %d backup %d",
	     transid, volinfo->name, volinfo->type, volinfo->parentID,
	     volinfo->volid, volinfo->backupID);

    ret = vld_verify_trans(transid);
    if (ret)
	return ret;

    ret = vld_get_trans(transid, &trans);
    if (ret)
	return ret;
    
    ret = volser_fetch_vh (trans->partition, trans->volid, &dp, &vh);
    if (ret) {
	vld_put_trans(trans);
	return ret;
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

    return 0;
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
    int ret;
    int32_t partIDs[26]; /* XXX */

    mlog_log(MDEBVOLDB, "VOLSER_AFSVolXListPartitions");

    i = 0;
    do {
	ret = dp_find(&dp);
	if (dp == NULL)
	    break;
	if (ret)
	    return ret;
	partIDs[i] = dp->num;
	i++;
    } while (i < 26);
    
    ent->len = i;
    ent->val = malloc(sizeof(int32_t) * ent->len);
    memcpy(ent->val, partIDs, sizeof(int32_t) * ent->len);

    return 0;
}

/*
 *
 */

int
VOLSER_AFSVolForwardMultiple(struct rx_call *call,
			     const int32_t fromTrans,
			     const int32_t fromData,
			     const replicas *destinations,
			     const int32_t spare0,
			     const struct restoreCookie *cookie,
			     const multi_results *results)
{
    return VOLSERFAILEDOP;
}


