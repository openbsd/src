/*	$OpenBSD: agentx_control.c,v 1.1 2020/09/14 11:30:25 martijn Exp $	*/

/*
 * Copyright (c) 2020 Martijn van Duren <martijn@openbsd.org>
 * Copyright (c) 2008 - 2014 Reyk Floeter <reyk@openbsd.org>
 *
 * Permission to use, copy, modify, and distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 * ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
 * OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */

#include <sys/types.h>
#include <sys/socket.h>
#include <sys/queue.h>
#include <sys/time.h>
#include <sys/un.h>

#include <netinet/in.h>

#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <errno.h>
#include <string.h>
#include <syslog.h>
#include <unistd.h>
#include <event.h>
#include <imsg.h>

#include "relayd.h"
#include "subagentx.h"

#define RELAYD_MIB	"1.3.6.1.4.1.30155.3"
#define SNMP_ELEMENT(x...)	do {				\
	if (snmp_element(RELAYD_MIB x) == -1)			\
		goto done;					\
} while (0)

/*
static struct snmp_oid	hosttrapoid = {
	{ 1, 3, 6, 1, 4, 1, 30155, 3, 1, 0 },
	10
};
*/

#define RELAYDINFO		SUBAGENTX_ENTERPRISES, 30155, 3, 2
#define RELAYDREDIRECTS		RELAYDINFO, 1
#define RELAYDREDIRECTENTRY	RELAYDREDIRECTS, 1
#define RELAYDREDIRECTINDEX	RELAYDREDIRECTENTRY, 1
#define RELAYDREDIRECTSTATUS	RELAYDREDIRECTENTRY, 2
#define RELAYDREDIRECTNAME	RELAYDREDIRECTENTRY, 3
#define RELAYDREDIRECTCNT	RELAYDREDIRECTENTRY, 4
#define RELAYDREDIRECTAVG	RELAYDREDIRECTENTRY, 5
#define RELAYDREDIRECTLAST	RELAYDREDIRECTENTRY, 6
#define RELAYDREDIRECTAVGHOUR	RELAYDREDIRECTENTRY, 7
#define RELAYDREDIRECTLASTHOUR	RELAYDREDIRECTENTRY, 8
#define RELAYDREDIRECTAVGDAY	RELAYDREDIRECTENTRY, 9
#define RELAYDREDIRECTLASTDAY	RELAYDREDIRECTENTRY, 10
#define RELAYDRELAYS		RELAYDINFO, 2
#define RELAYDRELAYENTRY	RELAYDRELAYS, 1
#define RELAYDRELAYINDEX	RELAYDRELAYENTRY, 1
#define RELAYDRELAYSTATUS	RELAYDRELAYENTRY, 2
#define RELAYDRELAYNAME		RELAYDRELAYENTRY, 3
#define RELAYDRELAYCNT		RELAYDRELAYENTRY, 4
#define RELAYDRELAYAVG		RELAYDRELAYENTRY, 5
#define RELAYDRELAYLAST		RELAYDRELAYENTRY, 6
#define RELAYDRELAYAVGHOUR	RELAYDRELAYENTRY, 7
#define RELAYDRELAYLASTHOUR	RELAYDRELAYENTRY, 8
#define RELAYDRELAYAVGDAY	RELAYDRELAYENTRY, 9
#define RELAYDRELAYLASTDAY	RELAYDRELAYENTRY, 10
#define RELAYDROUTERS		RELAYDINFO, 3
#define RELAYDROUTERENTRY	RELAYDROUTERS, 1
#define RELAYDROUTERINDEX	RELAYDROUTERENTRY, 1
#define RELAYDROUTERTABLEINDEX	RELAYDROUTERENTRY, 2
#define RELAYDROUTERSTATUS	RELAYDROUTERENTRY, 3
#define RELAYDROUTERNAME	RELAYDROUTERENTRY, 4
#define RELAYDROUTERLABEL	RELAYDROUTERENTRY, 5
#define RELAYDROUTERRTABLE	RELAYDROUTERENTRY, 6
#define RELAYDNETROUTES		RELAYDINFO, 4
#define RELAYDNETROUTEENTRY	RELAYDNETROUTES, 1
#define RELAYDNETROUTEINDEX	RELAYDNETROUTEENTRY, 1
#define RELAYDNETROUTEADDR	RELAYDNETROUTEENTRY, 2
#define RELAYDNETROUTEADDRTYPE	RELAYDNETROUTEENTRY, 3
#define RELAYDNETROUTEPREFIXLEN	RELAYDNETROUTEENTRY, 4
#define RELAYDNETROUTEROUTERINDEX RELAYDNETROUTEENTRY, 5
#define RELAYDHOSTS		RELAYDINFO, 5
#define RELAYDHOSTENTRY		RELAYDHOSTS, 1
#define RELAYDHOSTINDEX		RELAYDHOSTENTRY, 1
#define RELAYDHOSTPARENTINDEX	RELAYDHOSTENTRY, 2
#define RELAYDHOSTTABLEINDEX	RELAYDHOSTENTRY, 3
#define RELAYDHOSTNAME		RELAYDHOSTENTRY, 4
#define RELAYDHOSTADDRESS	RELAYDHOSTENTRY, 5
#define RELAYDHOSTADDRESSTYPE	RELAYDHOSTENTRY, 6
#define RELAYDHOSTSTATUS	RELAYDHOSTENTRY, 7
#define RELAYDHOSTCHECKCNT	RELAYDHOSTENTRY, 8
#define RELAYDHOSTUPCNT		RELAYDHOSTENTRY, 9
#define RELAYDHOSTERRNO		RELAYDHOSTENTRY, 10
#define RELAYDSESSIONS		RELAYDINFO, 6
#define RELAYDSESSIONENTRY	RELAYDSESSIONS, 1
#define RELAYDSESSIONINDEX	RELAYDSESSIONENTRY, 1
#define RELAYDSESSIONRELAYINDEX	RELAYDSESSIONENTRY, 2
#define RELAYDSESSIONINADDR	RELAYDSESSIONENTRY, 3
#define RELAYDSESSIONINADDRTYPE	RELAYDSESSIONENTRY, 4
#define RELAYDSESSIONOUTADDR	RELAYDSESSIONENTRY, 5
#define RELAYDSESSIONOUTADDRTYPE RELAYDSESSIONENTRY, 6
#define RELAYDSESSIONPORTIN	RELAYDSESSIONENTRY, 7
#define RELAYDSESSIONPORTOUT	RELAYDSESSIONENTRY, 8
#define RELAYDSESSIONAGE	RELAYDSESSIONENTRY, 9
#define RELAYDSESSIONIDLE	RELAYDSESSIONENTRY, 10
#define RELAYDSESSIONSTATUS	RELAYDSESSIONENTRY, 11
#define RELAYDSESSIONPID	RELAYDSESSIONENTRY, 12
#define RELAYDTABLES		RELAYDINFO, 7
#define RELAYDTABLEENTRY	RELAYDTABLES, 1
#define RELAYDTABLEINDEX	RELAYDTABLEENTRY, 1
#define RELAYDTABLENAME		RELAYDTABLEENTRY, 2
#define RELAYDTABLESTATUS	RELAYDTABLEENTRY, 3

