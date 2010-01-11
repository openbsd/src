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
 *
 * EAP Pass-through Authenticator の実装。
 *
 * @see RFC3748
 *	    Extensible Authentication Protocols(EAP)
 * @see RFC3579
 *	    RADIUS (Remote Authentication Dial In User Service) Support For
 *	    Extensible Authentication Protocol (EAP). B. Aboba, P. Calhoun.
 */
// $Id: eap.c,v 1.1 2010/01/11 04:20:57 yasuoka Exp $

/* FIXME: コメント/ログの意味がわからない */

#include <sys/types.h>
#include <sys/param.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <netinet/in.h>
#include <net/if_dl.h>
#include <stdlib.h>
#include <stdio.h>
#include <syslog.h>
#include <string.h>
#include <unistd.h>
#include <stdarg.h>
#include <time.h>
#include <event.h>

#ifdef USE_NPPPD_RADIUS
#include <radius+.h>
#include <radiusconst.h>
#endif

#include "debugutil.h"
#ifdef USE_NPPPD_RADIUS
#include "radius_chap_const.h"
#endif
#include "npppd_local.h"
#include "chap_ms.h"

/* initital state */
#define EAP_STATE_INITIAL		1
#define EAP_STATE_SEND_REQUEST_TO_PEER	2
#define EAP_STATE_STOPPED		3

#define	EAP_HEADERLEN	4

#define	EAP_TIMEOUT_INIT	3	/* 初期リトライ間隔 */
#define	EAP_TIMEOUT_MAX		20	/* 最大リトライ間隔 */
#define	EAP_RETRY		4	/* リトライ回数 */

#define	EAP_REQUEST	1
#define	EAP_RESPONSE	2
#define	EAP_SUCCESS	3
#define	EAP_FAILURE	4

/* MprError.h */
#define	ERROR_AUTH_SERVER_TIMEOUT		930

#define EAP_DEBUG
#ifdef	EAP_DEBUG
#define	EAP_DBG(x)	eap_log x 
#define	EAP_ASSERT(cond)					\
	if (!(cond)) {						\
	    fprintf(stderr,					\
		"\nASSERT(" #cond ") failed on %s() at %s:%d.\n"\
		, __func__, __FILE__, __LINE__);		\
	    abort(); 						\
	}
#else
#define	EAP_ASSERT(cond)			
#define	EAP_DBG(x)	
#endif

#define TMPBUF		256
#define IDENT_STRING 	"What your name?"
#define TIMER_CBFUNC	void(*)(void *)

#define INIT_EAPID	1

static void eap_restart(eap *_this);
static void eap_forward_to_radius(eap *_this, u_int8_t *data, int datalen);
static void eap_recv_from_radius(void *context, RADIUS_PACKET *pkt, int flags);
static int eap_forward_to_peer(eap *_this, u_int8_t *data, int datalen, int type, u_int8_t id);
static void eap_log(eap *_this, uint32_t prio, const char *fmt, ...) __printflike(3,4);
#ifdef USE_NPPPD_MPPE
static int get_mppe_keys(eap *_this, RADIUS_PACKET *pkt, const char *secret);
#endif

/**
 * {@link ::_eap EAPインスタンス}を初期化します。
 */
void
eap_init(eap *_this, npppd_ppp *ppp)
{
	EAP_ASSERT(ppp != NULL);
	EAP_ASSERT(_this != NULL);
        /* initiallize */
	memset(_this, 0, sizeof(eap));
	_this->ntry = EAP_RETRY;
	_this->ppp = ppp;
        _this->state = EAP_STATE_INITIAL;
}

/**
 * 認証者として、EAPを開始します。Identity Requestを投げます。
 */
