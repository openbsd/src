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


#include <sl.h>
#include "appl_locl.h"
#include <kafs.h>
#include <parse_units.h>
#include <xfs/xfs_debug.h>
#include <xfs/xfs_deb.h>
#include "fs_local.h"
#include <arladeb.h>

RCSID("$KTH: fs.c,v 1.92.2.4 2001/10/02 16:12:37 jimmy Exp $");

static int empty_cmd (int argc, char **argv);
static int apropos_cmd (int argc, char **argv);
static int arladebug_cmd (int argc, char **argv);
static int calculate_cmd (int argc, char **argv);
static int checkservers_cmd (int argc, char **argv);
static int checkvolumes_cmd (int argc, char **argv);
static int copyacl_cmd (int argc, char **argv);
static int diskfree_cmd (int argc, char **argv);
static int examine_cmd (int argc, char **argv);
static int flush_cmd (int argc, char **argv);
static int flushvolume_cmd (int argc, char **argv);
static int gc_cmd (int argc, char **argv);
static int getcache_cmd (int argc, char **argv);
static int getcrypt_cmd (int argc, char **argv);
static int getcellstatus_cmd (int argc, char **argv);
static int getfid_cmd (int argc, char **argv);
static int getstatistics_cmd (int argc, char **argv);
static int getprio_cmd (int argc, char **argv);
static int getmaxprio_cmd (int argc, char **argv);
static int help_cmd (int argc, char **argv);
static int invalidate_cmd (int argc, char **argv);
static int incompat_cmd (int argc, char **argv);
static int listacl_cmd (int argc, char **argv);
static int listcells_cmd (int argc, char **argv);
static int suidcells_cmd (int argc, char **argv);
static int listquota_cmd (int argc, char **argv);
static int lsmount_cmd (int argc, char **argv);
static int mkmount_cmd (int argc, char **argv);
static int connect_cmd (int argc, char **argv);
static int newcell_cmd (int argc, char **argv);
static int nop_cmd (int argc, char **argv);
static int quit_cmd (int argc, char **argv);
static int quota_cmd (int argc, char **argv);
static int rmmount_cmd (int argc, char **argv);
static int rmprio_cmd (int argc, char **argv);
static int setacl_cmd (int argc, char **argv);
static int setcache_cmd (int argc, char **argv);
static int setprio_cmd (int argc, char **argv);
static int setmaxprio_cmd (int argc, char **argv);
static int setquota_cmd (int argc, char **argv);
static int setcrypt_cmd (int argc, char **argv);
static int strerror_cmd (int argc, char **argv);
static int sysname_cmd (int argc, char **argv);
static int fsversion_cmd (int argc, char **argv);
static int venuslog_cmd (int argc, char **argv);
static int whereis_cmd (int argc, char **argv);
static int whichcell_cmd (int argc, char **argv);
static int wscell_cmd (int argc, char **argv);
static int xfsdebug_cmd (int argc, char **argv);
static int xfsdebug_print_cmd (int argc, char **argv);

static SL_cmd cmds[] = {
    {"apropos",        apropos_cmd,	"locate commands by keyword"},
    {"arladebug",      arladebug_cmd,	"tweek arla-debugging flags"},
    {"calculate cache", calculate_cmd,  "calculate the usege of cache"},
    {"checkservers",   checkservers_cmd,"check if servers is up"},
    {"checkvolumes",   checkvolumes_cmd, "lookup mappings between volume-Id's and names"},
    {"cleanacl",       empty_cmd,	"clear out numeric acl-entries"},
    {"copyacl",        copyacl_cmd,	"copy acl"},
    {"diskfree",       diskfree_cmd,	"show free partition space"},
    {"df"},
    {"examine",        examine_cmd,	"examine volume status"},
    {"flush",          flush_cmd,	"remove file from cache"},
    {"flushvolume",    flushvolume_cmd,	"remove volumedata (and files in volume) from cache"},
    {"gcpags",         gc_cmd,          "garbage collect pags"},
    {"getcacheparms",  getcache_cmd,	"get cache usage"},
    {"getcrypt",       getcrypt_cmd,	"get encrypt status"},
    {"getcellstatus",  getcellstatus_cmd, "get suid cell status"},
    {"getfid",         getfid_cmd,      "get fid"},
    {"getserverprefs", empty_cmd,	"show server rank"},
    {"getstatistics",  getstatistics_cmd, "get statistics"},
    {"getpriority",    getprio_cmd,     "get priority of a file/dir"},
    {"gp"},
    {"getmaxpriority", getmaxprio_cmd,  "get max priority for file gc"},
    {"gmp"},
    {"help",           help_cmd,	"help for commands"},
    {"incompat",       incompat_cmd,	"using old interface"},
    {"invalidate",     invalidate_cmd,	"invalidate (callback) path"},
    {"listacl",        listacl_cmd,	"show acl"},
    {"la"},
    {"listcells",      listcells_cmd,	"show cells configured"},
    {"listquota",      listquota_cmd,	"show volume quota"},
    {"lq"},
    {"lsmount",        lsmount_cmd,	"show mount point"},
    {"messages",       empty_cmd,	"change arlad logging"},
    {"mkmount",        mkmount_cmd,	"create mount point"},
    {"connect",        connect_cmd,     "connect mode"},
    {"monitor",        empty_cmd,	"set remote logging host"},
    {"newcell",        newcell_cmd,	"add new cell"},
    {"nop",            nop_cmd,         "do a pioctl-nop"},
    {"quit",           quit_cmd,	"leave interactive mode"},
    {"exit"},
    {"quota",          quota_cmd,	"show quota"},
    {"rmmount",        rmmount_cmd,	"delete mount point"},
    {"removepriority", rmprio_cmd,      "remove priority from file/directory"},
    {"rmp"},
    {"setacl",         setacl_cmd,	"set acl"},
    {"sa"},
    {"setcachesize",   setcache_cmd,	"change cache size"},
    {"setcell",        empty_cmd,	"change cell status"},
    {"setpriority",    setprio_cmd,     "set priority of a file/directory"},
    {"sp"},
    {"setmaxpriority", setmaxprio_cmd,  "set upper limit of prio gc"},
    {"smq"}, 
    {"setquota",       setquota_cmd,	"change maxquota on volume"},
    {"sq"},
    {"setserverprefs", empty_cmd,	"change server query order"},
    {"setcrypt",       setcrypt_cmd,	"set encryption on/off"},
    {"setvol",         empty_cmd,	"change status of volume"},
/*  {"storebehind",    empty_cmd,	""}, */
    {"strerror",       strerror_cmd,    "expand what errorcode this might be"},
    {"suidcells",      suidcells_cmd,	"list status of cells"},
    {"sysname",        sysname_cmd,	"read/change sysname"},
    {"version",        fsversion_cmd,   "get version of fs and fs_lib"},
    {"venuslog",       venuslog_cmd,    "make arlad print status"},
    {"whereis",        whereis_cmd,     "show server(s) of file"},
    {"whichcell",      whichcell_cmd,   "show cell of file"},
    {"wscell",         wscell_cmd,      "display cell of workstation"},
    {"xfsdebug",       xfsdebug_cmd,    "tweek xfs-debugging flags"},
    {"xfsprint",       xfsdebug_print_cmd,"make xfs print debug info"},
    {NULL}
};

int
main(int argc, char **argv)
{
  int ret = 0;

  if(!k_hasafs()) {
    printf ("Error detecting AFS\n");
    exit(1);
  }

  if (argc > 1) {
    ret = sl_command(cmds, argc - 1, argv + 1);
    if (ret == SL_BADCOMMAND)
      printf ("%s: Unknown command\n", argv[1]); 
  }
  else {
    fs_interactive = 1;
    printf("fs - an arla tool for administrating AFS files.\n");
    printf("Type \"help\" to get a list of commands.\n");
    ret = sl_loop(cmds, __progname": ");  
  }

  return ret;
}


static int
nop_cmd(int argc, char **argv)
{
    if (argc > 1)
	fprintf(stderr, "extraneous arguments ignored\n");

    printf("VIOCNOP returns %d\n", fs_nop());

    return 0;
}

