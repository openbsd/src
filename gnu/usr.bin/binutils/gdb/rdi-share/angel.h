/* 
 * Copyright (C) 1995 Advanced RISC Machines Limited. All rights reserved.
 * 
 * This software may be freely used, copied, modified, and distributed
 * provided that the above copyright notice is preserved in all copies of the
 * software.
 */

/*> angel.h <*/
/*---------------------------------------------------------------------------*/
/* This header file is the main holder for the declarations and
 * prototypes for the core Angel system. Some Angel concepts are
 * described at the start of this file to ensure that a complete view
 * of the Angel world can be derived purely from the source.
 *
 * $Revision: 1.3 $
 *     $Date: 2004/12/27 14:00:53 $
 *
 *
 * NOTE: Currently the Angel source is designed to be simple,
 * understandable and easy to port to new hardware platforms. However,
 * this does not always yield the highest performing system. The
 * current layered approach introduces an overhead to the performance
 * of the system. In a true commercial target, this code should be
 * re-designed to build a system where the Angel logical message
 * system, device driver and hardware accesses are merged to provide
 * the best performance.
 */
/*---------------------------------------------------------------------------*/
/* Angel overview:

... some comments describing Angel ...

 * Angel is designed as a kit-of-parts that can be used to provide
 * run-time support for the development of ARM applications. The main
 * core of Angel is in providing support for the "debug" message
 * communication with a host system. These messages do not just cover
 * debugging ARM processes, but also the process of downloading ARM
 * programs or attaching to executing processes on the target.
 *
 * A stand-alone ROM based Angel world is the basic starting point for
 * a system, since it will allow programs to be downloaded to the
 * target. The ROM version of Angel will provide the generic debug
 * support, but no system specific routines. The preferred method of
 * using Angel is as a link library. This ensures that applications
 * carry with them the Angel routines necessary to support debugging
 * (and also ensure that the Angel version is up-to-date, independant
 * of the version in the target ROM). Eventually, once a program has
 * been fully debugged, a ROMmed version of the program can be
 * generated with the Angel code being provided in the application.

.. more comments ..

 * The standard Angel routines do *NOT* perform any dynamic memory
 * allocation. To simplify the source, and aid the porting to a non C
 * library world, memory is either pre-allocated (as build-time
 * globals) or actually given to the particular Angel routine by the
 * active run-time. This ensures that the interaction between Angel
 * and the target O/S is minimised.
 *
 * Notes: We sub-include more header files to keep the source
 * modular. Since Angel is a kit-of-parts alternative systems may need
 * to change the prototypes of particular functions, whilst
 * maintaining a fixed external interface. e.g. using the standard
 * DEBUG messages, but with a different communications world.
 */
/*---------------------------------------------------------------------------*/

#ifndef __angel_h
#define __angel_h

/*---------------------------------------------------------------------------*/
/*-- Global Angel definitions and manifests ---------------------------------*/
/*---------------------------------------------------------------------------*/
/* When building Angel we may not include the standard library
 * headers. However, it is useful coding using standard macro names
 * since it makes the code easier to understand.
 */

typedef unsigned int  word ;
typedef unsigned char byte ;

/* The following typedefs can be used to access I/O registers: */
typedef volatile unsigned int  vuword ;
typedef volatile unsigned char vubyte ;

/*
 * The following typedefs are used when defining objects that may also
 * be created on a host system, where the word size is not
 * 32bits. This ensures that the same data values are manipulated.
 */
#ifdef TARGET
typedef unsigned int unsigned32;
typedef signed int   signed32;
typedef        int   int32;

typedef unsigned short int unsigned16;
typedef signed   short int signed16;

/*
 * yet another solution for the bool/boolean problem, this one is
 * copied from Scott's modifications to clx/host.h
 */
# ifdef IMPLEMENT_BOOL_AS_ENUM
   enum _bool { _false, _true };
#  define _bool enum _bool
# elif defined(IMPLEMENT_BOOL_AS_INT) || !defined(__cplusplus)
#  define _bool int
#  define _false 0
#  define _true 1
# endif

# ifdef _bool
#  define bool _bool
# endif

# ifndef true
#  define true _true
#  define false _false
# endif

# ifndef YES
#  define YES   true
#  define NO    false
# endif

# undef TRUE             /* some OSF headers define as 1 */
# define TRUE  true

# undef FALSE            /* some OSF headers define as 1 */
# define FALSE false

# ifndef NULL
#  define NULL 0
# endif

#else

# include "host.h"

#endif

#ifndef IGNORE
# define IGNORE(x) ((x)=(x))
#endif

/* The following typedef allows us to cast between integral and
 * function pointers. This isn't allowed by direct casting when
 * conforming to the ANSI spec.
 */
typedef union ansibodge
{
 word  w ;
 word *wp ;
 void *vp ;
 byte *bp ;
 void (*vfn)(void) ;
 word (*wfn)(void) ;
 int  (*ifn)(void) ;
 byte (*bfn)(void) ;
} ansibodge ;

/*---------------------------------------------------------------------------*/

/* The amount setup aside by the run-time system for stack overflow
 * handlers to execute in. This must be at least 256bytes, since that
 * value is assumed by the current ARM Ltd compiler.
 * This space is _only_ kept for the USR stack, not any of the privileged
 * mode stacks, as stack overflow on these is always fatal - there is
 * no point attemptingto recover.  In addition is is important that
 * Angel should keep privileged stack space requirements to a minimum.
 */
#define APCS_STACKGUARD 256

#endif /* __angel_h */

/* EOF angel.h */
