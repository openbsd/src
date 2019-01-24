/*	$OpenBSD: handshake_table.c,v 1.5 2019/01/24 03:48:09 tb Exp $	*/
/*
 * Copyright (c) 2019 Theo Buehler <tb@openbsd.org>
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

#include <err.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#include "tls13_handshake.h"

/*
 * From RFC 8446:
 *
 * Appendix A.  State Machine
 *
 *    This appendix provides a summary of the legal state transitions for
 *    the client and server handshakes.  State names (in all capitals,
 *    e.g., START) have no formal meaning but are provided for ease of
 *    comprehension.  Actions which are taken only in certain circumstances
 *    are indicated in [].  The notation "K_{send,recv} = foo" means "set
 *    the send/recv key to the given key".
 *
 * A.1.  Client
 *
 *                               START <----+
 *                Send ClientHello |        | Recv HelloRetryRequest
 *           [K_send = early data] |        |
 *                                 v        |
 *            /                 WAIT_SH ----+
 *            |                    | Recv ServerHello
 *            |                    | K_recv = handshake
 *        Can |                    V
 *       send |                 WAIT_EE
 *      early |                    | Recv EncryptedExtensions
 *       data |           +--------+--------+
 *            |     Using |                 | Using certificate
 *            |       PSK |                 v
 *            |           |            WAIT_CERT_CR
 *            |           |        Recv |       | Recv CertificateRequest
 *            |           | Certificate |       v
 *            |           |             |    WAIT_CERT
 *            |           |             |       | Recv Certificate
 *            |           |             v       v
 *            |           |              WAIT_CV
 *            |           |                 | Recv CertificateVerify
 *            |           +> WAIT_FINISHED <+
 *            |                  | Recv Finished
 *            \                  | [Send EndOfEarlyData]
 *                               | K_send = handshake
 *                               | [Send Certificate [+ CertificateVerify]]
 *     Can send                  | Send Finished
 *     app data   -->            | K_send = K_recv = application
 *     after here                v
 *                           CONNECTED
 *
 *    Note that with the transitions as shown above, clients may send
 *    alerts that derive from post-ServerHello messages in the clear or
 *    with the early data keys.  If clients need to send such alerts, they
 *    SHOULD first rekey to the handshake keys if possible.
 *
 */

struct child {
	enum tls13_message_type	mt;
	uint8_t			flag;
	uint8_t			forced;
	uint8_t			illegal;
};

#define DEFAULT			0x00

static struct child stateinfo[][TLS13_NUM_MESSAGE_TYPES] = {
	[CLIENT_HELLO] = {
		{SERVER_HELLO, DEFAULT, 0, 0},
	},
	[SERVER_HELLO] = {
		{SERVER_ENCRYPTED_EXTENSIONS, DEFAULT, 0, 0},
		{CLIENT_HELLO_RETRY, WITH_HRR, 0, 0},
	},
	[CLIENT_HELLO_RETRY] = {
		{SERVER_ENCRYPTED_EXTENSIONS, DEFAULT, 0, 0},
	},
	[SERVER_ENCRYPTED_EXTENSIONS] = {
		{SERVER_CERTIFICATE_REQUEST, DEFAULT, 0, 0},
		{SERVER_CERTIFICATE, WITHOUT_CR, 0, 0},
		{SERVER_FINISHED, WITH_PSK, 0, 0},
	},
	[SERVER_CERTIFICATE_REQUEST] = {
		{SERVER_CERTIFICATE, DEFAULT, 0, 0},
	},
	[SERVER_CERTIFICATE] = {
		{SERVER_CERTIFICATE_VERIFY, DEFAULT, 0, 0},
	},
	[SERVER_CERTIFICATE_VERIFY] = {
		{SERVER_FINISHED, DEFAULT, 0, 0},
	},
	[SERVER_FINISHED] = {
		{CLIENT_FINISHED, DEFAULT, WITHOUT_CR | WITH_PSK, 0},
		{CLIENT_CERTIFICATE, DEFAULT, 0, WITHOUT_CR | WITH_PSK},
		/* {CLIENT_END_OF_EARLY_DATA, WITH_0RTT, 0, 0}, */
	},
	[CLIENT_CERTIFICATE] = {
		{CLIENT_FINISHED, DEFAULT, 0, 0},
		{CLIENT_CERTIFICATE_VERIFY, WITH_CCV, 0, 0},
	},
	[CLIENT_CERTIFICATE_VERIFY] = {
		{CLIENT_FINISHED, DEFAULT, 0, 0},
	},
	[CLIENT_FINISHED] = {
		{APPLICATION_DATA, DEFAULT, 0, 0},
	},
	[APPLICATION_DATA] = {
		{0, DEFAULT, 0, 0},
	},
};

