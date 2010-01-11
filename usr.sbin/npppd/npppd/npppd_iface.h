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
#ifndef NPPPD_INTERFACE_H
#define NPPPD_INTERFACE_H 1

typedef struct _npppd_iface {
 	/** ベースとなる npppd */
	void	*npppd;
 	/** インタフェース名 */
	char	ifname[IFNAMSIZ];
 	/** デバイスファイルのデスクリプタ */
	int	devf;

 	/** 割り当てられた IPv4 アドレス */
	struct in_addr	ip4addr;
 	/** event(3) 用メンバー */
	struct event	ev;

	/** 同一PPPユーザが接続できる最大の PPPセッション数 */
	int		user_max_session;
	/** 接続できる最大の PPPセッション数 */
	int		max_session;

	/** 接続中の PPPセッション数 */
	int		nsession;

 	int	/**
 		 * npppd_iface の処理としてIPアドレスをセットするか。
 		 * <p>0 であれば、npppd_iface は、セットされた IPアドレスを
 		 * 参照するだけです。</p>
 		 */
 		set_ip4addr:1,
 		/** 初期化済みフラグ */
  		initialized:1;
} npppd_iface;

/** インタフェースのIPアドレスは使用できるか */
#define npppd_iface_ip_is_ready(int) \
    ((int)->initialized != 0 && (int)->ip4addr.s_addr != INADDR_ANY)

#ifdef __cplusplus
extern "C" {
#endif

void  npppd_iface_init (npppd_iface *, const char *);
int   npppd_iface_reinit (npppd_iface *);
int   npppd_iface_start (npppd_iface *);
void  npppd_iface_stop (npppd_iface *);
void  npppd_iface_fini (npppd_iface *);
void  npppd_iface_write (npppd_iface *, int proto, u_char *, int);

#ifdef __cplusplus
}
#endif
#endif
