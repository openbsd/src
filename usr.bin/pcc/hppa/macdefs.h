/*	$OpenBSD: macdefs.h,v 1.1 2007/11/16 08:36:23 otto Exp $	*/

/*
 * Copyright (c) 2007 Michael Shalayeff
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
 * Convert (multi-)character constant to integer.
 */
#define	makecc(val,i)	(lastcon = (lastcon<<8)|((val<<24)>>24))

#define	ARGINIT		(32*8)	/* bits below fp where args start */
#define	AUTOINIT	(4*8)	/* bits above fp where locals start */

/*
 * storage sizes
 */
#define	SZCHAR		8
#define	SZBOOL		8
#define	SZINT		32
#define	SZFLOAT		32
#define	SZDOUBLE	64
#define	SZLDOUBLE	64	/* or later 128 */
#define	SZLONG		32
#define	SZSHORT		16
#define	SZLONGLONG	64
#define	SZPOINT(t)	32

/*
 * alignment requirements
 */
#define	ALCHAR		8
#define	ALBOOL		8
#define	ALINT		32
#define	ALFLOAT		32
#define	ALDOUBLE	64
#define	ALLDOUBLE	64	/* 128 later */
#define	ALLONG		32
#define	ALLONGLONG	64
#define	ALSHORT		16
#define	ALPOINT		32
#define	ALSTRUCT	64
#define	ALSTACK		64

/*
 * type value limits
 */
#define	MIN_CHAR	-128
#define	MAX_CHAR	127
#define	MAX_UCHAR	255
#define	MIN_SHORT	-32768
#define	MAX_SHORT	32767
#define	MAX_USHORT	65535
#define	MIN_INT		(-0x7fffffff-1)
#define	MAX_INT		0x7fffffff
#define	MAX_UNSIGNED	0xffffffff
#define	MIN_LONG	MIN_INT
#define	MAX_LONG	MAX_INT
#define	MAX_ULONG	MAX_UNSIGNED
#define	MIN_LONGLONG	(-0x7fffffffffffffffLL-1)
#define	MAX_LONGLONG	0x7fffffffffffffffLL
#define	MAX_ULONGLONG	0xffffffffffffffffULL

#undef	CHAR_UNSIGNED
#define	BOOL_TYPE	CHAR
#define	WCHAR_TYPE	INT
#define	ENUMSIZE(high,low)	INT

typedef long long CONSZ;
typedef unsigned long long U_CONSZ;
typedef long long OFFSZ;

#define	CONFMT	"%lld"		/* format for printing constants */
#define	LABFMT	".L%d"		/* format for printing labels */
#define	STABLBL	".LL%d"		/* format for stab (debugging) labels */

#undef	BACKAUTO	/* stack grows upwards */
#undef	BACKTEMP	/* stack grows upwards */

#define	MYP2TREE(p)	myp2tree(p)

#define	FIELDOPS	/* have bit field ops */
#define	LTORBYTES	/* big endian */

#define	BYTEOFF(x)	((x)&03)
#define	wdal(k)		(BYTEOFF(k)==0)
#define	BITOOR(x)	(x)	/* bit offset to oreg offset XXX die! */

#define	STOARG(p)
#define	STOFARG(p)
#define	STOSTARG(p)

#define	szty(t)	(((t) == DOUBLE || (t) == FLOAT || \
        (t) == LONGLONG || (t) == ULONGLONG) ? 2 : (t) == LDOUBLE ? 2 : 1)

#define	R1	0
#define	RP	1
#define	FP	2
#define	R4	3
#define	R5	4
#define	R6	5
#define	R7	6
#define	R8	7
#define	R9	8
#define	R10	9
#define	R11	10
#define	R12	11
#define	R13	12
#define	R14	13
#define	R15	14
#define	R16	15
#define	R17	16
#define	R18	17
#define	T4	18
#define	T3	19
#define	T2	20
#define	T1	21
#define	ARG3	22
#define	ARG2	23
#define	ARG1	24
#define	ARG0	25
#define	DP	26
#define	RET0	27
#define	RET1	28
#define	SP	29
#define	R31	30

