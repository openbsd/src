/* $OpenBSD: scsi_ses.h,v 1.2 2005/04/09 15:24:16 marco Exp $ */
/*
 * Copyright (c) 2005 Marco Peereboom
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
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHORS AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL THE AUTHORS OR CONTRIBUTORS BE LIABLE FOR
 * ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 */

#ifndef _SCSI_SES_H_
#define _SCSI_SES_H_

/* FIXME add all other elements as well currently this only contains "device" */

struct ses_config_page {
	/* diagnostic page header */
	u_int8_t page_code;
	u_int8_t nr_sub_enc;
	u_int8_t length[2]; /* n - 3 */
	u_int8_t gencode[4];
	/* enclosure descriptor header */
	u_int8_t rsvd;
	u_int8_t sub_enc_id;
	u_int8_t nr_elem_typ; /* = T */
	u_int8_t enc_desc_len; /* = m */
	/* enclosure descriptor */
	u_int8_t enc_logical_id[8];
	u_int8_t enc_vendor_id[8];
	u_int8_t prod_id[16];
	u_int8_t prod_rev[4];
	u_int8_t vendor[0]; /* 48 - (11 + m) */
	/* type descriptor header list */
	/* ses_type_descr_hdr[T] */
	/* type descriptor text */
	/* variable, length n */
};

struct ses_type_desc_hdr {
	u_int8_t elem_type;
	u_int8_t nr_elem;
	u_int8_t sub_enc_id;
	u_int8_t type_desc_len;
};

/* control structures, control structs are uses when SENDING */
struct ses_dev_elmt_ctrl_diag {
	u_int8_t common_ctrl;
	u_int8_t reserved;
	u_int8_t byte3;
#define SDECD_RQST_IDENT	0x02
#define SDECD_RQST_REMOVE	0x04
#define SDECD_RQST_INSERT	0x08
#define SDECD_DONT_REMOVE	0x40
#define SDECD_ACTIVE		0x80
	u_int8_t byte4;
#define SDECD_ENABLE_BYP_B	0x04
#define SDECD_ENABLE_BYP_A	0x08
#define SDECD_DEVICE_OFF	0x10
#define SDECD_RQST_FAULT	0x20
};

struct ses_enc_ctrl_diag_page {
	u_int8_t page_code;
	u_int8_t byte2;
#define SECDP_UNREC	0x01
#define SECDP_CRIT	0x02
#define SECDP_NONCRIT	0x04
#define SECDP_INFO	0x08
	u_int8_t length[2];
	u_int8_t gencode[4];
	u_int8_t overallctrl[4];
	/* first element starts here */
	struct ses_dev_elmt_ctrl_diag elmts[0];
};

/* status structures, status structs are uses when RECEIVING */
struct ses_dev_elmt_status_diag {
	u_int8_t common_status;
	u_int8_t slot_addr;
	u_int8_t byte3;
#define SDESD_REPORT		0x01
#define SDESD_IDENT		0x02
#define SDESD_RMV		0x04
#define SDESD_RDY_INSRT		0x08
#define SDESD_ENC_BYP_B		0x10
#define SDESD_ENC_BYP_A		0x20
#define SDESD_DONT_REMV		0x40
#define SDESD_CLNT_BYP_A	0x80
	u_int8_t byte4;
#define SDESD_DEV_BYP_B		0x01
#define SDESD_DEV_BYP_A		0x02
#define SDESD_BYP_B		0x04
#define SDESD_BYP_A		0x08
#define SDESD_DEV_OFF		0x10
#define SDESD_FLT_RQSTD		0x20
#define SDESD_FLT_SENSED	0x40
#define SDESD_CLNT_BYP_B	0x80
};

struct ses_enc_stat_diag_page {
	u_int8_t page_code;
	u_int8_t byte2;
#define SESDP_UNREC	0x01
#define SESDP_CRIT	0x02
#define SESDP_NONCRIT	0x04
#define SESDP_INFO	0x08
	u_int8_t length[2];
	u_int8_t gencode[4];
	u_int8_t overallstat[4];
	struct ses_dev_elmt_status_diag elmts[0];
};
#endif /* _SCSI_SES_H_ */
