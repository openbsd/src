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
/* $Id: npppd_config.c,v 1.3 2010/01/31 05:49:51 yasuoka Exp $ */
/*@file
 * npppd 設定関連を操作する関数を格納するファイル。
 */
#include <sys/types.h>
#include <sys/param.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <net/if_dl.h>
#include <netinet/in_systm.h>
#include <netinet/ip.h>
#include <net/route.h>
#include <arpa/inet.h>
#include <syslog.h>
#include <time.h>
#include <event.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <stddef.h>
#include <errno.h>

#include "addr_range.h"
#include "debugutil.h"
#include "npppd_subr.h"
#include "npppd_local.h"
#include "npppd_ctl.h"
#include "npppd_auth.h"
#include "npppd_iface.h"
#include "config_helper.h"
#include "radish.h"

#include "pathnames.h"

#ifdef NPPPD_CONFIG_DEBUG
#define NPPPD_CONFIG_DBG(x) 	log_printf x
#define NPPPD_CONFIG_ASSERT(x) ASSERT(x)
#else
#define NPPPD_CONFIG_DBG(x) 	
#define NPPPD_CONFIG_ASSERT(x) 
#endif


#define	CFG_KEY(p, s)	config_key_prefix((p), (s))
#define	VAL_SEP		" \t\r\n"

static void  npppd_ipcp_config_load (npppd *);
static void  npppd_ipcp_config_load0 (npppd_ipcp_config *, const char *);
#ifdef USE_NPPPD_NPPPD_CTL
static int   npppd_ctl_reload (npppd *, npppd_ctl *);
#endif
static void  npppd_debug_log_reload (npppd *);
static int   npppd_ip_addr_pool_load(npppd *);
static int   npppd_auth_realm_reload (npppd *);
static void npppd_iface_binding_reload(npppd *, npppd_iface *, npppd_iface_binding *);
static int              realm_list_contains (slist *, const char *);
static npppd_auth_base  *realm_list_remove (slist *, const char *);

CONFIG_FUNCTIONS(npppd_config, npppd, properties);
PREFIXED_CONFIG_FUNCTIONS(ppp_config, npppd_ppp, pppd->properties, phy_label);

NAMED_PREFIX_CONFIG_DECL(npppd_ipcp_config, npppd_ipcp_config,
    npppd->properties, "ipcp", label);
NAMED_PREFIX_CONFIG_FUNCTIONS(npppd_ipcp_config, npppd_ipcp_config,
    npppd->properties, "ipcp", label);

/***********************************************************************
 * 設定読み込み。各部の読み込みをまとめた export 関数
 ***********************************************************************/
/**
 * 設定ファイルを再読み込みします。
 * @param   _this   npppd へのポインタ。
 * @returns 成功時は 0 を返します。設定にエラーがあった場合などには、
 *	    0 以外を返します。
 */
int
npppd_reload_config(npppd *_this)
{
	int retval = -1;
	FILE *conffp = NULL;
	struct properties *proptmp = NULL;

	if ((conffp = priv_fopen(_this->config_file)) == NULL) {
		log_printf(LOG_ERR, "Load configuration from='%s' failed: %m",
		    _this->config_file);
		retval = -1;
		goto reigai;
	}
	if ((proptmp = properties_create(1061)) == NULL) {
		log_printf(LOG_ERR, "Load configuration from='%s' failed: %m",
		    _this->config_file);
		retval = -1;
		goto reigai;
	}
	if (properties_load(proptmp, conffp) != 0) {
		log_printf(LOG_ERR, "Load configuration from='%s' failed: %m",
		    _this->config_file);
		retval = -1;
		goto reigai;
	}

	if (_this->properties != NULL) {
		/* 入れ換え */
		properties_remove_all(_this->properties);
		properties_put_all(_this->properties, proptmp);
		properties_destroy(proptmp);
	} else
		_this->properties = proptmp;
	proptmp = NULL;

	/* ログなので処理順としてここ。 */
	npppd_debug_log_reload(_this);

#ifndef	NO_DELAYED_RELOAD
	_this->delayed_reload = npppd_config_int(_this, "delayed_reload", 0);
	if (_this->delayed_reload < 0) {
		log_printf(LOG_WARNING, "Parse error at 'delayed_reload'");
		_this->delayed_reload = 0;
	}
#endif

	_this->max_session = npppd_config_int(_this, "max_session",
	    NPPPD_DEFAULT_MAX_PPP);

	retval = 0;
	log_printf(LOG_NOTICE, "Load configuration from='%s' successfully.",
	    _this->config_file);

	// FALL THROUGH
reigai:
	if (conffp != NULL)
		fclose(conffp);
	if (proptmp != NULL)
		properties_destroy(proptmp);

	return retval;
}

