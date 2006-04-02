/*
 * Copyright (c) 1997 - 2001 Kungliga Tekniska Högskolan
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
RCSID("$arla: agetarg.c,v 1.13 2002/09/17 18:30:53 lha Exp $");
#endif

#include <stdio.h>
#include <roken.h>
#include "agetarg.h"

#define ISFLAG(X) ((X)->type == aarg_flag || (X)->type == aarg_negative_flag)

extern char *__progname;

static size_t
print_arg (FILE *stream, int mdoc, int longp, struct agetargs *arg,
	   int style)
{
    const char *s = NULL;
    int len;

    if (ISFLAG(arg))
	return 0;

    if(mdoc){
	if(longp)
	    fprintf(stream, "= Ns");
	fprintf(stream, " Ar ");
    }else
	if (longp && !(style & AARG_TRANSLONG))
	    putc ('=', stream);
	else
	    putc (' ', stream);

    if (arg->arg_help)
	s = arg->arg_help;
    else if (arg->type == aarg_integer)
	s = "number";
    else if (arg->type == aarg_string)
	s = "string";
    else
	s = "undefined";

    if (style & AARG_TRANSLONG) {
	fprintf (stream, "<%s>", s);
	len = strlen(s) + 2;
    } else {
	fprintf (stream, "%s", s);
	len = strlen(s);
    }
    return 1 + len;
}

static void
mandoc_template(struct agetargs *args,
		const char *extra_string,
		int style)
{
    struct agetargs *arg;
    char timestr[64], cmd[64];
    const char *p;
    time_t t;
    extern char *__progname;

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
	    print_arg(stdout, 1, 0, args, style);
	    printf("\n");
	}
	if(arg->long_name){
	    printf(".Op Fl %s%s", style & AARG_TRANSLONG ? "" : "-", arg->long_name);
	    print_arg(stdout, 1, 1, args, style);
	    printf("\n");
	}
    /*
	    if(arg->type == aarg_strings)
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
	    print_arg(stdout, 1, 0, args, style);
	    printf("\n");
	}
	if(arg->long_name){
	    printf(".It Fl %s%s", style & AARG_TRANSLONG ? "" : "-", arg->long_name);
	    print_arg(stdout, 1, 1, args, style);
	    printf("\n");
	}
	if(arg->help)
	    printf("%s\n", arg->help);
    /*
	    if(arg->type == aarg_strings)
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
aarg_printusage (struct agetargs *args,
		 const char *progname,
		 const char *extra_string,
		 int style)
{
    struct agetargs *arg;
    size_t max_len = 0;

    if (progname == NULL)
	progname = __progname;

    if(getenv("GETARGMANDOC")){
	mandoc_template(args, extra_string, style);
	return;
    }
    fprintf (stderr, "Usage: %s", progname);
    for (arg = args; arg->type; arg++) {
	size_t len = 0;

	if (arg->long_name) {
	    if (style & AARG_TRANSLONG) {
		switch (arg->mandatoryp) {
		case aarg_mandatory:
		    fprintf (stderr, " -");
		    break;
		default:
		    fprintf (stderr, " [-");
		    break;
		}
	    } else
		fprintf (stderr, " [--");

	    if (arg->type == aarg_negative_flag) {
		fprintf (stderr, "no-");
		len += 3;
	    }
	    fprintf (stderr, "%s", arg->long_name);
	    len += 2 + strlen(arg->long_name);
	    len += print_arg (stderr, 0, 1, arg, style);
	    if(arg->type == aarg_strings)
		fprintf (stderr, "...");
	    if(!(style & AARG_TRANSLONG) || arg->mandatoryp != aarg_mandatory)
		putc (']', stderr);
	}
	if (arg->short_name) {
	    len += 2;
	    fprintf (stderr, " [-%c", arg->short_name);
	    len += print_arg (stderr, 0, 0, arg, style);
	    putc (']', stderr);
	    if(arg->type == aarg_strings)
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
		count += print_arg (stderr, 0, 0, arg, style);
	    }
	    if (arg->short_name && arg->long_name) {
		fprintf (stderr, " or ");
		count += 4;
	    }
	    if (arg->long_name) {
		fprintf (stderr, "-%s", style & AARG_TRANSLONG ? "" : "-");
		if (arg->type == aarg_negative_flag) {
		    fprintf (stderr, "no-");
		    count += 3;
		}
		fprintf (stderr, "%s", arg->long_name);
		count += 2 + strlen(arg->long_name);
		count += print_arg (stderr, 0, 1, arg, style);
	    }
	    while(count++ <= max_len)
		putc (' ', stderr);
	    fprintf (stderr, "%s\n", arg->help);
	}
    }
}

static void
add_string(agetarg_strings *s, char *value)
{
    s->strings = realloc(s->strings, (s->num_strings + 1) * sizeof(*s->strings));
    s->strings[s->num_strings] = value;
    s->num_strings++;
}

static int
parse_option(struct agetargs *arg, int style, char *optarg, int argc, 
	     char **argv, int *next, int negate)
{
    switch(arg->type){
    case aarg_integer:
    {
	int tmp;
	if(sscanf(optarg, "%d", &tmp) != 1)
	    return AARG_ERR_BAD_ARG;
	*(int*)arg->value = tmp;
	return 0;
    }
    case aarg_string:
    case aarg_generic_string:
    {
	*(char**)arg->value = optarg;
	return 0;
    }
    case aarg_strings:
    {
	add_string ((agetarg_strings*)arg->value, optarg);
	while ((style & AARG_TRANSLONG)
	       && argc > *next + 1
	       && argv[*next + 1]
	       && argv[*next + 1][0] != '-')
	{
	    add_string ((agetarg_strings*)arg->value, argv[*next + 1]);
	    (*next)++;
	}
	return 0;
    }
    case aarg_flag:
    case aarg_negative_flag:
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
	return AARG_ERR_BAD_ARG;
    }
    default:
	abort ();
    }
}


static int
arg_match_long(struct agetargs *args, int argc,
	       char **argv, int style, int *next, int *num_arg)
{
    char *optarg = NULL;
    int negate = 0;
    int numarg = -1;
    int partial_match = 0;
    int do_generic=0;
    struct agetargs *partial = NULL;
    struct agetargs *generic_arg = NULL;
    struct agetargs *current = NULL;
    struct agetargs *arg;
    int argv_len;
    char *p, *q;

    if (style & AARG_LONGARG) {
	q = *argv + 2;
	*next = 0;
    } else if (style & AARG_TRANSLONG) {
	q = *argv + 1;
	*next = 0;
    } else {
	*next = 0;
	q = *argv;
    }

    argv_len = strlen(q);
    p = strchr (q, '=');
    if (p != NULL)
	argv_len = p - q;

    for (arg = args; arg->type ; arg++) {
	/* parse a generic argument if it has not already been filled */
	if (!do_generic && arg->type == aarg_generic_string) {
	    char *hole = (char *)arg->value;

	    if (hole && *hole == '\0')
		do_generic = 1;
	}

	if(do_generic) {
	    generic_arg = arg;
	    optarg = *(argv);
	    *next = 0;
	}

	numarg++;
	if(arg->long_name) {
	    int len = strlen(arg->long_name);
	    char *p = q;
	    int p_len = argv_len;
	    negate = 0;

	    for (;;) {
		if (strncmp (arg->long_name, p, len) == 0) {
		    current = arg;
		    if (style & AARG_TRANSLONG) {
			if (ISFLAG(arg)) {
			    optarg = "";
			    *next = 0;
			} else if (*(argv +1)) {
			    optarg = *(argv + 1);
			    *next = 1;
			} else
			    optarg = "";
		    } else if(p[len] == '\0')
			optarg = p + len;
		    else
			optarg = p + len + 1;
		} else if (strncmp (arg->long_name,
				    p,
				    p_len) == 0) {
		    if (!(style & AARG_USEFIRST) || !partial_match) {
			++partial_match;
			partial = arg;
		    }
		    if (style & AARG_TRANSLONG) {
			if (ISFLAG(arg)) {
			    optarg = "";
			    *next = 0;
			} else if (*(argv + 1)) {
			    optarg = *(argv + 1);
			    *next = 1;
			} else
			    optarg = "";
		    } else
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
	/* Match a generic argument preferentially over a partial match */
	if (generic_arg && (!partial_match || (style & AARG_USEFIRST)))
	    current = generic_arg;
	else if (partial_match == 1)
	    current = partial;
	else
	    return AARG_ERR_NO_MATCH;

	numarg = current - args;
    }

    if(*optarg == '\0' && !ISFLAG(current))
	return AARG_ERR_NO_MATCH;

    *num_arg = numarg;
    return parse_option(current, style, optarg, argc, argv, next, negate);
}

int
agetarg(struct agetargs *args,
	int argc, char **argv, int *optind, int style)
{
    int i, j;
    struct agetargs *arg;
    int ret = 0;
    int swcount = *optind;
    int num_args = 0;
    char *usedargs;

    for(i = 0 ; args[i].type != aarg_end; i++)
	num_args++;
    
    usedargs = calloc (num_args, sizeof(char));
    if (usedargs == NULL)
	return ENOMEM;

    srand (time(NULL));
    (*optind)++;
    for(i = *optind; i < argc; i++) {
	if(argv[i][0] != '-'
	   && swcount != -1
	   && (args[swcount].mandatoryp == aarg_mandatory
	       || args[swcount].mandatoryp == aarg_optional_swless)) {
	    /* the mandatory junk up there is to prevent agetarg() from
	       automatically matching options even when not specified with
	       their flagged name
	    */
	    if (!(style & AARG_SWITCHLESS))
		break;
	    j = 0;
	    ret = parse_option(&args[swcount], style, argv[i],
			       argc - i, &argv[i], &j, 0);
	    if (ret) {
		*optind = i;
		free (usedargs);
		return ret;
	    }
	    usedargs[swcount] = 1;
	    i += j;
	    swcount++;
	} else if(argv[i][1] == '-' || 
		  ((style & AARG_TRANSLONG) && argv[i][1] != 0)) {
	    int k;

	    if(argv[i][2] == 0 && !(style & AARG_TRANSLONG)){
		i++;
		break;
	    }
	    swcount = -1;
	    ret = arg_match_long (args, argc - i, &argv[i], style, &j, &k);
	    if(ret) {
		*optind = i;
		free (usedargs);
		return ret;
	    }
	    usedargs[k] = 1;
	    i += j;
	}else if (style & AARG_SHORTARG && argv[i][0] == '-') {
	    for(j = 1; argv[i][j]; j++) {
		int arg_num = -1;
		for(arg = args; arg->type; arg++) {
		    char *optarg;
		    arg_num++;
		    if(arg->short_name == 0)
			continue;
		    if(argv[i][j] == arg->short_name){
			if(arg->type == aarg_flag){
			    *(int*)arg->value = 1;
			    usedargs[arg_num] = 1;
			    break;
			}
			if(arg->type == aarg_negative_flag){
			    *(int*)arg->value = 0;
			    usedargs[arg_num] = 1;
			    break;
			}
			if(argv[i][j + 1])
			    optarg = &argv[i][j + 1];
			else{
			    i++;
			    optarg = argv[i];
			}
			if(optarg == NULL) {
			    *optind = i - 1;
			    free (usedargs);
			    return AARG_ERR_NO_ARG;
			}
			if(arg->type == aarg_integer){
			    int tmp;
			    if(sscanf(optarg, "%d", &tmp) != 1) {
				*optind = i;
				free (usedargs);
				return AARG_ERR_BAD_ARG;
			    }
			    *(int*)arg->value = tmp;
			    usedargs[arg_num] = 1;
			    goto out;
			}else if(arg->type == aarg_string){
			    *(char**)arg->value = optarg;
			    usedargs[arg_num] = 1;
			    goto out;
			}else if(arg->type == aarg_strings){
			    add_string((agetarg_strings*)arg->value, optarg);
			    usedargs[arg_num] = 1;
			    goto out;
			}
			*optind = i;
			free (usedargs);
			return AARG_ERR_BAD_ARG;
		    }
			
		}
		if (!arg->type) {
		    *optind = i;
		    free (usedargs);
		    return AARG_ERR_NO_MATCH;
		}
	    }
	out:;
	}
    }
    *optind = i;

    for(i = 0 ; args[i].type != aarg_end; i++) {
	if (args[i].mandatoryp == aarg_mandatory && usedargs[i] == 0) {
	    *optind = i;
	    free (usedargs);
	    return AARG_ERR_NO_ARG;
	}
    }

    free (usedargs);
    return 0;
}

#if TEST
int foo_flag = 2;
int flag1 = 0;
int flag2 = 0;
int bar_int;
char *baz_string;

struct agetargs args[] = {
    { NULL, '1', aarg_flag, &flag1, "one", NULL },
    { NULL, '2', aarg_flag, &flag2, "two", NULL },
    { "foo", 'f', aarg_negative_flag, &foo_flag, "foo", NULL },
    { "bar", 'b', aarg_integer, &bar_int, "bar", "seconds"},
    { "baz", 'x', aarg_string, &baz_string, "baz", "name" },
    { NULL, 0, aarg_end, NULL}
};

int main(int argc, char **argv)
{
    int optind = 0;
    while(agetarg(args, 5, argc, argv, &optind))
	printf("Bad arg: %s\n", argv[optind]);
    printf("flag1 = %d\n", flag1);  
    printf("flag2 = %d\n", flag2);  
    printf("foo_flag = %d\n", foo_flag);  
    printf("bar_int = %d\n", bar_int);
    printf("baz_flag = %s\n", baz_string);
    aarg_printusage (args, 5, argv[0], "nothing here");
}
#endif
