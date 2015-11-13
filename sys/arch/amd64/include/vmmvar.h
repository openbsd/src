/*
 * Copyright (c) 2014 Mike Larkin <mlarkin@openbsd.org>
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

/*
 * CPU capabilities for VMM operation
 */
#ifndef _MACHINE_VMMVAR_H_
#define _MACHINE_VMMVAR_H_

#include <sys/rwlock.h>

#define VMM_HV_SIGNATURE 	"OpenBSDVMM58"

#define VMM_MAX_DISKS_PER_VM	2
#define VMM_MAX_PATH_DISK	128
#define VMM_MAX_NAME_LEN	32
#define VMM_MAX_KERNEL_PATH	128
#define VMM_MAX_VCPUS_PER_VM	64
#define VMM_MAX_VM_MEM_SIZE	(512 * 1024)
#define VMM_MAX_NICS_PER_VM	2

#define VMM_PCI_MMIO_BAR_BASE	0xF0000000
#define VMM_PCI_MMIO_BAR_END	0xF0FFFFFF
#define VMM_PCI_MMIO_BAR_SIZE	0x00010000
#define VMM_PCI_IO_BAR_BASE	0x1000
#define VMM_PCI_IO_BAR_END	0xFFFF
#define VMM_PCI_IO_BAR_SIZE	0x1000

/* VMX: Basic Exit Reasons */
#define VMX_EXIT_NMI				0
#define VMX_EXIT_EXTINT				1
#define VMX_EXIT_TRIPLE_FAULT			2
#define VMX_EXIT_INIT				3
#define VMX_EXIT_SIPI				4
#define VMX_EXIT_IO_SMI				5
#define VMX_EXIT_OTHER_SMI			6
#define VMX_EXIT_INT_WINDOW			7
#define VMX_EXIT_NMI_WINDOW			8
#define VMX_EXIT_TASK_SWITCH			9
#define VMX_EXIT_CPUID				10
#define VMX_EXIT_GETSEC				11
#define VMX_EXIT_HLT				12
#define VMX_EXIT_INVD				13
#define VMX_EXIT_INVLPG				14
#define VMX_EXIT_RDPMC				15
#define VMX_EXIT_RDTSC				16
#define VMX_EXIT_RSM				17
#define VMX_EXIT_VMCALL				18
#define VMX_EXIT_VMCLEAR			19
#define VMX_EXIT_VMLAUNCH			20
#define VMX_EXIT_VMPTRLD			21
#define VMX_EXIT_VMPTRST			22
#define VMX_EXIT_VMREAD				23
#define VMX_EXIT_VMRESUME			24
#define VMX_EXIT_VMWRITE			25
#define VMX_EXIT_VMXOFF				26
#define VMX_EXIT_VMXON				27
#define VMX_EXIT_CR_ACCESS			28
#define VMX_EXIT_MOV_DR				29
#define VMX_EXIT_IO				30
#define VMX_EXIT_RDMSR				31
#define VMX_EXIT_WRMSR				32
#define VMX_EXIT_ENTRY_FAILED_GUEST_STATE	33
#define VMX_EXIT_ENTRY_FAILED_MSR_LOAD		34
#define VMX_EXIT_MWAIT				36
#define VMX_EXIT_MTF				37
#define VMX_EXIT_MONITOR			39
#define VMX_EXIT_PAUSE				40
#define VMX_EXIT_ENTRY_FAILED_MCE		41
#define VMX_EXIT_TPR_BELOW_THRESHOLD		43
#define VMX_EXIT_APIC_ACCESS			44
#define VMX_EXIT_VIRTUALIZED_EOI		45
#define VMX_EXIT_GDTR_IDTR			46
#define	VMX_EXIT_LDTR_TR			47
#define VMX_EXIT_EPT_VIOLATION			48
#define VMX_EXIT_EPT_MISCONFIGURATION		49
#define VMX_EXIT_INVEPT				50
#define VMX_EXIT_RDTSCP				51
#define VMX_EXIT_VMX_PREEMPTION_TIMER_EXPIRED	52
#define VMX_EXIT_INVVPID			53
#define VMX_EXIT_WBINVD				54
#define VMX_EXIT_XSETBV				55
#define VMX_EXIT_APIC_WRITE			56
#define VMX_EXIT_RDRAND				57
#define VMX_EXIT_INVPCID			58
#define VMX_EXIT_VMFUNC				59

#define VM_EXIT_TERMINATED			0xFFFE
#define VM_EXIT_NONE				0xFFFF

enum {
	VCPU_STATE_STOPPED,
	VCPU_STATE_RUNNING,
	VCPU_STATE_REQSTOP,
	VCPU_STATE_UNKNOWN
};

enum {
	VEI_DIR_OUT,
	VEI_DIR_IN
};


/*
 * vm exit data
 *  vm_exit_inout		: describes an IN/OUT exit
 */
