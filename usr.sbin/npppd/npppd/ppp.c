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
/* $Id: ppp.c,v 1.1 2010/01/11 04:20:57 yasuoka Exp $ */
/**@file
 * {@link :: _npppd_ppp PPPインスタンス} に関する処理を提供します。
 */
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/param.h>
#include <netinet/in.h>
#include <net/if_dl.h>
#include <arpa/inet.h>
#include <stdlib.h>
#include <netdb.h>
#include <stdio.h>
#include <stdarg.h>
#include <strings.h>
#include <unistd.h>
#include <errno.h>
#include <syslog.h>
#include <sys/time.h>
#include <time.h>
#include <event.h>

#include "slist.h"

#include "npppd.h"
#include "time_utils.h"
#include "ppp.h"
#include "psm-opt.h"

#include "debugutil.h"

#ifdef	PPP_DEBUG
#define	PPP_DBG(x)	ppp_log x
#define	PPP_ASSERT(cond)					\
	if (!(cond)) {						\
	    fprintf(stderr,					\
		"\nASSERT(" #cond ") failed on %s() at %s:%d.\n"\
		, __func__, __FILE__, __LINE__);		\
	    abort(); 						\
	}
#else
#define	PPP_ASSERT(cond)			
#define	PPP_DBG(x)
#endif

#include "debugutil.h"

static u_int32_t ppp_seq = 0;

static void  ppp_stop0 __P((npppd_ppp *));
static int   ppp_recv_packet (npppd_ppp *, unsigned char *, int, int);
static const char * ppp_peer_auth_string(npppd_ppp *);
static void ppp_idle_timeout(int, short, void *);
#ifdef USE_NPPPD_PIPEX
static void ppp_on_network_pipex(npppd_ppp *);
#endif

#define AUTH_IS_PAP(ppp) 	((ppp)->peer_auth == PPP_AUTH_PAP)
#define AUTH_IS_CHAP(ppp)	((ppp)->peer_auth == PPP_AUTH_CHAP_MD5 ||\
				(ppp)->peer_auth == PPP_AUTH_CHAP_MS ||	\
				(ppp)->peer_auth == PPP_AUTH_CHAP_MS_V2)
#define AUTH_IS_EAP(ppp) 	((ppp)->peer_auth == PPP_AUTH_EAP)

/*
 * 終了処理
 *	ppp_lcp_finished	LCP が終了
 *				先方が TermReq
 *				こちらが TermReq (ppp_stop から状態遷移で)
 *	ppp_phy_downed		物理層が切れた
 *
 * どちらも ppp_stop0、ppp_down_others を呼び出している。
 */
/** npppd_ppp オブジェクトを生成。 */
npppd_ppp *
ppp_create()
{
	npppd_ppp *_this;

	if ((_this = malloc(sizeof(npppd_ppp))) == NULL) {
		log_printf(LOG_ERR, "malloc() failed in %s(): %m", __func__ );
		return NULL;
	}
	memset(_this, 0, sizeof(npppd_ppp));

	_this->snp.snp_family = AF_INET;
	_this->snp.snp_len = sizeof(_this->snp);
	_this->snp.snp_type = SNP_PPP;
	_this->snp.snp_data_ptr = _this;

	return _this;
}

/**
 * npppd_ppp を初期化します。
 * npppd_ppp#mru npppd_ppp#phy_label は呼び出し前にセットしてください。
 */
int
ppp_init(npppd *pppd, npppd_ppp *_this)
{

	PPP_ASSERT(_this != NULL);
	PPP_ASSERT(strlen(_this->phy_label) > 0);

	_this->id = -1;
	_this->ifidx = -1;
	_this->has_acf = 1;
	_this->recv_packet = ppp_recv_packet;
	_this->id = ppp_seq++;
	_this->pppd = pppd;

	lcp_init(&_this->lcp, _this);

	_this->mru = ppp_config_int(_this, "lcp.mru", DEFAULT_MRU);
	if (_this->outpacket_buf == NULL) {
		_this->outpacket_buf = malloc(_this->mru + 64);
		if (_this->outpacket_buf == NULL){
			log_printf(LOG_ERR, "malloc() failed in %s(): %m",
			    __func__);
			return -1;
		}
	}
	_this->adjust_mss = ppp_config_str_equal(_this, "ip.adjust_mss", "true",
	    0);
#ifdef USE_NPPPD_PIPEX
	_this->use_pipex = ppp_config_str_equal(_this, "pipex.enabled", "true",
	    1);
#endif
	/*
	 * ログの設定を読み込む。
	 */
	_this->log_dump_in =
	    ppp_config_str_equal(_this, "log.in.pktdump",  "true", 0);
	_this->log_dump_out =
	    ppp_config_str_equal(_this, "log.out.pktdump",  "true", 0);


#ifdef	USE_NPPPD_MPPE
	mppe_init(&_this->mppe, _this);
#endif
	ccp_init(&_this->ccp, _this);
	ipcp_init(&_this->ipcp, _this);
	pap_init(&_this->pap, _this);
	chap_init(&_this->chap, _this);

	/* アイドルタイマー関連 */
	_this->timeout_sec = ppp_config_int(_this, "idle_timeout", 0);
	if (!evtimer_initialized(&_this->idle_event))
		evtimer_set(&_this->idle_event, ppp_idle_timeout, _this);

	_this->auth_timeout = ppp_config_int(_this, "auth.timeout",
	    DEFAULT_AUTH_TIMEOUT);

	_this->lcp.echo_interval = ppp_config_int(_this,
	    "lcp.echo_interval", DEFAULT_LCP_ECHO_INTERVAL);
	_this->lcp.echo_max_retries = ppp_config_int(_this,
	    "lcp.echo_max_retries", DEFAULT_LCP_ECHO_MAX_RETRIES);
	_this->lcp.echo_retry_interval = ppp_config_int(_this,
	    "lcp.echo_retry_interval", DEFAULT_LCP_ECHO_RETRY_INTERVAL);

	return 0;
}

