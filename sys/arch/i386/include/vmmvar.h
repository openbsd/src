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
#define VMM_MAX_DISKS_PER_VM	4
#define VMM_MAX_PATH_DISK	128
#define VMM_MAX_PATH_CDROM	128
#define VMM_MAX_NAME_LEN	32
#define VMM_MAX_KERNEL_PATH	128
#define VMM_MAX_VCPUS_PER_VM	64
#define VMM_MAX_VM_MEM_SIZE	3072
#define VMM_MAX_NICS_PER_VM	4

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
 * SVM: Intercept codes (exit reasons)
 */
#define SVM_VMEXIT_CR0_READ			0x00
#define SVM_VMEXIT_CR1_READ			0x01
#define SVM_VMEXIT_CR2_READ			0x02
#define SVM_VMEXIT_CR3_READ			0x03
#define SVM_VMEXIT_CR4_READ			0x04
#define SVM_VMEXIT_CR5_READ			0x05
#define SVM_VMEXIT_CR6_READ			0x06
#define SVM_VMEXIT_CR7_READ			0x07
#define SVM_VMEXIT_CR8_READ			0x08
#define SVM_VMEXIT_CR9_READ			0x09
#define SVM_VMEXIT_CR10_READ			0x0A
#define SVM_VMEXIT_CR11_READ			0x0B
#define SVM_VMEXIT_CR12_READ			0x0C
#define SVM_VMEXIT_CR13_READ			0x0D
#define SVM_VMEXIT_CR14_READ			0x0E
#define SVM_VMEXIT_CR15_READ			0x0F
#define SVM_VMEXIT_CR0_WRITE			0x10
#define SVM_VMEXIT_CR1_WRITE			0x11
#define SVM_VMEXIT_CR2_WRITE			0x12
#define SVM_VMEXIT_CR3_WRITE			0x13
#define SVM_VMEXIT_CR4_WRITE			0x14
#define SVM_VMEXIT_CR5_WRITE			0x15
#define SVM_VMEXIT_CR6_WRITE			0x16
#define SVM_VMEXIT_CR7_WRITE			0x17
#define SVM_VMEXIT_CR8_WRITE			0x18
#define SVM_VMEXIT_CR9_WRITE			0x19
#define SVM_VMEXIT_CR10_WRITE			0x1A
#define SVM_VMEXIT_CR11_WRITE			0x1B
#define SVM_VMEXIT_CR12_WRITE			0x1C
#define SVM_VMEXIT_CR13_WRITE			0x1D
#define SVM_VMEXIT_CR14_WRITE			0x1E
#define SVM_VMEXIT_CR15_WRITE			0x1F
#define SVM_VMEXIT_DR0_READ			0x20
#define SVM_VMEXIT_DR1_READ			0x21
#define SVM_VMEXIT_DR2_READ			0x22
#define SVM_VMEXIT_DR3_READ			0x23
#define SVM_VMEXIT_DR4_READ			0x24
#define SVM_VMEXIT_DR5_READ			0x25
#define SVM_VMEXIT_DR6_READ			0x26
#define SVM_VMEXIT_DR7_READ			0x27
#define SVM_VMEXIT_DR8_READ			0x28
#define SVM_VMEXIT_DR9_READ			0x29
#define SVM_VMEXIT_DR10_READ			0x2A
#define SVM_VMEXIT_DR11_READ			0x2B
#define SVM_VMEXIT_DR12_READ			0x2C
#define SVM_VMEXIT_DR13_READ			0x2D
#define SVM_VMEXIT_DR14_READ			0x2E
#define SVM_VMEXIT_DR15_READ			0x2F
#define SVM_VMEXIT_DR0_WRITE			0x30
#define SVM_VMEXIT_DR1_WRITE			0x31
#define SVM_VMEXIT_DR2_WRITE			0x32
#define SVM_VMEXIT_DR3_WRITE			0x33
#define SVM_VMEXIT_DR4_WRITE			0x34
#define SVM_VMEXIT_DR5_WRITE			0x35
#define SVM_VMEXIT_DR6_WRITE			0x36
#define SVM_VMEXIT_DR7_WRITE			0x37
#define SVM_VMEXIT_DR8_WRITE			0x38
#define SVM_VMEXIT_DR9_WRITE			0x39
#define SVM_VMEXIT_DR10_WRITE			0x3A
#define SVM_VMEXIT_DR11_WRITE			0x3B
#define SVM_VMEXIT_DR12_WRITE			0x3C
#define SVM_VMEXIT_DR13_WRITE			0x3D
#define SVM_VMEXIT_DR14_WRITE			0x3E
#define SVM_VMEXIT_DR15_WRITE			0x3F
#define SVM_VMEXIT_EXCP0			0x40
#define SVM_VMEXIT_EXCP1			0x41
#define SVM_VMEXIT_EXCP2			0x42
#define SVM_VMEXIT_EXCP3			0x43
#define SVM_VMEXIT_EXCP4			0x44
#define SVM_VMEXIT_EXCP5			0x45
#define SVM_VMEXIT_EXCP6			0x46
#define SVM_VMEXIT_EXCP7			0x47
#define SVM_VMEXIT_EXCP8			0x48
#define SVM_VMEXIT_EXCP9			0x49
#define SVM_VMEXIT_EXCP10			0x4A
#define SVM_VMEXIT_EXCP11			0x4B
#define SVM_VMEXIT_EXCP12			0x4C
#define SVM_VMEXIT_EXCP13			0x4D
#define SVM_VMEXIT_EXCP14			0x4E
#define SVM_VMEXIT_EXCP15			0x4F
#define SVM_VMEXIT_EXCP16			0x50
#define SVM_VMEXIT_EXCP17			0x51
#define SVM_VMEXIT_EXCP18			0x52
#define SVM_VMEXIT_EXCP19			0x53
#define SVM_VMEXIT_EXCP20			0x54
#define SVM_VMEXIT_EXCP21			0x55
#define SVM_VMEXIT_EXCP22			0x56
#define SVM_VMEXIT_EXCP23			0x57
#define SVM_VMEXIT_EXCP24			0x58
#define SVM_VMEXIT_EXCP25			0x59
#define SVM_VMEXIT_EXCP26			0x5A
#define SVM_VMEXIT_EXCP27			0x5B
#define SVM_VMEXIT_EXCP28			0x5C
#define SVM_VMEXIT_EXCP29			0x5D
#define SVM_VMEXIT_EXCP30			0x5E
#define SVM_VMEXIT_EXCP31			0x5F
#define SVM_VMEXIT_INTR				0x60
#define SVM_VMEXIT_NMI				0x61
#define SVM_VMEXIT_SMI				0x62
#define SVM_VMEXIT_INIT				0x63
#define SVM_VMEXIT_VINTR			0x64
#define SVM_VMEXIT_CR0_SEL_WRITE		0x65
#define SVM_VMEXIT_IDTR_READ			0x66
#define SVM_VMEXIT_GDTR_READ			0x67
#define SVM_VMEXIT_LDTR_READ			0x68
#define SVM_VMEXIT_TR_READ			0x69
#define SVM_VMEXIT_IDTR_WRITE			0x6A
#define SVM_VMEXIT_GDTR_WRITE			0x6B
#define SVM_VMEXIT_LDTR_WRITE			0x6C
#define SVM_VMEXIT_TR_WRITE			0x6D
#define SVM_VMEXIT_RDTSC			0x6E
#define SVM_VMEXIT_RDPMC			0x6F
#define SVM_VMEXIT_PUSHF			0x70
#define SVM_VMEXIT_POPF				0x71
#define SVM_VMEXIT_CPUID			0x72
#define SVM_VMEXIT_RSM				0x73
#define SVM_VMEXIT_IRET				0x74
#define SVM_VMEXIT_SWINT			0x75
#define SVM_VMEXIT_INVD				0x76
#define SVM_VMEXIT_PAUSE			0x77
#define SVM_VMEXIT_HLT				0x78
#define SVM_VMEXIT_INVLPG			0x79
#define SVM_VMEXIT_INVLPGA			0x7A
#define SVM_VMEXIT_IOIO				0x7B
#define SVM_VMEXIT_MSR				0x7C
#define SVM_VMEXIT_TASK_SWITCH			0x7D
#define SVM_VMEXIT_FERR_FREEZE			0x7E
#define SVM_VMEXIT_SHUTDOWN			0x7F
#define SVM_VMEXIT_VMRUN			0x80
#define SVM_VMEXIT_VMMCALL			0x81
#define SVM_VMEXIT_VMLOAD			0x82
#define SVM_VMEXIT_VMSAVE			0x83
#define SVM_VMEXIT_STGI				0x84
#define SVM_VMEXIT_CLGI				0x85
#define SVM_VMEXIT_SKINIT			0x86
#define SVM_VMEXIT_RDTSCP			0x87
#define SVM_VMEXIT_ICEBP			0x88
#define SVM_VMEXIT_WBINVD			0x89
#define SVM_VMEXIT_MONITOR			0x8A
#define SVM_VMEXIT_MWAIT			0x8B
#define SVM_VMEXIT_MWAIT_CONDITIONAL		0x8C
#define SVM_VMEXIT_NPF				0x400
#define SVM_VMEXIT_INVALID			-1

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

