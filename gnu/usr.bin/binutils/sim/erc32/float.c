/*
 * This file is part of SIS.
 *
 * SIS, SPARC instruction simulator. Copyright (C) 1995 Jiri Gaisler, European
 * Space Agency
 *
 * This program is free software; you can redistribute it and/or modify it under
 * the terms of the GNU General Public License as published by the Free
 * Software Foundation; either version 2 of the License, or (at your option)
 * any later version.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * more details.
 *
 * You should have received a copy of the GNU General Public License along with
 * this program; if not, write to the Free Software Foundation, Inc., 675
 * Mass Ave, Cambridge, MA 02139, USA.
 *
 *
 * This file implements the interface between the host and the simulated
 * FPU. IEEE trap handling is done as follows: 
 * 1. In the host, all IEEE traps are masked
 * 2. After each simulated FPU instruction, check if any exception occured 
 *    by reading the exception bits from the host FPU status register 
 *    (get_accex()).
 * 3. Propagate any exceptions to the simulated FSR.
 * 4. Clear host exception bits
 *
 *
 * This can also be done using ieee_flags() library routine on sun.
 */

#include "sis.h"

/* This host dependent routine should return the accrued exceptions */
int
get_accex()
{
#ifdef sparc
    return ((_get_fsr_raw() >> 5) & 0x1F);
#elif i386
    uint32 accx;

    accx = _get_sw() & 0x3f;
    accx = ((accx & 1) << 4) | ((accx & 2) >> 1) | ((accx & 4) >> 1) |
	   (accx & 8) | ((accx & 16) >> 2) | ((accx & 32) >> 5);
    return(accx);
#else
    return(0);
/*warning no fpu trap support for this target*/
#endif

}

/* How to clear the accrued exceptions */
int
clear_accex()
{
#ifdef sparc
    set_fsr((_get_fsr_raw() & ~0x3e0));
#elif i386
    asm("
.text
	fnclex

    ");
#else
/*warning no fpu trap support for this target*/
#endif
}

/* How to map SPARC FSR onto the host */
int
set_fsr(fsr)
uint32 fsr;
{
#ifdef sparc
	_set_fsr_raw(fsr & ~0x0f800000);
#elif i386
     uint32 rawfsr;

     fsr >>= 30;
     switch (fsr) {
	case 0: 
	case 2: break;
	case 1: fsr = 3;
	case 3: fsr = 1;
    }
    rawfsr = _get_cw();
    rawfsr |= (fsr << 10) | 0x3ff;
    __setfpucw(rawfsr);
#else
/*warning no fpu trap support for this target*/
#endif
}


/* Host dependent support functions */

#ifdef sparc

    asm("

.text
        .align 4
        .global __set_fsr_raw,_set_fsr_raw
__set_fsr_raw:
_set_fsr_raw:
        save %sp,-104,%sp
        st %i0,[%fp+68]
        ld [%fp+68], %fsr
        mov 0,%i0
        ret
        restore
 
        .align 4
        .global __get_fsr_raw
        .global _get_fsr_raw
__get_fsr_raw:
_get_fsr_raw:
        save %sp,-104,%sp
        st %fsr,[%fp+68]
        ld [%fp+68], %i0
        ret
        restore
 
    ");

#elif i386
	/* both these align statements were 16, not 8 */

    asm("

.text
        .align 8
.globl _get_sw,__get_sw
__get_sw:
_get_sw:
        pushl %ebp
        movl %esp,%ebp
        movl $0,%eax
        fnstsw %ax
        movl %ebp,%esp
        popl %ebp
        ret

        .align 8
.globl _get_cw,__get_cw
__get_cw:
_get_cw:
        pushl %ebp
        movl %esp,%ebp
        subw $2,%esp
        fnstcw -2(%ebp)
        movw -2(%ebp),%eax
        movl %ebp,%esp
        popl %ebp
        ret


    ");


#else
/*warning no fpu trap support for this target*/
#endif

