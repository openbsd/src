/*	$OpenBSD: mbusreg.h,v 1.1 2008/08/18 23:19:25 miod Exp $	*/

/*
 * Copyright (c) 2008 Miodrag Vallat.
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
 * M-bus
 *
 * The M-bus connects up to 8 slots, of 32MB memory space each.
 *
 * All these modules contain a ``Firefox Bus Interface Chip'' (FBIC),
 * which provides common registers at the end of each slot address space,
 * allowing modules to recognize and configure each other.
 */

#define	MBUS_SLOT_MAX			8

/*
 * Addressing
 *
 * The M-bus provides a 32-bit address space.
 *
 * The low half (bit 31 clear) is the physical memory space (and being on
 * vax, really stops at 512MB).
 * The high half is the I/O space, where each slot has a 32MB window.
 * In addition to its window, there are two 128MB areas, allowing a given
 * module to provide functionnality regardless of its actual position on
 * the bus.
 *
 * From the host CPU, the M-bus I/O space is remapped in the vax I/O space.
 *
 * The address map is thus:
 * M-bus address	CPU address	Length
 * 0000.0000		0000.0000	2000.0000	memory space
 * 8000.0000		2000.0000	0800.0000	global I/O
 * 8800.0000		2800.0000	0800.0000	local I/O
 * 9000.0000		3000.0000	0200.0000	slot 0 I/O
 * 9200.0000		3200.0000	0200.0000	slot 1 I/O
 * 9400.0000		3400.0000	0200.0000	slot 2 I/O
 * 9600.0000		3600.0000	0200.0000	slot 3 I/O
 * 9800.0000		3800.0000	0200.0000	slot 4 I/O
 * 9a00.0000		3a00.0000	0200.0000	slot 5 I/O
 * 9c00.0000		3c00.0000	0200.0000	slot 6 I/O
 * 9e00.0000		3e00.0000	0200.0000	slot 7 I/O
 */

/* base address of a slot, as seen from the cpu */
#define	MBUS_SLOT_BASE(s)		(0x30000000 + ((s) << 25))

/* convert I/O space addresses (assumed to be in their correct range) */
#define	HOST_TO_MBUS(pa)		((pa) ^ 0xa0000000)
#define	MBUS_TO_HOST(pa)		((pa) ^ 0xa0000000)

/*
 * Common FBIC slot registers
 */

/* FBIC or compatible registers occupy the last page (running down)... */
#define	FBIC_BASE			0x1fffe00
/* ...but dual-CPU modules have two of them. */
#define	FBIC_CPUA_BASE			0x0fffe00
#define	FBIC_CPUB_BASE			0x1fffe00

/* module identification */
#define	FBIC_MODTYPE			0x1fc
/* M-bus error status */
#define	FBIC_BUSCSR			0x1f8
/* M-bus error control signal log */
#define	FBIC_BUSCTL			0x1f4
/* M-bus error address signal log */
#define	FBIC_BUSADR			0x1f0
/* M-bus error data signal log */
#define	FBIC_BUSDAT			0x1ec
/* FBIC control and status */
#define	FBIC_CSR			0x1e8
/* I/O space range decode */
#define	FBIC_RANGE			0x1e4
/* Interprocessor and device interrupt */
#define	FBIC_IPDVINT			0x1e0
/* Unique software ID */
#define	FBIC_WHAMI			0x1dc
/* Unique hardware ID */
#define	FBIC_CPUID			0x1d8
/* Interlock 1 address */
#define	FBIC_IADR1			0x1d4
/* Interlock 2 address */
#define	FBIC_IADR2			0x1d0
/* Console scratch register */
#define	FBIC_SAVGPR			0x1c4

/*
 * Module identification
 */
#define	MODTYPE_CLASS_MASK		0x000000ff
#define	MODTYPE_CLASS_SHIFT		0
#define	CLASS_BA			0x01
#define	CLASS_GRAPHICS			0x02
#define	CLASS_IO			0x04
#define	CLASS_CPU			0x08
#define	CLASS_MEMORY			0x10
#define	MODTYPE_SUBCLASS_MASK		0x0000ff00
#define	MODTYPE_SUBCLASS_SHIFT		8
#define	MODTYPE_INTERFACE_MASK		0x00ff0000
#define	MODTYPE_INTERFACE_SHIFT		16
#define	INTERFACE_FBIC			0x01
#define	INTERFACE_FMDC			0x02 /* ECC memory */
#define	INTERFACE_FMCM			0xfe /* 8MB board */
#define	MODTYPE_REVISION_MASK		0xff000000
#define	MODTYPE_REVISION_SHIFT		24

/*
 * M-bus error status and error logging
 * Conditions are active low
 */
#define	BUSCSR_FRZN			0x80000000 /* logging frozen */
#define	BUSCSR_ARB			0x40000000 /* arbitration error */
#define	BUSCSR_ICMD			0x20000000 /* invalid MCMD encoding */
#define	BUSCSR_IDAT			0x10000000 /* invalid data supplied */
#define	BUSCSR_MTPE			0x08000000 /* tag parity error */
#define	BUSCSR_MDPE			0x04000000 /* MDAL parity error */
#define	BUSCSR_MSPE			0x02000000 /* MSTATUS parity error */
#define	BUSCSR_MCPE			0x01000000 /* MCMD parity error */
#define	BUSCSR_ILCK			0x00800000 /* interlock violation */
#define	BUSCSR_MTO			0x00400000 /* slave timeout */
#define	BUSCSR_NOS			0x00200000 /* no slave response */
#define	BUSCSR_CTO			0x00100000 /* CDAL data cycle timeout */
#define	BUSCSR_CDPE			0x00080000 /* CDAL parity error */
#define	BUSCSR_CTPE			0x00040000 /* CDAL tag parity error */
#define	BUSCSR_SERR			0x00020000 /* error on MSTATUS */
#define	BUSCSR_DBLE			0x00010000 /* double M-bus error */

