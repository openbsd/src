/*
 * Copyright (C) 1999-2002  Internet Software Consortium.
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

/* $ISC: main.c,v 1.119.2.2 2002/08/05 06:57:01 marka Exp $ */

#include <config.h>

#include <ctype.h>
#include <stdlib.h>
#include <string.h>

#include <isc/app.h>
#include <isc/commandline.h>
#include <isc/dir.h>
#include <isc/entropy.h>
#include <isc/file.h>
#include <isc/os.h>
#include <isc/platform.h>
#include <isc/resource.h>
#include <isc/task.h>
#include <isc/timer.h>
#include <isc/util.h>

#include <isccc/result.h>

#include <dns/dispatch.h>
#include <dns/result.h>
#include <dns/view.h>

#include <dst/result.h>

/*
 * Defining NS_MAIN provides storage declarations (rather than extern)
 * for variables in named/globals.h.
 */
#define NS_MAIN 1

#include <named/control.h>
#include <named/globals.h>	/* Explicit, though named/log.h includes it. */
#include <named/interfacemgr.h>
#include <named/log.h>
#include <named/os.h>
#include <named/server.h>
#include <named/lwresd.h>
#include <named/main.h>

/*
 * Include header files for database drivers here.
 */
/* #include "xxdb.h" */

static isc_boolean_t	want_stats = ISC_FALSE;
static char		program_name[ISC_DIR_NAMEMAX] = "named";
static char		absolute_conffile[ISC_DIR_PATHMAX];
static char    		saved_command_line[512];

void
ns_main_earlywarning(const char *format, ...) {
	va_list args;

	va_start(args, format);
	if (ns_g_lctx != NULL) {
		isc_log_vwrite(ns_g_lctx, NS_LOGCATEGORY_GENERAL,
			       NS_LOGMODULE_MAIN, ISC_LOG_WARNING,
			       format, args);
	} else {
		fprintf(stderr, "%s: ", program_name);
		vfprintf(stderr, format, args);
		fprintf(stderr, "\n");
		fflush(stderr);
	}
	va_end(args);
}

void
ns_main_earlyfatal(const char *format, ...) {
	va_list args;

	va_start(args, format);
	if (ns_g_lctx != NULL) {
		isc_log_vwrite(ns_g_lctx, NS_LOGCATEGORY_GENERAL,
			       NS_LOGMODULE_MAIN, ISC_LOG_CRITICAL,
			       format, args);
		isc_log_write(ns_g_lctx, NS_LOGCATEGORY_GENERAL,
			       NS_LOGMODULE_MAIN, ISC_LOG_CRITICAL,
			       "exiting (due to early fatal error)");
	} else {
		fprintf(stderr, "%s: ", program_name);
		vfprintf(stderr, format, args);
		fprintf(stderr, "\n");
		fflush(stderr);
	}
	va_end(args);

	exit(1);
}

static void
assertion_failed(const char *file, int line, isc_assertiontype_t type,
		 const char *cond)
{
	/*
	 * Handle assertion failures.
	 */

	if (ns_g_lctx != NULL) {
		/*
		 * Reset the assetion callback in case it is the log
		 * routines causing the assertion.
		 */
		isc_assertion_setcallback(NULL);

		isc_log_write(ns_g_lctx, NS_LOGCATEGORY_GENERAL,
			      NS_LOGMODULE_MAIN, ISC_LOG_CRITICAL,
			      "%s:%d: %s(%s) failed", file, line,
			      isc_assertion_typetotext(type), cond);
		isc_log_write(ns_g_lctx, NS_LOGCATEGORY_GENERAL,
			      NS_LOGMODULE_MAIN, ISC_LOG_CRITICAL,
			      "exiting (due to assertion failure)");
	} else {
		fprintf(stderr, "%s:%d: %s(%s) failed\n",
			file, line, isc_assertion_typetotext(type), cond);
		fflush(stderr);
	}

	if (ns_g_coreok)
		abort();
	exit(1);
}

static void
library_fatal_error(const char *file, int line, const char *format,
		    va_list args) ISC_FORMAT_PRINTF(3, 0);

