/* $OpenBSD: dhtest.c,v 1.2 2018/06/08 17:28:36 jsing Exp $ */
/*
 * Copyright (c) 2018 Joel Sing <jsing@openbsd.org>
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
#include <stdio.h>
#include <string.h>

#include <csi.h>

struct dh_params_test {
	const char *name;
	struct csi_dh_params *(*func)(void);
};

struct dh_params_test dh_params_tests[] = {
	{
		.name = "modp group1",
		.func = csi_dh_params_modp_group1,
	},
	{
		.name = "modp group2",
		.func = csi_dh_params_modp_group2,
	},
	{
		.name = "modp group5",
		.func = csi_dh_params_modp_group5,
	},
	{
		.name = "modp group14",
		.func = csi_dh_params_modp_group14,
	},
	{
		.name = "modp group15",
		.func = csi_dh_params_modp_group15,
	},
	{
		.name = "modp group16",
		.func = csi_dh_params_modp_group16,
	},
	{
		.name = "modp group17",
		.func = csi_dh_params_modp_group17,
	},
	{
		.name = "modp group18",
		.func = csi_dh_params_modp_group18,
	},
};

#define N_DH_PARAMS_TESTS \
    (sizeof(dh_params_tests) / sizeof(*dh_params_tests))

static int
dh_params_test(void)
{
	struct csi_dh_params *params;
	size_t i;

	for (i = 0; i < N_DH_PARAMS_TESTS; i++) {
		if ((params = dh_params_tests[i].func()) == NULL) {
			fprintf(stderr, "FAIL: %s params failed\n",
			    dh_params_tests[i].name);
			return 1;
		}
		csi_dh_params_free(params);
	}

	return 0;
}

static int
dh_generate_keys_test(void)
{
	return 0;
}

static int
dh_peer_public_test(void)
{
	uint8_t data[] = {0x01, 0x00, 0x01};
	struct csi_dh_params *params;
	struct csi_dh_public public;
	struct csi_dh *cdh;
	int failed = 1;

	if ((cdh = csi_dh_new()) == NULL)
		errx(1, "out of memory");
	if ((params = csi_dh_params_modp_group1()) == NULL)
		errx(1, "out of memory");

	if (csi_dh_set_params(cdh, params) == -1) {
		fprintf(stderr, "FAIL: failed to set dh params: %s\n",
		    csi_dh_error(cdh));
		goto fail;
	}

	public.key.data = data;
	public.key.len = sizeof(data);

	if (csi_dh_set_peer_public(cdh, &public) != -1) {
		fprintf(stderr, "FAIL: successfully set public key, "
		    "should have failed!\n");
		goto fail;
	}

	failed = 0;

 fail:
	csi_dh_params_free(params);
	csi_dh_free(cdh);

	return failed;
}

static int
dh_kex_test(void)
{
	struct csi_dh_public *client_public = NULL, *server_public = NULL;
	struct csi_dh_shared *client_shared = NULL, *server_shared = NULL;
	struct csi_dh *client = NULL, *server = NULL;
	struct csi_dh_params *params;
	int failed = 1;

	if ((client = csi_dh_new()) == NULL)
		errx(1, "out of memory");
	if ((server = csi_dh_new()) == NULL)
		errx(1, "out of memory");

	params = csi_dh_params_modp_group2();

	if (csi_dh_set_params(client, params) == -1) {
		fprintf(stderr, "FAIL: failed to set client params: %s\n",
		    csi_dh_error(client));
		goto fail;
	}
	if (csi_dh_set_params(server, params) == -1) {
		fprintf(stderr, "FAIL: failed to set server params: %s\n",
		    csi_dh_error(server));
		goto fail;
	}

	if (csi_dh_generate_keys(client, 0, &client_public) == -1) {
		fprintf(stderr, "FAIL: failed to generate client keys: %s\n",
		    csi_dh_error(client));
		goto fail;
	}
	if (csi_dh_generate_keys(server, 0, &server_public) == -1) {
		fprintf(stderr, "FAIL: failed to generate server keys: %s\n",
		    csi_dh_error(server));
		goto fail;
	}

	if (csi_dh_set_peer_public(client, server_public) == -1) {
		fprintf(stderr, "FAIL: failed to set client peer public: %s\n",
		    csi_dh_error(client));
		goto fail;
	}
	if (csi_dh_set_peer_public(server, client_public) == -1) {
		fprintf(stderr, "FAIL: failed to set server peer public: %s\n",
		    csi_dh_error(server));
		goto fail;
	}

	if (csi_dh_derive_shared_key(client, &client_shared) == -1) {
		fprintf(stderr, "FAIL: failed to derive client shared key: %s\n",
		    csi_dh_error(client));
		goto fail;
	}
	if (csi_dh_derive_shared_key(server, &server_shared) == -1) {
		fprintf(stderr, "FAIL: failed to derive server shared key: %s\n",
		    csi_dh_error(server));
		goto fail;
	}

	if (client_shared->key.len != server_shared->key.len) {
		fprintf(stderr, "FAIL: shared key lengths differ (%zu != %zu)\n",
		    client_shared->key.len, server_shared->key.len);
		goto fail;
	}
	if (memcmp(client_shared->key.data, server_shared->key.data,
	    client_shared->key.len) != 0) {
		fprintf(stderr, "FAIL: shared keys differ\n");
		goto fail;
	}

	failed = 0;

 fail:
	csi_dh_params_free(params);
	csi_dh_free(client);
	csi_dh_free(server);
	csi_dh_public_free(client_public);
	csi_dh_public_free(server_public);
	csi_dh_shared_free(client_shared);
	csi_dh_shared_free(server_shared);

	return failed;
}

int
main(int argc, char **argv)
{
	int failed = 0;

	failed |= dh_params_test();
	failed |= dh_generate_keys_test();
	failed |= dh_peer_public_test();
	failed |= dh_kex_test();

	return failed;
}
