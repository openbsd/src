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

#include <arla_local.h>
#include <sl.h>
#include <getarg.h>
#include <arla-version.h>

RCSID("$KTH: arla-cli.c,v 1.7 2000/12/04 22:45:19 lha Exp $");

char *default_log_file = "/dev/stderr";
char *default_arla_cachedir = ".arlacache";
int client_port = 4712;


/* creds used for all the interactive usage */

static CredCacheEntry *ce;

static VenusFid cwd;
static VenusFid rootcwd;

static int arla_chdir(int, char **);
static int arla_ls(int, char **);
static int arla_cat(int, char **);
static int arla_cp(int, char **);
static int arla_wc(int, char **);
static int help(int, char **);
static int arla_quit(int, char **);
static int arla_checkserver(int, char **);
static int arla_conn_status(int, char **);
static int arla_vol_status(int, char **);
static int arla_cred_status(int, char **);
static int arla_fcache_status(int, char **);
static int arla_cell_status (int, char **);
static int arla_sysname(int, char**);
#ifdef RXDEBUG
static int arla_rx_status(int argc, char **argv);
#endif
static int arla_flushfid(int argc, char **argv);

static SL_cmd cmds[] = {
    {"chdir", arla_chdir, "chdir directory"},
    {"cd"},
    {"ls",    arla_ls, "ls"},
    {"cat",   arla_cat, "cat file"},
    {"cp",    arla_cp, "copy file"},
    {"wc",    arla_wc, "wc file"},
    {"help",  help, "help"},
    {"?"},
    {"checkservers", arla_checkserver, "poll servers are down"},
    {"conn-status", arla_conn_status, "connection status"},
    {"vol-status", arla_vol_status, "volume cache status"},
    {"cred-status", arla_cred_status, "credentials status"},
    {"fcache-status", arla_fcache_status, "file cache status"},
    {"cell-status", arla_cell_status, "cell status"},
#ifdef RXDEBUG
    {"rx-status", arla_rx_status, "rx connection status"},
#endif
    {"flushfid", arla_flushfid, "flush a fid from the cache"},
    {"quit", arla_quit, "quit"},
    {"exit"},
    {"sysname", arla_sysname, "sysname"},
    { NULL }
};

/* An emulation of kernel lookup, convert (startdir, fname) into
 * (startdir).  Strips away leading /afs, removes duobles slashes,
 * and resolves symlinks.
 * Return 0 for success, otherwise -1.
 */

static int
walk (VenusFid *startdir, char *fname)
{
     VenusFid cwd = *startdir;
     char *base;
     VenusFid file;
     Result ret;
     FCacheEntry *entry;
     int error;
     char symlink[MAXPATHLEN];
     char store_name[MAXPATHLEN];

     strlcpy(store_name, fname, sizeof(store_name));
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
	 ret = cm_lookup (&cwd, base, &file, &ce, TRUE);
	 if (ret.res) {
	     arla_warn (ADEBWARN, ret.error, "lookup(%s)", base);
	     return -1;
	 }
	 error = fcache_get_data (&entry, &file, &ce);
	 if (error) {
	     arla_warn (ADEBWARN, error, "fcache_get");
	     return -1;
	 }

	 /* handle symlinks here */
	 if (entry->status.FileType == TYPE_LINK) {
	     int len;
	     int fd;

	     fd = fcache_open_file (entry, O_RDONLY);
	     /* read the symlink and null-terminate it */
	     if (fd < 0) {
		 fcache_release(entry);
		 arla_warn (ADEBWARN, errno, "fcache_open_file");
		 return -1;
	     }
	     len = read (fd, symlink, sizeof(symlink));
	     close (fd);
	     if (len <= 0) {
		 fcache_release(entry);
		 arla_warnx (ADEBWARN, "cannot read symlink");
		 return -1;
	     }
	     symlink[len] = '\0';
	     /* if we're not at the end (i.e. fname is not null), take
	      * the expansion of the symlink and append fname to it.
	      */
	     if (fname != NULL) {
		 strlcat(symlink, "/", sizeof(symlink));
		 strlcat(symlink, fname, sizeof(symlink));
	     }
	     strlcpy(store_name, symlink, sizeof(store_name));
	     fname = store_name;
	 } else {
	     /* if not a symlink, just update cwd */
	     cwd = file;
	 }
	 fcache_release(entry);

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

    if(walk (&cwd, argv[1]))
	printf ("walk %s failed\n", argv[1]);
    return 0;
}

