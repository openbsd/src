/*	$OpenBSD: pts.c,v 1.1.1.1 1998/09/14 21:52:53 art Exp $	*/
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
 *      Höskolan and its contributors.
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
#include <pts.h>
#include <pts.cs.h>
#include <err.h>
#include <kerberosIV/kafs.h>
#include <ctype.h>

RCSID("$KTH: pts.c,v 1.6 1998/07/30 18:36:22 rb Exp $");


static int empty_cmd(int, char **);
static int help_cmd(int, char **);
static int dump_cmd(int, char **);
static int examine_cmd(int, char **);
static int listmax_cmd(int, char **);
static int member_cmd(int, char **);
static int listowned_cmd(int, char **);

static SL_cmd cmds[] = {
    {"adduser",     empty_cmd,      "not yet implemented"},
    {"chown",       empty_cmd,      "not yet implemented"},
    {"creategroup", empty_cmd,      "not yet implemented"},
    {"cg"},
    {"createuser",  empty_cmd,      "not yet implemented"},
    {"dump",        dump_cmd,       "dump pts database"},
    {"delete",      empty_cmd,      "not yet implemented"},
    {"examine",     examine_cmd,    "examine a user or a group"},
    {"help",        help_cmd,       "get help on pts"},
    {"?"},
    {"listmax",     listmax_cmd,    "print largest uid and gid"},
    {"listowned",   listowned_cmd,      "list groups owned by a user or group, or orphaned groups"},
    {"membership",  member_cmd,     "list group or user membership"},
    {"groups"},
    {"removeuser",  empty_cmd,      "not yet implemented"},
    {"rename",      empty_cmd,      "not yet implemented"},
    {"setfields",   empty_cmd,      "not yet implemented"},
    {"setmax",      empty_cmd,      "not yet implemented"},
    {NULL}
};

