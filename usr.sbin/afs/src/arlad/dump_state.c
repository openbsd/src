/*
 * Copyright (c) 2001 - 2002 Kungliga Tekniska Högskolan
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

/*
 * Dump arla disk storage
 */

#include "arla_local.h"
#include <getarg.h>
#include <version.h>
RCSID("$arla: dump_state.c,v 1.4 2003/01/17 03:01:00 lha Exp $");

static char *prefix = ARLACACHEDIR;
static char *fcache_file;
static char *volcache_file;
static int version_flag;
static int verbose_flag;
static int help_flag;

static int
print_fcache_entry(struct fcache_store *st, void *ptr)
{
    printf("Fid: %s %d.%d.%d %02x/%02x\n", st->cell,
	   st->fid.Volume, st->fid.Vnode, st->fid.Unique,
	   (st->index >> 8) & 0xff, st->index & 0xff );
    if (verbose_flag)
	printf("Length: accounted: %d fetched: %d\n", 
	       st->length, st->fetched_length);
    return 0;
}

static void
print_vol(nvldbentry *e, const char *str, int32_t flags, int type)
{
    if (e->flags & flags)
	printf (" %s: %d", str, e->volumeId[type]);
}

static int
print_volcache_entry(struct volcache_store *st, void *ptr)
{
    printf("Volume: %s %s\n", st->cell, st->entry.name);
    printf("volume-id:");
    print_vol(&st->entry, "RW", VLF_RWEXISTS, RWVOL);
    print_vol(&st->entry, "RO", VLF_ROEXISTS, ROVOL);
    print_vol(&st->entry, "BU", VLF_BOEXISTS, BACKVOL);
    printf("\n");
    return 0;
}

static struct getargs args[] = {
    {"prefix", 'p',	arg_string,	&prefix,
     "prefix to arla cache dir", "dir"},
    {"verbose",	'v',	arg_flag,	&verbose_flag,
     NULL, NULL},
    {"version",	0,	arg_flag,	&version_flag,
     NULL, NULL},
    {"help",	0,	arg_flag,	&help_flag,
     NULL, NULL}
};

static void
usage (int ret)
{
    arg_printusage (args, sizeof(args)/sizeof(*args), NULL, "");
    exit (ret);
}

int
main(int argc, char **argv)
{
    int optind = 0;
    int ret;

    set_progname (argv[0]);

    if (getarg (args, sizeof(args)/sizeof(*args), argc, argv, &optind))
	usage (1);

    if (version_flag) {
	print_version(NULL);
	exit(0);
    }

    if (fcache_file == NULL)
	asprintf(&fcache_file, "%s/fcache", prefix);

    printf("fcache:\n");
    ret = state_recover_fcache(fcache_file, print_fcache_entry, NULL);
    if (ret)
	warn("failed with %d to dump fcache state (%s)", ret, fcache_file);

    if (volcache_file == NULL)
	asprintf(&volcache_file, "%s/volcache", prefix);

    printf("volcache\n");
    ret = state_recover_volcache(volcache_file, print_volcache_entry, NULL);
    if (ret)
	warn("failed with %d to dump volcache state (%s)", ret, volcache_file);

    return 0;
}
