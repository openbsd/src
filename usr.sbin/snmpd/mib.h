/*	$OpenBSD: mib.h,v 1.12 2008/01/18 18:38:35 reyk Exp $	*/

/*
 * Copyright (c) 2007, 2008 Reyk Floeter <reyk@vantronix.net>
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

#ifndef _SNMPD_MIB_H
#define _SNMPD_MIB_H

/*
 * Adding new MIBs:
 * - add the OID definitions below
 * - add the OIDs to the MIB_TREE table at the end of this file
 * - optional: write the implementation in mib.c
 */

/* From the SNMPv2-SMI MIB */
#define MIB_iso				1
#define MIB_org				MIB_iso, 3
#define MIB_dod				MIB_org, 6
#define MIB_internet			MIB_dod, 1
#define MIB_directory			MIB_internet, 1
#define MIB_mgmt			MIB_internet, 2
#define MIB_mib_2			MIB_mgmt, 1	/* XXX mib-2 */
#define MIB_system			MIB_mib_2, 1
#define OIDIDX_system			7
#define MIB_sysDescr			MIB_system, 1
#define MIB_sysOID			MIB_system, 2
#define MIB_sysUpTime			MIB_system, 3
#define MIB_sysContact			MIB_system, 4
#define MIB_sysName			MIB_system, 5
#define MIB_sysLocation			MIB_system, 6
#define MIB_sysServices			MIB_system, 7
#define MIB_sysORLastChange		MIB_system, 8
#define MIB_sysORTable			MIB_system, 9
#define MIB_sysOREntry			MIB_sysORTable, 1
#define OIDIDX_sysOR			9
#define OIDIDX_sysOREntry		10
#define MIB_sysORIndex			MIB_sysOREntry, 1
#define MIB_sysORID			MIB_sysOREntry, 2
#define MIB_sysORDescr			MIB_sysOREntry, 3
#define MIB_sysORUpTime			MIB_sysOREntry, 4
#define MIB_transmission		MIB_mib_2, 10
#define MIB_snmp			MIB_mib_2, 11
#define OIDIDX_snmp			7
#define MIB_snmpInPkts			MIB_snmp, 1
#define MIB_snmpOutPkts			MIB_snmp, 2
#define MIB_snmpInBadVersions		MIB_snmp, 3
#define MIB_snmpInBadCommunityNames	MIB_snmp, 4
#define MIB_snmpInBadCommunityUses	MIB_snmp, 5
#define MIB_snmpInASNParseErrs		MIB_snmp, 6
#define MIB_snmpInTooBigs		MIB_snmp, 8
#define MIB_snmpInNoSuchNames		MIB_snmp, 9
#define MIB_snmpInBadValues		MIB_snmp, 10
#define MIB_snmpInReadOnlys		MIB_snmp, 11
#define MIB_snmpInGenErrs		MIB_snmp, 12
#define MIB_snmpInTotalReqVars		MIB_snmp, 13
#define MIB_snmpInTotalSetVars		MIB_snmp, 14
#define MIB_snmpInGetRequests		MIB_snmp, 15
#define MIB_snmpInGetNexts		MIB_snmp, 16
#define MIB_snmpInSetRequests		MIB_snmp, 17
#define MIB_snmpInGetResponses		MIB_snmp, 18
#define MIB_snmpInTraps			MIB_snmp, 19
#define MIB_snmpOutTooBigs		MIB_snmp, 20
#define MIB_snmpOutNoSuchNames		MIB_snmp, 21
#define MIB_snmpOutBadValues		MIB_snmp, 22
#define MIB_snmpOutGenErrs		MIB_snmp, 24
#define MIB_snmpOutGetRequests		MIB_snmp, 25
#define MIB_snmpOutGetNexts		MIB_snmp, 26
#define MIB_snmpOutSetRequests		MIB_snmp, 27
#define MIB_snmpOutGetResponses		MIB_snmp, 28
#define MIB_snmpOutTraps		MIB_snmp, 29
#define MIB_snmpEnableAuthenTraps	MIB_snmp, 30
#define MIB_snmpSilentDrops		MIB_snmp, 31
#define MIB_snmpProxyDrops		MIB_snmp, 32
#define MIB_experimental		MIB_internet, 3
#define MIB_private			MIB_internet, 4
#define MIB_enterprises			MIB_private, 1
#define MIB_security			MIB_internet, 5
#define MIB_snmpV2			MIB_internet, 6
#define MIB_snmpDomains			MIB_snmpV2, 1
#define MIB_snmpProxies			MIB_snmpV2, 2
#define MIB_snmpModules			MIB_snmpV2, 3
#define MIB_snmpMIB			MIB_snmpModules, 1
#define MIB_snmpMIBObjects		MIB_snmpMIB, 1
#define MIB_snmpTrap			MIB_snmpMIBObjects, 4
#define MIB_snmpTrapOID			MIB_snmpTrap, 1
#define MIB_snmpTrapEnterprise		MIB_snmpTrap, 3
#define MIB_snmpTraps			MIB_snmpMIBObjects, 5
#define MIB_coldStart			MIB_snmpTraps, 1
#define MIB_warmStart			MIB_snmpTraps, 2
#define MIB_linkDown			MIB_snmpTraps, 3
#define MIB_linkUp			MIB_snmpTraps, 4
#define MIB_authenticationFailure	MIB_snmpTraps, 5
#define MIB_egpNeighborLoss		MIB_snmpTraps, 6