static int
checkservers_cmd (int argc, char **argv)
{
    char *cell = NULL;
    int flags = 0;
    int nopoll = 0;
    int optind = 0;
    u_int32_t hosts[CKSERV_MAXSERVERS + 1];
    int ret;
    int i;

    struct agetargs cksargs[] = {
	{"cell",	0, aarg_string,  NULL, "cell", NULL},
	{"nopoll",	0, aarg_flag,    NULL, "dont ping each server, "
	                                       "use internal info", NULL},
	{NULL,      0, aarg_end, NULL}}, 
					 *arg;

    arg = cksargs;
    arg->value = &cell;   arg++;
    arg->value = &nopoll;   arg++;

    if (agetarg (cksargs, argc, argv, &optind, AARG_AFSSTYLE)) {
	aarg_printusage(cksargs, "checkservers", NULL, AARG_AFSSTYLE);
	return 0;
    }

    if (cell)
	flags |= CKSERV_FSONLY;
    if (nopoll)
	flags |= CKSERV_DONTPING;
    
    ret = fs_checkservers(cell, flags, hosts, sizeof(hosts)/sizeof(hosts[0]));
    if (ret) {
	if (ret == ENOENT)
	    fprintf (stderr, "%s: cell `%s' doesn't exist\n",
		     PROGNAME, cell);
	else
	    fserr (PROGNAME, ret, NULL);
	return 0;
    }

    if (hosts[0] == 0)
	printf ("All servers are up");

    for (i = 1; i < min(CKSERV_MAXSERVERS, hosts[0]) + 1; ++i) {
	if (hosts[i]) {
	    struct hostent *he;

	    he = gethostbyaddr ((char *)&hosts[i], sizeof(hosts[i]), AF_INET);

	    if (he != NULL) {
		printf ("%s ", he->h_name);
	    } else {
		struct in_addr in;

		in.s_addr = hosts[i];
		printf ("%s ", inet_ntoa(in));

	    }
	}
    }
    printf("\n");
    return 0;
}

static int 
checkvolumes_cmd (int argc, char **argv)
{
    int ret;

    ret = fs_checkvolumes();
    if (ret)
	fserr (PROGNAME, ret, NULL);
    return 0;
}

static int
connect_usage(void)
{
    printf("connect [connected|fetch|disconnected]\n");
    return 0;
}

static int
connect_cmd(int argc, char **argv)
{
    int ret;
    int32_t flags;

    argc--;
    argv++;

    if (argc == 0) {
	ret = fs_connect(CONNMODE_PROBE, &flags);
	if (ret) {
	    fserr(PROGNAME, ret, NULL);
	    return 0;
	}

	switch(flags) {
	case CONNMODE_CONN:
	    printf("Connected mode\n");
	    break;
	case CONNMODE_FETCH:
	    printf("Fetch only mode\n");
  	    break;
	case CONNMODE_DISCONN:
	    printf("Disconnected mode\n");
	    break;
	default:
	    printf("Unknown or error\n");
	    break;
	}
	return 0;
    }

    if (strncmp("dis", *argv, 3) == 0) 
	ret = fs_connect(CONNMODE_DISCONN, &flags);    
    else if (strncmp("fetch", *argv, 5) == 0)
        ret = fs_connect(CONNMODE_FETCH, &flags);
    else if (strncmp("conn", *argv, 4) == 0)
	ret = fs_connect(CONNMODE_CONN, &flags);
    else
	return connect_usage();

    if (ret)
	fserr(PROGNAME, ret, NULL);

    return 0;
}

static int
copyacl_cmd (int argc, char **argv)
{
    argc--;
    argv++;
    
    if (argc != 2) {
	printf ("fs: copyacl: Too many or too few arguments\n");
	return 0;
    }

    afs_copyacl (argv[0], argv[1]);

    return 0;
}

static int
diskfree_cmd (int argc, char **argv)
{
    int i;

    argc--;
    argv++;

    printf ("Volume Name           kbytes  used     avail     %%used\n");

    if (argc == 0)
	afs_diskfree (".");
    else
	for (i = 0; i < argc; i++)
	    afs_diskfree (argv[i]);

    return 0;
}

static int
empty_cmd (int argc, char **argv)
{
    printf ("%s%s not implemented yet!\n", PROGNAME, argv[0]);
    return 0;
}

static int
examine_cmd (int argc, char **argv)
{
    int   i;

    argc--;
    argv++;

    if (argc == 0)
	afs_examine (".");
    else
	for (i = 0; i < argc; i++)
	    afs_examine (argv[i]);

    return 0;
}

static int
flushvolume_cmd (int argc, char **argv)
{
    int i;

    argc--;
    argv++;

    if (argc == 0)
	fs_flushvolume (".");
    else
	for (i = 0; i < argc; i++)
	    fs_flushvolume (argv[i]);

    return 0;
}

static int
flush_cmd (int argc, char **argv)
{
    int   i;

    argc--;
    argv++;

    if (argc == 0)
	fs_flush (".");
    else
	for (i = 0; i < argc; i++)
	    fs_flush (argv[i]);

    return 0;
}

static int
gc_cmd (int argc, char **argv)
{
    int ret;

    argc--;
    argv++;

    if (argc != 0)
	printf ("gcpags: extraneous arguments ignored\n");
    
    ret = fs_gcpags();
    if (ret)
	fserr(PROGNAME, ret, NULL);

    return 0;
}

static int
getcellstatus_cmd (int argc, char **argv)
{
    int   i;

    argc--;
    argv++;

    if (argc == 0)
	printf ("%s: Missing required parameter '-cell'\n", PROGNAME);
    else
	for (i = 0; i < argc; i++)
	    afs_getcellstatus (argv[i]);

    return 0;
}

static int
getcrypt_cmd (int argc, char **argv)
{
    u_int32_t n;
    int ret;

    argc--;
    argv++;

    if (argc != 0)
	printf ("getcrypt: extraneous arguments ignored\n");

    ret = fs_getcrypt (&n);
    if (ret) {
	fserr(PROGNAME, ret, NULL);
	return 0;
    }

    switch (n) {
    case 0 :
	printf ("not encrypted\n");
	break;
    case 1 :
	printf ("encrypted\n");
	break;
    default :
	printf ("getcrypt: unknown reply %d\n", n);
    }
    return 0;
}

static int
setcrypt_cmd (int argc, char **argv)
{
    u_int32_t n;
    int ret ;

    --argc;
    ++argv;

    if (argc != 1) {
	printf ("setcrypt: Missing parameter on/off\n");
	return 0;
    }
    if (strcasecmp(argv[0], "on") == 0)
	n = 1;
    else if(strcasecmp(argv[0], "off") == 0)
	n = 0;
    else {
	printf ("setcrypt: Unknown parameter '%s'\n", argv[0]);
	return 0;
    }
    ret = fs_setcrypt (n);
    if (ret) 
	fserr(PROGNAME, ret, NULL);

    return 0;
}

static int
getfid_cmd(int argc, char **argv)
{
    argc--;
    argv++;

    if (argc == 0) 
	printf("%s: Missing required parameter '-path'\n", PROGNAME);
    else if (argc == 1)
	afs_getfid(*argv);
    else
	while (argc) {
	    afs_getfid(*argv);
	    argc--;
	    argv++;
	}
    return 0;
}

static
int setmaxprio_usage()
{
    fprintf(stderr, "usage: setmaxprio maxprio");
    return 0;
}

static int
setmaxprio_cmd (int argc, char **argv)
{
    int prio_tmp;
    int16_t maxprio;
    int ret;
    
    if (argc != 1) 
	return setmaxprio_usage();

    if (sscanf(argv[1], "%d", &prio_tmp) != 1) 
	return setmaxprio_usage();

    maxprio = prio_tmp;

    ret = fs_setmaxfprio(maxprio);
    if (ret) {
	fserr(PROGNAME, ret, argv[1]);
	return 0;
    }

    return 0;

}

static int
setprio_usage(void)
{
    fprintf(stderr, "usage: file prio\n");
    return 0;
}

