/*	$OpenBSD: macdefs.h,v 1.2 2007/11/01 10:52:58 otto Exp $	*/
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
#define SZLDOUBLE	96
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

#define CHAR_UNSIGNED
#define TARGET_STDARGS
#define	BOOL_TYPE	INT	/* what used to store _Bool */
#define	WCHAR_TYPE	INT	/* what used to store wchar_t */

#define ELFABI

/*
 * Use large-enough types.
 */
typedef	long long CONSZ;
typedef	unsigned long long U_CONSZ;
typedef long long OFFSZ;

#define CONFMT	"%lld"		/* format for printing constants */
#ifdef ELFABI
#define LABFMT	".L%d"		/* format for printing labels */
#else
#define LABFMT	"L%d"		/* format for printing labels */
#endif
#define	STABLBL	"LL%d"		/* format for stab (debugging) labels */
#define STAB_LINE_ABSOLUTE	/* S_LINE fields use absolute addresses */

#define	MYP2TREE(p) myp2tree(p);

#undef	FIELDOPS		/* no bit-field instructions */
#if 0
#define	RTOLBYTES		/* bytes are numbered right to left */
#endif

#define ENUMSIZE(high,low) INT	/* enums are always stored in full int */

/* Definitions mostly used in pass2 */

#define BYTEOFF(x)	((x)&03)
#define wdal(k)		(BYTEOFF(k)==0)
#define BITOOR(x)	(x)	/* bit offset to oreg offset XXX die! */

#define STOARG(p)
#define STOFARG(p)
#define STOSTARG(p)

#define	szty(t)	(((t) == DOUBLE || (t) == FLOAT || \
	(t) == LONGLONG || (t) == ULONGLONG) ? 2 : (t) == LDOUBLE ? 3 : 1)

/*
 * The PPC register definition are taken from apple docs.
 *
 * The classes used are:
 *	A - general registers
 *	B - 64-bit register pairs
 */

#define R0	0	// scratch register
#define R1	1	// stack base pointer
#define R2	2
#define R3	3	// return register / argument 0
#define R4	4	// return register (for longlong) / argument 1
#define R5	5	// scratch register / argument 2
#define R6	6	// scratch register / argument 3
#define R7	7	// scratch register / argument 4
#define R8	8	// scratch register / argument 5
#define R9	9	// scratch register / argument 6
#define R10	10	// scratch register / argument 7
#define R11	11	// scratch register
#define R12	12	// scratch register
#define R13	13
#define R14	14
#define R15	15
#define R16	16
#define R17	17
#define R18	18
#define R19	19
#define R20	20
#define R21	21
#define R22	22
#define R23	23
#define R24	24
#define R25	25
#define R26	26
#define R27	27
#define R28	28
#define R29	29
#define R30	30
#define R31	31

#define R3R4	32
#define R4R5	33
#define R5R6	34
#define R6R7	35
#define R7R8	36
#define R8R9	37
#define R9R10	38
#define R14R15	39
#define R16R17	40
#define R18R19	41
#define R20R21	42
#define R22R23	43
#define R24R25	44
#define R26R27	45
#define R28R29	46
#define R30R31	47

#define NUMCLASS 4		// XXX must always be 4
#define	MAXREGS	48

#define RSTATUS \
	0, 0, SAREG|TEMPREG, SAREG|TEMPREG,			\
	SAREG|TEMPREG, SAREG|TEMPREG, SAREG|TEMPREG, SAREG|TEMPREG,	\
	SAREG|TEMPREG, SAREG|TEMPREG, SAREG|TEMPREG, SAREG,	\
	SAREG, SAREG, SAREG, SAREG,	\
	SAREG, SAREG, SAREG, SAREG,	\
	SAREG, SAREG, SAREG, SAREG,	\
	SAREG, SAREG, SAREG, SAREG,	\
	SAREG, SAREG, SAREG, SAREG,	\
					\
        SBREG|TEMPREG, SBREG|TEMPREG, SBREG|TEMPREG, SBREG|TEMPREG,	\
        SBREG|TEMPREG, SBREG|TEMPREG, SBREG|TEMPREG,			\
	\
        SBREG, SBREG, SBREG, SBREG,	\
        SBREG, SBREG, SBREG, SBREG,	\
	SBREG, 

