/*	$OpenBSD: qop_cdnr.h,v 1.1.1.1 2001/06/27 18:23:30 kjc Exp $	*/
/*	$KAME: qop_cdnr.h,v 1.4 2000/10/18 09:15:19 kjc Exp $	*/
/*
 * Copyright (C) 1999-2000
 *	Sony Computer Science Laboratories, Inc.  All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY SONY CSL AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL SONY CSL OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

/*
 * struct classinfo is used also for traffic conditioners
 */

/* discipline specific class info */
struct cdnrinfo {
	int	tce_type;	
	union {
		struct {
			struct tc_action	action;
		} element;
		struct {
			struct tb_profile	profile;
			struct tc_action	in_action;
			struct tc_action	out_action;
		} tbmeter;
		struct {
			struct tb_profile	cmtd_profile;
			struct tb_profile	peak_profile;
			struct tc_action	green_action;
			struct tc_action	yellow_action;
			struct tc_action	red_action;
			int			coloraware;
		} trtcm;
		struct {
			u_int32_t		cmtd_rate;
			u_int32_t		peak_rate;
			u_int32_t		avg_interval;
			struct tc_action	green_action;
			struct tc_action	yellow_action;
			struct tc_action	red_action;
		} tswtcm;
	} tce_un;
};

u_long cdnr_name2handle(const char *ifname, const char *cdnr_name);

int qcmd_cdnr_add_element(struct tc_action *rp, const char *ifname,
			  const char *cdnr_name, struct tc_action *action);
int qcmd_cdnr_add_tbmeter(struct tc_action *rp, const char *ifname,
			  const char *cdnr_name, 
			  struct tb_profile *profile,
			  struct tc_action *in_action,
			  struct tc_action *out_action);
int qcmd_cdnr_add_trtcm(struct tc_action *rp, const char *ifname,
			const char *cdnr_name, 
			struct tb_profile *cmtd_profile,
			struct tb_profile *peak_profile,
			struct tc_action *green_action,
			struct tc_action *yellow_action,
			struct tc_action *red_action, int coloraware);
int qcmd_cdnr_add_tswtcm(struct tc_action *rp, const char *ifname,
			 const char *cdnr_name, const u_int32_t cmtd_rate,
			 const u_int32_t peak_rate,
			 const u_int32_t avg_interval, 
			 struct tc_action *green_action,
			 struct tc_action *yellow_action,
			 struct tc_action *red_action);
int qcmd_cdnr_delete(const char *ifname, const char *cdnr_name);
int qcmd_nop_add_if(const char *ifname);

int qop_add_cdnr(struct classinfo **rp, const char *cdnr_name,
		 struct ifinfo *ifinfo, struct classinfo **childlist,
		 void *cdnr_private);
int qop_delete_cdnr(struct classinfo *clinfo);
int qop_cdnr_add_element(struct classinfo **rp, const char *cdnr_name,
			 struct ifinfo *ifinfo, struct tc_action *action);
int qop_cdnr_add_tbmeter(struct classinfo **rp, const char *cdnr_name,
		struct ifinfo *ifinfo, struct tb_profile *profile,
		struct tc_action *in_action, struct tc_action *out_action);
int qop_cdnr_add_trtcm(struct classinfo **rp, const char *cdnr_name,
	   struct ifinfo *ifinfo,
	   struct tb_profile *cmtd_profile, struct tb_profile *peak_profile,
	   struct tc_action *green_action, struct tc_action *yellow_action,
	   struct tc_action *red_action, int colorware);
int qop_cdnr_add_tswtcm(struct classinfo **rp, const char *cdnr_name,
			struct ifinfo *ifinfo, const u_int32_t cmtd_rate,
			const u_int32_t peak_rate,
			const u_int32_t avg_interval,
			struct tc_action *green_action,
			struct tc_action *yellow_action,
			struct tc_action *red_action);
int qop_cdnr_modify_tbmeter(struct classinfo *clinfo,
			    struct tb_profile *profile);
int qop_cdnr_modify_trtcm(struct classinfo *clinfo,
			  struct tb_profile *cmtd_profile,
			  struct tb_profile *peak_profile, int coloraware);
int qop_cdnr_modify_tswtcm(struct classinfo *clinfo,
			   const u_int32_t cmtd_rate,
			   const u_int32_t peak_rate,
			   const u_int32_t avg_interval);
