/*
 * Copyright (c) 1995, 1996, 1997, 1998, 1999, 2000, 2001 Kungliga
 * Tekniska Högskolan (Royal Institute of Technology, Stockholm,
 * Sweden). All rights reserved.
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

RCSID("$arla: vos_zap.c,v 1.1 2001/09/28 15:02:19 ahltorp Exp $");

/*
 * zap volume on a afs-server
 */

static char *server;
static char *partition;
static int32_t id;
static char *cell;
static int noauth;
static int localauth;
static int helpflag;
static int force;

static struct agetargs args[] = {
    {"server",	0, aarg_string,  &server,  
     "server name", NULL, aarg_mandatory},
    {"partition", 0, aarg_string, &partition,
     "partition name", NULL, aarg_mandatory},
    {"id", 0, aarg_integer, &id,
     "volume ID", NULL, aarg_mandatory},
    {"force",		0, aarg_flag,	&force,
     "force", NULL},
    {"cell",	0, aarg_string,  &cell, 
     "cell", NULL},
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
    aarg_printusage (args, "vos zap", "", AARG_AFSSTYLE);
}

/*
 * delete the volume at the volserver
 */

static int
zapvol(struct rx_connection *connvolser,
       int32_t volume,
       int32_t part)
{
    int error;
    int delete_error;
    int32_t trans;
    int32_t rcode;

    error = VOLSER_AFSVolTransCreate(connvolser,
				     volume, part, ITOffline,
				     &trans);
    if (error)
	return error;

    delete_error = VOLSER_AFSVolDeleteVolume(connvolser, trans);

    error = VOLSER_AFSVolEndTrans(connvolser, trans, &rcode);

    if (delete_error)
	return delete_error;

    if (error)
	return error;

    if (rcode)
	return rcode;

    return 0;
}


int
vos_zap(int argc, char **argv)
{
    struct rx_connection *connvolser;
    int optind = 0;
    int error;
    int part;

    server = partition = cell = NULL;
    noauth = localauth = helpflag = force = 0;

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

    if (server == NULL) {
	usage();
	return 0;
    }

    if (partition == NULL) {
	usage();
	return 0;
    } else {
	part = partition_name2num(partition);
	if (part == -1) {
	    usage();
	    return 0;
	}
    }

    connvolser = arlalib_getconnbyname(cell, server, afsvolport,
				       VOLSERVICE_ID,
		 arlalib_getauthflag (noauth, 0, 0, 0));

    if (connvolser == NULL) {
	printf("vos zap: failed to contact volser on host %s",
	       server);
	return -1 ;
    }

    error = zapvol(connvolser, id, part);
    if (error) {
	printf("vos zap: zapvol failed with: %s (%d)\n", 
	       koerr_gettext(error), error);
	return -1;
    }

    arlalib_destroyconn(connvolser);

    return 0;
}
