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
 *       $Id: udiphsun.h,v 1.2 1996/11/23 04:11:29 niklas Exp $
 *	 $Id: @(#)udiphsun.h	2.3, AMD
 */

/* This file is to be used to reconfigure the UDI Procedural interface
   for a given host. This file should be placed so that it will be
   included from udiproc.h. Everything in here may need to be changed
   when you change either the host CPU or its compiler. Nothing in
   here should change to support different targets. There are multiple
   versions of this file, one for each of the different host/compiler
   combinations in use.
*/

#define UDIStruct  struct		/* _packed not needed on Sun */
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
typedef int		UDIInt;
typedef unsigned int	UDIUInt;

typedef unsigned int	UDISizeT;

/* Now two void types. The first is for function return types,
the other for pointers to no particular type. Since these types
are used solely for documentational clarity, if your host/compiler
doesn't support either one, replace them with int and char *
respectively.
*/
typedef void		UDIVoid;		/* void type */
typedef void *		UDIVoidPtr;		/* void pointer type */
typedef void *		UDIHostMemPtr;		/* Arbitrary memory pointer */

/* Now we want a type optimized for boolean values. Normally this
   would be int, but on some machines (Z80s, 8051s, etc) it might
   be better to map it onto a char
*/
typedef	int		UDIBool;

/* Now indicate whether your compiler support full ANSI style
   prototypes. If so, use #if 1. If not use #if 0.
*/
#if 0
#define UDIParams(x)	x
#else
#define UDIParams(x)	()
#endif
