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
/**@file
 * L2TP LNS のコントロールコネクションの処理を提供します。
 */
// $Id: l2tp_ctrl.c,v 1.1 2010/01/11 04:20:57 yasuoka Exp $
#include <sys/types.h>
#include <sys/param.h>
#include <sys/time.h>
#include <sys/socket.h>
#include <sys/endian.h>
#include <netinet/in.h>
#include <net/if.h>
#include <arpa/inet.h>
#include <stdlib.h>
#include <syslog.h>
#include <stdio.h>
#include <stdarg.h>
#include <stddef.h>
#include <netdb.h>
#include <time.h>
#include <string.h>
#include <event.h>
#include <ifaddrs.h>

#ifdef USE_LIBSOCKUTIL
#include <seil/sockfromto.h>
#endif

#include "time_utils.h"
#include "ipsec_util.h"
#include "bytebuf.h"
#include "hash.h"
#include "debugutil.h"
#include "slist.h"
#include "l2tp.h"
#include "l2tp_local.h"
#include "l2tp_subr.h"
#include "net_utils.h"
#include "config_helper.h"
#include "version.h"
#ifdef _SEIL_EXT_
#include "rtev.h"
#endif

static int                l2tp_ctrl_init (l2tp_ctrl *, l2tpd *, struct sockaddr *, struct sockaddr *, void *);
static void               l2tp_ctrl_reload (l2tp_ctrl *);
static int                l2tp_ctrl_send_disconnect_notify (l2tp_ctrl *);
#if 0
static void               l2tp_ctrl_purge_ipsec_sa (l2tp_ctrl *);
#endif
static void               l2tp_ctrl_timeout (int, short, void *);
static int                l2tp_ctrl_resend_una_packets (l2tp_ctrl *);
static void               l2tp_ctrl_destroy_all_calls (l2tp_ctrl *);
static int                l2tp_ctrl_disconnect_all_calls (l2tp_ctrl *);
static void               l2tp_ctrl_reset_timeout (l2tp_ctrl *);
static inline int         l2tp_ctrl_txwin_size (l2tp_ctrl *);
static inline int         l2tp_ctrl_txwin_is_full (l2tp_ctrl *);
static int                l2tp_ctrl_recv_SCCRQ (l2tp_ctrl *, u_char *, int, l2tpd *, struct sockaddr *);
static int                l2tp_ctrl_send_StopCCN (l2tp_ctrl *, int);
static int                l2tp_ctrl_recv_StopCCN (l2tp_ctrl *, u_char *, int);
static void               l2tp_ctrl_send_SCCRP (l2tp_ctrl *);
static int                l2tp_ctrl_send_HELLO (l2tp_ctrl *);
static int                l2tp_ctrl_send_ZLB (l2tp_ctrl *);
static inline const char  *l2tp_ctrl_state_string (l2tp_ctrl *);

#ifdef	L2TP_CTRL_DEBUG
#define	L2TP_CTRL_ASSERT(x)	ASSERT(x)
#define	L2TP_CTRL_DBG(x)	l2tp_ctrl_log x
#else
#define	L2TP_CTRL_ASSERT(x)
#define	L2TP_CTRL_DBG(x)
#endif

/** l2tp_ctrl の ID番号のシーケンス番号 */
static unsigned l2tp_ctrl_id_seq = 0;

#define SEQ_LT(a,b)	((int16_t)((a) - (b)) <  0)
#define SEQ_GT(a,b)	((int16_t)((a) - (b)) >  0)

/**
 * {@link ::_l2tp_ctrl L2TP LNS コントロールコネクション}のインスタンス生成
 */
l2tp_ctrl *
l2tp_ctrl_create(void)
{
	l2tp_ctrl *_this;

	if ((_this = malloc(sizeof(l2tp_ctrl))) == NULL)
		return NULL;

	memset(_this, 0, sizeof(l2tp_ctrl));
	return (l2tp_ctrl *)_this;
}

/**
 * {@link ::_l2tp_ctrl L2TP LNS コントロールコネクション}のインスタンスの
 * 初期化と開始を行います。
 */
static int
l2tp_ctrl_init(l2tp_ctrl *_this, l2tpd *_l2tpd, struct sockaddr *peer,
    struct sockaddr *sock, void *nat_t_ctx)
{
	int tunid, i;
	bytebuffer *bytebuf;
	time_t curr_time;

	memset(_this, 0, sizeof(l2tp_ctrl));

	curr_time = get_monosec();
	_this->l2tpd = _l2tpd;
	_this->state = L2TP_CTRL_STATE_IDLE;
	_this->last_snd_ctrl = curr_time;

	slist_init(&_this->call_list);
	/*
	 * 空いているトンネルIDを探す
	 */
	i = 0;
	_this->id = ++l2tp_ctrl_id_seq;
	for (i = 0, tunid = _this->id; ; i++, tunid++) {
		tunid &= 0xffff;
		_this->tunnel_id = l2tp_ctrl_id_seq & 0xffff;
		if (tunid == 0)
			continue;
		if (l2tpd_get_ctrl(_l2tpd, tunid) == NULL)
			break;
		if (i > 80000) {
			// バグに違いない
			l2tpd_log(_l2tpd, LOG_ERR, "Too many l2tp controls");
			return -1;
		}
	}

	_this->tunnel_id = tunid;

	L2TP_CTRL_ASSERT(peer != NULL);
	L2TP_CTRL_ASSERT(sock != NULL);
	memcpy(&_this->peer, peer, peer->sa_len);
	memcpy(&_this->sock, sock, sock->sa_len);

	/* 送信バッファの準備 */
	_this->winsz = L2TPD_DEFAULT_SEND_WINSZ;
	if ((_this->snd_buffers = calloc(_this->winsz, sizeof(bytebuffer *)))
	    == NULL) {
		l2tpd_log(_l2tpd, LOG_ERR,
		    "calloc() failed in %s(): %m", __func__);
		goto reigai;
	}
	for (i = 0; i < _this->winsz; i++) {
		if ((bytebuf = bytebuffer_create(L2TPD_SND_BUFSIZ)) == NULL) {
			l2tpd_log(_l2tpd, LOG_ERR,
			    "bytebuffer_create() failed in %s(): %m", __func__);
			goto reigai;
		}
		_this->snd_buffers[i] = bytebuf;
	}
	if ((_this->zlb_buffer = bytebuffer_create(sizeof(struct l2tp_header)
	    + 128)) == NULL) {
		l2tpd_log(_l2tpd, LOG_ERR,
		    "bytebuffer_create() failed in %s(): %m", __func__);
		goto reigai;
	}
#ifdef USE_LIBSOCKUTIL
	if (nat_t_ctx != NULL) {
		if ((_this->sa_cookie = malloc(
		    sizeof(struct in_ipsec_sa_cookie))) != NULL) {
			*(struct in_ipsec_sa_cookie *)_this->sa_cookie =
			    *(struct in_ipsec_sa_cookie *)nat_t_ctx;
		} else {
			l2tpd_log(_l2tpd, LOG_ERR,
			    "creating sa_cookie failed: %m");
			goto reigai;
		}
	}
#endif
	_this->hello_interval = L2TP_CTRL_DEFAULT_HELLO_INTERVAL;
	_this->hello_timeout = L2TP_CTRL_DEFAULT_HELLO_TIMEOUT;
	_this->hello_io_time = curr_time;

	/* タイマーのセット */
	l2tp_ctrl_reset_timeout(_this);

	/* 登録 */
	l2tpd_add_ctrl(_l2tpd, _this);
	return 0;
reigai:
	l2tp_ctrl_stop(_this, 0);
	return -1;
}

/**
 * {@link ::_l2tp_ctrl L2TP LNS コントロールコネクション} のインスタンスの
 * 設定を行います。
 */
static void
l2tp_ctrl_reload(l2tp_ctrl *_this)
{
	int ival;

	_this->data_use_seq = l2tp_ctrl_config_str_equal(_this,
	    "l2tp.data_use_seq", "true", 1);

	if ((ival = l2tp_ctrl_config_int(_this, "l2tp.hello_interval", 0))!= 0)
		_this->hello_interval = ival;
	if ((ival = l2tp_ctrl_config_int(_this, "l2tp.hello_timeout", 0)) != 0)
		_this->hello_timeout = ival;

	return;
}

/**
 * {@link ::_l2tp_ctrl L2TP LNS コントロールコネクション}のインスタンスを解放
 * します。
 */
