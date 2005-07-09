/*	$OpenBSD: tables.c,v 1.7 2005/07/09 14:36:16 krw Exp $	*/

/* Tables of information... */

/*
 * Copyright (c) 1995, 1996 The Internet Software Consortium.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. Neither the name of The Internet Software Consortium nor the names
 *    of its contributors may be used to endorse or promote products derived
 *    from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE INTERNET SOFTWARE CONSORTIUM AND
 * CONTRIBUTORS ``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES,
 * INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED.  IN NO EVENT SHALL THE INTERNET SOFTWARE CONSORTIUM OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF
 * USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
 * OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT
 * OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 * This software has been written for the Internet Software Consortium
 * by Ted Lemon <mellon@fugue.com> in cooperation with Vixie
 * Enterprises.  To learn more about the Internet Software Consortium,
 * see ``http://www.vix.com/isc''.  To learn more about Vixie
 * Enterprises, see ``http://www.vix.com''.
 */

#include "dhcpd.h"

/*
 * DHCP Option names, formats and codes, from RFC1533.
 *
 * Format codes:
 *
 * e - end of data
 * I - IP address
 * l - 32-bit signed integer
 * L - 32-bit unsigned integer
 * s - 16-bit signed integer
 * S - 16-bit unsigned integer
 * b - 8-bit signed integer
 * B - 8-bit unsigned integer
 * t - ASCII text
 * f - flag (true or false)
 * A - array of whatever precedes (e.g., IA means array of IP addresses)
 */

