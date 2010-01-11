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
#ifndef	_NPPPD_H
#define	_NPPPD_H 1

#define	PPP_HDRLEN		4
#define PPP_ALLSTATIONS		0xff
#define	PPP_UI			0x03

#define PPP_PROTO_IP		0x0021
#define PPP_PROTO_MPPE		0x00FD

#define PPP_PROTO_LCP		0xC021
#define PPP_PROTO_PAP		0xC023
#define PPP_PROTO_LQR		0xC025
#define PPP_PROTO_CHAP		0xC223
#define PPP_PROTO_EAP		0xC227
#define PPP_PROTO_NCP		0x8000
#define		NCP_CCP		0xfd
#define		NCP_IPCP	0x21
#define		NCP_IP6CP	0x57
#define PPP_PROTO_ACSP		0x8235 		/* Apple Client Server Protocol
						   Control */

#define	PPP_LCP_MRU			1	/* Maximum-Receive-Unit */
#define	PPP_LCP_AUTH_PROTOCOL		3	/* Authentication-Protcol */
#define	PPP_LCP_QUALITY_PROTOCOL	4	/* Quality-Control */
#define	PPP_LCP_MAGICNUMBER		5	/* Magic-Number */
#define	PPP_LCP_PFC			7	/* Protocol Field Compression */
#define	PPP_LCP_ACFC			8	/* Address-and-Control-Field-
						   Compression */

#define DEFAULT_MRU		1400
#define	NPPPD_MIN_MRU		500

#define	PPP_AUTH_PAP	0xc023	/* Password Authentication Protocol */
#define	PPP_AUTH_CHAP	0xc223	/* Challenge Handshake Authentication Protocol*/
#define	PPP_AUTH_EAP	0xc227	/* Extensible Authentication Protocol */

/* EAP Type */
#define	PPP_AUTH_EAP_ANY 	 	0x00
#define	PPP_AUTH_EAP_IDENTITY		0x01
#define	PPP_AUTH_EAP_NOTIFICATION  	0x02
#define	PPP_AUTH_EAP_NAK 	 	0x03
#define	PPP_AUTH_EAP_MD5_CHALLENGE	0x04
#define	PPP_AUTH_EAP_OTP	 	0x05
#define	PPP_AUTH_EAP_GTC	 	0x06
#define	PPP_AUTH_EAP_TLS 	 	0x0d
#define	PPP_AUTH_EAP_EXPANDED_TYPES 	0xFE
#define	PPP_AUTH_EAP_EXPERIMENTAL_USE 	0xFF

/* Chap Algorithms */
#define	PPP_AUTH_CHAP_MD5	0x05	/* MD5 */
#define	PPP_AUTH_CHAP_MS	0x80	/* MS-CHAP */
#define	PPP_AUTH_CHAP_MS_V2	0x81	/* MS-CHAP-V2 */

#define	MAX_USERNAME_LENGTH	256
#define	MAX_PASSWORD_LENGTH	256
#define MAX_CHALLENGE_LENGTH    24

#define INADDR_IPCP_OBEY_REMOTE_REQ	0x00000000L

#define	MPPE_KEYLEN	16	/* 128bit */
#define	CCP_MPPE		0x12
#define	CCP_MPPC_ALONE		0x00000001 /* See RFC 2118, Hi/fn */
#define	CCP_MPPE_LM_40bit	0x00000010 /* obsolute */
#define	CCP_MPPE_NT_40bit	0x00000020
#define	CCP_MPPE_NT_128bit	0x00000040
#define	CCP_MPPE_NT_56bit	0x00000080
#define	CCP_MPPE_STATELESS	0x01000000 /* Packet-by-packet encryption */
#define	CCP_MPPE_KEYLENMASK	0x00000FF0 
#define	CCP_MPPE_HEADER_LEN	4	/* mppe header + protocol */

#define	INADDR_USER_SELECT	(htonl(0xFFFFFFFFL))
#define	INADDR_NAS_SELECT	(htonl(0xFFFFFFFEL))

/** トンネルタイプ定数 */
#define PPP_TUNNEL_NONE		0	/** トンネルタイプ無し */
#define PPP_TUNNEL_L2TP		1	/** L2TP トンネルタイプ */
#define PPP_TUNNEL_PPTP		2	/** PPTP トンネルタイプ */
#define PPP_TUNNEL_PPPOE	3	/** PPPoE トンネルタイプ */

