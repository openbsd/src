/*
 * Copyright (c) 1995 Mats O Jansson <moj@stacken.kth.se>
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
 * 3. The name of the author may not be used to endorse or promote
 *    products derived from this software without specific prior written
 *    permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS
 * OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 *	$Id: yp.h,v 1.1 1995/11/01 16:56:33 deraadt Exp $
 */

#ifndef _YPSERV_H_RPCGEN
#define _YPSERV_H_RPCGEN

#include <rpc/rpc.h>

#define YPMAXRECORD 1024
#define YPMAXDOMAIN 64
#define YPMAXMAP 64
#define YPMAXPEER 64

enum ypstat {
	YP_TRUE = 1,
	YP_NOMORE = 2,
	YP_FALSE = 0,
	YP_NOMAP = -1,
	YP_NODOM = -2,
	YP_NOKEY = -3,
	YP_BADOP = -4,
	YP_BADDB = -5,
	YP_YPERR = -6,
	YP_BADARGS = -7,
	YP_VERS = -8,
};
typedef enum ypstat ypstat;
#ifdef __cplusplus 
extern "C" bool_t xdr_ypstat(XDR *, ypstat*);
#elif __STDC__ 
extern  bool_t xdr_ypstat(XDR *, ypstat*);
#else /* Old Style C */ 
bool_t xdr_ypstat();
#endif /* Old Style C */ 


enum ypxfrstat {
	YPXFR_SUCC = 1,
	YPXFR_AGE = 2,
	YPXFR_NOMAP = -1,
	YPXFR_NODOM = -2,
	YPXFR_RSRC = -3,
	YPXFR_RPC = -4,
	YPXFR_MADDR = -5,
	YPXFR_YPERR = -6,
	YPXFR_BADARGS = -7,
	YPXFR_DBM = -8,
	YPXFR_FILE = -9,
	YPXFR_SKEW = -10,
	YPXFR_CLEAR = -11,
	YPXFR_FORCE = -12,
	YPXFR_XFRERR = -13,
	YPXFR_REFUSED = -14,
};
typedef enum ypxfrstat ypxfrstat;
#ifdef __cplusplus 
extern "C" bool_t xdr_ypxfrstat(XDR *, ypxfrstat*);
#elif __STDC__ 
extern  bool_t xdr_ypxfrstat(XDR *, ypxfrstat*);
#else /* Old Style C */ 
bool_t xdr_ypxfrstat();
#endif /* Old Style C */ 


typedef char *domainname;
#ifdef __cplusplus 
extern "C" bool_t xdr_domainname(XDR *, domainname*);
#elif __STDC__ 
extern  bool_t xdr_domainname(XDR *, domainname*);
#else /* Old Style C */ 
bool_t xdr_domainname();
#endif /* Old Style C */ 


typedef char *mapname;
#ifdef __cplusplus 
extern "C" bool_t xdr_mapname(XDR *, mapname*);
#elif __STDC__ 
extern  bool_t xdr_mapname(XDR *, mapname*);
#else /* Old Style C */ 
bool_t xdr_mapname();
#endif /* Old Style C */ 


typedef char *peername;
#ifdef __cplusplus 
extern "C" bool_t xdr_peername(XDR *, peername*);
#elif __STDC__ 
extern  bool_t xdr_peername(XDR *, peername*);
#else /* Old Style C */ 
bool_t xdr_peername();
#endif /* Old Style C */ 


typedef struct {
	u_int keydat_len;
	char *keydat_val;
} keydat;
#ifdef __cplusplus 
extern "C" bool_t xdr_keydat(XDR *, keydat*);
#elif __STDC__ 
extern  bool_t xdr_keydat(XDR *, keydat*);
#else /* Old Style C */ 
bool_t xdr_keydat();
#endif /* Old Style C */ 


typedef struct {
	u_int valdat_len;
	char *valdat_val;
} valdat;
#ifdef __cplusplus 
extern "C" bool_t xdr_valdat(XDR *, valdat*);
#elif __STDC__ 
extern  bool_t xdr_valdat(XDR *, valdat*);
#else /* Old Style C */ 
bool_t xdr_valdat();
#endif /* Old Style C */ 


struct ypmap_parms {
	domainname domain;
	mapname map;
	u_int ordernum;
	peername peer;
};
typedef struct ypmap_parms ypmap_parms;
#ifdef __cplusplus 
extern "C" bool_t xdr_ypmap_parms(XDR *, ypmap_parms*);
#elif __STDC__ 
extern  bool_t xdr_ypmap_parms(XDR *, ypmap_parms*);
#else /* Old Style C */ 
bool_t xdr_ypmap_parms();
#endif /* Old Style C */ 


