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

/* $ISC: cfg_test.c,v 1.11.2.1 2001/10/22 23:52:19 gson Exp $ */

#include <config.h>

#include <errno.h>
#include <stdlib.h>

#include <isc/mem.h>
#include <isc/string.h>
#include <isc/util.h>

#include <isccfg/cfg.h>

#include <dns/log.h>

static void
check_result(isc_result_t result, const char *format, ...) {
	va_list args;

	if (result == ISC_R_SUCCESS)
		return;

	va_start(args, format);
	vfprintf(stderr, format, args);
	va_end(args);
	fprintf(stderr, ": %s\n", isc_result_totext(result));
	exit(1);
}

static void
output(void *closure, const char *text, int textlen) {
	UNUSED(closure);
	(void) fwrite(text, 1, textlen, stdout);
}

static void
usage(void) {
	fprintf(stderr, "usage: cfg_test --rndc|--named "
		"[--grammar] [--memstats] conffile\n");
	exit(1);
}

int
main(int argc, char **argv) {
	isc_result_t result;
	isc_mem_t *mctx = NULL;
	isc_log_t *lctx = NULL;
	isc_logconfig_t *lcfg = NULL;
	isc_logdestination_t destination;
	cfg_parser_t *pctx = NULL;
	cfg_obj_t *cfg = NULL;
	cfg_type_t *type = NULL;
	isc_boolean_t grammar = ISC_FALSE;
	isc_boolean_t memstats = ISC_FALSE;	
	char *filename = NULL;

	RUNTIME_CHECK(isc_mem_create(0, 0, &mctx) == ISC_R_SUCCESS);

	result = isc_log_create(mctx, &lctx, &lcfg);
	check_result(result, "isc_log_create()");
	isc_log_setcontext(lctx);

	/*
	 * Create and install the default channel.
	 */
	destination.file.stream = stderr;
	destination.file.name = NULL;
	destination.file.versions = ISC_LOG_ROLLNEVER;
	destination.file.maximum_size = 0;
	result = isc_log_createchannel(lcfg, "_default",
				       ISC_LOG_TOFILEDESC,
				       ISC_LOG_DYNAMIC,
				       &destination, ISC_LOG_PRINTTIME);
	check_result(result, "isc_log_createchannel()");
	result = isc_log_usechannel(lcfg, "_default", NULL, NULL);
	check_result(result, "isc_log_usechannel()");

	/*
	 * Set the initial debug level.
	 */
	isc_log_setdebuglevel(lctx, 2);

	if (argc < 3)
		usage();

	while (argc > 1) {
		if (strcmp(argv[1], "--grammar") == 0) {
			grammar = ISC_TRUE;
		} else if (strcmp(argv[1], "--memstats") == 0) {
			memstats = ISC_TRUE;
		} else if (strcmp(argv[1], "--named") == 0) {
			type = &cfg_type_namedconf;
		} else if (strcmp(argv[1], "--rndc") == 0) {
			type = &cfg_type_rndcconf;
		} else if (argv[1][0] == '-') {
			usage();
		} else {
			filename = argv[1];
		}
		argv++, argc--;       
	}

	if (grammar) {
		if (type == NULL)
			usage();
		cfg_print_grammar(type, output, NULL);
	} else {
		if (type == NULL || filename == NULL)
			usage();
		RUNTIME_CHECK(cfg_parser_create(mctx, lctx, &pctx) == ISC_R_SUCCESS);

		result = cfg_parse_file(pctx, filename, type, &cfg);

		fprintf(stderr, "read config: %s\n", isc_result_totext(result));

		if (result != ISC_R_SUCCESS)
			exit(1);

		cfg_print(cfg, output, NULL);

		cfg_obj_destroy(pctx, &cfg);

		cfg_parser_destroy(&pctx);
	}

	isc_log_destroy(&lctx);
	if (memstats)
		isc_mem_stats(mctx, stderr);
	isc_mem_destroy(&mctx);

	return (0);
}
