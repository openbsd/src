/* Emulate waitpid on systems that just have wait.
   Copyright (C) 1994 Free Software Foundation, Inc.

This file is part of GNU DIFF.

GNU DIFF is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation; either version 2, or (at your option)
any later version.

GNU DIFF is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.  */

#ifndef __VMS_VER
#define __VMS_VER 0
#endif
#ifndef __DECC_VER
#define __DECC_VER 0
#endif

#if __VMS_VER < 70200000 || __DECC_VER < 50700000

#include "vms.h"

#define WAITPID_CHILDREN 8
static pid_t waited_pid[WAITPID_CHILDREN];
static int waited_status[WAITPID_CHILDREN];

pid_t
waitpid (pid, stat_loc, options)
     pid_t pid;
     int *stat_loc;
     int options;
{
  int i;
  pid_t p;

  if (!options  &&  (0 < pid || pid == -1))
    {
      /* If we have already waited for this child, return it immediately.  */
      for (i = 0;  i < WAITPID_CHILDREN;  i++)
	{
	  p = waited_pid[i];
	  if (p  &&  (p == pid  ||  pid == -1))
	    {
	      waited_pid[i] = 0;
	      goto success;
	    }
	}

      /* The child has not returned yet; wait for it, accumulating status.  */
      for (i = 0;  i < WAITPID_CHILDREN;  i++)
	if (! waited_pid[i])
	  {
	    p = wait (&waited_status[i]);
	    if (p < 0)
	      return p;
	    if (p == pid  ||  pid == -1)
	      goto success;
	    waited_pid[i] = p;
	  }
    }

  /* We cannot emulate this wait call, e.g. because of too many children.  */
  abort ();

success:
  if (stat_loc)
    *stat_loc = waited_status[i];
  return p;
}

#else  /*  __VMS_VER >= 70200000 && __DECC_VER >= 50700000  */
#pragma message disable EMPTYFILE
#endif  /*  __VMS_VER >= 70200000 && __DECC_VER >= 50700000  */