/** モジュール毎の設定の再読み込み。 */
int
npppd_modules_reload(npppd *_this)
{
	int i, rval;

	rval = 0;
	/* アドレスプール */
	if (npppd_ip_addr_pool_load(_this) != 0)
		return -1;

	npppd_ipcp_config_load(_this);

	npppd_auth_realm_reload(_this);
#ifdef	USE_NPPPD_NPPPD_CTL
	npppd_ctl_reload(_this, &_this->ctl);
#endif
#ifdef USE_NPPPD_L2TP
	rval |= l2tpd_reload(&_this->l2tpd, _this->properties, "l2tpd", 1);
#endif
#ifdef USE_NPPPD_PPTP
	rval |= pptpd_reload(&_this->pptpd, _this->properties, "pptpd",
	    1);
#endif
#ifdef USE_NPPPD_PPPOE
	rval |= pppoed_reload(&_this->pppoed, _this->properties, "pppoed", 0);
#endif
	for (i = 0; i < countof(_this->iface_bind); i++)
		npppd_iface_binding_reload(_this, &_this->iface[i],
		    &_this->iface_bind[i]);

	return rval;
}

/***********************************************************************
 * 設定読み込み。各部
 ***********************************************************************/
#ifdef	USE_NPPPD_NPPPD_CTL
/** npppd制御機能の設定を読み込んで、必要なら起動します。*/
static int
npppd_ctl_reload(npppd *_npppd, npppd_ctl *_this)
{
	int enabled;
	const char *val;

	enabled = npppd_config_str_equal(_npppd, "ctl.enabled", "true", 1);
	if (_this->enabled)
		npppd_ctl_stop(_this);

	if (enabled) {
		val = npppd_config_str(_npppd, "ctl.path");
		npppd_ctl_init(_this, _npppd, (val != NULL)?
		    val : DEFAULT_NPPPD_CTL_SOCK_PATH);
		_this->enabled = enabled;
		_this->max_msgsz  = npppd_config_int(_npppd, "ctl.max_msgsz",
		    DEFAULT_NPPPD_CTL_MAX_MSGSZ);
		return npppd_ctl_start(_this);
	}

	return 0;
}
#endif

static void
npppd_ipcp_config_load(npppd *_this)
{
	int n;
	const char *val;
	char *tok, *buf0, buf[NPPPD_CONFIG_BUFSIZ];

	for (n = 0; n < countof(_this->ipcp_config); n++)
		memset(&_this->ipcp_config[n], 0, sizeof(npppd_ipcp_config));
	n = 0;
	if ((val = npppd_config_str(_this, "ipcp_list")) != NULL) {
		strlcpy(buf, val, sizeof(buf));
		buf0 = buf;
		while ((tok = strsep(&buf0, VAL_SEP)) != NULL) {
			if (tok[0] == '\0')
				continue;
			if (n >= countof(_this->ipcp_config)) {
				log_printf(LOG_WARNING,
				    "number of the ipcp configration reached "
				    "limit=%d",
				    (int)countof(_this->ipcp_config));
				    break;
			}
			_this->ipcp_config[n].npppd = _this;
			npppd_ipcp_config_load0(&_this->ipcp_config[n], tok);
			n++;
		}
	} else {
		_this->ipcp_config[n].npppd = _this;
		npppd_ipcp_config_load0(&_this->ipcp_config[n++], NULL);
	}
}

