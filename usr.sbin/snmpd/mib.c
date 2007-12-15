/*	$OpenBSD: mib.c,v 1.6 2007/12/15 02:14:30 reyk Exp $	*/

/*
 * Copyright (c) 2007 Reyk Floeter <reyk@vantronix.net>
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

#include <sys/queue.h>
#include <sys/param.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <sys/tree.h>
#include <sys/utsname.h>
#include <sys/sysctl.h>
#include <sys/sensors.h>

#include <net/if.h>
#include <net/if_types.h>
#include <netinet/in.h>
#include <netinet/ip_var.h>
#include <arpa/inet.h>

#include <stdlib.h>
#include <stdio.h>
#include <errno.h>
#include <event.h>
#include <fcntl.h>
#include <string.h>
#include <unistd.h>
#include <pwd.h>

#include "snmpd.h"
#include "mib.h"

extern struct snmpd	*env;

/*
 * Defined in SNMPv2-MIB.txt (RFC 3418)
 */

int	 mib_getsys(struct oid *, struct ber_oid *, struct ber_element **);
int	 mib_getsnmp(struct oid *, struct ber_oid *, struct ber_element **);
int	 mib_sysor(struct oid *, struct ber_oid *, struct ber_element **);
int	 mib_setsnmp(struct oid *, struct ber_oid *, struct ber_element **);

/* base MIB tree */
static struct oid base_mib[] = {
	{ MIB(ISO), "iso" },
	{ MIB(ORG), "org" },
	{ MIB(DOD), "dod" },
	{ MIB(INTERNET), "internet" },
	{ MIB(DIRECTORY), "directory" },
	{ MIB(MGMT), "mgmt" },
	{ MIB(MIB_2), "mib-2", OID_MIB },
	{ MIB(SYSTEM), "system" },
	{ MIB(SYSDESCR), "sysDescr", OID_RD,		mib_getsys },
	{ MIB(SYSOID), "sysOID", OID_RD,		mib_getsys },
	{ MIB(SYSUPTIME), "sysUpTime", OID_RD,		mib_getsys },
	{ MIB(SYSCONTACT), "sysContact", OID_RW,	mib_getsys, mps_setstr },
	{ MIB(SYSNAME), "sysName", OID_RW,		mib_getsys, mps_setstr },
	{ MIB(SYSLOCATION), "sysLocation", OID_RW,	mib_getsys, mps_setstr },
	{ MIB(SYSSERVICES), "sysServices", OID_RS,	mib_getsys },
	{ MIB(SYSORLASTCHANGE), "sysORLastChange", OID_RD, mps_getts },
	{ MIB(SYSORTABLE), "sysORTable" },
	{ MIB(SYSORENTRY), "sysOREntry" },
	{ MIB(SYSORINDEX), "sysORIndex", OID_TRD,	mib_sysor },
	{ MIB(SYSORID), "sysORID", OID_TRD,		mib_sysor },
	{ MIB(SYSORDESCR), "sysORDescr", OID_TRD,	mib_sysor },
	{ MIB(SYSORUPTIME), "sysORUptime", OID_TRD,	mib_sysor },
	{ MIB(TRANSMISSION), "transmission" },
	{ MIB(SNMP), "snmp", OID_MIB },
	{ MIB(SNMPINPKTS), "snmpInPkts", OID_RD,	mib_getsnmp },
	{ MIB(SNMPOUTPKTS), "snmpOutPkts", OID_RD,	mib_getsnmp },
	{ MIB(SNMPINBADVERSIONS), "snmpInBadVersions", OID_RD, mib_getsnmp },
	{ MIB(SNMPINBADCOMNNAMES), "snmpInBadCommunityNames", OID_RD, mib_getsnmp },
	{ MIB(SNMPINBADCOMNUSES), "snmpInBadCommunityUses", OID_RD, mib_getsnmp },
	{ MIB(SNMPINASNPARSEERRS), "snmpInASNParseErrs", OID_RD, mib_getsnmp },
	{ MIB(SNMPINTOOBIGS), "snmpInTooBigs", OID_RD,	mib_getsnmp },
	{ MIB(SNMPINNOSUCHNAMES), "snmpInNoSuchNames", OID_RD, mib_getsnmp },
	{ MIB(SNMPINBADVALUES), "snmpInBadValues", OID_RD, mib_getsnmp },
	{ MIB(SNMPINREADONLYS), "snmpInReadOnlys", OID_RD, mib_getsnmp },
	{ MIB(SNMPINGENERRS), "snmpInGenErrs", OID_RD,	mib_getsnmp },
	{ MIB(SNMPINTOTALREQVARS), "snmpInTotalReqVars", OID_RD, mib_getsnmp },
	{ MIB(SNMPINTOTALSETVARS), "snmpInTotalSetVars", OID_RD, mib_getsnmp },
	{ MIB(SNMPINGETREQUESTS), "snmpInGetRequests", OID_RD, mib_getsnmp },
	{ MIB(SNMPINGETNEXTS), "snmpInGetNexts", OID_RD, mib_getsnmp },
	{ MIB(SNMPINSETREQUESTS), "snmpInSetRequests", OID_RD, mib_getsnmp },
	{ MIB(SNMPINGETRESPONSES), "snmpInGetResponses", OID_RD, mib_getsnmp },
	{ MIB(SNMPINTRAPS), "snmpInTraps", OID_RD,	mib_getsnmp },
	{ MIB(SNMPOUTTOOBIGS), "snmpOutTooBigs", OID_RD, mib_getsnmp },
	{ MIB(SNMPOUTNOSUCHNAMES), "snmpOutNoSuchNames", OID_RD, mib_getsnmp },
	{ MIB(SNMPOUTBADVALUES), "snmpOutBadValues", OID_RD, mib_getsnmp },
	{ MIB(SNMPOUTGENERRS), "snmpOutGenErrs", OID_RD, mib_getsnmp },
	{ MIB(SNMPOUTGETREQUESTS), "snmpOutGetRequests", OID_RD, mib_getsnmp },
	{ MIB(SNMPOUTGETNEXTS), "snmpOutGetNexts", OID_RD, mib_getsnmp },
	{ MIB(SNMPOUTSETREQUESTS), "snmpOutSetRequests", OID_RD, mib_getsnmp },
	{ MIB(SNMPOUTGETRESPONSES), "snmpOutGetResponses", OID_RD, mib_getsnmp },
	{ MIB(SNMPOUTTRAPS), "snmpOutTraps", OID_RD,	mib_getsnmp },
	{ MIB(SNMPENAUTHTRAPS), "snmpEnableAuthenTraps", OID_RW, mib_getsnmp, mib_setsnmp },
	{ MIB(SNMPSILENTDROPS), "snmpSilentDrops", OID_RD, mib_getsnmp },
	{ MIB(SNMPPROXYDROPS), "snmpProxyDrops", OID_RD, mib_getsnmp },
	{ MIB(EXPERIMENTAL), "experimental" },
	{ MIB(PRIVATE), "private" },
	{ MIB(ENTERPRISES), "enterprises" },
	{ MIB(SECURITY), "security" },
	{ MIB(SNMPV2), "snmpV2" },
	{ MIB(SNMPDOMAINS), "snmpDomains" },
	{ MIB(SNMPPROXIES), "snmpProxies" },
	{ MIB(SNMPMODULES), "snmpModules" },
	{ MIB(SNMPMIB), "snmpMIB" },
	{ MIB(SNMPMIBOBJECTS), "snmpMIBObjects" },
	{ MIB(SNMPTRAP), "snmpTrap" },
	{ MIB(SNMPTRAPOID), "snmpTrapOID" },
	{ MIB(SNMPTRAPENTERPRISE), "snmpTrapEnterprise" },
	{ MIB(SNMPTRAPS), "snmpTraps" },
	{ MIB(COLDSTART), "coldStart" },
	{ MIB(WARMSTART), "warmStart" },
	{ MIB(LINKDOWN), "linkDown" },
	{ MIB(LINKUP), "linkUp" },
	{ MIB(AUTHFAILURE), "authenticationFailure" },
	{ MIB(EGPNEIGHBORLOSS), "egpNeighborLoss" },
	{ MIBEND }
};

