/*	$OpenBSD: vos.c,v 1.1.1.1 1998/09/14 21:52:53 art Exp $	*/
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

#include "appl_locl.h"
#include <sl.h>
#include "vos_local.h"

RCSID("$KTH: vos.c,v 1.46 1998/08/23 22:50:21 assar Exp $");

static int empty_cmd (int, char **);
static int exa_cmd (int, char **);
static int help_cmd (int, char **);
static int quit_cmd (int, char **);
static int listp_cmd (int, char **);
static int livol_cmd (int, char **);
static int parti_cmd (int, char **);
static int stat_cmd (int, char **);
static int syncsite_cmd (int, char **);

static SL_cmd cmds[] = {
    {"addsite",    empty_cmd,       "not yet implemented"},
    {"apropos",    empty_cmd,       "not yet implemented"},
    {"backup",     empty_cmd,       "not yet implemented"},
    {"backupsys",  empty_cmd,       "not yet implemented"},
    {"changeaddr", empty_cmd,       "not yet implemented"},
    {"create",     empty_cmd,       "not yet implemented"},
    {"delentry",   empty_cmd,       "not yet implemented"},
    {"dump",       empty_cmd,       "not yet implemented"},
    {"examine",    exa_cmd,         "print information about a volume"},
    {"volinfo"},
    {"help",       help_cmd,        "print help"},
    {"?"},
    {"listpart",   listp_cmd,       "list partitions on a server"},
    {"listvldb",   empty_cmd,       "not yet implemented"},
    {"listvol",    livol_cmd,       "list volumes on a server"},
    {"lock",       empty_cmd,       "not yet implemented"},
    {"move",       empty_cmd,       "not yet implemented"},
    {"partinfo",   parti_cmd,       "print partition information on a server"},
    {"release",    empty_cmd,       "not yet implemented"},
    {"remove",     empty_cmd,       "not yet implemented"},
    {"remsite",    empty_cmd,       "not yet implemented"},
    {"rename",     empty_cmd,       "not yet implemented"},
    {"restore",    empty_cmd,       "not yet implemented"},
    {"status",     stat_cmd,        "Show volume server transactions"},
    {"syncserv",   empty_cmd,       "not yet implemented"},
    {"syncvldb",   empty_cmd,       "not yet implemented"},
    {"syncsite",   syncsite_cmd,    "print the syncsite"},
    {"unlock",     empty_cmd,       "not yet implemented"},
    {"unlockvldb", empty_cmd,       "not yet implemented"},
    {"zap",        empty_cmd,       "not yet implemented"},
    {"quit",       quit_cmd,        "exit interactive mode"},
    {NULL}
};



int printlistparts(char *server);
int printvolstat(char *volname,
		 const char *cell, const char *host, int noauth);

#define LISTVOL_PART      0x1
#define LISTVOL_NOAUTH    0x2
#define LISTVOL_LOCALAUTH 0x4

int printlistvol(char *server, int part, int flags);
int printpartinfo(char *server, char *partition);
int printstatus(char *server);

static int
empty_cmd(int argc, char **argv)
{
    printf("%s%s has not been implemented yet!\n", PROGNAME, argv[0]);
    return 0;
}

static int
quit_cmd(int argc, char **argv)
{
    printf("exiting\n");
    return 1;
}

static int
help_cmd(int argc, char **argv)
{
    sl_help(cmds, argc, argv);
    return 0;
}

/*
 * volume_name2num 
 *   convert a char string that might be a partition to a 
 *   number.
 *
 *  returns -1 is there is an error
 *
 */

int
volume_name2num(char *name)
{
    int ret;

    if (strncmp(name, "/vicep", 6) == 0)
	name += 6;
    else if (strncmp(name, "vicep", 5) == 0)
	name += 5;

    if (*name == '\0')
	return -1;

    if(*(name+1) == '\0') {
	if(isalpha(*name)) {
	    ret = tolower(*name) - 'a';
	} else
	    return -1;
    } else if (*(name+2) == '\0') {
	if (isalpha(*name) && isalpha(*name+1)) {
	    ret = 26 * (tolower(*(name)) - 'a' + 1) + tolower(*(name+1)) - 'a';
	} else
	    return -1;
    } else
	return -1;

    if(ret > 255)
	return -1;

    return ret;

}