static void
print_dir (VenusFid *fid, const char *name, void *v)
{
     printf("(%d, %d, %d, %d): %s\n", fid->Cell,
	    fid->fid.Volume,
	    fid->fid.Vnode,
	    fid->fid.Unique, name);
}

struct ls_context {
    VenusFid *dir_fid;
    CredCacheEntry *ce;
};

static void
print_dir_long (VenusFid *fid, const char *name, void *v)
{
    Result res;
    int ret;
    AFSFetchStatus status;
    VenusFid realfid;
    AccessEntry *ae;
    struct ls_context *context = (struct ls_context *)v;
    char type;
    CredCacheEntry *ce = context->ce;
    VenusFid *dir_fid  = context->dir_fid;
    char timestr[20];
    struct tm *t;
    time_t ti;

    if (VenusFid_cmp(fid, dir_fid) == 0)
	return;

    ret = followmountpoint (fid, dir_fid, NULL, &ce);
    if (ret) {
	printf ("follow %s: %d\n", name, ret);
	return;
    }

    /* Have we follow a mountpoint to ourself ? */
    if (VenusFid_cmp(fid, dir_fid) == 0)
	return;

    res = cm_getattr (*fid, &status, &realfid, context->ce, &ae);
    if (res.res) {
	printf ("%s: %d\n", name, res.res);
	return;
    }

    switch (status.FileType) {
    case TYPE_FILE :
	type = '-';
	break;
    case TYPE_DIR :
	type = 'd';
	break;
    case TYPE_LINK :
	type = 'l';
	break;
    default :
	abort ();
    }

    printf("(%4d, %8d, %8d, %8d): ",
	   fid->Cell,
	   fid->fid.Volume,
	   fid->fid.Vnode,
	   fid->fid.Unique);

    ti = status.ClientModTime;
    t = localtime (&ti);
    strftime (timestr, sizeof(timestr), "%Y-%m-%d", t);
    printf ("%c%c%c%c%c%c%c%c%c%c %2d %6d %6d %8d %s ",
	    type,
	    status.UnixModeBits & 0x100 ? 'w' : '-',
	    status.UnixModeBits & 0x080 ? 'r' : '-',
	    status.UnixModeBits & 0x040 ? 'x' : '-',
	    status.UnixModeBits & 0x020 ? 'w' : '-',
	    status.UnixModeBits & 0x010 ? 'r' : '-',
	    status.UnixModeBits & 0x008 ? 'x' : '-',
	    status.UnixModeBits & 0x004 ? 'w' : '-',
	    status.UnixModeBits & 0x002 ? 'r' : '-',
	    status.UnixModeBits & 0x001 ? 'x' : '-',
	    status.LinkCount,
	    status.Owner,
	    status.Group,
	    status.Length,
	    timestr);

    printf ("v %d ", status.DataVersion);

    printf ("%s\n", name);
}

static int
arla_ls (int argc, char **argv)
{
    struct getargs args[] = {
	{NULL, 'l', arg_flag, NULL},
    };
    int l_flag = 0;
    int error;
    int optind = 0;
    struct ls_context context;

    args[0].value = &l_flag;

    if (getarg (args, sizeof(args)/sizeof(*args),  argc, argv, &optind)) {
	arg_printusage (args, sizeof(args)/sizeof(*args), "ls", NULL);
	return 0;
    }
    context.dir_fid = &cwd;
    context.ce      = ce;
    error = adir_readdir (&cwd, l_flag ? print_dir_long : print_dir,
			  &context, &ce);
    if (error)
	printf ("adir_readdir failed: %s\n", koerr_gettext(error));
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
	strlcpy(arlasysname, argv[1], SYSNAMEMAXLEN);
	printf("setting sysname to: %s\n", arlasysname);
	break;
    default:
	printf("syntax: sysname <sysname>\n");
	break;
    }
    return 0;
}


