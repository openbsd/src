/*	$OpenBSD: options.c,v 1.113 2018/07/22 21:32:04 krw Exp $	*/

/* DHCP options parsing and reassembly. */

/*
 * Copyright (c) 1995, 1996, 1997, 1998 The Internet Software Consortium.
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

#include <sys/queue.h>
#include <sys/socket.h>

#include <arpa/inet.h>

#include <net/if.h>

#include <netinet/in.h>
#include <netinet/if_ether.h>

#include <ctype.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <vis.h>

#include "dhcp.h"
#include "dhcpd.h"
#include "log.h"

int parse_option_buffer(struct option_data *, unsigned char *, int);
int expand_search_domain_name(unsigned char *, size_t, int *, unsigned char *);

/*
 * DHCP Option names, formats and codes, from RFC1533.
 *
 * Format codes:
 *
 * e - end of data
 * I - IP address
 * l - 32-bit signed integer
 * L - 32-bit unsigned integer
 * S - 16-bit unsigned integer
 * B - 8-bit unsigned integer
 * t - ASCII text
 * f - flag (true or false)
 * A - array of whatever precedes (e.g., IA means array of IP addresses)
 * C - CIDR description
 */

static const struct {
	char *name;
	char *format;
} dhcp_options[DHO_COUNT] = {
	/*   0 */ { "pad", "" },
	/*   1 */ { "subnet-mask", "I" },
	/*   2 */ { "time-offset", "l" },
	/*   3 */ { "routers", "IA" },
	/*   4 */ { "time-servers", "IA" },
	/*   5 */ { "ien116-name-servers", "IA" },
	/*   6 */ { "domain-name-servers", "IA" },
	/*   7 */ { "log-servers", "IA" },
	/*   8 */ { "cookie-servers", "IA" },
	/*   9 */ { "lpr-servers", "IA" },
	/*  10 */ { "impress-servers", "IA" },
	/*  11 */ { "resource-location-servers", "IA" },
	/*  12 */ { "host-name", "t" },
	/*  13 */ { "boot-size", "S" },
	/*  14 */ { "merit-dump", "t" },
	/*  15 */ { "domain-name", "t" },
	/*  16 */ { "swap-server", "I" },
	/*  17 */ { "root-path", "t" },
	/*  18 */ { "extensions-path", "t" },
	/*  19 */ { "ip-forwarding", "f" },
	/*  20 */ { "non-local-source-routing", "f" },
	/*  21 */ { "policy-filter", "IIA" },
	/*  22 */ { "max-dgram-reassembly", "S" },
	/*  23 */ { "default-ip-ttl", "B" },
	/*  24 */ { "path-mtu-aging-timeout", "L" },
	/*  25 */ { "path-mtu-plateau-table", "SA" },
	/*  26 */ { "interface-mtu", "S" },
	/*  27 */ { "all-subnets-local", "f" },
	/*  28 */ { "broadcast-address", "I" },
	/*  29 */ { "perform-mask-discovery", "f" },
	/*  30 */ { "mask-supplier", "f" },
	/*  31 */ { "router-discovery", "f" },
	/*  32 */ { "router-solicitation-address", "I" },
	/*  33 */ { "static-routes", "IIA" },
	/*  34 */ { "trailer-encapsulation", "f" },
	/*  35 */ { "arp-cache-timeout", "L" },
	/*  36 */ { "ieee802-3-encapsulation", "f" },
	/*  37 */ { "default-tcp-ttl", "B" },
	/*  38 */ { "tcp-keepalive-interval", "L" },
	/*  39 */ { "tcp-keepalive-garbage", "f" },
	/*  40 */ { "nis-domain", "t" },
	/*  41 */ { "nis-servers", "IA" },
	/*  42 */ { "ntp-servers", "IA" },
	/*  43 */ { "vendor-encapsulated-options", "X" },
	/*  44 */ { "netbios-name-servers", "IA" },
	/*  45 */ { "netbios-dd-server", "IA" },
	/*  46 */ { "netbios-node-type", "B" },
	/*  47 */ { "netbios-scope", "t" },
	/*  48 */ { "font-servers", "IA" },
	/*  49 */ { "x-display-manager", "IA" },
	/*  50 */ { "dhcp-requested-address", "I" },
	/*  51 */ { "dhcp-lease-time", "L" },
	/*  52 */ { "dhcp-option-overload", "B" },
	/*  53 */ { "dhcp-message-type", "B" },
	/*  54 */ { "dhcp-server-identifier", "I" },
	/*  55 */ { "dhcp-parameter-request-list", "BA" },
	/*  56 */ { "dhcp-message", "t" },
	/*  57 */ { "dhcp-max-message-size", "S" },
	/*  58 */ { "dhcp-renewal-time", "L" },
	/*  59 */ { "dhcp-rebinding-time", "L" },
	/*  60 */ { "dhcp-class-identifier", "t" },
	/*  61 */ { "dhcp-client-identifier", "X" },
	/*  62 */ { NULL, NULL },
	/*  63 */ { NULL, NULL },
	/*  64 */ { "nisplus-domain", "t" },
	/*  65 */ { "nisplus-servers", "IA" },
	/*  66 */ { "tftp-server-name", "t" },
	/*  67 */ { "bootfile-name", "t" },
	/*  68 */ { "mobile-ip-home-agent", "IA" },
	/*  69 */ { "smtp-server", "IA" },
	/*  70 */ { "pop-server", "IA" },
	/*  71 */ { "nntp-server", "IA" },
	/*  72 */ { "www-server", "IA" },
	/*  73 */ { "finger-server", "IA" },
	/*  74 */ { "irc-server", "IA" },
	/*  75 */ { "streettalk-server", "IA" },
	/*  76 */ { "streettalk-directory-assistance-server", "IA" },
	/*  77 */ { "user-class", "t" },
	/*  78 */ { NULL, NULL },
	/*  79 */ { NULL, NULL },
	/*  80 */ { NULL, NULL },
	/*  81 */ { NULL, NULL },
	/*  82 */ { "relay-agent-information", "X" },
	/*  83 */ { NULL, NULL },
	/*  84 */ { NULL, NULL },
	/*  85 */ { "nds-servers", "IA" },
	/*  86 */ { "nds-tree-name", "X" },
	/*  87 */ { "nds-context", "X" },
	/*  88 */ { NULL, NULL },
	/*  89 */ { NULL, NULL },
	/*  90 */ { NULL, NULL },
	/*  91 */ { NULL, NULL },
	/*  92 */ { NULL, NULL },
	/*  93 */ { NULL, NULL },
	/*  94 */ { NULL, NULL },
	/*  95 */ { NULL, NULL },
	/*  96 */ { NULL, NULL },
	/*  97 */ { NULL, NULL },
	/*  98 */ { NULL, NULL },
	/*  99 */ { NULL, NULL },
	/* 100 */ { NULL, NULL },
	/* 101 */ { NULL, NULL },
	/* 102 */ { NULL, NULL },
	/* 103 */ { NULL, NULL },
	/* 104 */ { NULL, NULL },
	/* 105 */ { NULL, NULL },
	/* 106 */ { NULL, NULL },
	/* 107 */ { NULL, NULL },
	/* 108 */ { NULL, NULL },
	/* 109 */ { NULL, NULL },
	/* 110 */ { NULL, NULL },
	/* 111 */ { NULL, NULL },
	/* 112 */ { NULL, NULL },
	/* 113 */ { NULL, NULL },
	/* 114 */ { NULL, NULL },
	/* 115 */ { NULL, NULL },
	/* 116 */ { NULL, NULL },
	/* 117 */ { NULL, NULL },
	/* 118 */ { NULL, NULL },
	/* 119 */ { "domain-search", "X" },
	/* 120 */ { NULL, NULL },
	/* 121 */ { "classless-static-routes", "CIA" },
	/* 122 */ { NULL, NULL },
	/* 123 */ { NULL, NULL },
	/* 124 */ { NULL, NULL },
	/* 125 */ { NULL, NULL },
	/* 126 */ { NULL, NULL },
	/* 127 */ { NULL, NULL },
	/* 128 */ { NULL, NULL },
	/* 129 */ { NULL, NULL },
	/* 130 */ { NULL, NULL },
	/* 131 */ { NULL, NULL },
	/* 132 */ { NULL, NULL },
	/* 133 */ { NULL, NULL },
	/* 134 */ { NULL, NULL },
	/* 135 */ { NULL, NULL },
	/* 136 */ { NULL, NULL },
	/* 137 */ { NULL, NULL },
	/* 138 */ { NULL, NULL },
	/* 139 */ { NULL, NULL },
	/* 140 */ { NULL, NULL },
	/* 141 */ { NULL, NULL },
	/* 142 */ { NULL, NULL },
	/* 143 */ { NULL, NULL },
	/* 144 */ { "tftp-config-file", "t" },
	/* 145 */ { NULL, NULL },
	/* 146 */ { NULL, NULL },
	/* 147 */ { NULL, NULL },
	/* 148 */ { NULL, NULL },
	/* 149 */ { NULL, NULL },
	/* 150 */ { "voip-configuration-server", "IA" },
	/* 151 */ { NULL, NULL },
	/* 152 */ { NULL, NULL },
	/* 153 */ { NULL, NULL },
	/* 154 */ { NULL, NULL },
	/* 155 */ { NULL, NULL },
	/* 156 */ { NULL, NULL },
	/* 157 */ { NULL, NULL },
	/* 158 */ { NULL, NULL },
	/* 159 */ { NULL, NULL },
	/* 160 */ { NULL, NULL },
	/* 161 */ { NULL, NULL },
	/* 162 */ { NULL, NULL },
	/* 163 */ { NULL, NULL },
	/* 164 */ { NULL, NULL },
	/* 165 */ { NULL, NULL },
	/* 166 */ { NULL, NULL },
	/* 167 */ { NULL, NULL },
	/* 168 */ { NULL, NULL },
	/* 169 */ { NULL, NULL },
	/* 170 */ { NULL, NULL },
	/* 171 */ { NULL, NULL },
	/* 172 */ { NULL, NULL },
	/* 173 */ { NULL, NULL },
	/* 174 */ { NULL, NULL },
	/* 175 */ { NULL, NULL },
	/* 176 */ { NULL, NULL },
	/* 177 */ { NULL, NULL },
	/* 178 */ { NULL, NULL },
	/* 179 */ { NULL, NULL },
	/* 180 */ { NULL, NULL },
	/* 181 */ { NULL, NULL },
	/* 182 */ { NULL, NULL },
	/* 183 */ { NULL, NULL },
	/* 184 */ { NULL, NULL },
	/* 185 */ { NULL, NULL },
	/* 186 */ { NULL, NULL },
	/* 187 */ { NULL, NULL },
	/* 188 */ { NULL, NULL },
	/* 189 */ { NULL, NULL },
	/* 190 */ { NULL, NULL },
	/* 191 */ { NULL, NULL },
	/* 192 */ { NULL, NULL },
	/* 193 */ { NULL, NULL },
	/* 194 */ { NULL, NULL },
	/* 195 */ { NULL, NULL },
	/* 196 */ { NULL, NULL },
	/* 197 */ { NULL, NULL },
	/* 198 */ { NULL, NULL },
	/* 199 */ { NULL, NULL },
	/* 200 */ { NULL, NULL },
	/* 201 */ { NULL, NULL },
	/* 202 */ { NULL, NULL },
	/* 203 */ { NULL, NULL },
	/* 204 */ { NULL, NULL },
	/* 205 */ { NULL, NULL },
	/* 206 */ { NULL, NULL },
	/* 207 */ { NULL, NULL },
	/* 208 */ { NULL, NULL },
	/* 209 */ { NULL, NULL },
	/* 210 */ { NULL, NULL },
	/* 211 */ { NULL, NULL },
	/* 212 */ { NULL, NULL },
	/* 213 */ { NULL, NULL },
	/* 214 */ { NULL, NULL },
	/* 215 */ { NULL, NULL },
	/* 216 */ { NULL, NULL },
	/* 217 */ { NULL, NULL },
	/* 218 */ { NULL, NULL },
	/* 219 */ { NULL, NULL },
	/* 220 */ { NULL, NULL },
	/* 221 */ { NULL, NULL },
	/* 222 */ { NULL, NULL },
	/* 223 */ { NULL, NULL },
	/* 224 */ { NULL, NULL },
	/* 225 */ { NULL, NULL },
	/* 226 */ { NULL, NULL },
	/* 227 */ { NULL, NULL },
	/* 228 */ { NULL, NULL },
	/* 229 */ { NULL, NULL },
	/* 230 */ { NULL, NULL },
	/* 231 */ { NULL, NULL },
	/* 232 */ { NULL, NULL },
	/* 233 */ { NULL, NULL },
	/* 234 */ { NULL, NULL },
	/* 235 */ { NULL, NULL },
	/* 236 */ { NULL, NULL },
	/* 237 */ { NULL, NULL },
	/* 238 */ { NULL, NULL },
	/* 239 */ { NULL, NULL },
	/* 240 */ { NULL, NULL },
	/* 241 */ { NULL, NULL },
	/* 242 */ { NULL, NULL },
	/* 243 */ { NULL, NULL },
	/* 244 */ { NULL, NULL },
	/* 245 */ { NULL, NULL },
	/* 246 */ { NULL, NULL },
	/* 247 */ { NULL, NULL },
	/* 248 */ { NULL, NULL },
	/* 249 */ { "classless-ms-static-routes", "CIA" },
	/* 250 */ { NULL, NULL },
	/* 251 */ { NULL, NULL },
	/* 252 */ { "autoproxy-script", "t" },
	/* 253 */ { NULL, NULL },
	/* 254 */ { NULL, NULL },
	/* 255 */ { "option-end", "e" },
};

