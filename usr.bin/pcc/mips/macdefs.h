/*	$OpenBSD: macdefs.h,v 1.2 2007/11/16 08:34:55 otto Exp $	*/
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

#if TARGOS == netbsd
#define USE_GAS
#endif

/*
 * Convert (multi-)character constant to integer.
 * Assume: If only one value; store at left side (char size), otherwise 
 * treat it as an integer.
 */
#define makecc(val,i)	lastcon = (lastcon<<8)|((val<<24)>>24);

#define ARGINIT		(12*8)	/* # bits above fp where arguments start */
#define AUTOINIT	(0)	/* # bits below fp where automatics start */

/*
 * Storage space requirements
 */
#define SZCHAR		8
#define SZBOOL		32
#define SZINT		32
#define SZFLOAT		32
#define SZDOUBLE	64
#define SZLDOUBLE	128
#define SZLONG		32
#define SZSHORT		16
#define SZLONGLONG	64
#define SZPOINT(t)	32

/*
 * Alignment constraints
 */
#define ALCHAR		8
#define ALBOOL		32
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

#undef	CHAR_UNSIGNED
#define TARGET_STDARGS
#define BOOL_TYPE	INT
#define WCHAR_TYPE	INT

/*
 * Use large-enough types.
 */
typedef	long long CONSZ;
typedef	unsigned long long U_CONSZ;
typedef long long OFFSZ;

#define CONFMT	"%lld"		/* format for printing constants */
#ifdef USE_GAS
#define LABFMT	"$L%d"		/* format for printing labels */
#define	STABLBL	"$LL%d"		/* format for stab (debugging) labels */
#else
#define LABFMT	"L%d"		/* format for printing labels */
#define	STABLBL	"LL%d"		/* format for stab (debugging) labels */
#endif

#define BACKAUTO 		/* stack grows negatively for automatics */
#define BACKTEMP 		/* stack grows negatively for temporaries */

#undef	FIELDOPS		/* no bit-field instructions */
#define RTOLBYTES		/* bytes are numbered right to left */

#define ENUMSIZE(high,low) INT	/* enums are always stored in full int */

/* Definitions mostly used in pass2 */

#define BYTEOFF(x)	((x)&03)
#define BITOOR(x)	(x)	/* bit offset to oreg offset */

#define	szty(t)	(((t) == DOUBLE || (t) == FLOAT || \
	(t) == LONGLONG || (t) == ULONGLONG) ? 2 : 1)

/*
 * Register names.  These must match rnames[] and rstatus[] in local2.c.
 */
#define ZERO	0
#define AT	1
#define V0	2
#define V1	3
#define A0	4
#define A1	5
#define A2	6
#define A3	7
#if defined(MIPS_N32) || defined(MIPS_N64)
#define A4	8
#define A5	9
#define A6	10
#define A7	11
#define T0	12
#define T1	13
#define	T2	14
#define	T3	15
#else
#define	T0	8
#define	T1	9
#define	T2	10
#define	T3	11
#define	T4	12
#define	T5	13
#define	T6	14
#define	T7	15
#endif
#define S0	16
#define S1	17
#define S2	18
#define S3	19
#define S4	20
#define S5	21
#define S6	22
#define S7	23
#define T8	24
#define T9	25
#define K0	26
#define K1	27
#define GP	28
#define SP	29
#define FP	30
#define RA	31

#define V0V1	32
#define A0A1	33
#define A1A2	34
#define A2A3	35
#if defined(MIPS_N32) || defined(MIPS_N64)
#define A3A4	36
#define A4A5	37
#define A5A6	38
#define A6A7	39
#define T0T1	41
#define T1T2	42
#define T2T3	43
#else
#define T0T1	37
#define T1T2	38
#define T2T3	39
#define T3T4	40
#define T4T5	41
#define T5T6	42
#define T6T7	43
#endif
#define T8T9	44
#define S0S1	45
#define S1S2	46
#define S2S3	47
#define S3S4	48
#define S4S5	49
#define S5S6	50
#define S6S7	51

#define F0	52
#define F1	53
#define F2	54

#define MAXREGS 55
#define NUMCLASS 3

#define RETREG(x)	((x) == ULONGLONG || (x) == LONGLONG ? V0V1 : V0)
#define FPREG	FP	/* frame pointer */
#define STKREG	SP

#if defined(MIPS_N32) || defined(MIPS_N64)
#define MIPS_NARGREGS	8
#else
#define MIPS_NARGREGS	4
#endif