int
mib_getsys(struct oid *oid, struct ber_oid *o, struct ber_element **elm)
{
	struct ber_oid		 sysoid = OID(MIB_SYSOID_DEFAULT);
	char			*s = oid->o_data;
	struct ber_oid		*so = oid->o_data;
	struct utsname		 u;
	long long		 ticks;

	if (uname(&u) == -1)
		return (-1);

	switch (oid->o_oid[OIDIDX_SYSTEM]) {
	case 1:
		if (s == NULL) {
			if (asprintf(&s, "%s %s %s %s %s",
			    u.sysname, u.nodename, u.release,
			    u.version, u.machine) == -1)
				return (-1);
			oid->o_data = s;
			oid->o_val = strlen(s);
		}
		*elm = ber_add_string(*elm, s);
		break;
	case 2:
		if (so == NULL)
			so = &sysoid;
		mps_oidlen(so);
		*elm = ber_add_oid(*elm, so);
		break;
	case 3:
		ticks = mps_getticks();
		*elm = ber_add_integer(*elm, ticks);
		ber_set_header(*elm, BER_CLASS_APPLICATION, SNMP_T_TIMETICKS);
		break;
	case 4:
		if (s == NULL) {
			if (asprintf(&s, "root@%s", u.nodename) == -1)
				return (-1);
			oid->o_data = s;
			oid->o_val = strlen(s);
		}
		*elm = ber_add_string(*elm, s);
		break;
	case 5:
		if (s == NULL) {
			if ((s = strdup(u.nodename)) == NULL)
				return (-1);
			oid->o_data = s;
			oid->o_val = strlen(s);
		}
		*elm = ber_add_string(*elm, s);
		break;
	case 6:
		if (s == NULL)
			s = "";
		*elm = ber_add_string(*elm, s);
		break;
	case 7:
		*elm = ber_add_integer(*elm, oid->o_val);
		break;
	default:
		return (-1);
	}
	return (0);
}

int
mib_sysor(struct oid *oid, struct ber_oid *o, struct ber_element **elm)
{
	struct ber_element	*ber = *elm;
	u_int32_t		 idx = 1, nmib = 0;
	struct oid		*next, *miboid;
	char			 buf[SNMPD_MAXSTRLEN], *ptr;

	/* Count MIB root OIDs in the tree */
	for (next = NULL;
	    (next = mps_foreach(next, OID_MIB)) != NULL; nmib++);

	/* Get and verify the current row index */
	idx = o->bo_id[OIDIDX_ORENTRY];
	if (idx > nmib)
		return (1);

	/* Find the MIB root element for this Id */
	for (next = miboid = NULL, nmib = 1;
	    (next = mps_foreach(next, OID_MIB)) != NULL; nmib++) {
		if (nmib == idx)
			miboid = next;
	}
	if (miboid == NULL)
		return (-1);

	/* Tables need to prepend the OID on their own */
	ber = ber_add_oid(ber, o);

	switch (o->bo_id[OIDIDX_OR]) {
	case 1:
		ber = ber_add_integer(ber, idx);
		break;
	case 2:
		ber = ber_add_oid(ber, &miboid->o_id);
		break;
	case 3:
		/*
		 * This should be a description of the MIB.
		 * But we use the symbolic OID string for now, it may
		 * help to display names of internal OIDs.
		 */
		mps_oidstring(&miboid->o_id, buf, sizeof(buf));
		if ((ptr = strdup(buf)) == NULL) {
			ber = ber_add_string(ber, miboid->o_name);
		} else {
			ber = ber_add_string(ber, ptr);
			ber->be_free = 1;
		}
		break;
	case 4:
		/*
		 * We do not support dynamic loading of MIB at runtime,
		 * the sysORUpTime value of 0 will indicate "loaded at
		 * startup".
		 */
		ber = ber_add_integer(ber, 0);
		ber_set_header(ber,
		    BER_CLASS_APPLICATION, SNMP_T_TIMETICKS);
		break;
	default:
		return (-1);
	}

	return (0);
}

int
mib_getsnmp(struct oid *oid, struct ber_oid *o, struct ber_element **elm)
{
	struct snmp_stats	*stats = &env->sc_stats;
	long long		 i;
	struct statsmap {
		u_int8_t	 m_id;
		u_int32_t	*m_ptr;
	}			 mapping[] = {
		{ 1, &stats->snmp_inpkts },
		{ 2, &stats->snmp_outpkts },
		{ 3, &stats->snmp_inbadversions },
		{ 4, &stats->snmp_inbadcommunitynames },
		{ 5, &stats->snmp_inbadcommunityuses },
		{ 6, &stats->snmp_inasnparseerrs },
		{ 8, &stats->snmp_intoobigs },
		{ 9, &stats->snmp_innosuchnames },
		{ 10, &stats->snmp_inbadvalues },
		{ 11, &stats->snmp_inreadonlys },
		{ 12, &stats->snmp_ingenerrs },
		{ 13, &stats->snmp_intotalreqvars },
		{ 14, &stats->snmp_intotalsetvars },
		{ 15, &stats->snmp_ingetrequests },
		{ 16, &stats->snmp_ingetnexts },
		{ 17, &stats->snmp_insetrequests },
		{ 18, &stats->snmp_ingetresponses },
		{ 19, &stats->snmp_intraps },
		{ 20, &stats->snmp_outtoobigs },
		{ 21, &stats->snmp_outnosuchnames },
		{ 22, &stats->snmp_outbadvalues },
		{ 24, &stats->snmp_outgenerrs },
		{ 25, &stats->snmp_outgetrequests },
		{ 26, &stats->snmp_outgetnexts },
		{ 27, &stats->snmp_outsetrequests },
		{ 28, &stats->snmp_outgetresponses },
		{ 29, &stats->snmp_outtraps },
		{ 31, &stats->snmp_silentdrops },
		{ 32, &stats->snmp_proxydrops }
	};

	switch (oid->o_oid[OIDIDX_SNMP]) {
	case 30:
		i = stats->snmp_enableauthentraps == 1 ? 1 : 2;
		*elm = ber_add_integer(*elm, i);
		break;
	default:
		for (i = 0;
		    (u_int)i < (sizeof(mapping) / sizeof(mapping[0])); i++) {
			if (oid->o_oid[OIDIDX_SNMP] == mapping[i].m_id) {
				*elm = ber_add_integer(*elm, *mapping[i].m_ptr);
				ber_set_header(*elm,
				    BER_CLASS_APPLICATION, SNMP_T_COUNTER32);
				return (0);
			}
		}
		return (-1);
	}

	return (0);
}

int
mib_setsnmp(struct oid *oid, struct ber_oid *o, struct ber_element **elm)
{
	struct snmp_stats	*stats = &env->sc_stats;
	long long		 i;

	if (ber_get_integer(*elm, &i) == -1)
		return (-1);

	stats->snmp_enableauthentraps = i == 1 ? 1 : 0;

	return (0);
}

/*
 * Defined in IF-MIB.txt (RFCs 1229, 1573, 2233, 2863)
 */

int	 mib_ifnumber(struct oid *, struct ber_oid *, struct ber_element **);
struct kif
	*mib_ifget(u_int);
int	 mib_iftable(struct oid *, struct ber_oid *, struct ber_element **);
int	 mib_ifxtable(struct oid *, struct ber_oid *, struct ber_element **);
int	 mib_ifstacklast(struct oid *, struct ber_oid *, struct ber_element **);
int	 mib_ifrcvtable(struct oid *, struct ber_oid *, struct ber_element **);

