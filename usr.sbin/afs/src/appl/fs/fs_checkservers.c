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

RCSID("$arla: fs_checkservers.c,v 1.2 2002/02/07 17:58:19 lha Exp $");

int
checkservers_cmd (int argc, char **argv)
{
    char *cell = NULL;
    int flags = 0;
    int nopoll = 0;
    int optind = 0;
    uint32_t hosts[CKSERV_MAXSERVERS + 1];
    int ret;
    int i;

    struct agetargs cksargs[] = {
	{"cell",	0, aarg_string,  NULL, "cell", NULL},
	{"nopoll",	0, aarg_flag,    NULL, "dont ping each server, "
	                                       "use internal info", NULL},
	{NULL,      0, aarg_end, NULL}}, 
					 *arg;

    arg = cksargs;
    arg->value = &cell;   arg++;
    arg->value = &nopoll;   arg++;

    if (agetarg (cksargs, argc, argv, &optind, AARG_AFSSTYLE)) {
	aarg_printusage(cksargs, "checkservers", NULL, AARG_AFSSTYLE);
	return 0;
    }

    if (cell)
	flags |= CKSERV_FSONLY;
    if (nopoll)
	flags |= CKSERV_DONTPING;
    
    ret = fs_checkservers(cell, flags, hosts, sizeof(hosts)/sizeof(hosts[0]));
    if (ret) {
	if (ret == ENOENT)
	    fprintf (stderr, "%s: cell `%s' doesn't exist\n",
		     PROGNAME, cell);
	else
	    fserr (PROGNAME, ret, NULL);
	return 0;
    }

    if (hosts[0] == 0)
	printf ("All servers are up");

    for (i = 1; i < min(CKSERV_MAXSERVERS, hosts[0]) + 1; ++i) {
	if (hosts[i]) {
	    struct hostent *he;

	    he = gethostbyaddr ((char *)&hosts[i], sizeof(hosts[i]), AF_INET);

	    if (he != NULL) {
		printf ("%s ", he->h_name);
	    } else {
		struct in_addr in;

		in.s_addr = hosts[i];
		printf ("%s ", inet_ntoa(in));

	    }
	}
    }
    printf("\n");
    return 0;
}
