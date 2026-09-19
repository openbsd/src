/* $OpenBSD: vmm.c,v 1.13 2026/09/19 17:21:52 dv Exp $ */
/*
 * Copyright (c) 2014-2023 Mike Larkin <mlarkin@openbsd.org>
 *
 * Permission to use, copy, modify, and distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 * ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
 * OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/device.h>
#include <sys/fcntl.h>
#include <sys/file.h>
#include <sys/filedesc.h>
#include <sys/pool.h>
#include <sys/pledge.h>
#include <sys/proc.h>
#include <sys/ioctl.h>
#include <sys/malloc.h>
#include <sys/signalvar.h>
#include <sys/stat.h>

#include <uvm/uvm_extern.h>
#include <uvm/uvm_aobj.h>

#include <machine/vmmvar.h>

#include <dev/vmm/vmm.h>

struct vmm_softc *vmm_softc;
struct pool vm_pool;
struct pool vcpu_pool;

int	vmm_probe(struct device *, void *, void *);
int	vmm_activate(struct device *, int);
void	vmm_attach(struct device *, struct device *,  void *);
int	vmmopen(dev_t, int, int, struct proc *);
int	vmmclose(dev_t, int, int, struct proc *);
int	vm_find_file(int, struct proc *, struct vm **);

struct cfdriver vmm_cd = {
	NULL, "vmm", DV_DULL, CD_SKIPHIBERNATE
};

const struct cfattach vmm_ca = {
	sizeof(struct vmm_softc), vmm_probe, vmm_attach, NULL, vmm_activate
};

int	vmm_dev_enter(int);
void	vmm_dev_exit(void);
int	vm_create(struct vm_create_params *, struct proc *);
size_t	vm_create_check_mem_ranges(struct vm_create_params *);
int	vm_intr_pending(struct vm *, struct vm_intr_params *);
int	vm_resetcpu(struct vm *, struct vm_resetcpu_params *);
int	vm_rwvmparams(struct vm *, struct vm_rwvmparams_params *, int);
int	vm_share_mem(struct vm *, struct vm_sharemem_params *, struct proc *);
void	vm_teardown(struct vm **);
int	vm_rele(struct vm *);
void	vm_request_stop(struct vm *);

int	vm_read(struct file *, struct uio *, int);
int	vm_write(struct file *, struct uio *, int);
int	vm_close(struct file *, struct proc *);
int	vm_kqfilter(struct file *, struct knote *);
int	vm_ioctl(struct file *, u_long, caddr_t, struct proc *);
int	vm_stat(struct file *, struct stat *, struct proc *);

static const struct fileops vmops = {
	.fo_read	= vm_read,
	.fo_write	= vm_write,
	.fo_ioctl	= vm_ioctl,
	.fo_kqfilter	= vm_kqfilter,
	.fo_stat	= vm_stat,
	.fo_close	= vm_close,
	.fo_seek	= NULL,		/* lseek(2) checks for NULL. */
};

int
vmm_probe(struct device *parent, void *match, void *aux)
{
	const char **busname = (const char **)aux;

	if (strcmp(*busname, vmm_cd.cd_name) != 0)
		return (0);
	return (vmm_probe_machdep(parent, match, aux));
}

void
vmm_attach(struct device *parent, struct device *self, void *aux)
{
	struct vmm_softc *sc = (struct vmm_softc *)self;

	rw_init(&sc->sc_slock, "vmmslk");
	sc->sc_status = VMM_ACTIVE;
	refcnt_init(&sc->sc_refcnt);

	sc->vcpu_ct = 0;
	sc->vcpu_max = VMM_MAX_VCPUS;
	sc->vm_ct = 0;
	sc->vm_idx = 0;

	SLIST_INIT(&sc->vm_list);
	rw_init(&sc->vm_lock, "vm_list");

	pool_init(&vm_pool, sizeof(struct vm), 0, IPL_MPFLOOR, PR_WAITOK,
	    "vmpool", NULL);
	pool_init(&vcpu_pool, sizeof(struct vcpu), 64, IPL_MPFLOOR, PR_WAITOK,
	    "vcpupl", NULL);

	vmm_attach_machdep(parent, self, aux);

	vmm_softc = sc;
	printf("\n");
}