static int exa_helpflag;
static char *exa_host;
static char *exa_cell;
static char *exa_vol;
static int exa_noauth;
static int exa_localauth;

static struct getargs exa_args[] = {
    {"id",	0, arg_string,  &exa_vol,  "id of volume", NULL, arg_mandatory},
    {"host",	0, arg_string,  &exa_host, "what host to use", NULL},
    {"cell",	0, arg_string,  &exa_cell, "what cell to use", NULL},
    {"noauth",	0, arg_flag,    &exa_noauth, "what cell to use", NULL},
    {"localauth",0,arg_flag,    &exa_localauth, "localauth", NULL},
    {"help",	0, arg_flag,    &exa_helpflag, NULL, NULL},
    {NULL,      0, arg_end, NULL}
};


int
exa_usage()
{
    arg_printusage(exa_args, NULL, "examine");
    return 0;
}


static int
exa_cmd(int argc, char **argv)
{
    int optind = 0;
    exa_noauth = exa_helpflag = 0;
    exa_vol = NULL;
    
    exa_cell = NULL;
    exa_host = NULL;
    exa_noauth = 0;

    if (getarg (exa_args, argc, argv, &optind, ARG_AFSSTYLE)) 
	return exa_usage();

    argc -= optind;
    argv += optind;

    if (argc) {
	printf("unknown option %s\n", *argv);
	return 0;
    }

    /* don't allow any bogus volname */
    if (exa_vol == NULL || exa_vol[0] == '\0') 
	return exa_usage();

    printvolstat(exa_vol, exa_cell, exa_host, exa_noauth);
    return 0;
}

static int listp_helpflag;
static char *listp_server;
static int listp_noauth;

static struct getargs listp_args[] = {
    {"server",	0, arg_string,  &listp_server, "server", NULL, arg_mandatory},
    {"noauth",	0, arg_flag,    &listp_noauth, "what cell to use", NULL},
    {"help",	0, arg_flag,    &listp_helpflag, NULL, NULL}
};

int
listp_usage()
{
    arg_printusage(listp_args, NULL, "listpart");
    return 0;
}


static int
listp_cmd(int argc, char **argv)
{
    int optind = 0;
    exa_noauth = exa_helpflag = 0;
    exa_vol = NULL;
    

    if (getarg (listp_args,argc, argv, &optind, ARG_AFSSTYLE)) 
	return listp_usage();

    argc -= optind;
    argv += optind;

    if (argc) {
	printf("unknown option %s\n", *argv);
	return 0;
    }

    if (listp_server == NULL || listp_server[0] == '\0') 
	return listp_usage();

    printlistparts(listp_server);
    return 0;
}

/*
 * list volume on a afs-server
 */

char *listvol_server;
char *listvol_partition;
int   listvol_machine;
char *listvol_cell;
char *listvol_noauth;
char *listvol_localauth;
char *listvol_help;

static struct getargs listvol_args[] = {
    {"server",	0, arg_string,  &listvol_server,  
     "server to probe", NULL, arg_mandatory},
    {"partition", 0, arg_string, &listvol_partition,
     "partition to probe", NULL},
    {"machine", 'm', arg_flag, &listvol_machine,
     "machineparseableform", NULL},
    {"cell",	0, arg_string,  &listvol_cell, 
     "what cell to use", NULL},
    {"noauth",	0, arg_flag,    &listvol_noauth, 
     "don't authenticate", NULL},
    {"localauth",	0, arg_flag,    &listvol_localauth, 
     "use local authentication", NULL},
    {"help",	0, arg_flag,    &listvol_help, 
     NULL, NULL},
    {NULL,      0, arg_end, NULL}
};

void
listvol_usage()
{
    printf("Usage: %slistvol [-server] servername [[-partition] part ] [[-cell] cell] [-noauth] [-localauth] [-machine] [-help]\n", PROGNAME);
}

