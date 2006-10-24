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

#include <sys/types.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <strings.h>
#include <assert.h>
#include <ctype.h>
#include <err.h>
#include <roken.h>

#include <cmd.h>

#ifdef RCSID
RCSID("$KTH: cmd.c,v 1.11 2000/09/29 22:43:02 assar Exp $");
#endif

static struct cmd_syndesc *cmd_cmds = NULL;
static struct cmd_syndesc **cmd_lastcmd = &cmd_cmds;
static int _cmd_init = 0;

static int (*before_proc) (void *rock) = NULL;
static void *before_rock = NULL;
static int (*after_proc) (void *rock) = NULL;
static void *after_rock = NULL;

static void
cmd_PrintSyntaxManDoc (const char *commandname);


static struct cmd_syndesc *
new_cmd (const char *name, const char *help_str, struct cmd_syndesc *alias)
{
    struct cmd_syndesc *ts;
    
    ts = malloc (sizeof(*ts));
    if (ts == NULL)
	return NULL;
    memset (ts, 0, sizeof (*ts));
    
    if (name) {
	ts->name = strdup (name);
	if (ts->name == NULL) {
	    free (ts);
	    return NULL;
	}
    }
    if (help_str) {
	ts->help = strdup (help_str);
	if (ts->help == NULL) {
	    free (ts->name);
	    free (ts);
	    return NULL;
	}
    }
    if (alias) {
	ts->aliasOf = alias;
	ts->nextAlias = alias->nextAlias;
	alias->nextAlias = ts;
	ts->flags |= CMD_ALIAS;
    }
    if (ts->name == NULL) {
	assert (cmd_cmds == NULL || cmd_cmds->name != NULL);
	ts->next = cmd_cmds;
	if (&cmd_cmds == cmd_lastcmd)
	    cmd_lastcmd = &ts->next;
	cmd_cmds = ts;
    } else {
	*cmd_lastcmd = ts;
	cmd_lastcmd = &ts->next;
    }
    return ts;
}

/*
 *
 */

static int
find_command (struct cmd_syndesc *cmd_cmds, const char *func, 
	      struct cmd_syndesc **ret)
{
    struct cmd_syndesc *partial_match = NULL;
    struct cmd_syndesc *ts;
    int partial_len = strlen (func);

    for (ts = cmd_cmds; ts; ts = ts->next) {
	if (ts->name == NULL)
	    continue;
	if (strcasecmp (ts->name, func) == 0) {
	    if (ts->aliasOf)
		ts = ts->aliasOf;
	    *ret = ts;
	    return 0;
	}
	if (strncasecmp (ts->name, func, partial_len) == 0) {
	    if (partial_match)
		return CMD_AMBIG;
	    partial_match = ts;
	}
    }
    if (partial_match) {
	if (partial_match->aliasOf)
	    partial_match = partial_match->aliasOf;
	*ret = partial_match;
	return 0;
    }
    return CMD_UNKNOWNCMD;
}

/*
 *
 */

static void
print_usage (const char *name, struct cmd_syndesc *ts)
{
    int i;
    
    if (getenv ("CMD_MANDOC") != NULL) {
	cmd_PrintSyntaxManDoc(__progname); /* XXX */
	return;
    }

    printf ("%s", name);
    if (ts->name)
	printf (" %s", ts->name);

    for (i = 0; i < CMD_MAXPARMS ; i++) {
	char *help = ts->parms[i].help;
	char *name = ts->parms[i].name;
	if (help == NULL)
	    help = "arg";
	if ((ts->parms[i].flags & CMD_HIDE) == CMD_HIDE || name == NULL)
	    continue;
	printf (" ");
	if ((ts->parms[i].flags & CMD_OPTIONAL) == CMD_OPTIONAL)
	    printf ("[");
	switch (ts->parms[i].type) {
	case CMD_FLAG:
	    printf ("%s", name);
	    break;
	case CMD_SINGLE:
	    printf ("%s <%s>", name, help);
	    break;
	case CMD_LIST:
	    printf ("%s <%s>+", name, help);
	    break;
	default:
	    errx(-1, "print_usage - unknown command type\n");
	    /* NOTREACHED */
	}
	if ((ts->parms[i].flags & CMD_OPTIONAL) == CMD_OPTIONAL)
	    printf ("]");
    }
    if (ts->name == NULL)
	printf (" command");
    printf ("\n");
}

/*
 *
 */