static void
ppp_set_tunnel_label(npppd_ppp *_this, char *buf, int lbuf)
{
	int flag, af;
	char hbuf[NI_MAXHOST], sbuf[NI_MAXSERV];

	hbuf[0] = 0;
	sbuf[0] = 0;
	af = ((struct sockaddr *)&_this->phy_info)->sa_family;
	if (af < AF_MAX) {
		flag = NI_NUMERICHOST;
		if (af == AF_INET || af == AF_INET6)
			flag |= NI_NUMERICSERV;
		if (getnameinfo((struct sockaddr *)&_this->phy_info,
		    ((struct sockaddr *)&_this->phy_info)->sa_len, hbuf,
		    sizeof(hbuf), sbuf, sizeof(sbuf), flag) != 0) {
			ppp_log(_this, LOG_ERR, "getnameinfo() failed at %s",
			    __func__);
			strlcpy(hbuf, "0.0.0.0", sizeof(hbuf));
			strlcpy(sbuf, "0", sizeof(sbuf));
		}
		if (af == AF_INET || af == AF_INET6)
			snprintf(buf, lbuf, "%s:%s", hbuf, sbuf);
		else
			snprintf(buf, lbuf, "%s", hbuf);
	} else if (af == NPPPD_AF_PHONE_NUMBER) {
		strlcpy(buf,
		    ((npppd_phone_number *)&_this->phy_info)->pn_number, lbuf);
	}
}
/**
 * npppd_ppp を開始します。
 * npppd_ppp#phy_context
 * npppd_ppp#send_packet
 * npppd_ppp#phy_close
 * npppd_ppp#phy_info
 * は呼び出し前にセットしてください。
 */
void
ppp_start(npppd_ppp *_this)
{
	char label[512];

	PPP_ASSERT(_this != NULL);
	PPP_ASSERT(_this->recv_packet != NULL);
	PPP_ASSERT(_this->send_packet != NULL);
	PPP_ASSERT(_this->phy_close != NULL);

	_this->start_time = time(NULL);
	_this->start_monotime = get_monosec();
	/*
	 * 下位レイヤの情報をログに残す。
	 */
	ppp_set_tunnel_label(_this, label, sizeof(label));
	ppp_log(_this, LOG_INFO, "logtype=Started tunnel=%s(%s)",
	    _this->phy_label, label);

	lcp_lowerup(&_this->lcp);
}

/**
 * Dialin proxy の準備をします。dialin proxy できない場合には、0 以外が
 * 返ります。
 */
int
ppp_dialin_proxy_prepare(npppd_ppp *_this, dialin_proxy_info *dpi)
{
	int renego_force, renego;

	renego = (ppp_config_str_equal(_this,
	    "l2tp.dialin.lcp_renegotiation", "disable", 0))? 0 : 1;
	renego_force = ppp_config_str_equal(_this,
	    "l2tp.dialin.lcp_renegotiation", "force", 0);
	if (renego_force)
		renego = 1;

	if (lcp_dialin_proxy(&_this->lcp, dpi, renego, renego_force) != 0) {
		ppp_log(_this, LOG_ERR, 
		    "Failed to proxy-dialin, proxied lcp is broken.");
		return 1;
	}

	return 0;
}

static void
ppp_down_others(npppd_ppp *_this)
{
	fsm_lowerdown(&_this->ccp.fsm);
	fsm_lowerdown(&_this->ipcp.fsm);

	npppd_release_ip(_this->pppd, _this);
	if (AUTH_IS_PAP(_this))
		pap_stop(&_this->pap);
	if (AUTH_IS_CHAP(_this))
		chap_stop(&_this->chap);
#ifdef USE_NPPPD_EAP_RADIUS 
	if (AUTH_IS_EAP(_this))
		eap_stop(&_this->eap);
#endif
	evtimer_del(&_this->idle_event);
}

void
ppp_stop(npppd_ppp *_this, const char *reason)
{
	ppp_stop_ex(_this, reason, PPP_DISCON_NO_INFORMATION, 0, 0, NULL);
}

