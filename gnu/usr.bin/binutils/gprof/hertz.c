/*
 * Copyright (c) 1983 Regents of the University of California.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms are permitted
 * provided that: (1) source distributions retain this entire copyright
 * notice and comment, and (2) distributions including binaries display
 * the following acknowledgement:  ``This product includes software
 * developed by the University of California, Berkeley and its contributors''
 * in the documentation or other materials provided with the distribution
 * and in all advertising materials mentioning features or use of this
 * software. Neither the name of the University nor the names of its
 * contributors may be used to endorse or promote products derived
 * from this software without specific prior written permission.
 * THIS SOFTWARE IS PROVIDED ``AS IS'' AND WITHOUT ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, WITHOUT LIMITATION, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE.
 */
#include "hertz.h"


int
hertz ()
{
#ifdef HERTZ
  return HERTZ;
#else /* ! defined (HERTZ) */
#ifdef HAVE_SETITIMER
  struct itimerval tim;

  tim.it_interval.tv_sec = 0;
  tim.it_interval.tv_usec = 1;
  tim.it_value.tv_sec = 0;
  tim.it_value.tv_usec = 0;
  setitimer (ITIMER_REAL, &tim, 0);
  setitimer (ITIMER_REAL, 0, &tim);
  if (tim.it_interval.tv_usec >= 2)
    {
      return 1000000 / tim.it_interval.tv_usec;
    }
#endif /* ! defined (HAVE_SETITIMER) */
#if defined (HAVE_SYSCONF) && defined (_SC_CLK_TCK)
  return sysconf (_SC_CLK_TCK);
#else /* ! defined (HAVE_SYSCONF) || ! defined (_SC_CLK_TCK) */
#ifdef __MSDOS__
  return 18;
#else  /* ! defined (__MSDOS__) */
  return HZ_WRONG;
#endif /* ! defined (__MSDOS__) */
#endif /* ! defined (HAVE_SYSCONF) || ! defined (_SC_CLK_TCK) */
#endif /* ! defined (HERTZ) */
}