void
l2tp_ctrl_destroy(l2tp_ctrl *_this)
{
	L2TP_CTRL_ASSERT(_this != NULL);
#ifdef USE_LIBSOCKUTIL
	if (_this->sa_cookie != NULL)
		free(_this->sa_cookie);
#endif
	free(_this);
}

/**
 * 切断を先方に通知します。
 *
 * @return	CDN、StopCCN を送信済みの場合には 0。CDN を送信できていない
 *		コールが存在する場合にはその数、StopCCN の送信に失敗した場合
 *		-1 が返ります。
 */
static int
l2tp_ctrl_send_disconnect_notify(l2tp_ctrl *_this)
{
	int ncalls;

	L2TP_CTRL_ASSERT(_this != NULL)
	L2TP_CTRL_ASSERT(_this->state == L2TP_CTRL_STATE_ESTABLISHED ||
	    _this->state == L2TP_CTRL_STATE_CLEANUP_WAIT);

	// アクティブじゃなかったり、StopCCN を送信済み。
	if (_this->active_closing == 0)
		return 0;

	// すべての Call に CDN 
	ncalls = 0;
	if (slist_length(&_this->call_list) != 0) {
		ncalls = l2tp_ctrl_disconnect_all_calls(_this);
		if (ncalls > 0) {
			/*
			 * 送信 Window が埋まっているかどうかを検査するために
			 * 再度呼び出す。ゼロになれば、すべての call に CDN を
			 * 送信し終わった。
			 */
			ncalls = l2tp_ctrl_disconnect_all_calls(_this);
		}
	}
	if (ncalls > 0)
		return ncalls;

	if (l2tp_ctrl_send_StopCCN(_this, _this->active_closing) != 0)
		return -1;
	_this->active_closing = 0;

	return 0;
}

/**
 * コントロールコネクションを終了します。
 *
 * <p>
 * アクティブクローズの (StopCCN を送信する) 場合には、StopCCN の
 * ResultCode AVP ように result に 1 以上の適切な値を指定してください。</p>
 * <p>
 * 戻り値が 0 の場合、_this は解放されていますので、l2tp_ctrl の処理を
 * 続行することはできません。また、1 の場合(解放されていない場合) は、
 * タイマはリセットされています。</p>
 *
 * @return	完全に終了した場合には 0 を、まだ完全に終了していない場合
 *		には、0 以外を返します。
 */
int
l2tp_ctrl_stop(l2tp_ctrl *_this, int result)
{
	int i;
	l2tpd *_l2tpd;

	L2TP_CTRL_ASSERT(_this != NULL);

	switch (_this->state) {
	case L2TP_CTRL_STATE_ESTABLISHED:
		_this->state = L2TP_CTRL_STATE_CLEANUP_WAIT;
		if (result > 0) {
			_this->active_closing = result;
			l2tp_ctrl_send_disconnect_notify(_this);
			break;
		}
		goto cleanup;
	default:
		l2tp_ctrl_log(_this, LOG_DEBUG, "%s() unexpected state=%s",
		    __func__, l2tp_ctrl_state_string(_this));
		// FALL THROUGH;
	case L2TP_CTRL_STATE_WAIT_CTL_CONN:
		// FALL THROUGH;
	case L2TP_CTRL_STATE_CLEANUP_WAIT:
cleanup:
		if (slist_length(&_this->call_list) != 0) {
			if (l2tp_ctrl_disconnect_all_calls(_this) > 0) 
				break;
		}
#if 0
		if (_this->l2tpd->purge_ipsec_sa != 0)
			l2tp_ctrl_purge_ipsec_sa(_this);
#endif

		l2tp_ctrl_log(_this, LOG_NOTICE, "logtype=Finished");

		evtimer_del(&_this->ev_timeout);

		/* 送信バッファの解放 */
		if (_this->snd_buffers != NULL) {
			for (i = 0; i < _this->winsz; i++)
				bytebuffer_destroy(_this->snd_buffers[i]);
			free(_this->snd_buffers);
			_this->snd_buffers = NULL;
		}
		if (_this->zlb_buffer != NULL) {
			bytebuffer_destroy(_this->zlb_buffer);
			_this->zlb_buffer = NULL;
		}
		/* l2tp_call の解放 */
		l2tp_ctrl_destroy_all_calls(_this);
		slist_fini(&_this->call_list);

		l2tpd_remove_ctrl(_this->l2tpd, _this->tunnel_id);

		_l2tpd = _this->l2tpd;
		l2tp_ctrl_destroy(_this);

		l2tpd_ctrl_finished_notify(_l2tpd);
		return 0;	// stopped
	}
	l2tp_ctrl_reset_timeout(_this);

	return 1;
}

#if 0
/** Delete the IPsec SA for disconnection */
static void
l2tp_ctrl_purge_ipsec_sa(l2tp_ctrl *_this)
{
	int is_natt, proto;
	struct sockaddr_in peer, sock;
	hash_link *hl;
#ifdef USE_LIBSOCKUTIL
	struct in_ipsec_sa_cookie *ipsec_sa_cookie;
#endif
	l2tp_ctrl *anot;

	L2TP_CTRL_ASSERT(_this->peer.ss_family == AF_INET);
	L2TP_CTRL_ASSERT(_this->sock.ss_family == AF_INET);

	/*
	 * Search another tunnel that uses the same IPsec SA
	 * by lineer.
	 */
	for (hl = hash_first(_this->l2tpd->ctrl_map);
	    hl != NULL; hl = hash_next(_this->l2tpd->ctrl_map)) {
		anot = hl->item;
		if (anot == _this)
			continue;
		switch (_this->peer.ss_family) {
		case AF_INET:
			if (SIN(&_this->peer)->sin_addr.s_addr ==
			    SIN(&anot->peer)->sin_addr.s_addr)
				break;
			continue;

		default:
			L2TP_CTRL_ASSERT(0);
			/* Not implemented yet */
		}
#ifdef USE_LIBSOCKUTIL
		if (_this->sa_cookie != NULL &&
		    anot->sa_cookie != NULL) {
			/* Both tunnels belong the same NAT box.  */

			if (memcmp(_this->sa_cookie, anot->sa_cookie,
			    sizeof(struct in_ipsec_sa_cookie)) != 0)
				/* Different hosts behind the NAT box.  */
				continue;

			/* The SA is shared by another tunnels by one host. */
			return;	/* don't purge the sa */

		} else if (_this->sa_cookie != NULL ||
		    anot->sa_cookie != NULL) {
			/* Only one is behind the NAT */
			continue;
		}
#endif
		return;	/* don't purge the sa */
	}

#ifdef USE_LIBSOCKUTIL
	is_natt = (_this->sa_cookie != NULL)? 1 : 0;
#else
	is_natt = 0;
#endif
	memcpy(&peer, &_this->peer, sizeof(peer));
	memcpy(&sock, &_this->sock, sizeof(sock));
	if (!is_natt) {
		proto = 0;
		peer.sin_port = sock.sin_port = 0;
	}
#ifdef USE_LIBSOCKUTIL
	else {
		ipsec_sa_cookie = _this->sa_cookie;
		peer.sin_port = ipsec_sa_cookie->remote_port;
		sock.sin_port = ipsec_sa_cookie->local_port;
#if 1
		/*
		 * XXX: As RFC 2367, protocol sould be specified if the port
		 * XXX: number is non-zero.
		 */
		proto = 0;
#else
		proto = IPPROTO_UDP;
#endif
	}
#endif
	if (ipsec_util_purge_transport_sa((struct sockaddr *)&peer,
	    (struct sockaddr *)&sock, proto, IPSEC_UTIL_DIRECTION_BOTH) != 0) {
		l2tp_ctrl_log(_this, LOG_NOTICE, "failed to purge IPSec SA");
	}
}
#endif