#define RSTATUS \
	0, 0,								\
	SAREG|TEMPREG, SAREG|TEMPREG, 					\
	SAREG|TEMPREG, SAREG|TEMPREG, SAREG|TEMPREG, SAREG|TEMPREG,	\
	SAREG|TEMPREG, SAREG|TEMPREG, SAREG|TEMPREG, SAREG|TEMPREG,	\
	SAREG|TEMPREG, SAREG|TEMPREG, SAREG|TEMPREG, SAREG|TEMPREG,	\
	SAREG|PERMREG, SAREG|PERMREG, SAREG|PERMREG, SAREG|PERMREG,	\
	SAREG|PERMREG, SAREG|PERMREG, SAREG|PERMREG, SAREG|PERMREG,	\
	SAREG|TEMPREG, SAREG|TEMPREG, 					\
	0, 0,								\
	0, 0, 0, 0,							\
	\
	SBREG|TEMPREG,							\
	SBREG|TEMPREG, SBREG|TEMPREG, SBREG|TEMPREG,			\
 	SBREG|TEMPREG, /* only available on n32/n64 */			\
	SBREG|TEMPREG, SBREG|TEMPREG, SBREG|TEMPREG,			\
	SBREG|TEMPREG, /* only available on o32 */			\
	SBREG|TEMPREG,							\
	SBREG|TEMPREG, SBREG|TEMPREG, SBREG|TEMPREG,			\
	SBREG, SBREG, SBREG, SBREG,					\
	SBREG, SBREG, SBREG, 						\
	SCREG, SCREG, SCREG

#define ROVERLAP \
	{ -1 }, { -1 },							\
	{ V0V1, -1 }, { V0V1, -1 },					\
	{ A0A1, -1 },							\
	{ A0A1, A1A2, -1 },						\
	{ A1A2, A2A3, -1 },						\
	{ A2A3, -1 },							\
	\
	{ T0T1, -1 },							\
	{ T0T1, T1T2, -1 },						\
	{ T1T2, T2T3, -1 },						\
	{ T2T3, T3T4, -1 },						\
	{ T3T4, T4T5, -1 },						\
	{ T4T5, T5T6, -1 },						\
	{ T5T6, T6T7, -1 },						\
	{ T6T7, -1 },							\
	\
	{ S0S1, -1 },							\
	{ S0S1, S1S2, -1 },						\
	{ S1S2, S2S3, -1 },						\
	{ S2S3, S3S4, -1 },						\
	{ S3S4, S4S5, -1 },						\
	{ S4S5, S5S6, -1 },						\
	{ S5S6, S6S7, -1 },						\
	{ S6S7, -1 },							\
	\
	{ T8T9, -1 },							\
	{ T8T9, -1 },							\
	\
	{ -1 }, { -1 },							\
	{ -1 }, { -1 }, { -1 }, { -1 },					\
	\
	{ V0, V1, -1 },							\
	\
	{ A0, A1, -1 },							\
	{ A1, A2, -1 },							\
	{ A2, A3, -1 },							\
	\
	{ -1 }, /* only useful on n32/n64 */				\
	{ T0, T1, T1T2, -1 },						\
	{ T1, T2, T0T1, T2T3, -1 },					\
	{ T2, T3, T1T2, T3T4, -1 },					\
	{ T3, T4, T2T3, T4T5, -1 }, /* only useful on o32 */		\
	{ T4, T5, T3T4, T5T6, -1 },					\
	{ T5, T6, T4T5, T6T7, -1 },					\
	{ T6, T7, T5T6, -1 },						\
	\
	{ T8, T9, -1 },							\
	\
	{ S0, S1, S1S2, -1 },						\
	{ S1, S2, S0S1, S2S3, -1 },					\
	{ S2, S3, S1S2, S3S4, -1 },					\
	{ S3, S4, S2S3, S4S5, -1 },					\
	{ S4, S5, S3S4, S5S6, -1 },					\
	{ S5, S6, S4S5, S6S7, -1 },					\
	{ S6, S7, S5S6, -1 },						\
	\
	{ -1 },								\
	{ -1 },								\
	{ -1 },


#define GCLASS(x)	(x < 32 ? CLASSA : (x < 52 ? CLASSB : CLASSC))
#define PCLASS(p) (p->n_type == LONGLONG || p->n_type == ULONGLONG ? SBREG : \
                  (p->n_type >= FLOAT && p->n_type <= LDOUBLE ? SCREG : SAREG))
#define DECRA(x,y)	(((x) >> (y*6)) & 63)   /* decode encoded regs */
#define ENCRA(x,y)	((x) << (6+y*6))        /* encode regs in int */

int COLORMAP(int c, int *r);