struct universe dhcp_universe;
struct option dhcp_options[256] = {
	{ "pad", "",					0 },
	{ "subnet-mask", "I",				1 },
	{ "time-offset", "l",				2 },
	{ "routers", "IA",				3 },
	{ "time-servers", "IA",				4 },
	{ "ien116-name-servers", "IA",			5 },
	{ "domain-name-servers", "IA",			6 },
	{ "log-servers", "IA",				7 },
	{ "cookie-servers", "IA",			8 },
	{ "lpr-servers", "IA",				9 },
	{ "impress-servers", "IA",			10 },
	{ "resource-location-servers", "IA",		11 },
	{ "host-name", "X",				12 },
	{ "boot-size", "S",				13 },
	{ "merit-dump", "t",				14 },
	{ "domain-name", "t",				15 },
	{ "swap-server", "I",				16 },
	{ "root-path", "t",				17 },
	{ "extensions-path", "t",			18 },
	{ "ip-forwarding", "f",				19 },
	{ "non-local-source-routing", "f",		20 },
	{ "policy-filter", "IIA",			21 },
	{ "max-dgram-reassembly", "S",			22 },
	{ "default-ip-ttl", "B",			23 },
	{ "path-mtu-aging-timeout", "L",		24 },
	{ "path-mtu-plateau-table", "SA",		25 },
	{ "interface-mtu", "S",				26 },
	{ "all-subnets-local", "f",			27 },
	{ "broadcast-address", "I",			28 },
	{ "perform-mask-discovery", "f",		29 },
	{ "mask-supplier", "f",				30 },
	{ "router-discovery", "f",			31 },
	{ "router-solicitation-address", "I",		32 },
	{ "static-routes", "IIA",			33 },
	{ "trailer-encapsulation", "f",			34 },
	{ "arp-cache-timeout", "L",			35 },
	{ "ieee802-3-encapsulation", "f",		36 },
	{ "default-tcp-ttl", "B",			37 },
	{ "tcp-keepalive-interval", "L",		38 },
	{ "tcp-keepalive-garbage", "f",			39 },
	{ "nis-domain", "t",				40 },
	{ "nis-servers", "IA",				41 },
	{ "ntp-servers", "IA",				42 },
	{ "vendor-encapsulated-options", "X",		43 },
	{ "netbios-name-servers", "IA",			44 },
	{ "netbios-dd-server", "IA",			45 },
	{ "netbios-node-type", "B",			46 },
	{ "netbios-scope", "t",				47 },
	{ "font-servers", "IA",				48 },
	{ "x-display-manager", "IA",			49 },
	{ "dhcp-requested-address", "I",		50 },
	{ "dhcp-lease-time", "L",			51 },
	{ "dhcp-option-overload", "B",			52 },
	{ "dhcp-message-type", "B",			53 },
	{ "dhcp-server-identifier", "I",		54 },
	{ "dhcp-parameter-request-list", "BA",		55 },
	{ "dhcp-message", "t",				56 },
	{ "dhcp-max-message-size", "S",			57 },
	{ "dhcp-renewal-time", "L",			58 },
	{ "dhcp-rebinding-time", "L",			59 },
	{ "dhcp-class-identifier", "t",			60 },
	{ "dhcp-client-identifier", "X",		61 },
	{ "option-62", "X",				62 },
	{ "option-63", "X",				63 },
	{ "nisplus-domain", "t",			64 },
	{ "nisplus-servers", "IA",			65 },
	{ "tftp-server-name", "t",			66 },
	{ "bootfile-name", "t",				67 },
	{ "mobile-ip-home-agent", "IA",			68 },
	{ "smtp-server", "IA",				69 },
	{ "pop-server", "IA",				70 },
	{ "nntp-server", "IA",				71 },
	{ "www-server", "IA",				72 },
	{ "finger-server", "IA",			73 },
	{ "irc-server", "IA",				74 },
	{ "streettalk-server", "IA",			75 },
	{ "streettalk-directory-assistance-server", "IA", 76 },
	{ "user-class", "t",				77 },
	{ "option-78", "X",				78 },
	{ "option-79", "X",				79 },
	{ "option-80", "X",				80 },
	{ "option-81", "X",				81 },
	{ "option-82", "X",				82 },
	{ "option-83", "X",				83 },
	{ "option-84", "X",				84 },
	{ "nds-servers", "IA",				85 },
	{ "nds-tree-name", "X",				86 },
	{ "nds-context", "X",				87 },
	{ "option-88", "X",				88 },
	{ "option-89", "X",				89 },
	{ "option-90", "X",				90 },
	{ "option-91", "X",				91 },
	{ "option-92", "X",				92 },
	{ "option-93", "X",				93 },
	{ "option-94", "X",				94 },
	{ "option-95", "X",				95 },
	{ "option-96", "X",				96 },
	{ "option-97", "X",				97 },
	{ "option-98", "X",				98 },
	{ "option-99", "X",				99 },
	{ "option-100", "X",				100 },
	{ "option-101", "X",				101 },
	{ "option-102", "X",				102 },
	{ "option-103", "X",				103 },
	{ "option-104", "X",				104 },
	{ "option-105", "X",				105 },
	{ "option-106", "X",				106 },
	{ "option-107", "X",				107 },
	{ "option-108", "X",				108 },
	{ "option-109", "X",				109 },
	{ "option-110", "X",				110 },
	{ "option-111", "X",				111 },
	{ "option-112", "X",				112 },
	{ "option-113", "X",				113 },
	{ "option-114", "X",				114 },
	{ "option-115", "X",				115 },
	{ "option-116", "X",				116 },
	{ "option-117", "X",				117 },
	{ "option-118", "X",				118 },
	{ "option-119", "X",				119 },
	{ "option-120", "X",				120 },
	{ "option-121", "X",				121 },
	{ "option-122", "X",				122 },
	{ "option-123", "X",				123 },
	{ "option-124", "X",				124 },
	{ "option-125", "X",				125 },
	{ "option-126", "X",				126 },
	{ "option-127", "X",				127 },
	{ "option-128", "X",				128 },
	{ "option-129", "X",				129 },
	{ "option-130", "X",				130 },
	{ "option-131", "X",				131 },
	{ "option-132", "X",				132 },
	{ "option-133", "X",				133 },
	{ "option-134", "X",				134 },
	{ "option-135", "X",				135 },
	{ "option-136", "X",				136 },
	{ "option-137", "X",				137 },
	{ "option-138", "X",				138 },
	{ "option-139", "X",				139 },
	{ "option-140", "X",				140 },
	{ "option-141", "X",				141 },
	{ "option-142", "X",				142 },
	{ "option-143", "X",				143 },
	{ "option-144", "X",				144 },
	{ "option-145", "X",				145 },
	{ "option-146", "X",				146 },
	{ "option-147", "X",				147 },
	{ "option-148", "X",				148 },
	{ "option-149", "X",				149 },
	{ "option-150", "X",				150 },
	{ "option-151", "X",				151 },
	{ "option-152", "X",				152 },
	{ "option-153", "X",				153 },
	{ "option-154", "X",				154 },
	{ "option-155", "X",				155 },
	{ "option-156", "X",				156 },
	{ "option-157", "X",				157 },
	{ "option-158", "X",				158 },
	{ "option-159", "X",				159 },
	{ "option-160", "X",				160 },
	{ "option-161", "X",				161 },
	{ "option-162", "X",				162 },
	{ "option-163", "X",				163 },
	{ "option-164", "X",				164 },
	{ "option-165", "X",				165 },
	{ "option-166", "X",				166 },
	{ "option-167", "X",				167 },
	{ "option-168", "X",				168 },
	{ "option-169", "X",				169 },
	{ "option-170", "X",				170 },
	{ "option-171", "X",				171 },
	{ "option-172", "X",				172 },
	{ "option-173", "X",				173 },
	{ "option-174", "X",				174 },
	{ "option-175", "X",				175 },
	{ "option-176", "X",				176 },
	{ "option-177", "X",				177 },
	{ "option-178", "X",				178 },
	{ "option-179", "X",				179 },
	{ "option-180", "X",				180 },
	{ "option-181", "X",				181 },
	{ "option-182", "X",				182 },
	{ "option-183", "X",				183 },
	{ "option-184", "X",				184 },
	{ "option-185", "X",				185 },
	{ "option-186", "X",				186 },
	{ "option-187", "X",				187 },
	{ "option-188", "X",				188 },
	{ "option-189", "X",				189 },
	{ "option-190", "X",				190 },
	{ "option-191", "X",				191 },
	{ "option-192", "X",				192 },
	{ "option-193", "X",				193 },
	{ "option-194", "X",				194 },
	{ "option-195", "X",				195 },
	{ "option-196", "X",				196 },
	{ "option-197", "X",				197 },
	{ "option-198", "X",				198 },
	{ "option-199", "X",				199 },
	{ "option-200", "X",				200 },
	{ "option-201", "X",				201 },
	{ "option-202", "X",				202 },
	{ "option-203", "X",				203 },
	{ "option-204", "X",				204 },
	{ "option-205", "X",				205 },
	{ "option-206", "X",				206 },
	{ "option-207", "X",				207 },
	{ "option-208", "X",				208 },
	{ "option-209", "X",				209 },
	{ "option-210", "X",				210 },
	{ "option-211", "X",				211 },
	{ "option-212", "X",				212 },
	{ "option-213", "X",				213 },
	{ "option-214", "X",				214 },
	{ "option-215", "X",				215 },
	{ "option-216", "X",				216 },
	{ "option-217", "X",				217 },
	{ "option-218", "X",				218 },
	{ "option-219", "X",				219 },
	{ "option-220", "X",				220 },
	{ "option-221", "X",				221 },
	{ "option-222", "X",				222 },
	{ "option-223", "X",				223 },
	{ "option-224", "X",				224 },
	{ "option-225", "X",				225 },
	{ "option-226", "X",				226 },
	{ "option-227", "X",				227 },
	{ "option-228", "X",				228 },
	{ "option-229", "X",				229 },
	{ "option-230", "X",				230 },
	{ "option-231", "X",				231 },
	{ "option-232", "X",				232 },
	{ "option-233", "X",				233 },
	{ "option-234", "X",				234 },
	{ "option-235", "X",				235 },
	{ "option-236", "X",				236 },
	{ "option-237", "X",				237 },
	{ "option-238", "X",				238 },
	{ "option-239", "X",				239 },
	{ "option-240", "X",				240 },
	{ "option-241", "X",				241 },
	{ "option-242", "X",				242 },
	{ "option-243", "X",				243 },
	{ "option-244", "X",				244 },
	{ "option-245", "X",				245 },
	{ "option-246", "X",				246 },
	{ "option-247", "X",				247 },
	{ "option-248", "X",				248 },
	{ "option-249", "X",				249 },
	{ "option-250", "X",				250 },
	{ "option-251", "X",				251 },
	{ "option-252", "X",				252 },
	{ "option-253", "X",				253 },
	{ "option-254", "X",				254 },
	{ "option-end", "e",				255 },
};

