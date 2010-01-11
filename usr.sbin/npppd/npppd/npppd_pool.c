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
/**@file */
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netinet/in_systm.h>
#include <net/route.h>
#include <netinet/ip.h>
#include <arpa/inet.h>
#include <net/if_dl.h>
#include <stdio.h>
#include <time.h>
#include <event.h>
#include <string.h>
#include <errno.h>
#include <unistd.h>
#include <syslog.h>
#include <stdlib.h>
#include <stdarg.h>
#include <netdb.h>

#include "slist.h"
#include "debugutil.h"
#include "properties.h"
#include "addr_range.h"
#include "radish.h"
#include "config_helper.h"
#include "npppd_local.h"
#include "npppd_pool.h"
#include "npppd_subr.h"
#include "net_utils.h"

#ifdef	NPPPD_POOL_DEBUG
#define	NPPPD_POOL_DBG(x)	npppd_pool_log x
#define	NPPPD_POOL_ASSERT(cond)					\
	if (!(cond)) {						\
	    fprintf(stderr,					\
		"\nASSERT(" #cond ") failed on %s() at %s:%d.\n"\
		, __func__, __FILE__, __LINE__);		\
	    abort(); 						\
	}
#else
#define	NPPPD_POOL_ASSERT(cond)			
#define	NPPPD_POOL_DBG(x)
#endif
#define	A(v) ((0xff000000 & (v)) >> 24), ((0x00ff0000 & (v)) >> 16),	\
	    ((0x0000ff00 & (v)) >> 8), (0x000000ff & (v))
#define	SA(sin4)	((struct sockaddr *)(sin4))

#define SHUFLLE_MARK 0xffffffffL
static int  npppd_pool_log __P((npppd_pool *, int, const char *, ...)) __printflike(3, 4);
static int  in_addr_range_list_add_all (struct in_addr_range **, const char *);
static int  is_valid_host_address (uint32_t);
static int  npppd_pool_regist_radish(npppd_pool *, struct in_addr_range *,
    struct sockaddr_npppd *, int );


/***********************************************************************
 * npppd_pool オブジェクト操作
 ***********************************************************************/
/** npppd_poll を初期化します */
int
npppd_pool_init(npppd_pool *_this, npppd *base, const char *name)
{
	memset(_this, 0, sizeof(npppd_pool));

	strlcpy(_this->label, name, sizeof(_this->label));
	_this->npppd = base;
	slist_init(&_this->dyna_addrs);

	_this->initialized = 1;

	return 0;
}

/** npppd_pool の使用を開始します */
int
npppd_pool_start(npppd_pool *_this)
{
	return 0;	//  やることなし。
}

/* 設定テンプレート展開 */;
NAMED_PREFIX_CONFIG_DECL(npppd_pool_config, npppd_pool, npppd->properties,
    "pool", label);
NAMED_PREFIX_CONFIG_FUNCTIONS(npppd_pool_config, npppd_pool, npppd->properties,
    "pool", label);

/** npppd_poll を終了化します */
void
npppd_pool_uninit(npppd_pool *_this)
{
	_this->initialized = 0;

	slist_fini(&_this->dyna_addrs);
	if (_this->addrs != NULL)
		free(_this->addrs);
	_this->addrs = NULL;
	_this->addrs_size = 0;
	_this->npppd = NULL;
}

