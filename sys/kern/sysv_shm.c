/*	$OpenBSD: sysv_shm.c,v 1.27 2002/07/16 23:06:05 art Exp $	*/
/*	$NetBSD: sysv_shm.c,v 1.50 1998/10/21 22:24:29 tron Exp $	*/

/*
 * Copyright (c) 1994 Adam Glass and Charles M. Hannum.  All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *	This product includes software developed by Adam Glass and Charles M.
 *	Hannum.
 * 4. The names of the authors may not be used to endorse or promote products
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

#include <sys/types.h>
#include <sys/param.h>
#include <sys/kernel.h>
#include <sys/shm.h>
#include <sys/proc.h>
#include <sys/uio.h>
#include <sys/time.h>
#include <sys/malloc.h>
#include <sys/mman.h>
#include <sys/systm.h>
#include <sys/stat.h>

#include <sys/mount.h>
#include <sys/syscallargs.h>

#include <uvm/uvm_extern.h>

struct shminfo shminfo;
struct shmid_ds *shmsegs;

struct shmid_ds *shm_find_segment_by_shmid(int);

/*
 * Provides the following externally accessible functions:
 *
 * shminit(void);		                 initialization
 * shmexit(struct vmspace *)                     cleanup
 * shmfork(struct vmspace *, struct vmspace *)   fork handling
 * shmsys(arg1, arg2, arg3, arg4);         shm{at,ctl,dt,get}(arg2, arg3, arg4)
 *
 * Structures:
 * shmsegs (an array of 'struct shmid_ds')
 * per proc array of 'struct shmmap_state'
 */

#define	SHMSEG_FREE		0x0200
#define	SHMSEG_REMOVED  	0x0400
#define	SHMSEG_ALLOCATED	0x0800
#define	SHMSEG_WANTED		0x1000

int shm_last_free, shm_nused, shm_committed;

struct shm_handle {
	struct uvm_object *shm_object;
};

struct shmmap_state {
	vaddr_t va;
	int shmid;
};

int shm_find_segment_by_key(key_t);
void shm_deallocate_segment(struct shmid_ds *);
int shm_delete_mapping(struct vmspace *, struct shmmap_state *);
int shmget_existing(struct proc *, struct sys_shmget_args *,
			 int, int, register_t *);
int shmget_allocate_segment(struct proc *, struct sys_shmget_args *,
				 int, register_t *);

int
shm_find_segment_by_key(key)
	key_t key;
{
	int i;

	for (i = 0; i < shminfo.shmmni; i++)
		if ((shmsegs[i].shm_perm.mode & SHMSEG_ALLOCATED) &&
		    shmsegs[i].shm_perm.key == key)
			return i;
	return -1;
}

struct shmid_ds *
shm_find_segment_by_shmid(shmid)
	int shmid;
{
	int segnum;
	struct shmid_ds *shmseg;

	segnum = IPCID_TO_IX(shmid);
	if (segnum < 0 || segnum >= shminfo.shmmni)
		return NULL;
	shmseg = &shmsegs[segnum];
	if ((shmseg->shm_perm.mode & (SHMSEG_ALLOCATED | SHMSEG_REMOVED))
	    != SHMSEG_ALLOCATED ||
	    shmseg->shm_perm.seq != IPCID_TO_SEQ(shmid))
		return NULL;
	return shmseg;
}

void
shm_deallocate_segment(shmseg)
	struct shmid_ds *shmseg;
{
	struct shm_handle *shm_handle;
	size_t size;

	shm_handle = shmseg->shm_internal;
	size = round_page(shmseg->shm_segsz);
	uao_detach(shm_handle->shm_object);
	free((caddr_t)shm_handle, M_SHM);
	shmseg->shm_internal = NULL;
	shm_committed -= btoc(size);
	shmseg->shm_perm.mode = SHMSEG_FREE;
	shm_nused--;
}

int
shm_delete_mapping(vm, shmmap_s)
	struct vmspace *vm;
	struct shmmap_state *shmmap_s;
{
	struct shmid_ds *shmseg;
	int segnum, result;
	size_t size;
	
	segnum = IPCID_TO_IX(shmmap_s->shmid);
	shmseg = &shmsegs[segnum];
	size = round_page(shmseg->shm_segsz);
	result = uvm_deallocate(&vm->vm_map, shmmap_s->va, size);
	if (result != KERN_SUCCESS)
		return EINVAL;
	shmmap_s->shmid = -1;
	shmseg->shm_dtime = time.tv_sec;
	if ((--shmseg->shm_nattch <= 0) &&
	    (shmseg->shm_perm.mode & SHMSEG_REMOVED)) {
		shm_deallocate_segment(shmseg);
		shm_last_free = segnum;
	}
	return 0;
}