static int
empty_cmd (int argc, char **argv)
{
    printf ("%s not implemented yet!\n", argv[0]);
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

void
dump_usage ()
{
    printf("Usage: pts dump <vldb server>+ [-cell <cell>]\n");
}

static int
dump_cmd (int argc, char **argv)
{
    struct rx_connection  *connptdb = NULL;
    struct prdebugentry    entry;
    struct prheader        header;
    char                  *host;
    int                    noauth = 0;
    unsigned int           pos;
    int                    error;

    argc--;
    argv++;

    if (argc < 1) {
	dump_usage ();
	return -1;
    }

    host = argv[0];

    connptdb = arlalib_getconnbyname(host,
				     afsprport,
				     PR_SERVICE_ID,
				     noauth);

    if (connptdb == NULL)
	return 0;

    error = PR_DumpEntry(connptdb, 0, (struct prdebugentry *) &header);

    if (error) {
	printf("dump_cmd: DumpEntry failed with: %s (%d)\n",
	       koerr_gettext(error),
	       error);
	return 0;
    }

    for (pos = header.headerSize; pos < header.eofPtr; pos += sizeof (struct prdebugentry)) {
	error = PR_DumpEntry(connptdb, pos, &entry);
	if (error) {
	    printf("dump_cmd: DumpEntry failed with: %s (%d)\n",
		   koerr_gettext(error),
		   error);
/*	    return -1;*/
	}
	else {
	    printf("-----\n");
	    printf("Name: %s, id: %d, owner: %d, creator: %d,\n",
		   entry.name, entry.id, entry.owner, entry.creator);
	    printf("  membership: %d, flags:", entry.ngroups);
	    if ((entry.flags & PRTYPE) == PRFREE)
		printf (" PRFREE");
	    if ((entry.flags & PRTYPE) == PRGRP)
		printf (" PRGRP");
	    if ((entry.flags & PRTYPE) == PRCONT)
		printf (" PRCONT");
	    if ((entry.flags & PRTYPE) == PRCELL)
		printf (" PRCELL");
	    if ((entry.flags & PRTYPE) == PRFOREIGN)
		printf (" PRFOREIGN");
	    if ((entry.flags & PRTYPE) == PRINST)
		printf (" PRINST");
	    if ((entry.flags & PRTYPE) == PRUSER)
		printf (" PRUSER");
	    if ((entry.flags & PRACCESS) == PRACCESS)
		printf (" PRACCESS");
	    if ((entry.flags & PRQUOTA) == PRQUOTA)
		printf (" PRQUOTA");

	    printf (" , group quota: %d.\n",
		    entry.ngroups);
	}
    }

    arlalib_destroyconn(connptdb);
    return 0;
}

void
examine_usage ()
{
    printf("Usage: pts examine <name or id of a user/group>+ [-cell <cell>] [-noauth] [-help]\n");
}


int
flags_to_string(int flags, char *string)
{
    strcpy(string, "-----");
    if((flags & PRP_STATUS_ANY) != 0)
	string[0]='S';
    else if((flags & PRP_STATUS_MEM) != 0)
	string[0]='s';
    if((flags & PRP_OWNED_ANY) != 0)
	string[1]='O';
    if((flags & PRP_MEMBER_ANY) != 0)
	string[2]='M';
    else if((flags & PRP_MEMBER_MEM) != 0)
	string[2]='m';
    if((flags & PRP_ADD_ANY) != 0)
	string[3]='A';
    else if((flags & PRP_ADD_MEM) != 0)
	string[3]='a';
    if((flags & PRP_REMOVE_MEM) != 0)
	string[4]='r';
    return 0;
}

static int
examine_id(struct rx_connection *conn, int32_t id, char *idname)
{
    prcheckentry ent;
    int res;
    namelist nlist;
    idlist ilist;
    char flags_str[6];

    res = PR_ListEntry(conn, id, &ent);
    if(res != 0) {
        if(res == PRPERM)
            errx(1, "No permissions");
        else if(res == PRNOENT) {
            errx(1, "User or group doesn't exist"
		 "; unable to find entry for %s", idname);
	}
        else if (res == PRDBBAD)
            errx(1, "An error was found relating to the PDB");
        else
            errx(1, "PR_ListEntry returned errorcode %d", res); 
    }

    nlist.len = 3;
    nlist.val = malloc(sizeof(prname) * nlist.len);
    if(nlist.val == NULL)
        errx(1, "Out of memory");

    ilist.len = 3;
    ilist.val = malloc(sizeof(int32_t) * ilist.len);
    if(ilist.val == NULL)
        errx(1, "Out of memory");

    ilist.val[0] = ent.id;
    ilist.val[1] = ent.owner;
    ilist.val[2] = ent.creator;
    
    PR_IDToName(conn, &ilist, &nlist);

    flags_to_string(ent.flags << 16, flags_str); /* XXX why do i have to shift by 16? seems strange */

    printf("----- %x\n", ent.flags);
    printf("Name: %s, id: %d, owner: %s, creator: %s,\n", 
           nlist.val[0], ent.id, nlist.val[1], nlist.val[2]);
    printf("  membership: %d, flags: %s, group quota: %d.\n",
	   ent.count, flags_str, ent.ngroups);
    free(ilist.val);
    free(nlist.val);
    return 0;
}

static int
examine_cmd (int argc, char **argv)
{
    const char *host = NULL;
    const char *cell = NULL;
    struct rx_connection *connptdb = NULL;
    namelist nlist;
    idlist ilist;
    int i = 0;
    int switches = 0;
    int noauth = 1;

    argc--; argv++; /* Get rid of command name */

    /* pts examine needs at least one argument */
    if(argc < 1) {
	examine_usage();
	return 0;
    }

    /* get rid of -nameorid that is allowed in front of usernames */
    if(strcmp(argv[0], "-nameorid") == 0) {
	argv++;
	argc--;
    }

    /* scan for switches */
    for(i = 0; i < argc; i++) {
	if(strncmp(argv[i], "-cell", 2) == 0) {
	    if(cell != NULL || (argc - 1 <= i) ) {
		examine_usage();
		return 0;
	    }		
	    switches+=2;
	    cell = argv[i+1];
	    argv[i] = NULL; /* don't examine "-cell" as a user/group */
	    argv[i+1] = NULL; /* don't examine cell name as a user/group */
	    i+=1;
	} else if(strncmp(argv[i], "-noauth", 2) == 0) {
	    noauth = 1;
	    switches++;
	}
    }

    if (cell == NULL)
	cell = cell_getthiscell();

    host = cell_findnamedbbyname (cell);

    if(host == NULL)
	errx(1, "Can't find cell %s", cell);

    connptdb = arlalib_getconnbyname(host,
				     7002,
				     PR_SERVICE_ID,
				     noauth);
    if (connptdb == NULL)
	errx(1, "Could not connect to ptserver");

    nlist.len = argc - switches;
    nlist.val = malloc(sizeof(prname) * nlist.len);
    if(nlist.val == NULL)
	errx(1, "Out of memory.");

    ilist.len = nlist.len;
    ilist.val = malloc(sizeof(int32_t)*ilist.len);
    if(ilist.val == NULL)
	errx(1, "Out of memory.");

    for(i = 0; i < argc - switches; i++) {
	while(strcmp(argv[0], "") == 0)
	    argv++;
	strncpy(nlist.val[i], argv[0], sizeof(prname));
	argv++;
    }

    PR_NameToID(connptdb, &nlist, &ilist);

    for(i = 0; i < nlist.len; i++) {
	if(ilist.val[i] == ANONYMOUSID) {
	    ilist.val[i] = atoi(nlist.val[i]);
	}
    }


    for(i = 0; i < nlist.len; i++) {
	examine_id(connptdb, ilist.val[i], nlist.val[i]);
    }

    free(nlist.val);
    free(ilist.val);

    arlalib_destroyconn(connptdb);
    return 0;
   
}

void
listmax_usage()
{
    printf("Usage: pts listmax [-cell <cell>] [-help]\n");
}

static int
listmax_cmd (int argc, char **argv)
{
    int32_t uid = 0;
    int32_t gid = 0;
    int32_t res = 0;
    const char *host = NULL;
    const char *cell = NULL;
    struct rx_connection *connptdb = NULL;

    argc--; argv++; /* Get rid of command name */

    while(argc) {
	if(strcmp(argv[0], "-cell") == 0 && argc > 0) {
	    cell = argv[1];
	    argv++;
	    argc--;
	} else {
	    listmax_usage();
	    return 0;
	}
	argv++;
	argc--;
    }

    if (cell == NULL)
	cell = cell_getthiscell();
 
    host = cell_findnamedbbyname (cell);

    connptdb = arlalib_getconnbyname(host,
				     7002,
				     PR_SERVICE_ID,
				     1); /* XXX this means no auth */
    if (connptdb == NULL)
	errx(1, "Could not connect to ptserver");

    res = PR_ListMax(connptdb, &uid, &gid);

    if (res == 0) 
	printf("Max user id is %d and max group id is %d.\n", uid, gid);
    else {
	errx(1, "PR_ListMax failed with errorcode %d", res);
    }

    arlalib_destroyconn(connptdb);
    return 0;
}

int
pts_id_to_name(struct rx_connection *conn, int id, prname *pr)
{
    int32_t res = 0;
    namelist nlist;
    idlist ilist;
    nlist.len = 1;
    nlist.val = malloc(sizeof(prname) * nlist.len);
    ilist.len = 1;
    ilist.val = malloc(sizeof(int32_t) * ilist.len);
    if((ilist.val == NULL) || (nlist.val == NULL))
	errx(1, "Out of memory");
    ilist.val[0]=id;
    res = PR_IDToName(conn, &ilist, &nlist);
    if(res == 0)
	strncpy((char *)pr, (char *)nlist.val[0], PR_MAXNAMELEN);
    free(ilist.val);
    free(nlist.val);
    return res;
}

int
pts_name_to_id(struct rx_connection *conn, char *name, int *id)
{
    int32_t res = 0;
    namelist nlist;
    idlist ilist;
    int isdig = 1;
    char *ptr = name;
    while(*ptr && isdig) {
	if(!isdigit(*ptr))
	    isdig = 0;
	ptr++;
    }
    if(isdig) {
	*id = atoi(name);
	return 0;
    }

    nlist.len = 1;
    nlist.val = malloc(sizeof(prname) * nlist.len);
    ilist.len = 1;
    ilist.val = malloc(sizeof(int32_t) * ilist.len);
    if((nlist.val == NULL) || (ilist.val == NULL))
	errx(1, "Out of memory");
    strncpy(nlist.val[0], name, sizeof(prname));
    res = PR_NameToID(conn, &nlist, &ilist);
    *id = ilist.val[0];
    free(ilist.val);
    free(nlist.val);
    return res;
}

void
listmembershipbyname(struct rx_connection *conn, char *name)
{
    int32_t res = 0;
    int32_t id = 0;
    res = pts_name_to_id(conn, name, &id);
    if(res != 0) {
	if(res == PRPERM) {
	    errx(1, "pts: Permission denied ; unable to get membership "
		 "of %s (id: %d)", name, id);
	}
	else if(res == PRNOENT)
	    errx(1, "pts: User or group doesn't exist so couldn't look up"
		 "id for %s (id: %d)", name, id);
	else 
	    errx(1, "pts: pts_name_to_id(..) returned %d", res); /* shouldnt happen */
    }
    else {
	int32_t over;
	int i;
	prlist elist;
	elist.len = PR_MAXGROUPS; /* XXX this will allocate 5000 ints, should
				  check how many groups first. That will take
				  a lot of code though. */
	elist.val = malloc(sizeof(int32_t) * elist.len);
	if(elist.val == NULL)
	    errx(1, "Out of memory");
	res = PR_ListElements(conn, id, &elist, &over);
	if(res != 0) {
	    if(res == PRPERM) {
		if(id == ANONYMOUSID) /* this sucks */
		    errx(1, "pts: User or group doesn't exist so couldn't"
			 "look up id for %s (id: %d)", name, id);
		else
		    errx(1, "pts: Permission denied ; unable to get "
			 "membership of %s (id: %d)", name, id);
	    }
	    else if(res == PRNOENT)
		errx(1, "pts: User or group doesn't exist so couldn't look up"
		     "id for %s (id: %d)", name, id);
	    else 
		errx(1, "pts: PR_ListElements(..) returned %d", res);
	} 
	else {
	    if(id>=0)
		printf("Groups %s (id: %d) is a member of:\n", name, id);
	    else
		printf("Members of %s (id: %d) are:\n", name, id);
	    for(i = 0; i < elist.len; i++) {
		prname pr;
		res = pts_id_to_name(conn, elist.val[i], &pr);
		printf("  %s\n", (char *)&pr);
	    }
	    free(elist.val);
	}
    }
    return;
}

void
member_usage()
{
    printf("Usage: pts membership [-nameorid] <users and groups>+ "
	   "[-cell <cell>] [-noauth] [-help]\n");
}

static int
member_cmd (int argc, char **argv)
{
    const char *host = NULL;
    const char *cell = NULL;
    struct rx_connection *connptdb = NULL;
    int32_t noauth = 0;
    int i = 0;
    int switches = 0;

    if(argc <= 1) {
	member_usage();
	return 0;
    }

    argc--; argv++; /* Get rid of command name */

    /* get rid of -nameorid that is allowed on commandline */
    if(strcmp("-nameorid", argv[0]) == 0) {
	argv++;
	argc--;
    }

    /* scan for switches */
    for(i = 0; i < argc; i++) {
	if(strncmp(argv[i], "-cell", 2) == 0) {
	    if(cell != NULL || (argc - 1 <= i) ) {
		member_usage();
		return 0;
	    }		
	    switches+=2;
	    cell = argv[i+1];
	    argv[i] = NULL;
	    argv[i+1] = NULL;
	    i+=1;
	} else if(strncmp(argv[i], "-noauth", 2) == 0) {
	    noauth = 1;
	    switches++;
	}
    }    

    if (cell == NULL)
	cell = cell_getthiscell();
 
    host = cell_findnamedbbyname (cell);

    connptdb = arlalib_getconnbyname(host,
				     7002,
				     PR_SERVICE_ID,
				     noauth); 
    if (connptdb == NULL)
	errx(1, "Could not connect to ptserver");
    
    for(i = 0; i < argc - switches; i++) {
	if(argv[0] != NULL)
	    listmembershipbyname(connptdb, argv[0]);
	argv++;
    }

    arlalib_destroyconn(connptdb);
    return 0;
}

void
listowned_usage()
{
    printf("Usage: pts [-id] <user or group> [-cell <cell>] [-noauth] [-help]\n");
}

static char *listowned_id;
static char *listowned_cell;
static int listowned_noauth;
static int listowned_help;

static struct getargs listowned_args[] = {
    {"id",	0, arg_string,  &listowned_id,  "id of user/group", NULL, arg_mandatory},
    {"cell",	0, arg_string,  &listowned_cell, "what cell to use", NULL},
    {"noauth",	0, arg_flag,    &listowned_noauth, "don't authenticate", NULL},
    {"help",	0, arg_flag,    &listowned_help, NULL, NULL},
    {NULL,      0, arg_end, NULL}
};

int
listowned_cmd(int argc, char **argv)
{
    int optind = 0;
    int i;
    const char *host = NULL;
    struct rx_connection *connptdb = NULL;
    prlist pr;
    int32_t over;
    int32_t res;
    int32_t id;
    prname *name;

    listowned_noauth = listowned_help = 0;
    listowned_id = listowned_cell = NULL;

    if (getarg (listowned_args, argc, argv, &optind, ARG_AFSSTYLE)) {
	listowned_usage();
	return 0;
    }

    if(listowned_help) {
	listowned_usage();
	return 0;
    }
    
    argc -= optind;
    argv += optind;
    
    if (listowned_cell == NULL)
	listowned_cell = (char *) cell_getthiscell();
 
    host = cell_findnamedbbyname (listowned_cell);

    connptdb = arlalib_getconnbyname(host,
				     7002,
				     PR_SERVICE_ID,
				     listowned_noauth); 
    if (connptdb == NULL)
	errx(1, "Could not connect to ptserver");

    res = pts_name_to_id(connptdb, listowned_id, &id);
    if(res != 0) {
	if(res == PRPERM) {
	    errx(1, "pts: Permission denied ; unable to get owner list "
		 "for %s (id: %d)", listowned_id, id);
	}
	else if(res == PRNOENT)
	    errx(1, "pts: User or group doesn't exist so couldn't look up"
		 "id for %s (id: %d)", listowned_id, id);
	else 
	    errx(1, "pts: pts_name_to_id(..) returned %d", res); /* shouldnt happen */
    }

    pr.len = PR_MAXGROUPS;
    pr.val = malloc(sizeof(int32_t) * pr.len);
    if(pr.val == NULL)
	errx(1, "Out of memory");

    res = PR_ListOwned(connptdb, id, &pr, &over);
    if(res != 0)
	errx(1, "PR_ListOwned() returned %d", res); 

    printf("id = %d\n", id);
    printf("pr.len = %d\n", pr.len);

    i = 0;
    name = malloc(PR_MAXNAMELEN);
    if(name == NULL)
	errx(1, "Out of memory");

    printf("Groups owned by %s (id: %d) are:\n", listowned_id, id);

    while(pr.val[i] != 0 && i < pr.len) {
	pts_id_to_name(connptdb, pr.val[i++], name);
	printf("  %s\n", (char *) name);
    }

    free(pr.val);
    arlalib_destroyconn(connptdb);
    return 0;
}

int
main(int argc, char **argv)
{
    int ret = 0;

    initports();
    cell_init(0); /* XXX */

    if(argc > 1) {
	ret = sl_command(cmds, argc - 1, argv + 1);
	if (ret == -1)
	    printf("%s: Unknown command\n", argv[1]);
    }
    else {
	printf("pts - an arla tool for administrating AFS users"
	       " and groups.\n");
	printf("Type \"pts help\" to get a list of commands.\n");
    }
    return ret;
}