/*
 * Default dhcp option priority list (this is ad hoc and should not be
 * mistaken for a carefully crafted and optimized list).
 */
unsigned char dhcp_option_default_priority_list[256] = {
	DHO_DHCP_MESSAGE_TYPE,
	DHO_DHCP_SERVER_IDENTIFIER,
	DHO_DHCP_LEASE_TIME,
	DHO_DHCP_MESSAGE,
	DHO_DHCP_REQUESTED_ADDRESS,
	DHO_DHCP_OPTION_OVERLOAD,
	DHO_DHCP_MAX_MESSAGE_SIZE,
	DHO_DHCP_RENEWAL_TIME,
	DHO_DHCP_REBINDING_TIME,
	DHO_DHCP_CLASS_IDENTIFIER,
	DHO_DHCP_CLIENT_IDENTIFIER,
	DHO_SUBNET_MASK,
	DHO_TIME_OFFSET,
	DHO_ROUTERS,
	DHO_TIME_SERVERS,
	DHO_NAME_SERVERS,
	DHO_DOMAIN_NAME_SERVERS,
	DHO_HOST_NAME,
	DHO_LOG_SERVERS,
	DHO_COOKIE_SERVERS,
	DHO_LPR_SERVERS,
	DHO_IMPRESS_SERVERS,
	DHO_RESOURCE_LOCATION_SERVERS,
	DHO_HOST_NAME,
	DHO_BOOT_SIZE,
	DHO_MERIT_DUMP,
	DHO_DOMAIN_NAME,
	DHO_SWAP_SERVER,
	DHO_ROOT_PATH,
	DHO_EXTENSIONS_PATH,
	DHO_IP_FORWARDING,
	DHO_NON_LOCAL_SOURCE_ROUTING,
	DHO_POLICY_FILTER,
	DHO_MAX_DGRAM_REASSEMBLY,
	DHO_DEFAULT_IP_TTL,
	DHO_PATH_MTU_AGING_TIMEOUT,
	DHO_PATH_MTU_PLATEAU_TABLE,
	DHO_INTERFACE_MTU,
	DHO_ALL_SUBNETS_LOCAL,
	DHO_BROADCAST_ADDRESS,
	DHO_PERFORM_MASK_DISCOVERY,
	DHO_MASK_SUPPLIER,
	DHO_ROUTER_DISCOVERY,
	DHO_ROUTER_SOLICITATION_ADDRESS,
	DHO_STATIC_ROUTES,
	DHO_TRAILER_ENCAPSULATION,
	DHO_ARP_CACHE_TIMEOUT,
	DHO_IEEE802_3_ENCAPSULATION,
	DHO_DEFAULT_TCP_TTL,
	DHO_TCP_KEEPALIVE_INTERVAL,
	DHO_TCP_KEEPALIVE_GARBAGE,
	DHO_NIS_DOMAIN,
	DHO_NIS_SERVERS,
	DHO_NTP_SERVERS,
	DHO_VENDOR_ENCAPSULATED_OPTIONS,
	DHO_NETBIOS_NAME_SERVERS,
	DHO_NETBIOS_DD_SERVER,
	DHO_NETBIOS_NODE_TYPE,
	DHO_NETBIOS_SCOPE,
	DHO_FONT_SERVERS,
	DHO_X_DISPLAY_MANAGER,
	DHO_DHCP_PARAMETER_REQUEST_LIST,

	/* Presently-undefined options... */
	62, 63, 64, 65, 66, 67, 68, 69, 70, 71, 72, 73, 74, 75, 76,
	78, 79, 80, 81, 82, 83, 84, 85, 86, 87, 88, 89, 90, 91, 92,
	93, 94, 95, 96, 97, 98, 99, 100, 101, 102, 103, 104, 105, 106,
	107, 108, 109, 110, 111, 112, 113, 114, 115, 116, 117, 118,
	119, 120, 121, 122, 123, 124, 125, 126, 127, 128, 129, 130,
	131, 132, 133, 134, 135, 136, 137, 138, 139, 140, 141, 142,
	143, 144, 145, 146, 147, 148, 149, 150, 151, 152, 153, 154,
	155, 156, 157, 158, 159, 160, 161, 162, 163, 164, 165, 166,
	167, 168, 169, 170, 171, 172, 173, 174, 175, 176, 177, 178,
	179, 180, 181, 182, 183, 184, 185, 186, 187, 188, 189, 190,
	191, 192, 193, 194, 195, 196, 197, 198, 199, 200, 201, 202,
	203, 204, 205, 206, 207, 208, 209, 210, 211, 212, 213, 214,
	215, 216, 217, 218, 219, 220, 221, 222, 223, 224, 225, 226,
	227, 228, 229, 230, 231, 232, 233, 234, 235, 236, 237, 238,
	239, 240, 241, 242, 243, 244, 245, 246, 247, 248, 249, 250,
	251, 252, 253, 254,
};

void
initialize_dhcp_universe(void)
{
	int i;

	dhcp_universe.hash = new_hash();
	if (!dhcp_universe.hash)
		error("Can't allocate dhcp option hash table.");
	for (i = 0; i < 256; i++) {
		dhcp_universe.options[i] = &dhcp_options[i];
		add_hash(dhcp_universe.hash,
		    (unsigned char *)dhcp_options[i].name, 0,
		    (unsigned char *)&dhcp_options[i]);
	}
}