static u_int8_t ether_zeroaddr[] = { 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 };
static struct ber_oid zerodotzero = { { 0, 0 }, 2 };

static struct oid if_mib[] = {
	{ MIB(IFMIB), "ifMIB", OID_MIB },
	{ MIB(IFMIBOBJECTS), "ifMIBObjects" },
	{ MIB(IFXTABLE), "ifXTable" },
	{ MIB(IFXENTRY), "ifXEntry" },
	{ MIB(IFNAME), "ifName", OID_TRD,		mib_ifxtable },
	{ MIB(IFINMASTPKTS), "ifInMulticastPkts", OID_TRD, mib_ifxtable },
	{ MIB(IFINBASTPKTS), "ifInBroadcastPkts", OID_TRD, mib_ifxtable },
	{ MIB(IFOUTMASTPKTS), "ifOutMulticastPkts", OID_TRD, mib_ifxtable },
	{ MIB(IFOUTBASTPKTS), "ifOurBroadcastPkts", OID_TRD, mib_ifxtable },
	{ MIB(IFHCINOCTETS), "ifHCInOctets", OID_TRD,	mib_ifxtable },
	{ MIB(IFHCINUCASTPKTS), "ifHCInUcastPkts", OID_TRD, mib_ifxtable },
	{ MIB(IFHCINMCASTPKTS), "ifHCInMulticastPkts", OID_TRD, mib_ifxtable },
	{ MIB(IFHCINBCASTPKTS), "ifHCInBroadcastPkts", OID_TRD, mib_ifxtable },
	{ MIB(IFHCOUTOCTETS), "ifHCOutOctets", OID_TRD,	mib_ifxtable },
	{ MIB(IFHCOUTUCASTPKTS), "ifHCOutUcastPkts", OID_TRD, mib_ifxtable },
	{ MIB(IFHCOUTMCASTPKTS), "ifHCOutMulticastPkts", OID_TRD, mib_ifxtable },
	{ MIB(IFHCOUTBCASTPKTS), "ifHCOutBroadcastPkts", OID_TRD, mib_ifxtable },
	{ MIB(IFLINKUPDORNTRAPENABLE), "ifLinkUpDownTrapEnable", OID_TRD, mib_ifxtable },
	{ MIB(IFHIGHSPEED), "ifHighSpeed", OID_TRD,	mib_ifxtable },
	{ MIB(IFPROMISCMODE), "ifPromiscuousMode", OID_TRD, mib_ifxtable },
	{ MIB(IFCONNECTORPRESENT), "ifConnectorPresent", OID_TRD, mib_ifxtable },
	{ MIB(IFALIAS), "ifAlias", OID_TRD,		mib_ifxtable },
	{ MIB(IFCNTDISCONTINUITYTIME), "ifCounterDiscontinuityTime", OID_TRD, mib_ifxtable },
	{ MIB(IFSTACKTABLE), "ifStackTable" },
	{ MIB(IFSTACKENTRY), "ifStackEntry" },
	{ MIB(IFRCVTABLE), "ifRcvAddressTable" },
	{ MIB(IFRCVENTRY), "ifRcvAddressEntry" },
	{ MIB(IFRCVSTATUS), "ifRcvAddressStatus", OID_TRD, mib_ifrcvtable },
	{ MIB(IFRCVTYPE), "ifRcvAddressType", OID_TRD,	mib_ifrcvtable },
	{ MIB(IFSTACKLASTCHANGE), "ifStackLastChange", OID_RD, mib_ifstacklast },
	{ MIB(INTERFACES), "interfaces" },
	{ MIB(IFNUMBER), "ifNumber", OID_RD,		mib_ifnumber },
	{ MIB(IFTABLE), "ifTable" },
	{ MIB(IFENTRY), "ifEntry" },
	{ MIB(IFINDEX), "ifIndex", OID_TRD,		mib_iftable },
	{ MIB(IFDESCR), "ifDescr", OID_TRD,		mib_iftable },
	{ MIB(IFTYPE), "ifDescr", OID_TRD,		mib_iftable },
	{ MIB(IFMTU), "ifMtu", OID_TRD,			mib_iftable },
	{ MIB(IFSPEED), "ifSpeed", OID_TRD,		mib_iftable },
	{ MIB(IFPHYSADDR), "ifPhysAddress", OID_TRD,	mib_iftable },
	{ MIB(IFADMINSTATUS), "ifAdminStatus", OID_TRD,	mib_iftable },
	{ MIB(IFOPERSTATUS), "ifOperStatus", OID_TRD,	mib_iftable },
	{ MIB(IFLASTCHANGE), "ifLastChange", OID_TRD,	mib_iftable },
	{ MIB(IFINOCTETS), "ifInOctets", OID_TRD,	mib_iftable },
	{ MIB(IFINUCASTPKTS), "ifInUcastPkts", OID_TRD,	mib_iftable },
	{ MIB(IFINNUCASTPKTS), "ifInNUcastPkts", OID_TRD, mib_iftable },
	{ MIB(IFINDISCARDS), "ifInDiscards", OID_TRD,	mib_iftable },
	{ MIB(IFINERRORS), "ifInErrors", OID_TRD,	mib_iftable },
	{ MIB(IFINUNKNOWNERRORS), "ifInUnknownErrors", OID_TRD,	mib_iftable },
	{ MIB(IFOUTOCTETS), "ifOutOctets", OID_TRD,	mib_iftable },
	{ MIB(IFOUTUCASTPKTS), "ifOutUcastPkts", OID_TRD, mib_iftable },
	{ MIB(IFOUTNUCASTPKTS), "ifOutNUcastPkts", OID_TRD, mib_iftable },
	{ MIB(IFOUTDISCARDS), "ifOutDiscards", OID_TRD,	mib_iftable },
	{ MIB(IFOUTERRORS), "ifOutErrors", OID_TRD,	mib_iftable },
	{ MIB(IFOUTQLEN), "ifOutQLen", OID_TRD,		mib_iftable },
	{ MIB(IFSPECIFIC), "ifSpecific", OID_TRD,	mib_iftable },
	{ MIBEND }
};

int
mib_ifnumber(struct oid *oid, struct ber_oid *o, struct ber_element **elm)
{
	*elm = ber_add_integer(*elm, kr_ifnumber());
	return (0);
}

struct kif *
mib_ifget(u_int idx)
{
	struct kif	*kif;

	if ((kif = kr_getif(idx)) == NULL) {
		/*
		 * It may happen that a interface with a specific index
		 * does not exist or has been removed. Jump to the next
		 * available interface index.
		 */
		for (kif = kr_getif(0); kif != NULL;
		    kif = kr_getnextif(kif->if_index))
			if (kif->if_index > idx)
				break;
		if (kif == NULL)
			return (NULL);
	}

	/* Update interface information */
	kr_updateif(kif->if_index);
	if ((kif = kr_getif(kif->if_index)) == NULL) {
		log_debug("mib_ifxtable: interface disappeared?");
		return (NULL);
	}

	return (kif);
}

