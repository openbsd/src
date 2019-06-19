/*	$OpenBSD: getconf.c,v 1.20 2018/10/26 17:11:32 mestre Exp $	*/

/*-
 * Copyright (c) 1996 The NetBSD Foundation, Inc.
 * All rights reserved.
 *
 * This code is derived from software contributed to The NetBSD Foundation
 * by J.T. Conklin.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *      This product includes software developed by Winning Strategies, Inc.
 * 4. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

/*
 * POSIX.2 getconf utility
 *
 * Written by:
 *	J.T. Conklin (jtc@wimsey.com), Winning Strategies, Inc.
 */

#include <err.h>
#include <errno.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

static void __dead usage(void);
static void list_var(int);
static int compilation_spec_valid(const char *);

struct conf_variable
{
  const char *name;
  enum { SYSCONF, CONFSTR, PATHCONF, CONSTANT } type;
  long value;
};


#define constant_row(name)		 { #name, CONSTANT, name },
#define sysconf_row(name)		 { #name, SYSCONF,  _SC_##name },
#define pathconf_row(name)		 { #name, PATHCONF, _PC_##name },
#define confstr_row(name)		 { #name, CONFSTR,  _CS_##name },
#define posix_constant_row(name)	 { #name, CONSTANT, _POSIX_##name },
#define posix_confstr_row(name)		 { #name, CONFSTR,  _CS_POSIX_##name },
#define compat_posix2_sysconf_row(name)	 { #name, SYSCONF,  _SC_2_##name },
#define compat_posix2_constant_row(name) { #name, CONSTANT, _POSIX2_##name },

