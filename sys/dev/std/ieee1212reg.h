/*	$OpenBSD: ieee1212reg.h,v 1.2 2002/12/13 02:52:11 tdeval Exp $	*/
/*	$NetBSD: ieee1212reg.h,v 1.7 2002/04/02 10:10:54 jmc Exp $	*/

/*
 * Copyright (c) 2000 The NetBSD Foundation, Inc.
 * All rights reserved.
 *
 * This code is derived from software contributed to The NetBSD Foundation
 * by 
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
 *	This product includes software developed by the NetBSD
 *	Foundation, Inc. and its contributors.
 * 4. Neither the name of The NetBSD Foundation nor the names of its
 *    contributors may be used to endorse or promote products derived
 *    from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE NETBSD FOUNDATION, INC. AND CONTRIBUTORS
 * ``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED
 * TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE FOUNDATION OR CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef	_DEV_STD_IEEE1212REG_H_
#define	_DEV_STD_IEEE1212REG_H_

/* This file contains definitions from ISO/IEC 1312 or ANSI/IEEE Std 1212
 *	Informaton techonology
 *		Microprocessor systems
 *			Control and Status Registers (CSR)
 *			Architecture for microcomputer buses
 *	First edition 1994-10-05
 */

/* Lock transaction codes (Table 5)
 */
#define	P1212_XTCODE_RESERVED_0		0
#define	P1212_XTCODE_MASK_SWAP		1
#define	P1212_XTCODE_COMPARE_SWAP	2
#define	P1212_XTCODE_FETCH_ADD		3
#define	P1212_XTCODE_LITTLE_ADD		4
#define	P1212_XTCODE_BOUNDED_ADD	5
#define	P1212_XTCODE_WRAP_ADD		6
#define	P1212_XTCODE_VENDOR_DEPENDENT	7

/*	Header:		Rom Format (1 Quadlet)
 *	Bus Info Block:	<info-length> Quadlets
 *	Root Directory
 *	Unit Directory
 *	Root & Unit Leaves
 *	Vendor Dependent Information
 */
/* ROM Formats
 *	0x00-0x000000		Initializing
 *	0x01-0xzzyyxx		Minimal (zz-yy-xx is an OUI)
 *	0xii-0xcc-0xllll	General (ii is info-length, 
 *				cc is crc-length, llll is length)
 */

#define	P1212_ROMFMT_INIT			0x00
#define	P1212_ROMFMT_MINIMAL			0x01

/*	uint32_t P1212_ROMFMT_MK_INIT(void)
 */
#define	P1212_ROMFMT_MK_INIT()			0x00000000

/*	uint32_t P1212_ROMFMT_MK_MINIMAL(const uint8_t *oui);
 */
#define	P1212_ROMFMT_MK_MINIMAL(oui) \
	((P1212_ROMFMT_MINIMAL << 24) \
	 | ((oui[0]) << 16) \
	 | ((oui[1]) <<  8) \
	 | ((oui[2]) <<  0))

/*	uint32_t P1212_ROMFMT_MK_GENERAL(size_t info_len, size_t crc_len,
 *			uint16_t crc);
 */
#define	P1212_ROMFMT_MK_GENERAL(info_len, crc_len, crc_value) \
	(((info_len) << 24) \
	 | ((crc_len) << 16) \
	 | ((crc_value) << 0))

/*	unsigned P1212_ROMFMT_GET_FMT(uint32_t);
 */
#define	P1212_ROMFMT_GET_FMT(quadlet)	(((quadlet) >> 24) & 0xff)

/*	void P1212_ROMFMT_GET_OUI(uint32_t quadlet, uint8_t *oui);
 */
#define	P1212_ROMTFMT_GET_OUI(quadlet, oui) do { \
	(oui)[0] = ((quadlet) >> 16) & 0xff; \
	(oui)[1] = ((quadlet) >>  8) & 0xff; \
	(oui)[2] = ((quadlet) >>  0) & 0xff; \
    } while (0)

/*	size_t P1212_ROMGET_GET_INFOLEN(uint32_t quadlet);
 */
#define	P1212_ROMFMT_GET_INFOLEN(quadlet)	(((quadlet) >> 24) & 0xff)

/*	size_t P1212_ROMGET_GET_CRCLEN(uint32_t quadlet);
 */
#define	P1212_ROMFMT_GET_CRCLEN(quadlet)	(((quadlet) >> 16) & 0xff)

