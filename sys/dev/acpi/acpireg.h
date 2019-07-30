/*	$OpenBSD: acpireg.h,v 1.40 2018/03/14 18:52:16 patrick Exp $	*/
/*
 * Copyright (c) 2005 Thorsten Lockert <tholo@sigmasoft.com>
 * Copyright (c) 2005 Marco Peereboom <marco@openbsd.org>
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

#ifndef _DEV_ACPI_ACPIREG_H_
#define _DEV_ACPI_ACPIREG_H_

/*	Root System Descriptor Pointer */
struct acpi_rsdp1 {
	u_int8_t	signature[8];
#define	RSDP_SIG	"RSD PTR "
#define	rsdp_signaturee	rsdp1.signature
	u_int8_t	checksum;	/* make sum == 0 */
#define	rsdp_checksum	rsdp1.checksum
	u_int8_t	oemid[6];
#define	rsdp_oemid	rsdp1.oemid
	u_int8_t	revision;	/* 0 for 1, 2 for 2 */
#define	rsdp_revision	rsdp1.revision
	u_int32_t	rsdt;		/* physical */
#define	rsdp_rsdt	rsdp1.rsdt
} __packed;

struct acpi_rsdp {
	struct acpi_rsdp1 rsdp1;
	/*
	 * The following values are only valid
	 * when rsdp_revision == 2
	 */
	u_int32_t	rsdp_length;		/* length of rsdp */
	u_int64_t	rsdp_xsdt;		/* physical */
	u_int8_t	rsdp_extchecksum;	/* entire table */
	u_int8_t	rsdp_reserved[3];	/* must be zero */
} __packed;

struct acpi_table_header {
	u_int8_t	signature[4];
#define	hdr_signature		hdr.signature
	u_int32_t	length;
#define	hdr_length		hdr.length
	u_int8_t	revision;
#define	hdr_revision		hdr.revision
	u_int8_t	checksum;
#define	hdr_checksum		hdr.checksum
	u_int8_t	oemid[6];
#define hdr_oemid		hdr.oemid
	u_int8_t	oemtableid[8];
#define hdr_oemtableid		hdr.oemtableid
	u_int32_t	oemrevision;
#define	hdr_oemrevision		hdr.oemrevision
	u_int8_t	aslcompilerid[4];
#define hdr_aslcompilerid	hdr.aslcompilerid
	u_int32_t	aslcompilerrevision;
#define	hdr_aslcompilerrevision	hdr.aslcompilerrevision
} __packed;

struct acpi_rsdt {
	struct acpi_table_header	hdr;
#define RSDT_SIG	"RSDT"
	u_int32_t			table_offsets[1];
} __packed;

struct acpi_xsdt {
	struct acpi_table_header	hdr;
#define XSDT_SIG	"XSDT"
	u_int64_t			table_offsets[1];
} __packed;

struct acpi_gas {
	u_int8_t	address_space_id;
#define GAS_SYSTEM_MEMORY	0
#define GAS_SYSTEM_IOSPACE	1
#define GAS_PCI_CFG_SPACE	2
#define GAS_EMBEDDED		3
#define GAS_SMBUS		4
#define GAS_FUNCTIONAL_FIXED	127
	u_int8_t	register_bit_width;
	u_int8_t	register_bit_offset;
	u_int8_t	access_size;
#define GAS_ACCESS_UNDEFINED	0
#define GAS_ACCESS_BYTE		1
#define GAS_ACCESS_WORD		2
#define GAS_ACCESS_DWORD	3
#define GAS_ACCESS_QWORD	4
	u_int64_t	address;
} __packed;