int
vmm_activate(struct device *self, int act)
{
	switch (act) {
	case DVACT_QUIESCE:
		/* Block device users as we're suspending operation. */
		rw_enter_write(&vmm_softc->sc_slock);
		KASSERT(vmm_softc->sc_status == VMM_ACTIVE);
		vmm_softc->sc_status = VMM_SUSPENDED;
		rw_exit_write(&vmm_softc->sc_slock);

		/* Wait for any device users to finish. */
		refcnt_finalize(&vmm_softc->sc_refcnt, "vmmsusp");

		vmm_activate_machdep(self, act);
		break;
	case DVACT_WAKEUP:
		vmm_activate_machdep(self, act);

		/* Set the device back to active. */
		rw_enter_write(&vmm_softc->sc_slock);
		KASSERT(vmm_softc->sc_status == VMM_SUSPENDED);
		refcnt_init(&vmm_softc->sc_refcnt);
		vmm_softc->sc_status = VMM_ACTIVE;
		rw_exit_write(&vmm_softc->sc_slock);

		/* Notify any waiting device users. */
		wakeup(&vmm_softc->sc_status);
		break;
	}

	return (0);
}

/*
 * vmmopen
 *
 * Called during open of /dev/vmm.
 *
 * Parameters:
 *  dev, flag, mode, p: These come from the character device and are
 *   all unused for this function
 *
 * Return values:
 *  ENODEV: if vmm(4) didn't attach or no supported CPUs detected
 *  0: successful open
 */
int
vmmopen(dev_t dev, int flag, int mode, struct proc *p)
{
	/* Don't allow open if we didn't attach */
	if (vmm_softc == NULL)
		return (ENODEV);

	/* Don't allow open if we didn't detect any supported CPUs */
	if (vmm_softc->mode == VMM_MODE_UNKNOWN)
		return (ENODEV);

	return 0;
}

/*
 * vmmclose
 *
 * Called when /dev/vmm is closed. Presently unused.
 */
int
vmmclose(dev_t dev, int flag, int mode, struct proc *p)
{
	return 0;
}

int
vm_find_file(int fd, struct proc *p, struct vm **res)
{
	struct file *fp;
	struct vm *vm = NULL;

	*res = NULL;

	if ((fp = fd_getfile(p->p_fd, fd)) == NULL)
		return (EBADF);

	if (fp->f_type != DTYPE_VMM) {
		FRELE(fp, p);
		return (EINVAL);
	}

	vm = (struct vm *)fp->f_data;
	refcnt_take(&vm->vm_refcnt);
	*res = vm;
	FRELE(fp, p);

	return (0);
}

/*
 * vmm_dev_enter
 *
 * Acquire a reference to the vmm softc instance, sleeping if it's not
 * currently active due to power management (i.e. suspend/resume).
 * If interruptable is zero, wait until a reference is acquired.
 */
int
vmm_dev_enter(int interruptable)
{
	int flags, priority, ret;

	flags = RW_READ;
	priority = PWAIT;
	if (interruptable) {
		flags |= RW_INTR;
		priority |= PCATCH;
	}

	ret = rw_enter(&vmm_softc->sc_slock, flags);
	if (ret != 0)
		return (ret);
	while (vmm_softc->sc_status != VMM_ACTIVE) {
		ret = rwsleep_nsec(&vmm_softc->sc_status, &vmm_softc->sc_slock,
		    priority, "vmmresume", INFSLP);
		if (ret != 0) {
			rw_exit(&vmm_softc->sc_slock);
			return (ret);
		}
	}
	refcnt_take(&vmm_softc->sc_refcnt);
	rw_exit(&vmm_softc->sc_slock);
	return (0);
}

