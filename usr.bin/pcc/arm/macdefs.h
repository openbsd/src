/*	$OpenBSD: macdefs.h,v 1.3 2008/04/11 20:45:52 stefan Exp $	*/
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

/*
 * Convert (multi-)character constant to integer.
 */
#define makecc(val,i)	lastcon = (lastcon<<8)|((val<<24)>>24);

/*
 * Storage space requirements
 */
#define SZCHAR		8
#define SZBOOL		32
#define SZINT		32
#define SZFLOAT		32
#define SZDOUBLE	64
#define SZLDOUBLE	64
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

#define	BOOL_TYPE	INT	/* what used to store _Bool */
#define	WCHAR_TYPE	INT	/* what used to store wchar_t */

/*
 * Use large-enough types.
 */
typedef	long long CONSZ;
typedef	unsigned long long U_CONSZ;
typedef long long OFFSZ;

#define CONFMT	"#%lld"		/* format for printing constants */
#define LABFMT	".L%d"		/* format for printing labels */
#define	STABLBL	"LL%d"		/* format for stab (debugging) labels */
#define STAB_LINE_ABSOLUTE	/* S_LINE fields use absolute addresses */

#undef	FIELDOPS		/* no bit-field instructions */

#define ENUMSIZE(high,low) INT	/* enums are always stored in full int */

/* Definitions mostly used in pass2 */

#define BYTEOFF(x)	((x)&03)
#define wdal(k)		(BYTEOFF(k)==0)
#define BITOOR(x)	(x)	/* bit offset to oreg offset XXX die! */

#define STOARG(p)
#define STOFARG(p)
#define STOSTARG(p)

#define	szty(t)	(((t) == DOUBLE || (t) == LDOUBLE || \
	(t) == LONGLONG || (t) == ULONGLONG) ? 2 : 1)

#define R0	0
#define R1	1
#define R2	2
#define R3	3
#define R4	4
#define R5	5
#define R6	6
#define R7	7
#define R8	8
#define R9	9
#define R10	10

#define FP	11
#define IP	12
#define SP	13
#define LR	14
#define PC	15

#define R0R1	16
#define R1R2	17
#define R2R3	18
#define R3R4	19
#define R4R5	20
#define R5R6	21
#define R6R7	22
#define R7R8	23
#define R8R9	24
#define R9R10	25

#define F0	26
#define F1	27
#define F2	28
#define F3	29
#define F4	30
#define F5	31
#define F6	32
#define F7	33

#define NUMCLASS 3
#define	MAXREGS	34

#define RSTATUS \
	SAREG|TEMPREG, SAREG|TEMPREG, SAREG|TEMPREG, SAREG|TEMPREG,	\
	SAREG|PERMREG, SAREG|PERMREG, SAREG|PERMREG, SAREG|PERMREG,	\
	SAREG|PERMREG, SAREG|PERMREG, SAREG|PERMREG,			\
	0, 0, 0, 0, 0,							\
        SBREG|TEMPREG, SBREG|TEMPREG, SBREG|TEMPREG, SBREG,		\
        SBREG, SBREG, SBREG, SBREG, SBREG, SBREG,			\
	SCREG, SCREG, SCREG, SCREG,					\
	SCREG, SCREG, SCREG, SCREG,					\

#define ROVERLAP \
	{ R0R1, -1 },					\
	{ R0R1, R1R2, -1 },				\
	{ R1R2, R2R3, -1 },				\
	{ R2R3, R3R4, -1 },				\
	{ R3R4, R4R5, -1 },				\
	{ R4R5, R5R6, -1 },				\
	{ R5R6, R6R7, -1 },				\
	{ R6R7, R7R8, -1 },				\
	{ R7R8, R8R9, -1 },				\
	{ R8R9, R9R10, -1 },				\
	{ R9R10, -1 },					\
	{ -1 }, 					\
	{ -1 }, 					\
	{ -1 }, 					\
	{ -1 }, 					\
	{ -1 }, 					\
	{ R0, R1, R1R2, -1 },				\
	{ R1, R2, R0R1, R2R3, -1 },			\
	{ R2, R3, R1R2, R3R4, -1 },			\
	{ R3, R4, R2R3, R4R5, -1 },			\
	{ R4, R5, R3R4, R5R6, -1 },			\
	{ R5, R6, R4R5, R6R7, -1 },			\
	{ R6, R7, R5R6, R7R8, -1 },			\
	{ R7, R8, R6R7, R8R9, -1 },			\
	{ R8, R9, R7R8, R9R10, -1 },			\
	{ R9, R10, R8R9, -1 },				\
	{ -1, },					\
	{ -1, },					\
	{ -1, },					\
	{ -1, },					\
	{ -1, },					\
	{ -1, },					\
	{ -1, },					\
	{ -1, },					\

#define BACKTEMP 		/* stack grows negatively for temporaries */
#define BACKAUTO 		/* stack grows negatively for automatics */

#define ARGINIT		(4*8)	/* # bits above fp where arguments start */
#define AUTOINIT	(12*8)	/* # bits above fp where automatics start */

#undef	FIELDOPS		/* no bit-field instructions */
#define RTOLBYTES 1		/* bytes are numbered right to left */

/* XXX - to die */
#define FPREG   FP	/* frame pointer */

/* Return a register class based on the type of the node */
#define PCLASS(p)	(1 << gclass((p)->n_type))

#define GCLASS(x)	(x < 16 ? CLASSA : x < 26 ? CLASSB : CLASSC)
#define DECRA(x,y)      (((x) >> (y*6)) & 63)   /* decode encoded regs */
#define ENCRD(x)        (x)             /* Encode dest reg in n_reg */
#define ENCRA1(x)       ((x) << 6)      /* A1 */
#define ENCRA2(x)       ((x) << 12)     /* A2 */
#define ENCRA(x,y)      ((x) << (6+y*6))        /* encode regs in int */
#define RETREG(x)	retreg(x)

int COLORMAP(int c, int *r);
int retreg(int ty);
int features(int f);

#define FEATURE_BIGENDIAN	0x00010000
#define FEATURE_HALFWORDS	0x00020000	/* ldrsh/ldrh, ldrsb */
#define FEATURE_EXTEND		0x00040000	/* sxth, sxtb, uxth, uxtb */
#define FEATURE_MUL		0x00080000
#define FEATURE_MULL		0x00100000
#define FEATURE_FPA		0x10000000
#define FEATURE_VFP		0x20000000
#define FEATURE_HARDFLOAT	(FEATURE_FPA|FEATURE_VFP)

#define TARGET_STDARGS
#define TARGET_BUILTINS						\
	{ "__builtin_stdarg_start", arm_builtin_stdarg_start },	\
	{ "__builtin_va_arg", arm_builtin_va_arg },		\
	{ "__builtin_va_end", arm_builtin_va_end },		\
	{ "__builtin_va_copy", arm_builtin_va_copy },

#define NODE struct node
struct node;
NODE *arm_builtin_stdarg_start(NODE *f, NODE *a);
NODE *arm_builtin_va_arg(NODE *f, NODE *a);
NODE *arm_builtin_va_end(NODE *f, NODE *a);
NODE *arm_builtin_va_copy(NODE *f, NODE *a);
#undef NODE

#define COM     "\t@ "
#define NARGREGS	4
