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

RCSID("$arla: fs_connect.c,v 1.2 2003/03/04 13:05:08 lha Exp $");

static int
connect_usage(void)
{
    printf("connect [connected|fetch|disconnected|callback-connected]\n");
    return 0;
}

int
connect_cmd(int argc, char **argv)
{
    int ret;
    int32_t flags;

    argc--;
    argv++;

    if (argc == 0) {
	ret = fs_connect(CONNMODE_PROBE, &flags);
	if (ret) {
	    fserr(PROGNAME, ret, NULL);
	    return 0;
	}

	switch(flags) {
	case CONNMODE_CONN:
	    printf("Connected mode\n");
	    break;
	case CONNMODE_FETCH:
	    printf("Fetch only mode\n");
  	    break;
	case CONNMODE_DISCONN:
	    printf("Disconnected mode\n");
	    break;
	default:
	    printf("Unknown or error\n");
	    break;
	}
	return 0;
    }

    if (strncmp("dis", *argv, 3) == 0) 
	ret = fs_connect(CONNMODE_DISCONN, &flags);    
    else if (strncmp("fetch", *argv, 5) == 0)
        ret = fs_connect(CONNMODE_FETCH, &flags);
    else if (strncmp("conn", *argv, 4) == 0)
	ret = fs_connect(CONNMODE_CONN, &flags);
    else if (strncmp(*argv, "call", 4) == 0)
	ret = fs_connect(CONNMODE_CONN_WITHCALLBACKS, &flags);
    else
	return connect_usage();

    if (ret)
	fserr(PROGNAME, ret, NULL);

    return 0;
}
