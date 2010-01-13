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
#ifndef PPTP_H
#define PPTP_H	1

/************************************************************************
 * プロトコル上の定数
 ************************************************************************/
#define	PPTP_MES_TYPE_CTRL			1
#define	PPTP_MAGIC_COOKIE			0x1a2b3c4d
#define	PPTP_RFC_2637_VERSION			0x0100

#ifndef	PPTP_MAX_CALL
#define	PPTP_MAX_CALL				8192
#endif


/** Start-Control-Connection-Request */
#define	PPTP_CTRL_MES_CODE_SCCRQ		1

/** Start-Control-Connection-Reply */
#define	PPTP_CTRL_MES_CODE_SCCRP		2

/** Stop-Control-Connection-Request */
#define	PPTP_CTRL_MES_CODE_StopCCRQ		3

/** Stop-Control-Connection-Reply */
#define	PPTP_CTRL_MES_CODE_StopCCRP		4

/** Echo-Request */
#define	PPTP_CTRL_MES_CODE_ECHO_RQ		5

/** Echo-Reply */
#define	PPTP_CTRL_MES_CODE_ECHO_RP		6

/** Outgoing-Call-Request */
#define	PPTP_CTRL_MES_CODE_OCRQ			7

/** Outgoing-Call-Reply */
#define	PPTP_CTRL_MES_CODE_OCRP			8

/** Incoming-Call-Request */
#define	PPTP_CTRL_MES_CODE_ICRQ			9

/** Incoming-Call-Reply */
#define	PPTP_CTRL_MES_CODE_ICRP			10

/** Incoming-Call-Connected */
#define	PPTP_CTRL_MES_CODE_ICCN			11

/** Call-Clear-Request */
#define	PPTP_CTRL_MES_CODE_CCR			12

/** Call-Disconnect-Notify */
#define	PPTP_CTRL_MES_CODE_CDN			13

/** Set-Link-Info */
#define	PPTP_CTRL_MES_CODE_SLI			15


#define	PPTP_CTRL_FRAMING_ASYNC			1
#define	PPTP_CTRL_FRAMING_SYNC			2

#define	PPTP_CTRL_BEARER_ANALOG			1
#define	PPTP_CTRL_BEARER_DIGITAL		2

/* Start-Control-Connection-Reply の Result Code */
#define PPTP_SCCRP_RESULT_SUCCESS		1
#define PPTP_SCCRP_RESULT_GENERIC_ERROR		2
#define PPTP_SCCRP_RESULT_CHANNEL_EXISTS	3
#define PPTP_SCCRP_RESULT_NOT_AUTHORIZIZED	4
#define PPTP_SCCRP_RESULT_BAD_PROTOCOL_VERSION	5

/* General Error Code (RFC 2637 2.16 pp.36) */
#define PPTP_ERROR_NONE				0
#define PPTP_ERROR_NOT_CONNECTED		1
#define PPTP_ERROR_BAD_FORMAT			2
#define PPTP_ERROR_NO_RESOURCE			3
#define PPTP_ERROR_BAD_CALL			4
#define PPTP_ERROR_PAC_ERROR			5

/* Outgoing-Call-Reply の Result Code */
#define PPTP_OCRP_RESULT_CONNECTED		1
#define PPTP_OCRP_RESULT_GENERIC_ERROR		2
#define PPTP_OCRP_RESULT_NO_CARRIER		3
#define PPTP_OCRP_RESULT_BUSY			4
#define PPTP_OCRP_RESULT_NO_DIALTONE		5
#define PPTP_OCRP_RESULT_TIMEOUT		6
#define PPTP_OCRP_RESULT_DO_NOT_ACCEPT		7

/* Echo-Reply の Result Code */
#define PPTP_ECHO_RP_RESULT_OK			1
#define PPTP_ECHO_RP_RESULT_GENERIC_ERROR	2

/* Stop-Control-Connection-Request の Reason */
#define	PPTP_StopCCRQ_REASON_NONE			1
#define	PPTP_StopCCRQ_REASON_STOP_PROTOCOL		2
#define	PPTP_StopCCRQ_REASON_STOP_LOCAL_SHUTDOWN	3

/* Stop-Control-Connection-Response の Result */
#define	PPTP_StopCCRP_RESULT_OK			1
#define	PPTP_StopCCRP_RESULT_GENERIC_ERROR	2

#define	PPTP_CDN_RESULT_LOST_CARRIER		1
#define	PPTP_CDN_RESULT_GENRIC_ERROR		2
#define	PPTP_CDN_RESULT_ADMIN_SHUTDOWN		3
#define	PPTP_CDN_RESULT_REQUEST			4

/** デフォルトの待ち受け TCP ポート番号 */
#define	PPTPD_DEFAULT_TCP_PORT			1723


#define	PPTP_GRE_PROTOCOL_TYPE			0x880b
#define	PPTP_GRE_VERSION			1

/************************************************************************
 * この実装の定数
 ************************************************************************/
/* pptpd ステータス */
#define	PPTPD_STATE_INIT 		0
#define	PPTPD_STATE_RUNNING 		1
#define	PPTPD_STATE_SHUTTING_DOWN 	2
#define	PPTPD_STATE_STOPPED 		3

