/*
 * Copyright (c) 1995 - 2002 Kungliga Tekniska Högskolan
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
#include <vers.h>

RCSID("$arla: arla-cli.c,v 1.35 2003/06/10 04:23:16 lha Exp $");

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
static int arla_sleep(int, char **);
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
static int arla_mkdir (int, char**);
static int arla_rmdir (int, char**);
static int arla_rm (int, char**);
static int arla_put (int, char**);
static int arla_get (int, char**);
#ifdef RXDEBUG
static int arla_rx_status(int argc, char **argv);
#endif
static int arla_flushfid(int argc, char **argv);

static char *copy_dirname(const char *s);
static char *copy_basename(const char *s);


static SL_cmd cmds[] = {
    {"chdir", arla_chdir, "chdir directory"},
    {"cd"},
    {"ls",    arla_ls, "ls"},
    {"cat",   arla_cat, "cat file"},
    {"cp",    arla_cp, "copy file"},
    {"sleep", arla_sleep, "sleep seconds"},
    {"wc",    arla_wc, "wc file"},
    {"mkdir", arla_mkdir, "mkdir dir"},
    {"rmdir", arla_rmdir, "rmdir dir"},
    {"rm",    arla_rm, "rm file"},
    {"put",   arla_put, "put localfile [afsfile]"},
    {"get",   arla_get, "get afsfile [localfile]"},
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

/*
 * Return a malloced copy of the dirname of `s'
 */

static char *
copy_dirname (const char *s)
{
    const char *p;
    char *res;

    p = strrchr (s, '/');
    if (p == NULL)
	return strdup(".");
    res = malloc (p - s + 1);
    if (res == NULL)
	return NULL;
    memmove (res, s, p - s);
    res[p - s] = '\0';
    return res;
}

/*
 * Return the basename of `s'.
 * The result is malloc'ed.
 */

static char *
copy_basename (const char *s)
{
     const char *p, *q;
     char *res;

     p = strrchr (s, '/');
     if (p == NULL)
	  p = s;
     else
	  ++p;
     q = s + strlen (s);
     res = malloc (q - p + 1);
     if (res == NULL)
	 return NULL;
     memmove (res, p, q - p);
     res[q - p] = '\0';
     return res;
}

/*
 *
 */

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

    if(cm_walk (cwd, argv[1], &cwd))
	printf ("walk %s failed\n", argv[1]);
    return 0;
}

static int
print_dir (VenusFid *fid, const char *name, void *v)
{
     printf("(%d, %d, %d, %d): %s\n", fid->Cell,
	    fid->fid.Volume,
	    fid->fid.Vnode,
	    fid->fid.Unique, name);
     return 0;
}

struct ls_context {
    VenusFid *dir_fid;
    CredCacheEntry *ce;
};