struct ypreq_key {
	domainname domain;
	mapname map;
	keydat key;
};
typedef struct ypreq_key ypreq_key;
#ifdef __cplusplus 
extern "C" bool_t xdr_ypreq_key(XDR *, ypreq_key*);
#elif __STDC__ 
extern  bool_t xdr_ypreq_key(XDR *, ypreq_key*);
#else /* Old Style C */ 
bool_t xdr_ypreq_key();
#endif /* Old Style C */ 


struct ypreq_nokey {
	domainname domain;
	mapname map;
};
typedef struct ypreq_nokey ypreq_nokey;
#ifdef __cplusplus 
extern "C" bool_t xdr_ypreq_nokey(XDR *, ypreq_nokey*);
#elif __STDC__ 
extern  bool_t xdr_ypreq_nokey(XDR *, ypreq_nokey*);
#else /* Old Style C */ 
bool_t xdr_ypreq_nokey();
#endif /* Old Style C */ 


struct ypreq_xfr {
	ypmap_parms map_parms;
	u_int transid;
	u_int prog;
	u_int port;
};
typedef struct ypreq_xfr ypreq_xfr;
#ifdef __cplusplus 
extern "C" bool_t xdr_ypreq_xfr(XDR *, ypreq_xfr*);
#elif __STDC__ 
extern  bool_t xdr_ypreq_xfr(XDR *, ypreq_xfr*);
#else /* Old Style C */ 
bool_t xdr_ypreq_xfr();
#endif /* Old Style C */ 


struct ypresp_val {
	ypstat stat;
	valdat val;
};
typedef struct ypresp_val ypresp_val;
#ifdef __cplusplus 
extern "C" bool_t xdr_ypresp_val(XDR *, ypresp_val*);
#elif __STDC__ 
extern  bool_t xdr_ypresp_val(XDR *, ypresp_val*);
#else /* Old Style C */ 
bool_t xdr_ypresp_val();
#endif /* Old Style C */ 


struct ypresp_key_val {
	ypstat stat;
	keydat key;
	valdat val;
};
typedef struct ypresp_key_val ypresp_key_val;
#ifdef __cplusplus 
extern "C" bool_t xdr_ypresp_key_val(XDR *, ypresp_key_val*);
#elif __STDC__ 
extern  bool_t xdr_ypresp_key_val(XDR *, ypresp_key_val*);
#else /* Old Style C */ 
bool_t xdr_ypresp_key_val();
#endif /* Old Style C */ 


struct ypresp_master {
	ypstat stat;
	peername peer;
};
typedef struct ypresp_master ypresp_master;
#ifdef __cplusplus 
extern "C" bool_t xdr_ypresp_master(XDR *, ypresp_master*);
#elif __STDC__ 
extern  bool_t xdr_ypresp_master(XDR *, ypresp_master*);
#else /* Old Style C */ 
bool_t xdr_ypresp_master();
#endif /* Old Style C */ 


struct ypresp_order {
	ypstat stat;
	u_int ordernum;
};
typedef struct ypresp_order ypresp_order;
#ifdef __cplusplus 
extern "C" bool_t xdr_ypresp_order(XDR *, ypresp_order*);
#elif __STDC__ 
extern  bool_t xdr_ypresp_order(XDR *, ypresp_order*);
#else /* Old Style C */ 
bool_t xdr_ypresp_order();
#endif /* Old Style C */ 


struct ypresp_all {
	bool_t more;
	union {
		ypresp_key_val val;
	} ypresp_all_u;
};
typedef struct ypresp_all ypresp_all;
#ifdef __cplusplus 
extern "C" bool_t xdr_ypresp_all(XDR *, ypresp_all*);
#elif __STDC__ 
extern  bool_t xdr_ypresp_all(XDR *, ypresp_all*);
#else /* Old Style C */ 
bool_t xdr_ypresp_all();
#endif /* Old Style C */ 


struct ypresp_xfr {
	u_int transid;
	ypxfrstat xfrstat;
};
typedef struct ypresp_xfr ypresp_xfr;
#ifdef __cplusplus 
extern "C" bool_t xdr_ypresp_xfr(XDR *, ypresp_xfr*);
#elif __STDC__ 
extern  bool_t xdr_ypresp_xfr(XDR *, ypresp_xfr*);
#else /* Old Style C */ 
bool_t xdr_ypresp_xfr();
#endif /* Old Style C */ 


struct ypmaplist {
	mapname map;
	struct ypmaplist *next;
};
typedef struct ypmaplist ypmaplist;
#ifdef __cplusplus 
extern "C" bool_t xdr_ypmaplist(XDR *, ypmaplist*);
#elif __STDC__ 
extern  bool_t xdr_ypmaplist(XDR *, ypmaplist*);
#else /* Old Style C */ 
bool_t xdr_ypmaplist();
#endif /* Old Style C */ 