/* IF-MIB */
#define MIB_ifMIB			MIB_mib_2, 31
#define MIB_ifMIBObjects		MIB_ifMIB, 1
#define MIB_ifXTable			MIB_ifMIBObjects, 1
#define MIB_ifXEntry			MIB_ifXTable, 1
#define OIDIDX_ifX			10
#define OIDIDX_ifXEntry			11
#define MIB_ifName			MIB_ifXEntry, 1
#define MIB_ifInMulticastPkts		MIB_ifXEntry, 2
#define MIB_ifInBroadcastPkts		MIB_ifXEntry, 3
#define MIB_ifOutMulticastPkts		MIB_ifXEntry, 4
#define MIB_ifOutBroadcastPkts		MIB_ifXEntry, 5
#define MIB_ifHCInOctets		MIB_ifXEntry, 6
#define MIB_ifHCInUcastPkts		MIB_ifXEntry, 7
#define MIB_ifHCInMulticastPkts		MIB_ifXEntry, 8
#define MIB_ifHCInBroadcastPkts		MIB_ifXEntry, 9
#define MIB_ifHCOutOctets		MIB_ifXEntry, 10
#define MIB_ifHCOutUcastPkts		MIB_ifXEntry, 11
#define MIB_ifHCOutMulticastPkts	MIB_ifXEntry, 12
#define MIB_ifHCOutBroadcastPkts	MIB_ifXEntry, 13
#define MIB_ifLinkUpDownTrapEnable	MIB_ifXEntry, 14
#define MIB_ifHighSpeed			MIB_ifXEntry, 15
#define MIB_ifPromiscuousMode		MIB_ifXEntry, 16
#define MIB_ifConnectorPresent		MIB_ifXEntry, 17
#define MIB_ifAlias			MIB_ifXEntry, 18
#define MIB_ifCounterDiscontinuityTime	MIB_ifXEntry, 19
#define MIB_ifStackTable		MIB_ifMIBObjects, 2
#define MIB_ifStackEntry		MIB_ifStackTable, 1
#define OIDIDX_ifStack			10
#define OIDIDX_ifStackEntry		11
#define MIB_ifStackStatus		MIB_ifStackEntry, 3
#define MIB_ifRcvAddressTable		MIB_ifMIBObjects, 4
#define MIB_ifRcvAddressEntry		MIB_ifRcvAddressTable, 1
#define OIDIDX_ifRcvAddress		10
#define OIDIDX_ifRcvAddressEntry	11
#define MIB_ifRcvAddressStatus		MIB_ifRcvAddressEntry, 2
#define MIB_ifRcvAddressType		MIB_ifRcvAddressEntry, 3
#define MIB_ifStackLastChange		MIB_ifMIBObjects, 6
#define MIB_interfaces			MIB_mib_2, 2
#define MIB_ifNumber			MIB_interfaces, 1
#define MIB_ifTable			MIB_interfaces, 2
#define MIB_ifEntry			MIB_ifTable, 1
#define OIDIDX_if			9
#define OIDIDX_ifEntry			10
#define MIB_ifIndex			MIB_ifEntry, 1
#define MIB_ifDescr			MIB_ifEntry, 2
#define MIB_ifType			MIB_ifEntry, 3
#define MIB_ifMtu			MIB_ifEntry, 4
#define MIB_ifSpeed			MIB_ifEntry, 5
#define MIB_ifPhysAddress		MIB_ifEntry, 6
#define MIB_ifAdminStatus		MIB_ifEntry, 7
#define MIB_ifOperStatus		MIB_ifEntry, 8
#define MIB_ifLastChange		MIB_ifEntry, 9
#define MIB_ifInOctets			MIB_ifEntry, 10
#define MIB_ifInUcastPkts		MIB_ifEntry, 11
#define MIB_ifInNUcastPkts		MIB_ifEntry, 12
#define MIB_ifInDiscards		MIB_ifEntry, 13
#define MIB_ifInErrors			MIB_ifEntry, 14
#define MIB_ifInUnknownErrors		MIB_ifEntry, 15
#define MIB_ifOutOctets			MIB_ifEntry, 16
#define MIB_ifOutUcastPkts		MIB_ifEntry, 17
#define MIB_ifOutNUcastPkts		MIB_ifEntry, 18
#define MIB_ifOutDiscards		MIB_ifEntry, 19
#define MIB_ifOutErrors			MIB_ifEntry, 20
#define MIB_ifOutQLen			MIB_ifEntry, 21
#define MIB_ifSpecific			MIB_ifEntry, 22

