/* This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 2, or (at your option)
   any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.  */

#undef TEST

#include <stdio.h>
#include <iodef.h>
#include <descrip.h>
#include <starlet.h>
#include <string.h>

#ifdef TEST
#include <stdlib.h>
static void error (int, int, char *);
#else
#  include "cvs.h"
#endif

char *
getpass (char *prompt)
{
    int status;
    unsigned short chan;
    static $DESCRIPTOR (sys_command, "SYS$COMMAND");
    unsigned short iosb[4];
    /* Arbitrary limit.  It doesn't seem worth going through multiple
       SYS$QIOW calls and who knows what to get rid of it, I don't
       think.  */
    static char buf[2048];

    /* Try to ensure that we avoid stepping on whatever output has
       been sent to stdout.  */
    printf ("\n");
    fflush (stdout);

    status = sys$assign (&sys_command, &chan, 0, 0);
    if (!(status & 1))
	error (1, 0, "sys$assign failed in getpass");
    status = sys$qiow (0, chan, IO$_READPROMPT | IO$M_NOECHO, &iosb, 0, 0,
		       buf, sizeof (buf) - 1, 0, 0, prompt, strlen (prompt));
    if (!(status & 1))
	error (1, 0, "sys$qiow failed in getpass");
    if (!(iosb[0] & 1))
	error (1, 0, "sys$qiow (iosb) failed in getpass");
    buf[iosb[1]] = '\0';
    status = sys$dassgn (chan);
    if (!(status & 1))
	error (0, 0, "sys$dassgn failed in getpass");
    /* Since there is no echo, we better go to the next line ourselves.  */
    printf ("\n");
    return buf;
}

#ifdef TEST
int
main ()
{
    printf ("thank you for saying \"%s\"\n", getpass ("What'll it be? "));
    return 0;
}

static void error (int x, int y, char *msg)
{
    printf ("error: %s\n", msg);
    if (x)
        exit (EXIT_FAILURE);
}
#endif