struct acpi_fadt {
	struct acpi_table_header	hdr;
#define	FADT_SIG	"FACP"
	u_int32_t	firmware_ctl;	/* phys addr FACS */
	u_int32_t	dsdt;		/* phys addr DSDT */
	u_int8_t	int_model;	/* interrupt model (hdr_revision < 3) */
#define	FADT_INT_DUAL_PIC	0
#define	FADT_INT_MULTI_APIC	1
	u_int8_t	pm_profile;	/* power mgmt profile */
#define	FADT_PM_UNSPEC		0
#define	FADT_PM_DESKTOP		1
#define	FADT_PM_MOBILE		2
#define	FADT_PM_WORKSTATION	3
#define	FADT_PM_ENT_SERVER	4
#define	FADT_PM_SOHO_SERVER	5
#define	FADT_PM_APPLIANCE	6
#define	FADT_PM_PERF_SERVER	7
	u_int16_t	sci_int;	/* SCI interrupt */
	u_int32_t	smi_cmd;	/* SMI command port */
	u_int8_t	acpi_enable;	/* value to enable */
	u_int8_t	acpi_disable;	/* value to disable */
	u_int8_t	s4bios_req;	/* value for S4 */
	u_int8_t	pstate_cnt;	/* value for performance (hdr_revision > 2) */
	u_int32_t	pm1a_evt_blk;	/* power management 1a */
	u_int32_t	pm1b_evt_blk;	/* power mangement 1b */
	u_int32_t	pm1a_cnt_blk;	/* pm control 1a */
	u_int32_t	pm1b_cnt_blk;	/* pm control 1b */
	u_int32_t	pm2_cnt_blk;	/* pm control 2 */
	u_int32_t	pm_tmr_blk;
	u_int32_t	gpe0_blk;
	u_int32_t	gpe1_blk;
	u_int8_t	pm1_evt_len;
	u_int8_t	pm1_cnt_len;
	u_int8_t	pm2_cnt_len;
	u_int8_t	pm_tmr_len;
	u_int8_t	gpe0_blk_len;
	u_int8_t	gpe1_blk_len;
	u_int8_t	gpe1_base;
	u_int8_t	cst_cnt;	/* (hdr_revision > 2) */
	u_int16_t	p_lvl2_lat;
	u_int16_t	p_lvl3_lat;
	u_int16_t	flush_size;
	u_int16_t	flush_stride;
	u_int8_t	duty_offset;
	u_int8_t	duty_width;
	u_int8_t	day_alrm;
	u_int8_t	mon_alrm;
	u_int8_t	century;
	u_int16_t	iapc_boot_arch;	/* (hdr_revision > 2) */
#define	FADT_LEGACY_DEVICES		0x0001	/* Legacy devices supported */
#define	FADT_i8042			0x0002	/* Keyboard controller present */
#define	FADT_NO_VGA			0x0004	/* Do not probe VGA */
	u_int8_t	reserved1;
	u_int32_t	flags;
#define	FADT_WBINVD			0x00000001
#define	FADT_WBINVD_FLUSH		0x00000002
#define	FADT_PROC_C1			0x00000004
#define	FADT_P_LVL2_UP			0x00000008
#define	FADT_PWR_BUTTON			0x00000010
#define	FADT_SLP_BUTTON			0x00000020
#define	FADT_FIX_RTC			0x00000040
#define	FADT_RTC_S4			0x00000080
#define	FADT_TMR_VAL_EXT		0x00000100
#define	FADT_DCK_CAP			0x00000200
#define	FADT_RESET_REG_SUP		0x00000400
#define	FADT_SEALED_CASE		0x00000800
#define	FADT_HEADLESS			0x00001000
#define	FADT_CPU_SW_SLP			0x00002000
#define	FADT_PCI_EXP_WAK		0x00004000
#define	FADT_USE_PLATFORM_CLOCK		0x00008000
#define	FADT_S4_RTC_STS_VALID		0x00010000
#define	FADT_REMOTE_POWER_ON_CAPABLE	0x00020000
#define	FADT_FORCE_APIC_CLUSTER_MODEL	0x00040000
#define	FADT_FORCE_APIC_PHYS_DEST_MODE	0x00080000
#define	FADT_HW_REDUCED_ACPI		0x00100000
#define	FADT_POWER_S0_IDLE_CAPABLE	0x00200000
	/*
	 * Following values only exist when rev > 1
	 * If the extended addresses exists, they
	 * must be used in preferense to the non-
	 * extended values above
	 */
	struct acpi_gas	reset_reg;
	u_int8_t	reset_value;
	u_int8_t	reserved2a;
	u_int8_t	reserved2b;
	u_int8_t	reserved2c;
	u_int64_t	x_firmware_ctl;
	u_int64_t	x_dsdt;
	struct acpi_gas	x_pm1a_evt_blk;
	struct acpi_gas	x_pm1b_evt_blk;
	struct acpi_gas	x_pm1a_cnt_blk;
	struct acpi_gas	x_pm1b_cnt_blk;
	struct acpi_gas	x_pm2_cnt_blk;
	struct acpi_gas	x_pm_tmr_blk;
	struct acpi_gas	x_gpe0_blk;
	struct acpi_gas	x_gpe1_blk;
	struct acpi_gas sleep_control_reg;
	struct acpi_gas sleep_status_reg;
} __packed;