struct ypresp_maplist {
	ypstat stat;
	ypmaplist *maps;
};
typedef struct ypresp_maplist ypresp_maplist;
#ifdef __cplusplus 
extern "C" bool_t xdr_ypresp_maplist(XDR *, ypresp_maplist*);
#elif __STDC__ 
extern  bool_t xdr_ypresp_maplist(XDR *, ypresp_maplist*);
#else /* Old Style C */ 
bool_t xdr_ypresp_maplist();
#endif /* Old Style C */ 


#define YPPROG ((u_long)100004)
#define YPVERS ((u_long)2)

#ifdef __cplusplus
#define YPPROC_NULL ((u_long)0)
extern "C" void * ypproc_null_2(void *, CLIENT *);
extern "C" void * ypproc_null_2_svc(void *, struct svc_req *, SVCXPRT *);
#define YPPROC_DOMAIN ((u_long)1)
extern "C" bool_t * ypproc_domain_2(domainname *, CLIENT *);
extern "C" bool_t * ypproc_domain_2_svc(domainname *, struct svc_req *, SVCXPRT *);
#define YPPROC_DOMAIN_NONACK ((u_long)2)
extern "C" bool_t * ypproc_domain_nonack_2(domainname *, CLIENT *);
extern "C" bool_t * ypproc_domain_nonack_2_svc(domainname *, struct svc_req *, SVCXPRT *);
#define YPPROC_MATCH ((u_long)3)
extern "C" ypresp_val * ypproc_match_2(ypreq_key *, CLIENT *);
extern "C" ypresp_val * ypproc_match_2_svc(ypreq_key *, struct svc_req *, SVCXPRT *);
#define YPPROC_FIRST ((u_long)4)
extern "C" ypresp_key_val * ypproc_first_2(ypreq_key *, CLIENT *);
extern "C" ypresp_key_val * ypproc_first_2_svc(ypreq_key *, struct svc_req *, SVCXPRT *);
#define YPPROC_NEXT ((u_long)5)
extern "C" ypresp_key_val * ypproc_next_2(ypreq_key *, CLIENT *);
extern "C" ypresp_key_val * ypproc_next_2_svc(ypreq_key *, struct svc_req *, SVCXPRT *);
#define YPPROC_XFR ((u_long)6)
extern "C" ypresp_xfr * ypproc_xfr_2(ypreq_xfr *, CLIENT *);
extern "C" ypresp_xfr * ypproc_xfr_2_svc(ypreq_xfr *, struct svc_req *, SVCXPRT *);
#define YPPROC_CLEAR ((u_long)7)
extern "C" void * ypproc_clear_2(void *, CLIENT *);
extern "C" void * ypproc_clear_2_svc(void *, struct svc_req *, SVCXPRT *);
#define YPPROC_ALL ((u_long)8)
extern "C" ypresp_all * ypproc_all_2(ypreq_nokey *, CLIENT *);
extern "C" ypresp_all * ypproc_all_2_svc(ypreq_nokey *, struct svc_req *, SVCXPRT *);
#define YPPROC_MASTER ((u_long)9)
extern "C" ypresp_master * ypproc_master_2(ypreq_nokey *, CLIENT *);
extern "C" ypresp_master * ypproc_master_2_svc(ypreq_nokey *, struct svc_req *, SVCXPRT *);
#define YPPROC_ORDER ((u_long)10)
extern "C" ypresp_order * ypproc_order_2(ypreq_nokey *, CLIENT *);
extern "C" ypresp_order * ypproc_order_2_svc(ypreq_nokey *, struct svc_req *, SVCXPRT *);
#define YPPROC_MAPLIST ((u_long)11)
extern "C" ypresp_maplist * ypproc_maplist_2(domainname *, CLIENT *);
extern "C" ypresp_maplist * ypproc_maplist_2_svc(domainname *, struct svc_req *, SVCXPRT *);

