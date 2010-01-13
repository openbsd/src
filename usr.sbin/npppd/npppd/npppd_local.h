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
#ifndef	_NPPPD_LOCAL_H
#define	_NPPPD_LOCAL_H	1

#ifndef	NPPPD_BUFSZ	
/** バッファサイズ */
#define	NPPPD_BUFSZ			BUFSZ
#endif

#include <sys/param.h>
#include <net/if.h>

#include "npppd_defs.h"

#include "slist.h"
#include "hash.h"
#include "properties.h"

#ifdef	USE_NPPPD_RADIUS
#include <radius+.h>
#include "radius_req.h"
#endif

#ifdef	USE_NPPPD_L2TP
#include "debugutil.h"
#include "bytebuf.h"
#include "l2tp.h"
#endif

#ifdef	USE_NPPPD_PPTP
#include "bytebuf.h"
#include "pptp.h"
#endif
#ifdef	USE_NPPPD_PPPOE
#if defined(__NetBSD__)
#include <net/if_ether.h>
#else
#include <netinet/if_ether.h>
#endif
#include "bytebuf.h"
#include "pppoe.h"
#endif
#include "npppd_auth.h"
#include "npppd_iface.h"
#include "npppd.h"

#ifdef	USE_NPPPD_NPPPD_CTL
typedef struct _npppd_ctl {
	/** イベントコンテキスト */
	struct event ev_sock;
	/** ソケット */
	int sock;
	/** 有効/無効 */
	int enabled;
	/** 親 npppd */
	void *npppd;
	/** ソケットのパス名 */
	char pathname[MAXPATHLEN];
	/** 最大メッセージ長 */
	int max_msgsz;
} npppd_ctl;
#endif

#include "addr_range.h"
#include "npppd_pool.h"

/** プールを示す型 */
struct _npppd_pool {
	/** 基となる npppd */
	npppd		*npppd;
	/** ラベル名 */
	char		label[NPPPD_GENERIC_NAME_LEN];
	/** 名前(name) */
	char		name[NPPPD_GENERIC_NAME_LEN];
	/** sockaddr_npppd 配列のサイズ */
	int		addrs_size;
	/** sockaddr_npppd 配列 */
	struct sockaddr_npppd *addrs;
	/** 動的に割り当てるアドレスのリスト */
	slist 		dyna_addrs;
	int		/** 初期化済 */
			initialized:1,
			/** 利用中 */
			running:1;
};

/** IPCP設定を示す型 */
typedef struct _npppd_ipcp_config {
	/** 名前 */
	char	name[NPPPD_GENERIC_NAME_LEN];
	/** ラベル(紐付けるため) */
	char	label[NPPPD_GENERIC_NAME_LEN];
	/** 親 npppd へのポインタ */
	npppd	*npppd;
	/**
	 * プライマリDNSサーバ。先方に通知しない場合は INADDR_NONE。
	 * ネットワークバイトオーダー。
	 */
	struct in_addr	dns_pri;

	/** セカンダリDNSサーバ。先方に通知しない場合は INADDR_NONE。
	 * ネットワークバイトオーダー。
	 */
	struct in_addr	dns_sec;

	/**
	 * プライマリWINSサーバ。先方に通知しない場合は INADDR_NONE。
	 * ネットワークバイトオーダー。
	 */
	struct in_addr	nbns_pri;

	/**
	 * セカンダリWINSサーバ。先方に通知しない場合は INADDR_NONE。
	 * ネットワークバイトオーダー。
	 */
	struct in_addr	nbns_sec;

	/**
	 * IPアドレス割り当て方法のビットフラグ。
	 * @see	#NPPPD_IP_ASSIGN_FIXED
	 * @see	#NPPPD_IP_ASSIGN_USER_SELECT
	 * @see	#NPPPD_IP_ASSIGN_RADIUS
	 */
	int 		ip_assign_flags;

	int		/** DNS サーバとしてトンネル終端アドレスを使う */
			dns_use_tunnel_end:1,
			/** 初期化済かどうか */
			initialized:1,
			reserved:30;
} npppd_ipcp_config;

