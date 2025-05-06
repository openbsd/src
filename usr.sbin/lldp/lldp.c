/* $OpenBSD: lldp.c,v 1.3 2025/05/06 06:04:58 jmc Exp $ */

/*
 * Copyright (c) 2024 David Gwynne <dlg@openbsd.org>
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

#include <sys/socket.h>
#include <sys/types.h>
#include <sys/un.h>

#include <net/if.h> /* IFNAMSIZ */
#include <net/frame.h>
#include <net/lldp.h>

#include <netinet/in.h>
#include <netinet/if_ether.h> /* ether_ntoa */

#include <arpa/inet.h> /* inet_ntop */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stddef.h>
#include <stdint.h>
#include <unistd.h>
#include <vis.h>
#include <assert.h>
#include <ctype.h>
#include <err.h>
#include <errno.h>

#include "pdu.h"
#include "lldpctl.h"

#ifndef nitems
#define nitems(_a) (sizeof((_a)) / sizeof((_a)[0]))
#endif

#ifndef ISSET
#define ISSET(_a, _b) ((_a) & (_b))
#endif

struct slice {
	const void	*sl_base;
	size_t		 sl_len;
};

struct string {
	char		*str_base;
	size_t		 str_len;
};

void hexdump(const void *, size_t);

enum lldp_tlv_idx {
	tlv_chassis_id,
	tlv_port_id,
	tlv_ttl,
	tlv_port_descr,
	tlv_system_name,
	tlv_system_descr,
	tlv_system_cap,
	tlv_management_addr,

	tlv_org,

	tlv_count
};

struct lldp_tlv {
	uint8_t			 type;
	const char		*name;

	void (*toscratch)(const void *, size_t, int);
};

static void	lldp_bytes_to_scratch(const void *, size_t, int);
static void	lldp_string_to_scratch(const void *, size_t, int);

static void	lldp_chassis_id_to_scratch(const void *, size_t, int);
static void	lldp_port_id_to_scratch(const void *, size_t, int);
static void	lldp_ttl_to_scratch(const void *, size_t, int);
static void	lldp_system_cap_to_scratch(const void *, size_t, int);
static void	lldp_system_descr_to_scratch(const void *, size_t, int);
static void	lldp_mgmt_addr_to_scratch(const void *, size_t, int);
static void	lldp_org_to_scratch(const void *, size_t, int);

static const struct lldp_tlv lldp_tlv_map[] = {
	[tlv_chassis_id] = {
		.type =		LLDP_TLV_CHASSIS_ID,
		.name =		"Chassis ID",
		.toscratch = 	lldp_chassis_id_to_scratch,
	},
	[tlv_port_id] = {
		.type =		LLDP_TLV_PORT_ID,
		.name = 	"Port ID",
		.toscratch = 	lldp_port_id_to_scratch,
	},
	[tlv_ttl] = {
		.type =		LLDP_TLV_TTL,
		.name = 	"Time-To-Live",
		.toscratch = 	lldp_ttl_to_scratch,
	},
	[tlv_port_descr] = {
		.type =		LLDP_TLV_PORT_DESCR,
		.name = 	"Port Description",
		.toscratch = 	lldp_string_to_scratch,
	},
	[tlv_system_name] = {
		.type =		LLDP_TLV_SYSTEM_NAME,
		.name = 	"System Name",
		.toscratch = 	lldp_string_to_scratch,
	},
	[tlv_system_descr] = {
		.type =		LLDP_TLV_SYSTEM_DESCR,
		.name = 	"System Description",
		.toscratch = 	lldp_system_descr_to_scratch,
	},
	[tlv_system_cap] = {
		.type =		LLDP_TLV_SYSTEM_CAP,
		.name = 	"System Capabilities",
		.toscratch = 	lldp_system_cap_to_scratch,
	},
	[tlv_management_addr] = {
		.type =		LLDP_TLV_MANAGEMENT_ADDR,
		.name = 	"Management Address",
		.toscratch = 	lldp_mgmt_addr_to_scratch,
	},

	[tlv_org] = {
		.type =		LLDP_TLV_ORG,
		.name = 	NULL,
		.toscratch = 	lldp_org_to_scratch,
	},
};

