/*
 * Copyright (c) 1995-2000 Kungliga Tekniska Högskolan
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


#include "appl_locl.h"
#include <kafs.h>

RCSID("$arla: up.c,v 1.6 2000/10/03 00:06:41 lha Exp $");

static void do_help (int exitval);
static int  copyacl (char *from, char *to);

static int arg_help = 0;
static int arg_verbose = 0;
static int arg_one = 0;
static int arg_force = 0;
static int arg_backup = 0;
static int arg_savedate = 0;

struct agetargs args[] = {
    { NULL , 'h', aarg_flag, &arg_help,
      "verbose", NULL, aarg_optional},
    { NULL , 'v', aarg_flag, &arg_verbose,
      "verbose", NULL, aarg_optional},
    { NULL , '1', aarg_flag, &arg_one,
      "top level only", NULL, aarg_optional},
    { NULL , 'f', aarg_flag, &arg_force,
      "force", NULL, aarg_optional},
    { NULL , 'r', aarg_flag, &arg_backup,
      "verbose", NULL, aarg_optional},
    { NULL , 'x', aarg_flag, &arg_savedate,
      "verbose", NULL, aarg_optional},
    { NULL, 0, aarg_end, NULL, NULL }
};

static void
do_help (int exitval)
{
    aarg_printusage(args, NULL,
		    "<from-directory> <to-directory>",
		    AARG_SHORTARG);
    exit(exitval);
}


static int
copyacl (char *from, char *to)
{
    struct ViceIoctl a_params;
    char buf[AFSOPAQUEMAX];

    a_params.in_size	= 0;
    a_params.in		= NULL;
    a_params.out_size	= sizeof(buf);
    a_params.out	= buf;

    if (k_pioctl (from, VIOCGETAL, &a_params, 1) != 0) {
	fprintf (stderr, "k_pioctl(\"%s\", VIOCGETAL) failed %d\n", 
		 from, errno);
	return errno;
    }

    a_params.in_size	= sizeof(buf);
    a_params.in		= buf;
    a_params.out_size	= 0;
    a_params.out	= NULL;

    if (k_pioctl (to, VIOCSETAL, &a_params, 1) != 0) {
	fprintf (stderr, "k_pioctl(\"%s\", VIOCSETAL) failed %d\n",
		 to, errno);
	return errno;
    }
    return 0;
}


static int
check_source_dir (const char *path)
{
    struct stat sb;
    int ret;

    ret = lstat (path, &sb);
    if (ret) {
	if (errno == ENOENT)
	    errx (1, "source diretory `%s' doesn't exist", path);
	else 
	    err (1, "check_source_dir: lstat: path `%s'", path);
    }
    
    /* XXX */
	
    return 0;
}



int
main(int argc, char **argv)
{
    int optind = 0;
    char *fromdir;
    char *todir;
    int ret;

    if (agetarg (args, argc, argv, &optind, AARG_SHORTARG))
	do_help(1);
    
    if (arg_help)
	do_help(0);

    argc -= optind;
    argv += optind;

    if (argc != 2)
	do_help(1);

    if (!k_hasafs())
	errx (1, "there seam to be no AFS no this computer");

    fromdir = argv[0];
    todir = argv[1];

    if (arg_verbose)
	printf ("fromdir: \"%s\" todir: \"%s\"\n", fromdir, todir);

    check_source_dir (todir);
    ret = copyacl (fromdir, todir);
    if (ret)
	errx (1, "copyacl failed with %s (%d)", strerror(ret), ret);

    return 0;
}