static void
library_fatal_error(const char *file, int line, const char *format,
		    va_list args)
{
	/*
	 * Handle isc_error_fatal() calls from our libraries.
	 */

	if (ns_g_lctx != NULL) {
		/*
		 * Reset the error callback in case it is the log
		 * routines causing the assertion.
		 */
		isc_error_setfatal(NULL);

		isc_log_write(ns_g_lctx, NS_LOGCATEGORY_GENERAL,
			      NS_LOGMODULE_MAIN, ISC_LOG_CRITICAL,
			      "%s:%d: fatal error:", file, line);
		isc_log_vwrite(ns_g_lctx, NS_LOGCATEGORY_GENERAL,
			       NS_LOGMODULE_MAIN, ISC_LOG_CRITICAL,
			       format, args);
		isc_log_write(ns_g_lctx, NS_LOGCATEGORY_GENERAL,
			      NS_LOGMODULE_MAIN, ISC_LOG_CRITICAL,
			      "exiting (due to fatal error in library)");
	} else {
		fprintf(stderr, "%s:%d: fatal error: ", file, line);
		vfprintf(stderr, format, args);
		fprintf(stderr, "\n");
		fflush(stderr);
	}

	if (ns_g_coreok)
		abort();
	exit(1);
}

static void
library_unexpected_error(const char *file, int line, const char *format,
			 va_list args) ISC_FORMAT_PRINTF(3, 0);

static void
library_unexpected_error(const char *file, int line, const char *format,
			 va_list args)
{
	/*
	 * Handle isc_error_unexpected() calls from our libraries.
	 */

	if (ns_g_lctx != NULL) {
		isc_log_write(ns_g_lctx, NS_LOGCATEGORY_GENERAL,
			      NS_LOGMODULE_MAIN, ISC_LOG_ERROR,
			      "%s:%d: unexpected error:", file, line);
		isc_log_vwrite(ns_g_lctx, NS_LOGCATEGORY_GENERAL,
			       NS_LOGMODULE_MAIN, ISC_LOG_ERROR,
			       format, args);
	} else {
		fprintf(stderr, "%s:%d: fatal error: ", file, line);
		vfprintf(stderr, format, args);
		fprintf(stderr, "\n");
		fflush(stderr);
	}
}

static void
lwresd_usage(void) {
	fprintf(stderr,
		"usage: lwresd [-c conffile | -C resolvconffile] "
		"[-d debuglevel] [-f|-g]\n"
		"              [-n number_of_cpus] [-p port]"
		"[-P listen-port] [-s]\n"
		"              [-t chrootdir] [-u username] [-i pidfile]\n");
}

static void
usage(void) {
	if (ns_g_lwresdonly) {
		lwresd_usage();
		return;
	}
	fprintf(stderr,
		"usage: named [-c conffile] [-d debuglevel] "
		"[-f|-g] [-n number_of_cpus]\n"
		"             [-p port] [-s] [-t chrootdir] [-u username] [-i pidfile]\n");
}

static void
save_command_line(int argc, char *argv[]) {
	int i;
	char *src;
	char *dst;
	char *eob;
	const char truncated[] = "...";
	isc_boolean_t quoted = ISC_FALSE;

	dst = saved_command_line;
	eob = saved_command_line + sizeof(saved_command_line);

	for (i = 1; i < argc && dst < eob; i++) {
		*dst++ = ' ';

		src = argv[i];
		while (*src != '\0' && dst < eob) {
			/*
			 * This won't perfectly produce a shell-independent
			 * pastable command line in all circumstances, but
			 * comes close, and for practical purposes will
			 * nearly always be fine.
			 */
			if (quoted || isalnum(*src & 0xff) ||
			    *src == '-' || *src == '_' ||
			    *src == '.' || *src == '/') {
				*dst++ = *src++;
				quoted = ISC_FALSE;
			} else {
				*dst++ = '\\';
				quoted = ISC_TRUE;
			}
		}
	}

	INSIST(sizeof(saved_command_line) >= sizeof(truncated));

	if (dst == eob)
		strcpy(eob - sizeof(truncated), truncated);
	else
		*dst = '\0';
}

