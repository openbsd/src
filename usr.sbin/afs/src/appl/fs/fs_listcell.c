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

#include "fs_local.h"

RCSID("$arla: fs_listcell.c,v 1.2 2002/02/07 17:58:23 lha Exp $");

/* 
 * List all the known cells, with servers iff printservers, resolving
 * IP addresses to names iff resolve and printing suid status iff
 * suid.
 */

static int
afs_listcells (int printservers, int resolve, int suid)
{
    struct in_addr     addr;
    int		       i, j;
    char               cellname[MAXSIZE];
    uint32_t	       servers[8];
    int 	       ret;
    unsigned	       max_servers = sizeof(servers)/sizeof(servers[0]);

    for (i = 1;
	 (ret = fs_getcells (i, servers, 
			     max_servers,
			     cellname, sizeof (cellname))) == 0;
	 ++i) {
	printf ("%s", cellname);

	if (printservers) {
	    printf (": ");

	    for (j = 0; j < max_servers && servers[j]; ++j) {
		struct hostent  *h = NULL;
		addr.s_addr = servers[j];
		if (resolve)
		    h = gethostbyaddr ((const char *) &addr, 
				       sizeof(addr), 
				       AF_INET);
		if (h == NULL) {
		    printf (" %s", inet_ntoa (addr));
		} else {
		    printf (" %s", h->h_name);
		}
	    }
	}
	if (suid) {
	    uint32_t status;

	    ret = fs_getcellstatus (cellname, &status);
	    if (ret)
		fserr (PROGNAME, ret, NULL);
	    else {
		if (status & CELLSTATUS_SETUID)
		    printf (", suid cell");
	    }
	}
	printf (".\n");
    }

    if (errno != EDOM)
	fserr(PROGNAME, errno, NULL);

    return 0;
}

int
listcells_cmd (int argc, char **argv)
{
    int printhosts = 1;
    int resolve = 1;
    int printsuid = 0;
    int optind = 0;

    struct agetargs lcargs[] = {
	{"servers", 's', aarg_negative_flag,  
	 NULL,"do not print servers in cell", NULL},
	{"resolve",	 'r', aarg_negative_flag,   
	 NULL,"do not resolve hostnames", NULL},
        {"suid",	 'p', aarg_flag,   
	 NULL,"print if cell is suid", NULL },
        {NULL,      0, aarg_end, NULL}}, 
				  *arg;

    arg = lcargs;
    arg->value = &printhosts;   arg++;
    arg->value = &resolve;     arg++;
    arg->value = &printsuid;   arg++;

    if (agetarg (lcargs, argc, argv, &optind, AARG_AFSSTYLE)) {
	aarg_printusage(lcargs, "listcells", NULL, AARG_AFSSTYLE);
	return 0;
    }

    printf ("%d %d %d\n", printhosts, resolve, printsuid);
    
    afs_listcells (printhosts,resolve,printsuid);

    return 0;
}

int
suidcells_cmd (int argc, char **argv)
{
    afs_listcells (0, 0, 1);

    return 0;
}

