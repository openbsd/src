/*	$OpenBSD: mp.h,v 1.2 2004/06/13 21:49:16 niklas Exp $	*/

/*-
 * Copyright (c) 1996 SigmaSoft, Th. Lockert <tholo@sigmasoft.com>
 * Copyright (c) 2000 Niklas Hallqvist.
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
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *	This product includes software developed by SigmaSoft, Th.  Lockert.
 * 4. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED ``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES,
 * INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY
 * AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL
 * THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS;
 * OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
 * WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR
 * OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF
 * ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef _MACHINE_MP_H
#define _MACHINE_MP_H

/*
 * Configuration structures as defined in the Intel MP specification,
 * version 1.4
 */

/*
 * MP Floating Pointer structure; must be located on 16-byte boundary
 */
struct mp_float {
	u_int8_t	signature[4];
#define	MPF_SIGNATURE	"_MP_"
	u_int32_t	pointer;
	u_int8_t	length;
	u_int8_t	revision;
	u_int8_t	checksum;
	u_int8_t	feature1;
#define	MP_CONF_EXTENDED	0
#define	MP_CONF_2_ISA		1
#define	MP_CONF_2_EISA_NO_8	2
#define	MP_CONF_2_EISA		3
#define	MP_CONF_2_MCA		4
#define	MP_CONF_2_ISA_PCI	5
#define	MP_CONF_2_EISA_PCI	6
#define	MP_CONF_2_MCA_PCI	7
	u_int8_t	feature2;
#define	MP_IMCR		0x80
	u_int8_t	feature3;
	u_int8_t	feature4;
	u_int8_t	feature5;
};

/*
 * MP configuration table header
 */
struct mp_conf {
	u_int8_t	signature[4];
#define	MPC_SIGNATURE	"PCMP"
	u_int16_t	length;
	u_int8_t	revision;
	u_int8_t	checksum;
	u_int8_t	oem[8];
	u_int8_t	product[12];
	u_int32_t	oem_pointer;
	u_int16_t	oem_length;
	u_int16_t	entry_count;
	u_int32_t	local_apic;
	u_int16_t	ext_length;
	u_int8_t	et_checksum;
	u_int8_t	reserved;
};

/*
 * Processor entry
 */
struct mp_proc {
	u_int8_t	type;
#define	MP_PROCESSOR	0
	u_int8_t	local_apic;
	u_int8_t	apic_version;
	u_int8_t	flags;
#define	MP_ENABLE	0x01
#define	MP_BOOTCPU	0x02
	u_int32_t	cpu_signature;
#define	MP_STEPPING	0x0000000F
#define	MP_MODEL	0x000000F0
#define	MP_FAMILY	0x00000F00
	u_int32_t	feature_flags;
#define	MP_FP		0x00000001
#define	MP_MCE		0x00000080
#define	MP_CX8		0x00000100
#define	MP_APIC		0x00000200
	u_int32_t	reserved1;
	u_int32_t	reserved2;
};

/*
 * Bus entry
 */
struct mp_bus {
	u_int8_t	type;
#define	MP_BUS		1
	u_int8_t	bus_id;
	u_int8_t	bustype[6]	__attribute((packed));
#define	MP_BUS_CBUS	"CBUS  "
#define	MP_BUS_CBUSII	"CBUSII"
#define	MP_BUS_EISA	"EISA  "
#define	MP_BUS_FUTURE	"FUTURE"
#define	MP_BUS_INTERN	"INTERN"
#define	MP_BUS_ISA	"ISA   "
#define	MP_BUS_MBI	"MBI   "
#define	MP_BUS_MBII	"MBII  "
#define	MP_BUS_MCA	"MCA   "
#define	MP_BUS_MPI	"MPI   "
#define	MP_BUS_MPSA	"MPSA  "
#define	MP_BUS_NUBUS	"NUBUS "
#define	MP_BUS_PCI	"PCI   "
#define	MP_BUS_PCCARD	"PCMCIA"
#define	MP_BUS_TC	"TC    "
#define	MP_BUS_VLB	"VL    "
#define	MP_BUS_VME	"VME   "
#define	MP_BUS_XPRESS	"XPRESS"
};

/*
 * I/O APIC entry
 */
struct mp_apic {
	u_int8_t	type;
#define	MP_IOAPIC	2
	u_int8_t	apic_id;
	u_int8_t	apic_version;
	u_int8_t	apic_flags;
#define	MP_APIC_ENABLE	0x80
	u_int32_t	apic_address;
};

/*
 * I/O Interrupt Assignment entry
 * Local Interrupt Assignment entry
 */
struct mp_irq {
	u_int8_t	type;
#define	MP_INTSRC	3
#define	MP_LOCINTSRC	4
	u_int8_t	irqtype;
#define	MP_INT_NORMAL	0
#define	MP_INT_NMI	1
#define	MP_INT_SMI	2
#define	MP_INT_EXT	3
	u_int16_t	irqflags;
	u_int8_t	bus_id;
	u_int8_t	source_irq;
	u_int8_t	destination_apic;
#define	MP_ALL_APIC	0xFF
	u_int8_t	apic_intr;
};

/*
 * System Address Space Mapping entry
 */
struct mp_map {
	u_int8_t	type;
#define	MP_SYSMAP	128
	u_int8_t	length;
	u_int8_t	bus;
	u_int8_t	address_type;
#define	MP_ADDR_IO	0
#define	MP_ADDR_MEM	1
#define	MP_ADDR_PRE	2
	u_int64_t	address_base;
	u_int64_t	address_length;
};

/*
 * Bus Hierarchy Descriptor entry
 */
struct mp_bushier {
	u_int8_t	type;
#define	MP_BUSHIER	129
	u_int8_t	length;
	u_int8_t	bus_id;
	u_int8_t	bus_info;
#define	MP_BUS_SUB	0x01
	u_int8_t	parent;
	u_int8_t	reserved1;
	u_int16_t	reserved2;
};

/*
 * Compatibility Bus Address Space Modifier entry
 */
struct mp_buscompat {
	u_int8_t	type;
#define	MP_BUSCOMPAT	130
	u_int8_t	length;
	u_int8_t	bus_id;
	u_int8_t	modifier;
#define	MP_COMPAT_SUB	0x01
	u_int32_t	range;
};

#ifdef _KERNEL
extern int napics;
#endif /* _KERNEL */

#endif /* _MACHINE_MP_H */
