/*
 * Copyright (C) 1999-2001  Internet Software Consortium.
 *
 * Permission to use, copy, modify, and distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND INTERNET SOFTWARE CONSORTIUM
 * DISCLAIMS ALL WARRANTIES WITH REGARD TO THIS SOFTWARE INCLUDING ALL
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL
 * INTERNET SOFTWARE CONSORTIUM BE LIABLE FOR ANY SPECIAL, DIRECT,
 * INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING
 * FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN ACTION OF CONTRACT,
 * NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF OR IN CONNECTION
 * WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */

/* $ISC: wire_test.c,v 1.60 2001/05/09 18:59:55 bwelling Exp $ */

#include <config.h>

#include <stdlib.h>

#include <isc/buffer.h>
#include <isc/commandline.h>
#include <isc/mem.h>
#include <isc/string.h>
#include <isc/util.h>

#include <dns/result.h>

#include "printmsg.h"

static inline void
CHECKRESULT(isc_result_t result, const char *msg) {
	if (result != ISC_R_SUCCESS) {
		printf("%s: %s\n", msg, dns_result_totext(result));

		exit(1);
	}
}

static int
fromhex(char c) {
	if (c >= '0' && c <= '9')
		return (c - '0');
	else if (c >= 'a' && c <= 'f')
		return (c - 'a' + 10);
	else if (c >= 'A' && c <= 'F')
		return (c - 'A' + 10);

	printf("bad input format: %02x\n", c);
	exit(3);
	/* NOTREACHED */
}

static void
usage(void) {
	fprintf(stderr, "wire_test [-p] [-b] [-s] [-r]\n");
	fprintf(stderr, "\t-p\tPreserve order of the records in messages\n");
	fprintf(stderr, "\t-b\tBest-effort parsing (ignore some errors)\n");
	fprintf(stderr, "\t-s\tPrint memory statistics\n");
	fprintf(stderr, "\t-r\tAfter parsing, re-render the message\n");
	fprintf(stderr, "\t-t\tTCP mode - ignore the first 2 bytes\n");
}

int
main(int argc, char *argv[]) {
	char *rp, *wp;
	unsigned char *bp;
	isc_buffer_t source;
	size_t len, i;
	int n;
	FILE *f;
	isc_boolean_t need_close = ISC_FALSE;
	unsigned char b[64 * 1024];
	char s[4000];
	dns_message_t *message;
	isc_result_t result;
	dns_compress_t cctx;
	isc_mem_t *mctx;
	int parseflags = 0;
	isc_boolean_t printmemstats = ISC_FALSE;
	isc_boolean_t dorender = ISC_FALSE;
	isc_boolean_t tcp = ISC_FALSE;
	int ch;

	mctx = NULL;
	RUNTIME_CHECK(isc_mem_create(0, 0, &mctx) == ISC_R_SUCCESS);

	while ((ch = isc_commandline_parse(argc, argv, "pbsrt")) != -1) {
		switch (ch) {
			case 'p':
				parseflags |= DNS_MESSAGEPARSE_PRESERVEORDER;
				break;
			case 'b':
				parseflags |= DNS_MESSAGEPARSE_BESTEFFORT;
				break;
			case 's':
				printmemstats = ISC_TRUE;
				break;
			case 'r':
				dorender = ISC_TRUE;
				break;
			case 't':
				tcp = ISC_TRUE;
				break;
			default:
				usage();
				exit(1);
		}
	}

	argc -= isc_commandline_index;
	argv += isc_commandline_index;

	if (argc > 1) {
		f = fopen(argv[1], "r");
		if (f == NULL) {
			printf("fopen failed\n");
			exit(1);
		}
		need_close = ISC_TRUE;
	} else
		f = stdin;

	bp = b;
	while (fgets(s, sizeof s, f) != NULL) {
		rp = s;
		wp = s;
		len = 0;
		while (*rp != '\0') {
			if (*rp == '#')
				break;
			if (*rp != ' ' && *rp != '\t' &&
			    *rp != '\r' && *rp != '\n') {
				*wp++ = *rp;
				len++;
			}
			rp++;
		}
		if (len == 0)
			break;
		if (len % 2 != 0) {
			printf("bad input format: %d\n", len);
			exit(1);
		}
		if (len > (sizeof b) * 2) {
			printf("input too long\n");
			exit(2);
		}
		rp = s;
		for (i = 0; i < len; i += 2) {
			n = fromhex(*rp++);
			n *= 16;
			n += fromhex(*rp++);
			*bp++ = n;
		}
	}

	if (need_close)
		fclose(f);

	if (tcp) {
		isc_buffer_init(&source, b + 2, sizeof(b) - 2);
		isc_buffer_add(&source, bp - b - 2);
	} else {
		isc_buffer_init(&source, b, sizeof(b));
		isc_buffer_add(&source, bp - b);
	}

	message = NULL;
	result = dns_message_create(mctx, DNS_MESSAGE_INTENTPARSE, &message);
	CHECKRESULT(result, "dns_message_create failed");

	result = dns_message_parse(message, &source, parseflags);
	if (result == DNS_R_RECOVERABLE)
		result = ISC_R_SUCCESS;
	CHECKRESULT(result, "dns_message_parse failed");

	result = printmessage(message);
	CHECKRESULT(result, "printmessage() failed");

	if (printmemstats)
		isc_mem_stats(mctx, stdout);

	if (dorender) {
		/*
		 * XXXMLG
		 * Changing this here is a hack, and should not be done in
		 * reasonable application code, ever.
	 	*/
		message->from_to_wire = DNS_MESSAGE_INTENTRENDER;
		memset(b, 0, sizeof(b));
		isc_buffer_clear(&source);

		for (i = 0 ; i < DNS_SECTION_MAX ; i++)
			message->counts[i] = 0;  /* Another hack XXX */

		result = dns_compress_init(&cctx, -1, mctx);
		CHECKRESULT(result, "dns_compress_init() failed");

		result = dns_message_renderbegin(message, &cctx, &source);
		CHECKRESULT(result, "dns_message_renderbegin() failed");

		result = dns_message_rendersection(message,
						   DNS_SECTION_QUESTION, 0);
		CHECKRESULT(result,
			    "dns_message_rendersection(QUESTION) failed");

		result = dns_message_rendersection(message,
						   DNS_SECTION_ANSWER, 0);
		CHECKRESULT(result,
			    "dns_message_rendersection(ANSWER) failed");

		result = dns_message_rendersection(message,
						   DNS_SECTION_AUTHORITY, 0);
		CHECKRESULT(result,
			    "dns_message_rendersection(AUTHORITY) failed");

		result = dns_message_rendersection(message,
						   DNS_SECTION_ADDITIONAL, 0);
		CHECKRESULT(result,
			    "dns_message_rendersection(ADDITIONAL) failed");

		dns_message_renderend(message);

		dns_compress_invalidate(&cctx);

		message->from_to_wire = DNS_MESSAGE_INTENTPARSE;
		dns_message_destroy(&message);

		printf("Message rendered.\n");
		if (printmemstats)
			isc_mem_stats(mctx, stdout);

		result = dns_message_create(mctx, DNS_MESSAGE_INTENTPARSE,
					    &message);
		CHECKRESULT(result, "dns_message_create failed");

		result = dns_message_parse(message, &source, parseflags);
		CHECKRESULT(result, "dns_message_parse failed");

		result = printmessage(message);
		CHECKRESULT(result, "printmessage() failed");
	}

	dns_message_destroy(&message);

	if (printmemstats)
		isc_mem_stats(mctx, stdout);
	isc_mem_destroy(&mctx);

	return (0);
}
