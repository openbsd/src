/*	$OpenBSD: pucdata.c,v 1.25 2003/02/11 19:20:28 mickey Exp $	*/
/*	$NetBSD: pucdata.c,v 1.6 1999/07/03 05:55:23 cgd Exp $	*/

/*
 * Copyright (c) 1998, 1999 Christopher G. Demetriou.  All rights reserved.
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
 *      This product includes software developed by Christopher G. Demetriou
 *	for the NetBSD Project.
 * 4. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

/*
 * PCI "universal" communications card driver configuration data (used to
 * match/attach the cards).
 */

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/device.h>

#include <dev/pci/pcireg.h>
#include <dev/pci/pcivar.h>
#include <dev/pci/pucvar.h>
#include <dev/pci/pcidevs.h>
#include <dev/ic/comreg.h>

const struct puc_device_description puc_devices[] = {
	/*
	 * XXX no entry because I have no data:
	 * XXX Dolphin Peripherals 4006 (single parallel)
	 */

	/*
	 * Dolphin Peripherals 4014 (dual parallel port) card.  PLX 9050, with
	 * a seemingly-lame EEPROM setup that puts the Dolphin IDs
	 * into the subsystem fields, and claims that it's a
	 * network/misc (0x02/0x80) device.
	 */
	{   /* "Dolphin Peripherals 4014" */
	    {	PCI_VENDOR_PLX,	PCI_PRODUCT_PLX_9050,	0xd84d,	0x6810	},
	    {	0xffff,	0xffff,				0xffff,	0xffff	},
	    {
		{ PUC_PORT_TYPE_LPT, 0x20, 0x00 },
		{ PUC_PORT_TYPE_LPT, 0x24, 0x00 },
	    },
	},

	/*
	 * XXX no entry because I have no data:
	 * XXX Dolphin Peripherals 4025 (single serial)
	 */

	/*
	 * Dolphin Peripherals 4035 (dual serial port) card.  PLX 9050, with
	 * a seemingly-lame EEPROM setup that puts the Dolphin IDs
	 * into the subsystem fields, and claims that it's a
	 * network/misc (0x02/0x80) device.
	 */
	{   /* "Dolphin Peripherals 4035" */
	    {	PCI_VENDOR_PLX, PCI_PRODUCT_PLX_9050,	0xd84d,	0x6808	},
	    {	0xffff,	0xffff,				0xffff,	0xffff	},
	    {
		{ PUC_PORT_TYPE_COM, 0x18, 0x00, COM_FREQ },
		{ PUC_PORT_TYPE_COM, 0x1c, 0x00, COM_FREQ },
	    },
	},

	/*
	 * XXX no entry because I have no data:
	 * XXX Dolphin Peripherals 4078 (dual serial and single parallel)
	 */


	/*
	 * SIIG Boards.
	 *
	 * SIIG provides documentation for their boards at:
	 * <URL:http://www.siig.com/driver.htm>
	 *
	 * Please excuse the weird ordering, it's the order they
	 * use in their documentation.
	 */

	/*
	 * SIIG "10x" family boards.
	 */

	/* SIIG Cyber Serial PCI 16C550 (10x family): 1S */
	{   /* "SIIG Cyber Serial PCI 16C550 (10x family)", */
	    {	PCI_VENDOR_SIIG, PCI_PRODUCT_SIIG_1000,		0, 0	},
	    {	0xffff,	0xffff,					0, 0	},
	    {
		{ PUC_PORT_TYPE_COM, 0x18, 0x00, COM_FREQ },
	    },
	},

	/* SIIG Cyber Serial PCI 16C650 (10x family): 1S */
	{   /* "SIIG Cyber Serial PCI 16C650 (10x family)", */
	    {	PCI_VENDOR_SIIG, PCI_PRODUCT_SIIG_1001,		0, 0	},
	    {	0xffff,	0xffff,					0, 0	},
	    {
		{ PUC_PORT_TYPE_COM, 0x18, 0x00, COM_FREQ },
	    },
	},

	/* SIIG Cyber Serial PCI 16C850 (10x family): 1S */
	{   /* "SIIG Cyber Serial PCI 16C850 (10x family)", */
	    {	PCI_VENDOR_SIIG, PCI_PRODUCT_SIIG_1002,		0, 0	},
	    {	0xffff,	0xffff,					0, 0	},
	    {
		{ PUC_PORT_TYPE_COM, 0x18, 0x00, COM_FREQ },
	    },
	},

	/* SIIG Cyber I/O PCI 16C550 (10x family): 1S, 1P */
	{   /* "SIIG Cyber I/O PCI 16C550 (10x family)", */
	    {	PCI_VENDOR_SIIG, PCI_PRODUCT_SIIG_1010,		0, 0	},
	    {	0xffff,	0xffff,					0, 0	},
	    {
		{ PUC_PORT_TYPE_COM, 0x18, 0x00, COM_FREQ },
		{ PUC_PORT_TYPE_LPT, 0x1c, 0x00 },
	    },
	},

	/* SIIG Cyber I/O PCI 16C650 (10x family): 1S, 1P */
	{   /* "SIIG Cyber I/O PCI 16C650 (10x family)", */
	    {	PCI_VENDOR_SIIG, PCI_PRODUCT_SIIG_1011,		0, 0	},
	    {	0xffff,	0xffff,					0, 0	},
	    {
		{ PUC_PORT_TYPE_COM, 0x18, 0x00, COM_FREQ },
		{ PUC_PORT_TYPE_LPT, 0x1c, 0x00 },
	    },
	},

	/* SIIG Cyber I/O PCI 16C850 (10x family): 1S, 1P */
	{   /* "SIIG Cyber I/O PCI 16C850 (10x family)", */
	    {	PCI_VENDOR_SIIG, PCI_PRODUCT_SIIG_1012,		0, 0	},
	    {	0xffff,	0xffff,					0, 0	},
	    {
		{ PUC_PORT_TYPE_COM, 0x18, 0x00, COM_FREQ },
		{ PUC_PORT_TYPE_LPT, 0x1c, 0x00 },
	    },
	},

	/* SIIG Cyber Parallel PCI (10x family): 1P */
	{   /* "SIIG Cyber Parallel PCI (10x family)", */
	    {	PCI_VENDOR_SIIG, PCI_PRODUCT_SIIG_1020,		0, 0	},
	    {	0xffff,	0xffff,					0, 0	},
	    {
		{ PUC_PORT_TYPE_LPT, 0x18, 0x00 },
	    },
	},

	/* SIIG Cyber Parallel Dual PCI (10x family): 2P */
	{   /* "SIIG Cyber Parallel Dual PCI (10x family)", */
	    {	PCI_VENDOR_SIIG, PCI_PRODUCT_SIIG_1021,		0, 0	},
	    {	0xffff,	0xffff,					0, 0	},
	    {
		{ PUC_PORT_TYPE_LPT, 0x18, 0x00 },
		{ PUC_PORT_TYPE_LPT, 0x20, 0x00 },
	    },
	},

	/* SIIG Cyber Serial Dual PCI 16C550 (10x family): 2S */
	{   /* "SIIG Cyber Serial Dual PCI 16C550 (10x family)", */
	    {	PCI_VENDOR_SIIG, PCI_PRODUCT_SIIG_1030,		0, 0	},
	    {	0xffff,	0xffff,					0, 0	},
	    {
		{ PUC_PORT_TYPE_COM, 0x18, 0x00, COM_FREQ },
		{ PUC_PORT_TYPE_COM, 0x1c, 0x00, COM_FREQ },
	    },
	},

	/* SIIG Cyber Serial Dual PCI 16C650 (10x family): 2S */
	{   /* "SIIG Cyber Serial Dual PCI 16C650 (10x family)", */
	    {	PCI_VENDOR_SIIG, PCI_PRODUCT_SIIG_1031,		0, 0	},
	    {	0xffff,	0xffff,					0, 0	},
	    {
		{ PUC_PORT_TYPE_COM, 0x18, 0x00, COM_FREQ },
		{ PUC_PORT_TYPE_COM, 0x1c, 0x00, COM_FREQ },
	    },
	},

	/* SIIG Cyber Serial Dual PCI 16C850 (10x family): 2S */
	{   /* "SIIG Cyber Serial Dual PCI 16C850 (10x family)", */
	    {	PCI_VENDOR_SIIG, PCI_PRODUCT_SIIG_1032,		0, 0	},
	    {	0xffff,	0xffff,					0, 0	},
	    {
		{ PUC_PORT_TYPE_COM, 0x18, 0x00, COM_FREQ },
		{ PUC_PORT_TYPE_COM, 0x1c, 0x00, COM_FREQ },
	    },
	},

	/* SIIG Cyber 2S1P PCI 16C550 (10x family): 2S, 1P */
	{   /* "SIIG Cyber 2S1P PCI 16C550 (10x family)", */
	    {	PCI_VENDOR_SIIG, PCI_PRODUCT_SIIG_1034,		0, 0	},
	    {	0xffff,	0xffff,					0, 0	},
	    {
		{ PUC_PORT_TYPE_COM, 0x18, 0x00, COM_FREQ },
		{ PUC_PORT_TYPE_COM, 0x1c, 0x00, COM_FREQ },
		{ PUC_PORT_TYPE_LPT, 0x20, 0x00 },
	    },
	},

	/* SIIG Cyber 2S1P PCI 16C650 (10x family): 2S, 1P */
	{   /* "SIIG Cyber 2S1P PCI 16C650 (10x family)", */
	    {	PCI_VENDOR_SIIG, PCI_PRODUCT_SIIG_1035,		0, 0	},
	    {	0xffff,	0xffff,					0, 0	},
	    {
		{ PUC_PORT_TYPE_COM, 0x18, 0x00, COM_FREQ },
		{ PUC_PORT_TYPE_COM, 0x1c, 0x00, COM_FREQ },
		{ PUC_PORT_TYPE_LPT, 0x20, 0x00 },
	    },
	},

	/* SIIG Cyber 2S1P PCI 16C850 (10x family): 2S, 1P */
	{   /* "SIIG Cyber 2S1P PCI 16C850 (10x family)", */
	    {	PCI_VENDOR_SIIG, PCI_PRODUCT_SIIG_1036,		0, 0	},
	    {	0xffff,	0xffff,					0, 0	},
	    {
		{ PUC_PORT_TYPE_COM, 0x18, 0x00, COM_FREQ },
		{ PUC_PORT_TYPE_COM, 0x1c, 0x00, COM_FREQ },
		{ PUC_PORT_TYPE_LPT, 0x20, 0x00 },
	    },
	},

	/* SIIG Cyber 4S PCI 16C550 (10x family): 4S */
	{   /* "SIIG Cyber 4S PCI 16C550 (10x family)", */
	    {	PCI_VENDOR_SIIG, PCI_PRODUCT_SIIG_1050,		0, 0	},
	    {	0xffff,	0xffff,					0, 0	},
	    {
		{ PUC_PORT_TYPE_COM, 0x18, 0x00, COM_FREQ },
		{ PUC_PORT_TYPE_COM, 0x1c, 0x00, COM_FREQ },
		{ PUC_PORT_TYPE_COM, 0x20, 0x00, COM_FREQ },
		{ PUC_PORT_TYPE_COM, 0x24, 0x00, COM_FREQ },
	    },
	},

	/* SIIG Cyber 4S PCI 16C650 (10x family): 4S */
	{   /* "SIIG Cyber 4S PCI 16C650 (10x family)", */
	    {	PCI_VENDOR_SIIG, PCI_PRODUCT_SIIG_1051,		0, 0	},
	    {	0xffff,	0xffff,					0, 0	},
	    {
		{ PUC_PORT_TYPE_COM, 0x18, 0x00, COM_FREQ },
		{ PUC_PORT_TYPE_COM, 0x1c, 0x00, COM_FREQ },
		{ PUC_PORT_TYPE_COM, 0x20, 0x00, COM_FREQ },
		{ PUC_PORT_TYPE_COM, 0x24, 0x00, COM_FREQ },
	    },
	},

	/* SIIG Cyber 4S PCI 16C850 (10x family): 4S */
	{   /* "SIIG Cyber 4S PCI 16C850 (10x family)", */
	    {	PCI_VENDOR_SIIG, PCI_PRODUCT_SIIG_1052,		0, 0	},
	    {	0xffff,	0xffff,					0, 0	},
	    {
		{ PUC_PORT_TYPE_COM, 0x18, 0x00, COM_FREQ },
		{ PUC_PORT_TYPE_COM, 0x1c, 0x00, COM_FREQ },
		{ PUC_PORT_TYPE_COM, 0x20, 0x00, COM_FREQ },
		{ PUC_PORT_TYPE_COM, 0x24, 0x00, COM_FREQ },
	    },
	},

	/*
	 * SIIG "20x" family boards.
	 */

	/* SIIG Cyber Parallel PCI (20x family): 1P */
	{   /* "SIIG Cyber Parallel PCI (20x family)", */
	    {	PCI_VENDOR_SIIG, PCI_PRODUCT_SIIG_2020,		0, 0	},
	    {	0xffff,	0xffff,					0, 0	},
	    {
		{ PUC_PORT_TYPE_LPT, 0x10, 0x00 },
	    },
	},

	/* SIIG Cyber Parallel Dual PCI (20x family): 2P */
	{   /* "SIIG Cyber Parallel Dual PCI (20x family)", */
	    {	PCI_VENDOR_SIIG, PCI_PRODUCT_SIIG_2021,		0, 0	},
	    {	0xffff,	0xffff,					0, 0	},
	    {
		{ PUC_PORT_TYPE_LPT, 0x10, 0x00 },
		{ PUC_PORT_TYPE_LPT, 0x18, 0x00 },
	    },
	},

	/* SIIG Cyber 2P1S PCI 16C550 (20x family): 1S, 2P */
	{   /* "SIIG Cyber 2P1S PCI 16C550 (20x family)", */
	    {	PCI_VENDOR_SIIG, PCI_PRODUCT_SIIG_2040,		0, 0	},
	    {	0xffff,	0xffff,					0, 0	},
	    {
		{ PUC_PORT_TYPE_COM, 0x10, 0x00, COM_FREQ },
		{ PUC_PORT_TYPE_LPT, 0x14, 0x00 },
		{ PUC_PORT_TYPE_LPT, 0x1c, 0x00 },
	    },
	},

	/* SIIG Cyber 2P1S PCI 16C650 (20x family): 1S, 2P */
	{   /* "SIIG Cyber 2P1S PCI 16C650 (20x family)", */
	    {	PCI_VENDOR_SIIG, PCI_PRODUCT_SIIG_2041,		0, 0	},
	    {	0xffff,	0xffff,					0, 0	},
	    {
		{ PUC_PORT_TYPE_COM, 0x10, 0x00, COM_FREQ },
		{ PUC_PORT_TYPE_LPT, 0x14, 0x00 },
		{ PUC_PORT_TYPE_LPT, 0x1c, 0x00 },
	    },
	},

	/* SIIG Cyber 2P1S PCI 16C850 (20x family): 1S, 2P */
	{   /* "SIIG Cyber 2P1S PCI 16C850 (20x family)", */
	    {	PCI_VENDOR_SIIG, PCI_PRODUCT_SIIG_2042,		0, 0	},
	    {	0xffff,	0xffff,					0, 0	},
	    {
		{ PUC_PORT_TYPE_COM, 0x10, 0x00, COM_FREQ },
		{ PUC_PORT_TYPE_LPT, 0x14, 0x00 },
		{ PUC_PORT_TYPE_LPT, 0x1c, 0x00 },
	    },
	},

	/* SIIG Cyber Serial PCI 16C550 (20x family): 1S */
	{   /* "SIIG Cyber Serial PCI 16C550 (20x family)", */
	    {	PCI_VENDOR_SIIG, PCI_PRODUCT_SIIG_2000,		0, 0	},
	    {	0xffff,	0xffff,					0, 0	},
	    {
		{ PUC_PORT_TYPE_COM, 0x10, 0x00, COM_FREQ },
	    },
	},

	/* SIIG Cyber Serial PCI 16C650 (20x family): 1S */
	{   /* "SIIG Cyber Serial PCI 16C650 (20x family)", */
	    {	PCI_VENDOR_SIIG, PCI_PRODUCT_SIIG_2001,		0, 0	},
	    {	0xffff,	0xffff,					0, 0	},
	    {
		{ PUC_PORT_TYPE_COM, 0x10, 0x00, COM_FREQ },
	    },
	},

	/* SIIG Cyber Serial PCI 16C850 (20x family): 1S */
	{   /* "SIIG Cyber Serial PCI 16C850 (20x family)", */
	    {	PCI_VENDOR_SIIG, PCI_PRODUCT_SIIG_2002,		0, 0	},
	    {	0xffff,	0xffff,					0, 0	},
	    {
		{ PUC_PORT_TYPE_COM, 0x10, 0x00, COM_FREQ },
	    },
	},

	/* SIIG Cyber I/O PCI 16C550 (20x family): 1S, 1P */
	{   /* "SIIG Cyber I/O PCI 16C550 (20x family)", */
	    {	PCI_VENDOR_SIIG, PCI_PRODUCT_SIIG_2010,		0, 0	},
	    {	0xffff,	0xffff,					0, 0	},
	    {
		{ PUC_PORT_TYPE_COM, 0x10, 0x00, COM_FREQ },
		{ PUC_PORT_TYPE_LPT, 0x14, 0x00 },
	    },
	},

	/* SIIG Cyber I/O PCI 16C650 (20x family): 1S, 1P */
	{   /* "SIIG Cyber I/O PCI 16C650 (20x family)", */
	    {	PCI_VENDOR_SIIG, PCI_PRODUCT_SIIG_2011,		0, 0	},
	    {	0xffff,	0xffff,					0, 0	},
	    {
		{ PUC_PORT_TYPE_COM, 0x10, 0x00, COM_FREQ },
		{ PUC_PORT_TYPE_LPT, 0x14, 0x00 },
	    },
	},

	/* SIIG Cyber I/O PCI 16C850 (20x family): 1S, 1P */
	{   /* "SIIG Cyber I/O PCI 16C850 (20x family)", */
	    {	PCI_VENDOR_SIIG, PCI_PRODUCT_SIIG_2012,		0, 0	},
	    {	0xffff,	0xffff,					0, 0	},
	    {
		{ PUC_PORT_TYPE_COM, 0x10, 0x00, COM_FREQ },
		{ PUC_PORT_TYPE_LPT, 0x14, 0x00 },
	    },
	},

	/* SIIG Cyber Serial Dual PCI 16C550 (20x family): 2S */
	{   /* "SIIG Cyber Serial Dual PCI 16C550 (20x family)", */
	    {	PCI_VENDOR_SIIG, PCI_PRODUCT_SIIG_2030,		0, 0	},
	    {	0xffff,	0xffff,					0, 0	},
	    {
		{ PUC_PORT_TYPE_COM, 0x10, 0x00, COM_FREQ },
		{ PUC_PORT_TYPE_COM, 0x14, 0x00, COM_FREQ },
	    },
	},

	/* SIIG Cyber Serial Dual PCI 16C650 (20x family): 2S */
	{   /* "SIIG Cyber Serial Dual PCI 16C650 (20x family)", */
	    {	PCI_VENDOR_SIIG, PCI_PRODUCT_SIIG_2031,		0, 0	},
	    {	0xffff,	0xffff,					0, 0	},
	    {
		{ PUC_PORT_TYPE_COM, 0x10, 0x00, COM_FREQ },
		{ PUC_PORT_TYPE_COM, 0x14, 0x00, COM_FREQ },
	    },
	},

	/* SIIG Cyber Serial Dual PCI 16C850 (20x family): 2S */
	{   /* "SIIG Cyber Serial Dual PCI 16C850 (20x family)", */
	    {	PCI_VENDOR_SIIG, PCI_PRODUCT_SIIG_2032,		0, 0	},
	    {	0xffff,	0xffff,					0, 0	},
	    {
		{ PUC_PORT_TYPE_COM, 0x10, 0x00, COM_FREQ },
		{ PUC_PORT_TYPE_COM, 0x14, 0x00, COM_FREQ },
	    },
	},

	/* SIIG Cyber 2S1P PCI 16C550 (20x family): 2S, 1P */
	{   /* "SIIG Cyber 2S1P PCI 16C550 (20x family)", */
	    {	PCI_VENDOR_SIIG, PCI_PRODUCT_SIIG_2060,		0, 0	},
	    {	0xffff,	0xffff,					0, 0	},
	    {
		{ PUC_PORT_TYPE_COM, 0x10, 0x00, COM_FREQ },
		{ PUC_PORT_TYPE_COM, 0x14, 0x00, COM_FREQ },
		{ PUC_PORT_TYPE_LPT, 0x18, 0x00 },
	    },
	},

	/* SIIG Cyber 2S1P PCI 16C650 (20x family): 2S, 1P */
	{   /* "SIIG Cyber 2S1P PCI 16C650 (20x family)", */
	    {	PCI_VENDOR_SIIG, PCI_PRODUCT_SIIG_2061,		0, 0	},
	    {	0xffff,	0xffff,					0, 0	},
	    {
		{ PUC_PORT_TYPE_COM, 0x10, 0x00, COM_FREQ },
		{ PUC_PORT_TYPE_COM, 0x14, 0x00, COM_FREQ },
		{ PUC_PORT_TYPE_LPT, 0x18, 0x00 },
	    },
	},

	/* SIIG Cyber 2S1P PCI 16C850 (20x family): 2S, 1P */
	{   /* "SIIG Cyber 2S1P PCI 16C850 (20x family)", */
	    {	PCI_VENDOR_SIIG, PCI_PRODUCT_SIIG_2062,		0, 0	},
	    {	0xffff,	0xffff,					0, 0	},
	    {
		{ PUC_PORT_TYPE_COM, 0x10, 0x00, COM_FREQ },
		{ PUC_PORT_TYPE_COM, 0x14, 0x00, COM_FREQ },
		{ PUC_PORT_TYPE_LPT, 0x18, 0x00 },
	    },
	},

	/* SIIG Cyber 4S PCI 16C550 (20x family): 4S */
	{   /* "SIIG Cyber 4S PCI 16C550 (20x family)", */
	    {	PCI_VENDOR_SIIG, PCI_PRODUCT_SIIG_2050,		0, 0	},
	    {	0xffff,	0xffff,					0, 0	},
	    {
		{ PUC_PORT_TYPE_COM, 0x10, 0x00, COM_FREQ },
		{ PUC_PORT_TYPE_COM, 0x14, 0x00, COM_FREQ },
		{ PUC_PORT_TYPE_COM, 0x18, 0x00, COM_FREQ },
		{ PUC_PORT_TYPE_COM, 0x1c, 0x00, COM_FREQ },
	    },
	},

	/* SIIG Cyber 4S PCI 16C650 (20x family): 4S */
	{   /* "SIIG Cyber 4S PCI 16C650 (20x family)", */
	    {	PCI_VENDOR_SIIG, PCI_PRODUCT_SIIG_2051,		0, 0	},
	    {	0xffff,	0xffff,					0, 0	},
	    {
		{ PUC_PORT_TYPE_COM, 0x10, 0x00, COM_FREQ },
		{ PUC_PORT_TYPE_COM, 0x14, 0x00, COM_FREQ },
		{ PUC_PORT_TYPE_COM, 0x18, 0x00, COM_FREQ },
		{ PUC_PORT_TYPE_COM, 0x1c, 0x00, COM_FREQ },
	    },
	},

	/* SIIG Cyber 4S PCI 16C850 (20x family): 4S */
	{   /* "SIIG Cyber 4S PCI 16C850 (20x family)", */
	    {	PCI_VENDOR_SIIG, PCI_PRODUCT_SIIG_2052,		0, 0	},
	    {	0xffff,	0xffff,					0, 0	},
	    {
		{ PUC_PORT_TYPE_COM, 0x10, 0x00, COM_FREQ },
		{ PUC_PORT_TYPE_COM, 0x14, 0x00, COM_FREQ },
		{ PUC_PORT_TYPE_COM, 0x18, 0x00, COM_FREQ },
		{ PUC_PORT_TYPE_COM, 0x1c, 0x00, COM_FREQ },
	    },
	},

	/* SIIG Cyber 8S PCI 16C850 (20x family): 8S */
	{   /* "SIIG Cyber 8S PCI 16C850 (20x family)", */
	    {	PCI_VENDOR_SIIG, PCI_PRODUCT_SIIG_2081,		0, 0	},
	    {	0xffff, 0xffff,					0, 0	},
	    {
		{ PUC_PORT_TYPE_COM, 0x10, 0x00, COM_FREQ },
		{ PUC_PORT_TYPE_COM, 0x14, 0x00, COM_FREQ },
		{ PUC_PORT_TYPE_COM, 0x18, 0x00, COM_FREQ },
		{ PUC_PORT_TYPE_COM, 0x1c, 0x00, COM_FREQ },
		{ PUC_PORT_TYPE_COM, 0x20, 0x00, COM_FREQ },
		{ PUC_PORT_TYPE_COM, 0x20, 0x08, COM_FREQ },
		{ PUC_PORT_TYPE_COM, 0x20, 0x10, COM_FREQ },
		{ PUC_PORT_TYPE_COM, 0x20, 0x18, COM_FREQ },
	    },
	},

	/*
	 * VScom PCI-800, as sold on http://www.swann.com.au/isp/titan.html.
	 * Some PLX chip.  Note: This board has a software selectable(?)
	 * clock multiplier which this driver doesn't support, so you'll
	 * have to use an appropriately scaled baud rate when talking to
	 * the card.
	 */
	{   /* "VScom PCI-800", */
	    {	PCI_VENDOR_PLX,	PCI_PRODUCT_PLX_1076,	0x10b5,	0x1076	},
	    {	0xffff,	0xffff,				0xffff,	0xffff	},
	    {
		{ PUC_PORT_TYPE_COM, 0x18, 0x00, COM_FREQ },
		{ PUC_PORT_TYPE_COM, 0x18, 0x08, COM_FREQ },
		{ PUC_PORT_TYPE_COM, 0x18, 0x10, COM_FREQ },
		{ PUC_PORT_TYPE_COM, 0x18, 0x18, COM_FREQ },
		{ PUC_PORT_TYPE_COM, 0x18, 0x20, COM_FREQ },
		{ PUC_PORT_TYPE_COM, 0x18, 0x28, COM_FREQ },
		{ PUC_PORT_TYPE_COM, 0x18, 0x30, COM_FREQ },
		{ PUC_PORT_TYPE_COM, 0x18, 0x38, COM_FREQ },
	    },
	},

	/*
	* VScom PCI 011H, 1 lpt.
	*/
	{   /* "VScom PCI-011H", */
	    {	PCI_VENDOR_OXFORD2, PCI_PRODUCT_OXFORD2_VSCOM_PCI011H,	0, 0 },
	    {	0xffff, 0xffff,						0, 0 },
	    {
		{ PUC_PORT_TYPE_LPT, 0x10, 0x00 },
	    },
	},

	/*
	 * VScom PCI x10H, 1 lpt.
	 * is the lpt part of VScom 110H, 210H, 410H
	 */
	{   /* "VScom PCI-x10H", */
	    {	PCI_VENDOR_OXFORD, PCI_PRODUCT_OXFORD_VSCOM_PCIx10H,	0, 0 },
	    {	0xffff, 0xffff,						0, 0 },
	    {
		{ PUC_PORT_TYPE_LPT, 0x10, 0x00 },
	    },
	},

	/*
	 * VScom PCI 100H, little sister of 800H, 1 com.
	 * also com part of VScom 110H
	 * The one I have defaults to a fequency of 14.7456 MHz which is
	 * jumper J1 set to 2-3.
	 */
	{   /* "VScom PCI-100H", */
	    {	PCI_VENDOR_OXFORD, PCI_PRODUCT_OXFORD_VSCOM_PCI100H,	0, 0 },
	    {	0xffff, 0xffff,						0, 0 },
	    {
		{ PUC_PORT_TYPE_COM, 0x10, 0x00, COM_FREQ * 8 },
	    },
	},

	/*
	 * VScom PCI 200H, little sister of 800H, 2 com.
	 * also com part of VScom 210H
	 * The one I have defaults to a fequency of 14.7456 MHz which is
	 * jumper J1 set to 2-3.
	 */

	{   /* "VScom PCI-200H", */
	    {	PCI_VENDOR_OXFORD, PCI_PRODUCT_OXFORD_VSCOM_PCI200H,	0, 0 },
	    {	0xffff, 0xffff,						0, 0 },
	    {
		{ PUC_PORT_TYPE_COM, 0x10, 0x00, COM_FREQ * 8 },
		{ PUC_PORT_TYPE_COM, 0x10, 0x08, COM_FREQ * 8 },
	    },
	},

	/*
	 * VScom PCI 400H and 800H. Uses 4/8 16950 UART, behind a PCI chips
	 * that offers 4 com port on PCI device 0 (both 400H and 800H)
	 * and 4 on PCI device 1 (800H only). PCI device 0 has
	 * device ID 3 and PCI device 1 device ID 4. Uses a 14.7456 MHz crystal
	 * instead of the standart 1.8432MHz.
	 * There's a version with a jumper for selecting the crystal frequency,
	 * defaults to 8x as used here. The jumperless version uses 8x, too.
	 */
	{   /* "VScom PCI-400H/800H", */
	    {	PCI_VENDOR_OXFORD, PCI_PRODUCT_OXFORD_VSCOM_PCI800H_0,	0, 0 },
	    {	0xffff, 0xffff,						0, 0 },
	    {
		{ PUC_PORT_TYPE_COM, 0x10, 0x00, COM_FREQ * 8 },
		{ PUC_PORT_TYPE_COM, 0x10, 0x08, COM_FREQ * 8 },
		{ PUC_PORT_TYPE_COM, 0x10, 0x10, COM_FREQ * 8 },
		{ PUC_PORT_TYPE_COM, 0x10, 0x18, COM_FREQ * 8 },
	    },
	},
	{   /* "VScom PCI-400H/800H", */
	    {	PCI_VENDOR_OXFORD, PCI_PRODUCT_OXFORD_VSCOM_PCI800H_1,	0, 0 },
	    {	0xffff, 0xffff,						0, 0 },
	    {
		{ PUC_PORT_TYPE_COM, 0x10, 0x00, COM_FREQ * 8 },
		{ PUC_PORT_TYPE_COM, 0x10, 0x08, COM_FREQ * 8 },
		{ PUC_PORT_TYPE_COM, 0x10, 0x10, COM_FREQ * 8 },
		{ PUC_PORT_TYPE_COM, 0x10, 0x18, COM_FREQ * 8 },
	    },
	},

	/*
	 * VScom PCI 200HV2, is 200H Version 2.
	 * Sells as 200H
	 * strange things with the second port, either my card is simply
	 * broken or something really strange is going on.
	 * second port does not work at all. I'll request another one for
	 * further testing.
	 */
	{   /* "VScom PCI-200HV2", */
	    {	PCI_VENDOR_OXFORD, PCI_PRODUCT_OXFORD_VSCOM_PCI200HV2,	0, 0 },
	    {	0xffff, 0xffff,						0, 0 },
	    {
		{ PUC_PORT_TYPE_COM, 0x10, 0x00, COM_FREQ * 8 },
		{ PUC_PORT_TYPE_COM, 0x10, 0x08, COM_FREQ * 8 },
	    },
	},

	/*
	 * VScom PCI 010L
	 * one lpt
	 * untested
	 */
	{   /* "VScom PCI-010L", */
	    {	PCI_VENDOR_OXFORD, PCI_PRODUCT_OXFORD_VSCOM_PCI010L,    0, 0 },
	    {	0xffff, 0xffff,                                         0, 0 },
	    {
		{ PUC_PORT_TYPE_LPT, 0x1c, 0x00 },
	    },
	},

	/*
	 * VScom PCI 100L
	 * one com
	 * The one I have defaults to a fequency of 14.7456 MHz which is
	 * jumper J1 set to 2-3.
	 */
	{   /* "VScom PCI-100L", */
	    {	PCI_VENDOR_OXFORD, PCI_PRODUCT_OXFORD_VSCOM_PCI100L,	0, 0 },
	    {	0xffff, 0xffff,						0, 0 },
	    {
		{ PUC_PORT_TYPE_COM, 0x14, 0x00, COM_FREQ * 8 },
	    },
	},

	/*
	 * VScom PCI 110L
	 * one com, one lpt
	 * untested
	 */
	{   /* "VScom PCI-110L", */
	    {	PCI_VENDOR_OXFORD, PCI_PRODUCT_OXFORD_VSCOM_PCI110L,	0, 0 },
	    {	0xffff, 0xffff,						0, 0 },
	    {
		{ PUC_PORT_TYPE_COM, 0x14, 0x00, COM_FREQ * 8 },
		{ PUC_PORT_TYPE_LPT, 0x1c, 0x00 },
	    },
	},

	/*
	 * VScom PCI-200L has 2 x 16550 UARTS.
	 * The board has a jumper which allows you to select a clock speed
	 * of either 14.7456MHz or 1.8432MHz. By default it runs at 
	 * the fast speed.
	 */
	{   /* "VScom PCI-200L with 2 x 16550 UARTS" */
	    {	PCI_VENDOR_OXFORD, PCI_PRODUCT_OXFORD_VSCOM_PCI200L,	0, 0 },
	    {	0xffff, 0xffff,					    	0, 0 },
	    {
		{ PUC_PORT_TYPE_COM, 0x14, 0x00, COM_FREQ * 8},
		{ PUC_PORT_TYPE_COM, 0x18, 0x00, COM_FREQ * 8},
	    },
	},

	/*
	 * VScom PCI-210L
	 * Has a jumper for frequency selection, defaults to 8x as used here
	 * two com, one lpt
	 */
	{   /* "VScom PCI-210L" */
	    {	PCI_VENDOR_OXFORD, PCI_PRODUCT_OXFORD_VSCOM_PCI210L,	0, 0 },
	    {	0xffff, 0xffff,					    	0, 0 },
	    {
		{ PUC_PORT_TYPE_COM, 0x14, 0x00, COM_FREQ * 8 },
		{ PUC_PORT_TYPE_COM, 0x18, 0x00, COM_FREQ * 8 },
		{ PUC_PORT_TYPE_LPT, 0x1c, 0x00 },
	    },
	},

	/*
	 * VScom PCI 400L
	 * Has a jumper for frequency selection, defaults to 8x as used here
	 * This is equal to J1 in pos 2-3
	 * VendorID mismatch with docs, should be 14d2 (oxford), is 10d2 (molex)
	 */
	{   /* "VScom PCI-400L", */
	    {	PCI_VENDOR_MOLEX, PCI_PRODUCT_MOLEX_VSCOM_PCI400L,	0, 0 },
	    {	0xffff, 0xffff,						0, 0 },
	    {
		{ PUC_PORT_TYPE_COM, 0x14, 0x00, COM_FREQ * 8 },
		{ PUC_PORT_TYPE_COM, 0x18, 0x00, COM_FREQ * 8 },
		{ PUC_PORT_TYPE_COM, 0x20, 0x00, COM_FREQ * 8 },
		{ PUC_PORT_TYPE_COM, 0x20, 0x08, COM_FREQ * 8 },
	    },
	},

	/*
	 * VScom PCI 800L
	 * Has a jumper for frequency selection, defaults to 8x as used here
	 */
	{   /* "VScom PCI-800L", */
	    {	PCI_VENDOR_OXFORD, PCI_PRODUCT_OXFORD_VSCOM_PCI800L,	0, 0 },
	    {	0xffff, 0xffff,						0, 0 },
	    {
		{ PUC_PORT_TYPE_COM, 0x14, 0x00, COM_FREQ * 8 },
		{ PUC_PORT_TYPE_COM, 0x18, 0x00, COM_FREQ * 8 },
		{ PUC_PORT_TYPE_COM, 0x20, 0x00, COM_FREQ * 8 },
		{ PUC_PORT_TYPE_COM, 0x20, 0x08, COM_FREQ * 8 },
		{ PUC_PORT_TYPE_COM, 0x20, 0x10, COM_FREQ * 8 },
		{ PUC_PORT_TYPE_COM, 0x20, 0x18, COM_FREQ * 8 },
		{ PUC_PORT_TYPE_COM, 0x20, 0x20, COM_FREQ * 8 },
		{ PUC_PORT_TYPE_COM, 0x20, 0x28, COM_FREQ * 8 },
	    },
	},

	/*
	 * NEC PK-UG-X001 K56flex PCI Modem card.
	 * NEC MARTH bridge chip and Rockwell RCVDL56ACF/SP using.
	 */
	{   /* "NEC PK-UG-X001 K56flex PCI Modem", */
	    {	PCI_VENDOR_NEC,	PCI_PRODUCT_NEC_MARTH,	0x1033,	0x8014	},
	    {	0xffff,	0xffff,				0xffff,	0xffff	},
	    {
		{ PUC_PORT_TYPE_COM, 0x10, 0x00, COM_FREQ },
	    },
	},

	/* NEC PK-UG-X008 */
	{   /* "NEC PK-UG-X008", */
	    {	PCI_VENDOR_NEC,	PCI_PRODUCT_NEC_PKUG,	0x1033,	0x8012	},
	    {	0xffff,	0xffff,				0xffff,	0xffff	},
	    {
		{ PUC_PORT_TYPE_COM, 0x10, 0x00, COM_FREQ },
	    },
	},

	/* Lava Computers 2SP-PCI (0x8000-0x8003) */
	{   /* "Lava Computers 2SP-PCI parallel port", */
	    {	PCI_VENDOR_LAVA, PCI_PRODUCT_LAVA_TWOSP_1P,	0, 0	},
	    {	0xffff,	0xfffc,					0, 0	},
	    {
		{ PUC_PORT_TYPE_LPT, 0x10, 0x00 },
	    },
	},

	/* Lava Computers 2SP-PCI and Quattro-PCI serial ports */
	{   /* "Lava Computers dual serial port", */
	    {	PCI_VENDOR_LAVA, PCI_PRODUCT_LAVA_TWOSP_2S,	0, 0	},
	    {	0xffff,	0xfffc,					0, 0	},
	    {
		{ PUC_PORT_TYPE_COM, 0x10, 0x00, COM_FREQ },
		{ PUC_PORT_TYPE_COM, 0x14, 0x00, COM_FREQ },
	    },
	},

	/*
         * Lava Computers Quattro-PCI serial ports.
         * A second version of the Quattro-PCI with different PCI ids.
         */
	{   /* "Lava Computers Quattro-PCI 4-port serial", */
	    {	PCI_VENDOR_LAVA, PCI_PRODUCT_LAVA_QUATTRO_AB2,	0, 0	},
	    {	0xffff,	0xfffe,					0, 0	},
	    {
		{ PUC_PORT_TYPE_COM, 0x10, 0x00, COM_FREQ },
		{ PUC_PORT_TYPE_COM, 0x14, 0x00, COM_FREQ },
	    },
	},

	/*
	 * Lava Computers LavaPort-Dual and LavaPort-Quad 4*clock PCI
	 * serial ports.
	 */
	{   /* "Lava Computers high-speed port", */
	    {	PCI_VENDOR_LAVA, PCI_PRODUCT_LAVA_LAVAPORT_0,	0, 0	},
	    {	0xffff,	0xfffc,					0, 0	},
	    {	
		{ PUC_PORT_TYPE_COM, 0x10, 0x00, COM_FREQ*4 },
		{ PUC_PORT_TYPE_COM, 0x14, 0x00, COM_FREQ*4 },
	    },
	},

	/* Lava Computers LavaPort-650 */
	{   /* "Lava Computers high-speed port", */
	    {	PCI_VENDOR_LAVA, PCI_PRODUCT_LAVA_650,		0, 0	},
	    {	0xffff,	0xfffc,					0, 0	},
	    {
		{ PUC_PORT_TYPE_COM, 0x10, 0x00, COM_FREQ*4 },
	    },
	},

	/* Koutech IOFLEX-2S PCI Dual Port Serial, port 1 */
	{   /* "Koutech IOFLEX-2S PCI Dual Port Serial, port 1", */
	    {	PCI_VENDOR_LAVA, PCI_PRODUCT_LAVA_IOFLEX_2S_0,	0, 0	},
	    {	0xffff,	0xfffc,					0, 0	},
	    {
		{ PUC_PORT_TYPE_COM, 0x10, 0x00, COM_FREQ },
	    },
	},

	/* Koutech IOFLEX-2S PCI Dual Port Serial, port 2 */
	{   /* "Koutech IOFLEX-2S PCI Dual Port Serial, port 2", */
	    {	PCI_VENDOR_LAVA, PCI_PRODUCT_LAVA_IOFLEX_2S_1,	0, 0	},
	    {	0xffff,	0xfffc,					0, 0	},
	    {
		{ PUC_PORT_TYPE_COM, 0x10, 0x00, COM_FREQ },
	    },
	},

	/* Lava Computers Octopus-550 serial ports */
	{   /* "Lava Computers Octopus-550 8-port serial", */
	    {   PCI_VENDOR_LAVA, PCI_PRODUCT_LAVA_OCTOPUS550_0,	0, 0	},
	    {   0xffff, 0xfffc,					0, 0	},
	    {
		{ PUC_PORT_TYPE_COM, 0x10, 0x00, COM_FREQ },
		{ PUC_PORT_TYPE_COM, 0x14, 0x00, COM_FREQ },
		{ PUC_PORT_TYPE_COM, 0x18, 0x00, COM_FREQ },
		{ PUC_PORT_TYPE_COM, 0x1c, 0x00, COM_FREQ },
	    },
	},

	/* US Robotics (3Com) PCI Modems */
	{   /* "US Robotics (3Com) 3CP5609 PCI 16550 Modem", */
	    {	PCI_VENDOR_USR, PCI_PRODUCT_USR_3CP5610,	0, 0	},
	    {	0xffff,	0xffff,					0, 0	},
	    {
		{ PUC_PORT_TYPE_COM, 0x10, 0x00, COM_FREQ },
	    },
	},

	/* IBM 33L4618: AT&T/Lucent Venus Modem */
	{   /* "IBM 33L4618: AT&T/Lucent Venus Modem", */
	    /* "Actiontec 56K PCI Master" */
	    {	PCI_VENDOR_LUCENT, PCI_PRODUCT_LUCENT_VENUSMODEM,	0, 0 },
	    {	0xffff,	0xffff,						0, 0 },
	    {
		{ PUC_PORT_TYPE_COM, 0x18, 0x08, COM_FREQ },
	    },
	},

	/* Topic/SmartLink 5634PCV SurfRider */
	{   /* "Topic/SmartLink 5634PCV SurfRider" */
	    {	PCI_VENDOR_TOPIC, PCI_PRODUCT_TOPIC_5634PCV,	0, 0	},
	    {	0xffff,	0xffff,					0, 0	},
	    {
		{ PUC_PORT_TYPE_COM, 0x10, 0x00, COM_FREQ },
	    },
	},

	/* SD-LAB PCI I/O Card 4S2P */
	{   /* "Syba Tech Ltd. PCI-4S2P-550-ECP" */
	    {   PCI_VENDOR_SYBA, PCI_PRODUCT_SYBA_4S2P,		0, 0	},
	    {	0xffff,	0xffff,					0, 0	},
	    {
		{ PUC_PORT_TYPE_COM, 0x10, 0x2e8, COM_FREQ },
		{ PUC_PORT_TYPE_COM, 0x10, 0x2f8, COM_FREQ },
		{ PUC_PORT_TYPE_LPT, 0x10, 0x000, 0x00 },
		{ PUC_PORT_TYPE_COM, 0x10, 0x3e8, COM_FREQ },
		{ PUC_PORT_TYPE_COM, 0x10, 0x3f8, COM_FREQ },
		{ PUC_PORT_TYPE_LPT, 0x10, 0x000, 0x00 },
	    },
	},

	/* Moxa Technologies Co., Ltd. PCI I/O Card 4S RS232/422/485 */
	{   /* "Moxa Technologies, Industio CP-114" */
	    {	PCI_VENDOR_MOXA, PCI_PRODUCT_MOXA_CP114,	0, 0	},
	    {	0xffff,	0xffff,					0, 0	},
	    {
		{ PUC_PORT_TYPE_COM, 0x18, 0x00, COM_FREQ * 8 },
		{ PUC_PORT_TYPE_COM, 0x18, 0x08, COM_FREQ * 8 },
		{ PUC_PORT_TYPE_COM, 0x18, 0x10, COM_FREQ * 8 },
		{ PUC_PORT_TYPE_COM, 0x18, 0x18, COM_FREQ * 8 },
	    },
	},

	/* Moxa Technologies Co., Ltd. PCI I/O Card 8S RS232 */
	{   /* "Moxa Technologies, Industio C168H" */
	    {	PCI_VENDOR_MOXA, PCI_PRODUCT_MOXA_C168H,	0, 0	},
	    {	0xffff,	0xffff,					0, 0	},
	    {
		{ PUC_PORT_TYPE_COM, 0x18, 0x00, COM_FREQ * 8 },
		{ PUC_PORT_TYPE_COM, 0x18, 0x08, COM_FREQ * 8 },
		{ PUC_PORT_TYPE_COM, 0x18, 0x10, COM_FREQ * 8 },
		{ PUC_PORT_TYPE_COM, 0x18, 0x18, COM_FREQ * 8 },
		{ PUC_PORT_TYPE_COM, 0x18, 0x20, COM_FREQ * 8 },
		{ PUC_PORT_TYPE_COM, 0x18, 0x28, COM_FREQ * 8 },
		{ PUC_PORT_TYPE_COM, 0x18, 0x30, COM_FREQ * 8 },
		{ PUC_PORT_TYPE_COM, 0x18, 0x38, COM_FREQ * 8 },
	    },
	},

	/* NetMos 2S1P PCI 16C650 : 2S, 1P */
	{   /* "NetMos NM9835 Dual UART and 1284 Printer port" */
	    {   PCI_VENDOR_NETMOS, PCI_PRODUCT_NETMOS_2S1P,	0, 0	},
	    {	0xffff,	0xffff,					0, 0	},
	    {
		{ PUC_PORT_TYPE_COM, 0x10, 0x00, COM_FREQ },
		{ PUC_PORT_TYPE_COM, 0x14, 0x00, COM_FREQ },
		{ PUC_PORT_TYPE_LPT, 0x18, 0x00, 0x00 },
	    },
	},

	{ /* Sunix 4018A : 2-port parallel */
	    {   PCI_VENDOR_SUNIX, PCI_PRODUCT_SUNIX_4018A,	0, 0	},
	    {	0xffff,	0xffff,					0, 0	},
	    {
                { PUC_PORT_TYPE_LPT, 0x10, 0x00 },                              
                { PUC_PORT_TYPE_LPT, 0x18, 0x00 },                              
	    },
	},

	/* Sunix 4065A : 8-port */
	{   /* "Sunix 4065A : 8-port" */
	    {   PCI_VENDOR_SUNIX, PCI_PRODUCT_SUNIX_4065A,	0, 0	},
	    {	0xffff,	0xffff,					0, 0	},
	    {
		{ PUC_PORT_TYPE_COM, 0x10, 0x00, COM_FREQ * 8 },
		{ PUC_PORT_TYPE_COM, 0x10, 0x08, COM_FREQ * 8 },
		{ PUC_PORT_TYPE_COM, 0x14, 0x00, COM_FREQ * 8 },
		{ PUC_PORT_TYPE_COM, 0x14, 0x08, COM_FREQ * 8 },
		{ PUC_PORT_TYPE_COM, 0x18, 0x00, COM_FREQ * 8 },
		{ PUC_PORT_TYPE_COM, 0x18, 0x00, COM_FREQ * 8 },
		{ PUC_PORT_TYPE_COM, 0x1A, 0x00, COM_FREQ * 8 },
		{ PUC_PORT_TYPE_COM, 0x1A, 0x00, COM_FREQ * 8 },
	    },
	},

	/*
	 * Boca Research Turbo Serial 654 (4 serial port) card.
	 * Appears to be the same as Chase Research PLC PCI-FAST4 card,
	 * same as Perle PCI-FAST4 Multi-Port serial card
	 */
	{   /* "Boca Turbo Serial 654 - IOP654" */
	    {   PCI_VENDOR_PLX, PCI_PRODUCT_PLX_9050,	0x12e0, 0x0031  },
	    {   0xffff, 0xffff,				0xffff, 0xffff  },
	    {
		{ PUC_PORT_TYPE_COM, 0x18, 0x00, COM_FREQ * 4 },
		{ PUC_PORT_TYPE_COM, 0x18, 0x08, COM_FREQ * 4 },
		{ PUC_PORT_TYPE_COM, 0x18, 0x10, COM_FREQ * 4 },
		{ PUC_PORT_TYPE_COM, 0x18, 0x18, COM_FREQ * 4 },
	    }, 
	},

	/*
	 * Boca Research Turbo Serial 658 (8 serial port) card.
	 * Appears to be the same as Chase Research PLC PCI-FAST8 card
	 * same as Perle PCI-FAST8 Multi-Port serial card
	 */
	{   /* "Boca Turbo Serial 658 - IOP658" */
	    {   PCI_VENDOR_PLX, PCI_PRODUCT_PLX_9050,	0x12e0, 0x0021  },
	    {   0xffff, 0xffff,				0xffff, 0xffff  },
	    {
		{ PUC_PORT_TYPE_COM, 0x18, 0x00, COM_FREQ * 4 },
		{ PUC_PORT_TYPE_COM, 0x18, 0x08, COM_FREQ * 4 },
		{ PUC_PORT_TYPE_COM, 0x18, 0x10, COM_FREQ * 4 },
		{ PUC_PORT_TYPE_COM, 0x18, 0x18, COM_FREQ * 4 },
		{ PUC_PORT_TYPE_COM, 0x18, 0x20, COM_FREQ * 4 },
		{ PUC_PORT_TYPE_COM, 0x18, 0x28, COM_FREQ * 4 },
		{ PUC_PORT_TYPE_COM, 0x18, 0x30, COM_FREQ * 4 },
		{ PUC_PORT_TYPE_COM, 0x18, 0x38, COM_FREQ * 4 },
	    }, 
	},

	{   /* NULL, */

	    {	0,	0,		0, 0	},
	    {	0,	0,		0, 0	},
	    {
		{ 0, 0, 0 },
	    },
	}
};