/** IPCP設定を読み込みます。 */
static void
npppd_ipcp_config_load0(npppd_ipcp_config *_this, const char *label)
{
	uint32_t ip_assign_flags;
	const char *val;

	if (label != NULL)
		strlcpy(_this->label, label, sizeof(_this->label));
	else
		memset(_this->label, 0, sizeof(_this->label));

	_this->initialized = 1;

	val = npppd_ipcp_config_str(_this, "name");
	if (val == NULL) {
		if (_this->label[0] == '\0')
			val = "default";
		else
			val = _this->label;
	}
	strlcpy(_this->name, val, sizeof(_this->name));

	/* IPアドレス割り当てポリシー */
	ip_assign_flags = 0;
	if (npppd_ipcp_config_str_equal(_this, "assign_userselect", "true", 1))
		ip_assign_flags |= NPPPD_IP_ASSIGN_USER_SELECT;
	else
		ip_assign_flags &= ~NPPPD_IP_ASSIGN_USER_SELECT;

	if (npppd_ipcp_config_str_equal(_this, "assign_fixed", "true", 1))
		ip_assign_flags |= NPPPD_IP_ASSIGN_FIXED;
	else
		ip_assign_flags &= ~NPPPD_IP_ASSIGN_FIXED;

	if (npppd_ipcp_config_str_equal(_this, "assign_radius", "true", 0))
		ip_assign_flags |= NPPPD_IP_ASSIGN_RADIUS;
	else
		ip_assign_flags &= ~NPPPD_IP_ASSIGN_RADIUS;
	_this->ip_assign_flags = ip_assign_flags;

#define	LOAD_IPADDR_SETTING(field, conf)				\
	if ((val = npppd_ipcp_config_str(_this, conf)) == NULL ||	\
	    strlen(val) <= 0) {						\
		_this->field.s_addr = INADDR_NONE;			\
	} else {							\
		if (inet_aton(val, &_this->field) != 1) {		\
			log_printf(LOG_ERR, "configuration error at "	\
			    conf ": parse error");			\
		}							\
		_this->field	= _this->field;				\
	}

	_this->dns_use_tunnel_end = npppd_ipcp_config_str_equal(_this,
	    "dns_use_tunnel_end", "true", 0);
	if (npppd_ipcp_config_str_equal(_this, "dns_use_resolver",
	    "true", 0)) {
		if (load_resolv_conf(&_this->dns_pri, &_this->dns_sec) != 0)
			log_printf(LOG_ERR, "loading resolv.conf failed: %m");
	} else {
		LOAD_IPADDR_SETTING(dns_pri, "dns_primary");
		LOAD_IPADDR_SETTING(dns_sec, "dns_secondary");
	}
	LOAD_IPADDR_SETTING(nbns_pri, "nbns_primary");
	LOAD_IPADDR_SETTING(nbns_sec, "nbns_secondary");
#undef	LOAD_IPADDR_SETTING
}

/** デバッグとログファイルについての設定を再読込 */
static void
npppd_debug_log_reload(npppd *_this)
{
	int ival, oval;
	FILE *debugfp;
	const char *sval;

	if ((ival = npppd_config_int(_this, "debug.level", debuglevel)) == 
	    debuglevel)
		return;

	// デバッグレベル変更
	oval = debuglevel;
	debuglevel = ival;
	log_printf(LOG_NOTICE, "Debug level is changed %d => %d", oval, ival);

	debugfp = debug_get_debugfp();
	if (debugfp != stderr) {
		sval = npppd_config_str(_this, "debug.logpath");
		// フォアグランドモードではない
		if (debugfp != NULL)
			fclose(debugfp);
		if (sval != NULL) {
			if ((debugfp = fopen(sval, "a+")) == NULL) {
				log_printf(LOG_ERR, 
				    "Failed to open logfile %s: %m", sval);
			} else {
				log_printf(LOG_INFO, 
				    "open logfile successfully %s", sval);
				debug_set_debugfp(debugfp);
			}
		}
	}
}

