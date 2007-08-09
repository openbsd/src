/* $OpenBSD: http_main.c,v 1.49 2007/08/09 10:44:54 martynas Exp $ */

/* ====================================================================
 * The Apache Software License, Version 1.1
 *
 * Copyright (c) 2000-2003 The Apache Software Foundation.  All rights
 * reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 *
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in
 *    the documentation and/or other materials provided with the
 *    distribution.
 *
 * 3. The end-user documentation included with the redistribution,
 *    if any, must include the following acknowledgment:
 *       "This product includes software developed by the
 *        Apache Software Foundation (http://www.apache.org/)."
 *    Alternately, this acknowledgment may appear in the software itself,
 *    if and wherever such third-party acknowledgments normally appear.
 *
 * 4. The names "Apache" and "Apache Software Foundation" must
 *    not be used to endorse or promote products derived from this
 *    software without prior written permission. For written
 *    permission, please contact apache@apache.org.
 *
 * 5. Products derived from this software may not be called "Apache",
 *    nor may "Apache" appear in their name, without prior written
 *    permission of the Apache Software Foundation.
 *
 * THIS SOFTWARE IS PROVIDED ``AS IS'' AND ANY EXPRESSED OR IMPLIED
 * WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED.  IN NO EVENT SHALL THE APACHE SOFTWARE FOUNDATION OR
 * ITS CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF
 * USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
 * OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT
 * OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 * ====================================================================
 *
 * This software consists of voluntary contributions made by many
 * individuals on behalf of the Apache Software Foundation.  For more
 * information on the Apache Software Foundation, please see
 * <http://www.apache.org/>.
 *
 * Portions of this software are based upon public domain software
 * originally written at the National Center for Supercomputing Applications,
 * University of Illinois, Urbana-Champaign.
 */

/*
 * httpd.c: simple http daemon for answering WWW file requests
 *
 * 
 * 03-21-93  Rob McCool wrote original code (up to NCSA HTTPd 1.3)
 * 
 * 03-06-95  blong
 *  changed server number for child-alone processes to 0 and changed name
 *   of processes
 *
 * 03-10-95  blong
 *      Added numerous speed hacks proposed by Robert S. Thau (rst@ai.mit.edu) 
 *      including set group before fork, and call gettime before to fork
 *      to set up libraries.
 *
 * 04-14-95  rst / rh
 *      Brandon's code snarfed from NCSA 1.4, but tinkered to work with the
 *      Apache server, and also to have child processes do accept() directly.
 *
 * April-July '95 rst
 *      Extensive rework for Apache.
 */

#define REALMAIN main

#define CORE_PRIVATE

#include "httpd.h"
#include "http_main.h"
#include "http_log.h"
#include "http_config.h"	/* for read_config */
#include "http_protocol.h"	/* for read_request */
#include "http_request.h"	/* for process_request */
#include "http_conf_globals.h"
#include "http_core.h"		/* for get_remote_host */
#include "http_vhost.h"
#include "util_script.h"	/* to force util_script.c linking */
#include "util_uri.h"
#include "fdcache.h"
#include "scoreboard.h"
#include "multithread.h"
#include <sys/stat.h>
#include <sys/time.h>
#include <sys/resource.h>
#include <netinet/tcp.h>
#ifdef MOD_SSL
#include <openssl/evp.h>
#endif

/* This next function is never used. It is here to ensure that if we
 * make all the modules into shared libraries that core httpd still
 * includes the full Apache API. Without this function the objects in
 * main/util_script.c would not be linked into a minimal httpd.
 * And the extra prototype is to make gcc -Wmissing-prototypes quiet.
 */
API_EXPORT(void) ap_force_library_loading(void);
API_EXPORT(void) ap_force_library_loading(void) {
    ap_add_cgi_vars(NULL);
}

#include "explain.h"

#if !defined(max)
#define max(a,b)        (a > b ? a : b)
#endif

#define PATHSEPARATOR '/'

DEF_Explain

/* Defining GPROF when compiling uses the moncontrol() function to
 * disable gprof profiling in the parent, and enable it only for
 * request processing in children (or in one_process mode).  It's
 * absolutely required to get useful gprof results under linux
 * because the profile itimers and such are disabled across a
 * fork().  It's probably useful elsewhere as well.
 */
#ifdef GPROF
extern void moncontrol(int);
#define MONCONTROL(x) moncontrol(x)
#else
#define MONCONTROL(x)
#endif

/* this just need to be anything non-NULL */
void *ap_dummy_mutex = &ap_dummy_mutex;

/*
 * Actual definitions of config globals... here because this is
 * for the most part the only code that acts on 'em.  (Hmmm... mod_main.c?)
 */
int ap_thread_count = 0;
API_VAR_EXPORT int ap_standalone=0;
API_VAR_EXPORT int ap_configtestonly=0;
int ap_docrootcheck=1;
API_VAR_EXPORT uid_t ap_user_id=0;
API_VAR_EXPORT char *ap_user_name=NULL;
API_VAR_EXPORT gid_t ap_group_id=0;
API_VAR_EXPORT int ap_max_requests_per_child=0;
API_VAR_EXPORT int ap_max_cpu_per_child=0;
API_VAR_EXPORT int ap_max_data_per_child=0;
API_VAR_EXPORT int ap_max_nofile_per_child=0;
API_VAR_EXPORT int ap_max_rss_per_child=0;
API_VAR_EXPORT int ap_max_stack_per_child=0;
API_VAR_EXPORT int ap_threads_per_child=0;
API_VAR_EXPORT int ap_excess_requests_per_child=0;
API_VAR_EXPORT char *ap_pid_fname=NULL;
API_VAR_EXPORT char *ap_scoreboard_fname=NULL;
API_VAR_EXPORT char *ap_lock_fname=NULL;
API_VAR_EXPORT char *ap_server_argv0=NULL;
API_VAR_EXPORT struct in_addr ap_bind_address={0};
API_VAR_EXPORT int ap_daemons_to_start=0;
API_VAR_EXPORT int ap_daemons_min_free=0;
API_VAR_EXPORT int ap_daemons_max_free=0;
API_VAR_EXPORT int ap_daemons_limit=0;
API_VAR_EXPORT time_t ap_restart_time=0;
API_VAR_EXPORT int ap_suexec_enabled = 0;
API_VAR_EXPORT int ap_listenbacklog=0;

struct accept_mutex_methods_s {
    void (*child_init)(pool *p);
    void (*init)(pool *p);
    void (*on)(void);
    void (*off)(void);
    char *name;
};
typedef struct accept_mutex_methods_s accept_mutex_methods_s;
accept_mutex_methods_s *amutex;

int ap_dump_settings = 0;
API_VAR_EXPORT int ap_extended_status = 0;
API_VAR_EXPORT ap_ctx *ap_global_ctx;

/*
 * The max child slot ever assigned, preserved across restarts.  Necessary
 * to deal with MaxClients changes across SIGUSR1 restarts.  We use this
 * value to optimize routines that have to scan the entire scoreboard.
 */
static int max_daemons_limit = -1;

/*
 * During config time, listeners is treated as a NULL-terminated list.
 * child_main previously would start at the beginning of the list each time
 * through the loop, so a socket early on in the list could easily starve out
 * sockets later on in the list.  The solution is to start at the listener
 * after the last one processed.  But to do that fast/easily in child_main it's
 * way more convenient for listeners to be a ring that loops back on itself.
 * The routine setup_listeners() is called after config time to both open up
 * the sockets and to turn the NULL-terminated list into a ring that loops back
 * on itself.
 *
 * head_listener is used by each child to keep track of what they consider
 * to be the "start" of the ring.  It is also set by make_child to ensure
 * that new children also don't starve any sockets.
 *
 * Note that listeners != NULL is ensured by read_config().
 */
listen_rec *ap_listeners=NULL;
static listen_rec *head_listener;

API_VAR_EXPORT char ap_server_root[MAX_STRING_LEN]="";
API_VAR_EXPORT char ap_server_confname[MAX_STRING_LEN]="";
API_VAR_EXPORT char ap_coredump_dir[MAX_STRING_LEN]="";

API_VAR_EXPORT array_header *ap_server_pre_read_config=NULL;
API_VAR_EXPORT array_header *ap_server_post_read_config=NULL;
API_VAR_EXPORT array_header *ap_server_config_defines=NULL;

API_VAR_EXPORT int ap_server_chroot=1;
API_VAR_EXPORT int is_chrooted=0;

/* *Non*-shared http_main globals... */

static server_rec *server_conf;
static JMP_BUF APACHE_TLS jmpbuffer;
static int sd;
static fd_set listenfds;
static int listenmaxfd;
static pid_t pgrp;

/* one_process --- debugging mode variable; can be set from the command line
 * with the -X flag.  If set, this gets you the child_main loop running
 * in the process which originally started up (no detach, no make_child),
 * which is a pretty nice debugging environment.  (You'll get a SIGHUP
 * early in standalone_main; just continue through.  This is the server
 * trying to kill off any child processes which it might have lying
 * around --- Apache doesn't keep track of their pids, it just sends
 * SIGHUP to the process group, ignoring it in the root process.
 * Continue through and you'll be fine.).
 */

static int one_process = 0;

static int do_detach = 1;

/* set if timeouts are to be handled by the children and not by the parent.
 * i.e. child_timeouts = !standalone || one_process.
 */
static int child_timeouts;

#ifdef DEBUG_SIGSTOP
int raise_sigstop_flags;
#endif

/* used to maintain list of children which aren't part of the scoreboard */
typedef struct other_child_rec other_child_rec;
struct other_child_rec {
    other_child_rec *next;
    int pid;
    void (*maintenance) (int, void *, ap_wait_t);
    void *data;
    int write_fd;
};
static other_child_rec *other_children;

static pool *pglobal;		/* Global pool */
static pool *pconf;		/* Pool for config stuff */
static pool *plog;		/* Pool for error-logging files */
static pool *ptrans;		/* Pool for per-transaction stuff */
static pool *pchild;		/* Pool for httpd child stuff */
static pool *pmutex;            /* Pool for accept mutex in child */
static pool *pcommands;	/* Pool for -C and -c switches */

static int APACHE_TLS my_pid;	/* it seems silly to call getpid all the time */
static int my_child_num;


scoreboard *ap_scoreboard_image = NULL;

/*
 * Pieces for managing the contents of the Server response header
 * field.
 */
static char *server_version = NULL;
static int version_locked = 0;

/* Global, alas, so http_core can talk to us */
enum server_token_type ap_server_tokens = SrvTk_PRODUCT_ONLY;

/* Also global, for http_core and http_protocol */
API_VAR_EXPORT int ap_protocol_req_check = 1;

API_VAR_EXPORT int ap_change_shmem_uid = 0;

/*
 * This routine is called when the pconf pool is vacuumed.  It resets the
 * server version string to a known value and [re]enables modifications
 * (which are disabled by configuration completion). 
 */
static void reset_version(void *dummy)
{
    version_locked = 0;
    ap_server_tokens = SrvTk_PRODUCT_ONLY;
    server_version = NULL;
}

API_EXPORT(const char *) ap_get_server_version(void)
{
    return (server_version ? server_version : SERVER_BASEVERSION);
}

API_EXPORT(void) ap_add_version_component(const char *component)
{
    if (! version_locked) {
        /*
         * If the version string is null, register our cleanup to reset the
         * pointer on pool destruction. We also know that, if NULL,
	 * we are adding the original SERVER_BASEVERSION string.
         */
        if (server_version == NULL) {
	    ap_register_cleanup(pconf, NULL, (void (*)(void *))reset_version, 
				ap_null_cleanup);
	    server_version = ap_pstrdup(pconf, component);
	}
	else {
	    /*
	     * Tack the given component identifier to the end of
	     * the existing string.
	     */
	    server_version = ap_pstrcat(pconf, server_version, " ",
					component, NULL);
	}
    }
}

/*
 * This routine adds the real server base identity to the version string,
 * and then locks out changes until the next reconfig.
 */
static void ap_set_version(void)
{
    if (ap_server_tokens == SrvTk_PRODUCT_ONLY) {
	ap_add_version_component(SERVER_PRODUCT);
    }
    else if (ap_server_tokens == SrvTk_MIN) {
	ap_add_version_component(SERVER_BASEVERSION);
    }
    else {
	ap_add_version_component(SERVER_BASEVERSION " (" PLATFORM ")");
    }
    /*
     * Lock the server_version string if we're not displaying
     * the full set of tokens
     */
    if (ap_server_tokens != SrvTk_FULL) {
	version_locked++;
    }
}

API_EXPORT(void) ap_add_config_define(const char *define)
{
    char **var;
    var = (char **)ap_push_array(ap_server_config_defines);
    *var = ap_pstrdup(pcommands, define);
    return;
}

/*
 * Invoke the `close_connection' hook of modules to let them do
 * some connection dependent actions before we close it.
 */
static void ap_call_close_connection_hook(conn_rec *c)
{
    module *m;
    for (m = top_module; m != NULL; m = m->next)
        if (m->magic == MODULE_MAGIC_COOKIE_EAPI)
            if (m->close_connection != NULL)
                (*m->close_connection)(c);
    return;
}

static APACHE_TLS int volatile exit_after_unblock = 0;

#ifdef GPROF
/* 
 * change directory for gprof to plop the gmon.out file
 * configure in httpd.conf:
 * GprofDir logs/   -> $ServerRoot/logs/gmon.out
 * GprofDir logs/%  -> $ServerRoot/logs/gprof.$pid/gmon.out
 */
static void chdir_for_gprof(void)
{
    core_server_config *sconf = 
	ap_get_module_config(server_conf->module_config, &core_module);    
    char *dir = sconf->gprof_dir;

    if(dir) {
	char buf[512];
	int len = strlen(sconf->gprof_dir) - 1;
	if(*(dir + len) == '%') {
	    dir[len] = '\0';
	    ap_snprintf(buf, sizeof(buf), "%sgprof.%d", dir, (int)getpid());
	} 
	dir = ap_server_root_relative(pconf, buf[0] ? buf : dir);
	if(mkdir(dir, 0755) < 0 && errno != EEXIST) {
	    ap_log_error(APLOG_MARK, APLOG_ERR, server_conf,
			 "gprof: error creating directory %s", dir);
	}
    }
    else {
	dir = ap_server_root_relative(pconf, "logs");
    }

    chdir(dir);
}
#else
#define chdir_for_gprof()
#endif