struct acpi_dsdt {
	struct acpi_table_header	hdr;
#define DSDT_SIG	"DSDT"
	u_int8_t	aml[1];
} __packed;

struct acpi_ssdt {
	struct acpi_table_header	hdr;
#define SSDT_SIG	"SSDT"
	u_int8_t	aml[1];
} __packed;

/*
 * Table deprecated by ACPI 2.0
 */
struct acpi_psdt {
	struct acpi_table_header	hdr;
#define PSDT_SIG	"PSDT"
} __packed;

struct acpi_madt {
	struct acpi_table_header	hdr;
#define MADT_SIG	"APIC"
	u_int32_t	local_apic_address;
	u_int32_t	flags;
#define ACPI_APIC_PCAT_COMPAT	0x00000001
} __packed;

struct acpi_madt_lapic {
	u_int8_t	apic_type;
#define	ACPI_MADT_LAPIC		0
	u_int8_t	length;
	u_int8_t	acpi_proc_id;
	u_int8_t	apic_id;
	u_int32_t	flags;
#define	ACPI_PROC_ENABLE	0x00000001
} __packed;

struct acpi_madt_ioapic {
	u_int8_t	apic_type;
#define	ACPI_MADT_IOAPIC	1
	u_int8_t	length;
	u_int8_t	acpi_ioapic_id;
	u_int8_t	reserved;
	u_int32_t	address;
	u_int32_t	global_int_base;
} __packed;

struct acpi_madt_override {
	u_int8_t	apic_type;
#define	ACPI_MADT_OVERRIDE	2
	u_int8_t	length;
	u_int8_t	bus;
#define	ACPI_OVERRIDE_BUS_ISA	0
	u_int8_t	source;
	u_int32_t	global_int;
	u_int16_t	flags;
#define	ACPI_OVERRIDE_POLARITY_BITS	0x3
#define	ACPI_OVERRIDE_POLARITY_BUS		0x0
#define	ACPI_OVERRIDE_POLARITY_HIGH		0x1
#define	ACPI_OVERRIDE_POLARITY_LOW		0x3
#define	ACPI_OVERRIDE_TRIGGER_BITS	0xc
#define	ACPI_OVERRIDE_TRIGGER_BUS		0x0
#define	ACPI_OVERRIDE_TRIGGER_EDGE		0x4
#define	ACPI_OVERRIDE_TRIGGER_LEVEL		0xc
} __packed;

struct acpi_madt_nmi {
	u_int8_t	apic_type;
#define	ACPI_MADT_NMI		3
	u_int8_t	length;
	u_int16_t	flags;		/* Same flags as acpi_madt_override */
	u_int32_t	global_int;
} __packed;

struct acpi_madt_lapic_nmi {
	u_int8_t	apic_type;
#define	ACPI_MADT_LAPIC_NMI	4
	u_int8_t	length;
	u_int8_t	acpi_proc_id;
	u_int16_t	flags;		/* Same flags as acpi_madt_override */
	u_int8_t	local_apic_lint;
} __packed;

struct acpi_madt_lapic_override {
	u_int8_t	apic_type;
#define	ACPI_MADT_LAPIC_OVERRIDE	5
	u_int8_t	length;
	u_int16_t	reserved;
	u_int64_t	lapic_address;
} __packed;

struct acpi_madt_io_sapic {
	u_int8_t	apic_type;
#define	ACPI_MADT_IO_SAPIC	6
	u_int8_t	length;
	u_int8_t	iosapic_id;
	u_int8_t	reserved;
	u_int32_t	global_int_base;
	u_int64_t	iosapic_address;
} __packed;

struct acpi_madt_local_sapic {
	u_int8_t	apic_type;
#define	ACPI_MADT_LOCAL_SAPIC	7
	u_int8_t	length;
	u_int8_t	acpi_proc_id;
	u_int8_t	local_sapic_id;
	u_int8_t	local_sapic_eid;
	u_int8_t	reserved[3];
	u_int32_t	flags;		/* Same flags as acpi_madt_lapic */
	u_int32_t	acpi_proc_uid;
	u_int8_t	acpi_proc_uid_string[1];
} __packed;