/** 設定を再読み込みします。*/
int
npppd_pool_reload(npppd_pool *_this)
{
	int i, count, addrs_size;
	struct sockaddr_npppd *addrs;
	struct in_addr_range *pool, *dyna_pool, *range;
	const char *val, *val0;
	char buf0[BUFSIZ], buf1[BUFSIZ];

	addrs = NULL;
	pool = NULL;
	dyna_pool = NULL;
	buf0[0] = '\0';

	val = npppd_pool_config_str(_this, "name");
	if (val == NULL)
		val = _this->label;
	strlcpy(_this->name, val, sizeof(_this->name));

	/* 動的アドレスプール */
	val0 = NULL;
	val = npppd_pool_config_str(_this, "dyna_pool");
	if (val != NULL) {
		if (in_addr_range_list_add_all(&dyna_pool, val) != 0) {
			npppd_pool_log(_this, LOG_WARNING,
			    "parse error at 'dyna_pool': %s", val);
			goto reigai;
		}
		val0 = val;
	}

	/* 固定アドレスプール */
	val = npppd_pool_config_str(_this, "pool");
	if (val != NULL) {
		if (in_addr_range_list_add_all(&pool, val) != 0) {
			npppd_pool_log(_this, LOG_WARNING,
			    "parse error at 'pool': %s", val);
			goto reigai;
		}
		if (val0 != NULL)
			/* Aggregate */
			in_addr_range_list_add_all(&pool, val0);
	}

	/* RADISH 登録準備 */
	addrs_size = 0;
	for (range = dyna_pool; range != NULL; range = range->next)
		addrs_size++;
	for (range = pool; range != NULL; range = range->next)
		addrs_size++;
	
	if ((addrs = calloc(addrs_size + 1, sizeof(struct sockaddr_npppd)))
	    == NULL) {
		/* +1 しているのは calloc(0) を回避するため */
		npppd_pool_log(_this, LOG_WARNING,
		    "calloc() failed in %s: %m", __func__);
		goto reigai;
	}

	/* 動的プール => RADISH 登録 */
	count = 0;
	for (i = 0, range = dyna_pool; range != NULL; range = range->next, i++){
		if (npppd_pool_regist_radish(_this, range, &addrs[count], 1))
			goto reigai;
		if (count == 0)
			strlcat(buf0, "dyn_pool=[", sizeof(buf0));
		else
			strlcat(buf0, ",", sizeof(buf0));
		snprintf(buf1, sizeof(buf1), "%d.%d.%d.%d/%d",
		    A(range->addr), netmask2prefixlen(range->mask));
		strlcat(buf0, buf1, sizeof(buf0));
		count++;
	}
	if (i > 0)
		strlcat(buf0, "] ", sizeof(buf0));

	/* 固定プール => RADISH 登録 */
	for (i = 0, range = pool; range != NULL; range = range->next, i++) {
		if (npppd_pool_regist_radish(_this, range, &addrs[count], 0))
			goto reigai;
		if (i == 0)
			strlcat(buf0, "pool=[", sizeof(buf0));
		else
			strlcat(buf0, ",", sizeof(buf0));
		snprintf(buf1, sizeof(buf1), "%d.%d.%d.%d/%d",
		    A(range->addr), netmask2prefixlen(range->mask));
		strlcat(buf0, buf1, sizeof(buf0));
		count++;
	}
	if (i > 0)
		strlcat(buf0, "]", sizeof(buf0));

	npppd_pool_log(_this, LOG_INFO, "%s", buf0);

	count = 0;
	slist_add(&_this->dyna_addrs, (void *)SHUFLLE_MARK);
	for (range = dyna_pool; range != NULL; range = range->next) {
		if (count >= NPPPD_MAX_POOLED_ADDRS)
			break;
		for (i = 0; i <= ~(range->mask); i++) {
			if (!is_valid_host_address(range->addr + i))
				continue;
			if (count >= NPPPD_MAX_POOLED_ADDRS)
				break;
			slist_add(&_this->dyna_addrs, (void *)
			    (range->addr + i));
			count++;
		}
	}
	if (_this->addrs != NULL)
		free(_this->addrs);
	_this->addrs = addrs;
	_this->addrs_size = addrs_size;
	in_addr_range_list_remove_all(&pool);
	in_addr_range_list_remove_all(&dyna_pool);

	return 0;
reigai:
	in_addr_range_list_remove_all(&pool);
	in_addr_range_list_remove_all(&dyna_pool);

	if (addrs != NULL)
		free(addrs);

	return 1;
}

static int
npppd_pool_regist_radish(npppd_pool *_this, struct in_addr_range *range,
    struct sockaddr_npppd *snp, int is_dynamic)
{
	int rval;
	struct sockaddr_in sin4a, sin4b;
	struct sockaddr_npppd *snp0;
	npppd_pool *npool0;

	memset(&sin4a, 0, sizeof(sin4a));
	memset(&sin4b, 0, sizeof(sin4b));
	sin4a.sin_len = sin4b.sin_len = sizeof(sin4a);
	sin4a.sin_family = sin4b.sin_family = AF_INET;
	sin4a.sin_addr.s_addr = htonl(range->addr);
	sin4b.sin_addr.s_addr = htonl(range->mask);

	snp->snp_len = sizeof(struct sockaddr_npppd);
	snp->snp_family = AF_INET;
	snp->snp_addr.s_addr = htonl(range->addr);
	snp->snp_mask.s_addr = htonl(range->mask);
	snp->snp_data_ptr = _this;
	if (is_dynamic)
		snp->snp_type = SNP_DYN_POOL;
	else
		snp->snp_type = SNP_POOL;

