/*
 * Copyright (c) 1995 - 2000 Kungliga Tekniska Högskolan
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

RCSID("$arla: vos_examine.c,v 1.33 2003/03/08 02:03:12 lha Exp $");

static void
print_extended_stats (const xvolintInfo *v)
{
	printf("    File count:\t%d\n\n", v->filecount);

	printf("                      Raw Read/Write Stats\n"
	       "          |-------------------------------------------|\n"
	       "          |    Same Network     |    Diff Network     |\n"
	       "          |----------|----------|----------|----------|\n"
	       "          |  Total   |   Auth   |   Total  |   Auth   |\n"
	       "          |----------|----------|----------|----------|\n");
	printf("Reads     |%9d |%9d |%9d |%9d |\n",
	       v->stat_reads[0],
	       v->stat_reads[1],
	       v->stat_reads[2],
	       v->stat_reads[3]);
	printf("Writes    |%9d |%9d |%9d |%9d |\n",
	       v->stat_writes[0],
	       v->stat_writes[1],
	       v->stat_writes[2],
	       v->stat_writes[3]);
	printf("          |-------------------------------------------|\n");
	printf("\n");
	printf("                   Writes Affecting Authorship\n"
	       "          |-------------------------------------------|\n"
	       "          |   File Authorship   | Directory Authorship|\n"
	       "          |----------|----------|----------|----------|\n"
	       "          |   Same   |   Diff   |    Same  |   Diff   |\n"
	       "          |----------|----------|----------|----------|\n");
	printf("0-60 sec  |%9d |%9d |%9d |%9d |\n",
	       v->stat_fileSameAuthor[0], v->stat_fileDiffAuthor[0],
	       v->stat_dirSameAuthor[0],  v->stat_dirDiffAuthor[0]);
	printf("1-10 min  |%9d |%9d |%9d |%9d |\n",
	       v->stat_fileSameAuthor[1], v->stat_fileDiffAuthor[1],
	       v->stat_dirSameAuthor[1],  v->stat_dirDiffAuthor[1]);
	printf("10min-1hr |%9d |%9d |%9d |%9d |\n",
	       v->stat_fileSameAuthor[2], v->stat_fileDiffAuthor[2],
	       v->stat_dirSameAuthor[2],  v->stat_dirDiffAuthor[2]);
	printf("1hr-1day  |%9d |%9d |%9d |%9d |\n",
	       v->stat_fileSameAuthor[3], v->stat_fileDiffAuthor[3],
	       v->stat_dirSameAuthor[3],  v->stat_dirDiffAuthor[3]);
	printf("1day-1wk  |%9d |%9d |%9d |%9d |\n",
	       v->stat_fileSameAuthor[4], v->stat_fileDiffAuthor[4],
	       v->stat_dirSameAuthor[4],  v->stat_dirDiffAuthor[4]);
	printf("> 1wk     |%9d |%9d |%9d |%9d |\n",
	       v->stat_fileSameAuthor[5], v->stat_fileDiffAuthor[5],
	       v->stat_dirSameAuthor[5],  v->stat_dirDiffAuthor[5]);
	printf("          |-------------------------------------------|\n");
}

static void
print_volume (const nvldbentry *nvlentry, const xvolintInfo *v,
	      const char *server_name, int extended)
{
    char part_name[17];
    char timestr[128];

    printf("%s\t\t\t%10u %s ",
	   v->name,
	   v->volid,
	   volumetype_from_volsertype(v->type));

    if (v != NULL)
	printf("%8d K  %s\n", v->size, v->status == VOK ? "On-line" : "Busy");
    else
	printf("unknown  K\n");
    
    if (v != NULL && v->status == VOK) {
	struct tm tm;
	
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

	printf("\n    MaxQuota %10d K\n", v->maxquota);
	
	memset (&tm, 0, sizeof(tm));
	if (strftime (timestr, sizeof(timestr), "%Y-%m-%d %H:%M:%S %Z",
		      localtime_r((time_t*) &v->creationDate, &tm)) > 0)
	    printf("    Creation    %s\n", timestr);
	
	memset (&tm, 0, sizeof(tm));
	if (strftime (timestr, sizeof(timestr), "%Y-%m-%d %H:%M:%S %Z",
		      localtime_r((time_t*) &v->updateDate, &tm)) > 0)
	    printf("    Last Update %s\n", timestr);

	printf("    %d accesses in the past day (i.e., vnode references)\n",
	       v->dayUse);
    }

    if (extended && v != NULL)
	print_extended_stats (v);

    printf("\n");
}

static int
printvolstat(const char *vol, const char *cell, const char *host,
	     arlalib_authflags_t auth, int verbose, int extended)
{
    struct rx_connection *connvolser;
    int error;
    int i;
    xvolEntries xvolint;
    nvldbentry nvlentry;
    char server_name[MAXHOSTNAMELEN];
    char part_name[17];
    int was_xvol = 1;
    char volname[VLDB_MAXNAMELEN];
    int type;
    uint32_t server;
    int part;
    int bit;
    int xvolintp = 0;

    find_db_cell_and_host (&cell, &host);

    if (cell == NULL) {
	fprintf (stderr, "Unable to find cell of host '%s'\n", host);
	return -1;
    }

    if (host == NULL) {
	fprintf (stderr, "Unable to find DB server in cell '%s'\n", cell);
	return -1;
    }
	
    strlcpy (volname, vol, sizeof(volname));
    type = volname_canonicalize (volname);

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

    switch (type) {
    case RWVOL :
	bit = VLSF_RWVOL;
	break;
    case ROVOL :
	bit = VLSF_ROVOL;
	break;
    case BACKVOL :
	bit = (VLSF_BACKVOL|VLSF_RWVOL);
	break;
    default :
	abort ();
    }

    for (i = 0; i < nvlentry.nServers; ++i)
	if (nvlentry.serverFlags[i] & bit) {
	    server = nvlentry.serverNumber[i];
	    part   = nvlentry.serverPartition[i];
	    break;
	}

    if (i == nvlentry.nServers) {
	fprintf (stderr, "Volume %s does not have a %s clone\n",
		 volname, volumetype_from_volsertype(type));
	return -1;
    }

    arlalib_host_to_name (htonl(server),
			  server_name, sizeof(server_name));

    connvolser = arlalib_getconnbyaddr(cell,
				       htonl(server),
				       NULL,
				       afsvolport,
				       VOLSERVICE_ID,
				       auth);
    if (connvolser == NULL)
	return -1;
    
    if (verbose) {
	fprintf (stderr, "getting information on `%s' from %s\n",
		 vol, server_name);
    }

    xvolint.val = NULL;
    error = VOLSER_AFSVolXListOneVolume(connvolser,
					part,
					nvlentry.volumeId[type],
					&xvolint);

    if (error == RXGEN_OPCODE) {
	volEntries volint;

	was_xvol   = 0;
	volint.val = NULL;
	error = VOLSER_AFSVolListOneVolume(connvolser,
					   part,
					   nvlentry.volumeId[type],
					   &volint);
	if (error == 0) {
	    xvolint.val = emalloc (sizeof (*xvolint.val));
	    volintInfo2xvolintInfo (volint.val, xvolint.val);
	}
    }

    if (error != 0) {
	printf("ListOneVolume of %s from %s failed with: %s (%d)\n", 
	       vol, server_name,
	       koerr_gettext(error), error);
    } else {
	xvolintp = 1;
	if (verbose)
	    fprintf (stderr, "done\n");
    }


    if(xvolintp)
	print_volume (&nvlentry, xvolint.val, server_name, was_xvol && extended);
    else {
	printf("\nDump only information from VLDB\n\n");
	printf("%s\n", nvlentry.name);

	printf("    ");
	
	if(nvlentry.flags & VLF_RWEXISTS)
	    printf("RWrite: %u\t", nvlentry.volumeId[RWVOL]);
	if(nvlentry.flags & VLF_ROEXISTS)
	    printf("ROnly: %u\t", nvlentry.volumeId[ROVOL]);
	if(nvlentry.flags & VLF_BACKEXISTS)
	    printf("Backup: %u\t", nvlentry.volumeId[BACKVOL]);
    }

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
     
     if (nvlentry.flags & VLOP_ALLOPERS) {
	 char msg[100];

	 printf("Volume is currently LOCKED, reson: %s\n", 
		vol_getopname(nvlentry.flags, msg, sizeof(msg)));
     }
     
     free(xvolint.val);
     arlalib_destroyconn(connvolser);
     return 0;
}

static int helpflag;
static char *host;
static char *cell;
static char *vol;
static int noauth;
static int localauth;
static int verbose;
static int extended;

static struct agetargs args[] = {
    {"id",	0, aarg_string,  &vol,  "id of volume", "volume",
     aarg_mandatory},
    {"host",	0, aarg_string,  &host, "what host to use", NULL},
    {"cell",	0, aarg_string,  &cell, "what cell to use", NULL},
    {"noauth",	0, aarg_flag,    &noauth, "do not authenticate", NULL},
    {"localauth",0,aarg_flag,    &localauth, "localauth", NULL},
    {"verbose", 0, aarg_flag,	&verbose, "be verbose", NULL},
    {"extended",0, aarg_flag,	&extended, "more output", NULL},
    {"help",	0, aarg_flag,    &helpflag, NULL, NULL},
    {NULL,      0, aarg_end,	NULL}
};

static void
usage(void)
{
    aarg_printusage(args, "vos examine", "", AARG_AFSSTYLE);
}

int
vos_examine(int argc, char **argv)
{
    int optind = 0;

    helpflag = noauth = localauth = verbose = extended = 0;
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
		 verbose, extended);
    return 0;
}