void build_table(enum tls13_message_type
    table[UINT8_MAX][TLS13_NUM_MESSAGE_TYPES], struct child current,
    struct child end, struct child path[], uint8_t flags, unsigned int depth);
size_t count_handshakes(void);
const char *flag2str(uint8_t flag);
const char *mt2str(enum tls13_message_type mt);
void print_entry(enum tls13_message_type path[TLS13_NUM_MESSAGE_TYPES],
    uint8_t flags);
void print_flags(uint8_t flags);
__dead void usage(void);
int verify_table(enum tls13_message_type
    table[UINT8_MAX][TLS13_NUM_MESSAGE_TYPES], int print);

const char *
flag2str(uint8_t flag)
{
	const char *ret;

	if (flag & (flag - 1))
		errx(1, "more than one bit is set");

	switch (flag) {
	case INITIAL:
		ret = "INITIAL";
		break;
	case NEGOTIATED:
		ret = "NEGOTIATED";
		break;
	case WITHOUT_CR:
		ret = "WITHOUT_CR";
		break;
	case WITH_HRR:
		ret = "WITH_HRR";
		break;
	case WITH_PSK:
		ret = "WITH_PSK";
		break;
	case WITH_CCV:
		ret = "WITH_CCV";
		break;
	case WITH_0RTT:
		ret = "WITH_0RTT";
		break;
	default:
		ret = "UNKNOWN";
	}

	return ret;
}

const char *
mt2str(enum tls13_message_type mt)
{
	const char *ret;

	switch (mt) {
	case INVALID:
		ret = "INVALID";
		break;
	case CLIENT_HELLO:
		ret = "CLIENT_HELLO";
		break;
	case CLIENT_HELLO_RETRY:
		ret = "CLIENT_HELLO_RETRY";
		break;
	case CLIENT_END_OF_EARLY_DATA:
		ret = "CLIENT_END_OF_EARLY_DATA";
		break;
	case CLIENT_CERTIFICATE:
		ret = "CLIENT_CERTIFICATE";
		break;
	case CLIENT_CERTIFICATE_VERIFY:
		ret = "CLIENT_CERTIFICATE_VERIFY";
		break;
	case CLIENT_FINISHED:
		ret = "CLIENT_FINISHED";
		break;
	case CLIENT_KEY_UPDATE:
		ret = "CLIENT_KEY_UPDATE";
		break;
	case SERVER_HELLO:
		ret = "SERVER_HELLO";
		break;
	case SERVER_NEW_SESSION_TICKET:
		ret = "SERVER_NEW_SESSION_TICKET";
		break;
	case SERVER_ENCRYPTED_EXTENSIONS:
		ret = "SERVER_ENCRYPTED_EXTENSIONS";
		break;
	case SERVER_CERTIFICATE:
		ret = "SERVER_CERTIFICATE";
		break;
	case SERVER_CERTIFICATE_VERIFY:
		ret = "SERVER_CERTIFICATE_VERIFY";
		break;
	case SERVER_CERTIFICATE_REQUEST:
		ret = "SERVER_CERTIFICATE_REQUEST";
		break;
	case SERVER_FINISHED:
		ret = "SERVER_FINISHED";
		break;
	case APPLICATION_DATA:
		ret = "APPLICATION_DATA";
		break;
	case TLS13_NUM_MESSAGE_TYPES:
		ret = "TLS13_NUM_MESSAGE_TYPES";
		break;
	default:
		ret = "UNKNOWN";
		break;
	}

	return ret;
}

void
print_flags(uint8_t flags)
{
	int first = 1, i;

	if (flags == 0) {
		printf("%s", flag2str(flags));
		return;
	}

	for (i = 0; i < 8; i++) {
		uint8_t set = flags & (1U << i);

		if (set) {
			printf("%s%s", first ? "" : " | ", flag2str(set));
			first = 0;
		}
	}
}