struct acpi_madt_platform_int {
	u_int8_t	apic_type;
#define	ACPI_MADT_PLATFORM_INT	8
	u_int8_t	length;
	u_int16_t	flags;		/* Same flags as acpi_madt_override */
	u_int8_t	int_type;
#define	ACPI_MADT_PLATFORM_PMI		1
#define	ACPI_MADT_PLATFORM_INIT		2
#define	ACPI_MADT_PLATFORM_CORR_ERROR	3
	u_int8_t	proc_id;
	u_int8_t	proc_eid;
	u_int8_t	io_sapic_vec;
	u_int32_t	global_int;
	u_int32_t	platform_int_flags;
#define	ACPI_MADT_PLATFORM_CPEI		0x00000001
} __packed;

struct acpi_madt_x2apic {
	u_int8_t	apic_type;
#define	ACPI_MADT_X2APIC	9
	u_int8_t	length;
	u_int8_t	reserved[2];
	u_int32_t	apic_id;
	u_int32_t	flags;		/* Same flags as acpi_madt_lapic */
	u_int32_t	acpi_proc_uid;
} __packed;

struct acpi_madt_x2apic_nmi {
	u_int8_t	apic_type;
#define	ACPI_MADT_X2APIC_NMI	10
	u_int8_t	length;
	u_int16_t	flags;		/* Same flags as acpi_madt_override */
	u_int32_t	apic_proc_uid;
	u_int8_t	local_x2apic_lint;
	u_int8_t	reserved[3];
} __packed;

union acpi_madt_entry {
	struct acpi_madt_lapic		madt_lapic;
	struct acpi_madt_ioapic		madt_ioapic;
	struct acpi_madt_override	madt_override;
	struct acpi_madt_nmi		madt_nmi;
	struct acpi_madt_lapic_nmi	madt_lapic_nmi;
	struct acpi_madt_lapic_override	madt_lapic_override;
	struct acpi_madt_io_sapic	madt_io_sapic;
	struct acpi_madt_local_sapic	madt_local_sapic;
	struct acpi_madt_platform_int	madt_platform_int;
	struct acpi_madt_x2apic		madt_x2apic;
	struct acpi_madt_x2apic_nmi	madt_x2apic_nmi;
} __packed;

struct acpi_sbst {
	struct acpi_table_header	hdr;
#define SBST_SIG	"SBST"
	u_int32_t	warning_energy_level;
	u_int32_t	low_energy_level;
	u_int32_t	critical_energy_level;
} __packed;

struct acpi_ecdt {
	struct acpi_table_header	hdr;
#define ECDT_SIG	"ECDT"
	struct acpi_gas	ec_control;
	struct acpi_gas ec_data;
	u_int32_t	uid;
	u_int8_t	gpe_bit;
	u_int8_t	ec_id[1];
} __packed;

struct acpi_srat {
	struct acpi_table_header	hdr;
#define SRAT_SIG	"SRAT"
	u_int32_t	reserved1;
	u_int64_t	reserved2;
} __packed;

struct acpi_slit {
	struct acpi_table_header	hdr;
#define SLIT_SIG	"SLIT"
	u_int64_t	number_of_localities;
} __packed;

struct acpi_hpet {
	struct acpi_table_header	hdr;
#define HPET_SIG	"HPET"
	u_int32_t	event_timer_block_id;
	struct acpi_gas	base_address;
	u_int8_t	hpet_number;
	u_int16_t	main_counter_min_clock_tick;
	u_int8_t	page_protection;
} __packed;

struct acpi_mcfg {
	struct acpi_table_header	hdr;
#define MCFG_SIG	"MCFG"
	u_int8_t	reserved[8];
	u_int64_t	base_address;
	u_int16_t	segment;
	u_int8_t	min_bus_number;
	u_int8_t	max_bus_number;
	u_int32_t	reserved1;
} __packed;

struct acpi_facs {
	u_int8_t	signature[4];
#define	FACS_SIG	"FACS"
	u_int32_t	length;
	u_int32_t	hardware_signature;
	u_int32_t	wakeup_vector;
	u_int32_t	global_lock;
#define	FACS_LOCK_PENDING	0x00000001
#define	FACS_LOCK_OWNED		0x00000002
	u_int32_t	flags;
#define	FACS_S4BIOS_F		0x00000001	/* S4BIOS_REQ supported */
	uint64_t	x_wakeup_vector;
	u_int8_t	version;
	u_int8_t	reserved[31];
} __packed;

/*
 * Intel ACPI DMA Remapping Entries
 */
