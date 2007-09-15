/*	$OpenBSD: macdefs.h,v 1.2 2007/09/15 22:04:38 ray Exp $	*/
/*
 * Copyright(C) Caldera International Inc. 2001-2002. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * Redistributions of source code and documentation must retain the above
 * copyright notice, this list of conditions and the following disclaimer.
 * Redistributions in binary form must reproduce the above copyright
 * notice, this list of conditionsand the following disclaimer in the
 * documentation and/or other materials provided with the distribution.
 * All advertising materials mentioning features or use of this software
 * must display the following acknowledgement:
 * 	This product includes software developed or owned by Caldera
 *	International, Inc.
 * Neither the name of Caldera International, Inc. nor the names of other
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * USE OF THE SOFTWARE PROVIDED FOR UNDER THIS LICENSE BY CALDERA
 * INTERNATIONAL, INC. AND CONTRIBUTORS ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED.  IN NO EVENT SHALL CALDERA INTERNATIONAL, INC. BE LIABLE
 * FOR ANY DIRECT, INDIRECT INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OFLIABILITY, WHETHER IN CONTRACT,
 * STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING
 * IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE 
 * POSSIBILITY OF SUCH DAMAGE.
 */

# define makecc(val,i)  lastcon = (lastcon<<8)|((val<<24)>>24);  

# define  ARGINIT 32 
# define  AUTOINIT 0 
# define  SZCHAR 8
# define  SZINT 32
# define  SZFLOAT 32
# define  SZDOUBLE 64
# define  SZLONG 32
# define  SZSHORT 16
# define SZPOINT 32
# define ALCHAR 8
# define ALINT 32
# define ALFLOAT 32
# define ALDOUBLE 32
# define ALLONG 32
# define ALSHORT 16
# define ALPOINT 32
# define ALSTRUCT 8
# define  ALSTACK 32 

/*	size in which constants are converted */
/*	should be long if feasable */

# define CONSZ long
# define CONFMT "%Ld"

/*	size in which offsets are kept
/*	should be large enough to cover address space in bits
*/

# define OFFSZ long

/* 	character set macro */

# define  CCTRANS(x) x

/* register cookie for stack poINTer */

# define  STKREG 13
# define ARGREG 12

/*	maximum and minimum register variables */

# define MAXRVAR 11
# define MINRVAR 6

	/* various standard pieces of code are used */
# define STDPRTREE
# define LABFMT "L%d"

/* show stack grows negatively */
#define BACKAUTO
#define BACKTEMP

/* show field hardware support on VAX */
#define FIELDOPS

/* bytes are numbered from right to left */
#define RTOLBYTES

/* we want prtree included */
# define STDPRTREE
# ifndef FORT
# define ONEPASS
#endif

# define ENUMSIZE(high,low) INT

/*	VAX-11/780 Registers */

	/* scratch registers */
# define R0 0
# define R1 1
# define R2 2
# define R3 3
# define R4 4
# define R5 5

	/* register variables */
# define R6 6
# define R7 7
# define R8 8
# define R9 9
# define R10 10
# define R11 11

	/* special purpose */
# define AP 12		/* argument pointer */
# define FP 13		/* frame pointer */
# define SP 14	/* stack pointer */
# define PC 15	/* program counter */

	/* floating registers */

	/* there are no floating point registers on the VAX */

extern int fregs;
extern int maxargs;

# define BYTEOFF(x) ((x)&03)
# define wdal(k) (BYTEOFF(k)==0)
# define BITOOR(x) ((x)>>3)  /* bit offset to oreg offset */

# define REGSZ 16

# define TMPREG FP

# define R2REGS   /* permit double indexing */

# define STOARG(p)     /* just evaluate the arguments, and be done with it... */
# define STOFARG(p)
# define STOSTARG(p)
# define genfcall(a,b) gencall(a,b)

# define NESTCALL

# define MYREADER(p) walkf(p, optim2)
int optim2();
# define special(a, b) 0
