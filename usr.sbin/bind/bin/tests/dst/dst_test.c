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

/* $ISC: dst_test.c,v 1.37 2001/04/04 02:02:50 bwelling Exp $ */

#include <config.h>

#include <stdlib.h>

#include <unistd.h>		/* XXX */

#include <isc/buffer.h>
#include <isc/entropy.h>
#include <isc/mem.h>
#include <isc/region.h>
#include <isc/string.h>		/* Required for HP/UX (and others?) */

#include <dns/fixedname.h>
#include <dns/name.h>
#include <dns/result.h>

#include <dst/dst.h>
#include <dst/result.h>

char *current;
const char *tmp = "/tmp";

static void
use(dst_key_t *key, isc_mem_t *mctx) {
	isc_result_t ret;
	const char *data = "This is some data";
	unsigned char sig[512];
	isc_buffer_t databuf, sigbuf;
	isc_region_t datareg, sigreg;
	dst_context_t *ctx = NULL;

	isc_buffer_init(&sigbuf, sig, sizeof(sig));
	/*
	 * Advance 1 byte for fun.
	 */
	isc_buffer_add(&sigbuf, 1);

	isc_buffer_init(&databuf, data, strlen(data));
	isc_buffer_add(&databuf, strlen(data));
	isc_buffer_usedregion(&databuf, &datareg);

	ret = dst_context_create(key, mctx, &ctx);
	if (ret != ISC_R_SUCCESS) {
		printf("contextcreate(%d) returned: %s\n", dst_key_alg(key),
		       isc_result_totext(ret));
		return;
	}
	ret = dst_context_adddata(ctx, &datareg);
	if (ret != ISC_R_SUCCESS) {
		printf("adddata(%d) returned: %s\n", dst_key_alg(key),
		       isc_result_totext(ret));
		dst_context_destroy(&ctx);
		return;
	}
	ret = dst_context_sign(ctx, &sigbuf);
	printf("sign(%d) returned: %s\n", dst_key_alg(key),
	       isc_result_totext(ret));
	dst_context_destroy(&ctx);

	isc_buffer_forward(&sigbuf, 1);
	isc_buffer_remainingregion(&sigbuf, &sigreg);
	ret = dst_context_create(key, mctx, &ctx);
	if (ret != ISC_R_SUCCESS) {
		printf("contextcreate(%d) returned: %s\n", dst_key_alg(key),
		       isc_result_totext(ret));
		return;
	}
	ret = dst_context_adddata(ctx, &datareg);
	if (ret != ISC_R_SUCCESS) {
		printf("adddata(%d) returned: %s\n", dst_key_alg(key),
		       isc_result_totext(ret));
		dst_context_destroy(&ctx);
		return;
	}
	ret = dst_context_verify(ctx, &sigreg);
	printf("verify(%d) returned: %s\n", dst_key_alg(key),
	       isc_result_totext(ret));
	dst_context_destroy(&ctx);
}

static void
dns(dst_key_t *key, isc_mem_t *mctx) {
	unsigned char buffer1[2048];
	unsigned char buffer2[2048];
	isc_buffer_t buf1, buf2;
	isc_region_t r1, r2;
	dst_key_t *newkey = NULL;
	isc_result_t ret;
	isc_boolean_t match;

	isc_buffer_init(&buf1, buffer1, sizeof(buffer1));
	ret = dst_key_todns(key, &buf1);
	printf("todns(%d) returned: %s\n", dst_key_alg(key),
	       isc_result_totext(ret));
	if (ret != ISC_R_SUCCESS)
		return;
	ret = dst_key_fromdns(dst_key_name(key), dns_rdataclass_in,
			      &buf1, mctx, &newkey);
	printf("fromdns(%d) returned: %s\n", dst_key_alg(key),
	       isc_result_totext(ret));
	if (ret != ISC_R_SUCCESS)
		return;
	isc_buffer_init(&buf2, buffer2, sizeof(buffer2));
	ret = dst_key_todns(newkey, &buf2);
	printf("todns2(%d) returned: %s\n", dst_key_alg(key),
	       isc_result_totext(ret));
	if (ret != ISC_R_SUCCESS)
		return;
	isc_buffer_usedregion(&buf1, &r1);
	isc_buffer_usedregion(&buf2, &r2);
	match = ISC_TF(r1.length == r2.length &&
		       memcmp(r1.base, r2.base, r1.length) == 0);
	printf("compare(%d): %s\n", dst_key_alg(key),
	       match ? "true" : "false");
	dst_key_free(&newkey);
}

static void
io(dns_name_t *name, int id, int alg, int type, isc_mem_t *mctx) {
	dst_key_t *key = NULL;
	isc_result_t ret;

	ret = dst_key_fromfile(name, id, alg, type, current, mctx, &key);
	printf("read(%d) returned: %s\n", alg, isc_result_totext(ret));
	if (ret != 0)
		return;
	ret = dst_key_tofile(key, type, tmp);
	printf("write(%d) returned: %s\n", alg, isc_result_totext(ret));
	if (ret != 0)
		return;
	use(key, mctx);
	dns(key, mctx);
	dst_key_free(&key);
}

