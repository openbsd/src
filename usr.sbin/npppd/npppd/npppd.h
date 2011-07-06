/* $OpenBSD: npppd.h,v 1.6 2011/07/06 20:52:28 yasuoka Exp $ */

/*-
 * Copyright (c) 2009 Internet Initiative Japan Inc.
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
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */
#ifndef	NPPPD_H
#define	NPPPD_H 1

#define	NPPPD_USER			"_npppd"

#ifndef	NPPPD_DEFAULT_TUN_IFNAME
#define	NPPPD_DEFAULT_TUN_IFNAME	"tun0"
#endif


#define	DEFAULT_RADIUS_AUTH_PORT	1812
#define	DEFAULT_RADIUS_ACCT_PORT	1813
#define	DEFAULT_RADIUS_TIMEOUT		9
#define	DEFAULT_RADIUS_MAX_TRIES	3
#define	DEFAULT_RADIUS_MAX_FAILOVERS	1
#define	DEFAULT_AUTH_TIMEOUT		30

/** assign fixed IP address */
#define NPPPD_IP_ASSIGN_FIXED		0x0001

/** accept IP address which is proposed by peer and assign it */
#define NPPPD_IP_ASSIGN_USER_SELECT	0x0002

/** use RADIUS Framed-IP-Address */
#define NPPPD_IP_ASSIGN_RADIUS		0x0004

/** sockaddr_npppd */
struct sockaddr_npppd {
	struct sockaddr_in sin4;
	struct sockaddr_in sin4mask;
#define			snp_len		sin4.sin_len
#define			snp_family	sin4.sin_family
#define			snp_addr	sin4.sin_addr
	int		snp_type;	/* SNP_POOL or SNP_PPP */
#define			snp_mask	sin4mask.sin_addr
	/** next entry */
	struct sockaddr_npppd *snp_next;
	/** contents of entry */
	void 		*snp_data_ptr;
};
#define	SNP_POOL		1
#define	SNP_DYN_POOL		2
#define	SNP_PPP			3

typedef struct _npppd		npppd;

#include "ppp.h"

#ifdef __cplusplus
extern "C" {
#endif


npppd      *npppd_get_npppd (void);
int        npppd_init (npppd *, const char *);
void       npppd_stop (npppd *);
void       npppd_fini (npppd *);
int        npppd_prepare_ip (npppd *, npppd_ppp *);
void       npppd_release_ip (npppd *, npppd_ppp *);
int        nppp_load_user_setting(npppd *, npppd_ppp *);
void       npppd_set_ip_enabled (npppd *, npppd_ppp *, int);
int        npppd_get_user_password (npppd *, npppd_ppp *, const char *, char *, int *);
struct in_addr *npppd_get_user_framed_ip_address(npppd *, npppd_ppp *, const char *);
int        npppd_check_calling_number (npppd *, npppd_ppp *);
void       npppd_network_output (npppd *, npppd_ppp *, int, u_char *, int);
void       npppd_network_input (npppd *, uint8_t *, int);
npppd_ppp  *npppd_get_ppp_by_ip (npppd *, struct in_addr);
slist      *npppd_get_ppp_by_user (npppd *, const char *);
int        npppd_check_user_max_session(npppd *, npppd_ppp *);
int        npppd_get_all_users (npppd *, slist *);
int        npppd_set_radish (npppd *, void *);
int        npppd_ppp_iface_is_ready(npppd *, npppd_ppp *);
int        sockaddr_npppd_match(void *, void *);
npppd_ppp  *npppd_get_ppp_by_id(npppd *, int);

const char  *npppd_config_str (npppd *, const char *);
int         npppd_config_int (npppd *, const char *, int);
int         npppd_config_str_equal (npppd *, const char *, const char *, int);
int         npppd_config_str_equali (npppd *, const char *, const char *, int);
int         npppd_modules_reload (npppd *);
int         npppd_ifaces_load_config(npppd *);
int         npppd_reload_config (npppd *);
int         npppd_assign_ip_addr (npppd *, npppd_ppp *, uint32_t);
void            npppd_release_ip_addr (npppd *, npppd_ppp *);

int        npppd_ppp_bind_realm(npppd *, npppd_ppp *, const char *, int);
int        npppd_ppp_is_realm_local(npppd *, npppd_ppp *);
int        npppd_ppp_is_realm_radius(npppd *, npppd_ppp *);
int        npppd_ppp_is_realm_ready(npppd *, npppd_ppp *);
const char *npppd_ppp_get_realm_name(npppd *, npppd_ppp *);
int        npppd_ppp_bind_iface(npppd *, npppd_ppp *);
void       npppd_ppp_unbind_iface(npppd *, npppd_ppp *);
const char *npppd_ppp_get_iface_name(npppd *, npppd_ppp *);
void *     npppd_get_radius_auth_setting(npppd *, npppd_ppp *);
void       npppd_radius_auth_server_failure_notify(npppd *, npppd_ppp *, void *, const char *);
int        npppd_ppp_pipex_enable(npppd *, npppd_ppp *);
int        npppd_ppp_pipex_disable(npppd *, npppd_ppp *);
const char *npppd_ppp_get_username_for_auth(npppd *, npppd_ppp *, const char *, char *);
int        npppd_reset_routing_table(npppd *, int);
#ifdef __cplusplus
}
#endif
#endif