static int
setprio_cmd (int argc, char **argv)
{
    VenusFid fid;
    int prio_tmp;
    int16_t prio;
    int ret;
    
    if (argc != 2) 
	return setprio_usage();

    if (sscanf(argv[2], "%d", &prio_tmp) != 1) 
	return setprio_usage();

    prio = prio_tmp;

    ret = fs_getfid(argv[1], &fid);
    if (ret) {
	fserr(PROGNAME, ret, argv[1]);
	return 0;
    }
	
    ret = fs_setfprio(fid, prio);
    if (ret) {
	fserr(PROGNAME, ret, argv[1]);
	return 0;
    }

    return 0;
}

static int
help_cmd (int argc, char **argv)
{
    SL_cmd *cmd;

    for (cmd = cmds; cmd->name != NULL; ++cmd)
        if (cmd->usage != NULL)
	    printf ("%-20s%s\n", cmd->name, cmd->usage);

    return 0;
}

static int
apropos_cmd (int argc, char **argv)
{
    if (argc == 0) {
	printf ("apropos: missing topic\n");
	return 0;
    }

    sl_apropos (cmds, argv[1]);
    return 0;
}

static int
mkmount_cmd (int argc, char **argv)
{
    char  buf[MAXSIZE];
    char prefix;
    char *dirname = NULL;
    char *volname = NULL;
    char *cell = NULL;
    int rwflag = 0;
    int helpflag = 0;
    int optind = 0;

    struct agetargs mkmargs[] = {
	{"dir",     0, aarg_string, NULL, "mount point directory name",
	 "directory", aarg_mandatory},
	{"vol",     0, aarg_string, NULL, "volume to mount",
	 "volume",    aarg_mandatory},
	{"cell",    0, aarg_string, NULL, "cell of volume",
	 "cell",      aarg_optional_swless},
	{"rw",      0, aarg_flag, NULL, "mount read-write", NULL},
	{"help",    0, aarg_flag, NULL, NULL, NULL},
	{NULL,      0, aarg_end,  NULL}}, *arg;
    
    arg = mkmargs;
    arg->value = &dirname; arg++;
    arg->value = &volname; arg++;
    arg->value = &cell; arg++;
    arg->value = &rwflag; arg++;
    arg->value = &helpflag; arg++;

    if (agetarg (mkmargs, argc, argv, &optind, AARG_AFSSTYLE)) {
	aarg_printusage(mkmargs, "fs mkmount", "", AARG_AFSSTYLE);
 	return 0;
    }

    argc -= optind;
    argv += optind;

    if (argc) {
	printf("unknown option %s\n", *argv);
	return 0;
    }

    if (rwflag)
	prefix = '%';
    else
	prefix = '#';
    if (cell)
	snprintf(buf, sizeof(buf), "%c%s:%s.", prefix, cell, volname);
    else
	snprintf(buf, sizeof(buf), "%c%s.", prefix, volname);

    if (symlink (buf, dirname) == -1) {
	perror ("fs");
	return 0;
    }

    return 0;
}

static int
incompat_cmd (int argc, char **argv)
{
    int status, ret;

    ret = fs_incompat_renumber(&status);
    if (ret) {
	fserr(PROGNAME, ret, NULL);
    } else {
	if (status)
	    printf ("new interface\n");
	else
	    printf ("old interface\n");
    }
    return 0;
}

static int
invalidate_cmd (int argc, char **argv)
{
    unsigned int i;

    argc--;
    argv++;
    
    if(!argc)
	fs_invalidate(".");
    else
	for(i=0;i<argc;i++) {
	    if(i)
		printf("\n");
	    fs_invalidate(argv[i]);
	}
    
    return 0;
}

static int
listacl_cmd (int argc, char **argv)
{
    unsigned int i;

    argc--;
    argv++;

    if(!argc)
      afs_listacl(".");
    else
      for(i=0;i<argc;i++) {
	if(i)
	  printf("\n");
	afs_listacl(argv[i]);
      }

    return 0;
}

static int
listcells_cmd (int argc, char **argv)
{
    int printhosts = 1;
    int resolve = 1;
    int printsuid = 0;
    int optind = 0;

    struct agetargs lcargs[] = {
	{"servers", 's', aarg_negative_flag,  
	 NULL,"do not print servers in cell", NULL},
	{"resolve",	 'r', aarg_negative_flag,   
	 NULL,"do not resolve hostnames", NULL},
        {"suid",	 'p', aarg_flag,   
	 NULL,"print if cell is suid", NULL },
        {NULL,      0, aarg_end, NULL}}, 
				  *arg;

    arg = lcargs;
    arg->value = &printhosts;   arg++;
    arg->value = &resolve;     arg++;
    arg->value = &printsuid;   arg++;

    if (agetarg (lcargs, argc, argv, &optind, AARG_AFSSTYLE)) {
	aarg_printusage(lcargs, "listcells", NULL, AARG_AFSSTYLE);
	return 0;
    }

    printf ("%d %d %d\n", printhosts, resolve, printsuid);
    
    afs_listcells (printhosts,resolve,printsuid);

    return 0;
}

static int
strerror_cmd (int argc, char **argv)
{
    if (argc == 0) {
	fprintf (stderr, "usage: fs strerror errno ...\n");
    } else {
	int error;
	argv++;

	while (--argc) {
	    if (sscanf (argv[0], "%d", &error) != 1) {
		fprintf (stderr, "warning: %s isn't a number\n", argv[0]);
	    } else {
		printf ("%d => %s\n", error, koerr_gettext(error));
	    }
	    argv++;
	}
    }
	
    return 0;
}

static int
suidcells_cmd (int argc, char **argv)
{
    afs_listcells (0, 0, 1);

    return 0;
}

static int
newcell_cmd (int argc, char **argv)
{
    char *cell = NULL;
    agetarg_strings servers = { 0, NULL };
    int ret, help = 0;
    int optind = 0;

    struct agetargs ncargs[] = {
	{"cell", 'c', aarg_string,  
	 NULL, "new cell", NULL, aarg_mandatory},
	{"servers", 's', aarg_strings,
	 NULL, "server in cell", "one server"},
	{"help", 'h', aarg_flag,
	 NULL, "get help", NULL},
        {NULL,      0, aarg_end, NULL}}, 
				  *arg;
			       
    arg = ncargs;
    arg->value = &cell; arg++;
    arg->value = &servers; arg++;
    arg->value = &help; arg++;

    if (agetarg (ncargs, argc, argv, &optind, AARG_AFSSTYLE)) {
	aarg_printusage(ncargs, "newcell", NULL, AARG_AFSSTYLE);
	return 0;
    }

    if (help) {
	aarg_printusage(ncargs, "newcell", NULL, AARG_AFSSTYLE);
	goto out;
    }

    if (servers.num_strings == 0) {
	fprintf (stderr, "You didn't give servers, will use DNS\n");
	goto out;
    }

    ret = fs_newcell (cell, servers.num_strings, servers.strings);
    if (ret)
	fprintf (stderr, "fs_newcell failed with %s (%d)\n",
		 koerr_gettext(ret), ret);
    
 out:
    if (servers.strings)
	free (servers.strings);

    return 0;
}

static int
listquota_cmd (int argc, char **argv)
{
    int   i;

    argc--;
    argv++;

    printf("Volume Name            Quota    Used    %% Used   Partition\n");

    if (argc == 0)
	afs_listquota (".");
    else
	for (i = 0; i < argc; i++)
	    afs_listquota (argv[i]);

    return 0;
}

static int
getprio_usage(void)
{
    fprintf(stderr, "usage getprio file ...\n");
    return 0;
}

static void
printtimeslot(u_int32_t slot)
{
    int v;

    v = 1 << slot;

    if (v < 1000) {
	printf("%3d us ", v);
	return;
    }
    if (v < 1000000) {
	printf("%3d ms ", v / 1000);
	return;
    }
    printf("%3d s  ", v / 1000000);
}

static void
printsizeslot(u_int32_t slot)
{
    int v;

    v = 1 << slot;

    if (v < 1024) {
	printf("%3d   ", v);
	return;
    }
    if (v < 1000000) {
	printf("%3d K ", v / 1000);
	return;
    }
    printf("%3d M ", v / 1000000);
}

static void
print_stat_header(char *server, char *part,
		  int type, int time, int size, int *once)
{
    if (*once)
	return;
    *once = 1;
    printf("%30s %10s\n", server, part);

    if (time)
	printf("%6s ", "tmrng");

    if (size)
	printf("%5s ", "szrng");
    
    printf("%6s ", "#req");

    printf("%8s ", "tot B");
    printf("%8s ", "tot us");
    printf("%8s ", "avg us");
    printf("%8s\n", "KB/s");
}