static int
parse_int(char *arg, const char *desc) {
	char *endp;
	int tmp;
	long int ltmp;

	ltmp = strtol(arg, &endp, 10);
	tmp = (int) ltmp;
	if (*endp != '\0')
		ns_main_earlyfatal("%s '%s' must be numeric", desc, arg);
	if (tmp < 0 || tmp != ltmp)
		ns_main_earlyfatal("%s '%s' out of range", desc, arg);
	return (tmp);
}

static void
parse_command_line(int argc, char *argv[]) {
	int ch;
	int port;

	save_command_line(argc, argv);

	isc_commandline_errprint = ISC_FALSE;
	while ((ch = isc_commandline_parse(argc, argv,
					   "c:C:d:fgi:ln:N:p:P:st:u:vx:")) !=
	       -1) {
		switch (ch) {
		case 'c':
			ns_g_conffile = isc_commandline_argument;
			lwresd_g_conffile = isc_commandline_argument;
			if (lwresd_g_useresolvconf)
				ns_main_earlyfatal("cannot specify -c and -C");
			ns_g_conffileset = ISC_TRUE;
			break;
		case 'C':
			lwresd_g_resolvconffile = isc_commandline_argument;
			if (ns_g_conffileset)
				ns_main_earlyfatal("cannot specify -c and -C");
			lwresd_g_useresolvconf = ISC_TRUE;
			break;
		case 'd':
			ns_g_debuglevel = parse_int(isc_commandline_argument,
						    "debug level");
			break;
		case 'f':
			ns_g_foreground = ISC_TRUE;
			break;
		case 'g':
			ns_g_foreground = ISC_TRUE;
			ns_g_logstderr = ISC_TRUE;
			break;
		case 'i':
			ns_g_pidfile = isc_commandline_argument;
			break;
		case 'l':
			ns_g_lwresdonly = ISC_TRUE;
			break;
		case 'N': /* Deprecated. */
		case 'n':
			ns_g_cpus = parse_int(isc_commandline_argument,
					      "number of cpus");
			if (ns_g_cpus == 0)
				ns_g_cpus = 1;
			break;
		case 'p':
			port = parse_int(isc_commandline_argument, "port");
			if (port < 1 || port > 65535)
				ns_main_earlyfatal("port '%s' out of range",
						   isc_commandline_argument);
			ns_g_port = port;
			break;
		/* XXXBEW Should -P be removed? */
		case 'P':
			port = parse_int(isc_commandline_argument, "port");
			if (port < 1 || port > 65535)
				ns_main_earlyfatal("port '%s' out of range",
						   isc_commandline_argument);
			lwresd_g_listenport = port;
			break;
		case 's':
			/* XXXRTH temporary syntax */
			want_stats = ISC_TRUE;
			break;
		case 't':
			/* XXXJAB should we make a copy? */
			ns_g_chrootdir = isc_commandline_argument;
			break;
		case 'u':
			ns_g_username = isc_commandline_argument;
			break;
		case 'v':
			printf("BIND %s\n", ns_g_version);
			exit(0);
		case '?':
			usage();
			ns_main_earlyfatal("unknown option '-%c'",
					   isc_commandline_option);
		default:
			ns_main_earlyfatal("parsing options returned %d", ch);
		}
	}

	argc -= isc_commandline_index;
	argv += isc_commandline_index;

	if (argc > 0) {
		usage();
		ns_main_earlyfatal("extra command line arguments");
	}

	
}