#define ROVERLAP \
	{ -1 }, { -1 }, { -1 },			\
	{ R3R4,       -1 }, { R3R4, R4R5, -1 },	\
	{ R4R5, R5R6, -1 }, { R5R6, R6R7, -1 },	\
	{ R6R7, R7R8, -1 }, { R7R8, R8R9, -1 },	\
	{ R8R9, R9R10, -1 }, { R9R10, -1 },	\
	{ -1 }, { -1 }, { -1 },			\
	{ R14R15, -1 }, { R14R15, -1 }, 	\
	{ R16R17, -1 }, { R16R17, -1 },		\
	{ R18R19, -1 }, { R18R19, -1 }, 	\
	{ R20R21, -1 }, { R20R21, -1 },		\
	{ R22R23, -1 }, { R22R23, -1 }, 	\
	{ R24R25, -1 }, { R24R25, -1 },		\
	{ R26R27, -1 }, { R26R27, -1 }, 	\
	{ R28R29, -1 }, { R28R29, -1 },		\
	{ R30R31, -1 }, { R30R31, -1 }, 	\
	\
	{ R3, R4,       R4R5, -1 }, { R4, R5, R3R4, R5R6, -1 },	\
	{ R5, R6, R4R5, R6R7, -1 }, { R6, R7, R5R6, R7R8, -1 },	\
	{ R7, R8, R6R7, R8R9, -1 }, { R8, R9, R7R8, R8R9, -1 }, \
	{ R9, R10, R8R9,      -1 }, 	\
	{ R14, R15, -1 }, { R16, R17, -1 },	\
	{ R18, R19, -1 }, { R20, R21, -1 }, 	\
	{ R22, R23, -1 }, { R24, R25, -1 },	\
	{ R26, R27, -1 }, { R28, R29, -1 }, 	\
	{ R30, R31, -1 },

#if 0
#define BACKAUTO 		/* stack grows negatively for automatics */
#define BACKTEMP 		/* stack grows negatively for temporaries */
#endif

#define ARGINIT		(24*8)	/* # bits above fp where arguments start */
#define AUTOINIT	(56*8)	/* # bits above fp where automatics start */

/* XXX - to die */
#define FPREG   R1     /* frame pointer */
#if 0
#define STKREG  R30    /* stack pointer */
#endif

/* Return a register class based on the type of the node */
#define PCLASS(p) (p->n_type == LONGLONG || p->n_type == ULONGLONG ? SBREG : \
                  (p->n_type >= FLOAT && p->n_type <= LDOUBLE ? SCREG : SAREG))

#define GCLASS(x)	(x < 32 ? CLASSA : CLASSB)
#define DECRA(x,y)      (((x) >> (y*6)) & 63)   /* decode encoded regs */
#define ENCRD(x)        (x)             /* Encode dest reg in n_reg */
#define ENCRA1(x)       ((x) << 6)      /* A1 */
#define ENCRA2(x)       ((x) << 12)     /* A2 */
#define ENCRA(x,y)      ((x) << (6+y*6))        /* encode regs in int */
#define RETREG(x)       ((x) == ULONGLONG || (x) == LONGLONG ? R3R4 : R3)

int COLORMAP(int c, int *r);

#define MYREADER(p) myreader(p)
#define MYCANON(p) mycanon(p)
#define	MYOPTIM

#define	SHSTR		(MAXSPECIAL+1)	/* short struct */
#define	SFUNCALL	(MAXSPECIAL+2)	/* struct assign after function call */
#define SPCON		(MAXSPECIAL+3)  /* positive constant */

struct stub {
	struct { struct stub *q_forw, *q_back; } link;
	char *name;
};

#define FIXEDSTACKSIZE	200 	/* in bytes */