/** インタフェースの IPCP 設定やプールアドレスへの参照を保持する型 */
typedef struct _npppd_iface_binding {
	npppd_ipcp_config	*ipcp;
	slist			pools;
} npppd_iface_binding;

/**
 * npppd
 */
struct _npppd {
	/** イベントハンドラー */
	struct event ev_sigterm, ev_sigint, ev_sighup, ev_timer;

	/** PPPを集約するインターフェース */
	npppd_iface		iface[NPPPD_MAX_IFACE];
	/** インタフェースの IPCP 設定やプールアドレスへの参照 */
	npppd_iface_binding	iface_bind[NPPPD_MAX_IFACE];

	/** アドレスプール */
	npppd_pool		pool[NPPPD_MAX_POOL];

	/** radish プール、割り当てアドレス管理用 */
	struct radish_head *rd;

	/** IPCP 設定 */
	npppd_ipcp_config ipcp_config[NPPPD_MAX_IPCP_CONFIG];

	/** ユーザ名 → slist of npppd_ppp のマップ */
	hash_table *map_user_ppp;

	/** 認証レルム */
	slist realms;

	/** 認証レルム終了化処理のインターバル時間(sec) */
	int auth_finalizer_itvl;

	/** 設定ファイル名 */
	char 	config_file[MAXPATHLEN];

	/** PIDファイル名 */
	char 	pidpath[MAXPATHLEN];

	/** プロセス ID */
	pid_t	pid;

#ifdef	USE_NPPPD_L2TP
	/** L2TP デーモン */
	l2tpd l2tpd;
#endif
#ifdef	USE_NPPPD_PPTP
	/** PPTP デーモン */
	pptpd pptpd;
#endif
#ifdef	USE_NPPPD_PPPOE
	/** PPPOE デーモン */
	pppoed pppoed;
#endif
	/** 設定ファイル */
	struct properties * properties;

	/** ユーザ設定ファイル */
	struct properties * users_props;

#ifdef	USE_NPPPD_NPPPD_CTL
	npppd_ctl ctl;
#endif
	/** 起動してからの秒数。*/
	uint32_t	secs;

	/** 設定再読み込みを何秒猶予するか */
	int16_t		delayed_reload;
	/** 設定再読み込みカウンタ */
	int16_t		reloading_count;

	/** 処理済みのルーティングイベントシリアル */
	int		rtev_event_serial;

	/** 接続できる最大の PPPセッション数 */
	int		max_session;

	int /** 終了処理中 */
	    finalizing:1,
	    /** 終了処理完了 */
	    finalized:1;
};

#ifndef	NPPPD_CONFIG_BUFSIZ
#define	NPPPD_CONFIG_BUFSIZ	65536	// 64K
#endif
#ifndef	NPPPD_KEY_BUFSIZ
#define	NPPPD_KEY_BUFSIZ	512
#endif
#define	ppp_iface(ppp)	(&(ppp)->pppd->iface[(ppp)->ifidx])
#define	ppp_ipcp(ppp)	((ppp)->pppd->iface_bind[(ppp)->ifidx].ipcp)
#define	ppp_pools(ppp)	(&(ppp)->pppd->iface_bind[(ppp)->ifidx].pools)

#define	SIN(sa)		((struct sockaddr_in *)(sa))

#define	TIMER_TICK_RUP(interval)			\
	((((interval) % NPPPD_TIMER_TICK_IVAL) == 0)	\
	    ? (interval)				\
	    : (interval) + NPPPD_TIMER_TICK_IVAL	\
		- ((interval) % NPPPD_TIMER_TICK_IVAL))

#ifdef	USE_NPPPD_NPPPD_CTL
void  npppd_ctl_init (npppd_ctl *, npppd *, const char *);
int   npppd_ctl_start (npppd_ctl *);
void  npppd_ctl_stop (npppd_ctl *);
#endif
#define	sin46_port(x)	(((x)->sa_family == AF_INET6)	\
	? ((struct sockaddr_in6 *)(x))->sin6_port		\
	: ((struct sockaddr_in *)(x))->sin_port)


#endif
