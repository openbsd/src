/*
 * Copyright (c) 1995 - 2000 Kungliga Tekniska Högskolan
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
 * Test to talk with FS
 */

#include "arla_local.h"
#include <parse_units.h>
#include <getarg.h>

RCSID("$KTH: arla.c,v 1.135.2.2 2001/09/14 13:26:31 lha Exp $") ;

enum connected_mode connected_mode = CONNECTED;

static void
initrx (int port)
{
     int error;

     error = rx_Init (htons(port));
     if (error)
	  arla_err (1, ADEBERROR, error, "rx_init");
}


void
store_state (void)
{
    arla_warnx (ADEBMISC, "storing state");
    fcache_store_state ();
    volcache_store_state ();
    cm_store_state ();
}

struct conf_param {
    const char *name;
    unsigned *val;
    unsigned default_val;
};

/*
 * Reads in a configuration file, and sets some defaults.
 */

static struct units size_units[] = {
    { "M", 1024 * 1024 },
    { "k", 1024 },
    { NULL, 0 }
};

static void
read_conffile(char *fname,
	      struct conf_param *params)
{
    FILE *fp;
    char buf[256];
    int lineno;
    struct conf_param *p;

    for (p = params; p->name; ++p)
	*(p->val) = p->default_val;

    arla_warnx (ADEBINIT, "read_conffile: %s", fname);

    fp = fopen(fname, "r");
    if (fp == NULL) {
	arla_warn (ADEBINIT, errno, "open %s", fname);
	return;
    }

    lineno = 0;

    while (fgets (buf, sizeof(buf), fp) != NULL) {
	struct conf_param *partial_param = NULL;
	int partial_match = 0;
	char *save = NULL;
	char *n;
	char *v;
	unsigned val;
	char *endptr;

	++lineno;
	if (buf[strlen(buf) - 1] == '\n')
	    buf[strlen(buf) - 1] = '\0';
	if (buf[0] == '#')
	    continue;

	n = strtok_r (buf, " \t", &save);
	if (n == NULL) {
	    fprintf (stderr, "%s:%d: no parameter?\n", fname, lineno);
	    continue;
	}

	v = strtok_r (NULL, " \t", &save);
	if (v == NULL) {
	    fprintf (stderr, "%s:%d: no value?\n", fname, lineno);
	    continue;
	}

	val = parse_units(v, size_units, NULL);
	if(val == (unsigned)-1) {
	    val = strtol(v, &endptr, 0);
	    if (endptr == v)
		fprintf (stderr, "%s:%d: bad value `%s'\n",
			 fname, lineno, v);
	}
	    
	for (p = params; p->name; ++p) {
	    if (strcmp(n, p->name) == 0) {
		partial_match = 1;
		partial_param = p;
		break;
	    } else if(strncmp(n, p->name, strlen(n)) == 0) {
		++partial_match;
		partial_param = p;
	    }
	}
	if (partial_match == 1)
	    *(partial_param->val) = val;
	else if (partial_match == 0)
	    fprintf (stderr, "%s:%d: unknown parameter `%s'\n",
		     fname, lineno, n);
	else
	    fprintf (stderr, "%s:%d: ambiguous parameter `%s'\n",
		     fname, lineno, n);
    }
    fclose(fp);
}

#if KERBEROS && !defined(HAVE_KRB_GET_ERR_TEXT)

#ifndef MAX_KRB_ERRORS
#define MAX_KRB_ERRORS 256
#endif

static const char err_failure[] = "Unknown error code passed (krb_get_err_text)";

const char *
krb_get_err_text(int code)
{
  if(code < 0 || code >= MAX_KRB_ERRORS)
    return err_failure;
  return krb_err_txt[code];
}
#endif

static unsigned low_vnodes, high_vnodes, low_bytes, high_bytes;
static unsigned numcreds, numconns, numvols, dynrootlevel;

static struct conf_param conf_params[] = {
    {"low_vnodes",		&low_vnodes,	 ARLA_LOW_VNODES},
    {"high_vnodes",		&high_vnodes,	 ARLA_HIGH_VNODES},
    {"low_bytes",		&low_bytes,	 ARLA_LOW_BYTES},
    {"high_bytes",		&high_bytes,	 ARLA_HIGH_BYTES},
    {"numcreds",		&numcreds,	 ARLA_NUMCREDS},
    {"numconns",		&numconns,	 ARLA_NUMCREDS},
    {"numvols",			&numvols,	 ARLA_NUMVOLS},
    {"fpriority",               &fprioritylevel, FPRIO_DEFAULT},
    {"dynroot",                 &dynrootlevel,   DYNROOT_DEFAULT},
    {NULL,			NULL,		 0}};

char *conf_file = ARLACONFFILE;
char *log_file  = NULL;
char *debug_levels = NULL;
char *connected_mode_string = NULL;
#ifdef KERBEROS
char *rxkad_level_string = "auth";
#endif
const char *temp_sysname = NULL;
char *root_volume;
int cpu_usage;
int version_flag;
int help_flag;
int recover = 0;
int dynroot_enable = 0;
int cm_consistency = 0;

