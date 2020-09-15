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

/* $Id: grammar.h,v 1.6 2020/09/15 08:19:29 florian Exp $ */

#ifndef ISCCFG_GRAMMAR_H
#define ISCCFG_GRAMMAR_H 1

/*! \file isccfg/grammar.h */

#include <isc/lex.h>
#include <isc/sockaddr.h>
#include <isc/region.h>
#include <isc/types.h>

#include <isccfg/cfg.h>

/*
 * Definitions shared between the configuration parser
 * and the grammars; not visible to users of the parser.
 */

typedef struct cfg_clausedef cfg_clausedef_t;
typedef ISC_LIST(cfg_listelt_t) cfg_list_t;
typedef struct cfg_map cfg_map_t;
typedef struct cfg_rep cfg_rep_t;

/*
 * Function types for configuration object methods
 */

typedef isc_result_t (*cfg_parsefunc_t)(cfg_parser_t *, const cfg_type_t *type,
					cfg_obj_t **);
typedef void	     (*cfg_freefunc_t)(cfg_parser_t *, cfg_obj_t *);

/*
 * Structure definitions
 */

/*% A clause definition. */
struct cfg_clausedef {
	const char      *name;
	cfg_type_t      *type;
	unsigned int	flags;
};

/*% A configuration object type definition. */
struct cfg_type {
	const char *name;	/*%< For debugging purposes only */
	cfg_parsefunc_t	parse;
	cfg_rep_t *	rep;	/*%< Data representation */
	const void *	of;	/*%< Additional data for meta-types */
};

struct cfg_map {
	cfg_obj_t	 *id; /*%< Used for 'named maps' like keys, zones, &c */
	const cfg_clausedef_t * const *clausesets; /*%< The clauses that
						      can occur in this map;
						      used for printing */
	isc_symtab_t     *symtab;
};

/*%
 * A configuration data representation.
 */
struct cfg_rep {
	const char *	name;	/*%< For debugging only */
	cfg_freefunc_t 	free;	/*%< How to free this kind of data. */
};

/*%
 * A configuration object.  This is the main building block
 * of the configuration parse tree.
 */

struct cfg_obj {
	const cfg_type_t *type;
	union {
		isc_textregion_t string; /*%< null terminated, too */
		cfg_map_t	map;
		cfg_list_t	list;
	}               value;
	const char *	file;
	unsigned int    line;
};

/*% A list element. */
struct cfg_listelt {
	cfg_obj_t               *obj;
	ISC_LINK(cfg_listelt_t)  link;
};

/*% The parser object. */
struct cfg_parser {
	isc_log_t *	lctx;
	isc_lex_t *	lexer;
	unsigned int    errors;
	isc_token_t     token;

	/*% We are at the end of all input. */
	int	seen_eof;

	/*% The current token has been pushed back. */
	int	ungotten;

	/*%
	 * The stack of currently active files, represented
	 * as a configuration list of configuration strings.
	 * The head is the top-level file, subsequent elements
	 * (if any) are the nested include files, and the
	 * last element is the file currently being parsed.
	 */
	cfg_obj_t *	open_files;

	/*%
	 * Names of files that we have parsed and closed
	 * and were previously on the open_file list.
	 * We keep these objects around after closing
	 * the files because the file names may still be
	 * referenced from other configuration objects
	 * for use in reporting semantic errors after
	 * parsing is complete.
	 */
	cfg_obj_t *	closed_files;

	/*%
	 * Current line number.  We maintain our own
	 * copy of this so that it is available even
	 * when a file has just been closed.
	 */
	unsigned int	line;

	/*%
	 * Parser context flags, used for maintaining state
	 * from one token to the next.
	 */
	unsigned int flags;
};

/*@{*/
/*%
 * Predefined data representation types.
 */
extern cfg_rep_t cfg_rep_map;
/*@}*/

/*@{*/
/*%
 * Predefined configuration object types.
 */
extern cfg_type_t cfg_type_astring;
extern cfg_type_t cfg_type_sstring;
/*@}*/

isc_result_t
cfg_parse_named_map(cfg_parser_t *pctx, const cfg_type_t *type, cfg_obj_t **ret);

isc_result_t
cfg_parse_mapbody(cfg_parser_t *pctx, const cfg_type_t *type, cfg_obj_t **ret);

#endif /* ISCCFG_GRAMMAR_H */
