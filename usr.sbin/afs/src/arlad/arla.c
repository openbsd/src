/*	$OpenBSD: arla.c,v 1.1.1.1 1998/09/14 21:52:54 art Exp $	*/
/*	$OpenBSD: arla.c,v 1.1.1.1 1998/09/14 21:52:54 art Exp $	*/
/*
 * Copyright (c) 1995, 1996, 1997, 1998 Kungliga Tekniska Högskolan
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
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *      This product includes software developed by the Kungliga Tekniska
 *      Högskolan and its contributors.
 * 
 * 4. Neither the name of the Institute nor the names of its contributors
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
#include <sl.h>
#include <parse_units.h>
#include <getarg.h>

#include <version.h>

RCSID("$KTH: arla.c,v 1.87 1998/08/23 22:50:22 assar Exp $") ;

enum connected_mode connected_mode = CONNECTED;

static CredCacheEntry *tmpce;

static VenusFid cwd;
static VenusFid rootcwd;

static int arla_chdir(int, char **);
static int arla_ls(int, char **);
static int arla_cat(int, char **);
static int help(int, char **);
static int arla_quit(int, char **);
static int arla_conn_status(int, char **);
static int arla_vol_status(int, char **);
static int arla_cred_status(int, char **);
static int arla_fcache_status(int, char **);
static int arla_sysname(int, char**);
static int arla_rx_status(int argc, char **argv);
static int arla_flushfid(int argc, char **argv);

static SL_cmd cmds[] = {
    {"chdir", arla_chdir, "chdir directory"},
    {"cd"},
    {"ls",    arla_ls, "ls"},
    {"cat",   arla_cat, "cat file"},
    {"help",  help, "help"},
    {"?"},
    {"conn-status", arla_conn_status, "connection status"},
    {"vol-status", arla_vol_status, "volume cache status"},
    {"cred-status", arla_cred_status, "credentials status"},
    {"fcache-status", arla_fcache_status, "file cache status"},
    {"rx-status", arla_rx_status, "rx connection status"},
    {"flushfid", arla_flushfid, "flush a fid from the cache"},
    {"quit", arla_quit, "quit"},
    {"exit"},
    {"sysname", arla_sysname, "sysname"},
    { NULL }
};

static void
print_dir (VenusFid *fid, const char *name, void *v)
{
     printf("(%d, %d, %d, %d): %s\n", fid->Cell,
	    fid->fid.Volume,
	    fid->fid.Vnode,
	    fid->fid.Unique, name);
}

/*
 * Return 0 iff OK.
 */

static int
newwalk (VenusFid *startdir, char *fname)
{
     VenusFid cwd = *startdir;
     char *base;
     VenusFid file;
     Result ret;
     FCacheEntry *entry;
     int error;
     char symlink[MAXPATHLEN];
     char store_name[MAXPATHLEN];

     strcpy(store_name, fname);
     fname = store_name;

     do {
        /* set things up so that fname points to the remainder of the path,
         * whereas base points to the whatever preceeds the first /
         */
        base = fname;
        fname = strchr(fname, '/');
        if (fname) {
            /* deal with repeated adjacent / chars by eliminating the
             * duplicates. 
             */
            while (*fname == '/') {
                *fname = '\0';
                fname++;
            }
        }
 
        /* deal with absolute pathnames first. */
        if (*base == '\0') {
            cwd = rootcwd;
            if (fname) {
                if (strncmp("afs",fname,3) == 0) {
                    fname += 3;
                }
                continue;
            } else {
                break;
            }
	 }
	 ret = cm_lookup (cwd, base, &file, &tmpce);
	 if (ret.res) {
	     arla_warn (ADEBWARN, ret.error, "lookup(%s)", base);
	     return -1;
	 }
	 error = fcache_get (&entry, file, tmpce);
	 if (error) {
	     arla_warn (ADEBWARN, error, "fcache_get");
	     return -1;
	 }
	 error = fcache_get_data (entry, tmpce);
	 if (error) {
	     ReleaseWriteLock (&entry->lock);

	     arla_warn (ADEBWARN, error, "fcache_get_data");
	     return -1;
	 }
	 /* handle symlinks here */
	 if (entry->status.FileType == TYPE_LINK) {
	     int len;
	     int fd;

	     fd = fcache_open_file (entry, O_RDONLY, 0);
	     /* read the symlink and null-terminate it */
	     if (fd < 0) {
		 ReleaseWriteLock(&entry->lock);
		 arla_warn (ADEBWARN, errno, "fcache_open_file");
		 return -1;
	     }
	     len = read (fd, symlink, sizeof(symlink));
	     close (fd);
	     if (len <= 0) {
		 ReleaseWriteLock(&entry->lock);
		 arla_warnx (ADEBWARN, "cannot read symlink");
		 return -1;
	     }
	     symlink[len] = '\0';
	     /* if we're not at the end (i.e. fname is not null), take
	      * the expansion of the symlink and append fname to it.
	      */
	     if (fname != NULL) {
		 strcat (symlink, "/");
		 strcat (symlink, fname);
	     }
	     strcpy(store_name,symlink);
	     fname = store_name;
	 } else {
	     /* if not a symlink, just update cwd */
	     cwd = file;
	 }
	 ReleaseWriteLock (&entry->lock);

	 /* the *fname condition below deals with a trailing / in a
	  * path-name */
     } while (fname != NULL && *fname);
     *startdir = cwd;
     return 0;
}

