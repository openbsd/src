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
#ifndef NPPPD_DEFS_H
#define NPPPD_DEFS_H 1

#define NPPPD_MAX_POOLED_ADDRS		8192
#define NPPPD_USER_HASH_SIZ		1777
#define	NPPPD_GENERIC_NAME_LEN		32
#ifndef	LOG_NPPPD
#define	LOG_NPPPD			LOG_LOCAL1
#endif

#ifndef NPPPD_MAX_SERVERS
/** RADIUSサーバの数 */
#define NPPPD_MAX_SERVERS			8
#endif

#ifndef	NPPPD_TIMER_TICK_IVAL 
#define	NPPPD_TIMER_TICK_IVAL 			4
#endif

/** 認証レルム終了化処理のインターバル時間(sec) */
#define NPPPD_AUTH_REALM_FINALIZER_INTERVAL		300

#ifndef	NPPPD_MAX_IPCP_CONFIG
/** IPCP設定の数 */
#define	NPPPD_MAX_IPCP_CONFIG			1
#endif

#ifndef	NPPPD_MAX_IFACE
/** PPP集約インタフェース(tun や pppac) の数 */
#define	NPPPD_MAX_IFACE				1
#endif

#ifndef	NPPPD_MAX_POOL
/** プールの数 */
#define	NPPPD_MAX_POOL				1
#endif

#ifndef	NPPPD_MAX_PPTP
/** ローカル認証レルムの数 */
#define	NPPPD_MAX_PPTP				2
#endif

#ifndef	NPPPD_DEFAULT_AUTH_LOCAL_RELOADABLE
#define	NPPPD_DEFAULT_AUTH_LOCAL_RELOADABLE	0
#endif

/** 同一ユーザが接続できる最大の PPPセッション数のデフォルト */
#define	NPPPD_DEFAULT_USER_MAX_PPP	3

#ifndef	NPPPD_DEFAULT_MAX_PPP
/** 同時に接続できる最大の PPPセッション数のデフォルト */
#define	NPPPD_DEFAULT_MAX_PPP		8192
#endif

#define	NPPPD_UID			-1	/* 特に指定しない */
#ifndef	NPPPD_GID			
/** npppd 実行時のグループID。*/
#define	NPPPD_GID			0
#endif

#ifndef	LOOPBACK_IFNAME
#define	LOOPBACK_IFNAME			"lo0"
#endif

#ifndef	NPPPD_DEFAULT_IP_ASSIGN_USER_SELECT
#define	NPPPD_DEFAULT_IP_ASSIGN_USER_SELECT	1
#endif
#ifndef	NPPPD_DEFAULT_IP_ASSIGN_FIXED
#define	NPPPD_DEFAULT_IP_ASSIGN_FIXED		1
#endif
#ifndef	NPPPD_DEFAULT_IP_ASSIGN_RADIUS
#define	NPPPD_DEFAULT_IP_ASSIGN_RADIUS		0
#endif

/** rtev_write() を使う */
#define NPPPD_USE_RTEV_WRITE			1

#ifndef DEFAULT_RTSOCK_EVENT_DELAY
/** Routing ソケットイベントを受けてから、処理を開始するまでの待ち時間(秒)*/
#define	DEFAULT_RTSOCK_EVENT_DELAY		5
#endif
#ifndef DEFAULT_RTSOCK_SEND_NPKTS
/** Routing ソケットに書き込む際に一度に書くパケット数*/
#define	DEFAULT_RTSOCK_SEND_NPKTS		16
#endif
#ifndef DEFAULT_RTSOCK_SEND_WAIT_MILLISEC
/** Routing ソケットへの連続書き込みで間隔を空ける時間(ミリ秒) */
#define	DEFAULT_RTSOCK_SEND_WAIT_MILLISEC	0
#endif

#ifndef	countof
#define	countof(x)	(sizeof(x) / sizeof((x)[0]))
#endif

#endif
