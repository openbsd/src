/*
 * Copyright (c) 2002, Stockholms Universitet
 * (Stockholm University, Stockholm Sweden)
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
 * 3. Neither the name of the university nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

#include "at_locl.h"
#include <rtbl.h>

RCSID("$arla: at_cm_whoareyou.c,v 1.5 2003/03/05 14:55:48 lha Exp $");

/*
 * Print the afs cache manager `name's interfaceAddress `addr'
 * information.
 */

static void
print_addr(const char *name, interfaceAddr *addr)
{
    char uuid[1024];
    rtbl_t tbl;
    int i;

    printf("client %s\n\n", name);

    afsUUID_to_string(&addr->uuid, uuid, sizeof(uuid));

    printf("  uuid: %s\n\n", uuid);

    if (addr->numberOfInterfaces > AFS_MAX_INTERFACE_ADDR) {
	printf("too many addresses (%d)\n\n",
	       addr->numberOfInterfaces);
	addr->numberOfInterfaces = AFS_MAX_INTERFACE_ADDR;
    } else if (addr->numberOfInterfaces == 0) {
	printf("  There is no addresses\n\n");
	return;
    } else if (addr->numberOfInterfaces == 1)
	printf("  There is one address\n\n");
    else 
	printf("  Number of addresses is %d\n\n",
	       addr->numberOfInterfaces);
    
    tbl = rtbl_create();
    
    rtbl_add_column(tbl, "Address", 0);
    rtbl_add_column(tbl, "Netmask", 0);
    rtbl_add_column(tbl, "MTU", 0);
    
    for (i = 0; i < addr->numberOfInterfaces ; i++) {
	struct in_addr in;
	char mtu[100];
	
	in.s_addr = addr->addr_in[i];
	rtbl_add_column_entry(tbl, "Address", inet_ntoa(in));
	in.s_addr = addr->subnetmask[i];
	rtbl_add_column_entry(tbl, "Netmask", inet_ntoa(in));
	snprintf(mtu, sizeof(mtu), "%d", addr->mtu[i]);
	rtbl_add_column_entry(tbl, "MTU", mtu);
    }
    
    rtbl_set_prefix (tbl, "  ");
    
    rtbl_format(tbl, stdout);
    
    rtbl_destroy(tbl);
}

static int helpflag;
static char *host = NULL;
static char *portstr = NULL;
static char *cell = NULL;
static int auth = 1;
static int helpflag = 0;


static struct getargs args[] = {
    {"port",	0, arg_string,  &portstr, "what port to use", NULL},
    {"cell",	0, arg_string,  &cell, "what cell to use", NULL},
    {"auth",	0, arg_negative_flag, &auth, "no authentication", NULL},
    {"help",	0, arg_flag,    &helpflag, NULL, NULL}
};

static void
usage(void)
{
    char helpstring[100];

    snprintf(helpstring, sizeof(helpstring), "%s whoareyou", getprogname());
    arg_printusage(args, sizeof(args)/sizeof(args[0]), helpstring, "host ...");
}

int
cm_whoareyou_cmd (int argc, char **argv)
{
    struct rx_connection *conn;
    interfaceAddr addr;
    int optind = 0;
    int ret, i;

    if (getarg (args, sizeof(args)/sizeof(args[0]), argc, argv, &optind)) {
	usage ();
	return 0;
    }

    if (helpflag) {
	usage ();
	return 0;
    }

    argc -= optind;
    argv += optind;

    if (argc == 0) {
	printf("missing host\n");
	return 0;
    }

    for (i = 0 ; i < argc; i++) {
	conn = cbgetconn(cell, argv[i], portstr, CM_SERVICE_ID, auth);
	
	ret = RXAFSCB_WhoAreYou(conn, &addr);
	if (ret == 0)
	    print_addr(host, &addr);
	else
	    printf("%s returned %s %d\n", argv[i], koerr_gettext(ret), ret);
	
	arlalib_destroyconn(conn);
	argc--; 
	argv++;
    }

    return 0;
}