/* IP-MIB */
#define MIB_ipMIB			MIB_mib_2, 4
#define OIDIDX_ip			7
#define MIB_ipForwarding		MIB_ipMIB, 1
#define MIB_ipDefaultTTL		MIB_ipMIB, 2
#define MIB_ipInReceives		MIB_ipMIB, 3
#define MIB_ipInHdrErrors		MIB_ipMIB, 4
#define MIB_ipInAddrErrors		MIB_ipMIB, 5
#define MIB_ipForwDatagrams		MIB_ipMIB, 6
#define MIB_ipInUnknownProtos		MIB_ipMIB, 7
#define MIB_ipInDiscards		MIB_ipMIB, 8
#define MIB_ipInDelivers		MIB_ipMIB, 9
#define MIB_ipOutRequests		MIB_ipMIB, 10
#define MIB_ipOutDiscards		MIB_ipMIB, 11
#define MIB_ipOutNoRoutes		MIB_ipMIB, 12
#define MIB_ipReasmTimeout		MIB_ipMIB, 13
#define MIB_ipReasmReqds		MIB_ipMIB, 14
#define MIB_ipReasmOKs			MIB_ipMIB, 15
#define MIB_ipReasmFails		MIB_ipMIB, 16
#define MIB_ipFragOKs			MIB_ipMIB, 17
#define MIB_ipFragFails			MIB_ipMIB, 18
#define MIB_ipFragCreates		MIB_ipMIB, 19
#define MIB_ipAddrTable			MIB_ipMIB, 20
#define MIB_ipAddrEntry			MIB_ipAddrTable, 1
#define OIDIDX_ipAddr			9
#define OIDIDX_ipAddrEntry		10
#define MIB_ipAdEntAddr			MIB_ipAddrEntry, 1
#define MIB_ipAdEntIfIndex		MIB_ipAddrEntry, 2
#define MIB_ipAdEntNetMask		MIB_ipAddrEntry, 3
#define MIB_ipAdEntBcastAddr		MIB_ipAddrEntry, 4
#define MIB_ipAdEntReasmMaxSize		MIB_ipAddrEntry, 5
#define MIB_ipNetToMediaTable		MIB_ipMIB, 22
#define MIB_ipNetToMediaEntry		MIB_ipNetToMediaTable, 1
#define MIB_ipNetToMediaIfIndex		MIB_ipNetToMediaEntry, 1
#define MIB_ipNetToMediaPhysAddress	MIB_ipNetToMediaEntry, 2
#define MIB_ipNetToMediaNetAddress	MIB_ipNetToMediaEntry, 3
#define MIB_ipNetToMediaType		MIB_ipNetToMediaEntry, 4
#define MIB_ipRoutingDiscards		MIB_ipMIB, 23

