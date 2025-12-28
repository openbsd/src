/*	$OpenBSD: print-dhcp6.c,v 1.17 2025/12/28 00:16:17 dlg Exp $	*/

/*
 * Copyright (c) 2019 David Gwynne <dlg@openbsd.org>
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

#include <sys/time.h>
#include <sys/socket.h>

#include <net/if.h>

#include <netinet/in.h>

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <vis.h>
#include <netdb.h>
#include <uuid.h>
#include <arpa/inet.h>

#include "interface.h"
#include "extract.h"
#include "addrtoname.h"

#ifndef nitems
#define nitems(_a) (sizeof((_a)) / sizeof((_a)[0]))
#endif

/* Message type */
#define DH6_SOLICIT		1
#define DH6_ADVERTISE		2
#define DH6_REQUEST		3
#define DH6_CONFIRM		4
#define DH6_RENEW		5
#define DH6_REBIND		6
#define DH6_REPLY		7
#define DH6_RELEASE		8
#define DH6_DECLINE		9
#define DH6_RECONFIGURE		10
#define DH6_INFORMATION_REQUEST	11
#define DH6_RELAY_FORW		12
#define DH6_RELAY_REPL		13

struct dhcp6opt {
	uint16_t		 code;
	const char		*name;
	void (*print)(uint16_t, const u_char *cp, u_int length);
};

static void	dhcp6opt_default(uint16_t, const u_char *, u_int);
static void	dhcp6opt_string(uint16_t, const u_char *, u_int);

static void	dhcp6opt_duid(uint16_t, const u_char *, u_int);
static void	dhcp6opt_ia_na(uint16_t, const u_char *, u_int);
static void	dhcp6opt_oro(uint16_t, const u_char *, u_int);
static void	dhcp6opt_elapsed(uint16_t, const u_char *, u_int);
static void	dhcp6opt_status_code(uint16_t, const u_char *, u_int);
static void	dhcp6opt_dns_servers(uint16_t, const u_char *, u_int);
static void	dhcp6opt_ia_pd(uint16_t, const u_char *, u_int);

static const struct dhcp6opt dhcp6opts[] = {
	{ 1,	"Client-Id",		dhcp6opt_duid },
	{ 2,	"Server-Id",		dhcp6opt_duid },
	{ 3,	"IA_NA",		dhcp6opt_ia_na },
	{ 6,	"Option-Request",	dhcp6opt_oro },
	{ 8,	"Elapsed-Time",		dhcp6opt_elapsed },
	{ 13,	"Status-Code",		dhcp6opt_status_code },
	{ 14,	"Rapid-Commit",		dhcp6opt_default },
	{ 23,	"DNS-Servers",		dhcp6opt_dns_servers },
	{ 24,	"Domain-List",		dhcp6opt_default },
	{ 25,	"IA_PD",		dhcp6opt_ia_pd },
	{ 59,	"BootFile-URL",		dhcp6opt_string },
};

static const struct dhcp6opt *
dhcp6opt_lookup(uint16_t code)
{
	size_t i;

	for (i = 0; i < nitems(dhcp6opts); i++) {
		const struct dhcp6opt *o = &dhcp6opts[i];
		if (o->code == code)
			return (o);
	}

	return (NULL);
}

static void
dhcp6opt_default(uint16_t code, const u_char *cp, u_int len)
{
	u_int i;

	for (i = 0; i < len; i++)
		printf("%02x", cp[i]);
}

static void
dhcp6opt_string(uint16_t code, const u_char *cp, u_int len)
{
	u_int i;
	char dst[5];

	for (i = 0; i < len; i++) {
		vis(dst, cp[i], VIS_TAB|VIS_NL, 0);
		printf("%s", dst);
	}
}

static int
dhcp6_iaid(const u_char *p, u_int l)
{
	uint32_t iaid, t1, t2;

	if (l < sizeof(iaid)) {
		printf("[|iaid]");
		return (-1);
	}
	iaid = EXTRACT_32BITS(p);
	p += sizeof(iaid);
	l -= sizeof(iaid);

	printf("IAID: %u, ", iaid);

	if (l < sizeof(t1)) {
		printf("[|t1]");
		return (-1);
	}
	t1 = EXTRACT_32BITS(p);
	p += sizeof(t1);
	l -= sizeof(t1);

	printf("T1: %us, ", t1);

	if (l < sizeof(t2)) {
		printf("[|t2]");
		return (-1);
	}
	t2 = EXTRACT_32BITS(p);
	p += sizeof(t2);
	l -= sizeof(t2);

	printf("T2: %us", t2);

	return (sizeof(iaid) + sizeof(t1) + sizeof(t2));
}

#define DH6_DUID_LLT		1
#define DH6_DUID_EN		2
#define DH6_DUID_LL		3
#define DH6_DUID_UUID		4

