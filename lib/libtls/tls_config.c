/* $OpenBSD: tls_config.c,v 1.4 2015/02/11 06:46:33 jsing Exp $ */
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

#include <tls.h>
#include "tls_internal.h"

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

struct tls_config *
tls_config_new(void)
{
	struct tls_config *config;

	if ((config = calloc(1, sizeof(*config))) == NULL)
		return (NULL);

	/*
	 * Default configuration.
	 */
	if (tls_config_set_ca_file(config, _PATH_SSL_CA_FILE) != 0) {
		tls_config_free(config);
		return (NULL);
	}
	tls_config_set_dheparams(config, "none");
	tls_config_set_ecdhecurve(config, "auto");
	tls_config_set_protocols(config, TLS_PROTOCOLS_DEFAULT);
	tls_config_set_verify_depth(config, 6);
	
	tls_config_verify(config);

	return (config);
}

void
tls_config_free(struct tls_config *config)
{
	if (config == NULL)
		return;

	tls_config_clear_keys(config);

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
tls_config_clear_keys(struct tls_config *config)
{
	tls_config_set_ca_mem(config, NULL, 0);
	tls_config_set_cert_mem(config, NULL, 0);
	tls_config_set_key_mem(config, NULL, 0);
}

int
tls_config_set_ca_file(struct tls_config *config, const char *ca_file)
{
	return set_string(&config->ca_file, ca_file);
}

int
tls_config_set_ca_path(struct tls_config *config, const char *ca_path)
{
	return set_string(&config->ca_path, ca_path);
}

int
tls_config_set_ca_mem(struct tls_config *config, const uint8_t *ca, size_t len)
{
	return set_mem(&config->ca_mem, &config->ca_len, ca, len);
}

int
tls_config_set_cert_file(struct tls_config *config, const char *cert_file)
{
	return set_string(&config->cert_file, cert_file);
}

int
tls_config_set_cert_mem(struct tls_config *config, const uint8_t *cert,
    size_t len)
{
	return set_mem(&config->cert_mem, &config->cert_len, cert, len);
}

int
tls_config_set_ciphers(struct tls_config *config, const char *ciphers)
{
	return set_string(&config->ciphers, ciphers);
}

int
tls_config_set_dheparams(struct tls_config *config, const char *params)
{
	int keylen;

	if (params == NULL || strcasecmp(params, "none") == 0)
		keylen = 0;
	else if (strcasecmp(params, "auto") == 0)
		keylen = -1;
	else if (strcmp(params, "legacy"))
		keylen = 1024;
	else
		return (-1);

	config->dheparams = keylen;

	return (0);
}

int
tls_config_set_ecdhecurve(struct tls_config *config, const char *name)
{
	int nid;

	if (name == NULL || strcasecmp(name, "none") == 0)
		nid = NID_undef;
	else if (strcasecmp(name, "auto") == 0)
		nid = -1;
	else if ((nid = OBJ_txt2nid(name)) == NID_undef)
		return (-1);

	config->ecdhecurve = nid;

	return (0);
}

int
tls_config_set_key_file(struct tls_config *config, const char *key_file)
{
	return set_string(&config->key_file, key_file);
}

int
tls_config_set_key_mem(struct tls_config *config, const uint8_t *key,
    size_t len)
{
	if (config->key_mem)
		explicit_bzero(config->key_mem, config->key_len);
	return set_mem(&config->key_mem, &config->key_len, key, len);
}

void
tls_config_set_protocols(struct tls_config *config, uint32_t protocols)
{
	config->protocols = protocols;
}

void
tls_config_set_verify_depth(struct tls_config *config, int verify_depth)
{
	config->verify_depth = verify_depth;
}

/* XXX - rename to noverifyname. */
void
tls_config_insecure_noverifyhost(struct tls_config *config)
{
	config->verify_name = 0;
}

void
tls_config_insecure_noverifycert(struct tls_config *config)
{
	config->verify_cert = 0;
}

void
tls_config_verify(struct tls_config *config)
{
	config->verify_cert = 1;
	config->verify_name = 1;
}