/**
 * PPP を停止し、npppd_ppp オブジェクトを破棄します。
 * @param reason	停止の理由。特に理由がなければ NULL を指定します。
 *	この値で LCP の TermReq パケットの reason フィールドに格納されて、
 *	先方に通知されます。
 * @param code		disconnect code in {@link ::npppd_ppp_disconnect_code}.
 * @param proto		control protocol number.  see RFC3145.
 * @param direction	disconnect direction.  see RFC 3145
 */
void
ppp_stop_ex(npppd_ppp *_this, const char *reason,
    npppd_ppp_disconnect_code code, int proto, int direction,
    const char *message)
{
	PPP_ASSERT(_this != NULL);

	if (_this->disconnect_code == PPP_DISCON_NO_INFORMATION) {
		_this->disconnect_code = code;
		_this->disconnect_proto = proto;
		_this->disconnect_direction = direction;
		_this->disconnect_message = message;
	}
	ppp_down_others(_this);
	fsm_close(&_this->lcp.fsm, reason);
}

static void
ppp_stop0(npppd_ppp *_this)
{
	char mppe_str[BUFSIZ];
	char label[512];

	_this->end_monotime = get_monosec();

	if (_this->phy_close != NULL)
		_this->phy_close(_this);
	_this->phy_close = NULL;

	/*
	 * PPTP(GRE) の NAT/ブラックホール検出
	 */
	if (_this->lcp.dialin_proxy != 0 &&
	    _this->lcp.dialin_proxy_lcp_renegotiation == 0) {
		/*
		 * dialin-proxy、再ネゴシエーション無しでは LCPのやりとりは
		 * ない
		 */
	} else if (_this->lcp.recv_ress == 0) {	// 応答なし
		if (_this->lcp.recv_reqs == 0)	// 要求なし
			ppp_log(_this, LOG_WARNING, "no PPP frames from the "
			    "peer.  router/NAT issue? (may have filtered out)");
		else
			ppp_log(_this, LOG_WARNING, "my PPP frames may not "
			    "have arrived at the peer.  router/NAT issue? (may "
			    "be the only-first-person problem)");
	}
#ifdef USE_NPPPD_PIPEX
	if (npppd_ppp_pipex_disable(_this->pppd, _this) != 0)
		ppp_log(_this, LOG_ERR,
		    "npppd_ppp_pipex_disable() failed: %m");
#endif

	ppp_set_tunnel_label(_this, label, sizeof(label));
#ifdef	USE_NPPPD_MPPE
	if (_this->mppe_started) {
		snprintf(mppe_str, sizeof(mppe_str),
		    "mppe=yes mppe_in=%dbits,%s mppe_out=%dbits,%s",
		    _this->mppe.recv.keybits,
		    (_this->mppe.recv.stateless)? "stateless" : "stateful",
		    _this->mppe.send.keybits,
		    (_this->mppe.send.stateless)? "stateless" : "stateful");
	} else
#endif
		snprintf(mppe_str, sizeof(mppe_str), "mppe=no");
	ppp_log(_this, LOG_NOTICE,
		"logtype=TUNNELUSAGE user=\"%s\" duration=%ldsec layer2=%s "
		"layer2from=%s auth=%s data_in=%qubytes,%upackets "
		"data_out=%qubytes,%upackets error_in=%u error_out=%u %s "
		"iface=%s",
		_this->username[0]? _this->username : "<unknown>",
		(long)(_this->end_monotime - _this->start_monotime),
		_this->phy_label,  label,
		_this->username[0]? ppp_peer_auth_string(_this) : "none",
		_this->ibytes, _this->ipackets, _this->obytes, _this->opackets,
		_this->ierrors, _this->oerrors, mppe_str,
		npppd_ppp_get_iface_name(_this->pppd, _this));

	npppd_ppp_unbind_iface(_this->pppd, _this);
#ifdef	USE_NPPPD_MPPE
	mppe_fini(&_this->mppe);
#endif
	evtimer_del(&_this->idle_event);

	npppd_release_ip(_this->pppd, _this);
	ppp_destroy(_this);
}

/**
 * npppd_ppp オブジェクトを破棄します。ppp_start をコール後は、ppp_stop() を
 * を使用し、この関数は使いません。
 */
void
ppp_destroy(void *ctx)
{
	npppd_ppp *_this = ctx;

	if (_this->proxy_authen_resp != NULL)
		free(_this->proxy_authen_resp);
	/*
	 * ppp_stop しても、先方から PPP フレームが届き、また開始してしま
	 * っている場合があるので、再度 down, stop
	 */
	fsm_lowerdown(&_this->ccp.fsm);
	fsm_lowerdown(&_this->ipcp.fsm);
	pap_stop(&_this->pap);
	chap_stop(&_this->chap);

	if (_this->outpacket_buf != NULL)
		free(_this->outpacket_buf);

	free(_this);
}

/************************************************************************
 * プロトコルに関するイベント
 ************************************************************************/
