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

/* $ISC: log_test.c,v 1.23 2001/07/09 22:39:27 gson Exp $ */

/* Principal Authors: DCL */

#include <config.h>

#include <stdlib.h>
#include <unistd.h>

#include <isc/commandline.h>
#include <isc/mem.h>
#include <isc/string.h>

#include <dns/log.h>

#define TEST_FILE "/tmp/test_log"
#define SYSLOG_FILE "/var/log/daemon.log"
#define FILE_VERSIONS 10

char usage[] = "Usage: %s [-m] [-s syslog_logfile] [-r file_versions]\n";

#define CHECK(expr) result = expr; \
	if (result != ISC_R_SUCCESS) { \
		fprintf(stderr, "%s: " #expr "%s: exiting\n", \
			progname, isc_result_totext(result)); \
	}

int
main(int argc, char **argv) {
	const char *progname, *syslog_file, *message;
	int ch, i, file_versions, stderr_line;
	isc_boolean_t show_final_mem = ISC_FALSE;
	isc_log_t *lctx;
	isc_logconfig_t *lcfg;
	isc_mem_t *mctx;
	isc_result_t result;
	isc_logdestination_t destination;
	const isc_logcategory_t *category;
	const isc_logmodule_t *module;

	progname = strrchr(*argv, '/');
	if (progname != NULL)
		progname++;
	else
		progname = *argv;

	syslog_file = SYSLOG_FILE;
	file_versions = FILE_VERSIONS;

	while ((ch = isc_commandline_parse(argc, argv, "ms:r:")) != -1) {
		switch (ch) {
		case 'm':
			show_final_mem = ISC_TRUE;
			break;
		case 's':
			syslog_file = isc_commandline_argument;
			break;
		case 'r':
			file_versions = atoi(isc_commandline_argument);
			if (file_versions < 0 &&
			    file_versions != ISC_LOG_ROLLNEVER &&
			    file_versions != ISC_LOG_ROLLINFINITE) {
				fprintf(stderr, "%s: file rotations must be "
					"%d (ISC_LOG_ROLLNEVER),\n\t"
					"%d (ISC_LOG_ROLLINFINITE) "
					"or > 0\n", progname,
					ISC_LOG_ROLLNEVER,
					ISC_LOG_ROLLINFINITE);
				exit(1);
			}
			break;
		case '?':
			fprintf(stderr, usage, progname);
			exit(1);
		}
	}

	argc -= isc_commandline_index;
	argv += isc_commandline_index;

	if (argc > 0) {
		fprintf(stderr, usage, progname);
		exit(1);
	}

	fprintf(stderr, "EXPECT:\n%s%d%s%s%s",
		"8 lines to stderr (first 4 numbered, #3 repeated)\n",
		file_versions == 0 || file_versions == ISC_LOG_ROLLNEVER ? 1 :
		file_versions > 0 ? file_versions + 1 : FILE_VERSIONS + 1,
		" " TEST_FILE " files, and\n",
		"2 lines to syslog\n",
		"lines ending with exclamation marks are errors\n\n");

	isc_log_opensyslog(progname, LOG_PID, LOG_DAEMON);

	mctx = NULL;
	lctx = NULL;
	lcfg = NULL;

	CHECK(isc_mem_create(0, 0, &mctx));
	CHECK(isc_log_create(mctx, &lctx, &lcfg));

	CHECK(isc_log_settag(lcfg, progname));

	isc_log_setcontext(lctx);
	dns_log_init(lctx);
	dns_log_setcontext(lctx);

	/*
	 * Test isc_log_categorybyname and isc_log_modulebyname.
	 */
	category = isc_log_categorybyname(lctx, "notify");
	if (category != NULL)
		fprintf(stderr, "%s category found. (expected)\n",
			category->name);
	else
		fprintf(stderr, "notify category not found!\n");

	module = isc_log_modulebyname(lctx, "xyzzy");
	if (module != NULL)
		fprintf(stderr, "%s module found!\n", module->name);
	else
		fprintf(stderr, "xyzzy module not found. (expected)\n");

	/*
	 * Create a file channel to test file opening, size limiting and
	 * version rolling.
	 */

	destination.file.name = TEST_FILE;
	destination.file.maximum_size = 1;
	destination.file.versions = file_versions;

	CHECK(isc_log_createchannel(lcfg, "file_test", ISC_LOG_TOFILE,
				    ISC_LOG_INFO, &destination,
				    ISC_LOG_PRINTTIME|
				    ISC_LOG_PRINTTAG|
				    ISC_LOG_PRINTLEVEL|
				    ISC_LOG_PRINTCATEGORY|
				    ISC_LOG_PRINTMODULE));

	/*
	 * Create a dynamic debugging channel to a file descriptor.
	 */
	destination.file.stream = stderr;

	CHECK(isc_log_createchannel(lcfg, "debug_test", ISC_LOG_TOFILEDESC,
				    ISC_LOG_DYNAMIC, &destination,
				    ISC_LOG_PRINTTIME|
				    ISC_LOG_PRINTLEVEL|
				    ISC_LOG_DEBUGONLY));

	/*
	 * Test the usability of the four predefined logging channels.
	 */
	CHECK(isc_log_usechannel(lcfg, "default_syslog",
				 DNS_LOGCATEGORY_DATABASE,
				 DNS_LOGMODULE_CACHE));
	CHECK(isc_log_usechannel(lcfg, "default_stderr",
				 DNS_LOGCATEGORY_DATABASE,
				 DNS_LOGMODULE_CACHE));
	CHECK(isc_log_usechannel(lcfg, "default_debug",
				 DNS_LOGCATEGORY_DATABASE,
				 DNS_LOGMODULE_CACHE));
	CHECK(isc_log_usechannel(lcfg, "null",
				 DNS_LOGCATEGORY_DATABASE,
				 NULL));

	/*
	 * Use the custom channels.
	 */
	CHECK(isc_log_usechannel(lcfg, "file_test",
				 DNS_LOGCATEGORY_GENERAL,
				 DNS_LOGMODULE_DB));

	CHECK(isc_log_usechannel(lcfg, "debug_test",
				 DNS_LOGCATEGORY_GENERAL,
				 DNS_LOGMODULE_RBTDB));

	fprintf(stderr, "\n==> stderr begin\n");

	/*
	 * Write to the internal default by testing both a category for which
	 * no channel has been specified and a category which was specified
	 * but not with the named module.
	 */
	stderr_line = 1;

	isc_log_write(lctx, DNS_LOGCATEGORY_SECURITY, DNS_LOGMODULE_RBT,
		      ISC_LOG_CRITICAL, "%s (%d)",
		      "Unspecified category and unspecified module to stderr",
		      stderr_line++);
	isc_log_write(lctx, DNS_LOGCATEGORY_GENERAL, DNS_LOGMODULE_RBT,
		      ISC_LOG_CRITICAL, "%s (%d)",
		      "Specified category and unspecified module to stderr",
		      stderr_line++);

	/*
	 * Write to default_syslog, default_stderr and default_debug.
	 */
	isc_log_write(lctx, DNS_LOGCATEGORY_DATABASE, DNS_LOGMODULE_CACHE,
		      ISC_LOG_WARNING, "%s (%d twice)",
		      "Using the predefined channels to syslog+stderr",
		      stderr_line++);

	/*
	 * Write to predefined null channel.
	 */
	isc_log_write(lctx, DNS_LOGCATEGORY_DATABASE, DNS_LOGMODULE_RBTDB,
		      ISC_LOG_INFO, "This is to null and should not appear!");

	/*
	 * Reset the internal default to use syslog instead of stderr,
	 * and test it.
	 */
	CHECK(isc_log_usechannel(lcfg, "default_syslog",
				 ISC_LOGCATEGORY_DEFAULT, NULL));
	isc_log_write(lctx, DNS_LOGCATEGORY_SECURITY, DNS_LOGMODULE_RBT,
		      ISC_LOG_ERROR, "%s%s",
		      "This message to the redefined default category should ",
		      "be second in syslog");
	/*
	 * Write to the file channel.
	 */
	if (file_versions >= 0 || file_versions == ISC_LOG_ROLLINFINITE) {

		/*
		 * If file_versions is 0 or ISC_LOG_ROLLINFINITE, write
		 * the "should not appear" and "should be in file" messages
		 * to ensure they get rolled.
		 */
		if (file_versions <= 0)
			file_versions = FILE_VERSIONS;

		else
			isc_log_write(lctx, DNS_LOGCATEGORY_GENERAL,
				      DNS_LOGMODULE_DB, ISC_LOG_NOTICE,
				      "This should be rolled over "
				      "and not appear!");

		for (i = file_versions - 1; i >= 0; i--)
			isc_log_write(lctx, DNS_LOGCATEGORY_GENERAL,
				      DNS_LOGMODULE_DB, ISC_LOG_NOTICE,
				      "should be in file %d/%d", i,
				      file_versions - 1);

		isc_log_write(lctx, DNS_LOGCATEGORY_GENERAL,
			      DNS_LOGMODULE_DB, ISC_LOG_NOTICE,
			      "should be in base file");
	} else {
		file_versions = FILE_VERSIONS;
		for (i = 1; i <= file_versions; i++)
			isc_log_write(lctx, DNS_LOGCATEGORY_GENERAL,
				      DNS_LOGMODULE_DB, ISC_LOG_NOTICE,
				      "This is message %d in the log file", i);
	}


	/*
	 * Write a debugging message to a category that has no
	 * debugging channels for the named module.
	 */
	isc_log_write(lctx, DNS_LOGCATEGORY_GENERAL, DNS_LOGMODULE_DB,
		      ISC_LOG_DEBUG(1),
		      "This debug message should not appear!");

	/*
	 * Write debugging messages to a dynamic debugging channel.
	 */
	isc_log_write(lctx, DNS_LOGCATEGORY_GENERAL, DNS_LOGMODULE_RBTDB,
		      ISC_LOG_CRITICAL, "This critical message should "
		      "not appear because the debug level is 0!");

	isc_log_setdebuglevel(lctx, 3);

	isc_log_write(lctx, DNS_LOGCATEGORY_GENERAL, DNS_LOGMODULE_RBTDB,
		      ISC_LOG_DEBUG(1), "%s (%d)",
		      "Dynamic debugging to stderr", stderr_line++);
	isc_log_write(lctx, DNS_LOGCATEGORY_GENERAL, DNS_LOGMODULE_RBTDB,
		      ISC_LOG_DEBUG(5),
		      "This debug level is too high and should not appear!");

	/*
	 * Test out the duplicate filtering using the debug_test channel.
	 */
	isc_log_setduplicateinterval(lcfg, 10);
	message = "This message should appear only once on stderr";

	isc_log_write1(lctx, DNS_LOGCATEGORY_GENERAL, DNS_LOGMODULE_RBTDB,
		       ISC_LOG_CRITICAL, "%s", message);
	isc_log_write1(lctx, DNS_LOGCATEGORY_GENERAL, DNS_LOGMODULE_RBTDB,
		       ISC_LOG_CRITICAL, message);

	isc_log_setduplicateinterval(lcfg, 1);
	message = "This message should appear twice on stderr";

	isc_log_write1(lctx, DNS_LOGCATEGORY_GENERAL, DNS_LOGMODULE_RBTDB,
		       ISC_LOG_CRITICAL, message);
	sleep(2);
	isc_log_write1(lctx, DNS_LOGCATEGORY_GENERAL, DNS_LOGMODULE_RBTDB,
		       ISC_LOG_CRITICAL, message);

	/*
	 * Review where everything went.
	 * XXXDCL NT
	 */
	fputc('\n', stderr);
	system("head " TEST_FILE "*; rm -f " TEST_FILE "*");

	freopen(syslog_file, "r", stdin);
	fprintf(stderr, "\n==> %s <==\n", syslog_file);
	system("tail -2");
	fputc('\n', stderr);

	isc_log_destroy(&lctx);

	if (show_final_mem)
		isc_mem_stats(mctx, stderr);

	return (0);
}