#define	PPTPD_CONFIG_BUFSIZ		65535

#define	PPTP_BACKLOG	32
#define PPTP_BUFSIZ	1024

#define PPTPD_DEFAULT_LAYER2_LABEL		"PPTP"

/** ステートマシン */
#define PPTP_CTRL_STATE_IDLE			0
#define PPTP_CTRL_STATE_WAIT_CTRL_REPLY		1
#define PPTP_CTRL_STATE_ESTABLISHED		2
#define PPTP_CTRL_STATE_WAIT_STOP_REPLY		3
#define PPTP_CTRL_STATE_DISPOSING		4

#ifndef	PPTPD_DEFAULT_VENDOR_NAME
#define	PPTPD_DEFAULT_VENDOR_NAME		""
#endif

#ifndef	PPTP_CALL_DEFAULT_MAXWINSZ
#define	PPTP_CALL_DEFAULT_MAXWINSZ		64
#endif

#ifndef	PPTP_CALL_CONNECT_SPEED
/** 接続スピード。OCRP で通知する値で、現在は 10Mbps で固定 */
#define	PPTP_CALL_CONNECT_SPEED			10000000
#endif

#ifndef	PPTP_CALL_INITIAL_PPD
/** OCRP で通知する。初期 Packet Processing Dealy */
#define PPTP_CALL_INITIAL_PPD			0
#endif

#ifndef	PPTP_CALL_NMAX_INSEQ
/** シーケンスが戻った場合に何パケットまでを戻ったとみなすか。*/
#define	PPTP_CALL_NMAX_INSEQ	64
#endif

/** call のステートマシン */
#define	PPTP_CALL_STATE_IDLE			0
#define	PPTP_CALL_STATE_WAIT_CONN		1
#define	PPTP_CALL_STATE_ESTABLISHED		2
#define	PPTP_CALL_STATE_CLEANUP_WAIT		3

/* タイムアウト関連 */
#define PPTPD_SHUTDOWN_TIMEOUT			5

#define	PPTPD_IDLE_TIMEOUT			60

#define	PPTP_CALL_CLEANUP_WAIT_TIME		3

#define PPTP_CTRL_DEFAULT_ECHO_INTERVAL		60
#define PPTP_CTRL_DEFAULT_ECHO_TIMEOUT		60
#define	PPTP_CTRL_StopCCRP_WAIT_TIME		3

/** アドレスは最大何個 bind 可能か。*/
#ifndef	PPTP_NLISTENER
#define	PPTP_NLISTENER				6
#endif

/** PPTPデーモンが停止したかどうかを返します。 */
#define	pptpd_is_stopped(pptpd)					\
	(((pptpd)->state != PPTPD_STATE_SHUTTING_DOWN &&	\
	    (pptpd)->state != PPTPD_STATE_RUNNING)? 1 : 0)

/** PPTPデーモンが停止処理中かどうかを返します。 */
#define	pptpd_is_shutting_down(pptpd)				\
	(((pptpd)->state == PPTPD_STATE_SHUTTING_DOWN)? 1 : 0)

/************************************************************************
 * 型
 ************************************************************************/
/** PPTP デーモンの型*/
struct _pptpd;
/** PPTP デーモン待ち受け型 */
typedef struct _pptpd_listener {
	/** イベントコンテキスト */
	struct event ev_sock;
	/** GREイベントコンテキスト */
	struct event ev_sock_gre;
	/** PPTPD 自身 */
	struct _pptpd	*self;
	/** インデックス番号 */
	uint16_t	index;
	/** 待ち受けソケット */
	int		sock;
	/** GREソケット */
	int		sock_gre;
	/** 待ち受けアドレス TCP */
	struct sockaddr_in bind_sin;
	/** 待ち受けアドレス GRE */
	struct sockaddr_in bind_sin_gre;
	/** 物理層のラベル */
	char	phy_label[16];
} pptpd_listener;

/** PPTP デーモンの型*/
typedef struct _pptpd {
	/** インスタンスの Id */
	unsigned	id;
	/** 待ち受けリスト */
	slist listener;
	/** ステート */
	int state;
	/** タイマーイベントコンテキスト */
	struct event ev_timer;
	/** PPTP コントロールのリスト */
	slist  ctrl_list;

	/** 設定 */
	struct properties *config;

	/** 空きコールリスト */
	slist call_free_list;
	/** コールId => Call マップ */
	hash_table *call_id_map;
	/** 接続を許可するIPv4ネットワーク */
	struct in_addr_range *ip4_allow;

	/** フラグ */
	uint32_t
	    initialized:1,
	    ctrl_in_pktdump:1,
	    ctrl_out_pktdump:1,
	    data_in_pktdump:1,
	    data_out_pktdump:1,
	    phy_label_with_ifname:1;
} pptpd;

#define pptp_ctrl_sock_gre(ctrl)	\
	((pptpd_listener *)slist_get(&(ctrl)->pptpd->listener,\
	    (ctrl)->listener_index))->sock_gre