	if ((snp0 = rd_lookup(SA(&sin4a), SA(&sin4b),
	    _this->npppd->rd)) != NULL) {
		/*
		 * radish ツリーは、初期化直後で POOL のエントリしかないことを
		 * 仮定。
		 */
		NPPPD_POOL_ASSERT(snp0->snp_type != SNP_PPP);
		npool0 = snp0->snp_data_ptr;

		if (!is_dynamic && npool0 == _this)
			/* 動的アドレスとして登録済 */
			return 0;

		npppd_pool_log(_this, LOG_WARNING,
		    "%d.%d.%d.%d/%d is already defined as '%s'(%s)",
		    A(range->addr), netmask2prefixlen(range->mask),
		    npool0->name, (snp0->snp_type == SNP_POOL)
			? "static" : "dynamic");
		goto reigai;
	}
	if ((rval = rd_insert(SA(&sin4a), SA(&sin4b), _this->npppd->rd,
	    snp)) != 0) {
		errno = rval;
		npppd_pool_log(_this, LOG_WARNING,
		    "rd_insert(%d.%d.%d.%d/%d) failed: %m",
		    A(range->addr), netmask2prefixlen(range->mask));
		goto reigai;
	}

	return 0;
reigai:
	return 1;

}

/***********************************************************************
 * API
 ***********************************************************************/
/** 動的アドレスを割り当てます */
uint32_t
npppd_pool_get_dynamic(npppd_pool *_this, npppd_ppp *ppp)
{
	int shuffle_cnt;
	void *result = NULL;
	struct sockaddr_in sin4 = {
		.sin_len = sizeof(struct sockaddr_in),
		.sin_family = AF_INET
	};
	struct sockaddr_npppd *snp;
	npppd_ppp *ppp0;

	shuffle_cnt = 0;
	slist_itr_first(&_this->dyna_addrs);
	while (slist_length(&_this->dyna_addrs) > 1 &&
	    slist_itr_has_next(&_this->dyna_addrs)) {
		result = slist_itr_next(&_this->dyna_addrs);
		if (result == NULL)
			break;
		/* シャッフル */
		if ((uint32_t)result == SHUFLLE_MARK) {
			/*
			 * 使えるアドレスが無くなると length > 1 でも、
			 * shuffle を連続してツモる。2回ツモったら、
			 * つまり使えるアドレスがない。
			 */
			if (shuffle_cnt++ > 0) {
				result = NULL;
				break;
			}
			NPPPD_POOL_DBG((_this, LOG_DEBUG, "shuffle"));
			slist_itr_remove(&_this->dyna_addrs);
			slist_shuffle(&_this->dyna_addrs);
			slist_add(&_this->dyna_addrs, result);
			slist_itr_first(&_this->dyna_addrs);
			continue;
		}
		slist_itr_remove(&_this->dyna_addrs);

		switch (npppd_pool_get_assignability(_this, (uint32_t)result,
		    0xffffffffL, &snp)) {
		case ADDRESS_OK:
			/* 成功するのはココだけ。 */
			return (uint32_t)result;
		default:
			/* インタフェースのアドレスだった場合 */
			/*
			 * リストから削除しているので、インタフェースのアドレ
			 * スだけを再変更すると、アドレスをリークしていく問題
			 * があるが、現実装では、アドレスだけを変更していくこ
			 * とはないので問題ない。運用上も、プールを変更せず、
			 * tunnel-end-address だけを変更していく、というのは、
			 * 定常的に発生するとは考えづらい。
			 */
			continue;
		case ADDRESS_BUSY:
			sin4.sin_addr.s_addr = htonl((uint32_t)result);
			/*
			 * 設定再読み込みにより、アクティブな PPP セッションが
			 * リセットされた
			 */
			NPPPD_POOL_ASSERT(snp != NULL);
			NPPPD_POOL_ASSERT(snp->snp_type == SNP_PPP);
			ppp0 = snp->snp_data_ptr;
			ppp0->assigned_pool = _this;
			ppp0->assign_dynapool = 1;	/* 返却よろしく */
			continue;
		}
		break;
	}
	return (uint32_t)0;
}

inline static int
npppd_is_ifcace_ip4addr(npppd *_this, uint32_t ip4addr)
{
	int i;

	for (i = 0; i < countof(_this->iface); i++) {
		if (npppd_iface_ip_is_ready(&_this->iface[i]) &&
		    _this->iface[i].ip4addr.s_addr == ip4addr)
			return 1;
	}

	return 0;
}

