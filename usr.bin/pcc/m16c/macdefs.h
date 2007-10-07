/*	$OpenBSD: macdefs.h,v 1.1 2007/10/07 17:58:51 otto Exp $	*/
/*
 * Copyright (c) 2003 Anders Magnusson (ragge@ludd.luth.se).
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
 * 3. The name of the author may not be used to endorse or promote products
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

/*
 * Machine-dependent defines for both passes.
 */
#define makecc(val,i)   lastcon = (lastcon<<8)|((val<<8)>>8);

#define ARGINIT		40	/* # bits above fp where arguments start */
#define AUTOINIT	0	/* # bits below fp where automatics start */

/*
 * Convert (multi-)character constant to integer.
 * Assume: If only one value; store at left side (char size), otherwise 
 * treat it as an integer.
 */

/*
 * Storage space requirements
 */
#define SZCHAR		8
#define SZINT		16
#define SZFLOAT         16
#define SZDOUBLE        16
#define SZLDOUBLE       16
#define SZLONG		32
#define SZSHORT		16
#define SZLONGLONG      32
/* pointers are of different sizes on m16c */
#define SZPOINT(t) 	(ISFTN(DECREF(t)) ? 32 : 16)

/*
 * Alignment constraints
 */
#define ALCHAR		8
#define ALINT		16
#define ALFLOAT		16
#define ALDOUBLE	16
#define ALLDOUBLE	16
#define ALLONG		16
#define ALLONGLONG	16
#define ALSHORT		16
#define ALPOINT		16
#define ALSTRUCT	16
#define ALSTACK		16

/*
 * Min/max values.
 */
#define MIN_CHAR	-128
#define MAX_CHAR	127
#define MAX_UCHAR	255
#define MIN_SHORT	-32768
#define MAX_SHORT	32767
#define MAX_USHORT	65535
#define MIN_INT		-32768
#define MAX_INT		32767
#define MAX_UNSIGNED	65535
#define MIN_LONG	-2147483648
#define MAX_LONG	2147483647
#define MAX_ULONG	4294967295UL
#define MIN_LONGLONG	-2147483648
#define MAX_LONGLONG	2147483647
#define MAX_ULONGLONG	4294967295UL

/* Default char is unsigned */
#undef	CHAR_UNSIGNED

/*
 * Use large-enough types.
 */
typedef long long CONSZ;
typedef unsigned long long U_CONSZ;
typedef long long OFFSZ;

#define CONFMT	"%lld"		/* format for printing constants */
#define LABFMT	"L%d"		/* format for printing labels */

#define BACKAUTO		/* stack grows negatively for automatics */
#define BACKTEMP		/* stack grows negatively for temporaries */

//#define	MYP2TREE(p) myp2tree(p);

#undef	FIELDOPS		/* no bit-field instructions */
#define RTOLBYTES		/* bytes are numbered right to left */

/* Definitions mostly used in pass2 */

#define BYTEOFF(x)	1
#define BITOOR(x)	((x)/SZCHAR)	/* bit offset to oreg offset */

#define STOARG(p)
#define STOFARG(p)
#define STOSTARG(p)
#define genfcall(a,b)	gencall(a,b)

#define szty(t) (((t) == LONG || (t) == ULONG || \
	(ISPTR(t) && ISFTN(DECREF(t)))) ? 2 : 1)

/*
 * m16c register classes:
 * A - 16-bit data registers R0-R3
 * B - 16-bit address registers A0-A1
 * C - 8-bit data registers R0H, R0L, R1H, R1L
 */

#define R0	0
#define R2	1
#define R1	2
#define R3	3

#define A0	4
#define A1	5
#define FB	6
#define SP	7

#define R0H     8
#define R0L     9
#define R1H     10
#define R1L     11

#define NUMCLASS 4      /* Number of register classes */

#define RETREG(x)	(x == CHAR || x == UCHAR ? R0L : R0)

#define FPREG	FB	/* frame pointer */
#define STKREG	SP	/* stack pointer */

#if 0
#define REGSZ	8	/* Number of registers */
#define MINRVAR R1	/* first register variable */
#define MAXRVAR R2	/* last register variable */
#endif

#define MAXREGS 12 /* 12 registers */

#define RSTATUS \
	SAREG|TEMPREG, SAREG|PERMREG, SAREG|TEMPREG, SAREG|PERMREG, \
	SBREG|TEMPREG, SBREG|PERMREG, 0, 0, SCREG, SCREG, SCREG, SCREG,

#define ROVERLAP \
	{R0H, R0L, -1},\
	{-1},\
	{R1H, R1L, -1},\
	{-1},\
\
	{-1},\
	{-1},\
\
	{-1},\
	{-1},\
\
	{R0, -1},\
	{R0, -1},\
	{R1, -1},\
	{R1, -1},

#define PCLASS(p) (p->n_type <= UCHAR ? SCREG : ISPTR(p->n_type) ? SBREG:SAREG)
	    
int COLORMAP(int c, int *r);
#define	GCLASS(x) (x < 4 ? CLASSA : x < 6 ? CLASSB : x < 12 ? CLASSC : CLASSD)
#define DECRA(x,y)	(((x) >> (y*6)) & 63)	/* decode encoded regs */
#define	ENCRD(x)	(x)		/* Encode dest reg in n_reg */
#define ENCRA1(x)	((x) << 6)	/* A1 */
#define ENCRA2(x)	((x) << 12)	/* A2 */
#define ENCRA(x,y)	((x) << (6+y*6))	/* encode regs in int */

#define	MYADDEDGE(x, t)
#define MYREADER(p) myreader(p)
#define	MYP2TREE(p) myp2tree(p)

#if 0
#define MYCANON(p) mycanon(p)
#define MYOPTIM
#endif

#ifndef NEW_READER
//#define TAILCALL
#endif
#define	SFTN	(SPECIAL|6)