/*
 * vmm_dev_exit
 *
 * Release a reference to the vmm softc, waking any waiters.
 */
void
vmm_dev_exit(void)
{
	refcnt_rele_wake(&vmm_softc->sc_refcnt);
}

/*
 * vmmioctl
 *
 * Control device ioctl dispatch for creating virtual machines.
 */
int
vmmioctl(dev_t dev, u_long cmd, caddr_t data, int flag, struct proc *p)
{
	int ret = ENOTTY;

	KERNEL_UNLOCK();

	ret = vmm_dev_enter(1);
	if (ret != 0)
		goto out;

	switch (cmd) {
	case VMM_IOC_CREATE:
		ret = vmm_start();
		if (ret) {
			vmm_stop();
			break;
		}
		ret = vm_create((struct vm_create_params *)data, p);
		if (ret)
			break;
		break;
	default:
		ret = ENOTTY;
		break;
	}

	vmm_dev_exit();
out:
	KERNEL_LOCK();

	return (ret);
}

/*
 * vm_find_vcpu
 *
 * Lookup VMM VCPU by ID number
 *
 * Parameters:
 *  vm: vm structure
 *  id: index id of vcpu
 *
 * Returns pointer to vcpu structure if successful, NULL otherwise
 */
struct vcpu *
vm_find_vcpu(struct vm *vm, uint32_t id)
{
	struct vcpu *vcpu;

	if (vm == NULL)
		return (NULL);

	SLIST_FOREACH(vcpu, &vm->vm_vcpu_list, vc_vcpu_link) {
		if (vcpu->vc_id == id)
			return (vcpu);
	}

	return (NULL);
}

/*
 * vm_create
 *
 * Creates the in-memory VMM structures for the VM defined by 'vcp'. The
 * parent of this VM shall be the process defined by 'p'.
 * This function does not start the VCPU(s) - see vm_start.
 *
 * Return Values:
 *  0: the create operation was successful
 *  ENOMEM: out of memory
 *  various other errors from vcpu_init/vm_impl_init
 */
