/*
 * Copyright (c) 1999 - 2000 Kungliga Tekniska Högskolan
 * (Royal Institute of Technology, Stockholm, Sweden).
 * All rights reserved.
 * 
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 
 * 3. Neither the name of the Institute nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 * 
 * THIS SOFTWARE IS PROVIDED BY THE INSTITUTE AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE INSTITUTE OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

/*
 * Test if agetarg works as expected
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <stdio.h>

#include <err.h>
#include <roken.h>

#include <agetarg.h>

RCSID("$KTH: ga-test.c,v 1.12 2000/10/03 00:33:59 lha Exp $");

typedef struct {
    int style;
    int argc;
    char *argv[10];
    enum { GA_SUCCESS = 0, GA_FAILURE } retval ;
} ga_tests;


/* XXX TODO: aarg_negative_flag, manualpage generation ? */

/*
 *
 */

static void
test_simple_string (void)
{
    char *string;
    int i, optind;
    ga_tests tests[] = {
	{ AARG_GNUSTYLE, 2, { "string", "--string=foo", NULL } },
	{ AARG_GNUSTYLE, 3, { "string", "-s", "foo", NULL} },
	{ AARG_AFSSTYLE, 3, { "string", "-string", "foo", NULL} },
	{ AARG_AFSSTYLE, 2, { "string", "--flag"}, 		GA_FAILURE },
	{ AARG_AFSSTYLE, 2, { "string", "foo", NULL} }
    };

    struct agetargs args[] = {
	{ "string", 's', aarg_string, NULL,
	  "string test", "stringfoo", aarg_mandatory},
	{ NULL, 0, aarg_end, NULL, NULL }
    }, *a = args;

    a->value = &string;

    for (i = 0 ; i < sizeof(tests)/sizeof(*tests); i++) {
	string = NULL;
	optind = 0;

	if (agetarg (args, tests[i].argc, tests[i].argv, &optind, 
		    tests[i].style)) {
	    if (tests[i].retval == GA_FAILURE)
		continue;
	    warnx ("test_string: %s failed for test %d",
		   tests[i].argv[1], i);
	    continue;
	} else {
	    if (tests[i].retval != GA_SUCCESS) {
		warnx ("test_string: %s failed to fail for test %d",
		       tests[i].argv[1], i);
		continue;
	    }
	}
	
	if (optind != tests[i].argc) {
	    warnx ("argc != optind for test %s, %d", tests[i].argv[1], i);
	    continue;
	}

	if (string == NULL || strcmp (string, "foo") != 0) {
	    warnx ("error parsing for test %d: string", i);
	    continue;
	}
    }
}

/*
 *
 */

static void
test_simple_strings (void)
{
    agetarg_strings strings;

    int i, optind;
    ga_tests tests[] = {
	{ AARG_GNUSTYLE, 3, { "strings", 
			     "--strings=foo", "--strings=bar", NULL } },
	{ AARG_GNUSTYLE, 5, { "strings", "-s", "foo", "-s", "bar", NULL} },
	{ AARG_AFSSTYLE, 4, { "strings", "-string", "foo", "bar", NULL} }
#if 0
	{ AARG_AFSSTYLE, 3, { "strings", "foo", "bar", NULL} }
#endif
    };

    struct agetargs args[] = {
	{ "strings", 's', aarg_strings, NULL,
	  "strings test", "stringsfoo", aarg_optional},
	{ NULL, 0, aarg_end, NULL, NULL }
    }, *a = args;

    a->value = &strings;

    for (i = 0 ; i < sizeof(tests)/sizeof(*tests); i++) {
	strings.num_strings = 0;
	strings.strings = NULL;
	optind = 0;

	if (agetarg (args, tests[i].argc, tests[i].argv, &optind, 
		    tests[i].style)) {
	    if (tests[i].retval == GA_FAILURE)
		continue;
	    warnx ("test_strings: %s failed for test %d",
		   tests[i].argv[1], i);
	    continue;
	} else {
	    if (tests[i].retval != GA_SUCCESS) {
		warnx ("test_strings: %s failed to fail for test %d",
		       tests[i].argv[1], i);
		continue;
	    }
	}
	
	if (optind != tests[i].argc) {
	    warnx ("argc != optind for test %s, %d",
		   tests[i].argv[1], i);
	    continue;
	}

	if (strings.num_strings != 2 
	    || strcmp(strings.strings[0], "foo") != 0
	    || strcmp(strings.strings[1], "bar") != 0)
	{
	    warnx ("error parsing for test %d: strings", i);
	    continue;
	}
    }
}