/** タイマー関連処理 */
static void
l2tp_ctrl_timeout(int fd, short evtype, void *ctx)
{
	int next_timeout, need_resend;
	time_t curr_time;
	l2tp_ctrl *_this;
	l2tp_call *call;

	/*
	 * この関数から抜ける場合は、タイマをリセットしなければならない。
	 * l2tp_ctrl_stop は、l2tp_ctrl_stop 内でタイマをリセットする。
	 * l2tp_ctrl_stop は、_this を解放する可能性がある点にも注意。
	 */
	_this = ctx;
	L2TP_CTRL_ASSERT(_this != NULL);

	curr_time = get_monosec();

	next_timeout = 2;
	need_resend = 0;

	if (l2tp_ctrl_txwin_size(_this) > 0)  {
		if (_this->state == L2TP_CTRL_STATE_ESTABLISHED) {
			if (_this->hello_wait_ack != 0) {
				/* Hello 応答待ち */
				if (curr_time - _this->hello_io_time >=
				    _this->hello_timeout) {
					l2tp_ctrl_log(_this, LOG_NOTICE,
					    "timeout waiting ack for hello "
					    "packets.");
					l2tp_ctrl_stop(_this,
					    L2TP_STOP_CCN_RCODE_GENERAL);
					return;
				}
			}
		} else if (curr_time - _this->last_snd_ctrl >=
		    L2TP_CTRL_CTRL_PKT_TIMEOUT) {
			l2tp_ctrl_log(_this, LOG_NOTICE,
			    "timeout waiting ack for ctrl packets.");
			l2tp_ctrl_stop(_this,
			    L2TP_STOP_CCN_RCODE_GENERAL);
			return;
		}
		need_resend = 1;
	} else {
		for (slist_itr_first(&_this->call_list);
		    slist_itr_has_next(&_this->call_list);) {
			call = slist_itr_next(&_this->call_list);
			if (call->state == L2TP_CALL_STATE_CLEANUP_WAIT) {
				l2tp_call_destroy(call, 1);
				slist_itr_remove(&_this->call_list);
			}
		}
	}

	switch (_this->state) {
	case L2TP_CTRL_STATE_IDLE:
		/*
		 * idle の場合
		 *	この実装ではあり得ない。
		 */
		l2tp_ctrl_log(_this, LOG_ERR,
		    "Internal error, timeout on illegal state=idle");
		l2tp_ctrl_stop(_this, L2TP_STOP_CCN_RCODE_GENERAL);
		break;
	case L2TP_CTRL_STATE_WAIT_CTL_CONN:
		/*
		 * wait-ctrl-conn の場合
		 *	SCCRP に対する確認応答がない場合は、先方は SCCRQ
		 *	を再送するが、この実装側で「再送」であることを検知
		 *	できない。こちらからは再送しない。
		 */
		need_resend = 0;
		break;
	case L2TP_CTRL_STATE_ESTABLISHED:
		if (slist_length(&_this->call_list) == 0 &&
		    curr_time - _this->last_snd_ctrl >=
			    L2TP_CTRL_WAIT_CALL_TIMEOUT) {
			if (_this->ncalls == 0)
				/* 最初の call がこない。 */
				l2tp_ctrl_log(_this, LOG_WARNING,
				    "timeout waiting call");
			l2tp_ctrl_stop(_this,
			    L2TP_STOP_CCN_RCODE_GENERAL);
			return;
		}
		if (_this->hello_wait_ack == 0 && _this->hello_interval > 0) {
			/*
			 * Hello 送信
			 */
			if (curr_time - _this->hello_interval >=
			    _this->hello_io_time) {
				if (l2tp_ctrl_send_HELLO(_this) == 0)
					/* 成功した場合 */
					_this->hello_wait_ack = 1;
				_this->hello_io_time = curr_time;
				need_resend = 0;
			}
		}
		break;
	case L2TP_CTRL_STATE_CLEANUP_WAIT:
		if (curr_time - _this->last_snd_ctrl >=
		    L2TP_CTRL_CLEANUP_WAIT_TIME) {
			l2tp_ctrl_log(_this, LOG_NOTICE,
			    "Cleanup timeout state=%d", _this->state);
			l2tp_ctrl_stop(_this, 0);
			return;
		}
		if (_this->active_closing != 0)
			l2tp_ctrl_send_disconnect_notify(_this);
		break;
	default:	
		l2tp_ctrl_log(_this, LOG_ERR,
		    "Internal error, timeout on illegal state=%d",
			_this->state);
		l2tp_ctrl_stop(_this, L2TP_STOP_CCN_RCODE_GENERAL);
		return;
	}
	/* 再送の必要があれば、再送 */
	if (need_resend)
		l2tp_ctrl_resend_una_packets(_this);
	l2tp_ctrl_reset_timeout(_this);
}

int
l2tp_ctrl_send(l2tp_ctrl *_this, const void *msg, int len)
{
	int rval;

#ifdef USE_LIBSOCKUTIL
	if (_this->sa_cookie != NULL)
		rval = sendfromto_nat_t(LISTENER_SOCK(_this), msg, len, 0,
		    (struct sockaddr *)&_this->sock,
		    (struct sockaddr *)&_this->peer, _this->sa_cookie);
	else
		rval = sendfromto(LISTENER_SOCK(_this), msg, len, 0,
		    (struct sockaddr *)&_this->sock,
		    (struct sockaddr *)&_this->peer);
#else
	rval = sendto(LISTENER_SOCK(_this), msg, len, 0,
	    (struct sockaddr *)&_this->peer, _this->peer.ss_len);
#endif
	return rval;
}

/** 確認応答待ちのパケットを再送する。 */
static int
l2tp_ctrl_resend_una_packets(l2tp_ctrl *_this)
{
	uint16_t seq;
	bytebuffer *bytebuf;
	struct l2tp_header *header;
	int nsend;

	nsend = 0;
	for (seq = _this->snd_una; SEQ_LT(seq, _this->snd_nxt); seq++) {
		bytebuf = _this->snd_buffers[seq % _this->winsz];
		header = bytebuffer_pointer(bytebuf);
		header->nr = htons(_this->rcv_nxt);
#ifdef L2TP_CTRL_DEBUG
		if (debuglevel >= 3) {
			l2tp_ctrl_log(_this, DEBUG_LEVEL_3, "RESEND seq=%u",
			    ntohs(header->ns));
			show_hd(debug_get_debugfp(),
			    bytebuffer_pointer(bytebuf),
			    bytebuffer_remaining(bytebuf));
		}
#endif
		if (l2tp_ctrl_send(_this, bytebuffer_pointer(bytebuf),
		    bytebuffer_remaining(bytebuf)) < 0) {
			l2tp_ctrl_log(_this, LOG_ERR,
			    "sendto() failed in %s: %m", __func__);
			return -1;
		}
		nsend++;
	}
	return nsend;
}

/**
 * すべてのコールを解放します。
 */
static void
l2tp_ctrl_destroy_all_calls(l2tp_ctrl *_this)
{
	l2tp_call *call;

	L2TP_CTRL_ASSERT(_this != NULL);

	while ((call = slist_remove_first(&_this->call_list)) != NULL)
		l2tp_call_destroy(call, 1);
}

/**
 * このコントロールの全ての call を切断します。
 * @return 解放待ちになっていない call の数を返します。
 */
static int
l2tp_ctrl_disconnect_all_calls(l2tp_ctrl *_this)
{
	int i, len, ncalls;
	l2tp_call *call;

	L2TP_CTRL_ASSERT(_this != NULL);
	
	ncalls = 0;
	len = slist_length(&_this->call_list);
	for (i = 0; i < len; i++) {
		call = slist_get(&_this->call_list, i);
		if (call->state != L2TP_CALL_STATE_CLEANUP_WAIT) {
			ncalls++;

			if (l2tp_ctrl_txwin_is_full(_this)) {
				L2TP_CTRL_DBG((_this, LOG_INFO,
				    "Too many calls.  Sending window is not "
				    "enough to send CDN to all clients."));
				/* nothing to do */
			} else
				l2tp_call_admin_disconnect(call);
		}
	}
	return ncalls;
}

/**
 * タイムアウトを再設定します。
 */
static void
l2tp_ctrl_reset_timeout(l2tp_ctrl *_this)
{
	int intvl;
	struct timeval tv0;

	L2TP_CTRL_ASSERT(_this != NULL);

	if (evtimer_initialized(&_this->ev_timeout))
		evtimer_del(&_this->ev_timeout);

	switch (_this->state) {
	case L2TP_CTRL_STATE_CLEANUP_WAIT:
		intvl = 1;
		break;
	default:
		intvl = 2;
		break;
	}
	tv0.tv_usec = 0;
	tv0.tv_sec = intvl;
	if (!evtimer_initialized(&_this->ev_timeout))
		evtimer_set(&_this->ev_timeout, l2tp_ctrl_timeout, _this);
	evtimer_add(&_this->ev_timeout, &tv0);
}

/***********************************************************************
 * プロトコル - 送受信
 ***********************************************************************/