void agentx_needsock(struct subagentx *, void *, int);

struct relayd *env;

struct subagentx *sa = NULL;
struct subagentx_index *relaydRedirectIdx, *relaydRelayIdx;
struct subagentx_index *relaydRouterIdx, *relaydNetRouteIdx;
struct subagentx_index *relaydHostIdx, *relaydSessionRelayIdx;
struct subagentx_index *relaydSessionIdx, *relaydTableIdx;

struct subagentx_object *relaydRedirectIndex, *relaydRedirectStatus;
struct subagentx_object *relaydRedirectName, *relaydRedirectCnt;
struct subagentx_object *relaydRedirectAvg, *relaydRedirectLast;
struct subagentx_object *relaydRedirectAvgHour, *relaydRedirectLastHour;
struct subagentx_object *relaydRedirectAvgDay, *relaydRedirectLastDay;

struct subagentx_object *relaydRelayIndex, *relaydRelayStatus;
struct subagentx_object *relaydRelayName, *relaydRelayCnt;
struct subagentx_object *relaydRelayAvg, *relaydRelayLast;
struct subagentx_object *relaydRelayAvgHour, *relaydRelayLastHour;
struct subagentx_object *relaydRelayAvgDay, *relaydRelayLastDay;

struct subagentx_object *relaydRouterIndex, *relaydRouterTableIndex;
struct subagentx_object *relaydRouterStatus, *relaydRouterName;
struct subagentx_object *relaydRouterLabel, *relaydRouterRtable;

struct subagentx_object *relaydNetRouteIndex, *relaydNetRouteAddr;
struct subagentx_object *relaydNetRouteAddrType, *relaydNetRoutePrefixLen;
struct subagentx_object *relaydNetRouteRouterIndex;

struct subagentx_object *relaydHostIndex, *relaydHostParentIndex;
struct subagentx_object *relaydHostTableIndex, *relaydHostName;
struct subagentx_object *relaydHostAddress, *relaydHostAddressType;
struct subagentx_object *relaydHostStatus, *relaydHostCheckCnt;
struct subagentx_object *relaydHostUpCnt, *relaydHostErrno;

struct subagentx_object *relaydSessionIndex, *relaydSessionRelayIndex;
struct subagentx_object *relaydSessionInAddr, *relaydSessionInAddrType;
struct subagentx_object *relaydSessionOutAddr, *relaydSessionOutAddrType;
struct subagentx_object *relaydSessionPortIn, *relaydSessionPortOut;
struct subagentx_object *relaydSessionAge, *relaydSessionIdle;
struct subagentx_object *relaydSessionStatus, *relaydSessionPid;

struct subagentx_object *relaydTableIndex, *relaydTableName, *relaydTableStatus;

void	*sstodata(struct sockaddr_storage *);
size_t	 sstolen(struct sockaddr_storage *);

struct rdr *agentx_rdr_byidx(uint32_t, enum subagentx_request_type);
void agentx_redirect(struct subagentx_varbind *);
struct relay *agentx_relay_byidx(uint32_t, enum subagentx_request_type);
void agentx_relay(struct subagentx_varbind *);
struct router *agentx_router_byidx(uint32_t, enum subagentx_request_type);
void agentx_router(struct subagentx_varbind *);
struct netroute *agentx_netroute_byidx(uint32_t, enum subagentx_request_type);
void agentx_netroute(struct subagentx_varbind *);
struct host *agentx_host_byidx(uint32_t, enum subagentx_request_type);
void agentx_host(struct subagentx_varbind *);
struct rsession *agentx_session_byidx(uint32_t, uint32_t,
    enum subagentx_request_type);
void agentx_session(struct subagentx_varbind *);
struct table *agentx_table_byidx(uint32_t, enum subagentx_request_type);
void agentx_table(struct subagentx_varbind *);

void	 agentx_sock(int, short, void *);
#if 0
int	 snmp_element(const char *, enum snmp_type, void *, int64_t,
	    struct agentx_pdu *);
int	 snmp_string2oid(const char *, struct snmp_oid *);
#endif

