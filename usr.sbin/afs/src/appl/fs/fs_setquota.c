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

RCSID("$arla: fs_setquota.c,v 1.1 2001/09/24 23:50:21 mattiasa Exp $");

static void
afs_setmaxquota(char *path, int32_t maxquota)
{
    struct ViceIoctl a_params;
    struct VolumeStatus *vs;
    int insize;

    a_params.in_size=0;
    a_params.out_size=MAXSIZE;
    a_params.in=NULL;
    a_params.out=malloc(MAXSIZE);

    if (a_params.out == NULL) {
	printf ("fs: Out of memory\n");
	return;
    }

    /* Read the old volume status */
    if(k_pioctl(path,VIOCGETVOLSTAT,&a_params,1)==-1) {
	fserr(PROGNAME, errno, path);
	free(a_params.out);
	return;
    }

    insize=sizeof(struct VolumeStatus)+strlen(path)+2;

    a_params.in_size=MAXSIZE<insize?MAXSIZE:insize;
    a_params.out_size=0;
    a_params.in=a_params.out;
    a_params.out=NULL;
  
    vs=(struct VolumeStatus *) a_params.in;
    vs->MaxQuota=maxquota;

    if(k_pioctl(path,VIOCSETVOLSTAT,&a_params,1)==-1) {
	fserr(PROGNAME, errno, path);
	free(a_params.in);
	return;
    }

    free(a_params.in);
}

int
setquota_cmd (int argc, char **argv)
{
    char *path = NULL;
    int quota = 0;
    int helpflag = 0;
    int optind = 0;
    
    struct agetargs sqargs[] = {
	{"path", 0, aarg_string,  NULL,  "pathname to file/directory",
	 "pathname", aarg_mandatory},
	{"max",  0, aarg_integer, NULL, "max quota in kbytes",
	 "kbytes",   aarg_mandatory},
	{"help",    0, aarg_flag, NULL },
        {NULL,      0, aarg_end, NULL}}, *arg;

    arg = sqargs;
    arg->value = &path; arg++;
    arg->value = &quota; arg++;
    arg->value = &helpflag; arg++;

    if (agetarg (sqargs, argc, argv, &optind, AARG_AFSSTYLE)) {
	aarg_printusage(sqargs, "fs setquota", NULL, AARG_AFSSTYLE);
	return 0;
    }

    if (helpflag) {
	aarg_printusage(sqargs, "fs setquota", NULL, AARG_AFSSTYLE);
	return 0;
    }
    argc -= optind;
    argv += optind;

    if (argc) {
	printf("unknown option %s\n", *argv);
	return 0;
    }
 
    afs_setmaxquota(path, quota);
 
    return 0;
}