void
eap_start(eap *_this)
{
        u_int8_t *req,*req0;
        int len;

	EAP_ASSERT(_this != NULL);
	EAP_ASSERT(_this->ppp != NULL);

	/* 
	 * initialize for timeout callback 
	 */
	_this->name_len = 0;
	memset(_this->name, 0, sizeof(_this->name));
	_this->attr_state_len = 0;
	memset(_this->attr_state, 0, RADIUS_ATTR_STATE_LEN);

	if (_this->state == EAP_STATE_INITIAL ||
            _this->state == EAP_STATE_SEND_REQUEST_TO_PEER){ 
		if (_this->ntry > 0) {
			_this->ntry--;

                        /* eap header
                         * code:      1 (request)     [1 byte]
                         * ID:        0x01 (sequence) [1 byte]
                         * length:    ?               [2 byte]
			 */

			/*
                         * type:      Identity        [1 byte]
                         * data:      data            [  ?   ]
                        */

			req = ppp_packetbuf(_this->ppp, PPP_AUTH_EAP);
			req += PPP_HDRLEN;
			req0 = req;

                        PUTCHAR(PPP_AUTH_EAP_IDENTITY, req);
                        BCOPY(IDENT_STRING, req, (len = strlen(IDENT_STRING)));
                        req += strlen(IDENT_STRING);

			if (_this->eapid == 0)
				_this->eapid = INIT_EAPID;
			else 
				_this->eapid++;

                        /*                  
                         * send eap request
                         */                 
			ppp_output(_this->ppp, PPP_PROTO_EAP, EAP_REQUEST,
			    _this->eapid, req0, req - req0);
			_this->state =  EAP_STATE_SEND_REQUEST_TO_PEER;

			TIMEOUT((TIMER_CBFUNC)eap_restart, _this,
			    EAP_TIMEOUT_INIT);
		} else {
			eap_log(_this, LOG_NOTICE,
			    "Client didn't respond our EAP request");
			eap_stop(_this);
			ppp_stop(_this->ppp, "Authentication Required");
		}
	}
}

void
eap_restart(eap *_this) {
	if (_this == NULL) {
		log_printf(LOG_INFO, "Failed restart authentication, "
		    "already peer session closed with eap");
		return;
	}

	eap_log(_this, LOG_INFO, "Retry authentication");
	_this->name_len = 0;
	_this->attr_state_len = 0;
	memset(_this->name, 0, sizeof(_this->name));
	memset(_this->attr_state, 0, sizeof(_this->attr_state));
	if (_this->radctx != NULL)
		radius_cancel_request(_this->radctx);

	eap_start(_this);
}

void 
eap_input(eap *_this, unsigned char *pktp, int len){
        u_int8_t *pkthp;
        int code, id, length, type;

	if (_this->state == EAP_STATE_INITIAL ||
	    _this->state == EAP_STATE_STOPPED) {
		eap_log(_this, LOG_INFO, "Received eap packet.  But eap is "
		    "not started");
		return;
	}
        pkthp = pktp;
        
        UNTIMEOUT(eap_restart, _this);

	if(len < EAP_HEADERLEN + 1){
		/* discard */
               	eap_log(_this, LOG_NOTICE, "Packet has unexpect length");
		return;
	}

        GETCHAR(code, pkthp);
	if (code == EAP_FAILURE) {
		/* discard */
		eap_log(_this, LOG_NOTICE,
		    "Recieved unexpected packet from peer (code = %d)", code);
		return;
	}
       
       	GETCHAR(id, pkthp);
        if (id != _this->eapid) {
		/* discard */
		eap_log(_this, LOG_NOTICE,
		    "Not match EAP identifier (request = %d, response = %d)",
		    _this->eapid, id);
		return;
	}

	/*
	 * get user name  from itentity response
	 */ 
       	GETSHORT(length, pkthp);
       	GETCHAR(type, pkthp);
	if (type == PPP_AUTH_EAP_IDENTITY && _this->name_len == 0) {
       		if (length != len) {
			/* discard */
               		eap_log(_this, LOG_NOTICE,
			    "Identity packet has Invalid length");
			return;
          	} else {
            		_this->name_len = length - ( EAP_HEADERLEN + 1 );
              		if (_this->name_len <= MAX_USERNAME_LENGTH) {
                      		memcpy(_this->name, pkthp, _this->name_len);
				_this->name[_this->name_len] = '\0';
	    		} else {
				/* discard */
                       		_this->name_len = 0;
				eap_log(_this, LOG_ERR,
				    "Identity name is too long");
				return;				
	    		}
        	}
	}

	if (type == PPP_AUTH_EAP_NAK){
		/* 
		 * Nak check
		 */
		_this->flags |= PPP_EAP_FLAG_NAK_RESPONSE;
		eap_log(_this, LOG_DEBUG, "peer response is nak");
	}
 
	if(_this->name_len != 0){
		eap_forward_to_radius(_this, pktp, len);
		return;
	}

	/* unexpected process
         * discard
         */ 
	eap_log(_this, LOG_DEBUG, 
	    "recieve eap length = %d, "
	    "eap info: code = %d, id = %d, length = %d, type = %d, "
	    "name length = %d",
	    len, code, id, length, type, _this->name_len );
        eap_log(_this, LOG_NOTICE, "Recieved unexpected eap packet from peer");
	return;
}