/**
 * パケット受信
 */
void
l2tp_ctrl_input(l2tpd *_this, int listener_index, struct sockaddr *peer,
    struct sockaddr *sock, void *nat_t_ctx, u_char *pkt, int pktlen)
{
	int i, len, offsiz, reqlen, is_ctrl;
	uint16_t mestype;
	struct sockaddr_in *peersin, *socksin;
	struct l2tp_avp *avp, *avp0;
	l2tp_ctrl *ctrl;
	l2tp_call *call;
	char buf[L2TP_AVP_MAXSIZ], errmsg[256];
	time_t curr_time;
	u_char *pkt0;
	char ifname[IF_NAMESIZE], phy_label[256];
	struct l2tp_header hdr;

	ctrl = NULL;
	curr_time = get_monosec();
	pkt0 = pkt;

	L2TP_CTRL_ASSERT(peer->sa_family == AF_INET);
	L2TP_CTRL_ASSERT(sock->sa_family == AF_INET);

	if (peer->sa_family != AF_INET) {
		l2tpd_log(_this, LOG_ERR,
		    "Received a packet peer unknown address "
		    "family=%d", peer->sa_family);
		return;	// ここまでは reigai に飛ばさない
	}
	peersin = (struct sockaddr_in *)peer;
	socksin = (struct sockaddr_in *)sock;

    /*
     * Parse L2TP Header
     */
	memset(&hdr, 0, sizeof(hdr));
	if (pktlen < 2) {
		snprintf(errmsg, sizeof(errmsg), "a short packet.  "
		    "length=%d", pktlen);
		goto bad_packet;
	}
	memcpy(&hdr, pkt, 2);
	pkt += 2;
	if (hdr.ver != L2TP_HEADER_VERSION_RFC2661) {
		/* 現在 RFC2661 のみサポートします */
		snprintf(errmsg, sizeof(errmsg),
		    "Unsupported version at header = %d", hdr.ver);
		goto bad_packet;
	}
	is_ctrl = (hdr.t != 0)? 1 : 0;
	
	/* calc required length */
	reqlen = 6;		/* for Flags, Tunnel-Id, Session-Id field */
	if (hdr.l) reqlen += 2;	/* for Length field (opt) */
	if (hdr.s) reqlen += 4;	/* for Ns, Nr field (opt) */
	if (hdr.o) reqlen += 2;	/* for Offset Size field (opt) */
	if (reqlen > pktlen) {
		snprintf(errmsg, sizeof(errmsg),
		    "a short packet. length=%d", pktlen);
		goto bad_packet;
	}

	if (hdr.l != 0) {
		GETSHORT(hdr.length, pkt);
		if (hdr.length > pktlen) {
			snprintf(errmsg, sizeof(errmsg),
			    "Actual packet size is smaller than the length "
			    "field %d < %d", pktlen, hdr.length);
			goto bad_packet;
		}
		pktlen = hdr.length;	/* remove trailing trash */
	}
	GETSHORT(hdr.tunnel_id, pkt);
	GETSHORT(hdr.session_id, pkt);
	if (hdr.s != 0) {
		GETSHORT(hdr.ns, pkt);
		GETSHORT(hdr.nr, pkt);
	}
	if (hdr.o != 0) {
		GETSHORT(offsiz, pkt);
		if (pktlen < offsiz) {
			snprintf(errmsg, sizeof(errmsg),
			    "offset field is bigger than remaining packet "
			    "length %d > %d", offsiz, pktlen);
			goto bad_packet;
		}
		pkt += offsiz;
	}
	L2TP_CTRL_ASSERT(pkt - pkt0 == reqlen);
	pktlen -= (pkt - pkt0);	/* cut down the length of header */

	ctrl = NULL;
	memset(buf, 0, sizeof(buf));
	mestype = 0;
	avp = NULL;

	if (is_ctrl) {
		avp0 = (struct l2tp_avp *)buf;
		avp = avp_find_message_type_avp(avp0, pkt, pktlen);
		if (avp != NULL)
			mestype = avp->attr_value[0] << 8 | avp->attr_value[1];
	}
	ctrl = l2tpd_get_ctrl(_this, hdr.tunnel_id);