/* Some sysconf variables don't follow the pattern of the others */
#define posix2_sysconf_row(name) \
			{ "_POSIX2_" #name, SYSCONF,  _SC_2_##name },
#define posix2_pathconf_row(name) \
			{ "_POSIX2_" #name, PATHCONF,  _PC_2_##name },
#define pthread_sysconf_row(name) \
			{ "_PTHREAD_" #name, SYSCONF,  _SC_THREAD_##name },
#define xopen_sysconf_row(name) \
			{ "_XOPEN_" #name, SYSCONF,  _SC_XOPEN_##name },

const struct conf_variable conf_table[] =
{
  /* Configuration strings */
  confstr_row(PATH)
  confstr_row(V7_ENV)
  confstr_row(V6_ENV)

  /* Symbolic Utility Limits */
  sysconf_row(BC_BASE_MAX)
  sysconf_row(BC_DIM_MAX)
  sysconf_row(BC_SCALE_MAX)
  sysconf_row(BC_STRING_MAX)
  sysconf_row(COLL_WEIGHTS_MAX)
  sysconf_row(EXPR_NEST_MAX)
  sysconf_row(LINE_MAX)
  sysconf_row(RE_DUP_MAX)

  /* POSIX.1 Configurable System Variables */
  sysconf_row(AIO_LISTIO_MAX)
  sysconf_row(AIO_MAX)
  sysconf_row(AIO_PRIO_DELTA_MAX)
  sysconf_row(ARG_MAX)
  sysconf_row(CHILD_MAX)
  sysconf_row(CLK_TCK)
  sysconf_row(NGROUPS_MAX)
  sysconf_row(OPEN_MAX)
  sysconf_row(STREAM_MAX)
  sysconf_row(TZNAME_MAX)
  sysconf_row(PAGE_SIZE)
  sysconf_row(PAGESIZE)

  sysconf_row(SEM_NSEMS_MAX)
  sysconf_row(SEM_VALUE_MAX)
  sysconf_row(HOST_NAME_MAX)
  sysconf_row(LOGIN_NAME_MAX)

  sysconf_row(ATEXIT_MAX)
  sysconf_row(DELAYTIMER_MAX)
  sysconf_row(IOV_MAX)
  sysconf_row(MQ_OPEN_MAX)
  sysconf_row(MQ_PRIO_MAX)
  sysconf_row(RTSIG_MAX)
  sysconf_row(SIGQUEUE_MAX)
  sysconf_row(SYMLOOP_MAX)
  sysconf_row(TIMER_MAX)
  sysconf_row(TTY_NAME_MAX)

  posix2_sysconf_row(PBS)
  posix2_sysconf_row(PBS_ACCOUNTING)
  posix2_sysconf_row(PBS_CHECKPOINT)
  posix2_sysconf_row(PBS_LOCATE)
  posix2_sysconf_row(PBS_MESSAGE)
  posix2_sysconf_row(PBS_TRACK)

  pthread_sysconf_row(DESTRUCTOR_ITERATIONS)
  pthread_sysconf_row(KEYS_MAX)
  pthread_sysconf_row(STACK_MIN)
  pthread_sysconf_row(THREADS_MAX)

  xopen_sysconf_row(SHM)
  xopen_sysconf_row(CRYPT)
  xopen_sysconf_row(ENH_I18N)
  xopen_sysconf_row(REALTIME)
  xopen_sysconf_row(REALTIME_THREADS)
  xopen_sysconf_row(STREAMS)
  xopen_sysconf_row(UNIX)
  xopen_sysconf_row(UUCP)
  xopen_sysconf_row(VERSION)

  pathconf_row(FILESIZEBITS)
  pathconf_row(LINK_MAX)
  pathconf_row(MAX_CANON)
  pathconf_row(MAX_INPUT)
  pathconf_row(NAME_MAX)
  pathconf_row(PATH_MAX)
  pathconf_row(PIPE_BUF)
  pathconf_row(SYMLINK_MAX)

  posix2_pathconf_row(SYMLINKS)

  constant_row(_POSIX2_CHARCLASS_NAME_MAX)
  constant_row(_XOPEN_IOV_MAX)
  constant_row(_XOPEN_NAME_MAX)
  constant_row(_XOPEN_PATH_MAX)

  /* Extensions */
  sysconf_row(PHYS_PAGES)
  sysconf_row(AVPHYS_PAGES)
  sysconf_row(NPROCESSORS_CONF)
  sysconf_row(NPROCESSORS_ONLN)

  { NULL }
};

/*
 * Lots of names have a leading "_POSIX_", so put them in a table with
 * that prefix trimmed
 */
const char uposix_prefix[] = "_POSIX_";
const struct conf_variable uposix_conf_table[] =
{
  /* POSIX.1 Maximum Values */
  posix_constant_row(CLOCKRES_MIN)

  /* POSIX.1 Minimum Values */
  /*posix_constant_row(AIO_LISTIO_MAX)*/
  /*posix_constant_row(AIO_MAX)*/
  posix_constant_row(ARG_MAX)
  posix_constant_row(CHILD_MAX)
  /*posix_constant_row(DELAYTIMER_MAX)*/
  posix_constant_row(HOST_NAME_MAX)
  posix_constant_row(LINK_MAX)
  posix_constant_row(LOGIN_NAME_MAX)
  posix_constant_row(MAX_CANON)
  posix_constant_row(MAX_INPUT)
  /*posix_constant_row(MQ_OPEN_MAX)*/
  /*posix_constant_row(MQ_PRIO_MAX)*/
  posix_constant_row(NAME_MAX)
  posix_constant_row(NGROUPS_MAX)
  posix_constant_row(OPEN_MAX)
  posix_constant_row(PATH_MAX)
  posix_constant_row(PIPE_BUF)
  posix_constant_row(RE_DUP_MAX)
  /*posix_constant_row(RTSIG_MAX)*/
  posix_constant_row(SEM_NSEMS_MAX)
  posix_constant_row(SEM_VALUE_MAX)
  /*posix_constant_row(SIGQUEUE_MAX)*/
  posix_constant_row(SSIZE_MAX)
  /*posix_constant_row(SS_REPL_MAX)*/
  posix_constant_row(STREAM_MAX)
  posix_constant_row(SYMLINK_MAX)
  posix_constant_row(SYMLOOP_MAX)
  posix_constant_row(THREAD_DESTRUCTOR_ITERATIONS)
  posix_constant_row(THREAD_KEYS_MAX)
  posix_constant_row(THREAD_THREADS_MAX)
  /*posix_constant_row(TIMER_MAX)*/
  posix_constant_row(TTY_NAME_MAX)
  posix_constant_row(TZNAME_MAX)

  /* POSIX.1 Configurable System Variables */
  sysconf_row(JOB_CONTROL)
  sysconf_row(SAVED_IDS)
  sysconf_row(VERSION)
  sysconf_row(FSYNC)
  sysconf_row(MONOTONIC_CLOCK)
  sysconf_row(THREAD_SAFE_FUNCTIONS)
  sysconf_row(ADVISORY_INFO)
  sysconf_row(BARRIERS)
  sysconf_row(ASYNCHRONOUS_IO)
  sysconf_row(CLOCK_SELECTION)
  sysconf_row(CPUTIME)
  sysconf_row(IPV6)
  sysconf_row(MAPPED_FILES)
  sysconf_row(MEMLOCK)
  sysconf_row(MEMLOCK_RANGE)
  sysconf_row(MEMORY_PROTECTION)
  sysconf_row(MESSAGE_PASSING)
  sysconf_row(PRIORITIZED_IO)
  sysconf_row(PRIORITY_SCHEDULING)
  sysconf_row(RAW_SOCKETS)
  sysconf_row(READER_WRITER_LOCKS)
  sysconf_row(REALTIME_SIGNALS)
  sysconf_row(REGEXP)
  sysconf_row(SEMAPHORES)
  sysconf_row(SHARED_MEMORY_OBJECTS)
  sysconf_row(SHELL)
  sysconf_row(SPAWN)
  sysconf_row(SPIN_LOCKS)
  sysconf_row(SPORADIC_SERVER)
  sysconf_row(SS_REPL_MAX)
  sysconf_row(SYNCHRONIZED_IO)
  sysconf_row(THREAD_ATTR_STACKADDR)
  sysconf_row(THREAD_ATTR_STACKSIZE)
  sysconf_row(THREAD_CPUTIME)
  sysconf_row(THREAD_PRIO_INHERIT)
  sysconf_row(THREAD_PRIO_PROTECT)
  sysconf_row(THREAD_PRIORITY_SCHEDULING)
  sysconf_row(THREAD_PROCESS_SHARED)
  sysconf_row(THREAD_ROBUST_PRIO_INHERIT)
  sysconf_row(THREAD_SPORADIC_SERVER)
  sysconf_row(THREADS)
  sysconf_row(TIMEOUTS)
  sysconf_row(TIMERS)
  sysconf_row(TRACE)
  sysconf_row(TRACE_EVENT_FILTER)
  sysconf_row(TRACE_EVENT_NAME_MAX)
  sysconf_row(TRACE_INHERIT)
  sysconf_row(TRACE_LOG)
  sysconf_row(TRACE_NAME_MAX)
  sysconf_row(TRACE_SYS_MAX)
  sysconf_row(TRACE_USER_EVENT_MAX)
  sysconf_row(TYPED_MEMORY_OBJECTS)

  /*
   * If new compilation specification are added (V8_*?) then add them
   * to the compilation_specs array below too
   */
  sysconf_row(V7_ILP32_OFF32)
  sysconf_row(V7_ILP32_OFFBIG)
  sysconf_row(V7_LP64_OFF64)
  sysconf_row(V7_LPBIG_OFFBIG)
  sysconf_row(V6_ILP32_OFF32)
  sysconf_row(V6_ILP32_OFFBIG)
  sysconf_row(V6_LP64_OFF64)
  sysconf_row(V6_LPBIG_OFFBIG)

  /* POSIX.1 Configurable Path Variables */
  pathconf_row(CHOWN_RESTRICTED)
  pathconf_row(NO_TRUNC)
  pathconf_row(VDISABLE)
  pathconf_row(ASYNC_IO)
  pathconf_row(PRIO_IO)
  pathconf_row(SYNC_IO)
  pathconf_row(TIMESTAMP_RESOLUTION)

  { NULL }
};

/*
 * Then there are the "POSIX_*" values
 */
const char posix_prefix[] = "POSIX_";
const struct conf_variable posix_conf_table[] =
{
  pathconf_row(ALLOC_SIZE_MIN)
  pathconf_row(REC_INCR_XFER_SIZE)
  pathconf_row(REC_MAX_XFER_SIZE)
  pathconf_row(REC_MIN_XFER_SIZE)
  pathconf_row(REC_XFER_ALIGN)

  posix_confstr_row(V7_ILP32_OFF32_CFLAGS)
  posix_confstr_row(V7_ILP32_OFF32_LDFLAGS)
  posix_confstr_row(V7_ILP32_OFF32_LIBS)
  posix_confstr_row(V7_ILP32_OFFBIG_CFLAGS)
  posix_confstr_row(V7_ILP32_OFFBIG_LDFLAGS)
  posix_confstr_row(V7_ILP32_OFFBIG_LIBS)
  posix_confstr_row(V7_LP64_OFF64_CFLAGS)
  posix_confstr_row(V7_LP64_OFF64_LDFLAGS)
  posix_confstr_row(V7_LP64_OFF64_LIBS)
  posix_confstr_row(V7_LPBIG_OFFBIG_CFLAGS)
  posix_confstr_row(V7_LPBIG_OFFBIG_LDFLAGS)
  posix_confstr_row(V7_LPBIG_OFFBIG_LIBS)
  posix_confstr_row(V7_THREADS_CFLAGS)
  posix_confstr_row(V7_THREADS_LDFLAGS)
  posix_confstr_row(V7_WIDTH_RESTRICTED_ENVS)
  posix_confstr_row(V6_ILP32_OFF32_CFLAGS)
  posix_confstr_row(V6_ILP32_OFF32_LDFLAGS)
  posix_confstr_row(V6_ILP32_OFF32_LIBS)
  posix_confstr_row(V6_ILP32_OFFBIG_CFLAGS)
  posix_confstr_row(V6_ILP32_OFFBIG_LDFLAGS)
  posix_confstr_row(V6_ILP32_OFFBIG_LIBS)
  posix_confstr_row(V6_LP64_OFF64_CFLAGS)
  posix_confstr_row(V6_LP64_OFF64_LDFLAGS)
  posix_confstr_row(V6_LP64_OFF64_LIBS)
  posix_confstr_row(V6_LPBIG_OFFBIG_CFLAGS)
  posix_confstr_row(V6_LPBIG_OFFBIG_LDFLAGS)
  posix_confstr_row(V6_LPBIG_OFFBIG_LIBS)
  posix_confstr_row(V6_WIDTH_RESTRICTED_ENVS)

  { NULL }
};

/*
 * Finally, there are variables that are accepted with a prefix
 * of either "_POSIX2_" or "POSIX2_"
 */
const char compat_posix2_prefix[] = "POSIX2_";
const struct conf_variable compat_posix2_conf_table[] =
{
  /* Optional Facility Configuration Values */
  compat_posix2_sysconf_row(VERSION)
  compat_posix2_sysconf_row(C_BIND)
  compat_posix2_sysconf_row(C_DEV)
  compat_posix2_sysconf_row(CHAR_TERM)
  compat_posix2_sysconf_row(FORT_DEV)
  compat_posix2_sysconf_row(FORT_RUN)
  compat_posix2_sysconf_row(LOCALEDEF)
  compat_posix2_sysconf_row(SW_DEV)
  compat_posix2_sysconf_row(UPE)

  /* Utility Limit Minimum Values */
  compat_posix2_constant_row(BC_BASE_MAX)
  compat_posix2_constant_row(BC_DIM_MAX)
  compat_posix2_constant_row(BC_SCALE_MAX)
  compat_posix2_constant_row(BC_STRING_MAX)
  compat_posix2_constant_row(COLL_WEIGHTS_MAX)
  compat_posix2_constant_row(EXPR_NEST_MAX)
  compat_posix2_constant_row(LINE_MAX)
  compat_posix2_constant_row(RE_DUP_MAX)

  { NULL }
};

#undef constant_row
#undef sysconf_row
#undef pathconf_row
#undef confstr_row
#undef posix_constant_row
#undef posix_confstr_row
#undef compat_posix2_sysconf_row
#undef compat_posix2_constant_row


/*
 * What values are possibly accepted by the -v option?
 * These are implied to have a prefix of posix_prefix
 */
const char *compilation_specs[] = {
  "V7_ILP32_OFF32",
  "V7_ILP32_OFFBIG",
  "V7_LP64_OFF64",
  "V7_LPBIG_OFFBIG",
  "V6_ILP32_OFF32",
  "V6_ILP32_OFFBIG",
  "V6_LP64_OFF64",
  "V6_LPBIG_OFFBIG",
  NULL
};

int
main(int argc, char *argv[])
{
	int ch;
	const struct conf_variable *cp;

	long val;
	size_t slen;
	char * sval;

	while ((ch = getopt(argc, argv, "lLv:")) != -1) {
		switch (ch) {
		case 'l':	/* nonstandard: list system variables */
			list_var(0);
			return (0);
		case 'L':	/* nonstandard: list path variables */
			list_var(1);
			return (0);
		case 'v':
			if (! compilation_spec_valid(optarg))
				errx(1, "%s: unknown specification", optarg);
			break;
		case '?':
		default:
			usage();
		}
	}
	argc -= optind;
	argv += optind;

	if (argc < 1 || argc > 2)
		usage();

	/* pick a table based on a possible prefix */
	if (strncmp(*argv, uposix_prefix, sizeof(uposix_prefix) - 1) == 0) {
		cp = uposix_conf_table;
		slen = sizeof(uposix_prefix) - 1;
	} else if (strncmp(*argv, posix_prefix,
	    sizeof(posix_prefix) - 1) == 0) {
		cp = posix_conf_table;
		slen = sizeof(posix_prefix) - 1;
	} else {
		cp = conf_table;
		slen = 0;
	}

	/* scan the table */
	for (; cp->name != NULL; cp++)
		if (strcmp(*argv + slen, cp->name) == 0)
			break;

	/*
	 * If no match, then make a final check against
	 * compat_posix2_conf_table, with special magic to accept/skip
	 * a leading underbar
	 */
	slen = argv[0][0] == '_';
	if (cp->name == NULL && strncmp(*argv + slen, compat_posix2_prefix,
	    sizeof(compat_posix2_prefix) - 1) == 0) {
		slen += sizeof(compat_posix2_prefix) - 1;
		for (cp = compat_posix2_conf_table; cp->name != NULL; cp++) {
			if (strcmp(*argv + slen, cp->name) == 0)
				break;
		}
	}

	if (cp->name == NULL)
		errx(1, "%s: unknown variable", *argv);

	if (cp->type == PATHCONF) {
		if (argc != 2) usage();
	} else {
		if (argc != 1) usage();
	}

	switch (cp->type) {
	case CONSTANT:
		if (pledge("stdio", NULL) == -1)
			err(1, "pledge");
		printf("%ld\n", cp->value);
		break;

	case CONFSTR:
		if (pledge("stdio", NULL) == -1)
			err(1, "pledge");
		errno = 0;
		if ((slen = confstr(cp->value, NULL, 0)) == 0) {
			if (errno != 0)
				err(1, NULL);

			printf("undefined\n");
		} else {
			if ((sval = malloc(slen)) == NULL)
				err(1, NULL);

			confstr(cp->value, sval, slen);
			printf("%s\n", sval);
		}
		break;

	case SYSCONF:
		if (pledge("stdio inet ps vminfo", NULL) == -1)
			err(1, "pledge");
		errno = 0;
		if ((val = sysconf(cp->value)) == -1) {
			if (errno != 0)
				err(1, NULL);

			printf("undefined\n");
		} else {
			printf("%ld\n", val);
		}
		break;

	case PATHCONF:
		if (unveil(argv[1], "r") == -1)
			err(1, "unveil");
		if (pledge("stdio rpath", NULL) == -1)
			err(1, "pledge");
		errno = 0;
		if ((val = pathconf(argv[1], cp->value)) == -1) {
			if (errno != 0)
				err(1, "%s", argv[1]);

			printf("undefined\n");
		} else {
			printf("%ld\n", val);
		}
		break;
	}

	return ferror(stdout);
}


static void __dead
usage(void)
{
	extern char *__progname;

	(void)fprintf(stderr,
	    "usage: %s [-Ll] [-v specification] name [pathname]\n",
	    __progname);
	exit(1);
}

static void
list_var(int do_pathconf)
{
	const struct conf_variable *cp;

	for (cp = uposix_conf_table; cp->name != NULL; cp++)
		if ((cp->type == PATHCONF) == do_pathconf)
			printf("%s%s\n", uposix_prefix, cp->name);
	for (cp = posix_conf_table; cp->name != NULL; cp++)
		if ((cp->type == PATHCONF) == do_pathconf)
			printf("%s%s\n", posix_prefix, cp->name);
	for (cp = conf_table; cp->name != NULL; cp++)
		if ((cp->type == PATHCONF) == do_pathconf)
			printf("%s\n", cp->name);
	for (cp = compat_posix2_conf_table; cp->name != NULL; cp++)
		if ((cp->type == PATHCONF) == do_pathconf)
			printf("_%s%s\n", compat_posix2_prefix, cp->name);
}

static int
compilation_spec_valid(const char *spec)
{
	const char **sp;
	const struct conf_variable *cp;

	if (strncmp(spec, posix_prefix, sizeof(posix_prefix) - 1) != 0)
		return (0);

	spec += sizeof(posix_prefix) - 1;
	for (sp = compilation_specs; *sp != NULL; sp++)
		if (strcmp(spec, *sp) == 0)
			break;
	if (*sp == NULL)
		return (0);

	for (cp = uposix_conf_table; cp->name != NULL; cp++)
		if (strcmp(spec, cp->name) == 0 && cp->type == SYSCONF)
			return (sysconf(cp->value) != -1);

	return (0);
}
