/*	$OpenBSD: macdefs.h,v 1.2 2007/12/22 13:13:06 stefan Exp $	*/
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
 * Assume: If only one value; store at left side (char size), otherwise 
 * treat it as an integer.
 */
#define makecc(val,i) {			\
	if (i == 0) { lastcon = val;	\
	} else if (i == 1) { lastcon = (lastcon << 9) | val; lastcon <<= 18; \
	} else { lastcon |= (val << (27 - (i * 9))); } }

#define ARGINIT		36	/* # bits below fp where arguments start */
#define AUTOINIT	36	/* # bits above fp where automatics start */

/*
 * Storage space requirements
 */
#define SZCHAR		9
#define SZBOOL		36
#define SZINT		36
#define SZFLOAT		36
#define SZDOUBLE	72
#define SZLDOUBLE	72
#define SZLONG		36
#define SZSHORT		18
#define SZPOINT(x)	36
#define SZLONGLONG	72

/*
 * Alignment constraints
 */
#define ALCHAR		9
#define ALBOOL		36
#define ALINT		36
#define ALFLOAT		36
#define ALDOUBLE	36
#define ALLDOUBLE	36
#define ALLONG		36
#define ALLONGLONG	36
#define ALSHORT		18
#define ALPOINT		36
#define ALSTRUCT	36
#define ALSTACK		36 

/*
 * Max values.
 */
#define	MIN_CHAR	-256
#define	MAX_CHAR	255
#define	MAX_UCHAR	511
#define	MIN_SHORT	-131072
#define	MAX_SHORT	131071
#define	MAX_USHORT	262143
#define	MIN_INT		(-0377777777777LL-1)
#define	MAX_INT		0377777777777LL
#define	MAX_UNSIGNED	0777777777777ULL
#define	MIN_LONG	(-0377777777777LL-1)
#define	MAX_LONG	0377777777777LL
#define	MAX_ULONG	0777777777777ULL
#define	MIN_LONGLONG	(000777777777777777777777LL-1)	/* XXX cross */
#define	MAX_LONGLONG	000777777777777777777777LL	/* XXX cross */
#define	MAX_ULONGLONG	001777777777777777777777ULL	/* XXX cross */

/* Default char is unsigned */
#define TARGET_STDARGS
#define	CHAR_UNSIGNED
#define WCHAR_TYPE	INT	/* what used to store wchar_t */
#define	BOOL_TYPE	INT

/*
 * Use large-enough types.
 */
typedef	long long CONSZ;
typedef	unsigned long long U_CONSZ;
typedef long long OFFSZ;

#define CONFMT	"0%llo"		/* format for printing constants */
#define LABFMT	".L%d"		/* format for printing labels */
#define STABLBL ".LL%d"		/* format for stab (debugging) labels */

#undef BACKAUTO 		/* stack grows negatively for automatics */
#undef BACKTEMP 		/* stack grows negatively for temporaries */

#undef	FIELDOPS		/* no bit-field instructions */
#undef	RTOLBYTES		/* bytes are numbered left to right */

#define ENUMSIZE(high,low) INT	/* enums are always stored in full int */

/* Definitions mostly used in pass2 */

#define BYTEOFF(x)	((x)&03)
#define wdal(k)		(BYTEOFF(k)==0)
#define BITOOR(x)	((x)/36)		/* bit offset to oreg offset */

#define STOARG(p)
#define STOFARG(p)
#define STOSTARG(p)
#define genfcall(a,b)	gencall(a,b)

#define	szty(t)	(((t) == DOUBLE || (t) == FLOAT || \
	(t) == LONGLONG || (t) == ULONGLONG) ? 2 : 1)

#define	shltype(o, p) \
	((o) == REG || (o) == NAME || (o) == ICON || \
	 (o) == OREG || ((o) == UMUL && shumul((p)->n_left)))

#undef	SPECIAL_INTEGERS

/*
 * Special shapes used in code generation.
 */
#define	SUSHCON	(SPECIAL|6)	/* unsigned short constant */
#define	SNSHCON	(SPECIAL|7)	/* negative short constant */
#define	SILDB	(SPECIAL|8)	/* use ildb here */

