/*
 * Copyright (c) 2003-2004 Kungliga Tekniska Högskolan
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
 * 3. Neither the name of KTH nor the names of its contributors may be
 *    used to endorse or promote products derived from this software without
 *    specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY KTH AND ITS CONTRIBUTORS ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL KTH OR ITS CONTRIBUTORS BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR
 * BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
 * WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR
 * OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF
 * ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "gssapi_locl.h"
#include <err.h>
#include <getarg.h>

RCSID("$KTH: test_cred.c,v 1.2 2005/05/02 13:46:39 lha Exp $");

static void
acquire_release_loop(gss_name_t name, int counter, gss_cred_usage_t usage)
{
    OM_uint32 maj_stat, min_stat;
    gss_cred_id_t cred;
    int i;

    for (i = 0; i < counter; i++) {
	maj_stat = gss_acquire_cred(&min_stat, name,
				    GSS_C_INDEFINITE,
				    GSS_C_NO_OID_SET,
				    usage,
				    &cred,
				    NULL,
				    NULL);
	if (maj_stat != GSS_S_COMPLETE)
	    errx(1, "aquire %d %d != GSS_S_COMPLETE", i, (int)maj_stat);
				    
	maj_stat = gss_release_cred(&min_stat, &cred);
	if (maj_stat != GSS_S_COMPLETE)
	    errx(1, "release %d %d != GSS_S_COMPLETE", i, (int)maj_stat);
    }
}


static void
acquire_add_release_add(gss_name_t name, gss_cred_usage_t usage)
{
    OM_uint32 maj_stat, min_stat;
    gss_cred_id_t cred, cred2, cred3;

    maj_stat = gss_acquire_cred(&min_stat, name,
				GSS_C_INDEFINITE,
				GSS_C_NO_OID_SET,
				usage,
				&cred,
				NULL,
				NULL);
    if (maj_stat != GSS_S_COMPLETE)
	errx(1, "aquire %d != GSS_S_COMPLETE", (int)maj_stat);
    
    maj_stat = gss_add_cred(&min_stat,
			    cred,
			    GSS_C_NO_NAME,
			    GSS_KRB5_MECHANISM,
			    usage,
			    GSS_C_INDEFINITE,
			    GSS_C_INDEFINITE,
			    &cred2,
			    NULL,
			    NULL,
			    NULL);
			    
    if (maj_stat != GSS_S_COMPLETE)
	errx(1, "add_cred %d != GSS_S_COMPLETE", (int)maj_stat);

    maj_stat = gss_release_cred(&min_stat, &cred);
    if (maj_stat != GSS_S_COMPLETE)
	errx(1, "release %d != GSS_S_COMPLETE", (int)maj_stat);

    maj_stat = gss_add_cred(&min_stat,
			    cred2,
			    GSS_C_NO_NAME,
			    GSS_KRB5_MECHANISM,
			    GSS_C_BOTH,
			    GSS_C_INDEFINITE,
			    GSS_C_INDEFINITE,
			    &cred3,
			    NULL,
			    NULL,
			    NULL);

    maj_stat = gss_release_cred(&min_stat, &cred2);
    if (maj_stat != GSS_S_COMPLETE)
	errx(1, "release 2 %d != GSS_S_COMPLETE", (int)maj_stat);

    maj_stat = gss_release_cred(&min_stat, &cred3);
    if (maj_stat != GSS_S_COMPLETE)
	errx(1, "release 2 %d != GSS_S_COMPLETE", (int)maj_stat);
}

static int version_flag = 0;
static int help_flag	= 0;

static struct getargs args[] = {
    {"version",	0,	arg_flag,	&version_flag, "print version", NULL },
    {"help",	0,	arg_flag,	&help_flag,  NULL, NULL }
};

static void
usage (int ret)
{
    arg_printusage (args, sizeof(args)/sizeof(*args),
		    NULL, "service@host");
    exit (ret);
}


int
main(int argc, char **argv)
{
    struct gss_buffer_desc_struct name_buffer;
    OM_uint32 maj_stat, min_stat;
    gss_name_t name;
    int optind = 0;

    setprogname(argv[0]);
    if(getarg(args, sizeof(args) / sizeof(args[0]), argc, argv, &optind))
	usage(1);
    
    if (help_flag)
	usage (0);

    if(version_flag){
	print_version(NULL);
	exit(0);
    }

    argc -= optind;
    argv += optind;

    if (argc < 1)
	errx(1, "argc < 1");

    name_buffer.value = argv[0];
    name_buffer.length = strlen(argv[0]);

    maj_stat = gss_import_name(&min_stat, &name_buffer,
			       GSS_C_NT_HOSTBASED_SERVICE,
			       &name);
    if (maj_stat != GSS_S_COMPLETE)
	errx(1, "import name error");

    acquire_release_loop(name, 100, GSS_C_ACCEPT);
    acquire_release_loop(name, 100, GSS_C_INITIATE);
    acquire_release_loop(name, 100, GSS_C_BOTH);

    acquire_add_release_add(name, GSS_C_ACCEPT);
    acquire_add_release_add(name, GSS_C_INITIATE);
    acquire_add_release_add(name, GSS_C_BOTH);

    gss_release_name(&min_stat, &name);

    return 0;
}