char *
code_to_name(int code)
{
	static char	 unknown[11];	/* "option-NNN" */
	int		 ret;

	if (code < 0 || code >= DHO_COUNT)
		return "";

	if (dhcp_options[code].name != NULL)
		return dhcp_options[code].name;

	ret = snprintf(unknown, sizeof(unknown), "option-%d", code);
	if (ret == -1 || ret >= (int)sizeof(unknown))
		return "";

	return unknown;
}

int
name_to_code(char *name)
{
	char	unknown[11];	/* "option-NNN" */
	int	code, ret;

	for (code = 1; code < DHO_END; code++) {
		if (dhcp_options[code].name == NULL) {
			ret = snprintf(unknown, sizeof(unknown), "option-%d",
			    code);
			if (ret == -1 || ret >= (int)sizeof(unknown))
				return DHO_END;
			if (strcasecmp(unknown, name) == 0)
				return code;
		} else if (strcasecmp(dhcp_options[code].name, name) == 0) {
			return code;
		}
	}

	return DHO_END;
}

char *
code_to_format(int code)
{
	if (code < 0 || code >= DHO_COUNT)
		return "";

	if (dhcp_options[code].format == NULL)
		return "X";

	return dhcp_options[code].format;
}

/*
 * Parse options out of the specified buffer, storing addresses of
 * option values in options. Return 0 if errors, 1 if not.
 */
