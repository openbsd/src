/* @(#)memspcs.h	5.18 93/07/30 16:39:58, Srini, AMD */
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
 **       This header file describes the memory spaces in the
 **       AM29000 family of processors.
 *****************************************************************************
 */

#ifndef	_MEMSPCS_H_INCLUDED_
#define	_MEMSPCS_H_INCLUDED_

#define LOCAL_REG    0  /* Local processor register     */
#define GLOBAL_REG   1  /* Global processor register    */
#define SPECIAL_REG  2  /* Special processor register   */
#define TLB_REG      3  /* Translation Lookaside Buffer */
#define COPROC_REG   4  /* Coprocessor register         */
#define I_MEM        5  /* Instruction Memory           */
#define D_MEM        6  /* Data Memory                  */
#define I_ROM        7  /* Instruction ROM              */
#define D_ROM        8  /* Data ROM                     */
#define I_O          9  /* Input/Output                 */
#define I_CACHE     10  /* Instruction Cache            */
#define D_CACHE     11  /* Data Cache                   */
#define	PC_SPACE    12 /* 29K PC0, PC1 space */
#define	A_SPCL_REG  13 /* Applications view of cps/ops */
#define	ABSOLUTE_REG 	14 /* Absolute register number */
#define	PC_RELATIVE	15	/* PC relative offsets */

#define	VERSION_SPACE	-1	/* to get target version numbers */
#define	GENERIC_SPACE	0xfe
#endif /* _MEMSPCS_H_INCLUDED_ */
