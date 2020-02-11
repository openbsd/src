/*
 * Portions Copyright (C) Internet Systems Consortium, Inc. ("ISC")
 *
 * Permission to use, copy, modify, and/or distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND ISC AND NETWORK ASSOCIATES DISCLAIMS
 * ALL WARRANTIES WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS.  IN NO EVENT SHALL ISC BE LIABLE
 * FOR ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 * ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF OR
 * IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 *
 * See the COPYRIGHT file distributed with this work for additional
 * information regarding copyright ownership.
 *
 * Portions Copyright (C) Network Associates, Inc.
 *
 * Permission to use, copy, modify, and/or distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND ISC AND NETWORK ASSOCIATES DISCLAIMS
 * ALL WARRANTIES WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS.  IN NO EVENT SHALL ISC BE LIABLE
 * FOR ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 * ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF OR
 * IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */

/*%
 * Principal Author: Brian Wellington
 * $Id: dst_parse.c,v 1.3 2020/02/11 23:26:11 jsg Exp $
 */



#include <isc/base64.h>
#include <isc/lex.h>
#include <isc/stdtime.h>
#include <string.h>
#include <isc/util.h>
#include <dns/time.h>

#include "dst_internal.h"
#include "dst_parse.h"
#include "dst/result.h"

#define DST_AS_STR(t) ((t).value.as_textregion.base)

#define PRIVATE_KEY_STR "Private-key-format:"
#define ALGORITHM_STR "Algorithm:"

#define TIMING_NTAGS (DST_MAX_TIMES + 1)
static const char *timetags[TIMING_NTAGS] = {
	"Created:",
	"Publish:",
	"Activate:",
	"Revoke:",
	"Inactive:",
	"Delete:",
	"DSPublish:"
};

#define NUMERIC_NTAGS (DST_MAX_NUMERIC + 1)
static const char *numerictags[NUMERIC_NTAGS] = {
	"Predecessor:",
	"Successor:",
	"MaxTTL:",
	"RollPeriod:"
};

struct parse_map {
	const int value;
	const char *tag;
};

static struct parse_map map[] = {
	{TAG_HMACSHA1_KEY, "Key:"},
	{TAG_HMACSHA1_BITS, "Bits:"},

	{TAG_HMACSHA224_KEY, "Key:"},
	{TAG_HMACSHA224_BITS, "Bits:"},

	{TAG_HMACSHA256_KEY, "Key:"},
	{TAG_HMACSHA256_BITS, "Bits:"},

	{TAG_HMACSHA384_KEY, "Key:"},
	{TAG_HMACSHA384_BITS, "Bits:"},

	{TAG_HMACSHA512_KEY, "Key:"},
	{TAG_HMACSHA512_BITS, "Bits:"},

	{0, NULL}
};

static int
find_value(const char *s, const unsigned int alg) {
	int i;

	for (i = 0; map[i].tag != NULL; i++) {
		if (strcasecmp(s, map[i].tag) == 0 &&
		    (TAG_ALG(map[i].value) == alg))
			return (map[i].value);
	}
	return (-1);
}

static int
find_metadata(const char *s, const char *tags[], int ntags) {
	int i;

	for (i = 0; i < ntags; i++) {
		if (strcasecmp(s, tags[i]) == 0)
			return (i);
	}

	return (-1);
}

static int
find_timedata(const char *s) {
	return (find_metadata(s, timetags, TIMING_NTAGS));
}

static int
find_numericdata(const char *s) {
	return (find_metadata(s, numerictags, NUMERIC_NTAGS));
}

static int
check_hmac_sha(const dst_private_t *priv, unsigned int ntags,
	       unsigned int alg)
{
	unsigned int i, j;
	if (priv->nelements != ntags)
		return (-1);
	for (i = 0; i < ntags; i++) {
		for (j = 0; j < priv->nelements; j++)
			if (priv->elements[j].tag == TAG(alg, i))
				break;
		if (j == priv->nelements)
			return (-1);
	}
	return (0);
}

static int
check_data(const dst_private_t *priv, const unsigned int alg,
	   isc_boolean_t old, isc_boolean_t external)
{
	UNUSED(old);
	/* XXXVIX this switch statement is too sparse to gen a jump table. */
	switch (alg) {
	case DST_ALG_HMACSHA1:
		return (check_hmac_sha(priv, HMACSHA1_NTAGS, alg));
	case DST_ALG_HMACSHA224:
		return (check_hmac_sha(priv, HMACSHA224_NTAGS, alg));
	case DST_ALG_HMACSHA256:
		return (check_hmac_sha(priv, HMACSHA256_NTAGS, alg));
	case DST_ALG_HMACSHA384:
		return (check_hmac_sha(priv, HMACSHA384_NTAGS, alg));
	case DST_ALG_HMACSHA512:
		return (check_hmac_sha(priv, HMACSHA512_NTAGS, alg));
	default:
		return (DST_R_UNSUPPORTEDALG);
	}
}

void
dst__privstruct_free(dst_private_t *priv) {
	int i;

	if (priv == NULL)
		return;
	for (i = 0; i < priv->nelements; i++) {
		if (priv->elements[i].data == NULL)
			continue;
		memset(priv->elements[i].data, 0, MAXFIELDSIZE);
		free(priv->elements[i].data);
	}
	priv->nelements = 0;
}