void
agentx_init(struct relayd *nenv)
{
	struct subagentx_session *sas;
	struct subagentx_context *sac;
	struct subagentx_region *sar;
	struct subagentx_index *session_idxs[2];

	subagentx_log_fatal = fatalx;
	subagentx_log_warn = log_warnx;
	subagentx_log_info = log_info;
	subagentx_log_debug = log_debug;

	env = nenv;

	if ((env->sc_conf.flags & F_AGENTX) == 0) {
		if (sa != NULL)
			subagentx_free(sa);
		return;
	}
	if (sa != NULL)
		return;

	if ((sa = subagentx(agentx_needsock, NULL)) == NULL)
		fatal("%s: agentx alloc", __func__);
	if ((sas = subagentx_session(sa, NULL, 0, "relayd", 0)) == NULL)
		fatal("%s: agentx session alloc", __func__);
	if ((sac = subagentx_context(sas,
		env->sc_conf.agentx_context[0] == '\0' ? NULL :
		env->sc_conf.agentx_context)) == NULL)
		fatal("%s: agentx context alloc", __func__);
	sar = subagentx_region(sac, SUBAGENTX_OID(RELAYDINFO), 0);
	if (sar == NULL)
		fatal("%s: agentx region alloc", __func__);
	if ((relaydRedirectIdx = subagentx_index_integer_dynamic(sar,
	    SUBAGENTX_OID(RELAYDREDIRECTINDEX))) == NULL ||
	    (relaydRelayIdx = subagentx_index_integer_dynamic(sar,
	    SUBAGENTX_OID(RELAYDRELAYINDEX))) == NULL ||
	    (relaydRouterIdx = subagentx_index_integer_dynamic(sar,
	    SUBAGENTX_OID(RELAYDROUTERINDEX))) == NULL ||
	    (relaydNetRouteIdx = subagentx_index_integer_dynamic(sar,
	    SUBAGENTX_OID(RELAYDNETROUTEINDEX))) == NULL ||
	    (relaydHostIdx = subagentx_index_integer_dynamic(sar,
	    SUBAGENTX_OID(RELAYDHOSTINDEX))) == NULL ||
	    (relaydSessionIdx = subagentx_index_integer_dynamic(sar,
	    SUBAGENTX_OID(RELAYDSESSIONINDEX))) == NULL ||
	    (relaydSessionRelayIdx = subagentx_index_integer_dynamic(sar,
	    SUBAGENTX_OID(RELAYDSESSIONRELAYINDEX))) == NULL ||
	    (relaydTableIdx = subagentx_index_integer_dynamic(sar,
	    SUBAGENTX_OID(RELAYDTABLEINDEX))) == NULL)
		fatal("%s: agentx index alloc", __func__);
	session_idxs[0] = relaydSessionRelayIdx;
	session_idxs[1] = relaydSessionIdx;
	if ((relaydRedirectIndex = subagentx_object(sar,
	    SUBAGENTX_OID(RELAYDREDIRECTINDEX), &relaydRedirectIdx, 1, 0,
	    agentx_redirect)) == NULL ||
	    (relaydRedirectStatus = subagentx_object(sar,
	    SUBAGENTX_OID(RELAYDREDIRECTSTATUS), &relaydRedirectIdx, 1, 0,
	    agentx_redirect)) == NULL ||
	    (relaydRedirectName = subagentx_object(sar,
	    SUBAGENTX_OID(RELAYDREDIRECTNAME), &relaydRedirectIdx, 1, 0,
	    agentx_redirect)) == NULL ||
	    (relaydRedirectCnt = subagentx_object(sar,
	    SUBAGENTX_OID(RELAYDREDIRECTCNT), &relaydRedirectIdx, 1, 0,
	    agentx_redirect)) == NULL ||
	    (relaydRedirectAvg = subagentx_object(sar,
	    SUBAGENTX_OID(RELAYDREDIRECTAVG), &relaydRedirectIdx, 1, 0,
	    agentx_redirect)) == NULL ||
	    (relaydRedirectLast = subagentx_object(sar,
	    SUBAGENTX_OID(RELAYDREDIRECTLAST), &relaydRedirectIdx, 1, 0,
	    agentx_redirect)) == NULL ||
	    (relaydRedirectAvgHour = subagentx_object(sar,
	    SUBAGENTX_OID(RELAYDREDIRECTAVGHOUR), &relaydRedirectIdx, 1, 0,
	    agentx_redirect)) == NULL ||
	    (relaydRedirectLastHour = subagentx_object(sar,
	    SUBAGENTX_OID(RELAYDREDIRECTLASTHOUR), &relaydRedirectIdx, 1, 0,
	    agentx_redirect)) == NULL ||
	    (relaydRedirectAvgDay = subagentx_object(sar,
	    SUBAGENTX_OID(RELAYDREDIRECTAVGDAY), &relaydRedirectIdx, 1, 0,
	    agentx_redirect)) == NULL ||
	    (relaydRedirectLastDay = subagentx_object(sar,
	    SUBAGENTX_OID(RELAYDREDIRECTLASTDAY), &relaydRedirectIdx, 1, 0,
	    agentx_redirect)) == NULL ||
	    (relaydRelayIndex = subagentx_object(sar,
	    SUBAGENTX_OID(RELAYDRELAYINDEX), &relaydRelayIdx, 1, 0,
	    agentx_relay)) == NULL ||
	    (relaydRelayStatus = subagentx_object(sar,
	    SUBAGENTX_OID(RELAYDRELAYSTATUS), &relaydRelayIdx, 1, 0,
	    agentx_relay)) == NULL ||
	    (relaydRelayName = subagentx_object(sar,
	    SUBAGENTX_OID(RELAYDRELAYNAME), &relaydRelayIdx, 1, 0,
	    agentx_relay)) == NULL ||
	    (relaydRelayCnt = subagentx_object(sar,
	    SUBAGENTX_OID(RELAYDRELAYCNT), &relaydRelayIdx, 1, 0,
	    agentx_relay)) == NULL ||
	    (relaydRelayAvg = subagentx_object(sar,
	    SUBAGENTX_OID(RELAYDRELAYAVG), &relaydRelayIdx, 1, 0,
	    agentx_relay)) == NULL ||
	    (relaydRelayLast = subagentx_object(sar,
	    SUBAGENTX_OID(RELAYDRELAYLAST), &relaydRelayIdx, 1, 0,
	    agentx_relay)) == NULL ||
	    (relaydRelayAvgHour = subagentx_object(sar,
	    SUBAGENTX_OID(RELAYDRELAYAVGHOUR), &relaydRelayIdx, 1, 0,
	    agentx_relay)) == NULL ||
	    (relaydRelayLastHour = subagentx_object(sar,
	    SUBAGENTX_OID(RELAYDRELAYLASTHOUR), &relaydRelayIdx, 1, 0,
	    agentx_relay)) == NULL ||
	    (relaydRelayAvgDay = subagentx_object(sar,
	    SUBAGENTX_OID(RELAYDRELAYAVGDAY), &relaydRelayIdx, 1, 0,
	    agentx_relay)) == NULL ||
	    (relaydRelayLastDay = subagentx_object(sar,
	    SUBAGENTX_OID(RELAYDRELAYLASTDAY), &relaydRelayIdx, 1, 0,
	    agentx_relay)) == NULL ||
	    (relaydRouterIndex = subagentx_object(sar,
	    SUBAGENTX_OID(RELAYDROUTERINDEX), &relaydRouterIdx, 1, 0,
	    agentx_router)) == NULL ||
	    (relaydRouterTableIndex = subagentx_object(sar,
	    SUBAGENTX_OID(RELAYDROUTERTABLEINDEX), &relaydRouterIdx, 1, 0,
	    agentx_router)) == NULL ||
	    (relaydRouterStatus = subagentx_object(sar,
	    SUBAGENTX_OID(RELAYDROUTERSTATUS), &relaydRouterIdx, 1, 0,
	    agentx_router)) == NULL ||
	    (relaydRouterName = subagentx_object(sar,
	    SUBAGENTX_OID(RELAYDROUTERNAME), &relaydRouterIdx, 1, 0,
	    agentx_router)) == NULL ||
	    (relaydRouterLabel = subagentx_object(sar,
	    SUBAGENTX_OID(RELAYDROUTERLABEL), &relaydRouterIdx, 1, 0,
	    agentx_router)) == NULL ||
	    (relaydRouterRtable = subagentx_object(sar,
	    SUBAGENTX_OID(RELAYDROUTERRTABLE), &relaydRouterIdx, 1, 0,
	    agentx_router)) == NULL ||
	    (relaydNetRouteIndex = subagentx_object(sar,
	    SUBAGENTX_OID(RELAYDNETROUTEINDEX), &relaydNetRouteIdx, 1, 0,
	    agentx_netroute)) == NULL ||
	    (relaydNetRouteAddr = subagentx_object(sar,
	    SUBAGENTX_OID(RELAYDNETROUTEADDR), &relaydNetRouteIdx, 1, 0,
	    agentx_netroute)) == NULL ||
	    (relaydNetRouteAddrType = subagentx_object(sar,
	    SUBAGENTX_OID(RELAYDNETROUTEADDRTYPE), &relaydNetRouteIdx, 1, 0,
	    agentx_netroute)) == NULL ||
	    (relaydNetRoutePrefixLen = subagentx_object(sar,
	    SUBAGENTX_OID(RELAYDNETROUTEPREFIXLEN), &relaydNetRouteIdx, 1, 0,
	    agentx_netroute)) == NULL ||
	    (relaydNetRouteRouterIndex = subagentx_object(sar,
	    SUBAGENTX_OID(RELAYDNETROUTEROUTERINDEX), &relaydNetRouteIdx, 1, 0,
	    agentx_netroute)) == NULL ||
	    (relaydHostIndex = subagentx_object(sar,
	    SUBAGENTX_OID(RELAYDHOSTINDEX), &relaydHostIdx, 1, 0,
	    agentx_host)) == NULL ||
	    (relaydHostParentIndex = subagentx_object(sar,
	    SUBAGENTX_OID(RELAYDHOSTPARENTINDEX), &relaydHostIdx, 1, 0,
	    agentx_host)) == NULL ||
	    (relaydHostTableIndex = subagentx_object(sar,
	    SUBAGENTX_OID(RELAYDHOSTTABLEINDEX), &relaydHostIdx, 1, 0,
	    agentx_host)) == NULL ||
	    (relaydHostName = subagentx_object(sar,
	    SUBAGENTX_OID(RELAYDHOSTNAME), &relaydHostIdx, 1, 0,
	    agentx_host)) == NULL ||
	    (relaydHostAddress = subagentx_object(sar,
	    SUBAGENTX_OID(RELAYDHOSTADDRESS), &relaydHostIdx, 1, 0,
	    agentx_host)) == NULL ||
	    (relaydHostAddressType = subagentx_object(sar,
	    SUBAGENTX_OID(RELAYDHOSTADDRESSTYPE), &relaydHostIdx, 1, 0,
	    agentx_host)) == NULL ||
	    (relaydHostStatus = subagentx_object(sar,
	    SUBAGENTX_OID(RELAYDHOSTSTATUS), &relaydHostIdx, 1, 0,
	    agentx_host)) == NULL ||
	    (relaydHostCheckCnt = subagentx_object(sar,
	    SUBAGENTX_OID(RELAYDHOSTCHECKCNT), &relaydHostIdx, 1, 0,
	    agentx_host)) == NULL ||
	    (relaydHostUpCnt = subagentx_object(sar,
	    SUBAGENTX_OID(RELAYDHOSTUPCNT), &relaydHostIdx, 1, 0,
	    agentx_host)) == NULL ||
	    (relaydHostErrno = subagentx_object(sar,
	    SUBAGENTX_OID(RELAYDHOSTERRNO), &relaydHostIdx, 1, 0,
	    agentx_host)) == NULL ||
	    (relaydSessionIndex = subagentx_object(sar,
	    SUBAGENTX_OID(RELAYDSESSIONINDEX), session_idxs, 2, 0,
	    agentx_session)) == NULL ||
	    (relaydSessionRelayIndex = subagentx_object(sar,
	    SUBAGENTX_OID(RELAYDSESSIONRELAYINDEX), session_idxs, 2, 0,
	    agentx_session)) == NULL ||
	    (relaydSessionInAddr = subagentx_object(sar,
	    SUBAGENTX_OID(RELAYDSESSIONINADDR), session_idxs, 2, 0,
	    agentx_session)) == NULL ||
	    (relaydSessionInAddrType = subagentx_object(sar,
	    SUBAGENTX_OID(RELAYDSESSIONINADDRTYPE), session_idxs, 2, 0,
	    agentx_session)) == NULL ||
	    (relaydSessionOutAddr = subagentx_object(sar,
	    SUBAGENTX_OID(RELAYDSESSIONOUTADDR), session_idxs, 2, 0,
	    agentx_session)) == NULL ||
	    (relaydSessionOutAddrType = subagentx_object(sar,
	    SUBAGENTX_OID(RELAYDSESSIONOUTADDRTYPE), session_idxs, 2, 0,
	    agentx_session)) == NULL ||
	    (relaydSessionPortIn = subagentx_object(sar,
	    SUBAGENTX_OID(RELAYDSESSIONPORTIN), session_idxs, 2, 0,
	    agentx_session)) == NULL ||
	    (relaydSessionPortOut = subagentx_object(sar,
	    SUBAGENTX_OID(RELAYDSESSIONPORTOUT), session_idxs, 2, 0,
	    agentx_session)) == NULL ||
	    (relaydSessionAge = subagentx_object(sar,
	    SUBAGENTX_OID(RELAYDSESSIONAGE), session_idxs, 2, 0,
	    agentx_session)) == NULL ||
	    (relaydSessionIdle = subagentx_object(sar,
	    SUBAGENTX_OID(RELAYDSESSIONIDLE), session_idxs, 2, 0,
	    agentx_session)) == NULL ||
	    (relaydSessionStatus = subagentx_object(sar,
	    SUBAGENTX_OID(RELAYDSESSIONSTATUS), session_idxs, 2, 0,
	    agentx_session)) == NULL ||
	    (relaydSessionPid = subagentx_object(sar,
	    SUBAGENTX_OID(RELAYDSESSIONPID), session_idxs, 2, 0,
	    agentx_session)) == NULL ||
	    (relaydTableIndex = subagentx_object(sar,
	    SUBAGENTX_OID(RELAYDTABLEINDEX), &relaydTableIdx, 1, 0,
	    agentx_table)) == NULL ||
	    (relaydTableName = subagentx_object(sar,
	    SUBAGENTX_OID(RELAYDTABLENAME), &relaydTableIdx, 1, 0,
	    agentx_table)) == NULL ||
	    (relaydTableStatus = subagentx_object(sar,
	    SUBAGENTX_OID(RELAYDTABLESTATUS), &relaydTableIdx, 1, 0,
	    agentx_table)) == NULL)
		fatal("%s: agentx object alloc", __func__);
}

