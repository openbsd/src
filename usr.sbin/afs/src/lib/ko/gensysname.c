/*	$OpenBSD: gensysname.c,v 1.2 1999/04/30 01:59:11 art Exp $	*/
/*
 * Copyright (c) 1998, 1999 Kungliga Tekniska Högskolan
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

RCSID("$KTH: gensysname.c,v 1.24 1999/01/04 23:21:29 lha Exp $");

typedef int (*test_sysname)(void);
typedef void (*gen_sysname)(char*, size_t, const char*, 
			    const char*, const char*);

struct sysname {
    const char *sysname;
    const char *cpu;
    const char *vendor;
    const char *os;
    test_sysname atest;
    gen_sysname gen;
};

enum { OUTPUT_C, OUTPUT_TEXT, OUTPUT_MACHINE } output = OUTPUT_TEXT;

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

#ifdef HAVE_SYS_UTSNAME_H
static void
bsd_gen_sysname(char *buf, 
		size_t len, 
		const char *cpu, 
		const char *vendor, 
		const char *os)
{
    struct utsname uts;
    int major, minor;
    const char *name;
    if(uname(&uts) < 0) {
	warn("uname");
	strcpy(buf, "bsdhost");
	return;
    }
    if(strcmp(uts.sysname, "FreeBSD") == 0)
	name = "fbsd";
    else if(strcmp(uts.sysname, "NetBSD") == 0)
	name = "nbsd";
    else if(strcmp(uts.sysname, "OpenBSD") == 0)
	name = "obsd";
    else if(strcmp(uts.sysname, "BSD/OS") == 0)
	name = "bsdi";
    else
	name = "bsd";
    /* this is perhaps a bit oversimplified */
    if(sscanf(uts.release, "%d.%d", &major, &minor) == 2)
	snprintf(buf, len, "%s_%s%d%d", uts.machine, name, major, minor);
    else
	snprintf(buf, len, "%s_%s", uts.machine, name);
}
#endif

struct sysname sysnames[] = {
    { "sparc_linux6", "sparc", "*", "linux-gnu*", &linux_glibc_test },
    { "sparc_linux5", "sparc", "*", "linux-gnu*", NULL },
    { "i386_linux6", "i*86*", "*pc*", "linux-gnu*", &linux_glibc_test },
    { "i386_linux5", "i*86*", "*pc*", "linux-gnu*", NULL },
    { "alpha_linux6", "alpha", "*", "linux-gnu*", &linux_glibc_test },
    { "alpha_linux5", "alpha", "*", "linux-gnu*", NULL },
    { "alpha_dux40",  "alpha", "*", "osf4.0*", NULL },
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
    { "i386_nt35",    "i*86*", "*", "cygwin*", NULL },
#ifdef HAVE_SYS_UTSNAME_H
    /* catch-all bsd entry */
    { "",	      "*",    "*", "*bsd*",       NULL, &bsd_gen_sysname },
#endif
    {NULL}
};

static void
printsysname(const char *sysname)
{
    switch (output) {
    case OUTPUT_TEXT:
	printf("%s\n", sysname);
	break;
    case OUTPUT_MACHINE:
	printf("%s\n", sysname);
	break;
    case OUTPUT_C:
	printf("/* Generated from $KTH: gensysname.c,v 1.24 1999/01/04 23:21:29 lha Exp $ */\n");
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
    arg_printusage(args, NULL, "[sysname]", 0);
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
	errx(0, "Version: $KTH: gensysname.c,v 1.24 1999/01/04 23:21:29 lha Exp $");

    if (ccodeflag)
	output = OUTPUT_C;
    if (humanflag)
	output = OUTPUT_TEXT;
    if (machineflag)
	output = OUTPUT_MACHINE;

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
	char sn[64];
	if (!strmatch(sysname->cpu, cpu) &&
	    !strmatch(sysname->vendor, vendor) &&
	    !strmatch(sysname->os, os) &&
	    (sysname->atest == NULL || ((*(sysname->atest))()))) {
	    
	    found = 1;
	    if(sysname->gen != NULL)
		(*sysname->gen)(sn, sizeof(sn), cpu, vendor, os);
	    else {
		strncpy(sn, sysname->sysname, sizeof(sn));
		sn[sizeof(sn) - 1] = '\0';
	    }
	    printsysname(sn);   
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