isc_result_t
dst__privstruct_parse(dst_key_t *key, unsigned int alg, isc_lex_t *lex,
		      dst_private_t *priv)
{
	int n = 0, major, minor, check;
	isc_buffer_t b;
	isc_token_t token;
	unsigned char *data = NULL;
	unsigned int opt = ISC_LEXOPT_EOL;
	isc_result_t ret;
	isc_boolean_t external = ISC_FALSE;

	REQUIRE(priv != NULL);

	priv->nelements = 0;
	memset(priv->elements, 0, sizeof(priv->elements));

#define NEXTTOKEN(lex, opt, token)				\
	do {							\
		ret = isc_lex_gettoken(lex, opt, token);	\
		if (ret != ISC_R_SUCCESS)			\
			goto fail;				\
	} while (0)

#define READLINE(lex, opt, token)				\
	do {							\
		ret = isc_lex_gettoken(lex, opt, token);	\
		if (ret == ISC_R_EOF)				\
			break;					\
		else if (ret != ISC_R_SUCCESS)			\
			goto fail;				\
	} while ((*token).type != isc_tokentype_eol)

	/*
	 * Read the description line.
	 */
	NEXTTOKEN(lex, opt, &token);
	if (token.type != isc_tokentype_string ||
	    strcmp(DST_AS_STR(token), PRIVATE_KEY_STR) != 0)
	{
		ret = DST_R_INVALIDPRIVATEKEY;
		goto fail;
	}

	NEXTTOKEN(lex, opt, &token);
	if (token.type != isc_tokentype_string ||
	    (DST_AS_STR(token))[0] != 'v')
	{
		ret = DST_R_INVALIDPRIVATEKEY;
		goto fail;
	}
	if (sscanf(DST_AS_STR(token), "v%d.%d", &major, &minor) != 2)
	{
		ret = DST_R_INVALIDPRIVATEKEY;
		goto fail;
	}

	if (major > DST_MAJOR_VERSION) {
		ret = DST_R_INVALIDPRIVATEKEY;
		goto fail;
	}

	READLINE(lex, opt, &token);

	/*
	 * Read the algorithm line.
	 */
	NEXTTOKEN(lex, opt, &token);
	if (token.type != isc_tokentype_string ||
	    strcmp(DST_AS_STR(token), ALGORITHM_STR) != 0)
	{
		ret = DST_R_INVALIDPRIVATEKEY;
		goto fail;
	}

	NEXTTOKEN(lex, opt | ISC_LEXOPT_NUMBER, &token);
	if (token.type != isc_tokentype_number ||
	    token.value.as_ulong != (unsigned long) dst_key_alg(key))
	{
		ret = DST_R_INVALIDPRIVATEKEY;
		goto fail;
	}

	READLINE(lex, opt, &token);

	/*
	 * Read the key data.
	 */
	for (n = 0; n < MAXFIELDS; n++) {
		int tag;
		isc_region_t r;
		do {
			ret = isc_lex_gettoken(lex, opt, &token);
			if (ret == ISC_R_EOF)
				goto done;
			if (ret != ISC_R_SUCCESS)
				goto fail;
		} while (token.type == isc_tokentype_eol);

		if (token.type != isc_tokentype_string) {
			ret = DST_R_INVALIDPRIVATEKEY;
			goto fail;
		}

		if (strcmp(DST_AS_STR(token), "External:") == 0) {
			external = ISC_TRUE;
			goto next;
		}

		/* Numeric metadata */
		tag = find_numericdata(DST_AS_STR(token));
		if (tag >= 0) {
			INSIST(tag < NUMERIC_NTAGS);

			NEXTTOKEN(lex, opt | ISC_LEXOPT_NUMBER, &token);
			if (token.type != isc_tokentype_number) {
				ret = DST_R_INVALIDPRIVATEKEY;
				goto fail;
			}
			goto next;
		}

		/* Timing metadata */
		tag = find_timedata(DST_AS_STR(token));
		if (tag >= 0) {
			INSIST(tag < TIMING_NTAGS);

			NEXTTOKEN(lex, opt, &token);
			if (token.type != isc_tokentype_string) {
				ret = DST_R_INVALIDPRIVATEKEY;
				goto fail;
			}

			goto next;
		}

		/* Key data */
		tag = find_value(DST_AS_STR(token), alg);
		if (tag < 0 && minor > DST_MINOR_VERSION)
			goto next;
		else if (tag < 0) {
			ret = DST_R_INVALIDPRIVATEKEY;
			goto fail;
		}

		priv->elements[n].tag = tag;

		data = (unsigned char *) malloc(MAXFIELDSIZE);
		if (data == NULL)
			goto fail;

		isc_buffer_init(&b, data, MAXFIELDSIZE);
		ret = isc_base64_tobuffer(lex, &b, -1);
		if (ret != ISC_R_SUCCESS)
			goto fail;

		isc_buffer_usedregion(&b, &r);
		priv->elements[n].length = r.length;
		priv->elements[n].data = r.base;
		priv->nelements++;

	  next:
		READLINE(lex, opt, &token);
		data = NULL;
	}

 done:
	if (external && priv->nelements != 0) {
		ret = DST_R_INVALIDPRIVATEKEY;
		goto fail;
	}

	check = check_data(priv, alg, ISC_TRUE, external);
	if (check < 0) {
		ret = DST_R_INVALIDPRIVATEKEY;
		goto fail;
	} else if (check != ISC_R_SUCCESS) {
		ret = check;
		goto fail;
	}

	key->external = external;

	return (ISC_R_SUCCESS);

fail:
	dst__privstruct_free(priv);
	if (data != NULL)
		free(data);

	return (ret);
}

/*! \file */
