/*	$OpenBSD: getarg.c,v 1.1.1.1 1998/09/14 21:53:02 art Exp $	*/
/*
 * Copyright (c) 1997, 1998 Kungliga Tekniska Högskolan
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

#ifdef HAVE_CONFIG_H
#include <config.h>
RCSID("$KTH: getarg.c,v 1.9 1998/08/23 22:47:54 assar Exp $");
#endif

#include <stdio.h>
#include <roken.h>
#include "getarg.h"

#define ISFLAG(X) ((X)->type == arg_flag || (X)->type == arg_negative_flag)

static size_t
print_arg (FILE *stream, int mdoc, int longp, struct getargs *arg)
{
    const char *s = NULL;

    if (ISFLAG(arg))
	return 0;

    if(mdoc){
	if(longp)
	    fprintf(stream, "= Ns");
	fprintf(stream, " Ar ");
    }else
	if (longp)
	    putc ('=', stream);
	else
	    putc (' ', stream);

    if (arg->arg_help)
	s = arg->arg_help;
    else if (arg->type == arg_integer)
	s = "number";
    else if (arg->type == arg_string)
	s = "string";
    else
	s = "<undefined>";

    fprintf (stream, "%s", s);
    return 1 + strlen(s);
}

static void
mandoc_template(struct getargs *args,
		const char *extra_string)
{
    int i;
    struct getargs *arg;
    char timestr[64], cmd[64];
    const char *p;
    time_t t;

    printf(".\\\" Things to fix:\n");
    printf(".\\\"   * correct section, and operating system\n");
    printf(".\\\"   * remove Op from mandatory flags\n");
    printf(".\\\"   * use better macros for arguments (like .Pa for files)\n");
    printf(".\\\"\n");
    t = time(NULL);
    strftime(timestr, sizeof(timestr), "%b %d, %Y", localtime(&t));
    printf(".Dd %s\n", timestr);
    p = strrchr(__progname, '/');
    if(p) p++; else p = __progname;
    strncpy(cmd, p, sizeof(cmd));
    cmd[sizeof(cmd)-1] = '\0';
    strupr(cmd);
       
    printf(".Dt %s SECTION\n", cmd);
    printf(".Os OPERATING_SYSTEM\n");
    printf(".Sh NAME\n");
    printf(".Nm %s\n", p);
    printf(".Nd\n");
    printf("in search of a description\n");
    printf(".Sh SYNOPSIS\n");
    printf(".Nm\n");
    for(arg = args; arg->type; arg++) {
	if(arg->short_name){
	    printf(".Op Fl %c", arg->short_name);
	    print_arg(stdout, 1, 0, args + i);
	    printf("\n");
	}
	if(arg->long_name){
	    printf(".Op Fl -%s", arg->long_name);
	    print_arg(stdout, 1, 1, args + i);
	    printf("\n");
	}
    /*
	    if(arg->type == arg_strings)
		fprintf (stderr, "...");
		*/
    }
    if (extra_string && *extra_string)
	printf (".Ar %s\n", extra_string);
    printf(".Sh DESCRIPTION\n");
    printf("Supported options:\n");
    printf(".Bl -tag -width Ds\n");
    for(arg = args; arg->type; arg++) {
	if(arg->short_name){
	    printf(".It Fl %c", arg->short_name);
	    print_arg(stdout, 1, 0, args + i);
	    printf("\n");
	}
	if(arg->long_name){
	    printf(".It Fl -%s", arg->long_name);
	    print_arg(stdout, 1, 1, args + i);
	    printf("\n");
	}
	if(arg->help)
	    printf("%s\n", arg->help);
    /*
	    if(arg->type == arg_strings)
		fprintf (stderr, "...");
		*/
    }
    printf(".El\n");
    printf(".\\\".Sh ENVIRONMENT\n");
    printf(".\\\".Sh FILES\n");
    printf(".\\\".Sh EXAMPLES\n");
    printf(".\\\".Sh DIAGNOSTICS\n");
    printf(".\\\".Sh SEE ALSO\n");
    printf(".\\\".Sh STANDARDS\n");
    printf(".\\\".Sh HISTORY\n");
    printf(".\\\".Sh AUTHORS\n");
    printf(".\\\".Sh BUGS\n");
}

