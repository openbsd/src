/*	$OpenBSD: pucdata.c,v 1.74 2011/04/20 04:58:29 mlarkin Exp $	*/
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

const struct puc_device_description puc_devs[] = {

	{   /* Intel GM45 SOL */
	    {   PCI_VENDOR_INTEL, PCI_PRODUCT_INTEL_82GM45_AMT_SOL, 0x0000, 0x0000 },
	    {	0xffff,	0xffff,				      0x0000, 0x0000 },
	    {
		{ PUC_PORT_TYPE_COM, 0x10, 0x00, COM_FREQ },
	    },
	},
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
	 * Decision PCCOM PCI series. PLX 9052 with 1 or 2 16554 UARTS
	 */

	/* Decision Computer Inc PCCOM 2 Port RS232/422/485: 2S */
	{   /* "Decision Computer Inc PCCOM 2 Port RS232/422/485", */
	    {	PCI_VENDOR_DCI,	PCI_PRODUCT_DCI_APCI2,		0, 0	},
	    {	0xffff,	0xffff,					0, 0	},
	    {
		{ PUC_PORT_TYPE_COM, 0x1c, 0x00, COM_FREQ },
		{ PUC_PORT_TYPE_COM, 0x1c, 0x08, COM_FREQ },
	    },
	},

	/* Decision Computer Inc PCCOM 4 Port RS232/422/485: 4S */
	{   /* "Decision Computer Inc PCCOM 4 Port RS232/422/485", */
	    {	PCI_VENDOR_DCI,	PCI_PRODUCT_DCI_APCI4,		0, 0	},
	    {	0xffff,	0xffff,					0, 0	},
	    {
		{ PUC_PORT_TYPE_COM, 0x1c, 0x00, COM_FREQ },
		{ PUC_PORT_TYPE_COM, 0x1c, 0x08, COM_FREQ },
		{ PUC_PORT_TYPE_COM, 0x1c, 0x10, COM_FREQ },
		{ PUC_PORT_TYPE_COM, 0x1c, 0x18, COM_FREQ },
	    },
	},

	/* Decision Computer Inc PCCOM 8 Port RS232/422/485: 8S */
	{   /* "Decision Computer Inc PCCOM 8 Port RS232/422/485", */
	    {	PCI_VENDOR_DCI, PCI_PRODUCT_DCI_APCI8,		0, 0	},
	    {	0xffff,	0xffff,					0, 0	},
	    {
		{ PUC_PORT_TYPE_COM, 0x1c, 0x00, COM_FREQ },
		{ PUC_PORT_TYPE_COM, 0x1c, 0x08, COM_FREQ },
		{ PUC_PORT_TYPE_COM, 0x1c, 0x10, COM_FREQ },
		{ PUC_PORT_TYPE_COM, 0x1c, 0x18, COM_FREQ },
		{ PUC_PORT_TYPE_COM, 0x1c, 0x20, COM_FREQ },
		{ PUC_PORT_TYPE_COM, 0x1c, 0x28, COM_FREQ },
		{ PUC_PORT_TYPE_COM, 0x1c, 0x30, COM_FREQ },
		{ PUC_PORT_TYPE_COM, 0x1c, 0x38, COM_FREQ },
	    },
	},
	/* IBM SurePOS 300 Series (481033H) serial ports */
	{   /* "IBM SurePOS 300 Series (481033H) serial ports", */
	    {	PCI_VENDOR_IBM, PCI_PRODUCT_IBM_4810_SCC,	0, 0	},
	    {	0xffff, 0xffff,					0, 0	},
	    {
		{ PUC_PORT_TYPE_COM, 0x10, 0x00, COM_FREQ }, /* Port C */
		{ PUC_PORT_TYPE_COM, 0x18, 0x00, COM_FREQ }, /* Port D */
		{ PUC_PORT_TYPE_COM, 0x14, 0x00, COM_FREQ }, /* Port E */
		{ PUC_PORT_TYPE_COM, 0x1c, 0x00, COM_FREQ }, /* Port F */
	    },
	},

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

	/* SIIG Cyber 8S PCI 16C850 (20x family): 8S */
	{   /* "SIIG Cyber 8S PCI 16C850 (20x family)", */
	    {	PCI_VENDOR_OXFORD2, PCI_PRODUCT_OXFORD2_OX16PCI954,
		PCI_VENDOR_SIIG, PCI_PRODUCT_SIIG_2082	},
	    {	0xffff, 0xffff,	0xffff, 0xffff	},
	    {
		{ PUC_PORT_TYPE_COM, 0x10, 0x00, COM_FREQ * 10 },
		{ PUC_PORT_TYPE_COM, 0x10, 0x08, COM_FREQ * 10 },
		{ PUC_PORT_TYPE_COM, 0x10, 0x10, COM_FREQ * 10 },
		{ PUC_PORT_TYPE_COM, 0x10, 0x18, COM_FREQ * 10 },
	    },
	},

	/* OX16PCI954, first part of Serial Technologies Expander PCI-232-108 */
	{   /* "OX16PCI954" */
	    {	PCI_VENDOR_OXFORD2, PCI_PRODUCT_OXFORD2_OX16PCI954,
		PCI_VENDOR_OXFORD2, 0	},
	    {	0xffff, 0xffff,	0xffff, 0xffff	},
	    {
		{ PUC_PORT_TYPE_COM, 0x10, 0x00, COM_FREQ * 8 },
		{ PUC_PORT_TYPE_COM, 0x10, 0x08, COM_FREQ * 8 },
		{ PUC_PORT_TYPE_COM, 0x10, 0x10, COM_FREQ * 8 },
		{ PUC_PORT_TYPE_COM, 0x10, 0x18, COM_FREQ * 8 },
	    },
	},

	/* Exsys EX-41098, second part of Serial Technologies Expander PCI-232-108 */
	{   /* "Exsys EX-41098", */
	    {	PCI_VENDOR_OXFORD2, PCI_PRODUCT_OXFORD2_EXSYS_EX41098,
		PCI_VENDOR_OXFORD2, 0	},
	    {	0xffff, 0xffff, 0xffff, 0xffff },
	    {
		{ PUC_PORT_TYPE_COM, 0x10, 0x00, COM_FREQ * 8 },
		{ PUC_PORT_TYPE_COM, 0x10, 0x08, COM_FREQ * 8 },
		{ PUC_PORT_TYPE_COM, 0x10, 0x10, COM_FREQ * 8 },
		{ PUC_PORT_TYPE_COM, 0x10, 0x18, COM_FREQ * 8 },
	    },
	},

	/* Exsys EX-41098, second part of SIIG Cyber 8S PCI Card */
	{   /* "Exsys EX-41098", */
	    {	PCI_VENDOR_OXFORD2, PCI_PRODUCT_OXFORD2_EXSYS_EX41098,
		PCI_VENDOR_SIIG, PCI_PRODUCT_SIIG_2082	},
	    {	0xffff, 0xffff, 0xffff, 0xffff },
	    {
		{ PUC_PORT_TYPE_COM, 0x10, 0x00, COM_FREQ * 10 },
		{ PUC_PORT_TYPE_COM, 0x10, 0x08, COM_FREQ * 10 },
		{ PUC_PORT_TYPE_COM, 0x10, 0x10, COM_FREQ * 10 },
		{ PUC_PORT_TYPE_COM, 0x10, 0x18, COM_FREQ * 10 },
	    },
	},

	/*
	 * VScom PCI-400S, based on PLX 9050 Chip, 16k buffer
	 */
	{ /* "VScom PCI-400S", */
	    { PCI_VENDOR_PLX, PCI_PRODUCT_PLX_1077, 0x10b5, 0x1077 },
	    { 0xffff, 0xffff, 0xffff, 0xffff },
	    {
		{ PUC_PORT_TYPE_COM, 0x18, 0x00, COM_FREQ * 8 },
		{ PUC_PORT_TYPE_COM, 0x18, 0x08, COM_FREQ * 8 },
		{ PUC_PORT_TYPE_COM, 0x18, 0x10, COM_FREQ * 8 },
		{ PUC_PORT_TYPE_COM, 0x18, 0x18, COM_FREQ * 8 },
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
	 */
	{   /* "VScom PCI-200HV2", */
	    {	PCI_VENDOR_OXFORD, PCI_PRODUCT_OXFORD_VSCOM_PCI200HV2,	0, 0 },
	    {	0xffff, 0xffff,						0, 0 },
	    {
		{ PUC_PORT_TYPE_COM, 0x10, 0x00, COM_FREQ * 8 },
		{ PUC_PORT_TYPE_COM, 0x14, 0x00, COM_FREQ * 8 },
	    },
	},

	/*
	 * VScom PCI 010L
	 * one lpt
	 * untested
	 */
	{   /* "VScom PCI-010L", */
	    {	PCI_VENDOR_OXFORD, PCI_PRODUCT_OXFORD_VSCOM_PCI010L,    0, 0 },
	    {	0xffff, 0xffff,						0, 0 },
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
	    {	0xffff, 0xffff,						0, 0 },
	    {
		{ PUC_PORT_TYPE_COM, 0x14, 0x00, COM_FREQ * 8 },
		{ PUC_PORT_TYPE_COM, 0x18, 0x00, COM_FREQ * 8 },
	    },
	},

	/*
	 * VScom PCI-210L
	 * Has a jumper for frequency selection, defaults to 8x as used here
	 * two com, one lpt
	 */
	{   /* "VScom PCI-210L" */
	    {	PCI_VENDOR_OXFORD, PCI_PRODUCT_OXFORD_VSCOM_PCI210L,	0, 0 },
	    {	0xffff, 0xffff,						0, 0 },
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
	 * Exsys EX-41098
	 */
	{   /* "Exsys EX-41098", */
	    {	PCI_VENDOR_OXFORD2, PCI_PRODUCT_OXFORD2_EXSYS_EX41098,	0, 0 },
	    {	0xffff, 0xffff,						0, 0 },
	    {
		{ PUC_PORT_TYPE_COM, 0x10, 0x00, COM_FREQ },
		{ PUC_PORT_TYPE_COM, 0x10, 0x08, COM_FREQ },
		{ PUC_PORT_TYPE_COM, 0x10, 0x10, COM_FREQ },
		{ PUC_PORT_TYPE_COM, 0x10, 0x18, COM_FREQ },
	    },
	},

	/*
	 * Boards with an Oxford Semiconductor chip.
	 *
	 * Oxford Semiconductor provides documentation for their chip at:
	 * <URL:http://www.plxtech.com/products/uart/>
	 *
	 * As sold by Kouwell <URL:http://www.kouwell.com/>.
	 * I/O Flex PCI I/O Card Model-223 with 4 serial and 1 parallel ports.
	 */

	/* Exsys EX-1372 (uses Oxford OX16PCI952 and a 8x clock) */
	{   /* "Oxford Semiconductor OX16PCI952 UARTs", */
	    {   PCI_VENDOR_OXFORD2, PCI_PRODUCT_OXFORD2_OX16PCI952,
		PCI_VENDOR_OXFORD2, 0x0001 },
	    {   0xffff, 0xffff,	0xffff, 0xffff },
	    {
		{ PUC_PORT_TYPE_COM, 0x10, 0x00, COM_FREQ * 8 },
		{ PUC_PORT_TYPE_COM, 0x14, 0x00, COM_FREQ * 8 },
	    },
	},

	/* Oxford Semiconductor OX16PCI952 PCI `950 UARTs - 128 byte FIFOs */
	{   /* "Oxford Semiconductor OX16PCI952 UARTs", */
	    {   PCI_VENDOR_OXFORD2, PCI_PRODUCT_OXFORD2_OX16PCI952,	0, 0 },
	    {   0xffff, 0xffff,						0, 0 },
	    {
		{ PUC_PORT_TYPE_COM, 0x10, 0x00, COM_FREQ },
		{ PUC_PORT_TYPE_COM, 0x14, 0x00, COM_FREQ },
	    },
	},

	/* Oxford Semiconductor OX16PCI952 PCI Parallel port */
	{   /* "Oxford Semiconductor OX16PCI952 Parallel port", */
	    {   PCI_VENDOR_OXFORD2, PCI_PRODUCT_OXFORD2_OX16PCI952P,	0, 0 },
	    {   0xffff, 0xffff,						0, 0 },
	    {
		{ PUC_PORT_TYPE_LPT, 0x10, 0x00, 0x00 },
	    },
	},

	/* SIIG 2050 (uses Oxford 16PCI954 and a 10x clock) */
	{   /* "Oxford Semiconductor OX16PCI954 UARTs", */
	    {   PCI_VENDOR_OXFORD2, PCI_PRODUCT_OXFORD2_OX16PCI954,
		PCI_VENDOR_SIIG, PCI_PRODUCT_SIIG_2050 },
	    {   0xffff, 0xffff, 0xffff, 0xffff },
	    {
		{ PUC_PORT_TYPE_COM, 0x10, 0x00, COM_FREQ * 10 },
		{ PUC_PORT_TYPE_COM, 0x10, 0x08, COM_FREQ * 10 },
		{ PUC_PORT_TYPE_COM, 0x10, 0x10, COM_FREQ * 10 },
		{ PUC_PORT_TYPE_COM, 0x10, 0x18, COM_FREQ * 10 },
	    },
	},

	/* Oxford Semiconductor OX16PCI954 PCI UARTs */
	{   /* "Oxford Semiconductor OX16PCI954 UARTs", */
	    {   PCI_VENDOR_OXFORD2, PCI_PRODUCT_OXFORD2_OX16PCI954,	0, 0 },
	    {   0xffff, 0xffff,						0, 0 },
	    {
		{ PUC_PORT_TYPE_COM, 0x10, 0x00, COM_FREQ },
		{ PUC_PORT_TYPE_COM, 0x10, 0x08, COM_FREQ },
		{ PUC_PORT_TYPE_COM, 0x10, 0x10, COM_FREQ },
		{ PUC_PORT_TYPE_COM, 0x10, 0x18, COM_FREQ },
	    },
	},

	/* Commell MP-954GPS, GPS and 2 COM */
	{   /* "Oxford Semiconductor OX16mPCI954 UARTs", */
	    {   PCI_VENDOR_OXFORD2, PCI_PRODUCT_OXFORD2_OXMPCI954,	0, 0 },
	    {   0xffff, 0xffff,						0, 0 },
	    {
		{ PUC_PORT_TYPE_COM, 0x10, 0x00, COM_FREQ * 4 },
		{ PUC_PORT_TYPE_COM, 0x10, 0x08, COM_FREQ * 4 },
		{ PUC_PORT_TYPE_COM, 0x10, 0x10, COM_FREQ * 4 },
	    },
	},

	/* Oxford Semiconductor OX16PCI954K PCI UARTs */
	{   /* "Oxford Semiconductor OX16PCI954K UARTs", */
	    {   PCI_VENDOR_OXFORD2, PCI_PRODUCT_OXFORD2_OX16PCI954K,	0, 0 },
	    {   0xffff, 0xffff,						0, 0 },
	    {
		{ PUC_PORT_TYPE_COM, 0x10, 0x00, COM_FREQ },
		{ PUC_PORT_TYPE_COM, 0x14, 0x00, COM_FREQ },
		{ PUC_PORT_TYPE_COM, 0x18, 0x00, COM_FREQ },
		{ PUC_PORT_TYPE_COM, 0x1c, 0x00, COM_FREQ },
	    },
	},

	/* Oxford Semiconductor OX16PCI954 PCI Parallel port */
	{   /* "Oxford Semiconductor OX16PCI954 Parallel port", */
	    {   PCI_VENDOR_OXFORD2, PCI_PRODUCT_OXFORD2_OX16PCI954P,	0, 0 },
	    {   0xffff, 0xffff,						0, 0 },
	    {
		{ PUC_PORT_TYPE_LPT, 0x10, 0x00, 0x00 },
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
		{ PUC_PORT_TYPE_COM, 0x10, 0x00, COM_FREQ * 4 },
		{ PUC_PORT_TYPE_COM, 0x14, 0x00, COM_FREQ * 4 },
	    },
	},

	/*
	 * Lava Computers LavaPort-single serial port.
	 */
	{   /* "Lava Computers high-speed port", */
	    {	PCI_VENDOR_LAVA, PCI_PRODUCT_LAVA_LAVAPORT_2,	0, 0	},
	    {	0xffff,	0xfffc,					0, 0	},
	    {
		{ PUC_PORT_TYPE_COM, 0x10, 0x00, COM_FREQ * 4 },
	    },
	},

	/* Lava Computers LavaPort-650 */
	{   /* "Lava Computers high-speed port", */
	    {	PCI_VENDOR_LAVA, PCI_PRODUCT_LAVA_650,		0, 0	},
	    {	0xffff,	0xfffc,					0, 0	},
	    {
		{ PUC_PORT_TYPE_COM, 0x10, 0x00, COM_FREQ * 4 },
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

	/* Lava Computers Octopus-550 serial ports */
	{   /* "Lava Computers Octopus-550 8-port serial", */
	    {   PCI_VENDOR_LAVA, PCI_PRODUCT_LAVA_OCTOPUS550_1,	0, 0	},
	    {   0xffff, 0xfffc,					0, 0	},
	    {
		{ PUC_PORT_TYPE_COM, 0x10, 0x00, COM_FREQ },
		{ PUC_PORT_TYPE_COM, 0x14, 0x00, COM_FREQ },
		{ PUC_PORT_TYPE_COM, 0x18, 0x00, COM_FREQ },
		{ PUC_PORT_TYPE_COM, 0x1c, 0x00, COM_FREQ },
	    },
	},

	/* US Robotics (3Com) PCI Modems */
	{   /* "US Robotics (3Com) 3CP5610 PCI 16550 Modem", */
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

	/* SD-LAB PCI I/O Card 4S */
	{   /* "Syba Tech Ltd. PCI-4S" */
	    {   PCI_VENDOR_SYBA, PCI_PRODUCT_SYBA_4S,		0, 0	},
	    {	0xffff,	0xffff,					0, 0	},
	    {
		{ PUC_PORT_TYPE_COM, 0x10, 0x3e8, COM_FREQ },
		{ PUC_PORT_TYPE_COM, 0x10, 0x2e8, COM_FREQ },
		{ PUC_PORT_TYPE_COM, 0x10, 0x3f8, COM_FREQ },
		{ PUC_PORT_TYPE_COM, 0x10, 0x2f8, COM_FREQ },
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

	/* Moxa Technologies Co., Ltd. PCI I/O Card 4S RS232/422/485 */
	{   /* "Moxa Technologies, SmartIO C104H/PCI" */
	    {	PCI_VENDOR_MOXA, PCI_PRODUCT_MOXA_C104H,	0, 0	},
	    {	0xffff,	0xffff,					0, 0	},
	    {
		{ PUC_PORT_TYPE_COM, 0x18, 0x00, COM_FREQ * 8 },
		{ PUC_PORT_TYPE_COM, 0x18, 0x08, COM_FREQ * 8 },
		{ PUC_PORT_TYPE_COM, 0x18, 0x10, COM_FREQ * 8 },
		{ PUC_PORT_TYPE_COM, 0x18, 0x18, COM_FREQ * 8 },
	    },
	},

	/* Moxa Technologies Co., Ltd. PCI I/O Card 4S RS232 */
	{   /* "Moxa Technologies, SmartIO CP104UL/PCI" */
	    {	PCI_VENDOR_MOXA, PCI_PRODUCT_MOXA_CP104UL,	0, 0	},
	    {	0xffff,	0xffff,					0, 0	},
	    {
		{ PUC_PORT_TYPE_COM, 0x18, 0x00, COM_FREQ * 8 },
		{ PUC_PORT_TYPE_COM, 0x18, 0x08, COM_FREQ * 8 },
		{ PUC_PORT_TYPE_COM, 0x18, 0x10, COM_FREQ * 8 },
		{ PUC_PORT_TYPE_COM, 0x18, 0x18, COM_FREQ * 8 },
	    },
	},

	/* Moxa Technologies Co., Ltd. PCI I/O Card 4S RS232 */
	{   /* "Moxa Technologies, SmartIO CP104JU/PCI" */
	    {	PCI_VENDOR_MOXA, PCI_PRODUCT_MOXA_CP104JU,	0, 0	},
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

	/* NetMos 1P PCI: 1P */
	{   /* "NetMos NM9805 1284 Printer Port" */
	    {   PCI_VENDOR_NETMOS, PCI_PRODUCT_NETMOS_NM9805,	0, 0	},
	    {	0xffff,	0xffff,					0, 0	},
	    {
		{ PUC_PORT_TYPE_LPT, 0x10, 0x00, 0x00 },
	    },
	},

	/* NetMos 1S PCI 16C650 : 1S */
	{   /* "NetMos NM9835 UART" */
	    {   PCI_VENDOR_NETMOS, PCI_PRODUCT_NETMOS_NM9835,	0x1000, 0x0001	},
	    {	0xffff,	0xffff,					0xffff, 0xffff	},
	    {
		{ PUC_PORT_TYPE_COM, 0x10, 0x00, COM_FREQ },
	    },
	},

	/* NetMos 2S1P PCI 16C650 : 2S, 1P */
	{   /* "NetMos NM9835 Dual UART and 1284 Printer port" */
	    {   PCI_VENDOR_NETMOS, PCI_PRODUCT_NETMOS_NM9835,	0, 0	},
	    {	0xffff,	0xffff,					0, 0	},
	    {
		{ PUC_PORT_TYPE_COM, 0x10, 0x00, COM_FREQ },
		{ PUC_PORT_TYPE_COM, 0x14, 0x00, COM_FREQ },
		{ PUC_PORT_TYPE_LPT, 0x18, 0x00, 0x00 },
	    },
	},

	/* NetMos 4S PCI 16C650 : 4S, 0P */
	{   /* "NetMos NM9845 Quad UART" */
	    {   PCI_VENDOR_NETMOS, PCI_PRODUCT_NETMOS_NM9845, 0x1000, 0x0004 },
	    {	0xffff,	0xffff,				      0xffff, 0xffff },
	    {
		{ PUC_PORT_TYPE_COM, 0x10, 0x00, COM_FREQ },
		{ PUC_PORT_TYPE_COM, 0x14, 0x00, COM_FREQ },
		{ PUC_PORT_TYPE_COM, 0x18, 0x00, COM_FREQ },
		{ PUC_PORT_TYPE_COM, 0x1c, 0x00, COM_FREQ },
	    },
	},

	/* NetMos 4S1P PCI 16C650 : 4S, 1P */
	{   /* "NetMos NM9845 Quad UART and 1284 Printer port" */
	    {   PCI_VENDOR_NETMOS, PCI_PRODUCT_NETMOS_NM9845, 0x1000, 0x0014 },
	    {	0xffff,	0xffff,				      0xffff, 0xffff },
	    {
		{ PUC_PORT_TYPE_COM, 0x10, 0x00, COM_FREQ },
		{ PUC_PORT_TYPE_COM, 0x14, 0x00, COM_FREQ },
		{ PUC_PORT_TYPE_COM, 0x18, 0x00, COM_FREQ },
		{ PUC_PORT_TYPE_COM, 0x1c, 0x00, COM_FREQ },
		{ PUC_PORT_TYPE_LPT, 0x20, 0x00, 0x00 },
	    },
	},

	/* NetMos 6S PCI 16C650 : 6S, 0P */
	{   /* "NetMos NM9845 6 UART" */
	    {   PCI_VENDOR_NETMOS, PCI_PRODUCT_NETMOS_NM9845, 0x1000, 0x0006 },
	    {	0xffff,	0xffff,				      0xffff, 0xffff },
	    {
		{ PUC_PORT_TYPE_COM, 0x10, 0x00, COM_FREQ },
		{ PUC_PORT_TYPE_COM, 0x14, 0x00, COM_FREQ },
		{ PUC_PORT_TYPE_COM, 0x18, 0x00, COM_FREQ },
		{ PUC_PORT_TYPE_COM, 0x1c, 0x00, COM_FREQ },
		{ PUC_PORT_TYPE_COM, 0x20, 0x00, COM_FREQ },
		{ PUC_PORT_TYPE_COM, 0x24, 0x00, COM_FREQ },
	    },
	},

	/* NetMos 2S PCI 16C650 : 2S */
	{   /* "NetMos NM9845 Dual UART" */
	    {   PCI_VENDOR_NETMOS, PCI_PRODUCT_NETMOS_NM9845,	0, 0	},
	    {	0xffff,	0xffff,					0, 0	},
	    {
		{ PUC_PORT_TYPE_COM, 0x10, 0x00, COM_FREQ },
		{ PUC_PORT_TYPE_COM, 0x14, 0x00, COM_FREQ },
	    },
	},

	/* NetMos 6S PCI 16C650 : 6S
	 * Shows up as three PCI devices, two with a single serial
	 * port and one with four serial ports (on a special ISA
	 * extender chip).
	 */
	{   /* "NetMos NM9865 6 UART: 1 UART" */
	    {   PCI_VENDOR_NETMOS, PCI_PRODUCT_NETMOS_NM9865, 0xa000, 0x1000 },
	    {	0xffff,	0xffff,				      0xffff, 0xffff },
	    {
		{ PUC_PORT_TYPE_COM, 0x10, 0x00, COM_FREQ },
	    },
	},
	{   /* "NetMos NM9865 6 UART: 4 UART ISA" */
	    {   PCI_VENDOR_NETMOS, PCI_PRODUCT_NETMOS_NM9865, 0xa000, 0x3004 },
	    {	0xffff,	0xffff,				      0xffff, 0xffff },
	    {
		{ PUC_PORT_TYPE_COM, 0x10, 0x00, COM_FREQ },
		{ PUC_PORT_TYPE_COM, 0x14, 0x00, COM_FREQ },
		{ PUC_PORT_TYPE_COM, 0x18, 0x00, COM_FREQ },
		{ PUC_PORT_TYPE_COM, 0x1c, 0x00, COM_FREQ },
	    },
	},

	/* NetMos PCIe Peripheral Controller :UART part */
	{   /* "NetMos NM9901 UART" */
	    {   PCI_VENDOR_NETMOS, PCI_PRODUCT_NETMOS_NM9901, 0xa000, 0x1000 },
	    {	0xffff,	0xffff,				      0xffff, 0xffff },
	    {
		{ PUC_PORT_TYPE_COM, 0x10, 0x00, COM_FREQ },
	    },
	},

	/* NetMos PCIe Peripheral Controller :parallel part */
	{   /* "NetMos NM9901 UART" */
	    {   PCI_VENDOR_NETMOS, PCI_PRODUCT_NETMOS_NM9901, 0xa000, 0x2000 },
	    {	0xffff,	0xffff,				      0xffff, 0xffff },
	    {
		{ PUC_PORT_TYPE_LPT, 0x10, 0x00 },
	    },
	},

	{   /* NetMos NM9922: 2S */
	    {   PCI_VENDOR_NETMOS, PCI_PRODUCT_NETMOS_NM9922, 0xa000, 0x1000 },
	    {	0xffff,	0xffff,				      0xffff, 0xffff },
	    {
		{ PUC_PORT_TYPE_COM, 0x10, 0x00, COM_FREQ },
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

	/*
	 * SUNIX 40XX series of serial/parallel combo cards.
	 * Tested with 4055A and 4065A.
	 */
	{   /* SUNIX 400X 1P */
	    {	PCI_VENDOR_SUNIX, PCI_PRODUCT_SUNIX_40XX, 0x1409, 0x4000 },
	    {	0xffff,	0xffff,	0xffff,	0xeff0 },
	    {
		{ PUC_PORT_TYPE_LPT, 0x10, 0x00, 0x00 },
	    },
	},

	{   /* SUNIX 401X 2P */
	    {	PCI_VENDOR_SUNIX, PCI_PRODUCT_SUNIX_40XX, 0x1409, 0x4010 },
	    {	0xffff,	0xffff,	0xffff,	0xeff0 },
	    {
		{ PUC_PORT_TYPE_LPT, 0x10, 0x00, 0x00 },
		{ PUC_PORT_TYPE_LPT, 0x18, 0x00, 0x00 },
	    },
	},

	{   /* SUNIX 402X 1S */
	    {	PCI_VENDOR_SUNIX, PCI_PRODUCT_SUNIX_40XX, 0x1409, 0x4020 },
	    {	0xffff,	0xffff,	0xffff,	0xeff0 },
	    {
		{ PUC_PORT_TYPE_COM, 0x10, 0x00, COM_FREQ * 8 },
	    },
	},

	{   /* SUNIX 403X 2S */
	    {	PCI_VENDOR_SUNIX, PCI_PRODUCT_SUNIX_40XX, 0x1409, 0x4030 },
	    {	0xffff,	0xffff,	0xffff,	0xeff0 },
	    {
		{ PUC_PORT_TYPE_COM, 0x10, 0x00, COM_FREQ * 8 },
		{ PUC_PORT_TYPE_COM, 0x10, 0x08, COM_FREQ * 8 },
	    },
	},

	{   /* SUNIX 4036 2S */
	    {	PCI_VENDOR_SUNIX, PCI_PRODUCT_SUNIX_40XX, 0x1409, 0x0002 },
	    {	0xffff,	0xffff,	0xffff,	0xffff },
	    {
		{ PUC_PORT_TYPE_COM, 0x10, 0x00, COM_FREQ * 8 },
		{ PUC_PORT_TYPE_COM, 0x10, 0x08, COM_FREQ * 8 },
	    },
	},


	{   /* SUNIX 405X 4S */
	    {	PCI_VENDOR_SUNIX, PCI_PRODUCT_SUNIX_40XX, 0x1409, 0x4050 },
	    {	0xffff,	0xffff,	0xffff,	0xe0f0 },
	    {
		{ PUC_PORT_TYPE_COM, 0x10, 0x00, COM_FREQ * 8 },
		{ PUC_PORT_TYPE_COM, 0x10, 0x08, COM_FREQ * 8 },
		{ PUC_PORT_TYPE_COM, 0x14, 0x00, COM_FREQ },
		{ PUC_PORT_TYPE_COM, 0x14, 0x08, COM_FREQ },
	    },
	},

	{   /* SUNIX 406X 8S */
	    {	PCI_VENDOR_SUNIX, PCI_PRODUCT_SUNIX_40XX, 0x1409, 0x4060 },
	    {	0xffff,	0xffff,	0xffff,	0xe0f0 },
	    {
		{ PUC_PORT_TYPE_COM, 0x10, 0x00, COM_FREQ * 8 },
		{ PUC_PORT_TYPE_COM, 0x10, 0x08, COM_FREQ * 8 },
		{ PUC_PORT_TYPE_COM, 0x14, 0x00, COM_FREQ * 8 },
		{ PUC_PORT_TYPE_COM, 0x14, 0x08, COM_FREQ * 8 },
		{ PUC_PORT_TYPE_COM, 0x18, 0x00, COM_FREQ * 8 },
		{ PUC_PORT_TYPE_COM, 0x1c, 0x00, COM_FREQ * 8 },
		{ PUC_PORT_TYPE_COM, 0x20, 0x00, COM_FREQ * 8 },
		{ PUC_PORT_TYPE_COM, 0x24, 0x00, COM_FREQ * 8 },
	    },
	},

	{   /* SUNIX 407X 2S/1P */
	    {	PCI_VENDOR_SUNIX, PCI_PRODUCT_SUNIX_40XX, 0x1409, 0x4070 },
	    {	0xffff,	0xffff,	0xffff,	0xeff0 },
	    {
		{ PUC_PORT_TYPE_COM, 0x10, 0x00, COM_FREQ * 8 },
		{ PUC_PORT_TYPE_COM, 0x10, 0x08, COM_FREQ * 8 },
		{ PUC_PORT_TYPE_LPT, 0x18, 0x00, 0x00 },
	    },
	},

	{   /* SUNIX 408X 2S/2P */
	    {	PCI_VENDOR_SUNIX, PCI_PRODUCT_SUNIX_40XX, 0x1409, 0x4080 },
	    {	0xffff,	0xffff,	0xffff,	0xeff0 },
	    {
		{ PUC_PORT_TYPE_COM, 0x10, 0x00, COM_FREQ * 8 },
		{ PUC_PORT_TYPE_COM, 0x10, 0x08, COM_FREQ * 8 },
		{ PUC_PORT_TYPE_LPT, 0x18, 0x00, 0x00 },
		{ PUC_PORT_TYPE_LPT, 0x20, 0x00, 0x00 },
	    },
	},

	{   /* SUNIX 409X 4S/2P */
	    {	PCI_VENDOR_SUNIX, PCI_PRODUCT_SUNIX_40XX, 0x1409, 0x4090 },
	    {	0xffff,	0xffff,	0xffff,	0xeff0 },
	    {
		{ PUC_PORT_TYPE_COM, 0x10, 0x00, COM_FREQ * 8 },
		{ PUC_PORT_TYPE_COM, 0x10, 0x08, COM_FREQ * 8 },
		{ PUC_PORT_TYPE_COM, 0x14, 0x00, COM_FREQ },
		{ PUC_PORT_TYPE_COM, 0x14, 0x08, COM_FREQ },
		{ PUC_PORT_TYPE_LPT, 0x18, 0x00, 0x00 },
		{ PUC_PORT_TYPE_LPT, 0x20, 0x00, 0x00 },
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

	/* Cronyx Engineering Ltd. Omega-PCI (8 serial port) card. */
	{    /* "Cronyx Omega-PCI" */
	    {	PCI_VENDOR_PLX,	PCI_PRODUCT_PLX_CRONYX_OMEGA,	0, 0 },
	    {	0xffff,	0xffff,					0, 0 },
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

	/* PLX 9016 8 port serial card. (i.e. Syba) */
	{    /* "PLX 9016 - Syba" */
	    {	PCI_VENDOR_PLX,	PCI_PRODUCT_PLX_9016,	0, 0 },
	    {	0xffff,	0xffff,					0, 0 },
	    {
		{ PUC_PORT_TYPE_COM, 0x10, 0x00, COM_FREQ * 4 },
		{ PUC_PORT_TYPE_COM, 0x10, 0x08, COM_FREQ * 4 },
		{ PUC_PORT_TYPE_COM, 0x10, 0x10, COM_FREQ * 4 },
		{ PUC_PORT_TYPE_COM, 0x10, 0x18, COM_FREQ * 4 },
		{ PUC_PORT_TYPE_COM, 0x10, 0x20, COM_FREQ * 4 },
		{ PUC_PORT_TYPE_COM, 0x10, 0x28, COM_FREQ * 4 },
		{ PUC_PORT_TYPE_COM, 0x10, 0x30, COM_FREQ * 4 },
		{ PUC_PORT_TYPE_COM, 0x10, 0x38, COM_FREQ * 4 },
	    },
	},

	/* Avlab Technology, Inc. Low Profile PCI 4 Serial: 4S */
	{   /* "Avlab Low Profile PCI 4 Serial" */
	    {	PCI_VENDOR_AVLAB, PCI_PRODUCT_AVLAB_LPPCI4S_2,	0, 0  },
	    {	0xffff,	0xffff,					0, 0  },
	    {
		{ PUC_PORT_TYPE_COM, 0x10, 0x00, COM_FREQ },
		{ PUC_PORT_TYPE_COM, 0x14, 0x00, COM_FREQ },
		{ PUC_PORT_TYPE_COM, 0x18, 0x00, COM_FREQ },
		{ PUC_PORT_TYPE_COM, 0x1c, 0x00, COM_FREQ },
	    },
	},

	/* Avlab Technology, Inc. Low Profile PCI 4 Serial: 4S */
	{   /* "Avlab Low Profile PCI 4 Serial" */
	    {	PCI_VENDOR_AVLAB, PCI_PRODUCT_AVLAB_LPPCI4S,	0, 0  },
	    {	0xffff,	0xffff,					0, 0  },
	    {
		{ PUC_PORT_TYPE_COM, 0x10, 0x00, COM_FREQ },
		{ PUC_PORT_TYPE_COM, 0x14, 0x00, COM_FREQ },
		{ PUC_PORT_TYPE_COM, 0x18, 0x00, COM_FREQ },
		{ PUC_PORT_TYPE_COM, 0x1c, 0x00, COM_FREQ },
	    },
	},

	/* Avlab Technology, Inc. PCI 2 Serial: 2S */
	{   /* "Avlab PCI 2 Serial" */
	    {	PCI_VENDOR_AVLAB, PCI_PRODUCT_AVLAB_PCI2S,	0, 0  },
	    {	0xffff,	0xffff,					0, 0  },
	    {
		{ PUC_PORT_TYPE_COM, 0x10, 0x00, COM_FREQ },
		{ PUC_PORT_TYPE_COM, 0x14, 0x00, COM_FREQ },
	    },
	},

	/* Digi International Digi Neo 4 Serial */
	{
	    {	PCI_VENDOR_DIGI, PCI_PRODUCT_DIGI_NEO4,		0, 0  },
	    {	0xffff, 0xffff,					0, 0  },
	    {
		{ PUC_PORT_TYPE_COM, 0x10, 0x0000, COM_FREQ * 8 },
		{ PUC_PORT_TYPE_COM, 0x10, 0x0200, COM_FREQ * 8 },
		{ PUC_PORT_TYPE_COM, 0x10, 0x0400, COM_FREQ * 8 },
		{ PUC_PORT_TYPE_COM, 0x10, 0x0600, COM_FREQ * 8 },
	    },
	},

	/* Digi International Digi Neo 8 Serial */
	{
	    {	PCI_VENDOR_DIGI, PCI_PRODUCT_DIGI_NEO8,		0, 0  },
	    {	0xffff, 0xffff,					0, 0  },
	    {
		{ PUC_PORT_TYPE_COM, 0x10, 0x0000, COM_FREQ * 8 },
		{ PUC_PORT_TYPE_COM, 0x10, 0x0200, COM_FREQ * 8 },
		{ PUC_PORT_TYPE_COM, 0x10, 0x0400, COM_FREQ * 8 },
		{ PUC_PORT_TYPE_COM, 0x10, 0x0600, COM_FREQ * 8 },
		{ PUC_PORT_TYPE_COM, 0x10, 0x0800, COM_FREQ * 8 },
		{ PUC_PORT_TYPE_COM, 0x10, 0x0a00, COM_FREQ * 8 },
		{ PUC_PORT_TYPE_COM, 0x10, 0x0c00, COM_FREQ * 8 },
		{ PUC_PORT_TYPE_COM, 0x10, 0x0e00, COM_FREQ * 8 },
	    },
	},

	/*
	 * Multi-Tech ISI5634PCI/4 4-port modem board.
	 * Has a 4-channel Exar XR17C154 UART, but with bogus product ID in its
	 * config EEPROM.
	 */
	{
	    {   PCI_VENDOR_EXAR, PCI_PRODUCT_EXAR_XR17C158, 0x2205, 0x2003 },
	    {	0xffff,	0xffff,				    0xffff, 0xffff },
	    {
		{ PUC_PORT_TYPE_COM, 0x10, 0x0000, COM_FREQ * 8 },
		{ PUC_PORT_TYPE_COM, 0x10, 0x0200, COM_FREQ * 8 },
		{ PUC_PORT_TYPE_COM, 0x10, 0x0400, COM_FREQ * 8 },
		{ PUC_PORT_TYPE_COM, 0x10, 0x0600, COM_FREQ * 8 },
	    },
	},

	{   /* EXAR XR17C152 Dual UART */
	    {   PCI_VENDOR_EXAR, PCI_PRODUCT_EXAR_XR17C152,	0, 0 },
	    {   0xffff, 0xffff,					0, 0 },
	    {
		{ PUC_PORT_TYPE_COM, 0x10, 0x0000, COM_FREQ * 8 },
		{ PUC_PORT_TYPE_COM, 0x10, 0x0200, COM_FREQ * 8 },
	    },
	},

	{   /* Exar XR17C154 Quad UART */
	    {   PCI_VENDOR_EXAR, PCI_PRODUCT_EXAR_XR17C154,	0, 0 },
	    {   0xffff, 0xffff,					0, 0 },
	    {
		{ PUC_PORT_TYPE_COM, 0x10, 0x0000, COM_FREQ * 8 },
		{ PUC_PORT_TYPE_COM, 0x10, 0x0200, COM_FREQ * 8 },
		{ PUC_PORT_TYPE_COM, 0x10, 0x0400, COM_FREQ * 8 },
		{ PUC_PORT_TYPE_COM, 0x10, 0x0600, COM_FREQ * 8 },
	    },
	},

	{   /* Exar XR17C158 Eight Channel UART */
	    {   PCI_VENDOR_EXAR, PCI_PRODUCT_EXAR_XR17C158,	0, 0 },
	    {   0xffff, 0xffff,					0, 0 },
	    {
		{ PUC_PORT_TYPE_COM, 0x10, 0x0000, COM_FREQ * 8 },
		{ PUC_PORT_TYPE_COM, 0x10, 0x0200, COM_FREQ * 8 },
		{ PUC_PORT_TYPE_COM, 0x10, 0x0400, COM_FREQ * 8 },
		{ PUC_PORT_TYPE_COM, 0x10, 0x0600, COM_FREQ * 8 },
		{ PUC_PORT_TYPE_COM, 0x10, 0x0800, COM_FREQ * 8 },
		{ PUC_PORT_TYPE_COM, 0x10, 0x0a00, COM_FREQ * 8 },
		{ PUC_PORT_TYPE_COM, 0x10, 0x0c00, COM_FREQ * 8 },
		{ PUC_PORT_TYPE_COM, 0x10, 0x0e00, COM_FREQ * 8 },
	    },
	},

	{   /* Dell DRAC 3 Virtual UART */
	    {   PCI_VENDOR_DELL, PCI_PRODUCT_DELL_DRAC_3_VUART,	0, 0 },
	    {   0xffff, 0xffff,					0, 0 },
	    {
		{ PUC_PORT_TYPE_COM, 0x14, 0x0000, COM_FREQ * 128 },
	    },
	},
	{   /* Dell DRAC 4 Virtual UART */
	    {   PCI_VENDOR_DELL, PCI_PRODUCT_DELL_DRAC_4_VUART,	0, 0 },
	    {   0xffff, 0xffff,					0, 0 },
	    {
		{ PUC_PORT_TYPE_COM, 0x14, 0x0000, COM_FREQ * 128 },
	    },
	},

	/*
	 * Cardbus devices which can potentially show up because of
	 * Expresscard adapters
	 * XXX Keep this list syncronized with cardbus/com_cardbus.c
	*/

	{   /* "", */
	    {	PCI_VENDOR_3COM, PCI_PRODUCT_3COM_GLOBALMODEM56,0, 0	},
	    {	0xffff,	0xffff,					0, 0	},
	    {
		{ PUC_PORT_TYPE_COM, 0x10, 0x00, COM_FREQ },
	    },
	},
	{   /* "", */
	    {	PCI_VENDOR_3COM, PCI_PRODUCT_3COM_MODEM56,	0, 0	},
	    {	0xffff,	0xffff,					0, 0	},
	    {
		{ PUC_PORT_TYPE_COM, 0x10, 0x00, COM_FREQ },
	    },
	},
	{   /* "", */
	    {	PCI_VENDOR_BROADCOM, PCI_PRODUCT_BROADCOM_SERIAL,0, 0	},
	    {	0xffff,	0xffff,					0, 0	},
	    {
		{ PUC_PORT_TYPE_COM, 0x10, 0x00, COM_FREQ },
	    },
	},
	{   /* "", */
	    {	PCI_VENDOR_BROADCOM, PCI_PRODUCT_BROADCOM_SERIAL_2,0, 0	},
	    {	0xffff,	0xffff,					0, 0	},
	    {
		{ PUC_PORT_TYPE_COM, 0x10, 0x00, COM_FREQ },
	    },
	},
	{   /* "", */
	    {	PCI_VENDOR_BROADCOM, PCI_PRODUCT_BROADCOM_SERIAL_GC,0, 0	},
	    {	0xffff,	0xffff,					0, 0	},
	    {
		{ PUC_PORT_TYPE_COM, 0x10, 0x00, COM_FREQ },
	    },
	},
	{   /* "", */
	    {	PCI_VENDOR_INTEL, PCI_PRODUCT_INTEL_MODEM56,	0, 0	},
	    {	0xffff,	0xffff,					0, 0	},
	    {
		{ PUC_PORT_TYPE_COM, 0x10, 0x00, COM_FREQ },
	    },
	},
	{   /* "", */
	    {	PCI_VENDOR_OXFORD2, PCI_PRODUCT_OXFORD2_OXCB950,0, 0	},
	    {	0xffff,	0xffff,					0, 0	},
	    {
		{ PUC_PORT_TYPE_COM, 0x10, 0x00, COM_FREQ },
	    },
	},
	{   /* "Xircom Cardbus 56K Modem", */
	    {	PCI_VENDOR_XIRCOM, PCI_PRODUCT_XIRCOM_MODEM_56K,0, 0	},
	    {	0xffff,	0xffff,					0, 0	},
	    {
		{ PUC_PORT_TYPE_COM, 0x10, 0x00, COM_FREQ },
	    },
	},
	{   /* "Xircom CBEM56G Modem", */
	    {	PCI_VENDOR_XIRCOM, PCI_PRODUCT_XIRCOM_CBEM56G,	0, 0	},
	    {	0xffff,	0xffff,					0, 0	},
	    {
		{ PUC_PORT_TYPE_COM, 0x10, 0x00, COM_FREQ },
	    },
	},
	{   /* "Xircom 56k Modem", */
	    {	PCI_VENDOR_XIRCOM, PCI_PRODUCT_XIRCOM_MODEM56,	0, 0	},
	    {	0xffff,	0xffff,					0, 0	},
	    {
		{ PUC_PORT_TYPE_COM, 0x10, 0x00, COM_FREQ },
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