/* double regs overlay */
#define	RD0	31	/* r1:r31 */
#define	RD1	32	/* r5:r4 */
#define	RD2	33	/* r7:r6 */
#define	RD3	34	/* r9:r8 */
#define	RD4	35	/* r11:r10 */
#define	RD5	36	/* r13:r12 */
#define	RD6	37	/* r15:r14 */
#define	RD7	38	/* r17:r16 */
#define	TD2	39	/* t4:t3 */
#define	TD1	40	/* t2:t1 */
#define	AD2	41	/* arg3:arg2 */
#define	AD1	42	/* arg1:arg0 */
#define	RETD0	43	/* ret1:ret0 */

/* FPU regs */
#define	FR0	44
#define	FR4	45
#define	FR5	46
#define	FR6	47
#define	FR7	48
#define	FR8	49
#define	FR9	50
#define	FR10	51
#define	FR11	52
#define	FR12	53
#define	FR13	54
#define	FR14	55
#define	FR15	56
#define	FR16	57
#define	FR17	58
#define	FR18	59
#define	FR19	60
#define	FR20	61
#define	FR21	62
#define	FR22	63
#define	FR23	64
#define	FR24	65
#define	FR25	66
#define	FR26	67
#define	FR27	68
#define	FR28	69
#define	FR29	70
#define	FR30	71
#define	FR31	72

#define	FR0L	73
#define	FR0R	74
#define	FR4L	75
#define	FR4R	76
#define	FR5L	77
#define	FR5R	78
#define	FR6L	79
#define	FR6R	80
#define	FR7L	81
#define	FR7R	82
#define	FR8L	83
#define	FR8R	84
#define	FR9L	85
#define	FR9R	86
#define	FR10L	87
#define	FR10R	88
#define	FR11L	89
#define	FR11R	90
#define	FR12L	91
#define	FR12R	92
#define	FR13L	93
#define	FR13R	94
#define	FR14L	95
#define	FR14R	96
#define	FR15L	97
#define	FR15R	98
#define	FR16L	99
#define	FR16R	100
#define	FR17L	101
#define	FR17R	102
#ifdef __hppa64__
#define	FR18L	103
#define	FR18R	104
#define	FR19L	105
#define	FR19R	106
#define	FR20L	107
#define	FR20R	108
#define	FR21L	109
#define	FR21R	110
#define	FR22L	111
#define	FR22R	112
#define	FR23L	113
#define	FR23R	114
#define	FR24L	115
#define	FR24R	116
#define	FR25L	117
#define	FR25R	118
#define	FR26L	119
#define	FR26R	120
#define	FR27L	121
#define	FR27R	122
#define	FR28L	123
#define	FR28R	124
#define	FR29L	125
#define	FR29R	126
#define	FR30L	127
#define	FR30R	128
#define	FR31L	129
#define	FR31R	130

#define	MAXREGS	131
#else
#define	MAXREGS	103
#endif

