/*
 * Copyright (c) 2000-2001 Sendmail, Inc. and its suppliers.
 *	All rights reserved.
 *
 * By using this file, you agree to the terms and conditions set
 * forth in the LICENSE file which can be found at the top level of
 * the sendmail distribution.
 */

#include <sm/gen.h>
SM_IDSTR(Id, "@(#)$Sendmail: test.c,v 1.12 2001/03/05 03:22:41 ca Exp $")

/*
**  Abstractions for writing libsm test programs.
*/

#include <stdlib.h>
#include <unistd.h>
#include <sm/debug.h>
#include <sm/io.h>
#include <sm/test.h>

extern char *optarg;
extern int optind;
extern int optopt;
extern int opterr;

int SmTestIndex;
int SmTestNumErrors;
bool SmTestVerbose;

static char Help[] = "\
%s [-h] [-d debugging] [-v]\n\
\n\
%s\n\
\n\
-h		Display this help information.\n\
-d debugging	Set debug activation levels.\n\
-v		Verbose output.\n\
";

static char Usage[] = "\
Usage: %s [-h] [-v]\n\
Use %s -h for help.\n\
";

/*
**  SM_TEST_BEGIN -- initialize test system.
**
**	Parameters:
**		argc -- argument counter.
**		argv -- argument vector.
**		testname -- description of tests.
**
**	Results:
**		none.
*/

void
sm_test_begin(argc, argv, testname)
	int argc;
	char **argv;
	char *testname;
{
	int c;

	SmTestIndex = 0;
	SmTestNumErrors = 0;
	SmTestVerbose = false;
	opterr = 0;

	while ((c = getopt(argc, argv, "vhd:")) != -1)
	{
		switch (c)
		{
		  case 'v':
			SmTestVerbose = true;
			break;
		  case 'd':
			sm_debug_addsettings_x(optarg);
			break;
		  case 'h':
			(void) sm_io_fprintf(smioout, SM_TIME_DEFAULT, Help,
				argv[0], testname);
			exit(0);
		  default:
			(void) sm_io_fprintf(smioerr, SM_TIME_DEFAULT,
				"Unknown command line option -%c\n", optopt);
			(void) sm_io_fprintf(smioerr, SM_TIME_DEFAULT,  Usage,
				argv[0], argv[0]);
			exit(1);
		}
	}
}

/*
**  SM_TEST -- single test.
**
**	Parameters:
**		success -- did test succeeed?
**		expr -- expression that has been evaluated.
**		filename -- guess...
**		lineno -- line number.
**
**	Results:
**		value of success.
*/

bool
sm_test(success, expr, filename, lineno)
	bool success;
	char *expr;
	char *filename;
	int lineno;
{
	++SmTestIndex;
	if (SmTestVerbose)
		(void) sm_io_fprintf(smioerr, SM_TIME_DEFAULT,  "%d..",
			SmTestIndex);
	if (!success)
	{
		++SmTestNumErrors;
		if (!SmTestVerbose)
			(void) sm_io_fprintf(smioerr, SM_TIME_DEFAULT,
				 "%d..", SmTestIndex);
		(void) sm_io_fprintf(smioerr, SM_TIME_DEFAULT,
			"bad! %s:%d %s\n", filename, lineno, expr);
	}
	else
	{
		if (SmTestVerbose)
			(void) sm_io_fprintf(smioerr, SM_TIME_DEFAULT,  "ok\n");
	}
	return success;
}

/*
**  SM_TEST_END -- end of test system.
**
**	Parameters:
**		none.
**
**	Results:
**		number of errors.
*/

int
sm_test_end()
{
	(void) sm_io_fprintf(smioerr, SM_TIME_DEFAULT,
		"%d of %d tests completed successfully\n",
		SmTestIndex - SmTestNumErrors, SmTestIndex);
	if (SmTestNumErrors != 0)
		(void) sm_io_fprintf(smioerr, SM_TIME_DEFAULT,
			"*** %d error%s in test! ***\n",
			SmTestNumErrors,
			SmTestNumErrors > 1 ? "s" : "");

	return SmTestNumErrors;
}