	if (ctrl == NULL) {
		/* 新しいコントロール */
		if (!is_ctrl) {
			snprintf(errmsg, sizeof(errmsg),
			    "bad data message: tunnelId=%d is not "
			    "found.", hdr.tunnel_id);
			goto bad_packet;
		}
		if (mestype != L2TP_AVP_MESSAGE_TYPE_SCCRQ) {
			snprintf(errmsg, sizeof(errmsg),
			    "bad control message: tunnelId=%d is not "
			    "found.  mestype=%s", hdr.tunnel_id,
			    avp_mes_type_string(mestype));
			goto bad_packet;
		}

		strlcpy(phy_label,
		    ((l2tpd_listener *)slist_get(&_this->listener,
		    listener_index))->phy_label, sizeof(phy_label));
		if (_this->phy_label_with_ifname != 0) {
			if (get_ifname_by_sockaddr(sock, ifname) == NULL) {
				l2tpd_log_access_deny(_this,
				    "could not get interface informations",
				    peer);
				goto reigai;
			}
			if (l2tpd_config_str_equal(_this, 
			    config_key_prefix("l2tpd.interface", ifname),
			    "accept", 0)){
				strlcat(phy_label, "%", sizeof(phy_label));
				strlcat(phy_label, ifname, sizeof(phy_label));
			} else if (l2tpd_config_str_equal(_this, 
			    config_key_prefix("l2tpd.interface", "any"),
			    "accept", 0)){
			} else {
				/* このインタフェースは許可されていない。*/
				snprintf(errmsg, sizeof(errmsg),
				    "'%s' is not allowed by config.", ifname);
				l2tpd_log_access_deny(_this, errmsg, peer);
				goto reigai;
			}
#if defined(_SEIL_EXT_) && !defined(USE_LIBSOCKUTIL)
			if (!rtev_ifa_is_primary(ifname, sock)) {
				char hostbuf[NI_MAXHOST];
				getnameinfo(sock, sock->sa_len, hostbuf,
				    sizeof(hostbuf), NULL, 0, NI_NUMERICHOST);
				snprintf(errmsg, sizeof(errmsg),
				    "connecting to %s (an alias address of %s)"
				    " is not allowed by this version.",
				    hostbuf, ifname);
				l2tpd_log_access_deny(_this, errmsg, peer);
				goto reigai;
			}
#endif
		}

		if ((ctrl = l2tp_ctrl_create()) == NULL) {
			l2tp_ctrl_log(ctrl, LOG_ERR,
			    "l2tp_ctrl_create() failed: %m");
			goto reigai;
		}
		if (l2tp_ctrl_init(ctrl, _this, peer, sock, nat_t_ctx) != 0) {
			l2tp_ctrl_log(ctrl, LOG_ERR,
			    "l2tp_ctrl_start() failed: %m");
			goto reigai;
		}

		ctrl->listener_index = listener_index;
		strlcpy(ctrl->phy_label, phy_label, sizeof(ctrl->phy_label));
		l2tp_ctrl_reload(ctrl);
	} else {
		/*
		 * 始点アドレス/ポートが異なる場合には、エラーとする。(DoS
		 * の可能性があるので)
		 */
		L2TP_CTRL_ASSERT(ctrl->peer.ss_family == peer->sa_family);

		switch (peer->sa_family) {
		case AF_INET: 
		    {
			struct sockaddr_in *peersin1;

			peersin1 = (struct sockaddr_in *)&ctrl->peer;
			if (peersin1->sin_addr.s_addr !=
			    peersin->sin_addr.s_addr ||
			    peersin1->sin_port != peersin->sin_port) {
				snprintf(errmsg, sizeof(errmsg),
				    "tunnelId=%u is already assigned for %s:%u",
				    hdr.tunnel_id,
				    inet_ntoa(peersin1->sin_addr),
				    ntohs(peersin1->sin_port));
				goto bad_packet;
			}
		    }
		}
	}
	ctrl->last_rcv = curr_time;
	call = NULL;
	if (hdr.session_id != 0) {
		/* Session Id から l2tp_call を探す。リニアサーチ */
		len = slist_length(&ctrl->call_list);
		for (i = 0; i < len; i++) {
			call = slist_get(&ctrl->call_list, i);
			if (call->session_id == hdr.session_id)
				break;
			call = NULL;
		}
	}
	if (!is_ctrl) {
		/*
		 * L2TP データ
		 */
		if (ctrl->state != L2TP_CTRL_STATE_ESTABLISHED) {
			l2tp_ctrl_log(ctrl, LOG_WARNING,
			    "Received Data packet in '%s'",
			    l2tp_ctrl_state_string(ctrl));
			goto reigai;
		}
		if (call == NULL) {
			l2tp_ctrl_log(ctrl, LOG_WARNING,
			    "Received a data packet but it has no call.  "
			    "session_id=%u",  hdr.session_id);
			goto reigai;
		}
		L2TP_CTRL_DBG((ctrl, DEBUG_LEVEL_2,
		    "call=%u RECV   ns=%u nr=%u snd_nxt=%u rcv_nxt=%u len=%d",
		    call->id, hdr.ns, hdr.nr, call->snd_nxt, call->rcv_nxt,
		    pktlen));
		if (call->state != L2TP_CALL_STATE_ESTABLISHED){
			l2tp_ctrl_log(ctrl, LOG_WARNING,
			    "Received a data packet but call is not "
			    "established");
			goto reigai;
		}
		
		if (hdr.s != 0) {
			if (SEQ_LT(hdr.ns, call->rcv_nxt)) {
				/* シーケンスが戻った。*/
				/* 統計情報に残すべきかもしれない */
				L2TP_CTRL_DBG((ctrl, LOG_DEBUG,
				    "receive a out of sequence data packet: "
				    "%u < %u.  ", hdr.ns, call->rcv_nxt));
				return;
			}
			call->rcv_nxt = hdr.ns + 1;
		}
		l2tp_call_ppp_input(call, pkt, pktlen);

		return;
	}
	if (hdr.s != 0) {
		L2TP_CTRL_DBG((ctrl, DEBUG_LEVEL_2,
		    "RECV %s ns=%u nr=%u snd_nxt=%u snd_una=%u rcv_nxt=%u "
		    "len=%d", (is_ctrl)? "C" : "", hdr.ns, hdr.nr,
		    ctrl->snd_nxt, ctrl->snd_una, ctrl->rcv_nxt, pktlen));

		if (pktlen <= 0)
			l2tp_ctrl_log(ctrl, LOG_INFO, "RecvZLB");

		if (SEQ_GT(hdr.nr, ctrl->snd_una)) {
			if (hdr.nr == ctrl->snd_nxt ||
			    SEQ_LT(hdr.nr, ctrl->snd_nxt))
				ctrl->snd_una = hdr.nr;
			else {
				l2tp_ctrl_log(ctrl, LOG_INFO,
				    "Received message has bad Nr field: "
				    "%u < %u.", hdr.ns, ctrl->snd_nxt);
				/* XXX Drop with ZLB? */
				goto reigai;
			}
		}
		if (l2tp_ctrl_txwin_size(ctrl) <= 0) {
			/* 確認応答待ちなし */
			if (ctrl->hello_wait_ack != 0) {
				/*
				 * Hello に対する Ack が返ったので、Hello
				 * の状態をリセット
				 */
				ctrl->hello_wait_ack = 0;
				ctrl->hello_io_time = curr_time;
			}
			switch (ctrl->state) {
			case L2TP_CTRL_STATE_CLEANUP_WAIT:
				l2tp_ctrl_stop(ctrl, 0);
				return;
			}
		}
		if (hdr.ns != ctrl->rcv_nxt) {
			// 受信してないパケットがある
			if (l2tp_ctrl_resend_una_packets(ctrl) <= 0) {
				// 再送または ZLB 送信
				l2tp_ctrl_send_ZLB(ctrl);
			}
#ifdef	L2TP_CTRL_DEBUG
			if (pktlen != 0) {	// ZLB ではない。
				L2TP_CTRL_DBG((ctrl, LOG_DEBUG,
				    "receive out of sequence %u must be %u.  "
				    "mestype=%s", hdr.ns, ctrl->rcv_nxt,
				    avp_mes_type_string(mestype)));
			}
#endif
			return;
		}
		if (pktlen <= 0)
			return;		/* ZLB */

		if (l2tp_ctrl_txwin_is_full(ctrl)) {
			L2TP_CTRL_DBG((ctrl, LOG_DEBUG,
			    "Received message cannot be handled. "
			    "Transmission window is full."));
			l2tp_ctrl_send_ZLB(ctrl);
			return;
		}

		ctrl->rcv_nxt++;
		if (avp == NULL) {
			l2tpd_log(_this, LOG_WARNING,
			    "bad control message: no message-type AVP.");
			goto reigai;
		}
	}

    /*
     * ステートマシン (RFC2661 pp. 56-57)
     */
	switch (ctrl->state) {
	case L2TP_CTRL_STATE_IDLE:
		switch (mestype) {
		case L2TP_AVP_MESSAGE_TYPE_SCCRQ:
			if (l2tp_ctrl_recv_SCCRQ(ctrl, pkt, pktlen, _this,
			    peer) == 0) {
				// acceptable
				l2tp_ctrl_send_SCCRP(ctrl);
				ctrl->state = L2TP_CTRL_STATE_WAIT_CTL_CONN;
				return;
			}
			/*
			 * un-accectable な場合は、l2tp_ctrl_recv_SCCRQ 側で
			 * 処理済みです。
			 */
			return;
		case L2TP_AVP_MESSAGE_TYPE_SCCRP:
			/*
			 * RFC上は StopCCN を送信するが、この LNS の実装では、
			 * Passive Open だけなので、 このパケットは受け取らな
			 * いはず。
			 */
			// FALL THROUGH
		case L2TP_AVP_MESSAGE_TYPE_SCCCN:
		default:
			break;
		}
		goto fsm_reigai;

	case L2TP_CTRL_STATE_WAIT_CTL_CONN:
	    /* Wait-Ctl-Conn */
		switch (mestype) {
		case L2TP_AVP_MESSAGE_TYPE_SCCCN:
			l2tp_ctrl_log(ctrl, LOG_INFO, "RecvSCCN");
			if (l2tp_ctrl_send_ZLB(ctrl) == 0) {
				ctrl->state = L2TP_CTRL_STATE_ESTABLISHED;
			}
			return;
		case L2TP_AVP_MESSAGE_TYPE_StopCCN:
			goto receive_stop_ccn;
		case L2TP_AVP_MESSAGE_TYPE_SCCRQ:
		case L2TP_AVP_MESSAGE_TYPE_SCCRP:
		default:
			break;
		}
		break;	/* fsm_reigai */
	case L2TP_CTRL_STATE_ESTABLISHED:
	    /* Established */
		switch (mestype) {
		case L2TP_AVP_MESSAGE_TYPE_SCCCN:
		case L2TP_AVP_MESSAGE_TYPE_SCCRQ:
		case L2TP_AVP_MESSAGE_TYPE_SCCRP:
			break;
receive_stop_ccn:
		case L2TP_AVP_MESSAGE_TYPE_StopCCN:
			if (l2tp_ctrl_recv_StopCCN(ctrl, pkt, pktlen) == 0) {
				if (l2tp_ctrl_resend_una_packets(ctrl) <= 0)
					l2tp_ctrl_send_ZLB(ctrl);
				l2tp_ctrl_stop(ctrl, 0);
				return;
			}
			l2tp_ctrl_log(ctrl, LOG_ERR, "Received bad StopCCN");
			l2tp_ctrl_send_ZLB(ctrl);
			l2tp_ctrl_stop(ctrl, 0);
			return;

		case L2TP_AVP_MESSAGE_TYPE_HELLO:
			if (l2tp_ctrl_resend_una_packets(ctrl) <= 0)
				l2tp_ctrl_send_ZLB(ctrl);
			return;
		case L2TP_AVP_MESSAGE_TYPE_CDN:
		case L2TP_AVP_MESSAGE_TYPE_ICRP:
		case L2TP_AVP_MESSAGE_TYPE_ICCN:
			if (call == NULL) {
				l2tp_ctrl_log(ctrl, LOG_INFO,
				    "Unknown call message: %s",
				    avp_mes_type_string(mestype));
				goto reigai;
			}
			// FALL THROUGH
		case L2TP_AVP_MESSAGE_TYPE_ICRQ:
			l2tp_call_recv_packet(ctrl, call, mestype, pkt,
			    pktlen);
			return;
		default:
			break;
		}
		break;	/* fsm_reigai */
	case L2TP_CTRL_STATE_CLEANUP_WAIT:
		if (mestype == L2TP_AVP_MESSAGE_TYPE_StopCCN) {
			/*
			 * StopCCN が交錯したか、Window が埋まっていて
			 * StopCCN が送信できない間に、StopCCN を受信
			 */
			goto receive_stop_ccn;
		}
		break;	/* fsm_reigai */
	}

fsm_reigai:
	/* ステートマシンのエラー */
	l2tp_ctrl_log(ctrl, LOG_WARNING, "Received %s in '%s' state",
	    avp_mes_type_string(mestype), l2tp_ctrl_state_string(ctrl));
	l2tp_ctrl_stop(ctrl, L2TP_STOP_CCN_RCODE_FSM_ERROR);

