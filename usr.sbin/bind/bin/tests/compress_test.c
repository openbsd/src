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

/* $ISC: compress_test.c,v 1.24 2001/01/09 21:40:56 bwelling Exp $ */

#include <config.h>

#include <stdlib.h>
#include <string.h>

#include <isc/buffer.h>
#include <isc/commandline.h>
#include <isc/mem.h>
#include <isc/util.h>

#include <dns/compress.h>
#include <dns/name.h>

unsigned char plain1[] = "\003yyy\003foo";
unsigned char plain2[] = "\003bar\003yyy\003foo";
unsigned char plain3[] = "\003xxx\003bar\003foo";
unsigned char plain[] = "\003yyy\003foo\0\003bar\003yyy\003foo\0\003"
			"bar\003yyy\003foo\0\003xxx\003bar\003foo";

/*
 * Result concatenate (plain1, plain2, plain2, plain3).
 */
unsigned char bit1[] = "\101\010b";
unsigned char bit2[] = "\101\014b\260";
unsigned char bit3[] = "\101\020b\264";
unsigned char bit[] = "\101\010b\0\101\014b\260\0\101\014b\260\0\101\020b\264";

int raw = 0;
int verbose = 0;

void
test(unsigned int, dns_name_t *, dns_name_t *, dns_name_t *,
     unsigned char *, unsigned int);

int
main(int argc, char *argv[]) {
	dns_name_t name1;
	dns_name_t name2;
	dns_name_t name3;
	isc_region_t region;
	int c;

	while ((c = isc_commandline_parse(argc, argv, "rv")) != -1) {
		switch (c) {
		case 'r':
			raw++;
			break;
		case 'v':
			verbose++;
			break;
		}
	}

	dns_name_init(&name1, NULL);
	region.base = plain1;
	region.length = sizeof plain1;
	dns_name_fromregion(&name1, &region);

	dns_name_init(&name2, NULL);
	region.base = plain2;
	region.length = sizeof plain2;
	dns_name_fromregion(&name2, &region);

	dns_name_init(&name3, NULL);
	region.base = plain3;
	region.length = sizeof plain3;
	dns_name_fromregion(&name3, &region);

	test(DNS_COMPRESS_NONE, &name1, &name2, &name3, plain, sizeof plain);
	test(DNS_COMPRESS_GLOBAL14, &name1, &name2, &name3, plain,
	     sizeof plain);
	test(DNS_COMPRESS_ALL, &name1, &name2, &name3, plain, sizeof plain);

	dns_name_init(&name1, NULL);
	region.base = bit1;
	region.length = sizeof bit1;
	dns_name_fromregion(&name1, &region);

	dns_name_init(&name2, NULL);
	region.base = bit2;
	region.length = sizeof bit2;
	dns_name_fromregion(&name2, &region);

	dns_name_init(&name3, NULL);
	region.base = bit3;
	region.length = sizeof bit3;
	dns_name_fromregion(&name3, &region);

	test(DNS_COMPRESS_NONE, &name1, &name2, &name3, bit, sizeof bit);
	test(DNS_COMPRESS_GLOBAL14, &name1, &name2, &name3, bit, sizeof bit);
	test(DNS_COMPRESS_ALL, &name1, &name2, &name3, bit, sizeof bit);

	return (0);
}

void
test(unsigned int allowed, dns_name_t *name1, dns_name_t *name2,
     dns_name_t *name3, unsigned char *result, unsigned int length)
{
	isc_mem_t *mctx = NULL;
	dns_compress_t cctx;
	dns_decompress_t dctx;
	isc_buffer_t source;
	isc_buffer_t target;
	dns_name_t name;
	unsigned char buf1[1024];
	unsigned char buf2[1024];

	if (verbose) {
		const char *s;
		switch (allowed) {
		case DNS_COMPRESS_NONE: s = "DNS_COMPRESS_NONE"; break;
		case DNS_COMPRESS_GLOBAL14: s = "DNS_COMPRESS_GLOBAL14"; break;
		/* case DNS_COMPRESS_ALL: s = "DNS_COMPRESS_ALL"; break; */
		default: s = "UNKOWN"; break;
		}
		fprintf(stdout, "Allowed = %s\n", s);
	}
	RUNTIME_CHECK(isc_mem_create(0, 0, &mctx) == ISC_R_SUCCESS);
	isc_buffer_init(&source, buf1, sizeof(buf1));
	RUNTIME_CHECK(dns_compress_init(&cctx, -1, mctx) == ISC_R_SUCCESS);

	RUNTIME_CHECK(dns_name_towire(name1, &cctx, &source) == ISC_R_SUCCESS);

	/*
	RUNTIME_CHECK(dns_compress_localinit(&cctx, name1, &source) ==
		      ISC_R_SUCCESS);
	*/
	dns_compress_setmethods(&cctx, allowed);
	RUNTIME_CHECK(dns_name_towire(name2, &cctx, &source) == ISC_R_SUCCESS);
	RUNTIME_CHECK(dns_name_towire(name2, &cctx, &source) == ISC_R_SUCCESS);
	RUNTIME_CHECK(dns_name_towire(name3, &cctx, &source) == ISC_R_SUCCESS);

	/*
	dns_compress_localinvalidate(&cctx);
	*/
	dns_compress_rollback(&cctx, 0);	/* testing only */
	dns_compress_invalidate(&cctx);

	if (raw) {
		unsigned int i;
		for (i = 0 ; i < source.used ; /* */ ) {
			fprintf(stdout, "%02x",
				((unsigned char *)source.base)[i]);
			if ((++i % 20) == 0)
				fputs("\n", stdout);
			else
				if (i == source.used)
					fputs("\n", stdout);
				else
					fputs(" ", stdout);
		}
	}

	isc_buffer_setactive(&source, source.used);
	isc_buffer_init(&target, buf2, sizeof(buf2));
	dns_decompress_init(&dctx, -1, DNS_DECOMPRESS_STRICT);

	dns_name_init(&name, NULL);
	RUNTIME_CHECK(dns_name_fromwire(&name, &source, &dctx, ISC_FALSE,
					&target) == ISC_R_SUCCESS);
	dns_decompress_setmethods(&dctx, allowed);
	/*
	dns_decompress_localinit(&dctx, &name, &source);
	*/
	RUNTIME_CHECK(dns_name_fromwire(&name, &source, &dctx, ISC_FALSE,
					&target) == ISC_R_SUCCESS);
	RUNTIME_CHECK(dns_name_fromwire(&name, &source, &dctx, ISC_FALSE,
					&target) == ISC_R_SUCCESS);
	RUNTIME_CHECK(dns_name_fromwire(&name, &source, &dctx, ISC_FALSE,
					&target) == ISC_R_SUCCESS);
	/*
	dns_decompress_localinvalidate(&dctx);
	*/
	dns_decompress_invalidate(&dctx);

	if (raw) {
		unsigned int i;
		for (i = 0 ; i < target.used ; /* */ ) {
			fprintf(stdout, "%02x",
				((unsigned char *)target.base)[i]);
			if ((++i % 20) == 0)
				fputs("\n", stdout);
			else
				if (i == target.used)
					fputs("\n", stdout);
				else
					fputs(" ", stdout);
		}
		fputs("\n", stdout);
		fflush(stdout);
	}

	RUNTIME_CHECK(target.used == length);
	RUNTIME_CHECK(memcmp(target.base, result, target.used) == 0);
	isc_mem_destroy(&mctx);
}