/*
 *
 */

static void
test_simple_integer (void)
{
    int integer;
    int i, optind;
    ga_tests tests[] = {
	{ AARG_GNUSTYLE, 2, { "integer", "--integer=4711", NULL } },
	{ AARG_GNUSTYLE, 3, { "integer", "-i", "4711", NULL} },
	{ AARG_AFSSTYLE, 3, { "integer", "-integer", "4711", NULL} },
	{ AARG_AFSSTYLE, 2, { "integer", "4711", NULL} }
    };

    struct agetargs args[] = {
	{ "integer", 'i', aarg_integer, NULL,
	  "integer test", "integer", aarg_mandatory},
	{ NULL, 0, aarg_end, NULL, NULL }
    }, *a = args;

    a->value = &integer;

    for (i = 0 ; i < sizeof(tests)/sizeof(*tests); i++) {
	integer = 0;
	optind = 0;

	if (agetarg (args, tests[i].argc, tests[i].argv, &optind, 
		    tests[i].style)) {
	    if (tests[i].retval == GA_FAILURE)
		continue;
	    warnx ("test_integer: %s failed for test %d",
		   tests[i].argv[1], i);
	    continue;
	} else {
	    if (tests[i].retval != GA_SUCCESS) {
		warnx ("test_integer: %s failed to fail for test %d",
		       tests[i].argv[1], i);
		continue;
	    }
	}
	
	if (optind != tests[i].argc) {
	    warnx ("argc != optind for test %s, %d",
		   tests[i].argv[1], i);	
	    continue;
	}

	if (integer != 4711) {
	    warnx ("error parsing for test %d: integer 4711", i);
	    continue;
	}
    }
}

/*
 *
 */

static void
test_simple_flag (void)
{
    int flag;
    int i, optind;
    ga_tests tests[] = {
	{ AARG_GNUSTYLE, 2, { "flag", "--flag=yes", NULL },	GA_SUCCESS },
	{ AARG_GNUSTYLE, 2, { "flag", "-g", NULL},		GA_SUCCESS },
	{ AARG_AFSSTYLE, 2, { "flag", "--flag"}, 		GA_FAILURE },
	{ AARG_AFSSTYLE, 2, { "flag", "-flag", NULL},		GA_SUCCESS },
#if 0
	/* XXX */
	{ AARG_AFSSTYLE, 2, { "flag", "yes", NULL},		GA_SUCCESS },
#endif
	{ AARG_GNUSTYLE, 2, { "flag", "--no-flag", NULL},	GA_SUCCESS }
    };

    struct agetargs args[] = {
	{ "flag", 'g', aarg_flag, NULL,
	  "flag", "flag bar", aarg_optional},
	{ NULL, 0, aarg_end, NULL, NULL }
    }, *a = args;

    a->value = &flag;

    for (i = 0 ; i < sizeof(tests)/sizeof(*tests); i++) {
	if (i < 4)
	    flag = 0;
	else
	    flag = 1;
	optind = 0;

	if (agetarg (args, tests[i].argc, tests[i].argv, &optind, 
		    tests[i].style)) {
	    if (tests[i].retval == GA_FAILURE)
		continue;
	    warnx ("test_flag: %s failed for test %d",
		   tests[i].argv[1], i);
	    continue;
	} else {
	    if (tests[i].retval != GA_SUCCESS) {
		warnx ("test_flag: %s failed to fail for test %d",
		       tests[i].argv[1], i);
		continue;
	    }
	}
	
	if (optind != tests[i].argc) {
	    warnx ("argc != optind for test %s, %d",
		   tests[i].argv[1], i);
	    continue;
	}

	if (i < 4) {
	    if (flag == 0) {
		warnx ("error parsing for test %d: flag %s",
		       i,
		       tests[i].argv[1]);
		continue;
	    }
	} else {
	    if (flag != 0) {
		warnx ("error parsing test %d: flag %s",
		       i,
		       tests[i].argv[1]);
		    continue;
	    }
	}
    }
}

/*
 *
 */

int
main (int argc, char **argv)
{
    set_progname (argv[0]);

    test_simple_string();
    test_simple_strings();
    test_simple_integer();
    test_simple_flag();
    
    return 0;
}