static void
print_statistics(int64_t count, int64_t items_total, int64_t total_time,
		 int timeslot, int sizeslot, int time, int size)
{
    if (time)
	printtimeslot(timeslot);
    if (size)
	printsizeslot(sizeslot);

    printf("%6lld %8lld %8lld %8lld %8lld\n",
	   count, items_total,
	   total_time,
	   total_time/count,
	   items_total*1000LL/total_time);
}

static int
getstatistics_cmd (int argc, char **argv)
{
    u_int32_t host[100];
    u_int32_t part[100];
    int n = 100;
    int i;
    int j;
    int k;
    char server_name[100];
    char partition_name[100];
    u_int32_t count[32];
    int64_t items_total[32];
    int64_t total_time[32];
    int64_t tot_total_time;
    int64_t tot_count;
    int64_t tot_items_total;
    int error;
    int type;
    char *reqtype = NULL;
    char *filter_host = NULL;
    int time = 0, size = 0, help = 0;
    int optind = 0;
    int printed_header = 0;

    struct agetargs statargs[] = {
	{"type", 0, aarg_string,  
	 NULL, "request type", "type", aarg_mandatory},
	{"host", 0, aarg_string,  
	 NULL, "host", "type", aarg_optional},
	{"time", 0, aarg_flag,
	 NULL, "time statistics", NULL},
	{"size", 0, aarg_flag,
	 NULL, "size statistics", NULL},
	{"help", 0, aarg_flag,
	 NULL, "get help", NULL},
        {NULL,      0, aarg_end, NULL}},
	*arg;

    arg = statargs;
    arg->value = &reqtype; arg++;
    arg->value = &filter_host; arg++;
    arg->value = &time; arg++;
    arg->value = &size; arg++;
    arg->value = &help; arg++;


    if (agetarg (statargs, argc, argv, &optind, AARG_AFSSTYLE)) {
	aarg_printusage(statargs, "getstatistics", NULL, AARG_AFSSTYLE);
	return 0;
    }

    if (help) {
	aarg_printusage(statargs, "getstatistics", NULL, AARG_AFSSTYLE);
	return 0;
    }


    if (strcmp(reqtype, "fetchstatus") == 0) {
	type = STATISTICS_REQTYPE_FETCHSTATUS;
    } else if (strcmp(reqtype, "fetchdata") == 0) {
	type = STATISTICS_REQTYPE_FETCHDATA;
    } else if (strcmp(reqtype, "bulkstatus") == 0) {
	type = STATISTICS_REQTYPE_BULKSTATUS;
    } else {
	fprintf(stderr, "type must be fetchstatus, fetchdata or bulkstatus\n");
	return 0;
    }

    error = fs_statistics_list(host, part, &n);
    if (error) {
	fserr(PROGNAME, error, NULL);
	return 0;
    }
    
    for (i = 0; i < n; i++) {
	printed_header = 0;
	arlalib_host_to_name(host[i], server_name, 100);
	if (filter_host && strcmp(filter_host, server_name) != 0)
	    continue;
	partition_num2name(part[i], partition_name, 100);

	tot_total_time = 0;
	tot_count = 0;
	tot_items_total = 0;
	for (j = 0; j < 32; j++) {
	    error = fs_statistics_entry(host[i], part[i], type,
					j, count, items_total, total_time);
	    if (error) {
		fserr(PROGNAME, error, NULL);
		return 0;
	    }
	    if (size) {
		tot_total_time = 0;
		tot_count = 0;
		tot_items_total = 0;
	    }
	    for (k = 0; k < 32; k++) {
		if (count[k]) {
		    if (time && size) {
			print_stat_header(server_name, partition_name,
					  type, time, size, &printed_header);
			print_statistics(count[k], items_total[k],
					 total_time[k], k, j,
					 time, size);
		    }
		    tot_total_time += total_time[k];
		    tot_count += count[k];
		    tot_items_total += items_total[k];
		}
	    }
	    if (!time && size && tot_count) {
		print_stat_header(server_name, partition_name,
				  type, time, size, &printed_header);
		print_statistics(tot_count, tot_items_total,
				 tot_total_time, 0, j,
				 time, size);
	    }
	}
	if (!time && !size && tot_count) {
	    print_stat_header(server_name, partition_name,
			      type, time, size, &printed_header);
	    print_statistics(tot_count, tot_items_total,
			     tot_total_time, 0, 0,
			     time, size);
	}
    }
    return 0;
}

static int
getprio_cmd (int argc, char **argv)
{
    VenusFid fid;
    int16_t prio;
    int ret, i;
    char *path;
    char cell[MAXNAME];
    
    if (argc == 0) 
	return getprio_usage();

    for (i = 0; i < argc; i++) {
	path = argv[i];

	ret = fs_getfid(path, &fid);
	if (ret) {
	    fserr(PROGNAME, ret, path);
	    continue;
	}

	ret = fs_getfprio(fid, &prio);
	if (ret) {
	    fserr(PROGNAME, ret, path);
	    continue;
	}

	ret = fs_getfilecellname(path, cell, sizeof(cell));
	if (ret) {
	    fserr(PROGNAME, ret, path);
	    continue;
	}

	printf("File %s(%s %d.%d.%d) have priority %d\n",
	       path, cell, fid.fid.Volume, fid.fid.Vnode,
	       fid.fid.Unique, prio);

    }
    return 0;
}

static int
getmaxprio_cmd (int argc, char **argv)
{
    int16_t prio;
    int ret;
    
    if (argc != 0) 
	fprintf(stderr, "getmaxprio: extraneous argumets ignored\n");

    ret = fs_getmaxfprio(&prio);
    if (ret) {
	fserr(PROGNAME, ret, NULL);
	return 0;
    }

    return 0;
}


static int
setquota_cmd (int argc, char **argv)
{
    char *path = NULL;
    int quota = 0;
    int helpflag = 0;
    int optind = 0;
    
    struct agetargs sqargs[] = {
	{"path", 0, aarg_string,  NULL,  "pathname to file/directory",
	 "pathname", aarg_mandatory},
	{"max",  0, aarg_integer, NULL, "max quota in kbytes",
	 "kbytes",   aarg_mandatory},
	{"help",    0, aarg_flag, NULL },
        {NULL,      0, aarg_end, NULL}}, *arg;

    arg = sqargs;
    arg->value = &path; arg++;
    arg->value = &quota; arg++;
    arg->value = &helpflag; arg++;

    if (agetarg (sqargs, argc, argv, &optind, AARG_AFSSTYLE)) {
	aarg_printusage(sqargs, "fs setquota", NULL, AARG_AFSSTYLE);
	return 0;
    }

    if (helpflag) {
	aarg_printusage(sqargs, "fs setquota", NULL, AARG_AFSSTYLE);
	return 0;
    }
    argc -= optind;
    argv += optind;

    if (argc) {
	printf("unknown option %s\n", *argv);
	return 0;
    }
 
    afs_setmaxquota(path, quota);
 
    return 0;
}


static int
lsmount_cmd (int argc, char **argv)
{
    int   i, ret;

    argc--;
    argv++;

    if (argc == 0) {
	printf ("fs: Required parameter '-dir' missing\n");
	return 0;
    }
    
    for (i = 0; i < argc; i++) {
	ret = fs_lsmount (argv[i]);
	if (ret == EINVAL)
	    fserr (PROGNAME, ret, argv[i]);
	else if (ret)
	    printf ("'%s' is not a mount point.\n", argv[i]);
    }

    return 0;
}

static void
setcache_usage()
{
    fprintf(stderr, "Usage: setcachesize <lowvnodes> [<highvnodes> "
	    "<lowbytes> <highbytes>]\n");

}

