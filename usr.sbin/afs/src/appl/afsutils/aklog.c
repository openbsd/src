/*
 * Copyright (c) 1995 - 2001 Kungliga Tekniska Högskolan
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
 * allbery@ece.cmu.edu 2000-05-13
 * Lifted from kth-krb, bodged parts of klog.c into it.  The result is that
 * it gets the AFS token for your Kerberos principal, not your user ID.
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

RCSID("$arla: aklog.c,v 1.10 2002/04/26 16:11:39 lha Exp $");

#include "appl_locl.h"
#include "kafs.h"

#ifndef KERBEROS

int
main(int argc, char **argv)
{
    errx (1, "Kerberos support isn't compiled in");
    return 1;
}

#else

#ifndef HAVE_KRB_AFSLOG_UID
static int
krb_afslog_uid (const char *cell, const char *realm, uid_t afsid)
{
    return k_afsklog_uid(cell, realm, afsid);
}
#endif

#ifndef HAVE_KRB_GET_DEFAULT_PRINCIPAL

/* stolen from krb4's lib/krb/get_default_principal.c */

static int
krb_get_default_principal(char *name, char *instance, char *realm)
{
  char *file;
  int ret;
  char *p;

  file = tkt_string ();
  
  ret = krb_get_tf_fullname(file, name, instance, realm);
  if(ret == KSUCCESS)
      return 0;

  p = getenv("KRB4PRINCIPAL");
  if(p && kname_parse(name, instance, realm, p) == KSUCCESS)
      return 1;

#ifdef HAVE_PWD_H
  {
    struct passwd *pw;
    pw = getpwuid(getuid());
    if(pw == NULL){
      return -1;
    }

    strlcpy (name, pw->pw_name, ANAME_SZ);
    strlcpy (instance, "", INST_SZ);
    krb_get_lrealm(realm, 1);

    if(strcmp(name, "root") == 0) {
      p = NULL;
#if defined(HAVE_GETLOGIN) && !defined(POSIX_GETLOGIN)
      p = getlogin();
#endif
      if(p == NULL)
	p = getenv("USER");
      if(p == NULL)
	p = getenv("LOGNAME");
      if(p){
	  strlcpy (name, p, ANAME_SZ);
	  strlcpy (instance, "root", INST_SZ);
      }
    }
    return 1;
  }
#else
  return -1;
#endif
}
#endif

static int debug = 0;

static void __attribute__((format (printf, 1, 2)))
DEBUG (const char *fmt, ...)  
{
    va_list ap;
    if (debug) {
	va_start(ap, fmt);
	vwarnx(fmt, ap);
	va_end(ap);
    }
}

static char krb_name[ANAME_SZ];
static char krb_instance[INST_SZ];
static char krb_realm[REALM_SZ];
static char name_inst[ANAME_SZ+INST_SZ+2];
static char *arg_cell = NULL;
static int arg_createuser = 0;
static int arg_debug = 0;
static int arg_help = 0;
static int arg_hosts = 0;
static char *arg_realm = NULL;
static int arg_noprdb = 0;
static char *arg_path = NULL;
static int arg_quiet = 0;
static int arg_unlog = 0;
static int arg_version = 0;
static int arg_zsubs = 0;

/* 
 * Figure out the AFS ID of a user name 
 */

static uint32_t
get_afs_id (const char *username, const char *cell, int dontuseafs_p)
{
    int32_t returned_id;
    int ret;
    struct passwd *pwd;

    DEBUG ("get_afs_id for %s in cell %s %susing pts-db", username, cell,
	   dontuseafs_p ? "not ": "");

    if (dontuseafs_p == 0) {
	ret = arlalib_get_viceid (username, cell, &returned_id);
	if (ret == 0)
	    return returned_id;
	DEBUG ("arlalib_get_viceid failed with: %s (%d).",
	       koerr_gettext(ret), ret);
    }

    /* 
     * If we failed to talk to any server, try various stupid means of
     * guessing the AFS ID 
     */

    pwd = getpwnam(krb_name);
    if(pwd == NULL) {
	returned_id = getuid();
	warnx ("Couldn't get AFS ID for %s%s, using current UID (%d)",
	       username, cell, returned_id);
    } else {
	returned_id = pwd->pw_uid;
	warnx ("Couldn't get AFS ID for %s@%s, using %d "
	       "from /etc/passwd",
	       username, cell, returned_id);
    }
    return (returned_id);
}

static int
createuser (const char *username, const char *remotecell)
{
    char cmd[1024];

    snprintf (cmd, sizeof(cmd), "pts createuser %s -cell %s",
	      username, remotecell);
    DEBUG("Executing %s", cmd);
    return system(cmd);
}