/** デフォルトの LCP ECHO 試行間隔(sec) */
#define DEFAULT_LCP_ECHO_INTERVAL	300

/** デフォルトの LCP ECHO 再試行間隔(sec) */
#define DEFAULT_LCP_ECHO_RETRY_INTERVAL	60

/** デフォルトの LCP ECHO 再試行回数(sec) */
#define DEFAULT_LCP_ECHO_MAX_RETRIES	3

/** MRU に MPPE/CCP ヘッダ分が含まれているかどうか */
/* #define MRU_INCLUDES_MPPE_CCP	 */

/** length for phone number */
#define	NPPPD_PHONE_NUMBER_LEN	32

/**
 * PPP Disconnect Codes based on RFC 3145
 */
typedef enum _npppd_ppp_disconnect_code {
    /*
     * 3.1.  Global Errors
     */
	/** No information available. */
	PPP_DISCON_NO_INFORMATION = 0,

	/** Administrative disconnect. */
	PPP_DISCON_ADMINITRATIVE = 1,

	/**
	 * Link Control Protocol (LCP) renegotiation at LNS disabled; LNS
	 * expects proxy LCP information, LAC did not send it.
	 */
	PPP_DISCON_LCP_RENEGOTIATION_DISABLED = 2,

   	/** Normal Disconnection, LCP Terminate-Request sent. */
	PPP_DISCON_NORMAL = 3,

    /*
     * 3.2.  LCP Errors
     */
	/**
	 * Compulsory encryption required by a PPP peer was refused by the
         * other.
	 */
	PPP_DISCON_COMPULSORY_ENCRYPTION_REQUIRED = 4,

   	/** FSM (Finite State Machine) Timeout error.  (PPP event "TO-".) */
	PPP_DISCON_LCP_FSM_TIMEOUT = 5,

   	/** No recognizable LCP packets were received. */
	PPP_DISCON_RECOGNIZABLE_LCP  = 6,

   	/** LCP failure: Magic Number error; link possibly looped back. */
	PPP_DISCON_LCP_MAGIC_NUMBER_ERROR = 7,

   	/** LCP link failure: Echo Request timeout. */
	PPP_DISCON_LCP_TIMEOUT = 8,

   	/**
	 * Peer has unexpected Endpoint-Discriminator for existing
   	 * Multilink PPP (MP) bundle.
	 */
	PPP_DISCON_LCP_UNEXPECTED_ENDPOINT_DISC = 9,

   	/** Peer has unexpected MRRU for existing MP bundle. */
	PPP_DISCON_LCP_UNEXPECTED_MRRU = 10,

	/**
	 * Peer has unexpected Short-Sequence-Number option for existing
	 * MP bundle.
	 */
	PPP_DISCON_LCP_UNEXPECTED_SHORT_SEQNUM = 11,

   	/**
	 * Compulsory call-back required by a PPP peer was refused by the
	 * other.
	 */
	PPP_DISCON_LCP_COMPULSORY_CALL_BACK_REQUIRED = 12,

    /* 
     * 3.3.  Authentication Errors
     */
	/** FSM Timeout error. */
	PPP_DISCON_AUTH_FSM_TIMEOUT = 13,

	/** Peer has unexpected authenticated name for existing MP bundle. */
	PPP_DISCON_AUTH_UNEXPECTED_AUTH_NAME = 14,

	/**
	 * PPP authentication failure: Authentication protocol
	 * unacceptable.
	 */
	PPP_DISCON_AUTH_PROTOCOL_UNACCEPTABLE= 15,

	/**
	 * PPP authentication failure: Authentication failed (bad name,
	 * password, or secret).
	 */
	PPP_DISCON_AUTH_FAILED = 16,

    /*
     * 3.4.  Network Control Protocol (NCP) Errors
     */
	/** FSM Timeout error. */
	PPP_DISCON_NCP_FSM_TIMEOUT = 17,

   	/**
	 * No NCPs available (all disabled or rejected); no NCPs went to
         * Opened state.  (Control Protocol Number may be zero only if
         * neither peer has enabled NCPs.)
	 */
	PPP_DISCON_NCP_NO_NCP_AVAILABLE = 18,

   	/** NCP failure: failed to converge on acceptable addresses. */
	PPP_DISCON_NCP_NO_ACCEPTABLE_ADDRESS= 19,

   	/** NCP failure: user not permitted to use any addresses. */
	PPP_DISCON_NCP_NO_PERMITTED_ADDRESS = 20
} npppd_ppp_disconnect_code;