static int
arla_cat_et_wc (int argc, char **argv, int do_cat, int out_fd)
{
    VenusFid fid;
    int fd;
    char buf[8192];
    int ret;
    FCacheEntry *e;
    size_t size = 0;
    
    if (argc != 2) {
	printf ("usage: %s file\n", argv[0]);
	return 0;
    }
    fid = cwd;
    if(walk (&fid, argv[1]) == 0) {

	ret = fcache_get_data (&e, &fid, &ce);
	if (ret) {
	    printf ("fcache_get_data failed: %d\n", ret);
	    return 0;
	}

	fd = fcache_open_file (e, O_RDONLY);

	if (fd < 0) {
	    fcache_release(e);
	    printf ("fcache_open_file failed: %d\n", errno);
	    return 0;
	}
	while ((ret = read (fd, buf, sizeof(buf))) > 0) {
	    if(do_cat)
		write (out_fd, buf, ret);
	    else
		size += ret;
	}
	if(!do_cat)
	    printf("%lu %s\n", (unsigned long)size, argv[1]);
	close (fd);
	fcache_release(e);
    }
    return 0;
}

static int
arla_cat (int argc, char **argv)
{
    return arla_cat_et_wc(argc, argv, 1, STDOUT_FILENO);
}

static int
arla_cp (int argc, char **argv)
{
    char *nargv[3];
    int fd, ret;

    if (argc != 3) {
	printf ("usage: %s from-file to-file\n", argv[0]);
	return 0;
    }
    
    fd = open (argv[2], O_CREAT|O_WRONLY|O_TRUNC, 0600);
    if (fd < 0) {
	warn ("open");
	return 0;
    }	

    nargv[0] = argv[0];
    nargv[1] = argv[1];
    nargv[2] = NULL;

    ret = arla_cat_et_wc(argc-1, nargv, 1, fd);
    close (fd);
    return ret;
	
}

static int
arla_wc (int argc, char **argv)
{
    return arla_cat_et_wc(argc, argv, 0, -1);
}


static int
help (int argc, char **argv)
{
    sl_help(cmds, argc, argv);
    return 0;
}

static int
arla_checkserver (int argc, char **argv)
{
    u_int32_t hosts[12];
    int num = sizeof(hosts)/sizeof(hosts[0]);

    conn_downhosts(cwd.Cell, hosts, &num, 0);
    if (num < 0 || num > sizeof(hosts)/sizeof(hosts[0])) {
	fprintf (stderr, "conn_downhosts returned bogus num: %d\n", num);
	return 0;
    }
    if (num == 0) {
	printf ("no servers down in %s\n", cell_num2name(cwd.Cell));
    } else {
	while (num) {
	    struct in_addr in;
	    in.s_addr = hosts[num];
	    printf ("down: %s\n", inet_ntoa(in));
	    num--;
	}
    }
    
    return 0;
}

static int
arla_conn_status (int argc, char **argv)
{
    conn_status ();
    return 0;
}

static int
arla_vol_status (int argc, char **argv)
{
    volcache_status ();
    return 0;
}

static int
arla_cred_status (int argc, char **argv)
{
    cred_status ();
    return 0;
}

static int
arla_fcache_status (int argc, char **argv)
{
    fcache_status ();
    return 0;
}

static int
arla_cell_status (int argc, char **argv)
{
    cell_entry *c;

    if (argc != 2) {
	printf ("usage: %s <cell-name>\n", argv[0]);
	return 0;
    }
    c = cell_get_by_name(argv[1]);
    if (c == NULL)
	printf ("no such cell\n");
    else
	cell_print_cell (c, stdout);
    return 0;
}

#ifdef RXDEBUG
static int
arla_rx_status(int argc, char **argv)
{
    rx_PrintStats(stderr);
    return 0;
}
#endif


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