static int
cmd_internal_help (struct cmd_syndesc *t, void *ptr)
{
    struct cmd_syndesc *ts;
    char *help;

    if (getenv ("CMD_MANDOC") != NULL) {
	cmd_PrintSyntaxManDoc(__progname); /* XXX */
	return 0;
    }

    if (t->parms[0].items == NULL) {
	
	for (ts = cmd_cmds; ts; ts = ts->next) {
	    if (ts->name == NULL && cmd_cmds == ts)
		continue;
	    if ((ts->flags & CMD_HIDDEN) == CMD_HIDDEN)
		continue;
	    if (ts->aliasOf)
		continue;
	    if (ts->help)
		help = ts->help;
	    else
		help = "";
	    
	    printf ("%-10s %s\n", ts->name, help);
	}    
    } else {
	char *cmd = t->parms[0].items->data;
	struct cmd_syndesc *ts;
	int ret;
	
	ret = find_command (cmd_cmds, cmd, &ts);
	if (ret)
	    return ret;
	print_usage (__progname, ts); /* XXX */
    }
    return 0;
}

/*
 *
 */

static int
cmd_internal_apropos (struct cmd_syndesc *t, void *ptr)
{
    struct cmd_syndesc *ts;
    char *cmd, *help;

    cmd = t->parms[0].items->data;

    for (ts = cmd_cmds; ts; ts = ts->next) {
	if (ts->name == NULL && cmd_cmds == ts)
	    continue;
	if (ts->help)
	    help = ts->help;
	else
	    help = "";
	if (strstr(ts->name, cmd))
	    printf ("%-10s %s\n", ts->name, help);
    }    
    return 0;
}

/*
 * Create a new command with `name' that will run the function `main'
 * with the `rock'. When help is choose `help_str' will be displayed.
 */

struct cmd_syndesc *
cmd_CreateSyntax (const char *name, cmd_proc main, void *rock, 
		  const char *help_str)
{
    struct cmd_syndesc *ts;

    ts = new_cmd (name, help_str, NULL);

    ts->proc = main;
    ts->rock = rock;

    cmd_Seek (ts, CMD_HELPPARM);
    cmd_AddParm (ts, "-help", CMD_FLAG, CMD_OPTIONAL, "get detailed help");
    cmd_Seek (ts, 0);

    return ts;
}

/*
 *
 */

int
cmd_SetBeforeProc (int (*proc) (void *), void *rock)
{
    before_proc = proc;
    before_rock = rock;
    return 0;
}

/*
 *
 */

int
cmd_SetAfterProc (int (*proc) (void *rock), void *rock)
{
    after_proc = proc;
    after_rock = rock;
    return 0;
}

/*
 * Allocate a new parameter to `ts' with name `cmd' of `type' and with
 * `flags'. When help is needed show `help_str'.
 */

void
cmd_AddParm (struct cmd_syndesc *ts, const char *name, 
	     cmd_parmdesc_type type, cmd_parmdesc_flags flags,
	     const char *help_str)
{
    struct cmd_parmdesc *p = &ts->parms[ts->nParams];

    memset (p, 0, sizeof(*p));

    if (ts->nParams > CMD_HELPPARM)
        return;

    p->name = strdup (name);
    if (p->name == NULL)
	return;
    p->help = strdup(help_str);
    if (p->help == NULL) {
	free (p->name);
	return;
    }
    ts->nParams++;
    
    p->type = type;
    p->flags = flags;
}

/*
 *
 */

int
cmd_CreateAlias (struct cmd_syndesc *orignal, const char *name)
{
    struct cmd_syndesc *ts;
    ts = new_cmd (name, NULL, orignal);
    if (ts == NULL)
	return CMD_INTERALERROR;
    return 0;
}

/*
 *
 */

int
cmd_Seek (struct cmd_syndesc *ts, int pos)
{
    if (pos > CMD_HELPPARM || pos < 0)
	return CMD_EXCESSPARMS;
    ts->nParams = pos;
    return 0;
}

/*
 *
 */

void
cmd_FreeArgv (char **argv)
{
    return;
}

/*
 *
 */

#define CMD_IS_CMD(str) (str[0] == '-' && !isdigit((unsigned char)str[1]))

/*
 *
 */

static struct cmd_parmdesc *
find_next_param (struct cmd_syndesc *ts, int *curval)
{
    int i;
    for (i = *curval; i < CMD_MAXPARMS ; i++) {
	if ((ts->parms[i].flags & CMD_EXPANDS) == CMD_EXPANDS)
	    break;
	if (ts->parms[i].name == NULL)
	    continue;
	if (ts->parms[i].items != NULL)
	    continue;
	break;
    }
    if (i >= CMD_MAXPARMS)
	return NULL;
    *curval = i;
    return &ts->parms[i];
}

