/*

minfd.c

Author: David Mazieres <dm@lcs.mit.edu>
	Contributed to be part of ssh.

Copyright (c) 1995 Tatu Ylonen <ylo@cs.hut.fi>, Espoo, Finland
                   All rights reserved

Created: Tue Aug 22 17:25:30 1995 ylo

*/

#include "includes.h"
RCSID("$Id: minfd.c,v 1.2 1999/09/30 05:43:33 deraadt Exp $");

#include <sys/resource.h> /* Needed by fdlim.h */
#include "fdlim.h"
#include "minfd.h"

static int
_get_permanent_fd(const char *shellpath)
{
  const char *shell;
  struct passwd *pwd;
  int fdmin;
  int fdlim;
  int fd;
  int i;

  if (!shellpath) 
    {
      if (!shellpath)
	shellpath = getenv("SHELL");
      if (!shellpath)
	if ((pwd = getpwuid(getuid())))
	  shellpath = pwd->pw_shell;
      if (!shellpath)
	shellpath = _PATH_BSHELL;
    }
  if ((shell = strrchr(shellpath, '/')))
    shell++;
  else
    shell = shellpath;
  
  for (i = 0; strcmp(mafd[i].shell, shell); i++)
    if (i == MAFD_MAX - 1)
      return -1;

  fdmin = mafd[i].fd;
  fdlim = fdlim_get(0);
  
  if (fdmin < fdlim) 
    {
      /* First try to find a file descriptor as high as possible without
	 upping the limit */
      fd = fdlim - 1;
      while (fd >= fdmin)
	{
	  if (fcntl(fd, F_GETFL, NULL) < 0)
	    return fd;
	  fd--;
	}
    }

  fd = fdlim;
  for (;;) 
    {
      if (fdlim_set(fd + 1) < 0)
	return -1;
      if (fcntl(fd, F_GETFL, NULL) < 0)
	break;
      fd++;
    }
  return fd;
}

int
get_permanent_fd(const char *shellpath)
{
  static int fd = -2;

  if (fd >= -1)
    return fd;
  fd = _get_permanent_fd(shellpath);
  if (fd < 0)
    fd = -1;
  return fd;
}
