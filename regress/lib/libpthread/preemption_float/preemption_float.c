/*	$OpenBSD: preemption_float.c,v 1.1.1.1 2001/08/15 14:37:12 fgsch Exp $	*/
/*
 * Copyright (c) 1993, 1994, 1995, 1996 by Chris Provenzano and contributors, 
 * proven@mit.edu All rights reserved.
 * 
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *	This product includes software developed by Chris Provenzano,
 *	the University of California, Berkeley, and contributors.
 * 4. Neither the name of Chris Provenzano, the University, nor the names of
 *   contributors may be used to endorse or promote products derived
 *   from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY CHRIS PROVENZANO AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL CHRIS PROVENZANO, THE REGENTS OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS;
 * OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
 * WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR
 * OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF
 * ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */ 

/* Test to see if floating point state is being properly maintained
   for each thread.  Different threads doing floating point operations
   simultaneously should not interfere with one another.  This
   includes operations that might change some FPU flags, such as
   rounding modes, at least implicitly.  */

#include <pthread.h>
#include <math.h>
#include <stdio.h>
#include "test.h"

int limit = 2;
int float_passed = 0;
int float_failed = 1;

void *log_loop (void *x) {
  int i;
  double d, d1, d2;
  /* sleep (1); */
  for (i = 0; i < limit; i++) {
    d = 42.0;
    d = log (exp (d));
    d = (d + 39.0) / d;
    if (i == 0)
      d1 = d;
    else {
		d2 = d;
		d = sin(d);
		/* if (d2 != d1) { */
		if (memcmp (&d2, &d1, 8)) {
			pthread_exit(&float_failed);
		}
	}
  }
  pthread_exit(&float_passed);
}

void *trig_loop (void *x) {
  int i;
  double d, d1, d2;
  /* sleep (1);  */
  for (i = 0; i < limit; i++) {
    d = 35.0;
    d *= M_PI;
    d /= M_LN2;
    d = sin (d);
    d = cos (1 / d);
    if (i == 0)
      d1 = d;
    else {
		d2 = d;
		d = sin(d);
		/* if (d2 != d1) { */
		if (memcmp (&d2, &d1, 8)) {
  			pthread_exit(&float_failed);
		}
	}
  }
  pthread_exit(&float_passed);
}

int
floatloop(pthread_attr_t *attrp)
{
	pthread_t thread[2];
	int *x, *y;

	CHECKr(pthread_create (&thread[0], attrp, trig_loop, 0));
	CHECKr(pthread_create (&thread[1], attrp, log_loop, 0));
	CHECKr(pthread_join(thread[0], (void **) &x));	
	CHECKr(pthread_join(thread[1], (void **) &y));	

	/* Return 0 for success */
	return ((*y == float_failed)?2:0) | 
	       ((*x == float_failed)?1:0);
}

#define N 10
int
main()
{
	pthread_attr_t attr;
	int i;

	/* Try with float point state not preserved */

	CHECKr(pthread_attr_init(&attr));
	CHECKr(pthread_attr_setfloatstate(&attr, PTHREAD_NOFLOAT));

	for(limit = 2; limit < 100000; limit *=4)
		if (floatloop(&attr) != 0)
			break;

	if (limit >= 100000) {
		printf("results are INDETERMINATE\n");
		SUCCEED; /* XXX */
	}

	limit *= 4;  /* just to make sure */

	printf("using limit = %d\n", limit);

	for (i = 0; i < 32; i++) {
		/* Try the failure mode one more time. */
		if (floatloop(&attr) == 0) {
			printf("%d ", i);
			fflush(stdout);
		}
		/* Now see if saving float state will get rid of failure. */
		ASSERT(floatloop(NULL) == 0);
	}

	SUCCEED;
}