struct lldp_pdu {
	struct slice	slices[tlv_count];
	struct string	strings[tlv_count];
};

static void	lldpctl_req_msaps(int, const char *);
static void	lldp_dump(const struct lldp_ctl_msg_msap *,
		    const void *, size_t);
static void	lldp_dump_verbose(const struct lldp_ctl_msg_msap *,
		    const void *, size_t, int);

static char scratch_mem[8192];
static FILE *scratch;

static void	scratch_reset(void);
static size_t	scratch_len(void);
static size_t	scratch_end(void);

static void	reltime_to_scratch(time_t);

__dead static void
usage(void)
{
	extern char *__progname;

	fprintf(stderr, "usage: %s [-v] [-i interface] [-s socket]\n",
	    __progname);

	exit(1);
}

int
main(int argc, char *argv[])
{
	const char *ifname = NULL;
	const char *sockname = LLDP_CTL_PATH;
	int verbose = 0;
	struct sockaddr_un sun = {
		.sun_family = AF_UNIX,
	};
	int ch;
	int s;

	scratch = fmemopen(scratch_mem, sizeof(scratch_mem), "w");
	if (scratch == NULL)
		err(1, "fmemopen scratch");

	while ((ch = getopt(argc, argv, "i:s:v")) != -1) {
		switch (ch) {
		case 'i':
			ifname = optarg;
			break;
		case 's':
			sockname = optarg;
			break;
		case 'v':
			verbose++;
			break;
		default:
			usage();
			/* NOTREACHED */
		}
	}

	if (strlcpy(sun.sun_path, sockname, sizeof(sun.sun_path)) >=
	    sizeof(sun.sun_path))
		errc(1, ENAMETOOLONG, "socket name");

	s = socket(AF_UNIX, SOCK_SEQPACKET, 0);
	if (s == -1)
		err(1, "socket");

	if (connect(s, (struct sockaddr *)&sun, sizeof(sun)) == -1)
		err(1, "%s connect", sun.sun_path);

	lldpctl_req_msaps(s, ifname);

	if (!verbose) {
		printf("%-8s %-24s %-24s %s\n", "IFACE",
		    "SYSTEM", "PORTID", "CHASSISID");
	}

	for (;;) {
		unsigned int msgtype;
		struct lldp_ctl_msg_msap msg_msap;
		char buf[2048];
		struct iovec iov[3] = {
		    { &msgtype, sizeof(msgtype) },
		    { &msg_msap, sizeof(msg_msap) },
		    { buf, sizeof(buf) },
		};
		struct msghdr msg = {
			.msg_iov = iov,
			.msg_iovlen = nitems(iov),
		};
		ssize_t rv;
		size_t len;

		rv = recvmsg(s, &msg, 0);
		if (rv == -1)
			err(1, "recv");
		if (rv == 0)
			break;
		len = rv;

		if (len < sizeof(msgtype)) {
			warnx("too short for msgtype\n");
			continue;
		}
		if (msgtype == LLDP_CTL_MSG_MSAP_END)
			break;
		if (msgtype != LLDP_CTL_MSG_MSAP) {
			warnx("unexpected msgtype %u", msgtype);
			continue;
		}
		len -= sizeof(msgtype);

		if (len < sizeof(msg_msap)) {
			warnx("too short for msg_msap\n");
			continue;
		}
		len -= sizeof(msg_msap);
#if 0
		printf("%s %s\n", msg_msap.ifname,
		    ether_ntoa(&msg_msap.saddr));
		hexdump(buf, len);
#endif

		if (!verbose)
			lldp_dump(&msg_msap, buf, len);
		else
			lldp_dump_verbose(&msg_msap, buf, len, verbose);
	}

	return (0);
}