static void
eap_forward_to_radius(eap *_this, u_int8_t *data, int datalen)
{
	RADIUS_REQUEST_CTX radctx;
        RADIUS_PACKET *radpkt = NULL;
        const char *secret;
        int secretlen;
        int rlength = datalen;
	radius_req_setting *rad_setting;
	int retry = 0;
	unsigned int timeout;
	char buf0[MAX_USERNAME_LENGTH];

	/* FIXME: 毎度やる必要なし */
	if (npppd_ppp_bind_realm(_this->ppp->pppd, _this->ppp, _this->name, 1)
	    != 0) {
		/* 
		 * internal error 
		 * retry
		 */
		eap_log(_this, LOG_ERR, "Not found realm");
		retry = 1;
		goto reigai;
	}

	if (npppd_ppp_is_realm_radius(
	    _this->ppp->pppd, _this->ppp) == 0) {
		/*
		 * internal error 
		 * retry 
		 */
		eap_log(_this, LOG_ERR, "Not found realm");
		retry = 1;
		goto reigai;
	}

	if ((rad_setting = npppd_get_radius_req_setting(
	    _this->ppp->pppd, _this->ppp)) == NULL) {
		/*
		 * internal error 
		 * retry  
		 */
		eap_log(_this, LOG_ERR, "Not found radius server setting");
		retry = 1;
		goto reigai;
	}

        /*                          
         * make  new request packet
         */                          
        if ((radpkt = radius_new_request_packet(RADIUS_CODE_ACCESS_REQUEST))
	    == NULL){
		/* 
		 * internal error 
		 * retry 
		 */
		eap_log(_this, LOG_ERR, "Can't make new request packet");
		retry = 1;
		goto reigai;
        }

	if (ppp_set_radius_attrs_for_authreq(_this->ppp, rad_setting, radpkt)
	    != 0) {
		/*
		 * internal error 
		 * retry  
		 */
		retry = 1;
		goto reigai;
	}

	/* avoid EAP fragmentation */
	if (radius_put_uint32_attr(radpkt, RADIUS_TYPE_FRAMED_MTU,
		_this->ppp->mru) != 0) {
		/*
		 * internal error 
		 * retry  
		 */
		retry = 1;
		goto reigai;
	}

        /*                                    
         * set user name attribute 
         */ 
	if (_this->name_len != 0) {
		if (radius_put_string_attr(radpkt, RADIUS_TYPE_USER_NAME, 
		    npppd_ppp_get_username_for_auth(_this->ppp->pppd,
			    _this->ppp, _this->name, buf0)) != 0) {
			/* 
			 * internal error
			 * retry
			 */
			eap_log(_this, LOG_ERR, 
			    "Can't put attribute to radius packet. type = %d", 
			    RADIUS_TYPE_USER_NAME);
			retry = 1;
			goto reigai;
		}
	} else {
		/*
		 * none Identity 
		 * discard 
		 */
		eap_log(_this, LOG_NOTICE, "Identity name is not seted");
		goto reigai;
	}

	/*
	 * set state attribute
	 */
	if (_this->attr_state_len != 0) {
		if (radius_put_raw_attr(radpkt, 
		    RADIUS_TYPE_STATE, 
		    _this->attr_state, 
		    _this->attr_state_len) != 0) {
			/* 
			 * internal error
			 * discard
			 */ 
			eap_log(_this, LOG_ERR, 
			    "Can't put attribute to radius packet. type = %d", 
			    RADIUS_TYPE_STATE);
			goto reigai;
		}
	}

	/*
	 * set EAP message attribute
	 * radius packet has some eap message attribute
	 */
	while (rlength > 0) {
		if (rlength > 253) {
			if (radius_put_raw_attr(radpkt, 
			    RADIUS_TYPE_EAP_MESSAGE, 
			    data+(datalen-rlength), 253) != 0) {
				/* 
				 * internal error 
				 * retry 
				 */
				eap_log(_this, LOG_ERR, 
				    "Can't put attribute to radius packet.  "
				    "type = %d", RADIUS_TYPE_EAP_MESSAGE);
				retry = 1;
				goto reigai;
			}
			rlength -= 253;
		} else {
			if (radius_put_raw_attr(radpkt, 
		    	    RADIUS_TYPE_EAP_MESSAGE, 
		    	    data+(datalen-rlength), 
		    	    rlength) != 0) {
				/*
				 * internal error
				 * retry
				 */ 
				eap_log(_this, LOG_ERR, 
			    	    "Can't put attribute to radius packet.  "
				    "type = %d", RADIUS_TYPE_EAP_MESSAGE);
				retry = 1;
                     		goto reigai;
                 	}
                 	rlength -= rlength;
		}
	}

	/*
	 * request cancel
	 */
	if (_this->radctx != NULL)
		radius_cancel_request(_this->radctx);

	/* 
	 * prepare request 
	 */
	if (_this->session_timeout != 0) 
/* FIXME: 認証タイムアウトと独立したタイマーが必要なのか? */
		timeout = _this->session_timeout/2;
	else
		timeout = _this->ppp->auth_timeout;
        if (radius_prepare(rad_setting, _this, &radctx,
	    eap_recv_from_radius, timeout) != 0) {
		/* 
		 * internal error 
		 * retry
		 */
		eap_log(_this, LOG_ERR, "Can't prepare to send access request "
		    "packet to radius");
		if (!npppd_ppp_is_realm_ready(_this->ppp->pppd, _this->ppp)) {
			eap_log(_this, LOG_ERR,
			    "radius server setting is not ready");
		}
		retry  = 1;
		goto reigai;
	}
	_this->radctx = radctx;

	/*                        
	 * get secret password  
         *     for radius         
         */
        secret = radius_get_server_secret(_this->radctx);
        secretlen = strlen(secret);

        /*                                      
         * set message authenticator attribute  
         */
	if (radius_put_message_authenticator(radpkt, secret) != 0) {
		eap_log(_this, LOG_ERR, "couldn't put message authentication "
		    "attribute to radius packet");
		retry = 1;
		goto reigai;
	}

	radius_get_authenticator(radpkt, _this->authenticator);

	/* 
	 * send request 
	 */
        radius_request(_this->radctx, radpkt);
        return;
reigai:
	/*
         *  don't give peer user infomation
         */
	if (radpkt != NULL)
		radius_delete_packet(radpkt);
        eap_log(_this, LOG_NOTICE, "Can't forward packet to radius from peer");
	if (retry) { 
		eap_restart(_this);
	}
        return;
}

