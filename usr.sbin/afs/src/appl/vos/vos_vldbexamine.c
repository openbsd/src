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

#include "appl_locl.h"
#include <sl.h>
#include "vos_local.h"

RCSID("$arla: vos_vldbexamine.c,v 1.16 2002/07/09 16:13:00 lha Exp $");

static void
print_volume (const nvldbentry *nvlentry, const char *server_name)
{
    char part_name[17];

    printf("%s\t\t\t%10u %s\n", 
	   nvlentry->name, 
	   nvlentry->volumeId[0], 
	   volumetype_from_serverflag(nvlentry->serverFlags[0]));
    
    partition_num2name (nvlentry->serverPartition[0],
			part_name, sizeof(part_name));

    printf("    %s %s\n", server_name, part_name);
    printf("    ");
	
    if (nvlentry->flags & VLF_RWEXISTS)
	printf("RWrite %u\t", nvlentry->volumeId[RWVOL]);
    
    if (nvlentry->flags & VLF_ROEXISTS )
	printf("ROnly %u\t", nvlentry->volumeId[ROVOL]);
    
    if (nvlentry->flags & VLF_BACKEXISTS)
	printf("Backup %u\t", nvlentry->volumeId[BACKVOL]);

}
	

static int
printvolstat(const char *volname, const char *cell, const char *host,
	     arlalib_authflags_t auth, int verbose)
{
    int error;
    int i;
    nvldbentry nvlentry;
    char server_name[MAXHOSTNAMELEN];
    char part_name[17];

    find_db_cell_and_host (&cell, &host);

    if (cell == NULL) {
	fprintf (stderr, "Unable to find cell of host '%s'\n", host);
	return -1;
    }

    if (host == NULL) {
	fprintf (stderr, "Unable to find DB server in cell '%s'\n", cell);
	return -1;
    }
	
    if (verbose)
	fprintf (stderr,
		 "Getting volume `%s' from the VLDB at `%s'...",
		 volname, host);

    error = get_vlentry (cell, host, volname, auth, &nvlentry);

    if (error) {
	fprintf(stderr, "VL_GetEntryByName(%s) failed: %s\n",
		volname, koerr_gettext(error));
	return -1;
    }

    if (verbose)
	fprintf (stderr, "done\n");

    arlalib_host_to_name (htonl(nvlentry.serverNumber[0]),
			  server_name, sizeof(server_name));
    print_volume (&nvlentry, server_name);

    printf("\n    number of sites -> %d\n", nvlentry.nServers );
    
     for (i = 0; i < nvlentry.nServers; i++) {
	 printf("       ");
	 
	 arlalib_host_to_name (htonl(nvlentry.serverNumber[i]),
			       server_name, sizeof(server_name));

	 partition_num2name (nvlentry.serverPartition[i],
			     part_name, sizeof(part_name));
	 printf("server %s partition %s %s Site",
		server_name, part_name,
		volumetype_from_serverflag(nvlentry.serverFlags[i]));


	 if (nvlentry.serverFlags[i] & VLSF_DONTUSE)
	     printf(" -- not replicated yet");

	 printf("\n");

     }
     
     return 0;
}

static int helpflag;
static char *host;
static char *cell;
static char *vol;
static int noauth;
static int localauth;
static int verbose;

static struct agetargs args[] = {
    {"id",	0, aarg_string,  &vol,  "id of volume", "volume",
     aarg_mandatory},
    {"host",	0, aarg_string,  &host, "what host to use", NULL},
    {"cell",	0, aarg_string,  &cell, "what cell to use", NULL},
    {"noauth",	0, aarg_flag,    &noauth, "do not authenticate", NULL},
    {"localauth",0,aarg_flag,    &localauth, "localauth", NULL},
    {"verbose", 0, aarg_flag,	&verbose, "be verbose", NULL},
    {"help",	0, aarg_flag,    &helpflag, NULL, NULL},
    {NULL,      0, aarg_end,	NULL}
};

static void
usage(void)
{
    aarg_printusage(args, "vos vldbexamine", "", AARG_AFSSTYLE);
}

int
vos_vldbexamine(int argc, char **argv)
{
    int optind = 0;

    helpflag = noauth = localauth = verbose = 0;
    host = cell = vol = NULL;

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

    if (argc) {
	printf("unknown option %s\n", *argv);
	return 0;
    }

    /* don't allow any bogus volname */
    if (vol == NULL || vol[0] == '\0') {
	usage ();
	return 0;
    }

    printvolstat(vol, cell, host, 
		 arlalib_getauthflag (noauth, localauth, 0, 0), 
		 verbose);
    return 0;
}
