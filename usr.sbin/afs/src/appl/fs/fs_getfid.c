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

RCSID("$arla: fs_getfid.c,v 1.1 2001/09/24 23:50:15 mattiasa Exp $");

static void
afs_getfid(char *path)
{
    VenusFid fid;
    int ret;
    char cellname[MAXNAME];

    ret = fs_getfid(path, &fid); 
    if (ret) {
	fserr(PROGNAME, ret, path);
	return;
    }
    
    ret = fs_getfilecellname(path, cellname, sizeof(cellname));
    if (ret) {
	fserr(PROGNAME, ret, path);
	return;
    }
    
    printf("Fid: %u.%u.%u in %s (%u) \n", fid.fid.Volume,
	   fid.fid.Vnode, fid.fid.Unique, cellname, fid.Cell);
}

int
getfid_cmd(int argc, char **argv)
{
    argc--;
    argv++;

    if (argc == 0) 
	printf("%s: Missing required parameter '-path'\n", PROGNAME);
    else if (argc == 1)
	afs_getfid(*argv);
    else
	while (argc) {
	    afs_getfid(*argv);
	    argc--;
	    argv++;
	}
    return 0;
}
