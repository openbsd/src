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

RCSID("$arla: fs_examine.c,v 1.1 2001/09/24 23:50:13 mattiasa Exp $");

static void
afs_examine (char *path)
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

    printf ("Volume status for vid = %d named %s\n", status->Vid, 
	    a_params.out + sizeof (struct VolumeStatus));
    printf ("Current disk quota is %d\n", status->MaxQuota);
    printf ("Current blocks used are %d\n", status->BlocksInUse);
    printf ("The partition has %d blocks available out of %d\n\n", status->PartBlocksAvail, status->PartMaxBlocks);

    free (a_params.out);
}

int
examine_cmd (int argc, char **argv)
{
    int   i;

    argc--;
    argv++;

    if (argc == 0)
	afs_examine (".");
    else
	for (i = 0; i < argc; i++)
	    afs_examine (argv[i]);

    return 0;
}
