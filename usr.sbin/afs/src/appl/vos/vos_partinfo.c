/*
 * Copyright (c) 1995, 1996, 1997, 1998, 1999 Kungliga Tekniska Högskolan
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

RCSID("$arla: vos_partinfo.c,v 1.8 2001/01/09 01:59:08 lha Exp $");

static int
print_one_partition (struct rx_connection *conn, const char *part)
{
    int error;
    struct diskPartition partinfo ;

    error = VOLSER_AFSVolPartitionInfo(conn, part, &partinfo);
    if (error != 0) {
	printf("printpartinfo: PartitionInfo failed with: %s (%d)\n", 
	       koerr_gettext(error), error);
	return -1;
    }

    printf("Free space on partition %s %d K blocks out of total %d\n",
	   partinfo.name, 
	   partinfo.free,
	   partinfo.minFree
/* XXX -	       partinfo.totalUsable */
	);
    return 0;
}

static int
printpartinfo(const char *cell, const char *host, const char *part,
	      int verbose, arlalib_authflags_t auth)
{
    struct rx_connection *connvolser;
    char part_name[30];
    int error;
    
    find_db_cell_and_host (&cell, &host);

    connvolser = arlalib_getconnbyname(cell,
				       host,
				       afsvolport,
				       VOLSERVICE_ID,
				       auth);
    if (connvolser == NULL)
	return -1;

    if (part == NULL) {
	int i;
	part_entries parts;

	error = getlistparts (cell, host, &parts, auth);
	if (error != 0)
	    return error;

	for (i = 0; i < parts.len; ++i) {
	    partition_num2name (parts.val[i], part_name, sizeof(part_name));
	    error = print_one_partition (connvolser, part_name);
	    if (error != 0)
		return error;
	}
    } else {
	if (strlen(part) <= 2) {
	    snprintf(part_name, sizeof(part_name), "/vicep%s", part);
	    part = part_name;
	}
	error = print_one_partition (connvolser, part);
	if (error != 0)
	    return error;
    }
    
    arlalib_destroyconn(connvolser);
    return 0;
}

static int helpflag;
static char *server;
static char *cell;
static char *part;
static int noauth;
static int localauth;
static int verbose;

static struct agetargs args[] = {
    {"server",	0, aarg_string,  &server, "server", NULL, aarg_mandatory},
    {"cell",	0, aarg_string,	&cell, "cell", NULL},
    {"partition",0, aarg_string, &part, "partition", NULL},
    {"noauth",	0, aarg_flag,    &noauth, "no authentication", NULL},
    {"localauth",0,aarg_flag,    &localauth, "localauth", NULL},    
    {"verbose",	0, aarg_flag,	&verbose, "be verbose", NULL},
    {"help",	0, aarg_flag,    &helpflag, NULL, NULL},
    {NULL,      0, aarg_end, NULL}
};

static void
usage(void)
{
    aarg_printusage(args, "vos partinfo", "", AARG_AFSSTYLE);
}

int
vos_partinfo(int argc, char **argv)
{
    int optind = 0;

    helpflag = noauth = localauth = verbose = 0;
    server = cell = part = NULL;

    if (agetarg (args, argc, argv, &optind, AARG_AFSSTYLE)) {
	usage ();
	return 0;
    }
	
    if (helpflag) {
	usage ();
	return 0;
    }

    argc -= optind;
    argv += optind;

    if (argc == 1 && part == NULL) {
	part = argv[0];
	--argc;
	++argv;
    }

    if (argc != 0) {
	usage ();
	return 0;
    }

    if (server == NULL) {
	printf ("vos partinfo: you need to specify server\n");
	return 0;
    }

    printpartinfo(cell, server, part, verbose,
		  arlalib_getauthflag (noauth, localauth, 0, 0));
    return 0;
}