static int
print_dir_long (VenusFid *fid, const char *name, void *v)
{
    int ret;
    AccessEntry *ae;
    struct ls_context *context = (struct ls_context *)v;
    char type;
    CredCacheEntry *ce = context->ce;
    FCacheEntry *entry;
    VenusFid *dir_fid  = context->dir_fid;
    char timestr[20];
    struct tm *t;
    time_t ti;

    if (VenusFid_cmp(fid, dir_fid) == 0)
	return 0;

    ret = followmountpoint (fid, dir_fid, NULL, &ce);
    if (ret) {
	printf ("follow %s: %d\n", name, ret);
	return 0;
    }

    /* Have we follow a mountpoint to ourself ? */
    if (VenusFid_cmp(fid, dir_fid) == 0)
	return 0;

    ret = fcache_get(&entry, *fid, context->ce);
    if (ret) {
	printf("%s: %d\n", name, ret);
	return 0;
    }

    ret = cm_getattr (entry, context->ce, &ae);
    if (ret) {
	fcache_release(entry);
	printf ("%s: %d\n", name, ret);
	return 0;
    }

    switch (entry->status.FileType) {
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

    ti = entry->status.ClientModTime;
    t = localtime (&ti);
    strftime (timestr, sizeof(timestr), "%Y-%m-%d", t);
    printf ("%c%c%c%c%c%c%c%c%c%c %2d %6d %6d %8d %s ",
	    type,
	    entry->status.UnixModeBits & 0x100 ? 'r' : '-',
	    entry->status.UnixModeBits & 0x080 ? 'w' : '-',
	    entry->status.UnixModeBits & 0x040 ? 'x' : '-',
	    entry->status.UnixModeBits & 0x020 ? 'r' : '-',
	    entry->status.UnixModeBits & 0x010 ? 'w' : '-',
	    entry->status.UnixModeBits & 0x008 ? 'x' : '-',
	    entry->status.UnixModeBits & 0x004 ? 'r' : '-',
	    entry->status.UnixModeBits & 0x002 ? 'w' : '-',
	    entry->status.UnixModeBits & 0x001 ? 'x' : '-',
	    entry->status.LinkCount,
	    entry->status.Owner,
	    entry->status.Group,
	    entry->status.Length,
	    timestr);

    printf ("v %d ", entry->status.DataVersion);

    printf ("%s\n", name);
    fcache_release(entry);
    return 0;
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
    FCacheEntry *entry;

    args[0].value = &l_flag;

    if (getarg (args, sizeof(args)/sizeof(*args),  argc, argv, &optind)) {
	arg_printusage (args, sizeof(args)/sizeof(*args), "ls", NULL);
	return 0;
    }
    context.dir_fid = &cwd;
    context.ce      = ce;
    error = fcache_get(&entry, cwd, ce);
    if (error) {
	printf ("fcache_get failed: %s\n", koerr_gettext(error));
	return 0;
    }

    error = adir_readdir (&entry, l_flag ? print_dir_long : print_dir,
			  &context, &ce);
    fcache_release(entry);
    if (error) {
	printf ("adir_readdir failed: %s\n", koerr_gettext(error));
	return 0;
    }
    cwd = entry->fid;
    return 0;
}

static int
arla_sysname (int argc, char **argv)
{
    switch (argc) {
    case 1:
	printf("sysname: %s\n", fcache_getdefsysname());
	break;
    case 2:
	fcache_setdefsysname(argv[1]);
	printf("setting sysname to: %s\n", fcache_getdefsysname());
	break;
    default:
	printf("syntax: sysname <sysname>\n");
	break;
    }
    return 0;
}

static int
arla_mkdir (int argc, char **argv)
{
    VenusFid fid;
    int ret;
    FCacheEntry *e;
    char *dirname;
    char *basename;
    AFSStoreStatus store_attr;
    VenusFid res;
    AFSFetchStatus fetch_attr;
    
    if (argc != 2) {
	printf ("usage: %s file\n", argv[0]);
	return 0;
    }
    dirname = strdup(argv[1]);
    if (dirname == NULL)
	err(1, "strdup");
    basename = strrchr(dirname, '/');
    if (basename == NULL) {
	printf ("%s: filename contains no /\n", argv[0]);
	free(dirname);
	return 0;
    }
    basename[0] = '\0';
    basename++;

    if(cm_walk (cwd, dirname, &fid) == 0) {

	ret = fcache_get(&e, fid, ce);
	if (ret) {
	    printf ("fcache_get failed: %d\n", ret);
	    free(dirname);
	    return 0;
	}

	store_attr.Mask = 0;
	store_attr.ClientModTime = 0;
	store_attr.Owner = 0;
	store_attr.Group = 0;
	store_attr.UnixModeBits = 0;
	store_attr.SegSize = 0;
	ret = cm_mkdir(&e, basename, &store_attr, &res, &fetch_attr, &ce);
	if (ret) {
	    arla_warn (ADEBWARN, ret,
		       "%s: cannot create directory `%s'",
		       argv[0], argv[1]);
	    fcache_release(e);
	    free(dirname);
	    return 1;
	}

	fcache_release(e);
    }
    free(dirname);
    return 0;
}

static int
arla_rmdir (int argc, char **argv)
{
    VenusFid fid;
    int ret;
    FCacheEntry *e;
    char *dirname;
    char *basename;
    
    if (argc != 2) {
	printf ("usage: %s file\n", argv[0]);
	return 0;
    }
    dirname = strdup(argv[1]);
    if (dirname == NULL)
	err(1, "strdup");
    basename = strrchr(dirname, '/');
    if (basename == NULL) {
	printf ("%s: filename contains no /\n", argv[0]);
	free(dirname);
	return 0;
    }
    basename[0] = '\0';
    basename++;

    if(cm_walk (cwd, dirname, &fid) == 0) {

	ret = fcache_get(&e, fid, ce);
	if (ret) {
	    printf ("fcache_get failed: %d\n", ret);
	    free(dirname);
	    return 0;
	}

	ret = cm_rmdir(&e, basename, &ce);
	if (ret) {
	    arla_warn (ADEBWARN, ret,
		       "%s: cannot remove directory `%s'",
		       argv[0], argv[1]);
	    fcache_release(e);
	    free(dirname);
	    return 1;
	}

	fcache_release(e);
    }
    free(dirname);
    return 0;
}

static int
arla_rm (int argc, char **argv)
{
    VenusFid fid;
    int ret;
    FCacheEntry *e;
    char *dirname;
    char *basename;
    
    if (argc != 2) {
	printf ("usage: %s file\n", argv[0]);
	return 0;
    }
    dirname = copy_dirname(argv[1]);
    if (dirname == NULL)
	err(1, "copy_dirname");
    basename = copy_basename(argv[1]);
    if (basename == NULL)
	err(1, "copy_basename");

    if(cm_walk (cwd, dirname, &fid) == 0) {

	ret = fcache_get(&e, fid, ce);
	if (ret) {
	    printf ("fcache_get failed: %d\n", ret);
	    free(dirname);
	    free(basename);
	    return 0;
	}

	ret = cm_remove(&e, basename, &ce);
	if (ret) {
	    arla_warn (ADEBWARN, ret,
		       "%s: cannot remove file `%s'",
		       argv[0], argv[1]);
	    fcache_release(e);
	    free(dirname);
	    free(basename);
	    return 1;
	}

	fcache_release(e);
    }
    free(dirname);
    free(basename);
    return 0;
}

static int
arla_put (int argc, char **argv)
{
    VenusFid dirfid;
    VenusFid fid;
    int ret;
    FCacheEntry *e;
    char *localname;
    char *localbasename;
    char *afsname;
    char *afsbasename;
    char *afsdirname;
    AFSStoreStatus store_attr;
    AFSFetchStatus fetch_attr;
    int afs_fd;
    int local_fd;
    char buf[8192];
    int write_ret;
    CredCacheEntry *ce;
    
    if (argc != 2 && argc != 3) {
	printf ("usage: %s localfile [afsfile]\n", argv[0]);
	return 0;
    }

    localname = argv[1];

    localbasename = copy_basename(localname);
    if (localbasename == NULL)
	err(1, "copy_basename");

    if (argc == 3) {
	afsname = argv[2];
    } else {
	afsname = localbasename;
    }

    afsdirname = copy_dirname(afsname);
    if (afsdirname == NULL)
	err(1, "copy_dirname");
    afsbasename = copy_basename(afsname);
    if (afsbasename == NULL)
	err(1, "copy_basename");


    printf("localbasename: *%s* afsname: *%s* afsdirname: *%s* afsbasename: *%s*\n",
	   localbasename, afsname, afsdirname, afsbasename);

    local_fd = open (localname, O_RDONLY, 0);

    if (local_fd < 0) {
	printf ("open %s: %s\n", localname, strerror(errno));
	ret = 0;
	goto out;
    }

    if(cm_walk (cwd, afsdirname, &dirfid))
	goto out;

    ce = cred_get (dirfid.Cell, getuid(), CRED_ANY);

    ret = fcache_get(&e, dirfid, ce);
    if (ret) {
	printf ("fcache_get failed: %d\n", ret);
	ret = 1;
	goto out;
    }

    memset(&store_attr, 0, sizeof(store_attr));

    ret = cm_create(&e, afsbasename, &store_attr, &fid, &fetch_attr, &ce);
    if (ret) {
	if (ret != EEXIST) {
	    arla_warn (ADEBWARN, ret,
		       "%s: cannot create file `%s'",
		       argv[0], afsname);
	    fcache_release(e);
	    ret = 1;
	    goto out;
	} else {
	    ret = cm_lookup (&e, afsbasename, &fid, &ce, 1);
	    if (ret) {
		arla_warn (ADEBWARN, ret,
			   "%s: cannot open file `%s'",
			   argv[0], afsname);
		fcache_release(e);
		ret = 1;
		goto out;
	    }
	}
    }
    
    fcache_release(e);

    ret = fcache_get(&e, fid, ce);
    if (ret) {
	printf ("fcache_get failed: %d\n", ret);
	ret = 1;
	goto out;
    }

    ret = fcache_get_data (&e, &ce, 0);
    if (ret) {
	fcache_release(e);
	printf ("fcache_get_data failed: %d\n", ret);
	ret = 1;
	goto out;
    }
    
    afs_fd = fcache_open_file (e, O_WRONLY);

    if (afs_fd < 0) {
	fcache_release(e);
	printf ("fcache_open_file failed: %d\n", errno);
	ret = 0;
	goto out;
    }

    ret = ftruncate(afs_fd, 0);
    if (ret) {
	fcache_release(e);
	printf ("ftruncate failed: %d\n", errno);
    }
    
    while ((ret = read (local_fd, buf, sizeof(buf))) > 0) {
	write_ret = write (afs_fd, buf, ret);
	if (write_ret < 0) {
	    printf("write failed: %d\n", errno);
	    ret = 1;
	    goto out;
	} else if (write_ret != ret) {
	    printf("short write: %d should be %d\n", write_ret, ret);
	    ret = 1;
	    goto out;
	}
    }

    close(afs_fd);
    close(local_fd);

    memset(&store_attr, 0, sizeof(store_attr));

    ret = cm_close (e, NNPFS_WRITE, &store_attr, ce);
    if (ret) {
	arla_warn (ADEBWARN, ret,
		   "%s: cannot close file `%s'",
		   argv[0], afsname);
	fcache_release(e);
	ret = 1;
	goto out;
    }

    fcache_release(e);

 out:
    free(localbasename);
    free(afsdirname);
    free(afsbasename);
    return 0;
}

static int
arla_get (int argc, char **argv)
{
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
    if(cm_walk (cwd, argv[1], &fid) == 0) {

	ret = fcache_get(&e, fid, ce);
	if (ret) {
	    printf ("fcache_get failed: %d\n", ret);
	    return 0;
	}

	ret = fcache_get_data (&e, &ce, 0);
	if (ret) {
	    fcache_release(e);
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
arla_sleep(int argc, char **argv)
{
    struct timeval tv;

    if (argc != 2) {
	printf ("usage: %s <time>\n", argv[0]);
	return 0;
    }

    tv.tv_sec = atoi(argv[1]);
    tv.tv_usec = 0;
    IOMGR_Select(0, NULL, NULL, NULL, &tv);

    return 0;
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
    uint32_t hosts[12];
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


#ifdef HAVE_KRB4

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

#endif /* HAVE_KRB4 */


static void
arla_start (char *device_file, const char *cache_dir, int argc, char **argv)
{
    CredCacheEntry *ce;
    int error;

#ifdef HAVE_KRB4
    {
	struct cred_rxkad cred;
	CREDENTIALS c;
	int ret;
	char *realm;
	const char *this_cell = cell_getthiscell ();
	const char *db_server = cell_findnamedbbyname (this_cell);
	
	if (db_server == NULL)
	    arla_errx (1, ADEBERROR,
		       "no db server for cell %s", this_cell);
	realm = krb_realmofhost (db_server);
	
	ret = get_cred("afs", this_cell, realm, &c);
	if (ret)
	    ret = get_cred("afs", "", realm, &c);
	
	if (ret) {
	    arla_warnx (ADEBWARN,
			"getting ticket for %s: %s",
			this_cell,
			krb_get_err_text (ret));
	    return;
	} 
	
	memset(&cred, 0, sizeof(cred));

	memcpy(&cred.ct.HandShakeKey, c.session, sizeof(cred.ct.AuthHandle));
	cred.ct.AuthHandle = c.kvno;
	cred.ct.ViceId = getuid();
	cred.ct.BeginTimestamp = c.issue_date + 1;
	cred.ct.EndTimestamp = krb_life_to_time(c.issue_date, c.lifetime);
	
	cred.ticket_len = c.ticket_st.length;
	if (cred.ticket_len > sizeof(cred.ticket))
	    arla_errx (1, ADEBERROR, "ticket too large");
	memcpy(cred.ticket, c.ticket_st.dat, cred.ticket_len);

	cred_add (getuid(), CRED_KRB4, 2, cell_name2num(cell_getthiscell()), 
		  2, &cred, sizeof(cred), getuid());
	
    }
#endif /* HAVE_KRB4 */
    
    ce = cred_get (cell_name2num(cell_getthiscell()), getuid(), CRED_ANY);

    assert (ce != NULL);
    
    nnpfs_message_init ();
    kernel_opendevice ("null");
    
    arla_warnx (ADEBINIT, "Getting root...");
    error = getroot (&rootcwd, ce);
    if (error)
	    arla_err (1, ADEBERROR, error, "getroot");
    cwd = rootcwd;
    arla_warnx(ADEBINIT, "arla loop started");
    error = 0;
    if (argc > 0) {
	error = sl_command(cmds, argc, argv);
	if (error == -1)
	    errx(1, "%s: Unknown command", argv[0]); 
    } else {
	sl_loop(cmds, "arla> ");
    }
    store_state();
    fcache_giveup_all_callbacks();
    if (error)
	exit(1);
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
    {"sysname",	 's',	arg_string,	&argv_sysname,
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
    arg_printusage (args, sizeof(args)/sizeof(*args), NULL, "[command]");
    exit (ret);
}

int
main (int argc, char **argv)
{
    int optind = 0;
    int ret;

    set_progname (argv[0]);
    tzset();
    srand(time(NULL));

    if (getarg (args, sizeof(args)/sizeof(*args), argc, argv, &optind))
	usage (1);

    argc -= optind;
    argv += optind;

    if (help_flag)
	usage (0);

    if (version_flag) {
	print_version (NULL);
	exit (0);
    }
    
    default_log_file = "/dev/stderr";

    ret = arla_init();
    if (ret)
	return ret;

    {
	struct timeval tv = { 0, 10000} ;
	IOMGR_Select(0, NULL, NULL, NULL, &tv);
    }

    arla_start (NULL, cache_dir, argc, argv);
    
    return 0;
}