static const char *
ppp_peer_auth_string(npppd_ppp *_this)
{
	switch(_this->peer_auth) {
	case PPP_AUTH_PAP:		return "PAP";
	case PPP_AUTH_CHAP_MD5:		return "MD5-CHAP";
	case PPP_AUTH_CHAP_MS:		return "MS-CHAP";
	case PPP_AUTH_CHAP_MS_V2:	return "MS-CHAP-V2";
	case PPP_AUTH_EAP:		return "EAP";
	default:			return "ERROR";
	}
}

/**
 * LCPがアップした場合に呼び出されます。
 */
void
ppp_lcp_up(npppd_ppp *_this)
{
#ifdef USE_NPPPD_MPPE
	if (MPPE_REQUIRED(_this) && !MPPE_MUST_NEGO(_this)) {
		ppp_log(_this, LOG_ERR, "MPPE is required, auth protocol must "
		    "be MS-CHAP-V2 or EAP");
		ppp_stop(_this, "Encryption required");
		return;
	}
#endif
	/*
	 * 相手が大きな MRU を指定しても、自分の MRU 以下にする。ここで、
	 * peer_mtu を縮めれると、経路 MTU が縮むので、MRU を越えるような
	 * パケットは到達しないようになる。(ことを期待している)
	 */
	if (_this->peer_mru > _this->mru)
		_this->peer_mru = _this->mru;

	if (_this->peer_auth != 0 && _this->auth_runonce == 0) {
		if (AUTH_IS_PAP(_this)) {
			pap_start(&_this->pap);
			_this->auth_runonce = 1;
			return;
		}
		if (AUTH_IS_CHAP(_this)) {
			chap_start(&_this->chap);
			_this->auth_runonce = 1;
			return;
		}
#ifdef USE_NPPPD_EAP_RADIUS
                if (AUTH_IS_EAP(_this)) {
                        eap_init(&_this->eap, _this);
                        eap_start(&_this->eap);
                        return;
                }
#endif
	}
	if (_this->peer_auth == 0)
		ppp_auth_ok(_this);
}

/**
 * LCPが終了した場合に呼び出されます。
 * <p>STOPPED また CLOSED ステートに入った場合に呼び出されます。</p>
 */
void
ppp_lcp_finished(npppd_ppp *_this)
{
	PPP_ASSERT(_this != NULL);

	ppp_down_others(_this);

	fsm_lowerdown(&_this->lcp.fsm);
	ppp_stop0(_this);
}

/**
 * 物理層が切断された場合に物理層から呼び出されます。
 * <p>
 * 物理層が PPPフレームを入出力できないという状況でこの関数を呼び出して
 * ください。紳士的に PPP を切断する場合には、{@link ::#ppp_stop} を使い
 * ます。</p>
 */
void
ppp_phy_downed(npppd_ppp *_this)
{
	PPP_ASSERT(_this != NULL);

	ppp_down_others(_this);
	fsm_lowerdown(&_this->lcp.fsm);
	fsm_close(&_this->lcp.fsm, NULL);

	ppp_stop0(_this);
}

static const char *
proto_name(uint16_t proto)
{
	switch (proto) {
	case PPP_PROTO_IP:		return "ip";
	case PPP_PROTO_LCP:		return "lcp";
	case PPP_PROTO_PAP:		return "pap";
	case PPP_PROTO_CHAP:		return "chap";
	case PPP_PROTO_EAP:		return "eap";
	case PPP_PROTO_MPPE:		return "mppe";
	case PPP_PROTO_NCP | NCP_CCP:	return "ccp";
	case PPP_PROTO_NCP | NCP_IPCP:	return "ipcp";
	// 以下ログ出力用
	case PPP_PROTO_NCP | NCP_IP6CP:	return "ip6cp";
	case PPP_PROTO_ACSP:		return "acsp";
	}
	return "unknown";
}

/** 認証が成功した場合に呼び出されます。*/
void
ppp_auth_ok(npppd_ppp *_this)
{
	if (npppd_ppp_bind_iface(_this->pppd, _this) != 0) {
		ppp_log(_this, LOG_WARNING, "No interface binding.");
		ppp_stop(_this, NULL);

		return;
	}
	if (_this->realm != NULL) {
		npppd_ppp_get_username_for_auth(_this->pppd, _this,
		    _this->username, _this->username);
		if (!npppd_check_calling_number(_this->pppd, _this)) {
			ppp_log(_this, LOG_ALERT,
			    "logtype=TUNNELDENY user=\"%s\" "
			    "reason=\"Calling number check is failed\"",
			    _this->username);
			    /* XXX */
			ppp_stop(_this, NULL);
			return;
		}
	}
	if (_this->peer_auth != 0) {
		/* ユーザ毎の最大接続数を制限する */
		if (!npppd_check_user_max_session(_this->pppd, _this)) {
#ifdef IDGW
			ppp_log(_this, LOG_ALERT, "logtype=TUNNELDENY user=\"%s\" "
			    "reason=\"PPP duplicate login limit exceeded\"",
			    _this->username);
#else
			ppp_log(_this, LOG_WARNING,
			    "user %s exceeds user-max-session limit",
			    _this->username);
#endif
			ppp_stop(_this, NULL);

			return;
		}
		PPP_ASSERT(_this->realm != NULL);
	}

	if (!npppd_ppp_iface_is_ready(_this->pppd, _this)) {
		ppp_log(_this, LOG_WARNING,
		    "interface '%s' is not ready.",
		    npppd_ppp_get_iface_name(_this->pppd, _this));
		ppp_stop(_this, NULL);

		return;
	}
	if (_this->proxy_authen_resp != NULL) {
		free(_this->proxy_authen_resp);
		_this->proxy_authen_resp = NULL;
	}

	fsm_lowerup(&_this->ipcp.fsm);
	fsm_open(&_this->ipcp.fsm);
#ifdef	USE_NPPPD_MPPE
	if (MPPE_MUST_NEGO(_this)) {
		fsm_lowerup(&_this->ccp.fsm);
		fsm_open(&_this->ccp.fsm);
	}
#endif

	return;
}

