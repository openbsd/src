/*	$OpenBSD: pcibios.h,v 1.2 2000/03/27 08:35:22 brad Exp $	*/
/*	$NetBSD$	*/

/*
 * Copyright (c) 1999, by UCHIYAMA Yasushi
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. The name of the developer may NOT be used to endorse or promote products
 *    derived from this software without specific prior written permission.
 * 
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND 
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE 
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE 
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE 
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL 
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS 
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) 
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT 
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY 
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF 
 * SUCH DAMAGE. 
 */

/*
 * Data structure definitions for the PCI BIOS interface.
 */

/*
 * PCI BIOS return codes.
 */
#define	PCIBIOS_SUCCESS			0x00
#define	PCIBIOS_SERVICE_NOT_PRESENT	0x80
#define	PCIBIOS_FUNCTION_NOT_SUPPORTED	0x81
#define	PCIBIOS_BAD_VENDOR_ID		0x83
#define	PCIBIOS_DEVICE_NOT_FOUND	0x86
#define	PCIBIOS_BAD_REGISTER_NUMBER	0x87
#define	PCIBIOS_SET_FAILED		0x88
#define	PCIBIOS_BUFFER_TOO_SMALL	0x89

/*
 * PCI IRQ Routing Table definitions.
 */

/*
 * Slot entry (per PCI 2.1)
 */
struct pcibios_linkmap {
	u_int8_t	link;
	u_int16_t	bitmap;
} __attribute__((__packed__));

struct pcibios_intr_routing {
	u_int8_t	bus;
	u_int8_t	device;
	struct pcibios_linkmap linkmap[4];	/* INT[A:D]# */
	u_int8_t	slot;
	u_int8_t	reserved;
} __attribute__((__packed__));

/*
 * $PIR header.  Reference:
 *
 *	http://www.microsoft.com/HWDEV/busbios/PCIIRQ.htm
 */
struct pcibios_pir_header {
	u_int32_t	signature;		/* $PIR */
	u_int16_t	version;
	u_int16_t	tablesize;
	u_int8_t	router_bus;
	u_int8_t	router_devfunc;
	u_int16_t	exclusive_irq;
	u_int32_t	compat_router;		/* PCI vendor/product */
	u_int32_t	miniport;
	u_int8_t	reserved[11];
	u_int8_t	checksum;
} __attribute__((__packed__));

void	pcibios_init __P((void));

extern struct pcibios_pir_header pcibios_pir_header;
extern struct pcibios_intr_routing *pcibios_pir_table;
extern int pcibios_pir_table_nentries;
extern int pcibios_max_bus;
