/*	$OpenBSD: macdefs.h,v 1.3 2008/04/11 20:45:52 stefan Exp $	*/

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

#define	FIELDOPS	/* have bit field ops */
#define	LTORBYTES	/* big endian */

#define	BYTEOFF(x)	((x)&03)
#define	wdal(k)		(BYTEOFF(k)==0)
#define	BITOOR(x)	(x)	/* bit offset to oreg offset XXX die! */

#define	STOARG(p)
#define	STOFARG(p)
#define	STOSTARG(p)

#define	szty(t)	(((t) == DOUBLE || (t) == LONGLONG || (t) == ULONGLONG) ? 2 : \
	    (t) == LDOUBLE ? 2 : 1)

#define	R0	0
#define	R1	1
#define	RP	2
#define	FP	3
#define	R4	4
#define	R5	5
#define	R6	6
#define	R7	7
#define	R8	8
#define	R9	9
#define	R10	10
#define	R11	11
#define	R12	12
#define	R13	13
#define	R14	14
#define	R15	15
#define	R16	16
#define	R17	17
#define	R18	18
#define	T4	19
#define	T3	20
#define	T2	21
#define	T1	22
#define	ARG3	23
#define	ARG2	24
#define	ARG1	25
#define	ARG0	26
#define	DP	27
#define	RET0	28
#define	RET1	29
#define	SP	30
#define	R31	31

/* double regs overlay */
#define	RD0	32	/* r0:r0 */
#define	RD1	33	/* r1:r31 */
#define	RD2	34	/* r1:t4 */
#define	RD3	35	/* r1:t3 */
#define	RD4	36	/* r1:t2 */
#define	RD5	37	/* r1:t1 */
#define	RD6	38	/* r31:t4 */
#define	RD7	39	/* r31:t3 */
#define	RD8	40	/* r31:t2 */
#define	RD9	41	/* r31:t1 */
#define	RD10	42	/* r4:r18 */
#define	RD11	43	/* r5:r4 */
#define	RD12	44	/* r6:r5 */
#define	RD13	45	/* r7:r6 */
#define	RD14	46	/* r8:r7 */
#define	RD15	47	/* r9:r8 */
#define	RD16	48	/* r10:r9 */
#define	RD17	49	/* r11:r10 */
#define	RD18	50	/* r12:r11 */
#define	RD19	51	/* r13:r12 */
#define	RD20	52	/* r14:r13 */
#define	RD21	53	/* r15:r14 */
#define	RD22	54	/* r16:r15 */
#define	RD23	55	/* r17:r16 */
#define	RD24	56	/* r18:r17 */
#define	TD4	57	/* t1:t4 */
#define	TD3	58	/* t4:t3 */
#define	TD2	59	/* t3:t2 */
#define	TD1	60	/* t2:t1 */
#define	AD2	61	/* arg3:arg2 */
#define	AD1	62	/* arg1:arg0 */
#define	RETD0	63	/* ret1:ret0 */

/* FPU regs */
#define	FR0	64
#define	FR4	65
#define	FR5	66
#define	FR6	67
#define	FR7	68
#define	FR8	69
#define	FR9	70
#define	FR10	71
#define	FR11	72
#define	FR12	73
#define	FR13	74
#define	FR14	75
#define	FR15	76
#define	FR16	77
#define	FR17	78
#define	FR18	79
#define	FR19	80
#define	FR20	81
#define	FR21	82
#define	FR22	83
#define	FR23	84
#define	FR24	85
#define	FR25	86
#define	FR26	87
#define	FR27	88
#define	FR28	89
#define	FR29	90
#define	FR30	91
#define	FR31	92

#define	FR0L	93
#define	FR0R	94
#define	FR4L	95
#define	FR4R	96
#define	FR5L	97
#define	FR5R	98
#define	FR6L	99
#define	FR6R	100
#define	FR7L	101
#define	FR7R	102
#define	FR8L	103
#define	FR8R	104
#define	FR9L	105
#define	FR9R	106
#define	FR10L	107
#define	FR10R	108
#define	FR11L	109
#define	FR11R	110
#define	FR12L	111
#define	FR12R	112
#define	FR13L	113
#define	FR13R	114
#define	FR14L	115
#define	FR14R	116
#define	FR15L	117
#define	FR15R	118
#define	FR16L	119
#define	FR16R	120
#define	FR17L	121
#define	FR17R	122
#define	FR18L	123
#define	FR18R	124
#ifdef __hppa64__
#define	FR19L	125
#define	FR19R	126
#define	FR20L	127
#define	FR20R	128
#define	FR21L	129
#define	FR21R	130
#define	FR22L	131
#define	FR22R	132
#define	FR23L	133
#define	FR23R	134
#define	FR24L	135
#define	FR24R	136
#define	FR25L	137
#define	FR25R	138
#define	FR26L	139
#define	FR26R	140
#define	FR27L	141
#define	FR27R	142
#define	FR28L	143
#define	FR28R	144
#define	FR29L	145
#define	FR29R	146
#define	FR30L	147
#define	FR30R	148
#define	FR31L	149
#define	FR31R	150

#define	MAXREGS	151
#else
#define	MAXREGS	125
#endif