int
sys_shmdt(p, v, retval)
	struct proc *p;
	void *v;
	register_t *retval;
{
	struct sys_shmdt_args /* {
		syscallarg(const void *) shmaddr;
	} */ *uap = v;
	struct shmmap_state *shmmap_s;
	int i;

	shmmap_s = (struct shmmap_state *)p->p_vmspace->vm_shm;
	if (shmmap_s == NULL)
		return EINVAL;

	for (i = 0; i < shminfo.shmseg; i++, shmmap_s++)
		if (shmmap_s->shmid != -1 &&
		    shmmap_s->va == (vaddr_t)SCARG(uap, shmaddr))
			break;
	if (i == shminfo.shmseg)
		return EINVAL;
	return shm_delete_mapping(p->p_vmspace, shmmap_s);
}

int
sys_shmat(p, v, retval)
	struct proc *p;
	void *v;
	register_t *retval;
{
	struct sys_shmat_args /* {
		syscallarg(int) shmid;
		syscallarg(const void *) shmaddr;
		syscallarg(int) shmflg;
	} */ *uap = v;
	int error, i, flags;
	struct ucred *cred = p->p_ucred;
	struct shmid_ds *shmseg;
	struct shmmap_state *shmmap_s = NULL;
	struct shm_handle *shm_handle;
	vaddr_t attach_va;
	vm_prot_t prot;
	vsize_t size;
	int rv;

	shmmap_s = (struct shmmap_state *)p->p_vmspace->vm_shm;
	if (shmmap_s == NULL) {
		size = shminfo.shmseg * sizeof(struct shmmap_state);
		shmmap_s = malloc(size, M_SHM, M_WAITOK);
		for (i = 0; i < shminfo.shmseg; i++)
			shmmap_s[i].shmid = -1;
		p->p_vmspace->vm_shm = (caddr_t)shmmap_s;
	}
	shmseg = shm_find_segment_by_shmid(SCARG(uap, shmid));
	if (shmseg == NULL)
		return EINVAL;
	error = ipcperm(cred, &shmseg->shm_perm,
		    (SCARG(uap, shmflg) & SHM_RDONLY) ? IPC_R : IPC_R|IPC_W);
	if (error)
		return error;
	for (i = 0; i < shminfo.shmseg; i++) {
		if (shmmap_s->shmid == -1)
			break;
		shmmap_s++;
	}
	if (i >= shminfo.shmseg)
		return EMFILE;
	size = round_page(shmseg->shm_segsz);
	prot = VM_PROT_READ;
	if ((SCARG(uap, shmflg) & SHM_RDONLY) == 0)
		prot |= VM_PROT_WRITE;
	flags = MAP_ANON | MAP_SHARED;
	if (SCARG(uap, shmaddr)) {
		flags |= MAP_FIXED;
		if (SCARG(uap, shmflg) & SHM_RND) 
			attach_va =
			    (vaddr_t)SCARG(uap, shmaddr) & ~(SHMLBA-1);
		else if (((vaddr_t)SCARG(uap, shmaddr) & (SHMLBA-1)) == 0)
			attach_va = (vaddr_t)SCARG(uap, shmaddr);
		else
			return EINVAL;
	} else {
		/* This is just a hint to uvm_map() about where to put it. */
		attach_va = round_page((vaddr_t)p->p_vmspace->vm_taddr +
		    MAXTSIZ + MAXDSIZ);
	}
	shm_handle = shmseg->shm_internal;
	uao_reference(shm_handle->shm_object);
	rv = uvm_map(&p->p_vmspace->vm_map, &attach_va, size,
	    shm_handle->shm_object, 0, 0, UVM_MAPFLAG(prot, prot,
	    UVM_INH_SHARE, UVM_ADV_RANDOM, 0));
	if (rv != KERN_SUCCESS) {
		return ENOMEM;
	}

	shmmap_s->va = attach_va;
	shmmap_s->shmid = SCARG(uap, shmid);
	shmseg->shm_lpid = p->p_pid;
	shmseg->shm_atime = time.tv_sec;
	shmseg->shm_nattch++;
	*retval = attach_va;
	return 0;
}