#define	RSTATUS \
	SAREG|TEMPREG, 0, 0, SAREG|PERMREG, SAREG|PERMREG, \
	SAREG|PERMREG, SAREG|PERMREG, SAREG|PERMREG, SAREG|PERMREG,	\
	SAREG|PERMREG, SAREG|PERMREG, SAREG|PERMREG, SAREG|PERMREG,	\
	SAREG|PERMREG, SAREG|PERMREG, SAREG|PERMREG, SAREG|PERMREG,	\
	SAREG|PERMREG,							\
	SAREG|TEMPREG, SAREG|TEMPREG, SAREG|TEMPREG, SAREG|TEMPREG, 	\
	SAREG|TEMPREG, SAREG|TEMPREG, SAREG|TEMPREG, SAREG|TEMPREG, 	\
	0, SAREG|TEMPREG, SAREG|TEMPREG, 0, SAREG|TEMPREG,		\
	/* double overlays */						\
	SBREG, SBREG, SBREG, SBREG, SBREG, SBREG, SBREG,		\
	SBREG, SBREG, SBREG, SBREG, SBREG, SBREG,			\
	/* double-precision floats */					\
	0,								\
	SDREG|TEMPREG, SDREG|TEMPREG, SDREG|TEMPREG, SDREG|TEMPREG,	\
	SDREG|TEMPREG, SDREG|TEMPREG, SDREG|TEMPREG, SDREG|TEMPREG,	\
	SDREG|PERMREG, SDREG|PERMREG, SDREG|PERMREG, SDREG|PERMREG,	\
	SDREG|PERMREG, SDREG|PERMREG, SDREG|PERMREG, SDREG|PERMREG,	\
	SDREG|PERMREG, SDREG|PERMREG,					\
	SDREG|TEMPREG, SDREG|TEMPREG, SDREG|TEMPREG, SDREG|TEMPREG,	\
	SDREG|TEMPREG, SDREG|TEMPREG, SDREG|TEMPREG, SDREG|TEMPREG,	\
	SDREG|TEMPREG, SDREG|TEMPREG,					\
	/* single-precision floats */					\
	0, 0, SCREG, SCREG, SCREG, SCREG, SCREG, SCREG, SCREG,		\
	SCREG, SCREG, SCREG, SCREG, SCREG, SCREG, SCREG, SCREG,		\
	SCREG, SCREG, SCREG, SCREG, SCREG, SCREG, SCREG, SCREG,		\
	SCREG, SCREG, SCREG, SCREG, SCREG,
#ifdef __hppa64__
	SCREG, SCREG, SCREG,		\
	SCREG, SCREG, SCREG, SCREG, SCREG, SCREG, SCREG, SCREG,		\
	SCREG, SCREG, SCREG, SCREG, SCREG, SCREG, SCREG, SCREG,		\
	SCREG, SCREG, SCREG, SCREG, SCREG, SCREG, SCREG, SCREG, SCREG,
#endif

#define	ROVERLAP \
	{ RD0, -1 }, { -1 }, { -1 },	\
	{ RD1, -1 }, { RD1, -1 },	\
	{ RD2, -1 }, { RD2, -1 },	\
	{ RD3, -1 }, { RD3, -1 },	\
	{ RD4, -1 }, { RD4, -1 },	\
	{ RD5, -1 }, { RD5, -1 },	\
	{ RD6, -1 }, { RD6, -1 },	\
	{ RD7, -1 }, { RD7, -1 },	\
	{ -1 },				\
	{ TD2, -1 }, { TD2, -1 },	\
	{ TD1, -1 }, { TD1, -1 },	\
	{ AD2, -1 }, { AD2, -1 },	\
	{ AD1, -1 }, { AD1, -1 },	\
	{ -1 },				\
	{ RETD0, -1 }, { RETD0, -1 },	\
	{ -1 }, { RD0, -1 },		\
	{ R1, R31, -1 },		\
	{ R4, R5, -1 },			\
	{ R6, R7, -1 },			\
	{ R8, R9, -1 },			\
	{ R10, R11, -1 },		\
	{ R12, R13, -1 },		\
	{ R14, R15, -1 },		\
	{ R16, R17, -1 },		\
	{ T4, T3, -1 },			\
	{ T2, T1, -1 },			\
	{ ARG3, ARG2, -1 },		\
	{ ARG1, ARG0, -1 },		\
	{ RET1, RET0, -1 },		\
	{ -1 },				\
	{ FR4L, FR4R, -1 },		\
	{ FR5L, FR5R, -1 },		\
	{ FR6L, FR6R, -1 },		\
	{ FR7L, FR7R, -1 },		\
	{ FR8L, FR8R, -1 },		\
	{ FR9L, FR9R, -1 },		\
	{ FR10L, FR10R, -1 },		\
	{ FR11L, FR11R, -1 },		\
	{ FR12L, FR12R, -1 },		\
	{ FR13L, FR13R, -1 },		\
	{ FR14L, FR14R, -1 },		\
	{ FR15L, FR15R, -1 },		\
	{ FR16L, FR16R, -1 },		\
	{ FR17L, FR17R, -1 },		\
	{ -1 },				\
	{ -1 },				\
	{ -1 },				\
	{ -1 },				\
	{ -1 },				\
	{ -1 },				\
	{ -1 },				\
	{ -1 },				\
	{ -1 },				\
	{ -1 },				\
	{ -1 },				\
	{ -1 },				\
	{ -1 },				\
	{ -1 },				\
	{ -1 }, { -1 },			\
	{ FR4, -1 }, { FR4, -1 },	\
	{ FR5, -1 }, { FR5, -1 },	\
	{ FR6, -1 }, { FR6, -1 },	\
	{ FR7, -1 }, { FR7, -1 },	\
	{ FR8, -1 }, { FR8, -1 },	\
	{ FR9, -1 }, { FR9, -1 },	\
	{ FR10, -1 }, { FR10, -1 },	\
	{ FR11, -1 }, { FR11, -1 },	\
	{ FR12, -1 }, { FR12, -1 },	\
	{ FR13, -1 }, { FR13, -1 },	\
	{ FR14, -1 }, { FR14, -1 },	\
	{ FR15, -1 }, { FR15, -1 },	\
	{ FR16, -1 }, { FR16, -1 },	\
	{ FR17, -1 }, { FR17, -1 },