void
print_entry(enum tls13_message_type path[TLS13_NUM_MESSAGE_TYPES],
    uint8_t flags)
{
	int i;

	printf("\t[");
	print_flags(flags);
	printf("] = {\n");

	for (i = 0; i < TLS13_NUM_MESSAGE_TYPES; i++) {
		if (path[i] == 0)
			break;
		printf("\t\t%s,\n", mt2str(path[i]));
	}
	printf("\t},\n");
}

extern enum tls13_message_type handshakes[][TLS13_NUM_MESSAGE_TYPES];
extern size_t handshake_count;

size_t
count_handshakes(void)
{
	size_t	ret = 0, i;

	for (i = 0; i < handshake_count; i++) {
		if (handshakes[i][0] != INVALID)
			ret++;
	}

	return ret;
}

void
build_table(enum tls13_message_type table[UINT8_MAX][TLS13_NUM_MESSAGE_TYPES],
    struct child current, struct child end, struct child path[], uint8_t flags,
    unsigned int depth)
{
	unsigned int i;

	if (depth >= TLS13_NUM_MESSAGE_TYPES - 1)
		errx(1, "recursed too deeply");

	/* Record current node. */
	path[depth++] = current;
	flags |= current.flag;

	/* If we haven't reached the end, recurse over the children. */
	if (current.mt != end.mt) {
		for (i = 0; stateinfo[current.mt][i].mt != 0; i++) {
			struct child child = stateinfo[current.mt][i];
			int forced = stateinfo[current.mt][i].forced;
			int illegal = stateinfo[current.mt][i].illegal;

			if ((forced == 0 || (forced & flags)) &&
			    (illegal == 0 || !(illegal & flags)))
				build_table(table, child, end, path, flags,
				    depth);
		}
		return;
	}

	if (flags == 0)
		errx(1, "path does not set flags");

	if (table[flags][0] != 0)
		errx(1, "path traversed twice");

	for (i = 0; i < depth; i++)
		table[flags][i] = path[i].mt;
}

int
verify_table(enum tls13_message_type table[UINT8_MAX][TLS13_NUM_MESSAGE_TYPES],
    int print)
{
	int	success = 1, i;
	size_t	num_valid, num_found = 0;
	uint8_t	flags = 0;

	do {
		if (table[flags][0] == 0)
			continue;

		num_found++;

		for (i = 0; i < TLS13_NUM_MESSAGE_TYPES; i++) {
			if (table[flags][i] != handshakes[flags][i]) {
				printf("incorrect entry %d of handshake ", i);
				print_flags(flags);
				printf("\n");
				success = 0;
			}
		}

		if (print)
			print_entry(table[flags], flags);
	} while(++flags != 0);

	num_valid = count_handshakes();
	if (num_valid != num_found) {
		printf("incorrect number of handshakes: want %zu, got %zu.\n",
		    num_valid, num_found);
		success = 0;
	}

	return success;
}

__dead void
usage(void)
{
	fprintf(stderr, "usage: handshake_table [-C]\n");
	exit(1);
}

int
main(int argc, char *argv[])
{
	static enum tls13_message_type
	    hs_table[UINT8_MAX][TLS13_NUM_MESSAGE_TYPES] = {
		[INITIAL] = {
			CLIENT_HELLO,
			SERVER_HELLO,
		},
	};
	struct child	start = {
		CLIENT_HELLO, DEFAULT, 0, 0,
	};
	struct child	end = {
		APPLICATION_DATA, DEFAULT, 0, 0,
	};
	struct child	path[TLS13_NUM_MESSAGE_TYPES] = {{0}};
	uint8_t		flags = NEGOTIATED;
	unsigned int	depth = 0;
	int		ch, print = 0;

	while ((ch = getopt(argc, argv, "C")) != -1) {
		switch (ch) {
		case 'C':
			print = 1;
			break;
		default:
			usage();
		}
	}
	argc -= optind;
	argv += optind;

	if (argc != 0)
		usage();

	build_table(hs_table, start, end, path, flags, depth);
	if (!verify_table(hs_table, print))
		return 1;

	if (!print)
		printf("SUCCESS\n");

	return 0;
}