/*
 *
 */

static int
find_next_name (struct cmd_syndesc *ts, const char *name, 
		struct cmd_parmdesc **ret)
{
    struct cmd_parmdesc *partial_match = NULL;
    int i, partial_len = strlen(name);

    for (i = 0; i < CMD_MAXPARMS; i++) {
	if (ts->parms[i].name == NULL)
	    continue;
	if (strcasecmp (name, ts->parms[i].name) == 0) {
	    *ret = &ts->parms[i];
	    return 0;
	}
	if (strncasecmp (name, ts->parms[i].name, partial_len) == 0) {
	    if (partial_match)
		return CMD_AMBIG;
	    partial_match = &ts->parms[i];
	}
    }
    if (partial_match) {
	*ret = partial_match;
	return 0;
    }
    return CMD_UNKNOWNSWITCH;
}

/*
 *
 */

static struct cmd_item *
new_items (struct cmd_parmdesc *p)
{
    struct cmd_item *i;
    i = malloc (sizeof (*i));
    if (i == NULL)
	err (1, "new_items");
    i->next = p->items;
    p->items = i;
    return i;
}

/*
 *
 */

static int
parse_options (int argc, char **argv, int *optind, struct cmd_syndesc *ts,
	       int failp)
{    
    struct cmd_parmdesc *p;
    int i, ret;

    for (i = 0; i < CMD_MAXPARMS; i++) {
	if (ts->parms[i].name)
	    break;
    }
    if (i == CMD_MAXPARMS)
	return 0;


    for (i = 0; i < argc; i++) {
	ret = find_next_name (ts, argv[i], &p);
	if (ret) {
	    if (failp)
		return ret;
	    break;
	}
	switch (p->type) {
	case CMD_FLAG:
	    assert (p->items == NULL);
	    p->items = new_items (p);
	    p->items->data = p;
	    break;
	case CMD_SINGLE:
	    assert (p->items == NULL);
	    if (i == argc - 1)
		return CMD_TOOFEW;
	    i++;
	    p->items = new_items (p);
	    p->items->data = argv[i];
	    break;
	case CMD_LIST:
	    if (i == argc - 1)
		return CMD_TOOFEW;
	    i++;
	    while (i < argc) {
		p->items = new_items (p);
		p->items->data = argv[i];
		if (i >= argc - 1 || CMD_IS_CMD(argv[i+1]))
		    break;
		i++;
	    }
	    break;
	default:
	    errx(-1, "parse_options - unknown command type\n");
	    /* NOTREACHED */
	}
    }
    if (argc < i)
	i = argc;
    *optind = i;
    return 0;
}

/*
 *
 */

static int
parse_magic_options (int argc, char **argv, int *optind,
		     struct cmd_syndesc *ts)
{    
    struct cmd_parmdesc *p;
    int i, curval = 0;

    for (i = 0; i < argc; i++) {
	if (CMD_IS_CMD(argv[i]))
	    break;
	p = find_next_param (ts, &curval);
	if (p == NULL)
	    break;
	if (p->type == CMD_FLAG) {
#if 0
	    assert (p->items == NULL);
	    p->items = new_items (p);
	    p->items->data = p;
#else
	    break;
#endif
	} else if (p->type == CMD_SINGLE) {
	    assert (p->items == NULL);
	    p->items = new_items (p);
	    p->items->data = argv[i];
	} else if (p->type == CMD_LIST) {
	    while (i < argc) {
		p->items = new_items (p);
		p->items->data = argv[i];
		if ((i >= argc - 1 || CMD_IS_CMD(argv[i+1]))
		    || (p->flags & CMD_EXPANDS) == 0)
		    break;
		i++;
	    }
	} else {
	    break;
	}
    }
    if (argc < i)
	i = argc;
    *optind = i;
    return 0;
}

/*
 *
 */

static struct cmd_parmdesc *
req_argumentp (struct cmd_syndesc *ts)
{
    int i;
    
    for (i = 0; i < CMD_MAXPARMS; i++) {
	if (ts->parms[i].name == NULL)
	    continue;
	if ((ts->parms[i].flags & CMD_OPTIONAL) == 0
	    && ts->parms[i].items == NULL)
	    return &ts->parms[i];
    }
    return NULL;
}

/*
 *
 */

int
cmd_ParseLine (const char *line, char **argv, int *n, int maxn)
{
    return 0;
}

