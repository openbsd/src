/* @(#)macros.h	5.19 93/07/30 16:39:54, Srini, AMD */
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
 * This header file defines various macros used by the host module of 
 * MiniMON29K.
 *****************************************************************************
 */

#ifndef	_MACROS_H_INCLUDED_
#define	_MACROS_H_INCLUDED_

/*
** Macros
*/

#define MIN(x,y)            ((x)<(y) ? (x) : (y))
#define MAX(x,y)            ((x)<(y) ? (y) : (x))

/* Does the memory space contain registers? */
#define ISREG(x)      (((x) == LOCAL_REG) ||\
                       ((x) == ABSOLUTE_REG) ||\
                       ((x) == GLOBAL_REG) ||\
                       ((x) == SPECIAL_REG) ||\
                       ((x) == A_SPCL_REG) ||\
                       ((x) == TLB_REG) ||\
                       ((x) == PC_SPACE) ||\
                       ((x) == COPROC_REG))

#define ISMEM(x)      (((x) == I_MEM) ||\
                       ((x) == D_MEM) ||\
                       ((x) == I_ROM) ||\
                       ((x) == D_ROM) ||\
                       ((x) == PC_RELATIVE) ||\
                       ((x) == GENERIC_SPACE) ||\
                       ((x) == I_O))

#define ISGENERAL(x)   (((x) == LOCAL_REG) ||\
                       ((x) == ABSOLUTE_REG) ||\
                       ((x) == GLOBAL_REG))

#define ISSPECIAL(x)   (((x) == SPECIAL_REG) ||\
			((x) == A_SPCL_REG))

#define ISTLB(x)       (((x) == TLB_REG))

/*
** These macros are used to align addresses to 64, 32
** 16 and 8 bit boundaries (rounding upward).  The
** ALIGN8() macro is usually not necessary, but included
** for completeness.
*/

#define ALIGN64(x)     (((x) + 0x07) & 0xfffffff8);
#define ALIGN32(x)     (((x) + 0x03) & 0xfffffffc);
#define ALIGN16(x)     (((x) + 0x01) & 0xfffffffe);
#define ALIGN8(x)      (((x) + 0x00) & 0xffffffff);

/*
** This macro is used to get the processor from the PRL.
** It is assumed that the PRL is an eight bit quantity.
*/

#define PROCESSOR(prl)  (prl & 0xf1)

#endif /* _MACROS_H_INCLUDED_ */