struct acpidmar_devpath {
	uint8_t		device;
	uint8_t		function;
} __packed;

struct acpidmar_devscope {
	uint8_t		type;
#define DMAR_ENDPOINT			0x1
#define DMAR_BRIDGE			0x2
#define DMAR_IOAPIC			0x3
#define DMAR_HPET			0x4
	uint8_t		length;
	uint16_t	reserved;
	uint8_t		enumid;
	uint8_t		bus;
} __packed;

/* DMA Remapping Hardware Unit */
struct acpidmar_drhd {
	uint16_t	type;
	uint16_t	length;

	uint8_t		flags;
	uint8_t		reserved;
	uint16_t	segment;
	uint64_t	address;
	/* struct acpidmar_devscope[]; */
} __packed;

/* Reserved Memory Region Reporting */
struct acpidmar_rmrr {
	uint16_t	type;
	uint16_t	length;

	uint16_t	reserved;
	uint16_t	segment;
	uint64_t	base;
	uint64_t	limit;
	/* struct acpidmar_devscope[]; */
} __packed;

/* Root Port ATS Capability Reporting */
struct acpidmar_atsr {
	uint16_t	type;
	uint16_t	length;

	uint8_t		flags;
	uint8_t		reserved;
	uint16_t	segment;
	/* struct acpidmar_devscope[]; */
} __packed;

union acpidmar_entry {
	struct {
		uint16_t	type;
#define DMAR_DRHD			0x0
#define DMAR_RMRR			0x1
#define DMAR_ATSR			0x2
#define DMAR_RHSA			0x3
		uint16_t	length;
	} __packed;
	struct acpidmar_drhd	drhd;
	struct acpidmar_rmrr	rmrr;
	struct acpidmar_atsr	atsr;
} __packed;

struct acpi_dmar {
	struct acpi_table_header	hdr;
#define DMAR_SIG	"DMAR"
	uint8_t		haw;
	uint8_t		flags;
	uint8_t		reserved[10];
	/* struct acpidmar_entry[]; */
} __packed;

/*
 * AMD I/O Virtualization Remapping Entries
 */
union acpi_ivhd_entry {
	uint8_t		type;
#define IVHD_ALL			1
#define IVHD_SEL			2
#define IVHD_SOR			3
#define IVHD_EOR			4
#define IVHD_ALIAS_SEL			66
#define IVHD_ALIAS_SOR			67
#define IVHD_EXT_SEL			70
#define IVHD_EXT_SOR			71
#define IVHD_SPECIAL			72
	struct {
		uint8_t		type;
		uint16_t	resvd;
		uint8_t		data;
	} __packed all;
	struct {
		uint8_t		type;
		uint16_t	devid;
		uint8_t		data;
	} __packed sel;
	struct {
		uint8_t		type;
		uint16_t	devid;
		uint8_t		data;
	} __packed sor;
	struct {
		uint8_t		type;
		uint16_t	devid;
		uint8_t		resvd;
	} __packed eor;
	struct {
		uint8_t		type;
		uint16_t	devid;
		uint8_t		data;
		uint8_t		resvd1;
		uint16_t	srcid;
		uint8_t		resvd2;
	} __packed alias;
	struct {
		uint8_t		type;
		uint16_t	devid;
		uint8_t		data;
		uint32_t	extdata;
#define IVHD_ATS_DIS			(1L << 31)
	} __packed ext;
	struct {
		uint8_t		type;
		uint16_t	resvd;
		uint8_t		data;
		uint8_t		handle;
		uint16_t	devid;
		uint8_t		variety;
#define IVHD_IOAPIC			0x01
#define IVHD_HPET			0x02
	} __packed special;
} __packed;

struct acpi_ivmd {
	uint8_t		type;
	uint8_t		flags;
#define	IVMD_EXCLRANGE			(1L << 3)
#define IVMD_IW				(1L << 2)
#define IVMD_IR				(1L << 1)
#define IVMD_UNITY			(1L << 0)
	uint16_t	length;
	uint16_t	devid;
	uint16_t	auxdata;
	uint8_t		reserved[8];
	uint64_t	base;
	uint64_t	limit;
} __packed;