int
vm_create(struct vm_create_params *vcp, struct proc *p)
{
	int i, ret = EINVAL;
	size_t memsize;
	struct file *fp;
	struct filedesc *fdp;
	struct vm *vm;
	struct vcpu *vcpu;
	struct uvm_object *uao;
	struct vm_mem_range *vmr;
	unsigned int uvmflags = 0;

	memsize = vm_create_check_mem_ranges(vcp);
	if (memsize == 0)
		return (EINVAL);

	if (vcp->vcp_ncpus == 0 ||
	    vcp->vcp_ncpus > VMM_MAX_VCPUS_PER_VM)
		return (EINVAL);

	/*
	 * Increment global counts early to see if the capacity limits
	 * would be violated and prevent vmm(4) from disabling any
	 * virtualization extensions on the host while creating a vm.
	 */
	rw_enter_write(&vmm_softc->vm_lock);
	if (vmm_softc->vcpu_ct + vcp->vcp_ncpus > vmm_softc->vcpu_max) {
		DPRINTF("%s: maximum vcpus (%lu) reached\n", __func__,
		    vmm_softc->vcpu_max);
		rw_exit_write(&vmm_softc->vm_lock);
		return (ENOMEM);
	}
	vmm_softc->vcpu_ct += vcp->vcp_ncpus;
	vmm_softc->vm_ct++;
	rw_exit_write(&vmm_softc->vm_lock);

	/* Instantiate and configure the new vm. */
	vm = pool_get(&vm_pool, PR_WAITOK | PR_ZERO);

	/* Create the VM's identity. */
	vm->vm_creator_pid = p->p_p->ps_pid;
	strncpy(vm->vm_name, vcp->vcp_name, VMM_MAX_NAME_LEN - 1);

	/* Create the pmap for nested paging. */
	vm->vm_pmap = pmap_create();

	/* Initialize memory slots. */
	vm->vm_nmemranges = vcp->vcp_nmemranges;
	memcpy(vm->vm_memranges, vcp->vcp_memranges,
	    vm->vm_nmemranges * sizeof(vm->vm_memranges[0]));
	vm->vm_memory_size = memsize; /* Calculated above. */

	uvmflags = UVM_MAPFLAG(PROT_READ | PROT_WRITE, PROT_READ | PROT_WRITE,
	    MAP_INHERIT_NONE, MADV_NORMAL, UVM_FLAG_CONCEAL);
	for (i = 0; i < vm->vm_nmemranges; i++) {
		vmr = &vm->vm_memranges[i];
		if (vmr->vmr_type == VM_MEM_MMIO)
			continue;

		uao = NULL;
		uao = uao_create(vmr->vmr_size, UAO_FLAG_CANFAIL);
		if (uao == NULL) {
			printf("%s: failed to initialize memory slot\n",
			    __func__);
			ret = ENOMEM;
			goto err;
		}

		/* Map the UVM aobj into the process. It owns this reference. */
		ret = uvm_map(&p->p_vmspace->vm_map, &vmr->vmr_va,
		    vmr->vmr_size, uao, 0, 0, uvmflags);
		if (ret) {
			printf("%s: uvm_map failed: %d\n", __func__, ret);
			uao_detach(uao);
			ret = ENOMEM;
			goto err;
		}

		/* Make this mapping immutable so userland cannot change it. */
		ret = uvm_map_immutable(&p->p_vmspace->vm_map, vmr->vmr_va,
		    vmr->vmr_va + vmr->vmr_size, 1);
		if (ret) {
			printf("%s: uvm_map_immutable failed: %d\n", __func__,
			    ret);
			uvm_unmap(&p->p_vmspace->vm_map, vmr->vmr_va,
			    vmr->vmr_va + vmr->vmr_size);
			goto err;
		}

		uao_reference(uao);	/* Take a reference for vmm. */
		vm->vm_memory_slot[i] = uao;
	}

	if (vm_impl_init(vm, p)) {
		printf("failed to init arch-specific features for vm %p\n", vm);
		ret = ENOMEM;
		goto err;
	}

	vm->vm_vcpu_ct = 0;

	/* Initialize each VCPU defined in 'vcp' */
	SLIST_INIT(&vm->vm_vcpu_list);
	for (i = 0; i < vcp->vcp_ncpus; i++) {
		vcpu = pool_get(&vcpu_pool, PR_WAITOK | PR_ZERO);

		vcpu->vc_parent = vm;
		vcpu->vc_id = vm->vm_vcpu_ct;
		vm->vm_vcpu_ct++;

		if ((ret = vcpu_init(vcpu, vcp)) != 0) {
			printf("failed to init vcpu %d for vm %p\n", i, vm);
			pool_put(&vcpu_pool, vcpu);
			goto err;
		}
		SLIST_INSERT_HEAD(&vm->vm_vcpu_list, vcpu, vc_vcpu_link);
	}

	/* Create our file. */
	fdp = p->p_fd;
	fdplock(fdp);

	ret = falloc(p, &fp, &vcp->vcp_fd);
	if (ret) {
		fdpunlock(fdp);
		goto err;
	}
	fp->f_flag = FREAD | FWRITE;
	fp->f_type = DTYPE_VMM;
	fp->f_data = vm;
	fp->f_ops = &vmops;

	/* Increment the global index and insert into the list. */
	rw_enter_write(&vmm_softc->vm_lock);
	vmm_softc->vm_idx++;
	vm->vm_id = vmm_softc->vm_idx;

	refcnt_init(&vm->vm_refcnt);
	SLIST_INSERT_HEAD(&vmm_softc->vm_list, vm, vm_link);
	rw_exit_write(&vmm_softc->vm_lock);

	/* Update the userland process's view of guest memory. */
	memcpy(vcp->vcp_memranges, vm->vm_memranges,
	    vcp->vcp_nmemranges * sizeof(vcp->vcp_memranges[0]));

	/* Publish the file descriptor. */
	refcnt_take(&vm->vm_refcnt);
	fdinsert(fdp, vcp->vcp_fd, 0, fp);
	FRELE(fp, p);
	fdpunlock(fdp);

	return (0);

err:
	vm_teardown(&vm);
	rw_enter_write(&vmm_softc->vm_lock);
	vmm_softc->vm_ct--;
	vmm_softc->vcpu_ct -= vcp->vcp_ncpus;
	if (vmm_softc->vm_ct < 1)
		vmm_stop();
	rw_exit_write(&vmm_softc->vm_lock);
	return (ret);
}