/*
 * These are exported to other modules
 */

int num_workers = 16;
char *cache_dir;
int fake_mp;
int fork_flag = 1;

/*
 * Global AFS variables, se arla_local.h for comment
 */

int afs_BusyWaitPeriod = 15;

/*
 *
 */

static int
parse_string_list (const char *s, const char **units)
{
    const char **p;
    int partial_val = 0;
    int partial_match = 0;
    
    for (p = units; *p; ++p) {
	if (strcmp (s, *p) == 0)
	    return p - units;
	if (strncmp (s, *p, strlen(s)) == 0) {
	    partial_match++;
	    partial_val = p - units;
	}
    }
    if (partial_match == 1)
	return partial_val;
    else
	return -1;
}

#ifdef KERBEROS
static const char *rxkad_level_units[] = {
"clear",			/* 0 */
"auth",				/* 1 */
"crypt",			/* 2 */
NULL
};

static int
parse_rxkad_level (const char *s)
{
    return parse_string_list (s, rxkad_level_units);
}
#endif

static const char *connected_levels[] = {
"connected",			/* CONNECTED   = 0 */
"fetch-only",			/* FETCH_ONLY  = 1 */
"disconnected",			/* DISCONNCTED = 2 */
NULL
};

static int
set_connected_mode (const char *s)
{
    return parse_string_list (s, connected_levels);
}

/*
 *
 */

int
arla_init (int argc, char **argv)
{
    log_flags log_flags;
    char fpriofile[MAXPATHLEN];

    if (temp_sysname == NULL)
	temp_sysname = arla_getsysname ();

    if (temp_sysname != NULL)
        strlcpy(arlasysname, temp_sysname, SYSNAMEMAXLEN);

#ifdef KERBEROS
    conn_rxkad_level = parse_rxkad_level (rxkad_level_string);
    if (conn_rxkad_level < 0)
	errx (1, "bad rxkad level `%s'", rxkad_level_string);
#endif

    if (log_file == NULL)
	log_file = default_log_file;

    log_flags = 0;
    if (cpu_usage)
	log_flags |= LOG_CPU_USAGE;
    arla_loginit(log_file, log_flags);
     
    if (debug_levels != NULL) {
	if (arla_log_set_level (debug_levels) < 0) {
	    warnx ("bad debug levels: `%s'", debug_levels);
	    arla_log_print_levels (stderr);
	    exit (1);
	}
    }

    if (connected_mode_string != NULL) {
	int tmp = set_connected_mode (connected_mode_string);

	if (tmp < 0)
	    errx (1, "bad connected mode: `%s'", connected_mode_string);
	connected_mode = tmp;
    }

    read_conffile(conf_file, conf_params);

    if (cache_dir == NULL)
	cache_dir = get_default_cache_dir();

    if (mkdir (cache_dir, 0777) < 0 && errno != EEXIST)
	arla_err (1, ADEBERROR, errno, "mkdir %s", cache_dir);
    if (chdir (cache_dir) < 0)
	arla_err (1, ADEBERROR, errno, "chdir %s", cache_dir);


    if (dynrootlevel || dynroot_enable)
	dynroot_setenable (TRUE);

    snprintf(fpriofile, sizeof(fpriofile), "%s/%s", cache_dir, "fprio");

    /*
     * Init
     */ 

    arla_warnx (ADEBINIT,"Arlad booting sequence:");
    arla_warnx (ADEBINIT, "connected mode: %s",
		connected_levels[connected_mode]);
    arla_warnx (ADEBINIT, "ports_init");
    ports_init ();
    arla_warnx (ADEBINIT, "rx");
    initrx (client_port);
    arla_warnx (ADEBINIT, "conn_init numconns = %u", numconns);
    conn_init (numconns);
    arla_warnx (ADEBINIT, "cellcache");
    cell_init (0, arla_log_method);
    arla_warnx (ADEBINIT, "fprio");
    fprio_init(fpriofile);
    arla_warnx (ADEBINIT, "volcache numvols = %u", numvols);
    volcache_init (numvols, recover);
    if (root_volume != NULL)
	volcache_set_rootvolume (root_volume);
#ifdef KERBEROS
    arla_warnx (ADEBINIT, "using rxkad level %s",
		rxkad_level_units[conn_rxkad_level]);
#endif

    /*
     * Credential cache
     */
    arla_warnx (ADEBINIT, "credcache numcreds = %u", numcreds);
    cred_init (numcreds);

    arla_warnx (ADEBINIT,
		"fcache low_vnodes = %u, high_vnodes = %u"
		"low_bytes = %u, high_bytes = %u",
		low_vnodes, high_vnodes,
		low_bytes, high_bytes);
    fcache_init (low_vnodes, high_vnodes,
		 low_bytes, high_bytes, recover);

    arla_warnx (ADEBINIT, "cmcb");
    cmcb_init ();

    arla_warnx(ADEBINIT, "cm");
    cm_init ();

    if (cm_consistency) {
	arla_warnx(ADEBINIT, "turning on consistency test");
	cm_turn_on_consistency_check();
    }

    arla_warnx(ADEBINIT, "arla init done.");

    return 0;
}
