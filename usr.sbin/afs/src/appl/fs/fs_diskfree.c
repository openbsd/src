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

RCSID("$arla: fs_diskfree.c,v 1.3 2003/01/24 07:25:44 lha Exp $");

static void
afs_diskfree (char *path)
{
    struct ViceIoctl      a_params;
    struct VolumeStatus  *status;

    a_params.in_size = 0;
    a_params.out_size = MAXSIZE;
    a_params.in = NULL;
    a_params.out = malloc (MAXSIZE);

    if (a_params.out == NULL) {
	printf ("fs: Out of memory\n");
	return;
    }

    if (k_pioctl (path, VIOCGETVOLSTAT, &a_params, 1) == -1) {
	fserr(PROGNAME, errno, path);
	free(a_params.out);
	return;
    }

    status = (struct VolumeStatus *) a_params.out;

    printf ("%-20s %10d %10d %10d %6.0f%%\n",
	    a_params.out + sizeof (struct VolumeStatus),
	    status->PartMaxBlocks,
	    status->PartMaxBlocks - status->PartBlocksAvail,
	    status->PartBlocksAvail,
	    (float) (status->PartMaxBlocks - status->PartBlocksAvail) / status->PartMaxBlocks * 100);

    free (a_params.out);
}

int
diskfree_cmd (int argc, char **argv)
{
    int i;

    argc--;
    argv++;

    printf ("Volume Name              kbytes       used      avail   %%used\n");

    if (argc == 0)
	afs_diskfree (".");
    else
	for (i = 0; i < argc; i++)
	    afs_diskfree (argv[i]);

    return 0;
}
