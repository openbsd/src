/*
 * tsig-openssl.h -- Interface to OpenSSL for TSIG support.
 *
 * Copyright (c) 2001-2006, NLnet Labs. All rights reserved.
 *
 * See LICENSE for the license.
 *
 */

#include "config.h"

#if defined(HAVE_SSL)

#include "tsig-openssl.h"
#include "tsig.h"
#include "util.h"

static void *create_context(region_type *region);
static void init_context(void *context,
			 tsig_algorithm_type *algorithm,
			 tsig_key_type *key);
static void update(void *context, const void *data, size_t size);
static void final(void *context, uint8_t *digest, size_t *size);

static int
tsig_openssl_init_algorithm(region_type* region,
	const char* digest, const char* name, const char* wireformat)
{
	tsig_algorithm_type* algorithm;
	const EVP_MD *hmac_algorithm;

	hmac_algorithm = EVP_get_digestbyname(digest);
	if (!hmac_algorithm) {
		/* skip but don't error */
		return 0;
	}

	algorithm = (tsig_algorithm_type *) region_alloc(
		region, sizeof(tsig_algorithm_type));
	algorithm->short_name = name;
	algorithm->wireformat_name
		= dname_parse(region, wireformat);
	if (!algorithm->wireformat_name) {
		log_msg(LOG_ERR, "cannot parse %s algorithm", wireformat);
		return 0;
	}
	algorithm->maximum_digest_size = EVP_MAX_MD_SIZE;
	algorithm->data = hmac_algorithm;
	algorithm->hmac_create_context = create_context;
	algorithm->hmac_init_context = init_context;
	algorithm->hmac_update = update;
	algorithm->hmac_final = final;
	tsig_add_algorithm(algorithm);

	return 1;
}

int
tsig_openssl_init(region_type *region)
{
	int count = 0;
	OpenSSL_add_all_digests();

	count += tsig_openssl_init_algorithm(region,
	    "md5", "hmac-md5","hmac-md5.sig-alg.reg.int.");
	count += tsig_openssl_init_algorithm(region,
	    "sha1", "hmac-sha1", "hmac-sha1.");
	count += tsig_openssl_init_algorithm(region,
	    "sha224", "hmac-sha224", "hmac-sha224.");
	count += tsig_openssl_init_algorithm(region,
	    "sha256", "hmac-sha256", "hmac-sha256.");
	count += tsig_openssl_init_algorithm(region,
	    "sha384", "hmac-sha384", "hmac-sha384.");
	count += tsig_openssl_init_algorithm(region,
	    "sha512", "hmac-sha512", "hmac-sha512.");

	return count;
}

static void
cleanup_context(void *data)
{
	HMAC_CTX *context = (HMAC_CTX *) data;
	HMAC_CTX_cleanup(context);
}

static void *
create_context(region_type *region)
{
	HMAC_CTX *context
		= (HMAC_CTX *) region_alloc(region, sizeof(HMAC_CTX));
	region_add_cleanup(region, cleanup_context, context);
	HMAC_CTX_init(context);
	return context;
}

static void
init_context(void *context,
			  tsig_algorithm_type *algorithm,
			  tsig_key_type *key)
{
	HMAC_CTX *ctx = (HMAC_CTX *) context;
	const EVP_MD *md = (const EVP_MD *) algorithm->data;
	HMAC_Init_ex(ctx, key->data, key->size, md, NULL);
}

static void
update(void *context, const void *data, size_t size)
{
	HMAC_CTX *ctx = (HMAC_CTX *) context;
	HMAC_Update(ctx, (unsigned char *) data, (int) size);
}

static void
final(void *context, uint8_t *digest, size_t *size)
{
	HMAC_CTX *ctx = (HMAC_CTX *) context;
	unsigned len = (unsigned) *size;
	HMAC_Final(ctx, digest, &len);
	*size = (size_t) len;
}

void
tsig_openssl_finalize()
{
	EVP_cleanup();
}

#endif /* defined(HAVE_SSL) */
