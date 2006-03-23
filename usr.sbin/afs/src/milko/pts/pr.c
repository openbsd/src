/*
 * Copyright (c) 1998, 1999 Kungliga Tekniska Högskolan
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

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <sys/types.h>
#include <ctype.h>
#include <assert.h>

#include <rx/rx.h>
#include <rx/rx_null.h>

#include <ports.h>
#include <ko.h>
#include <bool.h>

#ifdef KERBEROS
#ifdef HAVE_OPENSSL
#include <openssl/des.h>
#else
#include <des.h>
#endif
#include <krb.h>
#include <rxkad.h>
#include "rxkad_locl.h"
#endif

#include <err.h>

#ifndef HAVE_UNISTD_H
#include <unistd.h>
#endif

#include <service.h>

#include "pts.h"
#include "pts.ss.h"
#include "ptserver.h"
#include "pts.ss.h"

#include "msecurity.h"

#include <mlog.h>
#include <mdebug.h>

RCSID("$arla: pr.c,v 1.24 2002/04/20 15:57:17 lha Exp $");

/*
 *
 */

int
PR_NameToID(struct rx_call *call, const namelist *nlist, idlist *ilist)
{
    int i;
    int status;

    mlog_log (MDEBPR, "PR_NameToID: securityIndex: %d ilen: %d",
	      call->conn->securityIndex, nlist->len);

#ifdef KERBEROS
    if (call->conn->securityIndex == 2) {
	serv_con_data *cdat = call->conn->securityData;
	mlog_log (MDEBPR, "  user: %s.%s@%s",
		  cdat->user->name,
		  cdat->user->instance,
		  cdat->user->realm);
    }
#endif

    ilist->len = nlist->len;
    ilist->val = malloc(sizeof(int) * ilist->len);
    if (ilist->val == NULL)
	return PRDBBAD;

    for (i = 0; i < nlist->len; i++) {
	mlog_log (MDEBPR, "  name: %s", nlist->val[i]);
	
	status = conv_name_to_id(nlist->val[i], &ilist->val[i]);
	if (status == PRNOENT)
	    ilist->val[i] = PR_ANONYMOUSID;
	else if (status)
	    return status;
    }
    return 0;
}

/*
 *
 */

int
PR_IDToName(struct rx_call *call, const idlist *ilist, namelist *nlist)
{
    int i;
    int status;
    
    mlog_log (MDEBPR, "PR_IDToName: securityIndex: %d ilen %d",
	      call->conn->securityIndex, ilist->len);

    
    if (ilist->len < 0 || ilist->len >= PR_MAXLIST)
	return PRTOOMANY;

    nlist->len = ilist->len;

    if (ilist->len == 0) {
	nlist->val = NULL;
	return 0;
    }

    nlist->val = calloc(nlist->len, sizeof(prname));
    if (nlist->val == NULL)
	return PRDBBAD;

    for (i = 0; i < ilist->len; i++) {
	mlog_log (MDEBPR, "  id: %d", ilist->val[i]);
	status = conv_id_to_name(ilist->val[i], nlist->val[i]);
	if (status == PRNOENT)
	    snprintf (nlist->val[i], PR_MAXNAMELEN, "%d", ilist->val[i]);
	else if (status)
	    return status;
    }
    return 0;
}

/*
 *
 */

int
PR_NewEntry(struct rx_call *call, const char *name, 
	    const int32_t flag, const int32_t oid, int32_t *id)
{
    int error;
    int32_t owner;
    char *localname;
    Bool localp;

    mlog_log (MDEBPR, "PR_NewEntry: securityIndex: %d name: %s oid: %d",
	      call->conn->securityIndex, name, oid);


    /* XXX should be authuser? */
    if (!sec_is_superuser(call))
	return PRPERM;

    localname = localize_name(name, &localp);

    /* XXX do it properly! */
    if (localp == FALSE)
	owner = PR_SYSADMINID;
    else
	owner = oid;

    if ((flag & PRTYPE) == PRUSER) {
	error = next_free_user_id(id);
	if (!error)
	    error = create_user(localname, *id, owner, PR_SYSADMINID); /* XXX */
    } else if ((flag & PRTYPE) == PRGRP) {
	error = next_free_group_id(id);
	if (!error)
	    error = create_group(localname, *id, owner, PR_SYSADMINID); /* XXX */
    } else {
	error = PRPERM;
    }

    return error;
}

/*
 *
 */

int
PR_INewEntry(struct rx_call *call, const char *name, 
	     const int32_t id, const int32_t oid)
{
    int error;
    char *localname;
    Bool localp;
    int32_t owner = PR_SYSADMINID;
    int32_t creator = PR_SYSADMINID;
    
    /* XXX should be authuser? */
    if (!sec_is_superuser(call))
	return PRPERM;

    mlog_log (MDEBPR, "PR_INewEntry securityIndex: %d name: %s oid: %d",
	      call->conn->securityIndex, name, oid);

    localname = localize_name(name, &localp);
    
    /* XXX do it properly! */
    if (localp == TRUE)
	owner = oid;
    
    if (id > 0)
	error = create_user(localname, id, owner, creator); /* XXX */
    else if (id < 0)
	error = create_group(localname, id, owner, creator); /* XXX */
    else
	error = PRPERM;

    return error;
}