static void
dhcp6opt_duid(uint16_t code, const u_char *p, u_int l)
{
	uint16_t duid;
	uint16_t htype;
	uint32_t time, en;

	if (l < sizeof(duid)) {
		printf("[|duid]");
		return;
	}

	duid = EXTRACT_16BITS(p);
	p += sizeof(duid);
	l -= sizeof(duid);

	switch (duid) {
	case DH6_DUID_LLT:
		printf("LLT ");

		if (l < sizeof(htype)) {
			printf(" [|htype]");
			return;
		}
		htype = EXTRACT_16BITS(p);
		p += sizeof(htype);
		l -= sizeof(htype);

		if (l < sizeof(time)) {
			printf(" [|time]");
			return;
		}
		time = EXTRACT_32BITS(p);
		p += sizeof(time);
		l -= sizeof(time);

		printf("time %u ", time);

		if (htype == 1) {
			printf("Ethernet ");
			if (l == 6) {
				printf("%s", etheraddr_string(p));
				return;
			}
		} else
			printf("hwtype %u ", htype);
		break;

	case DH6_DUID_EN:
		printf("EN ");
		if (l < sizeof(en)) {
			printf(" [|en]");
			return;
		}
		en = EXTRACT_32BITS(p);
		p += sizeof(en);
		l -= sizeof(en);

		printf("%u ", en);
		break;

	case DH6_DUID_LL:
		printf("LL ");

		if (l < sizeof(htype)) {
			printf(" [|htype]");
			return;
		}
		htype = EXTRACT_16BITS(p);
		p += sizeof(htype);
		l -= sizeof(htype);

		if (htype == 1) {
			printf("Ethernet ");
			if (l == 6) {
				printf("%s", etheraddr_string(p));
				return;
			}
		} else
			printf("hwtype %u ", htype);
		break;

	case DH6_DUID_UUID:
		printf("UUID ");
		if (l >= 16) {
			uuid_t uuid;
			char *str;
			uint32_t status;

			uuid_dec_be(p, &uuid);
			uuid_to_string(&uuid, &str, &status);
			if (status == uuid_s_ok) {
				printf("%s", str);
				free(str);

				if (l == 16)
					return;

				p += 16;
				l -= 16;
				printf(" ");
			}
		}
		break;

	default:
		printf("DUID type %u ", duid);
		break;
	}

	dhcp6opt_default(code, p, l);
}

static void
dhcp6opt_ia_na_opt_iaaddr(const u_char *p, u_int l)
{
	const struct in6_addr *ia;
	uint32_t pltime, vltime;
	char n[NI_MAXHOST];

	if (l < sizeof(*ia)) {
		printf("[|prefix]");
		return;
	}
	ia = (const struct in6_addr *)p;
	p += sizeof(*ia);
	l -= sizeof(*ia);

        if (inet_ntop(AF_INET6, ia, n, sizeof(n)) == NULL) {
		printf("address ?");
		return;
	}
	printf("%s, ", n);

	if (l < sizeof(pltime)) {
		printf("[|pltime]");
		return;
	}
	pltime = EXTRACT_32BITS(p);
	p += sizeof(pltime);
	l -= sizeof(pltime);

	printf("preferred-lifetime ");
	if (pltime == 0xffffffff)
		printf("infinity");
	else
		printf("%us", pltime);
	printf(", ");

	if (l < sizeof(vltime)) {
		printf("[|vltime]");
		return;
	}
	vltime = EXTRACT_32BITS(p);
	p += sizeof(vltime);
	l -= sizeof(vltime);

	printf("valid-lifetime ");
	if (vltime == 0xffffffff)
		printf("infinity");
	else
		printf("%us", vltime);

	if (l > 0) {
		printf(", ");
		dhcp6opt_default(0, p, l);
	}
}

static uint16_t
dhcp6opt_ia_na_opt(const u_char *p, u_int l)
{
	uint16_t opt, len;

	if (l < sizeof(opt)) {
		printf("[|opt]");
		return (-1);
	}
	opt = EXTRACT_16BITS(p);
	p += sizeof(opt);
	l -= sizeof(opt);

	if (l < sizeof(len)) {
		printf("[|option-len]");
		return (-1);
	}
	len = EXTRACT_16BITS(p);
	p += sizeof(len);
	l -= sizeof(len);

	printf("\n\t\t");
	if (l < len) {
		printf("[|ia_pd_opt]");
		return (-1);
	}

	if (opt == 5)
		dhcp6opt_ia_na_opt_iaaddr(p, len);
	else {
		printf("option %u", opt);
		if (len > 0) {
			printf(" ");
			dhcp6opt_default(opt, p, len);
		}
	}

	return (sizeof(opt) + sizeof(len) + len);
}