static void
lldpctl_req_msaps(int fd, const char *ifname)
{
	unsigned int msgtype = LLDP_CTL_MSG_MSAP_REQ;
	struct lldp_ctl_msg_msap_req msg_msap_req;
	struct iovec iov[2] = {
	    { &msgtype, sizeof(msgtype) },
	    { &msg_msap_req, sizeof(msg_msap_req) },
	};
	struct msghdr msg = {
		.msg_iov = iov,
		.msg_iovlen = nitems(iov),
	};
	ssize_t rv;

	memset(&msg_msap_req, 0, sizeof(msg_msap_req));

	if (ifname != NULL) {
		if (strlcpy(msg_msap_req.ifname, ifname,
		    sizeof(msg_msap_req.ifname)) >=
		    sizeof(msg_msap_req.ifname))
			errx(1, "interface name too long");
	}

	rv = sendmsg(fd, &msg, 0);
	if (rv == -1)
		err(1, "send msap msg req");
}

static const struct lldp_tlv *
lldp_tlv_lookup(unsigned int type)
{
	const struct lldp_tlv *ltlv;
	size_t i;

	for (i = 0; i < nitems(lldp_tlv_map); i++) {
		ltlv = &lldp_tlv_map[i];
		if (ltlv->type == type)
			return (ltlv);
	}

	return (NULL);
}

static size_t
lldp_tlv_to_idx(const struct lldp_tlv *ltlv)
{
	return (ltlv - lldp_tlv_map);
}

static void
lldp_dump(const struct lldp_ctl_msg_msap *msg_msap,
    const void *pdu, size_t len)
{
	struct lldp_pdu lpdu;
	struct tlv tlv;
	const struct lldp_tlv *ltlv;
	struct slice *sl;
	size_t i;

	for (i = 0; i < tlv_count; i++) {
		sl = &lpdu.slices[i];
		sl->sl_base = NULL;
		sl->sl_len = 0;
	}

	/* olldpd only gives us well formed PDUs */
	tlv_first(&tlv, pdu, len);
	do {
		struct slice *sl;

		ltlv = lldp_tlv_lookup(tlv.tlv_type);
		if (ltlv == NULL)
			continue;
		i = lldp_tlv_to_idx(ltlv);

		sl = &lpdu.slices[i];
		sl->sl_base = tlv.tlv_payload;
		sl->sl_len = tlv.tlv_len;
	} while (tlv_next(&tlv, pdu, len));

	printf("%-8s", msg_msap->ifname);

	scratch_reset();
	sl = &lpdu.slices[tlv_system_name];
	if (sl->sl_base == NULL) {
		fprintf(scratch, "-");
	} else {
		ltlv = &lldp_tlv_map[tlv_system_name];
		ltlv->toscratch(sl->sl_base, sl->sl_len, 0);
	}
	scratch_end();
	printf(" %-24s", scratch_mem);

	scratch_reset();
	sl = &lpdu.slices[tlv_port_id];
	if (sl->sl_base == NULL) {
		fprintf(scratch, "-");
	} else {
		ltlv = &lldp_tlv_map[tlv_port_id];
		ltlv->toscratch(sl->sl_base, sl->sl_len, 0);
	}
	scratch_end();
	printf(" %-24s", scratch_mem);

	scratch_reset();
	sl = &lpdu.slices[tlv_chassis_id];
	if (sl->sl_base == NULL) {
		fprintf(scratch, "-");
	} else {
		ltlv = &lldp_tlv_map[tlv_chassis_id];
		ltlv->toscratch(sl->sl_base, sl->sl_len, 0);
	}
	scratch_end();
	printf(" %s", scratch_mem);

	printf("\n");
}