	return;
reigai:
	if (ctrl != NULL && mestype != 0) {
		l2tp_ctrl_log(ctrl, LOG_WARNING, "Received %s in '%s' state",
		    avp_mes_type_string(mestype), l2tp_ctrl_state_string(ctrl));
		l2tp_ctrl_stop(ctrl, L2TP_STOP_CCN_RCODE_GENERAL_ERROR);
	}
	return;

bad_packet:
	l2tpd_log(_this, LOG_INFO, "Received from=%s:%u: %s", 
	    inet_ntoa(peersin->sin_addr), ntohs(peersin->sin_port), errmsg);
	return;
}

static inline int
l2tp_ctrl_txwin_size(l2tp_ctrl *_this)
{
	uint16_t sz;

	sz = _this->snd_nxt - _this->snd_una;

	L2TP_CTRL_ASSERT(sz <= _this->winsz);

	return sz;
}

static inline int
l2tp_ctrl_txwin_is_full(l2tp_ctrl *_this)
{
	return (l2tp_ctrl_txwin_size(_this) >= _this->winsz)? 1 : 0;
}

/** パケットの送信 */
int
l2tp_ctrl_send_packet(l2tp_ctrl *_this, int call_id, bytebuffer *bytebuf,
    int is_ctrl)
{
	struct l2tp_header *hdr;
	int rval, use_seq;
	time_t curr_time;

	curr_time = get_monosec();

#ifdef L2TP_DATA_WITH_SEQUENCE
	use_seq = 1;
#else
	use_seq = is_ctrl;
#endif

	bytebuffer_flip(bytebuf);
	hdr = (struct l2tp_header *)bytebuffer_pointer(bytebuf);
	memset(hdr, 0, sizeof(*hdr));

	hdr->t = 1;
	hdr->ver = L2TP_HEADER_VERSION_RFC2661;
	hdr->l = 1;
	hdr->length = htons(bytebuffer_remaining(bytebuf));
	hdr->tunnel_id = htons(_this->peer_tunnel_id);
	hdr->session_id = htons(call_id);

	hdr->s = 1;
	hdr->ns = htons(_this->snd_nxt);
	hdr->nr = htons(_this->rcv_nxt);

	if (is_ctrl &&
	    bytebuffer_remaining(bytebuf) > sizeof(struct l2tp_header))
		/* Not ZLB */
		_this->snd_nxt++;

	L2TP_CTRL_DBG((_this, DEBUG_LEVEL_2,
	    "SEND %s ns=%u nr=%u snd_nxt=%u snd_una=%u rcv_nxt=%u ",
	    (is_ctrl)? "C" : " ", ntohs(hdr->ns), htons(hdr->nr),
	    _this->snd_nxt, _this->snd_una, _this->rcv_nxt));

	if (_this->l2tpd->ctrl_out_pktdump != 0) {
		l2tpd_log(_this->l2tpd, LOG_DEBUG,
		    "L2TP Control output packet dump");
		show_hd(debug_get_debugfp(), bytebuffer_pointer(bytebuf),
		    bytebuffer_remaining(bytebuf));
	}

	if ((rval = l2tp_ctrl_send(_this, bytebuffer_pointer(bytebuf),
	    bytebuffer_remaining(bytebuf))) < 0) {
		L2TP_CTRL_DBG((_this, LOG_DEBUG, "sendto() failed: %m"));
	}

	_this->last_snd_ctrl = curr_time;

	return (rval == bytebuffer_remaining(bytebuf))? 0 : 1;
}

/**
 * SCCRQ の受信
 */
static int
l2tp_ctrl_recv_SCCRQ(l2tp_ctrl *_this, u_char *pkt, int pktlen, l2tpd *_l2tpd,
    struct sockaddr *peer)
{
	int avpsz, len, protover, protorev, firmrev, result;
	struct l2tp_avp *avp;
	char host[NI_MAXHOST], serv[NI_MAXSERV];
	char buf[L2TP_AVP_MAXSIZ], emes[256], hostname[256], vendorname[256];

	result = L2TP_STOP_CCN_RCODE_GENERAL_ERROR;
	strlcpy(hostname, "(no hostname)", sizeof(hostname));
	strlcpy(vendorname, "(no vendorname)", sizeof(vendorname));

	firmrev = 0;
	protover = 0;
	protorev = 0;
	avp = (struct l2tp_avp *)buf;
	while (pktlen >= 6 && (avpsz = avp_enum(avp, pkt, pktlen, 1)) > 0) {
		pkt += avpsz;
		pktlen -= avpsz;
		if (avp->vendor_id != 0) {
			L2TP_CTRL_DBG((_this, LOG_DEBUG,
			    "Received a Vendor-specific AVP vendor-id=%d "
			    "type=%d", avp->vendor_id, avp->attr_type));
			continue;
		}
		switch (avp->attr_type) {
		case L2TP_AVP_TYPE_MESSAGE_TYPE:
			AVP_SIZE_CHECK(avp, ==, 8);
			continue;
		case L2TP_AVP_TYPE_PROTOCOL_VERSION:
			AVP_SIZE_CHECK(avp, ==, 8);
			protover = avp->attr_value[0];
			protorev = avp->attr_value[1];

			if (protover != L2TP_RFC2661_VERSION ||
			    protorev != L2TP_RFC2661_REVISION) {
				result = L2TP_STOP_CCN_RCODE_GENERAL_ERROR;
				snprintf(emes, sizeof(emes),
				    "Peer's protocol version is not supported:"
				    " %d.%d", protover, protorev);
				goto not_acceptable;
			}
			continue;
		case L2TP_AVP_TYPE_FRAMING_CAPABILITIES:
			AVP_SIZE_CHECK(avp, ==, 10);
			if ((avp_get_val32(avp) & L2TP_FRAMING_CAP_FLAGS_SYNC)
			    == 0) {
				L2TP_CTRL_DBG((_this, LOG_DEBUG, "Peer doesn't "
				    "support synchronous framing"));
			}
			continue;
		case L2TP_AVP_TYPE_BEARER_CAPABILITIES:
			AVP_SIZE_CHECK(avp, ==, 10);
			continue;
		case L2TP_AVP_TYPE_TIE_BREAKER:
			AVP_SIZE_CHECK(avp, ==, 14);
			/*
			 * この実装からは SCCRQ は送らないので常に peer が
			 * winner。
			 */
			continue;
		case L2TP_AVP_TYPE_FIRMWARE_REVISION:
			AVP_SIZE_CHECK(avp, >=, 6);
			firmrev = avp_get_val16(avp);
			continue;
		case L2TP_AVP_TYPE_HOST_NAME:
			AVP_SIZE_CHECK(avp, >, 4);
			len = MIN(sizeof(hostname) - 1, avp->length - 6);
			memcpy(hostname, avp->attr_value, len);
			hostname[len] = '\0';
			continue;
		case L2TP_AVP_TYPE_VENDOR_NAME:
			AVP_SIZE_CHECK(avp, >, 4);
			len = MIN(sizeof(vendorname) - 1, avp->length - 6);
			memcpy(vendorname, avp->attr_value, len);
			vendorname[len] = '\0';
			continue;
		case L2TP_AVP_TYPE_ASSINGED_TUNNEL_ID:
			AVP_SIZE_CHECK(avp, ==, 8);
			_this->peer_tunnel_id = avp_get_val16(avp);
			continue;
		case L2TP_AVP_TYPE_RECV_WINDOW_SIZE:
			AVP_SIZE_CHECK(avp, ==, 8);
			_this->peer_winsz = avp_get_val16(avp);
			continue;
		}
		if (avp->is_mandatory) {
			l2tp_ctrl_log(_this, LOG_WARNING,
			    "Received AVP (%s/%d) is not supported, but it's "
			    "mandatory", avp_attr_type_string(avp->attr_type),
			    avp->attr_type);
#ifdef L2TP_CTRL_DEBUG
		} else {
			L2TP_CTRL_DBG((_this, LOG_DEBUG, 
			    "AVP (%s/%d) is not handled",
			    avp_attr_type_string(avp->attr_type),
			    avp->attr_type));
#endif
		}
	}
	if (getnameinfo((struct sockaddr *)&_this->peer, _this->peer.ss_len,
	    host, sizeof(host), serv, sizeof(serv),
	    NI_NUMERICHOST | NI_NUMERICSERV | NI_DGRAM) != 0) {
		l2tp_ctrl_log(_this, LOG_ERR,
		    "getnameinfo() failed at %s(): %m", __func__);
		strlcpy(host, "error", sizeof(host));
		strlcpy(serv, "error", sizeof(serv));
	}
	l2tp_ctrl_log(_this, LOG_NOTICE, "logtype=Started RecvSCCRQ "
	    "from=%s:%s/udp tunnel_id=%u/%u protocol=%d.%d winsize=%d "
	    "hostname=%s vendor=%s firm=%04X", host, serv, _this->tunnel_id,
	    _this->peer_tunnel_id, protover, protorev, _this->peer_winsz,
	    hostname, vendorname, firmrev);

