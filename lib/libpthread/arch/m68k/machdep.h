/* ==== machdep.h ============================================================
 * Copyright (c) 1993 Chris Provenzano, proven@athena.mit.edu
 *
 * $Id: machdep.h,v 1.2 1998/07/21 16:28:05 peter Exp $
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

#define SEMAPHORE_TEST_AND_SET(lock)    \
({										\
volatile long temp = SEMAPHORE_CLEAR;   \
										\
__asm__ volatile("tas (%2);	bpl 0f; movl #1,%0; 0:" \
        :"=r" (temp)                    \
        :"0" (temp),"r" (lock));        \
temp;                                   \
})

#define SEMAPHORE_RESET(lock)           *lock = SEMAPHORE_CLEAR

/*
 * New types
 */
typedef char    semaphore;

/*
 * sigset_t macros
 */
#define	SIG_ANY(sig)		(sig)
#define SIGMAX				31

/*
 * New Strutures
 */
struct machdep_pthread {
    void        		*(*start_routine)(void *);
    void        		*start_argument;
    void        		*machdep_stack;
	struct itimerval	machdep_timer;
    jmp_buf     		machdep_state;
};

/*
 * Min pthread stacksize
 */
#define PTHREAD_STACK_MIN	1024

/*
 * Static machdep_pthread initialization values.
 * For initial thread only.
 */
#define MACHDEP_PTHREAD_INIT    \
{ NULL, NULL, NULL, { { 0, 0 }, { 0, 100000 } }, 0 }

/*
 * New functions
 */

__BEGIN_DECLS

#if defined(PTHREAD_KERNEL)

int machdep_save_state      __P_((void));

#endif

__END_DECLS