/* BRIDGE-MIB */
#define MIB_dot1dBridge			MIB_mib_2, 17
#define MIB_dot1dBase			MIB_dot1dBridge, 1
#define MIB_dot1dBaseBridgeAddress	MIB_dot1dBase, 1
#define MIB_dot1dBaseNumPorts		MIB_dot1dBase, 2
#define MIB_dot1dBaseType		MIB_dot1dBase, 3
#define MIB_dot1dBasePortTable		MIB_dot1dBase, 4
#define OIDIDX_dot1d			10
#define OIDIDX_dot1dEntry		11
#define MIB_dot1dBasePortEntry		MIB_dot1dBasePortTable, 1
#define MIB_dot1dBasePort		MIB_dot1dBasePortEntry, 1
#define MIB_dot1dBasePortIfIndex	MIB_dot1dBasePortEntry, 2
#define MIB_dot1dBasePortCircuit	MIB_dot1dBasePortEntry, 3
#define MIB_dot1dBasePortDelayExceededDiscards	MIB_dot1dBasePortEntry, 4
#define MIB_dot1dBasePortMtuExceededDiscards	MIB_dot1dBasePortEntry, 5
#define MIB_dot1dStp			MIB_dot1dBridge, 2
#define MIB_dot1dSr			MIB_dot1dBridge, 3
#define MIB_dot1dTp			MIB_dot1dBridge, 4
#define MIB_dot1dStatic			MIB_dot1dBridge, 5

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
#define MIB_ibm				MIB_enterprises, 2
#define MIB_cmu				MIB_enterprises, 3
#define MIB_unix			MIB_enterprises, 4
#define MIB_ciscoSystems		MIB_enterprises, 9
#define MIB_hp				MIB_enterprises, 11
#define MIB_mit				MIB_enterprises, 20
#define MIB_nortelNetworks		MIB_enterprises, 35
#define MIB_sun				MIB_enterprises, 42
#define MIB_3com			MIB_enterprises, 43
#define MIB_synOptics			MIB_enterprises, 45
#define MIB_enterasys			MIB_enterprises, 52
#define MIB_sgi				MIB_enterprises, 59
#define MIB_apple			MIB_enterprises, 63
#define MIB_att				MIB_enterprises, 74
#define MIB_nokia			MIB_enterprises, 94
#define MIB_cern			MIB_enterprises, 96
#define MIB_fsc				MIB_enterprises, 231
#define MIB_compaq			MIB_enterprises, 232
#define MIB_dell			MIB_enterprises, 674
#define MIB_alteon			MIB_enterprises, 1872
#define MIB_extremeNetworks		MIB_enterprises, 1916
#define MIB_foundryNetworks		MIB_enterprises, 1991
#define MIB_huawaiTechnology		MIB_enterprises, 2011
#define MIB_ucDavis			MIB_enterprises, 2021
#define MIB_checkPoint			MIB_enterprises, 2620
#define MIB_juniper			MIB_enterprises, 2636
#define MIB_force10Networks		MIB_enterprises, 6027
#define MIB_alcatelLucent		MIB_enterprises, 7483
#define MIB_snom			MIB_enterprises, 7526
#define MIB_google			MIB_enterprises, 11129
#define MIB_f5Networks			MIB_enterprises, 12276
#define MIB_sFlow			MIB_enterprises, 14706
#define MIB_microSystems		MIB_enterprises, 18623
#define MIB_vantronix			MIB_enterprises, 26766
#define MIB_openBSD			MIB_enterprises, 30155

/* OPENBSD-MIB */
#define MIB_SYSOID_DEFAULT		MIB_openBSD, 23, 1
#define MIB_sensorMIBObjects		MIB_openBSD, 2
#define MIB_sensors			MIB_sensorMIBObjects, 1
#define MIB_sensorNumber		MIB_sensors, 1
#define MIB_sensorTable			MIB_sensors, 2
#define MIB_sensorEntry			MIB_sensorTable, 1
#define OIDIDX_sensor			11
#define OIDIDX_sensorEntry		12
#define MIB_sensorIndex			MIB_sensorEntry, 1
#define MIB_sensorDescr			MIB_sensorEntry, 2
#define MIB_sensorType			MIB_sensorEntry, 3
#define MIB_sensorDevice		MIB_sensorEntry, 4
#define MIB_sensorValue			MIB_sensorEntry, 5
#define MIB_sensorUnits			MIB_sensorEntry, 6
#define MIB_sensorStatus		MIB_sensorEntry, 7

