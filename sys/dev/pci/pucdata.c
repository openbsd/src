/*	$OpenBSD: pucdata.c,v 1.2 1999/11/14 01:27:57 downsj Exp $	*/
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
	{   /* "Dolphin Peripherals 4014", */
	    {	0x10b5,	0x9050,	0xd84d,	0x6810	},
	    {	0xffff,	0xffff,	0xffff,	0xffff	},
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
	{   /* "Dolphin Peripherals 4035", */
	    {	0x10b5,	0x9050,	0xd84d,	0x6808	},
	    {	0xffff,	0xffff,	0xffff,	0xffff	},
	    {
		{ PUC_PORT_TYPE_COM, 0x18, 0x00 },
		{ PUC_PORT_TYPE_COM, 0x1c, 0x00 },
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
	    {	0x131f,	0x1000,	0,	0	},
	    {	0xffff,	0xffff,	0,	0	},
	    {
		{ PUC_PORT_TYPE_COM, 0x18, 0x00 },
	    },
	},

	/* SIIG Cyber Serial PCI 16C650 (10x family): 1S */
	{   /* "SIIG Cyber Serial PCI 16C650 (10x family)", */
	    {	0x131f,	0x1001,	0,	0	},
	    {	0xffff,	0xffff,	0,	0	},
	    {
		{ PUC_PORT_TYPE_COM, 0x18, 0x00 },
	    },
	},

	/* SIIG Cyber Serial PCI 16C850 (10x family): 1S */
	{   /* "SIIG Cyber Serial PCI 16C850 (10x family)", */
	    {	0x131f,	0x1002,	0,	0	},
	    {	0xffff,	0xffff,	0,	0	},
	    {
		{ PUC_PORT_TYPE_COM, 0x18, 0x00 },
	    },
	},

	/* SIIG Cyber I/O PCI 16C550 (10x family): 1S, 1P */
	{   /* "SIIG Cyber I/O PCI 16C550 (10x family)", */
	    {	0x131f,	0x1010,	0,	0	},
	    {	0xffff,	0xffff,	0,	0	},
	    {
		{ PUC_PORT_TYPE_COM, 0x18, 0x00 },
		{ PUC_PORT_TYPE_LPT, 0x1c, 0x00 },
	    },
	},

	/* SIIG Cyber I/O PCI 16C650 (10x family): 1S, 1P */
	{   /* "SIIG Cyber I/O PCI 16C650 (10x family)", */
	    {	0x131f,	0x1011,	0,	0	},
	    {	0xffff,	0xffff,	0,	0	},
	    {
		{ PUC_PORT_TYPE_COM, 0x18, 0x00 },
		{ PUC_PORT_TYPE_LPT, 0x1c, 0x00 },
	    },
	},

	/* SIIG Cyber I/O PCI 16C850 (10x family): 1S, 1P */
	{   /* "SIIG Cyber I/O PCI 16C850 (10x family)", */
	    {	0x131f,	0x1012,	0,	0	},
	    {	0xffff,	0xffff,	0,	0	},
	    {
		{ PUC_PORT_TYPE_COM, 0x18, 0x00 },
		{ PUC_PORT_TYPE_LPT, 0x1c, 0x00 },
	    },
	},

	/* SIIG Cyber Parallel PCI (10x family): 1P */
	{   /* "SIIG Cyber Parallel PCI (10x family)", */
	    {	0x131f,	0x1020,	0,	0	},
	    {	0xffff,	0xffff,	0,	0	},
	    {
		{ PUC_PORT_TYPE_LPT, 0x18, 0x00 },
	    },
	},

	/* SIIG Cyber Parallel Dual PCI (10x family): 2P */
	{   /* "SIIG Cyber Parallel Dual PCI (10x family)", */
	    {	0x131f,	0x1021,	0,	0	},
	    {	0xffff,	0xffff,	0,	0	},
	    {
		{ PUC_PORT_TYPE_LPT, 0x18, 0x00 },
		{ PUC_PORT_TYPE_LPT, 0x20, 0x00 },
	    },
	},

	/* SIIG Cyber Serial Dual PCI 16C550 (10x family): 2S */
	{   /* "SIIG Cyber Serial Dual PCI 16C550 (10x family)", */
	    {	0x131f,	0x1030,	0,	0	},
	    {	0xffff,	0xffff,	0,	0	},
	    {
		{ PUC_PORT_TYPE_COM, 0x18, 0x00 },
		{ PUC_PORT_TYPE_COM, 0x1c, 0x00 },
	    },
	},

	/* SIIG Cyber Serial Dual PCI 16C650 (10x family): 2S */
	{   /* "SIIG Cyber Serial Dual PCI 16C650 (10x family)", */
	    {	0x131f,	0x1031,	0,	0	},
	    {	0xffff,	0xffff,	0,	0	},
	    {
		{ PUC_PORT_TYPE_COM, 0x18, 0x00 },
		{ PUC_PORT_TYPE_COM, 0x1c, 0x00 },
	    },
	},

	/* SIIG Cyber Serial Dual PCI 16C850 (10x family): 2S */
	{   /* "SIIG Cyber Serial Dual PCI 16C850 (10x family)", */
	    {	0x131f,	0x1032,	0,	0	},
	    {	0xffff,	0xffff,	0,	0	},
	    {
		{ PUC_PORT_TYPE_COM, 0x18, 0x00 },
		{ PUC_PORT_TYPE_COM, 0x1c, 0x00 },
	    },
	},

	/* SIIG Cyber 2S1P PCI 16C550 (10x family): 2S, 1P */
	{   /* "SIIG Cyber 2S1P PCI 16C550 (10x family)", */
	    {	0x131f,	0x1034,	0,	0	},	/* XXX really? */
	    {	0xffff,	0xffff,	0,	0	},
	    {
		{ PUC_PORT_TYPE_COM, 0x18, 0x00 },
		{ PUC_PORT_TYPE_COM, 0x1c, 0x00 },
		{ PUC_PORT_TYPE_LPT, 0x20, 0x00 },
	    },
	},

	/* SIIG Cyber 2S1P PCI 16C650 (10x family): 2S, 1P */
	{   /* "SIIG Cyber 2S1P PCI 16C650 (10x family)", */
	    {	0x131f,	0x1035,	0,	0	},	/* XXX really? */
	    {	0xffff,	0xffff,	0,	0	},
	    {
		{ PUC_PORT_TYPE_COM, 0x18, 0x00 },
		{ PUC_PORT_TYPE_COM, 0x1c, 0x00 },
		{ PUC_PORT_TYPE_LPT, 0x20, 0x00 },
	    },
	},

	/* SIIG Cyber 2S1P PCI 16C850 (10x family): 2S, 1P */
	{   /* "SIIG Cyber 2S1P PCI 16C850 (10x family)", */
	    {	0x131f,	0x1036,	0,	0	},	/* XXX really? */
	    {	0xffff,	0xffff,	0,	0	},
	    {
		{ PUC_PORT_TYPE_COM, 0x18, 0x00 },
		{ PUC_PORT_TYPE_COM, 0x1c, 0x00 },
		{ PUC_PORT_TYPE_LPT, 0x20, 0x00 },
	    },
	},

	/* SIIG Cyber 4S PCI 16C550 (10x family): 4S */
	{   /* "SIIG Cyber 4S PCI 16C550 (10x family)", */
	    {	0x131f,	0x1050,	0,	0	},
	    {	0xffff,	0xffff,	0,	0	},
	    {
		{ PUC_PORT_TYPE_COM, 0x18, 0x00 },
		{ PUC_PORT_TYPE_COM, 0x1c, 0x00 },
		{ PUC_PORT_TYPE_COM, 0x20, 0x00 },
		{ PUC_PORT_TYPE_COM, 0x24, 0x00 },
	    },
	},

	/* SIIG Cyber 4S PCI 16C650 (10x family): 4S */
	{   /* "SIIG Cyber 4S PCI 16C650 (10x family)", */
	    {	0x131f,	0x1051,	0,	0	},
	    {	0xffff,	0xffff,	0,	0	},
	    {
		{ PUC_PORT_TYPE_COM, 0x18, 0x00 },
		{ PUC_PORT_TYPE_COM, 0x1c, 0x00 },
		{ PUC_PORT_TYPE_COM, 0x20, 0x00 },
		{ PUC_PORT_TYPE_COM, 0x24, 0x00 },
	    },
	},

	/* SIIG Cyber 4S PCI 16C850 (10x family): 4S */
	{   /* "SIIG Cyber 4S PCI 16C850 (10x family)", */
	    {	0x131f,	0x1052,	0,	0	},
	    {	0xffff,	0xffff,	0,	0	},
	    {
		{ PUC_PORT_TYPE_COM, 0x18, 0x00 },
		{ PUC_PORT_TYPE_COM, 0x1c, 0x00 },
		{ PUC_PORT_TYPE_COM, 0x20, 0x00 },
		{ PUC_PORT_TYPE_COM, 0x24, 0x00 },
	    },
	},

	/*
	 * SIIG "20x" family boards.
	 */

	/* SIIG Cyber Parallel PCI (20x family): 1P */
	{   /* "SIIG Cyber Parallel PCI (20x family)", */
	    {	0x131f,	0x2020,	0,	0	},
	    {	0xffff,	0xffff,	0,	0	},
	    {
		{ PUC_PORT_TYPE_LPT, 0x10, 0x00 },
	    },
	},

	/* SIIG Cyber Parallel Dual PCI (20x family): 2P */
	{   /* "SIIG Cyber Parallel Dual PCI (20x family)", */
	    {	0x131f,	0x2021,	0,	0	},
	    {	0xffff,	0xffff,	0,	0	},
	    {
		{ PUC_PORT_TYPE_LPT, 0x10, 0x00 },
		{ PUC_PORT_TYPE_LPT, 0x18, 0x00 },
	    },
	},

	/* SIIG Cyber 2P1S PCI 16C550 (20x family): 1S, 2P */
	{   /* "SIIG Cyber 2P1S PCI 16C550 (20x family)", */
	    {	0x131f,	0x2040,	0,	0	},
	    {	0xffff,	0xffff,	0,	0	},
	    {
		{ PUC_PORT_TYPE_COM, 0x10, 0x00 },
		{ PUC_PORT_TYPE_LPT, 0x14, 0x00 },
		{ PUC_PORT_TYPE_LPT, 0x1c, 0x00 },
	    },
	},

	/* SIIG Cyber 2P1S PCI 16C650 (20x family): 1S, 2P */
	{   /* "SIIG Cyber 2P1S PCI 16C650 (20x family)", */
	    {	0x131f,	0x2041,	0,	0	},
	    {	0xffff,	0xffff,	0,	0	},
	    {
		{ PUC_PORT_TYPE_COM, 0x10, 0x00 },
		{ PUC_PORT_TYPE_LPT, 0x14, 0x00 },
		{ PUC_PORT_TYPE_LPT, 0x1c, 0x00 },
	    },
	},

	/* SIIG Cyber 2P1S PCI 16C850 (20x family): 1S, 2P */
	{   /* "SIIG Cyber 2P1S PCI 16C850 (20x family)", */
	    {	0x131f,	0x2042,	0,	0	},
	    {	0xffff,	0xffff,	0,	0	},
	    {
		{ PUC_PORT_TYPE_COM, 0x10, 0x00 },
		{ PUC_PORT_TYPE_LPT, 0x14, 0x00 },
		{ PUC_PORT_TYPE_LPT, 0x1c, 0x00 },
	    },
	},

	/* SIIG Cyber Serial PCI 16C550 (20x family): 1S */
	{   /* "SIIG Cyber Serial PCI 16C550 (20x family)", */
	    {	0x131f,	0x2000,	0,	0	},
	    {	0xffff,	0xffff,	0,	0	},
	    {
		{ PUC_PORT_TYPE_COM, 0x10, 0x00 },
	    },
	},

	/* SIIG Cyber Serial PCI 16C650 (20x family): 1S */
	{   /* "SIIG Cyber Serial PCI 16C650 (20x family)", */
	    {	0x131f,	0x2001,	0,	0	},
	    {	0xffff,	0xffff,	0,	0	},
	    {
		{ PUC_PORT_TYPE_COM, 0x10, 0x00 },
	    },
	},

	/* SIIG Cyber Serial PCI 16C850 (20x family): 1S */
	{   /* "SIIG Cyber Serial PCI 16C850 (20x family)", */
	    {	0x131f,	0x2002,	0,	0	},
	    {	0xffff,	0xffff,	0,	0	},
	    {
		{ PUC_PORT_TYPE_COM, 0x10, 0x00 },
	    },
	},

	/* SIIG Cyber I/O PCI 16C550 (20x family): 1S, 1P */
	{   /* "SIIG Cyber I/O PCI 16C550 (20x family)", */
	    {	0x131f,	0x2010,	0,	0	},
	    {	0xffff,	0xffff,	0,	0	},
	    {
		{ PUC_PORT_TYPE_COM, 0x10, 0x00 },
		{ PUC_PORT_TYPE_LPT, 0x14, 0x00 },
	    },
	},

	/* SIIG Cyber I/O PCI 16C650 (20x family): 1S, 1P */
	{   /* "SIIG Cyber I/O PCI 16C650 (20x family)", */
	    {	0x131f,	0x2011,	0,	0	},
	    {	0xffff,	0xffff,	0,	0	},
	    {
		{ PUC_PORT_TYPE_COM, 0x10, 0x00 },
		{ PUC_PORT_TYPE_LPT, 0x14, 0x00 },
	    },
	},

	/* SIIG Cyber I/O PCI 16C850 (20x family): 1S, 1P */
	{   /* "SIIG Cyber I/O PCI 16C850 (20x family)", */
	    {	0x131f,	0x2012,	0,	0	},
	    {	0xffff,	0xffff,	0,	0	},
	    {
		{ PUC_PORT_TYPE_COM, 0x10, 0x00 },
		{ PUC_PORT_TYPE_LPT, 0x14, 0x00 },
	    },
	},

	/* SIIG Cyber Serial Dual PCI 16C550 (20x family): 2S */
	{   /* "SIIG Cyber Serial Dual PCI 16C550 (20x family)", */
	    {	0x131f,	0x2030,	0,	0	},
	    {	0xffff,	0xffff,	0,	0	},
	    {
		{ PUC_PORT_TYPE_COM, 0x10, 0x00 },
		{ PUC_PORT_TYPE_COM, 0x14, 0x00 },
	    },
	},

	/* SIIG Cyber Serial Dual PCI 16C650 (20x family): 2S */
	{   /* "SIIG Cyber Serial Dual PCI 16C650 (20x family)", */
	    {	0x131f,	0x2031,	0,	0	},
	    {	0xffff,	0xffff,	0,	0	},
	    {
		{ PUC_PORT_TYPE_COM, 0x10, 0x00 },
		{ PUC_PORT_TYPE_COM, 0x14, 0x00 },
	    },
	},

	/* SIIG Cyber Serial Dual PCI 16C850 (20x family): 2S */
	{   /* "SIIG Cyber Serial Dual PCI 16C850 (20x family)", */
	    {	0x131f,	0x2032,	0,	0	},
	    {	0xffff,	0xffff,	0,	0	},
	    {
		{ PUC_PORT_TYPE_COM, 0x10, 0x00 },
		{ PUC_PORT_TYPE_COM, 0x14, 0x00 },
	    },
	},

	/* SIIG Cyber 2S1P PCI 16C550 (20x family): 2S, 1P */
	{   /* "SIIG Cyber 2S1P PCI 16C550 (20x family)", */
	    {	0x131f,	0x2060,	0,	0	},
	    {	0xffff,	0xffff,	0,	0	},
	    {
		{ PUC_PORT_TYPE_COM, 0x10, 0x00 },
		{ PUC_PORT_TYPE_COM, 0x14, 0x00 },
		{ PUC_PORT_TYPE_LPT, 0x18, 0x00 },
	    },
	},

	/* SIIG Cyber 2S1P PCI 16C650 (20x family): 2S, 1P */
	{   /* "SIIG Cyber 2S1P PCI 16C650 (20x family)", */
	    {	0x131f,	0x2061,	0,	0	},
	    {	0xffff,	0xffff,	0,	0	},
	    {
		{ PUC_PORT_TYPE_COM, 0x10, 0x00 },
		{ PUC_PORT_TYPE_COM, 0x14, 0x00 },
		{ PUC_PORT_TYPE_LPT, 0x18, 0x00 },
	    },
	},

	/* SIIG Cyber 2S1P PCI 16C850 (20x family): 2S, 1P */
	{   /* "SIIG Cyber 2S1P PCI 16C850 (20x family)", */
	    {	0x131f,	0x2062,	0,	0	},
	    {	0xffff,	0xffff,	0,	0	},
	    {
		{ PUC_PORT_TYPE_COM, 0x10, 0x00 },
		{ PUC_PORT_TYPE_COM, 0x14, 0x00 },
		{ PUC_PORT_TYPE_LPT, 0x18, 0x00 },
	    },
	},

	/* SIIG Cyber 4S PCI 16C550 (20x family): 4S */
	{   /* "SIIG Cyber 4S PCI 16C550 (20x family)", */
	    {	0x131f,	0x2050,	0,	0	},
	    {	0xffff,	0xffff,	0,	0	},
	    {
		{ PUC_PORT_TYPE_COM, 0x10, 0x00 },
		{ PUC_PORT_TYPE_COM, 0x14, 0x00 },
		{ PUC_PORT_TYPE_COM, 0x18, 0x00 },
		{ PUC_PORT_TYPE_COM, 0x1c, 0x00 },
	    },
	},

	/* SIIG Cyber 4S PCI 16C650 (20x family): 4S */
	{   /* "SIIG Cyber 4S PCI 16C650 (20x family)", */
	    {	0x131f,	0x2051,	0,	0	},
	    {	0xffff,	0xffff,	0,	0	},
	    {
		{ PUC_PORT_TYPE_COM, 0x10, 0x00 },
		{ PUC_PORT_TYPE_COM, 0x14, 0x00 },
		{ PUC_PORT_TYPE_COM, 0x18, 0x00 },
		{ PUC_PORT_TYPE_COM, 0x1c, 0x00 },
	    },
	},

	/* SIIG Cyber 4S PCI 16C850 (20x family): 4S */
	{   /* "SIIG Cyber 4S PCI 16C850 (20x family)", */
	    {	0x131f,	0x2052,	0,	0	},
	    {	0xffff,	0xffff,	0,	0	},
	    {
		{ PUC_PORT_TYPE_COM, 0x10, 0x00 },
		{ PUC_PORT_TYPE_COM, 0x14, 0x00 },
		{ PUC_PORT_TYPE_COM, 0x18, 0x00 },
		{ PUC_PORT_TYPE_COM, 0x1c, 0x00 },
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
	    {	0x10b5,	0x1076,	0x10b5,	0x1076	},
	    {	0xffff,	0xffff,	0xffff,	0xffff	},
	    {
		{ PUC_PORT_TYPE_COM, 0x18, 0x00 },
		{ PUC_PORT_TYPE_COM, 0x18, 0x08 },
		{ PUC_PORT_TYPE_COM, 0x18, 0x10 },
		{ PUC_PORT_TYPE_COM, 0x18, 0x18 },
		{ PUC_PORT_TYPE_COM, 0x18, 0x20 },
		{ PUC_PORT_TYPE_COM, 0x18, 0x28 },
		{ PUC_PORT_TYPE_COM, 0x18, 0x30 },
		{ PUC_PORT_TYPE_COM, 0x18, 0x38 },
	    },
	},

	/* NEC PK-UG-X001 K56flex PCI Modem card.
	   NEC MARTH bridge chip and Rockwell RCVDL56ACF/SP using. */
	{   /* "NEC PK-UG-X001 K56flex PCI Modem", */
	    {	0x1033,	0x0074,	0x1033,	0x8014	},
	    {	0xffff,	0xffff,	0xffff,	0xffff	},
	    {
		{ PUC_PORT_TYPE_COM, 0x10, 0x00 },
	    },
	},

	{   /* NULL, */
	    {	0,	0,	0,	0	},
	    {	0,	0,	0,	0	},
	    {
		{ 0, 0, 0 },
	    },
	}
};