int
parse_option_buffer(struct option_data *options, unsigned char *buffer,
    int length)
{
	unsigned char	*s, *t, *end;
	char		*name, *fmt;
	int		 code, len, newlen;

	s = buffer;
	end = s + length;
	while (s < end) {
		code = s[0];

		/* End options terminate processing. */
		if (code == DHO_END)
			break;

		/* Pad options don't have a length - just skip them. */
		if (code == DHO_PAD) {
			s++;
			continue;
		}

		name = code_to_name(code);
		fmt = code_to_format(code);

		/*
		 * All options other than DHO_PAD and DHO_END have a one-byte
		 * length field. It could be 0! Make sure that the length byte
		 * is present, and all the data is available.
		 */
		if (s + 1 < end) {
			len = s[1];
			if (s + 1 + len < end) {
				; /* option data is all there. */
			} else {
				log_warnx("%s: option %s (%d) larger than "
				    "buffer", log_procname, name, len);
				return 0;
			}
		} else {
			log_warnx("%s: option %s has no length field",
			    log_procname, name);
			return 0;
		}

		/*
		 * Strip trailing NULs from ascii ('t') options. RFC 2132
		 * says "Options containing NVT ASCII data SHOULD NOT include
		 * a trailing NULL; however, the receiver of such options
		 * MUST be prepared to delete trailing nulls if they exist."
		 */
		if (fmt[0] == 't') {
			while (len > 0 && s[len + 1] == '\0')
				len--;
		}

		/*
		 * Concatenate new data + NUL to existing option data.
		 *
		 * Note that the NUL is *not* counted in the len field!
		 */
		newlen = options[code].len + len;
		if ((t = realloc(options[code].data, newlen + 1)) == NULL)
			fatal("option %s", name);

		memcpy(t + options[code].len, &s[2], len);
		t[newlen] = 0;

		options[code].len = newlen;
		options[code].data = t;

		s += s[1] + 2;
	}

	return 1;
}

