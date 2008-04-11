/*	$OpenBSD: macdefs.h,v 1.4 2008/04/11 20:45:52 stefan Exp $	*/
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

#define ELFABI

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
#ifdef ELFABI
#define ALLONGLONG	64
#else
#define ALLONGLONG	32
#endif
#define ALSHORT		16
#define ALPOINT		32
#define ALSTRUCT	32
#define ALSTACK		(16*SZCHAR)

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
#define	BOOL_TYPE	INT	/* what used to store _Bool */
#define	WCHAR_TYPE	INT	/* what used to store wchar_t */

/*
 * Use large-enough types.
 */
typedef	long long CONSZ;
typedef	unsigned long long U_CONSZ;
typedef long long OFFSZ;

#define CONFMT	"%lld"		/* format for printing constants */
#if defined(ELFABI)
#define LABFMT	".L%d"		/* format for printing labels */
#define REGPREFIX	"%"	/* format for printing registers */
#elif defined(MACHOABI)
#define LABFMT	"L%d"		/* format for printing labels */
#define REGPREFIX
#else
#error undefined ABI
#endif
#define	STABLBL	"LL%d"		/* format for stab (debugging) labels */

#ifdef MACHOABI
#define STAB_LINE_ABSOLUTE	/* S_LINE fields use absolute addresses */
#endif

#undef	FIELDOPS		/* no bit-field instructions */

#define ENUMSIZE(high,low) INT	/* enums are always stored in full int */

/* Definitions mostly used in pass2 */

#define BYTEOFF(x)	((x)&03)
#define BITOOR(x)	(x)	/* bit offset to oreg offset XXX die! */

#define	szty(t)	(((t) == DOUBLE || (t) == LDOUBLE || \
	DEUNSIGN(t) == LONGLONG) ? 2 : 1)

/*
 * The PPC register definition are taken from apple docs.
 *
 * The classes used are:
 *	A - general registers
 *	B - 64-bit register pairs
 *	C - floating-point registers
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

#define F0	48	// scratch register
#define F1	49	// return value 0 / argument 0
#define F2	50	// return value 1 / argument 1
#define F3	51	// return value 2 / argument 2
#define F4	52	// return value 3 / argument 3
#define F5	53	// argument 4
#define F6	54	// argument 5
#define F7	55	// argument 6
#define F8	56	// argument 7
#define F9	57	// argument 8
#define F10	58	// argument 9
#define F11	59	// argument 10
#define F12	60	// argument 11
#define F13	61	// argument 12
#define F14	62
#define F15	63
#define F16	64
#define F17	65
#define F18	66
#define F19	67
#define F20	68
#define F21	69
#define F22	70
#define F23	71
#define F24	72
#define F25	73
#define F26	74
#define F27	75
#define F28	76
#define F29	77
#define F30	78
#define F31	79

#define NUMCLASS 3
#define	MAXREGS	64		// XXX cannot have more than 64

#define RSTATUS 				\
	0,			/* R0 */	\
	0,			/* R1 */	\
	SAREG|TEMPREG,		/* R2 */	\
	SAREG|TEMPREG,		/* R3 */	\
	SAREG|TEMPREG,		/* R4 */	\
	SAREG|TEMPREG,		/* R5 */	\
	SAREG|TEMPREG,		/* R6 */	\
	SAREG|TEMPREG,		/* R7 */	\
	SAREG|TEMPREG,		/* R8 */	\
	SAREG|TEMPREG,		/* R9 */	\
	SAREG|TEMPREG,		/* R10 */	\
	SAREG|TEMPREG,		/* R11 */	\
	SAREG|TEMPREG,		/* R12 */	\
	SAREG,			/* R13 */	\
	SAREG,			/* R14 */	\
	SAREG,			/* R15 */	\
	SAREG,			/* R16 */	\
	SAREG,			/* R17 */	\
	SAREG,			/* R18 */	\
	SAREG,			/* R19 */	\
	SAREG,			/* R20 */	\
	SAREG,			/* R21 */	\
	SAREG,			/* R22 */	\
	SAREG,			/* R23 */	\
	SAREG,			/* R24 */	\
	SAREG,			/* R25 */	\
	SAREG,			/* R26 */	\
	SAREG,			/* R27 */	\
	SAREG,			/* R28 */	\
	SAREG,			/* R29 */	\
	SAREG,			/* R30 */	\
	SAREG,			/* R31 */	\
	\
        SBREG|TEMPREG,		/* R3R4 */	\
	SBREG|TEMPREG,		/* R4R5 */	\
	SBREG|TEMPREG,		/* R5R6 */	\
	SBREG|TEMPREG,		/* R6R7 */	\
        SBREG|TEMPREG,		/* R7R8 */	\
	SBREG|TEMPREG,		/* R8R9 */	\
	SBREG|TEMPREG,		/* R9R10 */	\
	\
	SBREG,			/* R14R15 */	\
	SBREG,			/* R16R17 */	\
	SBREG,			/* R18R19 */	\
	SBREG,			/* R20R21 */	\
	SBREG,			/* R22R23 */	\
	SBREG,			/* R24R25 */	\
	SBREG,			/* R26R2k */	\
	SBREG,			/* R28R29 */	\
	SBREG, 			/* R30R31 */	\
	\
	SCREG|TEMPREG,		/* F0 */	\
	SCREG|TEMPREG,		/* F1 */	\
	SCREG|TEMPREG,		/* F2 */	\
	SCREG|TEMPREG,		/* F3 */	\
	SCREG|TEMPREG,		/* F4 */	\
	SCREG|TEMPREG,		/* F5 */	\
	SCREG|TEMPREG,		/* F6 */	\
	SCREG|TEMPREG,		/* F7 */	\
	SCREG|TEMPREG,		/* F8 */	\
	SCREG|TEMPREG,		/* F9 */	\
	SCREG|TEMPREG,		/* F10 */	\
	SCREG|TEMPREG,		/* F11 */	\
	SCREG|TEMPREG,		/* F12 */	\
	SCREG|TEMPREG,		/* F13 */	\
	SCREG,			/* F14 */	\
	SCREG,			/* F15 */	\

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
	{ R30, R31, -1 },		\
	\
	{ -1 }, { -1 }, { -1 }, { -1 },		\
	{ -1 }, { -1 }, { -1 }, { -1 },		\
	{ -1 }, { -1 }, { -1 }, { -1 },		\
	{ -1 }, { -1 }, { -1 }, { -1 },		\