#define VCPU_REGS_CR0		0
#define VCPU_REGS_CR2		1
#define VCPU_REGS_CR3		2
#define VCPU_REGS_CR4		3
#define VCPU_REGS_CR8		4
#define VCPU_REGS_PDPTE0	5
#define VCPU_REGS_PDPTE1	6
#define VCPU_REGS_PDPTE2	7
#define VCPU_REGS_PDPTE3	8
#define VCPU_REGS_NCRS	(VCPU_REGS_PDPTE3 + 1)

#define VCPU_REGS_CS		0
#define VCPU_REGS_DS		1
#define VCPU_REGS_ES		2
#define VCPU_REGS_FS		3
#define VCPU_REGS_GS		4
#define VCPU_REGS_SS		5
#define VCPU_REGS_LDTR		6
#define VCPU_REGS_TR		7
#define VCPU_REGS_NSREGS	(VCPU_REGS_TR + 1)

#define VCPU_REGS_EFER		0
#define VCPU_REGS_NMSRS		(VCPU_REGS_EFER + 1)

struct vcpu_reg_state {
	uint32_t			vrs_gprs[VCPU_REGS_NGPRS];
	uint32_t			vrs_crs[VCPU_REGS_NCRS];
	uint32_t			vrs_msrs[VCPU_REGS_NMSRS];
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
	char			vcp_cdrom[VMM_MAX_PATH_CDROM];
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
#define VM_RWREGS_MSRS	0x8	/* read/write MSRs */
#define VM_RWREGS_ALL	(VM_RWREGS_GPRS | VM_RWREGS_SREGS | VM_RWREGS_CRS | \
    VM_RWREGS_MSRS)

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