/** IPアドレスプールの設定を読み込みます。 */
static int
npppd_ip_addr_pool_load(npppd *_this)
{
	int n, i, j;
	const char *val;
	char *tok, *buf0, buf[NPPPD_CONFIG_BUFSIZ];
	npppd_pool pool0[NPPPD_MAX_POOL];
	struct radish_head *rd_curr, *rd_new;

	rd_curr = _this->rd;
	rd_new = NULL;
	
	n = 0;
	if (!rd_inithead((void *)&rd_new, 0x41,
	    sizeof(struct sockaddr_npppd),
	    offsetof(struct sockaddr_npppd, snp_addr),
	    sizeof(struct in_addr), sockaddr_npppd_match)) {
		goto reigai;
	}
	_this->rd = rd_new;

	/* 設定ファイル読み込み */
	if ((val = npppd_config_str(_this, "pool_list")) != NULL) {
		strlcpy(buf, val, sizeof(buf));
		buf0 = buf;
		while ((tok = strsep(&buf0, VAL_SEP)) != NULL) {
			if (tok[0] == '\0')
				continue;
			if (n >= countof(_this->pool)) {
				log_printf(LOG_WARNING,
				    "number of the pool reached "
				    "limit=%d",(int)countof(_this->pool));
				break;
			}
			if (npppd_pool_init(&pool0[n], _this, tok) != 0) {
				log_printf(LOG_WARNING, "Failed to initilize "
				    "npppd_pool '%s': %m", tok);
				goto reigai;
			}
			if (npppd_pool_reload(&pool0[n]) != 0)
				goto reigai;
			n++;
		}
	} else {
		if (npppd_pool_init(&pool0[n], _this, "default") != 0) {
			log_printf(LOG_WARNING, "Failed to initilize "
			    "npppd_pool 'default': %m");
			goto reigai;
		}
		if (npppd_pool_reload(&pool0[n++]) != 0)
			goto reigai;
	}
	for (; n < countof(pool0); n++)
		pool0[n].initialized = 0;

	_this->rd = rd_curr;	// backup 
	if (npppd_set_radish(_this, rd_new) != 0)
		goto reigai;

	for (i = 0; i < countof(_this->pool); i++) {
		if (_this->pool[i].initialized != 0)
			npppd_pool_uninit(&_this->pool[i]);
		if (pool0[i].initialized == 0)
			continue;
		_this->pool[i] = pool0[i];
		/* 参照の差し替え */
		for (j = 0; j < _this->pool[i].addrs_size; j++) {
			if (_this->pool[i].initialized == 0)
				continue;
			_this->pool[i].addrs[j].snp_data_ptr = &_this->pool[i];
		}
	}
	log_printf(LOG_INFO, "Loading pool config successfuly.");
	
	return 0;
reigai:
	/* rollback */
	for (i = 0; i < n; i++) {
		if (pool0[i].initialized != 0)
			npppd_pool_uninit(&pool0[i]);
	}

	if (rd_curr != NULL)
		_this->rd = rd_curr;

	if (rd_new != NULL) {
		rd_walktree(rd_new,
		    (int (*)(struct radish *, void *))rd_unlink,
		    rd_new->rdh_top);
		free(rd_new);
	}
	log_printf(LOG_NOTICE, "Loading pool config failed");

	return 1;
}

