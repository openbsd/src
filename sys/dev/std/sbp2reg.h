/*	$OpenBSD: sbp2reg.h,v 1.2 2002/12/30 11:48:34 tdeval Exp $	*/

/*
 * Copyright (c) 2002 Thierry Deval.  All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
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

#ifndef	_DEV_STD_SBP2REG_H_
#define	_DEV_STD_SBP2REG_H_

#define	SBP2_UNIT_SPEC_ID			0x00609E	/* NCITS OUI */
#define	SBP2_UNIT_SW_VERSION			0x010483

/*	0x38/8, oui/24					Unit Directory
 */
#define	SBP2_KEYVALUE_Command_Set_Spec_Id	0x38	/* immediate (OUI) */

/*	0x39/8, oui/24					Unit Directory
 */
#define	SBP2_KEYVALUE_Command_Set		0x39	/* immediate (OUI) */

/*	0x54/8, offset/24				Unit Directory
 */
#define	SBP2_KEYVALUE_Management_Agent \
		P1212_KEYVALUE_Unit_Dependent_Info	/* offset */

/*	0x3A/8, reserved/8,				Unit Directory
 *	msg_ORB_timeout/8, ORB_size/8
 */
#define	SBP2_KEYVALUE_Unit_Characteristics	0x3A	/* immediate */

/*	0x3B/8, reserver/8, max_reconnect_hold/16	Unit Directory
 */
#define	SBP2_KEYVALUE_Command_Set_Revision	0x3B	/* immediate */

/*	0x3C/8, firmware_revision/24			Unit Directory
 */
#define	SBP2_KEYVALUE_Firmware_Revision		0x3C	/* immediate */

/*	0x3D/8, reserver/8, max_reconnect_hold/16	Unit Directory
 */
#define	SBP2_KEYVALUE_Reconnect_Timeout		0x3D	/* immediate */

/*	0xD4/8, indirect_offset/24			Unit Directory
 */
#define	SBP2_KEYVALUE_Logical_Unit_Directory \
		P1212_KEYVALUE_Unit_Dependent_Info	/* directory */

/*	0x14/8, reserved/1, ordered/1, reserved/1	Unit Directory
 *	device_type/5, lun/16
 */
#define	SBP2_KEYVALUE_Logical_Unit_Number \
		P1212_KEYVALUE_Unit_Dependent_Info	/* immediate */

/*	0x8D/8, indirect_offset/24			Unit Directory
 */
#define	SBP2_KEYVALUE_Unit_Unique_Id \
		P1212_KEYVALUE_Node_Unique_Id		/* leaf */

#define	SBP2_ORB_LOGIN				0
#define	SBP2_ORB_QUERY_LOGINS			1
#define	SBP2_ORB_RECONNECT			3
#define	SBP2_ORB_LOGOUT				7

#define	SBP2_NULL_ORB_PTR			0x8000000000000000UL

#define	SBP2_MGMT_ORB				0x400000000000UL
#define	SBP2_CMD_ORB				0x440000000000UL

#define	SBP2_RESP_BLOCK				0x480000000000UL
#define	SBP2_STATUS_BLOCK			0x4C0000000000UL
#define	SBP2_DATA_BLOCK				0x500000000000UL

#define	SBP2_DATA_MASK				0x03FFFFFFFFFFUL
#define	SBP2_DATA_LEN				42
#define	SBP2_DATA_SHIFT				10	/* With 32 bits hash. */

/*
 * Some convention to separate addresses between nodes and luns.
 */
#define	SBP2_NODE_MASK				0x03F000000000UL
#define	SBP2_NODE_SHIFT				36
#define	SBP2_LUN_MASK				0x000FFFF00000UL
#define	SBP2_LUN_SHIFT				20
#define	SBP2_RECONNECT_OFFSET			0x70	/* Transient status. */

#define	SBP2_AGENT_STATE			0x00
#define	SBP2_AGENT_RESET			0x04
#define	SBP2_ORB_POINTER			0x08
#define	SBP2_DOORBEL				0x10
#define	SBP2_UNSOLICITED_STATUS_ENABLE		0x14

#define	SBP2_MAX_TRANS				(1024 * 1024)
#define	SBP2_MAX_LUNS				8

#endif	/* _DEV_STD_SBP2REG_H_ */