void
agentx_needsock(struct subagentx *usa, void *cookie, int fd)
{
	proc_compose(env->sc_ps, PROC_PARENT, IMSG_AGENTXSOCK, NULL, 0);
}

void
agentx_setsock(struct relayd *lenv, enum privsep_procid id)
{
	struct sockaddr_un	 sun;
	int			 s = -1;

	if ((s = socket(AF_UNIX, SOCK_STREAM | SOCK_NONBLOCK, 0)) == -1)
		goto done;

	bzero(&sun, sizeof(sun));
	sun.sun_family = AF_UNIX;
	if (strlcpy(sun.sun_path, lenv->sc_conf.agentx_path,
	    sizeof(sun.sun_path)) >= sizeof(sun.sun_path))
		fatalx("invalid socket path");

	if (connect(s, (struct sockaddr *)&sun, sizeof(sun)) == -1) {
		close(s);
		s = -1;
	}
 done:
	proc_compose_imsg(lenv->sc_ps, id, -1, IMSG_AGENTXSOCK, -1, s, NULL, 0);
}

int
agentx_getsock(struct imsg *imsg)
{
	struct timeval		 tv = AGENTX_RECONNECT_TIMEOUT;

	if (imsg->fd == -1)
		goto retry;

	event_del(&(env->sc_agentxev));
	event_set(&(env->sc_agentxev), imsg->fd, EV_READ | EV_PERSIST,
	    agentx_sock, env);
	event_add(&(env->sc_agentxev), NULL);

	subagentx_connect(sa, imsg->fd);

	return 0;
 retry:
	evtimer_set(&env->sc_agentxev, agentx_sock, env);
	evtimer_add(&env->sc_agentxev, &tv);
	return 0;
}