int
mib_iftable(struct oid *oid, struct ber_oid *o, struct ber_element **elm)
{
	struct ber_element	*ber = *elm;
	u_int32_t		 idx = 0;
	struct kif		*kif;
	long long		 i;
	size_t			 len;
	int			 ifq;
	int			 mib[5];
	char			*s;

	/* Get and verify the current row index */
	idx = o->bo_id[OIDIDX_IFENTRY];
	if ((kif = mib_ifget(idx)) == NULL)
		return (1);

	/* Tables need to prepend the OID on their own */
	o->bo_id[OIDIDX_IFENTRY] = kif->if_index;
	ber = ber_add_oid(ber, o);

	switch (o->bo_id[OIDIDX_IF]) {
	case 1:
		ber = ber_add_integer(ber, kif->if_index);
		break;
	case 2:
		/*
		 * The ifDescr should contain a vendor, product, etc.
		 * but we just use the interface name (like ifName).
		 * The interface name includes the driver name on OpenBSD.
		 */
		if ((s = strdup(kif->if_name)) == NULL)
			return (-1);
		ber = ber_add_string(ber, s);
		ber->be_free = 1;
		break;
	case 3:
		if (kif->if_type >= 0xf0) {
			/*
			 * It does not make sense to announce the private
			 * interface types for CARP, ENC, PFSYNC, etc.
			 */
			ber = ber_add_integer(ber, IFT_OTHER);
		} else
			ber = ber_add_integer(ber, kif->if_type);
		break;
	case 4:
		ber = ber_add_integer(ber, kif->if_mtu);
		break;
	case 5:
		ber = ber_add_integer(ber, kif->if_baudrate);
		ber_set_header(ber, BER_CLASS_APPLICATION, SNMP_T_GAUGE32);
		break;
	case 6:
		if (bcmp(kif->if_lladdr, ether_zeroaddr,
		    sizeof(kif->if_lladdr)) == 0) {
			ber = ber_add_string(ber, "");
		} else {
			ber = ber_add_nstring(ber, kif->if_lladdr,
			    sizeof(kif->if_lladdr));
		}
		break;
	case 7:
		/* ifAdminStatus up(1), down(2), testing(3) */
		i = (kif->if_flags & IFF_UP) ? 1 : 2;
		ber = ber_add_integer(ber, i);
		break;
	case 8:
		/* ifOperStatus */
		if ((kif->if_flags & IFF_UP) == 0) {
			i = 2;	/* down(2) */
		} else if (LINK_STATE_IS_UP(kif->if_link_state)) {
			i = 1;	/* up(1) */
		} else if (kif->if_link_state == LINK_STATE_DOWN) {
			i = 7;	/* lowerLayerDown(7) or dormant(5)? */
		} else
			i = 4;	/* unknown(4) */
		ber = ber_add_integer(ber, i);
		break;
	case 9:
		ber = ber_add_integer(ber, kif->if_ticks);
		ber_set_header(ber, BER_CLASS_APPLICATION, SNMP_T_TIMETICKS);
		break;
	case 10:
		ber = ber_add_integer(ber, (u_int32_t)kif->if_ibytes);
		ber_set_header(ber, BER_CLASS_APPLICATION, SNMP_T_COUNTER32);
		break;
	case 11:
		ber = ber_add_integer(ber, (u_int32_t)kif->if_ipackets);
		ber_set_header(ber, BER_CLASS_APPLICATION, SNMP_T_COUNTER32);
		break;
	case 12:
		ber = ber_add_integer(ber, (u_int32_t)kif->if_imcasts);
		ber_set_header(ber, BER_CLASS_APPLICATION, SNMP_T_COUNTER32);
		break;
	case 13:
		ber = ber_add_integer(ber, (u_int32_t)kif->if_iqdrops);
		ber_set_header(ber, BER_CLASS_APPLICATION, SNMP_T_COUNTER32);
		break;
	case 14:
		ber = ber_add_integer(ber, (u_int32_t)kif->if_ierrors);
		ber_set_header(ber, BER_CLASS_APPLICATION, SNMP_T_COUNTER32);
		break;
	case 15:
		ber = ber_add_integer(ber, 0);	/* unknown errors? */
		ber_set_header(ber, BER_CLASS_APPLICATION, SNMP_T_COUNTER32);
		break;
	case 16:
		ber = ber_add_integer(ber, (u_int32_t)kif->if_obytes);
		ber_set_header(ber, BER_CLASS_APPLICATION, SNMP_T_COUNTER32);
		break;
	case 17:
		ber = ber_add_integer(ber, (u_int32_t)kif->if_opackets);
		ber_set_header(ber, BER_CLASS_APPLICATION, SNMP_T_COUNTER32);
		break;
	case 18:
		ber = ber_add_integer(ber, (u_int32_t)kif->if_omcasts);
		ber_set_header(ber, BER_CLASS_APPLICATION, SNMP_T_COUNTER32);
		break;
	case 19:
		mib[0] = CTL_NET;
		mib[1] = AF_INET;
		mib[2] = IPPROTO_IP;
		mib[3] = IPCTL_IFQUEUE;
		mib[4] = IFQCTL_DROPS;
		len = sizeof(ifq);
		if (sysctl(mib, 5, &ifq, &len, 0, 0) == -1) {
			log_info("mib_iftable: %s: invalid ifq: %s",
			    kif->if_name, strerror(errno));
			return (-1);
		}
		ber = ber_add_integer(ber, kif->if_noproto + ifq);
		ber_set_header(ber, BER_CLASS_APPLICATION, SNMP_T_COUNTER32);
		break;
	case 20:
		ber = ber_add_integer(ber, (u_int32_t)kif->if_oerrors);
		ber_set_header(ber, BER_CLASS_APPLICATION, SNMP_T_COUNTER32);
		break;
	case 21:
		mib[0] = CTL_NET;
		mib[1] = AF_INET;
		mib[2] = IPPROTO_IP;
		mib[3] = IPCTL_IFQUEUE;
		mib[4] = IFQCTL_LEN;
		len = sizeof(ifq);
		if (sysctl(mib, 5, &ifq, &len, 0, 0) == -1) {
			log_info("mib_iftable: %s: invalid ifq: %s",
			    kif->if_name, strerror(errno));
			return (-1);
		}
		ber = ber_add_integer(ber, ifq);
		ber_set_header(ber, BER_CLASS_APPLICATION, SNMP_T_GAUGE32);
		break;
	case 22:
		ber = ber_add_oid(ber, &zerodotzero);
		break;
	default:
		return (-1);
	}

	return (0);
}

int
mib_ifxtable(struct oid *oid, struct ber_oid *o, struct ber_element **elm)
{
	struct ber_element	*ber = *elm;
	u_int32_t		 idx = 0;
	struct kif		*kif;
	int			 i = 0;
	char			*s;

	/* Get and verify the current row index */
	idx = o->bo_id[OIDIDX_IFXENTRY];
	if ((kif = mib_ifget(idx)) == NULL)
		return (1);

	/* Tables need to prepend the OID on their own */
	o->bo_id[OIDIDX_IFXENTRY] = kif->if_index;
	ber = ber_add_oid(ber, o);

	switch (o->bo_id[OIDIDX_IFX]) {
	case 1:
		if ((s = strdup(kif->if_name)) == NULL)
			return (-1);
		ber = ber_add_string(ber, s);
		ber->be_free = 1;
		break;
	case 2:
		ber = ber_add_integer(ber, (u_int32_t)kif->if_imcasts);
		ber_set_header(ber, BER_CLASS_APPLICATION, SNMP_T_COUNTER32);
		break;
	case 3:
		ber = ber_add_integer(ber, 0);
		ber_set_header(ber, BER_CLASS_APPLICATION, SNMP_T_COUNTER32);
		break;
	case 4:
		ber = ber_add_integer(ber, (u_int32_t)kif->if_omcasts);
		ber_set_header(ber, BER_CLASS_APPLICATION, SNMP_T_COUNTER32);
		break;
	case 5:
		ber = ber_add_integer(ber, 0);
		ber_set_header(ber, BER_CLASS_APPLICATION, SNMP_T_COUNTER32);
		break;
	case 6:
		ber = ber_add_integer(ber, (u_int64_t)kif->if_ibytes);
		ber_set_header(ber, BER_CLASS_APPLICATION, SNMP_T_COUNTER64);
		break;
	case 7:
		ber = ber_add_integer(ber, (u_int64_t)kif->if_ipackets);
		ber_set_header(ber, BER_CLASS_APPLICATION, SNMP_T_COUNTER64);
		break;
	case 8:
		ber = ber_add_integer(ber, (u_int64_t)kif->if_imcasts);
		ber_set_header(ber, BER_CLASS_APPLICATION, SNMP_T_COUNTER64);
		break;
	case 9:
		ber = ber_add_integer(ber, 0);
		ber_set_header(ber, BER_CLASS_APPLICATION, SNMP_T_COUNTER64);
		break;
	case 10:
		ber = ber_add_integer(ber, (u_int64_t)kif->if_obytes);
		ber_set_header(ber, BER_CLASS_APPLICATION, SNMP_T_COUNTER64);
		break;
	case 11:
		ber = ber_add_integer(ber, (u_int64_t)kif->if_opackets);
		ber_set_header(ber, BER_CLASS_APPLICATION, SNMP_T_COUNTER64);
		break;
	case 12:
		ber = ber_add_integer(ber, (u_int64_t)kif->if_omcasts);
		ber_set_header(ber, BER_CLASS_APPLICATION, SNMP_T_COUNTER64);
		break;
	case 13:
		ber = ber_add_integer(ber, 0);
		ber_set_header(ber, BER_CLASS_APPLICATION, SNMP_T_COUNTER64);
		break;
	case 14:
		ber = ber_add_integer(ber, 0);	/* enabled(1), disabled(2) */
		break;
	case 15:
		i = kif->if_baudrate >= 1000000 ?
		    kif->if_baudrate / 1000000 : 0;
		ber = ber_add_integer(ber, i);
		ber_set_header(ber, BER_CLASS_APPLICATION, SNMP_T_GAUGE32);
		break;
	case 16:
		/* ifPromiscuousMode: true(1), false(2) */
		i = kif->if_flags & IFF_PROMISC ? 1 : 2;
		ber = ber_add_integer(ber, i);
		break;
	case 17:
		/* ifConnectorPresent: false(2), true(1) */
		i = kif->if_type == IFT_ETHER ? 1 : 2;
		ber = ber_add_integer(ber, i);
		break;
	case 18:
		if ((s = strdup(kif->if_descr)) == NULL)
			return (-1);
		ber = ber_add_string(ber, s);
		ber->be_free = 1;
		break;
	case 19:
		ber = ber_add_integer(ber, 0);
		ber_set_header(ber, BER_CLASS_APPLICATION, SNMP_T_TIMETICKS);
		break;
	default:
		return (-1);
	}

	return (0);
}

