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

RCSID("$arla: fs_getcache.c,v 1.3 2002/06/02 17:50:49 lha Exp $");

/*
 *
 */

int
getcache_cmd (int argc, char **argv)
{
    int ret, help = 0;
    int64_t max_bytes, used_bytes, max_vnodes, used_vnodes;
    int optind = 0, bytes_flag = 0;

    struct agetargs ncargs[] = {
	{"byte", 'b', aarg_flag,  
	 NULL, "show result in byte instead of kbyte", NULL},
	{"help", 'h', aarg_flag,
	 NULL, "get help", NULL},
        {NULL,      0, aarg_end, NULL}}, 
				  *arg;
			       
    arg = ncargs;
    arg->value = &bytes_flag; arg++;
    arg->value = &help; arg++;

    if (agetarg (ncargs, argc, argv, &optind, AARG_AFSSTYLE)) {
	aarg_printusage(ncargs, "getcacheparams", NULL, AARG_AFSSTYLE);
	return 0;
    }

    if (help) {
	aarg_printusage(ncargs, "getcacheparams", NULL, AARG_AFSSTYLE);
	return 0;
    }


    ret = fs_getfilecachestats (&max_bytes,
				&used_bytes,
				NULL,
				&max_vnodes,
				&used_vnodes,
				NULL);
    if (ret) {
	fserr (PROGNAME, ret, NULL);
	return 0;
    }
    if (bytes_flag) {
	printf("Arla is using %lu of the cache's available "
	       "%lu byte blocks\n",
	       (unsigned long)used_bytes, (unsigned long)max_bytes);
	if (max_vnodes)
	    printf("(and %lu of the cache's available %lu vnodes)\n",
		   (unsigned long)used_vnodes, (unsigned long)max_vnodes);
    } else {
	printf("Arla is using %lu of the cache's available "
	       "%lu 1K byte blocks\n",
	       (unsigned long)used_bytes/1024, (unsigned long)max_bytes/1024);
	if (max_vnodes)
	    printf("(and %lu of the cache's available %lu vnodes)\n",
		   (unsigned long)used_vnodes, (unsigned long)max_vnodes);
    }
    return 0;
}
