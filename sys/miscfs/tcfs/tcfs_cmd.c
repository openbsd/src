/*	$OpenBSD: tcfs_cmd.c,v 1.3 2000/06/17 20:25:54 provos Exp $	*/
/*
 * Copyright 2000 The TCFS Project at http://tcfs.dia.unisa.it/
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. The name of the authors may not be used to endorse or promote products
 *    derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHORS ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHORS BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */
#include <sys/param.h>
#include <sys/systm.h>
#include <sys/proc.h>
#include <sys/time.h>
#include <sys/types.h>
#include <sys/vnode.h>
#include <sys/mount.h>
#include <sys/namei.h>
#include <sys/malloc.h>
#include <sys/buf.h>
#include <miscfs/tcfs/tcfs.h>
#include <miscfs/tcfs/tcfs_rw.h>
#include <miscfs/tcfs/tcfs_keytab.h>
#include <miscfs/tcfs/tcfs_cmd.h>
#include <miscfs/tcfs/tcfs_cipher.h>

int
tcfs_init_mp(struct tcfs_mount *mp, struct tcfs_args *req)
{
	int result = 0;
	int status = 0;

	if (!(mp->tcfs_uid_kt = tcfs_keytab_init())) {
		result = ENOMEM;
		status = ALLOCATION_FAILED;
	} else {
		if (!(mp->tcfs_gid_kt = tcfs_keytab_init())) {
			tcfs_keytab_dispose(mp->tcfs_uid_kt);
			status = ALLOCATION_FAILED;
			result = ENOMEM;
		} else {
			if ((req->cipher_num >= MaxNumOfCipher)||
			    (tcfs_cipher_vect[req->cipher_num].cipher_keysize == 0)) {
			 	result = EINVAL;
			 	status = BAD_CIPHER_NUMBER;
			 	tcfs_keytab_dispose(mp->tcfs_uid_kt);
			 	tcfs_keytab_dispose(mp->tcfs_gid_kt);
			} else
				mp->tcfs_cipher_num=req->cipher_num;
		}
	}
	
	(void)tcfs_set_status(mp,req,status);
	return result;
}

int
tcfs_exec_cmd(struct tcfs_mount *mp, struct tcfs_args *req)
{
	void *ks;
	int result = 0;
	int status = 0;
	

	switch (req->cmd) {
	case TCFS_PUT_UIDKEY:
		ks=TCFS_INIT_KEY(mp, req->tcfs_key);
		if (!ks) {
			result = ENOMEM;
			status = ALLOCATION_FAILED;
			break;
		}
		result = tcfs_keytab_push_uid(mp->tcfs_uid_kt, req->user,ks);
		if(result) {
			TCFS_CLEANUP_KEY(mp,ks);
			status = PUSHKEY_ERROR;
		}
		break;

	case TCFS_RM_UIDKEY:
		result = tcfs_keytab_rm_uid(mp->tcfs_uid_kt, req->user);
		status = (result ? RMKEY_ERROR : TCFS_OK);
		break;

	case TCFS_PUT_PIDKEY:
		ks = TCFS_INIT_KEY(mp, req->tcfs_key);
		if(!ks) {
			result = ENOMEM;
			status = ALLOCATION_FAILED;
			break;
		}
		result = tcfs_keytab_push_pid(mp->tcfs_uid_kt, req->user,
					      req->proc, ks);
		if(result) {
			TCFS_CLEANUP_KEY(mp,ks);
			status = PUSHKEY_ERROR;
		}
		break;

	case TCFS_RM_PIDKEY:
		result = tcfs_keytab_rm_pid(mp->tcfs_uid_kt, 
					    req->user, req->proc);
		status = (result ? RMKEY_ERROR : TCFS_OK);
		break;

	case TCFS_PUT_GIDKEY:
		result = tcfs_keytab_push_gid(mp, mp->tcfs_gid_kt, req->user,
					      req->group, req->treshold,
					      req->tcfs_key);
			
		status = (result ? PUSHKEY_ERROR : TCFS_OK);
		break; 

	case TCFS_RM_GIDKEY:
		result = tcfs_keytab_rm_gid(mp->tcfs_gid_kt, req->user,
					    req->group);
		status = (result ? RMKEY_ERROR : TCFS_OK);
		break;

	case TCFS_GET_STATUS:
		return tcfs_set_status(mp, req, TCFS_OK);
	}
	(void)tcfs_set_status(mp, req, status);
	return result;
}

int
tcfs_set_status(struct tcfs_mount *mp, struct tcfs_args *req, int error)
{
	req->st.status = error;
	req->st.tcfs_version = TCFS_VERSION_NUM;

	if(error != TCFS_OK)
		return error;

	req->st.n_ukey = mp->tcfs_uid_kt->cnt;
	req->st.n_gkey = mp->tcfs_gid_kt->cnt;
	strncpy(req->st.cipher_desc, TCFS_CIPHER_DESC(mp), MaxCipherNameLen);
	req->st.cipher_keysize = TCFS_CIPHER_KEYSIZE(mp);
	req->st.cipher_version = TCFS_CIPHER_VERSION(mp);

	return error;
}
	
int
tcfs_checkukey(struct ucred *c, struct proc *p, struct vnode *vp)
{
	return tcfs_keytab_check_uid(TCFS_VP2UKT(vp), c->cr_uid);
}

void *
tcfs_getukey(struct ucred *c, struct proc *p, struct vnode *vp)
{
	tcfs_keytab_node *n;

	n = tcfs_keytab_fetch_uid(TCFS_VP2UKT(vp),c->cr_uid);

	if (n)
		return n->kn_key;
	else
		return (void*)NULL;
}

int
tcfs_checkpkey(struct ucred *c, struct proc *p, struct vnode *vp)
{
	struct proc *cp;

	if (!p)
		cp = curproc;
	else
		cp = p;

	return tcfs_keytab_check_pid(TCFS_VP2UKT(vp), c->cr_uid, cp->p_pid);
}

void *
tcfs_getpkey(struct ucred *c, struct proc *p, struct vnode *vp)
{
	tcfs_keytab_node *n;
	struct proc *cp;

	if (!p)
		cp = curproc;
	else
		cp = p;
	
	n = tcfs_keytab_fetch_pid(TCFS_VP2UKT(vp), c->cr_uid, cp->p_pid);

	if (n)
		return n->kn_key;
	else
		return (void*)NULL;
}

int 
tcfs_checkgkey(struct ucred *c, struct proc *p, struct vnode *vp)
{
	return tcfs_keytab_check_uid(TCFS_VP2GKT(vp), c->cr_gid);
}

void *
tcfs_getgkey(struct ucred *c, struct proc *p, struct vnode *vp)
{
	tcfs_keytab_node *n;

	n = tcfs_keytab_fetch_gid(TCFS_VP2GKT(vp), c->cr_gid);

	if (n)
		return n->kn_key;
	else
		return (void*)NULL;
}