int
mib_ifstacklast(struct oid *oid, struct ber_oid *o, struct ber_element **elm)
{
	struct ber_element	*ber = *elm;
	ber = ber_add_integer(ber, kr_iflastchange());
	ber_set_header(ber, BER_CLASS_APPLICATION, SNMP_T_TIMETICKS);
	return (0);
}

int
mib_ifrcvtable(struct oid *oid, struct ber_oid *o, struct ber_element **elm)
{
	struct ber_element	*ber = *elm;
	u_int32_t		 idx = 0;
	struct kif		*kif;
	u_int			 i = 0;

	/* Get and verify the current row index */
	idx = o->bo_id[OIDIDX_IFRCVENTRY];
	if ((kif = mib_ifget(idx)) == NULL)
		return (1);

	/*
	 * The lladdr of the interface will be encoded in the returned OID
	 * ifRcvAddressX.ifindex.6.x.x.x.x.x.x = val
	 * Thanks to the virtual cloner interfaces, it is an easy 1:1
	 * mapping in OpenBSD; only one lladdr (MAC) address per interface.
	 */

	/* first set the base OID and caluculate the length */
	idx = 0;
	o->bo_id[OIDIDX_IFRCVENTRY + idx++] = kif->if_index;
	o->bo_id[OIDIDX_IFRCVENTRY + idx] = 0;
	mps_oidlen(o);

	/* extend the OID with the lladdr length and octets */
	o->bo_id[OIDIDX_IFRCVENTRY + idx++] = sizeof(kif->if_lladdr);
	o->bo_n++;
	for (i = 0; i < sizeof(kif->if_lladdr); i++, o->bo_n++)
		o->bo_id[OIDIDX_IFRCVENTRY + idx++] = kif->if_lladdr[i];

	/* write OID */
	ber = ber_add_oid(ber, o);

	switch (o->bo_id[OIDIDX_IFRCV]) {
	case 2:
		/* ifRcvAddressStatus: RowStatus active(1), notInService(2) */
		i = kif->if_flags & IFF_UP ? 1 : 2;
		ber = ber_add_integer(ber, i);
		break;
	case 3:
		/* ifRcvAddressType: other(1), volatile(2), nonVolatile(3) */
		ber = ber_add_integer(ber, 1);
		break;
	default:
		return (-1);
	}

	return (0);
}

/*
 * PRIVATE ENTERPRISE NUMBERS from
 * http://www.iana.org/assignments/enterprise-numbers
 *
 * This is not the complete list of private enterprise numbers, it only
 * includes some well-known companies and especially network companies
 * that are very common in the datacenters around the world. It would
 * be an overkill to include ~30.000 entries for all the organizations
 * from the official list.
 */
static struct oid enterprise_mib[] = {
	{ MIB(IBM), "ibm" },
	{ MIB(CMU), "cmu" },
	{ MIB(UNIX), "unix" },
	{ MIB(CISCO), "ciscoSystems" },
	{ MIB(HP), "hp" },
	{ MIB(MIT), "mit" },
	{ MIB(NORTEL), "nortelNetworks" },
	{ MIB(SUN), "sun" },
	{ MIB(3COM), "3com" },
	{ MIB(SYNOPTICS), "synOptics" },
	{ MIB(ENTERASYS), "enterasys" },
	{ MIB(SGI), "sgi" },
	{ MIB(APPLE), "apple" },
	{ MIB(ATT), "att" },
	{ MIB(NOKIA), "nokia" },
	{ MIB(CERN), "cern" },
	{ MIB(FSC), "fsc" },
	{ MIB(COMPAQ), "compaq" },
	{ MIB(DELL), "dell" },
	{ MIB(ALTEON), "alteon" },
	{ MIB(EXTREME), "extremeNetworks" },
	{ MIB(FOUNDRY), "foundryNetworks" },
	{ MIB(HUAWAI), "huawaiTechnology" },
	{ MIB(UCDAVIS), "ucDavis" },
	{ MIB(CHECKPOINT), "checkPoint" },
	{ MIB(JUNIPER), "juniper" },
	{ MIB(FORCE10), "force10Networks" },
	{ MIB(ALCATELLUCENT), "alcatelLucent" },
	{ MIB(SNOM), "snom" },
	{ MIB(GOOGLE), "google" },
	{ MIB(F5), "f5Networks" },
	{ MIB(SFLOW), "sFlow" },
	{ MIB(MSYS), "microSystems" },
	{ MIB(VANTRONIX), "vantronix" },
	{ MIB(OPENBSD), "openBSD" },
	{ MIBEND }
};

/*
 * Defined in OPENBSD-SENSORS-MIB.txt
 * (http://packetmischief.ca/openbsd/snmp/)
 */

int	 mib_sensornum(struct oid *, struct ber_oid *, struct ber_element **);
int	 mib_sensors(struct oid *, struct ber_oid *, struct ber_element **);
char	*mib_sensorunit(struct sensor *);
char	*mib_sensorvalue(struct sensor *);

static struct oid openbsd_mib[] = {
	{ MIB(SENSORMIBOBJECTS), "sensorMIBObjects", OID_MIB },
	{ MIB(SENSORS), "sensors" },
	{ MIB(SENSORNUMBER), "sensorNumber", OID_RD,	mib_sensornum },
	{ MIB(SENSORTABLE), "sensorTable" },
	{ MIB(SENSORENTRY), "sensorEntry" },
	{ MIB(SENSORINDEX), "sensorIndex", OID_TRD,	mib_sensors },
	{ MIB(SENSORDESCR), "sensorDescr", OID_TRD,	mib_sensors },
	{ MIB(SENSORTYPE), "sensorType", OID_TRD,	mib_sensors },
	{ MIB(SENSORDEVICE), "sensorDevice", OID_TRD,	mib_sensors },
	{ MIB(SENSORVALUE), "sensorValue", OID_TRD,	mib_sensors },
	{ MIB(SENSORUNITS), "sensorUnits", OID_TRD,	mib_sensors },
	{ MIB(SENSORSTATUS), "sensorStatus", OID_TRD,	mib_sensors },
	{ MIBEND }
};