/* CPUID masks */
/*
 * clone host capabilities minus:
 *  debug store (CPUIDECX_DTES64, CPUIDECX_DSCPL, CPUID_DS)
 *  monitor/mwait (CPUIDECX_MWAIT)
 *  vmx (CPUIDECX_VMX)
 *  smx (CPUIDECX_SMX)
 *  speedstep (CPUIDECX_EST)
 *  thermal (CPUIDECX_TM2, CPUID_ACPI, CPUID_TM)
 *  context id (CPUIDECX_CNXTID)
 *  silicon debug (CPUIDECX_SDBG)
 *  xTPR (CPUIDECX_XTPR)
 *  perf/debug (CPUIDECX_PDCM)
 *  pcid (CPUIDECX_PCID)
 *  direct cache access (CPUIDECX_DCA)
 *  x2APIC (CPUIDECX_X2APIC)
 *  apic deadline (CPUIDECX_DEADLINE)
 *  timestamp (CPUID_TSC)
 *  apic (CPUID_APIC)
 *  psn (CPUID_PSN)
 *  self snoop (CPUID_SS)
 *  hyperthreading (CPUID_HTT)
 *  pending break enabled (CPUID_PBE)
 *  MTRR (CPUID_MTRR)
 *  PAT (CPUID_PAT)
 */