int
sys_shmctl(p, v, retval)
	struct proc *p;
	void *v;
	register_t *retval;
{
	struct sys_shmctl_args /* {
		syscallarg(int) shmid;
		syscallarg(int) cmd;
		syscallarg(struct shmid_ds *) buf;
	} */ *uap = v;
	int error;
	struct ucred *cred = p->p_ucred;
	struct shmid_ds inbuf;
	struct shmid_ds *shmseg;

	shmseg = shm_find_segment_by_shmid(SCARG(uap, shmid));
	if (shmseg == NULL)
		return EINVAL;
	switch (SCARG(uap, cmd)) {
	case IPC_STAT:
		if ((error = ipcperm(cred, &shmseg->shm_perm, IPC_R)) != 0)
			return error;
		error = copyout((caddr_t)shmseg, SCARG(uap, buf),
				sizeof(inbuf));
		if (error)
			return error;
		break;
	case IPC_SET:
		if ((error = ipcperm(cred, &shmseg->shm_perm, IPC_M)) != 0)
			return error;
		error = copyin(SCARG(uap, buf), (caddr_t)&inbuf,
		    sizeof(inbuf));
		if (error)
			return error;
		shmseg->shm_perm.uid = inbuf.shm_perm.uid;
		shmseg->shm_perm.gid = inbuf.shm_perm.gid;
		shmseg->shm_perm.mode =
		    (shmseg->shm_perm.mode & ~ACCESSPERMS) |
		    (inbuf.shm_perm.mode & ACCESSPERMS);
		shmseg->shm_ctime = time.tv_sec;
		break;
	case IPC_RMID:
		if ((error = ipcperm(cred, &shmseg->shm_perm, IPC_M)) != 0)
			return error;
		shmseg->shm_perm.key = IPC_PRIVATE;
		shmseg->shm_perm.mode |= SHMSEG_REMOVED;
		if (shmseg->shm_nattch <= 0) {
			shm_deallocate_segment(shmseg);
			shm_last_free = IPCID_TO_IX(SCARG(uap, shmid));
		}
		break;
	case SHM_LOCK:
	case SHM_UNLOCK:
	default:
		return EINVAL;
	}
	return 0;
}

int
shmget_existing(p, uap, mode, segnum, retval)
	struct proc *p;
	struct sys_shmget_args /* {
		syscallarg(key_t) key;
		syscallarg(size_t) size;
		syscallarg(int) shmflg;
	} */ *uap;
	int mode;
	int segnum;
	register_t *retval;
{
	struct shmid_ds *shmseg;
	struct ucred *cred = p->p_ucred;
	int error;

	shmseg = &shmsegs[segnum];
	if (shmseg->shm_perm.mode & SHMSEG_REMOVED) {
		/*
		 * This segment is in the process of being allocated.  Wait
		 * until it's done, and look the key up again (in case the
		 * allocation failed or it was freed).
		 */
		shmseg->shm_perm.mode |= SHMSEG_WANTED;
		error = tsleep((caddr_t)shmseg, PLOCK | PCATCH, "shmget", 0);
		if (error)
			return error;
		return EAGAIN;
	}
	if ((error = ipcperm(cred, &shmseg->shm_perm, mode)) != 0)
		return error;
	if (SCARG(uap, size) && SCARG(uap, size) > shmseg->shm_segsz)
		return EINVAL;
	if ((SCARG(uap, shmflg) & (IPC_CREAT | IPC_EXCL)) ==
	    (IPC_CREAT | IPC_EXCL))
		return EEXIST;
	*retval = IXSEQ_TO_IPCID(segnum, shmseg->shm_perm);
	return 0;
}

int
shmget_allocate_segment(p, uap, mode, retval)
	struct proc *p;
	struct sys_shmget_args /* {
		syscallarg(key_t) key;
		syscallarg(size_t) size;
		syscallarg(int) shmflg;
	} */ *uap;
	int mode;
	register_t *retval;
{
	int i, segnum, shmid, size;
	struct ucred *cred = p->p_ucred;
	struct shmid_ds *shmseg;
	struct shm_handle *shm_handle;
	int error = 0;
	
	if (SCARG(uap, size) < shminfo.shmmin ||
	    SCARG(uap, size) > shminfo.shmmax)
		return EINVAL;
	if (shm_nused >= shminfo.shmmni) /* any shmids left? */
		return ENOSPC;
	size = round_page(SCARG(uap, size));
	if (shm_committed + btoc(size) > shminfo.shmall)
		return ENOMEM;
	if (shm_last_free < 0) {
		for (i = 0; i < shminfo.shmmni; i++)
			if (shmsegs[i].shm_perm.mode & SHMSEG_FREE)
				break;
		if (i == shminfo.shmmni)
			panic("shmseg free count inconsistent");
		segnum = i;
	} else  {
		segnum = shm_last_free;
		shm_last_free = -1;
	}
	shmseg = &shmsegs[segnum];
	/*
	 * In case we sleep in malloc(), mark the segment present but deleted
	 * so that noone else tries to create the same key.
	 */
	shmseg->shm_perm.mode = SHMSEG_ALLOCATED | SHMSEG_REMOVED;
	shmseg->shm_perm.key = SCARG(uap, key);
	shmseg->shm_perm.seq = (shmseg->shm_perm.seq + 1) & 0x7fff;
	shm_handle = (struct shm_handle *)
	    malloc(sizeof(struct shm_handle), M_SHM, M_WAITOK);
	shmid = IXSEQ_TO_IPCID(segnum, shmseg->shm_perm);
	

	shm_handle->shm_object = uao_create(size, 0);

	shmseg->shm_internal = shm_handle;
	shmseg->shm_perm.cuid = shmseg->shm_perm.uid = cred->cr_uid;
	shmseg->shm_perm.cgid = shmseg->shm_perm.gid = cred->cr_gid;
	shmseg->shm_perm.mode = (shmseg->shm_perm.mode & SHMSEG_WANTED) |
	    (mode & ACCESSPERMS) | SHMSEG_ALLOCATED;
	shmseg->shm_segsz = SCARG(uap, size);
	shmseg->shm_cpid = p->p_pid;
	shmseg->shm_lpid = shmseg->shm_nattch = 0;
	shmseg->shm_atime = shmseg->shm_dtime = 0;
	shmseg->shm_ctime = time.tv_sec;
	shm_committed += btoc(size);
	shm_nused++;

	*retval = shmid;
	if (shmseg->shm_perm.mode & SHMSEG_WANTED) {
		/*
		 * Somebody else wanted this key while we were asleep.  Wake
		 * them up now.
		 */
		shmseg->shm_perm.mode &= ~SHMSEG_WANTED;
		wakeup((caddr_t)shmseg);
	}
	return error;
}