int
mib_sensornum(struct oid *oid, struct ber_oid *o, struct ber_element **elm)
{
	struct sensordev	 sensordev;
	size_t			 len = sizeof(sensordev);
	int			 mib[3], i, c;

	mib[0] = CTL_HW;
	mib[1] = HW_SENSORS;

	for (i = c = 0; i < MAXSENSORDEVICES; i++) {
		mib[2] = i;
		if (sysctl(mib, 3, &sensordev, &len, NULL, 0) == -1) {
			if (errno != ENOENT)
				return(-1);
			continue;	
		}
		c += sensordev.sensors_count;
	}

	*elm = ber_add_integer(*elm, c);
	return (0);
}

int
mib_sensors(struct oid *oid, struct ber_oid *o, struct ber_element **elm)
{
	struct ber_element	*ber = *elm;
	struct sensordev	 sensordev;
	size_t			 len = sizeof(sensordev);
	struct sensor		 sensor;
	size_t			 slen = sizeof(sensor);
	int			 mib[5], i, c, j, k;
	u_int32_t		 idx = 0, n;
	char			*s;

	/* Get and verify the current row index */
	idx = o->bo_id[OIDIDX_SENSORENTRY];
 
	mib[0] = CTL_HW;
	mib[1] = HW_SENSORS;

	for (i = c = 0, n = 1; i < MAXSENSORDEVICES; i++) {
		mib[2] = i;
		if (sysctl(mib, 3, &sensordev, &len, NULL, 0) == -1) {
			if (errno != ENOENT)
				return(-1);
			continue;	
		}
		for (j = 0; j < SENSOR_MAX_TYPES; j++) {
			mib[3] = j;
			for (k = 0; k < sensordev.maxnumt[j]; k++, n++) {
				mib[4] = k;
				if (sysctl(mib, 5, &sensor,
				    &slen, NULL, 0) == -1) {
					if (errno != ENOENT)
						return (-1);
					continue;	
				}
				if (n == idx)
					goto found;
			}
		}
	}
	return (1);

 found:
	ber = ber_add_oid(ber, o);

	switch (o->bo_id[OIDIDX_SENSOR]) {
	case 1:
		ber = ber_add_integer(ber, (int32_t)n);
		break;
	case 2:
		if ((s = strdup(sensor.desc)) == NULL)
			return (-1);
		ber = ber_add_string(ber, s);
		ber->be_free = 1;
		break;
	case 3:
		ber = ber_add_integer(ber, sensor.type);
		break;
	case 4:
		if ((s = strdup(sensordev.xname)) == NULL)
			return (-1);
		ber = ber_add_string(ber, s);
		ber->be_free = 1;
		break;
	case 5:
		if ((s = mib_sensorvalue(&sensor)) == NULL)
			return (-1);
		ber = ber_add_string(ber, s);
		ber->be_free = 1;
		break;
	case 6:
		if ((s = mib_sensorunit(&sensor)) == NULL)
			return (-1);
		ber = ber_add_string(ber, s);
		ber->be_free = 1;
		break;
	case 7:
		ber = ber_add_integer(ber, sensor.status);
		break;
	}

	return (0);
}

#define SENSOR_DRIVE_STATES	(SENSOR_DRIVE_PFAIL + 1)
static const char * const sensor_drive_s[SENSOR_DRIVE_STATES] = {
	NULL, "empty", "ready", "powerup", "online", "idle", "active",
	"rebuild", "powerdown", "fail", "pfail"
};

static const char * const sensor_unit_s[SENSOR_MAX_TYPES + 1] = {
	"degC",	"RPM", "V DC", "V AC", "Ohm", "W", "A", "Wh", "Ah",
	"", "", "%", "lx", "", "sec", ""
};

char *
mib_sensorunit(struct sensor *s)
{
	u_int	 idx;
	idx = s->type > SENSOR_MAX_TYPES ?
	    SENSOR_MAX_TYPES : s->type;
	return (strdup(sensor_unit_s[idx]));
}

char *
mib_sensorvalue(struct sensor *s)
{
	char	*v;
	int	 ret = -1;

	switch (s->type) {
	case SENSOR_TEMP:
		ret = asprintf(&v, "%.2f",
		    (s->value - 273150000) / 1000000.0);
		break;
	case SENSOR_VOLTS_DC:
	case SENSOR_VOLTS_AC:
	case SENSOR_AMPS:
	case SENSOR_WATTHOUR:
	case SENSOR_AMPHOUR:
	case SENSOR_LUX:
		ret = asprintf(&v, "%.2f", s->value / 1000000.0);
		break;
	case SENSOR_INDICATOR:
		ret = asprintf(&v, "%s", s->value ? "on" : "off");
		break;
	case SENSOR_PERCENT:
		ret = asprintf(&v, "%.2f%%", s->value / 1000.0);
		break;
	case SENSOR_TIMEDELTA:
		ret = asprintf(&v, "%.6f", s->value / 1000000000.0);
		break;
	case SENSOR_DRIVE:
		if (s->value > 0 && s->value < SENSOR_DRIVE_STATES) {
			ret = asprintf(&v, "%s", sensor_drive_s[s->value]);
			break;
		}
		/* FALLTHROUGH */
	case SENSOR_FANRPM:
	case SENSOR_INTEGER:
	default:
		ret = asprintf(&v, "%lld", s->value);
		break;
	}

	if (ret == -1)
		return (NULL);
	return (v);
}

int mib_ipforwarding(struct oid *, struct ber_oid *, struct ber_element **);
int mib_ipdefaultttl(struct oid *, struct ber_oid *, struct ber_element **);
int mib_ipinreceives(struct oid *, struct ber_oid *, struct ber_element **);
int mib_ipinhdrerrors(struct oid *, struct ber_oid *, struct ber_element **);
int mib_ipinaddrerrors(struct oid *, struct ber_oid *, struct ber_element **);
int mib_ipforwdatagrams(struct oid *, struct ber_oid *, struct ber_element **);
int mib_ipinunknownprotos(struct oid *, struct ber_oid *, struct ber_element **);
int mib_ipindiscards(struct oid *, struct ber_oid *, struct ber_element **);
int mib_ipindelivers(struct oid *, struct ber_oid *, struct ber_element **);
int mib_ipoutrequests(struct oid *, struct ber_oid *, struct ber_element **);
int mib_ipoutdiscards(struct oid *, struct ber_oid *, struct ber_element **);
int mib_ipoutnoroutes(struct oid *, struct ber_oid *, struct ber_element **);

int mib_ipreasmtimeout(struct oid *, struct ber_oid *, struct ber_element **);
int mib_ipreasmreqds(struct oid *, struct ber_oid *, struct ber_element **);
int mib_ipreasmoks(struct oid *, struct ber_oid *, struct ber_element **);
int mib_ipreasmfails(struct oid *, struct ber_oid *, struct ber_element **);

int mib_ipfragoks(struct oid *, struct ber_oid *, struct ber_element **);
int mib_ipfragfails(struct oid *, struct ber_oid *, struct ber_element **);
int mib_ipfragcreate(struct oid *, struct ber_oid *, struct ber_element **);

int mib_iproutingdiscards(struct oid *, struct ber_oid *, struct ber_element **);

