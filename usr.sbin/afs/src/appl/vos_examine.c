/*	$OpenBSD: vos_examine.c,v 1.1 1999/04/30 01:59:05 art Exp $	*/
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

RCSID("$KTH: vos_examine.c,v 1.5 1999/03/14 19:04:38 assar Exp $");

static void
print_extended_stats (const xvolintInfo *v)
{
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
    char timestr[30];

    printf("%s\t\t\t%10u %s %8d K  %s\n", 
	   nvlentry->name, 
	   nvlentry->volumeId[0], 
	   getvolumetype(nvlentry->serverFlags[0]),
	   v->size,
	   v->status == VOK ? "On-line" : "Busy");
    
    if (v->status == VOK) {
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
	
	/* Get and format date */
	strncpy(timestr, 
		ctime( (time_t*) &v->creationDate),
		sizeof(timestr));
	timestr[strlen(timestr) - 1] = '\0';
	printf("    Creation    %s\n", timestr);
	
	/* Get and format date */
	strncpy(timestr, 
	       ctime((time_t *) &v->updateDate),
	       sizeof(timestr));
	timestr[strlen(timestr)-1] = '\0';
	printf("    Last Update %s\n", timestr);

	printf("    %d accesses in the past day (i.e., vnode references)\n\n",
	       v->dayUse);
    }

    if (extended)
	print_extended_stats (v);
}

static int
printvolstat(const char *volname, const char *cell, const char *host,
	     arlalib_authflags_t auth, int verbose, int extended)
{
    struct rx_connection *connvolser;
    int error;
    int i;
    xvolEntries xvolint;
    nvldbentry nvlentry;
    struct in_addr server_addr;
    char server_name[MAXHOSTNAMELEN];
    char part_name[17];
    int was_xvol = 1;

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

    server_addr.s_addr = htonl(nvlentry.serverNumber[0]);
    inaddr2str (server_addr, server_name, sizeof(server_name));

    connvolser = arlalib_getconnbyaddr(cell,
				       server_addr.s_addr,
				       NULL,
				       afsvolport,
				       VOLSERVICE_ID,
				       auth);
    if (connvolser == NULL)
	return -1;
    
    if (verbose) {
	fprintf (stderr, "getting information on `%s' from %s\n",
		 volname, server_name);
    }

    xvolint.val = NULL;
    error = VOLSER_AFSVolXListOneVolume(connvolser,
					nvlentry.serverPartition[0],
					nvlentry.volumeId[0],
					&xvolint);

    if (error == RXGEN_OPCODE) {
	volEntries volint;

	was_xvol   = 0;
	volint.val = NULL;
	error = VOLSER_AFSVolListOneVolume(connvolser,
					   nvlentry.serverPartition[0],
					   nvlentry.volumeId[0],
					   &volint);
	if (error == 0) {
	    xvolint.val = emalloc (sizeof (*xvolint.val));
	    volintInfo2xvolintInfo (volint.val, xvolint.val);
	}
    }

    if (error != 0) {
	printf("ListOneVolume of %s from %s failed with: %s (%d)\n", 
	       volname, server_name,
	       koerr_gettext(error), error);
	return -1;
    }

    if (verbose) {
	fprintf (stderr, "done\n");
    }
    
    print_volume (&nvlentry, xvolint.val, server_name, was_xvol && extended);

    printf("    ");
    printf("RWrite: %u\t", nvlentry.flags & VLF_RWEXISTS ? nvlentry.volumeId[RWVOL] : 0);
    printf("ROnly: %u\t", nvlentry.flags & VLF_ROEXISTS ? nvlentry.volumeId[ROVOL] : 0);
    printf("Backup: %u\t", nvlentry.flags & VLF_BACKEXISTS ? nvlentry.volumeId[BACKVOL] : 0);

    printf("\n    number of sites -> %d\n", nvlentry.nServers );
    
     for (i = 0; i < nvlentry.nServers; i++) {
	 printf("       ");
	 
	 server_addr.s_addr = htonl(nvlentry.serverNumber[i]);
	 inaddr2str (server_addr, server_name, sizeof(server_name));

	 partition_num2name (nvlentry.serverPartition[i],
			     part_name, sizeof(part_name));
	 printf("server %s partition %s %s Site\n",
		server_name, part_name,
		getvolumetype(nvlentry.serverFlags[i]));
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

static struct getargs args[] = {
    {"id",	0, arg_string,  &vol,  "id of volume", "volume",
     arg_mandatory},
    {"host",	0, arg_string,  &host, "what host to use", NULL},
    {"cell",	0, arg_string,  &cell, "what cell to use", NULL},
    {"noauth",	0, arg_flag,    &noauth, "do not authenticate", NULL},
    {"localauth",0,arg_flag,    &localauth, "localauth", NULL},
    {"verbose", 0, arg_flag,	&verbose, "be verbose", NULL},
    {"extended",0, arg_flag,	&extended, "more output", NULL},
    {"help",	0, arg_flag,    &helpflag, NULL, NULL},
    {NULL,      0, arg_end,	NULL}
};

static void
usage(void)
{
    arg_printusage(args, "vos examine", "", ARG_AFSSTYLE);
}

int
vos_examine(int argc, char **argv)
{
    int optind = 0;

    helpflag = noauth = localauth = verbose = extended = 0;
    host = cell = vol = NULL;

    if (getarg (args, argc, argv, &optind, ARG_AFSSTYLE)) {
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