/** event からコールバックされるイベントハンドラです */
static void
ppp_idle_timeout(int fd, short evtype, void *context)
{
	npppd_ppp *_this;

	_this = context;

	ppp_log(_this, LOG_NOTICE, "Idle timeout(%d sec)", _this->timeout_sec);
	ppp_stop(_this, NULL);
}

/** アイドルタイマーをリセットします。アイドルでは無い場合に呼び出します。 */
void
ppp_reset_idle_timeout(npppd_ppp *_this)
{
	struct timeval tv;

	//PPP_DBG((_this, LOG_INFO, "%s", __func__));
	evtimer_del(&_this->idle_event);
	if (_this->timeout_sec > 0) {
		tv.tv_usec = 0;
		tv.tv_sec = _this->timeout_sec;

		evtimer_add(&_this->idle_event, &tv);
	}
}

/** IPCP が完了した場合に呼び出されます */
void
ppp_ipcp_opened(npppd_ppp *_this)
{
	time_t curr_time;

	curr_time = get_monosec();

	npppd_set_ip_enabled(_this->pppd, _this, 1);
	if (_this->logged_acct_start == 0) {
		char label[512], ipstr[64];

		ppp_set_tunnel_label(_this, label, sizeof(label));

		strlcpy(ipstr, " ip=", sizeof(ipstr));
		strlcat(ipstr, inet_ntoa(_this->ppp_framed_ip_address),
		    sizeof(ipstr));
		if (_this->ppp_framed_ip_netmask.s_addr != 0xffffffffL) {
			strlcat(ipstr, ":", sizeof(ipstr));
			strlcat(ipstr, inet_ntoa(_this->ppp_framed_ip_netmask),
			    sizeof(ipstr));
		}

		ppp_log(_this, LOG_NOTICE,
		    "logtype=TUNNELSTART user=\"%s\" duration=%lusec layer2=%s "
 		    "layer2from=%s auth=%s %s iface=%s%s",
		    _this->username[0]? _this->username : "<unknown>",
		    (long)(curr_time - _this->start_monotime),
		    _this->phy_label, label,
		    _this->username[0]? ppp_peer_auth_string(_this) : "none",
 		    ipstr, npppd_ppp_get_iface_name(_this->pppd, _this),
		    (_this->lcp.dialin_proxy != 0)? " dialin_proxy=yes" : ""
		    );
		_this->logged_acct_start = 1;
		ppp_reset_idle_timeout(_this);
	}
#ifdef USE_NPPPD_PIPEX
	ppp_on_network_pipex(_this);
#endif
}

/** CCP が Opened になった場合に呼び出されます。*/
void
ppp_ccp_opened(npppd_ppp *_this)
{
#ifdef USE_NPPPD_MPPE
	if (_this->ccp.mppe_rej == 0) {
		if (_this->mppe_started == 0) {
			mppe_start(&_this->mppe);
		}
	} else {
		ppp_log(_this, LOG_INFO, "mppe is rejected by peer");
		if (_this->mppe.required)
			ppp_stop(_this, "MPPE is requred");
	}
#endif
#ifdef USE_NPPPD_PIPEX
	ppp_on_network_pipex(_this);
#endif
}

/************************************************************************
 * ネットワーク I/O 関連
 ************************************************************************/
/**
 * パケット受信
 * @param	flags	受信したパケットについての情報をフラグで表します。
 *	現在、PPP_IO_FLAGS_MPPE_ENCRYPTED が指定される場合があります。
 * @return	成功した場合に 0 が返り、失敗した場合に 1 が返ります。
 */
