/*
 * Copyright (C) 1998-2001  Internet Software Consortium.
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

/* $ISC: lex_test.c,v 1.18 2001/01/09 21:41:13 bwelling Exp $ */

#include <config.h>

#include <isc/commandline.h>
#include <isc/lex.h>
#include <isc/mem.h>
#include <isc/util.h>

isc_mem_t *mctx;
isc_lex_t *lex;

isc_lexspecials_t specials;

static void
print_token(isc_token_t *tokenp, FILE *stream) {
	switch (tokenp->type) {
	case isc_tokentype_unknown:
		fprintf(stream, "UNKNOWN");
		break;
	case isc_tokentype_string:
		fprintf(stream, "STRING %.*s",
			(int)tokenp->value.as_region.length,
			tokenp->value.as_region.base);
		break;
	case isc_tokentype_number:
		fprintf(stream, "NUMBER %lu", tokenp->value.as_ulong);
		break;
	case isc_tokentype_qstring:
		fprintf(stream, "QSTRING \"%.*s\"",
			(int)tokenp->value.as_region.length,
			tokenp->value.as_region.base);
		break;
	case isc_tokentype_eol:
		fprintf(stream, "EOL");
		break;
	case isc_tokentype_eof:
		fprintf(stream, "EOF");
		break;
	case isc_tokentype_initialws:
		fprintf(stream, "INITIALWS");
		break;
	case isc_tokentype_special:
		fprintf(stream, "SPECIAL %c", tokenp->value.as_char);
		break;
	case isc_tokentype_nomore:
		fprintf(stream, "NOMORE");
		break;
	default:
		FATAL_ERROR(__FILE__, __LINE__, "Unexpected type %d",
			    tokenp->type);
	}
}

int
main(int argc, char *argv[]) {
	isc_token_t token;
	isc_result_t result;
	int quiet = 0;
	int c;
	int masterfile = 1;
	int stats = 0;
	unsigned int options = 0;
	int done = 0;

	while ((c = isc_commandline_parse(argc, argv, "qmcs")) != -1) {
		switch (c) {
		case 'q':
			quiet = 1;
			break;
		case 'm':
			masterfile = 1;
			break;
		case 'c':
			masterfile = 0;
			break;
		case 's':
			stats = 1;
			break;
		}
	}

	RUNTIME_CHECK(isc_mem_create(0, 0, &mctx) == ISC_R_SUCCESS);
	RUNTIME_CHECK(isc_lex_create(mctx, 256, &lex) == ISC_R_SUCCESS);

	if (masterfile) {
		/* Set up to lex DNS master file. */

		specials['('] = 1;
		specials[')'] = 1;
		specials['"'] = 1;
		isc_lex_setspecials(lex, specials);
		options = ISC_LEXOPT_DNSMULTILINE | ISC_LEXOPT_ESCAPE |
			ISC_LEXOPT_EOF |
			ISC_LEXOPT_QSTRING | ISC_LEXOPT_NOMORE;
		isc_lex_setcomments(lex, ISC_LEXCOMMENT_DNSMASTERFILE);
	} else {
		/* Set up to lex DNS config file. */

		specials['{'] = 1;
		specials['}'] = 1;
		specials[';'] = 1;
		specials['/'] = 1;
		specials['"'] = 1;
		specials['!'] = 1;
		specials['*'] = 1;
		isc_lex_setspecials(lex, specials);
		options = ISC_LEXOPT_EOF |
			ISC_LEXOPT_QSTRING |
			ISC_LEXOPT_NUMBER | ISC_LEXOPT_NOMORE;
		isc_lex_setcomments(lex, (ISC_LEXCOMMENT_C|
					  ISC_LEXCOMMENT_CPLUSPLUS|
					  ISC_LEXCOMMENT_SHELL));
	}

	RUNTIME_CHECK(isc_lex_openstream(lex, stdin) == ISC_R_SUCCESS);

	while ((result = isc_lex_gettoken(lex, options, &token)) ==
	       ISC_R_SUCCESS && !done) {
		if (!quiet) {
			char *name = isc_lex_getsourcename(lex);
			print_token(&token, stdout);
			printf(" line = %lu file = %s\n",
				isc_lex_getsourceline(lex),
				(name == NULL) ? "<none>" : name);
		}
		if (token.type == isc_tokentype_eof)
			isc_lex_close(lex);
		if (token.type == isc_tokentype_nomore)
			done = 1;
	}
	if (result != ISC_R_SUCCESS)
		printf("Result: %s\n", isc_result_totext(result));

	isc_lex_close(lex);
	isc_lex_destroy(&lex);
	if (!quiet && stats)
		isc_mem_stats(mctx, stdout);
	isc_mem_destroy(&mctx);

	return (0);
}
