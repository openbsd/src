/*	$OpenBSD: fs.c,v 1.1.1.1 1998/09/14 21:52:52 art Exp $	*/
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


#include <sl.h>
#include "appl_locl.h"
#include <kerberosIV/kafs.h>
#include "fs_local.h"

RCSID("$KTH: fs.c,v 1.41 1998/07/28 14:35:06 assar Exp $");

static SL_cmd cmds[] = {
    {"apropos",        empty_cmd,	"locate commands by keyword"},
    {"checkservers",   empty_cmd,	"check servers in servers"},
    {"checkvolumes",   empty_cmd,	"lookup mappings between volume-Id's and names"},
    {"cleanacl",       empty_cmd,	"clear out numeric acl-entries"},
    {"copyacl",        empty_cmd,	"copy acl"},
    {"diskfree",       diskfree_cmd,	"show free partition space"},
    {"examine",        examine_cmd,	"examine volume status"},
    {"flush",          flush_cmd,	"remove file from cache"},
    {"flushvolume",    flushvolume_cmd,	"remove volumedata (and files in volume) from cache"},
    {"getcacheparms",  empty_cmd,	"get cache usage"},
    {"getcrypt",       getcrypt_cmd,	"get encrypt status"},
    {"getcellstatus",  getcellstatus_cmd, "get suid cell status"},
    {"getfid",         getfid_cmd,      "get fid"},
    {"getserverprefs", empty_cmd,	"show server rank"},
    {"getpriority",    getprio_cmd,     "get priority of a file/dir"},
    {"gp"},
    {"getmaxpriority", getmaxprio_cmd,  "get max priority for file gc"},
    {"gmp"},
    {"help",           help_cmd,	"help for commands"},
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
    {"newcell",        empty_cmd,	"add new cell"},
    {"nop",            nop_cmd,         "do a picol-nop"},
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
    {"sysname",        sysname_cmd,	"read/change sysname"},
    {"version",        fsversion_cmd,   "get version of fs and fs_lib"},
    {"venuslog",       venuslog_cmd,    "make arlad print status"},
    {"whereis",        whereis_cmd,     "show server(s) of file"},
    {"whichcell",      whichcell_cmd,   "show cell of file"},
    {"wscell",         wscell_cmd,      "display cell of workstation"},
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
	return flags;
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
    int   i;

    argc--;
    argv++;

    if (argc == 0)
	afs_flushvolume (".");
    else
	for (i = 0; i < argc; i++)
	    afs_flushvolume (argv[i]);

    return 0;
}

static int
flush_cmd (int argc, char **argv)
{
    int   i;

    argc--;
    argv++;

    if (argc == 0)
	afs_flush (".");
    else
	for (i = 0; i < argc; i++)
	    afs_flush (argv[i]);

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

int
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
    SL_cmd  *cmd = cmds;

    while (cmd->name != NULL) {
        if (cmd->usage != NULL)
	  printf ("%-20s%s\n", cmd->name, cmd->usage);
	cmd++;
    }

    return 0;
}

static int
mkmount_cmd (int argc, char **argv)
{
    char  buf[MAXSIZE];

    argc--;
    argv++;

    if (argc == 0) {
	printf ("fs: Required parameter '-dir' missing\n");
	return 0;
    }
    else if (argc == 1) {
	printf ("fs: Required parameter '-vol' missing\n");
	return 0;
    }
    else {
	snprintf(buf, sizeof(buf), "#%s.", argv[1]);
	if (symlink (buf, argv[0]) == -1) {
	    perror ("fs");
	    return 0;
	}
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
    afs_listcells ();

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
	return ret;
    }

    return ret;
}


static int
setquota_cmd (int argc, char **argv)
{
    if(argc!=3) {
      printf("Usage: fs sq <path> <max quota in kbytes>\n");
      return 0;
    }

    afs_setmaxquota(argv[1],(int32_t) atoi(argv[2]));

    return 0;
}