static void
eap_recv_from_radius(void *context, RADIUS_PACKET *pkt, int flags)
{
	int code;
        eap *_this;
        int errorCode;
	int finish;
	int retry = 0;
	char *notify_reason = NULL;
	RADIUS_REQUEST_CTX radctx;
	u_char msgbuf[4096], *cp;		/* FIXME: たぶん十分 */
	int len;
        u_int8_t attrlen = 0;
	
        u_int8_t eap_code = 0;
        u_int8_t eap_id = 0;
        size_t eap_length;

        const char *secret;
        int secretlen;

        EAP_ASSERT(context != NULL);

        _this = context;
	radctx = _this->radctx;
        errorCode = ERROR_AUTH_SERVER_TIMEOUT;
        _this->radctx = NULL; 

        if (pkt == NULL) {
		if (flags & RADIUS_REQUST_TIMEOUT) {
			/*
			 * timeout 
			 * retry
			 */
                	eap_log(_this, LOG_WARNING, "Timeout radius response");
			retry = 1;
			notify_reason = "timeout";
		} else  {
			/* 
			 * internal error
			 * retry
			 */
                	eap_log(_this, LOG_WARNING,
			    "Internal error with radius packet");           
			retry = 1;
			notify_reason = "intenal error";
		}
		goto auth_failed;
        }

        if(!(flags && RADIUS_REQUST_CHECK_AUTHENTICTOR_NO_CHECK) &&
            !(flags && RADIUS_REQUST_CHECK_AUTHENTICTOR_OK)){
                /* discard */
                eap_log(_this, LOG_WARNING, "Header has invalid authticator");           
		notify_reason = "bad authenticator";
		retry = 1;
            	goto auth_failed;
        }

	/*
	 *   get secret password from the radius request context
	 */
	secret = radius_get_server_secret(radctx);
	secretlen = strlen(secret);

        /*                                       
         * get radius code                        
         */                                      
	code = radius_get_code(pkt);
	if (radius_check_message_authenticator(pkt, secret) != 0) {
		eap_log(_this, LOG_WARNING, "bad message authenticator.");
            	goto auth_failed;
	} else {
		EAP_DBG((_this, LOG_INFO, "good message authenticator."));
	}

	/*
	 * get first eap message and get length  of eap message
	 */
	if (radius_get_raw_attr(pkt, RADIUS_TYPE_EAP_MESSAGE, msgbuf, &attrlen)
	    != 0) {
		/*
		 * check reject
		 */
		if ((_this->flags & PPP_EAP_FLAG_NAK_RESPONSE) 
		    && code == RADIUS_CODE_ACCESS_REJECT) {
			/*
			 * nak and reject
			 */
			eap_log(_this, LOG_NOTICE,
			    "Authentication reject with nak");
		} else if (code == RADIUS_CODE_ACCESS_REJECT) {
			/*
			 * reject
			 */
                	eap_log(_this, LOG_NOTICE, "Authentication reject");           
		} else {
			/*
			 * discard
			 */
			eap_log(_this, LOG_WARNING, "Not found eap attribute");
			goto auth_failed;
		}
		eap_stop(_this);
		ppp_stop(_this->ppp, "Authentication reject");
		goto auth_failed;
	}
	if (attrlen < 4) {
		/* 
		 * discard
		 */
		eap_log(_this, LOG_WARNING, "EAP message is too short");
       	     	goto auth_failed;
	}
	cp = msgbuf;
	GETCHAR(eap_code, cp);
	GETCHAR(eap_id, cp);
	_this->eapid = eap_id;
	GETSHORT(eap_length, cp);

	/*
	 * if challenge packet, try get state attribute
	 */
	if (code == RADIUS_CODE_ACCESS_CHALLENGE) {
		_this->attr_state_len = RADIUS_ATTR_STATE_LEN;
		if (radius_get_raw_attr(pkt, 
		    RADIUS_TYPE_STATE, 
		    _this->attr_state, 
		    &(_this->attr_state_len)) != 0) {
			/* discard */
			eap_log(_this, LOG_ERR, "Not found state attribute");
            		goto auth_failed;
		}
		if (_this->attr_state_len < 1) {
			/* discard */
			_this->attr_state_len = 0;
			eap_log(_this, LOG_WARNING,
			    "State attribute has invalid length");
            		goto auth_failed;
		}
	}

	/* 
	 * get session timeout field
	 */
	if (radius_get_uint32_attr(pkt, RADIUS_TYPE_SESSION_TIMEOUT, 
	    &_this->session_timeout) == 0) {
		if (_this->session_timeout > EAP_TIMEOUT_MAX) 
			_this->session_timeout = EAP_TIMEOUT_MAX;
		eap_log(_this, LOG_DEBUG, "Found session timeout attribute");	
	}

        /*                            
         *  get eap message attribute 
         */
	if (radius_get_raw_attr_all(pkt, RADIUS_TYPE_EAP_MESSAGE,
	    NULL, &len) != 0) {
		eap_log(_this, LOG_INFO, "Failed to get eap-message from the "
		    "radius");
		retry = 1;
		goto auth_failed;
	}

	if (len != eap_length) {
		eap_log(_this, LOG_INFO, "Received a bad eap-message: "
		    "length in the header is wrong.");
		retry = 1;
		goto auth_failed;
	}
	if (radius_get_raw_attr_all(pkt, RADIUS_TYPE_EAP_MESSAGE, msgbuf, &len)
	    != 0) {
		eap_log(_this, LOG_INFO,
		    "failed to get eap-message from the radius response.");
		retry = 1;
		goto auth_failed;
	}

        /*                       
         * forwarding validation 
         * RFC 3579, RFC 3784
	 * 
	 */ 
	finish = 0;                          
	switch (code) {
	case RADIUS_CODE_ACCESS_REQUEST:
		eap_log(_this, LOG_INFO,
		    "Invalid radius code (access request) code=%d eap_code=%d",
		    code, eap_code);
		goto auth_failed;
		break;
	case RADIUS_CODE_ACCESS_REJECT:
		switch (eap_code) {
		case EAP_REQUEST:
			eap_log(_this, LOG_INFO, "Abnormal reject");	
			eap_stop(_this);
			ppp_stop(_this->ppp, "Authentication failed");
			finish = 1;
			break;
		case EAP_RESPONSE:
			eap_log(_this, LOG_INFO, 
			    "Unexpected eap code(access reject)");	
			goto auth_failed;
			break;
		case EAP_FAILURE:
			eap_log(_this, LOG_INFO, "Eap failure");	
			eap_stop(_this);
			finish = eap_forward_to_peer(_this, 
			    msgbuf+EAP_HEADERLEN, len-EAP_HEADERLEN,
			    eap_code, eap_id);
			break;
		case EAP_SUCCESS:
		default:
			eap_log(_this, LOG_INFO, 
			    "Invalid combination code: radius code = %d and "
			    "eap code = %d", code ,eap_code);
			goto auth_failed;
				break;
		}
		break; 		
	case RADIUS_CODE_ACCESS_ACCEPT:
		switch (eap_code) {
		case EAP_REQUEST:
			finish = eap_forward_to_peer(_this, 
			    msgbuf+EAP_HEADERLEN, len-EAP_HEADERLEN,
			    eap_code, eap_id);
			break;
		case EAP_RESPONSE:
			eap_log(_this, LOG_INFO, 
			    "unexpected eap code(access accept)");	
			goto auth_failed;
			break;
		case EAP_FAILURE:
			eap_log(_this, LOG_INFO, 
			    "Invalid combination code: radius code = %d and "
			    "eap code = %d",
			    code ,eap_code);	
			goto auth_failed;
			break;
		case EAP_SUCCESS:
			ppp_proccess_radius_framed_ip(_this->ppp, pkt);
#ifdef USE_NPPPD_MPPE
			if (get_mppe_keys(_this, pkt, secret)) {
				if (MPPE_REQUIRED(_this->ppp)) {
					eap_log(_this, LOG_ERR,
					    "mppe is required but can't get "
					    "mppe keys");
					eap_stop(_this);
					ppp_stop(_this->ppp, "can't get mppe "
					    "attribute");
				} else {
					eap_log(_this, LOG_INFO,
					    "can't get mppe keys, unuse "
					    "encryption");
				}
                        } else {
				eap_log(_this, LOG_DEBUG, 
					    "Found attribute of mppe keys");
			}

#endif
			finish = eap_forward_to_peer(_this, 
			    msgbuf+EAP_HEADERLEN, len-EAP_HEADERLEN,
			    eap_code, eap_id);
			break;
		default:
			eap_log(_this, LOG_INFO, 
			    "Invalid combination code: radius code = %d and "
			    "eap code = %d", code ,eap_code);
			goto auth_failed;
			break;
		}
		break; 		
	case RADIUS_CODE_ACCESS_CHALLENGE:
		switch (eap_code) {
		case EAP_REQUEST:
			finish = eap_forward_to_peer(_this, 
			    msgbuf+EAP_HEADERLEN, len-EAP_HEADERLEN,
			    eap_code, eap_id);
			break;
		case EAP_RESPONSE:
			eap_log(_this, LOG_INFO, 
			    "Unexpected eap code(access challenge)");	
			goto auth_failed;
			break;
		case EAP_FAILURE:
		case EAP_SUCCESS:
		default:
			eap_log(_this, LOG_INFO, 
			    "Invalid combination code: radius code = %d and "
			    "eap code = %d", code ,eap_code);
			goto auth_failed;
			break;
		} 
		/* XXX TODO:not forward EAP-START */		
		break;
	default:
		eap_log(_this, LOG_INFO,
		    "Unknown radius code type code = %d and eap code = %d",
		    code ,eap_code);
		goto auth_failed;
		break;
	}

	if(!finish) {
		if (_this->session_timeout != 0) {
			TIMEOUT((TIMER_CBFUNC)eap_restart, _this,
			    _this->session_timeout/2);
		} else {
			TIMEOUT((TIMER_CBFUNC)eap_restart, _this,
			    EAP_TIMEOUT_INIT);
		}
	}
        return;

auth_failed:
	eap_log(_this, LOG_WARNING, 
	    "Can't forward packet to peer from radius");
	if (notify_reason != NULL) {
		npppd_radius_server_failure_notify(
		    _this->ppp->pppd, _this->ppp, radctx, notify_reason);
	}
	if (retry) {
		eap_restart(_this);
	}
	return;
}