struct acpi_ivhd {
	uint8_t		type;
	uint8_t		flags;
#define IVHD_IOTLB		(1L << 4)
#define IVHD_ISOC		(1L << 3)
#define IVHD_RESPASSPW		(1L << 2)
#define IVHD_PASSPW		(1L << 1)
#define IVHD_HTTUNEN		(1L << 0)
	uint16_t	length;
	uint16_t	devid;
	uint16_t	cap;
	uint64_t	address;
	uint16_t	segment;
	uint16_t	info;
#define IVHD_UNITID_SHIFT	8
#define IVHD_UNITID_MASK	0x1F
#define IVHD_MSINUM_SHIFT	0
#define IVHD_MSINUM_MASK	0x1F
	uint32_t	reserved;
} __packed;

union acpi_ivrs_entry {
	struct {
		uint8_t		type;
#define IVRS_IVHD			0x10
#define IVRS_IVMD_ALL			0x20
#define IVRS_IVMD_SPECIFIED		0x21
#define IVRS_IVMD_RANGE			0x22
		uint8_t		flags;
		uint16_t	length;
	} __packed;
	struct acpi_ivhd	ivhd;
	struct acpi_ivmd	ivmd;
} __packed;

struct acpi_ivrs {
	struct acpi_table_header	hdr;
#define IVRS_SIG	"IVRS"
	uint32_t		ivinfo;
#define IVRS_ATSRNG		(1L << 22)
#define IVRS_VASIZE_SHIFT	15
#define IVRS_VASIZE_MASK	0x7F
#define IVRS_PASIZE_SHIFT	8
#define IVRS_PASIZE_MASK	0x7F
	uint8_t			reserved[8];
} __packed;


#define ACPI_FREQUENCY	3579545		/* Per ACPI spec */

/*
 * PCI Configuration space
 */
#define ACPI_ADR_PCIDEV(addr)	(u_int16_t)(addr >> 16)
#define ACPI_ADR_PCIFUN(addr)	(u_int16_t)(addr & 0xFFFF)
#define ACPI_PCI_BUS(addr) (u_int16_t)((addr) >> 48)
#define ACPI_PCI_DEV(addr) (u_int16_t)((addr) >> 32)
#define ACPI_PCI_FN(addr)  (u_int16_t)((addr) >> 16)
#define ACPI_PCI_REG(addr) (u_int16_t)(addr)
#define ACPI_PCI_ADDR(b,d,f,r) ((u_int64_t)(b)<<48LL | (u_int64_t)(d)<<32LL | (f)<<16LL | (r))

/*
 * PM1 Status Registers Fixed Hardware Feature Status Bits
 */
#define	ACPI_PM1_STATUS			0x00
#define		ACPI_PM1_TMR_STS		0x0001
#define		ACPI_PM1_BM_STS			0x0010
#define		ACPI_PM1_GBL_STS		0x0020
#define		ACPI_PM1_PWRBTN_STS		0x0100
#define		ACPI_PM1_SLPBTN_STS		0x0200
#define		ACPI_PM1_RTC_STS		0x0400
#define		ACPI_PM1_PCIEXP_WAKE_STS	0x4000
#define		ACPI_PM1_WAK_STS		0x8000

#define	ACPI_PM1_ALL_STS (ACPI_PM1_TMR_STS | ACPI_PM1_BM_STS | \
	    ACPI_PM1_GBL_STS | ACPI_PM1_PWRBTN_STS | \
	    ACPI_PM1_SLPBTN_STS | ACPI_PM1_RTC_STS | \
	    ACPI_PM1_PCIEXP_WAKE_STS | ACPI_PM1_WAK_STS )

/*
 * PM1 Enable Registers
 */
#define	ACPI_PM1_ENABLE			0x02
#define		ACPI_PM1_TMR_EN			0x0001
#define		ACPI_PM1_GBL_EN			0x0020
#define		ACPI_PM1_PWRBTN_EN		0x0100
#define		ACPI_PM1_SLPBTN_EN		0x0200
#define		ACPI_PM1_RTC_EN			0x0400
#define		ACPI_PM1_PCIEXP_WAKE_DIS	0x4000

/*
 * PM1 Control Registers
 */
#define	ACPI_PM1_CONTROL		0x00
#define		ACPI_PM1_SCI_EN			0x0001
#define		ACPI_PM1_BM_RLD			0x0002
#define		ACPI_PM1_GBL_RLS		0x0004
#define		ACPI_PM1_SLP_TYPX(x)		((x) << 10)
#define		ACPI_PM1_SLP_TYPX_MASK		0x1c00
#define		ACPI_PM1_SLP_EN			0x2000

/*
 * PM2 Control Registers
 */
