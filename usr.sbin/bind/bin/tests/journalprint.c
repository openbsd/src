/*
 * Copyright (C) 2000, 2001  Internet Software Consortium.
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

/* $ISC: journalprint.c,v 1.3 2001/01/09 21:41:10 bwelling Exp $ */

#include <isc/mem.h>
#include <isc/util.h>

#include <dns/journal.h>
#include <dns/types.h>

#include <stdlib.h>

int
main(int argc, char **argv) {
	char *file;
	isc_mem_t *mctx = NULL;

	if (argc != 2) {
		printf("usage: %s journal", argv[0]);
		exit(1);
	}

	file = argv[1];

	RUNTIME_CHECK(isc_mem_create(0, 0, &mctx) == ISC_R_SUCCESS);

	RUNTIME_CHECK(dns_journal_print(mctx, file, stdout) == ISC_R_SUCCESS);
	isc_mem_detach(&mctx);
	exit(0);
}