/** IPアドレスを割り当てます */
int
npppd_pool_assign_ip(npppd_pool *_this, npppd_ppp *ppp)
{
	int rval;
	uint32_t ip4;
	void *rtent;
	struct sockaddr_in addr = {
		.sin_family = AF_INET,
		.sin_len = sizeof(struct sockaddr_in)
	}, mask = {
		.sin_family = AF_INET,
		.sin_len = sizeof(struct sockaddr_in),
	};
	struct sockaddr_npppd *snp;

	ip4 = ntohl(ppp->ppp_framed_ip_address.s_addr);

	/* 動的アドレスリストに含まれたらそこから取り外す。 */
	slist_itr_first(&_this->dyna_addrs);
	while (slist_itr_has_next(&_this->dyna_addrs)) {
		if ((uint32_t)slist_itr_next(
		    &_this->dyna_addrs) != ip4)
			continue;
		slist_itr_remove(&_this->dyna_addrs);
		break;
	}

	addr.sin_addr = ppp->ppp_framed_ip_address;
	mask.sin_addr = ppp->ppp_framed_ip_netmask;
	addr.sin_addr.s_addr &= mask.sin_addr.s_addr;

	if (rd_delete(SA(&addr), SA(&mask), _this->npppd->rd, &rtent) == 0) {
		snp = rtent;
		/* 重複エントリあり。プールから PPPへの差し替え */
		NPPPD_POOL_ASSERT(snp != NULL);
		NPPPD_POOL_ASSERT(snp->snp_type != SNP_PPP);
		ppp->snp.snp_next = snp;
		NPPPD_POOL_DBG((_this, DEBUG_LEVEL_2,
		    "pool %s/32 => %s(ppp=%d)",
		    inet_ntoa(ppp->ppp_framed_ip_address), ppp->username,
		    ppp->id));
	}
	NPPPD_POOL_DBG((_this, LOG_DEBUG, "rd_insert(%s) %s",
	    inet_ntoa(addr.sin_addr), ppp->username));
	if ((rval = rd_insert((struct sockaddr *)&addr,
	    (struct sockaddr *)&mask, _this->npppd->rd, &ppp->snp)) != 0) {
		errno = rval;
		log_printf(LOG_INFO, "rd_insert(%s) failed: %m",
		    inet_ntoa(ppp->ppp_framed_ip_address));
		return 1;
	}

	return 0;
}

/** IPアドレスを解放します */
void
npppd_pool_release_ip(npppd_pool *_this, npppd_ppp *ppp)
{
	void *item;
	int rval;
	struct sockaddr_npppd *snp;
	struct sockaddr_in addr = {
		.sin_family = AF_INET,
		.sin_len = sizeof(struct sockaddr_in)
	}, mask = {
		.sin_family = AF_INET,
		.sin_len = sizeof(struct sockaddr_in),
	};

	/* _this == NULL 設定変更によりプールは解放された */
	if (!ppp_ip_assigned(ppp))
		return;

	addr.sin_addr = ppp->ppp_framed_ip_address;
	mask.sin_addr = ppp->ppp_framed_ip_netmask;
	addr.sin_addr.s_addr &= mask.sin_addr.s_addr;

	if ((rval = rd_delete((struct sockaddr *)&addr,
	    (struct sockaddr *)&mask, ppp->pppd->rd, &item)) != 0) {
		errno = rval;
		log_printf(LOG_INFO, "Unexpected error: "
		    "rd_delete(%s) failed: %m",
		    inet_ntoa(ppp->ppp_framed_ip_address));
	}
	snp = item;

	if (_this != NULL && ppp->assign_dynapool != 0)
		/* 動的リストに返却 */
		slist_add(&((npppd_pool *)ppp->assigned_pool)->dyna_addrs, 
		    (void *)ntohl(ppp->ppp_framed_ip_address.s_addr));

	if (snp != NULL && snp->snp_next != NULL) {
		/*
		 * radish エントリがリストになっていて、アドレス/マスクが
		 * 一致していれば、次のエントリを再登録。
		 */
		if (rd_insert(SA(&addr), SA(&mask), ppp->pppd->rd,
		    snp->snp_next) != 0) {
			log_printf(LOG_INFO, "Unexpected error: "
			    "rd_insert(%s) failed: %m",
			    inet_ntoa(ppp->ppp_framed_ip_address));
		}
		NPPPD_POOL_DBG((_this, DEBUG_LEVEL_2,
		    "pool %s/%d <= %s(ppp=%d)",
		    inet_ntoa(ppp->ppp_framed_ip_address),
		    netmask2prefixlen(ntohl(ppp->ppp_framed_ip_netmask.s_addr)),
		    ppp->username, ppp->id));
		snp->snp_next = NULL;
	}
}

