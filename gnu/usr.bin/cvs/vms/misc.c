/*
 * Copyright © 1994 the Free Software Foundation, Inc.
 *
 * Author: Roland B. Roberts (roberts@nsrl.rochester.edu)
 *
 * This file is a part of GNU VMSLIB, the GNU library for porting GNU
 * software to VMS.
 *
 * GNU VMSLIB is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * GNU VMSLIB is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

/*
 * Miscellaneous utilities used by hackargv().
 * Some of these are useful in their own right.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <lib$routines.h>

/* See in misc.h why it is done like this.  */
void x_free (void *block)
{
  free (block);
}

/*
 * Some string utilities.
 */
char *downcase (char *s)
{
  register char *t;
  for (t = s ; *t; t++)
    *t = tolower(*t);
  return (s);
}

char *strndup (char *src, int len) {
  char *dst = (char *) xmalloc (len + 1);
  strncpy (dst, src, len);
  dst[len] = 0;
  return (dst);
}

#include <string.h>

/*
 * int fixpath (char *path)
 *
 * Synopsis:
 *   `Fix' VMS pathnames, converting them to canonical form.
 *
 * Description:
 *   The following conversions are performed
 *     x:[y.][000000.z] --> x:[y.z]
 *     x:[y.][z]        --> x:[y.z]
 *     x:[000000.y]     --> x:[y]
 *
 * Author:
 *   Roland B Roberts (roberts@nsrl.rochester.edu)
 *   March 1994
 */
int fixpath (char *path)
{
  char *s, *d, *t;
  int skip = 0;
  d = s = path;
  if (t = strstr(path ,".][000000"))
    skip = 9;
  else if (t = strstr(path,"]["))
    skip = 2;
  else if (t = strstr(path,"[000000."))
    t++, skip = 7;
  if (t) {
    while (s < t)
      *d++ = *s++;
    s += skip;
    while (*d++ = *s++);
  }
  return 0;
}


#include <ctype.h>
#include <string.h>

#ifndef TRUE
#define TRUE 1
#define FALSE 0
#endif

/*
 * char *argvconcat (int argc, char **argv)
 *
 * Synopsis:
 *   Concatenate all elements of argv into a single string suitable for
 *   use as a command line.
 *
 * Description:
 *   This is intended for use with hackargv() in order to build a command
 *   line for background() or popen().  Each element of argv (except the
 *   first) is surrounded by double quotes to insure the command line is
 *   unaltered when DCL rereads it.
 *
 * Side Effect:
 *   Space for the new string is allocated with xmalloc().
 *
 * Author:
 *   Roland B Roberts (roberts@nsrl.rochester.edu)
 *   March 1994
 */

char *argvconcat (int argc, char **argv)
{
  int i, j, n, addquotes, flags, pid, status;
  char *cmdline;
  /*
   * Allocate space
   */
  for (j = n = 0; j < argc; j++)
    n += 3 + strlen(argv[j]);	/* Need 3 extra spaces, not 1; see below */
  cmdline = (char *) xmalloc ((n + 1) * sizeof (char));
  sprintf (cmdline, "%s ", argv[0]);
  for (j = 1, addquotes = FALSE; j < argc; j++) {
    /*
     * Add double quotes to arg if it contains uppercase of spaces.
     * Hence, the need to allocate three extra spaces for each argument.
     */
    for (i = 0; i < strlen(argv[j]); i++)
      if (isupper(argv[j][i]) || isspace(argv[j][i])) {
	addquotes = TRUE;
	break;
      }
    if (addquotes) {
      strcat (cmdline, argv[j]);
      strcat (cmdline, " ");
    }
    else {
      strcat (cmdline, "\"");	/* use quotes to preserve case */
      strcat (cmdline, argv[j]);
      strcat (cmdline, "\" ");
    }
  }
  cmdline[strlen(cmdline)-1] = 0;
  return (cmdline);
}