static int
livol_cmd(int argc, char **argv)
{
    int optind = 0;
    int flags = 0;
    int part;

    if (getarg (listvol_args, argc, argv, &optind, ARG_AFSSTYLE)) {
	listvol_usage();
	return 0;
    }

    if(listvol_help) {
	listvol_usage();
	return 0;
    }
    
    argc -= optind;
    argv += optind;

    if (listvol_server == NULL) {
	listvol_usage();
	return 0;
    }

    if (listvol_partition == NULL) {
	part = -1;
    } else {
	part = volume_name2num(listvol_partition);
	if (part == -1) {
	    listvol_usage();
	    return 0;
	}
    }

    if (listvol_machine)
	flags |= LISTVOL_PART;
    if (listvol_noauth)
	flags |= LISTVOL_NOAUTH;
    if (listvol_localauth)
	flags |= LISTVOL_LOCALAUTH;
	
    printlistvol(listvol_server, part, flags ); 
    return 0;
}

void
parti_usage()
{
    printf("Usage: %spartinfo <server name> [partition name]\n", PROGNAME);
}

static int
parti_cmd(int argc, char **argv)
{
    char *server;
    char *partition = NULL;

    /* remove command name */
    argc--;
    argv++;

    if (argc < 1) {
	parti_usage();
	return 0;
    }

    /* dinosaur protection */
    if (strcmp(argv[0], "-server") == 0) {
	argc--;
	argv++;
    }

    server = argv[0];

    argc--;
    argv++;

    /* sanity check (assumes that argv[argc] == NULL)*/
    if (server == NULL || server[0] == '-' || server[0] == '\0') {
	parti_usage();
	return 0;
    }
    
    /* there could be a partition specified */
    if (argc > 0) {
	/* dinosaur protection */
	if (strcmp(argv[0], "-partition") == 0) {
	    argc--;
	    argv++;
	    partition = argv[0];
	    argc--;
	    argv++;
	} else
	    /* this could be the partition */
	    if (argv[0][0] != '-') {
		partition = argv[0];
		argc--;
		argv++;
	    }
    }

    printpartinfo(server, partition);
    return 0;
}

void
stat_usage()
{
    printf("Usage: %sstatus <server name>\n", PROGNAME);
}

static int
stat_cmd(int argc, char **argv)
{
    char *server;

    /* remove command name */
    argc--;
    argv++;

    if (argc < 1) {
	stat_usage();
	return 0;
    }

    /* dinosaur protection */
    if (strcmp(argv[0], "-server") == 0) {
	argc--;
	argv++;
    }

    server = argv[0];

    argc--;
    argv++;

    if (server == NULL || server[0] == '-' || server[0] == '\0') {
	stat_usage();
	return 0;
    }

    printstatus(server);
    return 0;
}

static int 
syncsite_cmd (int argc, char **argv)
{
  struct in_addr saddr;
  int error;

  error = arlalib_getsyncsite(NULL, NULL, afsvldbport, &saddr.s_addr, 0);
  if (error) {
      fprintf(stderr, "syncsite: %s (%d)\n", koerr_gettext(error), error);
      return 0;
  }

  printf("Our vldb syncsite is %s.\n", inet_ntoa(saddr));

  return 0;
}

static char*
getvolumetype(int32_t flag)
{
    char *str ;
    if (flag & VLSF_RWVOL)
	str = "RW";
    else if (flag & VLSF_ROVOL)
	str = "RO";
    else if (flag & VLSF_BACKVOL)
	str = "BACKUP";
    else
	str = "FOO!";

    return str;
}

static char*
getvolumetype2(int32_t type)
{
    char *str = NULL;
    switch (type) {
    case RWVOL:
	str = "RW";
	break;
    case ROVOL:
	str = "RO";
	break;
    case BACKVOL:
	str = "BK";
	break;
    default:
	str = "FOO!";
    }
    return str;
}