/*
 * vm_create_check_mem_ranges
 *
 * Make sure that the guest physical memory ranges given by the user process
 * do not overlap and are in ascending order.
 *
 * The last physical address may not exceed VMM_MAX_VM_MEM_SIZE.
 *
 * Return Values:
 *   The total memory size in bytes if the checks were successful
 *   0: One of the memory ranges was invalid or VMM_MAX_VM_MEM_SIZE was
 *   exceeded
 */
size_t
vm_create_check_mem_ranges(struct vm_create_params *vcp)
{
	size_t i, memsize = 0;
	struct vm_mem_range *vmr, *pvmr;
	const paddr_t maxgpa = VMM_MAX_VM_MEM_SIZE;

	if (vcp->vcp_nmemranges == 0 ||
	    vcp->vcp_nmemranges > VMM_MAX_MEM_RANGES) {
		DPRINTF("invalid number of guest memory ranges\n");
		return (0);
	}

	for (i = 0; i < vcp->vcp_nmemranges; i++) {
		vmr = &vcp->vcp_memranges[i];

		/* Only page-aligned addresses and sizes are permitted */
		if ((vmr->vmr_gpa & PAGE_MASK) || (vmr->vmr_va & PAGE_MASK) ||
		    (vmr->vmr_size & PAGE_MASK) || vmr->vmr_size == 0) {
			DPRINTF("memory range %zu is not page aligned\n", i);
			return (0);
		}

		/* Make sure that VMM_MAX_VM_MEM_SIZE is not exceeded */
		if (vmr->vmr_gpa >= maxgpa ||
		    vmr->vmr_size > maxgpa - vmr->vmr_gpa) {
			DPRINTF("exceeded max memory size\n");
			return (0);
		}

		/*
		 * Make sure that guest physical memory ranges do not overlap
		 * and that they are ascending.
		 */
		if (i > 0 && pvmr->vmr_gpa + pvmr->vmr_size > vmr->vmr_gpa) {
			DPRINTF("guest range %zu overlaps or !ascending\n", i);
			return (0);
		}

		/*
		 * No memory is mappable in MMIO ranges, so don't count towards
		 * the total guest memory size.
		 */
		if (vmr->vmr_type != VM_MEM_MMIO)
			memsize += vmr->vmr_size;
		pvmr = vmr;
	}

	return (memsize);
}

/*
 * vm_teardown
 *
 * Tears down (destroys) the vm indicated by 'vm'.
 *
 * Assumes the vm is already removed from the global vm list (or was never
 * added).
 *
 * Parameters:
 *  vm: vm to be torn down
 */
