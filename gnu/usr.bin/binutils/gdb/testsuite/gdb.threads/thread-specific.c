/* This testcase is part of GDB, the GNU debugger.

   Copyright 2004 Free Software Foundation, Inc.

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

#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <pthread.h>

void *thread_function(void *arg);

unsigned int args[1];

int main() {
    int res;
    pthread_t threads[2];
    void *thread_result;
    long i = 1;

    args[0] = 1;
    res = pthread_create(&threads[0],
			 NULL,
			 thread_function,
			 (void *) 0);

    /* thread-specific.exp: last thread start.  */
    args[1] = 1;

    /* Don't run forever.  Run just short of it :)  */
    while (i > 0)
      {
	/* thread-specific.exp: main loop.  */
	(i) ++;
      }

    exit(EXIT_SUCCESS);
}

void *thread_function(void *arg) {
    int my_number =  (long) arg;
    int *myp = &args[my_number];

    /* Don't run forever.  Run just short of it :)  */
    while (*myp > 0)
      {
	/* thread-specific.exp: thread loop.  */
	(*myp) ++;
      }

    pthread_exit(NULL);
}
