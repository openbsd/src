/*
 * Copyright (c) 2000 Kungliga Tekniska Högskolan
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

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <err.h>
#include <cmd.h>
#include <roken.h>

#ifdef RCSID
RCSID("$KTH: testc.c,v 1.4 2000/10/16 22:03:35 assar Exp $");
#endif

static int
MainProc (struct cmd_syndesc *t, void *ptr)
{
    printf ("main proc\n");
    return 0;
}

static int
setacl (struct cmd_syndesc *t, void *ptr)
{
    struct cmd_item *it;
    int i;

    printf ("setacl:");
    printf (" dir:");
    for (it = t->parms[0].items; it ; it = it->next) {
	printf (" %s", (char *)it->data);
    }
    printf (" acls:");
    for (i = 0, it = t->parms[1].items; it; it = it->next, i++)
	printf (" %s", (char *)it->data);
    printf (" flags:");
    if (t->parms[2].items) printf (" -clear");
    if (t->parms[3].items) printf (" -negative");
    if (t->parms[4].items) printf (" -id");
    if (t->parms[5].items) printf (" -if");
    printf ("\n");
    if (i % 2 != 0)
	errx (1, "ace pairs isn't pairs");
    return 0;
}

static int
listacl (struct cmd_syndesc *t, void *ptr)
{
    struct cmd_item *it;

    printf ("listacl: ");
    for (it = t->parms[0].items; it ; it = it->next)
	printf (" %s", (char *)it->data);
    printf ("\n");
    return 0;
}

static int
listquota (struct cmd_syndesc *t, void *ptr)
{
    printf ("listquota\n");
    return 0;
}

int
main (int argc, char **argv)
{
    struct cmd_syndesc *ts;
    int ret;

    set_progname (argv[0]);

    ts = cmd_CreateSyntax (NULL, MainProc, NULL, "foo");

    ts = cmd_CreateSyntax ("setacl", setacl, NULL, "set a acl on a directory");

    cmd_CreateAlias (ts, "sa");
    cmd_AddParm (ts, "-dir", CMD_LIST, CMD_REQUIRED, "dir");
    cmd_AddParm (ts, "-acl", CMD_LIST, CMD_REQUIRED|CMD_EXPANDS, "acl entry");
    cmd_AddParm (ts, "-clear", CMD_FLAG, CMD_OPTIONAL, "");
    cmd_AddParm (ts, "-negative", CMD_FLAG, CMD_OPTIONAL, "");
    cmd_AddParm (ts, "-id", CMD_FLAG, CMD_OPTIONAL, "");
    cmd_AddParm (ts, "-if", CMD_FLAG, CMD_OPTIONAL, "");

    ts = cmd_CreateSyntax ("listacl", listacl, NULL, 
			   "list a acl on a directory");
    cmd_CreateAlias (ts, "la");
    cmd_AddParm (ts, "-dir", CMD_LIST, CMD_OPTIONAL|CMD_EXPANDS, "dir");

    ts = cmd_CreateSyntax ("listquota", listquota, 
			   NULL, "show quota in a volume");
    cmd_CreateAlias (ts, "lq");

#if 0
    cmd_PrintSyntax("fs");
#endif

    ret = cmd_Dispatch (argc, argv);
#if 0
    if (ret) {
	const char *error = cmd_number2str (ret);
	if (error == NULL)
	    error = strerror (ret);
	errx (1, "dispatch failed: %s (%d)", error, ret);
    }
#endif

    return 0;
}

