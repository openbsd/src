/*	$OpenBSD: cardbusdevs.h,v 1.12 2001/08/22 16:34:31 aaron Exp $	*/

/*
 * THIS FILE AUTOMATICALLY GENERATED.  DO NOT EDIT.
 *
 * generated from:
 *	OpenBSD: cardbusdevs,v 1.10 2001/07/09 23:09:29 niklas Exp 
 */
/*	$NetBSD: cardbusdevs,v 1.7 1999/12/11 22:22:34 explorer Exp $	*/

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

/*
 * This code is stolen from sys/dev/pci/pcidevs.
 */


/*
 * List of known CardBus vendors
 */

#define	CARDBUS_VENDOR_DEC	0x1011		/* DEC */
#define	CARDBUS_VENDOR_OPTI	0x1045		/* Opti */
#define	CARDBUS_VENDOR_3COM	0x10B7		/* 3Com */
#define	CARDBUS_VENDOR_XIRCOM	0x115d		/* Xircom */
#define	CARDBUS_VENDOR_ADMTEK	0x1317		/* ADMtek */
#define	CARDBUS_VENDOR_ABOCOM	0x13d1		/* Abocom */
#define	CARDBUS_VENDOR_INTEL	0x8086		/* Intel */
#define	CARDBUS_VENDOR_ADP	0x9004		/* Adaptec */
#define	CARDBUS_VENDOR_ADP2	0x9005		/* Adaptec(2) */
#define	CARDBUS_VENDOR_INVALID	0xffff		/* INVALID */

/*
 * List of known products.  Grouped by vendor.
 */

/* 3COM Products */
#define	CARDBUS_PRODUCT_3COM_3C575	0x5057		/* 3C575 */
#define	CARDBUS_PRODUCT_3COM_3CCFE575BT	0x5157		/* 3CCFE575BT */
#define	CARDBUS_PRODUCT_3COM_3CCFE575CT	0x5257		/* 3CCFE575CT */
#define	CARDBUS_PRODUCT_3COM_3CCFEM656	0x6560		/* 3CCFEM656 */
#define	CARDBUS_PRODUCT_3COM_3CCFEM656B	0x6562		/* 3CCFEM656B */
#define	CARDBUS_PRODUCT_3COM_MODEM56	0x6563		/* 56k Modem */
#define	CARDBUS_PRODUCT_3COM_3CCFEM656C	0x6564		/* 3CCFEM656C */
#define	CARDBUS_PRODUCT_3COM_GLOBALMODEM56	0x6565		/* 56k Global Modem */

/* Abocom products */
#define	CARDBUS_PRODUCT_ABOCOM_FE2500	0xab02		/* FE2500 */
#define	CARDBUS_PRODUCT_ABOCOM_PCM200	0xab03		/* PCM200 */

/* ADMtek products */
#define	CARDBUS_PRODUCT_ADMTEK_AN985	0x1985		/* AN985 */

/* Adaptec products */
#define	CARDBUS_PRODUCT_ADP_1480	0x6075		/* APA-1480 */

/* DEC products */
#define	CARDBUS_PRODUCT_DEC_21142	0x0019		/* 21142/3 */

/* Intel products */
#define	CARDBUS_PRODUCT_INTEL_82557	0x1229		/* 82557 */
/* XXX product name? */
#define	CARDBUS_PRODUCT_INTEL_MODEM56	0x1002		/* Modem */
#define	CARDBUS_PRODUCT_INTEL_DFE660	0x7192		/* DFE-660 */

/* Opti products */
#define	CARDBUS_PRODUCT_OPTI_82C861	0xc861		/* 82C861 */

/* Xircom products */
/* is the `-3' here just indicating revision 3, or is it really part
   of the device name? */
#define	CARDBUS_PRODUCT_XIRCOM_X3201_3	0x0002		/* X3201-3 */
/* this is the device id `indicating 21143 driver compatibility' */
#define	CARDBUS_PRODUCT_XIRCOM_X3201_3_21143	0x0003		/* X3201-3 (21143) */
#define	CARDBUS_PRODUCT_XIRCOM_MODEM56	0x0103		/* 56k Modem */
#define	CARDBUS_PRODUCT_XIRCOM_CBEM56G	0x0105		/* CBEM56G Modem */
