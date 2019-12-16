/*
 * Copyright (C) 2004, 2005, 2007-2010, 2013, 2014, 2016  Internet Systems Consortium, Inc. ("ISC")
 * Copyright (C) 1999-2003  Internet Software Consortium.
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

/* $Id: t_api.c,v 1.6 2019/12/16 16:31:37 deraadt Exp $ */

/*! \file */

#include <config.h>

#include <ctype.h>
#include <errno.h>
#include <limits.h>
#include <signal.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <unistd.h>

#ifndef WIN32
#include <sys/wait.h>
#else
#include <direct.h>
#endif

#include <isc/boolean.h>
#include <isc/commandline.h>
#include <isc/print.h>
#include <isc/string.h>
#include <isc/mem.h>

#include <dns/compress.h>
#include <dns/result.h>

#include "include/tests/t_api.h"

static const char *Usage =
		"\t-a               : run all tests\n"
		"\t-b <dir>         : chdir to dir before running tests"
		"\t-c <config_file> : use specified config file\n"
		"\t-d <debug_level> : set debug level to debug_level\n"
		"\t-h               : print test info\n"
		"\t-u               : print usage info\n"
		"\t-n <test_name>   : run specified test name\n"
		"\t-t <test_number> : run specified test number\n"
		"\t-x               : don't execute tests in a subproc\n"
		"\t-q <timeout>     : use 'timeout' as the timeout value\n";
/*!<
 *		-a		-->	run all tests
 *		-b dir		-->	chdir to dir before running tests
 *		-c config	-->	use config file 'config'
 *		-d		-->	turn on api debugging
 *		-h		-->	print out available test names
 *		-u		-->	print usage info
 *		-n name		-->	run test named name
 *		-tn		-->	run test n
 *		-x		-->	don't execute testcases in a subproc
 *		-q timeout	-->	use 'timeout' as the timeout value
 */

#define	T_MAXTESTS		256	/*% must be 0 mod 8 */
#define	T_MAXENV		256
#define	T_DEFAULT_CONFIG	"t_config"
#define	T_BUFSIZ		256
#define	T_BIGBUF		4096

#define	T_TCTOUT		60

int			T_debug;
int			T_timeout;
pid_t			T_pid;
static const char *	T_config;
static char		T_tvec[T_MAXTESTS / 8];
static char *		T_env[T_MAXENV + 1];
static char		T_buf[T_BIGBUF];
static char *		T_dir;
#ifdef WIN32
static testspec_t	T_testlist[T_MAXTESTS];
#endif

static int
t_initconf(const char *path);

static int
t_dumpconf(const char *path);

static int
t_putinfo(const char *key, const char *info);

static char *
t_getdate(char *buf, size_t buflen);

static void
printhelp(void);

static void
printusage(void);

static int	T_int;

static void
t_sighandler(int sig) {
	T_int = sig;
}