/** pptp_ctrl から、リスナーの物理層のラベルを取り出すマクロ */
#define	PPTP_CTRL_LISTENER_LABEL(ctrl)	\
	((pptpd_listener *)slist_get(&(ctrl)->pptpd->listener,\
	    (ctrl)->listener_index))->phy_label

/** PPTP コントロールの型*/
typedef struct _pptp_ctrl {
	/** 親 pptpd */
	pptpd		*pptpd;
	/** リスナー インデックス番号*/
	uint16_t	listener_index;
	/** インスタンスの Id */
	unsigned 	id;
	/** ステート */
	int		state;
	/** 物理層のラベル */
	char		phy_label[16];

	/** ソケット */
	int		sock;
	/** 先方のアドレス */
	struct sockaddr_storage peer;
	/** 当方のアドレス */
	struct sockaddr_storage our;
	/** イベントコンテキスト */
	struct event	ev_sock;
	/** タイマーイベントコンテキスト */
	struct event	ev_timer;

	/** アイドル状態から ECHO 送信までの秒数。0以下は無効。*/
	int echo_interval;
	/** ECHO のタイムアウト */
	int echo_timeout;

	/** 送信可能かどうか。送信バッファが埋まっていないか */
	int		send_ready;
	/** 受信バッファ */
	bytebuffer	*recv_buf;
	/** 送信バッファ */
	bytebuffer	*send_buf;

	/** コールのリスト */
	slist		call_list;

	/** 最後にコントロールメッセージを送信した時間 */
	time_t	last_snd_ctrl;	
	/** 最後にコントロールメッセージを受信を送信した時間 */
	time_t	last_rcv_ctrl;	
	/** Echo Request の identifier */
	uint32_t	echo_seq;

	int16_t		/** I/O イベント処理中。*/
			on_io_event:1,
			reserved:15;

} pptp_ctrl;

/** PPTP コールの型 */
typedef struct _pptp_call {
	/** 親コントロール */
	pptp_ctrl	*ctrl;
	/** コールID*/
	unsigned	id;

	/** 受信インタフェース番号 */
	int		ifidx;

	/** ステート */
	int		state;

	/** 先方のコールID*/
	unsigned	peers_call_id;
	void		*ppp;

	/** 次の確認応答 */
	uint32_t	snd_una;
	/** 次の送信シーケンス番号 */
	uint32_t	snd_nxt;

	/** 受信シーケンス番号 */
	uint32_t	rcv_nxt;
	/** 応答確認した受信シーケンス番号 */
	uint32_t	rcv_acked;

	/** カレントウィンドウサイズ */
	int		winsz;
	/** 最大ウィンドウサイズ */
	int		maxwinsz;
	/** 先方の最大ウィンドウサイズ*/
	int		peers_maxwinsz;

	time_t		last_io;

} pptp_call;


/************************************************************************
 * 関数プロトタイプ
 ************************************************************************/
#ifdef __cplusplus
extern "C" {
#endif

int   pptpd_init (pptpd *);
void  pptpd_uninit (pptpd *);
int   pptpd_assign_call (pptpd *, pptp_call *);
void  pptpd_release_call (pptpd *, pptp_call *);
int   pptpd_start (pptpd *);
void  pptpd_stop (pptpd *);
void pptpd_stop_immediatly (pptpd *);
void  pptpd_ctrl_finished_notify(pptpd *, pptp_ctrl *);
int  pptpd_add_listener(pptpd *, int, const char *, struct sockaddr *);

pptp_ctrl  *pptp_ctrl_create (void);
int        pptp_ctrl_init (pptp_ctrl *);
int        pptp_ctrl_start (pptp_ctrl *);
void       pptp_ctrl_stop (pptp_ctrl *, int);
void       pptp_ctrl_destroy (pptp_ctrl *);
void       pptp_ctrl_output (pptp_ctrl *, u_char *, int);

pptp_call  *pptp_call_create (void);
int        pptp_call_init (pptp_call *, pptp_ctrl *);
int        pptp_call_start (pptp_call *);
int        pptp_call_stop (pptp_call *);
void       pptp_call_destroy (pptp_call *);
void       pptp_call_input (pptp_call *, int, u_char *, int);
void       pptp_call_gre_input (pptp_call *, uint32_t, uint32_t, int, u_char *, int);
void       pptp_call_disconnect(pptp_call *, int, int, const char *);
int        pptpd_reload(pptpd *, struct properties *, const char *, int);

/* config_helper  */
const char   *pptpd_config_str (pptpd *, const char *);
int          pptpd_config_int (pptpd *, const char *, int);
int          pptpd_config_str_equal (pptpd *, const char *, const char *, int);
int          pptpd_config_str_equali (pptpd *, const char *, const char *, int);
const char   *pptp_ctrl_config_str (pptp_ctrl *, const char *);
int          pptp_ctrl_config_int (pptp_ctrl *, const char *, int);
int          pptp_ctrl_config_str_equal (pptp_ctrl *, const char *, const char *, int);
int          pptp_ctrl_config_str_equali (pptp_ctrl *, const char *, const char *, int);

#ifdef __cplusplus
}
#endif
#endif