void
arg_printusage (struct getargs *args,
		const char *progname,
		const char *extra_string)
{
    struct getargs *arg;
    size_t max_len = 0;

    if(getenv("GETARGMANDOC")){
	mandoc_template(args, extra_string);
	return;
    }
    fprintf (stderr, "Usage: %s", __progname);
    for (arg = args; arg->type; arg++) {
	size_t len = 0;

	if (arg->long_name) {
	    fprintf (stderr, " [--");
	    if (arg->type == arg_negative_flag) {
		fprintf (stderr, "no-");
		len += 3;
	    }
	    fprintf (stderr, "%s", arg->long_name);
	    len += 2 + strlen(arg->long_name);
	    len += print_arg (stderr, 0, 1, arg);
	    putc (']', stderr);
	    if(arg->type == arg_strings)
		fprintf (stderr, "...");
	}
	if (arg->short_name) {
	    len += 2;
	    fprintf (stderr, " [-%c", arg->short_name);
	    len += print_arg (stderr, 0, 0, arg);
	    putc (']', stderr);
	    if(arg->type == arg_strings)
		fprintf (stderr, "...");
	}
	if (arg->long_name && arg->short_name)
	    len += 4;
	max_len = max(max_len, len);
    }
    if (extra_string)
	fprintf (stderr, " %s\n", extra_string);
    else
	fprintf (stderr, "\n");
    for (arg = args; arg->type; arg++) {
	if (arg->help) {
	    size_t count = 0;

	    if (arg->short_name) {
		fprintf (stderr, "-%c", arg->short_name);
		count += 2;
		count += print_arg (stderr, 0, 0, arg);
	    }
	    if (arg->short_name && arg->long_name) {
		fprintf (stderr, " or ");
		count += 4;
	    }
	    if (arg->long_name) {
		fprintf (stderr, "--");
		if (arg->type == arg_negative_flag) {
		    fprintf (stderr, "no-");
		    count += 3;
		}
		fprintf (stderr, "%s", arg->long_name);
		count += 2 + strlen(arg->long_name);
		count += print_arg (stderr, 0, 1, arg);
	    }
	    while(count++ <= max_len)
		putc (' ', stderr);
	    fprintf (stderr, "%s\n", arg->help);
	}
    }
}

static void
add_string(getarg_strings *s, char *value)
{
    s->strings = realloc(s->strings, (s->num_strings + 1) * sizeof(*s->strings));
    s->strings[s->num_strings] = value;
    s->num_strings++;
}

static int
parse_option(struct getargs *arg, char *optarg, int negate)
{
    switch(arg->type){
    case arg_integer:
    {
	int tmp;
	if(sscanf(optarg, "%d", &tmp) != 1)
	    return ARG_ERR_BAD_ARG;
	*(int*)arg->value = tmp;
	return 0;
    }
    case arg_string:
    {
	*(char**)arg->value = optarg;
	return 0;
    }
    case arg_strings:
    {
	add_string((getarg_strings*)arg->value, optarg);
	return 0;
    }
    case arg_flag:
    case arg_negative_flag:
    {
	int *flag = arg->value;
	if(*optarg == '\0' ||
	   strcmp(optarg, "yes") == 0 || 
	   strcmp(optarg, "true") == 0){
	    *flag = !negate;
	    return 0;
	} else if (*optarg && strcmp(optarg, "maybe") == 0) {
	    *flag = rand() & 1;
	} else {
	    *flag = negate;
	    return 0;
	}
	return ARG_ERR_BAD_ARG;
    }
    default:
	abort ();
    }
}


static int
arg_match_long(struct getargs *args,
	       char **argv, int style)
{
    char *optarg = NULL;
    int negate = 0;
    int partial_match = 0;
    struct getargs *partial = NULL;
    struct getargs *current = NULL;
    struct getargs *arg;
    int argv_len;
    char *p, *q;

    if (style & ARG_LONGARG)
	q = *argv + 2;
    else if (style & ARG_TRANSLONG)
	q = *argv + 1;
    else
	q = *argv;

    argv_len = strlen(q);
    p = strchr (q, '=');
    if (p != NULL)
	argv_len = p - q;

    for (arg = args; arg->type ; arg++) {
	if(arg->long_name) {
	    int len = strlen(arg->long_name);
	    char *p = q;
	    int p_len = argv_len;
	    negate = 0;

	    for (;;) {
		if (strncmp (arg->long_name, p, len) == 0) {
		    current = arg;
		    if (style & ARG_TRANSLONG && *(argv +1))
			optarg = *(argv + 1);
		    else if(p[len] == '\0')
			optarg = p + len;
		    else
			optarg = p + len + 1;
		} else if (strncmp (arg->long_name,
				    p,
				    p_len) == 0) {
		    ++partial_match;
		    partial = arg;
		    optarg  = p + p_len +1 ;
		} else if (ISFLAG(arg) && strncmp (p, "no-", 3) == 0) {
		    negate = !negate;
		    p += 3;
		    p_len -= 3;
		    continue;
		}
		break;
	    }
	    if (current)
		break;
	}
    }
    if (current == NULL) {
	if (partial_match == 1)
	    current = partial;
	else
	    return ARG_ERR_NO_MATCH;
    }

    if(*optarg == '\0' && !ISFLAG(current))
	return ARG_ERR_NO_MATCH;

    return parse_option(current, optarg, negate);
}

