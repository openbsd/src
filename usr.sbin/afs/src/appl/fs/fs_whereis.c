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

RCSID("$arla: fs_whereis.c,v 1.1 2001/09/24 23:50:22 mattiasa Exp $");

static void
afs_whereis(char *path)
{
    struct ViceIoctl a_params;
    struct in_addr addr;
    int32_t *curptr;
    int i=0;

    a_params.in_size=0;
    a_params.out_size=8*sizeof(int32_t);
    a_params.in=NULL;
    a_params.out=malloc(8*sizeof(int32_t));

    if(a_params.out == NULL) {
	printf ("fs: Out of memory\n");
	return;
    }

    if(k_pioctl(path,VIOCWHEREIS,&a_params,1)==-1) {
	fserr(PROGNAME, errno, path);
	free(a_params.out);
	return;
    }

    curptr=(int32_t *) a_params.out;
    printf("File %s is on host%s", path, curptr[0]&&curptr[1]?"s":"");

    while(curptr[i] && i<8) {
	struct hostent *h;
	addr.s_addr = curptr[i];
	h=gethostbyaddr((const char *) &addr, sizeof(addr), AF_INET);
	if (h == NULL)
	    printf (" %s", inet_ntoa (addr));
	else {
	    printf(" %s", h->h_name);
	}
	i++;
    }
    printf("\n");
    free(a_params.out);
}

int
whereis_cmd (int argc, char **argv)
{
    argc--;
    argv++;
    if (argc == 0)
	afs_whereis (".");
    else
	afs_whereis (argv[0]);

    return 0;
}