/*	size_t P1212_ROMGET_GET_CRC(uint32_t quadlet);
 */
#define	P1212_ROMFMT_GET_CRC(quadlet)		((uint16_t)(quadlet))

/*	uint8_t P1212_DIRENT_GET_KEY(uint32_t quadlet);
 */
#define	P1212_DIRENT_GET_KEY(quadlet)		(((quadlet) >> 24) & 0xff)

/*	unsigned int P1212_DIRENT_GET_KEYTYPE(uint32_t quadlet);
 */
#define	P1212_DIRENT_GET_KEYTYPE(quadlet)	(((quadlet) >> 30) & 0x03)

/*	unsigned int P1212_DIRENT_GET_KEYVALUE(uint32_t quadlet);
 */
#define	P1212_DIRENT_GET_KEYVALUE(quadlet)	(((quadlet) >> 24) & 0x3f)

/*	unsigned int P1212_DIRENT_GET_OFFSET(uint32_t quadlet);
 */
#define	P1212_DIRENT_GET_OFFSET(quadlet)	((quadlet) & 0xffffff)

/*	unsigned int P1212_DIRENT_GET_VALUE(uint32_t quadlet);
 */
#define	P1212_DIRENT_GET_VALUE(quadlet)		((quadlet) & 0xffffff)

/*	u_int16_t P1212_DIRENT_GET_LEN(quadlet);
 */
#define	P1212_DIRENT_GET_LEN(quadlet)		(((quadlet) >> 16) & 0xffff)

/*	u_int16_t P1212_DIRENT_GET_CRC(quadlet);
 */
#define	P1212_DIRENT_GET_CRC(quadlet)		((uint16_t)(quadlet))

/* Key Types are stored in bits 31-30 of a directory entry.
 */

#define	P1212_KEYTYPE_Immediate			0x00
#define	P1212_KEYTYPE_Offset			0x01
#define	P1212_KEYTYPE_Leaf			0x02
#define	P1212_KEYTYPE_Directory			0x03

/* Key Values are stored in bits 29-24 of a directory entry.
 */
#define	P1212_KEYVALUE_Textual_Descriptor	0x01	/* leaf | directory */
#define	P1212_KEYVALUE_Bus_Dependent_Info	0x02	/* leaf | directory */
#define	P1212_KEYVALUE_Module_Vendor_Id		0x03	/* immediate */
#define	P1212_KEYVALUE_Module_Hw_Version	0x04	/* immediate */
#define	P1212_KEYVALUE_Module_Spec_Id		0x05	/* immediate */
#define	P1212_KEYVALUE_Module_Sw_Version	0x06	/* immediate */
#define	P1212_KEYVALUE_Module_Dependent_Info	0x07	/* leaf | directory */
#define	P1212_KEYVALUE_Node_Vendor_Id		0x08	/* immediate */
#define	P1212_KEYVALUE_Node_Hw_Version		0x09	/* immediate */
#define	P1212_KEYVALUE_Node_Spec_Id		0x0a	/* immediate */
#define	P1212_KEYVALUE_Node_Sw_Version		0x0b	/* immediate */
#define	P1212_KEYVALUE_Node_Capabilities	0x0c	/* immediate */
#define	P1212_KEYVALUE_Node_Unique_Id		0x0d	/* leaf */
#define	P1212_KEYVALUE_Node_Units_Extent	0x0e	/* immediate | offset */
#define	P1212_KEYVALUE_Node_Memory_Extent	0x0f	/* immediate | offset */
#define	P1212_KEYVALUE_Node_Dependent_Info	0x10	/* leaf | directory */
#define	P1212_KEYVALUE_Unit_Directory		0x11	/* directory */
#define	P1212_KEYVALUE_Unit_Spec_Id		0x12	/* immediate */
#define	P1212_KEYVALUE_Unit_Sw_Version		0x13	/* immediate */
#define	P1212_KEYVALUE_Unit_Dependent_Info	0x14	/* imm|off|leaf|dir */
#define	P1212_KEYVALUE_Unit_Location		0x15	/* leaf */
#define	P1212_KEYVALUE_Unit_Poll_Mask		0x16	/* immediate */

/*
 * Items not in original p1212 standard but are in proposed drafts and in use
 * already in some roms.
 */

