/*	$OpenBSD: mib.c,v 1.1 2019/08/09 06:17:59 martijn Exp $	*/

/*
 * Copyright (c) 2012 Joel Knight <joel@openbsd.org>
 * Copyright (c) 2007, 2008, 2012 Reyk Floeter <reyk@openbsd.org>
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


#include <sys/tree.h>
#include <sys/types.h>
#include <sys/queue.h>

#include "ber.h"
#include "mib.h"
#include "smi.h"

static struct oid mib_tree[] = MIB_TREE;
static struct oid base_mib[] = {
	{ MIB(mib_2),			OID_MIB },
	{ MIB(sysDescr),		OID_RD },
	{ MIB(sysOID),			OID_RD },
	{ MIB(sysUpTime),		OID_RD },
	{ MIB(sysContact),		OID_RW },
	{ MIB(sysName),			OID_RW },
	{ MIB(sysLocation),		OID_RW },
	{ MIB(sysServices),		OID_RS },
	{ MIB(sysORLastChange),		OID_RD },
	{ MIB(sysORIndex),		OID_TRD },
	{ MIB(sysORID),			OID_TRD },
	{ MIB(sysORDescr),		OID_TRD },
	{ MIB(sysORUpTime),		OID_TRD },
	{ MIB(snmp),			OID_MIB },
	{ MIB(snmpInPkts),		OID_RD },
	{ MIB(snmpOutPkts),		OID_RD },
	{ MIB(snmpInBadVersions),	OID_RD },
	{ MIB(snmpInBadCommunityNames),	OID_RD },
	{ MIB(snmpInBadCommunityUses),	OID_RD },
	{ MIB(snmpInASNParseErrs),	OID_RD },
	{ MIB(snmpInTooBigs),		OID_RD },
	{ MIB(snmpInNoSuchNames),	OID_RD },
	{ MIB(snmpInBadValues),		OID_RD },
	{ MIB(snmpInReadOnlys),		OID_RD },
	{ MIB(snmpInGenErrs),		OID_RD },
	{ MIB(snmpInTotalReqVars),	OID_RD },
	{ MIB(snmpInTotalSetVars),	OID_RD },
	{ MIB(snmpInGetRequests),	OID_RD },
	{ MIB(snmpInGetNexts),		OID_RD },
	{ MIB(snmpInSetRequests),	OID_RD },
	{ MIB(snmpInGetResponses),	OID_RD },
	{ MIB(snmpInTraps),		OID_RD },
	{ MIB(snmpOutTooBigs),		OID_RD },
	{ MIB(snmpOutNoSuchNames),	OID_RD },
	{ MIB(snmpOutBadValues),	OID_RD },
	{ MIB(snmpOutGenErrs),		OID_RD },
	{ MIB(snmpOutGetRequests),	OID_RD },
	{ MIB(snmpOutGetNexts),		OID_RD },
	{ MIB(snmpOutSetRequests),	OID_RD },
	{ MIB(snmpOutGetResponses),	OID_RD },
	{ MIB(snmpOutTraps),		OID_RD },
	{ MIB(snmpEnableAuthenTraps),	OID_RW },
	{ MIB(snmpSilentDrops),		OID_RD },
	{ MIB(snmpProxyDrops),		OID_RD },
	{ MIBEND }
};

static struct oid usm_mib[] = {
	{ MIB(snmpEngine),			OID_MIB },
	{ MIB(snmpEngineID),			OID_RD },
	{ MIB(snmpEngineBoots),			OID_RD },
	{ MIB(snmpEngineTime),			OID_RD },
	{ MIB(snmpEngineMaxMsgSize),		OID_RD },
	{ MIB(usmStats),			OID_MIB },
	{ MIB(usmStatsUnsupportedSecLevels),	OID_RD },
	{ MIB(usmStatsNotInTimeWindow),		OID_RD },
	{ MIB(usmStatsUnknownUserNames),	OID_RD },
	{ MIB(usmStatsUnknownEngineId),		OID_RD },
	{ MIB(usmStatsWrongDigests),		OID_RD },
	{ MIB(usmStatsDecryptionErrors),	OID_RD },
	{ MIBEND }
};

static struct oid hr_mib[] = {
	{ MIB(host),				OID_MIB },
	{ MIB(hrSystemUptime),			OID_RD },
	{ MIB(hrSystemDate),			OID_RD },
	{ MIB(hrSystemProcesses),		OID_RD },
	{ MIB(hrSystemMaxProcesses),		OID_RD },
	{ MIB(hrMemorySize),			OID_RD },
	{ MIB(hrStorageIndex),			OID_TRD },
	{ MIB(hrStorageType),			OID_TRD },
	{ MIB(hrStorageDescr),			OID_TRD },
	{ MIB(hrStorageAllocationUnits),	OID_TRD },
	{ MIB(hrStorageSize),			OID_TRD },
	{ MIB(hrStorageUsed),			OID_TRD },
	{ MIB(hrStorageAllocationFailures),	OID_TRD },
	{ MIB(hrDeviceIndex),			OID_TRD },
	{ MIB(hrDeviceType),			OID_TRD },
	{ MIB(hrDeviceDescr),			OID_TRD },
	{ MIB(hrDeviceID),			OID_TRD },
	{ MIB(hrDeviceStatus),			OID_TRD },
	{ MIB(hrDeviceErrors),			OID_TRD },
	{ MIB(hrProcessorFrwID),		OID_TRD },
	{ MIB(hrProcessorLoad),			OID_TRD },
	{ MIB(hrSWRunIndex),			OID_TRD },
	{ MIB(hrSWRunName),			OID_TRD },
	{ MIB(hrSWRunID),			OID_TRD },
	{ MIB(hrSWRunPath),			OID_TRD },
	{ MIB(hrSWRunParameters),		OID_TRD },
	{ MIB(hrSWRunType),			OID_TRD },
	{ MIB(hrSWRunStatus),			OID_TRD },
	{ MIBEND }
};

static struct oid if_mib[] = {
	{ MIB(ifMIB),			OID_MIB },
	{ MIB(ifName),			OID_TRD },
	{ MIB(ifInMulticastPkts),	OID_TRD },
	{ MIB(ifInBroadcastPkts),	OID_TRD },
	{ MIB(ifOutMulticastPkts),	OID_TRD },
	{ MIB(ifOutBroadcastPkts),	OID_TRD },
	{ MIB(ifHCInOctets),		OID_TRD },
	{ MIB(ifHCInUcastPkts),		OID_TRD },
	{ MIB(ifHCInMulticastPkts),	OID_TRD },
	{ MIB(ifHCInBroadcastPkts),	OID_TRD },
	{ MIB(ifHCOutOctets),		OID_TRD },
	{ MIB(ifHCOutUcastPkts),	OID_TRD },
	{ MIB(ifHCOutMulticastPkts),	OID_TRD },
	{ MIB(ifHCOutBroadcastPkts),	OID_TRD },
	{ MIB(ifLinkUpDownTrapEnable),	OID_TRD },
	{ MIB(ifHighSpeed),		OID_TRD },
	{ MIB(ifPromiscuousMode),	OID_TRD },
	{ MIB(ifConnectorPresent),	OID_TRD },
	{ MIB(ifAlias),			OID_TRD },
	{ MIB(ifCounterDiscontinuityTime), OID_TRD },
	{ MIB(ifRcvAddressStatus),	OID_TRD },
	{ MIB(ifRcvAddressType),	OID_TRD },
	{ MIB(ifStackLastChange),	OID_RD },
	{ MIB(ifNumber),		OID_RD },
	{ MIB(ifIndex),			OID_TRD },
	{ MIB(ifDescr),			OID_TRD },
	{ MIB(ifType),			OID_TRD },
	{ MIB(ifMtu),			OID_TRD },
	{ MIB(ifSpeed),			OID_TRD },
	{ MIB(ifPhysAddress),		OID_TRD },
	{ MIB(ifAdminStatus),		OID_TRD },
	{ MIB(ifOperStatus),		OID_TRD },
	{ MIB(ifLastChange),		OID_TRD },
	{ MIB(ifInOctets),		OID_TRD },
	{ MIB(ifInUcastPkts),		OID_TRD },
	{ MIB(ifInNUcastPkts),		OID_TRD },
	{ MIB(ifInDiscards),		OID_TRD },
	{ MIB(ifInErrors),		OID_TRD },
	{ MIB(ifInUnknownProtos),	OID_TRD },
	{ MIB(ifOutOctets),		OID_TRD },
	{ MIB(ifOutUcastPkts),		OID_TRD },
	{ MIB(ifOutNUcastPkts),		OID_TRD },
	{ MIB(ifOutDiscards),		OID_TRD },
	{ MIB(ifOutErrors),		OID_TRD },
	{ MIB(ifOutQLen),		OID_TRD },
	{ MIB(ifSpecific),		OID_TRD },
	{ MIBEND }
};

static struct oid ip_mib[] = {
	{ MIB(ipMIB),			OID_MIB },
	{ MIB(ipForwarding),		OID_RD },
	{ MIB(ipDefaultTTL),		OID_RD },
	{ MIB(ipInReceives),		OID_RD },
	{ MIB(ipInHdrErrors),		OID_RD },
	{ MIB(ipInAddrErrors),		OID_RD },
	{ MIB(ipForwDatagrams),		OID_RD },
	{ MIB(ipInUnknownProtos),	OID_RD },
	{ MIB(ipInDelivers),		OID_RD },
	{ MIB(ipOutRequests),		OID_RD },
	{ MIB(ipOutDiscards),		OID_RD },
	{ MIB(ipOutNoRoutes),		OID_RD },
	{ MIB(ipReasmTimeout),		OID_RD },
	{ MIB(ipReasmReqds),		OID_RD },
	{ MIB(ipReasmOKs),		OID_RD },
	{ MIB(ipReasmFails),		OID_RD },
	{ MIB(ipFragOKs),		OID_RD },
	{ MIB(ipFragFails),		OID_RD },
	{ MIB(ipFragCreates),		OID_RD },
	{ MIB(ipAdEntAddr),		OID_TRD },
	{ MIB(ipAdEntIfIndex),		OID_TRD },
	{ MIB(ipAdEntNetMask),		OID_TRD },
	{ MIB(ipAdEntBcastAddr),	OID_TRD },
	{ MIB(ipAdEntReasmMaxSize),	OID_TRD },
	{ MIB(ipNetToMediaIfIndex),	OID_TRD },
	{ MIB(ipNetToMediaPhysAddress),	OID_TRD },
	{ MIB(ipNetToMediaNetAddress),	OID_TRD },
	{ MIB(ipNetToMediaType),	OID_TRD },
	{ MIBEND }
};

static struct oid ipf_mib[] = {
	{ MIB(ipfMIB),			OID_MIB },
	{ MIB(ipfInetCidrRouteNumber),	OID_RD },
	{ MIB(ipfRouteEntIfIndex),	OID_TRD },
	{ MIB(ipfRouteEntType),		OID_TRD },
	{ MIB(ipfRouteEntProto),	OID_TRD },
	{ MIB(ipfRouteEntAge),		OID_TRD },
	{ MIB(ipfRouteEntNextHopAS),	OID_TRD },
	{ MIB(ipfRouteEntRouteMetric1),	OID_TRD },
	{ MIB(ipfRouteEntRouteMetric2),	OID_TRD },
	{ MIB(ipfRouteEntRouteMetric3),	OID_TRD },
	{ MIB(ipfRouteEntRouteMetric4),	OID_TRD },
	{ MIB(ipfRouteEntRouteMetric5),	OID_TRD },
	{ MIB(ipfRouteEntStatus),	OID_TRD },
	{ MIBEND }
};

static struct oid bridge_mib[] = {
	{ MIB(dot1dBridge),		OID_MIB },
	{ MIB(dot1dBaseBridgeAddress) },
	{ MIB(dot1dBaseNumPorts),	OID_RD },
	{ MIB(dot1dBaseType),		OID_RD },
	{ MIB(dot1dBasePort),		OID_TRD },
	{ MIB(dot1dBasePortIfIndex),	OID_TRD },
	{ MIB(dot1dBasePortCircuit),	OID_TRD},
	{ MIB(dot1dBasePortDelayExceededDiscards), OID_TRD },
	{ MIB(dot1dBasePortMtuExceededDiscards), OID_TRD },
	{ MIBEND }
};

static struct oid diskio_mib[] = {
	{ MIB(ucdDiskIOMIB),			OID_MIB },
	{ MIB(diskIOIndex),			OID_TRD },
	{ MIB(diskIODevice),			OID_TRD },
	{ MIB(diskIONRead),			OID_TRD },
	{ MIB(diskIONWritten),			OID_TRD },
	{ MIB(diskIOReads),			OID_TRD },
	{ MIB(diskIOWrites),			OID_TRD },
	{ MIB(diskIONReadX),			OID_TRD },
	{ MIB(diskIONWrittenX),			OID_TRD },
	{ MIBEND }
};

static struct oid openbsd_mib[] = {
	{ MIB(pfMIBObjects),		OID_MIB },
	{ MIB(pfRunning),		OID_RD },
	{ MIB(pfRuntime),		OID_RD },
	{ MIB(pfDebug),			OID_RD },
	{ MIB(pfHostid),		OID_RD },
	{ MIB(pfCntMatch),		OID_RD },
	{ MIB(pfCntBadOffset),		OID_RD },
	{ MIB(pfCntFragment),		OID_RD },
	{ MIB(pfCntShort),		OID_RD },
	{ MIB(pfCntNormalize),		OID_RD },
	{ MIB(pfCntMemory),		OID_RD },
	{ MIB(pfCntTimestamp),		OID_RD },
	{ MIB(pfCntCongestion),		OID_RD },
	{ MIB(pfCntIpOptions),		OID_RD },
	{ MIB(pfCntProtoCksum),		OID_RD },
	{ MIB(pfCntStateMismatch),	OID_RD },
	{ MIB(pfCntStateInsert),	OID_RD },
	{ MIB(pfCntStateLimit),		OID_RD },
	{ MIB(pfCntSrcLimit),		OID_RD },
	{ MIB(pfCntSynproxy),		OID_RD },
	{ MIB(pfCntTranslate),		OID_RD },
	{ MIB(pfCntNoRoute),		OID_RD },
	{ MIB(pfStateCount),		OID_RD },
	{ MIB(pfStateSearches),		OID_RD },
	{ MIB(pfStateInserts),		OID_RD },
	{ MIB(pfStateRemovals),		OID_RD },
	{ MIB(pfLogIfName),		OID_RD },
	{ MIB(pfLogIfIpBytesIn),	OID_RD },
	{ MIB(pfLogIfIpBytesOut),	OID_RD },
	{ MIB(pfLogIfIpPktsInPass),	OID_RD },
	{ MIB(pfLogIfIpPktsInDrop),	OID_RD },
	{ MIB(pfLogIfIpPktsOutPass),	OID_RD },
	{ MIB(pfLogIfIpPktsOutDrop),	OID_RD },
	{ MIB(pfLogIfIp6BytesIn),	OID_RD },
	{ MIB(pfLogIfIp6BytesOut),	OID_RD },
	{ MIB(pfLogIfIp6PktsInPass),	OID_RD },
	{ MIB(pfLogIfIp6PktsInDrop),	OID_RD },
	{ MIB(pfLogIfIp6PktsOutPass),	OID_RD },
	{ MIB(pfLogIfIp6PktsOutDrop),	OID_RD },
	{ MIB(pfSrcTrackCount),		OID_RD },
	{ MIB(pfSrcTrackSearches),	OID_RD },
	{ MIB(pfSrcTrackInserts),	OID_RD },
	{ MIB(pfSrcTrackRemovals),	OID_RD },
	{ MIB(pfLimitStates),		OID_RD },
	{ MIB(pfLimitSourceNodes),	OID_RD },
	{ MIB(pfLimitFragments),	OID_RD },
	{ MIB(pfLimitMaxTables),	OID_RD },
	{ MIB(pfLimitMaxTableEntries),	OID_RD },
	{ MIB(pfTimeoutTcpFirst),	OID_RD },
	{ MIB(pfTimeoutTcpOpening),	OID_RD },
	{ MIB(pfTimeoutTcpEstablished),	OID_RD },
	{ MIB(pfTimeoutTcpClosing),	OID_RD },
	{ MIB(pfTimeoutTcpFinWait),	OID_RD },
	{ MIB(pfTimeoutTcpClosed),	OID_RD },
	{ MIB(pfTimeoutUdpFirst),	OID_RD },
	{ MIB(pfTimeoutUdpSingle),	OID_RD },
	{ MIB(pfTimeoutUdpMultiple),	OID_RD },
	{ MIB(pfTimeoutIcmpFirst),	OID_RD },
	{ MIB(pfTimeoutIcmpError),	OID_RD },
	{ MIB(pfTimeoutOtherFirst),	OID_RD },
	{ MIB(pfTimeoutOtherSingle),	OID_RD },
	{ MIB(pfTimeoutOtherMultiple),	OID_RD },
	{ MIB(pfTimeoutFragment),	OID_RD },
	{ MIB(pfTimeoutInterval),	OID_RD },
	{ MIB(pfTimeoutAdaptiveStart),	OID_RD },
	{ MIB(pfTimeoutAdaptiveEnd),	OID_RD },
	{ MIB(pfTimeoutSrcTrack),	OID_RD },
	{ MIB(pfIfNumber),		OID_RD },
	{ MIB(pfIfIndex),		OID_TRD },
	{ MIB(pfIfDescr),		OID_TRD },
	{ MIB(pfIfType),		OID_TRD },
	{ MIB(pfIfRefs),		OID_TRD },
	{ MIB(pfIfRules),		OID_TRD },
	{ MIB(pfIfIn4PassPkts),		OID_TRD },
	{ MIB(pfIfIn4PassBytes),	OID_TRD },
	{ MIB(pfIfIn4BlockPkts),	OID_TRD },
	{ MIB(pfIfIn4BlockBytes),	OID_TRD },
	{ MIB(pfIfOut4PassPkts),	OID_TRD },
	{ MIB(pfIfOut4PassBytes),	OID_TRD },
	{ MIB(pfIfOut4BlockPkts),	OID_TRD },
	{ MIB(pfIfOut4BlockBytes),	OID_TRD },
	{ MIB(pfIfIn6PassPkts),		OID_TRD },
	{ MIB(pfIfIn6PassBytes),	OID_TRD },
	{ MIB(pfIfIn6BlockPkts),	OID_TRD },
	{ MIB(pfIfIn6BlockBytes),	OID_TRD },
	{ MIB(pfIfOut6PassPkts),	OID_TRD },
	{ MIB(pfIfOut6PassBytes),	OID_TRD },
	{ MIB(pfIfOut6BlockPkts),	OID_TRD },
	{ MIB(pfIfOut6BlockBytes),	OID_TRD },
	{ MIB(pfTblNumber),		OID_RD },
	{ MIB(pfTblIndex),		OID_TRD },
	{ MIB(pfTblName),		OID_TRD },
	{ MIB(pfTblAddresses),		OID_TRD },
	{ MIB(pfTblAnchorRefs),		OID_TRD },
	{ MIB(pfTblRuleRefs),		OID_TRD },
	{ MIB(pfTblEvalsMatch),		OID_TRD },
	{ MIB(pfTblEvalsNoMatch),	OID_TRD },
	{ MIB(pfTblInPassPkts),		OID_TRD },
	{ MIB(pfTblInPassBytes),	OID_TRD },
	{ MIB(pfTblInBlockPkts),	OID_TRD },
	{ MIB(pfTblInBlockBytes),	OID_TRD },
	{ MIB(pfTblInXPassPkts),	OID_TRD },
	{ MIB(pfTblInXPassBytes),	OID_TRD },
	{ MIB(pfTblOutPassPkts),	OID_TRD },
	{ MIB(pfTblOutPassBytes),	OID_TRD },
	{ MIB(pfTblOutBlockPkts),	OID_TRD },
	{ MIB(pfTblOutBlockBytes),	OID_TRD },
	{ MIB(pfTblOutXPassPkts),	OID_TRD },
	{ MIB(pfTblOutXPassBytes),	OID_TRD },
	{ MIB(pfTblStatsCleared),	OID_TRD },
	{ MIB(pfTblInMatchPkts),	OID_TRD },
	{ MIB(pfTblInMatchBytes),	OID_TRD },
	{ MIB(pfTblOutMatchPkts),	OID_TRD },
	{ MIB(pfTblOutMatchBytes),	OID_TRD },
	{ MIB(pfTblAddrTblIndex),	OID_TRD },
	{ MIB(pfTblAddrNet),		OID_TRD },
	{ MIB(pfTblAddrMask),		OID_TRD },
	{ MIB(pfTblAddrCleared),	OID_TRD },
	{ MIB(pfTblAddrInBlockPkts),	OID_TRD },
	{ MIB(pfTblAddrInBlockBytes),	OID_TRD },
	{ MIB(pfTblAddrInPassPkts),	OID_TRD },
	{ MIB(pfTblAddrInPassBytes),	OID_TRD },
	{ MIB(pfTblAddrOutBlockPkts),	OID_TRD },
	{ MIB(pfTblAddrOutBlockBytes),	OID_TRD },
	{ MIB(pfTblAddrOutPassPkts),	OID_TRD },
	{ MIB(pfTblAddrOutPassBytes),	OID_TRD },
	{ MIB(pfTblAddrInMatchPkts),	OID_TRD },
	{ MIB(pfTblAddrInMatchBytes),	OID_TRD },
	{ MIB(pfTblAddrOutMatchPkts),	OID_TRD },
	{ MIB(pfTblAddrOutMatchBytes),	OID_TRD },
	{ MIB(pfLabelNumber),		OID_RD },
	{ MIB(pfLabelIndex),		OID_TRD },
	{ MIB(pfLabelName),		OID_TRD },
	{ MIB(pfLabelEvals),		OID_TRD },
	{ MIB(pfLabelPkts),		OID_TRD },
	{ MIB(pfLabelBytes),		OID_TRD },
	{ MIB(pfLabelInPkts),		OID_TRD },
	{ MIB(pfLabelInBytes),		OID_TRD },
	{ MIB(pfLabelOutPkts),		OID_TRD },
	{ MIB(pfLabelOutBytes),		OID_TRD },
	{ MIB(pfLabelTotalStates),	OID_TRD },
	{ MIB(pfsyncIpPktsRecv),	OID_RD },
	{ MIB(pfsyncIp6PktsRecv),	OID_RD },
	{ MIB(pfsyncPktDiscardsForBadInterface), OID_RD },
	{ MIB(pfsyncPktDiscardsForBadTtl), OID_RD },
	{ MIB(pfsyncPktShorterThanHeader), OID_RD },
	{ MIB(pfsyncPktDiscardsForBadVersion), OID_RD },
	{ MIB(pfsyncPktDiscardsForBadAction), OID_RD },
	{ MIB(pfsyncPktDiscardsForBadLength), OID_RD },
	{ MIB(pfsyncPktDiscardsForBadAuth), OID_RD },
	{ MIB(pfsyncPktDiscardsForStaleState), OID_RD },
	{ MIB(pfsyncPktDiscardsForBadValues), OID_RD },
	{ MIB(pfsyncPktDiscardsForBadState), OID_RD },
	{ MIB(pfsyncIpPktsSent),	OID_RD },
	{ MIB(pfsyncIp6PktsSent),	OID_RD },
	{ MIB(pfsyncNoMemory),		OID_RD },
	{ MIB(pfsyncOutputErrors),	OID_RD },
	{ MIB(sensorsMIBObjects),	OID_MIB },
	{ MIB(sensorNumber),		OID_RD },
	{ MIB(sensorIndex),		OID_TRD },
	{ MIB(sensorDescr),		OID_TRD },
	{ MIB(sensorType),		OID_TRD },
	{ MIB(sensorDevice),		OID_TRD },
	{ MIB(sensorValue),		OID_TRD },
	{ MIB(sensorUnits),		OID_TRD },
	{ MIB(sensorStatus),		OID_TRD },
	{ MIB(carpMIBObjects),		OID_MIB },
	{ MIB(carpAllow),		OID_RD },
	{ MIB(carpPreempt),		OID_RD },
	{ MIB(carpLog),			OID_RD },
	{ MIB(carpIpPktsRecv),		OID_RD },
	{ MIB(carpIp6PktsRecv),		OID_RD },
	{ MIB(carpPktDiscardsBadIface),	OID_RD },
	{ MIB(carpPktDiscardsBadTtl),	OID_RD },
	{ MIB(carpPktShorterThanHdr),	OID_RD },
	{ MIB(carpDiscardsBadCksum),	OID_RD },
	{ MIB(carpDiscardsBadVersion),	OID_RD },
	{ MIB(carpDiscardsTooShort),	OID_RD },
	{ MIB(carpDiscardsBadAuth),	OID_RD },
	{ MIB(carpDiscardsBadVhid),	OID_RD },
	{ MIB(carpDiscardsBadAddrList),	OID_RD },
	{ MIB(carpIpPktsSent),		OID_RD },
	{ MIB(carpIp6PktsSent),		OID_RD },
	{ MIB(carpNoMemory),		OID_RD },
	{ MIB(carpTransitionsToMaster),	OID_RD },
	{ MIB(carpIfNumber),		OID_RD },
	{ MIB(carpIfIndex),		OID_TRD },
	{ MIB(carpIfDescr),		OID_TRD },
	{ MIB(carpIfVhid),		OID_TRD },
	{ MIB(carpIfDev	),		OID_TRD },
	{ MIB(carpIfAdvbase),		OID_TRD },
	{ MIB(carpIfAdvskew),		OID_TRD },
	{ MIB(carpIfState),		OID_TRD },
	{ MIB(carpGroupName),		OID_TRD },
	{ MIB(carpGroupDemote),		OID_TRD },
	{ MIB(memMIBObjects),		OID_MIB },
	{ MIB(memMIBVersion),		OID_RD },
	{ MIB(memIfName),		OID_TRD },
	{ MIB(memIfLiveLocks),		OID_TRD },
	{ MIBEND }
};

void
mib_init(void)
{
	/*
	 * MIB declarations (to register the OID names)
	 */
	smi_mibtree(mib_tree);

	/*
	 * MIB definitions (the implementation)
	 */

	/* SNMPv2-MIB */
	smi_mibtree(base_mib);

	/* SNMP-USER-BASED-SM-MIB */
	smi_mibtree(usm_mib);

	/* HOST-RESOURCES-MIB */
	smi_mibtree(hr_mib);

	/* IF-MIB */
	smi_mibtree(if_mib);

	/* IP-MIB */
	smi_mibtree(ip_mib);

	/* IP-FORWARD-MIB */
	smi_mibtree(ipf_mib);

	/* BRIDGE-MIB */
	smi_mibtree(bridge_mib);

	/* UCD-DISKIO-MIB */
	smi_mibtree(diskio_mib);

	/* OPENBSD-MIB */
	smi_mibtree(openbsd_mib);
}