static void
lldp_dump_verbose(const struct lldp_ctl_msg_msap *msg_msap,
    const void *pdu, size_t len, int verbose)
{
	struct tlv tlv;
	const struct lldp_tlv *ltlv;

	printf("Local interface: %s, Source address: %s\n", msg_msap->ifname,
	    ether_ntoa((struct ether_addr *)&msg_msap->saddr));

	if (verbose > 1) {
		struct timespec now, diff;
		if (clock_gettime(CLOCK_BOOTTIME, &now) == -1)
			err(1, "clock_gettime(CLOCK_BOOTTIME)");

		scratch_reset();
		timespecsub(&now, &msg_msap->created, &diff);
		reltime_to_scratch(diff.tv_sec);
		scratch_end();
		printf("LLDP entry created: %s\n", scratch_mem);

		scratch_reset();
		timespecsub(&now, &msg_msap->updated, &diff);
		reltime_to_scratch(diff.tv_sec);
		scratch_end();
		printf("LLDP entry updated: %s\n", scratch_mem);

		printf("LLDP packets: %llu\n", msg_msap->packets);
		printf("LLDP updates: %llu\n", msg_msap->updates);
	}

	/* olldpd only gives us well formed PDUs */
	tlv_first(&tlv, pdu, len);
	do {
		void (*toscratch)(const void *, size_t, int) =
		    lldp_bytes_to_scratch;

		ltlv = lldp_tlv_lookup(tlv.tlv_type);
		if (ltlv == NULL) {
			printf("tlv type %u: ", tlv.tlv_type);
		} else {
			if (ltlv->name)
				printf("%s: ", ltlv->name);
			toscratch = ltlv->toscratch;
		}

		scratch_reset();
		toscratch(tlv.tlv_payload, tlv.tlv_len, 1);
		scratch_end();
		printf("%s\n", scratch_mem);

	} while (tlv_next(&tlv, pdu, len));

	printf("---\n");
}

static void
scratch_reset(void)
{
	if (fseeko(scratch, 0, SEEK_SET) == -1)
		err(1, "scratch reset");
}

static size_t
scratch_len(void)
{
	off_t len = ftello(scratch);
	assert(len < sizeof(scratch_mem));
	return (len);
}

static size_t
scratch_end(void)
{
	off_t len = scratch_len();
	scratch_mem[len] = '\0';
	return (len);
}

struct interval {
	const char	p;
	time_t		s;
};

static const struct interval intervals[] = {
	{ 'w',	60 * 60 * 24 * 7 },
	{ 'd',	60 * 60 * 24 },
	{ 'h',	60 * 60 },
	{ 'm',	60 },
	{ 's',	1 },
};

static void
reltime_to_scratch(time_t sec)
{
	size_t i;

	for (i = 0; i < nitems(intervals); i++) {
		const struct interval *ival = &intervals[i];

		if (sec >= ival->s) {
			time_t d = sec / ival->s;
			fprintf(scratch, "%lld%c", d, ival->p);
			sec -= d * ival->s;
		}
	}
}

static void
lldp_bytes_to_scratch(const void *base, size_t len, int flags)
{
	const uint8_t *buf = base;
	size_t i;

	for (i = 0; i < len; i++)
		fprintf(scratch, "%02x", buf[i]);
}

static void
lldp_string_to_scratch(const void *base, size_t len, int flags)
{
	const uint8_t *buf = base;
	size_t i;
	char dst[8];

	for (i = 0; i < len; i++) {
		vis(dst, buf[i], VIS_NL, 0);
		fprintf(scratch, "%s", dst);
	}
}

static void
lldp_system_descr_to_scratch(const void *base, size_t len, int flags)
{
	const uint8_t *buf = base;
	size_t i;
	char dst[8];

	for (i = 0; i < len; i++) {
		int ch = buf[i];
		switch (ch) {
		case '\r':
			break;
		case '\n':
			fprintf(scratch, "\n\t");
			break;
		default:
			vis(dst, ch, 0, 0);
			fprintf(scratch, "%s", dst);
			break;
		}
	}
}

static void
lldp_macaddr_to_scratch(const void *base, size_t len, int flags)
{
	struct ether_addr *ea;

	if (len < sizeof(*ea)) {
		lldp_bytes_to_scratch(base, len, flags);
		return;
	}

	ea = (struct ether_addr *)base;
	fprintf(scratch, "%s", ether_ntoa(ea));
}