static void
dh(dns_name_t *name1, int id1, dns_name_t *name2, int id2, isc_mem_t *mctx) {
	dst_key_t *key1 = NULL, *key2 = NULL;
	isc_result_t ret;
	isc_buffer_t b1, b2;
	isc_region_t r1, r2;
	unsigned char array1[1024], array2[1024];
	int alg = DST_ALG_DH;
	int type = DST_TYPE_PUBLIC|DST_TYPE_PRIVATE;

	ret = dst_key_fromfile(name1, id1, alg, type, current, mctx, &key1);
	printf("read(%d) returned: %s\n", alg, isc_result_totext(ret));
	if (ret != 0)
		return;
	ret = dst_key_fromfile(name2, id2, alg, type, current, mctx, &key2);
	printf("read(%d) returned: %s\n", alg, isc_result_totext(ret));
	if (ret != 0)
		return;

	ret = dst_key_tofile(key1, type, tmp);
	printf("write(%d) returned: %s\n", alg, isc_result_totext(ret));
	if (ret != 0)
		return;
	ret = dst_key_tofile(key2, type, tmp);
	printf("write(%d) returned: %s\n", alg, isc_result_totext(ret));
	if (ret != 0)
		return;

	isc_buffer_init(&b1, array1, sizeof(array1));
	ret = dst_key_computesecret(key1, key2, &b1);
	printf("computesecret() returned: %s\n", isc_result_totext(ret));
	if (ret != 0)
		return;

	isc_buffer_init(&b2, array2, sizeof(array2));
	ret = dst_key_computesecret(key2, key1, &b2);
	printf("computesecret() returned: %s\n", isc_result_totext(ret));
	if (ret != 0)
		return;

	isc_buffer_usedregion(&b1, &r1);
	isc_buffer_usedregion(&b2, &r2);

	if (r1.length != r2.length || memcmp(r1.base, r2.base, r1.length) != 0)
	{
		int i;
		printf("secrets don't match\n");
		printf("secret 1: %d bytes\n", r1.length);
		for (i = 0; i < (int) r1.length; i++)
			printf("%02x ", r1.base[i]);
		printf("\n");
		printf("secret 2: %d bytes\n", r2.length);
		for (i = 0; i < (int) r2.length; i++)
			printf("%02x ", r2.base[i]);
		printf("\n");
	}
	dst_key_free(&key1);
	dst_key_free(&key2);
}

static void
generate(int alg, isc_mem_t *mctx) {
	isc_result_t ret;
	dst_key_t *key = NULL;

	ret = dst_key_generate(dns_rootname, alg, 512, 0, 0, 0,
			       dns_rdataclass_in, mctx, &key);
	printf("generate(%d) returned: %s\n", alg, isc_result_totext(ret));
	if (ret != ISC_R_SUCCESS)
		return;

	if (alg != DST_ALG_DH)
		use(key, mctx);

	dst_key_free(&key);
}

int
main(void) {
	isc_mem_t *mctx = NULL;
	isc_entropy_t *ectx = NULL;
	isc_buffer_t b;
	dns_fixedname_t fname;
	dns_name_t *name;

	isc_mem_create(0, 0, &mctx);

	current = isc_mem_get(mctx, 256);
	getcwd(current, 256);

	dns_result_register();

	isc_entropy_create(mctx, &ectx);
	isc_entropy_createfilesource(ectx, "randomfile");
	dst_lib_init(mctx, ectx, ISC_ENTROPY_BLOCKING|ISC_ENTROPY_GOODONLY);

	dns_fixedname_init(&fname);
	name = dns_fixedname_name(&fname);
	isc_buffer_init(&b, "test.", 5);
	isc_buffer_add(&b, 5);
	dns_name_fromtext(name, &b, NULL, ISC_FALSE, NULL);
	io(name, 23616, DST_ALG_DSA, DST_TYPE_PRIVATE|DST_TYPE_PUBLIC, mctx);
	io(name, 54622, DST_ALG_RSAMD5, DST_TYPE_PRIVATE|DST_TYPE_PUBLIC,
	   mctx);

	io(name, 49667, DST_ALG_DSA, DST_TYPE_PRIVATE|DST_TYPE_PUBLIC, mctx);
	io(name, 2, DST_ALG_RSAMD5, DST_TYPE_PRIVATE|DST_TYPE_PUBLIC, mctx);

	isc_buffer_init(&b, "dh.", 3);
	isc_buffer_add(&b, 3);
	dns_name_fromtext(name, &b, NULL, ISC_FALSE, NULL);
	dh(name, 18602, name, 48957, mctx);

	generate(DST_ALG_RSAMD5, mctx);
	generate(DST_ALG_DH, mctx);
	generate(DST_ALG_DSA, mctx);
	generate(DST_ALG_HMACMD5, mctx);

	dst_lib_destroy();
	isc_entropy_detach(&ectx);

	isc_mem_put(mctx, current, 256);
/*	isc_mem_stats(mctx, stdout);*/
	isc_mem_destroy(&mctx);

	return (0);
}