/* a clean exit from a child with proper cleanup */
static void clean_child_exit(int code) __attribute__ ((noreturn));
static void clean_child_exit(int code)
{
    if (pchild) {
        /* make sure the accept mutex is released before calling child
         * exit hooks and cleanups...  otherwise, modules can segfault
         * in such code and, depending on the mutex mechanism, leave
         * the server deadlocked...  even if the module doesn't segfault,
         * if it performs extensive processing it can temporarily prevent
         * the server from accepting new connections
         */
        ap_clear_pool(pmutex);
	ap_child_exit_modules(pchild, server_conf);
	ap_destroy_pool(pchild);
    }
    chdir_for_gprof();
    exit(code);
}

/*
 * Start of accept() mutex fluff:
 *  Concept: Each method has it's own distinct set of mutex functions,
 *   which it shoves in a nice struct for us. We then pick
 *   which struct to use. We tell Apache which methods we
 *   support via HAVE_FOO_SERIALIZED_ACCEPT. We can
 *   specify the default via USE_FOO_SERIALIZED_ACCEPT
 *   (this pre-1.3.21 builds which use that at the command-
 *   line during builds work as expected). Without a set
 *   method, we pick the 1st from the following order:
 *   uslock, pthread, sysvsem, fcntl, flock, os2sem, tpfcore and none.
 */

static void expand_lock_fname(pool *p)
{
    /* XXXX possibly bogus cast */
    ap_lock_fname = ap_psprintf(p, "%s.%lu",
	ap_server_root_relative(p, ap_lock_fname), (unsigned long)getpid());
}

#include <sys/types.h>
#include <sys/ipc.h>
#include <sys/sem.h>

static int sem_id = -1;
static struct sembuf op_on;
static struct sembuf op_off;

/* We get a random semaphore ... the lame sysv semaphore interface
 * means we have to be sure to clean this up or else we'll leak
 * semaphores.
 */
static void accept_mutex_cleanup_sysvsem(void *foo)
{
    union semun ick;

    if (sem_id < 0)
	return;
    /* this is ignored anyhow */
    ick.val = 0;
    semctl(sem_id, 0, IPC_RMID, ick);
}

#define accept_mutex_child_init_sysvsem(x)

static void accept_mutex_init_sysvsem(pool *p)
{
    union semun ick;
    struct semid_ds buf;

    /* acquire the semaphore */
    sem_id = semget(IPC_PRIVATE, 1, IPC_CREAT | 0600);
    if (sem_id < 0) {
	perror("semget");
	exit(APEXIT_INIT);
    }
    ick.val = 1;
    if (semctl(sem_id, 0, SETVAL, ick) < 0) {
	perror("semctl(SETVAL)");
	exit(APEXIT_INIT);
    }
    if (!getuid()) {
	/* restrict it to use only by the appropriate user_id ... not that this
	 * stops CGIs from acquiring it and dinking around with it.
	 */
	buf.sem_perm.uid = ap_user_id;
	buf.sem_perm.gid = ap_group_id;
	buf.sem_perm.mode = 0600;
	ick.buf = &buf;
	if (semctl(sem_id, 0, IPC_SET, ick) < 0) {
	    perror("semctl(IPC_SET)");
	    exit(APEXIT_INIT);
	}
    }
    ap_register_cleanup(p, NULL, accept_mutex_cleanup_sysvsem, ap_null_cleanup);

    /* pre-initialize these */
    op_on.sem_num = 0;
    op_on.sem_op = -1;
    op_on.sem_flg = SEM_UNDO;
    op_off.sem_num = 0;
    op_off.sem_op = 1;
    op_off.sem_flg = SEM_UNDO;
}

static void accept_mutex_on_sysvsem(void)
{
    while (semop(sem_id, &op_on, 1) < 0) {
	if (errno != EINTR) {
	    perror("accept_mutex_on");
	    clean_child_exit(APEXIT_CHILDFATAL);
	}
    }
}

static void accept_mutex_off_sysvsem(void)
{
    while (semop(sem_id, &op_off, 1) < 0) {
	if (errno != EINTR) {
	    perror("accept_mutex_off");
	    clean_child_exit(APEXIT_CHILDFATAL);
	}
    }
}

accept_mutex_methods_s accept_mutex_sysvsem_s = {
    NULL,
    accept_mutex_init_sysvsem,
    accept_mutex_on_sysvsem,
    accept_mutex_off_sysvsem,
    "sysvsem"
};

static int flock_fd = -1;

static void accept_mutex_cleanup_flock(void *foo)
{
    unlink(ap_lock_fname);
}

/*
 * Initialize mutex lock.
 * Done by each child at it's birth
 */
static void accept_mutex_child_init_flock(pool *p)
{

    flock_fd = ap_popenf_ex(p, ap_lock_fname, O_WRONLY, 0600, 1);
    if (flock_fd == -1) {
	ap_log_error(APLOG_MARK, APLOG_EMERG, server_conf,
		    "Child cannot open lock file: %s", ap_lock_fname);
	clean_child_exit(APEXIT_CHILDINIT);
    }
}

/*
 * Initialize mutex lock.
 * Must be safe to call this on a restart.
 */
static void accept_mutex_init_flock(pool *p)
{
    expand_lock_fname(p);
    ap_server_strip_chroot(ap_lock_fname, 0);
    unlink(ap_lock_fname);
    flock_fd = ap_popenf_ex(p, ap_lock_fname, O_CREAT | O_WRONLY | O_EXCL, 0600, 1);
    if (flock_fd == -1) {
	ap_log_error(APLOG_MARK, APLOG_EMERG, server_conf,
		    "Parent cannot open lock file: %s", ap_lock_fname);
	exit(APEXIT_INIT);
    }
    ap_register_cleanup(p, NULL, accept_mutex_cleanup_flock, ap_null_cleanup);
}

static void accept_mutex_on_flock(void)
{
    int ret;

    while ((ret = flock(flock_fd, LOCK_EX)) < 0 && errno == EINTR)
	continue;

    if (ret < 0) {
	ap_log_error(APLOG_MARK, APLOG_EMERG, server_conf,
		    "flock: LOCK_EX: Error getting accept lock. Exiting!");
	clean_child_exit(APEXIT_CHILDFATAL);
    }
}

static void accept_mutex_off_flock(void)
{
    if (flock(flock_fd, LOCK_UN) < 0) {
	ap_log_error(APLOG_MARK, APLOG_EMERG, server_conf,
		    "flock: LOCK_UN: Error freeing accept lock. Exiting!");
	clean_child_exit(APEXIT_CHILDFATAL);
    }
}

accept_mutex_methods_s accept_mutex_flock_s = {
    accept_mutex_child_init_flock,
    accept_mutex_init_flock,
    accept_mutex_on_flock,
    accept_mutex_off_flock,
    "flock"
};

#define AP_FPTR1(x,y)	{ if (x) ((* x)(y)); }
#define AP_FPTR0(x)	{ if (x) ((* x)()); }

#define accept_mutex_child_init(x) 	AP_FPTR1(amutex->child_init,x)
#define accept_mutex_init(x) 		AP_FPTR1(amutex->init,x)
#define accept_mutex_off() 		AP_FPTR0(amutex->off)
#define accept_mutex_on() 		AP_FPTR0(amutex->on)

char *ap_default_mutex_method(void)
{
    char *t;
    t = "sysvsem";
    if ((!(strcasecmp(t,"default"))) || (!(strcasecmp(t,"sysvsem"))))
    	return "sysvsem";
    if ((!(strcasecmp(t,"default"))) || (!(strcasecmp(t,"flock"))))
    	return "flock";
    fprintf(stderr, "No default accept serialization known!!\n");
    exit(APEXIT_INIT);
    /*NOTREACHED */
    return "unknown";
}

char *ap_init_mutex_method(char *t)
{
    if (!(strcasecmp(t,"default")))
	t = ap_default_mutex_method();

    if (!(strcasecmp(t,"sysvsem"))) {
    	amutex = &accept_mutex_sysvsem_s;
    } else 
    if (!(strcasecmp(t,"flock"))) {
    	amutex = &accept_mutex_flock_s;
    } else 
    {
/* Ignore this directive on Windows */
    if (server_conf) {
        ap_log_error(APLOG_MARK, APLOG_NOERRNO|APLOG_NOTICE, server_conf,
                    "Requested serialization method '%s' not available",t);
        exit(APEXIT_INIT);
    } else {
        fprintf(stderr, "Requested serialization method '%s' not available\n", t);
        exit(APEXIT_INIT);
    }
    }
    return NULL;
}

/* On some architectures it's safe to do unserialized accept()s in the single
 * Listen case.  But it's never safe to do it in the case where there's
 * multiple Listen statements.  Define SINGLE_LISTEN_UNSERIALIZED_ACCEPT
 * when it's safe in the single Listen case.
 */
#define SAFE_ACCEPT(stmt) do {if(ap_listeners->next != ap_listeners) {stmt;}} while(0)

static void usage(char *bin)
{
    char pad[MAX_STRING_LEN];
    unsigned i;

    for (i = 0; i < strlen(bin); i++)
	pad[i] = ' ';
    pad[i] = '\0';
    fprintf(stderr, "Usage: %s [-FhLlSTtuVvX] [-C directive] [-c directive] [-D parameter]\n", bin);
    fprintf(stderr, "       %s [-d serverroot] [-f config]\n", pad);
    fprintf(stderr, "Options:\n");
    fprintf(stderr, "  -C directive     : process directive before reading config files\n");
    fprintf(stderr, "  -c directive     : process directive after  reading config files\n");
    fprintf(stderr, "  -D parameter     : define a parameter for use in <IfDefine name> directives\n");
    fprintf(stderr, "  -d serverroot    : specify an alternate initial ServerRoot\n");
    fprintf(stderr, "  -F               : run main process in foreground, for process supervisors\n");
    fprintf(stderr, "  -f config        : specify an alternate ServerConfigFile\n");
    fprintf(stderr, "  -h               : list available command line options (this page)\n");
    fprintf(stderr, "  -L               : list available configuration directives\n");
    fprintf(stderr, "  -l               : list compiled-in modules\n");
    fprintf(stderr, "  -S               : show parsed settings (currently only vhost settings)\n");
    fprintf(stderr, "  -T               : run syntax check for config files (without docroot check)\n");
    fprintf(stderr, "  -t               : run syntax check for config files (with docroot check)\n");
    fprintf(stderr, "  -u               : unsecure mode: do not chroot into ServerRoot\n");
    fprintf(stderr, "  -V               : show compile settings\n");
    fprintf(stderr, "  -v               : show version number\n");
    fprintf(stderr, "  -X               : run in single-process mode\n");

    exit(1);
}


/*****************************************************************
 *
 * Timeout handling.  DISTINCTLY not thread-safe, but all this stuff
 * has to change for threads anyway.  Note that this code allows only
 * one timeout in progress at a time...
 */

static APACHE_TLS conn_rec *volatile current_conn;
static APACHE_TLS request_rec *volatile timeout_req;
static APACHE_TLS const char *volatile timeout_name = NULL;
static APACHE_TLS int volatile alarms_blocked = 0;
static APACHE_TLS int volatile alarm_pending = 0;


static void timeout(int sig)
{
    void *dirconf;
    if (alarms_blocked) {
	alarm_pending = 1;
	return;
    }
    if (exit_after_unblock) {
	clean_child_exit(0);
    }

    if (!current_conn) {
	ap_longjmp(jmpbuffer, 1);
    }

    if (timeout_req != NULL)
	dirconf = timeout_req->per_dir_config;
    else
	dirconf = current_conn->server->lookup_defaults;
    if (!current_conn->keptalive) {
	ap_log_error(APLOG_MARK, APLOG_NOERRNO|APLOG_INFO,
		     current_conn->server, "[client %s] %s timed out",
		     current_conn->remote_ip,
		     timeout_name ? timeout_name : "request");
    }

    if (timeout_req) {
	/* Someone has asked for this transaction to just be aborted
	 * if it times out...
	 */
	request_rec *log_req = timeout_req;
	request_rec *save_req = timeout_req;

	/* avoid looping... if ap_log_transaction started another
	 * timer (say via rfc1413.c) we could loop...
	 */
	timeout_req = NULL;

	while (log_req->main || log_req->prev) {
	    /* Get back to original request... */
	    if (log_req->main)
		log_req = log_req->main;
	    else
		log_req = log_req->prev;
	}

	if (!current_conn->keptalive) {
	    /* in some cases we come here before setting the time */
	    if (log_req->request_time == 0) {
                log_req->request_time = time(NULL);
	    }
	    ap_log_transaction(log_req);
	}

	ap_call_close_connection_hook(save_req->connection);

	ap_bsetflag(save_req->connection->client, B_EOUT, 1);
	ap_bclose(save_req->connection->client);
	
	if (!ap_standalone)
	    exit(0);
        ap_longjmp(jmpbuffer, 1);
    }
    else {			/* abort the connection */
	ap_call_close_connection_hook(current_conn);
	ap_bsetflag(current_conn->client, B_EOUT, 1);
	ap_bclose(current_conn->client);
	current_conn->aborted = 1;
    }
}


/*
 * These two called from alloc.c to protect its critical sections...
 * Note that they can nest (as when destroying the sub_pools of a pool
 * which is itself being cleared); we have to support that here.
 */

API_EXPORT(void) ap_block_alarms(void)
{
    ++alarms_blocked;
}

API_EXPORT(void) ap_unblock_alarms(void)
{
    --alarms_blocked;
    if (alarms_blocked == 0) {
	if (exit_after_unblock) {
	    /* We have a couple race conditions to deal with here, we can't
	     * allow a timeout that comes in this small interval to allow
	     * the child to jump back to the main loop.  Instead we block
	     * alarms again, and then note that exit_after_unblock is
	     * being dealt with.  We choose this way to solve this so that
	     * the common path through unblock_alarms() is really short.
	     */
	    ++alarms_blocked;
	    exit_after_unblock = 0;
	    clean_child_exit(0);
	}
	if (alarm_pending) {
	    alarm_pending = 0;
	    timeout(0);
	}
    }
}

static APACHE_TLS void (*volatile alarm_fn) (int) = NULL;

static void alrm_handler(int sig)
{
    if (alarm_fn) {
	(*alarm_fn) (sig);
    }
}

