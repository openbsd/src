/*
 * Copyright (C) 1999-2001  Internet Software Consortium.
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

/* $ISC: named-checkconf.c,v 1.12 2001/07/27 17:45:26 gson Exp $ */

#include <config.h>

#include <errno.h>
#include <stdlib.h>
#include <stdio.h>

#include <isc/commandline.h>
#include <isc/dir.h>
#include <isc/log.h>
#include <isc/mem.h>
#include <isc/result.h>
#include <isc/string.h>
#include <isc/util.h>

#include <isccfg/cfg.h>
#include <isccfg/check.h>

#include "check-tool.h"

isc_log_t *logc = NULL;

static void
usage(void) {
        fprintf(stderr, "usage: named-checkconf [-v] [-t directory] [named.conf]\n");
        exit(1);
}

static isc_result_t
directory_callback(const char *clausename, cfg_obj_t *obj, void *arg) {
	isc_result_t result;
	char *directory;

	REQUIRE(strcasecmp("directory", clausename) == 0);

	UNUSED(arg);
	UNUSED(clausename);

	/*
	 * Change directory.
	 */
	directory = cfg_obj_asstring(obj);
	result = isc_dir_chdir(directory);
	if (result != ISC_R_SUCCESS) {
		cfg_obj_log(obj, logc, ISC_LOG_ERROR,
			    "change directory to '%s' failed: %s",
			    directory, isc_result_totext(result));
		return (result);
	}

	return (ISC_R_SUCCESS);
}

int
main(int argc, char **argv) {
	int c;
	cfg_parser_t *parser = NULL;
	cfg_obj_t *config = NULL;
	const char *conffile = NULL;
	isc_mem_t *mctx = NULL;
	isc_result_t result;
	int exit_status = 0;

	while ((c = isc_commandline_parse(argc, argv, "t:v")) != EOF) {
		switch (c) {
		case 't':
			result = isc_dir_chroot(isc_commandline_argument);
			if (result != ISC_R_SUCCESS) {
				fprintf(stderr, "isc_dir_chroot: %s\n",
					isc_result_totext(result));
				exit(1);
			}
			result = isc_dir_chdir("/");
			if (result != ISC_R_SUCCESS) {
				fprintf(stderr, "isc_dir_chdir: %s\n",
					isc_result_totext(result));
				exit(1);
			}
			break;

		case 'v':
			printf(VERSION "\n");
			exit(0);

		default:
			usage();
		}
	}

	if (argv[isc_commandline_index] != NULL)
		conffile = argv[isc_commandline_index];
	if (conffile == NULL || conffile[0] == '\0')
		conffile = NAMED_CONFFILE;

	RUNTIME_CHECK(isc_mem_create(0, 0, &mctx) == ISC_R_SUCCESS);

	RUNTIME_CHECK(setup_logging(mctx, &logc) == ISC_R_SUCCESS);

	RUNTIME_CHECK(cfg_parser_create(mctx, logc, &parser) == ISC_R_SUCCESS);

	cfg_parser_setcallback(parser, directory_callback, NULL);

	if (cfg_parse_file(parser, conffile, &cfg_type_namedconf, &config) !=
	    ISC_R_SUCCESS)
		exit(1);

	result = cfg_check_namedconf(config, logc, mctx);
	if (result != ISC_R_SUCCESS)
		exit_status = 1;

	cfg_obj_destroy(parser, &config);

	cfg_parser_destroy(&parser);

	isc_log_destroy(&logc);

	isc_mem_destroy(&mctx);

	return (exit_status);
}