int
#ifndef WIN32
main(int argc, char **argv)
#else
t_main(int argc, char **argv)
#endif
{
	int			c;
	int			tnum;
#ifndef WIN32
	int			subprocs;
	pid_t			deadpid;
	int			status;
#endif
	int			len;
	isc_boolean_t		first;
	testspec_t		*pts;
#ifndef WIN32
	struct sigaction	sa;
#endif

	isc_mem_debugging = ISC_MEM_DEBUGRECORD;
	first = ISC_TRUE;
#ifndef WIN32
	subprocs = 1;
#endif
	T_timeout = T_TCTOUT;

	/*
	 * -a option is now default.
	 */
	memset(T_tvec, 0xff, sizeof(T_tvec));

	/*
	 * Parse args.
	 */
	while ((c = isc_commandline_parse(argc, argv, ":at:c:d:n:huxq:b:"))
	       != -1) {
		if (c == 'a') {
			/*
			 * Flag all tests to be run.
			 */
			memset(T_tvec, 0xff, sizeof(T_tvec));
		}
		else if (c == 'b') {
			T_dir = isc_commandline_argument;
		}
		else if (c == 't') {
			tnum = atoi(isc_commandline_argument);
			if ((tnum > 0) && (tnum < T_MAXTESTS)) {
				if (first) {
					/*
					 * Turn off effect of -a default
					 * and allow multiple -t and -n
					 * options.
					 */
					memset(T_tvec, 0, sizeof(T_tvec));
					first = ISC_FALSE;
				}
				/*
				 * Flag test tnum to be run.
				 */
				tnum -= 1;
				T_tvec[tnum / 8] |= (0x01 << (tnum % 8));
			}
		}
		else if (c == 'c') {
			T_config = isc_commandline_argument;
		}
		else if (c == 'd') {
			T_debug = atoi(isc_commandline_argument);
		}
		else if (c == 'n') {
			pts = &T_testlist[0];
			tnum = 0;
			while (pts->pfv != NULL) {
				if (! strcmp(pts->func_name,
					     isc_commandline_argument)) {
					if (first) {
						memset(T_tvec, 0,
						       sizeof(T_tvec));
						first = ISC_FALSE;
					}
					T_tvec[tnum/8] |= (0x01 << (tnum%8));
					break;
				}
				++pts;
				++tnum;
			}
			if (pts->pfv == NULL) {
				fprintf(stderr, "no such test %s\n",
					isc_commandline_argument);
				exit(1);
			}
		}
		else if (c == 'h') {
			printhelp();
			exit(0);
		}
		else if (c == 'u') {
			printusage();
			exit(0);
		}
		else if (c == 'x') {
#ifndef WIN32
			subprocs = 0;
#endif
		}
		else if (c == 'q') {
			T_timeout = atoi(isc_commandline_argument);
		}
		else if (c == ':') {
			fprintf(stderr, "Option -%c requires an argument\n",
						isc_commandline_option);
			exit(1);
		}
		else if (c == '?') {
			fprintf(stderr, "Unrecognized option -%c\n",
				isc_commandline_option);
			exit(1);
		}
	}

	/*
	 * Set cwd.
	 */

	if (T_dir != NULL && chdir(T_dir) != 0) {
		fprintf(stderr, "chdir %s failed\n", T_dir);
		exit(1);
	}

	/*
	 * We don't want buffered output.
	 */

	(void)setbuf(stdout, NULL);
	(void)setbuf(stderr, NULL);

	/*
	 * Setup signals.
	 */

#ifndef WIN32
	sa.sa_flags = 0;
	sigfillset(&sa.sa_mask);

	sa.sa_handler = t_sighandler;
	(void)sigaction(SIGINT,  &sa, NULL);
	(void)sigaction(SIGALRM, &sa, NULL);
#endif

	/*
	 * Output start stanza to journal.
	 */

	snprintf(T_buf, sizeof(T_buf), "%s:", argv[0]);
	len = strlen(T_buf);
	(void) t_getdate(T_buf + len, T_BIGBUF - len);
	t_putinfo("S", T_buf);

	/*
	 * Setup the test environment using the config file.
	 */

	if (T_config == NULL)
		T_config = T_DEFAULT_CONFIG;

	t_initconf(T_config);
	if (T_debug)
		t_dumpconf(T_config);

	/*
	 * Now invoke all the test cases.
	 */

	tnum = 0;
	pts = &T_testlist[0];
	while (pts->pfv != NULL) {
		if (T_tvec[tnum / 8] & (0x01 << (tnum % 8))) {
#ifndef WIN32
			if (subprocs) {
				T_pid = fork();
				if (T_pid == 0) {
					(*pts->pfv)();
					exit(0);
				} else if (T_pid > 0) {

					T_int = 0;
					sa.sa_handler = t_sighandler;
					(void)sigaction(SIGALRM, &sa, NULL);
					alarm(T_timeout);

					deadpid = (pid_t) -1;
					while (deadpid != T_pid) {
					    deadpid =
						    waitpid(T_pid, &status, 0);
					    if (deadpid == T_pid) {
						    if (WIFSIGNALED(status)) {
							if (WTERMSIG(status) ==
							    SIGTERM)
								t_info(
						  "the test case timed out\n");
							else
								t_info(
					 "the test case caused exception %d\n",
							     WTERMSIG(status));
							t_result(T_UNRESOLVED);
						    }
					    } else if ((deadpid == -1) &&
						       (errno == EINTR) &&
						       T_int) {
						    kill(T_pid, SIGTERM);
						    T_int = 0;
					    }
					    else if ((deadpid == -1) &&
						     ((errno == ECHILD) ||
						      (errno == ESRCH)))
						    break;
					}

					alarm(0);
					sa.sa_handler = SIG_IGN;
					(void)sigaction(SIGALRM, &sa, NULL);
				} else {
					t_info("fork failed, errno == %d\n",
					       errno);
					t_result(T_UNRESOLVED);
				}
			}
			else {
				(*pts->pfv)();
			}
#else
			(*pts->pfv)();
#endif
		}
		++pts;
		++tnum;
	}

	snprintf(T_buf, sizeof(T_buf), "%s:", argv[0]);
	len = strlen(T_buf);
	(void) t_getdate(T_buf + len, T_BIGBUF - len);
	t_putinfo("E", T_buf);

	return(0);
}