typedef struct _npppd_ppp	npppd_ppp;

#include "fsm.h"

#ifdef USE_NPPPD_RADIUS
#include <radius+.h>
#include <radiusconst.h>
#include <radius_req.h>
#endif

/**
 * LCP の型
 */
typedef struct _lcp {
	fsm 		fsm;
	/** LCP オプション */
	struct _opt {
		uint8_t		mru;
		uint8_t		pfc;
		uint8_t		acfc;
		uint8_t		pap;
		uint8_t		chap;
		uint8_t		chapms;
		uint8_t		chapms_v2;
                uint8_t		eap;
	} opt;
	/** 最後の認証方式の提案 */
	uint32_t	lastauth;
	/** マジック番号 */
	uint32_t	magic_number;

	/** 先方のマジック番号 */
	uint32_t	peer_magic_number;

	/** context for event(3) */
    	struct evtimer_wrap timerctx;

	/** echo の送信間隔(秒) */
	int echo_interval;

	/** echo の最大連続失敗回数 */
	int echo_max_retries;

	/** echo reply を待つ時間 */
	int echo_retry_interval;

	/** echo の失敗回数 */
	int echo_failures;

	// NAT/ブラックホール検出のために
	/** 受信した LCP 要求 */
	int8_t		recv_reqs;
	/** 受信した LCP の応答 */
	int8_t		recv_ress;
	/*
	 * Windows 2000 は ConfigReq に MRU は含まれず、送信した ConfigReq
	 * にたいして Nak を返してくる。
	 */
	uint32_t	xxxmru;

	/** 認証メソッド順 */
	int		auth_order[16];

	uint32_t	/** dialin proxy したかどうか */
			dialin_proxy:1,
			/** LCP を再調停するかどうか */
			dialin_proxy_lcp_renegotiation:1;
} lcp;

/**
 * CHAP の型
 */
typedef struct _chap {
	npppd_ppp 	*ppp;
	/** context for event(3) */
    	struct evtimer_wrap timerctx;
	uint32_t	state;
	char		myname[80];
	/** challenge */
	u_char		name[MAX_USERNAME_LENGTH];
	u_char		chall[MAX_CHALLENGE_LENGTH];
	int		lchall;			/* length of challenge */
	u_char		pktid;			/* PPP Packet Id */
	u_char		challid;		/* Id of challange */
	int		type;			/* chap type */
	int		ntry;		
	u_char		authenticator[16];
#ifdef USE_NPPPD_RADIUS
	RADIUS_REQUEST_CTX radctx;
#endif
} chap;

/**
 * PAP の型
 */
typedef struct _pap {
	npppd_ppp	*ppp;
	uint32_t	state;
	u_char		name[MAX_USERNAME_LENGTH];
	int		auth_id;
#ifdef USE_NPPPD_RADIUS
	RADIUS_REQUEST_CTX radctx;
#endif
} pap;

/**
 * EAP
 */
#ifdef USE_NPPPD_EAP_RADIUS
#define PPP_EAP_FLAG_NAK_RESPONSE 0x01
typedef struct _eap {
	npppd_ppp	*ppp;
    	struct evtimer_wrap timerctx;
	uint32_t	state;
	u_char		eapid;
	int 		ntry;		
	u_char		name[MAX_USERNAME_LENGTH];
	u_char		authenticator[16];
/* FIXME */
#define	RADIUS_ATTR_STATE_LEN 100
	int		name_len;
	u_char		attr_state[RADIUS_ATTR_STATE_LEN];
	u_char		attr_state_len;
	unsigned int	session_timeout;
	/* 
	 * nak response 0x01
	 */
	u_char		flags;
	RADIUS_REQUEST_CTX radctx;
} eap;
#endif
           
/**
 * CCP の型
 */
typedef struct _ccp {
	npppd_ppp 	*ppp;
	fsm		fsm;

	uint32_t	mppe_o_bits;
	uint32_t	mppe_p_bits;
	uint		mppe_rej;
} ccp;

/**
 * IPCP の型
 */
typedef	struct _ipcp {
	fsm		fsm;
	npppd_ppp 	*ppp;

	struct in_addr	ip4_our;

	struct in_addr	dns_pri;
	struct in_addr	dns_sec;
	struct in_addr	nbns_pri;
	struct in_addr	nbns_sec;
} ipcp;