/*
 * Pack as many options as fit in buflen bytes of buf. Return the
 * offset of the start of the last option copied. A caller can check
 * to see if it's DHO_END to decide if all the options were copied.
 */
int
pack_options(unsigned char *buf, int buflen, struct option_data *options)
{
	int	 ix, incr, length, bufix, code, lastopt = -1;

	memset(buf, 0, buflen);

	memcpy(buf, DHCP_OPTIONS_COOKIE, 4);
	if (options[DHO_DHCP_MESSAGE_TYPE].data != NULL) {
		memcpy(&buf[4], DHCP_OPTIONS_MESSAGE_TYPE, 3);
		buf[6] = options[DHO_DHCP_MESSAGE_TYPE].data[0];
		bufix = 7;
	} else
		bufix = 4;

	for (code = DHO_SUBNET_MASK; code < DHO_END; code++) {
		if (options[code].data == NULL ||
		    code == DHO_DHCP_MESSAGE_TYPE)
			continue;

		length = options[code].len;
		if (bufix + length + 2*((length+254)/255) >= buflen)
			return lastopt;

		lastopt = bufix;
		ix = 0;

		while (length) {
			incr = length > 255 ? 255 : length;

			buf[bufix++] = code;
			buf[bufix++] = incr;
			memcpy(buf + bufix, options[code].data + ix, incr);

			length -= incr;
			ix += incr;
			bufix += incr;
		}
	}

	if (bufix < buflen) {
		buf[bufix] = DHO_END;
		lastopt = bufix;
	}

	return lastopt;
}

