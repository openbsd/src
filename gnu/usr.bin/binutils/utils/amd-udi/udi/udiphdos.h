/******************************************************************************
 * Copyright 1991 Advanced Micro Devices, Inc.
 *
 * This software is the property of Advanced Micro Devices, Inc  (AMD)  which
 * specifically  grants the user the right to modify, use and distribute this
 * software provided this notice is not removed or altered.  All other rights
 * are reserved by AMD.
 *
 * AMD MAKES NO WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, WITH REGARD TO THIS
 * SOFTWARE.  IN NO EVENT SHALL AMD BE LIABLE FOR INCIDENTAL OR CONSEQUENTIAL
 * DAMAGES IN CONNECTION WITH OR ARISING FROM THE FURNISHING, PERFORMANCE, OR
 * USE OF THIS SOFTWARE.
 *
 * Comments about this software should be directed to udi@amd.com. If access
 * to electronic mail isn't available, send mail to:
 *
 * Advanced Micro Devices, Inc.
 * 29K Support Products
 * Mail Stop 573
 * 5900 E. Ben White Blvd.
 * Austin, TX 78741
 *****************************************************************************
 *       $Id: udiphdos.h,v 1.2 1996/11/23 04:11:28 niklas Exp $
 *       $Id: @(#)udiphdos.h	2.7, AMD
 */


/* Modified M.Typaldos 11/92 - Added '386 specific code (just changed
 *               far to _FAR really).
 */


/* This file is to be used to reconfigure the UDI Procedural interface
   for a given host. This file should be placed so that it will be
   included from udiproc.h. Everything in here may need to be changed
   when you change either the host CPU or its compiler. Nothing in
   here should change to support different targets. There are multiple
   versions of this file, one for each of the different host/compiler
   combinations in use.
*/

#ifdef DOS386
#ifdef WATC
#define UDIStruct _Packed struct
#define _FAR		
#else /* not WATC */
#define  UDIStruct	_packed struct
#define _FAR	
#define far		/* far not used in DOS386 (but _far is needed) */
#endif /* WATC */
#else
#define _packed		/* _packed only used on DOS386 */
#define  UDIStruct	struct
#define _FAR far
#endif
/* First, we need some types */
/* Types with at least the specified number of bits */
typedef double		UDIReal64;		/* 64-bit real value */
typedef float		UDIReal32;		/* 32-bit real value */
  
typedef unsigned long	UDIUInt32;		/* unsigned integers */
typedef unsigned short	UDIUInt16; 
typedef unsigned char	UDIUInt8;
  
typedef long		UDIInt32;		/* 32-bit integer */ 
typedef short		UDIInt16;		/* 16-bit integer */ 
typedef char		UDIInt8;		/* unreliable signedness */

/* To aid in supporting environments where the DFE and TIP use
different compilers or hosts (like DOS 386 on one side, 286 on the
other, or different Unix machines connected by sockets), we define
two abstract types - UDIInt and UDISizeT.
UDIInt should be defined to be int except for host/compiler combinations
that are intended to talk to existing UDI components that have a different
sized int. Similarly for UDISizeT.
*/
#ifndef DOS386
typedef int		UDIInt;
typedef unsigned int	UDIUInt;

typedef unsigned int	UDISizeT;
#else
	/* DOS386 is one of those host/compiler combinations that require UDIInt
	 * not be defined as int
	 */
typedef UDIInt16	UDIInt;
typedef UDIUInt16	UDIUInt;

typedef UDIUInt16	UDISizeT;
#endif
/* Now two void types. The first is for function return types,
the other for pointers to no particular type. Since these types
are used solely for documentational clarity, if your host/compiler
doesn't support either one, replace them with int and char *
respectively.
*/
typedef void		UDIVoid;		/* void type */
typedef void *		UDIVoidPtr;		/* void pointer type */
typedef void _FAR *	UDIHostMemPtr;		/* Arbitrary memory pointer */

/* Now we want a type optimized for boolean values. Normally this
   would be int, but on some machines (Z80s, 8051s, etc) it might
   be better to map it onto a char
*/
#ifndef DOS386
typedef	int		UDIBool;
#else
typedef	UDIInt16	UDIBool;	/* see reasoning above */
#endif

/* Now indicate whether your compiler support full ANSI style
   prototypes. If so, use #if 1. If not use #if 0.
*/
#if 1
#define UDIParams(x)	x
#else
#define UDIParams(x)	()
#endif