API_EXPORT(unsigned int) ap_set_callback_and_alarm(void (*fn) (int), int x)
{
    unsigned int old;

    if (alarm_fn && x && fn != alarm_fn) {
	ap_log_error(APLOG_MARK, APLOG_NOERRNO|APLOG_DEBUG, NULL,
	    "ap_set_callback_and_alarm: possible nested timer!");
    }
    alarm_fn = fn;
    if (child_timeouts) {
	old = alarm(x);
    }
    else {
	/* Just note the timeout in our scoreboard, no need to call the system.
	 * We also note that the virtual time has gone forward.
	 */
	ap_check_signals();
	old = ap_scoreboard_image->servers[my_child_num].timeout_len;
	ap_scoreboard_image->servers[my_child_num].timeout_len = x;
	++ap_scoreboard_image->servers[my_child_num].cur_vtime;
    }
    return (old);
}


/* reset_timeout (request_rec *) resets the timeout in effect,
 * as long as it hasn't expired already.
 */

API_EXPORT(void) ap_reset_timeout(request_rec *r)
{
    int i;
    if (timeout_name) {		/* timeout has been set */
	i = ap_set_callback_and_alarm(alarm_fn, r->server->timeout);
	if (i == 0)		/* timeout already expired, so set it back to 0 */
	    ap_set_callback_and_alarm(alarm_fn, 0);
    }
}




API_EXPORT(void) ap_keepalive_timeout(char *name, request_rec *r)
{
    unsigned int to;
    timeout_req = r;
    timeout_name = name;
    if (r->connection->keptalive)
	to = r->server->keep_alive_timeout;
    else
	to = r->server->timeout;
    ap_set_callback_and_alarm(timeout, to);
}

API_EXPORT(void) ap_hard_timeout(char *name, request_rec *r)
{
    timeout_req = r;
    timeout_name = name;
    ap_set_callback_and_alarm(timeout, r->server->timeout);
}

API_EXPORT(void) ap_soft_timeout(char *name, request_rec *r)
{
    timeout_name = name;
    ap_set_callback_and_alarm(timeout, r->server->timeout);
}

API_EXPORT(void) ap_kill_timeout(request_rec *dummy)
{
    ap_check_signals();
    ap_set_callback_and_alarm(NULL, 0);
    timeout_req = NULL;
    timeout_name = NULL;
}


/*
 * More machine-dependent networking gooo... on some systems,
 * you've got to be *really* sure that all the packets are acknowledged
 * before closing the connection, since the client will not be able
 * to see the last response if their TCP buffer is flushed by a RST
 * packet from us, which is what the server's TCP stack will send
 * if it receives any request data after closing the connection.
 *
 * In an ideal world, this function would be accomplished by simply
 * setting the socket option SO_LINGER and handling it within the
 * server's TCP stack while the process continues on to the next request.
 * Unfortunately, it seems that most (if not all) operating systems
 * block the server process on close() when SO_LINGER is used.
 * For those that don't, see USE_SO_LINGER below.  For the rest,
 * we have created a home-brew lingering_close.
 *
 * Many operating systems tend to block, puke, or otherwise mishandle
 * calls to shutdown only half of the connection.
 */
#ifndef MAX_SECS_TO_LINGER
#define MAX_SECS_TO_LINGER 30
#endif

#define sock_enable_linger(s)	/* NOOP */

/* Special version of timeout for lingering_close */

static void lingerout(int sig)
{
    if (alarms_blocked) {
	alarm_pending = 1;
	return;
    }

    if (!current_conn) {
	ap_longjmp(jmpbuffer, 1);
    }
    ap_bsetflag(current_conn->client, B_EOUT, 1);
    current_conn->aborted = 1;
}

static void linger_timeout(void)
{
    timeout_name = "lingering close";
    ap_set_callback_and_alarm(lingerout, MAX_SECS_TO_LINGER);
}

/* Since many clients will abort a connection instead of closing it,
 * attempting to log an error message from this routine will only
 * confuse the webmaster.  There doesn't seem to be any portable way to
 * distinguish between a dropped connection and something that might be
 * worth logging.
 */
static void lingering_close(request_rec *r)
{
    char dummybuf[512];
    struct timeval tv;
    fd_set lfds;
    int select_rv;
    int lsd;

    /* Prevent a slow-drip client from holding us here indefinitely */

    linger_timeout();

    /* Send any leftover data to the client, but never try to again */

    if (ap_bflush(r->connection->client) == -1) {
	ap_call_close_connection_hook(r->connection);
	ap_kill_timeout(r);
	ap_bclose(r->connection->client);
	return;
    }
    ap_call_close_connection_hook(r->connection);
    ap_bsetflag(r->connection->client, B_EOUT, 1);

    /* Close our half of the connection --- send the client a FIN */

    lsd = r->connection->client->fd;

    if ((shutdown(lsd, 1) != 0) || r->connection->aborted) {
	ap_kill_timeout(r);
	ap_bclose(r->connection->client);
	return;
    }

    /* Set up to wait for readable data on socket... */

    FD_ZERO(&lfds);

    /* Wait for readable data or error condition on socket;
     * slurp up any data that arrives...  We exit when we go for an
     * interval of tv length without getting any more data, get an error
     * from select(), get an error or EOF on a read, or the timer expires.
     */

    do {
	/* We use a 2 second timeout because current (Feb 97) browsers
	 * fail to close a connection after the server closes it.  Thus,
	 * to avoid keeping the child busy, we are only lingering long enough
	 * for a client that is actively sending data on a connection.
	 * This should be sufficient unless the connection is massively
	 * losing packets, in which case we might have missed the RST anyway.
	 * These parameters are reset on each pass, since they might be
	 * changed by select.
	 */

	FD_SET(lsd, &lfds);
	tv.tv_sec = 2;
	tv.tv_usec = 0;

	select_rv = ap_select(lsd + 1, &lfds, NULL, NULL, &tv);

    } while ((select_rv > 0) &&
             (read(lsd, dummybuf, sizeof(dummybuf)) > 0));

    /* Should now have seen final ack.  Safe to finally kill socket */

    ap_bclose(r->connection->client);

    ap_kill_timeout(r);
}

/*****************************************************************
 * dealing with other children
 */

API_EXPORT(void) ap_register_other_child(int pid,
		       void (*maintenance) (int reason, void *, ap_wait_t status),
			  void *data, int write_fd)
{
    other_child_rec *ocr;

    ocr = ap_palloc(pconf, sizeof(*ocr));
    ocr->pid = pid;
    ocr->maintenance = maintenance;
    ocr->data = data;
    ocr->write_fd = write_fd;
    ocr->next = other_children;
    other_children = ocr;
}

/* note that since this can be called by a maintenance function while we're
 * scanning the other_children list, all scanners should protect themself
 * by loading ocr->next before calling any maintenance function.
 */
API_EXPORT(void) ap_unregister_other_child(void *data)
{
    other_child_rec **pocr, *nocr;

    for (pocr = &other_children; *pocr; pocr = &(*pocr)->next) {
	if ((*pocr)->data == data) {
	    nocr = (*pocr)->next;
	    (*(*pocr)->maintenance) (OC_REASON_UNREGISTER, (*pocr)->data, (ap_wait_t)-1);
	    *pocr = nocr;
	    /* XXX: um, well we've just wasted some space in pconf ? */
	    return;
	}
    }
}

/* test to ensure that the write_fds are all still writable, otherwise
 * invoke the maintenance functions as appropriate */
static void probe_writable_fds(void)
{
    fd_set writable_fds;
    int fd_max;
    other_child_rec *ocr, *nocr;
    struct timeval tv;
    int rc;

    if (other_children == NULL)
	return;

    fd_max = 0;
    FD_ZERO(&writable_fds);
    do {
	for (ocr = other_children; ocr; ocr = ocr->next) {
	    if (ocr->write_fd == -1)
		continue;
	    FD_SET(ocr->write_fd, &writable_fds);
	    if (ocr->write_fd > fd_max) {
		fd_max = ocr->write_fd;
	    }
	}
	if (fd_max == 0)
	    return;

	tv.tv_sec = 0;
	tv.tv_usec = 0;
	rc = ap_select(fd_max + 1, NULL, &writable_fds, NULL, &tv);
    } while (rc == -1 && errno == EINTR);

    if (rc == -1) {
	/* XXX: uhh this could be really bad, we could have a bad file
	 * descriptor due to a bug in one of the maintenance routines */
	ap_log_unixerr("probe_writable_fds", "select",
		    "could not probe writable fds", server_conf);
	return;
    }
    if (rc == 0)
	return;

    for (ocr = other_children; ocr; ocr = nocr) {
	nocr = ocr->next;
	if (ocr->write_fd == -1)
	    continue;
	if (FD_ISSET(ocr->write_fd, &writable_fds))
	    continue;
	(*ocr->maintenance) (OC_REASON_UNWRITABLE, ocr->data, (ap_wait_t)-1);
    }
}

/* possibly reap an other_child, return 0 if yes, -1 if not */
static int reap_other_child(int pid, ap_wait_t status)
{
    other_child_rec *ocr, *nocr;

    for (ocr = other_children; ocr; ocr = nocr) {
	nocr = ocr->next;
	if (ocr->pid != pid)
	    continue;
	ocr->pid = -1;
	(*ocr->maintenance) (OC_REASON_DEATH, ocr->data, status);
	return 0;
    }
    return -1;
}

/*****************************************************************
 *
 * Dealing with the scoreboard... a lot of these variables are global
 * only to avoid getting clobbered by the longjmp() that happens when
 * a hard timeout expires...
 *
 * We begin with routines which deal with the file itself... 
 */

static void setup_shared_mem(pool *p)
{
    caddr_t m;

/* BSD style */
    m = mmap((caddr_t) 0, SCOREBOARD_SIZE,
	     PROT_READ | PROT_WRITE, MAP_ANON | MAP_SHARED, -1, 0);
    if (m == (caddr_t) - 1) {
	perror("mmap");
	fprintf(stderr, "%s: Could not mmap memory\n", ap_server_argv0);
	exit(APEXIT_INIT);
    }
    ap_scoreboard_image = (scoreboard *) m;
    ap_scoreboard_image->global.running_generation = 0;
}

/* Called by parent process */
static void reinit_scoreboard(pool *p)
{
    int running_gen = 0;
    if (ap_scoreboard_image)
	running_gen = ap_scoreboard_image->global.running_generation;

    if (ap_scoreboard_image == NULL) {
	setup_shared_mem(p);
    }
    memset(ap_scoreboard_image, 0, SCOREBOARD_SIZE);
    ap_scoreboard_image->global.running_generation = running_gen;
}

/* Routines called to deal with the scoreboard image
 * --- note that we do *not* need write locks, since update_child_status
 * only updates a *single* record in place, and only one process writes to
 * a given scoreboard slot at a time (either the child process owning that
 * slot, or the parent, noting that the child has died).
 *
 * As a final note --- setting the score entry to getpid() is always safe,
 * since when the parent is writing an entry, it's only noting SERVER_DEAD
 * anyway.
 */

API_EXPORT(int) ap_exists_scoreboard_image(void)
{
    return (ap_scoreboard_image ? 1 : 0);
}

/* a clean exit from the parent with proper cleanup */
static void clean_parent_exit(int code) __attribute__((noreturn));
static void clean_parent_exit(int code)
{
    /* Clear the pool - including any registered cleanups */
    ap_destroy_pool(pglobal);
    ap_kill_alloc_shared();
    fdcache_closeall();
    exit(code);
}

API_EXPORT(int) ap_update_child_status(int child_num, int status, request_rec *r)
{
    int old_status;
    short_score *ss;

    if (child_num < 0)
	return -1;

    ap_check_signals();

    ss = &ap_scoreboard_image->servers[child_num];
    old_status = ss->status;
    ss->status = status;

    ++ss->cur_vtime;

    if (ap_extended_status) {
	if (status == SERVER_READY || status == SERVER_DEAD) {
	    /*
	     * Reset individual counters
	     */
	    if (status == SERVER_DEAD) {
		ss->my_access_count = 0L;
		ss->my_bytes_served = 0L;
	    }
	    ss->conn_count = (unsigned short) 0;
	    ss->conn_bytes = (unsigned long) 0;
	}
        else if (status == SERVER_STARTING) {
            /* clean out the start_time so that mod_status will print Req=0 */
            /* Use memset to be independent from the type (struct timeval vs. clock_t) */
            memset (&ss->start_time, '\0', sizeof ss->start_time);
        }
	if (r) {
	    conn_rec *c = r->connection;
	    ap_cpystrn(ss->client, ap_get_remote_host(c, r->per_dir_config,
				  REMOTE_NOLOOKUP), sizeof(ss->client));
	    if (r->the_request == NULL) {
		    ap_cpystrn(ss->request, "NULL", sizeof(ss->request));
	    } else if (r->parsed_uri.password == NULL) {
		    ap_cpystrn(ss->request, r->the_request, sizeof(ss->request));
	    } else {
		/* Don't reveal the password in the server-status view */
		    ap_cpystrn(ss->request, ap_pstrcat(r->pool, r->method, " ",
					       ap_unparse_uri_components(r->pool, &r->parsed_uri, UNP_OMITPASSWORD),
					       r->assbackwards ? NULL : " ", r->protocol, NULL),
				       sizeof(ss->request));
	    }
	    ss->vhostrec =  r->server;
	}
    }
    if (status == SERVER_STARTING && r == NULL) {
	/* clean up the slot's vhostrec pointer (maybe re-used)
	 * and mark the slot as belonging to a new generation.
	 */
	ss->vhostrec = NULL;
	ap_scoreboard_image->parent[child_num].generation = ap_my_generation;
    }

    return old_status;
}

void ap_time_process_request(int child_num, int status)
{
    short_score *ss;

    if (child_num < 0)
	return;

    ss = &ap_scoreboard_image->servers[child_num];

    if (status == START_PREQUEST) {
	if (gettimeofday(&ss->start_time, (struct timezone *) 0) < 0)
	    ss->start_time.tv_sec =
		ss->start_time.tv_usec = 0L;
    }
    else if (status == STOP_PREQUEST) {
	if (gettimeofday(&ss->stop_time, (struct timezone *) 0) < 0)
	    ss->stop_time.tv_sec =
		ss->stop_time.tv_usec =
		ss->start_time.tv_sec =
		ss->start_time.tv_usec = 0L;

    }
}

static void increment_counts(int child_num, request_rec *r)
{
    long int bs = 0;
    short_score *ss;

    ss = &ap_scoreboard_image->servers[child_num];

    if (r->sent_bodyct)
	ap_bgetopt(r->connection->client, BO_BYTECT, &bs);

    times(&ss->times);
    ss->access_count++;
    ss->my_access_count++;
    ss->conn_count++;
    ss->bytes_served += (unsigned long) bs;
    ss->my_bytes_served += (unsigned long) bs;
    ss->conn_bytes += (unsigned long) bs;
}