static isc_result_t
create_managers(void) {
	isc_result_t result;

#ifdef ISC_PLATFORM_USETHREADS
	if (ns_g_cpus == 0)
		ns_g_cpus = isc_os_ncpus();
#else
	ns_g_cpus = 1;
#endif
	isc_log_write(ns_g_lctx, NS_LOGCATEGORY_GENERAL, NS_LOGMODULE_SERVER,
		      ISC_LOG_INFO, "using %u CPU%s",
		      ns_g_cpus, ns_g_cpus == 1 ? "" : "s");
	result = isc_taskmgr_create(ns_g_mctx, ns_g_cpus, 0, &ns_g_taskmgr);
	if (result != ISC_R_SUCCESS) {
		UNEXPECTED_ERROR(__FILE__, __LINE__,
				 "ns_taskmgr_create() failed: %s",
				 isc_result_totext(result));
		return (ISC_R_UNEXPECTED);
	}

	result = isc_timermgr_create(ns_g_mctx, &ns_g_timermgr);
	if (result != ISC_R_SUCCESS) {
		UNEXPECTED_ERROR(__FILE__, __LINE__,
				 "ns_timermgr_create() failed: %s",
				 isc_result_totext(result));
		return (ISC_R_UNEXPECTED);
	}

	result = isc_socketmgr_create(ns_g_mctx, &ns_g_socketmgr);
	if (result != ISC_R_SUCCESS) {
		UNEXPECTED_ERROR(__FILE__, __LINE__,
				 "isc_socketmgr_create() failed: %s",
				 isc_result_totext(result));
		return (ISC_R_UNEXPECTED);
	}

	result = isc_entropy_create(ns_g_mctx, &ns_g_entropy);
	if (result != ISC_R_SUCCESS) {
		UNEXPECTED_ERROR(__FILE__, __LINE__,
				 "isc_entropy_create() failed: %s",
				 isc_result_totext(result));
		return (ISC_R_UNEXPECTED);
	}

	return (ISC_R_SUCCESS);
}

static void
destroy_managers(void) {
	ns_lwresd_shutdown();

	isc_entropy_detach(&ns_g_entropy);
	if (ns_g_fallbackentropy != NULL) {
		isc_entropy_detach(&ns_g_fallbackentropy);
	}
	/*
	 * isc_taskmgr_destroy() will block until all tasks have exited,
	 */
	isc_taskmgr_destroy(&ns_g_taskmgr);
	isc_timermgr_destroy(&ns_g_timermgr);
	isc_socketmgr_destroy(&ns_g_socketmgr);
}

static void
setup(void) {
	isc_result_t result;

        /*
	 * Write pidfile before chroot if specified on the command line
	 */
	if (ns_g_pidfile != NULL)
		ns_os_preopenpidfile(ns_g_pidfile);

	/*
	 * Get the user and group information before changing the root
	 * directory, so the administrator does not need to keep a copy
	 * of the user and group databases in the chroot'ed environment.
	 */
	ns_os_inituserinfo(ns_g_username);

	/*
	 * Initialize time conversion information and /dev/null
	 */
	ns_os_tzset();
	ns_os_opendevnull();

	/*
	 * Initialize system's random device as fallback entropy source
	 * if running chroot'ed.
	 */
	result = isc_entropy_create(ns_g_mctx, &ns_g_fallbackentropy);
	if (result != ISC_R_SUCCESS)
		ns_main_earlyfatal("isc_entropy_create() failed: %s",
				   isc_result_totext(result));
#ifdef PATH_RANDOMDEV
	if (ns_g_chrootdir != NULL) {
		result = isc_entropy_createfilesource(ns_g_fallbackentropy,
						      PATH_RANDOMDEV);
		if (result != ISC_R_SUCCESS)
			ns_main_earlywarning("could not open pre-chroot "
					     "entropy source %s: %s",
					     PATH_RANDOMDEV,
					     isc_result_totext(result));
	}
#endif

	ns_os_chroot(ns_g_chrootdir);

	/*
	 * For operating systems which have a capability mechanism, now
	 * is the time to switch to minimal privs and change our user id.
	 * On traditional UNIX systems, this call will be a no-op, and we
	 * will change the user ID after reading the config file the first
	 * time.  (We need to read the config file to know which possibly
	 * privileged ports to bind() to.)
	 */
	ns_os_minprivs();

	result = ns_log_init(ISC_TF(ns_g_username != NULL));
	if (result != ISC_R_SUCCESS)
		ns_main_earlyfatal("ns_log_init() failed: %s",
				   isc_result_totext(result));

	/*
	 * Now is the time to daemonize (if we're not running in the
	 * foreground).  We waited until now because we wanted to get
	 * a valid logging context setup.  We cannot daemonize any later,
	 * because calling create_managers() will create threads, which
	 * would be lost after fork().
	 */
	if (!ns_g_foreground)
		ns_os_daemonize();

	isc_log_write(ns_g_lctx, NS_LOGCATEGORY_GENERAL, NS_LOGMODULE_MAIN,
		      ISC_LOG_NOTICE, "starting BIND %s%s", ns_g_version,
		      saved_command_line);

	/*
	 * Get the initial resource limits.
	 */
	(void)isc_resource_getlimit(isc_resource_stacksize,
				    &ns_g_initstacksize);
	(void)isc_resource_getlimit(isc_resource_datasize,
				    &ns_g_initdatasize);
	(void)isc_resource_getlimit(isc_resource_coresize,
				    &ns_g_initcoresize);
	(void)isc_resource_getlimit(isc_resource_openfiles,
				    &ns_g_initopenfiles);

	/*
	 * If the named configuration filename is relative, prepend the current
	 * directory's name before possibly changing to another directory.
	 */
	if (! isc_file_isabsolute(ns_g_conffile)) {
		result = isc_file_absolutepath(ns_g_conffile,
					       absolute_conffile,
					       sizeof(absolute_conffile));
		if (result != ISC_R_SUCCESS)
			ns_main_earlyfatal("could not construct absolute path of "
					   "configuration file: %s", 
					   isc_result_totext(result));
		ns_g_conffile = absolute_conffile;
	}

	result = create_managers();
	if (result != ISC_R_SUCCESS)
		ns_main_earlyfatal("create_managers() failed: %s",
				   isc_result_totext(result));

	/*
	 * Add calls to register sdb drivers here.
	 */
	/* xxdb_init(); */

	ns_server_create(ns_g_mctx, &ns_g_server);
}

