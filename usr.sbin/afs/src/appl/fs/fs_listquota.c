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

RCSID("$arla: fs_listquota.c,v 1.2 2003/01/28 14:34:00 lha Exp $");

static void
afs_listquota(char *path)
{
    struct ViceIoctl a_params;
    struct VolumeStatus *vs;
    char *name;
    double used_vol, used_part;

    a_params.in_size=0;
    a_params.out_size=MAXSIZE;
    a_params.in=NULL;
    a_params.out=malloc(MAXSIZE);

    if (a_params.out == NULL) {
	printf ("fs: Out of memory\n");
	return;
    }

    if(k_pioctl(path,VIOCGETVOLSTAT,&a_params,1)==-1) {
	fserr(PROGNAME, errno, path);
	free(a_params.out);
	return;
    }
  
    vs=(struct VolumeStatus *) a_params.out;
    name=a_params.out+sizeof(struct VolumeStatus);

    if (vs->MaxQuota)
	used_vol = ((double) vs->BlocksInUse / vs->MaxQuota) * 100;
    else
	used_vol = 0.0;

    if (vs->PartMaxBlocks)
	used_part = (1.0 - (double) vs->PartBlocksAvail / vs->PartMaxBlocks)
	    * 100;
    else
	used_part = 0.0;

    printf("%-20s %10d %10d%9.0f%%%s%9.0f%%%s%s\n",
	   name,
	   vs->MaxQuota,
	   vs->BlocksInUse,
	   used_vol,
	   used_vol > 90 ? "<<" : "  ",
	   used_part,
	   used_part > 97 ? "<<" : "  ",

	   /* Print a warning if more than 90% on home volume or 97% on */
	   /* the partion is being used */
	   (used_vol > 90 || used_part > 97) ? "\t<<WARNING" : "");

    free(a_params.out);
}

int
listquota_cmd (int argc, char **argv)
{
    int   i;

    argc--;
    argv++;

    printf("Volume Name               "
	   "Quota       Used    %% Used   Partition\n");

    if (argc == 0)
	afs_listquota (".");
    else
	for (i = 0; i < argc; i++)
	    afs_listquota (argv[i]);

    return 0;
}

