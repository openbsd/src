/*
 * Copyright (C) 2001  Internet Software Consortium.
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

/* $ISC: check.h,v 1.4 2001/08/03 17:24:11 gson Exp $ */

#ifndef ISCCFG_CHECK_H
#define ISCCFG_CHECK_H 1

#include <isc/lang.h>
#include <isc/types.h>

#include <isccfg/cfg.h>

ISC_LANG_BEGINDECLS

isc_result_t
cfg_check_namedconf(cfg_obj_t *config, isc_log_t *logctx, isc_mem_t *mctx);
/*
 * Check the syntactic validity of a configuration parse tree generated from
 * a named.conf file.
 *
 * Requires:
 *	config is a valid parse tree
 *
 *	logctx is a valid logging context.
 *
 * Returns:
 * 	ISC_R_SUCCESS
 * 	ISC_R_FAILURE
 */

isc_result_t
cfg_check_key(cfg_obj_t *config, isc_log_t *logctx);
/*
 * As above, but for a single 'key' statement.
 */

ISC_LANG_ENDDECLS

#endif /* ISCCFG_CHECK_H */