#define	BUSCSR_RESET			0xffff0000 /* reset all conditions */

/*
 * FBIC control and status
 */
#define	FBICSR_MFMD_MASK		0xc0000000 /* manufacturing mode */
#define	FBICSR_CMISS			0x08000000 /* CVAX cache miss */
#define	FBICSR_EXCAEN			0x04000000 /* external cache enable */
#define	FBICSR_HALTCPU			0x02000000 /* CVAX halt */
#define	FBICSR_RESET			0x01000000 /* CVAX reset */
#define	FBICSR_IRQEN_MASK		0x00f00000 /* interrupt enables */
#define	FBICSR_IRQEN_SHIFT		20
#define	FBIC_DEVIRQ0		0
#define	FBIC_DEVIRQ1		1
#define	FBIC_DEVIRQ2		2
#define	FBIC_DEVIRQ3		3
#define	FBIC_DEVIRQMAX		4
#define	FBICSR_IRQC2M_MASK		0x000f0000 /* interrupt direction */
#define	FBICSR_IRQC2M_SHIFT		16
#define	FBICSR_LEDS_MASK		0x00003f00 /* module leds, active low */
#define	FBICSR_LEDS_SHIFT		8
#define	FBICSR_HALTEN			0x00000080 /* halt enable */
#define	FBICSR_TSTFNC_MASK		0x0000007e /* test function */
#define	FBICSR_TSTFNC_SHIFT		1
#define	TSTFNC_NORMAL_MODE		0x1f	   /* normal operation */
#define	FBICSR_CDPE			0x00000001 /* CVAX parity enable */

/*
 * I/O Range
 *
 * This programs an M-bus address range which in the global I/O space, which
 * is answered by this module.  Note that the upper bit in the match field
 * must be set, for the address to be in the I/O space; this is why the
 * upper bit of the mask field acts as an enable.
 */
#define	RANGE_MATCH			0xffff0000 /* address bits 31:16 */
#define	RANGE_ENABLE			0x00008000 /* mask address bit 31 */
#define	RANGE_MASK			0x00007fff /* address bits 30:16 */

/*
 * Interprocessor and device interrupts
 */
#define	IPDVINT_IPL17			0x08000000 /* trigger IRQ3 */
#define	IPDVINT_IPL16			0x04000000 /* trigger IRQ2 */
#define	IPDVINT_IPL15			0x02000000 /* trigger IRQ1 */
#define	IPDVINT_IPL14			0x01000000 /* trigger IRQ0 */
#define	IPDVINT_IPUNIT			0x00020000 /* interrupts CPU */
#define	IPDVINT_DEVICE			0x00010000 /* interrupts M-bus */
#define	IPDVINT_VECTOR_MASK		0x0000fff0 /* interrupt vector */
#define	IPDVINT_VECTOR_SHIFT		4
#define	IPDVINT_IPL_MASK		0x0000000c /* interrupt ipl */
#define	IPDVINT_IPL_SHIFT		2

/*
 * CPUID (also EPR 14)
 */
#define	CPUID_MID_MASK			0x0000001c /* slot mid */
#define	CPUID_MID_SHIFT			2
#define	CPUID_PROC_MASK			0x00000003 /* slot processor id */
#define	CPUID_PROC_SHIFT		0
#define	CPUID_PROC_0			0x00
#define	CPUID_PROC_1			0x03

/*
 * FMCM registers (not FBIC compatible except for MODTYPE and BUSCSR)
 */

/* module identification */
#define	FMCM_MODTYPE			0x1fc
/* M-bus error status */
/* NOTE: only implements FRZN, ICMD, MDPE, MSPE and MCPE */
#define	FMCM_BUSCSR			0x1f8
/* FMCM control and status register */
#define	FMCM_FMDCSR			0x1f4
/* Memory space base address register */
#define	FMCM_BASEADDR			0x1f0

#define	FMDCSR_ISOLATE			0x00008000 /* no MABORT on error */
#define	FMDCSR_DIAGNOSTIC_REFRESH_START	0x00004000
#define	FMDCSR_REFRESH_PERIOD_SELECT	0x00002000 /* set: slow (80ns) */
#define	FMDCSR_DISABLE_REFRESH		0x00001000

#define	BASEADDR_STARTADDR_MASK		0x7ff00000
#define	BASEADDR_MEMORY_SPACE_ENABLE	0x80000000

/*
 * Interrupt vector assignments
 *
 * Since each FBIC controls four interrupts, and passes the IPI in bits
 * 3:2 of the vector number, we have to reserve them on 0x10 boundaries.
 *
 * Note that this is different from the usual scheme of using bits 5:4
 * for this purpose.
 *
 * CPU boards also use the IPDVINT to have an extra 0x10 range for IPIs.
 *
 * To make things simpler, we use a static assignment where the number is
 * computed from the mid and fbic number (for cpu boards).
 *
 * This means the 0x100..0x1fc range is used for M-bus interrupts only.
 * Q-bus interrupts will use the usual 0x200..0x3fc range.
 */

#define	MBUS_VECTOR_BASE(mid,fbic)	(0x100 + (mid) * 0x20 + (fbic) * 0x10)
#define	MBUS_VECTOR_TO_MID(vec)		(((vec) - 0x100) >> 5)
#define	MBUS_VECTOR_TO_FBIC(vec)	(((vec) & 0x10) >> 4)
#define	MBUS_VECTOR_TO_IRQ(vec)		(((vec) & 0xc) >> 2)