	return 0;
not_acceptable:
size_check_failed:
	l2tp_ctrl_log(_this, LOG_ERR, "Received bad SCCRQ: %s", emes);
	l2tp_ctrl_stop(_this, result);

	return 1;
}

/**
 * StopCCN を送信します。
 */
static int
l2tp_ctrl_send_StopCCN(l2tp_ctrl *_this, int result)
{
	struct l2tp_avp *avp;
	char buf[L2TP_AVP_MAXSIZ];
	bytebuffer *bytebuf;

	if ((bytebuf = l2tp_ctrl_prepare_snd_buffer(_this, 1)) == NULL) {
		l2tp_ctrl_log(_this, LOG_ERR,
		    "sending StopCCN failed: no buffer.");
		return -1;
	}
	avp = (struct l2tp_avp *)buf;

	/* Message Type = StopCCN */
	memset(avp, 0, sizeof(*avp));
	avp->is_mandatory = 1;
	avp->attr_type = L2TP_AVP_TYPE_MESSAGE_TYPE;
	avp_set_val16(avp, L2TP_AVP_MESSAGE_TYPE_StopCCN);
	bytebuf_add_avp(bytebuf, avp, 2);

	/* Assigned Tunnel Id */
	memset(avp, 0, sizeof(*avp));
	avp->is_mandatory = 1;
	avp->attr_type = L2TP_AVP_TYPE_ASSINGED_TUNNEL_ID;
	avp_set_val16(avp, _this->tunnel_id);
	bytebuf_add_avp(bytebuf, avp, 2);

	/* Result Code */
	memset(avp, 0, sizeof(*avp));
	avp->is_mandatory = 1;
	avp->attr_type = L2TP_AVP_TYPE_RESULT_CODE;
	avp_set_val16(avp, result);
	bytebuf_add_avp(bytebuf, avp, 2);

	if (l2tp_ctrl_send_packet(_this, 0, bytebuf, 1) != 0) {
		l2tp_ctrl_log(_this, LOG_ERR, "sending CCN failed");
		return - 1;
	}
	l2tp_ctrl_log(_this, LOG_INFO, "SendStopCCN result=%d", result);

	return 0;
}

/**
 * StopCCN の受信
 */
static int
l2tp_ctrl_recv_StopCCN(l2tp_ctrl *_this, u_char *pkt, int pktlen)
{
	int avpsz;
	uint32_t val32;
	uint16_t rcode, tunid, ecode;
	struct l2tp_avp *avp;
	char buf[L2TP_AVP_MAXSIZ + 16], emes[256], peermes[256];

	rcode = 0;
	ecode = 0;
	tunid = 0;
	peermes[0] = '\0';
	avp = (struct l2tp_avp *)buf;
	while (pktlen >= 6 && (avpsz = avp_enum(avp, pkt, pktlen, 1)) > 0) {
		pkt += avpsz;
		pktlen -= avpsz;
		if (avp->vendor_id != 0) {
			L2TP_CTRL_DBG((_this, LOG_DEBUG,
			    "Received a Vendor-specific AVP vendor-id=%d "
			    "type=%d", avp->vendor_id, avp->attr_type));
			continue;
		}
		if (avp->is_hidden != 0) {
			l2tp_ctrl_log(_this, LOG_WARNING,
			    "Received AVP (%s/%d) is hidden.  But we don't "
			    "share secret.",
			    avp_attr_type_string(avp->attr_type),
			    avp->attr_type);
			if (avp->is_mandatory != 0) {
				l2tp_ctrl_stop(_this,
				    L2TP_STOP_CCN_RCODE_GENERAL_ERROR |
				    L2TP_ECODE_UNKNOWN_MANDATORY_AVP);
				return 1;
			}
			continue;
		}
		switch (avp->attr_type) {
		case L2TP_AVP_TYPE_MESSAGE_TYPE:
			AVP_SIZE_CHECK(avp, ==, 8);
			continue;
		case L2TP_AVP_TYPE_RESULT_CODE:
			AVP_SIZE_CHECK(avp, >=, 10);
			val32 = avp_get_val32(avp);
			rcode = val32 >> 16;
			ecode = val32 & 0xffff;
			if (avp->length > 10) {
				avp->attr_value[avp->length - 6] = '\0';
				strlcpy(peermes,
				    (const char *)avp->attr_value + 4,
				    sizeof(peermes));
			}
			continue;
		case L2TP_AVP_TYPE_ASSINGED_TUNNEL_ID:
			AVP_SIZE_CHECK(avp, ==, 8);
			tunid = avp_get_val16(avp);
			continue;
		default:
			if (avp->is_mandatory != 0) {
				l2tp_ctrl_log(_this, LOG_WARNING,
				    "Received AVP (%s/%d) is not supported, "
				    "but it's mandatory",
				    avp_attr_type_string(avp->attr_type),
				    avp->attr_type);
#ifdef L2TP_CTRL_DEBUG
			} else {
				L2TP_CTRL_DBG((_this, LOG_DEBUG, 
				    "AVP (%s/%d) is not handled",
				    avp_attr_type_string(avp->attr_type),
				    avp->attr_type));
#endif
			}
		}
	}

	if (rcode == L2TP_CDN_RCODE_ERROR_CODE &&
	    ecode == L2TP_ECODE_NO_RESOURCE) {
		/*
		 * 現在観測された状況
		 *
		 *  (1) IDGWとWindows が同一 LAN セグメント上にあり、
		 *	Windows の(そのLAN  IP アドレスが、ナチュラルマスク
		 *	で評価した場合のブロードキャストアドレスだった場合
		 *	(192.168.0.255/23など)
		 *  (2) Windows 2000 を起動しっぱなしで、L2TPの接続切断を繰り
		 *	返すと、あるタイミングからこの状況に陥り、接続できない。
		 *	Windows が再起動するまで、問題は継続。
		 */
		l2tp_ctrl_log(_this, LOG_WARNING,
		    "Peer indicates \"No Resource\" error.");
	}

	l2tp_ctrl_log(_this, LOG_INFO, "RecvStopCCN result=%s/%u "
	    "error=%s/%u tunnel_id=%u message=\"%s\"",
	    l2tp_stopccn_rcode_string(rcode), rcode, l2tp_ecode_string(ecode),
	    ecode, tunid, peermes);

	return 0;

size_check_failed:
	l2tp_ctrl_log(_this, LOG_ERR, "Received bad StopCCN: %s", emes);

	return -1;
}

/**
 * SCCRP の送信
 */