#define VMM_CPUIDECX_MASK ~(CPUIDECX_EST | CPUIDECX_TM2 | \
    CPUIDECX_MWAIT | CPUIDECX_PDCM | CPUIDECX_VMX | CPUIDECX_DTES64 | \
    CPUIDECX_DSCPL | CPUIDECX_SMX | CPUIDECX_CNXTID | CPUIDECX_SDBG | \
    CPUIDECX_XTPR | CPUIDECX_PCID | CPUIDECX_DCA | CPUIDECX_X2APIC | \
    CPUIDECX_DEADLINE)
#define VMM_CPUIDEDX_MASK ~(CPUID_ACPI | CPUID_TM | CPUID_TSC | \
      CPUID_HTT | CPUID_DS | CPUID_APIC | CPUID_PSN | CPUID_SS | CPUID_PBE | \
      CPUID_MTRR | CPUID_PAT)


/*
 * SEFF flags - copy from host minus:
 *  SGX (SEFF0EBX_SGX)
 *  HLE (SEFF0EBX_HLE)
 *  INVPCID (SEFF0EBX_INVPCID)
 *  RTM (SEFF0EBX_RTM)
 *  PQM (SEFF0EBX_PQM)
 *  MPX (SEFF0EBX_MPX)
 *  PCOMMIT (SEFF0EBX_PCOMMIT)
 *  PT (SEFF0EBX_PT)
 */
#define VMM_SEFF0EBX_MASK ~(SEFF0EBX_SGX | SEFF0EBX_HLE | SEFF0EBX_INVPCID | \
    SEFF0EBX_RTM | SEFF0EBX_PQM | SEFF0EBX_MPX | \
    SEFF0EBX_PCOMMIT | SEFF0EBX_PT)
#define VMM_SEFF0ECX_MASK 0xFFFFFFFF

#ifdef _KERNEL

#define VMX_FAIL_LAUNCH_UNKNOWN 1
#define VMX_FAIL_LAUNCH_INVALID_VMCS 2
#define VMX_FAIL_LAUNCH_VALID_VMCS 3

#define VMX_NUM_MSR_STORE 1

/* MSR bitmap manipulation macros */
#define VMX_MSRIDX(m) ((m) / 8)
#define VMX_MSRBIT(m) (1 << (m) % 8)

#define SVM_MSRIDX(m) ((m) / 4)
#define SVM_MSRBIT_R(m) (1 << (((m) % 4) * 2))
#define SVM_MSRBIT_W(m) (1 << (((m) % 4) * 2 + 1))

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
struct vmcb_segment {
	uint16_t 			vs_sel;			/* 000h */
	uint16_t 			vs_attr;		/* 002h */
	uint32_t			vs_lim;			/* 004h */
	uint64_t			vs_base;		/* 008h */
};

struct vmcb {
	union {
		struct {
			uint32_t	v_cr_rw;		/* 000h */
			uint32_t	v_dr_rw;		/* 004h */
			uint32_t	v_excp;			/* 008h */
			uint32_t	v_intercept1;		/* 00Ch */
			uint32_t	v_intercept2;		/* 010h */
			uint8_t		v_pad1[0x28];		/* 014h-03Bh */
			uint16_t	v_pause_thr;		/* 03Ch */
			uint16_t	v_pause_ct;		/* 03Eh */
			uint64_t	v_iopm_pa;		/* 040h */
			uint64_t	v_msrpm_pa;		/* 048h */
			uint64_t	v_tsc_offset;		/* 050h */
			uint32_t	v_asid;			/* 058h */
			uint8_t		v_tlb_control;		/* 05Ch */
			uint8_t		v_pad2[0x3];		/* 05Dh-05Fh */
			uint8_t		v_tpr;			/* 060h */
			uint8_t		v_irq;			/* 061h */
			uint8_t		v_misc1;		/* 062h */
			uint8_t		v_misc2;		/* 063h */
			uint8_t		v_misc3;		/* 064h */
			uint8_t		v_pad3[0x3];		/* 065h-067h */
			uint64_t	v_intr_shadow;		/* 068h */
			uint64_t	v_exitcode;		/* 070h */
			uint64_t	v_exitinfo1;		/* 078h */
			uint64_t	v_exitinfo2;		/* 080h */
			uint64_t	v_exitintinfo;		/* 088h */
			uint64_t	v_np_enable;		/* 090h */
			uint64_t	v_avic_apic_bar;	/* 098h */
			uint64_t	v_pad4;			/* 0A0h */
			uint64_t	v_eventinj;		/* 0A8h */
			uint64_t	v_n_cr3;		/* 0B0h */
			uint64_t	v_lbr_virt_enable;	/* 0B8h */
			uint64_t	v_vmcb_clean_bits;	/* 0C0h */
			uint64_t	v_nrip;			/* 0C8h */
			uint8_t		v_n_bytes_fetched;	/* 0D0h */
			uint8_t		v_guest_ins_bytes[0xf];	/* 0D1h-0DFh */
			uint64_t	v_avic_apic_back_page;	/* 0E0h */
			uint64_t	v_pad5;			/* 0E8h-0EFh */
			uint64_t	v_avic_logical_table;	/* 0F0h */
			uint64_t	v_avic_phys;		/* 0F8h */
			
		};
		uint8_t vmcb_control[0x400];
	};