#elif __STDC__
#define YPPROC_NULL ((u_long)0)
extern  void * ypproc_null_2(void *, CLIENT *);
extern  void * ypproc_null_2_svc(void *, struct svc_req *, SVCXPRT *);
#define YPPROC_DOMAIN ((u_long)1)
extern  bool_t * ypproc_domain_2(domainname *, CLIENT *);
extern  bool_t * ypproc_domain_2_svc(domainname *, struct svc_req *, SVCXPRT *);
#define YPPROC_DOMAIN_NONACK ((u_long)2)
extern  bool_t * ypproc_domain_nonack_2(domainname *, CLIENT *);
extern  bool_t * ypproc_domain_nonack_2_svc(domainname *, struct svc_req *, SVCXPRT *);
#define YPPROC_MATCH ((u_long)3)
extern  ypresp_val * ypproc_match_2(ypreq_key *, CLIENT *);
extern  ypresp_val * ypproc_match_2_svc(ypreq_key *, struct svc_req *, SVCXPRT *);
#define YPPROC_FIRST ((u_long)4)
extern  ypresp_key_val * ypproc_first_2(ypreq_key *, CLIENT *);
extern  ypresp_key_val * ypproc_first_2_svc(ypreq_key *, struct svc_req *, SVCXPRT *);
#define YPPROC_NEXT ((u_long)5)
extern  ypresp_key_val * ypproc_next_2(ypreq_key *, CLIENT *);
extern  ypresp_key_val * ypproc_next_2_svc(ypreq_key *, struct svc_req *, SVCXPRT *);
#define YPPROC_XFR ((u_long)6)
extern  ypresp_xfr * ypproc_xfr_2(ypreq_xfr *, CLIENT *);
extern  ypresp_xfr * ypproc_xfr_2_svc(ypreq_xfr *, struct svc_req *, SVCXPRT *);
#define YPPROC_CLEAR ((u_long)7)
extern  void * ypproc_clear_2(void *, CLIENT *);
extern  void * ypproc_clear_2_svc(void *, struct svc_req *, SVCXPRT *);
#define YPPROC_ALL ((u_long)8)
extern  ypresp_all * ypproc_all_2(ypreq_nokey *, CLIENT *);
extern  ypresp_all * ypproc_all_2_svc(ypreq_nokey *, struct svc_req *, SVCXPRT *);
#define YPPROC_MASTER ((u_long)9)
extern  ypresp_master * ypproc_master_2(ypreq_nokey *, CLIENT *);
extern  ypresp_master * ypproc_master_2_svc(ypreq_nokey *, struct svc_req *, SVCXPRT *);
#define YPPROC_ORDER ((u_long)10)
extern  ypresp_order * ypproc_order_2(ypreq_nokey *, CLIENT *);
extern  ypresp_order * ypproc_order_2_svc(ypreq_nokey *, struct svc_req *, SVCXPRT *);
#define YPPROC_MAPLIST ((u_long)11)
extern  ypresp_maplist * ypproc_maplist_2(domainname *, CLIENT *);
extern  ypresp_maplist * ypproc_maplist_2_svc(domainname *, struct svc_req *, SVCXPRT *);

#else /* Old Style C */ 
#define YPPROC_NULL ((u_long)0)
extern  void * ypproc_null_2();
extern  void * ypproc_null_2_svc();
#define YPPROC_DOMAIN ((u_long)1)
extern  bool_t * ypproc_domain_2();
extern  bool_t * ypproc_domain_2_svc();
#define YPPROC_DOMAIN_NONACK ((u_long)2)
extern  bool_t * ypproc_domain_nonack_2();
extern  bool_t * ypproc_domain_nonack_2_svc();
#define YPPROC_MATCH ((u_long)3)
extern  ypresp_val * ypproc_match_2();
extern  ypresp_val * ypproc_match_2_svc();
#define YPPROC_FIRST ((u_long)4)
extern  ypresp_key_val * ypproc_first_2();
extern  ypresp_key_val * ypproc_first_2_svc();
#define YPPROC_NEXT ((u_long)5)
extern  ypresp_key_val * ypproc_next_2();
extern  ypresp_key_val * ypproc_next_2_svc();
#define YPPROC_XFR ((u_long)6)
extern  ypresp_xfr * ypproc_xfr_2();
extern  ypresp_xfr * ypproc_xfr_2_svc();
#define YPPROC_CLEAR ((u_long)7)
extern  void * ypproc_clear_2();
extern  void * ypproc_clear_2_svc();
#define YPPROC_ALL ((u_long)8)
extern  ypresp_all * ypproc_all_2();
extern  ypresp_all * ypproc_all_2_svc();
#define YPPROC_MASTER ((u_long)9)
extern  ypresp_master * ypproc_master_2();
extern  ypresp_master * ypproc_master_2_svc();
#define YPPROC_ORDER ((u_long)10)
extern  ypresp_order * ypproc_order_2();
extern  ypresp_order * ypproc_order_2_svc();
#define YPPROC_MAPLIST ((u_long)11)
extern  ypresp_maplist * ypproc_maplist_2();
extern  ypresp_maplist * ypproc_maplist_2_svc();
#endif /* Old Style C */ 

#endif /* !_YPSERV_H_RPCGEN */