static struct oid ip_mib[] = {
	{ MIB(IPMIB), "ipMIB", OID_MIB },
	{ MIB(IPFORWARDING), "ipForwarding", OID_RD, mib_ipforwarding },
	{ MIB(IPDEFAULTTTL), "ipDefaultTTL", OID_RD, mib_ipdefaultttl },
	{ MIB(IPINRECEIVES), "ipInReceives", OID_RD, mib_ipinreceives },
	{ MIB(IPINHDRERRORS), "ipInHdrErrors", OID_RD, mib_ipinhdrerrors },
	{ MIB(IPINADDRERRORS), "ipInAddrErrors", OID_RD, mib_ipinaddrerrors },
	{ MIB(IPFORWDATAGRAMS), "ipForwDatagrams", OID_RD, mib_ipforwdatagrams },
	{ MIB(IPINUNKNOWNPROTOS), "ipInUnknownProtos", OID_RD, mib_ipinunknownprotos },
	{ MIB(IPINDISCARDS), "ipInDiscards", OID_RD, mib_ipindiscards },
	{ MIB(IPINDELIVERS), "ipInDelivers", OID_RD, mib_ipindelivers },
	{ MIB(IPOUTREQUESTS), "ipOutRequests", OID_RD, mib_ipoutrequests },
	{ MIB(IPOUTDISCARDS), "ipOutDiscards" },
	{ MIB(IPOUTNOROUTES), "ipOutNoRoutes", OID_RD, mib_ipoutnoroutes },
	{ MIB(IPREASMTIMEOUT), "ipReasmTimeout", OID_RD, mib_ipreasmtimeout },
	{ MIB(IPREASMREQDS), "ipReasmReqds", OID_RD, mib_ipreasmreqds },
	{ MIB(IPREASMOKS), "ipReasmOKs", OID_RD, mib_ipreasmoks },
	{ MIB(IPREASMFAILS), "ipReasmFails", OID_RD, mib_ipreasmfails },
	{ MIB(IPFRAGOKS), "ipFragOKs", OID_RD, mib_ipfragoks },
	{ MIB(IPFRAGFAILS), "ipFragFails", OID_RD, mib_ipfragfails },
	{ MIB(IPFRAGCREATES), "ipFragCreate", OID_RD, mib_ipfragcreate },
	{ MIB(IPROUTINGDISCARDS), "ipRoutingDiscards" },

	{ MIB(IPADDRTABLE), "ipAddrTable" },
	{ MIB(IPADDRENTRY), "ipAddrEntry" },
	{ MIB(IPADENTADDR), "ipAdEntAddr" },
	{ MIB(IPADENTIFINDEX), "ipAdEntIfIndex" },
	{ MIB(IPADENTNETMASK), "ipAdEntNetMask" },
	{ MIB(IPADENTBCASTADDR), "ipAdEntBcastAddr" },
	{ MIB(IPADENTREASMMAXSIZE), "ipAdEntReasmMaxSize" },

	{ MIB(IPNETTOMEDIATABLE), "ipNetToMediaTable" },
	{ MIB(IPNETTOMEDIAENTRY), "ipNetToMediaEntry" },
	{ MIB(IPNETTOMEDIAIFINDEX), "ipNetToMediaIfIndex" },
	{ MIB(IPNETTOMEDIAPHYSADDRESS), "ipNetToMediaPhysAddress" },
	{ MIB(IPNETTOMEDIANETADDRESS), "ipNetToMediaNetAddress" },
	{ MIB(IPNETTOMEDIATYPE), "ipNetToMediaType" },
	{ MIB(IPNETTOMEDIATYPE), "ipNetToMediaType" },

	{ MIBEND }
};

int
mib_ipforwarding(struct oid *oid, struct ber_oid *o, struct ber_element **elm)
{
	int	mib[] = { CTL_NET, AF_INET, IPPROTO_IP, IPCTL_FORWARDING };
	int	v;
	size_t	len = sizeof(v);

	if (sysctl(mib, sizeof(mib) / sizeof(mib[0]),
	    &v, &len, NULL, 0) == -1)
		return (-1);
	
	*elm = ber_add_integer(*elm, v);

	return (0);
}

int
mib_ipdefaultttl(struct oid *oid, struct ber_oid *o, struct ber_element **elm)
{
	int	mib[] = { CTL_NET, AF_INET, IPPROTO_IP, IPCTL_DEFTTL };
	int	v;
	size_t	len = sizeof(v);

	if (sysctl(mib, sizeof(mib) / sizeof(mib[0]),
	    &v, &len, NULL, 0) == -1)
		return (-1);

	*elm = ber_add_integer(*elm, v);

	return (0);
}

int
mib_ipinreceives(struct oid *oid, struct ber_oid *o, struct ber_element **elm)
{
	int		mib[] = { CTL_NET, AF_INET, IPPROTO_IP, IPCTL_STATS };
	struct ipstat	ipstat;
	size_t		len = sizeof(ipstat);

	if (sysctl(mib, sizeof(mib) / sizeof(mib[0]),
	    &ipstat, &len, NULL, 0) == -1)
		return (-1);

	*elm = ber_add_integer(*elm, ipstat.ips_total);
	ber_set_header(*elm, BER_CLASS_APPLICATION, SNMP_T_COUNTER32);

	return (0);
}

int
mib_ipinhdrerrors(struct oid *oid, struct ber_oid *o, struct ber_element **elm)
{
	int		mib[] = { CTL_NET, AF_INET, IPPROTO_IP, IPCTL_STATS };
	struct ipstat	ipstat;
	size_t		len = sizeof(ipstat);
	u_int32_t	errors;

	if (sysctl(mib, sizeof(mib) / sizeof(mib[0]),
	    &ipstat, &len, NULL, 0) == -1)
		return (-1);

	errors = ipstat.ips_badsum + ipstat.ips_badvers +
	    ipstat.ips_tooshort + ipstat.ips_toosmall +
	    ipstat.ips_badhlen +  ipstat.ips_badlen +
	    ipstat.ips_badoptions + ipstat.ips_toolong +
	    ipstat.ips_badaddr;

	*elm = ber_add_integer(*elm, errors);
	ber_set_header(*elm, BER_CLASS_APPLICATION, SNMP_T_COUNTER32);

	return (0);
}

int
mib_ipinaddrerrors(struct oid *oid, struct ber_oid *o, struct ber_element **elm)
{
	int		mib[] = { CTL_NET, AF_INET, IPPROTO_IP, IPCTL_STATS };
	struct ipstat	ipstat;
	size_t		len = sizeof(ipstat);
	u_int32_t	errors;

	if (sysctl(mib, sizeof(mib) / sizeof(mib[0]),
	    &ipstat, &len, NULL, 0) == -1)
		return (-1);

	errors = ipstat.ips_cantforward + ipstat.ips_badaddr;


	*elm = ber_add_integer(*elm, errors);
	ber_set_header(*elm, BER_CLASS_APPLICATION, SNMP_T_COUNTER32);

	return (0);
}

int
mib_ipforwdatagrams(struct oid *oid, struct ber_oid *o, struct ber_element **elm)
{
	int		mib[] = { CTL_NET, AF_INET, IPPROTO_IP, IPCTL_STATS };
	struct ipstat	ipstat;
	size_t		len = sizeof(ipstat);
	u_int32_t	counter;

	if (sysctl(mib, sizeof(mib) / sizeof(mib[0]),
	    &ipstat, &len, NULL, 0) == -1)
		return (-1);

	counter = ipstat.ips_forward + ipstat.ips_redirectsent;

	*elm = ber_add_integer(*elm, counter);
	ber_set_header(*elm, BER_CLASS_APPLICATION, SNMP_T_COUNTER32);

	return (0);
}

int
mib_ipinunknownprotos(struct oid *oid, struct ber_oid *o, struct ber_element **elm)
{
	int		mib[] = { CTL_NET, AF_INET, IPPROTO_IP, IPCTL_STATS };
	struct ipstat	ipstat;
	size_t		len = sizeof(ipstat);

	if (sysctl(mib, sizeof(mib) / sizeof(mib[0]),
	    &ipstat, &len, NULL, 0) == -1)
		return (-1);

	*elm = ber_add_integer(*elm, ipstat.ips_noproto);
	ber_set_header(*elm, BER_CLASS_APPLICATION, SNMP_T_COUNTER32);

	return (0);
}

