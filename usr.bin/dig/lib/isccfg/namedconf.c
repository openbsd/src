/*
 * Copyright (C) Internet Systems Consortium, Inc. ("ISC")
 *
 * Permission to use, copy, modify, and/or distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND ISC DISCLAIMS ALL WARRANTIES WITH
 * REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF MERCHANTABILITY
 * AND FITNESS.  IN NO EVENT SHALL ISC BE LIABLE FOR ANY SPECIAL, DIRECT,
 * INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING FROM
 * LOSS OF USE, DATA OR PROFITS, WHETHER IN AN ACTION OF CONTRACT, NEGLIGENCE
 * OR OTHER TORTIOUS ACTION, ARISING OUT OF OR IN CONNECTION WITH THE USE OR
 * PERFORMANCE OF THIS SOFTWARE.
 */

/*! \file */
#include <stdlib.h>

#include <isccfg/cfg.h>
#include <isccfg/grammar.h>

/*%
 * Forward declarations of static functions.
 */

static cfg_type_t cfg_type_key;

/*%
 * Clauses that can be found within the 'key' statement.
 */
static cfg_clausedef_t
key_clauses[] = {
	{ "algorithm", &cfg_type_astring, 0 },
	{ "secret", &cfg_type_sstring, 0 },
	{ NULL, NULL, 0 }
};

static cfg_clausedef_t *
key_clausesets[] = {
	key_clauses,
	NULL
};
static cfg_type_t cfg_type_key = {
	"key", cfg_parse_named_map, &cfg_rep_map, key_clausesets
};

/*%
 * rndc
 */

static cfg_clausedef_t
rndckey_clauses[] = {
	{ "key", &cfg_type_key, 0 },
	{ NULL, NULL, 0 }
};

static cfg_clausedef_t *
rndckey_clausesets[] = {
	rndckey_clauses,
	NULL
};

/*
 * session.key has exactly the same syntax as rndc.key, but it's defined
 * separately for clarity (and so we can extend it someday, if needed).
 */
cfg_type_t cfg_type_sessionkey = {
	"sessionkey", cfg_parse_mapbody, &cfg_rep_map, rndckey_clausesets
};

