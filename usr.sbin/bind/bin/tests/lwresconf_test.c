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

/* $ISC: lwresconf_test.c,v 1.10 2001/01/09 21:41:17 bwelling Exp $ */

#include <config.h>

#include <stdlib.h>

#include <isc/mem.h>
#include <isc/util.h>

#include <lwres/lwres.h>

#define USE_ISC_MEM

static inline void
CHECK(int val, const char *msg) {
	if (val != 0) {
		fprintf(stderr, "%s returned %d\n", msg, val);
		exit(1);
	}
}

#ifdef USE_ISC_MEM
/*
 * Wrappers around our memory management stuff, for the lwres functions.
 */
static void *
mem_alloc(void *arg, size_t size) {
	return (isc_mem_get(arg, size));
}

static void
mem_free(void *arg, void *mem, size_t size) {
	isc_mem_put(arg, mem, size);
}
#endif

int
main(int argc, char *argv[]) {
	lwres_context_t *ctx;
	const char *file = "/etc/resolv.conf";
	int ret;
#ifdef USE_ISC_MEM
	isc_mem_t *mem;
	isc_result_t result;
#endif

	if (argc > 1) {
		file = argv[1];
	}

#ifdef USE_ISC_MEM
	mem = NULL;
	result = isc_mem_create(0, 0, &mem);
	INSIST(result == ISC_R_SUCCESS);
#endif

	ctx = NULL;
#ifdef USE_ISC_MEM
	ret = lwres_context_create(&ctx, mem, mem_alloc, mem_free, 0);
#else
	ret = lwres_context_create(&ctx, NULL, NULL, NULL, 0);
#endif
	CHECK(ret, "lwres_context_create");

	lwres_conf_init(ctx);
	if (lwres_conf_parse(ctx, file) == 0) {
		lwres_conf_print(ctx, stderr);
	} else {
		perror("lwres_conf_parse");
	}

	lwres_conf_clear(ctx);
	lwres_context_destroy(&ctx);

#ifdef USE_ISC_MEM
	isc_mem_stats(mem, stdout);
	isc_mem_destroy(&mem);
#endif

	return (0);
}
