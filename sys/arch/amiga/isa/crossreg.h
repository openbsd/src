/*	$OpenBSD: crossreg.h,v 1.2 1996/04/27 18:38:56 niklas Exp $	*/

/*
 * Copyright (c) 1994, 1996 Niklas Hallqvist, Carsten Hammer
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *      This product includes software developed by Niklas Hallqvist.
 * 4. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef _CROSSREG_H_
#define _CROSSREG_H_

/***
* 
*
* Hardware description:
*
*  The CrossLink board is a 64K autoconfig device. The board address can be
*  read from the xlink.resource structure instead of searching with the
*  expansion library. In this way, the manufacturer ID is not required by
*  the software developer.
*
*  Address mapping is as follows:
*   XL_ROM     16K of byte wide ROM appearing at even locations.
*              Used to hold the xlink.resource, and any autobooting
*              devices.
*   XL_MEM     Memory space. For 8 bit cards, bytes appear at even
*              locations, (ie multiply address by 2). For 16 bit cards,
*              words appear at long word boundaries (ie multiply by
*              2). You can read bytes from 16 bit cards - odd bytes
*              will appear at odd locations. SBHE must be set to the
*              appropriate value for this. Multiple pages are accessible
*              via the page register (see MemPage register).
*   XL_IO      I/O space. 8K of I/O space is supported by the hardware,
*              however only 1K is managed by this resource (as IBM's
*              *never* have cards which decode more I/O space than this).
*              You should AllocPortSpace() the area you want to use to
*              get exclusive access before using the I/O. 8/16 bit cards
*              read/write bytes/words at word boundaries. (You should know
*              whether the port is 8 or 16 bits wide).
*
* Within the 1K I/O space, there are some 8 and 16 bit registers on the
* xlink board itself. These registers are normally only accessed by
* the xlink.resource, they are only described here for completeness.
* Later revisions of the CrossLink board may move/change the meaning
* of these registers. They are as follows:
* (actual address = SBP_xxxx + SB_IO + board address)
*
*   XLP_LATCH  (Write Only) (offset = 2)
*              Latches most significant address lines. D0-D6 contain the
*              values for the most significant 7 address lines (A13-A19).
*              D7 contains data for SBHE line (always 1 for 8 bit transfers
*              on 8 bit boards, and 16 bit transfers on 16 bit boards.
*              Set SBHE to 0 to perform 8 bit transfers on 16 bit boards).
*
*   XLP_INTABLE (Write Only) (offset = 0)
*              Interrupt Enable & Disable. Bits map to the interrupts in
*              the following manner:
*                Data   Interrupt          Data   Interrupt
*                 2        10               10       2
*                 3        11               11       3
*                 4        12               12       4
*                 5        Master           13       5
*                 6        14               14       6
*                 7        15               15       7
*              Unused bits should not be interpreted in any way. When
*              writing to this register, unused bits should be zero. This
*              register should only be used by the xlink.resource.
*
*   XLP_INTSTAT (Read Only) (offset = 0)
*              Interrupt Status. Bit mapping is the same as the
*              XLP_INTABLE location. Normally you only need to add your
*              interrupt handler to the appropriate port, and not worry
*              about this register. However, it can be also used to
*              determine which interrupt a board is connected to. 
**/

/* hardware offsets from config address */

#define CROSS_XL_ROM          0x8000
#define CROSS_XL_MEM          0x4000
#define CROSS_XL_IO           0x0000

#define CROSS_XLP_INTSTAT 0
#define CROSS_XLP_INTABLE 0
#define CROSS_XLP_LATCH 2
#define CROSS_HANDLE_TO_XLP_LATCH(va) \
    ((volatile u_int16_t *)((va) & 0xffff | CROSS_XLP_LATCH))

#define CROSS_MEMORY_OFFSET (CROSS_XL_MEM - 2 * 0x90000)
#define CROSS_SBHE 0x40

#define CROSS_STATUS_ADDR(va) \
    ((volatile u_int16_t *)((va) + CROSS_XLP_INTSTAT))

#define CROSS_MASTER 5

#define CROSS_IRQ9 10	/* IRQ9 is an alias of IRQ2 */
#define CROSS_IRQ3 11
#define CROSS_IRQ4 12
#define CROSS_IRQ5 13
#define CROSS_IRQ6 14
#define CROSS_IRQ7 15
#define CROSS_IRQ10 2
#define CROSS_IRQ11 3
#define CROSS_IRQ12 4
#define CROSS_IRQ14 6
#define CROSS_IRQ15 7
#define CROSS_IRQMASK 0xfcdc
#define CROSS_GET_INT_STATUS(va) (CROSS_GET_STATUS(va) & CROSS_IRQMASK)

#define CROSS_ENABLE_INTS(va, ints) \
    (*(volatile u_int16_t *)((va) + CROSS_XLP_INTABLE) = ints) 

#endif