#define	P1212_KEYVALUE_Model		 	0x17	/* immediate */
#define	P1212_KEYVALUR_Instance_Directory	0x18	/* directory */
#define	P1212_KEYVALUE_Keyword			0x19	/* leaf */
#define	P1212_KEYVALUE_Feature_Directory	0x1A	/* directory */
#define	P1212_KEYVALUE_Extended_ROM		0x1B	/* leaf */
#define	P1212_KEYVALUE_Extended_Key_Spec_Id	0x1C	/* immediate */
#define	P1212_KEYVALUE_Extended_Key		0x1D	/* immediate */
#define	P1212_KEYVALUE_Extended_Data		0x1E	/* imm|off|leaf|dir */
#define	P1212_KEYVALUE_Modifiable_Descriptor	0x1F	/* leaf */
#define	P1212_KEYVALUE_Directory_Id		0x20	/* immediate */

#define	P1212_KEYTYPE_STRINGS { "Immediate", "Offset", "Leaf", "Directory" }

#define	P1212_KEYVALUE_STRINGS { "Root-Directory", \
	"Textual-Descriptor", "Bus-Dependent-Info", "Module-Vendor-Id", \
	"Module-Hw-Version", "Module-Spec-Id", "Module-Sw-Version", \
	"Module-Dependent-Info", "Node-Vendor-Id", "Node-Hw_Version", \
	"Node-Spec-Id", "Node-Sw-Verson", "Node-Capabilities", \
	"Node-Unique-Id", "Node-Units-Extent", "Node-Memory-Extent", \
	"Node-Dependent-Info", "Unit-Directory", "Unit-Spec-Id", \
	"Unit-Sw-Version", "Unit-Dependent-Info", "Unit-Location", \
	"Unit-Poll-Mask", "Model", "Instance-Directory", "Keyword", \
	"Feature-Directory", "Extended-ROM", "Extended-Key-Spec-Id", \
	"Extended-Key", "Extended-Data", "Modifiable-Descriptor", \
	"Directory-Id", NULL /* 0x21 */, NULL /* 0x22 */, NULL /* 0x23 */, \
	NULL /* 0x24 */, NULL /* 0x25 */, NULL /* 0x26 */, NULL /* 0x27 */, \
	NULL /* 0x28 */, NULL /* 0x29 */, NULL /* 0x2A */, NULL /* 0x2B */, \
	NULL /* 0x2C */, NULL /* 0x2D */, NULL /* 0x2E */, NULL /* 0x2F */, \
	NULL /* 0x30 */, NULL /* 0x31 */, NULL /* 0x32 */, NULL /* 0x33 */, \
	NULL /* 0x34 */, NULL /* 0x35 */, NULL /* 0x36 */, NULL /* 0x37 */, \
	"Command-Set-Spec-ID", "Commant-Set", "Unit-Characteristics", \
	"Command-Set-Revision", "Firmware-Revision", "Reconnect-Timeout", \
}

/* Leaf nodes look like:
 *
 *	[0]	0xnnnn-0xcccc		length, crc16
 *	[n]
 *
 * Text leaves look like:
 *	[0]	0xnnnn-0xcccc		length, crc16
 *	[1]	0xtt-0xiiiiii		specifier type, specifier id
 *	[2]	0xllllllll		language id
 */

#define	P1212_TEXT_Min_Leaf_Length		0x3
#define	P1212_TEXT_GET_Spec_Type(quadlet)	(((quadlet) & 0xff000000) >> 24)
#define	P1212_TEXT_GET_Spec_Id(quadlet)		((quadlet) & 0xffffff)

/*
 * Directory nodes look like:
 *	[0]	0xnnnn-0xcccc		length, crc16
 *	[1]	direntry
 *	[n]	direntry
 */

/* Some definitions for the p1212_find routines. */

#define	P1212_FIND_SEARCHALL			0x1
#define	P1212_FIND_RETURNALL			0x2

/* Mask definitions for overriding the p1212 standard checks. */

/*
 * XXX: Note that some of these go away if full p1212r support is done as
 * a lot of the restrictions were lifted there in what can go where. 
 */


/* Normally dependent info can only be leaf or directory. Allow offsets also */
#define	P1212_ALLOW_DEPENDENT_INFO_OFFSET_TYPE	0x1

/* Same thing applies for immediate types. */
#define	P1212_ALLOW_DEPENDENT_INFO_IMMED_TYPE	0x2
#define	P1212_ALLOW_VENDOR_DIRECTORY_TYPE	0x4

#endif	/* _DEV_STD_IEEE1212REG_H_ */
