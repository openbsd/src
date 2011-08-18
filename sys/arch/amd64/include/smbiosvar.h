/*	$OpenBSD: smbiosvar.h,v 1.9 2011/08/18 16:01:24 marco Exp $	*/
/*
 * Copyright (c) 2006 Gordon Willem Klok <gklok@cogeco.ca>
 * Copyright (c) 2005 Jordan Hargrave
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
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHORS AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL THE AUTHORS OR CONTRIBUTORS BE LIABLE FOR
 * ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */
#ifndef _I386_SMBIOSVAR_
#define _I386_SMBIOSVAR_

#define SMBIOS_START			0xf0000
#define SMBIOS_END			0xfffff

#define SMBIOS_UUID_NPRESENT		0x1
#define SMBIOS_UUID_NSET		0x2

/*
 * Section 3.5 of "UUIDs and GUIDs" found at
 * http://www.opengroup.org/dce/info/draft-leach-uuids-guids-01.txt
 * specifies the string repersentation of a UUID.
 */
#define SMBIOS_UUID_REP "%02x%02x%02x%02x-%02x%02x-%02x%02x-%02x%02x-%02x%02x%02x%02x%02x%02x"
#define SMBIOS_UUID_REPLEN 37 /* 16 zero padded values, 4 hyphens, 1 null */

struct smbios_entry {
	u_int8_t	mjr;
	u_int8_t	min;
	u_int8_t	*addr;
	u_int16_t	len;
	u_int16_t	count;
};

struct smbhdr {
	u_int32_t	sig;		/* "_SM_" */
	u_int8_t	checksum;	/* Entry point checksum */
	u_int8_t	len;		/* Entry point structure length */
	u_int8_t	majrev;		/* Specification major revision */
	u_int8_t	minrev;		/* Specification minor revision */
	u_int16_t	mss;		/* Maximum Structure Size */
	u_int8_t	epr;		/* Entry Point Revision */
	u_int8_t	fa[5];		/* value determined by EPR */
	u_int8_t	sasig[5];	/* Secondary Anchor "_DMI_" */
	u_int8_t	sachecksum;	/* Secondary Checksum */
	u_int16_t	size;		/* Length of structure table in bytes */
	u_int32_t	addr;		/* Structure table address */
	u_int16_t	count;		/* Number of SMBIOS structures */
	u_int8_t	rev;		/* BCD revision */
} __packed;

struct smbtblhdr {
	u_int8_t	type;
	u_int8_t	size;
	u_int16_t	handle;
} __packed;

struct smbtable {
	struct smbtblhdr *hdr;
	void		 *tblhdr;
	u_int32_t	 cookie;
};

#define	SMBIOS_TYPE_BIOS		0
#define	SMBIOS_TYPE_SYSTEM		1
#define	SMBIOS_TYPE_BASEBOARD		2
#define	SMBIOS_TYPE_ENCLOSURE		3
#define	SMBIOS_TYPE_PROCESSOR		4
#define	SMBIOS_TYPE_MEMCTRL		5
#define	SMBIOS_TYPE_MEMMOD		6
#define	SMBIOS_TYPE_CACHE		7
#define	SMBIOS_TYPE_PORT		8
#define	SMBIOS_TYPE_SLOTS		9
#define	SMBIOS_TYPE_OBD			10
#define	SMBIOS_TYPE_OEM			11
#define	SMBIOS_TYPE_SYSCONFOPT		12
#define	SMBIOS_TYPE_BIOSLANG		13
#define	SMBIOS_TYPE_GROUPASSOC		14
#define	SMBIOS_TYPE_SYSEVENTLOG		15
#define	SMBIOS_TYPE_PHYMEM		16
#define	SMBIOS_TYPE_MEMDEV		17
#define	SMBIOS_TYPE_ECCINFO32		18
#define	SMBIOS_TYPE_MEMMAPARRAYADDR	19
#define	SMBIOS_TYPE_MEMMAPDEVADDR	20
#define	SMBIOS_TYPE_INBUILTPOINT	21
#define	SMBIOS_TYPE_PORTBATT		22
#define	SMBIOS_TYPE_SYSRESET		23
#define	SMBIOS_TYPE_HWSECUIRTY		24
#define	SMBIOS_TYPE_PWRCTRL		25
#define	SMBIOS_TYPE_VOLTPROBE		26
#define	SMBIOS_TYPE_COOLING		27
#define	SMBIOS_TYPE_TEMPPROBE		28
#define	SMBIOS_TYPE_CURRENTPROBE	29
#define	SMBIOS_TYPE_OOB_REMOTEACCESS	30
#define	SMBIOS_TYPE_BIS			31
#define	SMBIOS_TYPE_SBI			32
#define	SMBIOS_TYPE_ECCINFO64		33
#define	SMBIOS_TYPE_MGMTDEV		34
#define	SMBIOS_TYPE_MGTDEVCOMP		35
#define	SMBIOS_TYPE_MGTDEVTHRESH	36
#define	SMBIOS_TYPE_MEMCHANNEL		37
#define	SMBIOS_TYPE_IPMIDEV		38
#define	SMBIOS_TYPE_SPS			39
#define	SMBIOS_TYPE_INACTIVE		126
#define	SMBIOS_TYPE_EOT			127

/*
 * SMBIOS Structure Type 0 "BIOS Information"
 * DMTF Specification DSP0134 Section: 3.3.1 p.g. 34
 */