static void
lldp_chassis_id_to_scratch(const void *base, size_t len, int flags)
{
	const uint8_t *buf = base;
	uint8_t subtype;

	assert(len >= 2);

	subtype = buf[0];

	buf++;
	len--;

	switch (subtype) {
	case LLDP_CHASSIS_ID_MACADDR:
		lldp_macaddr_to_scratch(buf, len, flags);
		break;
	case LLDP_CHASSIS_ID_CHASSIS:
	case LLDP_CHASSIS_ID_IFALIAS:
	case LLDP_CHASSIS_ID_PORT:
	case LLDP_CHASSIS_ID_ADDR:
	case LLDP_CHASSIS_ID_IFNAME:
	case LLDP_CHASSIS_ID_LOCAL:
		lldp_string_to_scratch(buf, len, flags);
		break;
	default:
		fprintf(scratch, "reserved (subtype %u) ", subtype);
		lldp_bytes_to_scratch(buf, len, flags);
		break;
	}
}

static void
lldp_port_id_to_scratch(const void *base, size_t len, int flags)
{
	const uint8_t *buf = base;
	uint8_t subtype;

	assert(len >= 2);

	subtype = buf[0];

	buf++;
	len--;

	switch (subtype) {
	case LLDP_PORT_ID_MACADDR:
		lldp_macaddr_to_scratch(base, len, flags);
		break;
	case LLDP_PORT_ID_IFALIAS:
	case LLDP_PORT_ID_PORT:
	case LLDP_PORT_ID_ADDR:
	case LLDP_PORT_ID_IFNAME:
	case LLDP_PORT_ID_AGENTCID:
	case LLDP_PORT_ID_LOCAL:
		lldp_string_to_scratch(buf, len, flags);
		break;
	default:
		fprintf(scratch, "reserved (subtype %u) ", subtype);
		lldp_bytes_to_scratch(buf, len, flags);
		break;
	}
}

static void
lldp_ttl_to_scratch(const void *base, size_t len, int flags)
{
	uint16_t ttl;

	assert(len >= sizeof(ttl));
	ttl = pdu_u16(base);

	reltime_to_scratch(ttl);
}

struct lldp_system_cap {
	uint16_t	 bit;
	const char	*name;
};

static const struct lldp_system_cap lldp_system_caps[] = {
	{ LLDP_SYSTEM_CAP_OTHER,	"Other" },
	{ LLDP_SYSTEM_CAP_REPEATER,	"Repeater" },
	{ LLDP_SYSTEM_CAP_BRIDGE,	"Bridge" },
	{ LLDP_SYSTEM_CAP_WLAN,		"WLAN" },
	{ LLDP_SYSTEM_CAP_ROUTER,	"Router" },
	{ LLDP_SYSTEM_CAP_TELEPHONE,	"Telephone" },
	{ LLDP_SYSTEM_CAP_DOCSIS,	"DOCSIS" },
	{ LLDP_SYSTEM_CAP_STATION,	"Station" },
};

static const char *
lldp_system_cap_name(uint16_t bit)
{
	size_t i;

	for (i = 0; i < nitems(lldp_system_caps); i++) {
		const struct lldp_system_cap *e = &lldp_system_caps[i];
		if (e->bit == bit)
			return (e->name);
	}

	return (NULL);
}

static void
lldp_system_cap_to_scratch(const void *base, size_t len, int flags)
{
	const uint8_t *buf;
	struct lldp_system_cap {
		uint16_t	available;
		uint16_t	enabled;
	} cap;
	const char *sep = "";
	unsigned int i;

	if (len < sizeof(cap) ){
		fprintf(scratch, "[|system cap]");
		return;
	}

	buf = base;

	cap.available = pdu_u16(buf +
	    offsetof(struct lldp_system_cap, available));
	cap.enabled = pdu_u16(buf +
	    offsetof(struct lldp_system_cap, enabled));

	for (i = 0; i < NBBY * sizeof(cap.available); i++) {
		const char *name;
		uint16_t bit = (1 << i);

		if (!ISSET(cap.available, bit))
			continue;

		fprintf(scratch, "%s", sep);
		name = lldp_system_cap_name(bit);
		if (name == NULL)
			fprintf(scratch, "Bit%u", i + 1);
		else
			fprintf(scratch, "%s", name);

		fprintf(scratch, ": %s",
		    ISSET(cap.enabled, bit) ? "enabled" : "disabled");

		sep = ", ";
	}
}

