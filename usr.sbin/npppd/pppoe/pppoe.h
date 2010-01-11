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
#ifndef	PPPOE_H
#define	PPPOE_H 1

/*
 * プロトコル上の定数 (RFC 2516)
 */
#define PPPOE_RFC2516_TYPE	0x01
#define PPPOE_RFC2516_VER	0x01

/** The PPPoE Active Discovery Initiation (PADI) packet */
#define	PPPOE_CODE_PADI		0x09

/** The PPPoE Active Discovery Offer (PADO) packet */
#define	PPPOE_CODE_PADO		0x07

/** The PPPoE Active Discovery Request (PADR) packet */
#define	PPPOE_CODE_PADR		0x19

/** The PPPoE Active Discovery Session-confirmation (PADS) packet */
#define	PPPOE_CODE_PADS		0x65

/** The PPPoE Active Discovery Terminate (PADT) packet */
#define	PPPOE_CODE_PADT		0xa7

#define	PPPOE_TAG_END_OF_LIST		0x0000
#define	PPPOE_TAG_SERVICE_NAME		0x0101
#define	PPPOE_TAG_AC_NAME		0x0102
#define	PPPOE_TAG_HOST_UNIQ		0x0103
#define	PPPOE_TAG_AC_COOKIE		0x0104
#define	PPPOE_TAG_VENDOR_SPECIFIC	0x0105
#define	PPPOE_TAG_RELAY_SESSION_ID	0x0110
#define	PPPOE_TAG_SERVICE_NAME_ERROR	0x0201
#define	PPPOE_TAG_AC_SYSTEM_ERROR	0x0202
#define	PPPOE_TAG_GENERIC_ERROR		0x0203

/** PPPoE ヘッダ */
struct pppoe_header {
#if _BYTE_ORDER == _BIG_ENDIAN
    uint8_t ver:4, type:4;
#else
    uint8_t type:4, ver:4;
#endif
    uint8_t code;
    uint16_t session_id;
    uint16_t length;
} __attribute__((__packed__));

/** PPPoE TLV ヘッダ */
struct pppoe_tlv {
	uint16_t	type;
	uint16_t	length;
	uint8_t		value[0];
} __attribute__((__packed__));

/*
 * 実装
 */
/** デフォルトの物理層ラベル */
#define PPPOED_DEFAULT_LAYER2_LABEL	"PPPoE"

#define	PPPOED_CONFIG_BUFSIZ		65535
#define	PPPOED_HOSTUNIQ_LEN		64
#define PPPOED_PHY_LABEL_SIZE		16

/*
 * pppoed ステータス
 */
/** 初期状態 */
#define	PPPOED_STATE_INIT 		0
/** 走行中 */
#define	PPPOED_STATE_RUNNING 		1
/** 停止j状態 */
#define	PPPOED_STATE_STOPPED 		2

#define pppoed_is_stopped(pppoed)	\
	(((pppoed)->state == PPPOED_STATE_STOPPED)? 1 : 0)
#define pppoed_is_running(pppoed)	\
	(((pppoed)->state == PPPOED_STATE_RUNNING)? 1 : 0)

#define	PPPOED_LISTENER_INVALID_INDEX	UINT16_MAX

/** PPPoE デーモン待ち受け型 */
typedef struct _pppoed_listener {
	/** bpf(4) デバイスファイルのディスクリプタ */
	int bpf;
	/** bpf 用のイベントコンテキスト */
	struct event ev_bpf;
	/** PPPoE デーモン自身 */
	struct _pppoed *self;
	/** イーサネットアドレス */
	u_char	ether_addr[ETHER_ADDR_LEN];
	/** インデックス番号 */
	uint16_t	index;
	/** 待ち受けるインタフェース名 */
	char	listen_ifname[IF_NAMESIZE];
	/** 物理層のラベル */
	char	phy_label[PPPOED_PHY_LABEL_SIZE];
} pppoed_listener;

