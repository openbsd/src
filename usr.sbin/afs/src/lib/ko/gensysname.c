/*	$OpenBSD: gensysname.c,v 1.1.1.1 1998/09/14 21:53:00 art Exp $	*/
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
 *      This product includes software developed by the Kungliga Tekniska
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

#include "ko_locl.h"

RCSID("$KTH: gensysname.c,v 1.13 1998/08/23 22:50:24 assar Exp $");

typedef int (*test_sysname)(void);

struct sysname {
    const char *sysname;
    const char *cpu;
    const char *vendor;
    const char *os;
    test_sysname atest;
};

enum { O_C, O_TEXT, O_MACHINE } output = O_TEXT;

/* 
 * HELP:
 *
 * Add your sysname to the struct below, it's searched from top
 * to bottom, first match wins.
 *
 * The test is for hosts that can not be matched with config.guess
 * (like a linux 1.2.x/elf)
 *
 * ? will match any character
 * * will match any sequence of characters
 */

static int
linux_glibc_test(void)
{
    int ret;
    struct stat sb;

    ret = stat("/lib/libc.so.6", &sb);
    return ret == 0;
}

struct sysname sysnames[] = {
    { "sparc_nbsd13", "sparc", "*", "netbsd1.3*", NULL },
    { "sparc_obsd23", "sparc", "*", "openbsd2.3*", NULL },
    { "sparc_linux6", "sparc", "*", "linux-gnu*", &linux_glibc_test },
    { "sparc_linux5", "sparc", "*", "linux-gnu*", NULL },
    { "i386_obsd23", "i*86*", "*", "openbsd2.3*", NULL },
    { "i386_fbsd30", "i*86*", "*", "freebsd3.0*", NULL },
    { "i386_linux6", "i*86*", "*pc*", "linux-gnu*", &linux_glibc_test },
    { "i386_linux5", "i*86*", "*pc*", "linux-gnu*", NULL },
    { "alpha_linux6", "alpha", "*", "linux-gnu*", &linux_glibc_test },
    { "alpha_linux5", "alpha", "*", "linux-gnu*", NULL },
    { "sun4x_54",     "sparc", "*", "solaris2.4*", NULL },
    { "sun4x_551",    "sparc", "*", "solaris2.5.1*", NULL },
    { "sun4x_55",     "sparc", "*", "solaris2.5*", NULL },
    { "sun4x_56",     "sparc", "*", "solaris2.6*", NULL },
    { "sun4x_57",     "sparc", "*", "solaris2.7*", NULL },
    { "sunx86_54",    "i386", "*", "solaris2.4*", NULL },
    { "sunx86_551",   "i386", "*", "solaris2.5.1*", NULL },
    { "sunx86_55",    "i386", "*", "solaris2.5*", NULL },
    { "sunx86_56",    "i386", "*", "solaris2.6*", NULL },
    { "sunx86_57",    "i386", "*", "solaris2.7*", NULL },
    {NULL}
};

static void
printsysname(const char *sysname)
{
    switch (output) {
    case O_TEXT:
	printf("%s\n", sysname);
	break;
    case O_MACHINE:
	printf("%s\n", sysname);
	break;
    case O_C:
	printf("/* Generated from $KTH: gensysname.c,v 1.13 1998/08/23 22:50:24 assar Exp $ */\n");
	printf("const char *arla_getsysname(void) { return \"%s\" ; }\n", 
	       sysname);
	break;
    default:
	abort();
    }
    
}

static int machineflag = 0;
static int ccodeflag = 0;
static int humanflag = 0;
static int helpflag = 0;
static int allflag = 0;
static int sysnameflag = 0;
static int versionflag = 0;

struct getargs args[] = {
    {"machine", 'm', arg_flag,    &machineflag, "machine output", NULL},
    {"human",   'h', arg_flag,    &humanflag,   "human", NULL},
    {"ccode",   'c', arg_flag,    &ccodeflag, "", NULL},
    {"sysname",	's', arg_flag,    &sysnameflag, NULL, NULL},
    {"version", 'v', arg_flag,    &versionflag, NULL, NULL},
    {"all",     'a', arg_flag,    &allflag, NULL, NULL},
    {"help",	0,   arg_flag,    &helpflag, NULL, NULL},
    {NULL,      0, arg_end, NULL}
};

static void
usage(void)
{
    arg_printusage(args, NULL, "[sysname]");
    exit(1);
    
}

static void
try_parsing (int *argc, char ***argv, const char **var)
{
    char *p;

    if (*argc > 0) {
	p = strchr (**argv, '-');

	*var = **argv;

	if (p != NULL) {
	    *p = '\0';
	    **argv = p + 1;
	} else {
	    --*argc;
	    ++*argv;
	}
    }
}

int 
main(int argc, char **argv)
{
    const char *cpu    = ARLACPU;
    const char *vendor = ARLAVENDOR;
    const char *os     = ARLAOS;
    struct sysname *sysname = sysnames;
    int found = 0;
    int optind = 0;

    set_progname (argv[0]);

    if (getarg (args, argc, argv, &optind, ARG_GNUSTYLE)) 
	usage();

    argc -= optind;
    argv += optind;

    if (helpflag)
	usage();

    if (versionflag)
	errx(0, "Version: $KTH: gensysname.c,v 1.13 1998/08/23 22:50:24 assar Exp $");

    if (ccodeflag)
	output = O_C;
    if (humanflag)
	output = O_TEXT;
    if (machineflag)
	output = O_MACHINE;

    if (sysnameflag) {
	printf ("%s-%s-%s\n", cpu, vendor, os);
	return 0;
    }

    if (allflag) {
	while (sysname->sysname) {
	    printf("%-20s == %s %s %s\n",
		   sysname->sysname,
		   sysname->cpu,
		   sysname->vendor,
		   sysname->os);
	    sysname++;
	}
	return 0;
    }
    
    try_parsing (&argc, &argv, &cpu);
    try_parsing (&argc, &argv, &vendor);
    try_parsing (&argc, &argv, &os);

    while (sysname->sysname && !found) {
	if (!strmatch(sysname->cpu, cpu) &&
	    !strmatch(sysname->vendor, vendor) &&
	    !strmatch(sysname->os, os) &&
	    (sysname->atest == NULL || ((*(sysname->atest))()))) {
	    
	    found = 1;
	    printsysname(sysname->sysname);   
	}
	sysname++;
    }

    /* XXX need some better here? */
    if (!found) {
	fprintf(stderr, "our host was not found using generic\n"); 
	printsysname("arlahost");
    }
    
    return 0;
}