static int
ppp_recv_packet(npppd_ppp *_this, unsigned char *pkt, int lpkt, int flags)
{
	u_char *inp, *inp_proto;
	uint16_t proto;

	PPP_ASSERT(_this != NULL);

	inp = pkt;

	if (lpkt < 4) {
		ppp_log(_this, LOG_DEBUG, "%s(): Rcvd short header.", __func__);
		return 0;
	}


	if (_this->has_acf == 0) {
		/* nothing to do */
	} else if (inp[0] == PPP_ALLSTATIONS && inp[1] == PPP_UI) {
		inp += 2;
	} else {
		/*
		 * Address and Control Field Compression
		 */
		if (!psm_opt_is_accepted(&_this->lcp, acfc) &&
		    _this->logged_no_address == 0) {
			/*
			 * パケット落ちが発生する環境では、こちらは LCP
			 * が確立していないのに、Windows 側が完了していて、
			 * ACFC されたパケットが届く。
			 */
			ppp_log(_this, LOG_INFO,
			    "%s: Rcvd broken frame.  ACFC is not accepted, "
			    "but received ppp frame that has no address.",
			    __func__);
			/*
			 * Yamaha RTX-1000 では、ACFC を Reject するのに、
			 * パケットにアドレスは入っていないので、ログが
			 * 大量に出力されてしまう。
			 */
			_this->logged_no_address = 1;
		}
	}
	inp_proto = inp;
	if ((inp[0] & 0x01) != 0) {
		/*
		 * Protocol Field Compression
		 */
		if (!psm_opt_is_accepted(&_this->lcp, pfc)) {
			ppp_log(_this, LOG_INFO,
			    "%s: Rcvd broken frame.  No protocol field: "
			    "%02x %02x", __func__, inp[0], inp[1]);
			return 1;
		}
		GETCHAR(proto, inp);
	} else {
		GETSHORT(proto, inp);
	}

	if (_this->log_dump_in != 0 && debug_get_debugfp() != NULL) {
		char buf[256];

		snprintf(buf, sizeof(buf), "log.%s.in.pktdump",
		    proto_name(proto));
		if (ppp_config_str_equal(_this, buf, "true", 0) != 0)  {
			ppp_log(_this, LOG_DEBUG,
			    "PPP input dump proto=%s(%d/%04x)",
			    proto_name(proto), proto, proto);
			show_hd(debug_get_debugfp(), pkt, lpkt);
		}
	}
#ifdef USE_NPPPD_PIPEX
	if (_this->pipex_enabled != 0 &&
	    _this->tunnel_type == PPP_TUNNEL_PPPOE) {
		switch (proto) {
		case PPP_PROTO_IP:
			return 2;		/* handled by PIPEX */
		case PPP_PROTO_NCP | NCP_CCP:
			if (lpkt - (inp - pkt) < 4)
				break;		/* エラーだが fsm.c で処理 */
			if (*inp == 0x0e ||	/* Reset-Request */
			    *inp == 0x0f	/* Reset-Ack */) {
				return 2;	/* handled by PIPEX */
			}
			/* FALLTHROUGH */
		default:
			break;
		}
	}
#endif /* USE_NPPPD_PIPEX */

	// MPPE のチェック
	switch (proto) {
#ifdef	USE_NPPPD_MPPE
	case PPP_PROTO_IP:
		if ((flags & PPP_IO_FLAGS_MPPE_ENCRYPTED) == 0) {
			if (MPPE_REQUIRED(_this)) {
				/* MPPE 必須なのに、生 IP。*/

				if (_this->logged_naked_ip == 0) {
					ppp_log(_this, LOG_INFO,
					    "mppe is required but received "
					    "naked IP.");
					/* ログに残すのは最初の 1 回だけ */
					_this->logged_naked_ip = 1;
				}
				/*
				 * Windows は、MPPE 未確立、IPCP 確立の状態で
				 * 生IPパケットを投げてくる※1。CCP がパケット
				 * ロスなどで確立が遅れた場合、高確率でこの状
				 * 態に陥る。ここで ppp_stop する場合、パケット
				 * ロスや順序入れ換えが発生する環境では、『繋
				 * がらない』現象にみえる。
				 * (※1 少なくとも Windows 2000 Pro SP4)
				 ppp_stop(_this, "Encryption is required.");
				 */
				return 1;
			}
			if (MPPE_READY(_this)) {
				/* MPPE 確立したのに、生 IP。*/
				ppp_log(_this, LOG_WARNING,
				    "mppe is avaliable but received naked IP.");
			}
		}
		/* else MPPE からの入力 */
		break;
	case PPP_PROTO_MPPE:
#ifdef USE_NPPPD_MPPE
		if (_this->mppe_started == 0)  {
#else
		{
#endif
			ppp_log(_this, LOG_ERR,
			    "mppe packet is received but mppe is stopped.");
			return 1;
		}
		break;
#endif
	}

	switch (proto) {
	case PPP_PROTO_IP:
		npppd_network_output(_this->pppd, _this, AF_INET, inp,
		    lpkt - (inp - pkt));
		goto handled;
	case PPP_PROTO_LCP:
		fsm_input(&_this->lcp.fsm, inp, lpkt - (inp - pkt));
		goto handled;
	case PPP_PROTO_PAP:
		pap_input(&_this->pap, inp, lpkt - (inp - pkt));
		goto handled;
	case PPP_PROTO_CHAP:
		chap_input(&_this->chap, inp, lpkt - (inp - pkt));
		goto handled;
#ifdef USE_NPPPD_EAP_RADIUS
	case PPP_PROTO_EAP:
		eap_input(&_this->eap, inp, lpkt - (inp - pkt));
		goto handled;
#endif
#ifdef	USE_NPPPD_MPPE
	case PPP_PROTO_MPPE:
#ifdef USE_NPPPD_PIPEX
		if (_this->pipex_enabled != 0)
			return -1; /* silent discard */
#endif /* USE_NPPPD_PIPEX */
		mppe_input(&_this->mppe, inp, lpkt - (inp - pkt));
		goto handled;
#endif
	default:
		if ((proto & 0xff00) == PPP_PROTO_NCP) {
			switch (proto & 0xff) {
			case NCP_CCP:	/* Compression */
#ifdef	USE_NPPPD_MPPE
				if (MPPE_MUST_NEGO(_this)) {
					fsm_input(&_this->ccp.fsm, inp,
					    lpkt - (inp - pkt));
					goto handled;
				}
				// ネゴする必要のない場合は Protocol Reject
#endif
				break;
			case NCP_IPCP:	/* IPCP */
				fsm_input(&_this->ipcp.fsm, inp,
				    lpkt - (inp - pkt));
				goto handled;
			}
		}
	}
	/* ProtoRej ログに残す */
	ppp_log(_this, LOG_INFO, "unhandled protocol %s, %d(%04x)",
	    proto_name(proto), proto, proto);

	if ((flags & PPP_IO_FLAGS_MPPE_ENCRYPTED) != 0) {
		/*
		 * Don't return a protocol-reject for the packet was encrypted,
		 * because lcp protocol-reject is not encrypted by mppe.
		 */
	} else {
		/*
		 * as RFC1661: Rejected-Information MUST be truncated to
		 * comply with the peer's established MRU.
		 */
		lcp_send_protrej(&_this->lcp, inp_proto,
		    MIN(lpkt - (inp_proto - pkt), NPPPD_MIN_MRU - 32));
	}

	return 1;
handled:

	return 0;
}

/** PPPに出力する場合に呼び出します。 */
inline void
ppp_output(npppd_ppp *_this, uint16_t proto, u_char code, u_char id,
    u_char *datap, int ldata)
{
	u_char *outp;
	int outlen, hlen, is_lcp = 0;

	outp = _this->outpacket_buf;

	/* LCPは圧縮を使わない */
	is_lcp = (proto == PPP_PROTO_LCP)? 1 : 0;


	if (_this->has_acf == 0 ||
		(!is_lcp && psm_peer_opt_is_accepted(&_this->lcp, acfc))) {
		/*
		 * Address and Control Field (ACF) がそもそも無い場合や
		 * ACFC がネゴされている場合は ACF を追加しない。
		 */
	} else {
		PUTCHAR(PPP_ALLSTATIONS, outp); 
		PUTCHAR(PPP_UI, outp); 
	}
	if (!is_lcp && proto <= 0xff &&
	    psm_peer_opt_is_accepted(&_this->lcp, pfc)) {
		/*
		 * Protocol Field Compression
		 */
		PUTCHAR(proto, outp); 
	} else {
		PUTSHORT(proto, outp); 
	}
	hlen = outp - _this->outpacket_buf;

	if (_this->mru > 0) {
		if (MRU_PKTLEN(_this->mru, proto) < ldata) {
			PPP_DBG((_this, LOG_ERR, "packet too large %d. mru=%d",
			    ldata , _this->mru));
			_this->oerrors++;
			PPP_ASSERT("NOT REACHED HERE" == NULL);
			return;
		}
	}

	if (code != 0) {
		outlen = ldata + HEADERLEN;

		PUTCHAR(code, outp);
		PUTCHAR(id, outp);
		PUTSHORT(outlen, outp);
	} else {
		outlen = ldata;
	}

	if (outp != datap && ldata > 0)
		memmove(outp, datap, ldata);

	if (_this->log_dump_out != 0 && debug_get_debugfp() != NULL) {
		char buf[256];

		snprintf(buf, sizeof(buf), "log.%s.out.pktdump",
		    proto_name(proto));
		if (ppp_config_str_equal(_this, buf, "true", 0) != 0)  {
			ppp_log(_this, LOG_DEBUG,
			    "PPP output dump proto=%s(%d/%04x)",
			    proto_name(proto), proto, proto);
			show_hd(debug_get_debugfp(),
			    _this->outpacket_buf, outlen + hlen);
		}
	}
	_this->send_packet(_this, _this->outpacket_buf, outlen + hlen, 0);
}

/**
 * PPP 出力用のバッファ領域を返します。ヘッダ圧縮によるズレを補正します。
 * バッファ領域の長さは npppd_ppp#mru 以上です。
 */
u_char *
ppp_packetbuf(npppd_ppp *_this, int proto)
{
	int save;

	save = 0;
	if (proto != PPP_PROTO_LCP) {
		if (psm_peer_opt_is_accepted(&_this->lcp, acfc))
			save += 2;
		if (proto <= 0xff && psm_peer_opt_is_accepted(&_this->lcp, pfc))
			save += 1;
	}
	return _this->outpacket_buf + (PPP_HDRLEN - save);
}

/** このインスタンスに基づいたラベルから始まるログを記録します。 */
int
ppp_log(npppd_ppp *_this, int prio, const char *fmt, ...)
{
	int status;
	char logbuf[BUFSIZ];
	va_list ap;

	PPP_ASSERT(_this != NULL);

	va_start(ap, fmt);
	snprintf(logbuf, sizeof(logbuf), "ppp id=%u layer=base %s",
	    _this->id, fmt);
	status = vlog_printf(prio, logbuf, ap);
	va_end(ap);

	return status;
}

#ifdef USE_NPPPD_RADIUS
#define UCHAR_BUFSIZ 255
/**
 * RADIUS パケットの Framed-IP-Address アートリビュートと Framed-IP-Netmask
 * アートリビュートを処理します。
 */ 
void
ppp_proccess_radius_framed_ip(npppd_ppp *_this, RADIUS_PACKET *pkt)
{
	struct in_addr ip4;
	
	if (radius_get_ipv4_attr(pkt, RADIUS_TYPE_FRAMED_IP_ADDRESS, &ip4)
	    == 0)
		_this->realm_framed_ip_address = ip4;

	_this->realm_framed_ip_netmask.s_addr = 0xffffffffL;
	if (radius_get_ipv4_attr(pkt, RADIUS_TYPE_FRAMED_IP_NETMASK, &ip4)
	    == 0)
		_this->realm_framed_ip_netmask = ip4;
}

/**
 * RADIUS 認証要求用の RADIUSアートリビュートをセットします。
 * 成功した場合には 0 が返ります。
 */
int
ppp_set_radius_attrs_for_authreq(npppd_ppp *_this,
    radius_req_setting *rad_setting, RADIUS_PACKET *radpkt)
{
	/* RFC 2865 "5.4 NAS-IP-Address" or RFC3162 "2.1. NAS-IPv6-Address" */
	if (radius_prepare_nas_address(rad_setting, radpkt) != 0)
		goto reigai;

	/* RFC 2865 "5.6. Service-Type" */
	if (radius_put_uint32_attr(radpkt, RADIUS_TYPE_SERVICE_TYPE,
	    RADIUS_SERVICE_TYPE_FRAMED) != 0)
		goto reigai;

	/* RFC 2865 "5.7. Framed-Protocol" */
	if (radius_put_uint32_attr(radpkt, RADIUS_TYPE_FRAMED_PROTOCOL, 
	    RADIUS_FRAMED_PROTOCOL_PPP) != 0)
		goto reigai;

	if (_this->calling_number[0] != '\0') {
		if (radius_put_string_attr(radpkt,
		    RADIUS_TYPE_CALLING_STATION_ID, _this->calling_number) != 0)
			return 1;
	}
	return 0;
reigai:
	return 1;
}
#endif

#ifdef USE_NPPPD_PIPEX
/** Network が有効になった時の callback 関数 PIPEX 用*/
static void
ppp_on_network_pipex(npppd_ppp *_this)
{
	if (_this->use_pipex == 0)
		return;	
	if (_this->tunnel_type != PPP_TUNNEL_PPTP &&
	    _this->tunnel_type != PPP_TUNNEL_PPPOE)
		return;
	if (_this->pipex_started != 0)
		return;	/* already started */

	PPP_DBG((_this, LOG_INFO, "%s() assigned_ip4_enabled = %s, "
	    "MPPE_MUST_NEGO = %s, ccp.fsm.state = %s", __func__,
	    (_this->assigned_ip4_enabled != 0)? "true" : "false",
	    (MPPE_MUST_NEGO(_this))? "true" : "false",
	    (_this->ccp.fsm.state == OPENED)? "true" : "false"));

	if (_this->assigned_ip4_enabled != 0 &&
	    (!MPPE_MUST_NEGO(_this) || _this->ccp.fsm.state == OPENED)) {
		/* IPCP が完了し，MPPE 不要または MPPE 完了した場合 */
		npppd_ppp_pipex_enable(_this->pppd, _this);
		ppp_log(_this, LOG_NOTICE, "Using pipex=%s",
		    (_this->pipex_enabled != 0)? "yes" : "no");
		_this->pipex_started = 1;
	}
	/* else CCP or IPCP 待ち */
}
#endif

#ifdef	NPPPD_USE_CLIENT_AUTH
#ifdef USE_NPPPD_LINKID
#include "linkid.h"
#endif
/** 端末IDをセットします */
void
ppp_set_client_auth_id(npppd_ppp *_this, const char *client_auth_id)
{
	PPP_ASSERT(_this != NULL);
	PPP_ASSERT(client_auth_id != NULL);
	PPP_ASSERT(strlen(client_auth_id) <= NPPPD_CLIENT_AUTH_ID_MAXLEN);

	strlcpy(_this->client_auth_id, client_auth_id,
	    sizeof(_this->client_auth_id));
	_this->has_client_auth_id = 1;
#ifdef USE_NPPPD_LINKID
	linkid_purge(_this->ppp_framed_ip_address);
#endif
	ppp_log(_this, LOG_NOTICE,
	    "Set client authentication id successfully.  linkid=\"%s\" client_auth_id=%s",
	    _this->username, client_auth_id);
}
#endif
