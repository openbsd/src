/*
 * Copyright (c) 1995, 1996, 1997, 1998, 1999 Kungliga Tekniska Högskolan
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
#include <sl.h>
#include "bos_local.h"

RCSID("$arla: bos.c,v 1.11 2003/01/17 03:24:48 lha Exp $");

int bos_interactive = 0;

static int empty_cmd(int argc, char **argv);
static int quit_cmd(int argc, char **argv);
static int help_cmd(int argc, char **argv);
static int apropos_cmd(int argc, char **argv);


/*
 * command table
 */

static SL_cmd cmds[] = {
    {"addhost",		bos_addhost,	"add host to cell database"},
    {"addkey",		empty_cmd,	"not yet implemented"},
    {"adduser",		bos_adduser,	"add users to super-user list"},
    {"apropos",		apropos_cmd,	"apropos help"},
    {"create",		empty_cmd,	"not yet implemented"},
    {"delete",		empty_cmd,	"not yet implemented"},
    {"exec",		empty_cmd,	"not yet implemented"},
    {"exit",		quit_cmd,    	"exit interactive mode"},
    {"getdate",		empty_cmd,	"not yet implemented"},
    {"getlog",		empty_cmd,	"not yet implemented"},
    {"getrestart",	bos_getrestart,	"get restart times"},
    {"help",		help_cmd,	"print help"},
    {"install",		empty_cmd,	"not yet implemented"},
    {"listhosts",	bos_listhosts,	"list VLDB-servers"},
    {"listkeys",	empty_cmd,	"not yet implemented"},
    {"listusers",	bos_listusers,	"list super-users"},
    {"prune",		empty_cmd,	"not yet implemented"},
    {"removehost",	bos_removehost,	"remove host from cell database"},
    {"removekey",	empty_cmd,	"not yet implemented"},
    {"removeuser",	empty_cmd,	"not yet implemented"},
    {"restart",		bos_restart,	"restarts an instace"},
    {"salvage",		empty_cmd,	"not yet implemented"},
    {"setauth",		empty_cmd,	"not yet implemented"},
    {"setcellname",	empty_cmd,	"not yet implemented"},
    {"setrestart",	empty_cmd,	"not yet implemented"},
    {"shutdown",	empty_cmd,	"not yet implemented"},
    {"start",		bos_start,	"start a server instance"},
    {"status",		bos_status,
     "Show volume server transactions"},
    {"stop",		bos_stop,	"stop a server instance"},
    {"version",		arlalib_version_cmd, "print version"},
    {"uninstall",	empty_cmd,	"not yet implemented"},
    {"quit",		quit_cmd,	"exit interactive mode"},
    {NULL}
};

/*
 * Dummy command
 */

static int
empty_cmd(int argc, char **argv)
{
    printf("%s%s has not been implemented yet!\n", PROGNAME, argv[0]);
    return 0;
}

/*
 * quit
 */

static int
quit_cmd(int argc, char **argv)
{
    printf("exiting\n");
    return 1;
}

/*
 * help
 */

static int
help_cmd (int argc, char **argv)
{
  SL_cmd *cmd;

  for (cmd = cmds; cmd->name != NULL; ++cmd)
    if (cmd->usage != NULL)
      printf ("%-20s%s\n", cmd->name, cmd->usage);

  return 0;
}

/*
 * apropos
 */

static int
apropos_cmd(int argc, char **argv)
{
    if (argc == 0) {
	fprintf (stderr, "apropos: missing topic");
	return 0;
    }
    
    sl_apropos(cmds, argv[1]);
    return 0;
}


/*
 * Main program
 */

int
main(int argc, char **argv)
{
    Log_method *method;
    int ret = 0;
    
    method = log_open (__progname, "/dev/stderr:notime");
    if (method == NULL)
	errx (1, "log_open failed");
    cell_init(0, method);
    ports_init();
    
    if (argc > 1)
	ret = sl_command(cmds, argc - 1, argv + 1);
    else {
	bos_interactive = 1;
	printf("bos - an arla tool for administrating AFS-servers.\n");
	printf("Type \"help\" to get a list of commands.\n");
	ret = sl_loop(cmds, __progname": ");
    }
    return ret;
}