void
agentx_sock(int fd, short event, void *arg)
{
	if (event & EV_TIMEOUT) {
		proc_compose(env->sc_ps, PROC_PARENT, IMSG_AGENTXSOCK, NULL, 0);
		return;
	}
	if (event & EV_WRITE) {
		event_del(&(env->sc_agentxev));
		event_set(&(env->sc_agentxev), fd, EV_READ | EV_PERSIST,
		    agentx_sock, NULL);
		event_add(&(env->sc_agentxev), NULL);
		subagentx_write(sa);
	}
	if (event & EV_READ)
		subagentx_read(sa);
	return;
}

void *
sstodata(struct sockaddr_storage *ss)
{
	if (ss->ss_family == AF_INET)
		return &((struct sockaddr_in *)ss)->sin_addr;
	if (ss->ss_family == AF_INET6)
		return &((struct sockaddr_in6 *)ss)->sin6_addr;
	return NULL;
}

size_t
sstolen(struct sockaddr_storage *ss)
{
	if (ss->ss_family == AF_INET)
		return sizeof(((struct sockaddr_in *)ss)->sin_addr);
	if (ss->ss_family == AF_INET6)
		return sizeof(((struct sockaddr_in6 *)ss)->sin6_addr);
	return 0;
}

struct rdr *
agentx_rdr_byidx(uint32_t instanceidx, enum subagentx_request_type type)
{
	struct rdr	*rdr;

	TAILQ_FOREACH(rdr, env->sc_rdrs, entry) {
		if (rdr->conf.id == instanceidx) {
			if (type == SUBAGENTX_REQUEST_TYPE_GET ||
			    type == SUBAGENTX_REQUEST_TYPE_GETNEXTINCLUSIVE)
				return rdr;
			else
				return TAILQ_NEXT(rdr, entry);
		} else if (rdr->conf.id > instanceidx) {
			if (type == SUBAGENTX_REQUEST_TYPE_GET)
				return NULL;
			return rdr;
		}
	}

	return NULL;
}


void
agentx_redirect(struct subagentx_varbind *sav)
{
	struct rdr	*rdr;

	rdr = agentx_rdr_byidx(subagentx_varbind_get_index_integer(sav,
	    relaydRedirectIdx), subagentx_varbind_request(sav));
	if (rdr == NULL) {
		subagentx_varbind_notfound(sav);
		return;
	}
	subagentx_varbind_set_index_integer(sav, relaydRedirectIdx,
	    rdr->conf.id);
	if (subagentx_varbind_get_object(sav) == relaydRedirectIndex)
		subagentx_varbind_integer(sav, rdr->conf.id);
	else if (subagentx_varbind_get_object(sav) == relaydRedirectStatus) {
		if (rdr->conf.flags & F_DISABLE)
			subagentx_varbind_integer(sav, 1);
		else if (rdr->conf.flags & F_DOWN)
			subagentx_varbind_integer(sav, 2);
		else if (rdr->conf.flags & F_BACKUP)
			subagentx_varbind_integer(sav, 3);
		else
			subagentx_varbind_integer(sav, 0);
	} else if (subagentx_varbind_get_object(sav) == relaydRedirectName)
		subagentx_varbind_string(sav, rdr->conf.name);
	else if (subagentx_varbind_get_object(sav) == relaydRedirectCnt)
		subagentx_varbind_counter64(sav, rdr->stats.cnt);
	else if (subagentx_varbind_get_object(sav) == relaydRedirectAvg)
		subagentx_varbind_gauge32(sav, rdr->stats.avg);
	else if (subagentx_varbind_get_object(sav) == relaydRedirectLast)
		subagentx_varbind_gauge32(sav, rdr->stats.last);
	else if (subagentx_varbind_get_object(sav) == relaydRedirectAvgHour)
		subagentx_varbind_gauge32(sav, rdr->stats.avg_hour);
	else if (subagentx_varbind_get_object(sav) == relaydRedirectLastHour)
		subagentx_varbind_gauge32(sav, rdr->stats.last_hour);
	else if (subagentx_varbind_get_object(sav) == relaydRedirectAvgDay)
		subagentx_varbind_gauge32(sav, rdr->stats.avg_day);
	else if (subagentx_varbind_get_object(sav) == relaydRedirectLastDay)
		subagentx_varbind_gauge32(sav, rdr->stats.last_day);
}

struct relay *
agentx_relay_byidx(uint32_t instanceidx, enum subagentx_request_type type)
{
	struct relay	*rly;

	TAILQ_FOREACH(rly, env->sc_relays, rl_entry) {
		if (rly->rl_conf.id == instanceidx) {
			if (type == SUBAGENTX_REQUEST_TYPE_GET ||
			    type == SUBAGENTX_REQUEST_TYPE_GETNEXTINCLUSIVE)
				return rly;
			else
				return TAILQ_NEXT(rly, rl_entry);
		} else if (rly->rl_conf.id > instanceidx) {
			if (type == SUBAGENTX_REQUEST_TYPE_GET)
				return NULL;
			return rly;
		}
	}

	return NULL;
}