#ifdef USE_NPPPD_MPPE
int
get_mppe_keys(eap *_this, RADIUS_PACKET *pkt, const char *secret) {
	struct RADIUS_MPPE_KEY sendkey, recvkey;
	u_int8_t len;

        EAP_ASSERT(_this != NULL);
        EAP_ASSERT(_this->ppp != NULL);


	if (_this->ppp->mppe.enabled == 0) {
		return 1;
	}
	len = sizeof(sendkey);
	if (radius_get_vs_raw_attr(pkt, RADIUS_VENDOR_MICROSOFT,
	    RADIUS_VTYPE_MPPE_SEND_KEY, &sendkey, &len) != 0) {
		eap_log(_this, LOG_ERR, "no mppe_send_key");
		return 1;
	}
	len = sizeof(recvkey);
	if (radius_get_vs_raw_attr(pkt, RADIUS_VENDOR_MICROSOFT,
	    RADIUS_VTYPE_MPPE_RECV_KEY, &recvkey, &len) != 0) {
		eap_log(_this, LOG_ERR, "no mppe_recv_key");
		return 1;
	}
	DecryptKeyFromRadius(_this->ppp->mppe.send.master_key,
	    sendkey.salt, _this->authenticator, secret);

	DecryptKeyFromRadius(_this->ppp->mppe.recv.master_key,
	    recvkey.salt, _this->authenticator, secret);

	return 0;
}
#endif