static int find_child_by_pid(int pid)
{
    int i;

    for (i = 0; i < max_daemons_limit; ++i)
	if (ap_scoreboard_image->parent[i].pid == pid)
	    return i;

    return -1;
}

static int safe_child_kill(pid_t pid, int sig)
{
    if (getpgid(pid) == getpgrp()) {
        return kill(pid, sig);
    }
    else {
        errno = EINVAL;
        return -1;
    }
}

static void reclaim_child_processes(int terminate)
{
    int i, status;
    long int waittime = 1024 * 16;	/* in usecs */
    struct timeval tv;
    int waitret, tries;
    int not_dead_yet;
    int ret;
    other_child_rec *ocr, *nocr;

    for (tries = terminate ? 4 : 1; tries <= 12; ++tries) {
	/* don't want to hold up progress any more than 
	 * necessary, but we need to allow children a few moments to exit.
	 * Set delay with an exponential backoff. NOTE: if we get
 	 * interrupted, we'll wait longer than expected...
	 */
	tv.tv_sec = waittime / 1000000;
	tv.tv_usec = waittime % 1000000;
	waittime = waittime * 4;
	do {
	    ret = ap_select(0, NULL, NULL, NULL, &tv);
	} while (ret == -1 && errno == EINTR);

	/* now see who is done */
	not_dead_yet = 0;
	for (i = 0; i < max_daemons_limit; ++i) {
	    int pid = ap_scoreboard_image->parent[i].pid;

	    if (pid == my_pid || pid == 0)
		continue;

	    waitret = waitpid(pid, &status, WNOHANG);
	    if (waitret == pid || waitret == -1) {
		ap_scoreboard_image->parent[i].pid = 0;
		continue;
	    }
	    ++not_dead_yet;
	    switch (tries) {
	    case 1:     /*  16ms */
	    case 2:     /*  82ms */
		break;
	    case 3:     /* 344ms */
		/* perhaps it missed the SIGHUP, lets try again */
		ap_log_error(APLOG_MARK, APLOG_NOERRNO|APLOG_WARNING,
			    server_conf,
		    "child process %d did not exit, sending another SIGHUP",
			    pid);
		safe_child_kill(pid, SIGHUP);
		waittime = 1024 * 16;
		break;
	    case 4:     /*  16ms */
	    case 5:     /*  82ms */
	    case 6:     /* 344ms */
		break;
	    case 7:     /* 1.4sec */
		/* ok, now it's being annoying */
		ap_log_error(APLOG_MARK, APLOG_NOERRNO|APLOG_WARNING,
			    server_conf,
		   "child process %d still did not exit, sending a SIGTERM",
			    pid);
		safe_child_kill(pid, SIGTERM);
		break;
	    case 8:     /*  6 sec */
		/* die child scum */
		ap_log_error(APLOG_MARK, APLOG_NOERRNO|APLOG_ERR, server_conf,
		   "child process %d still did not exit, sending a SIGKILL",
			    pid);
		safe_child_kill(pid, SIGKILL);
		waittime = 1024 * 16; /* give them some time to die */
		break;
	    case 9:     /*   6 sec */
	    case 10:    /* 6.1 sec */
	    case 11:    /* 6.4 sec */
		break;
	    case 12:    /* 7.4 sec */
		/* gave it our best shot, but alas...  If this really 
		 * is a child we are trying to kill and it really hasn't
		 * exited, we will likely fail to bind to the port
		 * after the restart.
		 */
		ap_log_error(APLOG_MARK, APLOG_NOERRNO|APLOG_ERR, server_conf,
			    "could not make child process %d exit, "
			    "attempting to continue anyway", pid);
		break;
	    }
	}
	for (ocr = other_children; ocr; ocr = nocr) {
	    nocr = ocr->next;
	    if (ocr->pid == -1)
		continue;

	    waitret = waitpid(ocr->pid, &status, WNOHANG);
	    if (waitret == ocr->pid) {
		ocr->pid = -1;
		(*ocr->maintenance) (OC_REASON_RESTART, ocr->data, (ap_wait_t)status);
	    }
	    else if (waitret == 0) {
		(*ocr->maintenance) (OC_REASON_RESTART, ocr->data, (ap_wait_t)-1);
		++not_dead_yet;
	    }
	    else if (waitret == -1) {
		/* uh what the heck? they didn't call unregister? */
		ocr->pid = -1;
		(*ocr->maintenance) (OC_REASON_LOST, ocr->data, (ap_wait_t)-1);
	    }
	}
	if (!not_dead_yet) {
	    /* nothing left to wait for */
	    break;
	}
    }
}


/* Finally, this routine is used by the caretaker process to wait for
 * a while...
 */

/* number of calls to wait_or_timeout between writable probes */
#ifndef INTERVAL_OF_WRITABLE_PROBES
#define INTERVAL_OF_WRITABLE_PROBES 10
#endif
static int wait_or_timeout_counter;

static int wait_or_timeout(ap_wait_t *status)
{
    struct timeval tv;
    int ret;

    ++wait_or_timeout_counter;
    if (wait_or_timeout_counter == INTERVAL_OF_WRITABLE_PROBES) {
	wait_or_timeout_counter = 0;
	probe_writable_fds();
    }
    ret = waitpid(-1, status, WNOHANG);
    if (ret == -1 && errno == EINTR) {
	return -1;
    }
    if (ret > 0) {
	return ret;
    }
    tv.tv_sec = SCOREBOARD_MAINTENANCE_INTERVAL / 1000000;
    tv.tv_usec = SCOREBOARD_MAINTENANCE_INTERVAL % 1000000;
    ap_select(0, NULL, NULL, NULL, &tv);
    return -1;
}

#if defined(NSIG)
#define NumSIG NSIG
#elif defined(_NSIG)
#define NumSIG _NSIG
#elif defined(__NSIG)
#define NumSIG __NSIG
#else
#define NumSIG 32   /* for 1998's unixes, this is still a good assumption */
#endif

#define SYS_SIGLIST ap_sys_siglist
#define INIT_SIGLIST() siglist_init();

const char *ap_sys_siglist[NumSIG];

static void siglist_init(void)
{
    int sig;

    ap_sys_siglist[0] = "Signal 0";
    ap_sys_siglist[SIGHUP] = "Hangup";
    ap_sys_siglist[SIGINT] = "Interrupt";
    ap_sys_siglist[SIGQUIT] = "Quit";
    ap_sys_siglist[SIGILL] = "Illegal instruction";
    ap_sys_siglist[SIGTRAP] = "Trace/BPT trap";
    ap_sys_siglist[SIGIOT] = "IOT instruction";
    ap_sys_siglist[SIGABRT] = "Abort";
    ap_sys_siglist[SIGEMT] = "Emulator trap";
    ap_sys_siglist[SIGFPE] = "Arithmetic exception";
    ap_sys_siglist[SIGKILL] = "Killed";
    ap_sys_siglist[SIGBUS] = "Bus error";
    ap_sys_siglist[SIGSEGV] = "Segmentation fault";
    ap_sys_siglist[SIGSYS] = "Bad system call";
    ap_sys_siglist[SIGPIPE] = "Broken pipe";
    ap_sys_siglist[SIGALRM] = "Alarm clock";
    ap_sys_siglist[SIGTERM] = "Terminated";
    ap_sys_siglist[SIGUSR1] = "User defined signal 1";
    ap_sys_siglist[SIGUSR2] = "User defined signal 2";
    ap_sys_siglist[SIGCHLD] = "Child status change";
    ap_sys_siglist[SIGWINCH] = "Window changed";
    ap_sys_siglist[SIGURG] = "urgent socket condition";
    ap_sys_siglist[SIGIO] = "socket I/O possible";
    ap_sys_siglist[SIGSTOP] = "Stopped (signal)";
    ap_sys_siglist[SIGTSTP] = "Stopped";
    ap_sys_siglist[SIGCONT] = "Continued";
    ap_sys_siglist[SIGTTIN] = "Stopped (tty input)";
    ap_sys_siglist[SIGTTOU] = "Stopped (tty output)";
    ap_sys_siglist[SIGVTALRM] = "virtual timer expired";
    ap_sys_siglist[SIGPROF] = "profiling timer expired";
    ap_sys_siglist[SIGXCPU] = "exceeded cpu limit";
    ap_sys_siglist[SIGXFSZ] = "exceeded file size limit";
    for (sig=0; sig < sizeof(ap_sys_siglist)/sizeof(ap_sys_siglist[0]); ++sig)
        if (ap_sys_siglist[sig] == NULL)
            ap_sys_siglist[sig] = "";
}

/* handle all varieties of core dumping signals */
static void sig_coredump(int sig)
{
    chdir(ap_coredump_dir);
    signal(sig, SIG_DFL);
    kill(getpid(), sig);
    /* At this point we've got sig blocked, because we're still inside
     * the signal handler.  When we leave the signal handler it will
     * be unblocked, and we'll take the signal... and coredump or whatever
     * is appropriate for this particular Unix.  In addition the parent
     * will see the real signal we received -- whereas if we called
     * abort() here, the parent would only see SIGABRT.
     */
}

/*****************************************************************
 * Connection structures and accounting...
 */

static void just_die(int sig)
{				/* SIGHUP to child process??? */
    /* if alarms are blocked we have to wait to die otherwise we might
     * end up with corruption in alloc.c's internal structures */
    if (alarms_blocked) {
	exit_after_unblock = 1;
    }
    else {
	clean_child_exit(0);
    }
}

static int volatile usr1_just_die = 1;
static int volatile deferred_die;

static void usr1_handler(int sig)
{
    if (usr1_just_die) {
	just_die(sig);
    }
    deferred_die = 1;
}

/* volatile just in case */
static int volatile shutdown_pending;
static int volatile restart_pending;
static int volatile is_graceful;
API_VAR_EXPORT ap_generation_t volatile ap_my_generation=0;


/*
 * ap_start_shutdown() and ap_start_restart(), below, are a first stab at
 * functions to initiate shutdown or restart without relying on signals. 
 * Previously this was initiated in sig_term() and restart() signal handlers, 
 * but we want to be able to start a shutdown/restart from other sources --
 * e.g. on Win32, from the service manager. Now the service manager can
 * call ap_start_shutdown() or ap_start_restart() as appropiate.  Note that
 * these functions can also be called by the child processes, since global
 * variables are no longer used to pass on the required action to the parent.
 */

API_EXPORT(void) ap_start_shutdown(void)
{
    if (shutdown_pending == 1) {
	/* Um, is this _probably_ not an error, if the user has
	 * tried to do a shutdown twice quickly, so we won't
	 * worry about reporting it.
	 */
	return;
    }
    shutdown_pending = 1;
}

/* do a graceful restart if graceful == 1 */
API_EXPORT(void) ap_start_restart(int graceful)
{
    if (restart_pending == 1) {
	/* Probably not an error - don't bother reporting it */
	return;
    }
    restart_pending = 1;
    is_graceful = graceful;
}

static void sig_term(int sig)
{
    ap_start_shutdown();
}

static void restart(int sig)
{
    ap_start_restart(sig == SIGUSR1);
}

static void set_signals(void)
{
    struct sigaction sa;

    sigemptyset(&sa.sa_mask);
    sa.sa_flags = 0;

    if (!one_process) {
	sa.sa_handler = sig_coredump;
	sa.sa_flags = SA_RESETHAND;
	if (sigaction(SIGBUS, &sa, NULL) < 0)
	    ap_log_error(APLOG_MARK, APLOG_WARNING, server_conf, "sigaction(SIGBUS)");
	if (sigaction(SIGABRT, &sa, NULL) < 0)
	    ap_log_error(APLOG_MARK, APLOG_WARNING, server_conf, "sigaction(SIGABRT)");
	if (sigaction(SIGILL, &sa, NULL) < 0)
	    ap_log_error(APLOG_MARK, APLOG_WARNING, server_conf, "sigaction(SIGILL)");
	sa.sa_flags = 0;
    }
    sa.sa_handler = sig_term;
    if (sigaction(SIGTERM, &sa, NULL) < 0)
	ap_log_error(APLOG_MARK, APLOG_WARNING, server_conf, "sigaction(SIGTERM)");
    if (sigaction(SIGINT, &sa, NULL) < 0)
        ap_log_error(APLOG_MARK, APLOG_WARNING, server_conf, "sigaction(SIGINT)");
    sa.sa_handler = SIG_DFL;
    if (sigaction(SIGXCPU, &sa, NULL) < 0)
	ap_log_error(APLOG_MARK, APLOG_WARNING, server_conf, "sigaction(SIGXCPU)");
    sa.sa_handler = SIG_DFL;
    if (sigaction(SIGXFSZ, &sa, NULL) < 0)
	ap_log_error(APLOG_MARK, APLOG_WARNING, server_conf, "sigaction(SIGXFSZ)");
    sa.sa_handler = SIG_IGN;
    if (sigaction(SIGPIPE, &sa, NULL) < 0)
	ap_log_error(APLOG_MARK, APLOG_WARNING, server_conf, "sigaction(SIGPIPE)");

    /* we want to ignore HUPs and USR1 while we're busy processing one */
    sigaddset(&sa.sa_mask, SIGHUP);
    sigaddset(&sa.sa_mask, SIGUSR1);
    sa.sa_handler = restart;
    if (sigaction(SIGHUP, &sa, NULL) < 0)
	ap_log_error(APLOG_MARK, APLOG_WARNING, server_conf, "sigaction(SIGHUP)");
    if (sigaction(SIGUSR1, &sa, NULL) < 0)
	ap_log_error(APLOG_MARK, APLOG_WARNING, server_conf, "sigaction(SIGUSR1)");
}


/*****************************************************************
 * Here follows a long bunch of generic server bookkeeping stuff...
 */