/**
 * 指定したアドレスが割り当て可能かどうか。
 * @return {@link ::#ADDRESS_OK}、{@link ::#ADDRESS_RESERVED}、
 * {@link ::#ADDRESS_BUSY}、{@link ::#ADDRESS_INVALID} もしくは
 * {@link ::#ADDRESS_OUT_OF_POOL} が返ります。
 */
int
npppd_pool_get_assignability(npppd_pool *_this, uint32_t ip4addr,
    uint32_t ip4mask, struct sockaddr_npppd **psnp)
{
	struct radish *radish;
	struct sockaddr_in sin4;
	struct sockaddr_npppd *snp;

	NPPPD_POOL_ASSERT(ip4mask != 0);
	NPPPD_POOL_DBG((_this, LOG_DEBUG, "%s(%08x,%08x)", __func__, ip4addr,
	    ip4mask));

	if (netmask2prefixlen(htonl(ip4mask)) == 32) {
		if (!is_valid_host_address(ip4addr))
			return ADDRESS_INVALID;
	}

	memset(&sin4, 0, sizeof(sin4));

	sin4.sin_len = sizeof(sin4);
	sin4.sin_family = AF_INET;
	sin4.sin_addr.s_addr = htonl(ip4addr);

	if (npppd_is_ifcace_ip4addr(_this->npppd, sin4.sin_addr.s_addr))
		return ADDRESS_RESERVED;
		/* インタフェースのアドレスは割り振らない */

	if (rd_match(SA(&sin4), _this->npppd->rd, &radish)) {
		do {
			snp = radish->rd_rtent;
			if (snp->snp_type == SNP_POOL ||
			    snp->snp_type == SNP_DYN_POOL) {
				if (psnp != NULL)
					*psnp = snp;
				if (snp->snp_data_ptr == _this)
					return  ADDRESS_OK;		
				else
					return ADDRESS_RESERVED;
			}
			if (snp->snp_type == SNP_PPP) {
				if (psnp != NULL)
					*psnp = snp;
				return ADDRESS_BUSY;		
			}
		} while (rd_match_next(SA(&sin4), _this->npppd->rd, &radish,
		    radish));
	}

	return ADDRESS_OUT_OF_POOL;
}
/***********************************************************************
 * 雑多
 ***********************************************************************/
/**
 * ホストアドレスとして正しいか。
 * <pre>
 * ナチュラルマスクのブロードキャストアドレスをホストとして利用すると、
 * いくつか問題があるので「正しくない」とする。問題とは、
 *
 * (1) BSD系は、該当アドレスは転送せず、自分宛として処理する。
 * (2) [IDGW-DEV 4405]『IPアドレスに .255 が割り当てられた Windows マシン
 *     から L2TP/IPsec を利用できない問題』</pre>
 */
static int
is_valid_host_address(uint32_t addr)
{
	if (IN_CLASSA(addr))
		return ((IN_CLASSA_HOST & addr) == 0 ||
		    (IN_CLASSA_HOST & addr) == IN_CLASSA_HOST)? 0 : 1;
	if (IN_CLASSB(addr))
		return ((IN_CLASSB_HOST & addr) == 0 ||
		    (IN_CLASSB_HOST & addr) == IN_CLASSB_HOST)? 0 : 1;
	if (IN_CLASSC(addr))
		return ((IN_CLASSC_HOST & addr) == 0 ||
		    (IN_CLASSC_HOST & addr) == IN_CLASSC_HOST)? 0 : 1;

	return 0;
}

/** このインスタンスに基づいたラベルから始まるログを記録します。 */
static int
npppd_pool_log(npppd_pool *_this, int prio, const char *fmt, ...)
{
	int status;
	char logbuf[BUFSIZ];
	va_list ap;

	/*
	 * npppd_pool_release_ip は _this == NULL で呼ばれるので
	 * NPPPD_POOL_ASSERT(_this != NULL);
	 * できない
	 */
	va_start(ap, fmt);
	snprintf(logbuf, sizeof(logbuf), "pool name=%s %s", 
	    (_this == NULL)? "null" : _this->name, fmt);
	status = vlog_printf(prio, logbuf, ap);
	va_end(ap);

	return status;
}

static int
in_addr_range_list_add_all(struct in_addr_range **range, const char *str)
{
	char *tok,  *buf0, buf[NPPPD_CONFIG_BUFSIZ];

	strlcpy(buf, str, sizeof(buf));
	buf0 = buf;

	while ((tok = strsep(&buf0, " \r\n\t")) != NULL) {
		if (tok[0] == '\0')
			continue;
		if (in_addr_range_list_add(range, tok) != 0)
			return 1;
	}
	return 0;
}