/*
 *
 */

int
PR_ListEntry(struct rx_call *call, const int32_t id,
	     struct prcheckentry *entry)
{
    prentry pr_entry;
    int status;
   
    mlog_log (MDEBPR, "PR_ListEntry securityIndex: %d id: %d", 
	      call->conn->securityIndex, id);
#ifdef KERBEROS
    if (call->conn->securityIndex == 2) {
	serv_con_data *cdat = call->conn->securityData;
	mlog_log (MDEBPR, "PR_ListEntry user: %s.%s@%s",
		  cdat->user->name,
		  cdat->user->instance,
		  cdat->user->realm);
    }
#endif

    memset(&pr_entry, 0, sizeof(pr_entry));
    status = read_prentry(id, &pr_entry);
    if (status)
	return status;
    entry->flags = pr_entry.flags;
    entry->id = pr_entry.id;
    entry->owner = pr_entry.owner;
    entry->creator = pr_entry.creator;
    entry->ngroups = pr_entry.ngroups;
    entry->nusers = pr_entry.nusers;
    entry->count = pr_entry.count;
    memcpy(entry->reserved, pr_entry.reserved, sizeof(pr_entry.reserved));
    strlcpy(entry->name, pr_entry.name, PR_MAXNAMELEN);

    return 0;
}

/*
 *
 */

int
PR_DumpEntry(struct rx_call *call, const int32_t pos, 
	     struct prdebugentry *entry)
{
    mlog_log (MDEBPR, "PR_DumpEntry");
    return -1;
}

/*
 *
 */

int
PR_ChangeEntry(struct rx_call *call, const int32_t id, const char *name,
	       const int32_t oid, const int32_t newid)
{
    mlog_log (MDEBPR, "PR_ChangeEntry");
    return -1;
}


/*
 *
 */

int
PR_SetFieldsEntry(struct rx_call *call, const int32_t id, const int32_t mask,
		  const int32_t flags, const int32_t ngroups, 
		  const int32_t nusers,
		  const int32_t spare1, const int32_t spare2)
{
    mlog_log (MDEBPR, "PR_SetFieldsEntry");
    return -1;
}


/*
 *
 */

int
PR_Delete(struct rx_call *call, const int32_t id)
{
    mlog_log (MDEBPR, "PR_Delete");

    if (!sec_is_superuser(call))
	return PRPERM;

    return -1;
}


/*
 *
 */

int
PR_WhereIsIt(struct rx_call *call, const int32_t id, int32_t *ps)
{
    mlog_log (MDEBPR, "PR_WhereIsIt");
    return -1;
}


/*
 *
 */

int
PR_AddToGroup(struct rx_call *call, const int32_t uid, const int32_t gid)
{
    mlog_log (MDEBPR, "PR_AddToGroup");

    if (!sec_is_superuser(call))
      return PRPERM;

    return addtogroup(uid,gid);
}


/*
 *
 */

int
PR_RemoveFromGroup(struct rx_call *call, const int32_t id, const int32_t gid)
{
    mlog_log (MDEBPR, "PR_RemoveFromGroup");

    if (!sec_is_superuser(call))
	return PRPERM;

    return removefromgroup(id, gid);
}


/*
 *
 */

int
PR_ListMax(struct rx_call *call, int32_t *uid, int32_t *gid)
{
    mlog_log (MDEBPR, "PR_ListMax");
    *uid = pr_header.maxID;
    *gid = pr_header.maxGroup;
    return 0;
}


/*
 *
 */

int
PR_SetMax(struct rx_call *call, const int32_t uid, const int32_t gflag)
{
    mlog_log (MDEBPR, "PR_SetMax");

    if(gflag) {
	pr_header.maxGroup = uid;
    } else {
	pr_header.maxID = uid;
    }

    return 0;
}


/*
 *
 */

int
PR_ListElements(struct rx_call *call, const int32_t id, 
		prlist *elist, int32_t *over)
{
    mlog_log (MDEBPR, "PR_ListElements");

    return listelements(id, elist, FALSE);
}


/*
 *
 */

int
PR_GetCPS(struct rx_call *call, const int32_t id, 
	  prlist *elist, int32_t *over)
{
    mlog_log (MDEBPR, "PR_GetCPS");

    return listelements(id, elist, TRUE);
}


/*
 *
 */

int
PR_ListOwned(struct rx_call *call, const int32_t id, 
	     prlist *elist, int32_t *over)
{
    mlog_log (MDEBPR, "PR_ListOwned");
    return -1;
}


/*
 *
 */

int
PR_IsAMemberOf(struct rx_call *call, const int32_t uid, const int32_t gid,
	       int32_t *flag)
{

  /* XXX Check authorization */

    prlist elist;
    int ret=0;
    int i=0;

    mlog_log (MDEBPR, "PR_IsAMemberOf");

    if((ret = listelements(uid, &elist, TRUE)) !=0) {
	free(elist.val);
	return ret;
    }

    for(i=0; i < elist.len ; i++) {
	if(elist.val[i] == gid) {
	    *flag=1;
	    free(elist.val);
	    return 0;
	}
    }
    
    free(elist.val);
    return 0;
}