void
agentx_relay(struct subagentx_varbind *sav)
{
	struct relay	*rly;
	uint64_t	 value = 0;
	int		 i, nrelay = env->sc_conf.prefork_relay;

	rly = agentx_relay_byidx(subagentx_varbind_get_index_integer(sav,
	    relaydRelayIdx), subagentx_varbind_request(sav));
	if (rly == NULL) {
		subagentx_varbind_notfound(sav);
		return;
	}
	subagentx_varbind_set_index_integer(sav, relaydRelayIdx,
	    rly->rl_conf.id);
	if (subagentx_varbind_get_object(sav) == relaydRelayIndex)
		subagentx_varbind_integer(sav, rly->rl_conf.id);
	else if (subagentx_varbind_get_object(sav) == relaydRelayStatus) {
		if (rly->rl_up == HOST_UP)
			subagentx_varbind_integer(sav, 1);
		else
			subagentx_varbind_integer(sav, 0);
	} else if (subagentx_varbind_get_object(sav) == relaydRelayName)
		subagentx_varbind_string(sav, rly->rl_conf.name);
	else if (subagentx_varbind_get_object(sav) == relaydRelayCnt) {
		for (i = 0; i < nrelay; i++)
			value += rly->rl_stats[i].cnt;
		subagentx_varbind_counter64(sav, value);
	} else if (subagentx_varbind_get_object(sav) == relaydRelayAvg) {
		for (i = 0; i < nrelay; i++)
			value += rly->rl_stats[i].avg;
		subagentx_varbind_gauge32(sav, (uint32_t)value);
	} else if (subagentx_varbind_get_object(sav) == relaydRelayLast) {
		for (i = 0; i < nrelay; i++)
			value += rly->rl_stats[i].last;
		subagentx_varbind_gauge32(sav, (uint32_t)value);
	} else if (subagentx_varbind_get_object(sav) == relaydRelayAvgHour) {
		for (i = 0; i < nrelay; i++)
			value += rly->rl_stats[i].avg_hour;
		subagentx_varbind_gauge32(sav, (uint32_t)value);
	} else if (subagentx_varbind_get_object(sav) == relaydRelayLastHour) {
		for (i = 0; i < nrelay; i++)
			value += rly->rl_stats[i].last_hour;
		subagentx_varbind_gauge32(sav, (uint32_t)value);
	} else if (subagentx_varbind_get_object(sav) == relaydRelayAvgDay) {
		for (i = 0; i < nrelay; i++)
			value += rly->rl_stats[i].avg_day;
		subagentx_varbind_gauge32(sav, (uint32_t)value);
	} else if (subagentx_varbind_get_object(sav) == relaydRelayLastDay) {
		for (i = 0; i < nrelay; i++)
			value += rly->rl_stats[i].last_day;
		subagentx_varbind_gauge32(sav, (uint32_t)value);
	}
}

struct router *
agentx_router_byidx(uint32_t instanceidx, enum subagentx_request_type type)
{
	struct router	*router;

	TAILQ_FOREACH(router, env->sc_rts, rt_entry) {
		if (router->rt_conf.id == instanceidx) {
			if (type == SUBAGENTX_REQUEST_TYPE_GET ||
			    type == SUBAGENTX_REQUEST_TYPE_GETNEXTINCLUSIVE)
				return router;
			else
				return TAILQ_NEXT(router, rt_entry);
		} else if (router->rt_conf.id > instanceidx) {
			if (type == SUBAGENTX_REQUEST_TYPE_GET)
				return NULL;
			return router;
		}
	}

	return NULL;
}

void
agentx_router(struct subagentx_varbind *sav)
{
	struct router	*router;

	router = agentx_router_byidx(subagentx_varbind_get_index_integer(sav,
	    relaydRouterIdx), subagentx_varbind_request(sav));
	if (router == NULL) {
		subagentx_varbind_notfound(sav);
		return;
	}
	subagentx_varbind_set_index_integer(sav, relaydRouterIdx,
	    router->rt_conf.id);
	if (subagentx_varbind_get_object(sav) == relaydRouterIndex)
		subagentx_varbind_integer(sav, router->rt_conf.id);
	else if (subagentx_varbind_get_object(sav) == relaydRouterTableIndex)
		subagentx_varbind_integer(sav, router->rt_conf.gwtable);
	else if (subagentx_varbind_get_object(sav) == relaydRouterStatus) {
		if (router->rt_conf.flags & F_DISABLE)
			subagentx_varbind_integer(sav, 1);
		else
			subagentx_varbind_integer(sav, 0);
	} else if (subagentx_varbind_get_object(sav) == relaydRouterName)
		subagentx_varbind_string(sav, router->rt_conf.name);
	else if (subagentx_varbind_get_object(sav) == relaydRouterLabel)
		subagentx_varbind_string(sav, router->rt_conf.label);
	else if (subagentx_varbind_get_object(sav) == relaydRouterRtable)
		subagentx_varbind_integer(sav, router->rt_conf.rtable);
}

struct netroute *
agentx_netroute_byidx(uint32_t instanceidx, enum subagentx_request_type type)
{
	struct netroute		*nr;

	TAILQ_FOREACH(nr, env->sc_routes, nr_route) {
		if (nr->nr_conf.id == instanceidx) {
			if (type == SUBAGENTX_REQUEST_TYPE_GET ||
			    type == SUBAGENTX_REQUEST_TYPE_GETNEXTINCLUSIVE)
				return nr;
			else
				return TAILQ_NEXT(nr, nr_entry);
		} else if (nr->nr_conf.id > instanceidx) {
			if (type == SUBAGENTX_REQUEST_TYPE_GET)
				return NULL;
			return nr;
		}
	}

	return NULL;
}

void
agentx_netroute(struct subagentx_varbind *sav)
{
	struct netroute	*nr;

	nr = agentx_netroute_byidx(subagentx_varbind_get_index_integer(sav,
	    relaydNetRouteIdx), subagentx_varbind_request(sav));
	if (nr == NULL) {
		subagentx_varbind_notfound(sav);
		return;
	}
	subagentx_varbind_set_index_integer(sav, relaydNetRouteIdx,
	    nr->nr_conf.id);
	if (subagentx_varbind_get_object(sav) == relaydNetRouteIndex)
		subagentx_varbind_integer(sav, nr->nr_conf.id);
	else if (subagentx_varbind_get_object(sav) == relaydNetRouteAddr)
		subagentx_varbind_nstring(sav, sstodata(&nr->nr_conf.ss),
		    sstolen(&nr->nr_conf.ss));
	else if (subagentx_varbind_get_object(sav) == relaydNetRouteAddrType) {
		if (nr->nr_conf.ss.ss_family == AF_INET)
			subagentx_varbind_integer(sav, 1);
		else if (nr->nr_conf.ss.ss_family == AF_INET6)
			subagentx_varbind_integer(sav, 2);
	} else if (subagentx_varbind_get_object(sav) == relaydNetRoutePrefixLen)
		subagentx_varbind_integer(sav, nr->nr_conf.prefixlen);
	else if (subagentx_varbind_get_object(sav) == relaydNetRouteRouterIndex)
		subagentx_varbind_integer(sav, nr->nr_conf.routerid);
}

struct host *
agentx_host_byidx(uint32_t instanceidx, enum subagentx_request_type type)
{
	struct host		*host;

	TAILQ_FOREACH(host, &(env->sc_hosts), globalentry) {
		if (host->conf.id == instanceidx) {
			if (type == SUBAGENTX_REQUEST_TYPE_GET ||
			    type == SUBAGENTX_REQUEST_TYPE_GETNEXTINCLUSIVE)
				return host;
			else
				return TAILQ_NEXT(host, globalentry);
		} else if (host->conf.id > instanceidx) {
			if (type == SUBAGENTX_REQUEST_TYPE_GET)
				return NULL;
			return host;
		}
	}

	return NULL;
}