static void
get_tokens (const char **cells,
	    const char *localcell,
	    const char *realm)
{
    uint32_t afsid;
    int rc;

    while (*cells) {
	char *username;
	/* 
	 * If the `*cells' isn't local cell, try the username
	 * `username@localcell'
	 */
	if (strcasecmp (localcell, *cells) != 0)
	    asprintf (&username, "%s@%s", name_inst, localcell);
	else
	    username = name_inst;
	
	afsid = get_afs_id (username, *cells, arg_noprdb);
	rc = krb_afslog_uid(*cells, realm, afsid);
	if (rc)
	    warnx ("Failed getting tokens for cell %s.", *cells);

	/* If not localcell, create user (if wanted) and free username) */
	if (name_inst != username) {
	    if (arg_createuser && createuser(username, *cells))
		warnx ("Failed creating user in cell %s", *cells);
	    free (username);
	}
	cells++;
    }
}


struct agetargs args[] = {
    {"cell", 0, aarg_string, &arg_cell,
     "cell to authenticate to", "cell name", aarg_optional},
    {"createuser", 0, aarg_flag, &arg_createuser,
     "create PTS user in remote cell", NULL, aarg_optional},
    {"debug", 0, aarg_flag, &arg_debug,
     "print debugging information", NULL, aarg_optional},
    {"help", 0, aarg_flag, &arg_help,
     "print help", NULL, aarg_optional},
    {"hosts", 0, aarg_flag, &arg_hosts,
     "print host address information (unimplemented)", NULL, aarg_optional},
    {"krbrealm", 0, aarg_string, &arg_realm,
     "kerberos realm for cell", "kerberos realm", aarg_optional},
    {"noprdb", 0, aarg_flag, &arg_noprdb,
     "don't try to determine AFS ID", NULL, aarg_optional},
    {"path", 0, aarg_string, &arg_path,
     "AFS path to authenticate to", "path", aarg_optional},
    {"quiet", 0, aarg_flag, &arg_quiet,
     "fail silently if no ticket cache", NULL, aarg_optional},
    {"unlog", 0, aarg_flag, &arg_unlog,
     "discard tokens", NULL, aarg_optional},
    {"version", 0, aarg_flag, &arg_version,
     "print version", NULL, aarg_optional},
    {"zsubs", 0, aarg_flag, &arg_zsubs,
     "update zephyr subscriptions (unimplemented)"},
    {NULL, 0, aarg_end, NULL, NULL},
};

static void
usage (int exit_code)
{
    aarg_printusage (args, NULL, "<cell or path>", AARG_AFSSTYLE|AARG_USEFIRST);
    exit (exit_code);
}

/*
 *
 */

int
main (int argc, char **argv)
{
    const char *cell = NULL;
    char cellbuf[64];
    const char *localcell;
    Log_method *method;
    int optind = 0;
    int rc;

    setprogname(argv[0]);

    ports_init();
    method = log_open (getprogname(), "/dev/stderr:notime");
    if (method == NULL)
	errx (1, "log_open failed");
    cell_init(0, method);
    localcell = cell_getthiscell();

    rc = agetarg(args, argc, argv, &optind, AARG_AFSSTYLE|AARG_USEFIRST);
    if(rc && *argv[optind] == '-') {
	warnx ("Bad argument: %s", argv[optind]);
	usage (1);
    }

    if(arg_help)
	usage (0);

    if(arg_version)
	errx (0, "part of %s-%s", PACKAGE, VERSION);

    if (arg_debug)
	debug = 1;

    if(!k_hasafs())
	errx (1, "AFS support is not loaded.");

    /*
     * get the realm and name of the user
     */

    if (krb_get_default_principal(krb_name, krb_instance, krb_realm) < 0) {
	if (!arg_quiet)
	    warnx ("Could not even figure out who you are");
	exit (1);
    }
    snprintf(name_inst, sizeof(name_inst),
	     "%s%s%s",
	     krb_name, *krb_instance ? "." : "", krb_instance);

    if (arg_cell)
	cell = cell_expand_cell(arg_cell);

    if (arg_unlog) {
	rc = k_unlog();
	exit (rc);
    }
    if (arg_hosts)
	warnx ("Argument -hosts is not implemented.");
    if (arg_zsubs)
	warnx ("Argument -zsubs is not implemented.");

    if (rc && argv[optind]) {
	if (strcmp(argv[optind], ".") == 0
	   || strcmp(argv[optind], "..") == 0
	   || strchr(argv[optind], '/')) {
	    DEBUG ("I guess that \"%s\" is a filename.", argv[optind]);
	    arg_path = argv[optind];
	} else {
	    cell = cell_expand_cell(argv[optind]);
	    DEBUG ("I guess that %s is cell %s.", argv[optind], cell);
	}
    }

    if (arg_path) {
	if (k_afs_cell_of_file(arg_path, cellbuf, sizeof(cellbuf)))
	    errx (1, "No cell found for file \"%s\".", arg_path);
	cell = cellbuf;
    }

    /*
     * If not given an argument, use TheseCells to fetch tokens,
     * If given argument, do as the user tells us.
     */

    if (cell == NULL && arg_realm == NULL && arg_path == NULL) {
	const char **cells = cell_thesecells ();
	get_tokens (cells, localcell, NULL);
    } else {
	const char *cells[2] = { NULL, NULL };
	cells[0] = cell;
	get_tokens (cells, localcell, arg_realm);
    }
    
    return 0;
}

#endif /* KERBEROS */