/*
 * Use vis() to encode characters of src and append encoded characters onto
 * dst. Also encode ", ', $, ` and \, to ensure resulting strings can be
 * represented as '"' delimited strings and safely passed to scripts. Surround
 * result with double quotes if emit_punct is true.
 */
char *
pretty_print_string(unsigned char *src, size_t srclen, int emit_punct)
{
	static char	 string[8196];
	char		 visbuf[5];
	unsigned char	*origsrc = src;
	size_t		 rslt = 0;

	memset(string, 0, sizeof(string));

	if (emit_punct != 0)
		rslt = strlcat(string, "\"", sizeof(string));

	for (; src < origsrc + srclen; src++) {
		if (*src && strchr("\"'$`\\", *src))
			vis(visbuf, *src, VIS_ALL | VIS_OCTAL, *src+1);
		else
			vis(visbuf, *src, VIS_OCTAL, *src+1);
		rslt = strlcat(string, visbuf, sizeof(string));
	}

	if (emit_punct != 0)
		rslt = strlcat(string, "\"", sizeof(string));

	if (rslt >= sizeof(string))
		return NULL;

	return string;
}

/*
 * Must special case *_CLASSLESS_* route options due to the variable size
 * of the CIDR element in its CIA format.
 */
void
pretty_print_classless_routes(unsigned char *src, size_t srclen,
    unsigned char *buf, size_t buflen)
{
	char		 bitsbuf[5];	/* to hold "/nn " */
	struct in_addr	 dest, netmask, gateway;
	unsigned int	 bits, i, len;
	uint32_t	 m;
	int		 rslt;

	i = 0;
	while (i < srclen) {
		len = extract_classless_route(&src[i], srclen - i,
		    &dest.s_addr, &netmask.s_addr, &gateway.s_addr);
		if (len == 0)
			goto bad;
		i += len;

		m = ntohl(netmask.s_addr);
		bits = 32;
		while ((bits > 0) && ((m & 1) == 0)) {
			m >>= 1;
			bits--;
		}

		rslt = snprintf(bitsbuf, sizeof(bitsbuf), "/%d ", bits);
		if (rslt == -1 || (unsigned int)rslt >= sizeof(bitsbuf))
			goto bad;

		if (strlen(buf) > 0)
			strlcat(buf, ", ", buflen);
		strlcat(buf, inet_ntoa(dest), buflen);
		strlcat(buf, bitsbuf, buflen);
		if (strlcat(buf, inet_ntoa(gateway), buflen) >= buflen)
			goto bad;
	}

	return;

bad:
	memset(buf, 0, buflen);
}