void
vm_teardown(struct vm **target)
{
	size_t i, nvcpu = 0;
	vaddr_t sva, eva;
	struct vcpu *vcpu, *tmp;
	struct vm *vm = *target;
	struct uvm_object *uao;

	KERNEL_ASSERT_UNLOCKED();

	/* Free VCPUs */
	SLIST_FOREACH_SAFE(vcpu, &vm->vm_vcpu_list, vc_vcpu_link, tmp) {
		SLIST_REMOVE(&vm->vm_vcpu_list, vcpu, vcpu, vc_vcpu_link);
		vcpu_deinit(vcpu);
		pool_put(&vcpu_pool, vcpu);
		nvcpu++;
	}

	/* Remove guest mappings from our nested page tables. */
	for (i = 0; i < vm->vm_nmemranges; i++) {
		sva = vm->vm_memranges[i].vmr_gpa;
		eva = sva + vm->vm_memranges[i].vmr_size - 1;
		pmap_remove(vm->vm_pmap, sva, eva);
	}

	/* Release UVM anon objects backing our guest memory. */
	for (i = 0; i < vm->vm_nmemranges; i++) {
		uao = vm->vm_memory_slot[i];
		vm->vm_memory_slot[i] = NULL;
		if (uao != NULL)
			uao_detach(uao);
	}

	/* At this point, no UVM-managed pages should reference our pmap. */
	pmap_destroy(vm->vm_pmap);
	vm->vm_pmap = NULL;

	pool_put(&vm_pool, vm);
	*target = NULL;
}

void
vm_request_stop(struct vm *vm)
{
	struct vcpu *vcpu;
	u_int old;
#ifdef MULTIPROCESSOR
	struct cpu_info *ci;
#endif

	SLIST_FOREACH(vcpu, &vm->vm_vcpu_list, vc_vcpu_link) {
		do {
			old = atomic_load_int(&vcpu->vc_state);
			if (old == VCPU_STATE_REQTERM ||
			    old == VCPU_STATE_TERMINATED)
				break;
		} while (atomic_cas_uint(&vcpu->vc_state, old,
		    VCPU_STATE_REQTERM) != old);

#ifdef MULTIPROCESSOR
		/*
		 * If this vCPU is currently running in guest mode, nudge the
		 * host CPU so it exits promptly and observes REQTERM.
		 */
		if (old != VCPU_STATE_TERMINATED) {
			ci = READ_ONCE(vcpu->vc_curcpu);
			if (ci != NULL)
				x86_send_ipi(ci, X86_IPI_NOP);
		}
#endif
	}
}

/*
 * vm_resetcpu
 *
 * Resets the vcpu defined in 'vrp' to power-on-init register state
 *
 * Parameters:
 *  vrp: ioctl structure defining the vcpu to reset (see vmmvar.h)
 *
 * Returns 0 if successful, or various error codes on failure:
 *  ENOENT if vrp describes an unknown vcpu for this VM
 *  EBUSY if the indicated VCPU is not stopped
 *  EIO if the indicated VCPU failed to reset
 */
int
vm_resetcpu(struct vm *vm, struct vm_resetcpu_params *vrp)
{
	struct vcpu *vcpu;
	int ret = 0;

	vcpu = vm_find_vcpu(vm, vrp->vrp_vcpu_id);

	if (vcpu == NULL) {
		DPRINTF("%s: vcpu id %u not found\n", __func__,
		    vrp->vrp_vcpu_id);
		ret = ENOENT;
		goto out;
	}

	rw_enter_write(&vcpu->vc_lock);
	if (atomic_load_int(&vcpu->vc_state) != VCPU_STATE_STOPPED)
		ret = EBUSY;
	else {
		if (vcpu_reset_regs(vcpu, &vrp->vrp_init_state)) {
			printf("%s: failed\n", __func__);
#ifdef VMM_DEBUG
			dump_vcpu(vcpu);
#endif /* VMM_DEBUG */
			ret = EIO;
		}
	}
	rw_exit_write(&vcpu->vc_lock);
out:
	return (ret);
}

/*
 * vcpu_must_yield
 *
 * Check if we need to (temporarily) stop running the VCPU for some reason,
 * such as:
 * - the VM was requested to terminate
 * - the proc running this VCPU has pending signals
 * - the scheduler asks us to yield
 *
 * Parameters:
 *  vcpu: the VCPU to check
 *
 * Return values:
 *  1: the VM owning this VCPU should stop
 *  0: no stop is needed
 */