struct vm_exit_inout {
	uint8_t			vei_size;	/* Size of access */
	uint8_t			vei_dir;	/* Direction */
	uint8_t			vei_rep;	/* REP prefix? */
	uint8_t			vei_string;	/* string variety? */
	uint8_t			vei_encoding;	/* operand encoding */
	uint16_t		vei_port;	/* port */
	uint32_t		vei_data;	/* data (for IN insns) */
};

union vm_exit {
	struct vm_exit_inout	vei;		/* IN/OUT exit */
};


struct vm_create_params {
	/* Input parameters to VMM_IOC_CREATE */
	size_t		vcp_memory_size;
	size_t		vcp_ncpus;
	size_t		vcp_ndisks;
	size_t		vcp_nnics;
	char		vcp_disks[VMM_MAX_DISKS_PER_VM][VMM_MAX_PATH_DISK];
	char		vcp_name[VMM_MAX_NAME_LEN];
	char		vcp_kernel[VMM_MAX_KERNEL_PATH];
	uint8_t		vcp_macs[VMM_MAX_NICS_PER_VM][6];

	/* Output parameter from VMM_IOC_CREATE */
	uint32_t	vcp_id;
};

struct vm_run_params {
	/* Input parameters to VMM_IOC_RUN */
	uint32_t	vrp_vm_id;
	uint32_t	vrp_vcpu_id;
	uint8_t		vrp_continue;		/* Continuing from an exit */
	int16_t		vrp_injint;		/* Injected interrupt vector */

	/* Input/output parameter to VMM_IOC_RUN */
	union vm_exit	*vrp_exit;		/* updated exit data */

	/* Output parameter from VMM_IOC_RUN */
	uint16_t	vrp_exit_reason;	/* exit reason */
};

struct vm_info_result {
	/* Output parameters from VMM_IOC_INFO */
	size_t		vir_memory_size;
	size_t		vir_ncpus;
	uint8_t		vir_vcpu_state[VMM_MAX_VCPUS_PER_VM];
	pid_t		vir_creator_pid;
	uint32_t	vir_id;
	char		vir_name[VMM_MAX_NAME_LEN];
};

struct vm_info_params {
	/* Input parameters to VMM_IOC_INFO */
	size_t			vip_size;	/* Output buffer size */

	/* Output Parameters from VMM_IOC_INFO */
	size_t			 vip_info_ct;	/* # of entries returned */
	struct vm_info_result	*vip_info;	/* Output buffer */
};

struct vm_terminate_params {
	/* Input parameters to VMM_IOC_TERM */
	uint32_t		vtp_vm_id;
};

struct vm_writepage_params {
	/* Input parameters to VMM_IOC_WRITEPAGE */
	uint32_t		vwp_vm_id; /* VM ID */
	paddr_t			vwp_paddr; /* Phys Addr */
	char			*vwp_data; /* Page Data */
	uint32_t		vwp_len;   /* Length */
};

struct vm_readpage_params {
	/* Input parameters to VMM_IOC_READPAGE */
	uint32_t		vrp_vm_id; /* VM ID */
	paddr_t			vrp_paddr; /* Phys Addr */
	uint32_t		vrp_len;   /* Length */

	/* Output parameters from VMM_IOC_READPAGE */
	char			*vrp_data; /* Page Data */
};

/* IOCTL definitions */
#define VMM_IOC_START _IO('V', 1)	/* Start virtualization */
#define VMM_IOC_STOP _IO('V', 2)	/* Stop virtualization */
#define VMM_IOC_CREATE _IOWR('V', 3, struct vm_create_params) /* Create VM */
#define VMM_IOC_RUN _IOWR('V', 4, struct vm_run_params) /* Run VCPU */
#define VMM_IOC_INFO _IOWR('V', 5, struct vm_info_params) /* Get VM Info */
#define VMM_IOC_TERM _IOW('V', 6, struct vm_terminate_params) /* Terminate VM */
#define VMM_IOC_WRITEPAGE _IOW('V', 7, struct vm_writepage_params) /* Wr Pg */
#define VMM_IOC_READPAGE _IOW('V', 8, struct vm_readpage_params) /* Rd Pg */

#ifdef _KERNEL

#include <uvm/uvm_extern.h>

#define VMX_FAIL_LAUNCH_UNKNOWN 1
#define VMX_FAIL_LAUNCH_INVALID_VMCS 2
#define VMX_FAIL_LAUNCH_VALID_VMCS 3

#ifdef VMM_DEBUG
#define dprintf(x...)	do { if (vmm_debug) printf(x); } while(0)
#else
#define dprintf(x...)
#endif /* VMM_DEBUG */

enum {
	VMM_MODE_UNKNOWN,
	VMM_MODE_VMX,
	VMM_MODE_EPT,
	VMM_MODE_SVM,
	VMM_MODE_RVI
};

enum {
	VMM_MEM_TYPE_REGULAR,
	VMM_MEM_TYPE_UNKNOWN	
};

/* Forward declarations */
struct vm;