int
expand_search_domain_name(unsigned char *src, size_t srclen, int *offset,
    unsigned char *domain_search)
{
	char		*cursor;
	unsigned int	 i;
	int		 domain_name_len, label_len, pointer, pointed_len;

	cursor = domain_search + strlen(domain_search);
	domain_name_len = 0;

	i = *offset;
	while (i <= srclen) {
		label_len = src[i];
		if (label_len == 0) {
			/*
			 * A zero-length label marks the end of this
			 * domain name.
			 */
			*offset = i + 1;
			return domain_name_len;
		} else if ((label_len & 0xC0) != 0) {
			/* This is a pointer to another list of labels. */
			if (i + 1 >= srclen) {
				/* The pointer is truncated. */
				log_warnx("%s: truncated pointer in DHCP "
				    "Domain Search option", log_procname);
				return -1;
			}

			pointer = ((label_len & ~(0xC0)) << 8) + src[i + 1];
			if (pointer >= *offset) {
				/*
				 * The pointer must indicates a prior
				 * occurance.
				 */
				log_warnx("%s: invalid forward pointer in DHCP "
				    "Domain Search option compression",
				    log_procname);
				return -1;
			}

			pointed_len = expand_search_domain_name(src, srclen,
			    &pointer, domain_search);
			domain_name_len += pointed_len;

			*offset = i + 2;
			return domain_name_len;
		}
		if (i + label_len + 1 > srclen) {
			log_warnx("%s: truncated label in DHCP Domain Search "
			    "option", log_procname);
			return -1;
		}
		/*
		 * Update the domain name length with the length of the
		 * current label, plus a trailing dot ('.').
		 */
		domain_name_len += label_len + 1;

		if (strlen(domain_search) + domain_name_len >=
		    DHCP_DOMAIN_SEARCH_LEN) {
			log_warnx("%s: domain search list too long",
			    log_procname);
			return -1;
		}

		/* Copy the label found. */
		memcpy(cursor, src + i + 1, label_len);
		cursor[label_len] = '.';

		/* Move cursor. */
		i += label_len + 1;
		cursor += label_len + 1;
	}

	log_warnx("%s: truncated DHCP Domain Search option", log_procname);

	return -1;
}

/*
 * Must special case DHO_DOMAIN_SEARCH because it is encoded as described
 * in RFC 1035 section 4.1.4.
 */
