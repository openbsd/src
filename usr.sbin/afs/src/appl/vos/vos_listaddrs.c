/*
 * Copyright (c) 2002 Kungliga Tekniska Högskolan
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
#include <afs_uuid.h>
#include "vos_local.h"

RCSID("$arla: vos_listaddrs.c,v 1.4 2002/10/31 06:59:27 lha Exp $");



/*
 * list addresses
 */

static const char *cell = NULL;
static const char *hoststring = NULL;
static const char *uuidstring = NULL;
static int noauth=0;
static int localauth=0;
static int helpflag=0;
static int noresolve=0;
static int printuuid=0;

static struct agetargs args[] = {
    {"uuid",	0, aarg_string,  &uuidstring,
     "uuid of server", NULL},
    {"host",	0, aarg_string,  &hoststring,
     "address of host", NULL},
    {"cell",	0, aarg_string,  &cell, 
     "cell", NULL},
    {"noauth",	0, aarg_flag,    &noauth, 
     "do not authenticate", NULL},
    {"localauth",	0, aarg_flag,    &localauth, 
     "use local authentication", NULL},
    {"noresolve",	0, aarg_flag,    &noresolve,
     "don't resolve addresses", NULL},
    {"printuuid",	0, aarg_flag,    &printuuid,
     "print uuid of hosts", NULL},
    {"help",	0, aarg_flag,    &helpflag,
     NULL, NULL},
    {NULL,      0, aarg_end, NULL}
};

static void
usage(void)
{
    aarg_printusage (args, "vos listaddrs", "", AARG_AFSSTYLE);
}

static void
print_addrs(const bulkaddrs *blkaddrs, const afsUUID *uuid)
{
    struct in_addr addr;
    char buf[1024];
    int i;

    if (printuuid) {
	afsUUID_to_string(uuid, buf, sizeof(buf));
	printf("  UUID: %s\n", buf);
    }

    for (i=0; i < blkaddrs->len; i++) {
	if (noresolve) {
	    addr.s_addr = htonl(blkaddrs->val[i]);
	    printf("  Address: %s\n", inet_ntoa(addr));
	} else {
	    arlalib_host_to_name(htonl(blkaddrs->val[i]), buf, sizeof(buf));
	    printf("  Hostname: %s\n", buf);
	}
    }
}

int
vos_listaddrs(int argc, char **argv)
{
    struct rx_connection *connvldb = NULL;
    int32_t uniquifier = 0, nentries = 0;
    struct ListAddrByAttributes attr;
    arlalib_authflags_t auth = 0;
    const char *db_host = NULL;
    afsUUID uuid, askuuid;
    struct in_addr addr;
    bulkaddrs blkaddrs;
    int optind = 0;
    int error = 0;
    int i=0;

    memset(&attr, 0, sizeof(struct ListAddrByAttributes));
    memset(&uuid, 0, sizeof(afsUUID));
    memset(&askuuid, 0, sizeof(afsUUID));
    memset(&addr, 0, sizeof(struct in_addr));

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

    attr.Mask = VLADDR_INDEX;

    if(uuidstring) {
	afsUUID_from_string(uuidstring, &askuuid);
	attr.Mask = VLADDR_UUID;
	attr.uuid = askuuid;
    }

    if(hoststring) {
	error = arlalib_name_to_host(hoststring, &addr.s_addr);
	if(error != 0) {
	    warnx ("listaddrs: arlalib_name_to_host: %s", gai_strerror(error));
	    arlalib_destroyconn(connvldb);
	    return -1;
	}
	attr.Mask = VLADDR_IPADDR;
	attr.ipaddr = ntohl(addr.s_addr);
    }
    
    {
	VL_Callback unique;
	bulkaddrs blkaddrs;
	
	error = VL_GetAddrs(connvldb, 0, 0, &unique, &nentries, &blkaddrs);
	if (error) {
	    warnx ("listaddrs: VL_GetAddr: %d", error);
	    arlalib_destroyconn(connvldb);
	    return -1;
	}
	/* XXX need to free blkaddrs */
    }
    
    for (i = 1; i < nentries + 1; i++) {
	int32_t ne;
	attr.index = i;
	
	error = VL_GetAddrsU(connvldb, &attr,
			     &uuid, &uniquifier, &ne, &blkaddrs);
	if(error == VL_NOENT) {
	    if (hoststring || uuidstring)
		break;
	    nentries++;
	    continue;
	}
	if (error == VL_INDEXERANGE)
	    break;
	
	if (error) {
	    warnx ("listaddrs: VL_GetAddrsU: %s", koerr_gettext(error));
	    return -1;
	}
	
	printf("Host number %d:\n", i);
	
	print_addrs(&blkaddrs, &uuid);
	
	if(hoststring || uuidstring)
	    break;
	/* XXX need to free blkaddrs */
}
arlalib_destroyconn(connvldb);
    
    return 0;
}