#ifdef __hppa64__
	{ FR18, -1 }, { FR18, -1 },	\
	{ FR19, -1 }, { FR19, -1 },	\
	{ FR20, -1 }, { FR20, -1 },	\
	{ FR21, -1 }, { FR21, -1 },	\
	{ FR22, -1 }, { FR22, -1 },	\
	{ FR23, -1 }, { FR23, -1 },	\
	{ FR24, -1 }, { FR24, -1 },	\
	{ FR25, -1 }, { FR25, -1 },	\
	{ FR26, -1 }, { FR26, -1 },	\
	{ FR27, -1 }, { FR27, -1 },	\
	{ FR28, -1 }, { FR28, -1 },	\
	{ FR29, -1 }, { FR29, -1 },	\
	{ FR30, -1 }, { FR30, -1 },	\
	{ FR31, -1 }, { FR31, -1 },
#endif

#define	PCLASS(p)	\
	(p->n_type == LONGLONG || p->n_type == ULONGLONG ? SBREG : \
	(p->n_type == FLOAT ? SCREG : \
	(p->n_type == DOUBLE || p->n_type == LDOUBLE ? SDREG : SAREG)))

#define	NUMCLASS	4	/* highest number of reg classes used */

int COLORMAP(int c, int *r);
TOWRD gtype(int);
#define	PERMTYPE(x) ((x) < 31? INT : ((x) < 44? LONGLONG : ((x) < 73? LDOUBLE : FLOAT)))
#define	GCLASS(x) ((x) < 31? CLASSA : ((x) < 44? CLASSB : ((x) < 73? CLASSD : CLASSC)))
#define	DECRA(x,y)	(((x) >> (y*8)) & 255)	/* decode encoded regs */
#define	ENCRD(x)	(x)			/* Encode dest reg in n_reg */
#define	ENCRA1(x)	((x) << 8)		/* A1 */
#define	ENCRA2(x)	((x) << 16)		/* A2 */
#define	ENCRA(x,y)	((x) << (8+y*8))	/* encode regs in int */
#define	RETREG(x)	(x == LONGLONG || x == ULONGLONG ? RETD0 : \
			 x == FLOAT? FR4L : \
			 x == DOUBLE || x == LDOUBLE ? FR4 : RET0)

#define	FPREG	FP	/* frame pointer */
#define	STKREG	SP	/* stack pointer */

#define	MYREADER(p)	myreader(p)
#define	MYCANON(p)	mycanon(p)
#define	MYOPTIM

#define	SFUNCALL	(MAXSPECIAL+1)	/* struct assign after function call */
#define	SPCON		(MAXSPECIAL+2)	/* smaller constant */
#define	SPICON		(MAXSPECIAL+3)	/* even smaller constant */
#define	SPNAME		(MAXSPECIAL+4)	/* ext symbol reference load/store */