/*
 *
 */

static int
call_proc (const char *programname, struct cmd_syndesc *ts)
{
    struct cmd_parmdesc *p;
    int ret;

    if (ts->parms[CMD_HELPPARM].items) {
	print_usage (programname, ts);
	return 0;
    }

    p = req_argumentp (ts);
    if (p) {
	print_usage (programname, ts);
	fprintf (stderr, "missing argument: %s\n", &p->name[1]);
	return CMD_USAGE;
    }

    if (before_proc)
	(*before_proc) (before_rock);

    ret = (*ts->proc) (ts, ts->rock);
    
    if (after_proc)
	(*after_proc) (before_rock);
    return ret;
}

/*
 *
 */

int
cmd_Dispatch (int argc, char **argv)
{
    struct cmd_syndesc *ts;
    int optind = 0;
    int ret;
    char *programname = argv[0];

    argc -= 1;
    argv += 1;

    if (!_cmd_init) {
	if (cmd_cmds->next != NULL || cmd_cmds->proc == NULL) {
	    ts = cmd_CreateSyntax ("help", cmd_internal_help, NULL, "help");
	    cmd_AddParm (ts, "-cmd", CMD_SINGLE, CMD_OPTIONAL, "command");
	    ts = cmd_CreateSyntax ("apropos", cmd_internal_apropos,
				   NULL, "apropos");
	    cmd_AddParm (ts, "-cmd", CMD_SINGLE, CMD_REQUIRED, "command");
	}
	if (cmd_cmds->name)
	    ts = cmd_CreateSyntax ("main", NULL, NULL, NULL);
	_cmd_init = 1;
    }

    if (cmd_cmds->next == NULL && cmd_cmds->proc) {
	ret = parse_magic_options (argc, argv, &optind, ts);
	if (ret)
	    return ret;
	
	argv += optind;
	argc -= optind;
	optind = 0;
    }
    ret = parse_options (argc, argv, &optind, cmd_cmds, 0);
    if (ret)
	errx (1, "main parse failed");
    if (cmd_cmds->next == NULL && cmd_cmds->proc) {
	return call_proc (programname, cmd_cmds);
    } else {
	struct cmd_parmdesc *p;

	if (cmd_cmds->parms[CMD_HELPPARM].items) {
	    print_usage (programname, cmd_cmds);
	    return 0;
	}

	p = req_argumentp (cmd_cmds);
	if (p) {
	    fprintf (stderr, "missing argument: %s\n", &p->name[1]);
	    return CMD_USAGE;
	}
    }

    argv += optind;
    argc -= optind;

    if (argc <= 0) {
	print_usage (programname, ts);
	return CMD_USAGE;
    }

    ret = find_command (cmd_cmds, argv[0], &ts);
    if (ret)
	return ret;

    argv += 1;
    argc -= 1;
    optind = 0;

    ret = parse_magic_options (argc, argv, &optind, ts);
    if (ret)
	return ret;

    argv += optind;
    argc -= optind;
    optind = 0;

    ret = parse_options (argc, argv, &optind, ts, 1);
    if (ret)
	return ret;

    argv += optind;
    argc -= optind;

    if (argc != 0) {
	print_usage (programname, ts);
	return CMD_USAGE;
    }

    return call_proc (programname, ts);
}


static void
cmd_PrintParams (struct cmd_syndesc *ts)
{
    int i;

    for (i = 0; i < CMD_MAXPARMS ; i++) {
	char *help = ts->parms[i].help;
	if (ts->parms[i].name == NULL)
	    continue;
	if (help == NULL)
	    help = "arg";
	if ((ts->parms[i].flags & CMD_HIDE) == CMD_HIDE)
	    continue;
	if ((ts->parms[i].flags & CMD_OPTIONAL) == CMD_OPTIONAL)
	    printf (".Op ");
	else
	    printf (".");
	switch (ts->parms[i].type) {
	case CMD_FLAG: 
	    printf ("Cm %s\n", ts->parms[i].name);
	    break;
	case CMD_SINGLE:
	    printf ("Cm %s Ar %s\n", ts->parms[i].name, help);
	    break;
	case CMD_LIST:
	    printf ("Cm %s Ar %s ...\n", ts->parms[i].name, help);
	    break;
	default:
	    errx(-1, "cmd_PrintParams - unknown command type\n");
	    /* NOTREACHED */

	}
    }
#if 0
	for (i = 0; i < CMD_MAXPARMS ; i++) {
	    if (ts->parms[i].name == NULL)
		continue;
	    printf ("\tflag: %s\n", ts->parms[i].name);
	    printf ("\t\thelp: %s\n", ts->parms[i].help);
	    printf ("\t\tflags: ");
	    if ((ts->parms[i].flags & CMD_REQUIRED) == CMD_REQUIRED)
		printf ("required");
	    if ((ts->parms[i].flags & CMD_EXPANDS) == CMD_EXPANDS)
		printf ("expands");
	    if ((ts->parms[i].flags & CMD_PROCESSED) == CMD_PROCESSED)
		printf ("processed");
	    printf ("\n");
	}
#endif
}