static void
lldp_mgmt_addr_to_scratch(const void *base, size_t len, int flags)
{
	const uint8_t *buf = base;
	uint8_t aftype;
	size_t alen;
	const uint8_t *abuf;
	char ipbuf[64];
	uint32_t ifnum;

	if (len < 1) {
		fprintf(scratch, "too short (%zu bytes)", len);
		return;
	}
	alen = buf[0];
	len--;
	buf++;

	if (len < alen) {
		fprintf(scratch, "address len %zu is longer than tlv", alen);
		return;
	}
	abuf = buf;
	len -= alen;
	buf += alen;

	if (alen < 1) {
		fprintf(scratch, "address len %zu is too short", alen);
		return;
	}
	aftype = abuf[0];
	alen--;
	abuf++;

	switch (aftype) {
	case 1:
		if (alen != 4) {
			fprintf(scratch, "IPv4? ");
			goto abytes;
		}
		inet_ntop(AF_INET, abuf, ipbuf, sizeof(ipbuf));
		fprintf(scratch, "%s", ipbuf);
		break;
	case 2:
		if (alen != 16) {
			fprintf(scratch, "IPv7? ");
			goto abytes;
		}
		inet_ntop(AF_INET6, abuf, ipbuf, sizeof(ipbuf));
		fprintf(scratch, "%s", ipbuf);
		break;

	case 6:
		if (alen != 6) {
			fprintf(scratch, "802? ");
			goto abytes;
		}
		fprintf(scratch, "%s", ether_ntoa((struct ether_addr *)abuf));
		break;

	default:
		fprintf(scratch, "af %u ", aftype);
abytes:
		lldp_bytes_to_scratch(abuf, alen, 0);
		break;
	}

	if (len < 5) {
		fprintf(scratch, ", [|interface number]");
		return;
	}
	ifnum = pdu_u32(buf + 1);

	switch (buf[0]) {
	case 0:
		break;
	case 1:
		fprintf(scratch, ", ifIndex %u", ifnum);
		break;
	case 2:
		fprintf(scratch, ", port %u", ifnum);
		break;
	default:
		fprintf(scratch, ", if type %u num %u", buf[0], ifnum);
		break;
	}

	len -= 5;
	buf += 5;

	if (len < 1) {
		fprintf(scratch, ", [|object identifier]");
		return;
	}
	alen = buf[0];
	len--;
	buf++;

	if (alen == 0)
		return;
	if (len < alen) {
		fprintf(scratch, ", oid %zu is longer than tlv", alen);
		return;
	}
	fprintf(scratch, ", oid ");
	lldp_bytes_to_scratch(buf, len, 0);
}

static void
lldp_org_to_scratch(const void *base, size_t len, int flags)
{
	const uint8_t *buf;

	if (len < 4) {
		fprintf(scratch, "[|org]");
		return;
	}

	buf = base;
	fprintf(scratch, "%02X-%02X-%02X subtype %u: ",
	    buf[0], buf[1], buf[2], buf[3]);

	buf += 4;
	len -= 4;

	lldp_bytes_to_scratch(buf, len, flags);
}

static int
printable(int ch)
{
	if (ch == '\0')
		return ('_');
	if (!isprint(ch))
		return ('~');

	return (ch);
}

void
hexdump(const void *d, size_t datalen)
{
	const uint8_t *data = d;
	size_t i, j = 0;

	for (i = 0; i < datalen; i += j) {
		printf("%4zu: ", i);
		for (j = 0; j < 16 && i+j < datalen; j++)
			printf("%02x ", data[i + j]);
		while (j++ < 16)
			printf("   ");
		printf("|");
		for (j = 0; j < 16 && i+j < datalen; j++)
			putchar(printable(data[i + j]));
		printf("|\n");
	}
}
