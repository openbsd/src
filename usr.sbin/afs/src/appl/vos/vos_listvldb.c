/*
 * Copyright (c) 1999 Kungliga Tekniska Högskolan
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

#include "appl_locl.h"
#include <sl.h>
#include "vos_local.h"

RCSID("$arla: vos_listvldb.c,v 1.17 2003/06/04 11:54:13 hin Exp $");

/*
 * listvldb iteration over all entries in the DB
 */

int
vos_listvldb_iter (const char *db_host, const char *cell, const char *volname,
		   const char *fileserver, const char *part,
		   arlalib_authflags_t auth,
		   int (*proc)(void *data, struct vldbentry *), void *data)
{
    struct rx_connection *connvldb = NULL;
    struct vldbentry *entry;
    struct vldbentry vol;
    int error;
    int32_t num;
    bulkentries bulkent;
    struct VldbListByAttributes attr;
    struct hostent *he;

    memset(&attr, '\0', sizeof(struct VldbListByAttributes));

    find_db_cell_and_host (&cell, &db_host);

    if (cell == NULL) {
	fprintf (stderr, "Unable to find cell of host '%s'\n", db_host);
	return -1;
    }

    if (db_host == NULL) {
	fprintf (stderr, "Unable to find DB server in cell '%s'\n", cell);
	return -1;
    }
	
    connvldb = arlalib_getconnbyname(cell, db_host,
				     afsvldbport,
				     VLDB_SERVICE_ID,
				     auth);

    if(volname != NULL) {

	error = VL_GetEntryByName(connvldb, volname, &vol);
	if (error) {
	    fprintf(stderr, "vos_listvldb: error %s (%d)\n",
		    koerr_gettext(error), error);
	    return -1;
	}

	attr.volumeid = vol.volumeId[vol.volumeType]; 
	attr.Mask |= VLLIST_VOLUMEID;

    }

    if(fileserver != NULL) {
	he = gethostbyname(fileserver);
	if (he == NULL) {
	    warnx("listvldb: unknown host: %s", fileserver);
	    return -1;
	}
	memcpy (&attr.server, he->h_addr_list[0], 4);
	attr.server = ntohl(attr.server);
	attr.Mask |= VLLIST_SERVER;
    }

    if(part != NULL) {
	attr.partition = partition_name2num(part);
	attr.Mask |= VLLIST_PARTITION;
    }

    bulkent.val = NULL;
    bulkent.len = 0;
    num = 0;
    error = VL_ListAttributes (connvldb, 
			       &attr,
			       &num,
			       &bulkent);
    if (error == 0) {
	int i;

	for(i=0; i<num ; i++) {
	    
	    entry = &bulkent.val[i];
	    error = proc (data, entry);
	    if (error)
		break;
	    
	    if (error) {
		warnx ("listvldb: VL_ListEntry: %s", koerr_gettext(error));
		return -1;
	    }
	}
	free(bulkent.val);
    } else
        warnx ("listvldb: VL_ListAttributes: %s", koerr_gettext(error));


    arlalib_destroyconn(connvldb);
    return 0;
}

/*
 * Print vldbentry on listvldb style
 */

static int
listvldb_print (void *data, struct vldbentry *e)
{
    int i;
    char *hostname;
    char part_name[30];

    assert (e);

    printf ("%s\n", e->name);
    if(e->flags & VLF_RWEXISTS)
	printf ("    RWrite: %d", e->volumeId[RWVOL]);
    if(e->flags & VLF_ROEXISTS)
	printf ("    ROnly: %d", e->volumeId[ROVOL]);
    if(e->flags & VLF_BACKEXISTS)
	printf ("    Backup: %d", e->volumeId[BACKVOL]);
    printf("\n");
    printf ("    Number of sites -> %d\n", e->nServers);
    for (i = 0 ; i < e->nServers ; i++) {
	if (e->serverNumber[i] == 0)
	    continue;
	if (arlalib_getservername(htonl(e->serverNumber[i]), &hostname))
	    continue;
	partition_num2name (e->serverPartition[i], part_name, sizeof(part_name));
	printf ("       server %s partition %s %s Site\n",
		hostname,
		part_name,
		volumetype_from_serverflag (e->serverFlags[i]));
	free (hostname);
    }

    printf ("\n");
    
    return 0;
}


/*
 * list vldb
 */

static char *volume;
static char *fileserver;
static char *partition;
static char *cell;
static char *dbserver;
static int noauth;
static int localauth;
static int helpflag;

static struct agetargs args[] = {
    {"name",           0, aarg_string, &volume,
     "Volume to list", NULL, aarg_optional_swless},
    {"fileserver",	0, aarg_string,  &fileserver,  
     "fileserver to list", NULL, aarg_optional_swless},
    {"partition",	0, aarg_string,  &partition,  
     "partition", NULL, aarg_optional_swless},
    {"cell",	0, aarg_string,  &cell, 
     "cell", NULL},
    {"dbserver",	0, aarg_string,  &dbserver,  
     "dbserver", NULL},
    {"noauth",	0, aarg_flag,    &noauth, 
     "do not authenticate", NULL},
    {"localauth",	0, aarg_flag,    &localauth, 
     "use local authentication", NULL},
    {"help",	0, aarg_flag,    &helpflag,
     NULL, NULL},
    {NULL,      0, aarg_end, NULL}
};

static void
usage(void)
{
    aarg_printusage (args, "vos listvldb", "", AARG_AFSSTYLE);
}

int
vos_listvldb(int argc, char **argv)
{
    int optind = 0;

    volume = fileserver = dbserver = cell = NULL;
    noauth = localauth = helpflag = 0;

    if (agetarg (args, argc, argv, &optind, AARG_AFSSTYLE)) {
	usage();
	return 0;
    }

    if(helpflag) {
	usage();
	return 0;
    }
    
    argc -= optind;
    argv += optind;


    vos_listvldb_iter (dbserver, cell, volume,
		       fileserver, partition,
		       arlalib_getauthflag (noauth, localauth, 0, 0),
		       listvldb_print, NULL);
    
    return 0;
}

