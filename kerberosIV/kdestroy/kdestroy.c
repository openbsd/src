/*	$OpenBSD: kdestroy.c,v 1.6 1998/08/12 23:39:40 art Exp $	*/
/*	$KTH: kdestroy.c,v 1.10 1998/05/13 22:44:24 assar Exp $		*/
/*
 * Copyright (c) 1998 Kungliga Tekniska Högskolan
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
 * 3. All advertising materials mentioning features or use of this software 
 *    must display the following acknowledgement: 
 *      This product includes software developed by Kungliga Tekniska 
 *      Högskolan and its contributors. 
 *
 * 4. Neither the name of the Institute nor the names of its contributors 
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

#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <sys/types.h>
#include <fcntl.h>
#include <time.h>
#include <sys/stat.h>
#include <kerberosIV/krb.h>
#include <kerberosIV/kafs.h>
#include <getarg.h>
#include <err.h>

int quiet_flag;
#ifdef LEGACY_KDESTROY
int unlog_flag;
#else
int unlog_flag = 1;
#endif
int help_flag;
int version_flag;

struct getargs args[] = {
    { "quiet", 		'q',	arg_flag, 	&quiet_flag, 
      "don't print any messages" },
    { NULL, 		'f',	arg_flag, 	&quiet_flag },
#ifdef LEGACY_KDESTROY
    { "unlog", 		0,	arg_flag, &unlog_flag,
    "destroy tokens" },
    { NULL, 		't',	arg_negative_flag, &unlog_flag,
    "don't destroy tokens (default)" },
#else
    { "unlog", 		't',	arg_negative_flag, &unlog_flag,
    "don't destroy tokens" },
#endif
    { "version", 	0,	arg_flag,	&version_flag },
    { "help",		'h',	arg_flag,	&help_flag }
};

int num_args = sizeof(args) / sizeof(args[0]);

static void
usage(int code)
{
    arg_printusage(args, num_args, "");
    exit(code);
}

int
main(int argc, char **argv)
{
    int optind = 0;
    int ret;

    if(getarg(args, num_args, argc, argv, &optind))
	usage(1);

    if(help_flag)
	usage(0);

    if(version_flag)
	errx(0, "%s", krb4_version);
    
    ret = dest_tkt();

    if(unlog_flag && k_hasafs())
	k_unlog();

    if (quiet_flag) {
	if (ret != 0 && ret != RET_TKFIL)
	    exit(1);
	else
	    exit(0);
    }
    if (ret == 0)
	printf("Tickets destroyed.\n");
    else if (ret == RET_TKFIL)
	printf("No tickets to destroy.\n");
    else {
	printf("Tickets NOT destroyed.\n");
	exit(1);
    }
    exit(0);
}