/*
 *
 */

static int
cmd_ExtraText (const char *cmd, const char *sectionname,
	       const char *class, const char *name, int withcr)
{
    char *fn, *section;
    char buf[1024];
    FILE *f;
    int len, searching = 1;

    asprintf (&fn, "%s.ctx", cmd);
    f = fopen (fn, "r");
    if (f == NULL) {
	free (fn);
	if (getenv ("srcdir")) {
	    asprintf (&fn, "%s/%s.ctx", getenv("srcdir"), cmd);
	    f = fopen (fn, "r");
	}
	if (f == NULL)
	    return 0;
    }
    len = asprintf (&section, "%%%s %s", class, name);
    
    while (fgets (buf, sizeof(buf), f) != NULL) {
	if (buf[0] != '\0' && buf[strlen(buf) - 1] == '\n')
	    buf[strlen(buf) - 1] = '\0';
	
	if (buf[0] == '#')
	    continue;

	if (searching) {
	    if (strncasecmp (buf, section, len) == 0) {
		searching = 0;
		if (sectionname)
		    printf ("%s", sectionname);
	    }
	} else {
	    char *p = buf;
	    if (buf[0] == '%') {
		break;
	    }
	    while (isspace ((unsigned char)*p)) p++;
	    if (*p == '\0')
		continue;
	    printf ("%s%s", p, withcr ? "\n" : "");
	}
    }
    fclose (f);
    return searching;
}

static void
cmd_PrintSyntaxManDoc (const char *commandname)
{
    char timestr[64], cmd[64];
    time_t t;
    struct cmd_syndesc *ts;
    
    printf(".\\\" Things to fix:\n");
    printf(".\\\"   * use better macros for arguments (like .Pa for files)\n");
    printf(".\\\"\n");

    t = time(NULL);
    strftime(timestr, sizeof(timestr), "%B %e, %Y", localtime(&t));
    strlcpy(cmd, commandname, sizeof(cmd));
    cmd[sizeof(cmd)-1] = '\0';
/* XXX-libroken    strupr(cmd);*/
       
    printf(".Dd %s\n", timestr);
    printf(".Dt %s ", cmd);
    if (cmd_ExtraText (commandname, NULL, "name", "section", 0))
	printf("SECTION");
    printf ("\n");
    if (cmd_ExtraText (commandname, ".Os ", "name", "OS", 0))
	printf(".Os OPERATING_SYSTEM");
    printf ("\n");
    printf(".Sh NAME\n");
    printf(".Nm %s\n", commandname);
    if (cmd_ExtraText (commandname, ".Nd ", "name", "description", 1))
	printf(".Nd in search of a description\n");
    printf(".Sh SYNOPSIS\n");
    printf(".Nm\n");
    if (cmd_cmds->name == NULL)
	cmd_PrintParams (cmd_cmds);
    if (cmd_cmds->next)
	printf (".Cm command\n.Op ...");
    printf ("\n");
    printf(".Sh DESCRIPTION\n");

    cmd_ExtraText (commandname, NULL, "section", "description", 1);

    if (cmd_cmds->name == NULL && cmd_cmds->next == NULL) {
	printf (".Pp\n");
	if (cmd_ExtraText (commandname, NULL, "command", "_main", 1))
	    printf ("Tell someone to add some helptext "
		    "here to the .ctx file\n");
    } else {
	printf (".Bl -tag -width Ds\n");
	
	for (ts = cmd_cmds; ts; ts = ts->next) {
	    struct cmd_syndesc *t = ts->nextAlias;
	    
	    if (ts->name == NULL && cmd_cmds == ts)
		continue;
	    if ((ts->flags & CMD_HIDDEN) == CMD_HIDDEN)
		continue;
	    if (ts->aliasOf)
		continue;
	    
	    printf (".It %s", ts->name);
	    
	    if (t) {
		while (t) {
		    printf (", %s", t->name);
		    t = t->nextAlias;
		}
	    }
	    printf ("\n.Pp\n");
	    printf (".Nm \"%s %s\"\n", commandname, ts->name);
	    cmd_PrintParams (ts);
	    printf (".Pp\n");
	    if (strcmp (ts->name, "help") == 0) {
		printf ("List all commands, or if\n"
			".Ar command\n"
			"is given as an argument, help for that command.\n");
	    } else if (strcmp (ts->name, "apropos") == 0) {
		printf ("List all command that have the argument\n.Ar command\n"
			"as a subtring in themself.");
	    } else {
		if (cmd_ExtraText (commandname, NULL, "command", ts->name, 1))
		    printf ("Tell someone to add some helptext "
			    "here to the .ctx file\n");
	    }
	}
	printf(".El\n");
    }

    cmd_ExtraText (commandname, ".Sh ERRORS\n", "section", "errors", 1);
    cmd_ExtraText (commandname, ".Sh SEE ALSO\n", "section", "see also", 1);
    cmd_ExtraText (commandname, ".Sh HISTORY\n", "section", "history", 1);
    cmd_ExtraText (commandname, ".Sh AUTHORS\n", "section", "authors", 1);
    cmd_ExtraText (commandname, ".Sh BUGS\n", "section", "bugs", 1);
}