void
t_assert(const char *component, int anum, int class, const char *what, ...) {
	va_list	args;
	char buf[T_BIGBUF];

	(void)printf("T:%s:%d:%s\n", component, anum, class == T_REQUIRED ?
		     "A" : "C");

	/*
	 * Format text to a buffer.
	 */
	va_start(args, what);
	(void)vsnprintf(buf, sizeof(buf), what, args);
	va_end(args);

	(void)t_putinfo("A", buf);
	(void)printf("\n");
}

void
t_info(const char *format, ...) {
	va_list	args;
	char buf[T_BIGBUF];

	va_start(args, format);
	(void) vsnprintf(buf, sizeof(buf), format, args);
	va_end(args);
	(void) t_putinfo("I", buf);
}

void
t_result(int result) {
	const char *p;

	switch (result) {
		case T_PASS:
			p = "PASS";
			break;
		case T_FAIL:
			p = "FAIL";
			break;
		case T_UNRESOLVED:
			p = "UNRESOLVED";
			break;
		case T_SKIPPED:
			p = "SKIPPED";
			break;
		case T_UNTESTED:
			p = "UNTESTED";
			break;
		case T_THREADONLY:
			p = "THREADONLY";
			break;
		case T_PKCS11ONLY:
			p = "PKCS11ONLY";
			break;
		default:
			p = "UNKNOWN";
			break;
	}
	printf("R:%s\n", p);
}

char *
t_getenv(const char *name) {
	char	*n;
	char	**p;
	size_t	len;

	n = NULL;
	if (name && *name) {

		p = &T_env[0];
		len = strlen(name);

		while (*p != NULL) {
			if (strncmp(*p, name, len) == 0) {
				if ( *(*p + len) == '=') {
					n = *p + len + 1;
					break;
				}
			}
			++p;
		}
	}
	return(n);
}

/*
 *
 * Read in the config file at path, initializing T_env.
 *
 * note: no format checking for now ...
 *
 */

static int
t_initconf(const char *path) {

	int	n;
	int	rval;
	char	**p;
	FILE	*fp;

	rval = -1;

	fp = fopen(path, "r");
	if (fp != NULL) {
		n = 0;
		p = &T_env[0];
		while (n < T_MAXENV) {
			*p = t_fgetbs(fp);
			if (*p == NULL)
				break;
			if ((**p == '#') || (strchr(*p, '=') == NULL)) {
				/*
				 * Skip comments and other junk.
				 */
				(void)free(*p);
				continue;
			}
			++p; ++n;
		}
		(void)fclose(fp);
		rval = 0;
	}

	return (rval);
}

/*
 *
 * Dump T_env to stdout.
 *
 */

static int
t_dumpconf(const char *path) {
	int	rval;
	char	**p;
	FILE	*fp;

	rval = -1;
	fp = fopen(path, "r");
	if (fp != NULL) {
		p = &T_env[0];
		while (*p != NULL) {
			printf("C:%s\n", *p);
			++p;
		}
		(void) fclose(fp);
		rval = 0;
	}
	return(rval);
}

/*
 *
 * Read a newline or EOF terminated string from fp.
 * On success:
 *	return a malloc'd buf containing the string with
 *	the newline converted to a '\0'.
 * On error:
 *	return NULL.
 *
 * Caller is responsible for freeing buf.
 *
 */

char *
t_fgetbs(FILE *fp) {
	int	c;
	size_t	n;
	size_t	size;
	char	*buf, *old;
	char	*p;

	n = 0;
	size = T_BUFSIZ;
	old = buf = (char *) malloc(T_BUFSIZ * sizeof(char));

	if (buf != NULL) {
		p = buf;
		while ((c = fgetc(fp)) != EOF) {

			if ((c == '\r') || (c == '\n'))
				break;

			*p++ = c;
			++n;
			if ( n >= size ) {
				size += T_BUFSIZ;
				buf = (char *)realloc(buf,
						      size * sizeof(char));
				if (buf == NULL)
					goto err;
				old = buf;
				p = buf + n;
			}
		}
		*p = '\0';
		if (c == EOF && n == 0U) {
			free(buf);
			return (NULL);
		}
		return (buf);
	} else {
 err:
		if (old != NULL)
			free(old);
		fprintf(stderr, "malloc/realloc failed %d", errno);
		return(NULL);
	}
}

