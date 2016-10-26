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

#define VMM_HV_SIGNATURE 	"OpenBSDVMM58"

#define VMM_MAX_MEM_RANGES	16
#define VMM_MAX_DISKS_PER_VM	2
#define VMM_MAX_PATH_DISK	128
#define VMM_MAX_NAME_LEN	32
#define VMM_MAX_KERNEL_PATH	128
#define VMM_MAX_VCPUS_PER_VM	64
#define VMM_MAX_VM_MEM_SIZE	2048
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
#define VMX_EXIT_RDSEED				61
#define VMX_EXIT_XSAVES				63
#define VMX_EXIT_XRSTORS			64

/*
 * VMX: Misc defines
 */
#define VMX_MAX_CR3_TARGETS			256

#define VM_EXIT_TERMINATED			0xFFFE
#define VM_EXIT_NONE				0xFFFF

/*
 * VCPU state values. Note that there is a conversion function in vmm.c
 * (vcpu_state_decode) that converts these to human readable strings,
 * so this enum and vcpu_state_decode should be kept in sync.
 */
enum {
	VCPU_STATE_STOPPED,
	VCPU_STATE_RUNNING,
	VCPU_STATE_REQTERM,
	VCPU_STATE_TERMINATED,
	VCPU_STATE_UNKNOWN,
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

/*
 * struct vcpu_segment_info describes a segment + selector set, used
 * in constructing the initial vcpu register content
 */
struct vcpu_segment_info {
	uint16_t vsi_sel;
	uint32_t vsi_limit;
	uint32_t vsi_ar;
	uint32_t vsi_base;
};

#define VCPU_REGS_EAX		0
#define VCPU_REGS_EBX		1
#define VCPU_REGS_ECX		2
#define VCPU_REGS_EDX		3
#define VCPU_REGS_ESI		4
#define VCPU_REGS_EDI		5
#define VCPU_REGS_ESP		6
#define VCPU_REGS_EBP		7
#define VCPU_REGS_EIP		8
#define VCPU_REGS_EFLAGS	9
#define VCPU_REGS_NGPRS		(VCPU_REGS_EFLAGS + 1)

#define VCPU_REGS_CR0	0
#define VCPU_REGS_CR2	1
#define VCPU_REGS_CR3	2
#define VCPU_REGS_CR4	3
#define VCPU_REGS_CR8	4
#define VCPU_REGS_NCRS	(VCPU_REGS_CR8 + 1)

#define VCPU_REGS_CS		0
#define VCPU_REGS_DS		1
#define VCPU_REGS_ES		2
#define VCPU_REGS_FS		3
#define VCPU_REGS_GS		4
#define VCPU_REGS_SS		5
#define VCPU_REGS_LDTR		6
#define VCPU_REGS_TR		7
#define VCPU_REGS_NSREGS	(VCPU_REGS_TR + 1)

struct vcpu_reg_state {
	uint32_t			vrs_gprs[VCPU_REGS_NGPRS];
	uint32_t			vrs_crs[VCPU_REGS_NCRS];
	struct vcpu_segment_info	vrs_sregs[VCPU_REGS_NSREGS];
	struct vcpu_segment_info	vrs_gdtr;
	struct vcpu_segment_info	vrs_idtr;
};

struct vm_mem_range {
	paddr_t	vmr_gpa;
	vaddr_t vmr_va;
	size_t	vmr_size;
};

struct vm_create_params {
	/* Input parameters to VMM_IOC_CREATE */
	size_t			vcp_nmemranges;
	size_t			vcp_ncpus;
	size_t			vcp_ndisks;
	size_t			vcp_nnics;
	struct vm_mem_range	vcp_memranges[VMM_MAX_MEM_RANGES];
	char			vcp_disks[VMM_MAX_DISKS_PER_VM][VMM_MAX_PATH_DISK];
	char			vcp_name[VMM_MAX_NAME_LEN];
	char			vcp_kernel[VMM_MAX_KERNEL_PATH];
	uint8_t			vcp_macs[VMM_MAX_NICS_PER_VM][6];

	/* Output parameter from VMM_IOC_CREATE */
	uint32_t	vcp_id;
};

struct vm_run_params {
	/* Input parameters to VMM_IOC_RUN */
	uint32_t	vrp_vm_id;
	uint32_t	vrp_vcpu_id;
	uint8_t		vrp_continue;		/* Continuing from an exit */
	uint16_t	vrp_irq;		/* IRQ to inject */

	/* Input/output parameter to VMM_IOC_RUN */
	union vm_exit	*vrp_exit;		/* updated exit data */

