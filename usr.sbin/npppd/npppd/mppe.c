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
/* $Id: mppe.c,v 1.3 2010/07/01 03:38:17 yasuoka Exp $ */
/**@file
 *
 * The implementation of MPPE(Microsoft Point-To-Point Encryption Protocol) 
 */
/*
 * To avoid the PPP packet out of sequence problem.
 * It may avoid if it reconstruct the frame order in L2TP/IPsec.
 */
#define	WORKAROUND_OUT_OF_SEQUENCE_PPP_FRAMING	1

#include <sys/types.h>
#include <sys/endian.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <net/if_dl.h>
#include <netinet/in.h>
#include <stdlib.h>
#include <stdio.h>
#include <stdarg.h>
#include <syslog.h>
#include <string.h>
#include <event.h>
#ifdef	WITH_OPENSSL
#include <openssl/sha.h>
#include <openssl/rc4.h>
#endif

#include "slist.h"
#include "npppd.h"
#include "debugutil.h"

#ifdef	MPPE_DEBUG
#define	MPPE_DBG(x)	mppe_log x
#define	MPPE_ASSERT(x)	\
	if (!(x)) { \
	    fprintf(stderr, \
		"\nASSERT(%s) failed on %s() at %s:%d.\n" \
		, #x, __func__, __FILE__, __LINE__); \
	    abort(); \
	}
#else
#define	MPPE_DBG(x)	
#define MPPE_ASSERT(x)
#endif

#define	SESS_KEY_LEN(len)	(len < 16)?		8 : 16

static const char  *mppe_bits_to_string __P((uint32_t));
static void        mppe_log __P((mppe *, uint32_t, const char *, ...)) __printflike(3,4);
static int         rc4_key __P((mppe *, mppe_rc4_t *, int, u_char *));
static void        rc4_destroy __P((mppe *, mppe_rc4_t *));
static void        rc4 __P((mppe *, mppe_rc4_t *, int, u_char *, u_char *));
static void        GetNewKeyFromSHA __P((u_char *, u_char *, int, u_char *));

/**
 * initializing mppe context.
 * 	- reading configuration.
 */
void
mppe_init(mppe *_this, npppd_ppp *ppp)
{
	const char *sval;
	int ival;

	MPPE_ASSERT(ppp != NULL);
	MPPE_ASSERT(_this != NULL);

	memset(_this, 0, sizeof(mppe));

	_this->ppp = ppp;

	_this->mode_auto = 1;
	_this->mode_stateless = 0;
	_this->keylen_auto = 1;
	_this->keylenbits = 128;

	_this->enabled = (ppp_config_str_equal(_this->ppp,
	    "mppe.disabled", "true", 0) != 0)?  0 : 1;

	if (_this->enabled == 0)
		goto mppe_config_done;

	_this->required = (ppp_config_str_equal(_this->ppp,
	    "mppe.required", "true", 0) != 0)?  1 : 0;

	if (_this->required == 0)
		goto mppe_config_done;

	sval = ppp_config_str(_this->ppp, "mppe.mode");
	if (sval != NULL) {
		if (strcmp(sval, "stateless") == 0) {
			_this->mode_auto = 0;
			_this->mode_stateless = 1;
		} else if (strcmp(sval, "stateful") == 0) {
			_this->mode_auto = 0;
			_this->mode_stateless = 0;
		} else if (strcmp(sval, "auto") == 0 ||
		    strcmp(sval, "*") == 0) {
			/* no changes from default. */
		} else {
			mppe_log(_this, LOG_WARNING,
			    "configuration \"mppe.mode\" has bad value");
			_this->mode_auto = 1;
			_this->mode_stateless = 0;
		}
	}
	if (ppp_config_str_equal(_this->ppp, "mppe.keylen", "auto", 0) ||
	    ppp_config_str_equal(_this->ppp, "mppe.keylen", "*", 0)) {
		/* no changes from default. */
	} else {
		ival = ppp_config_int(_this->ppp, "mppe.keylen", -1);
		if (ival != -1) {
			switch (ival) {
			case 40:
			case 56:
			case 128:
				_this->keylenbits = ival;
				_this->keylen_auto = 0;
				break;
			default:
				mppe_log(_this, LOG_WARNING,
				    "configuration \"mppe.keylen\" has bad "
				    "value");
			}
		}
	}
mppe_config_done:
	/* nothing */;
}

void
mppe_fini(mppe *_this)
{
	rc4_destroy(_this, &_this->send);
	rc4_destroy(_this, &_this->recv);
	rc4_destroy(_this, &_this->keychg);
}

static void
mppe_reduce_key(mppe_rc4_t *_this)
{
	switch (_this->keybits) {
	case 40:
		_this->session_key[1] = 0x26;
		_this->session_key[2] = 0x9e;
	case 56:
		_this->session_key[0] = 0xd1;
	}
}

static void
mppe_key_change(mppe *_mppe, mppe_rc4_t *_this)
{
	u_char interim[16];

	GetNewKeyFromSHA(_this->master_key, _this->session_key,
	    _this->keylen, interim);

	rc4_key(_mppe, &_mppe->keychg, _this->keylen, interim);
	rc4(_mppe, &_mppe->keychg, _this->keylen, interim, _this->session_key);
	mppe_reduce_key(_this);
}

/**
 * starting mppe protocol.
 */
void
mppe_start(mppe *_this)
{
	char buf[256];

	strlcpy(buf, mppe_bits_to_string(_this->ppp->ccp.mppe_o_bits),
	    sizeof(buf));

	mppe_log(_this, LOG_INFO, "logtype=Opened our=%s peer=%s", buf,
	    mppe_bits_to_string(_this->ppp->ccp.mppe_p_bits));

	_this->ppp->mppe_started = 1;

	_this->send.stateless =
	    ((_this->ppp->ccp.mppe_o_bits & CCP_MPPE_STATELESS) != 0)? 1 : 0;

	if ((_this->ppp->ccp.mppe_o_bits & CCP_MPPE_NT_40bit) != 0) {
		_this->send.keylen = 8;
		_this->send.keybits = 40;
	} else if ((_this->ppp->ccp.mppe_o_bits & CCP_MPPE_NT_56bit) != 0) {
		_this->send.keylen = 8;
		_this->send.keybits = 56;
	} else if ((_this->ppp->ccp.mppe_o_bits & CCP_MPPE_NT_128bit) != 0) {
		_this->send.keylen = 16;
		_this->send.keybits = 128;
	}

	_this->recv.stateless =
	    ((_this->ppp->ccp.mppe_p_bits & CCP_MPPE_STATELESS) != 0)? 1 : 0;
	if ((_this->ppp->ccp.mppe_p_bits & CCP_MPPE_NT_40bit) != 0) {
		_this->recv.keylen = 8;
		_this->recv.keybits = 40;
	} else if ((_this->ppp->ccp.mppe_p_bits & CCP_MPPE_NT_56bit) != 0) {
		_this->recv.keylen = 8;
		_this->recv.keybits = 56;
	} else if ((_this->ppp->ccp.mppe_p_bits & CCP_MPPE_NT_128bit) != 0) {
		_this->recv.keylen = 16;
		_this->recv.keybits = 128;
	}

	GetNewKeyFromSHA(_this->recv.master_key, _this->recv.master_key,
	    _this->recv.keylen, _this->recv.session_key);
	GetNewKeyFromSHA(_this->send.master_key, _this->send.master_key,
	    _this->send.keylen, _this->send.session_key);

	mppe_reduce_key(&_this->recv);
	mppe_reduce_key(&_this->send);

	rc4_key(_this, &_this->recv, _this->recv.keylen,
	    _this->recv.session_key);
	rc4_key(_this, &_this->send, _this->send.keylen,
	    _this->send.session_key);
}


/**
 * creating the mppe bits. In case of first proposal, it specifies the 
 * peer_bits as 0 value. If it specifies the peer_bits, it returns the
 * value as peer's proposal.
 */
uint32_t
mppe_create_our_bits(mppe *_this, uint32_t peer_bits)
{
	uint32_t our_bits;
	/* default proposal */
	our_bits = CCP_MPPE_NT_128bit;

	if (_this->keylen_auto == 0) {
		switch (_this->keylenbits) {
		case 40:
			our_bits = CCP_MPPE_NT_40bit; break;
		case 56:
			our_bits = CCP_MPPE_NT_56bit; break;
		case 128:
			our_bits = CCP_MPPE_NT_128bit; break;
		}
	} else {
		/* auto */
		our_bits = CCP_MPPE_NT_128bit | CCP_MPPE_NT_56bit
			| CCP_MPPE_NT_40bit;
		if (peer_bits != 0) {
			if ((peer_bits & CCP_MPPE_NT_128bit) != 0)
				our_bits = CCP_MPPE_NT_128bit;
			else if ((peer_bits & CCP_MPPE_NT_56bit) != 0)
				our_bits = CCP_MPPE_NT_56bit;
			else if ((peer_bits & CCP_MPPE_NT_40bit) != 0)
				our_bits = CCP_MPPE_NT_40bit;
		}
	}

	if (_this->mode_auto != 0) {
		/* in case of auto_mode */
		if (peer_bits == 0) {
			/*
			 * It proposes stateless mode in first time. Windows 9x has
			 * a bug that it is reverse to stateful and stateless in 
			 * sending and receiving packets.
			 * Windows 9x is prior to negotiate in stateless mode, so
			 * it will avoid the Windows bug to be prior to negotiate 
			 * in stateless mode.
			 *
			 * Even if this bug doesn't exists, the stateful mode is high
			 * cost from user's viewpoint when packets may loss more than a
			 * certain rate, so it is not good choice to use via Internet or
			 * wireless LAN.
			 */
			our_bits |= CCP_MPPE_STATELESS;
		} else {
			/* giving up */
			our_bits |= peer_bits & CCP_MPPE_STATELESS;
		}
	} else {
		/* it doesn't give up in case of setting non-auto value. */
		if (_this->mode_stateless != 0)
			our_bits |= CCP_MPPE_STATELESS;
	}
	if (peer_bits != 0 && our_bits != peer_bits) {
		char obuf[128], pbuf[128];

		/* in case of failure, it puts a log. */
		strlcpy(obuf, mppe_bits_to_string(our_bits), sizeof(obuf));
		strlcpy(pbuf, mppe_bits_to_string(peer_bits), sizeof(pbuf));
		mppe_log(_this, LOG_INFO,
		    "mismatch our=%s peer=%s", obuf, pbuf);
	}

	return our_bits;
}

#define	RESET_REQ	0x0e
#define	RESET_ACK	0x0f
#define	COHRENCY_CNT_MASK	0x0fff;

/**
 * receiving packets via MPPE.
 */
void
mppe_input(mppe *_this, u_char *pktp, int len)
{
	int pktloss, encrypt, flushed, m, n;
	uint16_t coher_cnt;
	u_char *pktp0, *opktp, *opktp0;

	encrypt = 0;
	flushed = 0;

	MPPE_ASSERT(len >= 2);

	pktp0 = pktp;
	GETSHORT(coher_cnt, pktp);

	flushed = (coher_cnt & 0x8000)? 1 : 0;
	encrypt = (coher_cnt & 0x1000)? 1 : 0;
	coher_cnt &= COHRENCY_CNT_MASK;
	pktloss = 0;

	MPPE_DBG((_this, DEBUG_LEVEL_2, "in coher_cnt=%03x/%03x %s%s",
	    _this->recv.coher_cnt, coher_cnt, (flushed)? "[flushed]" : "",
	    (encrypt)? "[encrypt]" : ""));

	if (encrypt == 0) {
		mppe_log(_this, LOG_WARNING,
		    "Received unexpected MPPE packet.  (no ecrypt)");
		return;
	}
#ifdef	WORKAROUND_OUT_OF_SEQUENCE_PPP_FRAMING
	/*
	 * In L2TP/IPsec implementation, in case that the ppp frame sequence
	 * is not able to reconstruct and the ppp frame is out of sequence, it
	 * is unable to identify with many packets losing. If it does so, MPPE 
	 * key is out of place.
	 * To avoid this problem, when it seems that more than 4096-256 packets
	 * drops, it assumes that the packet doesn't lose but the packet is out
	 * of sequence.
	 */
    {
	int coher_cnt0;

	coher_cnt0 = coher_cnt;
	if (coher_cnt < _this->recv.coher_cnt)
		coher_cnt0 += 0x1000;
	if (coher_cnt0 - _this->recv.coher_cnt > 0x0f00) {
		mppe_log(_this, LOG_INFO,
		    "Workaround the out-of-sequence PPP framing problem: "
		    "%d => %d", _this->recv.coher_cnt, coher_cnt);
		return;
	}
    }
#endif
	if (_this->recv.stateless != 0) {
		mppe_key_change(_this, &_this->recv);
		while (_this->recv.coher_cnt != coher_cnt) {
			mppe_key_change(_this, &_this->recv);
			_this->recv.coher_cnt++;
			_this->recv.coher_cnt &= COHRENCY_CNT_MASK;
			pktloss++;
		}
		flushed = 1;
	} else {
		if (flushed) {
			if (coher_cnt < _this->recv.coher_cnt) {
				/* in case of carrying up. */
				coher_cnt += 0x1000;
			}
			pktloss += coher_cnt - _this->recv.coher_cnt;
			m = _this->recv.coher_cnt / 256;
			n = coher_cnt / 256;
			while (m++ < n)
				mppe_key_change(_this, &_this->recv);

			coher_cnt &= COHRENCY_CNT_MASK;
			_this->recv.coher_cnt = coher_cnt;
		} else if (_this->recv.coher_cnt != coher_cnt) {
			_this->recv.resetreq = 1;

			opktp0 = ppp_packetbuf(_this->ppp,
			    PPP_PROTO_NCP | NCP_CCP);
			opktp = opktp0;

			PUTLONG(_this->ppp->ccp.mppe_p_bits, opktp);

			ppp_output(_this->ppp, PPP_PROTO_NCP | NCP_CCP,
			    RESET_REQ, _this->recv.resetreq, opktp0,
				opktp - opktp0);
			return;
		}
		if ((coher_cnt & 0xff) == 0xff) {
			mppe_key_change(_this, &_this->recv);
			flushed = 1;
		}
	}
#ifndef	WORKAROUND_OUT_OF_SEQUENCE_PPP_FRAMING
	if (pktloss > 1000) {
		/*
		 * In case of many packets losing or out of sequence.
		 * The latter is not able to communicate because the key is 
		 * out of place soon.
		 * 
		 */
		mppe_log(_this, LOG_WARNING, "%d packets loss", pktloss);
	}
#endif
	if (flushed) {
		rc4_key(_this, &_this->recv, _this->recv.keylen,
		    _this->recv.session_key);
	}

	rc4(_this, &_this->recv, len - 2, pktp, pktp);

	_this->recv.coher_cnt++;
	_this->recv.coher_cnt &= COHRENCY_CNT_MASK;

	_this->ppp->recv_packet(_this->ppp, pktp, len - 2,
	    PPP_IO_FLAGS_MPPE_ENCRYPTED);
}

/**
 * The call out function in case of receiving CCP Reset (key reset in case
 * of MPPE).
 */
void
mppe_recv_ccp_reset(mppe *_this)
{
	MPPE_DBG((_this, DEBUG_LEVEL_2, "%s() is called.", __func__));
	_this->send.resetreq = 1;
}

/**
 * sending packet via MPPE.
 */
void
mppe_pkt_output(mppe *_this, uint16_t proto, u_char *pktp, int len)
{
	int encrypt, flushed;
	uint16_t coher_cnt;
	u_char *outp, *outp0;

	MPPE_ASSERT(proto == PPP_PROTO_IP);

	flushed = 0;
	encrypt = 1;

	outp = ppp_packetbuf(_this->ppp, PPP_PROTO_MPPE);
	outp0 = outp;

	if (_this->send.stateless != 0) {
		flushed = 1;
		mppe_key_change(_this, &_this->send);
	} else {
		if ((_this->send.coher_cnt % 0x100) == 0xff) {
			flushed = 1;
			mppe_key_change(_this, &_this->send);
		} else if (_this->send.resetreq != 0) {
			flushed = 1;
			_this->send.resetreq = 0;
		}
	}

	if (flushed) {
		rc4_key(_this, &_this->send, _this->send.keylen,
		    _this->send.session_key);
	}

	MPPE_DBG((_this, DEBUG_LEVEL_2, "out coher_cnt=%03x %s%s",
	    _this->send.coher_cnt, (flushed)? "[flushed]" : "",
	    (encrypt)? "[encrypt]" : ""));

	coher_cnt = _this->send.coher_cnt & COHRENCY_CNT_MASK;
	if (flushed)
		coher_cnt |= 0x8000;
	if (encrypt)
		coher_cnt |= 0x1000;

	PUTSHORT(coher_cnt, outp);
	proto = htons(proto);
	rc4(_this, &_this->send, 2, (u_char *)&proto, outp);
	rc4(_this, &_this->send, len, pktp, outp + 2);

	ppp_output(_this->ppp, PPP_PROTO_MPPE, 0, 0, outp0, len + 4);
	_this->send.coher_cnt++;
	_this->send.coher_cnt &= COHRENCY_CNT_MASK;
}

static void
mppe_log(mppe *_this, uint32_t prio, const char *fmt, ...)
{
	char logbuf[BUFSIZ];
	va_list ap;

	va_start(ap, fmt);
	snprintf(logbuf, sizeof(logbuf), "ppp id=%u layer=mppe %s",
	    _this->ppp->id, fmt);
	vlog_printf(prio, logbuf, ap);
	va_end(ap);
}

static const char *
mppe_bits_to_string(uint32_t bits)
{
	static char buf[128];

	snprintf(buf, sizeof(buf), "%s%s%s%s%s%s"
	, ((CCP_MPPC_ALONE & bits) != 0)?	",mppc" : ""
	, ((CCP_MPPE_LM_40bit& bits) != 0)?	",40bit(LM)" : ""
	, ((CCP_MPPE_NT_40bit& bits) != 0)?	",40bit" : ""
	, ((CCP_MPPE_NT_128bit& bits) != 0)?	",128bit" : ""
	, ((CCP_MPPE_NT_56bit& bits) != 0)?	",56bit" : ""
	, ((CCP_MPPE_STATELESS& bits) != 0)?	",stateless" : ",stateful");
	
	if (buf[0] == '\0')
		return "";

	return buf + 1;
}

/************************************************************************
 * implementations of authentication/cipher algorism.
 ************************************************************************/
static u_char SHAPad1[] = {
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
}, SHAPad2[] = {
	0xf2, 0xf2, 0xf2, 0xf2, 0xf2, 0xf2, 0xf2, 0xf2,
	0xf2, 0xf2, 0xf2, 0xf2, 0xf2, 0xf2, 0xf2, 0xf2,
	0xf2, 0xf2, 0xf2, 0xf2, 0xf2, 0xf2, 0xf2, 0xf2,
	0xf2, 0xf2, 0xf2, 0xf2, 0xf2, 0xf2, 0xf2, 0xf2,
	0xf2, 0xf2, 0xf2, 0xf2, 0xf2, 0xf2, 0xf2, 0xf2,
};
#define	ZeroMemory(dst, len)		memset(dst, 0, len)
#define	MoveMemory(dst, src, len)	memcpy(dst, src, len)

#include <openssl/rc4.h>
#include <openssl/sha.h>

#define	SHA_CTX			SHA_CTX
#define	SHAInit			SHA1_Init
#define	SHAUpdate		SHA1_Update
#define	SHAFinal(ctx,digest)	SHA1_Final(digest, ctx)

/************************************************************************
 * implementations of OpenSSL version
 ************************************************************************/

static int
rc4_key(mppe *_mppe, mppe_rc4_t *_this, int lkey, u_char *key)
{
	if (_this->rc4ctx == NULL) {
		if ((_this->rc4ctx = malloc(sizeof(RC4_KEY))) == NULL) {
			mppe_log(_mppe, LOG_ERR, "malloc() failed at %s: %m",
			    __func__);
			return 1;
		}
	}

	RC4_set_key((RC4_KEY *)_this->rc4ctx, lkey, key);

	return 0;
}

static void
rc4(mppe *_mppe, mppe_rc4_t *_this, int len, u_char *indata, u_char *outdata)
{
	RC4((RC4_KEY *)_this->rc4ctx, len, indata, outdata);
}

static void
rc4_destroy(mppe *_mppe, mppe_rc4_t *_this)
{
	if (_this->rc4ctx != NULL)
		free(_this->rc4ctx);
	_this->rc4ctx = NULL;
}

static void
GetNewKeyFromSHA(u_char *StartKey, u_char *SessionKey, int SessionKeyLength,
    u_char *InterimKey)
{
	u_char Digest[20];
	SHA_CTX Context;

	ZeroMemory(Digest, 20);

	SHAInit(&Context);
	SHAUpdate(&Context, StartKey, SessionKeyLength);
	SHAUpdate(&Context, SHAPad1, 40);
	SHAUpdate(&Context, SessionKey, SessionKeyLength);
	SHAUpdate(&Context, SHAPad2, 40);
	SHAFinal(&Context, Digest);

	MoveMemory(InterimKey, Digest, SessionKeyLength);
}
