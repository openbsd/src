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

/* $Id: cfg.h,v 1.5 2020/02/25 05:00:43 jsg Exp $ */

#ifndef ISCCFG_CFG_H
#define ISCCFG_CFG_H 1

/*****
 ***** Module Info
 *****/

/*! \file isccfg/cfg.h
 * \brief
 * This is the new, table-driven, YACC-free configuration file parser.
 */

/***
 *** Imports
 ***/

#include <isc/refcount.h>
#include <isc/types.h>
#include <isc/list.h>

/***
 *** Types
 ***/

/*%
 * A configuration parser.
 */
typedef struct cfg_parser cfg_parser_t;

/*%
 * A configuration type definition object.  There is a single
 * static cfg_type_t object for each data type supported by
 * the configuration parser.
 */
typedef struct cfg_type cfg_type_t;

/*%
 * A configuration object.  This is the basic building block of the
 * configuration parse tree.  It contains a value (which may be
 * of one of several types) and information identifying the file
 * and line number the value came from, for printing error
 * messages.
 */
typedef struct cfg_obj cfg_obj_t;

/*%
 * A configuration object list element.
 */
typedef struct cfg_listelt cfg_listelt_t;

/***
 *** Functions
 ***/

isc_result_t
cfg_parser_create(isc_log_t *lctx, cfg_parser_t **ret);
/*%<
 * Create a configuration file parser.  Any warning and error
 * messages will be logged to 'lctx'.
 *
 * The parser object returned can be used for a single call
 * to cfg_parse_file() or cfg_parse_buffer().  It must not
 * be reused for parsing multiple files or buffers.
 */

isc_result_t
cfg_parse_file(cfg_parser_t *pctx, const char *filename,
	       const cfg_type_t *type, cfg_obj_t **ret);
/*%<
 * Read a configuration containing data of type 'type'
 * and make '*ret' point to its parse tree.
 *
 * The configuration is read from the file 'filename'
 * (isc_parse_file()) or the buffer 'buffer'
 * (isc_parse_buffer()).
 *
 * Returns an error if the file does not parse correctly.
 *
 * Requires:
 *\li 	"filename" is valid.
 *\li 	"mem" is valid.
 *\li	"type" is valid.
 *\li 	"cfg" is non-NULL and "*cfg" is NULL.
 *\li   "flags" be one or more of CFG_PCTX_NODEPRECATED or zero.
 *
 * Returns:
 *     \li #ISC_R_SUCCESS                 - success
 *\li      #ISC_R_NOMEMORY                - no memory available
 *\li      #ISC_R_INVALIDFILE             - file doesn't exist or is unreadable
 *\li      others	                      - file contains errors
 */

void
cfg_parser_destroy(cfg_parser_t **pctxp);
/*%<
 * Remove a reference to a configuration parser; destroy it if there are no
 * more references.
 */

isc_result_t
cfg_map_get(const cfg_obj_t *mapobj, const char* name, const cfg_obj_t **obj);
/*%<
 * Extract an element from a configuration object, which
 * must be of a map type.
 *
 * Requires:
 * \li     'mapobj' points to a valid configuration object of a map type.
 * \li     'name' points to a null-terminated string.
 * \li	'obj' is non-NULL and '*obj' is NULL.
 *
 * Returns:
 * \li     #ISC_R_SUCCESS                  - success
 * \li     #ISC_R_NOTFOUND                 - name not found in map
 */

const cfg_obj_t *
cfg_map_getname(const cfg_obj_t *mapobj);
/*%<
 * Get the name of a named map object, like a server "key" clause.
 *
 * Requires:
 *    \li  'mapobj' points to a valid configuration object of a map type.
 *
 * Returns:
 * \li     A pointer to a configuration object naming the map object,
 *	or NULL if the map object does not have a name.
 */

const char *
cfg_obj_asstring(const cfg_obj_t *obj);
/*%<
 * Returns the value of a configuration object of a string type
 * as a null-terminated string.
 *
 * Requires:
 * \li     'obj' points to a valid configuration object of a string type.
 *
 * Returns:
 * \li     A pointer to a null terminated string.
 */

isc_boolean_t
cfg_obj_islist(const cfg_obj_t *obj);
/*%<
 * Return true iff 'obj' is of list type.
 */

const cfg_listelt_t *
cfg_list_first(const cfg_obj_t *obj);
/*%<
 * Returns the first list element in a configuration object of a list type.
 *
 * Requires:
 * \li     'obj' points to a valid configuration object of a list type or NULL.
 *
 * Returns:
 *   \li   A pointer to a cfg_listelt_t representing the first list element,
 * 	or NULL if the list is empty or nonexistent.
 */

const cfg_listelt_t *
cfg_list_next(const cfg_listelt_t *elt);
/*%<
 * Returns the next element of a list of configuration objects.
 *
 * Requires:
 * \li     'elt' points to cfg_listelt_t obtained from cfg_list_first() or
 *	a previous call to cfg_list_next().
 *
 * Returns:
 * \li     A pointer to a cfg_listelt_t representing the next element,
 * 	or NULL if there are no more elements.
 */

unsigned int
cfg_list_length(const cfg_obj_t *obj, isc_boolean_t recurse);
/*%<
 * Returns the length of a list of configure objects.  If obj is
 * not a list, returns 0.  If recurse is true, add in the length of
 * all contained lists.
 */

void
cfg_print(const cfg_obj_t *obj,
	  void (*f)(void *closure, const char *text, int textlen),
	  void *closure);
void
cfg_printx(const cfg_obj_t *obj, unsigned int flags,
	   void (*f)(void *closure, const char *text, int textlen),
	   void *closure);

#define CFG_PRINTER_XKEY        0x1     /* '?' out shared keys. */

/*%<
 * Print the configuration object 'obj' by repeatedly calling the
 * function 'f', passing 'closure' and a region of text starting
 * at 'text' and comprising 'textlen' characters.
 *
 * If CFG_PRINTER_XKEY the contents of shared keys will be obscured
 * by replacing them with question marks ('?')
 */

void
cfg_print_grammar(const cfg_type_t *type,
	  void (*f)(void *closure, const char *text, int textlen),
	  void *closure);
/*%<
 * Print a summary of the grammar of the configuration type 'type'.
 */

void
cfg_obj_destroy(cfg_parser_t *pctx, cfg_obj_t **obj);
/*%<
 * Delete a reference to a configuration object; destroy the object if
 * there are no more references.
 *
 * Require:
 * \li     '*obj' is a valid cfg_obj_t.
 * \li     'pctx' is a valid cfg_parser_t.
 */

#endif /* ISCCFG_CFG_H */