static void detach(void)
{
    int x;

    chdir("/");
    if (do_detach) {
        if ((x = fork()) > 0)
            exit(0);
        else if (x == -1) {
            perror("fork");
	    fprintf(stderr, "%s: unable to fork new process\n", ap_server_argv0);
	    exit(1);
        }
        RAISE_SIGSTOP(DETACH);
    }
    if ((pgrp = setsid()) == -1) {
	perror("setsid");
	fprintf(stderr, "%s: setsid failed\n", ap_server_argv0);
	if (!do_detach) 
	    fprintf(stderr, "setsid() failed probably because you aren't "
		"running under a process management tool like daemontools\n");
	exit(1);
    }

    /* close out the standard file descriptors */
    if (freopen("/dev/null", "r", stdin) == NULL) {
	fprintf(stderr, "%s: unable to replace stdin with /dev/null: %s\n",
		ap_server_argv0, strerror(errno));
	/* continue anyhow -- note we can't close out descriptor 0 because we
	 * have nothing to replace it with, and if we didn't have a descriptor
	 * 0 the next file would be created with that value ... leading to
	 * havoc.
	 */
    }
    if (freopen("/dev/null", "w", stdout) == NULL) {
	fprintf(stderr, "%s: unable to replace stdout with /dev/null: %s\n",
		ap_server_argv0, strerror(errno));
    }
    /* stderr is a tricky one, we really want it to be the error_log,
     * but we haven't opened that yet.  So leave it alone for now and it'll
     * be reopened moments later.
     */
}

/* Set group privileges.
 *
 * Note that we use the username as set in the config files, rather than
 * the lookup of to uid --- the same uid may have multiple passwd entries,
 * with different sets of groups for each.
 */

static void set_group_privs(void)
{
    if (!geteuid()) {
	char *name;

	/* Get username if passed as a uid */

	if (ap_user_name[0] == '#') {
	    struct passwd *ent;
	    uid_t uid = atoi(&ap_user_name[1]);

	    if ((ent = getpwuid(uid)) == NULL) {
		ap_log_error(APLOG_MARK, APLOG_ALERT, server_conf,
			 "getpwuid: couldn't determine user name from uid %u, "
			 "you probably need to modify the User directive",
			 (unsigned)uid);
		clean_child_exit(APEXIT_CHILDFATAL);
	    }

	    name = ent->pw_name;
	}
	else
	    name = ap_user_name;

	/*
	 * Set the GID before initgroups(), since on some platforms
	 * setgid() is known to zap the group list.
	 */
	if (setgid(ap_group_id) == -1) {
	    ap_log_error(APLOG_MARK, APLOG_ALERT, server_conf,
			"setgid: unable to set group id to Group %u",
			(unsigned)ap_group_id);
	    clean_child_exit(APEXIT_CHILDFATAL);
	}

	/* Reset `groups' attributes. */

	if (initgroups(name, ap_group_id) == -1) {
	    ap_log_error(APLOG_MARK, APLOG_ALERT, server_conf,
			"initgroups: unable to set groups for User %s "
			"and Group %u", name, (unsigned)ap_group_id);
	    clean_child_exit(APEXIT_CHILDFATAL);
	}
    }
}

/* check to see if we have the 'suexec' setuid wrapper installed */
static int init_suexec(void)
{
    int result = 0;

    struct stat wrapper;

    if ((stat(SUEXEC_BIN, &wrapper)) != 0) {
	result = 0;
    }
    else if ((wrapper.st_mode & S_ISUID) && (wrapper.st_uid == 0)) {
	result = 1;
    }
    return result;
}

/*****************************************************************
 * Connection structures and accounting...
 */


static conn_rec *new_connection(pool *p, server_rec *server, BUFF *inout,
			     const struct sockaddr_in *remaddr,
			     const struct sockaddr_in *saddr,
			     int child_num)
{
    conn_rec *conn = (conn_rec *) ap_pcalloc(p, sizeof(conn_rec));

    /* Got a connection structure, so initialize what fields we can
     * (the rest are zeroed out by pcalloc).
     */

    conn->child_num = child_num;

    conn->pool = p;
    conn->local_addr = *saddr;
    conn->local_ip = ap_pstrdup(conn->pool,
				inet_ntoa(conn->local_addr.sin_addr));
    conn->server = server; /* just a guess for now */
    ap_update_vhost_given_ip(conn);
    conn->base_server = conn->server;
    conn->client = inout;

    conn->remote_addr = *remaddr;
    conn->remote_ip = ap_pstrdup(conn->pool,
			      inet_ntoa(conn->remote_addr.sin_addr));
    conn->ctx = ap_ctx_new(conn->pool);

    /*
     * Invoke the `new_connection' hook of modules to let them do
     * some connection dependent actions before we go on with
     * processing the request on this connection.
     */
    {
        module *m;
        for (m = top_module; m != NULL; m = m->next)
            if (m->magic == MODULE_MAGIC_COOKIE_EAPI)
                if (m->new_connection != NULL)
                    (*m->new_connection)(conn);
    }

    return conn;
}

static void sock_disable_nagle(int s, struct sockaddr_in *sin_client)
{
    /* The Nagle algorithm says that we should delay sending partial
     * packets in hopes of getting more data.  We don't want to do
     * this; we are not telnet.  There are bad interactions between
     * persistent connections and Nagle's algorithm that have very severe
     * performance penalties.  (Failing to disable Nagle is not much of a
     * problem with simple HTTP.)
     *
     * In spite of these problems, failure here is not a shooting offense.
     */
    int just_say_no = 1;

    if (setsockopt(s, IPPROTO_TCP, TCP_NODELAY, (char *) &just_say_no,
		   sizeof(int)) < 0) {
        if (sin_client) {
            ap_log_error(APLOG_MARK, APLOG_DEBUG, server_conf,
                         "setsockopt: (TCP_NODELAY), client %pA probably "
                         "dropped the connection", &sin_client->sin_addr);
        }
        else {
            ap_log_error(APLOG_MARK, APLOG_DEBUG, server_conf,
                         "setsockopt: (TCP_NODELAY)");
        }
    }
}

static int make_sock(pool *p, const struct sockaddr_in *server)
{
    int s;
    int one = 1;
    char addr[512];

    if (server->sin_addr.s_addr != htonl(INADDR_ANY))
	ap_snprintf(addr, sizeof(addr), "address %s port %d",
		inet_ntoa(server->sin_addr), ntohs(server->sin_port));
    else
	ap_snprintf(addr, sizeof(addr), "port %d", ntohs(server->sin_port));

    /* note that because we're about to slack we don't use psocket */
    ap_block_alarms();
    if ((s = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP)) == -1) {
	    ap_log_error(APLOG_MARK, APLOG_CRIT, server_conf,
		    "make_sock: failed to get a socket for %s", addr);

	    ap_unblock_alarms();
	    exit(1);
    }

    s = ap_slack(s, AP_SLACK_HIGH);

    ap_note_cleanups_for_socket_ex(p, s, 1);	/* arrange to close on exec or restart */

    if (setsockopt(s, SOL_SOCKET, SO_REUSEADDR, (char *) &one, sizeof(int)) < 0) {
	ap_log_error(APLOG_MARK, APLOG_CRIT, server_conf,
		    "make_sock: for %s, setsockopt: (SO_REUSEADDR)", addr);
	closesocket(s);
	ap_unblock_alarms();
	exit(1);
    }
    one = 1;
    if (setsockopt(s, SOL_SOCKET, SO_KEEPALIVE, (char *) &one, sizeof(int)) < 0) {
	ap_log_error(APLOG_MARK, APLOG_CRIT, server_conf,
		    "make_sock: for %s, setsockopt: (SO_KEEPALIVE)", addr);
	closesocket(s);

	ap_unblock_alarms();
	exit(1);
    }

    sock_disable_nagle(s, NULL);
    sock_enable_linger(s);

    /*
     * To send data over high bandwidth-delay connections at full
     * speed we must force the TCP window to open wide enough to keep the
     * pipe full.  The default window size on many systems
     * is only 4kB.  Cross-country WAN connections of 100ms
     * at 1Mb/s are not impossible for well connected sites.
     * If we assume 100ms cross-country latency,
     * a 4kB buffer limits throughput to 40kB/s.
     *
     * To avoid this problem I've added the SendBufferSize directive
     * to allow the web master to configure send buffer size.
     *
     * The trade-off of larger buffers is that more kernel memory
     * is consumed.  YMMV, know your customers and your network!
     *
     * -John Heidemann <johnh@isi.edu> 25-Oct-96
     *
     * If no size is specified, use the kernel default.
     */
    if (server_conf->send_buffer_size) {
	if (setsockopt(s, SOL_SOCKET, SO_SNDBUF,
		(char *) &server_conf->send_buffer_size, sizeof(int)) < 0) {
	    ap_log_error(APLOG_MARK, APLOG_WARNING, server_conf,
			"make_sock: failed to set SendBufferSize for %s, "
			"using default", addr);
	    /* not a fatal error */
	}
    }


    if (bind(s, (struct sockaddr *) server, sizeof(struct sockaddr_in)) == -1) {
	ap_log_error(APLOG_MARK, APLOG_CRIT, server_conf,
	    "make_sock: could not bind to %s", addr);

	closesocket(s);
	ap_unblock_alarms();
	exit(1);
    }

    if (listen(s, ap_listenbacklog) == -1) {
	ap_log_error(APLOG_MARK, APLOG_ERR, server_conf,
	    "make_sock: unable to listen for connections on %s", addr);
	closesocket(s);
	ap_unblock_alarms();
	exit(1);
    }

    ap_unblock_alarms();

    /* protect various fd_sets */
    if (s >= FD_SETSIZE) {
	ap_log_error(APLOG_MARK, APLOG_NOERRNO|APLOG_WARNING, NULL,
	    "make_sock: problem listening on %s, filedescriptor (%u) "
	    "larger than FD_SETSIZE (%u) "
	    "found, you probably need to rebuild Apache with a "
	    "larger FD_SETSIZE", addr, s, FD_SETSIZE);
	closesocket(s);
	exit(1);
    }

    return s;
}


/*
 * During a restart we keep track of the old listeners here, so that we
 * can re-use the sockets.  We have to do this because we won't be able
 * to re-open the sockets ("Address already in use").
 *
 * Unlike the listeners ring, old_listeners is a NULL terminated list.
 *
 * copy_listeners() makes the copy, find_listener() finds an old listener
 * and close_unused_listener() cleans up whatever wasn't used.
 */
static listen_rec *old_listeners;

/* unfortunately copy_listeners may be called before listeners is a ring */
static void copy_listeners(pool *p)
{
    listen_rec *lr;

    ap_assert(old_listeners == NULL);
    if (ap_listeners == NULL) {
	return;
    }
    lr = ap_listeners;
    do {
	listen_rec *nr = malloc(sizeof *nr);

        if (nr == NULL) {
            fprintf(stderr, "Ouch!  malloc failed in copy_listeners()\n");
            exit(1);
        }
	*nr = *lr;
	ap_kill_cleanups_for_socket(p, nr->fd);
	nr->next = old_listeners;
	ap_assert(!nr->used);
	old_listeners = nr;
	lr = lr->next;
    } while (lr && lr != ap_listeners);
}


static int find_listener(listen_rec *lr)
{
    listen_rec *or;

    for (or = old_listeners; or; or = or->next) {
	if (!memcmp(&or->local_addr, &lr->local_addr, sizeof(or->local_addr))) {
	    or->used = 1;
	    return or->fd;
	}
    }
    return -1;
}


static void close_unused_listeners(void)
{
    listen_rec *or, *next;

    for (or = old_listeners; or; or = next) {
	next = or->next;
	if (!or->used)
	    closesocket(or->fd);
	free(or);
    }
    old_listeners = NULL;
}


/* open sockets, and turn the listeners list into a singly linked ring */
static void setup_listeners(pool *p)
{
    listen_rec *lr;
    int fd;

    listenmaxfd = -1;
    FD_ZERO(&listenfds);
    lr = ap_listeners;
    for (;;) {
	fd = find_listener(lr);
	if (fd < 0) {
	    fd = make_sock(p, &lr->local_addr);
	}
	else {
	    ap_note_cleanups_for_socket_ex(p, fd, 1);
	}
	/* if we get here, (fd >= 0) && (fd < FD_SETSIZE) */
	FD_SET(fd, &listenfds);
	if (fd > listenmaxfd)
	    listenmaxfd = fd;
	lr->fd = fd;
	if (lr->next == NULL)
	    break;
	lr = lr->next;
    }
    /* turn the list into a ring */
    lr->next = ap_listeners;
    head_listener = ap_listeners;
    close_unused_listeners();

}


/*
 * Find a listener which is ready for accept().  This advances the
 * head_listener global.
 */
static ap_inline listen_rec *find_ready_listener(fd_set * main_fds)
{
    listen_rec *lr;

    lr = head_listener;
    do {
	if (FD_ISSET(lr->fd, main_fds)) {
	    head_listener = lr->next;
	    return (lr);
	}
	lr = lr->next;
    } while (lr != head_listener);
    return NULL;
}


static void show_compile_settings(void)
{
    printf("Server version: %s\n", ap_get_server_version());
    printf("Server's Module Magic Number: %u:%u\n",
	   MODULE_MAGIC_NUMBER_MAJOR, MODULE_MAGIC_NUMBER_MINOR);
    printf("Server compiled with....\n");
    printf(" -D EAPI\n");
#ifdef EAPI_MM
    printf(" -D EAPI_MM\n");
#ifdef EAPI_MM_CORE_PATH
    printf(" -D EAPI_MM_CORE_PATH=\"" EAPI_MM_CORE_PATH "\"\n");
#endif
#endif
    printf(" -D HAVE_MMAP\n");
    printf(" -D HAVE_SHMGET\n");
    printf(" -D USE_MMAP_SCOREBOARD\n");
    printf(" -D USE_MMAP_FILES\n");
#ifdef MMAP_SEGMENT_SIZE
	printf(" -D MMAP_SEGMENT_SIZE=%ld\n",(long)MMAP_SEGMENT_SIZE);
#endif
    printf(" -D HAVE_FLOCK_SERIALIZED_ACCEPT\n");
    printf(" -D HAVE_SYSVSEM_SERIALIZED_ACCEPT\n");
    printf(" -D SINGLE_LISTEN_UNSERIALIZED_ACCEPT\n");
#ifdef BUFFERED_LOGS
    printf(" -D BUFFERED_LOGS\n");
#ifdef PIPE_BUF
	printf(" -D PIPE_BUF=%ld\n",(long)PIPE_BUF);
#endif
#endif
    printf(" -D DYNAMIC_MODULE_LIMIT=%ld\n",(long)DYNAMIC_MODULE_LIMIT);
    printf(" -D HARD_SERVER_LIMIT=%ld\n",(long)HARD_SERVER_LIMIT);

/* This list displays the compiled-in default paths: */
#ifdef HTTPD_ROOT
    printf(" -D HTTPD_ROOT=\"" HTTPD_ROOT "\"\n");
#endif
#if defined(SUEXEC_BIN)
    printf(" -D SUEXEC_BIN=\"" SUEXEC_BIN "\"\n");
#endif
#ifdef DEFAULT_PIDLOG
    printf(" -D DEFAULT_PIDLOG=\"" DEFAULT_PIDLOG "\"\n");
#endif
#ifdef DEFAULT_SCOREBOARD
    printf(" -D DEFAULT_SCOREBOARD=\"" DEFAULT_SCOREBOARD "\"\n");
#endif
#ifdef DEFAULT_LOCKFILE
    printf(" -D DEFAULT_LOCKFILE=\"" DEFAULT_LOCKFILE "\"\n");
#endif
#ifdef DEFAULT_ERRORLOG
    printf(" -D DEFAULT_ERRORLOG=\"" DEFAULT_ERRORLOG "\"\n");
#endif
#ifdef TYPES_CONFIG_FILE
    printf(" -D TYPES_CONFIG_FILE=\"" TYPES_CONFIG_FILE "\"\n");
#endif
#ifdef SERVER_CONFIG_FILE
    printf(" -D SERVER_CONFIG_FILE=\"" SERVER_CONFIG_FILE "\"\n");
#endif
#ifdef ACCESS_CONFIG_FILE
    printf(" -D ACCESS_CONFIG_FILE=\"" ACCESS_CONFIG_FILE "\"\n");
#endif
#ifdef RESOURCE_CONFIG_FILE
    printf(" -D RESOURCE_CONFIG_FILE=\"" RESOURCE_CONFIG_FILE "\"\n");
#endif
}