int
sys_shmget(p, v, retval)
	struct proc *p;
	void *v;
	register_t *retval;
{
	struct sys_shmget_args /* {
		syscallarg(key_t) key;
		syscallarg(int) size;
		syscallarg(int) shmflg;
	} */ *uap = v;
	int segnum, mode, error;

	mode = SCARG(uap, shmflg) & ACCESSPERMS;
	if (SCARG(uap, key) != IPC_PRIVATE) {
	again:
		segnum = shm_find_segment_by_key(SCARG(uap, key));
		if (segnum >= 0) {
			error = shmget_existing(p, uap, mode, segnum, retval);
			if (error == EAGAIN)
				goto again;
			return error;
		}
		if ((SCARG(uap, shmflg) & IPC_CREAT) == 0) 
			return ENOENT;
	}
	return shmget_allocate_segment(p, uap, mode, retval);
}

void
shmfork(vm1, vm2)
	struct vmspace *vm1, *vm2;
{
	struct shmmap_state *shmmap_s;
	size_t size;
	int i;

	if (vm1->vm_shm == NULL) {
		vm2->vm_shm = NULL;
		return;
	}

	size = shminfo.shmseg * sizeof(struct shmmap_state);
	shmmap_s = malloc(size, M_SHM, M_WAITOK);
	bcopy(vm1->vm_shm, shmmap_s, size);
	vm2->vm_shm = (caddr_t)shmmap_s;
	for (i = 0; i < shminfo.shmseg; i++, shmmap_s++)
		if (shmmap_s->shmid != -1)
			shmsegs[IPCID_TO_IX(shmmap_s->shmid)].shm_nattch++;
}

void
shmexit(vm)
	struct vmspace *vm;
{
	struct shmmap_state *shmmap_s;
	int i;

	shmmap_s = (struct shmmap_state *)vm->vm_shm;
	if (shmmap_s == NULL)
		return;
	for (i = 0; i < shminfo.shmseg; i++, shmmap_s++)
		if (shmmap_s->shmid != -1)
			shm_delete_mapping(vm, shmmap_s);
	free(vm->vm_shm, M_SHM);
	vm->vm_shm = NULL;
}

void
shminit()
{
	int i;

	shminfo.shmmax *= PAGE_SIZE;

	for (i = 0; i < shminfo.shmmni; i++) {
		shmsegs[i].shm_perm.mode = SHMSEG_FREE;
		shmsegs[i].shm_perm.seq = 0;
	}
	shm_last_free = 0;
	shm_nused = 0;
	shm_committed = 0;
}

void
shmid_n2o(n, o)
	struct shmid_ds *n;
	struct oshmid_ds *o;
{
	o->shm_segsz = n->shm_segsz;
	o->shm_lpid = n->shm_lpid;
	o->shm_cpid = n->shm_cpid;
	o->shm_nattch = n->shm_nattch;
	o->shm_atime = n->shm_atime;
	o->shm_dtime = n->shm_dtime;
	o->shm_ctime = n->shm_ctime;
	o->shm_internal = n->shm_internal;
	ipc_n2o(&n->shm_perm, &o->shm_perm);
}
