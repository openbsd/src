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

#include "fs_local.h"

RCSID("$arla: fs.c,v 1.111 2003/01/17 03:16:18 lha Exp $");

static int empty_cmd (int argc, char **argv);
static int setmaxprio_cmd (int argc, char **argv);
static int setprio_cmd (int argc, char **argv);
static int incompat_cmd (int argc, char **argv);
static int invalidate_cmd (int argc, char **argv);
static int getmaxprio_cmd (int argc, char **argv);
static int quit_cmd (int argc, char **argv);
static int rmprio_cmd (int argc, char **argv);
static int gc_cmd (int argc, char **argv);
static int getcrypt_cmd (int argc, char **argv);
static int nop_cmd (int argc, char **argv);
static int sysname_cmd (int argc, char **argv);
static int venuslog_cmd (int argc, char **argv);
static int checkvolumes_cmd (int argc, char **argv);
static int calculate_cmd (int argc, char **argv);
static int apropos_cmd (int argc, char **argv);
static int help_cmd (int argc, char **argv);

/* if this is set the program runs in interactive mode */
int fs_interactive = 0;

static int
empty_cmd (int argc, char **argv)
{
    printf ("%s%s not implemented yet!\n", PROGNAME, argv[0]);
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
    
    argc--;
    argv++;

    if (argc != 2) 
	return setprio_usage();

    if (sscanf(argv[1], "%d", &prio_tmp) != 1) 
	return setprio_usage();

    prio = prio_tmp;

    ret = fs_getfid(argv[0], &fid);
    if (ret) {
	fserr(PROGNAME, ret, argv[0]);
	return 0;
    }
	
    ret = fs_setfprio(fid, prio);
    if (ret) {
	fserr(PROGNAME, ret, argv[0]);
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
getmaxprio_cmd (int argc, char **argv)
{
    int16_t prio;
    int ret;
    
    argc--;
    argv++;

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
quit_cmd (int argc, char **argv)
{
    printf ("Exiting...\n");
    return 1;
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
getcrypt_cmd (int argc, char **argv)
{
    uint32_t n;
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
nop_cmd(int argc, char **argv)
{
    if (argc > 1)
	fprintf(stderr, "extraneous arguments ignored\n");

    printf("VIOCNOP returns %d\n", fs_nop());

    return 0;
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
venuslog_cmd (int argc, char **argv)
{
    int ret = fs_venuslog();
    if (ret)
	fserr (PROGNAME, ret, NULL);
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
calculate_cmd (int argc, char **argv)
{
    int ret;
    uint32_t used, calc;

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

static SL_cmd cmds[] = {
    {"apropos",        apropos_cmd,	"locate commands by keyword"},
    {"arladebug",      arladebug_cmd,	"change arla-debugging flags"},
    {"calculate cache", calculate_cmd,  "calculate the usage of cache"},
    {"checkservers",   checkservers_cmd,"check if servers are up"},
    {"checkvolumes",   checkvolumes_cmd,
     "lookup mappings between volume-ids and names"},
    {"cleanacl",       empty_cmd,	"clear out numeric acl-entries"},
    {"copyacl",        copyacl_cmd,	"copy acl"},
    {"ca" },
    {"diskfree",       diskfree_cmd,	"show free partition space"},
    {"df"},
    {"examine",        examine_cmd,	"examine volume status"},
    {"listvol"},
    {"lv"},
    {"flush",          flush_cmd,	"remove file from cache"},
    {"flushvolume",    flushvolume_cmd,
     "remove volumedata (and files in volume) from cache"},
    {"gcpags",         gc_cmd,          "garbage collect pags"},
    {"getcacheparms",  getcache_cmd,	"get cache usage"},
    {"getcrypt",       getcrypt_cmd,	"get encryption status"},
    {"getcellstatus",  getcellstatus_cmd, "get suid cell status"},
#if 0
    {"getclientaddrs", empty_cmd,
     "get client network interface addresses"},
    {"gc"},
#endif
    {"getfid",         getfid_cmd,      "get fid"},
    {"getserverprefs", empty_cmd,	"show server rank"},
    {"gp"},
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
#if 0
    {"setclientaddrs", empty_cmd,
     "set client network interface addresses"},
    {"sc"},
#endif
    {"setpriority",    setprio_cmd,     "set priority of a file/directory"},
    {"setmaxpriority", setmaxprio_cmd,  "set upper limit of prio gc"},
    {"smq"}, 
    {"setquota",       setquota_cmd,	"change maxquota on volume"},
    {"sq"},
    {"setserverprefs", empty_cmd,	"change server query order"},
    {"sp"}, /* this is what openafs uses */
    {"setcrypt",       setcrypt_cmd,	"set encryption on/off"},
    {"setvol",         empty_cmd,	"change status of volume"},
    {"sv"},
#if 0
    {"storebehind",    empty_cmd,	""},
    {"sb"},
#endif
    {"strerror",       strerror_cmd,    "translate errorcode to message"},
    {"suidcells",      suidcells_cmd,	"list status of cells"},
    {"sysname",        sysname_cmd,	"read/change sysname"},
    {"version",        arlalib_version_cmd,  "print version"},
    {"venuslog",       venuslog_cmd,    "make arlad print status"},
    {"whereis",        whereis_cmd,     "show server(s) of file"},
    {"whichcell",      whichcell_cmd,   "show cell of file"},
    {"wscell",         wscell_cmd,      "display cell of workstation"},
    {"nnpfsdebug",       nnpfsdebug_cmd,    "change nnpfs-debugging flags"},
    {"nnpfsprint",       nnpfsdebug_print_cmd,"make nnpfs print debug info"},
    {NULL}
};

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
help_cmd (int argc, char **argv)
{
    SL_cmd *cmd;

    for (cmd = cmds; cmd->name != NULL; ++cmd)
        if (cmd->usage != NULL)
	    printf ("%-20s%s\n", cmd->name, cmd->usage);

    return 0;
}

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
    if (ret == -1)
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