static int
setcache_cmd (int argc, char **argv)
{
    int lv, hv, lb, hb;

    argc--;
    argv++;

    if (argc != 1 && argc != 4) {
	setcache_usage();
	return 0;
    }
    
    if (argc == 4) {
	if (sscanf(argv[0], "%d", &lv) &&
	    sscanf(argv[1], "%d", &hv) &&
	    sscanf(argv[2], "%d", &lb) &&
	    sscanf(argv[3], "%d", &hb))
	    afs_setcache(lv, hv, lb, hb);
	else
	    setcache_usage();
    } else {
	if (sscanf(argv[0], "%d", &lv))
	    afs_setcache(lv, 0, 0, 0);
	else 
	    setcache_usage();
    }

    return 0;
}

static int
quit_cmd (int argc, char **argv)
{
    printf ("Exiting...\n");
    return 1;
}

static int
quota_cmd (int argc, char **argv)
{
    int   i;

    argc--;
    argv++;

    if (argc == 0)
	afs_quota (".");
    else
	for (i = 0; i < argc; i++)
	    afs_quota (argv[i]);

    return 0;
}

static int
rmmount_cmd (int argc, char **argv)
{
    int   i, ret;

    argc--;
    argv++;

    if (argc == 0) {
	printf ("fs: Required parameter '-dir' missing\n");
	return 0;
    }
    
    for (i = 0; i < argc; i++) {
	ret = fs_rmmount (argv[i]);
	if (ret == EINVAL)
	    fserr (PROGNAME, ret, argv[i]);
	else if (ret)
	    printf ("'%s' is not a mount point.\n", argv[i]);
    }
    return 0;
}

static int
rmprio_usage(void)
{
    printf("usage: rmpriority file ...\n");
    return 0;
}

static int
rmprio_cmd(int argc, char **argv)
{
    VenusFid fid;
    int ret, i;
    char *path;
    
    if (argc == 0) 
	return rmprio_usage();

    for (i = 0; i < argc; i++) {
	path = argv[i];

	ret = fs_getfid(path, &fid);
	if (ret) {
	    fserr(PROGNAME, ret, path);
	    return 0;
	}

	ret = fs_setfprio(fid, 0);
	if (ret) {
	    fserr(PROGNAME, ret, path);
	    return 0;
	}
    }
    return 0;
}


static int
setacl_cmd (int argc, char **argv)
{
    argc--;
    argv++;

    if (argc != 3) {
	printf ("fs: setacl: Too many or too few arguments\n");
	return 0;
    }

    afs_setacl (argv[0], argv[1], argv[2]);

    return 0;
}

static int
sysname_cmd (int argc, char **argv)
{
    argc--;
    argv++;
    if (argc == 0)
	afs_print_sysname ();
    else
	afs_set_sysname (argv[0]);

    return 0;
}

static int
whereis_cmd (int argc, char **argv)
{
    argc--;
    argv++;
    if (argc == 0)
	afs_whereis (".");
    else
	afs_whereis (argv[0]);

    return 0;
}

static int
whichcell_cmd (int argc, char **argv)
{
    int   i;

    argc--;
    argv++;

    if (argc == 0) {
	afs_whichcell (".");
    }

    for (i = 0; i < argc; i++)
	afs_whichcell (argv[i]);

    return 0;
}

static int
wscell_cmd (int argc, char **argv)
{
    argc--;
    argv++;

    afs_wscell ();

    return 0;
}

static int
venuslog_cmd (int argc, char **argv)
{
    int ret = fs_venuslog();
    if (ret)
	fserr (PROGNAME, ret, NULL);
    return 0;
}

static int
fsversion_cmd(int argc, char **argv)
{
    if (argc != 1)
	printf ("version: extraneous arguments ignored\n");

    printf("fs: %s\nfs_lib: %s\n",
	   "$KTH: fs.c,v 1.92.2.4 2001/10/02 16:12:37 jimmy Exp $",
	   fslib_version());
    return 0;
}

static const unsigned all = (0
#ifdef HAVE_XDEBDEV
			     | XDEBDEV
#endif
#ifdef HAVE_XDEBMSG
			     | XDEBMSG
#endif
#ifdef HAVE_XDEBDNLC
			     | XDEBDNLC
#endif
#ifdef HAVE_XDEBNODE
			     | XDEBNODE
#endif
#ifdef HAVE_XDEBVNOPS
			     | XDEBVNOPS
#endif
#ifdef HAVE_XDEBVFOPS
			     | XDEBVFOPS
#endif
#ifdef HAVE_XDEBLKM
			     | XDEBLKM
#endif
#ifdef HAVE_XDEBSYS
			     | XDEBSYS
#endif
#ifdef HAVE_XDEBMEM
			     | XDEBMEM
#endif
#ifdef HAVE_XDEBREADDIR
			     | XDEBREADDIR
#endif
#ifdef HAVE_XDEBLOCK
			     | XDEBLOCK
#endif
#ifdef HAVE_XDEBCACHE
			     | XDEBCACHE
#endif
    );

static const unsigned almost_all_mask = (0
#ifdef HAVE_XDEBDEV
					 | XDEBDEV
#endif
#ifdef HAVE_XDEBMEM
					 | XDEBMEM
#endif	       
#ifdef HAVE_XDEBREADDIR
					 | XDEBREADDIR
#endif
    );

static struct units xfsdebug_units[] = {
    {"all",		0},
    {"almost-all",  0},
#ifdef HAVE_XDEBCACHE
    {"cache",	XDEBCACHE},
#endif
#ifdef HAVE_XDEBLOCK
    {"lock",	XDEBLOCK},
#endif
#ifdef HAVE_XDEBREADDIR
    {"readdir",	XDEBREADDIR},
#endif
#ifdef HAVE_XDEBMEM
    {"mem",		XDEBMEM},
#endif
#ifdef HAVE_XDEBSYS
    {"sys",		XDEBSYS},
#endif
#ifdef HAVE_XDEBLKM
    {"lkm",		XDEBLKM},
#endif
#ifdef HAVE_XDEBVFOPS
    {"vfsops",	XDEBVFOPS},
#endif
#ifdef HAVE_XDEBVNOPS
    {"vnops",	XDEBVNOPS},
#endif
#ifdef HAVE_XDEBNODE
    {"node",	XDEBNODE},
#endif
#ifdef HAVE_XDEBDNLC
    {"dnlc",	XDEBDNLC},
#endif
#ifdef HAVE_XDEBMSG
    {"msg",		XDEBMSG},
#endif
#ifdef HAVE_XDEBDEV
    {"dev",		XDEBDEV},
#endif
    {"none",	0 },
    { NULL, 0 }
};

static int
xfsdebug_cmd (int argc, char **argv)
{
    int ret;
    int flags;

    xfsdebug_units[0].mult = all;
    xfsdebug_units[1].mult = all & ~almost_all_mask;

    if ((argc > 1 && strncmp(argv[1], "-h", 2) == 0) || argc > 2) {
	fprintf (stderr, "usage: xfsdebug [-h] [");
	print_flags_table (xfsdebug_units, stderr);
	fprintf (stderr, "]\n");
	return 0;
    }

    ret = xfs_debug (-1, &flags);
    if (ret) {
	fprintf (stderr, "xfs_debug: %s\n", strerror(ret));
	return 0;
    }

    if (argc == 1) {
	char buf[1024];

	unparse_flags (flags, xfsdebug_units, buf, sizeof(buf));
	printf("xfsdebug is: %s\n", buf);
    } else if (argc == 2) {
	char *textflags;
	
	textflags = argv[1];
    
	ret = parse_flags (textflags, xfsdebug_units, flags);
	
	if (ret < 0) {
	    fprintf (stderr, "xfsdebug: unknown/bad flags `%s'\n",
		     textflags);
	    return 0;
	}
	
	flags = ret;
	ret = xfs_debug(flags, NULL);
	if (ret)
	    fprintf (stderr, "xfs_debug: %s\n", strerror(ret));
    }
    return 0;
}

static int
xfsdebug_print_cmd (int argc, char **argv)
{
    int ret;
    int flags = 0;
    char *textflags;

    xfsdebug_units[0].mult = all;
    xfsdebug_units[1].mult = all & ~almost_all_mask;

    if (argc != 2) {
	fprintf (stderr, "usage: xfsprint <");
	print_flags_table (xfsdebug_units, stderr);
	fprintf (stderr, ">\n");
	return 0;
    }

    textflags = argv[1];
    
    ret = parse_flags (textflags, xfsdebug_units, flags);
    
    if (ret < 0) {
	fprintf (stderr, "xfsprint: unknown/bad flags `%s'\n",
		 textflags);
	return 0;
    }
    
    flags = ret;
    ret = xfs_debug_print(flags);
    if (ret)
	fprintf (stderr, "xfsprint: %s\n", strerror(ret));
    return 0;
}

