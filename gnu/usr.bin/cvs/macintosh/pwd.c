/*  pwd.c - Try to approximate UN*X's getuser...() functions under MS-DOS.
    Copyright (C) 1990 by Thorsten Ohl, td12@ddagsi3.bitnet

    This program is free software; you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation; either version 1, or (at your option)
    any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

*/

/* This 'implementation' is conjectured from the use of this functions in
   the RCS and BASH distributions.  Of course these functions don't do too
   much useful things under MS-DOS, but using them avoids many "#ifdef
   MSDOS" in ported UN*X code ...  */
   
/* Stripped out stuff - MDLadwig <mike@twinpeaks.prc.com> --- Nov 1995 */

#include "mac_config.h"
#include <pwd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static char *home_dir = ".";	/* we feel (no|every)where at home */
static struct passwd pw;	/* should we return a malloc()'d structure   */
static struct group gr;		/* instead of pointers to static structures? */

pid_t getpid( void ) { return 0; }					/* getpid */
pid_t waitpid(pid_t, int *, int) { return 0; }	/* waitpid */

mode_t	umask(mode_t) { return 0; }				/* Umask */

/* return something like a username in a (butchered!) passwd structure. */

struct passwd *
getpwuid (int uid)
{
  pw.pw_name = NULL; /* getlogin (); */
  pw.pw_dir = home_dir;
  pw.pw_shell = NULL;
  pw.pw_uid = 0;

  return &pw;
}

/* Misc uid stuff */

struct passwd * getpwnam (char *name) { return (struct passwd *) 0; }
int getuid () { return 0; }
int geteuid () { return 0; }
int getegid () { return 0; }