static int
arla_quit (int argc, char **argv)
{
    printf("Thank you for using arla\n");
    return 1;
}

static int
arla_flushfid(int argc, char **argv)
{
    AFSCallBack broken_callback = {0, 0, CBDROPPED};
    VenusFid fid;
    
    if (argc != 2) {
	fprintf(stderr, "flushfid fid\n");
	return 0;
    }
    
    if ((sscanf(argv[1], "%d.%d.%d.%d", &fid.Cell, &fid.fid.Volume, 
		&fid.fid.Vnode, &fid.fid.Unique)) == 4) {
	;
    } else if ((sscanf(argv[1], "%d.%d.%d", &fid.fid.Volume, 
		       &fid.fid.Vnode, &fid.fid.Unique)) == 3) {
	fid.Cell = cwd.Cell;
    } else {
	fprintf(stderr, "flushfid fid\n");
	return 0;
    }
    
    fcache_stale_entry(fid, broken_callback);
    
    return 0;
}


static int
arla_chdir (int argc, char **argv)
{
    if (argc != 2) {
	printf ("usage: %s dir\n", argv[0]);
	return 0;
    }

    newwalk (&cwd, argv[1]);
    return 0;
}

static int
arla_ls (int argc, char **argv)
{
    int error;

    error = adir_readdir (cwd, print_dir, NULL, tmpce);
    if (error) {
	printf ("adir_readdir failed: %s\n", koerr_gettext(error));
    }
    return 0;
}

static int
arla_sysname (int argc, char **argv)
{
    switch (argc) {
    case 1:
	printf("sysname: %s\n", arlasysname);
	break;
    case 2:
	strncpy(arlasysname, argv[1], SYSNAMEMAXLEN);
	arlasysname[SYSNAMEMAXLEN-1] = '\0';
	printf("setting sysname to: %s\n", arlasysname);
	break;
    default:
	printf("syntax: sysname <sysname>\n");
	break;
    }
    return 0;
}



static int
arla_cat (int argc, char **argv)
{
    VenusFid fid;
    int fd;
    char buf[8192];
    int ret;
    FCacheEntry *e;
    
    if (argc != 2) {
	printf ("usage: %s file\n", argv[0]);
	return 0;
    }
    fid = cwd;
    if( newwalk (&fid, argv[1]) == 0) {

	ret = fcache_get (&e, fid, tmpce);
	if (ret) {
	    printf ("fcache_get failed: %d\n", ret);
	    return 0;
	}
	ret = fcache_get_data (e, tmpce);
	if (ret) {
	    ReleaseWriteLock (&e->lock);
	    printf ("fcache_get_data failed: %d\n", ret);
	    return 0;
	}

	fd = fcache_open_file (e, O_RDONLY, 0);

	if (fd < 0) {
	    ReleaseWriteLock (&e->lock);
	    printf ("fcache_open_file failed: %d\n", errno);
	    return 0;
	}
	while ((ret = read (fd, buf, sizeof(buf))) > 0) {
	    write (STDOUT_FILENO, buf, ret);
	}
	close (fd);
	ReleaseWriteLock (&e->lock);
    }
    return 0;
}

static int
help (int argc, char **argv)
{
    sl_help(cmds, argc, argv);
    return 0;
}

static int
arla_conn_status (int argc, char **argv)
{
    conn_status (stderr);
    return 0;
}

static int
arla_vol_status (int argc, char **argv)
{
    volcache_status (stderr);
    return 0;
}

static int
arla_cred_status (int argc, char **argv)
{
    cred_status (stderr);
    return 0;
}

static int
arla_fcache_status (int argc, char **argv)
{
    fcache_status (stderr);
    return 0;
}

static int
arla_rx_status(int argc, char **argv)
{
    rx_PrintStats(stderr);
    return 0;
}


static void
initrx (int port)
{
     int error;

     error = rx_Init (htons(port));
     if (error)
	  arla_err (1, ADEBERROR, error, "rx_init");
}

#ifdef KERBEROS