static int
arladebug_cmd (int argc, char **argv)
{
    int ret;
    int flags;

    if ((argc > 1 && strncmp("-h", argv[1], 2) == 0) || argc > 2) {
	fprintf (stderr, "arladebug [-h] [");
	arla_log_print_levels (stderr);
	fprintf (stderr, "]\n");
	return 0;
    }

    ret = arla_debug (-1, &flags);
    if (ret) {
	fprintf (stderr, "arla_debug: %s\n", strerror(ret));
	return 0;
    }

    if (argc == 1) {
	char buf[1024];

	unparse_flags (flags, arla_deb_units, buf, sizeof(buf));
	printf ("arladebug is: %s\n", buf);
    } else if (argc == 2) {
	const char *textflags = argv[1];

	ret = parse_flags (textflags, arla_deb_units, flags);
	if (ret < 0) {
	    fprintf (stderr, "arladebug: unknown/bad flags `%s'\n",
		     textflags);
	    return 0;
	}

	flags = ret;
	ret = arla_debug (flags, NULL);
	if (ret)
	    fprintf (stderr, "arla_debug: %s\n", strerror(ret));
    }
    return 0;
}

static int
calculate_cmd (int argc, char **argv)
{
    int ret;
    u_int32_t used, calc;

    ret = fs_calculate_cache(&calc, &used);
    if (ret)
	warn ("fs_calculate_cache");
    else
	printf ("usedbytes:   %10d\n"
		"calculated:  %10d\n"
		"diff: c - u  %10d\n",
		used, calc, calc - used );
		
    return 0;
}

void
afs_getfid(char *path)
{
    VenusFid fid;
    int ret;
    char cellname[MAXNAME];

    ret = fs_getfid(path, &fid); 
    if (ret) {
	fserr(PROGNAME, ret, path);
	return;
    }
    
    ret = fs_getfilecellname(path, cellname, sizeof(cellname));
    if (ret) {
	fserr(PROGNAME, ret, path);
	return;
    }
    
    printf("Fid: %u.%u.%u in %s (%u) \n", fid.fid.Volume,
	   fid.fid.Vnode, fid.fid.Unique, cellname, fid.Cell);
}

void
afs_copyacl(char *fromdir, char *todir)
{
    struct Acl *acl;
    struct AclEntry *position;
    struct ViceIoctl a_params;
    int i;
    char acltext[MAXSIZE];
    char tmpstr[MAXSIZE];

    if((acl=afs_getacl(fromdir))==NULL)
	exit(1);

    snprintf(acltext, sizeof(acltext), "%d\n%d\n", acl->NumPositiveEntries,
	    acl->NumNegativeEntries);
    position=acl->pos;
    for(i=0; i<acl->NumPositiveEntries; i++) {
	snprintf(tmpstr, sizeof(tmpstr), "%s %d\n", position->name, position->RightsMask);
	strlcat(acltext, tmpstr, sizeof(acltext));
	position=position->next;
    }
    position=acl->neg;
    for(i=0; i<acl->NumNegativeEntries; i++) {
	snprintf(tmpstr, sizeof tmpstr, "%s %d\n",
	    position->name, position->RightsMask);
	strlcat(acltext, tmpstr, sizeof(acltext));
	position=position->next;
    }

    a_params.in_size=strlen(acltext);
    a_params.out_size=0;
    a_params.in=acltext;
    a_params.out=0;

    if(k_pioctl(todir,VIOCSETAL,&a_params,1)==-1) {
	fserr(PROGNAME, errno, todir);
	return;
    }
}

void
afs_listacl(char *path)
{
    struct Acl *acl;
    struct AclEntry *position;
    int i;

    acl = afs_getacl(path);
    if (acl == NULL) {
	if (errno == EACCES)
	    return;
	else
	    exit(1);
    }

    printf("Access list for %s is\n", path);
    if(acl->NumPositiveEntries) {
	printf("Normal rights:\n");

	position=acl->pos;
	for(i=0;i<acl->NumPositiveEntries;i++) {
	    printf("  %s ", position->name);
	    if(position->RightsMask&PRSFS_READ)
		printf("r");
	    if(position->RightsMask&PRSFS_LOOKUP)
		printf("l");
	    if(position->RightsMask&PRSFS_INSERT)
		printf("i");
	    if(position->RightsMask&PRSFS_DELETE)
		printf("d");
	    if(position->RightsMask&PRSFS_WRITE)
		printf("w");
	    if(position->RightsMask&PRSFS_LOCK)
		printf("k");
	    if(position->RightsMask&PRSFS_ADMINISTER)
		printf("a");
	    printf("\n");
	    position=position->next;
	}
    }
    if(acl->NumNegativeEntries) {
	printf("Negative rights:\n");

	position=acl->neg;
	for(i=0;i<acl->NumNegativeEntries;i++) {
	    printf("  %s ", position->name);
	    if(position->RightsMask&PRSFS_READ)
		printf("r");
	    if(position->RightsMask&PRSFS_LOOKUP)
		printf("l");
	    if(position->RightsMask&PRSFS_INSERT)
		printf("i");
	    if(position->RightsMask&PRSFS_DELETE)
		printf("d");
	    if(position->RightsMask&PRSFS_WRITE)
		printf("w");
	    if(position->RightsMask&PRSFS_LOCK)
		printf("k");
	    if(position->RightsMask&PRSFS_ADMINISTER)
		printf("a");
	    printf("\n");
	    position=position->next;
	}
    }
}

void
afs_setacl(char *path, char *user, char *rights)
{
    struct Acl *acl;
    struct AclEntry *position;
    struct ViceIoctl a_params;
    int i;
    int newrights=0;
    int foundit=0;
    char *ptr;
    char acltext[MAXSIZE];
    char tmpstr[MAXSIZE];

    if((acl=afs_getacl(path))==NULL)
	exit(1);

    if(!strcmp(rights,"read"))
	newrights=PRSFS_READ | PRSFS_LOOKUP;
    else if(!strcmp(rights,"write"))
	newrights=PRSFS_READ | PRSFS_LOOKUP | PRSFS_INSERT | PRSFS_DELETE |
	    PRSFS_DELETE | PRSFS_WRITE | PRSFS_LOCK;
    else if(!strcmp(rights,"mail"))
	newrights=PRSFS_INSERT | PRSFS_LOCK | PRSFS_LOOKUP;
    else if(!strcmp(rights,"all"))
	newrights=PRSFS_READ | PRSFS_LOOKUP | PRSFS_INSERT | PRSFS_DELETE |
	    PRSFS_WRITE | PRSFS_LOCK | PRSFS_ADMINISTER;
    else {
	ptr=rights;
	while(*ptr!=0) {
	    if(*ptr=='r')
		newrights|=PRSFS_READ;
	    if(*ptr=='l')
		newrights|=PRSFS_LOOKUP;
	    if(*ptr=='i')
		newrights|=PRSFS_INSERT;
	    if(*ptr=='d')
		newrights|=PRSFS_DELETE;
	    if(*ptr=='w')
		newrights|=PRSFS_WRITE;
	    if(*ptr=='k')
		newrights|=PRSFS_LOCK;
	    if(*ptr=='a')
		newrights|=PRSFS_ADMINISTER;
	    ptr++;
	}
    }

    position=acl->pos;
    for(i=0; i<acl->NumPositiveEntries; i++) {
	if(!strncmp(user, position->name, 100)) {
	    position->RightsMask=newrights;
	    foundit=1;
	}
	if(position->next)
	    position=position->next;
    }

    if(!foundit) {
	if (position) {
	    position->next=malloc(sizeof(struct AclEntry));
	    position=position->next;
	} else {
	    acl->pos = malloc(sizeof(struct AclEntry));
	    position = acl->pos;
	}
	if(position==NULL) {
	    printf("fs: Out of memory\n");
	    exit(1);
	}
	acl->NumPositiveEntries++;

	position->next=NULL;
	strlcpy(position->name, user, sizeof(position->name));
	position->RightsMask=newrights;
    }

    acltext[0] = 0;
    for(position=acl->pos; 
	position && acl->NumPositiveEntries; 
	position = position->next) {
	if (position->RightsMask) {
	    snprintf(tmpstr, sizeof tmpstr, "%s %d\n",
		position->name, position->RightsMask);
	    strlcat(acltext, tmpstr, sizeof(acltext));
	} else
	    acl->NumPositiveEntries--;
    }
    for(position=acl->neg; 
	position && acl->NumNegativeEntries;
	position = position->next) {
	if (position->RightsMask) {
	    snprintf(tmpstr, sizeof tmpstr, "%s %d\n",
		position->name, position->RightsMask);
	    strlcat(acltext, tmpstr, sizeof(acltext));
	} else
	    acl->NumNegativeEntries--;
    }
    strlcpy (tmpstr, acltext, sizeof(tmpstr));
    snprintf(acltext, sizeof(acltext), "%d\n%d\n%s",
	     acl->NumPositiveEntries, acl->NumNegativeEntries, tmpstr);

    a_params.in_size=strlen(acltext);
    a_params.out_size=0;
    a_params.in=acltext;
    a_params.out=0;


    if(k_pioctl(path,VIOCSETAL,&a_params,1)==-1) {
	fserr(PROGNAME, errno, path);
	return;
    }

    /*    free(oldacl);   and its contents */
}