int
vcpu_must_yield(struct vcpu *vcpu)
{
	struct cpu_info *ci;

	if (atomic_load_int(&vcpu->vc_state) == VCPU_STATE_REQTERM)
		return (1);

	if (SIGPENDING(curproc) != 0)
		return (1);

	ci = curcpu();
	if (ci->ci_schedstate.spc_schedflags & SPCF_SHOULDYIELD)
		return (1);

	return (0);
}

/*
 * vm_share_mem
 *
 * Share a uvm mapping for the vm guest memory ranges into the calling process.
 *
 * Return values:
 *  0: if successful
 *  other errno on uvm_map or uvm_map_immutable failures
 */
int
vm_share_mem(struct vm *vm, struct vm_sharemem_params *vsp, struct proc *p)
{
	int ret = EINVAL, unmap = 0;
	size_t i, failed_uao = 0;
	struct vm_mem_range *vmr;
	struct uvm_object *uao;
	unsigned int uvmflags;

	/* Share each UVM aobj with the calling process. */
	uvmflags = UVM_MAPFLAG(PROT_READ | PROT_WRITE, PROT_READ | PROT_WRITE,
	    MAP_INHERIT_NONE, MADV_NORMAL, UVM_FLAG_CONCEAL);
	for (i = 0; i < vm->vm_nmemranges; i++) {
		vmr = &vm->vm_memranges[i];
		if (vmr->vmr_type == VM_MEM_MMIO)
			continue;

		uao = vm->vm_memory_slot[i];
		KASSERT(uao != NULL);
		ret = uvm_map(&p->p_p->ps_vmspace->vm_map, &vsp->vsp_va[i],
		    vmr->vmr_size, uao, 0, 0, uvmflags);
		if (ret) {
			printf("%s: uvm_map failed: %d\n", __func__, ret);
			unmap = (i > 0) ? 1 : 0;
			failed_uao = i;
			goto out;
		}
		uao_reference(uao);	/* Add a reference for the process. */

		ret = uvm_map_immutable(&p->p_p->ps_vmspace->vm_map,
		    vsp->vsp_va[i], vsp->vsp_va[i] + vmr->vmr_size, 1);
		if (ret) {
			printf("%s: uvm_map_immutable failed: %d\n",
			    __func__, ret);
			unmap = 1;
			failed_uao = i + 1;
			goto out;
		}
	}
	ret = 0;
out:
	if (unmap) {
		/* Unmap mapped aobjs, which drops the process's reference. */
		for (i = 0; i < failed_uao; i++) {
			vmr = &vm->vm_memranges[i];
			uvm_unmap(&p->p_p->ps_vmspace->vm_map,
			    vsp->vsp_va[i], vsp->vsp_va[i] + vmr->vmr_size);
		}
	}
	return (ret);
}

int
vm_read(struct file *fp, struct uio *uio, int fflags)
{
	return (ENXIO);
}

int
vm_write(struct file *fp, struct uio *uio, int fflags)
{
	return (ENXIO);
}

int
vm_kqfilter(struct file *fp, struct knote *kn)
{
	return (EINVAL);
}

/*
 * vm_ioctl
 *
 * Dispatcher for all virtual machine operations for the vm referenced
 * by the file fp.
 */
