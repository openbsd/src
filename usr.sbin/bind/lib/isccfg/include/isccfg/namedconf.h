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

/* $Id: namedconf.h,v 1.5 2020/01/28 17:09:03 florian Exp $ */

#ifndef ISCCFG_NAMEDCONF_H
#define ISCCFG_NAMEDCONF_H 1

/*! \file isccfg/namedconf.h
 * \brief
 * This module defines the named.conf, rndc.conf, and rndc.key grammars.
 */

#include <isccfg/cfg.h>

/*
 * Configuration object types.
 */

extern cfg_type_t cfg_type_sessionkey;
/*%< A complete session.key file. */

#endif /* ISCCFG_NAMEDCONF_H */
