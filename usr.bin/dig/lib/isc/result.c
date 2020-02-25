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

/* $Id: result.c,v 1.4 2020/02/25 05:00:43 jsg Exp $ */

/*! \file */

#include <stddef.h>
#include <stdlib.h>

#include <isc/resultclass.h>
#include <isc/util.h>

typedef struct resulttable {
	unsigned int				base;
	unsigned int				last;
	const char **				text;
	int					set;
	ISC_LINK(struct resulttable)		link;
} resulttable;

static const char *description[ISC_R_NRESULTS] = {
	"success",				/*%< 0 */
	"out of memory",			/*%< 1 */
	"timed out",				/*%< 2 */
	"no available threads",			/*%< 3 */
	"address not available",		/*%< 4 */
	"address in use",			/*%< 5 */
	"permission denied",			/*%< 6 */
	"no pending connections",		/*%< 7 */
	"network unreachable",			/*%< 8 */
	"host unreachable",			/*%< 9 */
	"network down",				/*%< 10 */
	"host down",				/*%< 11 */
	"connection refused",			/*%< 12 */
	"not enough free resources",		/*%< 13 */
	"end of file",				/*%< 14 */
	"socket already bound",			/*%< 15 */
	"reload",				/*%< 16 */
	"lock busy",				/*%< 17 */
	"already exists",			/*%< 18 */
	"ran out of space",			/*%< 19 */
	"operation canceled",			/*%< 20 */
	"socket is not bound",			/*%< 21 */
	"shutting down",			/*%< 22 */
	"not found",				/*%< 23 */
	"unexpected end of input",		/*%< 24 */
	"failure",				/*%< 25 */
	"I/O error",				/*%< 26 */
	"not implemented",			/*%< 27 */
	"unbalanced parentheses",		/*%< 28 */
	"no more",				/*%< 29 */
	"invalid file",				/*%< 30 */
	"bad base64 encoding",			/*%< 31 */
	"unexpected token",			/*%< 32 */
	"quota reached",			/*%< 33 */
	"unexpected error",			/*%< 34 */
	"already running",			/*%< 35 */
	"ignore",				/*%< 36 */
	"address mask not contiguous",		/*%< 37 */
	"file not found",			/*%< 38 */
	"file already exists",			/*%< 39 */
	"socket is not connected",		/*%< 40 */
	"out of range",				/*%< 41 */
	"out of entropy",			/*%< 42 */
	"invalid use of multicast address",	/*%< 43 */
	"not a file",				/*%< 44 */
	"not a directory",			/*%< 45 */
	"queue is full",			/*%< 46 */
	"address family mismatch",		/*%< 47 */
	"address family not supported",		/*%< 48 */
	"bad hex encoding",			/*%< 49 */
	"too many open files",			/*%< 50 */
	"not blocking",				/*%< 51 */
	"unbalanced quotes",			/*%< 52 */
	"operation in progress",		/*%< 53 */
	"connection reset",			/*%< 54 */
	"soft quota reached",			/*%< 55 */
	"not a valid number",			/*%< 56 */
	"disabled",				/*%< 57 */
	"max size",				/*%< 58 */
	"invalid address format",		/*%< 59 */
	"bad base32 encoding",			/*%< 60 */
	"unset",				/*%< 61 */
	"multiple",				/*%< 62 */
	"would block",				/*%< 63 */
};

#define ISC_RESULT_RESULTSET			2

static isc_boolean_t 				once = ISC_FALSE;
static ISC_LIST(resulttable)			tables;

static isc_result_t
register_table(unsigned int base, unsigned int nresults, const char **text,
	       int set)
{
	resulttable *table;

	REQUIRE(base % ISC_RESULTCLASS_SIZE == 0);
	REQUIRE(nresults <= ISC_RESULTCLASS_SIZE);
	REQUIRE(text != NULL);

	/*
	 * We use malloc() here because we we want to be able to use
	 * isc_result_totext() even if there is no memory context.
	 */
	table = malloc(sizeof(*table));
	if (table == NULL)
		return (ISC_R_NOMEMORY);
	table->base = base;
	table->last = base + nresults - 1;
	table->text = text;
	table->set = set;
	ISC_LINK_INIT(table, link);

	ISC_LIST_APPEND(tables, table, link);

	return (ISC_R_SUCCESS);
}

static void
initialize_action(void) {
	isc_result_t result;

	ISC_LIST_INIT(tables);

	result = register_table(ISC_RESULTCLASS_ISC, ISC_R_NRESULTS,
				description, ISC_RESULT_RESULTSET);
	if (result != ISC_R_SUCCESS)
		UNEXPECTED_ERROR(__FILE__, __LINE__,
				 "register_table() %s: %u", "failed",
				 result);
}

static void
initialize(void) {
	if (!once) {
		once = ISC_TRUE;
		initialize_action();
	}
}

const char *
isc_result_totext(isc_result_t result) {
	resulttable *table;
	const char *text;
	int index;

	initialize();

	text = NULL;
	for (table = ISC_LIST_HEAD(tables);
	     table != NULL;
	     table = ISC_LIST_NEXT(table, link)) {
		if (result >= table->base && result <= table->last) {
			index = (int)(result - table->base);
			text = table->text[index];
			break;
		}
	}
	if (text == NULL)
		text = "(result code text not available)";

	return (text);
}

isc_result_t
isc_result_register(unsigned int base, unsigned int nresults,
		    const char **text, int set)
{
	initialize();

	return (register_table(base, nresults, text, set));
}