void
eap_stop(eap *_this)
{
        _this->state = EAP_STATE_STOPPED;
        UNTIMEOUT(eap_restart, _this);
	if (_this->radctx != NULL) {
		radius_cancel_request(_this->radctx);
		_this->radctx = NULL;
	}
}

static int
eap_forward_to_peer(eap *_this, u_int8_t *data, int datalen, int type, u_int8_t id)
{
	int finish = 0;
        EAP_ASSERT(_this != NULL);
        EAP_ASSERT(data != NULL);

	switch (type) {
	case EAP_REQUEST: 
        	ppp_output(_this->ppp, PPP_PROTO_EAP, EAP_REQUEST, id, data,
		    datalen); 
        	break;
	case EAP_SUCCESS:
        	ppp_output(_this->ppp, PPP_PROTO_EAP, EAP_SUCCESS, id, data,
		    datalen);
        	eap_log(_this, LOG_INFO, "Authentication succeeded");
        	eap_stop(_this);
		memcpy(_this->ppp->username, _this->name, _this->name_len);
		ppp_auth_ok(_this->ppp);
		finish = 1;
		break;
	case EAP_FAILURE:
        	ppp_output(_this->ppp, PPP_PROTO_EAP, EAP_FAILURE, id, data,
		    datalen); 
        	eap_log(_this, LOG_INFO, "eap-failure has been received from the peer.");
        	eap_log(_this, LOG_INFO, "Authentication failed");
		eap_stop(_this);
		ppp_stop(_this->ppp, "Authentication failed");
		finish = 1;
		break;
	default:
		break;
        }
	return finish;
}

/************************************************************************
 * ユーティリティ関数
 ************************************************************************/
void
eap_log(eap *_this, uint32_t prio, const char *fmt, ...)
{
	char logbuf[BUFSIZ];
	va_list ap;

	EAP_ASSERT(_this != NULL);
	EAP_ASSERT(_this->ppp != NULL);

	va_start(ap, fmt);
	snprintf(logbuf, sizeof(logbuf), "ppp id=%u layer=eap %s",
	    _this->ppp->id, fmt);
	vlog_printf(prio, logbuf, ap);
	va_end(ap);
}
