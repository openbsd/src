/* $OpenBSD: radiusconst.h,v 1.3 2010/07/02 21:20:57 yasuoka Exp $ */

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
/*
 * radiusconst.h :
 *   RADIUS constatnts
 */

#ifndef RADIUS_CONST
#define RADIUS_CONST


/* RADIUS codes: see RFC2865 */
#define RADIUS_CODE_ACCESS_REQUEST             1
#define RADIUS_CODE_ACCESS_ACCEPT              2
#define RADIUS_CODE_ACCESS_REJECT              3
#define RADIUS_CODE_ACCOUNTING_REQUEST         4
#define RADIUS_CODE_ACCOUNTING_RESPONSE        5
#define RADIUS_CODE_ACCESS_CHALLENGE          11
#define RADIUS_CODE_STATUS_SERVER             12
#define RADIUS_CODE_STATUS_CLIENT             13


/* RADIUS attributes: see RFC2865-2869 */
#define RADIUS_TYPE_USER_NAME                  1
#define RADIUS_TYPE_USER_PASSWORD              2
#define RADIUS_TYPE_CHAP_PASSWORD              3
#define RADIUS_TYPE_NAS_IP_ADDRESS             4
#define RADIUS_TYPE_NAS_PORT                   5
#define RADIUS_TYPE_SERVICE_TYPE               6
#define RADIUS_TYPE_FRAMED_PROTOCOL            7
#define RADIUS_TYPE_FRAMED_IP_ADDRESS          8
#define RADIUS_TYPE_FRAMED_IP_NETMASK          9
#define RADIUS_TYPE_FRAMED_ROUTING            10
#define RADIUS_TYPE_FILTER_ID                 11
#define RADIUS_TYPE_FRAMED_MTU                12
#define RADIUS_TYPE_FRAMED_COMPRESSION        13
#define RADIUS_TYPE_LOGIN_IP_HOST             14
#define RADIUS_TYPE_LOGIN_SERVICE             15
#define RADIUS_TYPE_LOGIN_TCP_PORT            16
/*      unassigned                            17 */
#define RADIUS_TYPE_REPLY_MESSAGE             18
#define RADIUS_TYPE_CALLBACK_NUMBER           19
#define RADIUS_TYPE_CALLBACK_ID               20
/*      unassigned                            21 */
#define RADIUS_TYPE_FRAMED_ROUTE              22
#define RADIUS_TYPE_FRAMED_IPX_NETWORK        23
#define RADIUS_TYPE_STATE                     24
#define RADIUS_TYPE_CLASS                     25
#define RADIUS_TYPE_VENDOR_SPECIFIC           26
#define RADIUS_TYPE_SESSION_TIMEOUT           27
#define RADIUS_TYPE_IDLE_TIMEOUT              28
#define RADIUS_TYPE_TERMINATION_ACTION        29
#define RADIUS_TYPE_CALLED_STATION_ID         30
#define RADIUS_TYPE_CALLING_STATION_ID        31
#define RADIUS_TYPE_NAS_IDENTIFIER            32
#define RADIUS_TYPE_PROXY_STATE               33
#define RADIUS_TYPE_LOGIN_LAT_SERVICE         34
#define RADIUS_TYPE_LOGIN_LAT_NODE            35
#define RADIUS_TYPE_LOGIN_LAT_GROUP           36
#define RADIUS_TYPE_FRAMED_APPLETALK_LINK     37
#define RADIUS_TYPE_FRAMED_APPLETALK_NETWORK  38
#define RADIUS_TYPE_FRAMED_APPLETALK_ZONE     39
#define RADIUS_TYPE_ACCT_STATUS_TYPE          40
#define RADIUS_TYPE_ACCT_DELAY_TIME           41
#define RADIUS_TYPE_ACCT_INPUT_OCTETS         42
#define RADIUS_TYPE_ACCT_OUTPUT_OCTETS        43
#define RADIUS_TYPE_ACCT_SESSION_ID           44
#define RADIUS_TYPE_ACCT_AUTHENTIC            45
#define RADIUS_TYPE_ACCT_SESSION_TIME         46
#define RADIUS_TYPE_ACCT_INPUT_PACKETS        47
#define RADIUS_TYPE_ACCT_OUTPUT_PACKETS       48
#define RADIUS_TYPE_ACCT_TERMINATE_CAUSE      49
#define RADIUS_TYPE_ACCT_MULTI_SESSION_ID     50
#define RADIUS_TYPE_ACCT_LINK_COUNT           51
#define RADIUS_TYPE_ACCT_INPUT_GIGAWORDS      52
#define RADIUS_TYPE_ACCT_OUTPUT_GIGAWORDS     53
/*      unassigned (for accounting)           54 */
#define RADIUS_TYPE_EVENT_TIMESTAMP           55
/*      unassigned (for accounting)           56 */
/*      unassigned (for accounting)           57 */
/*      unassigned (for accounting)           58 */
/*      unassigned (for accounting)           59 */
#define RADIUS_TYPE_CHAP_CHALLENGE            60
#define RADIUS_TYPE_NAS_PORT_TYPE             61
#define RADIUS_TYPE_PORT_LIMIT                62
#define RADIUS_TYPE_LOGIN_LAT_PORT            63
#define RADIUS_TYPE_TUNNEL_TYPE               64
#define RADIUS_TYPE_TUNNEL_MEDIUM_TYPE        65
#define RADIUS_TYPE_TUNNEL_CLIENT_ENDPOINT    66
#define RADIUS_TYPE_TUNNEL_SERVER_ENDPOINT    67
#define RADIUS_TYPE_ACCT_TUNNEL_CONNECTION    68
#define RADIUS_TYPE_TUNNEL_PASSWORD           69
#define RADIUS_TYPE_ARAP_PASSWORD             70
#define RADIUS_TYPE_ARAP_FEATURES             71
#define RADIUS_TYPE_ARAP_ZONE_ACCESS          72
#define RADIUS_TYPE_ARAP_SECURITY             73
#define RADIUS_TYPE_ARAP_SECURITY_DATA        74
#define RADIUS_TYPE_PASSWORD_RETRY            75
#define RADIUS_TYPE_PROMPT                    76
#define RADIUS_TYPE_CONNECT_INFO              77
#define RADIUS_TYPE_CONFIGURATION_TOKEN       78
#define RADIUS_TYPE_EAP_MESSAGE               79
#define RADIUS_TYPE_MESSAGE_AUTHENTICATOR     80
#define RADIUS_TYPE_TUNNEL_PRIVATE_GROUP_ID   81
#define RADIUS_TYPE_TUNNEL_ASSIGNMENT_ID      82
#define RADIUS_TYPE_TUNNEL_PREFERENCE         83
#define RADIUS_TYPE_ARAP_CHALLENGE_RESPONSE   84
#define RADIUS_TYPE_ACCT_INTERIM_INTERVAL     85
#define RADIUS_TYPE_ACCT_TUNNEL_PACKETS_LOST  86
#define RADIUS_TYPE_NAS_PORT_ID               87
#define RADIUS_TYPE_FRAMED_POOL               88
/*      unassigned                            89 */
#define RADIUS_TYPE_TUNNEL_CLIENT_AUTH_ID     90
#define RADIUS_TYPE_TUNNEL_SERVER_AUTH_ID     91