/** パケットの recv/send 等デリゲートする際に使う関数ポインタ */
typedef int (*npppd_iofunc) (
	npppd_ppp 	*ppp,
	unsigned char	*bytes,
	int		nbytes,
	int		flags
);

/** オリジナルのパケットは MPPE で暗号化されていたことを示します。 */
#define	PPP_IO_FLAGS_MPPE_ENCRYPTED			0x0001

typedef void (*npppd_voidfunc) (
	npppd_ppp 	*ppp
);

#ifdef	USE_NPPPD_MPPE

typedef struct _mppe_rc4 {
	void		*rc4ctx;

	uint8_t		stateless;
	uint8_t		resetreq;

	/** session key length */
	uint8_t		keylen;
	/** key length in bits */
	uint8_t		keybits;

	/** Cohrency Counter */
	uint16_t	coher_cnt;

	uint8_t		master_key[MPPE_KEYLEN];
	uint8_t		session_key[MPPE_KEYLEN];
} mppe_rc4_t;

/**
 * MPPE の型
 */
typedef struct _mppe {
	npppd_ppp	*ppp;
	uint8_t		master_key[MPPE_KEYLEN];

	uint16_t	pkt_cnt;

	/*
	 * configuration parameters.
	 */
	uint16_t 	enabled		:1,	/* if 0 MPPE しない */
			required	:1,	/* if 1 MPPE なしで通信不可*/
			mode_auto	:1,
			keylen_auto	:1,
			mode_stateless	:1,
			reserved	:11;
	uint16_t	keylenbits;

	mppe_rc4_t	send, recv, keychg;
} mppe;
#endif

/**
 * Type for phone number.  Can be to use as a struct sockaddr. 
 */
typedef struct _npppd_phone_number {
#define	NPPPD_AF_PHONE_NUMBER	(AF_MAX + 0)
	/** total length */
	uint8_t		pn_len;	
	/** address family.  this must be NPPPD_AF_PHONE_NUMBER */
	sa_family_t     pn_family;
	/** phone number */
	char		pn_number[NPPPD_PHONE_NUMBER_LEN + 1];
} npppd_phone_number;

/**
 * npppd_ppp 型。
 */
struct _npppd_ppp {
	npppd 		*pppd;
	int		id;
	/*
	 * 入出力
	 */
	uint8_t		*outpacket_buf;		/** 出力バッファ */
	npppd_iofunc	send_packet;		/** send to phy */
	npppd_iofunc	recv_packet;		/** recv from phy */

	/** アイドルタイムアウトイベント */
	struct event	idle_event;
	/** アイドルタイムアウトの値 */
	int		timeout_sec;
	/*
	 * 物理層
	 */
	int		tunnel_type;		/** PPPトンネルタイプ */
	uint16_t	mru;			/** MRU */
	uint16_t	peer_mru;		/** Peer's MRU */
	void		*phy_context;		/** 物理層のコンテキスト */
	char		phy_label[16];		/** 物理層のラベル */
	union {
		struct sockaddr_in	peer_in;/** {L2TP,PPTP}/IPv4 */
#if defined(USE_NPPPD_PPPOE)
		struct sockaddr_dl	peer_dl;/** PPPoE */
#endif
		npppd_phone_number	peer_pn;/** DialIn */
	} phy_info;				/** 物理層の情報 */
	char		calling_number[NPPPD_PHONE_NUMBER_LEN + 1];
	npppd_voidfunc	phy_close;		/** close line */
	/*
	 * 切断。以下のどのケースでも、phy_close が呼ばれます。
	 *	- PPP側から切断
	 *	- 物理層側から ppp_close
	 *	- 物理層側から ppp_phy_downed
	 *
	 * phy_close が呼ばれた直後に ppp は解放されるので、以後アクセスして
	 * はいけない。
	 */

	/** 認証レルム */
	void *realm;
	/*
	 * プロトコル
	 */
	lcp		lcp;			/** lcp */
	chap		chap;			/** chap */
	pap		pap;			/** pap */
#ifdef USE_NPPPD_EAP_RADIUS
	eap		eap;			/** eap */
#endif
	ccp		ccp;			/** ccp */
	ipcp		ipcp;			/** ccp */

	char		username[MAX_USERNAME_LENGTH];	/** リモートユーザ名 */
	int		ifidx;			/** 集約インタフェースの index*/

