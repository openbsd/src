/*	$OpenBSD	*/

/* ==== machdep.h ============================================================
 * Copyright (c) 1993 Chris Provenzano, proven@athena.mit.edu
 *
 * $ I d: engine-m68000-netbsd.h,v 1.51 1994/11/08 15:39:15 proven Exp $
 *
 */
#include <unistd.h>
#include <setjmp.h>
#include <sys/time.h>

/*
 * The first machine dependent functions are the SEMAPHORES
 * needing the test and set instruction.
 */
#define SEMAPHORE_CLEAR 0
#define SEMAPHORE_SET   0x80;

#define SEMAPHORE_TEST_AND_SET(lock)		\
({						\
	volatile long temp = SEMAPHORE_CLEAR;   \
	__asm__ volatile( 			\
	  "tas %2; bpl 0f; movl #1,%0; 0:"	\
          :"=r" (temp)                    	\
          :"0" (temp),"m" (*lock));        	\
        temp;                                   \
})

#define SEMAPHORE_RESET(lock)           *lock = SEMAPHORE_CLEAR

/*
 * Min pthread stacksize
 */
#define PTHREAD_STACK_MIN	1024

/*
 * New functions
 */

__BEGIN_DECLS

#if defined(PTHREAD_KERNEL)

int machdep_save_state      __P((void));

#endif

__END_DECLS