void
agentx_host(struct subagentx_varbind *sav)
{
	struct host	*host;

	host = agentx_host_byidx(subagentx_varbind_get_index_integer(sav,
	    relaydHostIdx), subagentx_varbind_request(sav));
	if (host == NULL) {
		subagentx_varbind_notfound(sav);
		return;
	}
	subagentx_varbind_set_index_integer(sav, relaydHostIdx,
	    host->conf.id);
	if (subagentx_varbind_get_object(sav) == relaydHostIndex)
		subagentx_varbind_integer(sav, host->conf.id);
	else if (subagentx_varbind_get_object(sav) == relaydHostParentIndex)
		subagentx_varbind_integer(sav, host->conf.parentid);
	else if (subagentx_varbind_get_object(sav) == relaydHostTableIndex)
		subagentx_varbind_integer(sav, host->conf.tableid);
	else if (subagentx_varbind_get_object(sav) == relaydHostName)
		subagentx_varbind_string(sav, host->conf.name);
	else if (subagentx_varbind_get_object(sav) == relaydHostAddress)
		subagentx_varbind_nstring(sav, sstodata(&host->conf.ss),
		    sstolen(&host->conf.ss));
	else if (subagentx_varbind_get_object(sav) == relaydHostAddressType) {
		if (host->conf.ss.ss_family == AF_INET)
			subagentx_varbind_integer(sav, 1);
		else if (host->conf.ss.ss_family == AF_INET6)
			subagentx_varbind_integer(sav, 2);
	} else if (subagentx_varbind_get_object(sav) == relaydHostStatus) {
		if (host->flags & F_DISABLE)
			subagentx_varbind_integer(sav, 1);
		else if (host->up == HOST_UP)
			subagentx_varbind_integer(sav, 0);
		else if (host->up == HOST_DOWN)
			subagentx_varbind_integer(sav, 2);
		else
			subagentx_varbind_integer(sav, 3);
	} else if (subagentx_varbind_get_object(sav) == relaydHostCheckCnt)
		subagentx_varbind_counter64(sav, host->check_cnt);
	else if (subagentx_varbind_get_object(sav) == relaydHostUpCnt)
		subagentx_varbind_counter64(sav, host->up_cnt);
	else if (subagentx_varbind_get_object(sav) == relaydHostErrno)
		subagentx_varbind_integer(sav, host->he);
}

/*
 * Every session is spawned in one of multiple processes.
 * However, there is no central session id registration, so not every session
 * is shown here
 */
struct rsession *
agentx_session_byidx(uint32_t sessidx, uint32_t relayidx,
    enum subagentx_request_type type)
{
	struct rsession		*session;

	TAILQ_FOREACH(session, &(env->sc_sessions), se_entry) {
		if (session->se_id == sessidx) {
			if (type == SUBAGENTX_REQUEST_TYPE_GET) {
				if (relayidx != session->se_relayid)
					return NULL;
				return session;
			}
			if (type == SUBAGENTX_REQUEST_TYPE_GETNEXTINCLUSIVE) {
				if (relayidx <= session->se_relayid)
					return session;
				return TAILQ_NEXT(session, se_entry);
			}
			if (relayidx < session->se_relayid)
				return session;
			return TAILQ_NEXT(session, se_entry);
		} else if (session->se_id > sessidx) {
			if (type == SUBAGENTX_REQUEST_TYPE_GET)
				return NULL;
			return session;
		}
	}

	return NULL;
}

void
agentx_session(struct subagentx_varbind *sav)
{
	struct timeval	 tv, now;
	struct rsession	*session;

	session = agentx_session_byidx(subagentx_varbind_get_index_integer(sav,
	    relaydSessionIdx), subagentx_varbind_get_index_integer(sav,
	    relaydSessionRelayIdx), subagentx_varbind_request(sav));
	if (session == NULL) {
		subagentx_varbind_notfound(sav);
		return;
	}

	subagentx_varbind_set_index_integer(sav, relaydSessionIdx,
	    session->se_id);
	subagentx_varbind_set_index_integer(sav, relaydSessionRelayIdx,
	    session->se_relayid);
	if (subagentx_varbind_get_object(sav) == relaydSessionIndex)
		subagentx_varbind_integer(sav, session->se_id);
	else if (subagentx_varbind_get_object(sav) == relaydSessionRelayIndex)
		subagentx_varbind_integer(sav, session->se_relayid);
	else if (subagentx_varbind_get_object(sav) == relaydSessionInAddr)
		subagentx_varbind_nstring(sav, sstodata(&(session->se_in.ss)),
		    sstolen(&(session->se_in.ss)));
	else if (subagentx_varbind_get_object(sav) == relaydSessionInAddrType) {
		if (session->se_in.ss.ss_family == AF_INET)
			subagentx_varbind_integer(sav, 1);
		else if (session->se_in.ss.ss_family == AF_INET6)
			subagentx_varbind_integer(sav, 2);
	} else if (subagentx_varbind_get_object(sav) == relaydSessionOutAddr)
		subagentx_varbind_nstring(sav, sstodata(&(session->se_out.ss)),
		    sstolen(&(session->se_out.ss)));
	else if (subagentx_varbind_get_object(sav) == relaydSessionOutAddrType) {
		if (session->se_out.ss.ss_family == AF_INET)
			subagentx_varbind_integer(sav, 1);
		else if (session->se_out.ss.ss_family == AF_INET6)
			subagentx_varbind_integer(sav, 2);
		else
			subagentx_varbind_integer(sav, 0);
	} else if (subagentx_varbind_get_object(sav) == relaydSessionPortIn) 
		subagentx_varbind_integer(sav, session->se_in.port);
	else if (subagentx_varbind_get_object(sav) == relaydSessionPortOut)
		subagentx_varbind_integer(sav, session->se_out.port);
	else if (subagentx_varbind_get_object(sav) == relaydSessionAge) {
		getmonotime(&now);
		timersub(&now, &session->se_tv_start, &tv);
		subagentx_varbind_timeticks(sav,
		    tv.tv_sec * 100 + tv.tv_usec / 10000);
	} else if (subagentx_varbind_get_object(sav) == relaydSessionIdle) {
		getmonotime(&now);
		timersub(&now, &session->se_tv_last, &tv);
		subagentx_varbind_timeticks(sav,
		    tv.tv_sec * 100 + tv.tv_usec / 10000);
	} else if (subagentx_varbind_get_object(sav) == relaydSessionStatus) {
		if (session->se_done)
			subagentx_varbind_integer(sav, 1);
		else
			subagentx_varbind_integer(sav, 0);
	} else if (subagentx_varbind_get_object(sav) == relaydSessionPid)
		subagentx_varbind_integer(sav, session->se_pid);
}