static int
lsmount_cmd (int argc, char **argv)
{
    int   i;

    argc--;
    argv++;

    if (argc == 0) {
	printf ("fs: Required parameter '-dir' missing\n");
	return 0;
    }
    
    for (i = 0; i < argc; i++)
	afs_lsmount (argv[i]);

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
	    return afs_setcache(lv, hv, lb, hb);
	else
	    setcache_usage();
    } else 
	if (sscanf(argv[0], "%d", &lv))
	    afs_setcache(lv, 0, 0, 0);
	else 
	    setcache_usage();

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
    int   i;

    argc--;
    argv++;

    if (argc == 0) {
	printf ("fs: Required parameter '-dir' missing\n");
	return 0;
    }

    for (i = 0; i < argc; i++)
	afs_rmmount (argv[i]);

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
	afs_sysname (NULL);
    else
	afs_sysname (argv[0]);

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
    afs_venuslog();

    return 0;
}

static int
fsversion_cmd(int argc, char **argv)
{
    if (argc != 0)
	printf ("version: extraneous arguments ignored\n");

    printf("fs: %s\nfs_lib: %s\n",
	   "$KTH: fs.c,v 1.41 1998/07/28 14:35:06 assar Exp $",
	   fslib_version());
    return 0;
}

void afs_getfid(char *path)
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


void afs_listacl(char *path)
{
    struct Acl *acl;
    struct AclEntry *position;
    int i;

    if((acl=afs_getacl(path))==NULL)
	exit(1);

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

void afs_setacl(char *path, char *user, char *rights)
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
	acl->NumPositiveEntries++;

	/* We should already be at the last entry */
	position->next=malloc(sizeof(struct AclEntry));
	if(position->next==NULL) {
	    printf("fs: Out of memory\n");
	    exit(1);
	}
	position=position->next;
	position->next=NULL;
	strncpy(position->name, user, MAXNAME);
	position->RightsMask=newrights;
    }

    sprintf(acltext,"%d\n%d\n", acl->NumPositiveEntries,
	    acl->NumNegativeEntries);
    position=acl->pos;
    for(i=0; i<acl->NumPositiveEntries; i++) {
	sprintf(tmpstr, "%s %d\n", position->name, position->RightsMask);
	strcat(acltext,tmpstr);
	position=position->next;
    }
    position=acl->neg;
    for(i=0; i<acl->NumNegativeEntries; i++) {
	sprintf(tmpstr, "%s %d\n", position->name, position->RightsMask);
	strcat(acltext,tmpstr);
	position=position->next;
    }

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

struct Acl *afs_getacl(char *path)
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
	    sscanf(curptr, "%100s %d", tmpname, &tmprights);
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
	    strncpy(pos->name, tmpname, 100);
	    pos->next=NULL;
	}

    if(oldacl->NumNegativeEntries)
	for(i=0; i<oldacl->NumNegativeEntries; i++) {      
	    sscanf(curptr, "%100s %d", tmpname, &tmprights);
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
	    strncpy(neg->name, tmpname, 100);
	    neg->next=NULL;
	}

    free(a_params.out);
    return oldacl;
}

void afs_sysname(char *name)
{
    struct ViceIoctl a_params;
    int32_t get;
    char *space=malloc(MAXSIZE);
    char *curptr;

    if(space == NULL) {
	printf("fs: Out of memory\n");
	return;
    }

    if(!name) {
	get=0;
	memcpy(space,&get,sizeof(int32_t));
	a_params.in_size=sizeof(int32_t);
    }
    else {
	char *ptr=space;
	get=1;
	memcpy(space,&get,sizeof(int32_t));
	ptr+=sizeof(int32_t);
	strncpy(ptr,name,100-sizeof(int32_t));
	a_params.in_size=sizeof(int32_t)+strlen(ptr)+1;
    }
    
    a_params.out_size=MAXSIZE;
    a_params.in=space;
    a_params.out=space;
  
    if(k_pioctl(NULL,VIOC_AFS_SYSNAME,&a_params,1)==-1) {
	fserr(PROGNAME, errno, NULL);
	free(space);
	return;
    }

    curptr=a_params.out;
    if(!name) {
	curptr+=sizeof(int32_t);
	printf("Current sysname is '%s'\n",curptr);
    }
    else
	printf("fs: new sysname set.\n");
    free(space);
}

