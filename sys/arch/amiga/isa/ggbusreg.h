/*	$OpenBSD: ggbusreg.h,v 1.3 1996/05/04 14:15:27 niklas Exp $	*/

/*
 * Copyright (c) 1994, 1996 Niklas Hallqvist
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

#ifndef _GGBUSREG_H_
#define _GGBUSREG_H_

#define GG2_MEMORY_OFFSET (0x20000 - 2 * 0x90000)

#define GG2_STATUS 0x18000
#define GG2_STATUS_ADDR(va) (volatile u_int16_t *)((va) + GG2_STATUS)
#define GG2_GET_STATUS(va) (*GG2_STATUS_ADDR(va))

#define GG2_MASTER 0
#define GG2_WAIT 1
#define GG2_IRQ3 2
#define GG2_IRQ4 3
#define GG2_IRQ5 4
#define GG2_IRQ6 5
#define GG2_IRQ7 6
#define GG2_IRQ9 7
#define GG2_IRQ10 8
#define GG2_IRQ11 9
#define GG2_IRQ12 10
#define GG2_IRQ14 11
#define GG2_IRQ15 12
#define GG2_IRQ_MASK 0x1ffc
#define GG2_GET_INT_STATUS(va) (GG2_GET_STATUS(va) & GG2_IRQ_MASK)

#define GG2_INT_CTRL 0x18002
#define GG2_DISABLE_INTS(va) (*(volatile u_int16_t *)((va) + GG2_INT_CTRL))
#define GG2_ENABLE_INTS(va) (*(volatile u_int16_t *)((va) + GG2_INT_CTRL) = 0)

#define GG2_WAIT_CTRL 0x18004
#define GG2_TOGGLE_WAIT(va) (*(volatile u_int8_t *)((va) + GG2_WAIT_CTRL))
#define GG2_ENABLE_WAIT(va) \
    while ((GG2_GET_STATUS(va) & 1 << GG2_WAIT) == 0) GG2_TOGGLE_WAIT(va)
#define GG2_DISABLE_WAIT(va) \
    while (GG2_GET_STATUS(va) & 1 << GG2_WAIT) GG2_TOGGLE_WAIT(va)

#endif
