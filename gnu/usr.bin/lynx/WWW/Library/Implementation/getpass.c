/* Copyright (C) 1991 Free Software Foundation, Inc.
This file is part of the GNU C Library.

The GNU C Library is free software; you can redistribute it and/or
modify it under the terms of the GNU Library General Public License as
published by the Free Software Foundation; either version 2 of the
License, or (at your option) any later version.

The GNU C Library is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
Library General Public License for more details.

You should have received a copy of the GNU Library General Public
License along with the GNU C Library; see the file COPYING.LIB.  If
not, write to the Free Software Foundation, Inc., 675 Mass Ave,
Cambridge, MA 02139, USA.  */

/* CHANGED FOR VMS */

/*
 * <getpass.c>
 */

#include "HTUtils.h"
/*#include <stdio.h>  included by HTUtils.h -- FM */
#include <descrip.h>
#include <psldef.h>
#include <iodef.h>
#include <starlet.h>

#include "LYLeaks.h"

PUBLIC char * getpass ARGS1(CONST char *, prompt)
{
  static char *buf;

  int result;
  $DESCRIPTOR(devnam,"SYS$INPUT");
  int chan;
  int promptlen;
  struct {
     short result;
     short count;
     int   info;
  } iosb;

  promptlen = strlen(prompt);

  buf = (char *)malloc(256);
  if (buf == NULL)
     return(NULL);  

  result = sys$assign(&devnam, &chan, PSL$C_USER, 0, 0);

  result = sys$qiow(0, chan, IO$_READPROMPT | IO$M_PURGE |IO$M_NOECHO, &iosb, 0, 0,
                    buf, 255, 0, 0, prompt, promptlen);

  buf[iosb.count] = '\0';

  result = sys$dassgn(chan);

  return buf;
}
