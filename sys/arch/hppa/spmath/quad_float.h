/*
 * Copyright 1996 1995 by Open Software Foundation, Inc.   
 *              All Rights Reserved 
 *  
 * Permission to use, copy, modify, and distribute this software and 
 * its documentation for any purpose and without fee is hereby granted, 
 * provided that the above copyright notice appears in all copies and 
 * that both the copyright notice and this permission notice appear in 
 * supporting documentation. 
 *  
 * OSF DISCLAIMS ALL WARRANTIES WITH REGARD TO THIS SOFTWARE 
 * INCLUDING ALL IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS 
 * FOR A PARTICULAR PURPOSE. 
 *  
 * IN NO EVENT SHALL OSF BE LIABLE FOR ANY SPECIAL, INDIRECT, OR 
 * CONSEQUENTIAL DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING FROM 
 * LOSS OF USE, DATA OR PROFITS, WHETHER IN ACTION OF CONTRACT, 
 * NEGLIGENCE, OR OTHER TORTIOUS ACTION, ARISING OUT OF OR IN CONNECTION 
 * WITH THE USE OR PERFORMANCE OF THIS SOFTWARE. 
 * 
 */
/*
 * pmk1.1
 */
/*
 * (c) Copyright 1986 HEWLETT-PACKARD COMPANY
 *
 * To anyone who acknowledges that this file is provided "AS IS" 
 * without any express or implied warranty:
 *     permission to use, copy, modify, and distribute this file 
 * for any purpose is hereby granted without fee, provided that 
 * the above copyright notice and this notice appears in all 
 * copies, and that the name of Hewlett-Packard Company not be 
 * used in advertising or publicity pertaining to distribution 
 * of the software without specific, written prior permission.  
 * Hewlett-Packard Company makes no representations about the 
 * suitability of this software for any purpose.
 */

/******************************
 *  Quad precision functions  *
 ******************************/

/* 32-bit word grabing functions */
#define Quad_firstword(value) Qallp1(value)
#define Quad_secondword(value) Qallp2(value)
#define Quad_thirdword(value)  Qallp3(value)
#define Quad_fourthword(value)  Qallp4(value)


/* This magnitude comparison uses the signless first words and
 * the regular part2 words.  The comparison is graphically:
 *
 *       1st greater?  ----------->|
 *                                 |
 *       1st less?-----------------+------->|
 *                                 |        |
 *       2nd greater?------------->|        |
 *                                 |        |
 *       2nd less?-----------------+------->|
 *                                 |        |
 *       3rd greater?------------->|        |
 *                                 |        |
 *       3rd less?-----------------+------->|
 *                                 |        |
 *       4th greater or equal?---->|        |
 *                                 |        |
 *                               False     True
 */
#define Quad_ismagnitudeless(leftp3,leftp4,rightp1,rightp2,rightp3,rightp4,signlessleft,signlessright) \
/*  Quad_floating_point left, right;          *				\
 *  unsigned int signlessleft, signlessright; */			\
      ( signlessleft<=signlessright &&					\
       (signlessleft<signlessright || (Qallp2(leftp2)<=Qallp2(rightp2) && \
        (Qallp2(leftp2)<Qallp2(rightp2) || (Qallp3(leftp3)<=Qallp3(rightp3) && \
	 (Qallp3(leftp3)<Qallp3(rightp3) || Qallp4(leftp4)<Qallp4(rightp4)))))))
         
#define Quad_xor_to_intp1(leftp1,rightp1,result)		\
    /* quad_floating_point left, right;				\
     * unsigned int result; */					\
    result = Qallp1(leftp1) XOR Qallp1(rightp1);

#define Quad_xor_from_intp1(leftp1,rightp1,result)		\
    /* quad_floating_point right, result;			\
     * unsigned int left; */					\
    Qallp1(resultp1) = left XOR Qallp1(rightp1)

#define Quad_swap_lower(leftp1,leftp2,leftp3,leftp4,rightp1,rightp2,rightp3,rightp4)  \
    /* quad_floating_point left, right; */			\
    Qallp2(leftp2)  = Qallp2(leftp2) XOR Qallp2(rightp2)	\
    Qallp2(rightp2) = Qallp2(leftp2) XOR Qallp2(rightp2)	\
    Qallp2(leftp2)  = Qallp2(leftp2) XOR Qallp2(rightp2)	\
    Qallp3(leftp3)  = Qallp3(leftp3) XOR Qallp3(rightp3)	\
    Qallp3(rightp3) = Qallp3(leftp3) XOR Qallp3(rightp3)	\
    Qallp3(leftp3)  = Qallp3(leftp3) XOR Qallp3(rightp3)	\
    Qallp4(leftp4)  = Qallp4(leftp4) XOR Qallp4(rightp4)	\
    Qallp4(rightp4) = Qallp4(leftp4) XOR Qallp4(rightp4)	\
    Qallp4(leftp4)  = Qallp4(leftp4) XOR Qallp4(rightp4)
