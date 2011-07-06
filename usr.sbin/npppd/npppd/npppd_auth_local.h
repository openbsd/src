/* $OpenBSD: npppd_auth_local.h,v 1.4 2011/07/06 20:52:28 yasuoka Exp $ */

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

struct _npppd_auth_base {
	/** name of label */
	char	label[NPPPD_GENERIC_NAME_LEN];
	/** name of realm */
	char	name[NPPPD_GENERIC_NAME_LEN];
	/** reference indicated to parent npppd */
	void 	*npppd;
	/** type of authentication realm */
	int	type;
	/** PPP suffix */
	char	pppsuffix[64];
	/** PPP prefix */
	char	pppprefix[64];
	uint32_t
		/** whether initialized or not */
		initialized:1,
		/** whether reloadable or not */
		reloadable:1,
		/** in disposing */
		disposing:1,
		/** Is the account list ready */
		acctlist_ready:1,
		/** Is the radius configuration ready */
		radius_ready:1,
		/** whether EAP capable or not */
		eap_capable:1,
		/** whether force to strip Windows-NT domain or not */
		strip_nt_domain:1,
		/** whether force to strip after the '@' of PPP username or not */
		strip_atmark_realm:1,
		/** has account-list */
		has_acctlist:1,
		reserved:24;

	/** username => npppd_auth_user hash */
	hash_table *users_hash;
	/** path name of account list */
	char	acctlist_path[64];
	/** last load time */
	time_t	last_load;
};

#ifdef USE_NPPPD_RADIUS
struct _npppd_auth_radius {
	/** parent of npppd_auth_base */
	npppd_auth_base nar_base;

	/** RADIUS authentication server setting */
	radius_req_setting *rad_auth_setting;

	/** RADIUS accounting server setting */
	radius_req_setting *rad_acct_setting;
};
#endif

/** type of local authentication realm */
struct _npppd_auth_local {
	/* parent npppd_auth_base */
	npppd_auth_base nal_base;
};

/** the type of user account */
typedef struct _npppd_auth_user {
	/** username */
	char *username;
	/** password */
	char *password;
	/** Framed-IP-Address */
	struct in_addr	framed_ip_address;
	/** Framed-IP-Netmask */
	struct in_addr	framed_ip_netmask;
	/** Calling-Number */
	char *calling_number;
	/** field for space assignment */
	char space[0];
} npppd_auth_user;

static int                npppd_auth_reload_acctlist (npppd_auth_base *);
static npppd_auth_user    *npppd_auth_find_user (npppd_auth_base *, const char *);
static int                npppd_auth_base_log (npppd_auth_base *, int, const char *, ...);
static uint32_t           str_hash (const void *, int);
static const char *       npppd_auth_default_label(npppd_auth_base *);
static inline const char  *npppd_auth_config_prefix (npppd_auth_base *);
static const char         *npppd_auth_config_str (npppd_auth_base *, const char *);
static int                npppd_auth_config_int (npppd_auth_base *, const char *, int);
static int                npppd_auth_config_str_equal (npppd_auth_base *, const char *, const char *, int);

#ifdef USE_NPPPD_RADIUS
enum RADIUS_SERVER_TYPE {
	RADIUS_SERVER_TYPE_AUTH,
	RADIUS_SERVER_TYPE_ACCT
};

static int                npppd_auth_radius_reload (npppd_auth_base *);
static int                radius_server_address_load (radius_req_setting *, int, const char *, enum RADIUS_SERVER_TYPE);
static int                radius_loadconfig(npppd_auth_base *, radius_req_setting *, enum RADIUS_SERVER_TYPE);
#endif

#ifdef NPPPD_AUTH_DEBUG
#define NPPPD_AUTH_DBG(x) 	npppd_auth_base_log x
#define NPPPD_AUTH_ASSERT(x)	ASSERT(x)
#else
#define NPPPD_AUTH_DBG(x)
#define NPPPD_AUTH_ASSERT(x)
#endif