#define	RSTATUS \
	0, SAREG|TEMPREG, 0, 0, SAREG|PERMREG, SAREG|PERMREG,		\
	SAREG|PERMREG, SAREG|PERMREG, SAREG|PERMREG, SAREG|PERMREG,	\
	SAREG|PERMREG, SAREG|PERMREG, SAREG|PERMREG, SAREG|PERMREG,	\
	SAREG|PERMREG, SAREG|PERMREG, SAREG|PERMREG, SAREG|PERMREG,	\
	SAREG|PERMREG,							\
	SAREG|TEMPREG, SAREG|TEMPREG, SAREG|TEMPREG, SAREG|TEMPREG, 	\
	SAREG|TEMPREG, SAREG|TEMPREG, SAREG|TEMPREG, SAREG|TEMPREG, 	\
	0, SAREG|TEMPREG, SAREG|TEMPREG, 0, SAREG|TEMPREG,		\
	/* double overlays */						\
	0,								\
	SBREG, SBREG, SBREG, SBREG, SBREG, SBREG, SBREG, SBREG,		\
	SBREG, SBREG, SBREG, SBREG, SBREG, SBREG, SBREG, SBREG,		\
	SBREG, SBREG, SBREG, SBREG, SBREG, SBREG, SBREG, SBREG,		\
	SBREG, SBREG, SBREG, SBREG, SBREG, SBREG, SBREG,		\
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
	0, 0,								\
	SCREG, SCREG, SCREG, SCREG, SCREG, SCREG, SCREG, SCREG,		\
	SCREG, SCREG, SCREG, SCREG, SCREG, SCREG, SCREG, SCREG,		\
	SCREG, SCREG, SCREG, SCREG, SCREG, SCREG, SCREG, SCREG,		\
	SCREG, SCREG, SCREG, SCREG, SCREG, SCREG,
#ifdef __hppa64__
	SCREG, SCREG,		\
	SCREG, SCREG, SCREG, SCREG, SCREG, SCREG, SCREG, SCREG,		\
	SCREG, SCREG, SCREG, SCREG, SCREG, SCREG, SCREG, SCREG,		\
	SCREG, SCREG, SCREG, SCREG, SCREG, SCREG, SCREG, SCREG,
#endif

#define	ROVERLAP \
	{ -1 },				\
	{ RD1, RD2, RD3, RD4, RD5, -1 },\
	{ -1 }, { -1 },			\
	{ RD10, RD11, -1 },		\
	{ RD11, RD12, -1 },		\
	{ RD12, RD13, -1 },		\
	{ RD13, RD14, -1 },		\
	{ RD14, RD15, -1 },		\
	{ RD15, RD16, -1 },		\
	{ RD16, RD17, -1 },		\
	{ RD17, RD18, -1 },		\
	{ RD18, RD19, -1 },		\
	{ RD19, RD20, -1 },		\
	{ RD20, RD21, -1 },		\
	{ RD21, RD22, -1 },		\
	{ RD22, RD23, -1 },		\
	{ RD23, RD24, -1 },		\
	{ RD24, RD10, -1 },		\
	{ TD1, TD4, -1 },		\
	{ TD3, TD2, -1 },		\
	{ TD1, TD2, -1 },		\
	{ TD1, TD4, -1 },		\
	{ AD2, -1 }, { AD2, -1 },	\
	{ AD1, -1 }, { AD1, -1 },	\
	{ -1 },				\
	{ RETD0, -1 }, { RETD0, -1 },	\
	{ -1 },				\
	{ RD1, RD5, RD6, RD7, RD8, -1 },\
	{ -1 },				\
	{ R1, R31, -1 },		\
	{ R1, T4, -1 },			\
	{ R1, T3, -1 },			\
	{ R1, T2, -1 },			\
	{ R1, T1, -1 },			\
	{ R31, T4, -1 },		\
	{ R31, T3, -1 },		\
	{ R31, T2, -1 },		\
	{ R31, T1, -1 },		\
	{ R4, R18, -1 },		\
	{ R5, R4, -1 },			\
	{ R6, R5, -1 },			\
	{ R7, R6, -1 },			\
	{ R8, R7, -1 },			\
	{ R9, R8, -1 },			\
	{ R10, R9, -1 },		\
	{ R11, R10, -1 },		\
	{ R12, R11, -1 },		\
	{ R13, R12, -1 },		\
	{ R14, R15, -1 },		\
	{ R15, R14, -1 },		\
	{ R16, R15, -1 },		\
	{ R17, R16, -1 },		\
	{ R18, R17, -1 },		\
	{ T1, T4, -1 },			\
	{ T4, T3, -1 },			\
	{ T3, T2, -1 },			\
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
	{ FR18L, FR18R, -1 },		\
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
	{ FR17, -1 }, { FR17, -1 },	\
	{ FR18, -1 }, { FR18, -1 },
#ifdef __hppa64__
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
#define	PERMTYPE(x) ((x) < 32? INT : ((x) < 64? LONGLONG : ((x) < 93? LDOUBLE : FLOAT)))
#define	GCLASS(x) ((x) < 32? CLASSA : ((x) < 64? CLASSB : ((x) < 93? CLASSD : CLASSC)))
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
#define	SPIMM		(MAXSPECIAL+4)	/* immidiate const for depi/comib */
#define	SPNAME		(MAXSPECIAL+5)	/* ext symbol reference load/store */
