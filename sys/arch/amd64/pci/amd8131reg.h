/*	$OpenBSD: amd8131reg.h,v 1.1 2005/05/24 00:51:27 brad Exp $	*/
/*	$NetBSD: amd8131reg.h,v 1.1 2004/04/18 18:34:22 fvdl Exp $	*/

/*
 * Some register definitions for the AMD 8131 PCI-X Tunnel / IO apic.
 * Only the registers/bits that are currently used are defined here.
 */

#define AMD8131_PCIX_MISC	0x40
#	define AMD8131_NIOAMODE		0x00000001

#define AMD8131_IOAPIC_CTL	0x44
#	define AMD8131_IOAEN		0x00000002
