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

RCSID("$arla: fs_newcell.c,v 1.1 2001/09/24 23:50:18 mattiasa Exp $");

int
newcell_cmd (int argc, char **argv)
{
    char *cell = NULL;
    agetarg_strings servers = { 0, NULL };
    int ret, help = 0;
    int optind = 0;

    struct agetargs ncargs[] = {
	{"cell", 'c', aarg_string,  
	 NULL, "new cell", NULL, aarg_mandatory},
	{"servers", 's', aarg_strings,
	 NULL, "server in cell", "one server"},
	{"help", 'h', aarg_flag,
	 NULL, "get help", NULL},
        {NULL,      0, aarg_end, NULL}}, 
				  *arg;
			       
    arg = ncargs;
    arg->value = &cell; arg++;
    arg->value = &servers; arg++;
    arg->value = &help; arg++;

    if (agetarg (ncargs, argc, argv, &optind, AARG_AFSSTYLE)) {
	aarg_printusage(ncargs, "newcell", NULL, AARG_AFSSTYLE);
	return 0;
    }

    if (help) {
	aarg_printusage(ncargs, "newcell", NULL, AARG_AFSSTYLE);
	goto out;
    }

    if (servers.num_strings == 0) {
	fprintf (stderr, "You didn't give servers, will use DNS\n");
	goto out;
    }

    ret = fs_newcell (cell, servers.num_strings, servers.strings);
    if (ret)
	fprintf (stderr, "fs_newcell failed with %s (%d)\n",
		 koerr_gettext(ret), ret);
    
 out:
    if (servers.strings)
	free (servers.strings);

    return 0;
}