	/** Proxy Authen Response */
	u_char		*proxy_authen_resp;
	/** Length of 'Proxy Authen Response' */
	int		lproxy_authen_resp;

	/**
	 * 先方に要求する認証方式
	 * <pre>
	 * PAP		0xC023 
	 * EAP		0xC227 
	 * CHAP		0x0005 
	 * MSCHAP	0x0080 
	 * MSCHAPv2	0x0081 
	 * </pre>
	 */
	uint16_t	peer_auth;		
	u_short		auth_timeout;

#ifdef	USE_NPPPD_MPPE
	uint8_t		mppe_started;
	mppe		mppe;
#endif
	/** 割り当てる/たIPアドレス */
	struct sockaddr_npppd snp;
#define	ppp_framed_ip_address	snp.snp_addr
#define	ppp_framed_ip_netmask	snp.snp_mask
#define	ppp_ip_assigned(p)	(p->ppp_framed_ip_address.s_addr != 0)

	/** 割り当てで使用したプール */
	void		*assigned_pool;

	struct in_addr	realm_framed_ip_address;
	struct in_addr	realm_framed_ip_netmask;

	uint8_t		/** Address and Control Filed があるか */
			has_acf:1,
			/** TCP MSS を MRU 以下に調整するかどうか */
			adjust_mss:1,
			/** 認証は一回限り */
			auth_runonce:1,
			/** PIPEX を使うかどうか */
			use_pipex:1,
			/** PIPEX を開始したかどうか(使えたかどうかは別) */
			pipex_started:1,
			/** PIPEX を使っているかどうか */
			pipex_enabled:1,
			reserved:3;
	uint8_t		/** 動的割り当てかどうか */
			assign_dynapool:1,
			/** 割り当てたアドレスが有効かどうか */
			assigned_ip4_enabled:1,
			assigned_ip4_rcvd:6;

			/** 入力パケットをダンプするか */
	uint8_t		log_dump_in:1,
			/** 出力パケットをダンプするか */
			log_dump_out:1,
			log_rcvd:6;

	uint8_t		/** 生IPが流れたというログを出力済みかどうか */
			logged_naked_ip:1,
			/** 課金開始のログを出力したかどうか */
			logged_acct_start:1,
			/**
			 * Addressフィルードが無いというログを出力済かどうか。
			 */
			logged_no_address:1,
			logged_rcvd:5;
#ifdef	NPPPD_USE_CLIENT_AUTH
/** 端末認証 ID の長さ */
#define	NPPPD_CLIENT_AUTH_ID_MAXLEN		32
	char		client_auth_id[NPPPD_CLIENT_AUTH_ID_MAXLEN + 1];
	int		has_client_auth_id;		
#endif
	/*
	 * 統計情報
	 */
	/** 開始時刻 */
	time_t		start_time;
	/** 開始時刻(相対時間) */
	time_t		start_monotime;
	/** 終了時刻(相対時間) */
	time_t		end_monotime;
	/** 入力パケット数 */
	uint32_t	ipackets;
	/** 出力パケット数 */
	uint32_t	opackets;
	/** 入力エラーパケット数 */
	uint32_t	ierrors;
	/** 出力エラーパケット数 */
	uint32_t	oerrors;
	/** 入力パケットバイト*/
	uint64_t	ibytes;
	/** 出力パケットバイト*/
	uint64_t	obytes;

	/*
	 * Disconnect cause information for RFC3145
	 */
	/** disconnect code */
	npppd_ppp_disconnect_code	disconnect_code;
	/** disconnect control protocol */
	int16_t				disconnect_proto;
	/** disconnect direction */
	int8_t				disconnect_direction;
	/** disconnect message */
	const char			*disconnect_message;
};

/** proxied dialin */
typedef struct _dialin_proxy_info {
	/** Proxied LCP */
	struct proxy_lcp {
		/** Length of the data */
		int ldata;
		/** LCP data */
		u_char data[256];
	}   /** the last sent LCP */ last_sent_lcp,
	    /** the last recevied LCP */ last_recv_lcp;

	/** ID of authentication packet */
	int		auth_id;
	/** authen type.  use same value on npppd_ppp#peer_auth. */
	uint32_t	auth_type;
	/** Username */
	char		username[MAX_USERNAME_LENGTH];
	/** Authentication challenage */
	u_char          auth_chall[MAX_CHALLENGE_LENGTH];
	/** Authentication challenge length */
	int             lauth_chall;
	/** Authentication response */
	u_char          auth_resp[MAX_PASSWORD_LENGTH];
	/** Authentication response length */
	int             lauth_resp;

} dialin_proxy_info;