void
arla_start (char *device_file, const char *cache_dir)
{
    int error;
#ifdef KERBEROS
    {
	krbstruct krbdata;
	int ret;
	char *realm;
	const char *this_cell = cell_getthiscell ();
	const char *db_server = cell_findnamedbbyname (this_cell);
	
	if (db_server == NULL)
	    arla_errx (1, ADEBERROR,
		       "no db server for cell %s", this_cell);
	realm = krb_realmofhost (db_server);
	
	ret = get_cred("afs", this_cell, realm, &krbdata.c);
	if (ret)
	    ret = get_cred("afs", "", realm, &krbdata.c);
	
	if (ret) {
	    arla_warnx (ADEBWARN,
			"getting ticket for %s: %s",
			this_cell,
			krb_get_err_text (ret));
	} else if (cred_add_krb4(getuid(), getuid(), &krbdata.c) == NULL) {
	    arla_warnx (ADEBWARN, "Could not insert tokens to arla");
	}
    }
#endif
    
    ce = cred_get (cell_name2num(cell_getthiscell()), getuid(), CRED_ANY);
    
    xfs_message_init ();
    kernel_opendevice ("null");
    
    arla_warnx (ADEBINIT, "Getting root...");
    error = getroot (&rootcwd, ce);
    if (error)
	    arla_err (1, ADEBERROR, error, "getroot");
    cwd = rootcwd;
    arla_warnx(ADEBINIT, "arla loop started");
    sl_loop(cmds, "arla> ");
    store_state();
}

char *
get_default_cache_dir (void)
{
    static char cache_path[MAXPATHLEN];
    char *home;

    home = getenv("HOME");
    if (home == NULL)
	home = "/var/tmp";

    snprintf (cache_path, sizeof(cache_path), "%s/.arla-cache",
	      home);
    return cache_path;
}

static struct getargs args[] = {
    {"conffile", 'c',	arg_string,	&conf_file,
     "path to configuration file", "file"},
    {"check-consistency", 'C', arg_flag, &cm_consistency,
     "if we want extra paranoid consistency checks", NULL },
    {"log",	'l',	arg_string,	&log_file,
     "where to write log (stderr (default), syslog, or path to file)", NULL},
    {"debug",	0,	arg_string,	&debug_levels,
     "what to write in the log", NULL},
    {"connected-mode", 0, arg_string,	&connected_mode_string,
     "initial connected mode [conncted|fetch-only|disconnected]", NULL},
    {"dynroot", 'D', arg_flag,	&dynroot_enable,
     "if dynroot is enabled", NULL},
#ifdef KERBEROS
    {"rxkad-level", 'r', arg_string,	&rxkad_level_string,
     "the rxkad level to use (clear, auth or crypt)", NULL},
#endif
    {"sysname",	 's',	arg_string,	&temp_sysname,
     "set the sysname of this system", NULL},
    {"root-volume",0,   arg_string,     &root_volume},
    {"port",	0,	arg_integer,	&client_port,
     "port number to use",	"number"},
    {"recover",	'z',	arg_negative_flag, &recover,
     "don't recover state",	NULL},
    {"cache-dir", 0,	arg_string,	&cache_dir,
     "cache directory",	"directory"},
    {"workers",	  0,	arg_integer,	&num_workers,
     "number of worker threads", NULL},
    {"fake-mp",	  0,	arg_flag,	&fake_mp,
     "enable fake mountpoints", NULL},
    {"version",	0,	arg_flag,	&version_flag,
     NULL, NULL},
    {"help",	0,	arg_flag,	&help_flag,
     NULL, NULL}
};

static void
usage (int ret)
{
    arg_printusage (args, sizeof(args)/sizeof(*args), NULL, "[device]");
    exit (ret);
}

int
main (int argc, char **argv)
{
    int optind = 0;
    int ret;

    set_progname (argv[0]);

    if (getarg (args, sizeof(args)/sizeof(*args), argc, argv, &optind))
	usage (1);

    argc -= optind;
    argv += optind;

    if (help_flag)
	usage (0);

    if (version_flag)
	errx (1, "%s", arla_version);
    
    if (argc != 0)
	usage (1);

    default_log_file = "/dev/stderr";

    ret = arla_init(argc, argv);
    if (ret)
	return ret;

    arla_start (NULL, cache_dir);
    
    return 0;
}