#define MIB_TREE			{		\
	{ MIBDECL(iso) },				\
	{ MIBDECL(org) },				\
	{ MIBDECL(dod) },				\
	{ MIBDECL(internet) },				\
	{ MIBDECL(directory) },				\
	{ MIBDECL(mgmt) },				\
	{ MIBDECL(mib_2) },				\
	{ MIBDECL(system) },				\
	{ MIBDECL(sysDescr) },				\
	{ MIBDECL(sysOID) },				\
	{ MIBDECL(sysUpTime) },				\
	{ MIBDECL(sysContact) },			\
	{ MIBDECL(sysName) },				\
	{ MIBDECL(sysLocation) },			\
	{ MIBDECL(sysServices) },			\
	{ MIBDECL(sysORLastChange) },			\
	{ MIBDECL(sysORTable) },			\
	{ MIBDECL(sysOREntry) },			\
	{ MIBDECL(sysORIndex) },			\
	{ MIBDECL(sysORID) },				\
	{ MIBDECL(sysORDescr) },			\
	{ MIBDECL(sysORUpTime) },			\
	{ MIBDECL(transmission) },			\
	{ MIBDECL(snmp) },				\
	{ MIBDECL(snmpInPkts) },			\
	{ MIBDECL(snmpOutPkts) },			\
	{ MIBDECL(snmpInBadVersions) },			\
	{ MIBDECL(snmpInBadCommunityNames) },		\
	{ MIBDECL(snmpInBadCommunityUses) },		\
	{ MIBDECL(snmpInASNParseErrs) },		\
	{ MIBDECL(snmpInTooBigs) },			\
	{ MIBDECL(snmpInNoSuchNames) },			\
	{ MIBDECL(snmpInBadValues) },			\
	{ MIBDECL(snmpInReadOnlys) },			\
	{ MIBDECL(snmpInGenErrs) },			\
	{ MIBDECL(snmpInTotalReqVars) },		\
	{ MIBDECL(snmpInTotalSetVars) },		\
	{ MIBDECL(snmpInGetRequests) },			\
	{ MIBDECL(snmpInGetNexts) },			\
	{ MIBDECL(snmpInSetRequests) },			\
	{ MIBDECL(snmpInGetResponses) },		\
	{ MIBDECL(snmpInTraps) },			\
	{ MIBDECL(snmpOutTooBigs) },			\
	{ MIBDECL(snmpOutNoSuchNames) },		\
	{ MIBDECL(snmpOutBadValues) },			\
	{ MIBDECL(snmpOutGenErrs) },			\
	{ MIBDECL(snmpOutGetRequests) },		\
	{ MIBDECL(snmpOutGetNexts) },			\
	{ MIBDECL(snmpOutSetRequests) },		\
	{ MIBDECL(snmpOutGetResponses) },		\
	{ MIBDECL(snmpOutTraps) },			\
	{ MIBDECL(snmpEnableAuthenTraps) },		\
	{ MIBDECL(snmpSilentDrops) },			\
	{ MIBDECL(snmpProxyDrops) },			\
	{ MIBDECL(experimental) },			\
	{ MIBDECL(private) },				\
	{ MIBDECL(enterprises) },			\
	{ MIBDECL(security) },				\
	{ MIBDECL(snmpV2) },				\
	{ MIBDECL(snmpDomains) },			\
	{ MIBDECL(snmpProxies) },			\
	{ MIBDECL(snmpModules) },			\
	{ MIBDECL(snmpMIB) },				\
	{ MIBDECL(snmpMIBObjects) },			\
	{ MIBDECL(snmpTrap) },				\
	{ MIBDECL(snmpTrapOID) },			\
	{ MIBDECL(snmpTrapEnterprise) },		\
	{ MIBDECL(snmpTraps) },				\
	{ MIBDECL(coldStart) },				\
	{ MIBDECL(warmStart) },				\
	{ MIBDECL(linkDown) },				\
	{ MIBDECL(linkUp) },				\
	{ MIBDECL(authenticationFailure) },		\
	{ MIBDECL(egpNeighborLoss) },			\
							\
	{ MIBDECL(ifMIB) },				\
	{ MIBDECL(ifMIBObjects) },			\
	{ MIBDECL(ifXTable) },				\
	{ MIBDECL(ifXEntry) },				\
	{ MIBDECL(ifName) },				\
	{ MIBDECL(ifInMulticastPkts) },			\
	{ MIBDECL(ifInBroadcastPkts) },			\
	{ MIBDECL(ifOutMulticastPkts) },		\
	{ MIBDECL(ifOutBroadcastPkts) },		\
	{ MIBDECL(ifHCInOctets) },			\
	{ MIBDECL(ifHCInUcastPkts) },			\
	{ MIBDECL(ifHCInMulticastPkts) },		\
	{ MIBDECL(ifHCInBroadcastPkts) },		\
	{ MIBDECL(ifHCOutOctets) },			\
	{ MIBDECL(ifHCOutUcastPkts) },			\
	{ MIBDECL(ifHCOutMulticastPkts) },		\
	{ MIBDECL(ifHCOutBroadcastPkts) },		\
	{ MIBDECL(ifLinkUpDownTrapEnable) },		\
	{ MIBDECL(ifHighSpeed) },			\
	{ MIBDECL(ifPromiscuousMode) },			\
	{ MIBDECL(ifConnectorPresent) },		\
	{ MIBDECL(ifAlias) },				\
	{ MIBDECL(ifCounterDiscontinuityTime) },	\
	{ MIBDECL(ifStackTable) },			\
	{ MIBDECL(ifStackEntry) },			\
	{ MIBDECL(ifRcvAddressTable) },			\
	{ MIBDECL(ifRcvAddressEntry) },			\
	{ MIBDECL(ifRcvAddressStatus) },		\
	{ MIBDECL(ifRcvAddressType) },			\
	{ MIBDECL(ifStackLastChange) },			\
	{ MIBDECL(interfaces) },			\
	{ MIBDECL(ifNumber) },				\
	{ MIBDECL(ifTable) },				\
	{ MIBDECL(ifEntry) },				\
	{ MIBDECL(ifIndex) },				\
	{ MIBDECL(ifDescr) },				\
	{ MIBDECL(ifType) },				\
	{ MIBDECL(ifMtu) },				\
	{ MIBDECL(ifSpeed) },				\
	{ MIBDECL(ifPhysAddress) },			\
	{ MIBDECL(ifAdminStatus) },			\
	{ MIBDECL(ifOperStatus) },			\
	{ MIBDECL(ifLastChange) },			\
	{ MIBDECL(ifInOctets) },			\
	{ MIBDECL(ifInUcastPkts) },			\
	{ MIBDECL(ifInNUcastPkts) },			\
	{ MIBDECL(ifInDiscards) },			\
	{ MIBDECL(ifInErrors) },			\
	{ MIBDECL(ifInUnknownErrors) },			\
	{ MIBDECL(ifOutOctets) },			\
	{ MIBDECL(ifOutUcastPkts) },			\
	{ MIBDECL(ifOutNUcastPkts) },			\
	{ MIBDECL(ifOutDiscards) },			\
	{ MIBDECL(ifOutErrors) },			\
	{ MIBDECL(ifOutQLen) },				\
	{ MIBDECL(ifSpecific) },			\
							\
	{ MIBDECL(dot1dBridge) },			\
	{ MIBDECL(dot1dBase) },				\
	{ MIBDECL(dot1dBaseBridgeAddress) },		\
	{ MIBDECL(dot1dBaseNumPorts) },			\
	{ MIBDECL(dot1dBaseType) },			\
	{ MIBDECL(dot1dBasePortTable) },		\
	{ MIBDECL(dot1dBasePortEntry) },		\
	{ MIBDECL(dot1dBasePort) },			\
	{ MIBDECL(dot1dBasePortIfIndex) },		\
	{ MIBDECL(dot1dBasePortCircuit) },		\
	{ MIBDECL(dot1dBasePortDelayExceededDiscards) },\
	{ MIBDECL(dot1dBasePortMtuExceededDiscards) },	\
	{ MIBDECL(dot1dStp) },				\
	{ MIBDECL(dot1dSr) },				\
	{ MIBDECL(dot1dTp) },				\
	{ MIBDECL(dot1dStatic) },			\
							\
	{ MIBDECL(ibm) },				\
	{ MIBDECL(cmu) },				\
	{ MIBDECL(unix) },				\
	{ MIBDECL(ciscoSystems) },			\
	{ MIBDECL(hp) },				\
	{ MIBDECL(mit) },				\
	{ MIBDECL(nortelNetworks) },			\
	{ MIBDECL(sun) },				\
	{ MIBDECL(3com) },				\
	{ MIBDECL(synOptics) },				\
	{ MIBDECL(enterasys) },				\
	{ MIBDECL(sgi) },				\
	{ MIBDECL(apple) },				\
	{ MIBDECL(att) },				\
	{ MIBDECL(nokia) },				\
	{ MIBDECL(cern) },				\
	{ MIBDECL(fsc) },				\
	{ MIBDECL(compaq) },				\
	{ MIBDECL(dell) },				\
	{ MIBDECL(alteon) },				\
	{ MIBDECL(extremeNetworks) },			\
	{ MIBDECL(foundryNetworks) },			\
	{ MIBDECL(huawaiTechnology) },			\
	{ MIBDECL(ucDavis) },				\
	{ MIBDECL(checkPoint) },			\
	{ MIBDECL(juniper) },				\
	{ MIBDECL(force10Networks) },			\
	{ MIBDECL(alcatelLucent) },			\
	{ MIBDECL(snom) },				\
	{ MIBDECL(google) },				\
	{ MIBDECL(f5Networks) },			\
	{ MIBDECL(sFlow) },				\
	{ MIBDECL(microSystems) },			\
	{ MIBDECL(vantronix) },				\
	{ MIBDECL(openBSD) },				\
							\
	{ MIBDECL(sensorMIBObjects) },			\
	{ MIBDECL(sensors) },				\
	{ MIBDECL(sensorNumber) },			\
	{ MIBDECL(sensorTable) },			\
	{ MIBDECL(sensorEntry) },			\
	{ MIBDECL(sensorIndex) },			\
	{ MIBDECL(sensorDescr) },			\
	{ MIBDECL(sensorType) },			\
	{ MIBDECL(sensorDevice) },			\
	{ MIBDECL(sensorValue) },			\
	{ MIBDECL(sensorUnits) },			\
	{ MIBDECL(sensorStatus) },			\
							\
	{ MIBDECL(ipMIB) },				\
	{ MIBDECL(ipForwarding) },			\
	{ MIBDECL(ipDefaultTTL) },			\
	{ MIBDECL(ipInReceives) },			\
	{ MIBDECL(ipInHdrErrors) },			\
	{ MIBDECL(ipInAddrErrors) },			\
	{ MIBDECL(ipForwDatagrams) },			\
	{ MIBDECL(ipInUnknownProtos) },			\
	{ MIBDECL(ipInDiscards) },			\
	{ MIBDECL(ipInDelivers) },			\
	{ MIBDECL(ipOutRequests) },			\
	{ MIBDECL(ipOutDiscards) },			\
	{ MIBDECL(ipOutNoRoutes) },			\
	{ MIBDECL(ipReasmTimeout) },			\
	{ MIBDECL(ipReasmReqds) },			\
	{ MIBDECL(ipReasmOKs) },			\
	{ MIBDECL(ipReasmFails) },			\
	{ MIBDECL(ipFragOKs) },				\
	{ MIBDECL(ipFragFails) },			\
	{ MIBDECL(ipFragCreates) },			\
	{ MIBDECL(ipRoutingDiscards) },			\
	{ MIBDECL(ipAddrTable) },			\
	{ MIBDECL(ipAddrEntry) },			\
	{ MIBDECL(ipAdEntAddr) },			\
	{ MIBDECL(ipAdEntIfIndex) },			\
	{ MIBDECL(ipAdEntNetMask) },			\
	{ MIBDECL(ipAdEntBcastAddr) },			\
	{ MIBDECL(ipAdEntReasmMaxSize) },		\
	{ MIBDECL(ipNetToMediaTable) },			\
	{ MIBDECL(ipNetToMediaEntry) },			\
	{ MIBDECL(ipNetToMediaIfIndex) },		\
	{ MIBDECL(ipNetToMediaPhysAddress) },		\
	{ MIBDECL(ipNetToMediaNetAddress) },		\
	{ MIBDECL(ipNetToMediaType) },			\
	{ MIBDECL(ipNetToMediaType) },			\
							\
	{ MIBEND }					\
}

 void	 mib_init(void);

#endif /* _SNMPD_MIB_H */