struct Acl *
afs_getacl(char *path)
{
    struct Acl *oldacl;
    struct ViceIoctl a_params;
    struct AclEntry *pos=NULL;
    struct AclEntry *neg=NULL;
    char *curptr;
    char tmpname[MAXNAME];
    int tmprights;
    int i;

    oldacl=(struct Acl *) malloc(sizeof(struct Acl));
    if(oldacl == NULL) {
	printf("fs: Out of memory\n");
	return NULL;
    }

    a_params.in_size=0;
    a_params.out_size=MAXSIZE;
    a_params.in=NULL;
    a_params.out=malloc(MAXSIZE);

    if(a_params.out == NULL) {
	printf ("fs: Out of memory\n");
	free (oldacl);
	return NULL;
    }
    
    if(k_pioctl(path,VIOCGETAL,&a_params,1)==-1) {
	fserr(PROGNAME, errno, path);
	free (oldacl);
	free(a_params.out);
	return NULL;
    }

    curptr=a_params.out;

    /* Number of pos/neg entries parsing */
    sscanf(curptr, "%d\n%d\n", &oldacl->NumPositiveEntries,
	   &oldacl->NumNegativeEntries);
    skipline(&curptr);
    skipline(&curptr);
  
    if(oldacl->NumPositiveEntries)
	for(i=0; i<oldacl->NumPositiveEntries; i++) {      
	    sscanf(curptr, "%99s %d", tmpname, &tmprights);
	    skipline(&curptr);
	    if(!i) {
		pos=malloc(sizeof(struct AclEntry));
		oldacl->pos=pos;
	    }
	    else {
		pos->next=malloc(sizeof(struct AclEntry));
		pos=pos->next;
	    }
	    pos->RightsMask=tmprights;
	    strlcpy(pos->name, tmpname, sizeof(pos->name));
	    pos->next=NULL;
	}

    if(oldacl->NumNegativeEntries)
	for(i=0; i<oldacl->NumNegativeEntries; i++) {      
	    sscanf(curptr, "%99s %d", tmpname, &tmprights);
	    skipline(&curptr);
	    if(!i) {
		neg=malloc(sizeof(struct AclEntry));
		oldacl->neg=neg;
	    }
	    else {
		neg->next=malloc(sizeof(struct AclEntry));
		neg=neg->next;
	    }
	    neg->RightsMask=tmprights;
	    strlcpy(neg->name, tmpname, sizeof(neg->name));
	    neg->next=NULL;
	}

    free(a_params.out);
    return oldacl;
}

/*
 * Print current sysname.
 */

void
afs_print_sysname (void)
{
    int ret;
    char buf[2048];

    ret = fs_get_sysname (buf, sizeof(buf));
    if (ret)
	fserr (PROGNAME, ret, NULL);
    else
	printf ("Current sysname is '%s'\n", buf);
}

/*
 * Set sysname
 */

void
afs_set_sysname (const char *sys)
{
    int ret;

    ret = fs_set_sysname (sys);
    if (ret)
	fserr (PROGNAME, ret, NULL);
    else
	printf ("fs: new sysname set to `%s'\n", sys);
}

void
afs_listquota(char *path)
{
    struct ViceIoctl a_params;
    struct VolumeStatus *vs;
    char *name;
    double used_vol, used_part;

    a_params.in_size=0;
    a_params.out_size=MAXSIZE;
    a_params.in=NULL;
    a_params.out=malloc(MAXSIZE);

    if (a_params.out == NULL) {
	printf ("fs: Out of memory\n");
	return;
    }

    if(k_pioctl(path,VIOCGETVOLSTAT,&a_params,1)==-1) {
	fserr(PROGNAME, errno, path);
	free(a_params.out);
	return;
    }
  
    vs=(struct VolumeStatus *) a_params.out;
    name=a_params.out+sizeof(struct VolumeStatus);

    if (vs->MaxQuota)
	used_vol = ((double) vs->BlocksInUse / vs->MaxQuota) * 100;
    else
	used_vol = 0.0;

    if (vs->PartMaxBlocks)
	used_part = (1.0 - (double) vs->PartBlocksAvail / vs->PartMaxBlocks)
	    * 100;
    else
	used_part = 0.0;

    printf("%-20s%8d%8d%9.0f%%%s%9.0f%%%s%s\n",
	   name,
	   vs->MaxQuota,
	   vs->BlocksInUse,
	   used_vol,
	   used_vol > 90 ? "<<" : "  ",
	   used_part,
	   used_part > 97 ? "<<" : "  ",

	   /* Print a warning if more than 90% on home volume or 97% on */
	   /* the partion is being used */
	   (used_vol > 90 || used_part > 97) ? "\t<<WARNING" : "");

    free(a_params.out);
}

void
afs_setmaxquota(char *path, int32_t maxquota)
{
    struct ViceIoctl a_params;
    struct VolumeStatus *vs;
    int insize;

    a_params.in_size=0;
    a_params.out_size=MAXSIZE;
    a_params.in=NULL;
    a_params.out=malloc(MAXSIZE);

    if (a_params.out == NULL) {
	printf ("fs: Out of memory\n");
	return;
    }

    /* Read the old volume status */
    if(k_pioctl(path,VIOCGETVOLSTAT,&a_params,1)==-1) {
	fserr(PROGNAME, errno, path);
	free(a_params.out);
	return;
    }

    insize=sizeof(struct VolumeStatus)+strlen(path)+2;

    a_params.in_size=MAXSIZE<insize?MAXSIZE:insize;
    a_params.out_size=0;
    a_params.in=a_params.out;
    a_params.out=NULL;
  
    vs=(struct VolumeStatus *) a_params.in;
    vs->MaxQuota=maxquota;

    if(k_pioctl(path,VIOCSETVOLSTAT,&a_params,1)==-1) {
	fserr(PROGNAME, errno, path);
	free(a_params.in);
	return;
    }

    free(a_params.in);
}

void
afs_whereis(char *path)
{
    struct ViceIoctl a_params;
    struct in_addr addr;
    int32_t *curptr;
    int i=0;

    a_params.in_size=0;
    a_params.out_size=8*sizeof(int32_t);
    a_params.in=NULL;
    a_params.out=malloc(8*sizeof(int32_t));

    if(a_params.out == NULL) {
	printf ("fs: Out of memory\n");
	return;
    }

    if(k_pioctl(path,VIOCWHEREIS,&a_params,1)==-1) {
	fserr(PROGNAME, errno, path);
	free(a_params.out);
	return;
    }

    curptr=(int32_t *) a_params.out;
    printf("File %s is on host%s", path, curptr[0]&&curptr[1]?"s":"");

    while(curptr[i] && i<8) {
	struct hostent *h;
	addr.s_addr = curptr[i];
	h=gethostbyaddr((const char *) &addr, sizeof(addr), AF_INET);
	if (h == NULL)
	    printf (" %s", inet_ntoa (addr));
	else {
	    printf(" %s", h->h_name);
	}
	i++;
    }
    printf("\n");
    free(a_params.out);
}