/* 認証レルム */
static int
npppd_auth_realm_reload(npppd *_this)
{
	int rval, ndef;
	const char *val;
	char buf[NPPPD_CONFIG_BUFSIZ * 2], *bufp, *tok;
	slist realms0, nrealms;
	npppd_auth_base *auth_base;

	rval = 0;
	slist_init(&realms0);
	slist_init(&nrealms);

	if (slist_add_all(&realms0, &_this->realms) != 0) {
		log_printf(LOG_WARNING, "slist_add_all() failed in %s(): %m",
		__func__);
		goto reigai;
	}

	ndef = 0;
	/* ローカルレルムのラベルを取得 */
	if ((val = npppd_config_str(_this, "auth.local.realm_list")) != NULL) {
		ndef++;
		strlcpy(buf, val, sizeof(buf));
		bufp = buf;
		while ((tok = strsep(&bufp, VAL_SEP)) != NULL) {
			if (tok[0] == '\0')
				continue;
			if (realm_list_contains(&nrealms, tok)) {
				log_printf(LOG_WARNING,
				    "label '%s' for auth.*.realm_list is not "
				    "unique", tok);
				goto reigai;
			}
			auth_base = realm_list_remove(&realms0, tok);
			if (auth_base != NULL &&
			    npppd_auth_get_type(auth_base)
				    != NPPPD_AUTH_TYPE_LOCAL) {
				/* 同じラベル名で認証レルムの型が変わった */
				slist_add(&realms0, auth_base);
				auth_base = NULL;
			}
			if (auth_base == NULL) {
				/* 新規作成 */
				if ((auth_base = npppd_auth_create(
				    NPPPD_AUTH_TYPE_LOCAL, tok, _this))
				    == NULL) {
					log_printf(LOG_WARNING,
					    "npppd_auth_create() failed in "
					    "%s(): %m", __func__);
					goto reigai;
				}
			}
			slist_add(&nrealms, auth_base);
		}
	}
#ifdef USE_NPPPD_RADIUS
	/* RADIUSレルムのラベルを取得 */
	if ((val = npppd_config_str(_this, "auth.radius.realm_list")) != NULL) {
		ndef++;
		strlcpy(buf, val, sizeof(buf));
		bufp = buf;
		while ((tok = strsep(&bufp, VAL_SEP)) != NULL) {
			if (tok[0] == '\0')
				continue;
			if (realm_list_contains(&nrealms, tok)) {
				log_printf(LOG_WARNING,
				    "label '%s' for auth.*.realm_list is not "
				    "unique", tok);
				goto reigai;
			}
			auth_base = realm_list_remove(&realms0, tok);
			if (auth_base != NULL &&
			    npppd_auth_get_type(auth_base)
				    != NPPPD_AUTH_TYPE_RADIUS) {
				/* 同じラベル名で認証レルムの型が変わった */
				slist_add(&realms0, auth_base);
				auth_base = NULL;
			}
			if (auth_base == NULL) {
				/* 新規作成 */
				if ((auth_base = npppd_auth_create(
				    NPPPD_AUTH_TYPE_RADIUS, tok, _this))
				    == NULL) {
					log_printf(LOG_WARNING,
					    "npppd_auth_create() failed in "
					    "%s(): %m", __func__);
					goto reigai;
				}
			}
			slist_add(&nrealms, auth_base);
		}
	}
#endif
#ifndef	NO_DEFAULT_REALM
	if (ndef == 0) {
		/*
		 * 従来版との互換性。デフォルトのレルムを使う。
		 */
		if (slist_length(&realms0) > 0) {
			slist_add_all(&nrealms, &realms0);
			slist_remove_all(&realms0);
		} else {
			if ((auth_base = npppd_auth_create(
			    NPPPD_AUTH_TYPE_LOCAL, "", _this)) == NULL) {
				log_printf(LOG_WARNING,
				    "malloc() failed in %s(): %m", __func__);
				goto reigai;
			}
			slist_add(&nrealms, auth_base);
#ifdef USE_NPPPD_RADIUS
			if ((auth_base = npppd_auth_create(
			    NPPPD_AUTH_TYPE_RADIUS, "", _this)) == NULL) {
				log_printf(LOG_WARNING,
				    "malloc() failed in %s(): %m", __func__);
				goto reigai;
			}
			slist_add(&nrealms, auth_base);
#endif
		}
	}
#endif
	if (slist_set_size(&_this->realms, slist_length(&nrealms)) != 0) {
		log_printf(LOG_WARNING, "slist_set_size() failed in %s(): %m",
		    __func__);
		goto reigai;
	}

	slist_itr_first(&realms0);
	while (slist_itr_has_next(&realms0)) {
		auth_base = slist_itr_next(&realms0);
		if (npppd_auth_is_disposing(auth_base))
			continue;
		npppd_auth_dispose(auth_base);
	}

	slist_itr_first(&nrealms);
	while (slist_itr_has_next(&nrealms)) {
		auth_base = slist_itr_next(&nrealms);
		rval |= npppd_auth_reload(auth_base);
	}
	slist_remove_all(&_this->realms);
	(void)slist_add_all(&_this->realms, &nrealms);
	(void)slist_add_all(&_this->realms, &realms0);

	slist_fini(&realms0);
	slist_fini(&nrealms);

	return rval;
reigai:

	slist_itr_first(&nrealms);
	while (slist_itr_has_next(&nrealms)) {
		auth_base = slist_itr_next(&nrealms);
		npppd_auth_destroy(auth_base);
	}
	slist_fini(&realms0);
	slist_fini(&nrealms);

	return 1;
}

static int
realm_list_contains(slist *list0, const char *label)
{
	npppd_auth_base *base;

	for (slist_itr_first(list0); slist_itr_has_next(list0); ) {
		base = slist_itr_next(list0);
		if (npppd_auth_is_disposing(base))
			continue;
		if (strcmp(npppd_auth_get_label(base), label) == 0)
			return 1;
	}

	return 0;
}

static npppd_auth_base *
realm_list_remove(slist *list0, const char *label)
{
	npppd_auth_base *base;

	for (slist_itr_first(list0); slist_itr_has_next(list0); ) {
		base = slist_itr_next(list0);
		if (npppd_auth_is_disposing(base))
			continue;
		if (strcmp(npppd_auth_get_label(base), label) == 0)
			return slist_itr_remove(list0);
	}

	return NULL;
}