#define	DIALIN_PROXY_IS_REQUESTED(dpi) \
	(((dpi)->last_sent_lcp.ldata > 0)? 1 : 0)

/** MPPE をネゴすべき */
#define	MPPE_MUST_NEGO(ppp)				\
	(((ppp)->mppe.enabled != 0) &&			\
	(((ppp)->peer_auth == PPP_AUTH_CHAP_MS_V2) || 	\
	((ppp)->peer_auth == PPP_AUTH_EAP))) 
	
/** MPPE 必須 */
#define	MPPE_REQUIRED(ppp) 				\
	(((ppp)->mppe.enabled != 0) && ((ppp)->mppe.required != 0))

/** MPPE 利用可 */
#define	MPPE_READY(ppp) 	((ppp)->mppe_started  != 0)

/* NetBSD:/usr/src/usr.sbin/pppd/pppd/pppd.h より。*/
/*
 * Inline versions of get/put char/short/long.
 * Pointer is advanced; we assume that both arguments
 * are lvalues and will already be in registers.
 * cp MUST be u_char *.
 */
#define GETCHAR(c, cp) { \
	(c) = *(cp)++; \
}
#define PUTCHAR(c, cp) { \
	*(cp)++ = (u_char) (c); \
}

#define GETSHORT(s, cp) { \
	(s) = *(cp)++ << 8; \
	(s) |= *(cp)++; \
}
#define PUTSHORT(s, cp) { \
	*(cp)++ = (u_char) ((s) >> 8); \
	*(cp)++ = (u_char) (s); \
}

#define GETLONG(l, cp) { \
	(l) = *(cp)++ << 8; \
	(l) |= *(cp)++; (l) <<= 8; \
	(l) |= *(cp)++; (l) <<= 8; \
	(l) |= *(cp)++; \
}
#define PUTLONG(l, cp) { \
	*(cp)++ = (u_char) ((l) >> 24); \
	*(cp)++ = (u_char) ((l) >> 16); \
	*(cp)++ = (u_char) ((l) >> 8); \
	*(cp)++ = (u_char) (l); \
}
#define BCOPY(s, d, l)		memcpy(d, s, l)
#define BZERO(s, n)		memset(s, 0, n)

#ifndef	countof
#define	countof(x)	(sizeof(x) / sizeof((x)[0]))
#endif

/*
 * MAKEHEADER - Add Header fields to a packet.
 */
#define MAKEHEADER(p, t) { \
    PUTCHAR(PPP_ALLSTATIONS, p); \
    PUTCHAR(PPP_UI, p); \
    PUTSHORT(t, p); }

/* adapted from FreeBSD:/usr/include/sys/cdefs */
#ifndef __printflike
#if __GNUC__ < 2 || __GNUC__ == 2 && __GNUC_MINOR__ < 7
#define __printflike(fmtarg, firstvararg)
#else
#define __printflike(fmtarg, firstvararg) \
	__attribute__((__format__ (__printf__, fmtarg, firstvararg)))
#endif
#endif

/*
 * MRU は MPPE/CCP ヘッダをカバーするかどうか。
 *
 * RFC 1331:
 *	The Maximum-Receive-Unit covers only the Data Link Layer
 *	Information field.  It does not include the header,
 *	padding, FCS, nor any transparency bits or bytes.
 *
 * Windows XP:
 *	MRU で通知する値と、Windows が TCP MSS として使う値を比較すると、
 *	単純に 40 (IPヘッダとTCPヘッダ) バイトの差だけである。つまり、セ
 *	グメントサイズが MSS ぴったりの TCP パケットを送信すると、その PPP
 *	パケットは、MRU + 4 オクテットとなる。MPPE を使わない場合は、ぴっ
 *	たり MRU となる。
 * 
 * 「MRU は MPPE/CCP ヘッダをカバーする」と実装した場合、このホスト上で、
 *  MRU + 4 とならないような動作を行わねばならないことになる。
 */
