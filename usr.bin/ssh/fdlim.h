/*

fdlim.h

Author: David Mazieres <dm@lcs.mit.edu>
	Contributed to be part of ssh.

Copyright (c) 1995 Tatu Ylonen <ylo@cs.hut.fi>, Espoo, Finland
                   All rights reserved

Created: Tue Aug 22 17:21:32 1995 ylo

*/

/* RCSID("$Id: fdlim.h,v 1.1 1999/09/26 20:53:36 deraadt Exp $"); */

#ifndef FDLIM_H
#define FDLIM_H

static int
fdlim_get (int hard)
{
#ifdef RLIMIT_NOFILE
  struct rlimit rlfd;
  if (getrlimit (RLIMIT_NOFILE, &rlfd) < 0)
    return (-1);
#ifdef RLIM_INFINITY /* not defined on HPSUX */
  if ((hard ? rlfd.rlim_max : rlfd.rlim_cur) == RLIM_INFINITY)
    return 10000;
  else
    return hard ? rlfd.rlim_max : rlfd.rlim_cur;
#else /* RLIM_INFINITY */
  return hard ? rlfd.rlim_max : rlfd.rlim_cur;
#endif /* RLIM_INFINITY */
#else /* !RLIMIT_NOFILE */
#ifdef HAVE_GETDTABLESIZE
  return (getdtablesize ());
#else /* !HAVE_GETDTABLESIZE */
#ifdef _SC_OPEN_MAX
  return (sysconf (_SC_OPEN_MAX));
#else /* !_SC_OPEN_MAX */
#ifdef NOFILE
  return (NOFILE);
#else /* !NOFILE */
  return (25);
#endif /* !NOFILE */
#endif /* !_SC_OPEN_MAX */
#endif /* !HAVE_GETDTABLESIZE */
#endif /* !RLIMIT_NOFILE */
}

static int
fdlim_set (int lim) {
#ifdef RLIMIT_NOFILE
  struct rlimit rlfd;
  if (lim <= 0)
    return (-1);
  if (getrlimit (RLIMIT_NOFILE, &rlfd) < 0)
    return (-1);
  rlfd.rlim_cur = lim;
  if (setrlimit (RLIMIT_NOFILE, &rlfd) < 0)
    return (-1);
  return (0);
#else /* !RLIMIT_NOFILE */
  return (-1);
#endif /* !RLIMIT_NOFILE */
}

#endif /* FDLIM_H */
