/*	$OpenBSD: bthid.h,v 1.1 2007/07/27 16:52:24 gwk Exp $	*/
/*	$NetBSD: bthid.h,v 1.1 2006/06/19 15:44:45 gdamore Exp $	*/

/*-
 * Copyright (c) 2006 Itronix Inc.
 * All rights reserved.
 *
 * Written by Iain Hibbert for Itronix Inc.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. The name of Itronix Inc. may not be used to endorse
 *    or promote products derived from this software without specific
 *    prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY ITRONIX INC. ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED
 * TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL ITRONIX INC. BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef _DEV_BLUETOOTH_BTHID_H_
#define _DEV_BLUETOOTH_BTHID_H_

/* Transaction Types */
#define BTHID_HANDSHAKE			0x0
#define BTHID_CONTROL			0x1
#define BTHID_GET_REPORT		0x4
#define BTHID_SET_REPORT		0x5
#define BTHID_GET_PROTOCOL		0x6
#define BTHID_SET_PROTOCOL		0x7
#define BTHID_GET_IDLE			0x8
#define BTHID_SET_IDLE			0x9
#define BTHID_DATA			0xa
#define BTHID_DATC			0xb

#define BTHID_TYPE(b)		(((b) & 0xf0) >> 4)

/* HANDSHAKE Transaction Parameters */
#define BTHID_HANDSHAKE_SUCCESS		0x0
#define BTHID_HANDSHAKE_NOT_READY	0x1
#define BTHID_HANDSHAKE_INVALID_ID	0x2
#define BTHID_HANDSHAKE_UNSUPPORTED	0x3
#define BTHID_HANDSHAKE_INVALID_PARAM	0x4
#define BTHID_HANDSHAKE_UNKNOWN		0xe
#define BTHID_HANDSHAKE_FATAL		0xf

#define BTHID_HANDSHAKE_PARAM(b)	((b) & 0x0f)

/* HID_CONTROL Transaction Parameters */
#define BTHID_CONTROL_NOP		0x0
#define BTHID_CONTROL_HARD_RESET	0x1
#define BTHID_CONTROL_SOFT_RESET	0x2
#define BTHID_CONTROL_SUSPEND		0x3
#define BTHID_CONTROL_RESUME		0x4
#define BTHID_CONTROL_UNPLUG		0x5

#define BTHID_CONTROL_PARAM(b)		((b) & 0x0f)

/* GET_REPORT Transaction Parameters */
#define BTHID_CONTROL_SIZE		0x08

/* GET_PROTOCOL Transaction Parameters */
#define BTHID_PROTOCOL_REPORT		0
#define BTHID_PROTOCOL_BOOT		1

#define BTHID_PROTOCOL_PARAM(b)		((b) & 0x01)

/* DATA, DATC Transaction Parameters */
#define BTHID_DATA_OTHER		0
#define BTHID_DATA_INPUT		1
#define BTHID_DATA_OUTPUT		2
#define BTHID_DATA_FEATURE		3

#define BTHID_DATA_PARAM(b)		((b) & 0x03)

#endif /* _DEV_BLUETOOTH_BTHID_H_ */
