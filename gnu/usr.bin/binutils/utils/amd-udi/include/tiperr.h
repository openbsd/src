/* @(#)tiperr.h	1.26 93/07/30 16:40:20, Srini, AMD */
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
 *****************************************************************************
 */

#define TIPNOTIMPLM	-1 
#define TIPPARSECN	-2 
#define TIPMSGINIT	-3 
#define TIPCORELOAD	-4 
#define TIPGOTARGET	-5 
#define TIPGETERROR	-6 
#define TIPSENDCFG	-7 
#define TIPRECVCFG	-8 
#define TIPSENDRST	-9 
#define TIPRECVHLT	-10 
#define TIPINITARGS	-11
#define TIPSENDINIT	-12 
#define TIPRECVINIT	-13 
#define TIPSENDRD	-14 
#define TIPRECVRD	-15 
#define TIPSENDWRT	-16 
#define TIPRECVWRT	-17 
#define TIPSENDCPY	-18 
#define TIPRECVCPY	-19 
#define TIPSENDGO	-20 
#define TIPRECVGO	-21 
#define TIPSENDSTP	-22 
#define TIPRECVSTP	-23 
#define TIPSENDBRK	-24 
#define TIPRECVBRK	-25 
#define TIPSENDSTBP	-26 
#define TIPRECVSTBP	-27 
#define TIPSENDQYBP	-28 
#define TIPRECVQYBP	-29 
#define TIPSENDRMBP	-30 
#define TIPRECVRMBP	-31 
#define TIPHIFFAIL      -32
#define TIPTIMEOUT      -33
#define TIPUNXPMSG	-34
#define TIPINVSPACE	-35 
#define TIPINVBID	-36
#define	TIPNOCORE	-37
#define	TIPNOSEND	-38
#define	TIPNORECV	-39
#define	TIPMSG2BIG	-40

/* put new errors before here & increment TIPLASTERR */
#define TIPLASTERR	-40

extern	char	*tip_err[];