/* RFC 3162 "RADIUS and IPv6" */
#define RADIUS_TYPE_NAS_IPV6_ADDRESS          95
#define RADIUS_TYPE_FRAMED_INTERFACE_ID       96
#define RADIUS_TYPE_FRAMED_IPV6_PREFIX        97
#define RADIUS_TYPE_LOGIN_IPV6_HOST           98
#define RADIUS_TYPE_FRAMED_IPV6_ROUTE         99
#define RADIUS_TYPE_FRAMED_IPV6_POOL          100

/* RFC 2865 "5.6. Service-Type" */
#define	RADIUS_FRAMED_PROTOCOL_PPP		1

/* RFC 2865 "5.7. Framed-Protocol" */
#define RADIUS_SERVICE_TYPE_LOGIN             1
#define RADIUS_SERVICE_TYPE_FRAMED            2
#define RADIUS_SERVICE_TYPE_CB_LOGIN          3
#define RADIUS_SERVICE_TYPE_CB_FRAMED         4
#define RADIUS_SERVICE_TYPE_OUTBOUND          5
#define RADIUS_SERVICE_TYPE_ADMINISTRATIVE    6
#define RADIUS_SERVICE_TYPE_NAS_PROMPT        7
#define RADIUS_SERVICE_TYPE_AUTHENTICAT_ONLY  8
#define RADIUS_SERVICE_TYPE_CB_NAS_PROMPTi    9
#define RADIUS_SERVICE_TYPE_CALL_CHECK        10
#define RADIUS_SERVICE_TYPE_CB_ADMINISTRATIVE 11