/*
 *
 * Put info to log, using key.
 * For now, just dump it out.
 * Later format into pretty lines.
 *
 */

static int
t_putinfo(const char *key, const char *info) {
	int	rval;

	/*
	 * For now.
	 */
	rval = printf("%s:%s", key, info);
	return(rval);
}

static char *
t_getdate(char *buf, size_t buflen) {
	size_t		n;
	time_t		t;
	struct tm	*p;

	t = time(NULL);
	p = localtime(&t);
	n = strftime(buf, buflen - 1, "%A %d %B %H:%M:%S %Y\n", p);
	return(n != 0U ? buf : NULL);
}

/*
 * Some generally used utilities.
 */
static const struct dns_errormap {
	isc_result_t	result;
	const char *text;
} dns_errormap[] = {
	{ ISC_R_SUCCESS,		"ISC_R_SUCCESS"		},
	{ ISC_R_EXISTS,			"ISC_R_EXISTS"		},
	{ ISC_R_NOTFOUND,		"ISC_R_NOTFOUND"	},
	{ ISC_R_NOSPACE,		"ISC_R_NOSPACE"		},
	{ ISC_R_UNEXPECTED,		"ISC_R_UNEXPECTED"	},
	{ ISC_R_UNEXPECTEDEND,		"ISC_R_UNEXPECTEDEND"	},
	{ ISC_R_RANGE,			"ISC_R_RANGE"		},
	{ DNS_R_LABELTOOLONG,		"DNS_R_LABELTOOLONG"	},
	{ DNS_R_BADESCAPE,		"DNS_R_BADESCAPE"	},
	/* { DNS_R_BADBITSTRING,	"DNS_R_BADBITSTRING"	}, */
	/* { DNS_R_BITSTRINGTOOLONG,	"DNS_R_BITSTRINGTOOLONG"}, */
	{ DNS_R_EMPTYLABEL,		"DNS_R_EMPTYLABEL"	},
	{ DNS_R_BADDOTTEDQUAD,		"DNS_R_BADDOTTEDQUAD"	},
	{ DNS_R_UNKNOWN,		"DNS_R_UNKNOWN"		},
	{ DNS_R_BADLABELTYPE,		"DNS_R_BADLABELTYPE"	},
	{ DNS_R_BADPOINTER,		"DNS_R_BADPOINTER"	},
	{ DNS_R_TOOMANYHOPS,		"DNS_R_TOOMANYHOPS"	},
	{ DNS_R_DISALLOWED,		"DNS_R_DISALLOWED"	},
	{ DNS_R_EXTRATOKEN,		"DNS_R_EXTRATOKEN"	},
	{ DNS_R_EXTRADATA,		"DNS_R_EXTRADATA"	},
	{ DNS_R_TEXTTOOLONG,		"DNS_R_TEXTTOOLONG"	},
	{ DNS_R_SYNTAX,			"DNS_R_SYNTAX"		},
	{ DNS_R_BADCKSUM,		"DNS_R_BADCKSUM"	},
	{ DNS_R_BADAAAA,		"DNS_R_BADAAAA"		},
	{ DNS_R_NOOWNER,		"DNS_R_NOOWNER"		},
	{ DNS_R_NOTTL,			"DNS_R_NOTTL"		},
	{ DNS_R_BADCLASS,		"DNS_R_BADCLASS"	},
	{ DNS_R_PARTIALMATCH,		"DNS_R_PARTIALMATCH"	},
	{ DNS_R_NEWORIGIN,		"DNS_R_NEWORIGIN"	},
	{ DNS_R_UNCHANGED,		"DNS_R_UNCHANGED"	},
	{ DNS_R_BADTTL,			"DNS_R_BADTTL"		},
	{ DNS_R_NOREDATA,		"DNS_R_NOREDATA"	},
	{ DNS_R_CONTINUE,		"DNS_R_CONTINUE"	},
	{ DNS_R_DELEGATION,		"DNS_R_DELEGATION"	},
	{ DNS_R_GLUE,			"DNS_R_GLUE"		},
	{ DNS_R_DNAME,			"DNS_R_DNAME"		},
	{ DNS_R_CNAME,			"DNS_R_CNAME"		},
	{ DNS_R_NXDOMAIN,		"DNS_R_NXDOMAIN"	},
	{ DNS_R_NXRRSET,		"DNS_R_NXRRSET"		},
	{ DNS_R_BADDB,			"DNS_R_BADDB"		},
	{ DNS_R_ZONECUT,		"DNS_R_ZONECUT"		},
	{ DNS_R_NOTZONETOP,		"DNS_R_NOTZONETOP"	},
	{ DNS_R_SEENINCLUDE,		"DNS_R_SEENINCLUDE"	},
	{ DNS_R_SINGLETON,		"DNS_R_SINGLETON"	},
	{ (isc_result_t)0, NULL }
};