static void
dhcp6opt_ia_na(uint16_t code, const u_char *p, u_int l)
{
	int len;

	len = dhcp6_iaid(p, l);
	if (len == -1)
		return;

	p += len;
	l -= len;

	if (l == 0)
		return;

	while (l > 0) {
		len = dhcp6opt_ia_na_opt(p, l);
		if (len == -1)
			return;

		p += len;
		l -= len;
	}
}

static void
dhcp6opt_oro(uint16_t code, const u_char *cp, u_int len)
{
	const struct dhcp6opt *o;
	uint16_t ocode;
	u_int i;
	const char *sep = "";

	for (i = 0; i < len; i += sizeof(ocode)) {
		printf("%s", sep);
		sep = ", ";

		ocode = EXTRACT_16BITS(cp + i);
		o = dhcp6opt_lookup(ocode);
		if (o != NULL)
			printf("%s", o->name);
		else
			printf("option %u", ocode);
	}
}

static void
dhcp6opt_elapsed(uint16_t code, const u_char *cp, u_int len)
{
	uint16_t etime;

	if (len < sizeof(etime)) {
		printf("[|time]");
		return;
	}

	etime = EXTRACT_16BITS(cp);

	printf("%u.%02us", etime / 100, etime % 100);
}

static void
dhcp6opt_status_code(uint16_t code, const u_char *p, u_int l)
{
	uint16_t scode;
	u_int i;

	if (l < sizeof(scode)) {
		printf("[|status-code]");
		return;
	}
	scode = EXTRACT_16BITS(p);
	p += sizeof(scode);
	l -= sizeof(scode);

	switch (scode) {
	case 0:
		printf("Success");
		break;
	case 1:
		printf("UnspecFail");
		break;
	case 2:
		printf("NoAddrsAvail");
		break;
	case 3:
		printf("NoBinding");
		break;
	case 4:
		printf("NotOnLink");
		break;
	case 5:
		printf("UseMulticast");
		break;
	case 6:
		printf("NoPrefixAvail");
		break;
	default:
		printf("code-%u", scode);
		break;
	}

	if (l == 0)
		return;

	printf(" \"");
	for (i = 0; i < l; i++) {
		int ch = p[i];
		if (!isprint(ch))
			ch = '_';

		printf("%c", ch);
	}
	printf("\"");
}

static void
dhcp6opt_dns_servers(uint16_t code, const u_char *p, u_int l)
{
	const struct in6_addr *ia;
	char n[NI_MAXHOST];

	for (;;) {
		if (l < sizeof(*ia)) {
			printf("[|dns-server]");
			return;
		}
		ia = (const struct in6_addr *)p;
		p += sizeof(*ia);
		l -= sizeof(*ia);

		if (inet_ntop(AF_INET6, ia, n, sizeof(n)) == NULL) {
			printf("dns-server ?");
			return;
		}

		printf("%s", n);

		if (l == 0)
			break;

		printf(", ");
	}
}

static void
dhcp6opt_ia_pd_opt_prefix(const u_char *p, u_int l)
{
	uint32_t pltime, vltime;
	uint8_t plen;
	struct in6_addr ia;
	char n[NI_MAXHOST];

	if (l < sizeof(pltime)) {
		printf("[|pltime]");
		return;
	}
	pltime = EXTRACT_32BITS(p);
	p += sizeof(pltime);
	l -= sizeof(pltime);

	printf("preferred-lifetime ");
	if (pltime == 0xffffffff)
		printf("infinity");
	else
		printf("%us", pltime);
	printf(", ");

	if (l < sizeof(vltime)) {
		printf("[|vltime]");
		return;
	}
	vltime = EXTRACT_32BITS(p);
	p += sizeof(vltime);
	l -= sizeof(vltime);

	printf("valid-lifetime ");
	if (vltime == 0xffffffff)
		printf("infinity");
	else
		printf("%us", vltime);
	printf(", ");

	if (l < sizeof(plen)) {
		printf("[|plen]");
		return;
	}
	plen = *p;
	p += sizeof(plen);
	l -= sizeof(plen);

	if (l < sizeof(ia)) {
		printf("[|prefix]");
		return;
	}
	memcpy(&ia, p, sizeof(ia));
	p += sizeof(ia);
	l -= sizeof(ia);

        if (inet_ntop(AF_INET6, &ia, n, sizeof(n)) == NULL) {
		printf("prefix ?");
		return;
	}

	printf("prefix %s/%u", n, plen);

	if (l > 0) {
		printf(", ");
		dhcp6opt_default(0, p, l);
	}
}