/*
 * According to the ABI documents, there isn't really a frame pointer;
 * all references to data on the stack (autos and parameters) are
 * indexed relative to the stack pointer.  However, pcc isn't really
 * capable of running in this manner, and expects a frame pointer.
 */
#define SPREG   R1	/* stack pointer */
#define FPREG   R30	/* frame pointer */
#define GOTREG	R31	/* global offset table (PIC) */

#ifdef FPREG
#define ARGINIT		(24*8)	/* # bits above fp where arguments start */
#define AUTOINIT	(8*8)	/* # bits above fp where automatics start */
#define BACKAUTO 		/* stack grows negatively for automatics */
#define BACKTEMP 		/* stack grows negatively for temporaries */
#else
#define ARGINIT		(24*8)	/* # bits above fp where arguments start */
#define AUTOINIT	(56*8)	/* # bits above fp where automatics start */
#endif

/* Return a register class based on the type of the node */
#define PCLASS(p)	(1 << gclass((p)->n_type))

#define GCLASS(x)	((x) < 32 ? CLASSA : ((x) < 48 ? CLASSB : CLASSC))
#define DECRA(x,y)	(((x) >> (y*6)) & 63)   /* decode encoded regs */
#define ENCRA(x,y)	((x) << (6+y*6))        /* encode regs in int */
#define ENCRD(x)	(x)		/* Encode dest reg in n_reg */
#define RETREG(x)	retreg(x)

int COLORMAP(int c, int *r);
int retreg(int ty);

#define	SHSTR		(MAXSPECIAL+1)	/* short struct */
#define	SFUNCALL	(MAXSPECIAL+2)	/* struct assign after function call */
#define SPCON		(MAXSPECIAL+3)  /* positive constant */

int features(int f);
#define FEATURE_BIGENDIAN	0x00010000
#define FEATURE_PIC		0x00020000
#define FEATURE_HARDFLOAT	0x00040000

struct stub {
	struct { struct stub *q_forw, *q_back; } link;
	char *name;
};
extern struct stub stublist;
extern struct stub nlplist;
void addstub(struct stub *list, char *name);

#define TARGET_STDARGS
#define TARGET_BUILTINS							\
	{ "__builtin_stdarg_start", powerpc_builtin_stdarg_start },	\
	{ "__builtin_va_arg", powerpc_builtin_va_arg },			\
	{ "__builtin_va_end", powerpc_builtin_va_end },			\
	{ "__builtin_va_copy", powerpc_builtin_va_copy },		\
	{ "__builtin_return_address", powerpc_builtin_return_address },

#define NODE struct node
struct node;
NODE *powerpc_builtin_stdarg_start(NODE *f, NODE *a);
NODE *powerpc_builtin_va_arg(NODE *f, NODE *a);
NODE *powerpc_builtin_va_end(NODE *f, NODE *a);
NODE *powerpc_builtin_va_copy(NODE *f, NODE *a);
NODE *powerpc_builtin_return_address(NODE *f, NODE *a);
#undef NODE

#define NARGREGS	8

#ifdef ELFABI
#define COM     "       # "
#else
#define COM     "       ; "
#endif