static void
cleanup(void) {
	destroy_managers();

	ns_server_destroy(&ns_g_server);

	/*
	 * Add calls to unregister sdb drivers here.
	 */
	/* xxdb_clear(); */

	isc_log_write(ns_g_lctx, NS_LOGCATEGORY_GENERAL, NS_LOGMODULE_MAIN,
		      ISC_LOG_NOTICE, "exiting");
	ns_log_shutdown();
}

int
main(int argc, char *argv[]) {
	isc_result_t result;

	result = isc_file_progname(*argv, program_name, sizeof(program_name));
	if (result != ISC_R_SUCCESS)
		ns_main_earlyfatal("program name too long");

	if (strcmp(program_name, "lwresd") == 0)
		ns_g_lwresdonly = ISC_TRUE;

	isc_assertion_setcallback(assertion_failed);
	isc_error_setfatal(library_fatal_error);
	isc_error_setunexpected(library_unexpected_error);

	ns_os_init(program_name);

	result = isc_app_start();
	if (result != ISC_R_SUCCESS)
		ns_main_earlyfatal("isc_app_start() failed: %s",
				   isc_result_totext(result));

	result = isc_mem_create(0, 0, &ns_g_mctx);
	if (result != ISC_R_SUCCESS)
		ns_main_earlyfatal("isc_mem_create() failed: %s",
				   isc_result_totext(result));

	dns_result_register();
	dst_result_register();
	isccc_result_register();

	parse_command_line(argc, argv);

	setup();

	/*
	 * Start things running and then wait for a shutdown request
	 * or reload.
	 */
	do {
		result = isc_app_run();

		if (result == ISC_R_RELOAD) {
			ns_server_reloadwanted(ns_g_server);
		} else if (result != ISC_R_SUCCESS) {
			UNEXPECTED_ERROR(__FILE__, __LINE__,
					 "isc_app_run(): %s",
					 isc_result_totext(result));
			/*
			 * Force exit.
			 */
			result = ISC_R_SUCCESS;
		}
	} while (result != ISC_R_SUCCESS);

	cleanup();

	if (want_stats) {
		isc_mem_stats(ns_g_mctx, stdout);
		isc_mutex_stats(stdout);
	}
	isc_mem_destroy(&ns_g_mctx);

	isc_app_finish();

	ns_os_closedevnull();

	ns_os_shutdown();

	return (0);
}
