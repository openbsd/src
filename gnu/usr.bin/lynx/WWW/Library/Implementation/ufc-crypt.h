/*
 * UFC-crypt: ultra fast crypt(3) implementation
 *
 * Copyright (C) 1991, 1992, Free Software Foundation, Inc.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.

 * You should have received a copy of the GNU Library General Public
 * License along with this library; if not, write to the Free
 * Software Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 *
 * @(#)ufc-crypt.h	1.16 09/21/92
 *
 * Definitions of datatypes
 *
 */

/*
 * Requirements for datatypes:
 *
 * A datatype 'ufc_long' of at least 32 bit
 * *and*
 *   A type 'long32' of exactly 32 bits (_UFC_32_)
 *   *or*
 *   A type 'long64' of exactly 64 bits (_UFC_64_)
 *
 * 'int' is assumed to be at least 8 bit
 */

/*
 * #ifdef's for various architectures
 */

#ifdef cray
/* thanks to <hutton@opus.sdsc.edu> (Tom Hutton)  for testing */
typedef unsigned long ufc_long;
typedef unsigned long long64;
#define _UFC_64_
#endif

#ifdef convex
/* thanks to pcl@convex.oxford.ac.uk (Paul Leyland) for testing */
typedef unsigned long ufc_long;
typedef long long     long64;
#define _UFC_64_
#endif

#ifdef ksr
/*
 * Note - the KSR machine does not define a unique symbol
 * which we can check.  So you MUST add '-Dksr' to your Makefile.
 * Thanks to lijewski@theory.tc.cornell.edu (Mike Lijewski) for
 * the patch.
 */
typedef unsigned long ufc_long;
typedef unsigned long long64;
#define _UFC_64_
#endif

/*
 * For debugging 64 bit code etc with 'gcc'
 */

#ifdef GCC3232
typedef unsigned long ufc_long;
typedef unsigned long long32;
#define _UFC_32_
#endif

#ifdef GCC3264
typedef unsigned long ufc_long;
typedef long long     long64;
#define _UFC_64_
#endif

#ifdef GCC6432
typedef long long ufc_long;
typedef unsigned long long32;
#define _UFC_32_
#endif

#ifdef GCC6464
typedef long long     ufc_long;
typedef long long     long64;
#define _UFC_64_
#endif

/*
 * Catch all for 99.95% of all UNIX machines
 */

#ifndef _UFC_64_
#ifndef _UFC_32_
#define _UFC_32_
typedef unsigned long ufc_long;
typedef unsigned long long32;
#endif
#endif