	union {
		struct {
			/* Offsets here are relative to start of VMCB SSA */
			struct vmcb_segment	v_es;		/* 000h */
			struct vmcb_segment	v_cs;		/* 010h */
			struct vmcb_segment	v_ss;		/* 020h */
			struct vmcb_segment	v_ds;		/* 030h */
			struct vmcb_segment	v_fs;		/* 040h */
			struct vmcb_segment	v_gs;		/* 050h */
			struct vmcb_segment	v_gdtr;		/* 060h */
			struct vmcb_segment	v_ldtr;		/* 070h */
			struct vmcb_segment	v_idtr;		/* 080h */
			struct vmcb_segment	v_tr;		/* 090h */
			uint8_t 		v_pad6[0x2B];	/* 0A0h-0CAh */
			uint8_t			v_cpl;		/* 0CBh */
			uint32_t		v_pad7;		/* 0CCh-0CFh */
			uint64_t		v_efer;		/* 0D0h */
			uint8_t			v_pad8[0x70];	/* 0D8h-147h */
			uint64_t		v_cr4;		/* 148h */
			uint64_t		v_cr3;		/* 150h */
			uint64_t		v_cr0;		/* 158h */
			uint64_t		v_dr7;		/* 160h */
			uint64_t		v_dr6;		/* 168h */
			uint64_t		v_rflags;	/* 170h */
			uint64_t		v_rip;		/* 178h */
			uint64_t		v_pad9[0xB];	/* 180h-1D7h */
			uint64_t		v_rsp;		/* 1D8h */
			uint64_t		v_pad10[0x3];	/* 1E0h-1F7h */
			uint64_t		v_rax;		/* 1F8h */
			uint64_t		v_star;		/* 200h */
			uint64_t		v_lstar;	/* 208h */
			uint64_t		v_cstar;	/* 210h */
			uint64_t		v_sfmask;	/* 218h */
			uint64_t		v_kgsbase;	/* 220h */
			uint64_t		v_sysenter_cs;	/* 228h */
			uint64_t		v_sysenter_esp;	/* 230h */
			uint64_t		v_sysenter_eip;	/* 238h */
			uint64_t		v_cr2;		/* 240h */
			uint64_t		v_pad11[0x4];	/* 248h-267h */
			uint64_t		v_g_pat;	/* 268h */
			uint64_t		v_dbgctl;	/* 270h */
			uint64_t		v_br_from;	/* 278h */
			uint64_t		v_br_to;	/* 280h */
			uint64_t		v_lastexcpfrom;	/* 288h */
			uint64_t		v_lastexcpto;	/* 290h */
		};

		uint8_t vmcb_layout[PAGE_SIZE - 0x400];
	};
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

	/* SVM only */
	vaddr_t vc_svm_hsa_va;
	paddr_t vc_svm_hsa_pa;
	vaddr_t vc_svm_ioio_va;
	paddr_t vc_svm_ioio_pa;
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
