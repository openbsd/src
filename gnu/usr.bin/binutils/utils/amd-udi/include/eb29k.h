/* @(#)eb29k.h	5.18 93/07/30 16:39:45, Srini, AMD. */
/******************************************************************************
 * Copyright 1992 Advanced Micro Devices, Inc.
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
 * 29K Systems Engineering
 * Mail Stop 573
 * 5204 E. Ben White Blvd.
 * Austin, TX 78741
 * 800-292-9263
 * 29k-support@AMD.COM
 ****************************************************************************
 * Engineer: Srini Subramanian.
 ****************************************************************************
 **       This file defines values used in accessing the EB29K board.
 ****************************************************************************
 */

/* Control Port Register (PC_port_base+0) */
#define EB29K_RESET        0x80     /* (0=Reset EB29K, 1=Reset Am29000 */
#define EB29K_DRQEN        0x40     /* Enable DMA requests */
#define EB29K_IRQEN        0x20     /* Enable interrupts */

/* Address Register (PC_port_base+2) */
#define EB29K_I_MEM        0x00     /* Set window to Instruction Memory */
#define EB29K_D_MEM        0x80     /* Set window to Data Memory */

/*
** Shared memory definitions
*/

/*
** The "anchors" defined below represent addresses in the Am29000
** data memory space.  At these addresses are pointers to shared
** memory buffers.
*/

#define EB29K_RECV_BUF_PTR    0x80000400  /* Host receive buffer pointer   */

#define EB29K_SEND_BUF        0x80000404  /* Host send buffer    */