void afs_listquota(char *path)
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

void afs_setmaxquota(char *path, int32_t maxquota)
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

void afs_whereis(char *path)
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
	addr.s_addr=curptr[i];
	h=gethostbyaddr((const char *) &addr, sizeof(addr), AF_INET);
	if (h == NULL)
	    printf ("%s", inet_ntoa (addr));
	else {
	    printf(" %s",h->h_name);
	}
	i++;
    }
    printf("\n");
    free(a_params.out);
}

void afs_lsmount (char *path)
{
    struct ViceIoctl    a_params;
    char               *last;
    char               *path_bkp;
    int			error;

    path_bkp = strdup (path);
    if (path_bkp == NULL) {
	printf ("fs: Out of memory\n");
	return;
    }

    a_params.out = malloc (MAXSIZE);
    if (a_params.out == NULL) {
	printf ("fs: Out of memory\n");
	free (path_bkp);
    }

    /* If path contains more than the filename alone - split it */

    last = strrchr (path_bkp, '/');
    if (last) {
	*last = '\0';
	a_params.in = last + 1;
    }
    else
	a_params.in = path;

    a_params.in_size = strlen (a_params.in) + 1;
    a_params.out_size = MAXSIZE;

    error = k_pioctl (last ? path_bkp : "." ,
		      VIOC_AFS_STAT_MT_PT, &a_params, 0);
    if ((error == -1) && (errno != EINVAL)) {
	fserr(PROGNAME, errno, path);
	free (a_params.out);
	free (path_bkp);
	return;
    } else if ((error == -1) && (errno == EINVAL)) {
	printf ("'%s' is not a mount point.\n", path);
	free (a_params.out);
	free (path_bkp);
	return;	
    }

    printf ("'%s' is a mount point for volume '%s'\n", path, a_params.out);
    free (a_params.out);
    free (path_bkp);
}

void afs_rmmount (char *path)
{
    struct ViceIoctl a_params;

    a_params.in_size = strlen (path) + 1;
    a_params.out_size = 0;
    a_params.in = path;
    a_params.out = NULL;

    if (k_pioctl (".", VIOC_AFS_DELETE_MT_PT, &a_params, 0) == -1) {
	fserr(PROGNAME, errno, path);
	return;
    }
}

int
afs_setcache(int lv, int hv, int lb, int hb)
{
    struct ViceIoctl a_params;
    u_int32_t s[4];

    s[0] = lv;
    s[1] = hv;
    s[2] = lb;
    s[3] = hb;

    a_params.in_size = ((hv == 0) ? 1 : 4) * sizeof(u_int32_t);
    a_params.out_size = 0;
    a_params.in = (void *)s;
    a_params.out = NULL;

    if (k_pioctl(NULL, VIOCSETCACHESIZE, &a_params, 0) == -1) {
	fserr(PROGNAME, errno, ".");
	return 0;
    }
    return 0;
}

void afs_examine (char *path)
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

    if (k_pioctl (path, VIOCGETVOLSTAT, &a_params, 0) == -1) {
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
    struct ViceIoctl a_params;

    a_params.in_size = 0;
    a_params.out_size = MAXSIZE;
    a_params.in = NULL;
    a_params.out = malloc (MAXSIZE);

    if (a_params.out == NULL) {
	printf ("fs: Out of memory\n");
	return;
    }

    if (k_pioctl (NULL, VIOC_GET_WS_CELL, &a_params, 0) == -1) {
	fserr(PROGNAME, errno, NULL);
	free(a_params.out);
	return;
    }

    printf ("This workstation belongs to cell '%s'\n", a_params.out);

    free (a_params.out);
}

