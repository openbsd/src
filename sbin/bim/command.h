/*	$NetBSD: command.h,v 1.2 1995/03/18 12:28:17 cgd Exp $	*/

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
 * Command tool - a library for building command languages fast.
 *
 *   Philip A. Nelson,  Oct 1991
 *
 *   Some code is lifted from the PC532 monitor/debugger written
 *   by Bruce Culbertson.   (Thanks Bruce!)
 */

/* Defines that depend on ANSI or not ANSI. */

#ifdef __STDC__
#define CMD_PROC(name) int name (int, char **, char *)
#define PROTO(name,args) name args
#define CONST const
#else
#define CMD_PROC(name) name ();
#define PROTO(name,args) name ()
#define CONST 
#endif


/*  The commands are stored in a table that includes their name, a pointer
    to the function that processes the command and a help message.  */

struct command {	/* The commands, their names, help */
  PROTO (int (*fn), (int, char **, char *));
  char *name;
  char *syntax;
  char *help;
};

/*  The command loop will do the following:
	a) prompt the user for a command.
	b) read the command line.
	c) break the input line into arguments.
	d) search for the command in the command table.
	e) If the command is found, call the routine to process it.
        f) IF the return value from the command is NON ZERO, exit the loop.

Each function to process a command must be defined as follows:

   int name ( int num, char ** cmd_args ) where num is the number
   of arguments (the command name is not counted) and the cmd_args is
   an array of pointers to the arguments.  cmd_args[0] is the command
   name.  cmd_args[1] is the first argument.

*/


/* Constants defining the limits of the command processor.
 */
#define TRUE 1
#define FALSE 0
#define LINELEN 256
#define MAXARGS 16
#define BLANK_LINE 0

/* Stuff for myStrCmp()
 */
#define CMP_NOMATCH	0
#define CMP_MATCH	1
#define CMP_SUBSTR	2