/* Some init code that's common between win32 and unix... well actually
 * some of it is #ifdef'd but was duplicated before anyhow.  This stuff
 * is still a mess.
 */
static void common_init(void)
{
    INIT_SIGLIST()


    pglobal = ap_init_alloc();
    pconf = ap_make_sub_pool(pglobal);
    plog = ap_make_sub_pool(pglobal);
    ptrans = ap_make_sub_pool(pconf);

    ap_util_init();
    ap_util_uri_init();

    pcommands = ap_make_sub_pool(NULL);
    ap_server_pre_read_config  = ap_make_array(pcommands, 1, sizeof(char *));
    ap_server_post_read_config = ap_make_array(pcommands, 1, sizeof(char *));
    ap_server_config_defines   = ap_make_array(pcommands, 1, sizeof(char *));

    ap_hook_init();
    ap_hook_configure("ap::buff::read", 
                      AP_HOOK_SIG4(int,ptr,ptr,int), AP_HOOK_TOPMOST);
    ap_hook_configure("ap::buff::write",  
                      AP_HOOK_SIG4(int,ptr,ptr,int), AP_HOOK_TOPMOST);
    ap_hook_configure("ap::buff::writev",  
                      AP_HOOK_SIG4(int,ptr,ptr,int), AP_HOOK_TOPMOST);
    ap_hook_configure("ap::buff::sendwithtimeout", 
                      AP_HOOK_SIG4(int,ptr,ptr,int), AP_HOOK_TOPMOST);
    ap_hook_configure("ap::buff::recvwithtimeout", 
                      AP_HOOK_SIG4(int,ptr,ptr,int), AP_HOOK_TOPMOST);

    ap_global_ctx = ap_ctx_new(NULL);
}

/*****************************************************************
 * Child process main loop.
 * The following vars are static to avoid getting clobbered by longjmp();
 * they are really private to child_main.
 */

static int srv;
static int csd;
static int dupped_csd;
static int requests_this_child;
static fd_set main_fds;

API_EXPORT(void) ap_child_terminate(request_rec *r)
{
    r->connection->keepalive = 0;
    requests_this_child = ap_max_requests_per_child = 1;
}

static void child_main(int child_num_arg)
{
    NET_SIZE_T clen;
    struct sockaddr sa_server;
    struct sockaddr sa_client;
    listen_rec *lr;
    struct rlimit rlp;

    /* All of initialization is a critical section, we don't care if we're
     * told to HUP or USR1 before we're done initializing.  For example,
     * we could be half way through child_init_modules() when a restart
     * signal arrives, and we'd have no real way to recover gracefully
     * and exit properly.
     *
     * I suppose a module could take forever to initialize, but that would
     * be either a broken module, or a broken configuration (i.e. network
     * problems, file locking problems, whatever). -djg
     */
    ap_block_alarms();

    my_pid = getpid();
    csd = -1;
    dupped_csd = -1;
    my_child_num = child_num_arg;
    requests_this_child = 0;

    setproctitle("child");

    /*
     * set up rlimits to keep apache+scripting from leaking horribly
     */
    if (ap_max_cpu_per_child != 0){
	rlp.rlim_cur = rlp.rlim_max = ap_max_cpu_per_child;
	if (setrlimit(RLIMIT_CPU, &rlp) == -1){
	    ap_log_error(APLOG_MARK, APLOG_ALERT, server_conf,
		"setrlimit: unable to set CPU limit to %d",
		ap_max_cpu_per_child);
	    clean_child_exit(APEXIT_CHILDFATAL);
	}
    }
    if (ap_max_data_per_child != 0){
	rlp.rlim_cur = rlp.rlim_max = ap_max_data_per_child;
	if (setrlimit(RLIMIT_DATA, &rlp) == -1){
	    ap_log_error(APLOG_MARK, APLOG_ALERT, server_conf,
		"setrlimit: unable to set data limit to %d",
		ap_max_data_per_child);
	    clean_child_exit(APEXIT_CHILDFATAL);
	}
    }
    if (ap_max_nofile_per_child != 0){
	rlp.rlim_cur = rlp.rlim_max = ap_max_nofile_per_child;
	if (setrlimit(RLIMIT_NOFILE, &rlp) == -1){
	    ap_log_error(APLOG_MARK, APLOG_ALERT, server_conf,
		"setrlimit: unable to set open file limit to %d",
		ap_max_nofile_per_child);
	    clean_child_exit(APEXIT_CHILDFATAL);
	}
    }
    if (ap_max_rss_per_child != 0){
	rlp.rlim_cur = rlp.rlim_max = ap_max_rss_per_child;
	if (setrlimit(RLIMIT_RSS, &rlp) == -1){
	    ap_log_error(APLOG_MARK, APLOG_ALERT, server_conf,
		"setrlimit: unable to set RSS limit to %d",
		ap_max_rss_per_child);
	    clean_child_exit(APEXIT_CHILDFATAL);
	}
    }
    if (ap_max_stack_per_child != 0){
	rlp.rlim_cur = rlp.rlim_max = ap_max_stack_per_child;
	if (setrlimit(RLIMIT_STACK, &rlp) == -1){
	    ap_log_error(APLOG_MARK, APLOG_ALERT, server_conf,
		"setrlimit: unable to set stack size limit to %d",
		ap_max_stack_per_child);
	    clean_child_exit(APEXIT_CHILDFATAL);
	}
    }

    /* Get a sub pool for global allocations in this child, so that
     * we can have cleanups occur when the child exits.
     */
    pchild = ap_make_sub_pool(pconf);
    /* associate accept mutex cleanup with a subpool of pchild so we can
     * make sure the mutex is released before calling module code at
     * termination
     */
    pmutex = ap_make_sub_pool(pchild);

    /* needs to be done before we switch UIDs so we have permissions */
    SAFE_ACCEPT(accept_mutex_child_init(pmutex));

    set_group_privs();
    /* 
     * Only try to switch if we're running as root
     * In case of Cygwin we have the special super-user named SYSTEM
     */
    if (!geteuid() && (
	setuid(ap_user_id) == -1)) {
	ap_log_error(APLOG_MARK, APLOG_ALERT, server_conf,
		    "setuid: unable to change to uid: %u", ap_user_id);
	clean_child_exit(APEXIT_CHILDFATAL);
    }

    ap_child_init_modules(pchild, server_conf);

    /* done with the initialization critical section */
    ap_unblock_alarms();

    (void) ap_update_child_status(my_child_num, SERVER_READY, (request_rec *) NULL);

    /*
     * Setup the jump buffers so that we can return here after a timeout 
     */
    ap_setjmp(jmpbuffer);
    signal(SIGURG, timeout);
    if (signal(SIGALRM, alrm_handler) == SIG_ERR) {
	   fprintf(stderr, "installing signal handler for SIGALRM failed, errno %u\n", errno);
	}


    while (1) {
	BUFF *conn_io;
	request_rec *r;

	/* Prepare to receive a SIGUSR1 due to graceful restart so that
	 * we can exit cleanly.  Since we're between connections right
	 * now it's the right time to exit, but we might be blocked in a
	 * system call when the graceful restart request is made. */
	usr1_just_die = 1;
	signal(SIGUSR1, usr1_handler);

	/*
	 * (Re)initialize this child to a pre-connection state.
	 */

	ap_kill_timeout(0);	/* Cancel any outstanding alarms. */
	current_conn = NULL;

	ap_clear_pool(ptrans);

	if (ap_scoreboard_image->global.running_generation != ap_my_generation) {
	    clean_child_exit(0);
	}

	if ((ap_max_requests_per_child > 0
	     && requests_this_child++ >= ap_max_requests_per_child)) {
	    clean_child_exit(0);
	}

	(void) ap_update_child_status(my_child_num, SERVER_READY, (request_rec *) NULL);

	/*
	 * Wait for an acceptable connection to arrive.
	 */

	/* Lock around "accept", if necessary */
	SAFE_ACCEPT(accept_mutex_on());

	for (;;) {
	    if (ap_listeners->next != ap_listeners) {
		/* more than one socket */
		memcpy(&main_fds, &listenfds, sizeof(fd_set));
		srv = ap_select(listenmaxfd + 1, &main_fds, NULL, NULL, NULL);

		if (srv < 0 && errno != EINTR) {
		    /* Single Unix documents select as returning errnos
		     * EBADF, EINTR, and EINVAL... and in none of those
		     * cases does it make sense to continue.  In fact
		     * on Linux 2.0.x we seem to end up with EFAULT
		     * occasionally, and we'd loop forever due to it.
		     */
		    ap_log_error(APLOG_MARK, APLOG_ERR, server_conf, "select: (listen)");
		    clean_child_exit(1);
		}

		if (srv <= 0)
		    continue;

		lr = find_ready_listener(&main_fds);
		if (lr == NULL)
		    continue;
		sd = lr->fd;
	    }
	    else {
		/* only one socket, just pretend we did the other stuff */
		sd = ap_listeners->fd;
	    }

	    /* if we accept() something we don't want to die, so we have to
	     * defer the exit
	     */
	    deferred_die = 0;
	    usr1_just_die = 0;
	    for (;;) {
		clen = sizeof(sa_client);
		csd = ap_accept(sd, &sa_client, &clen);
		if (csd >= 0 || errno != EINTR)
		    break;
		if (deferred_die) {
		    /* we didn't get a socket, and we were told to die */
		    clean_child_exit(0);
		}
	    }

	    if (csd >= 0)
		break;		/* We have a socket ready for reading */
	    else {

		/* Our old behaviour here was to continue after accept()
		 * errors.  But this leads us into lots of troubles
		 * because most of the errors are quite fatal.  For
		 * example, EMFILE can be caused by slow descriptor
		 * leaks (say in a 3rd party module, or libc).  It's
		 * foolish for us to continue after an EMFILE.  We also
		 * seem to tickle kernel bugs on some platforms which
		 * lead to never-ending loops here.  So it seems best
		 * to just exit in most cases.
		 */
                switch (errno) {

                case ECONNABORTED:
		    /* Linux generates the rest of these, other tcp
		     * stacks (i.e. bsd) tend to hide them behind
		     * getsockopt() interfaces.  They occur when
		     * the net goes sour or the client disconnects
		     * after the three-way handshake has been done
		     * in the kernel but before userland has picked
		     * up the socket.
		     */
                case ECONNRESET:
                case ETIMEDOUT:
		case EHOSTUNREACH:
		case ENETUNREACH:
                    break;
		case ENETDOWN:
		     /*
		      * When the network layer has been shut down, there
		      * is not much use in simply exiting: the parent
		      * would simply re-create us (and we'd fail again).
		      * Use the CHILDFATAL code to tear the server down.
		      * @@@ Martin's idea for possible improvement:
		      * A different approach would be to define
		      * a new APEXIT_NETDOWN exit code, the reception
		      * of which would make the parent shutdown all
		      * children, then idle-loop until it detected that
		      * the network is up again, and restart the children.
		      * Ben Hyde noted that temporary ENETDOWN situations
		      * occur in mobile IP.
		      */
		    ap_log_error(APLOG_MARK, APLOG_EMERG, server_conf,
			"accept: giving up.");
		    clean_child_exit(APEXIT_CHILDFATAL);

		default:
		    ap_log_error(APLOG_MARK, APLOG_ERR, server_conf,
				"accept: (client socket)");
		    clean_child_exit(1);
		}
	    }

	    /* go around again, safe to die */
	    usr1_just_die = 1;
	    if (deferred_die) {
		/* ok maybe not, see ya later */
		clean_child_exit(0);
	    }
	    /* or maybe we missed a signal, you never know on systems
	     * without reliable signals
	     */
	    if (ap_scoreboard_image->global.running_generation != ap_my_generation) {
		clean_child_exit(0);
	    }
	}

	SAFE_ACCEPT(accept_mutex_off());	/* unlock after "accept" */


	/* We've got a socket, let's at least process one request off the
	 * socket before we accept a graceful restart request.
	 */
	signal(SIGUSR1, SIG_IGN);

	ap_note_cleanups_for_socket_ex(ptrans, csd, 1);

	/* protect various fd_sets */
	if (csd >= FD_SETSIZE) {
	    ap_log_error(APLOG_MARK, APLOG_NOERRNO|APLOG_WARNING, NULL,
		"[csd] filedescriptor (%u) larger than FD_SETSIZE (%u) "
		"found, you probably need to rebuild Apache with a "
		"larger FD_SETSIZE", csd, FD_SETSIZE);
	    continue;
	}

	/*
	 * We now have a connection, so set it up with the appropriate
	 * socket options, file descriptors, and read/write buffers.
	 */

	clen = sizeof(sa_server);
	if (getsockname(csd, &sa_server, &clen) < 0) {
	    ap_log_error(APLOG_MARK, APLOG_DEBUG, server_conf, 
                         "getsockname, client %pA probably dropped the "
                         "connection", 
                         &((struct sockaddr_in *)&sa_client)->sin_addr);
	    continue;
	}

	sock_disable_nagle(csd, (struct sockaddr_in *)&sa_client);

	(void) ap_update_child_status(my_child_num, SERVER_BUSY_READ,
				   (request_rec *) NULL);

	conn_io = ap_bcreate(ptrans, B_RDWR | B_SOCKET);

	dupped_csd = csd;
	ap_bpushfd(conn_io, csd, dupped_csd);

	current_conn = new_connection(ptrans, server_conf, conn_io,
				          (struct sockaddr_in *) &sa_client,
				          (struct sockaddr_in *) &sa_server,
				          my_child_num);

	/*
	 * Read and process each request found on our connection
	 * until no requests are left or we decide to close.
	 */

	while ((r = ap_read_request(current_conn)) != NULL) {

	    /* read_request_line has already done a
	     * signal (SIGUSR1, SIG_IGN);
	     */

	    (void) ap_update_child_status(my_child_num, SERVER_BUSY_WRITE, r);

	    /* process the request if it was read without error */

	    if (r->status == HTTP_OK)
		ap_process_request(r);

	    if(ap_extended_status)
		increment_counts(my_child_num, r);

	    if (!current_conn->keepalive || current_conn->aborted)
		break;

	    ap_destroy_pool(r->pool);
	    (void) ap_update_child_status(my_child_num, SERVER_BUSY_KEEPALIVE,
				       (request_rec *) NULL);

	    if (ap_scoreboard_image->global.running_generation != ap_my_generation) {
		ap_call_close_connection_hook(current_conn);
		ap_bclose(conn_io);
		clean_child_exit(0);
	    }

	    /* In case we get a graceful restart while we're blocked
	     * waiting for the request.
	     *
	     * XXX: This isn't perfect, we might actually read the
	     * request and then just die without saying anything to
	     * the client.  This can be fixed by using deferred_die
	     * but you have to teach buff.c about it so that it can handle
	     * the EINTR properly.
	     *
	     * In practice though browsers (have to) expect keepalive
	     * connections to close before receiving a response because
	     * of network latencies and server timeouts.
	     */
	    usr1_just_die = 1;
	    signal(SIGUSR1, usr1_handler);
	}

	/*
	 * Close the connection, being careful to send out whatever is still
	 * in our buffers.  If possible, try to avoid a hard close until the
	 * client has ACKed our FIN and/or has stopped sending us data.
	 */

	if (r && r->connection
	    && !r->connection->aborted
	    && r->connection->client
	    && (r->connection->client->fd >= 0)) {

	    lingering_close(r);
	}
	else {
	    ap_call_close_connection_hook(current_conn);
	    ap_bsetflag(conn_io, B_EOUT, 1);
	    ap_bclose(conn_io);
	}
    }
}