void afs_flushvolume (char *path)
{
    struct ViceIoctl a_params;

    a_params.in_size = 0;
    a_params.out_size = 0;
    a_params.in = NULL;
    a_params.out = NULL;

    if (k_pioctl (path, VIOC_FLUSHVOLUME, &a_params, 0) == -1) {
	perror ("fs");
	exit (1);
    }
}

void afs_flush (char *path)
{
    struct ViceIoctl a_params;

    a_params.in_size = 0;
    a_params.out_size = 0;
    a_params.in = NULL;
    a_params.out = NULL;

    if (k_pioctl (path, VIOCFLUSH, &a_params, 0) == -1) {
	perror ("fs");
	exit (1);
    }
}

void afs_venuslog (void)
{
    struct ViceIoctl a_params;
    int32_t status = 0;   /* XXX not really right, but anyway */

    a_params.in_size = sizeof(int32_t);
    a_params.out_size = 0;
    a_params.in = (caddr_t) &status;
    a_params.out = NULL;

    if (k_pioctl (NULL, VIOC_VENUSLOG, &a_params, 0) == -1) {
	perror ("fs");
	exit (1);
    }
}

void afs_whichcell (char *path)
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

void afs_diskfree (char *path)
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

    if (k_pioctl (path, VIOCGETVOLSTAT, &a_params, 0) == -1) {
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

void afs_quota (char *path)
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

    if (k_pioctl (path, VIOCGETVOLSTAT, &a_params, 0) == -1) {
	fserr(PROGNAME, errno, path);
	free(a_params.out);
	return;
    }

    status = (struct VolumeStatus *) a_params.out;
  
    printf("%.0f%% of quota used.\n",
	   (1.0 - (float) status->BlocksInUse / status->MaxQuota) * 100);

    free(a_params.out);
}

void afs_getcellstatus (char *cell)
{
    struct ViceIoctl   a_params;
    int32_t            flags;

    a_params.in_size = strlen (cell) + 1;
    a_params.out_size = sizeof (int32_t);
    a_params.in = cell;
    a_params.out = (char *) &flags;

    if (k_pioctl (NULL, VIOC_GETCELLSTATUS, &a_params, 0) == -1) {
	fserr(PROGNAME, errno, cell);
	return;
    }

    printf ("Cell %s status: %ssetuid allowed\n", cell, NO_SETUID_HONORED(flags) ? "no " : "");
}

void afs_listcells (void)
{
    struct ViceIoctl   a_params;
    struct in_addr     addr;
    int32_t            *ip;
    int32_t            i, j;

    a_params.in_size = sizeof (int32_t);
    a_params.out_size = MAXSIZE;
    a_params.in = (char *) &i;
    a_params.out = malloc (MAXSIZE);

    if (a_params.out == NULL) {
	printf ("fs: Out of memory\n");
	return;
    }

    i = 0;

    while (k_pioctl (NULL, VIOCGETCELL, &a_params, 0) != -1) {
	ip = (int32_t *) a_params.out;
	printf ("Cell %s on hosts", a_params.out + 8 * sizeof (int32_t));

	j = 0;

	while (ip[j] && j<8) {
	    struct hostent  *h;
	    addr.s_addr = ip[j];
	    h = gethostbyaddr ((const char *) &addr, sizeof(addr), AF_INET);
	    if (h == NULL) {
		printf (" %s", inet_ntoa (addr));
	    }
	    else {
		printf (" %s", h->h_name);
	    }
	    j++;
	}
	printf (".\n");
	i++;
    }

    if (errno) {
	fserr(PROGNAME, errno, NULL);
	free(a_params.out);
	return;
    }

    free (a_params.out);
}

void skipline(char **curptr)
{
  while(**curptr!='\n') (*curptr)++;
  (*curptr)++;
}


