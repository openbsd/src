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
#include "vos_local.h"

RCSID("$arla: vos_createvolume.c,v 1.7 2002/04/10 15:35:51 joda Exp $");

/*
 * create volume
 */


static int
vos_createvolume (char *host, int32_t part, char *cell, 
		  arlalib_authflags_t auth,
		  char *name, int quota, int verbose)
{
    struct rx_connection *connvldb = NULL;
    struct rx_connection *volser = NULL;
    nvldbentry entry;
    int32_t dbhost;
    int32_t fshost;
    int32_t volid;
    int32_t rcode;
    int32_t trans; /* transaction id */
    int error;

    if (host == NULL && name == NULL)
	return EINVAL;

    if (cell == NULL)
	cell = (char *)cell_getthiscell();

    error = arlalib_getsyncsite (cell, NULL, afsvldbport,
				 &dbhost, auth);
    if (error) {
	fprintf (stderr, "vos_createvolume: arla_getsyncsite: %s\n", 
	       koerr_gettext(error));
	return -1;
    }
    
    connvldb = arlalib_getconnbyaddr(cell, dbhost, NULL,
				     afsvldbport,
				     VLDB_SERVICE_ID,
				     auth);
    if (connvldb == NULL) {
	fprintf (stderr,
		 "vos_createvolume: arlalib_getconnbyaddr: vldb-host: 0x%x\n",
	       dbhost);
	return -1;
    }

    volser = arlalib_getconnbyname (cell, host,
				    afsvolport,
				    VOLSERVICE_ID,
				    auth);
    if (volser == NULL) {
	fprintf (stderr,"vos_createvolume: arlalib_getconnbyname: volser: %s\n",
	       host);
	arlalib_destroyconn (connvldb);
	return -1;
    }

    fshost = rx_HostOf(rx_PeerOf(volser));
    if (fshost == 0) {
	fprintf (stderr, "vos_createvolume: address of 0 is not permited\n");
	goto errout;
    }

    /*
     * Get three (3) new Id's from the vldb server's
     */

    error = VL_GetNewVolumeId (connvldb, 3, &volid);
    if (error) {
	fprintf (stderr, "vos_createvolume: VL_GetNewVolumeID: %s\n", 
	       koerr_gettext (error));
	goto errout;
    }
    if (verbose)
	printf ("vos_createvolume: got a VolumeID: %d\n", volid);
	
    /*
     * Create new volume on the server
     */

    error = VOLSER_AFSVolCreateVolume (volser, part, name,
				       RWVOL,
				       /* parent */ 0, &volid,
				       &trans);
    if (error) {
	fprintf (stderr, "vos_createvolume: VOLSER_AFSVolCreateVolume: %s\n", 
	       koerr_gettext (error));
	goto errout;
    }
    if (verbose)
	printf ("vos_createvolume: created volume on %s, got trans %d\n", 
		host, trans);
	
    /*
     * Set quota
     */
    if(quota != 0) {
	struct volintInfo volinfo;
	memset(&volinfo, 0, sizeof(volinfo));
	volinfo.dayUse = -1;
	volinfo.maxquota = quota;
	error = VOLSER_AFSVolSetInfo (volser, trans, &volinfo);
	if (error) {
	    fprintf (stderr, "vos_createvolume: VOLSER_AFSVolSetInfo: %s (continuing)\n", 
		     koerr_gettext (error));
	}
    }

    /*
     * Bring the volume on-line
     */

    error = VOLSER_AFSVolSetFlags(volser, trans, 0);
    if (error) {
	fprintf (stderr, "vos_createvolume: VOLSER_AFSVolSetFlags: %s\n", 
	       koerr_gettext (error));
	goto errout;
    }
    if (verbose)
	printf  ("vos_createvolume: updated flag for trans %d\n", trans);    

    /* 
     * Create vldb-entry for the volume, if that failes, remove the volume
     * from the server.
     */

    memset (&entry, 0, sizeof (entry));
    strlcpy (entry.name, name, sizeof(entry.name));
    entry.nServers = 1;
    entry.serverNumber[0] = ntohl(fshost);
    entry.serverPartition[0] = part;
    entry.serverFlags[0] = VLSF_RWVOL;
    entry.volumeId[RWVOL] = volid;
    entry.volumeId[ROVOL] = volid+1;
    entry.volumeId[BACKVOL] = volid+2;
    entry.cloneId = 0;
    entry.flags = VLF_RWEXISTS;

    error = new_vlentry (connvldb, NULL, NULL, &entry, auth);
    if (error) {
	fprintf (stderr, "vos_createvolume: new_vlentry: %s\n", 
	       koerr_gettext (error));

	fprintf (stderr, "vos_createvolume: removing new volume");
	error = VOLSER_AFSVolDeleteVolume (volser, trans);
	if (error)
	    fprintf (stderr, "vos_createvolume: failed to remove volume: %s\n", 
		     koerr_gettext (error));
	else
	    fprintf (stderr, "vos_createvolume: removed volume\n");
	
    } else if (verbose)
	printf  ("vos_createvolume: added entry to vldb\n");
    
    /*
     * End transaction to volumeserver
     */

 errout:
    error = VOLSER_AFSVolEndTrans(volser, trans, &rcode);
    if (error) {
	fprintf (stderr, 
		 "vos_createvolume: VOLSER_AFSVolEndTrans: %s, rcode: %d\n", 
		 koerr_gettext (error),
		 rcode);
	return -1;
    }
    if (verbose)
	printf  ("vos_createvolume: ending transaction %d, rcode %d\n", 
		 trans, rcode);    

    arlalib_destroyconn(volser);
    arlalib_destroyconn(connvldb);
    return error;
}

/*
 * list vldb
 */

static char *server;
static char *part;
static char *volume;
static int maxquota;
static char *cell;
static int noauth;
static int localauth;
static int helpflag;
static int verbose;

static struct agetargs args[] = {
    {"server",	0, aarg_string,  &server,  
     "server", NULL, aarg_mandatory},
    {"part",	0, aarg_string,  &part,  
     "part", NULL, aarg_mandatory},
    {"volume",	0, aarg_string,  &volume,  
     "volume", NULL, aarg_mandatory},
    {"maxquota",0, aarg_integer,  &maxquota,  
     "initial quota", NULL, aarg_optional_swless },
    {"cell",	0, aarg_string,  &cell, 
     "cell", NULL},
    {"noauth",	0, aarg_flag,    &noauth, 
     "do not authenticate", NULL},
    {"localauth",	0, aarg_flag,    &localauth, 
     "use local authentication", NULL},
    {"verbose",	0, aarg_flag,    &verbose, 
     "verbose output", NULL},
    {"help",	0, aarg_flag,    &helpflag,
     NULL, NULL},
    {NULL,      0, aarg_end, NULL}
};

/*
 * print usage
 */

static void
usage(void)
{
    aarg_printusage (args, "vos create", "", AARG_AFSSTYLE);
}

/*
 * createvolume, to be called from libsl
 */

int
vos_create(int argc, char **argv)
{
    int optind = 0;
    int npart = -1;
    int error;

    server = cell = NULL;
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

    if (argc > 0) {
	fprintf (stderr, "create volume: unparsed arguments\n");
	return 0;
    }

    npart = partition_name2num (part);

    error = vos_createvolume (server, npart, cell, 
			      arlalib_getauthflag (noauth, localauth, 0, 0),
			      volume, maxquota, verbose);
    if (error) {
	fprintf (stderr, "vos_createvolume failed (%d)\n", error);
	return 0;
    }

    return 0;
}

