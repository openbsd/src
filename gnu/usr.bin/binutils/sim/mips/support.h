/*> support.h <*/
/* Support for the MIPS architecture simulator.

   This file is part of the MIPS sim

		THIS SOFTWARE IS NOT COPYRIGHTED

   Cygnus offers the following for use in the public domain.  Cygnus
   makes no warranty with regard to the software or it's performance
   and the user accepts the software "AS IS" with all faults.

   CYGNUS DISCLAIMS ANY WARRANTIES, EXPRESS OR IMPLIED, WITH REGARD TO
   THIS SOFTWARE INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
   MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE.

   $Revision: 1.2 $
     $Author: niklas $
       $Date: 1996/11/23 03:55:16 $
*/

#ifndef __support_h
#define __support_h

/* For 64bit values either use the GCC "long long" feature, or have a
   structure made from two 32bit values. We can then have macros for
   accessing the LO and HI parts of the value. Also we can provide
   macros for the basic operations we want to perform on 64bit values
   (i.e. ADD64,SUB64,SHIFTLEFT64, etc.). This means we should be able
   to host the simulator on non-GCC compilers, and 32bit only
   architectures if desired. */

/* Control via a build boolean for the moment */
#if defined(__GNUC__) || defined(__WIN32__)

#ifdef __WIN32__
typedef signed __int64 word64;
typedef unsigned __int64 uword64;
#else
typedef long long word64;
typedef unsigned long long uword64;
#endif

#define WORD64LO(t)     (unsigned int)((t)&0xFFFFFFFF)
#define WORD64HI(t)     (unsigned int)(((uword64)(t))>>32)
#define SET64LO(t)      (((uword64)(t))&0xFFFFFFFF)
#define SET64HI(t)	(((uword64)(t))<<32)

/* Sign-extend the given value (e) as a value (b) bits long. We cannot
   assume the HI32bits of the operand are zero, so we must perform a
   mask to ensure we can use the simple subtraction to sign-extend. */
#define SIGNEXTEND(e,b) \
  (((e) & ((uword64) 1 << ((b) - 1))) \
   ? (((e) & (((uword64) 1 << (b)) - 1)) - ((uword64)1 << (b))) \
   : ((e) & (((((uword64) 1 << ((b) - 1)) - 1) << 1) | 1)))

/* Check if a value will fit within a word (unsigned int): */
#define NOTWORDVALUE(v) ((((((uword64)(v)>>32) == 0) && !((v) & ((unsigned)1 << 31))) || ((((uword64)(v)>>32) == 0xFFFFFFFF) && ((v) & ((unsigned)1 << 31)))) ? (1 == 0) : (1 == 1))

/* Check if a value will fit within a halfword: */
#define NOTHALFWORDVALUE(v) ((((((uword64)(v)>>16) == 0) && !((v) & ((unsigned)1 << 15))) || (((((uword64)(v)>>32) == 0xFFFFFFFF) && ((((uword64)(v)>>16) & 0xFFFF) == 0xFFFF)) && ((v) & ((unsigned)1 << 15)))) ? (1 == 0) : (1 == 1))

/* The following should be executed once at the start of day in the
   main emulator control function. The simulator assumes char is
   8bits, and from this: */
#define CHECKSIM() {\
                     if (sizeof(int) != (4 * sizeof(char)))\
                      SignalException(SimulatorFault,"sizeof(int) != 4");\
                     if (sizeof(word64) != (8 * sizeof(char)))\
                      SignalException(SimulatorFault,"sizeof(word64) != 8");\
                   }

#else /* non-GCC build */

#error "non-GCC build to be completed" /* avoid using long long */

typedef struct uword64 {
 unsigned int lo;
 unsigned int hi;
} uword64;

#define WORD64LO(t)     (unsigned int)(t.lo)
#define WORD64HI(t)     (unsigned int)(t.hi)
#define SET64LO(t)      (..TODO..) /* need structure into which value will be placed */
#define SET64HI(t)      (..TODO..) /* need structure into which value will be placed */

/* TODO: Update these to manipulate the split structure values */
#define SIGNEXTEND(e,b)         /* TODO */
#define NOTWORDVALUE(v)         /* TODO */
#define NOTHALFWORDVALUE(v)     /* TODO */

/* The following should be executed once at the start of day in the
   main emulator control function. The simulator assumes char is
   8bits, and from this: */
#define CHECKSIM() {\
                     if (sizeof(int) != (4 * sizeof(char)))\
                      SignalException(SimulatorFault,"sizeof(int) != 4");\
                   }

#endif /* non-GCC build */

#endif /* __support_h */

/*> EOF support.h <*/