int
getarg(struct getargs *args,
       int argc, char **argv, int *optind, int style)
{
    int i, j;
    struct getargs *arg;
    int ret = 0;
    int swcount = *optind;

    srand (time(NULL));
    (*optind)++;
    for(i = *optind; i < argc; i++) {
	if(argv[i][0] != '-' && swcount != -1) {
	    if (!(style & ARG_SWITCHLESS))
		break;
	    ret = parse_option(&args[swcount], argv[i], 0);
	    if (ret)
		return ret;
	    swcount++;
	} else if(argv[i][1] == '-' || 
		  ((style & ARG_TRANSLONG) && argv[i][1] != 0)) {
	    if(argv[i][2] == 0 && !(style & ARG_TRANSLONG)){
		i++;
		break;
	    }
	    swcount = -1;
	    ret = arg_match_long (args, &argv[i], style);
	    if(ret)
		return ret;
	    if (style & ARG_TRANSLONG && argv[i+1])
		++i;
	}else if (style & ARG_SHORTARG) {
	    for(j = 1; argv[i][j]; j++) {
		for(arg = args; arg->type; arg++) {
		    char *optarg;
		    if(arg->short_name == 0)
			continue;
		    if(argv[i][j] == arg->short_name){
			if(arg->type == arg_flag){
			    *(int*)arg->value = 1;
			    break;
			}
			if(arg->type == arg_negative_flag){
			    *(int*)arg->value = 0;
			    break;
			}
			if(argv[i][j + 1])
			    optarg = &argv[i][j + 1];
			else{
			    i++;
			    optarg = argv[i];
			}
			if(optarg == NULL)
			    return ARG_ERR_NO_ARG;
			if(arg->type == arg_integer){
			    int tmp;
			    if(sscanf(optarg, "%d", &tmp) != 1)
				return ARG_ERR_BAD_ARG;
			    *(int*)arg->value = tmp;
			    goto out;
			}else if(arg->type == arg_string){
			    *(char**)arg->value = optarg;
			    goto out;
			}else if(arg->type == arg_strings){
			    add_string((getarg_strings*)arg->value, optarg);
			    goto out;
			}
			return ARG_ERR_BAD_ARG;
		    }
			
		}
		if (!arg->type)
		    return ARG_ERR_NO_MATCH;
	    }
	out:;
	}
    }
    *optind = i;
    return 0;
}

#if TEST
int foo_flag = 2;
int flag1 = 0;
int flag2 = 0;
int bar_int;
char *baz_string;

struct getargs args[] = {
    { NULL, '1', arg_flag, &flag1, "one", NULL },
    { NULL, '2', arg_flag, &flag2, "two", NULL },
    { "foo", 'f', arg_negative_flag, &foo_flag, "foo", NULL },
    { "bar", 'b', arg_integer, &bar_int, "bar", "seconds"},
    { "baz", 'x', arg_string, &baz_string, "baz", "name" },
    { NULL, 0, arg_end, NULL}
};

int main(int argc, char **argv)
{
    int optind = 0;
    while(getarg(args, 5, argc, argv, &optind))
	printf("Bad arg: %s\n", argv[optind]);
    printf("flag1 = %d\n", flag1);  
    printf("flag2 = %d\n", flag2);  
    printf("foo_flag = %d\n", foo_flag);  
    printf("bar_int = %d\n", bar_int);
    printf("baz_flag = %s\n", baz_string);
    arg_printusage (args, 5, argv[0], "nothing here");
}
#endif