/* Microsoft vendor specific attributes: see RFC2548*/
#define RADIUS_VENDOR_MICROSOFT              311
#define RADIUS_VTYPE_MS_CHAP_RESPONSE          1
#define RADIUS_VTYPE_MS_CHAP_ERROR             2
#define RADIUS_VTYPE_MS_CHAP_PW_1              3
#define RADIUS_VTYPE_MS_CHAP_PW_2              4
#define RADIUS_VTYPE_MS_CHAP_LM_ENC_PW         5
#define RADIUS_VTYPE_MS_CHAP_NT_ENC_PW         6
#define RADIUS_VTYPE_MPPE_ENCRYPTION_POLICY    7
#define RADIUS_VTYPE_MPPE_ENCRYPTION_TYPES     8
#define RADIUS_VTYPE_MS_RAS_VENDOR             9
#define RADIUS_VTYPE_MS_CHAP_CHALLENGE        11
#define RADIUS_VTYPE_MS_CHAP_MPPE_KEYS        12
#define RADIUS_VTYPE_MS_BAP_USAGE             13
#define RADIUS_VTYPE_MS_LINK_UTILIZATION_THRESHOLD 14
#define RADIUS_VTYPE_MS_LINK_DROP_TIME_LIMIT  15
#define RADIUS_VTYPE_MPPE_SEND_KEY            16
#define RADIUS_VTYPE_MPPE_RECV_KEY            17
#define RADIUS_VTYPE_MS_RAS_VERSION           18
#define RADIUS_VTYPE_MS_OLD_ARAP_PASSWORD     19
#define RADIUS_VTYPE_MS_NEW_ARAP_PASSWORD     20
#define RADIUS_VTYPE_MS_ARAP_PASSWORD_CHANGE_REASON 21
#define RADIUS_VTYPE_MS_FILTER                22
#define RADIUS_VTYPE_MS_ACCT_AUTH_TYPE        23
#define RADIUS_VTYPE_MS_ACCT_EAP_TYPE         24
#define RADIUS_VTYPE_MS_CHAP2_RESPONSE        25
#define RADIUS_VTYPE_MS_CHAP2_SUCCESS         26
#define RADIUS_VTYPE_MS_CHAP2_PW              27
#define RADIUS_VTYPE_MS_PRIMARY_DNS_SERVER    28
#define RADIUS_VTYPE_MS_SECONDARY_DNS_SERVER  29
#define RADIUS_VTYPE_MS_PRIMARY_NBNS_SERVER   30
#define RADIUS_VTYPE_MS_SECONDARY_NBNS_SERVER 31
/*      unassigned?                           32 */
#define RADIUS_VTYPE_MS_ARAP_CHALLENGE        33


/* IIJ vendor specific attributes */
#define RADIUS_VENDOR_IIJ                    770
#define RADIUS_VTYPE_IIJ_SID                   1


#endif
