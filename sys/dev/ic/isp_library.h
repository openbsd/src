/*	$OpenBSD: isp_library.h,v 1.1 2008/11/16 00:00:13 krw Exp $ */
/* $FreeBSD: src/sys/dev/isp/isp_library.h,v 1.8 2007/04/02 01:04:20 mjacob Exp $ */
/*-
 *  Copyright (c) 1997-2007 by Matthew Jacob
 *  All rights reserved.
 * 
 *  Redistribution and use in source and binary forms, with or without
 *  modification, are permitted provided that the following conditions
 *  are met:
 * 
 *  1. Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 *  2. Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 * 
 *  THIS SOFTWARE IS PROVIDED BY AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 *  ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 *  IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 *  ARE DISCLAIMED.  IN NO EVENT SHALL AUTHOR OR CONTRIBUTORS BE LIABLE
 *  FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 *  DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 *  OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 *  HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 *  LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 *  OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 *  SUCH DAMAGE.
 */
#ifndef	_ISP_LIBRARY_H
#define	_ISP_LIBRARY_H

#include <dev/ic/isp_openbsd.h>

extern int isp_save_xs(struct ispsoftc *, XS_T *, u_int16_t *);
extern XS_T *isp_find_xs(struct ispsoftc *, u_int16_t);
extern u_int16_t isp_find_handle(struct ispsoftc *, XS_T *);
extern int isp_handle_index(u_int16_t);
extern void isp_destroy_handle(struct ispsoftc *, u_int16_t);
extern void isp_remove_handle(struct ispsoftc *, XS_T *);

extern int
isp_getrqentry(struct ispsoftc *, u_int16_t *, u_int16_t *, void **);

extern void
isp_print_qentry (struct ispsoftc *, char *, int, void *);


#define	TBA	(4 * (((QENTRY_LEN >> 2) * 3) + 1) + 1)

extern void
isp_print_bytes(struct ispsoftc *, char *, int, void *);

extern int isp_fc_runstate(struct ispsoftc *, int);

extern void isp_copy_out_hdr(struct ispsoftc *, isphdr_t *, isphdr_t *);
extern void isp_copy_in_hdr(struct ispsoftc *, isphdr_t *, isphdr_t *);
extern int isp_get_response_type(struct ispsoftc *, isphdr_t *);

extern void
isp_put_request(struct ispsoftc *, ispreq_t *, ispreq_t *);
extern void
isp_put_request_t2(struct ispsoftc *, ispreqt2_t *, ispreqt2_t *);
extern void
isp_put_request_t3(struct ispsoftc *, ispreqt3_t *, ispreqt3_t *);
extern void
isp_put_extended_request(struct ispsoftc *, ispextreq_t *, ispextreq_t *);
extern void
isp_put_cont_req(struct ispsoftc *, ispcontreq_t *, ispcontreq_t *);
extern void
isp_put_cont64_req(struct ispsoftc *, ispcontreq64_t *, ispcontreq64_t *);
extern void
isp_get_response(struct ispsoftc *, ispstatusreq_t *, ispstatusreq_t *);
extern void
isp_get_response_x(struct ispsoftc *, ispstatus_cont_t *, ispstatus_cont_t *);
extern void
isp_get_rio2(struct ispsoftc *, isp_rio2_t *, isp_rio2_t *);
extern void
isp_put_icb(struct ispsoftc *, isp_icb_t *, isp_icb_t *);
extern void
isp_get_pdb(struct ispsoftc *, isp_pdb_t *, isp_pdb_t *);
extern void
isp_get_ct_hdr(struct ispsoftc *isp, ct_hdr_t *, ct_hdr_t *);
extern void
isp_put_sns_request(struct ispsoftc *, sns_screq_t *, sns_screq_t *);
extern void
isp_put_gid_ft_request(struct ispsoftc *, sns_gid_ft_req_t *,
    sns_gid_ft_req_t *);
extern void
isp_put_gxn_id_request(struct ispsoftc *, sns_gxn_id_req_t *,
    sns_gxn_id_req_t *);
extern void
isp_get_sns_response(struct ispsoftc *, sns_scrsp_t *, sns_scrsp_t *, int);
extern void
isp_get_gid_ft_response(struct ispsoftc *, sns_gid_ft_rsp_t *,
    sns_gid_ft_rsp_t *, int);
extern void
isp_get_gxn_id_response(struct ispsoftc *, sns_gxn_id_rsp_t *,
    sns_gxn_id_rsp_t *);
extern void
isp_get_gff_id_response(struct ispsoftc *, sns_gff_id_rsp_t *,
    sns_gff_id_rsp_t *);
extern void
isp_get_ga_nxt_response(struct ispsoftc *, sns_ga_nxt_rsp_t *,
    sns_ga_nxt_rsp_t *);
#ifdef	ISP_TARGET_MODE
#ifndef	_ISP_TARGET_H
#include "isp_target.h"
#endif
extern void
isp_put_atio(struct ispsoftc *, at_entry_t *, at_entry_t *);
extern void
isp_get_atio(struct ispsoftc *, at_entry_t *, at_entry_t *);
extern void
isp_put_atio2(struct ispsoftc *, at2_entry_t *, at2_entry_t *);
extern void
isp_get_atio2(struct ispsoftc *, at2_entry_t *, at2_entry_t *);
extern void
isp_put_ctio(struct ispsoftc *, ct_entry_t *, ct_entry_t *);
extern void
isp_get_ctio(struct ispsoftc *, ct_entry_t *, ct_entry_t *);
extern void
isp_put_ctio2(struct ispsoftc *, ct2_entry_t *, ct2_entry_t *);
extern void
isp_get_ctio2(struct ispsoftc *, ct2_entry_t *, ct2_entry_t *);
extern void
isp_put_enable_lun(struct ispsoftc *, lun_entry_t *, lun_entry_t *);
extern void
isp_get_enable_lun(struct ispsoftc *, lun_entry_t *, lun_entry_t *);
extern void
isp_put_notify(struct ispsoftc *, in_entry_t *, in_entry_t *);
extern void
isp_get_notify(struct ispsoftc *, in_entry_t *, in_entry_t *);
extern void
isp_put_notify_fc(struct ispsoftc *, in_fcentry_t *, in_fcentry_t *);
extern void
isp_get_notify_fc(struct ispsoftc *, in_fcentry_t *, in_fcentry_t *);
extern void
isp_put_notify_ack(struct ispsoftc *, na_entry_t *, na_entry_t *);
extern void
isp_get_notify_ack(struct ispsoftc *, na_entry_t *, na_entry_t *);
extern void
isp_put_notify_ack_fc(struct ispsoftc *, na_fcentry_t *, na_fcentry_t *);
extern void
isp_get_notify_ack_fc(struct ispsoftc *, na_fcentry_t *, na_fcentry_t *);
#endif

#define	ISP_IS_SBUS(isp)	\
	(ISP_SBUS_SUPPORTED && (isp)->isp_bustype == ISP_BT_SBUS)

#endif	/* _ISP_LIBRARY_H */