#if !defined(USE_NPPPD_MPPE)
/* MPPE 使わない場合は何もしない */
#define MRU_IPMTU(mru)		(mru)
#define MRU_PKTLEN(mru, proto)	(mru)
#else
#ifdef MRU_INCLUDES_MPPE_CCP	
/* MRU は MPPE/CCP ヘッダをカバーする */
#define MRU_IPMTU(mru)		((mru) - CCP_MPPE_HEADER_LEN)
#define MRU_PKTLEN(mru, proto)	(mru)
#else
/* MRU は MPPE/CCP ヘッダをカバーしない */
#define MRU_IPMTU(mru)		(mru)
#define MRU_PKTLEN(mru, proto)	(((proto) == PPP_PROTO_MPPE)	\
	? (mru) + CCP_MPPE_HEADER_LEN : (mru))
#endif
#endif

#define	PPP_FSM_CONFIG(fsm, memb, confl)	\
	(fsm)->memb = ppp_config_int((fsm)->ppp, confl, (fsm)->memb)

#ifdef __cplusplus
extern "C" {
#endif


npppd_ppp    *ppp_create (void);
int          ppp_init (npppd *, npppd_ppp *);
void         ppp_start (npppd_ppp *);
int          ppp_dialin_proxy_prepare (npppd_ppp *, dialin_proxy_info *);
void         ppp_stop (npppd_ppp *, const char *);
void         ppp_stop_ex (npppd_ppp *, const char *, npppd_ppp_disconnect_code, int, int, const char *);
    
void         ppp_destroy (void *);
void         ppp_lcp_up (npppd_ppp *);
void         ppp_lcp_finished (npppd_ppp *);
void         ppp_phy_downed (npppd_ppp *);
void         ppp_auth_ok (npppd_ppp *);
void         ppp_ipcp_opened (npppd_ppp *);
void         ppp_ccp_opened (npppd_ppp *);
inline void  ppp_output (npppd_ppp *, uint16_t, u_char, u_char, u_char *, int);
u_char       *ppp_packetbuf (npppd_ppp *, int);
const char   *ppp_config_str (npppd_ppp *, const char *);
int          ppp_config_int (npppd_ppp *, const char *, int);
int          ppp_config_str_equal (npppd_ppp *, const char *, const char *, int);
int          ppp_config_str_equali (npppd_ppp *, const char *, const char *, int);
int          ppp_log (npppd_ppp *, int, const char *, ...) __printflike(3,4);
void         ppp_reset_idle_timeout(npppd_ppp *);
#ifdef USE_NPPPD_RADIUS
void        ppp_proccess_radius_framed_ip (npppd_ppp *, RADIUS_PACKET *);
int         ppp_set_radius_attrs_for_authreq (npppd_ppp *, radius_req_setting *, RADIUS_PACKET *);
#endif
void         ppp_set_client_auth_id(npppd_ppp *, const char *);

void  	  ccp_init (ccp *, npppd_ppp *);
void      ipcp_init (ipcp *, npppd_ppp *);

void       lcp_init (lcp *, npppd_ppp *);
void       lcp_lowerup (lcp *);
void       lcp_send_protrej(lcp *, u_char *, int );
int        lcp_dialin_proxy(lcp *, dialin_proxy_info *, int, int);

void       pap_init (pap *, npppd_ppp *);
int        pap_start (pap *);
int        pap_stop (pap *);
int        pap_input (pap *, u_char *, int);
int        pap_proxy_authen_prepare (pap *, dialin_proxy_info *);

void       chap_init (chap *, npppd_ppp *);
void       chap_stop (chap *);
void       chap_start (chap *);
void       chap_input (chap *, u_char *, int);
int        chap_proxy_authen_prepare (chap *, dialin_proxy_info *);

#ifdef USE_NPPPD_EAP_RADIUS
void       eap_init __P((eap *, npppd_ppp *));
void       eap_stop __P((eap *));
void       eap_start __P((eap *));
void       eap_input __P((eap *, u_char *, int));
#endif

#ifdef	USE_NPPPD_MPPE
void      mppe_init (mppe *, npppd_ppp *);
void      mppe_fini (mppe *);
void      mppe_start (mppe *);
uint32_t  mppe_create_our_bits (mppe *, uint32_t);
void      mppe_input (mppe *, u_char *, int);
void      mppe_recv_ccp_reset (mppe *);
void      mppe_pkt_output (mppe *, uint16_t, u_char *, int);
#endif


#ifdef __cplusplus
}
#endif
#endif