static uint16_t
dhcp6opt_ia_pd_opt(const u_char *p, u_int l)
{
	uint16_t opt, len;

	if (l < sizeof(opt)) {
		printf("[|opt]");
		return (-1);
	}
	opt = EXTRACT_16BITS(p);
	p += sizeof(opt);
	l -= sizeof(opt);

	if (l < sizeof(len)) {
		printf("[|option-len]");
		return (-1);
	}
	len = EXTRACT_16BITS(p);
	p += sizeof(len);
	l -= sizeof(len);

	printf("\n\t\t");
	if (l < len) {
		printf("[|ia_pd_opt]");
		return (-1);
	}

	if (opt == 26)
		dhcp6opt_ia_pd_opt_prefix(p, len);
	else {
		printf("option %u", opt);
		if (len > 0) {
			printf(" ");
			dhcp6opt_default(opt, p, len);
		}
	}

	return (sizeof(opt) + sizeof(len) + len);
}

static void
dhcp6opt_ia_pd(uint16_t code, const u_char *p, u_int l)
{
	int len;

	len = dhcp6_iaid(p, l);
	if (len == -1)
		return;

	p += len;
	l -= len;

	while (l > 0) {
		int len = dhcp6opt_ia_pd_opt(p, l);
		if (len == -1)
			return;

		p += len;
		l -= len;
	}
}

static void
dhcp6opt_print(const u_char *cp, u_int length)
{
	uint16_t code, len;
	int l = snapend - cp;
	const struct dhcp6opt *o;
	void (*print)(uint16_t, const u_char *cp, u_int length);

	while (length > 0) {
		if (l < sizeof(code))
			goto trunc;
		if (length < sizeof(code))
			goto iptrunc;

		code = EXTRACT_16BITS(cp);
		cp += sizeof(code);
		length -= sizeof(code);
		l -= sizeof(code);

		if (l < sizeof(len))
			goto trunc;
		if (length < sizeof(len))
			goto iptrunc;

		len = EXTRACT_16BITS(cp);
		cp += sizeof(len);
		length -= sizeof(len);
		l -= sizeof(len);

		printf("\n\t");

		o = dhcp6opt_lookup(code);
		if (o != NULL) {
			printf("%s", o->name);
			print = o->print;
		} else {
			printf("option %u", code);
			print = dhcp6opt_default;
		}

		if (len > 0) {
			if (l < len)
				goto trunc;
			if (length < len)
				goto iptrunc;

			printf(": ");
			(*print)(code, cp, len);

			cp += len;
			length -= len;
			l -= len;
		}
	}
	return;

trunc:
	printf(" [|dhcp6opt]");
	return;
iptrunc:
	printf(" ip truncated");
}

static void
dhcp6_relay_print(const u_char *cp, u_int length)
{
	uint8_t msgtype;
	const char *msgname = NULL;

	msgtype = *cp;

	switch (msgtype) {
	case DH6_RELAY_FORW:
		msgname = "Relay-forward";
		break;
	case DH6_RELAY_REPL:
		msgname = "Relay-reply";
		break;
	}

	printf(" %s", msgname);
}

void
dhcp6_print(const u_char *cp, u_int length)
{
	uint8_t msgtype;
	uint32_t hdr;
	int l = snapend - cp;
	const char *msgname;

	printf("DHCPv6");

	if (l < sizeof(msgtype))
		goto trunc;
	if (length < sizeof(msgtype))
		goto iptrunc;

	msgtype = *cp;

	switch (msgtype) {
	case DH6_SOLICIT:
		msgname = "Solicit";
		break;
	case DH6_ADVERTISE:
		msgname = "Advertise";
		break;
	case DH6_REQUEST:
		msgname = "Request";
		break;
	case DH6_CONFIRM:
		msgname = "Confirm";
		break;
	case DH6_RENEW:
		msgname = "Renew";
		break;
	case DH6_REBIND:
		msgname = "Rebind";
		break;
	case DH6_REPLY:
		msgname = "Reply";
		break;
	case DH6_RELEASE:
		msgname = "Release";
		break;
	case DH6_DECLINE:
		msgname = "Decline";
		break;
	case DH6_RECONFIGURE:
		msgname = "Reconfigure";
		break;
	case DH6_INFORMATION_REQUEST:
		msgname = "Information-request";
		break;
	case DH6_RELAY_FORW:
	case DH6_RELAY_REPL:
		dhcp6_relay_print(cp, length);
		return;
	default:
		printf(" unknown message type %u", msgtype);
		return;
	}

	printf(" %s", msgname);

	if (l < sizeof(hdr))
		goto trunc;
	if (length < sizeof(hdr))
		goto iptrunc;

	hdr = EXTRACT_32BITS(cp);
	printf(" xid %x", hdr & 0xffffff);

	if (vflag) {
		cp += sizeof(hdr);
		length -= sizeof(hdr);

		dhcp6opt_print(cp, length);
	}
	return;

trunc:
	printf(" [|dhcp6]");
	return;
iptrunc:
	printf(" ip truncated");
}
