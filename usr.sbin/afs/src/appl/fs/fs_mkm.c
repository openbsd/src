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

RCSID("$arla: fs_mkm.c,v 1.1 2001/09/24 23:50:18 mattiasa Exp $");

int
mkmount_cmd (int argc, char **argv)
{
    char  buf[MAXSIZE];
    char prefix;
    char *dirname = NULL;
    char *volname = NULL;
    char *cell = NULL;
    int rwflag = 0;
    int helpflag = 0;
    int optind = 0;

    struct agetargs mkmargs[] = {
	{"dir",     0, aarg_string, NULL, "mount point directory name",
	 "directory", aarg_mandatory},
	{"vol",     0, aarg_string, NULL, "volume to mount",
	 "volume",    aarg_mandatory},
	{"cell",    0, aarg_string, NULL, "cell of volume",
	 "cell",      aarg_optional_swless},
	{"rw",      0, aarg_flag, NULL, "mount read-write", NULL},
	{"help",    0, aarg_flag, NULL, NULL, NULL},
	{NULL,      0, aarg_end,  NULL}}, *arg;
    
    arg = mkmargs;
    arg->value = &dirname; arg++;
    arg->value = &volname; arg++;
    arg->value = &cell; arg++;
    arg->value = &rwflag; arg++;
    arg->value = &helpflag; arg++;

    if (agetarg (mkmargs, argc, argv, &optind, AARG_AFSSTYLE)) {
	aarg_printusage(mkmargs, "fs mkmount", "", AARG_AFSSTYLE);
 	return 0;
    }

    argc -= optind;
    argv += optind;

    if (argc) {
	printf("unknown option %s\n", *argv);
	return 0;
    }

    if (rwflag)
	prefix = '%';
    else
	prefix = '#';
    if (cell)
	snprintf(buf, sizeof(buf), "%c%s:%s.", prefix, cell, volname);
    else
	snprintf(buf, sizeof(buf), "%c%s.", prefix, volname);

    if (symlink (buf, dirname) == -1) {
	perror ("fs");
	return 0;
    }

    return 0;
}
