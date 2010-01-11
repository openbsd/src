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
	/** ラベル名 */
	char	label[NPPPD_GENERIC_NAME_LEN];
	/** レルム名 */
	char	name[NPPPD_GENERIC_NAME_LEN];
	/** 親 npppd の参照 */
	void 	*npppd;
	/** 認証レルムのタイプ */
	int	type;
	/** PPP サフィックス */
	char	pppsuffix[64];
	/** PPP プレフィックス */
	char	pppprefix[64];
	uint32_t
		/** 初期化済み */
		initialized:1,
		/** 再読み込み可能 */
		reloadable:1,
		/** 廃棄中 */
		disposing:1,
		/** Is the account list ready */
		acctlist_ready:1,
		/** Is the radius configuration ready */
		radius_ready:1,
		/** EAP が利用できるかどうか */
		eap_capable:1,
		/** Windows-NT ドメインを強制的に strip するかどうか */
		strip_nt_domain:1,
		/** PPPユーザ名の @ 以降を強制的に strip するかどうか。*/
		strip_atmark_realm:1,
		/** has account-list */
		has_acctlist:1,
		reserved:24;

	/** ユーザ名 => npppd_auth_user hash */
	hash_table *users_hash;
	/** アカウントリストのパス名 */
	char	acctlist_path[64];
	/** 最終ロード時間 */
	time_t	last_load;
};

#ifdef USE_NPPPD_RADIUS
struct _npppd_auth_radius {
	/** 親 npppd_auth_base */
	npppd_auth_base nar_base;

	/** 現在の利用中のサーバ */
	int curr_server;

	/** RADIUSサーバ */
	radius_req_setting rad_setting;

};
#endif

/** ローカル認証レルムの型 */
struct _npppd_auth_local {
	/* 親 npppd_auth_base */
	npppd_auth_base nal_base;
};

/** ユーザのアカウント情報を示す型 */
typedef struct _npppd_auth_user {
	/** ユーザ名 */
	char *username;
	/** パスワード */
	char *password;
	/** Framed-IP-Address */
	struct in_addr	framed_ip_address;
	/** Framed-IP-Netmask */
	struct in_addr	framed_ip_netmask;
	/** Calling-Number */
	char *calling_number;
	/** スペース確保用フィールド */
	char space[0];
} npppd_auth_user;

static int                npppd_auth_reload_acctlist (npppd_auth_base *);
static npppd_auth_user    *npppd_auth_find_user (npppd_auth_base *, const char *);
static int                radius_server_address_load (radius_req_setting *, int, const char *);
static int                npppd_auth_radius_reload (npppd_auth_base *);
static int                npppd_auth_base_log (npppd_auth_base *, int, const char *, ...);
static uint32_t           str_hash (const void *, int);
static const char *       npppd_auth_default_label(npppd_auth_base *);
static inline const char  *npppd_auth_config_prefix (npppd_auth_base *);
static const char         *npppd_auth_config_str (npppd_auth_base *, const char *);
static int                npppd_auth_config_int (npppd_auth_base *, const char *, int);
static int                npppd_auth_config_str_equal (npppd_auth_base *, const char *, const char *, int);

#ifdef NPPPD_AUTH_DEBUG
#define NPPPD_AUTH_DBG(x) 	npppd_auth_base_log x
#define NPPPD_AUTH_ASSERT(x)	ASSERT(x)
#else
#define NPPPD_AUTH_DBG(x) 
#define NPPPD_AUTH_ASSERT(x)
#endif