int
printvolstat(char *volname, const char *cell, const char *host, int noauth)
{
    struct rx_connection *connvolser = NULL;
    struct rx_connection *connvldb = NULL;
    int error;
    int i;
    char *servername;
    char timestr[30];
    volEntries volint ;
    vldbentry vlentry;

    if (cell == NULL && host != NULL) 
	cell = cell_getcellbyhost(host);
    if (cell == NULL)
	cell = cell_getthiscell();
    if (host == NULL)
	host = cell_findnamedbbyname (cell);

    
    connvldb = arlalib_getconnbyname(host,
				     afsvldbport,
				     VLDB_SERVICE_ID,
				     noauth);
    if (connvldb == NULL)
	return -1;
    
    
    error = VL_GetEntryByName(connvldb, volname, &vlentry);
    if (error) {
	fprintf(stderr, "VLDB: no such entry\n");
	return -1;
    } 

    connvolser = arlalib_getconnbyaddr(htonl(vlentry.serverNumber[0]),
				       NULL,
				       afsvolport,
				       VOLSERVICE_ID,
				       noauth);
    if (connvolser == NULL)
	return -1 ;
    
    volint.val = NULL;
    if ((error = VOLSER_AFSVolListOneVolume(connvolser,
					    vlentry.serverPartition[0],
					    vlentry.volumeId[0],
					    &volint)) != 0) {
	printf("ListOneVolume failed with: %s (%d)\n", 
	       koerr_gettext(error),
	       error);
	return -1;
    }
    
    printf("%s\t\t\t%10u %s %8d K  %s\n", 
	   vlentry.name, 
	   vlentry.volumeId[0], 
	   getvolumetype(vlentry.serverFlags[0]),
	   volint.val->size,
	   volint.val->status == VOK ? "On-line" : "Busy");
    
    if (volint.val->status == VOK) {
	
	arlalib_getservername(htonl(vlentry.serverNumber[0]), &servername);
	printf("    %s /vicep%c\n", servername, 
	       'a' + vlentry.serverPartition[0]);
	printf("    ");
	free(servername);
	
	if (vlentry.flags & VLF_RWEXISTS)
	    printf("RWrite %u\t", vlentry.volumeId[RWVOL]);
    
	if (vlentry.flags & VLF_ROEXISTS )
	    printf("ROnly %u\t", vlentry.volumeId[ROVOL]);

	if (vlentry.flags & VLF_BACKEXISTS)
	    printf("Backup %u\t", vlentry.volumeId[BACKVOL]);

	printf("\n    MaxQuota %10d K\n", volint.val->maxquota);
	
	/* Get and format date */
	strncpy(timestr, 
		ctime( (time_t*) &volint.val->creationDate),
		sizeof(timestr));
	timestr[strlen(timestr)-1] = '\0';
	printf("    Creation    %s\n", timestr);
	
	/* Get and format date */
	strncpy(timestr, 
	       ctime((time_t *) &volint.val->updateDate),
	       sizeof(timestr));
	timestr[strlen(timestr)-1] = '\0';
	printf("    Last Update %s\n", timestr);

	printf("    %d accesses in the past day (i.e., vnode references)\n\n",
	       volint.val->dayUse);
	
    }

    /* Print volume stats */
    printf("    ");
    printf("RWrite: %u\t", vlentry.flags & VLF_RWEXISTS ? vlentry.volumeId[RWVOL] : 0);
    printf("ROnly: %u\t", vlentry.flags & VLF_ROEXISTS ? vlentry.volumeId[ROVOL] : 0);
    printf("Backup: %u\t", vlentry.flags & VLF_BACKEXISTS ? vlentry.volumeId[BACKVOL] : 0);

    printf("\n    number of sites -> %d\n", vlentry.nServers );
    
     for ( i = 0 ; i < vlentry.nServers ; i++) {
	 printf("       ");
	 
	 if (vlentry.serverNumber[i] != 0)
	     arlalib_getservername(htonl(vlentry.serverNumber[i]), &servername);
	 else
	     servername = strdup("error");
	 
	 printf("server %s partition /vicep%c %s Site\n",
		servername,
		'a' + vlentry.serverPartition[i],
		getvolumetype(vlentry.serverFlags[i]));
	 free(servername);
	 
     }
     
     free(volint.val);
     arlalib_destroyconn(connvolser);
     arlalib_destroyconn(connvldb);
     return 0;
}


