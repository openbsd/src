/* Manythreads test program.
   Copyright 2004
   Free Software Foundation, Inc.

   Written by Jeff Johnston <jjohnstn@redhat.com> 
   Contributed by Red Hat

   This file is part of GDB.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 2 of the License, or
   (at your option) any later version.
   
   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.
   
   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 59 Temple Place - Suite 330,
   Boston, MA 02111-1307, USA.  */

#include <pthread.h>
#include <stdio.h>
#include <limits.h>

void *
thread_function (void *arg)
{
  int x = (int)arg;

  printf ("Thread <%d> executing\n", x);

  return NULL;
}

int 
main (int argc, char **argv)
{
  pthread_attr_t attr;
  pthread_t threads[256];
  int i, j;

  pthread_attr_init (&attr);
  pthread_attr_setstacksize (&attr, PTHREAD_STACK_MIN);

  /* Create a ton of quick-executing threads, then wait for them to
     complete.  */
  for (i = 0; i < 1000; ++i) 
    {
      for (j = 0; j < 256; ++j)
	{
	  pthread_create (&threads[j], &attr, thread_function, 
			  (void *)(i * 1000 + j));
	}

      for (j = 0; j < 256; ++j)
	{
	  pthread_join (threads[j], NULL);
	}
    }

  pthread_attr_destroy (&attr);

  return 0;
}
