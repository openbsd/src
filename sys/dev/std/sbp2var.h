/*	$OpenBSD: sbp2var.h,v 1.1 2002/12/13 02:57:56 tdeval Exp $	*/

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

#ifndef	_DEV_STD_SBP2VAR_H_
#define	_DEV_STD_SBP2VAR_H_

struct fwnode_softc;
struct fwnode_device_cap;
struct p1212_dir;

typedef struct sbp2_addr {
	u_int16_t	node_id;
	u_int16_t	hi;
	u_int32_t	lo;
} sbp2_addr_t;

typedef struct sbp2_orb {
	u_int16_t	flag;
#define	SBP2_NULL_ORB				0x8000
	u_int16_t	hi;
	u_int32_t	lo;
} sbp2_orb_t;

typedef struct sbp2_pte_u {
	u_int16_t	segmentLength;
	u_int16_t	segmentBaseAddress_Hi;
	u_int32_t	segmentBaseAddress_Lo;
} sbp2_pte_u;

typedef struct sbp2_pte_n {
	u_int16_t	segmentLength;
	u_int16_t	segmentBaseAddress_Hi;
	u_int32_t	segmentBaseAddress_Offset;
} sbp2_pte_n;

typedef struct sbp2_command_orb {
	sbp2_orb_t	next_orb;
	sbp2_addr_t	data_descriptor;
	u_int16_t	options;
#define	SBP2_DUMMY_TYPE				0x6000
	u_int16_t	data_size;
	u_int32_t	command_block[1];	/* most certainly more */
} sbp2_command_orb;

typedef struct sbp2_task_management_orb {
	sbp2_orb_t	orb_offset;
	u_int32_t	reserved[2];
	u_int16_t	options;
	u_int16_t	login_id;
	u_int32_t	reserved2;
	sbp2_addr_t	status_fifo;
} sbp2_task_management_orb;

typedef struct sbp2_login_orb {
	sbp2_addr_t	password;
	sbp2_addr_t	login_response;
	u_int16_t	options;
	u_int16_t	lun;
	u_int16_t	password_length;
	u_int16_t	login_response_length;
	sbp2_addr_t	status_fifo;
} sbp2_login_orb;

typedef struct sbp2_login_response {
	u_int16_t	length;
	u_int16_t	login_id;
	sbp2_addr_t	fetch_agent;
	u_int16_t	reserved;
	u_int16_t	reconnect_hold;
} sbp2_login_response;

typedef struct sbp2_query_logins_orb {
	u_int32_t	reserved[2];
	sbp2_addr_t	query_response;
	u_int16_t	options;
	u_int16_t	lun;
	u_int16_t	reserved2;
	u_int16_t	query_response_length;
	sbp2_addr_t	status_fifo;
} sbp2_query_logins_orb;

typedef struct sbp2_query_logins_response {
	u_int16_t	length;
	u_int16_t	max_logins;
	struct {
		u_int16_t	node_id;
		u_int16_t	login_id;
		sbp2_addr_t	initiator;
	}		login_info[1];		/* most certainly more */
} sbp2_query_logins_response;

typedef struct sbp2_status_block {
	u_int8_t	flags;
#define	SBP2_STATUS_SRC_MASK			0xC0
#define	SBP2_STATUS_RESPONSE_MASK		0x30
#define	SBP2_STATUS_RESP_REQ_COMPLETE		0x00
#define	SBP2_STATUS_RESP_TRANS_FAILURE		0x10
#define	SBP2_STATUS_RESP_ILLEGAL_REQ		0x20
#define	SBP2_STATUS_RESP_VENDOR			0x30
#define	SBP2_STATUS_DEAD			0x08
#define	SBP2_STATUS_LEN_MASK			0x07
	u_int8_t	status;
#define	SBP2_STATUS_NONE			0x00
#define	SBP2_STATUS_UNSUPPORTED_TYPE		0x01
#define	SBP2_STATUS_UNSUPPORTED_SPEED		0x02
#define	SBP2_STATUS_UNSUPPORTED_PGSIZ		0x03
#define	SBP2_STATUS_ACCESS_DENIED		0x04
#define	SBP2_STATUS_UNSUPPORTED_LUN		0x05
#define	SBP2_STATUS_MAX_PAYLOAD_SMALL		0x06
#define	SBP2_STATUS_RESOURCE_UNAVAIL		0x08
#define	SBP2_STATUS_FUNCTION_REJECTED		0x09
#define	SBP2_STATUS_INVALID_LOGINID		0x0A
#define	SBP2_STATUS_DUMMY_ORB_COMPLETE		0x0B
#define	SBP2_STATUS_REQUEST_ABORTED		0x0C
#define	SBP2_STATUS_UNSPECIFIED			0xFF
#define	SBP2_STATUS_OBJECT_MASK			0xC0
#define	SBP2_STATUS_OBJECT_ORB			0x00
#define	SBP2_STATUS_OBJECT_DATA_BUF		0x40
#define	SBP2_STATUS_OBJECT_PAGE_TABLE		0x80
#define	SBP2_STATUS_OBJECT_UNSPECIFIED		0xC0
#define	SBP2_STATUS_SERIAL_MASK			0x0F
#define	SBP2_STATUS_SERIAL_ACK_MISSING		0x00
#define	SBP2_STATUS_SERIAL_TIMEOUT		0x02
#define	SBP2_STATUS_SERIAL_ACK_BUSY_X		0x04
#define	SBP2_STATUS_SERIAL_ACK_BUSY_A		0x05
#define	SBP2_STATUS_SERIAL_ACK_BUSY_B		0x06
#define	SBP2_STATUS_SERIAL_TARDY_RETRY		0x0B
#define	SBP2_STATUS_SERIAL_CONFLICT		0x0C
#define	SBP2_STATUS_SERIAL_DATA			0x0D
#define	SBP2_STATUS_SERIAL_TYPE			0x0E
#define	SBP2_STATUS_SERIAL_ADDRESS		0x0F
	u_int16_t	orb_offset_hi;
	u_int32_t	orb_offset_lo;
	u_int32_t	status_info[6];
} sbp2_status_block;


/*
 * A Status Notification structure containing both a reference to the
 * original ORB, and the returned status info.
 */
typedef struct sbp2_status_notification {
	void *origin;
	struct sbp2_status_block *status;
} sbp2_status_notification;


int  sbp2_match(struct device *, void *, void *);
int  sbp2_init(struct fwnode_softc *, struct p1212_dir *);
void sbp2_clean(struct fwnode_softc *, struct p1212_dir *, int);
void sbp2_login(struct fwnode_softc *, struct sbp2_login_orb *,
    void (*)(void *, struct sbp2_status_notification *), void *);
void sbp2_query_logins(struct fwnode_softc *, struct sbp2_query_logins_orb *,
    void (*)(struct sbp2_status_notification *));
void sbp2_command_add(struct fwnode_softc *, int, struct sbp2_command_orb *,
    int, void *, void (*)(void *, struct sbp2_status_notification *), void *);
void sbp2_command_del(struct fwnode_softc *, int, struct sbp2_command_orb *);
void sbp2_agent_reset(struct fwnode_softc *, int);
void sbp2_agent_tickle(struct fwnode_softc *, int);
int  sbp2_agent_state(struct fwnode_softc *, int);
void sbp2_task_management(struct fwnode_softc *,
    struct sbp2_task_management_orb *,
    void (*)(struct sbp2_status_notification *));

#endif	/* _DEV_STD_SBP2VAR_H_ */