/** PPPoE デーモンを示す型です。*/
typedef struct _pppoed {
	/** PPPoE デーモンの Id */
	int id;
	/** 待ち受けリスト */
	slist listener;
	/** このデーモンの状態 */
	int state;
	/** タイムアウトイベント **/

	/** セッション番号 => pppoe_session のハッシュマップ */
	hash_table	*session_hash;
	/** 空きセッション番号リスト */
	slist	session_free_list;

	/** クッキー所持セッションのハッシュマップ */
	hash_table	*acookie_hash;
	/** 次のクッキー番号 */
	uint32_t	acookie_next;

	/** 設定プロパティ */
	struct properties *config;

	/** フラグ */
	uint32_t
	    desc_in_pktdump:1,
	    desc_out_pktdump:1,
	    session_in_pktdump:1,
	    session_out_pktdump:1,
	    listen_incomplete:1,
	    /* phy_label_with_ifname:1,	PPPoE は不要 */
	    reserved:27;
} pppoed;

/** PPPoE セッションを示す型です */
typedef struct _pppoe_session {
	int 		state;
	/** 親 PPPoE デーモン */
	pppoed		*pppoed;
	/** PPP コンテキスト */
	void 		*ppp;
	/** セッション Id */
	int		session_id;
	/** クッキー番号 */
	int		acookie;
	/** 対抗のイーサネットアドレス */
	u_char 		ether_addr[ETHER_ADDR_LEN];
	/** リスナインデックス */
	uint16_t	listener_index;
	/** イーサヘッダキャッシュ */
	struct ether_header ehdr;

	/** echo の送信間隔(秒) */
	int lcp_echo_interval;
	/** echo の最大連続失敗回数 */
	int lcp_echo_max_failure;

	/** 終了処理のための event コンテキスト */
	struct event ev_disposing;
} pppoe_session;

/** pppoe_session の状態が初期状態であることを示す定数です */
#define	PPPOE_SESSION_STATE_INIT		0

/** pppoe_session の状態が走行状態であることを示す定数です */
#define	PPPOE_SESSION_STATE_RUNNING		1

/** pppoe_session の状態が終了中であることを示す定数です */
#define	PPPOE_SESSION_STATE_DISPOSING		2

#define	pppoed_need_polling(pppoed)	(((pppoed)->listen_incomplete != 0)? 1 : 0)

#ifdef __cplusplus
extern "C" {
#endif

int         pppoe_session_init (pppoe_session *, pppoed *, int, int, u_char *);
void        pppoe_session_fini (pppoe_session *);
void        pppoe_session_stop (pppoe_session *);
int         pppoe_session_recv_PADR (pppoe_session *, slist *);
int         pppoe_session_recv_PADT (pppoe_session *, slist *);
void        pppoe_session_input (pppoe_session *, u_char *, int);
void        pppoe_session_disconnect (pppoe_session *);


int         pppoed_add_listener (pppoed *, int, const char *, const char *);
int         pppoed_reload_listeners(pppoed *);

int   pppoed_init (pppoed *);
int   pppoed_start (pppoed *);
void  pppoed_stop (pppoed *);
void  pppoed_uninit (pppoed *);
void  pppoed_pppoe_session_close_notify(pppoed *, pppoe_session *);
const char * pppoed_tlv_value_string(struct pppoe_tlv *);
int pppoed_reload(pppoed *, struct properties *, const char *, int);
const char   *pppoed_config_str (pppoed *, const char *);
int          pppoed_config_int (pppoed *, const char *, int);
int          pppoed_config_str_equal (pppoed *, const char *, const char *, int);
int          pppoed_config_str_equali (pppoed *, const char *, const char *, int);

const char   *pppoed_listener_config_str (pppoed_listener *, const char *);
int          pppoed_listener_config_int (pppoed_listener *, const char *, int);
int          pppoed_listener_config_str_equal (pppoed_listener *, const char *, const char *, int);
int          pppoed_listener_config_str_equali (pppoed_listener *, const char *, const char *, int);

#ifdef __cplusplus
}
#endif
#endif