static int make_child(server_rec *s, int slot, time_t now)
{
    int pid;

    if (slot + 1 > max_daemons_limit) {
	max_daemons_limit = slot + 1;
    }

    if (one_process) {
	signal(SIGHUP, just_die);
	signal(SIGINT, just_die);
	signal(SIGQUIT, SIG_DFL);
	signal(SIGTERM, just_die);
	child_main(slot);
    }

    /* avoid starvation */
    head_listener = head_listener->next;

    Explain1("Starting new child in slot %d", slot);
    (void) ap_update_child_status(slot, SERVER_STARTING, (request_rec *) NULL);


    if ((pid = fork()) == -1) {
	ap_log_error(APLOG_MARK, APLOG_ERR, s, "fork: Unable to fork new process");

	/* fork didn't succeed. Fix the scoreboard or else
	 * it will say SERVER_STARTING forever and ever
	 */
	(void) ap_update_child_status(slot, SERVER_DEAD, (request_rec *) NULL);

	/* In case system resources are maxxed out, we don't want
	   Apache running away with the CPU trying to fork over and
	   over and over again. */
	sleep(10);

	return -1;
    }

    if (!pid) {
	RAISE_SIGSTOP(MAKE_CHILD);
	MONCONTROL(1);
	/* Disable the restart signal handlers and enable the just_die stuff.
	 * Note that since restart() just notes that a restart has been
	 * requested there's no race condition here.
	 */
	signal(SIGHUP, just_die);
	signal(SIGUSR1, just_die);
	signal(SIGTERM, just_die);
	child_main(slot);
    }

    ap_scoreboard_image->parent[slot].last_rtime = now;
    ap_scoreboard_image->parent[slot].pid = pid;
    return 0;
}


/* start up a bunch of children */
static void startup_children(int number_to_start)
{
    int i;
    time_t now = time(NULL);

    for (i = 0; number_to_start && i < ap_daemons_limit; ++i) {
	if (ap_scoreboard_image->servers[i].status != SERVER_DEAD) {
	    continue;
	}
	if (make_child(server_conf, i, now) < 0) {
	    break;
	}
	--number_to_start;
    }
}


/*
 * idle_spawn_rate is the number of children that will be spawned on the
 * next maintenance cycle if there aren't enough idle servers.  It is
 * doubled up to MAX_SPAWN_RATE, and reset only when a cycle goes by
 * without the need to spawn.
 */
static int idle_spawn_rate = 1;
#ifndef MAX_SPAWN_RATE
#define MAX_SPAWN_RATE	(32)
#endif
static int hold_off_on_exponential_spawning;

/*
 * Define the signal that is used to kill off children if idle_count
 * is greater then ap_daemons_max_free. Usually we will use SIGUSR1
 * to gracefully shutdown, but unfortunatly some OS will need other 
 * signals to ensure that the child process is terminated and the 
 * scoreboard pool is not growing to infinity. Also set the signal we
 * use to kill of childs that exceed timeout. This effect has been
* seen at least on Cygwin 1.x. -- Stipe Tolj <tolj@wapme-systems.de>
 */
#define SIG_IDLE_KILL SIGUSR1
#define SIG_TIMEOUT_KILL SIGALRM

static void perform_idle_server_maintenance(void)
{
    int i;
    int to_kill;
    int idle_count;
    short_score *ss;
    time_t now = time(NULL);
    int free_length;
    int free_slots[MAX_SPAWN_RATE];
    int last_non_dead;
    int total_non_dead;

    /* initialize the free_list */
    free_length = 0;

    to_kill = -1;
    idle_count = 0;
    last_non_dead = -1;
    total_non_dead = 0;

    for (i = 0; i < ap_daemons_limit; ++i) {
	int status;

	if (i >= max_daemons_limit && free_length == idle_spawn_rate)
	    break;
	ss = &ap_scoreboard_image->servers[i];
	status = ss->status;
	if (status == SERVER_DEAD) {
	    /* try to keep children numbers as low as possible */
	    if (free_length < idle_spawn_rate) {
		free_slots[free_length] = i;
		++free_length;
	    }
	}
	else {
	    /* We consider a starting server as idle because we started it
	     * at least a cycle ago, and if it still hasn't finished starting
	     * then we're just going to swamp things worse by forking more.
	     * So we hopefully won't need to fork more if we count it.
	     * This depends on the ordering of SERVER_READY and SERVER_STARTING.
	     */
	    if (status <= SERVER_READY) {
		++ idle_count;
		/* always kill the highest numbered child if we have to...
		 * no really well thought out reason ... other than observing
		 * the server behaviour under linux where lower numbered children
		 * tend to service more hits (and hence are more likely to have
		 * their data in cpu caches).
		 */
		to_kill = i;
	    }

	    ++total_non_dead;
	    last_non_dead = i;
	    if (ss->timeout_len) {
		/* if it's a live server, with a live timeout then
		 * start checking its timeout */
		parent_score *ps = &ap_scoreboard_image->parent[i];
		if (ss->cur_vtime != ps->last_vtime) {
		    /* it has made progress, so update its last_rtime,
		     * last_vtime */
		    ps->last_rtime = now;
		    ps->last_vtime = ss->cur_vtime;
		}
		else if (ps->last_rtime + ss->timeout_len < now) {
		    /* no progress, and the timeout length has been exceeded */
		    ss->timeout_len = 0;
		    safe_child_kill(ps->pid, SIG_TIMEOUT_KILL);
		}
	    }
	}
    }
    max_daemons_limit = last_non_dead + 1;
    if (idle_count > ap_daemons_max_free) {
	/* kill off one child... we use SIGUSR1 because that'll cause it to
	 * shut down gracefully, in case it happened to pick up a request
	 * while we were counting. Use the define SIG_IDLE_KILL to reflect
	 * which signal should be used on the specific OS.
	 */
	safe_child_kill(ap_scoreboard_image->parent[to_kill].pid, SIG_IDLE_KILL);
	idle_spawn_rate = 1;
    }
    else if (idle_count < ap_daemons_min_free) {
	/* terminate the free list */
	if (free_length == 0) {
	    /* only report this condition once */
	    static int reported = 0;

	    if (!reported) {
		ap_log_error(APLOG_MARK, APLOG_NOERRNO|APLOG_ERR, server_conf,
			    "server reached MaxClients setting, consider"
			    " raising the MaxClients setting");
		reported = 1;
	    }
	    idle_spawn_rate = 1;
	}
	else {
	    if (idle_spawn_rate >= 8) {
		ap_log_error(APLOG_MARK, APLOG_NOERRNO|APLOG_INFO, server_conf,
		    "server seems busy, (you may need "
		    "to increase StartServers, or Min/MaxSpareServers), "
		    "spawning %d children, there are %d idle, and "
		    "%d total children", idle_spawn_rate,
		    idle_count, total_non_dead);
	    }
	    for (i = 0; i < free_length; ++i) {
		make_child(server_conf, free_slots[i], now);
	    }
	    /* the next time around we want to spawn twice as many if this
	     * wasn't good enough, but not if we've just done a graceful
	     */
	    if (hold_off_on_exponential_spawning) {
		--hold_off_on_exponential_spawning;
	    }
	    else if (idle_spawn_rate < MAX_SPAWN_RATE) {
		idle_spawn_rate *= 2;
	    }
	}
    }
    else {
	idle_spawn_rate = 1;
    }
}


static void process_child_status(int pid, ap_wait_t status)
{
    /* Child died... if it died due to a fatal error,
	* we should simply bail out.
	*/
    if ((WIFEXITED(status)) &&
	WEXITSTATUS(status) == APEXIT_CHILDFATAL) {
        /* cleanup pid file -- it is useless after our exiting */
        const char *pidfile = NULL;
        pidfile = ap_server_root_relative (pconf, ap_pid_fname);
        if ( pidfile != NULL && unlink(pidfile) == 0)
            ap_log_error(APLOG_MARK, APLOG_NOERRNO|APLOG_INFO,
                         server_conf,
                         "removed PID file %s (pid=%ld)",
                         pidfile, (long)getpid());
	ap_log_error(APLOG_MARK, APLOG_ALERT|APLOG_NOERRNO, server_conf,
			"Child %d returned a Fatal error... \n"
			"Apache is exiting!",
			pid);
	exit(APEXIT_CHILDFATAL);
    }
    if (WIFSIGNALED(status)) {
	switch (WTERMSIG(status)) {
	case SIGTERM:
	case SIGHUP:
	case SIGUSR1:
	case SIGKILL:
	    break;
	default:
	    if (WCOREDUMP(status)) {
		ap_log_error(APLOG_MARK, APLOG_NOERRNO|APLOG_NOTICE,
			     server_conf,
			     "child pid %d exit signal %s (%d), "
			     "possible coredump in %s",
			     pid, (WTERMSIG(status) >= NumSIG) ? "" : 
			     SYS_SIGLIST[WTERMSIG(status)], WTERMSIG(status),
			     ap_coredump_dir);
	    }
	    else {
		ap_log_error(APLOG_MARK, APLOG_NOERRNO|APLOG_NOTICE,
			     server_conf,
			     "child pid %d exit signal %s (%d)", pid,
			     SYS_SIGLIST[WTERMSIG(status)], WTERMSIG(status));
	    }
	}
    }
}


/*****************************************************************
 * Executive routines.
 */

#ifndef STANDALONE_MAIN
#define STANDALONE_MAIN standalone_main