int
getlistparts(char *host, struct pIDs *partIDs)
{
    struct rx_connection *connvolser = NULL;
    int error ;
    
    connvolser = arlalib_getconnbyname(host,
				       afsvolport,
				       VOLSERVICE_ID,
				       0); /* XXX this means try auth */
    if (connvolser == NULL)
	return -1 ;
    
    if ((error = VOLSER_AFSVolListPartitions(connvolser,
					     partIDs)) != 0) {
	printf("getlistparts: ListPartitions failed with: %s (%d)\n", 
	       koerr_gettext(error),
	       error);
	return -1;
    }

    arlalib_destroyconn(connvolser);
    return 0;
}

int
printlistparts(char *host)
{
    struct pIDs partIDs;
    int i,j ;
    
    if ((i = getlistparts(host, &partIDs)) != 0)
	return i;

    j = 0 ;
    
    printf("The partitions on the server are:\n ");
    for (i = 0 ; i < 26 ; i++) {
	if (partIDs.partIds[i] != -1) {
	    j++;
	    printf("   /vicep%c%c", i + 'a', j % 6 == 0 ? '\n':' ');
	}
    }
    
    printf("\nTotal: %d\n", j);
    return 0;
}


int
printlistvol(char *host, int part, int flags)
{
    struct rx_connection *connvolser = NULL;
    volEntries volint ;
    struct pIDs partIDs;
    volintInfo *vi;
    int32_t partnum = 0;
    int32_t vol;
    int online, busy, offline;
    int error ;
    
    connvolser = arlalib_getconnbyname(host,
				       afsvolport,
				       VOLSERVICE_ID,
				       flags & LISTVOL_NOAUTH);
    if (connvolser == NULL)
	return -1 ;

    if (part == -1) {
	if ((error = getlistparts(host, &partIDs)) != 0)
	    return -1;
    } else {
	for (partnum = 0 ; partnum < 26 ; partnum++)
	    partIDs.partIds[partnum] = -1 ;
	partIDs.partIds[0] = part ;
    }

    partnum = 0;
    while(1) {
	while(partIDs.partIds[partnum] == -1 && partnum < 26 ) partnum++;
	if (partnum >= 26) 
	    break;
	
	volint.val = NULL;
	if ((error = VOLSER_AFSVolListVolumes(connvolser,
					      partIDs.partIds[partnum],
					      1, /* We want full info */
					      &volint)) != 0) {
	    printf("printlistvol: PartitionInfo failed with: %s (%d)\n", 
		   koerr_gettext(error),
		   error);
	    return -1;
	}
	online =  busy = offline = 0;

	printf("Total number of volumes on server %s partition /vicep%c: %d\n",
	       host, 
	       partIDs.partIds[partnum] + 'a',
	       volint.len);

	for (vol = 0 ; vol < volint.len ; vol ++) {
	    vi = &volint.val[vol];
	    
	    if (vi->status == VBUSY)
		busy++;
	    else if (vi->inUse)
		online++;
	    else
		offline++;

	    if(vi->status == VOK) {
		printf("%-38s %10u %s %10u K %s %s%c\n", 
		       vi->name,
		       vi->volid,
		       getvolumetype2(vi->type),
		       vi->size,
		       vi->inUse ? "On-line" : "Off-line",
		       flags&LISTVOL_PART?"/vicep":"",
		       flags&LISTVOL_PART?partIDs.partIds[partnum] + 'a':' ');
		
	    } 
	}

	for (vol = 0 ; vol < volint.len ; vol ++) {
	    vi = &volint.val[vol];

	    if(vi->status == VBUSY)
		 printf("Volume with id number %u is currently busy\n",
			vi->volid);
	}

	printf("\nTotal volumes onLine %d ; Total volumes offLine %d " \
	       "; Total busy %d\n\n",
	       online, offline, busy);
	
	free(volint.val);
	partnum++;
    }

    arlalib_destroyconn(connvolser);
    return 0;
}

