/*

fdlim.h

Author: David Mazieres <dm@lcs.mit.edu>
	Contributed to be part of ssh.

Copyright (c) 1995 Tatu Ylonen <ylo@cs.hut.fi>, Espoo, Finland
                   All rights reserved

Created: Tue Aug 22 17:21:32 1995 ylo

*/

/* RCSID("$Id: fdlim.h,v 1.2 1999/09/30 05:43:33 deraadt Exp $"); */

#ifndef FDLIM_H
#define FDLIM_H

static int
fdlim_get (int hard)
{
  struct rlimit rlfd;

  if (getrlimit (RLIMIT_NOFILE, &rlfd) < 0)
    return (-1);
  if ((hard ? rlfd.rlim_max : rlfd.rlim_cur) == RLIM_INFINITY)
    return 10000;
  else
    return hard ? rlfd.rlim_max : rlfd.rlim_cur;
}

static int
fdlim_set (int lim) {
  struct rlimit rlfd;
  if (lim <= 0)
    return (-1);
  if (getrlimit (RLIMIT_NOFILE, &rlfd) < 0)
    return (-1);
  rlfd.rlim_cur = lim;
  if (setrlimit (RLIMIT_NOFILE, &rlfd) < 0)
    return (-1);
  return (0);
}

#endif /* FDLIM_H */
