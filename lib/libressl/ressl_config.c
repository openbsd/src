/* $OpenBSD: ressl_config.c,v 1.12 2014/09/29 15:11:29 jsing Exp $ */
/*
 * Copyright (c) 2014 Joel Sing <jsing@openbsd.org>
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

#include <errno.h>
#include <stdlib.h>

#include <ressl.h>
#include "ressl_internal.h"

static int
set_string(const char **dest, const char *src)
{
	free((char *)*dest);
	*dest = NULL;
	if (src != NULL)
		if ((*dest = strdup(src)) == NULL)
			return -1;
	return 0;
}

static void *
memdup(const void *in, size_t len)
{
	void *out;

	if ((out = malloc(len)) == NULL)
		return NULL;
	memcpy(out, in, len);
	return out;
}

static int
set_mem(char **dest, size_t *destlen, const void *src, size_t srclen)
{
	free(*dest);
	*dest = NULL;
	*destlen = 0;
	if (src != NULL)
		if ((*dest = memdup(src, srclen)) == NULL)
			return -1;
	*destlen = srclen;
	return 0;
}

struct ressl_config *
ressl_config_new(void)
{
	struct ressl_config *config;

	if ((config = calloc(1, sizeof(*config))) == NULL)
		return (NULL);

	/*
	 * Default configuration.
	 */
	if (ressl_config_set_ca_file(config, _PATH_SSL_CA_FILE) != 0) {
		ressl_config_free(config);
		return (NULL);
	}
	ressl_config_set_protocols(config, RESSL_PROTOCOLS_DEFAULT);
	ressl_config_set_verify_depth(config, 6);
	/* ? use function ? */
	config->ecdhcurve = NID_X9_62_prime256v1;
	
	ressl_config_verify(config);

	return (config);
}

void
ressl_config_free(struct ressl_config *config)
{
	if (config == NULL)
		return;

	ressl_config_clear_keys(config);

	free((char *)config->ca_file);
	free((char *)config->ca_path);
	free((char *)config->cert_file);
	free(config->cert_mem);
	free((char *)config->ciphers);
	free((char *)config->key_file);
	free(config->key_mem);

	free(config);
}

void
ressl_config_clear_keys(struct ressl_config *config)
{
	ressl_config_set_cert_mem(config, NULL, 0);
	ressl_config_set_key_mem(config, NULL, 0);
}

int
ressl_config_set_ca_file(struct ressl_config *config, const char *ca_file)
{
	return set_string(&config->ca_file, ca_file);
}

int
ressl_config_set_ca_path(struct ressl_config *config, const char *ca_path)
{
	return set_string(&config->ca_path, ca_path);
}

int
ressl_config_set_cert_file(struct ressl_config *config, const char *cert_file)
{
	return set_string(&config->cert_file, cert_file);
}

int
ressl_config_set_cert_mem(struct ressl_config *config, const uint8_t *cert,
    size_t len)
{
	return set_mem(&config->cert_mem, &config->cert_len, cert, len);
}

int
ressl_config_set_ciphers(struct ressl_config *config, const char *ciphers)
{
	return set_string(&config->ciphers, ciphers);
}

int
ressl_config_set_ecdhcurve(struct ressl_config *config, const char *name)
{
	int nid = NID_undef;

	if (name != NULL && (nid = OBJ_txt2nid(name)) == NID_undef)
		return (-1);

	config->ecdhcurve = nid;
	return (0);
}

int
ressl_config_set_key_file(struct ressl_config *config, const char *key_file)
{
	return set_string(&config->key_file, key_file);
}

int
ressl_config_set_key_mem(struct ressl_config *config, const uint8_t *key,
    size_t len)
{
	if (config->key_mem)
		explicit_bzero(config->key_mem, config->key_len);
	return set_mem(&config->key_mem, &config->key_len, key, len);
}

void
ressl_config_set_protocols(struct ressl_config *config, uint32_t protocols)
{
	config->protocols = protocols;
}

void
ressl_config_set_verify_depth(struct ressl_config *config, int verify_depth)
{
	config->verify_depth = verify_depth;
}

void
ressl_config_insecure_no_verify(struct ressl_config *config)
{
	config->verify = 0;
}

void
ressl_config_verify(struct ressl_config *config)
{
	config->verify = 1;
}