int
afs_setcache(int lv, int hv, int lb, int hb)
{
    int ret;

    ret = fs_setcache (lv, hv, lb, hb);
    if (ret)
	fserr(PROGNAME, ret, ".");
    return ret;
}

void
afs_examine (char *path)
{
    struct ViceIoctl      a_params;
    struct VolumeStatus  *status;

    a_params.in_size = 0;
    a_params.out_size = MAXSIZE;
    a_params.in = NULL;
    a_params.out = malloc (MAXSIZE);

    if (a_params.out == NULL) {
	printf ("fs: Out of memory\n");
	return;
    }

    if (k_pioctl (path, VIOCGETVOLSTAT, &a_params, 1) == -1) {
	fserr(PROGNAME, errno, path);
	free(a_params.out);
	return;
    }

    status = (struct VolumeStatus *) a_params.out;

    printf ("Volume status for vid = %d named %s\n", status->Vid, 
	    a_params.out + sizeof (struct VolumeStatus));
    printf ("Current disk quota is %d\n", status->MaxQuota);
    printf ("Current blocks used are %d\n", status->BlocksInUse);
    printf ("The partition has %d blocks available out of %d\n\n", status->PartBlocksAvail, status->PartMaxBlocks);

    free (a_params.out);
}

void
afs_wscell (void)
{
    char buf[2048];
    int ret;

    ret = fs_wscell (buf, sizeof(buf));
    if (ret) {
	fserr (PROGNAME, ret, NULL);
	return;
    }

    printf ("This workstation belongs to cell '%s'\n", buf);
}

void
afs_whichcell (char *path)
{
    struct ViceIoctl a_params;

    a_params.in_size = 0;
    a_params.out_size = MAXSIZE;
    a_params.in = NULL;
    a_params.out = malloc (MAXSIZE);

    if (a_params.out == NULL) {
	printf ("fs: Out of memory\n");
	return;
    }

    if (k_pioctl (path, VIOC_FILE_CELL_NAME, &a_params, 0) == -1) {
	fserr(PROGNAME, errno, path);
	free(a_params.out);
	return;
    }

    printf ("File %s lives in cell '%s'\n", path, a_params.out);
    free (a_params.out);
}

void
afs_diskfree (char *path)
{
    struct ViceIoctl      a_params;
    struct VolumeStatus  *status;

    a_params.in_size = 0;
    a_params.out_size = MAXSIZE;
    a_params.in = NULL;
    a_params.out = malloc (MAXSIZE);

    if (a_params.out == NULL) {
	printf ("fs: Out of memory\n");
	return;
    }

    if (k_pioctl (path, VIOCGETVOLSTAT, &a_params, 1) == -1) {
	fserr(PROGNAME, errno, path);
	free(a_params.out);
	return;
    }

    status = (struct VolumeStatus *) a_params.out;

    printf ("%-20s%8d%8d%8d%9.0f%%\n",
	    a_params.out + sizeof (struct VolumeStatus),
	    status->PartMaxBlocks,
	    status->PartMaxBlocks - status->PartBlocksAvail,
	    status->PartBlocksAvail,
	    (float) (status->PartMaxBlocks - status->PartBlocksAvail) / status->PartMaxBlocks * 100);

    free (a_params.out);
}

void
afs_quota (char *path)
{
    struct ViceIoctl      a_params;
    struct VolumeStatus  *status;

    a_params.in_size = 0;
    a_params.out_size = MAXSIZE;
    a_params.in = NULL;
    a_params.out = malloc (MAXSIZE);

    if (a_params.out == NULL) {
	printf ("fs: Out of memory\n");
	return;
    }

    if (k_pioctl (path, VIOCGETVOLSTAT, &a_params, 1) == -1) {
	fserr(PROGNAME, errno, path);
	free(a_params.out);
	return;
    }

    status = (struct VolumeStatus *) a_params.out;
  
    printf("%.0f%% of quota used.\n",
	   ((float) status->BlocksInUse / status->MaxQuota) * 100);

    free(a_params.out);
}

void
afs_getcellstatus (char *cell)
{
    u_int32_t flags;
    int ret;

    ret = fs_getcellstatus (cell, &flags);
    if (ret) {
	fserr(PROGNAME, ret, cell);
	return;
    }
    printf ("Cell %s status: %ssetuid allowed\n", cell,
	    !SETUID_HONORED(flags) ? "no " : "");
}

/* 
 * List all the known cells, with servers iff printservers, resolving
 * IP addresses to names iff resolve and printing suid status iff
 * suid.
 */

int
afs_listcells (int printservers, int resolve, int suid)
{
    struct in_addr     addr;
    int		       i, j;
    char               cellname[MAXSIZE];
    u_int32_t	       servers[8];
    int 	       ret;
    unsigned	       max_servers = sizeof(servers)/sizeof(servers[0]);

    for (i = 1;
	 (ret = fs_getcells (i, servers, 
			     max_servers,
			     cellname, sizeof (cellname))) == 0;
	 ++i) {
	printf ("%s", cellname);

	if (printservers) {
	    printf (": ");

	    for (j = 0; j < max_servers && servers[j]; ++j) {
		struct hostent  *h = NULL;
		addr.s_addr = servers[j];
		if (resolve)
		    h = gethostbyaddr ((const char *) &addr, 
				       sizeof(addr), 
				       AF_INET);
		if (h == NULL) {
		    printf (" %s", inet_ntoa (addr));
		} else {
		    printf (" %s", h->h_name);
		}
	    }
	}
	if (suid) {
	    u_int32_t status;

	    ret = fs_getcellstatus (cellname, &status);
	    if (ret)
		fserr (PROGNAME, ret, NULL);
	    else {
		if (status & CELLSTATUS_SETUID)
		    printf (", suid cell");
	    }
	}
	printf (".\n");
    }

    if (errno != EDOM)
	fserr(PROGNAME, errno, NULL);

    return 0;
}

void
skipline(char **curptr)
{
  while(**curptr!='\n') (*curptr)++;
  (*curptr)++;
}


/*
 *
 */

static int
getcache_cmd (int argc, char **argv)
{
    int ret, help = 0;
    u_int32_t max_bytes, used_bytes, max_vnodes, used_vnodes;
    int optind = 0, bytes_flag = 0;

    struct agetargs ncargs[] = {
	{"byte", 'b', aarg_flag,  
	 NULL, "show result in byte instead of kbyte", NULL},
	{"help", 'h', aarg_flag,
	 NULL, "get help", NULL},
        {NULL,      0, aarg_end, NULL}}, 
				  *arg;
			       
    arg = ncargs;
    arg->value = &bytes_flag; arg++;
    arg->value = &help; arg++;

    if (agetarg (ncargs, argc, argv, &optind, AARG_AFSSTYLE)) {
	aarg_printusage(ncargs, "getcacheparams", NULL, AARG_AFSSTYLE);
	return 0;
    }

    if (help) {
	aarg_printusage(ncargs, "getcacheparams", NULL, AARG_AFSSTYLE);
	return 0;
    }


    ret = fs_getfilecachestats (&max_bytes,
				&used_bytes,
				&max_vnodes,
				&used_vnodes);
    if (ret) {
	fserr (PROGNAME, ret, NULL);
	return 0;
    }
    if (bytes_flag) {
	printf("Arla is using %lu of the cache's available "
	       "%lu byte blocks\n",
	       (unsigned long)used_bytes, (unsigned long)max_bytes);
	if (max_vnodes)
	    printf("(and %lu of the cache's available %lu vnodes)\n",
		   (unsigned long)used_vnodes, (unsigned long)max_vnodes);
    } else {
	printf("Arla is using %lu of the cache's available "
	       "%lu 1K byte blocks\n",
	       (unsigned long)used_bytes/1024, (unsigned long)max_bytes/1024);
	if (max_vnodes)
	    printf("(and %lu of the cache's available %lu vnodes)\n",
		   (unsigned long)used_vnodes, (unsigned long)max_vnodes);
    }
    return 0;
}