static void
l2tp_ctrl_send_SCCRP(l2tp_ctrl *_this)
{
	int len;
	struct l2tp_avp *avp;
	char buf[L2TP_AVP_MAXSIZ];
	const char *val;
	bytebuffer *bytebuf;

	if ((bytebuf = l2tp_ctrl_prepare_snd_buffer(_this, 1)) == NULL) {
		l2tp_ctrl_log(_this, LOG_ERR,
		    "sending SCCRP failed: no buffer.");
		return;
	}
	avp = (struct l2tp_avp *)buf;

	/* Message Type = SCCRP */
	memset(avp, 0, sizeof(*avp));
	avp->is_mandatory = 1;
	avp->attr_type = L2TP_AVP_TYPE_MESSAGE_TYPE;
	avp_set_val16(avp, L2TP_AVP_MESSAGE_TYPE_SCCRP);
	bytebuf_add_avp(bytebuf, avp, 2);

	/* Protocol Version = 1.0 */
	memset(avp, 0, sizeof(*avp));
	avp->is_mandatory = 1;
	avp->attr_type = L2TP_AVP_TYPE_PROTOCOL_VERSION;
	avp->attr_value[0] = L2TP_RFC2661_VERSION;
	avp->attr_value[1] = L2TP_RFC2661_REVISION;
	bytebuf_add_avp(bytebuf, avp, 2);

	/* Framing Capability = Async */
	memset(avp, 0, sizeof(*avp));
	avp->is_mandatory = 1;
	avp->attr_type = L2TP_AVP_TYPE_FRAMING_CAPABILITIES;
	avp_set_val32(avp, L2TP_FRAMING_CAP_FLAGS_SYNC);
	bytebuf_add_avp(bytebuf, avp, 4);

	/* Host Name */
	memset(avp, 0, sizeof(*avp));
	avp->is_mandatory = 1;
	avp->attr_type = L2TP_AVP_TYPE_HOST_NAME;

	if ((val = l2tp_ctrl_config_str(_this, "l2tp.host_name")) == NULL)
		val = _this->l2tpd->default_hostname;
	if (val[0] == '\0')
		val = "G";	/* おまじない。ask yasuoka */
	len = strlen(val);
	memcpy(avp->attr_value, val, len);
	bytebuf_add_avp(bytebuf, avp, len);

	/* Assigned Tunnel Id */
	memset(avp, 0, sizeof(*avp));
	avp->is_mandatory = 1;
	avp->attr_type = L2TP_AVP_TYPE_ASSINGED_TUNNEL_ID;
	avp_set_val16(avp, _this->tunnel_id);
	bytebuf_add_avp(bytebuf, avp, 2);

	/* Bearer Capability
	 *
	 * この実装は LAC になり得ない LNS なので。
	 *
	memset(avp, 0, sizeof(*avp));
	avp->is_mandatory = 1;
	avp->attr_type = L2TP_AVP_TYPE_BEARER_CAPABILITIES;
	avp_set_val32(avp, 0);
	bytebuf_add_avp(bytebuf, avp, 4);
	 */

	/* Firmware Revision */
	memset(avp, 0, sizeof(*avp));
	avp->is_mandatory = 1;
	avp->attr_type = L2TP_AVP_TYPE_FIRMWARE_REVISION;
	avp->attr_value[0] = MAJOR_VERSION;
	avp->attr_value[1] = MINOR_VERSION;
	bytebuf_add_avp(bytebuf, avp, 2);

	/* Host Name */
	memset(avp, 0, sizeof(*avp));
	avp->is_mandatory = 1;
	avp->attr_type = L2TP_AVP_TYPE_VENDOR_NAME;

	if ((val = l2tp_ctrl_config_str(_this, "l2tp.vendor_name")) == NULL)
		val =  L2TPD_VENDOR_NAME;

	len = strlen(val);
	memcpy(avp->attr_value, val, len);
	bytebuf_add_avp(bytebuf, avp, len);

	/* Window Size */
	memset(avp, 0, sizeof(*avp));
	avp->is_mandatory = 1;
	avp->attr_type = L2TP_AVP_TYPE_RECV_WINDOW_SIZE;
	avp_set_val16(avp, _this->winsz);
	bytebuf_add_avp(bytebuf, avp, 2);

	if ((l2tp_ctrl_send_packet(_this, 0, bytebuf, 1)) != 0) {
		l2tp_ctrl_log(_this, LOG_ERR, "sending SCCRP failed");
		l2tp_ctrl_stop(_this, L2TP_STOP_CCN_RCODE_GENERAL);
		return;
	}
	l2tp_ctrl_log(_this, LOG_INFO, "SendSCCRP");
}

static int
l2tp_ctrl_send_HELLO(l2tp_ctrl *_this)
{
	struct l2tp_avp *avp;
	char buf[L2TP_AVP_MAXSIZ];
	bytebuffer *bytebuf;

	if ((bytebuf = l2tp_ctrl_prepare_snd_buffer(_this, 1)) == NULL) {
		l2tp_ctrl_log(_this, LOG_ERR,
		    "sending SCCRP failed: no buffer.");
		return 1;
	}
	avp = (struct l2tp_avp *)buf;

	/* Message Type = HELLO */
	memset(avp, 0, sizeof(*avp));
	avp->is_mandatory = 1;
	avp->attr_type = L2TP_AVP_TYPE_MESSAGE_TYPE;
	avp_set_val16(avp, L2TP_AVP_MESSAGE_TYPE_HELLO);
	bytebuf_add_avp(bytebuf, avp, 2);

	if ((l2tp_ctrl_send_packet(_this, 0, bytebuf, 1)) != 0) {
		l2tp_ctrl_log(_this, LOG_ERR, "sending HELLO failed");
		l2tp_ctrl_stop(_this, L2TP_STOP_CCN_RCODE_GENERAL);
		return 1;
	}
	l2tp_ctrl_log(_this, LOG_DEBUG, "SendHELLO");

	return 0;
}

/** ZLB の送信 */
static int
l2tp_ctrl_send_ZLB(l2tp_ctrl *_this)
{
	int loglevel;

	loglevel = (_this->state == L2TP_CTRL_STATE_ESTABLISHED)
	    ? LOG_DEBUG : LOG_INFO;
	l2tp_ctrl_log(_this, loglevel, "SendZLB");
	bytebuffer_clear(_this->zlb_buffer);
	bytebuffer_put(_this->zlb_buffer, BYTEBUFFER_PUT_DIRECT,
	    sizeof(struct l2tp_header));

	return l2tp_ctrl_send_packet(_this, 0, _this->zlb_buffer, 1);
}

/***********************************************************************
 * ユーティリティ関数
 ***********************************************************************/
/**
 * 送信バッファの準備
 * @return 送信バッファが Window を越えている場合には NULL が返ります。
 */
bytebuffer *
l2tp_ctrl_prepare_snd_buffer(l2tp_ctrl *_this, int with_seq)
{
	bytebuffer *bytebuf;

	L2TP_CTRL_ASSERT(_this != NULL);

	if (l2tp_ctrl_txwin_is_full(_this)) {
		l2tp_ctrl_log(_this, LOG_INFO, "sending buffer is full.");
		return NULL;
	}
	bytebuf = _this->snd_buffers[_this->snd_nxt % _this->winsz];
	bytebuffer_clear(bytebuf);
	if (with_seq)
		bytebuffer_put(bytebuf, BYTEBUFFER_PUT_DIRECT,
		    sizeof(struct l2tp_header));
	else
		bytebuffer_put(bytebuf, BYTEBUFFER_PUT_DIRECT,
		    offsetof(struct l2tp_header, ns));

	return bytebuf;
}

/**
 * 現在のステータスの文字列表現を返します。
 */
static inline const char *
l2tp_ctrl_state_string(l2tp_ctrl *_this)
{
	switch (_this->state) {
	case L2TP_CTRL_STATE_IDLE:		return "idle";
	case L2TP_CTRL_STATE_WAIT_CTL_CONN:	return "wait-ctl-conn";
	case L2TP_CTRL_STATE_WAIT_CTL_REPLY:	return "wait-ctl-reply";
	case L2TP_CTRL_STATE_ESTABLISHED:	return "established";
	case L2TP_CTRL_STATE_CLEANUP_WAIT:	return "cleanup-wait";
	}
	return "unknown";
}

/**
 * このインスタンスに基づいたラベルから始まるログを記録します。
 */
void
l2tp_ctrl_log(l2tp_ctrl *_this, int prio, const char *fmt, ...)
{
	char logbuf[BUFSIZ];
	va_list ap;

	va_start(ap, fmt);
#ifdef	L2TPD_MULITPLE
	snprintf(logbuf, sizeof(logbuf), "l2tpd id=%u ctrl=%u %s",
	    _this->l2tpd->id, _this->id, fmt);
#else
	snprintf(logbuf, sizeof(logbuf), "l2tpd ctrl=%u %s", _this->id, fmt);
#endif
	vlog_printf(prio, logbuf, ap);
	va_end(ap);
}