	/* Output parameter from VMM_IOC_RUN */
	uint16_t	vrp_exit_reason;	/* exit reason */
	uint8_t		vrp_irqready;		/* ready for IRQ on entry */
};

struct vm_info_result {
	/* Output parameters from VMM_IOC_INFO */
	size_t		vir_memory_size;
	size_t		vir_used_size;
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

struct vm_resetcpu_params {
	/* Input parameters to VMM_IOC_RESETCPU */
	uint32_t		vrp_vm_id;
	uint32_t		vrp_vcpu_id;
	struct vcpu_reg_state	vrp_init_state;
};

struct vm_intr_params {
	/* Input parameters to VMM_IOC_INTR */
	uint32_t		vip_vm_id;
	uint32_t		vip_vcpu_id;
	uint16_t		vip_intr;
};

#define VM_RWREGS_GPRS	0x1	/* read/write GPRs */
#define VM_RWREGS_SREGS	0x2	/* read/write segment registers */
#define VM_RWREGS_CRS	0x4	/* read/write CRs */
#define VM_RWREGS_ALL	(VM_RWREGS_GPRS | VM_RWREGS_SREGS | VM_RWREGS_CRS)

struct vm_rwregs_params {
	uint32_t		vrwp_vm_id;
	uint32_t		vrwp_vcpu_id;
	uint64_t		vrwp_mask;
	struct vcpu_reg_state	vrwp_regs;
};

/* IOCTL definitions */
#define VMM_IOC_CREATE _IOWR('V', 1, struct vm_create_params) /* Create VM */
#define VMM_IOC_RUN _IOWR('V', 2, struct vm_run_params) /* Run VCPU */
#define VMM_IOC_INFO _IOWR('V', 3, struct vm_info_params) /* Get VM Info */
#define VMM_IOC_TERM _IOW('V', 4, struct vm_terminate_params) /* Terminate VM */
#define VMM_IOC_RESETCPU _IOW('V', 5, struct vm_resetcpu_params) /* Reset */
#define VMM_IOC_INTR _IOW('V', 6, struct vm_intr_params) /* Intr pending */
#define VMM_IOC_READREGS _IOWR('V', 7, struct vm_rwregs_params) /* Get registers */
#define VMM_IOC_WRITEREGS _IOW('V', 8, struct vm_rwregs_params) /* Set registers */

#ifdef _KERNEL

#define VMX_FAIL_LAUNCH_UNKNOWN 1
#define VMX_FAIL_LAUNCH_INVALID_VMCS 2
#define VMX_FAIL_LAUNCH_VALID_VMCS 3

#define VMX_NUM_MSR_STORE 1

/* MSR bitmap manipulation macros */
#define MSRIDX(m) ((m) / 8)
#define MSRBIT(m) (1 << (m) % 8)

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
	uint64_t	vid_vpid;
	uint64_t	vid_addr;
};

struct vmx_invept_descriptor
{
	uint64_t	vid_eptp;
	uint64_t	vid_reserved;
};

struct vmx_msr_store
{
	uint64_t	vms_index;
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
	/* %esi should be first */
	uint32_t	vg_esi;			/* 0x00 */
	uint32_t	vg_eax;			/* 0x04 */
	uint32_t	vg_ebx;			/* 0x08 */
	uint32_t	vg_ecx;			/* 0x0c */
	uint32_t	vg_edx;			/* 0x10 */
	uint32_t	vg_edi;			/* 0x14 */
	uint32_t	vg_ebp;			/* 0x18 */
	uint32_t	vg_cr2;			/* 0x1c */
	uint32_t	vg_eip;			/* 0x20 */
	uint32_t	vg_exit_reason;		/* 0x24 */
	uint32_t	vg_eflags;		/* 0x28 */
};

/*
 * Virtual Machine
 */
struct vm;

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
	u_int vc_state;
	SLIST_ENTRY(vcpu) vc_vcpu_link;
	vaddr_t vc_hsa_stack_va;

	uint8_t vc_virt_mode;

	struct cpu_info *vc_last_pcpu;
	union vm_exit vc_exit;

	uint16_t vc_intr;
	uint8_t vc_irqready;

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

void	vmm_dispatch_intr(vaddr_t);
int	vmxon(uint64_t *);
int	vmxoff(void);
int	vmclear(uint64_t *);
int	vmptrld(uint64_t *);
int	vmptrst(uint64_t *);
int	vmwrite(uint32_t, uint32_t);
int	vmread(uint32_t, uint32_t *);
void	invvpid(uint32_t, struct vmx_invvpid_descriptor *);
void	invept(uint32_t, struct vmx_invept_descriptor *);
int	vmx_enter_guest(uint64_t *, struct vmx_gueststate *, int, vaddr_t);
void	start_vmm_on_cpu(struct cpu_info *);
void	stop_vmm_on_cpu(struct cpu_info *);

#endif /* _KERNEL */

#endif /* ! _MACHINE_VMMVAR_H_ */