int
printpartinfo(char *host, char *part)
{
    struct rx_connection *connvolser = NULL;
    struct diskPartition partinfo ;
    struct pIDs partIDs;
    int iter = 0;
    int partnum = 0;
    char name[30];
    int error ;
    
    connvolser = arlalib_getconnbyname(host,
				       afsvolport,
				       VOLSERVICE_ID,
				       0); /* XXX This means try auth */
    if (connvolser == NULL)
	return -1 ;

    if (part == NULL) {
	iter = 1;
	if ((error = getlistparts(host, &partIDs)) != 0)
	    return error;
    } else {
	if (strlen(part) <= 2) {
	    snprintf(name, sizeof(name), "/vicep%s", part);
	    part = name;
	}
    }

    while(part != NULL || iter) {
    
	if (iter) {
	    while(partIDs.partIds[partnum] == -1 && partnum < 26 ) partnum++;
	    if (partnum < 26) {
		snprintf(name, sizeof(name), "/vicep%c", 'a' + partnum);
		part = name;
		partnum++;
	    } else {
		iter = 0 ;
		continue;
	    }
	}   

	if ((error = VOLSER_AFSVolPartitionInfo(connvolser,
						part,
						&partinfo)) != 0) {
	    printf("printpartinfo: PartitionInfo failed with: %s (%d)\n", 
		   koerr_gettext(error),
		   error);
	    return -1;
	}
	printf("Free space on partition %s %d K blocks out of total %d\n",
	       partinfo.name, 
	       partinfo.free,
	       partinfo.minFree
/* XXX -	       partinfo.totalUsable */
	       );
	part = NULL;
    }
    
    arlalib_destroyconn(connvolser);
    return 0;
}

int
printstatus(char *host)
{
    struct rx_connection *connvolser = NULL;
    struct transDebugInfo *entries;
    transDebugEntries info;
    unsigned int entries_len, i;
    int error;

    connvolser = arlalib_getconnbyname(host,
				       afsvolport,
				       VOLSERVICE_ID,
				       0);

    if (connvolser == NULL)
	return -1;

    if ((error = VOLSER_AFSVolMonitor(connvolser,
				      &info)) != 0) {
	printf("printstatus: GetStat failed with: %s (%d)\n",
	       koerr_gettext(error),
	       error);
	return -1;
    }

    entries_len = info.len;
    entries = info.val;

    if (entries_len == 0)
	printf ("No active transactions on %s\n", host);
    else {
	for (i = 0; i < entries_len; i--) {
	    printf("--------------------------------------\n");
	    printf("transaction: %d  created: %s", entries->tid, ctime((time_t *) &entries->creationTime));
	    printf("attachFlags:  ");

	    if ((entries->iflags & ITOffline) == ITOffline)
		printf(" offline");
	    if ((entries->iflags & ITBusy) == ITBusy)
		printf(" busy");
	    if ((entries->iflags & ITReadOnly) == ITReadOnly)
		printf("read-only");
	    if ((entries->iflags & ITCreate) == ITCreate)
		printf("create");
	    if ((entries->iflags & ITCreateVolID) == ITCreateVolID)
		printf("create-VolID");

	    printf("\nvolume: %d  partition: <insert partition name here>  procedure: %s\n", entries->volid, entries->lastProcName);
	    printf("packetRead: %d  lastReceiveTime: %d  packetSend: %d  lastSendTime: %d\n", entries->readNext, entries->lastReceiveTime, entries->transmitNext, entries->lastSendTime);
	    entries++;
	}
	printf("--------------------------------------\n");
    }

    arlalib_destroyconn(connvolser);
    return 0;
}

int
main(int argc, char **argv)
{
    int ret = 0;
    
    cell_init(0);
    initports();

    if (argc > 1)
	ret = sl_command(cmds, argc - 1, argv + 1);
    else {
	vos_interactive = 1;
	printf("vos - an arla tool for administrating AFS volumes.\n");
	printf("Type \"help\" to get a list of commands.\n");
	ret = sl_loop(cmds, __progname": ");  
    }
    return ret;
}
