/* @(#)pceb.h	5.18 93/07/30 16:40:12, Srini, AMD */
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
**       This file defines values used in accessing the PCEB board.
 *****************************************************************************
 */


/*
** PCEB register addresses
*/

#define PCEB_PC229K_OFFSET        32
#define PCEB_PC29K2PC_OFFSET      34
#define PCEB_PCCNF_OFFSET         36


/*
** PC229K register bit definitions.
*/

#define PCEB_P_REQ      0x80       /* Request bit from PC to 29k */
#define PCEB_S_ENA      0x40       /* Enable interrupts to PC */
#define PCEB_LB_END     0x20       /* Set for Big Endian access to 29k */
#define PCEB_S_RESET    0x10       /* Reset the 29k processor */
#define PCEB_S_WARN     0x08       /* Assert WARN to the 29k */
#define PCEB_WINENA     0x04       /* Enable PC memory window */
#define PCEB_S_CTL1     0x02       /* Processor CTRL1 input */
#define PCEB_S_CTL0     0x01       /* Processor CTRL0 input */

#define PCEB_S_HALT     PCEB_S_CTL1
#define PCEB_S_NORMAL   (PCEB_S_CTL1 | PCEB_S_CTL0)


/*
** 29K2PC register bit definitions.
*/

#define PCEB_S_REQ      0x80       /* Request bit from 29k to PC */
#define PCEB_P_ENA      0x40       /* Enable interrupts to 29k */
#define PCEB_S_STAT2    0x04       /* STAT2 signal from 29k processor */
#define PCEB_S_STAT1    0x02       /* STAT1 signal from 29k processor */
#define PCEB_S_STAT0    0x01       /* STAT0 signal from 29k processor */

/*
** Shared memory definitions
*/

/*
** The "anchors" defined below represent addresses in the Am29000
** data memory space.  At these addresses are pointers to shared
** memory buffers.
*/


#define PCEB_RECV_BUF_PTR    0x0400  /* Host receive buffer pointer   */

#define PCEB_SEND_BUF        0x0404  /* Host send buffer    */