#define ACPI_PM2_CONTROL		0x06
#define	ACPI_PM2_ARB_DIS		0x0001

/*
 * Operation Region Address Space Identifiers
 */
#define ACPI_OPREG_SYSMEM	0	/* SystemMemory */
#define ACPI_OPREG_SYSIO	1	/* SystemIO */
#define ACPI_OPREG_PCICFG	2	/* PCI_Config */
#define ACPI_OPREG_EC		3	/* EmbeddedControl */
#define ACPI_OPREG_SMBUS	4	/* SMBus */
#define ACPI_OPREG_CMOS		5	/* CMOS */
#define ACPI_OPREG_PCIBAR	6	/* PCIBARTarget */
#define ACPI_OPREG_IPMI		7	/* IPMI */
#define ACPI_OPREG_GPIO		8	/* GeneralPurposeIO */
#define ACPI_OPREG_GSB		9	/* GenericSerialBus */

/*
 * Sleeping States
 */
#define ACPI_STATE_S0		0
#define ACPI_STATE_S1		1
#define ACPI_STATE_S2		2
#define ACPI_STATE_S3		3
#define ACPI_STATE_S4		4
#define ACPI_STATE_S5		5

/*
 * Device Power States
 */
#define ACPI_STATE_D0		0
#define ACPI_STATE_D1		1
#define ACPI_STATE_D2		2
#define ACPI_STATE_D3		3

/*
 * ACPI Device IDs
 */
#define ACPI_DEV_TIM	"PNP0100"	/* System timer */
#define ACPI_DEV_ACPI	"PNP0C08"	/* ACPI device */
#define ACPI_DEV_PCIB	"PNP0A03"	/* PCI bus */
#define ACPI_DEV_GISAB	"PNP0A05"	/* Generic ISA Bus */
#define ACPI_DEV_EIOB	"PNP0A06"	/* Extended I/O Bus */
#define ACPI_DEV_PCIEB	"PNP0A08"	/* PCIe bus */
#define ACPI_DEV_MR	"PNP0C02"	/* Motherboard resources */
#define ACPI_DEV_NPROC	"PNP0C04"	/* Numeric data processor */
#define ACPI_DEV_CS	"PNP0C08"	/* ACPI-Compliant System */
#define ACPI_DEV_ECD	"PNP0C09"	/* Embedded Controller Device */
#define ACPI_DEV_CMB	"PNP0C0A"	/* Control Method Battery */
#define ACPI_DEV_FAN	"PNP0C0B"	/* Fan Device */
#define ACPI_DEV_PBD	"PNP0C0C"	/* Power Button Device */
#define ACPI_DEV_LD	"PNP0C0D"	/* Lid Device */
#define ACPI_DEV_SBD	"PNP0C0E"	/* Sleep Button Device */
#define ACPI_DEV_PILD	"PNP0C0F"	/* PCI Interrupt Link Device */
#define ACPI_DEV_MEMD	"PNP0C80"	/* Memory Device */
#define ACPI_DEV_MOUSE	"PNP0F13"	/* PS/2 Mouse */
#define ACPI_DEV_SHC	"ACPI0001"	/* SMBus 1.0 Host Controller */
#define ACPI_DEV_SBS	"ACPI0002"	/* Smart Battery Subsystem */
#define ACPI_DEV_AC	"ACPI0003"	/* AC Device */
#define ACPI_DEV_MD	"ACPI0004"	/* Module Device */
#define ACPI_DEV_SMBUS	"ACPI0005"	/* SMBus 2.0 Host Controller */
#define ACPI_DEV_GBD	"ACPI0006"	/* GPE Block Device */
#define ACPI_DEV_PD	"ACPI0007"	/* Processor Device */
#define ACPI_DEV_ALSD	"ACPI0008"	/* Ambient Light Sensor Device */
#define ACPI_DEV_IOXA	"ACPI0009"	/* IO x APIC Device */
#define ACPI_DEV_IOA	"ACPI000A"	/* IO APIC Device */
#define ACPI_DEV_IOSA	"ACPI000B"	/* IO SAPIC Device */
#define ACPI_DEV_THZ	"THERMALZONE"	/* Thermal Zone */
#define ACPI_DEV_FFB	"FIXEDBUTTON"	/* Fixed Feature Button */
#define ACPI_DEV_IPMI	"IPI0001"	/* IPMI */

#endif	/* !_DEV_ACPI_ACPIREG_H_ */