struct smbios_struct_bios {
	u_int8_t	vendor;		/* string */
	u_int8_t	version;	/* string */
	u_int16_t	startaddr;
	u_int8_t	release;	/* string */
	u_int8_t	romsize;
	u_int64_t	characteristics;
	u_int32_t	charext;
	u_int8_t	major_rel;
	u_int8_t	minor_rel;
	u_int8_t	ecf_mjr_rel;	/* embedded controler firmware */
	u_int8_t	ecf_min_rel;	/* embedded controler firmware */
} __packed;

/*
 * SMBIOS Structure Type 1 "System Information"
 * DMTF Specification DSP0134 Section 3.3.2 p.g. 35
 */

struct smbios_sys {
/* SMBIOS spec 2.0+ */
	u_int8_t	vendor;		/* string */
	u_int8_t	product;	/* string */
	u_int8_t	version;	/* string */
	u_int8_t	serial;		/* string */
/* SMBIOS spec 2.1+ */
	u_int8_t	uuid[16];
	u_int8_t	wakeup;
/* SMBIOS spec 2.4+ */
	u_int8_t	sku;		/* string */
	u_int8_t	family;		/* string */
} __packed;

/*
 * SMBIOS Structure Type 2 "Base Board (Module) Information"
 * DMTF Specification DSP0134 Section 3.3.3 p.g. 37
 */
struct smbios_board {
	u_int8_t	vendor;		/* string */
	u_int8_t	product;	/* string */
	u_int8_t	version;	/* string */
	u_int8_t	serial;		/* string */
	u_int8_t	asset;		/* stirng */
	u_int8_t	feature;	/* feature flags */
	u_int8_t	location;	/* location in chassis */
	u_int16_t	handle;		/* chassis handle */
	u_int8_t	type;		/* board type */
	u_int8_t	noc;		/* number of contained objects */
} __packed;

/*
 * SMBIOS Structure Type 3 "System Wnclosure or Chassis"
 * DMTF Specification DSP0134
 */
struct smbios_enclosure {
	/* SMBIOS spec  2.0+ */
	u_int8_t	vendor;		/* string */
	u_int8_t	type;
	u_int8_t	version;	/* string */
	u_int8_t	serial;		/* string */
	u_int8_t	asset_tag;	/* string */
	/* SMBIOS spec  2.1+ */
	u_int8_t	boot_state;
	u_int8_t	psu_state;
	u_int8_t	thermal_state;
	u_int8_t	security_status;
	/* SMBIOS spec 2.3+ */
	u_int16_t	oem_defined;
	u_int8_t	height;
	u_int8_t	no_power_cords;
	u_int8_t	no_contained_element;
	u_int8_t	reclen_contained_element;
	u_int8_t	contained_elements;
	/* SMBIOS spec 2.7+ */
	u_int8_t	sku;		/* string */
} __packed;

/*
 * SMBIOS Structure Type 4 "processor Information"
 * DMTF Specification DSP0134 v2.5 Section 3.3.5 p.g. 24
 */
struct smbios_cpu {
	u_int8_t	cpu_socket_designation;	/* string */
	u_int8_t	cpu_type;
	u_int8_t	cpu_family;
	u_int8_t	cpu_mfg;		/* string */
	u_int32_t	cpu_id_eax;
	u_int32_t	cpu_id_edx;
	u_int8_t	cpu_version;		/* string */
	u_int8_t	cpu_voltage;
	u_int16_t	cpu_clock;
	u_int16_t	cpu_max_speed;
	u_int16_t	cpu_current_speed;
	u_int8_t	cpu_status;
#define SMBIOS_CPUST_POPULATED			(1<<6)
#define SMBIOS_CPUST_STATUSMASK			(0x07)
	u_int8_t	cpu_upgrade;
	u_int16_t	cpu_l1_handle;
	u_int16_t	cpu_l2_handle;
	u_int16_t	cpu_l3_handle;
	u_int8_t	cpu_serial;		/* string */
	u_int8_t	cpu_asset_tag;		/* string */
	u_int8_t	cpu_part_nr;		/* string */
	/* following fields were added in smbios 2.5 */
	u_int8_t	cpu_core_count;
	u_int8_t	cpu_core_enabled;
	u_int8_t	cpu_thread_count;
	u_int16_t	cpu_characteristics;
} __packed;

/*
 * SMBIOS Structure Type 38 "IPMI Information"
 * DMTF Specification DSP0134 Section 3.3.39 p.g. 91
 */
struct smbios_ipmi {
        u_int8_t        smipmi_if_type;         /* IPMI Interface Type */
        u_int8_t        smipmi_if_rev;          /* BCD IPMI Revision */
        u_int8_t        smipmi_i2c_address;     /* I2C address of BMC */
        u_int8_t        smipmi_nvram_address;   /* I2C address of NVRAM
						 * storage */
        u_int64_t       smipmi_base_address;    /* Base address of BMC (BAR
						 * format */
        u_int8_t        smipmi_base_flags;      /* Flags field:
						 * bit 7:6 : register spacing
						 *   00 = byte
						 *   01 = dword
						 *   02 = word
						 * bit 4 : Lower bit BAR
						 * bit 3 : IRQ valid
						 * bit 2 : N/A
						 * bit 1 : Interrupt polarity
						 * bit 0 : Interrupt trigger */
        u_int8_t        smipmi_irq;             /* IRQ if applicable */
} __packed;

int smbios_find_table(u_int8_t, struct smbtable *);
char *smbios_get_string(struct smbtable *, u_int8_t, char *, size_t);

#endif