/*
 *
 */

void
cmd_PrintSyntax (const char *commandname)
{
    const char *name;
    struct cmd_syndesc *ts;
    int i;
    
    name = strrchr (commandname, '/');
    if (name == NULL)
	name = commandname;
    else
	name++;

    if (getenv ("CMD_MANDOC") != NULL) {
	cmd_PrintSyntaxManDoc(name);
	return;
    }

    for (ts = cmd_cmds; ts; ts = ts->next) {
	struct cmd_syndesc *t = ts->nextAlias;

	if (ts->name == NULL && cmd_cmds == ts)
	    printf ("exec-file: %s\n", name);
	else
	    printf ("command: %s - %s\n", ts->name, ts->help);
	printf ("\tflags: ");
	if ((ts->flags & CMD_ALIAS) == CMD_ALIAS)
	    printf (" alias");
	if ((ts->flags & CMD_HIDDEN) == CMD_HIDDEN)
	    printf (" hidden");
	printf ("\n");
	if (ts->aliasOf)
	    continue;
	if (t) {
	    printf ("\taliases:");
	    while (t) {
		printf (" %s", t->name);
		t = t->nextAlias;
	    }
	    printf ("\n");
	}
	for (i = 0; i < CMD_MAXPARMS ; i++) {
	    if (ts->parms[i].name == NULL)
		continue;
	    printf ("\tflag: %s\n", ts->parms[i].name);
	    switch (ts->parms[i].type) {
	    case CMD_FLAG: printf ("\t\ttype: flag\n"); break;
	    case CMD_SINGLE: printf ("\t\ttype: single\n"); break;
	    case CMD_LIST: printf ("\t\ttype: list\n"); break;
	    default:
	        errx(-1, "cmd_PrintSyntax - unknown command type\n");
		/* NOTREACHED */
	    }
	    printf ("\t\thelp: %s\n", ts->parms[i].help);
	    printf ("\t\tflags: ");
	    if ((ts->parms[i].flags & CMD_REQUIRED) == CMD_REQUIRED)
		printf ("required");
	    if ((ts->parms[i].flags & CMD_OPTIONAL) == CMD_OPTIONAL)
		printf ("optional");
	    if ((ts->parms[i].flags & CMD_EXPANDS) == CMD_EXPANDS)
		printf ("expands");
	    if ((ts->parms[i].flags & CMD_HIDE) == CMD_HIDE)
		printf ("hide");
	    if ((ts->parms[i].flags & CMD_PROCESSED) == CMD_PROCESSED)
		printf ("processed");
	    printf ("\n");
	}
    }
}

/*
 *
 */

static const char *cmd_errors[] = {
    "cmd - Excess parameters",
    "cmd - Internal error",
    "cmd - Notlist",
    "cmd - Too many", 
    "cmd - Usage",
    "cmd - Unknown command",
    "cmd - Unknown switch",
    "cmd - Ambigous",
    "cmd - Too few arguments",
    "cmd - Too many arguments"
};

const char *
cmd_number2str(int error)
{
    if (error < CMD_EXCESSPARMS || error > CMD_TOOBIG)
	return NULL;
    return cmd_errors[error - CMD_EXCESSPARMS];
}