int
mib_ipindiscards(struct oid *oid, struct ber_oid *o, struct ber_element **elm)
{
	int		mib[] = { CTL_NET, AF_INET, IPPROTO_IP, IPCTL_STATS };
	struct ipstat	ipstat;
	size_t		len = sizeof(ipstat);
	u_int32_t	counter;

	if (sysctl(mib, sizeof(mib) / sizeof(mib[0]),
	    &ipstat, &len, NULL, 0) == -1)
		return (-1);

	counter = ipstat.ips_odropped;
	*elm = ber_add_integer(*elm, counter);
	ber_set_header(*elm, BER_CLASS_APPLICATION, SNMP_T_COUNTER32);

	return (0);
}

int
mib_ipindelivers(struct oid *oid, struct ber_oid *o, struct ber_element **elm)
{
	int		mib[] = { CTL_NET, AF_INET, IPPROTO_IP, IPCTL_STATS };
	struct ipstat	ipstat;
	size_t		len = sizeof(ipstat);

	if (sysctl(mib, sizeof(mib) / sizeof(mib[0]),
	    &ipstat, &len, NULL, 0) == -1)
		return (-1);
	
	*elm = ber_add_integer(*elm, ipstat.ips_delivered);
	ber_set_header(*elm, BER_CLASS_APPLICATION, SNMP_T_COUNTER32);

	return (0);
}

int
mib_ipoutrequests(struct oid *oid, struct ber_oid *o, struct ber_element **elm)
{
	int		mib[] = { CTL_NET, AF_INET, IPPROTO_IP, IPCTL_STATS };
	struct ipstat	ipstat;
	size_t		len = sizeof(ipstat);

	if (sysctl(mib, sizeof(mib) / sizeof(mib[0]),
	    &ipstat, &len, NULL, 0) == -1)
		return (-1);
	
	*elm = ber_add_integer(*elm, ipstat.ips_localout);
	ber_set_header(*elm, BER_CLASS_APPLICATION, SNMP_T_COUNTER32);

	return (0);
}

int
mib_ipoutdiscards(struct oid *oid, struct ber_oid *o, struct ber_element **elm)
{
	return (0);
}

int
mib_ipoutnoroutes(struct oid *oid, struct ber_oid *o, struct ber_element **elm)
{
	int		mib[] = { CTL_NET, AF_INET, IPPROTO_IP, IPCTL_STATS };
	struct ipstat	ipstat;
	size_t		len = sizeof(ipstat);

	if (sysctl(mib, sizeof(mib) / sizeof(mib[0]),
	    &ipstat, &len, NULL, 0) == -1)
		return (-1);
	
	*elm = ber_add_integer(*elm, ipstat.ips_noroute);
	ber_set_header(*elm, BER_CLASS_APPLICATION, SNMP_T_COUNTER32);

	return (0);
}

int
mib_ipreasmtimeout(struct oid *oid, struct ber_oid *o, struct ber_element **elm)
{
	return (0);
}

int
mib_ipreasmreqds(struct oid *oid, struct ber_oid *o, struct ber_element **elm)
{
	int		mib[] = { CTL_NET, AF_INET, IPPROTO_IP, IPCTL_STATS };
	struct ipstat	ipstat;
	size_t		len = sizeof(ipstat);

	if (sysctl(mib, sizeof(mib) / sizeof(mib[0]),
	    &ipstat, &len, NULL, 0) == -1)
		return (-1);
	
	*elm = ber_add_integer(*elm, ipstat.ips_fragments);
	ber_set_header(*elm, BER_CLASS_APPLICATION, SNMP_T_COUNTER32);

	return (0);
}

int
mib_ipreasmoks(struct oid *oid, struct ber_oid *o, struct ber_element **elm)
{
	int		mib[] = { CTL_NET, AF_INET, IPPROTO_IP, IPCTL_STATS };
	struct ipstat	ipstat;
	size_t		len = sizeof(ipstat);

	if (sysctl(mib, sizeof(mib) / sizeof(mib[0]),
	    &ipstat, &len, NULL, 0) == -1)
		return (-1);
	
	*elm = ber_add_integer(*elm, ipstat.ips_reassembled);
	ber_set_header(*elm, BER_CLASS_APPLICATION, SNMP_T_COUNTER32);

	return (0);
}

int
mib_ipreasmfails(struct oid *oid, struct ber_oid *o, struct ber_element **elm)
{
	int		mib[] = { CTL_NET, AF_INET, IPPROTO_IP, IPCTL_STATS };
	struct ipstat	ipstat;
	size_t		len = sizeof(ipstat);
	u_int32_t	counter;

	if (sysctl(mib, sizeof(mib) / sizeof(mib[0]),
	    &ipstat, &len, NULL, 0) == -1)
		return (-1);

	counter = ipstat.ips_fragdropped + ipstat.ips_fragtimeout;

	*elm = ber_add_integer(*elm, counter);
	ber_set_header(*elm, BER_CLASS_APPLICATION, SNMP_T_COUNTER32);

	return (0);
}

int
mib_ipfragoks(struct oid *oid, struct ber_oid *o, struct ber_element **elm)
{
	int		mib[] = { CTL_NET, AF_INET, IPPROTO_IP, IPCTL_STATS };
	struct ipstat	ipstat;
	size_t		len = sizeof(ipstat);

	if (sysctl(mib, sizeof(mib) / sizeof(mib[0]),
	    &ipstat, &len, NULL, 0) == -1)
		return (-1);
	
	*elm = ber_add_integer(*elm, ipstat.ips_fragmented);
	ber_set_header(*elm, BER_CLASS_APPLICATION, SNMP_T_COUNTER32);

	return (0);
}

int
mib_ipfragfails(struct oid *oid, struct ber_oid *o, struct ber_element **elm)
{
	int		mib[] = { CTL_NET, AF_INET, IPPROTO_IP, IPCTL_STATS };
	struct ipstat	ipstat;
	size_t		len = sizeof(ipstat);
	u_int32_t	counter;

	if (sysctl(mib, sizeof(mib) / sizeof(mib[0]),
	    &ipstat, &len, NULL, 0) == -1)
		return (-1);

	counter = ipstat.ips_badfrags + ipstat.ips_cantfrag;
	*elm = ber_add_integer(*elm, counter);
	ber_set_header(*elm, BER_CLASS_APPLICATION, SNMP_T_COUNTER32);

	return (0);
}

int
mib_ipfragcreate(struct oid *oid, struct ber_oid *o, struct ber_element **elm)
{
	int		mib[] = { CTL_NET, AF_INET, IPPROTO_IP, IPCTL_STATS };
	struct ipstat	ipstat;
	size_t		len = sizeof(ipstat);

	if (sysctl(mib, sizeof(mib) / sizeof(mib[0]),
	    &ipstat, &len, NULL, 0) == -1)
		return (-1);
	
	*elm = ber_add_integer(*elm, ipstat.ips_ofragments);
	ber_set_header(*elm, BER_CLASS_APPLICATION, SNMP_T_COUNTER32);

	return (0);
}

int
mib_iproutingdiscards(struct oid *oid, struct ber_oid *o, struct ber_element **elm)
{
	return (0);
}

/*
 * Import all MIBs
 */

void
mib_init(void)
{
	/* SNMPv2-MIB */
	mps_mibtree(base_mib);

	/* IF-MIB */
	mps_mibtree(if_mib);

	/* IP-MIB */
	mps_mibtree(ip_mib);

	/* some http://www.iana.org/assignments/enterprise-numbers */
	mps_mibtree(enterprise_mib);

	/* OPENBSD-MIB */
	mps_mibtree(openbsd_mib);
}