struct table *
agentx_table_byidx(uint32_t instanceidx, enum subagentx_request_type type)
{
	struct table		*table;

	TAILQ_FOREACH(table, env->sc_tables, entry) {
		if (table->conf.id == instanceidx) {
			if (type == SUBAGENTX_REQUEST_TYPE_GET ||
			    type == SUBAGENTX_REQUEST_TYPE_GETNEXTINCLUSIVE)
				return table;
			else
				return TAILQ_NEXT(table, entry);
		} else if (table->conf.id > instanceidx) {
			if (type == SUBAGENTX_REQUEST_TYPE_GET)
				return NULL;
			return table;
		}
	}

	return NULL;
}

void
agentx_table(struct subagentx_varbind *sav)
{
	struct table	*table;

	table = agentx_table_byidx(subagentx_varbind_get_index_integer(sav,
	    relaydTableIdx), subagentx_varbind_request(sav));
	if (table == NULL) {
		subagentx_varbind_notfound(sav);
		return;
	}
	subagentx_varbind_set_index_integer(sav, relaydTableIdx,
	    table->conf.id);
	if (subagentx_varbind_get_object(sav) == relaydTableIndex)
		subagentx_varbind_integer(sav, table->conf.id);
	else if (subagentx_varbind_get_object(sav) == relaydTableName)
		subagentx_varbind_string(sav, table->conf.name);
	else if (subagentx_varbind_get_object(sav) == relaydTableStatus) {
		if (TAILQ_EMPTY(&table->hosts))
			subagentx_varbind_integer(sav, 1);
		else if (table->conf.flags & F_DISABLE)
			subagentx_varbind_integer(sav, 2);
		else
			subagentx_varbind_integer(sav, 0);
	}

}
#if 0

int
snmp_element(const char *oidstr, enum snmp_type type, void *buf, int64_t val,
    struct agentx_pdu *pdu)
{
	u_int32_t		 d;
	u_int64_t		 l;
	struct snmp_oid		 oid;

	DPRINTF("%s: oid %s type %d buf %p val %lld", __func__,
	    oidstr, type, buf, val);

	if (snmp_string2oid(oidstr, &oid) == -1)
		return -1;

	switch (type) {
	case SNMP_GAUGE32:
	case SNMP_NSAPADDR:
	case SNMP_INTEGER32:
	case SNMP_UINTEGER32:
		d = (u_int32_t)val;
		if (snmp_agentx_varbind(pdu, &oid, AGENTX_INTEGER,
		    &d, sizeof(d)) == -1)
			return -1;
		break;

	case SNMP_COUNTER32:
		d = (u_int32_t)val;
		if (snmp_agentx_varbind(pdu, &oid, AGENTX_COUNTER32,
		    &d, sizeof(d)) == -1)
			return -1;
		break;

	case SNMP_TIMETICKS:
		d = (u_int32_t)val;
		if (snmp_agentx_varbind(pdu, &oid, AGENTX_TIME_TICKS,
		    &d, sizeof(d)) == -1)
			return -1;
		break;

	case SNMP_COUNTER64:
		l = (u_int64_t)val;
		if (snmp_agentx_varbind(pdu, &oid, AGENTX_COUNTER64,
		    &l, sizeof(l)) == -1)
			return -1;
		break;

	case SNMP_IPADDR:
	case SNMP_OPAQUE:
		d = (u_int32_t)val;
		if (snmp_agentx_varbind(pdu, &oid, AGENTX_OPAQUE,
		    buf, strlen(buf)) == -1)
			return -1;
		break;

	case SNMP_OBJECT: {
		struct snmp_oid		oid1;

		if (snmp_string2oid(buf, &oid1) == -1)
			return -1;
		if (snmp_agentx_varbind(pdu, &oid, AGENTX_OBJECT_IDENTIFIER,
		    &oid1, sizeof(oid1)) == -1)
			return -1;
	}

	case SNMP_BITSTRING:
	case SNMP_OCTETSTRING:
		if (snmp_agentx_varbind(pdu, &oid, AGENTX_OCTET_STRING,
		    buf, strlen(buf)) == -1)
			return -1;
		break;

	case SNMP_NULL:
		/* no data beyond the OID itself */
		if (snmp_agentx_varbind(pdu, &oid, AGENTX_NULL,
		    NULL, 0) == -1)
			return -1;
	}

	return 0;
}

/*
 * SNMP traps for relayd
 */

void
snmp_hosttrap(struct relayd *env, struct table *table, struct host *host)
{
	struct agentx_pdu *pdu;

	if (snmp_agentx == NULL || env->sc_snmp == -1)
		return;

	/*
	 * OPENBSD-RELAYD-MIB host status trap
	 * XXX The trap format needs some tweaks and other OIDs
	 */

	if ((pdu = snmp_agentx_notify_pdu(&hosttrapoid)) == NULL)
		return;

	SNMP_ELEMENT(".1.0", SNMP_NULL, NULL, 0, pdu);
	SNMP_ELEMENT(".1.1.0", SNMP_OCTETSTRING, host->conf.name, 0, pdu);
	SNMP_ELEMENT(".1.2.0", SNMP_INTEGER32, NULL, host->up, pdu);
	SNMP_ELEMENT(".1.3.0", SNMP_INTEGER32, NULL, host->last_up, pdu);
	SNMP_ELEMENT(".1.4.0", SNMP_INTEGER32, NULL, host->up_cnt, pdu);
	SNMP_ELEMENT(".1.5.0", SNMP_INTEGER32, NULL, host->check_cnt, pdu);
	SNMP_ELEMENT(".1.6.0", SNMP_OCTETSTRING, table->conf.name, 0, pdu);
	SNMP_ELEMENT(".1.7.0", SNMP_INTEGER32, NULL, table->up, pdu);
	if (!host->conf.retry)
		goto done;
	SNMP_ELEMENT(".1.8.0", SNMP_INTEGER32, NULL, host->conf.retry, pdu);
	SNMP_ELEMENT(".1.9.0", SNMP_INTEGER32, NULL, host->retry_cnt, pdu);

 done:
	snmp_agentx_send(snmp_agentx, pdu);
	snmp_event_add(env, EV_WRITE);
}

int
snmp_string2oid(const char *oidstr, struct snmp_oid *o)
{
	char			*sp, *p, str[BUFSIZ];
	const char		*errstr;

	if (strlcpy(str, oidstr, sizeof(str)) >= sizeof(str))
		return -1;
	bzero(o, sizeof(*o));

	for (p = sp = str; p != NULL; sp = p) {
		if ((p = strpbrk(p, ".-")) != NULL)
			*p++ = '\0';
		o->o_id[o->o_n++] = strtonum(sp, 0, UINT_MAX, &errstr);
		if (errstr || o->o_n > SNMP_MAX_OID_LEN)
			return -1;
	}

	return 0;
}
#endif
