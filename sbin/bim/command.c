/*	$NetBSD: command.c,v 1.2 1995/03/18 12:28:15 cgd Exp $	*/

/* 
 * Copyright (c) 1994 Philip A. Nelson.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *	This product includes software developed by Philip A. Nelson.
 * 4. The name of Philip A. Nelson may not be used to endorse or promote
 *    products derived from this software without specific prior written
 *    permission.
 *
 * THIS SOFTWARE IS PROVIDED BY PHILIP NELSON ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL PHILIP NELSON BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

/*
 * command.c - The routines to implement the command processor. 
 *
 *   The user must add to "cmdtable.h" all commands that are  available.
 */

#include <stdio.h>
#include <ctype.h>
#include "command.h"
#include "cmdtable.h"


/* This allows the user to define the prompt in cmdtable.h */
#ifndef PROMPT
#define PROMPT "Command (? for help): "
#endif


/* Other procedure prototypes. */
PROTO (void prompt, (char *, int, char *));
PROTO (int parse, (char *, char **));
PROTO (CONST struct command *find_entry, (char *));
PROTO (int StrCmp, (char *, char *));

/* A pointer to a function to be called after every command.
   A NULL pointer says no function is to be executed. 
   The int parameter is the "done" flag.. TRUE => last command */

PROTO (void (*after_cmd), (int)) = NULL;

/* This is the command processor loop. */

command_loop ()
{
  char cmdline [LINELEN];
  char *args [MAXARGS];
  int  numargs;
  CONST struct command *cmd;
  int  done = FALSE;

  int ix;

  while (!done) {
    prompt(cmdline, LINELEN, PROMPT);
    numargs = parse (cmdline, args);
    if (numargs == BLANK_LINE) continue;
    cmd = find_entry (args[0]);
    if (cmd != NULL)
      {
	done = (*(cmd->fn)) (numargs, args, cmd->syntax);
	if (after_cmd != NULL)
	    (*(after_cmd)) (done);
      }
    else
	printf ("Unknown command\n");
  }
}

/* This is the command processor for a single command. */
int
one_command (cmdline)
    char *cmdline;
{
  char *args [MAXARGS];
  int  numargs;
  CONST struct command *cmd;
  int done;

  numargs = parse (cmdline, args);
  if (numargs == BLANK_LINE) return 0;
  cmd = find_entry (args[0]);
  if (cmd != NULL)
    {
	done = (*(cmd->fn)) (numargs, args, cmd->syntax);
	if (after_cmd != NULL)
	    (*(after_cmd)) (done);
	return done;
    }
  else
    return 0;
}

/* prompt procedure....  Write out the prompt and then read
   a response. */

void prompt (cmdline, linelen, promptstr)
    char *cmdline;
    int  linelen;
    char *promptstr;
{
  int incount = 0;
  int inchar;

  /* Give the prompt. */
  printf ("%s", promptstr);
  fflush (stdout);

  /* Read chars until newline or EOF, toss chars that won't fit in the
     array. */
  while (TRUE)
    {
      inchar = getchar();
      if (inchar == '\n' || inchar == EOF) break;
      if (++incount < linelen) *cmdline++ = inchar;
    }
  *cmdline = 0;
}


/* parse the arguments on an input line.  It returns an array of
   pointers to substrings in the original string.  It puts the string
   terminator in the original line. */

int parse (cmdline, args)
    char *cmdline;
    char **args;
{
  int index;
  int argcnt = BLANK_LINE;

  /* Initialize the args. */
  for (index = 0; index < MAXARGS; index++)
    args[index] = NULL;

  /* Start looking for the commands */
  while (*cmdline != 0)
    {
      while (isspace(*cmdline)) cmdline++;  /* skip blanks. */
      if (*cmdline != 0)
        {
	  /* Start of new argument. */
	  if (argcnt < MAXARGS) args[argcnt] = cmdline;
	  while (!isspace(*cmdline) && *cmdline != 0) cmdline++;
	  if (*cmdline != 0) *cmdline++ = 0;
	  argcnt++;			
	}
    }
  return (argcnt < MAXARGS ? argcnt : MAXARGS -1);
}


/* Search cmd_tbl for 1) a command matching what the user typed or 2)
 * a unique command which is a superstring of what the user typed.
 */

CONST struct command *
find_entry (name)
char *name;
{
  CONST struct command *item, *save;
  int subcount = 0;

  for (item = cmd_table; item < cmd_table + CMDLEN; item++)
    switch (StrCmp (name, item->name)) {
      case CMP_MATCH:
	return item;
      case CMP_SUBSTR:
	subcount++;
	save = item;
	break;
      case CMP_NOMATCH:
	break;
    }
  return (subcount == 1)? save : NULL;
}


/* Returns CMP_MATCH if strings are the same, CMP_SUBSTR if p1 is a
 * proper substring of p2, and CMP_NOMATCH if neither.
 */
int
StrCmp (p1, p2)
register char *p1, *p2;
{
  while (*p1 == *p2 && *p1 != '\0') {
    ++p1;
    ++p2;
  }
  if (*p1 == '\0')
    return (*p2 == '\0')? CMP_MATCH: CMP_SUBSTR;
  return CMP_NOMATCH;
}


/* Other routines that may be helpful in evaluating arguments.... 
 */

int
Str2Int (str, num)
     char *str;
     int  *num;
{
  if (!isdigit(*str) && *str != '-') return FALSE;
  *num = atoi (str);
  return TRUE;
}



#ifndef NO_HELP
/* "?" command handler.  Print help and list of commands.
 */
help_cmds ()
{
    CONST struct command *q;

    printf("\nFor additional help, type HELP <command>.  Commands are:\n");
    for (q = cmd_table; q < cmd_table + CMDLEN; ++q) 
        if ((q - cmd_table) % 5 == 4)
            printf ("%s\n", q -> name);
        else
            printf ("%-15s", q -> name);
    if ((q - cmd_table) % 5 != 0)
        printf ("\n");
    printf ("\n");
}

/* The default help routine.  This may be redefined by the user. */
int help (num, args, syntax)
     int num;
     char **args;
     char *syntax;
{
  CONST struct command *item;
  int index;
  int show_cmds = FALSE;
  char dummy[2];

  if (args[0][0] == '?') num = 1;	/* Force "no args" for ?. */

  /* Print help for each argument. */
  if (num > 1)
    for (index = 1; index < num; index++)
      {
	item = find_entry (args[index]);
        if (item != NULL)
	  {
	    printf ("\n");
	    if (item -> syntax[0] != 0)
	      printf ("Syntax: %s\n", item -> syntax);
	    printf ("%s\n\n", item -> help);
	    if (index < num-1) prompt (dummy, 1, "cr to continue:");
	  }
	else
	  {
	    printf ("unknow command %s.\n", args[index]);
	    show_cmds = TRUE;
	  }
      }

  if (show_cmds || num == 1)
    help_cmds ();

  return FALSE;
}
#endif