/*
 * Implementation-specific cpu state
 */
struct vmcb {
};

struct vmcs {
	uint32_t	vmcs_revision;
};

struct vmx_invvpid_descriptor
{
	uint64_t	vid_vpid; // : 16;
	uint64_t	vid_addr;
};

struct vmx_invept_descriptor
{
	uint64_t	vid_eptp;
	uint64_t	vid_reserved;
};

struct vmx_msr_store
{
	uint64_t	vms_index : 32;
	uint64_t	vms_data;
};

/*
 * Storage for guest registers not preserved in VMCS and various exit
 * information.
 *
 * Note that vmx_enter_guest depends on the layout of this struct for
 * field access.
 */
struct vmx_gueststate
{
	/* %rsi should be first */
	uint64_t	vg_rsi;			/* 0x00 */
	uint64_t	vg_rax;			/* 0x08 */
	uint64_t	vg_rbx;			/* 0x10 */
	uint64_t	vg_rcx;			/* 0x18 */
	uint64_t	vg_rdx;			/* 0x20 */
	uint64_t	vg_rdi;			/* 0x28 */
	uint64_t	vg_rbp;			/* 0x30 */
	uint64_t	vg_r8;			/* 0x38 */
	uint64_t	vg_r9;			/* 0x40 */
	uint64_t	vg_r10;			/* 0x48 */
	uint64_t	vg_r11;			/* 0x50 */
	uint64_t	vg_r12;			/* 0x58 */
	uint64_t	vg_r13;			/* 0x60 */
	uint64_t	vg_r14;			/* 0x68 */
	uint64_t	vg_r15;			/* 0x70 */
	uint64_t	vg_cr2;			/* 0x78 */
	uint64_t	vg_rip;			/* 0x80 */
	uint32_t	vg_exit_reason;		/* 0x88 */
};

/*
 * Virtual CPU
 */
struct vcpu {
	/* VMCS / VMCB pointer */
	vaddr_t vc_control_va;
	uint64_t vc_control_pa;

	/* VLAPIC pointer */
	vaddr_t vc_vlapic_va;
	uint64_t vc_vlapic_pa;

	/* MSR bitmap address */
	vaddr_t vc_msr_bitmap_va;
	uint64_t vc_msr_bitmap_pa;

	struct vm *vc_parent;
	uint32_t vc_id;
	SLIST_ENTRY(vcpu) vc_vcpu_link;
	vaddr_t vc_hsa_stack_va;

	uint8_t vc_virt_mode;
	uint8_t vc_state;

	struct cpu_info *vc_last_pcpu;
	union vm_exit vc_exit;

	/* VMX only */
	uint64_t vc_vmx_basic;
	uint64_t vc_vmx_entry_ctls;
	uint64_t vc_vmx_true_entry_ctls;
	uint64_t vc_vmx_exit_ctls;
	uint64_t vc_vmx_true_exit_ctls;
	uint64_t vc_vmx_pinbased_ctls;
	uint64_t vc_vmx_true_pinbased_ctls;
	uint64_t vc_vmx_procbased_ctls;
	uint64_t vc_vmx_true_procbased_ctls;
	uint64_t vc_vmx_procbased2_ctls;
	struct vmx_gueststate vc_gueststate;
	vaddr_t vc_vmx_msr_exit_save_va;
	paddr_t vc_vmx_msr_exit_save_pa;
	vaddr_t vc_vmx_msr_exit_load_va;
	paddr_t vc_vmx_msr_exit_load_pa;
	vaddr_t vc_vmx_msr_entry_load_va;
	paddr_t vc_vmx_msr_entry_load_pa;
};

SLIST_HEAD(vcpu_head, vcpu);

/*
 * Virtual Machine
 */
struct vm {
	vm_map_t		 vm_map;
	uint32_t		 vm_id;
	pid_t			 vm_creator_pid;
	uint32_t		 vm_memory_size;
	char			 vm_name[VMM_MAX_NAME_LEN];

	struct vcpu_head	 vm_vcpu_list;
	uint32_t		 vm_vcpu_ct;
	struct rwlock		 vm_vcpu_lock;

	SLIST_ENTRY(vm)		 vm_link;
};

void	vmm_dispatch_intr(vaddr_t);
int	vmxon(uint64_t *);
int	vmxoff(void);
int	vmclear(uint64_t *);
int	vmptrld(uint64_t *);
int	vmptrst(uint64_t *);
int	vmwrite(uint64_t, uint64_t);
int	vmread(uint64_t, uint64_t *);
void	invvpid(uint64_t, struct vmx_invvpid_descriptor *);
void	invept(uint64_t, struct vmx_invept_descriptor *);
int	vmx_enter_guest(uint64_t *, struct vmx_gueststate *, int);
void	start_vmm_on_cpu(struct cpu_info *);
void	stop_vmm_on_cpu(struct cpu_info *);

#endif /* _KERNEL */

#endif /* ! _MACHINE_VMMVAR_H_ */
