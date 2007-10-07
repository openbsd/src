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
 * MIPS port by Jan Enoksson (janeno-1@student.ltu.se) and
 * Simon Olsson (simols-1@student.ltu.se) 2005.
 */

/*
 * Machine-dependent defines for both passes.
 */

/*
 * Convert (multi-)character constant to integer.
 * Assume: If only one value; store at left side (char size), otherwise 
 * treat it as an integer.
 */
#define makecc(val,i)	lastcon = (lastcon<<8)|((val<<24)>>24);

#define ARGINIT		64	/* # bits above fp where arguments start */
#define AUTOINIT	0	/* # bits below fp where automatics start */

/*
 * Storage space requirements
 */
#define SZCHAR		8
#define SZINT		32
#define SZFLOAT		32
#define SZDOUBLE	64
#define SZLDOUBLE	96
#define SZLONG		32
#define SZSHORT		16
#define SZLONGLONG	64
#define SZPOINT(t)	32

/*
 * Alignment constraints
 */
#define ALCHAR		8
#define ALINT		32
#define ALFLOAT		32
#define ALDOUBLE	32
#define ALLDOUBLE	32
#define ALLONG		32
#define ALLONGLONG	32
#define ALSHORT		16
#define ALPOINT		32
#define ALSTRUCT	32
#define ALSTACK		32 

/*
 * Min/max values.
 */
#define	MIN_CHAR	-128
#define	MAX_CHAR	127
#define	MAX_UCHAR	255
#define	MIN_SHORT	-32768
#define	MAX_SHORT	32767
#define	MAX_USHORT	65535
#define	MIN_INT		-1
#define	MAX_INT		0x7fffffff
#define	MAX_UNSIGNED	0xffffffff
#define	MIN_LONG	MIN_INT
#define	MAX_LONG	MAX_INT
#define	MAX_ULONG	MAX_UNSIGNED
#define	MIN_LONGLONG	0x8000000000000000LL
#define	MAX_LONGLONG	0x7fffffffffffffffLL
#define	MAX_ULONGLONG	0xffffffffffffffffULL

/* Default char is unsigned */
#undef	CHAR_UNSIGNED

/*
 * Use large-enough types.
 */
typedef	long long CONSZ;
typedef	unsigned long long U_CONSZ;
typedef long long OFFSZ;

#define CONFMT	"%lld"		/* format for printing constants */
#define LABFMT	".L%d"		/* format for printing labels */
#define	STABLBL	".LL%d"		/* format for stab (debugging) labels */
#ifdef FORTRAN
#define XL 8
#define	FLABELFMT "%s:\n"
#define USETEXT ".text"
#define USECONST ".data\t0" 	/* XXX - fix */
#define USEBSS  ".data\t1" 	/* XXX - fix */
#define USEINIT ".data\t2" 	/* XXX - fix */
#define MAXREGVAR 3             /* XXX - fix */
#define BLANKCOMMON "_BLNK_"
#define MSKIREG  (M(TYSHORT)|M(TYLONG))
#define TYIREG TYLONG
#define FSZLENG  FSZLONG
#define FUDGEOFFSET 1
#define	AUTOREG	EBP
#define	ARGREG	EBP
#define ARGOFFSET 4
#endif

#define BACKAUTO 		/* stack grows negatively for automatics */
#define BACKTEMP 		/* stack grows negatively for temporaries */

#define	MYP2TREE(p) myp2tree(p);

#undef	FIELDOPS		/* no bit-field instructions */
#define RTOLBYTES		/* bytes are numbered right to left */

#define ENUMSIZE(high,low) INT	/* enums are always stored in full int */

/* Definitions mostly used in pass2 */

#define BYTEOFF(x)	((x)&03)
#define wdal(k)		(BYTEOFF(k)==0)
#define BITOOR(x)	((x)/SZCHAR)	/* bit offset to oreg offset */

#define STOARG(p)
#define STOFARG(p)
#define STOSTARG(p)
#define genfcall(a,b)	gencall(a,b)

#define	szty(t)	(((t) == DOUBLE || (t) == FLOAT || \
	(t) == LONGLONG || (t) == ULONGLONG) ? 2 : 1)

/*
 * Register names.  These must match rnames[] and rstatus[] in local2.c.
 * The crazy order of the registers are due to the current register
 * allocations strategy and should be fixed.
 */
#define	T0 0	
#define	T1 1	
#define	T2 2	
#define	T3 3	
#define	T4 4	
#define	T5 5	
#define	T6 6	
#define	T7 7
#define T8 8
#define T9 9

#define V0 10
#define V1 11

#define ZERO 12
#define AT 13

#define A0 14
#define A1 15
#define A2 16
#define A3 17

#define S0 18
#define S1 19
#define S2 20
#define S3 21
#define S4 22
#define S5 23
#define S6 24
#define S7 25

#define K0 26
#define K1 27

#define GP 28
#define SP 29
#define FP 30
#define RA 31

#define	RETREG	V0	/* Return register */
#define REGSZ	32	/* number of registers */
#define FPREG	FP	/* frame pointer */
#define STKREG	SP	/* stack pointer */
#define MINRVAR	S0	/* first register variable */
#define MAXRVAR	S7	/* last register variable */

#define	NREGREG	(MAXRVAR-MINRVAR+1)

/*
 * Register types are described by bitmasks.
 */
#define AREGS   (REGBIT(T0)|REGBIT(T1)|REGBIT(T2)|REGBIT(T3)| \
	REGBIT(T4)|REGBIT(T5)|REGBIT(T6)|REGBIT(T7)|REGBIT(T8)|REGBIT(T9)|\
 	REGBIT(V0)|REGBIT(V1)|REGBIT(A0)|REGBIT(A1)|REGBIT(A2)|REGBIT(A3)|\
    	REGBIT(S0)|REGBIT(S1)|REGBIT(S2)|REGBIT(S3)|REGBIT(S4)|REGBIT(S5)|\
    	REGBIT(S6)|REGBIT(S7))
#define TAREGS  (REGBIT(T0)|REGBIT(T1)|REGBIT(T2)|REGBIT(T3)|REGBIT(T4)|\
		 REGBIT(T5)|REGBIT(T6)|REGBIT(T7)|REGBIT(T8)|REGBIT(T9)|\
		 REGBIT(V0)|REGBIT(V1))

/* For floating point? */
#define	BREGS	0xff00
#define	TBREGS	BREGS	

//#define	MYADDEDGE(x, t) if (t < INT) { AddEdge(x, ESI); AddEdge(x, EDI); }
#define MYADDEDGE(x, t)
#define PCLASS(p) SAREG

#define MYREADER(p) myreader(p)
#define MYCANON(p) mycanon(p)
#define	MYOPTIM

#define special(a, b)	SRNOPE