/*
 * Register allocator definitions.
 *
 * The pdp10 has 16 general-purpose registers, but the two
 * highest are used as sp and fp.  Register 0 has special 
 * constraints in its possible use as index register.
 * All regs can be used as pairs, named by the lowest number.
 * In here we call the registers Rn and the pairs XRn, in assembler
 * just its number prefixed with %.
 * 
 * R1/XR1 are return registers.
 *
 * R0 is currently not used.
 */

#define	MAXREGS		29 /* 16 + 13 regs */
#define	NUMCLASS	2

#define R0	00
#define R1	01
#define R2	02
#define R3	03
#define R4	04
#define R5	05
#define R6	06
#define R7	07
#define R10	010
#define R11	011
#define R12	012
#define R13	013
#define R14	014
#define R15	015
#define R16	016
#define R17	017
#define FPREG	R16		/* frame pointer */
#define STKREG	R17		/* stack pointer */


#define XR0	020
#define XR1	021
#define XR2	022
#define XR3	023
#define XR4	024
#define XR5	025
#define XR6	026
#define XR7	027
#define XR10	030
#define XR11	031
#define XR12	032
#define XR13	033
#define XR14	034


#define RSTATUS \
	0, SAREG|TEMPREG, SAREG|TEMPREG, SAREG|TEMPREG,			\
	SAREG|TEMPREG, SAREG|TEMPREG, SAREG|TEMPREG, SAREG|TEMPREG,	\
	SAREG|PERMREG, SAREG|PERMREG, SAREG|PERMREG, SAREG|PERMREG,	\
	SAREG|PERMREG, SAREG|PERMREG, 0, 0,				\
	SBREG, SBREG, SBREG, SBREG, SBREG, SBREG, SBREG, SBREG,		\
	SBREG, SBREG, SBREG, SBREG, SBREG,

#define ROVERLAP \
        { XR0, -1 },			\
        { XR0, XR1, -1 },		\
        { XR1, XR2, -1 },		\
        { XR2, XR3, -1 },		\
        { XR3, XR4, -1 },		\
        { XR4, XR5, -1 },		\
        { XR5, XR6, -1 },		\
        { XR6, XR7, -1 },		\
        { XR7, XR10, -1 },		\
        { XR10, XR11, -1 },		\
        { XR11, XR12, -1 },		\
        { XR12, XR13, -1 },		\
        { XR13, XR14, -1 },		\
        { XR14, -1 },			\
        { -1 },				\
        { -1 },				\
        { R0, R1, XR1, -1 },		\
        { R1, R2, XR0, XR2, -1 },	\
        { R2, R3, XR1, XR3, -1 },	\
        { R3, R4, XR2, XR4, -1 },	\
        { R4, R5, XR3, XR5, -1 },	\
        { R5, R6, XR4, XR6, -1 },	\
        { R6, R7, XR5, XR7, -1 },	\
        { R7, R10, XR6, XR10, -1 },	\
        { R10, R11, XR7, XR11, -1 },	\
        { R11, R12, XR10, XR12, -1 },	\
        { R12, R13, XR11, XR13, -1 },	\
        { R13, R14, XR12, XR14, -1 },	\
        { R14, R15, XR13, -1 },

/* Return a register class based on the type of the node */
#define PCLASS(p) (szty(p->n_type) == 2 ? SBREG : SAREG)
#define RETREG(x) (szty(x) == 2 ? XR1 : R1)
#define DECRA(x,y)      (((x) >> (y*6)) & 63)   /* decode encoded regs */
#define ENCRD(x)        (x)             /* Encode dest reg in n_reg */
#define ENCRA1(x)       ((x) << 6)      /* A1 */
#define ENCRA2(x)       ((x) << 12)     /* A2 */
#define ENCRA(x,y)      ((x) << (6+y*6))        /* encode regs in int */
#define GCLASS(x)	(x < 16 ? CLASSA : CLASSB)
int COLORMAP(int c, int *r);
