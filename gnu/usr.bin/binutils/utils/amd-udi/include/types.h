/* @(#)types.h	5.19 93/08/10 17:49:13, Srini, AMD */
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
 * So that all may benefit from your experience, please report  any  problems
 * or  suggestions about this software to the 29K Technical Support Center at
 * 800-29-29-AMD (800-292-9263) in the USA, or 0800-89-1131  in  the  UK,  or
 * 0031-11-1129 in Japan, toll free.  The direct dial number is 512-462-4118.
 *
 * Advanced Micro Devices, Inc.
 * 29K Support Products
 * Mail Stop 573
 * 5900 E. Ben White Blvd.
 * Austin, TX 78741
 * 800-292-9263
 *****************************************************************************
 *      Engineer: Srini Subramanian.
 *****************************************************************************
 **       This header file describes the basic data types used by the
 **       monitor.  These are of particular interest to the message
 **       passing portion of the code.
 **
 **       When porting to a machine / compiler whose data types differ
 **       in size from the descriptions below, a change to this file
 **       should permit successful compilation and execution.
 **
 *****************************************************************************
 */

#ifndef	_TYPES_H_INCLUDED_
#define	_TYPES_H_INCLUDED_

typedef long int INT32;            /* 32 bit integer */

typedef unsigned long int UINT32;  /* 32 bit integer (unsigned) */

typedef unsigned long int ADDR32;  /* 32 bit address */

typedef unsigned long int INST32;  /* 32 bit instruction */

typedef long int BOOLEAN;          /* Boolean value (32 bit) */

typedef unsigned char BYTE;        /* byte (8 bit) */

typedef short int INT16;           /* 16 bit integer */

typedef unsigned short int UINT16; /* 16 bit integer (unsigned) */

#ifdef	MSDOS
#define	PARAMS(x)	x
#else
#define	PARAMS(x)	()
#endif

#define	GLOBAL
#define	LOCAL		static

#define	SUCCESS		(INT32) 0
#define	FAILURE		(INT32) -1
#define	ABORT_FAILURE		(INT32) -2
#define	TIPFAILURE	(INT32) -1

#endif /* _TYPES_H_INCLUDED_ */