int
vm_ioctl(struct file *fp, u_long cmd, caddr_t data, struct proc *p)
{
	struct vm *vm = (struct vm *)fp->f_data;
	int ret = 0;

	if (vm == NULL)
		return (ENXIO);

	KERNEL_ASSERT_UNLOCKED();

	refcnt_take(&vm->vm_refcnt);
	ret = vmm_dev_enter(1);
	if (ret != 0)
		goto out;

	if (atomic_load_int(&vm->vm_dying) != VMM_VM_ALIVE) {
		if (cmd == VMM_IOC_RUN) {
			((struct vm_run_params *)data)->vrp_exit_reason =
			    VM_EXIT_TERMINATED;
			ret = 0;
		} else {
			ret = EBUSY;
		}
		goto out_active;
	}

	switch (cmd) {
	case VMM_IOC_RUN:
		ret = vm_run(vm, (struct vm_run_params *)data);
		break;
	case VMM_IOC_RESETCPU:
		ret = vm_resetcpu(vm, (struct vm_resetcpu_params *)data);
		break;
	case VMM_IOC_READREGS:
		ret = vm_rwregs(vm, (struct vm_rwregs_params *)data, 0);
		break;
	case VMM_IOC_WRITEREGS:
		ret = vm_rwregs(vm, (struct vm_rwregs_params *)data, 1);
		break;
	case VMM_IOC_READVMPARAMS:
		ret = vm_rwvmparams(vm, (struct vm_rwvmparams_params *)data, 0);
		break;
	case VMM_IOC_WRITEVMPARAMS:
		ret = vm_rwvmparams(vm, (struct vm_rwvmparams_params *)data, 1);
		break;
	case VMM_IOC_SHAREMEM:
		ret = vm_share_mem(vm, (struct vm_sharemem_params *)data, p);
		break;
	case VMM_IOC_INTR:
		ret = vm_intr_pending(vm, (struct vm_intr_params *)data);
		break;
	default:
		ret = ENOTTY;
		break;
	}

out_active:
	vmm_dev_exit();
out:
	vm_rele(vm);
	return (ret);
}

int
vm_rele(struct vm *vm)
{
	int nvcpu;

	/*
	 * May sleep if we drop the last reference and teardown,
	 * so confirm caller doesn't hold the big lock.
	 */
	KERNEL_ASSERT_UNLOCKED();

	if (refcnt_rele(&vm->vm_refcnt) == 0)
		return (0);

	nvcpu = vm->vm_vcpu_ct;

	vm_teardown(&vm);

	/* Update global accounting. */
	rw_enter_write(&vmm_softc->vm_lock);
	vmm_softc->vm_ct--;
	vmm_softc->vcpu_ct -= nvcpu;
	if (vmm_softc->vm_ct < 1)
		vmm_stop();
	rw_exit_write(&vmm_softc->vm_lock);

	return (1);
}

int
vm_close(struct file *fp, struct proc *p)
{
	struct vm *vm = (struct vm *)fp->f_data;
#ifdef MULTIPROCESSOR
	int relock;
#endif

	if (vm == NULL)
		return (0);

	/*
	 * vm_close is called from multiple contexts within the kernel,
	 * inside and outside of vmm(4). Some callers hold the kernel lock.
	 * Since vmm(4) operates without the kernel lock, we need to
	 * unlock and relock before return.
	 */
#ifdef MULTIPROCESSOR
	relock = _kernel_lock_held();
	if (relock)
		KERNEL_UNLOCK();
#endif

	vmm_dev_enter(0);

	atomic_swap_uint(&vm->vm_dying, VMM_VM_TEARDOWN);
	vm_request_stop(vm);

	/* Remove the vm from the global vm list. */
	rw_enter_write(&vmm_softc->vm_lock);
	SLIST_REMOVE(&vmm_softc->vm_list, vm, vm, vm_link);
	rw_exit_write(&vmm_softc->vm_lock);
	vm_rele(vm);

	/* Drop the file's reference. */
	vm_rele(vm);

	vmm_dev_exit();

#ifdef MULTIPROCESSOR
	if (relock)
		KERNEL_LOCK();
#endif
	return (0);
}

int
vm_stat(struct file *fp, struct stat *st, struct proc *p)
{
	struct vm *vm = (struct vm *)fp->f_data;

	if (vm == NULL)
		return (0);

	memset(st, 0, sizeof(*st));
	st->st_mode = S_IFCHR;
	st->st_blksize = PAGE_SIZE;
	st->st_blocks = pmap_resident_count(vm->vm_pmap);

	return (0);
}
