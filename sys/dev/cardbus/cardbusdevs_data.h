/*	$OpenBSD: cardbusdevs_data.h,v 1.1 2000/03/02 23:03:26 aaron Exp $ */
/*	$NetBSD: cardbusdevs_data.h,v 1.5 1999/12/11 22:22:54 explorer Exp $	*/

/*
 * THIS FILE AUTOMATICALLY GENERATED.  DO NOT EDIT.
 *
 * generated from:
 *	NetBSD: cardbusdevs,v 1.7 1999/12/11 22:22:34 explorer Exp 
 */

/*
 * Copyright (C) 1999  Hayakawa Koichi.
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
 *      This product includes software developed by the author
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

struct cardbus_knowndev cardbus_knowndevs[] = {
	{
	    CARDBUS_VENDOR_3COM, CARDBUS_PRODUCT_3COM_3C575TX,
	    0,
	    "3Com",
	    "3c575 100Base-TX",
	},
	{
	    CARDBUS_VENDOR_3COM, CARDBUS_PRODUCT_3COM_3C575BTX,
	    0,
	    "3Com",
	    "3c575B 100Base-TX",
	},
	{
	    CARDBUS_VENDOR_3COM, CARDBUS_PRODUCT_3COM_3CCFE575CT,
	    0,
	    "3Com",
	    "3CCFE575CT 100Base-TX",
	},
	{
	    CARDBUS_VENDOR_ADP, CARDBUS_PRODUCT_ADP_1480,
	    0,
	    "Adaptec",
	    "APA-1480",
	},
	{
	    CARDBUS_VENDOR_DEC, CARDBUS_PRODUCT_DEC_21142,
	    0,
	    "Digital Equipment",
	    "DECchip 21142/3",
	},
	{
	    CARDBUS_VENDOR_INTEL, CARDBUS_PRODUCT_INTEL_82557,
	    0,
	    "Intel",
	    "82557 Fast Ethernet LAN Controller",
	},
	{
	    CARDBUS_VENDOR_INTEL, CARDBUS_PRODUCT_INTEL_MODEM56,
	    0,
	    "Intel",
	    "Modem",
	},
	{
	    CARDBUS_VENDOR_OPTI, CARDBUS_PRODUCT_OPTI_82C861,
	    0,
	    "Opti",
	    "82C861 USB Host Controller (OHCI)",
	},
	{
	    CARDBUS_VENDOR_XIRCOM, CARDBUS_PRODUCT_XIRCOM_X3201_3,
	    0,
	    "Xircom",
	    "X3201-3 Fast Ethernet Controller",
	},
	{
	    CARDBUS_VENDOR_XIRCOM, CARDBUS_PRODUCT_XIRCOM_X3201_3_21143,
	    0,
	    "Xircom",
	    "X3201-3 Fast Ethernet Controller (21143)",
	},
	{
	    CARDBUS_VENDOR_DEC, 0,
	    CARDBUS_KNOWNDEV_NOPROD,
	    "Digital Equipment",
	    NULL,
	},
	{
	    CARDBUS_VENDOR_3COM, 0,
	    CARDBUS_KNOWNDEV_NOPROD,
	    "3Com",
	    NULL,
	},
	{
	    CARDBUS_VENDOR_ADP, 0,
	    CARDBUS_KNOWNDEV_NOPROD,
	    "Adaptec",
	    NULL,
	},
	{
	    CARDBUS_VENDOR_ADP2, 0,
	    CARDBUS_KNOWNDEV_NOPROD,
	    "Adaptec (2nd PCI Vendor ID)",
	    NULL,
	},
	{
	    CARDBUS_VENDOR_OPTI, 0,
	    CARDBUS_KNOWNDEV_NOPROD,
	    "Opti",
	    NULL,
	},
	{
	    CARDBUS_VENDOR_XIRCOM, 0,
	    CARDBUS_KNOWNDEV_NOPROD,
	    "Xircom",
	    NULL,
	},
	{
	    CARDBUS_VENDOR_INTEL, 0,
	    CARDBUS_KNOWNDEV_NOPROD,
	    "Intel",
	    NULL,
	},
	{
	    CARDBUS_VENDOR_INVALID, 0,
	    CARDBUS_KNOWNDEV_NOPROD,
	    "INVALID VENDOR ID",
	    NULL,
	},
	{ 0, 0, 0, NULL, NULL, }
};