char *
pretty_print_domain_search(unsigned char *src, size_t srclen)
{
	static char	 domain_search[DHCP_DOMAIN_SEARCH_LEN];
	unsigned char	*cursor;
	unsigned int	 offset;
	int		 len, expanded_len, domains;

	memset(domain_search, 0, sizeof(domain_search));

	/* Compute expanded length. */
	expanded_len = len = 0;
	domains = 0;
	offset = 0;
	while (offset < srclen) {
		cursor = domain_search + strlen(domain_search);
		if (domain_search[0] != '\0') {
			*cursor = ' ';
			expanded_len++;
		}
		len = expand_search_domain_name(src, srclen, &offset,
		    domain_search);
		if (len == -1)
			return NULL;
		domains++;
		expanded_len += len;
		if (domains > DHCP_DOMAIN_SEARCH_CNT)
			return NULL;
	}

	return domain_search;
}

/*
 * Format the specified option so that a human can easily read it.
 */
char *
pretty_print_option(unsigned int code, struct option_data *option,
    int emit_punct)
{
	static char	 optbuf[8192]; /* XXX */
	char		 fmtbuf[32];
	struct in_addr	 foo;
	unsigned char	*data = option->data;
	unsigned char	*dp = data;
	char		*op = optbuf, *buf, *name, *fmt;
	int		 hunksize = 0, numhunk = -1, numelem = 0;
	int		 i, j, k, opleft = sizeof(optbuf);
	int		 len = option->len;
	int		 opcount = 0;
	int32_t		 int32val;
	uint32_t	 uint32val;
	uint16_t	 uint16val;
	char		 comma;

	memset(optbuf, 0, sizeof(optbuf));

	/* Code should be between 0 and 255. */
	if (code > 255) {
		log_warnx("%s: pretty_print_option: bad code %d", log_procname,
		    code);
		goto done;
	}

	if (emit_punct != 0)
		comma = ',';
	else
		comma = ' ';

	/* Handle the princess class options with weirdo formats. */
	switch (code) {
	case DHO_CLASSLESS_STATIC_ROUTES:
	case DHO_CLASSLESS_MS_STATIC_ROUTES:
		pretty_print_classless_routes(dp, len, optbuf,
		    sizeof(optbuf));
		goto done;
	default:
		break;
	}

	name = code_to_name(code);
	fmt = code_to_format(code);

	/* Figure out the size of the data. */
	for (i = 0; fmt[i]; i++) {
		if (numhunk == 0) {
			log_warnx("%s: %s: excess information in format "
			    "string: %s", log_procname, name, &fmt[i]);
			goto done;
		}
		numelem++;
		fmtbuf[i] = fmt[i];
		switch (fmt[i]) {
		case 'A':
			--numelem;
			fmtbuf[i] = 0;
			numhunk = 0;
			if (hunksize == 0) {
				log_warnx("%s: %s: no size indicator before A"
				    " in format string: %s", log_procname,
				    name, fmt);
				goto done;
			}
			break;
		case 'X':
			for (k = 0; k < len; k++)
				if (isascii(data[k]) == 0 ||
				    isprint(data[k]) == 0)
					break;
			if (k == len) {
				fmtbuf[i] = 't';
				numhunk = -2;
			} else {
				hunksize++;
				comma = ':';
				numhunk = 0;
			}
			fmtbuf[i + 1] = 0;
			break;
		case 't':
			fmtbuf[i + 1] = 0;
			numhunk = -2;
			break;
		case 'I':
		case 'l':
		case 'L':
			hunksize += 4;
			break;
		case 'S':
			hunksize += 2;
			break;
		case 'B':
		case 'f':
			hunksize++;
			break;
		case 'e':
			break;
		default:
			log_warnx("%s: %s: garbage in format string: %s",
			    log_procname, name, &fmt[i]);
			goto done;
		}
	}

	/* Check for too few bytes. */
	if (hunksize > len) {
		log_warnx("%s: %s: expecting at least %d bytes; got %d",
		    log_procname, name, hunksize, len);
		goto done;
	}
	/* Check for too many bytes. */
	if (numhunk == -1 && hunksize < len) {
		log_warnx("%s: %s: expecting only %d bytes: got %d",
		    log_procname, name, hunksize, len);
		goto done;
	}

	/* If this is an array, compute its size. */
	if (numhunk == 0)
		numhunk = len / hunksize;
	/* See if we got an exact number of hunks. */
	if (numhunk > 0 && numhunk * hunksize != len) {
		log_warnx("%s: %s: expecting %d bytes: got %d", log_procname,
		    name, numhunk * hunksize, len);
		goto done;
	}

	/* A one-hunk array prints the same as a single hunk. */
	if (numhunk < 0)
		numhunk = 1;

	/* Cycle through the array (or hunk) printing the data. */
	for (i = 0; i < numhunk; i++) {
		for (j = 0; j < numelem; j++) {
			switch (fmtbuf[j]) {
			case 't':
				buf = pretty_print_string(dp, len, emit_punct);
				if (buf == NULL)
					opcount = -1;
				else
					opcount = strlcat(op, buf, opleft);
				break;
			case 'I':
				memcpy(&foo.s_addr, dp, sizeof(foo.s_addr));
				opcount = snprintf(op, opleft, "%s",
				    inet_ntoa(foo));
				dp += sizeof(foo.s_addr);
				break;
			case 'l':
				memcpy(&int32val, dp, sizeof(int32val));
				opcount = snprintf(op, opleft, "%d",
				    ntohl(int32val));
				dp += sizeof(int32val);
				break;
			case 'L':
				memcpy(&uint32val, dp, sizeof(uint32val));
				opcount = snprintf(op, opleft, "%u",
				    ntohl(uint32val));
				dp += sizeof(uint32val);
				break;
			case 'S':
				memcpy(&uint16val, dp, sizeof(uint16val));
				opcount = snprintf(op, opleft, "%hu",
				    ntohs(uint16val));
				dp += sizeof(uint16val);
				break;
			case 'B':
				opcount = snprintf(op, opleft, "%u", *dp);
				dp++;
				break;
			case 'X':
				opcount = snprintf(op, opleft, "%x", *dp);
				dp++;
				break;
			case 'f':
				opcount = snprintf(op, opleft, "%s",
				    *dp ? "true" : "false");
				dp++;
				break;
			default:
				log_warnx("%s: unexpected format code %c",
				    log_procname, fmtbuf[j]);
				goto toobig;
			}
			if (opcount >= opleft || opcount == -1)
				goto toobig;
			opleft -= opcount;
			op += opcount;
			if (j + 1 < numelem && comma != ':') {
				opcount = snprintf(op, opleft, " ");
				if (opcount >= opleft || opcount == -1)
					goto toobig;
				opleft -= opcount;
				op += opcount;
			}
		}
		if (i + 1 < numhunk) {
			opcount = snprintf(op, opleft, "%c", comma);
			if (opcount >= opleft || opcount == -1)
				goto toobig;
			opleft -= opcount;
			op += opcount;
		}
	}

done:
	return optbuf;

toobig:
	memset(optbuf, 0, sizeof(optbuf));
	return optbuf;
}

struct option_data *
unpack_options(struct dhcp_packet *packet)
{
	static struct option_data	 options[DHO_COUNT];
	int				 i;

	for (i = 0; i < DHO_COUNT; i++) {
		free(options[i].data);
		options[i].data = NULL;
		options[i].len = 0;
	}

	if (memcmp(&packet->options, DHCP_OPTIONS_COOKIE, 4) == 0) {
		/* Parse the BOOTP/DHCP options field. */
		parse_option_buffer(options, &packet->options[4],
		    sizeof(packet->options) - 4);

		/* DHCP packets can also use overload areas for options. */
		if (options[DHO_DHCP_MESSAGE_TYPE].data != NULL &&
		    options[DHO_DHCP_OPTION_OVERLOAD].data != NULL) {
			if ((options[DHO_DHCP_OPTION_OVERLOAD].data[0] & 1) !=
			    0)
				parse_option_buffer(options,
				    (unsigned char *)packet->file,
				    sizeof(packet->file));
			if ((options[DHO_DHCP_OPTION_OVERLOAD].data[0] & 2) !=
			    0)
				parse_option_buffer(options,
				    (unsigned char *)packet->sname,
				    sizeof(packet->sname));
		}
	}

	return options;
}