/** インタフェースの設定を読み込みます。*/
int
npppd_ifaces_load_config(npppd *_this)
{
	int rval, n, nsession;
	const char *val;
	char *tok, *buf0, buf[BUFSIZ], buf1[128];

	rval = 0;
	n = 0;
	if ((val = npppd_config_str(_this, "interface_list")) != NULL) {
		strlcpy(buf, val, sizeof(buf));
		buf0 = buf;
		while ((tok = strsep(&buf0, VAL_SEP)) != NULL) {
			if (tok[0] == '\0')
				continue;
			if (n >= countof(_this->iface)) {
				log_printf(LOG_WARNING,
				    "number of the interface reached "
				    "limit=%d",(int)countof(_this->iface));
				break;
			}

			if (_this->iface[n].initialized != 0)
				nsession = _this->iface[n].nsession;
			else {
				npppd_iface_init(&_this->iface[n], tok);
				nsession = 0;
			}

			strlcpy(buf1, "interface.", sizeof(buf1));
			strlcat(buf1, tok, sizeof(buf1));

			_this->iface[n].set_ip4addr = 0;
			if ((val = npppd_config_str(_this,
			    config_key_prefix(buf1, "ip4addr"))) != NULL){
				if (inet_pton(AF_INET, val,
				    &_this->iface[n].ip4addr) != 1) {
					log_printf(LOG_ERR,
					    "configuration error at %s",
					    config_key_prefix(buf1,
					    "ip4addr"));
					return 1;
				}
				_this->iface[n].set_ip4addr = 1;
			}

			_this->iface[n].user_max_session = npppd_config_int(
			    _this, config_key_prefix(buf1, "user_max_session"),
			    NPPPD_DEFAULT_USER_MAX_PPP);
			_this->iface[n].max_session = npppd_config_int(_this,
			    config_key_prefix(buf1, "max_session"),
			    _this->max_session);

			_this->iface[n].nsession = nsession;
			_this->iface[n].npppd = _this;
			_this->iface[n].initialized = 1;
			n++;
		}
	}

	return rval;
}

static void
npppd_iface_binding_reload(npppd *_this, npppd_iface *iface,
    npppd_iface_binding *binding)
{
	int i, using_default, had_ipcp;
	const char *val;
	char key[128], *tok, *buf0, buf[NPPPD_CONFIG_BUFSIZ];

	NPPPD_CONFIG_ASSERT(_this != NULL);
	NPPPD_CONFIG_ASSERT(iface != NULL);
	NPPPD_CONFIG_ASSERT(binding != NULL);

	had_ipcp = (binding->ipcp != NULL)? 1 : 0;
	slist_fini(&binding->pools);
	memset(binding, 0, sizeof(npppd_iface_binding));
	if (iface->initialized == 0)
		return;


	/* create the key */
	strlcpy(key, "interface.", sizeof(key));
	strlcat(key, iface->ifname, sizeof(key));
	strlcat(key, ".ipcp_configuration", sizeof(key));

	using_default = 0;
	if ((val = npppd_config_str(_this, key)) != NULL) {
		for (i = 0; i < countof(_this->ipcp_config); i++){
			if (_this->ipcp_config[i].initialized == 0)
				continue;
			if (strcmp(_this->ipcp_config[i].label, val) == 0) {
				binding->ipcp = &_this->ipcp_config[i];
				break;
			}
		}
	} else if (_this->ipcp_config[0].initialized != 0 &&
	    _this->ipcp_config[0].label[0] == '\0') {
#ifndef	NO_DEFAULT_IPCP
		/* デフォルト IPCP がある。*/
		binding->ipcp = &_this->ipcp_config[0];
		using_default = 1;
#else
		using_default = 0;
#endif
	}
	slist_init(&binding->pools);
	if (binding->ipcp == NULL) {
		if (had_ipcp)
			log_printf(LOG_INFO, "%s has no ipcp", iface->ifname);
		return;
	}
	if ((val = npppd_ipcp_config_str(binding->ipcp, "pool_list")) != NULL) {
		strlcpy(buf, val, sizeof(buf));
		buf0 = buf;
		while ((tok = strsep(&buf0, VAL_SEP)) != NULL) {
			if (tok[0] == '\0')
				continue;
			for (i = 0; i < countof(_this->pool); i++) {
				if (_this->pool[i].initialized == 0)
					continue;
				if (strcmp(tok, _this->pool[i].label) != 0)
					continue;
				slist_add(&binding->pools, &_this->pool[i]);
				break;
			}
		}
	} else if (using_default) {
		if (_this->pool[0].initialized != 0)
			slist_add(&binding->pools, &_this->pool[0]);
	}
	log_printf(LOG_INFO, "%s is using ipcp=%s(%d pools).",
	    iface->ifname, binding->ipcp->name, slist_length(&binding->pools));
}