static int
get_cred(const char *princ, const char *inst, const char *krealm, 
         CREDENTIALS *c)
{
  KTEXT_ST foo;
  int k_errno;

  k_errno = krb_get_cred((char*)princ, (char*)inst, (char*)krealm, c);

  if(k_errno != KSUCCESS) {
    k_errno = krb_mk_req(&foo, (char*)princ, (char*)inst, (char*)krealm, 0);
    if (k_errno == KSUCCESS)
      k_errno = krb_get_cred((char*)princ, (char*)inst, (char*)krealm, c);
  }
  return k_errno;
}


#endif /* KERBEROS */

#define KERNEL_STACKSIZE (16*1024)

static void
store_state (void)
{
    arla_warnx (ADEBMISC, "storing state");
    fcache_store_state ();
    volcache_store_state ();
    cm_store_state ();
}

/*
 * signal handlers...
 */

static void
sigint (int foo)
{
    arla_warnx (ADEBMISC, "fatal signal received");
    store_state ();
    exit (0);
}

static void
sighup (int foo)
{
    store_state ();
}

static void
daemonify (void)
{
    pid_t pid;
    int fd;

    pid = fork ();
    if (pid < 0)
	arla_err (1, ADEBERROR, errno, "fork");
    else if (pid > 0)
	exit(0);
    if (setsid() == -1)
	arla_err (1, ADEBERROR, errno, "setsid");
    fd = open(_PATH_DEVNULL, O_RDWR, 0);
    if (fd < 0)
	arla_err (1, ADEBERROR, errno, "open " _PATH_DEVNULL);
    dup2 (fd, STDIN_FILENO);
    dup2 (fd, STDOUT_FILENO);
    dup2 (fd, STDERR_FILENO);
    close (fd);
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

static unsigned low_vnodes, high_vnodes, low_bytes, high_bytes;
static unsigned numcreds, numconns, numvols;

static struct conf_param conf_params[] = {
    {"low_vnodes",		&low_vnodes,	 ARLA_LOW_VNODES},
    {"high_vnodes",		&high_vnodes,	 ARLA_HIGH_VNODES},
    {"low_bytes",		&low_bytes,	 ARLA_LOW_BYTES},
    {"high_bytes",		&high_bytes,	 ARLA_HIGH_BYTES},
    {"numcreds",		&numcreds,	 ARLA_NUMCREDS},
    {"numconns",		&numconns,	 ARLA_NUMCREDS},
    {"numvols",			&numvols,	 ARLA_NUMVOLS},
    {"fpriority",               &fprioritylevel, FPRIO_DEFAULT},
    {NULL,			NULL,		 0}};

static int test_flag;
static char *conf_file = ARLACONFFILE;
static char *log_file  = NULL;
static char *device_file = "/dev/xfs0";
static char *debug_levels = NULL;
static char *connected_mode_string = NULL;
#ifdef KERBEROS
static char *rxkad_level_string = "auth";
#endif
static const char *temp_sysname = NULL;
static char *root_volume;
static char *cache_dir;
static int version_flag;
static int help_flag;
static int no_fork;
static int client_port = 4711;
static int recover = 1;

static struct getargs args[] = {
    {"test",	't',	arg_flag,	&test_flag,
     "run in test mode", NULL},
    {"conffile", 'c',	arg_string,	&conf_file,
     "path to configuration file", NULL},
    {"log",	'l',	arg_string,	&log_file,
     "where to write log (stderr (default), syslog, or path to file)", NULL},
    {"debug",	0,	arg_string,	&debug_levels,
     "what to write in the log", NULL},
    {"device",	'd',	arg_string,	&device_file,
     "the XFS device to use [/dev/xfs0]", NULL},
    {"connected-mode", 0, arg_string,	&connected_mode_string,
     "initial connected mode [conncted|fetch-only|disconnected]", NULL},
    {"no-fork",	'n',	arg_flag,	&no_fork,
     "don't fork and demonize", NULL},
#ifdef KERBEROS
    {"rxkad-level", 'r', arg_string,	&rxkad_level_string,
     "the rxkad level to use (clear, auth or crypt)", NULL},
#endif
    {"sysname",	 's',	arg_string,	&temp_sysname,
     "set the sysname of this system", NULL},
    {"root-volume",0,   arg_string,     &root_volume},
    {"port",	0,	arg_integer,	&client_port,
     "port number to use",	NULL},
    {"recover",	'z',	arg_negative_flag, &recover,
     "don't recover state",	NULL},
    {"cache-dir", 0,	arg_string,	&cache_dir,
     "cache directory",	NULL},
    {"version",	0,	arg_flag,	&version_flag,
     NULL, NULL},
    {"help",	0,	arg_flag,	&help_flag,
     NULL, NULL},
    {NULL,      0,      arg_end,        NULL, NULL, NULL}
};

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

static void
usage (int ret)
{
    arg_printusage (args,
		    NULL,
		    "[device]");
    exit (ret);
}


/*
 *
 */

int
main (int argc, char **argv)
{
    PROCESS kernelpid;
    int error;
    int optind = 0;
    char *default_log_file;
    char fpriofile[MAXPATHLEN];

    set_progname (argv[0]);

    if (getarg (args, argc, argv, &optind, ARG_GNUSTYLE))
	usage (1);

    argc -= optind;
    argv += optind;

    if (help_flag)
	usage (0);

    if (version_flag)
	errx (1, "%s", arla_version);
    
    if (argc > 0) {
	device_file = *argv;
	argc--;
	argv++;
    }

    if (argc != 0)
	usage (1);

    if (temp_sysname == NULL)
	temp_sysname = arla_getsysname ();

    if (temp_sysname != NULL) {
        strncpy(arlasysname, temp_sysname, SYSNAMEMAXLEN - 1);
	arlasysname[SYSNAMEMAXLEN - 1] = '\0';
    }

#ifdef KERBEROS
    rxkad_min_level = parse_rxkad_level (rxkad_level_string);
    if (rxkad_min_level < 0)
	errx (1, "bad rxkad level `%s'", rxkad_level_string);
#endif

    signal (SIGINT, sigint);
    signal (SIGTERM, sigint);
    signal (SIGHUP, sighup);
    umask (S_IRWXG|S_IRWXO); /* 077 */

    if (!test_flag && !no_fork) {
	default_log_file = "syslog";
    } else {
	default_log_file = "/dev/stderr";
    }

    if (log_file == NULL)
	log_file = default_log_file;

    arla_loginit(log_file);
     
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

    if (!test_flag && !no_fork)
	daemonify ();

    if (cache_dir == NULL)
	cache_dir = ARLACACHEDIR;

    mkdir (cache_dir, 0777);
    if (chdir (cache_dir) < 0)
	arla_err (1, ADEBERROR, errno, "chdir %s", cache_dir);


    snprintf(fpriofile, sizeof(fpriofile), "%s/%s", cache_dir, "fprio");

    /*
     * Init
     */ 

    arla_warnx (ADEBINIT,"Arlad booting sequence:");
    arla_warnx (ADEBINIT, "connected mode: %s",
		connected_levels[connected_mode]);
    arla_warnx (ADEBINIT, "initports");
    initports ();
    arla_warnx (ADEBINIT, "conn_init numconns = %u", numconns);
    conn_init (numconns);
    arla_warnx (ADEBINIT, "cellcache");
    cell_init (0);
    arla_warnx (ADEBINIT, "fprio");
    fprio_init(fpriofile);
    arla_warnx (ADEBINIT, "volcache numvols = %u", numvols);
    volcache_init (numvols, recover);
    if (root_volume != NULL)
	volcache_set_rootvolume (root_volume);
    arla_warnx (ADEBINIT, "rx");
    initrx (client_port);
#ifdef KERBEROS
    arla_warnx (ADEBINIT, "using rxkad level %s",
		rxkad_level_units[rxkad_min_level]);
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

    arla_warnx(ADEBINIT, "arla init done.");

    if (test_flag) {
#ifdef KERBEROS
	{
	    krbstruct krbdata;
	    int ret;
	    const char *this_cell = cell_getthiscell ();
	    const char *db_server = cell_findnamedbbyname (this_cell);
	    char *realm = krb_realmofhost (db_server);
	    
	    ret = get_cred("afs", this_cell, realm, &krbdata.c);
	    if (ret)
		ret = get_cred("afs", "", realm, &krbdata.c);

	    if (ret) {
		arla_warnx (ADEBWARN,
			    "getting ticket for %s: %s",
			    this_cell,
			    krb_get_err_text (ret));
	    } else if (cred_add_krb4(getuid(), &krbdata.c) == NULL) {
		arla_warnx (ADEBWARN, "Could not insert tokens to arla");
	    }
	}
#endif

	tmpce = cred_get (0, getuid(), CRED_ANY);
	 
	arla_warnx (ADEBINIT, "Getting root...");
	error = getroot (&rootcwd, tmpce);
	if (error)
	    arla_err (1, ADEBERROR, error, "getroot");
	cwd = rootcwd;
	arla_warnx(ADEBINIT, "arla loop started");
	sl_loop(cmds, "arla> ");
	store_state ();
    } else {
	xfs_message_init ();

	if (LWP_CreateProcess (kernel_interface, KERNEL_STACKSIZE, 1,
			       device_file, "Kernel-interface", &kernelpid))
	    arla_errx (1, ADEBERROR,
		       "Cannot create kernel-interface process");
	LWP_WaitProcess ((char *)main);
	abort ();
    }

    return 0;
}