static void standalone_main(int argc, char **argv)
{
    int remaining_children_to_start;


    ap_standalone = 1;

    is_graceful = 0;

    if (!one_process) {
	detach();
    }
    else {
	MONCONTROL(1);
    }

    my_pid = getpid();

    do {
	copy_listeners(pconf);
	if (!is_graceful) {
	    ap_restart_time = time(NULL);
	}
	ap_clear_pool(pconf);
	ptrans = ap_make_sub_pool(pconf);

	ap_init_mutex_method(ap_default_mutex_method());

	server_conf = ap_read_config(pconf, ptrans, ap_server_confname);
	setup_listeners(pconf);
	ap_clear_pool(plog);

	/* 
	 * we cannot reopen the logfiles once we dropped permissions, 
	 * we cannot write the pidfile (pointless anyway), and we can't
	 * reload & reinit the modules.
	 */

	if (!is_chrooted) {
	    ap_open_logs(server_conf, plog);
	    ap_log_pid(pconf, ap_pid_fname);
	}
	ap_set_version();	/* create our server_version string */
	ap_init_modules(pconf, server_conf);
	ap_init_etag(pconf);
	version_locked++;	/* no more changes to server_version */

	if(!is_graceful && !is_chrooted)
	    if (ap_server_chroot) {
		if (geteuid()) {
		    ap_log_error(APLOG_MARK, APLOG_NOERRNO|APLOG_EMERG, 
			server_conf, "can't run in secure mode if not "
			"started with root privs.");
		    exit(1);
		}

		/* initialize /dev/crypto, XXX check for -DSSL option */
#ifdef MOD_SSL
		OpenSSL_add_all_algorithms();
#endif

		if (initgroups(ap_user_name, ap_group_id)) {
		    ap_log_error(APLOG_MARK, APLOG_CRIT, server_conf,
			"initgroups: unable to set groups for User %s "
			"and Group %u", ap_user_name, (unsigned)ap_group_id);
		    exit(1);
		}

		if (chroot(ap_server_root) < 0) {
		    ap_log_error(APLOG_MARK, APLOG_EMERG, server_conf,
			"unable to chroot into %s!", ap_server_root);
		    exit(1);
		}
		ap_log_error(APLOG_MARK, APLOG_NOERRNO|APLOG_NOTICE, 
		    server_conf, "chrooted in %s", ap_server_root);
		chdir("/");
		is_chrooted = 1;
		setproctitle("parent [chroot %s]", ap_server_root);

		if (setresgid(ap_group_id, ap_group_id, ap_group_id) != 0 ||
		    setresuid(ap_user_id, ap_user_id, ap_user_id) != 0) {
			ap_log_error(APLOG_MARK, APLOG_CRIT, server_conf,
			    "can't drop privileges!");
			exit(1);
		} else
		    ap_log_error(APLOG_MARK, APLOG_NOERRNO|APLOG_NOTICE,
			server_conf, "changed to uid %u, gid %u",
			ap_user_id, ap_group_id);
		} else
		    setproctitle("parent");


	SAFE_ACCEPT(accept_mutex_init(pconf));
	if (!is_graceful) {
	    reinit_scoreboard(pconf);
	}
	set_signals();

	if (ap_daemons_max_free < ap_daemons_min_free + 1)	/* Don't thrash... */
	    ap_daemons_max_free = ap_daemons_min_free + 1;

	/* If we're doing a graceful_restart then we're going to see a lot
	 * of children exiting immediately when we get into the main loop
	 * below (because we just sent them SIGUSR1).  This happens pretty
	 * rapidly... and for each one that exits we'll start a new one until
	 * we reach at least daemons_min_free.  But we may be permitted to
	 * start more than that, so we'll just keep track of how many we're
	 * supposed to start up without the 1 second penalty between each fork.
	 */
	remaining_children_to_start = ap_daemons_to_start;
	if (remaining_children_to_start > ap_daemons_limit) {
	    remaining_children_to_start = ap_daemons_limit;
	}
	if (!is_graceful) {
	    startup_children(remaining_children_to_start);
	    remaining_children_to_start = 0;
	}
	else {
	    /* give the system some time to recover before kicking into
	     * exponential mode */
	    hold_off_on_exponential_spawning = 10;
	}

	ap_log_error(APLOG_MARK, APLOG_NOERRNO|APLOG_NOTICE, server_conf,
		    "%s configured -- resuming normal operations",
		    ap_get_server_version());
	if (ap_suexec_enabled) {
	    ap_log_error(APLOG_MARK, APLOG_NOERRNO|APLOG_NOTICE, server_conf,
		         "suEXEC mechanism enabled (wrapper: %s)", SUEXEC_BIN);
	}
	ap_log_error(APLOG_MARK, APLOG_NOERRNO|APLOG_NOTICE, server_conf,
		    "Accept mutex: %s (Default: %s)",
		     amutex->name, ap_default_mutex_method());
	restart_pending = shutdown_pending = 0;

	while (!restart_pending && !shutdown_pending) {
	    int child_slot;
	    ap_wait_t status;
	    int pid = wait_or_timeout(&status);

	    /* XXX: if it takes longer than 1 second for all our children
	     * to start up and get into IDLE state then we may spawn an
	     * extra child
	     */
	    if (pid >= 0) {
		process_child_status(pid, status);
		/* non-fatal death... note that it's gone in the scoreboard. */
		child_slot = find_child_by_pid(pid);
		Explain2("Reaping child %d slot %d", pid, child_slot);
		if (child_slot >= 0) {
		    (void) ap_update_child_status(child_slot, SERVER_DEAD,
					       (request_rec *) NULL);
		    if (remaining_children_to_start
			&& child_slot < ap_daemons_limit) {
			/* we're still doing a 1-for-1 replacement of dead
			 * children with new children
			 */
			make_child(server_conf, child_slot, time(NULL));
			--remaining_children_to_start;
		    }
		}
		else if (reap_other_child(pid, status) == 0) {
		    /* handled */
		}
		else if (is_graceful) {
		    /* Great, we've probably just lost a slot in the
		     * scoreboard.  Somehow we don't know about this
		     * child.
		     */
		    ap_log_error(APLOG_MARK, APLOG_NOERRNO|APLOG_WARNING, server_conf,
				"long lost child came home! (pid %d)", pid);
		}
		/* Don't perform idle maintenance when a child dies,
		 * only do it when there's a timeout.  Remember only a
		 * finite number of children can die, and it's pretty
		 * pathological for a lot to die suddenly.
		 */
		continue;
	    }
	    else if (remaining_children_to_start) {
		/* we hit a 1 second timeout in which none of the previous
		 * generation of children needed to be reaped... so assume
		 * they're all done, and pick up the slack if any is left.
		 */
		startup_children(remaining_children_to_start);
		remaining_children_to_start = 0;
		/* In any event we really shouldn't do the code below because
		 * few of the servers we just started are in the IDLE state
		 * yet, so we'd mistakenly create an extra server.
		 */
		continue;
	    }

	    perform_idle_server_maintenance();
	}

	if (shutdown_pending) {
	    /* Time to gracefully shut down:
	     * Kill child processes, tell them to call child_exit, etc...
	     */
	    if (ap_killpg(pgrp, SIGTERM) < 0) {
		ap_log_error(APLOG_MARK, APLOG_WARNING, server_conf, "killpg SIGTERM");
	    }
	    reclaim_child_processes(1);		/* Start with SIGTERM */

	    /* cleanup pid file on normal shutdown */
	    {
		char *pidfile = NULL;
		pidfile = ap_server_root_relative (pconf, ap_pid_fname);
		ap_server_strip_chroot(pidfile, 0);
		if ( pidfile != NULL && unlink(pidfile) == 0)
		    ap_log_error(APLOG_MARK, APLOG_NOERRNO|APLOG_INFO,
				 server_conf,
				 "removed PID file %s (pid=%u)",
				 pidfile, getpid());
	    }

	    ap_log_error(APLOG_MARK, APLOG_NOERRNO|APLOG_NOTICE, server_conf,
			"caught SIGTERM, shutting down");
	    clean_parent_exit(0);
	}

	/* we've been told to restart */
	signal(SIGHUP, SIG_IGN);
	signal(SIGUSR1, SIG_IGN);

	if (one_process) {
	    /* not worth thinking about */
	    clean_parent_exit(0);
	}

	/* advance to the next generation */
	/* XXX: we really need to make sure this new generation number isn't in
	 * use by any of the children.
	 */
	++ap_my_generation;
	ap_scoreboard_image->global.running_generation = ap_my_generation;

	if (is_graceful) {
	    int i;
	    ap_log_error(APLOG_MARK, APLOG_NOERRNO|APLOG_NOTICE, server_conf,
			"SIGUSR1 received.  Doing graceful restart");

	    /* kill off the idle ones */
	    if (ap_killpg(pgrp, SIGUSR1) < 0) {
		ap_log_error(APLOG_MARK, APLOG_WARNING, server_conf, "killpg SIGUSR1");
	    }
	    /* This is mostly for debugging... so that we know what is still
	     * gracefully dealing with existing request.  But we can't really
	     * do it if we're in a SCOREBOARD_FILE because it'll cause
	     * corruption too easily.
	     */
	    for (i = 0; i < ap_daemons_limit; ++i) {
		if (ap_scoreboard_image->servers[i].status != SERVER_DEAD) {
		    ap_scoreboard_image->servers[i].status = SERVER_GRACEFUL;
		}
	    }
	}
	else {
	    /* Kill 'em off */
	    if (ap_killpg(pgrp, SIGHUP) < 0) {
		ap_log_error(APLOG_MARK, APLOG_WARNING, server_conf, "killpg SIGHUP");
	    }
	    reclaim_child_processes(0);		/* Not when just starting up */
	    ap_log_error(APLOG_MARK, APLOG_NOERRNO|APLOG_NOTICE, server_conf,
			"SIGHUP received.  Attempting to restart");
	}
    } while (restart_pending);

    /*add_common_vars(NULL);*/
}				/* standalone_main */
#else
/* prototype */
void STANDALONE_MAIN(int argc, char **argv);
#endif /* STANDALONE_MAIN */

extern char *optarg;
extern int optind;

int REALMAIN(int argc, char *argv[])
{
    int c;
    int sock_in;
    int sock_out;
    char *s;
    
    MONCONTROL(0);

    common_init();
    
    if ((s = strrchr(argv[0], PATHSEPARATOR)) != NULL) {
	ap_server_argv0 = ++s;
    }
    else {
	ap_server_argv0 = argv[0];
    }
    
    ap_cpystrn(ap_server_root, HTTPD_ROOT, sizeof(ap_server_root));
    ap_cpystrn(ap_server_confname, SERVER_CONFIG_FILE, sizeof(ap_server_confname));

    ap_setup_prelinked_modules();

    while ((c = getopt(argc, argv,
				    "D:C:c:xXd:Ff:vVlLR:StThu"
#ifdef DEBUG_SIGSTOP
				    "Z:"
#endif
			)) != -1) {
	char **new;
	switch (c) {
	case 'c':
	    new = (char **)ap_push_array(ap_server_post_read_config);
	    *new = ap_pstrdup(pcommands, optarg);
	    break;
	case 'C':
	    new = (char **)ap_push_array(ap_server_pre_read_config);
	    *new = ap_pstrdup(pcommands, optarg);
	    break;
	case 'D':
	    new = (char **)ap_push_array(ap_server_config_defines);
	    *new = ap_pstrdup(pcommands, optarg);
	    break;
	case 'd':
	    ap_cpystrn(ap_server_root, optarg, sizeof(ap_server_root));
	    break;
	case 'F':
	    do_detach = 0;
	    break;
	case 'f':
	    ap_cpystrn(ap_server_confname, optarg, sizeof(ap_server_confname));
	    break;
	case 'v':
	    ap_server_tokens = SrvTk_FULL;
	    ap_set_version();
	    printf("Server version: %s\n", ap_get_server_version());
	    exit(0);
	case 'V':
	    ap_server_tokens = SrvTk_FULL;
	    ap_set_version();
	    show_compile_settings();
	    exit(0);
	case 'l':
	    ap_suexec_enabled = init_suexec();
	    ap_show_modules();
	    exit(0);
	case 'L':
	    ap_show_directives();
	    exit(0);
	case 'X':
	    ++one_process;	/* Weird debugging mode. */
	    break;
#ifdef DEBUG_SIGSTOP
	case 'Z':
	    raise_sigstop_flags = atoi(optarg);
	    break;
#endif
	case 'S':
	    ap_dump_settings = 1;
	    break;
	case 't':
	    ap_configtestonly = 1;
	    ap_docrootcheck = 1;
	    break;
	case 'T':
	    ap_configtestonly = 1;
	    ap_docrootcheck = 0;
	    break;
	case 'h':
	    usage(argv[0]);
	case 'u':
	    ap_server_chroot = 0;
	    break;
	case '?':
	    usage(argv[0]);
	}
    }
    ap_init_alloc_shared(TRUE);

    ap_suexec_enabled = init_suexec();
    server_conf = ap_read_config(pconf, ptrans, ap_server_confname);

    ap_init_alloc_shared(FALSE);

    if (ap_configtestonly) {
        fprintf(stderr, "Syntax OK\n");
        clean_parent_exit(0);
    }
    if (ap_dump_settings) {
        clean_parent_exit(0);
    }

    child_timeouts = !ap_standalone || one_process;


    if (ap_standalone) {
	ap_open_logs(server_conf, plog);
	ap_set_version();
	ap_init_modules(pconf, server_conf);
	version_locked++;
	STANDALONE_MAIN(argc, argv);
    }
    else {
	conn_rec *conn;
	request_rec *r;
	struct sockaddr sa_server, sa_client;
	BUFF *cio;
	NET_SIZE_T l;

	ap_set_version();
	/* Yes this is called twice. */
	ap_init_modules(pconf, server_conf);
	version_locked++;
	ap_open_logs(server_conf, plog);
	ap_init_modules(pconf, server_conf);
	set_group_privs();

    /* 
     * Only try to switch if we're running as root
     * In case of Cygwin we have the special super-user named SYSTEM
     * with a pre-defined uid.
     */
	if (!geteuid() && setuid(ap_user_id) == -1) {
	    ap_log_error(APLOG_MARK, APLOG_ALERT, server_conf,
			"setuid: unable to change to uid: %u",
			ap_user_id);
	    exit(1);
	}
	if (ap_setjmp(jmpbuffer)) {
	    exit(0);
	}

    sock_in = fileno(stdin);
    sock_out = fileno(stdout);

	l = sizeof(sa_client);
	if ((getpeername(sock_in, &sa_client, &l)) < 0) {
/* get peername will fail if the input isn't a socket */
	    perror("getpeername");
	    memset(&sa_client, '\0', sizeof(sa_client));
	}

	l = sizeof(sa_server);
	if (getsockname(sock_in, &sa_server, &l) < 0) {
	    perror("getsockname");
	    fprintf(stderr, "Error getting local address\n");
	    exit(1);
	}
	server_conf->port = ntohs(((struct sockaddr_in *) &sa_server)->sin_port);
	cio = ap_bcreate(ptrans, B_RDWR | B_SOCKET);
        cio->fd = sock_out;
        cio->fd_in = sock_in;
	conn = new_connection(ptrans, server_conf, cio,
			          (struct sockaddr_in *) &sa_client,
			          (struct sockaddr_in *) &sa_server, -1);

	while ((r = ap_read_request(conn)) != NULL) {

	    if (r->status == HTTP_OK)
		ap_process_request(r);

	    if (!conn->keepalive || conn->aborted)
		break;

	    ap_destroy_pool(r->pool);
	}

	ap_call_close_connection_hook(conn);

	ap_bclose(cio);
    }
    exit(0);
}

#include "httpd.h"
/*
 * Force ap_validate_password() into the image so that modules like
 * mod_auth can use it even if they're dynamically loaded.
 */
void suck_in_ap_validate_password(void);
void suck_in_ap_validate_password(void)
{
    ap_validate_password("a", "b");
}

/* force Expat to be linked into the server executable */
#if defined(USE_EXPAT)
#include "xmlparse.h"
const XML_LChar *suck_in_expat(void);
const XML_LChar *suck_in_expat(void)
{
    return XML_ErrorString(XML_ERROR_NONE);
}
#endif /* USE_EXPAT */

API_EXPORT(void) ap_server_strip_chroot(char *src, int force)
{
    char buf[MAX_STRING_LEN];

    if(src != NULL && ap_server_chroot && (is_chrooted || force)) {
	if (strncmp(ap_server_root, src, strlen(ap_server_root)) == 0) {
	    strlcpy(buf, src+strlen(ap_server_root), MAX_STRING_LEN);
	    strlcpy(src, buf, strlen(src));
	} 
    }
}

API_EXPORT(int) ap_server_is_chrooted()
{
    return(is_chrooted);
}

API_EXPORT(int) ap_server_chroot_desired()
{
    return(ap_server_chroot);
}