isc_result_t
t_dns_result_fromtext(char *name) {

	isc_result_t			result;
	const struct dns_errormap	*pmap;

	result = ISC_R_UNEXPECTED;

	pmap = dns_errormap;
	while (pmap->text != NULL) {
		if (strcmp(name, pmap->text) == 0)
			break;
		++pmap;
	}

	if (pmap->text != NULL)
		result = pmap->result;

	return (result);
}

static const struct dc_method_map {
	unsigned int	dc_method;
	const char 	*text;
} dc_method_map[] = {

	{	DNS_COMPRESS_NONE,	"DNS_COMPRESS_NONE"	},
	{	DNS_COMPRESS_GLOBAL14,	"DNS_COMPRESS_GLOBAL14"	},
	{	DNS_COMPRESS_ALL,	"DNS_COMPRESS_ALL"	},
	{	0,			NULL			}
};

unsigned int
t_dc_method_fromtext(char *name) {
	unsigned int			dc_method;
	const struct dc_method_map	*pmap;

	dc_method = DNS_COMPRESS_NONE;

	pmap = dc_method_map;
	while (pmap->text != NULL) {
		if (strcmp(name, pmap->text) == 0)
			break;
		++pmap;
	}

	if (pmap->text != NULL)
		dc_method = pmap->dc_method;

	return(dc_method);
}

int
t_bustline(char *line, char **toks) {
	int	cnt;
	char	*p;

	cnt = 0;
	if (line && *line) {
		while ((p = strtok(line, "\t")) && (cnt < T_MAXTOKS)) {
			*toks++ = p;
			line = NULL;
			++cnt;
		}
	}
	return(cnt);
}

static void
printhelp(void) {
	int		cnt;
	testspec_t	*pts;

	cnt = 1;
	pts = &T_testlist[0];

	printf("Available tests:\n");
	while (pts->func_name) {
		printf("\t%d\t%s\n", cnt, pts->func_name);
		++pts;
		++cnt;
	}
}

static void
printusage(void) {
	printf("Usage:\n%s\n", Usage);
}

int
t_eval(const char *filename, int (*func)(char **), int nargs) {
	FILE		*fp;
	char		*p;
	int		line;
	int		cnt;
	int		result;
	int		tresult;
	int		nfails;
	int		nprobs;
	int		npass;
	char		*tokens[T_MAXTOKS + 1];

	tresult = T_UNTESTED;
	npass = 0;
	nfails = 0;
	nprobs = 0;

	fp = fopen(filename, "r");
	if (fp != NULL) {
		line = 0;
		while ((p = t_fgetbs(fp)) != NULL) {

			++line;

			/*
			 * Skip comment lines.
			 */
			if ((isspace((unsigned char)*p)) || (*p == '#')) {
				(void)free(p);
				continue;
			}

			cnt = t_bustline(p, tokens);
			if (cnt == nargs) {
				tresult = func(tokens);
				switch (tresult) {
				case T_PASS:
					++npass;
					break;
				case T_FAIL:
					++nfails;
					break;
				case T_SKIPPED:
				case T_UNTESTED:
					break;
				default:
					++nprobs;
					break;
				}
			} else {
				t_info("bad format in %s at line %d\n",
						filename, line);
				++nprobs;
			}

			(void)free(p);
		}
		(void)fclose(fp);
	} else {
		t_info("Missing datafile %s\n", filename);
		++nprobs;
	}

	result = T_UNRESOLVED;

	if (nfails == 0 && nprobs == 0 && npass > 0)
		result = T_PASS;
	else if (nfails > 0)
		result = T_FAIL;
	else if (npass == 0)
		result = tresult;

	return (result);
}

#ifdef WIN32
void
t_settests(const testspec_t list[]) {
	int			tnum;
	const testspec_t	*pts;

	memset(T_testlist, 0, sizeof(T_testlist));

	pts = &list[0];
	for (tnum = 0; tnum < T_MAXTESTS - 1; pts++, tnum++) {
		if (pts->pfv == NULL)
			break;
		T_testlist[tnum] = *pts;
	}
}
#endif
