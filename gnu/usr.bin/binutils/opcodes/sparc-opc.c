/* Table of opcodes for the sparc.
   Copyright 1989, 1991, 1992, 1995 Free Software Foundation, Inc.

This file is part of the BFD library.

BFD is free software; you can redistribute it and/or modify it under
the terms of the GNU General Public License as published by the Free
Software Foundation; either version 2, or (at your option) any later
version.

BFD is distributed in the hope that it will be useful, but WITHOUT ANY
WARRANTY; without even the implied warranty of MERCHANTABILITY or
FITNESS FOR A PARTICULAR PURPOSE. See the GNU General Public License
for more details.

You should have received a copy of the GNU General Public License
along with this software; see the file COPYING.  If not, write to
the Free Software Foundation, 59 Temple Place - Suite 330,
Boston, MA 02111-1307, USA.	*/

/* FIXME-someday: perhaps the ,a's and such should be embedded in the
   instruction's name rather than the args.  This would make gas faster, pinsn
   slower, but would mess up some macros a bit.  xoxorich. */

/* v9 FIXME: Doesn't accept `setsw', `setx' synthetic instructions for v9.  */

#include <stdio.h>
#include "ansidecl.h"
#include "opcode/sparc.h"

const char *architecture_pname[] = {
	"v6",
	"v7",
	"v8",
	"sparclite",
	"v9",
	NULL,
};

/* Branch condition field.  */
#define COND(x)		(((x)&0xf)<<25)

/* v9: Move (MOVcc and FMOVcc) condition field.  */
#define MCOND(x,i_or_f)	((((i_or_f)&1)<<18)|(((x)>>11)&(0xf<<14))) /* v9 */

/* v9: Move register (MOVRcc and FMOVRcc) condition field.  */
#define RCOND(x)	(((x)&0x7)<<10)	/* v9 */

#define CONDA	(COND(0x8))
#define CONDCC	(COND(0xd))
#define CONDCS	(COND(0x5))
#define CONDE	(COND(0x1))
#define CONDG	(COND(0xa))
#define CONDGE	(COND(0xb))
#define CONDGU	(COND(0xc))
#define CONDL	(COND(0x3))
#define CONDLE	(COND(0x2))
#define CONDLEU	(COND(0x4))
#define CONDN	(COND(0x0))
#define CONDNE	(COND(0x9))
#define CONDNEG	(COND(0x6))
#define CONDPOS	(COND(0xe))
#define CONDVC	(COND(0xf))
#define CONDVS	(COND(0x7))

#define CONDNZ	CONDNE
#define CONDZ	CONDE
#define CONDGEU	CONDCC
#define CONDLU	CONDCS

#define FCONDA		(COND(0x8))
#define FCONDE		(COND(0x9))
#define FCONDG		(COND(0x6))
#define FCONDGE		(COND(0xb))
#define FCONDL		(COND(0x4))
#define FCONDLE		(COND(0xd))
#define FCONDLG		(COND(0x2))
#define FCONDN		(COND(0x0))
#define FCONDNE		(COND(0x1))
#define FCONDO		(COND(0xf))
#define FCONDU		(COND(0x7))
#define FCONDUE		(COND(0xa))
#define FCONDUG		(COND(0x5))
#define FCONDUGE	(COND(0xc))
#define FCONDUL		(COND(0x3))
#define FCONDULE	(COND(0xe))

#define FCONDNZ	FCONDNE
#define FCONDZ	FCONDE

#define ICC (0)	/* v9 */
#define XCC (1<<12) /* v9 */
#define FCC(x)	(((x)&0x3)<<11) /* v9 */
#define FBFCC(x)	(((x)&0x3)<<20)	/* v9 */

/* The order of the opcodes in the table is significant:
	
	* The assembler requires that all instances of the same mnemonic must
	be consecutive.	If they aren't, the assembler will bomb at runtime.

	* The disassembler should not care about the order of the opcodes.

*/

struct sparc_opcode sparc_opcodes[] = {

{ "ld",	F3(3, 0x00, 0), F3(~3, ~0x00, ~0),		"[1+2],d", 0, v6 },
{ "ld",	F3(3, 0x00, 0), F3(~3, ~0x00, ~0)|RS2_G0,	"[1],d", 0, v6 }, /* ld [rs1+%g0],d */
{ "ld",	F3(3, 0x00, 1), F3(~3, ~0x00, ~1),		"[1+i],d", 0, v6 },
{ "ld",	F3(3, 0x00, 1), F3(~3, ~0x00, ~1),		"[i+1],d", 0, v6 },
{ "ld",	F3(3, 0x00, 1), F3(~3, ~0x00, ~1)|RS1_G0,	"[i],d", 0, v6 },
{ "ld",	F3(3, 0x00, 1), F3(~3, ~0x00, ~1)|SIMM13(~0),	"[1],d", 0, v6 }, /* ld [rs1+0],d */
{ "ld",	F3(3, 0x20, 0), F3(~3, ~0x20, ~0),		"[1+2],g", 0, v6 },
{ "ld",	F3(3, 0x20, 0), F3(~3, ~0x20, ~0)|RS2_G0,	"[1],g", 0, v6 }, /* ld [rs1+%g0],d */
{ "ld",	F3(3, 0x20, 1), F3(~3, ~0x20, ~1),		"[1+i],g", 0, v6 },
{ "ld",	F3(3, 0x20, 1), F3(~3, ~0x20, ~1),		"[i+1],g", 0, v6 },
{ "ld",	F3(3, 0x20, 1), F3(~3, ~0x20, ~1)|RS1_G0,	"[i],g", 0, v6 },
{ "ld",	F3(3, 0x20, 1), F3(~3, ~0x20, ~1)|SIMM13(~0),	"[1],g", 0, v6 }, /* ld [rs1+0],d */

{ "ld",	F3(3, 0x21, 0), F3(~3, ~0x21, ~0)|RD(~0),	"[1+2],F", 0, v6 },
{ "ld",	F3(3, 0x21, 0), F3(~3, ~0x21, ~0)|RS2_G0|RD(~0),"[1],F", 0, v6 }, /* ld [rs1+%g0],d */
{ "ld",	F3(3, 0x21, 1), F3(~3, ~0x21, ~1)|RD(~0),	"[1+i],F", 0, v6 },
{ "ld",	F3(3, 0x21, 1), F3(~3, ~0x21, ~1)|RD(~0),	"[i+1],F", 0, v6 },
{ "ld",	F3(3, 0x21, 1), F3(~3, ~0x21, ~1)|RS1_G0|RD(~0),"[i],F", 0, v6 },
{ "ld",	F3(3, 0x21, 1), F3(~3, ~0x21, ~1)|SIMM13(~0)|RD(~0),"[1],F", 0, v6 }, /* ld [rs1+0],d */

{ "ld",	F3(3, 0x30, 0), F3(~3, ~0x30, ~0),		"[1+2],D", F_NOTV9, v6 },
{ "ld",	F3(3, 0x30, 0), F3(~3, ~0x30, ~0)|RS2_G0,	"[1],D", F_NOTV9, v6 }, /* ld [rs1+%g0],d */
{ "ld",	F3(3, 0x30, 1), F3(~3, ~0x30, ~1),		"[1+i],D", F_NOTV9, v6 },
{ "ld",	F3(3, 0x30, 1), F3(~3, ~0x30, ~1),		"[i+1],D", F_NOTV9, v6 },
{ "ld",	F3(3, 0x30, 1), F3(~3, ~0x30, ~1)|RS1_G0,	"[i],D", F_NOTV9, v6 },
{ "ld",	F3(3, 0x30, 1), F3(~3, ~0x30, ~1)|SIMM13(~0),	"[1],D", F_NOTV9, v6 }, /* ld [rs1+0],d */
{ "ld",	F3(3, 0x31, 0), F3(~3, ~0x31, ~0),		"[1+2],C", F_NOTV9, v6 },
{ "ld",	F3(3, 0x31, 0), F3(~3, ~0x31, ~0)|RS2_G0,	"[1],C", F_NOTV9, v6 }, /* ld [rs1+%g0],d */
{ "ld",	F3(3, 0x31, 1), F3(~3, ~0x31, ~1),		"[1+i],C", F_NOTV9, v6 },
{ "ld",	F3(3, 0x31, 1), F3(~3, ~0x31, ~1),		"[i+1],C", F_NOTV9, v6 },
{ "ld",	F3(3, 0x31, 1), F3(~3, ~0x31, ~1)|RS1_G0,	"[i],C", F_NOTV9, v6 },
{ "ld",	F3(3, 0x31, 1), F3(~3, ~0x31, ~1)|SIMM13(~0),	"[1],C", F_NOTV9, v6 }, /* ld [rs1+0],d */

/* The v9 LDUW is the same as the old 'ld' opcode, it is not the same as the
   'ld' pseudo-op in v9.  */
{ "lduw",	F3(3, 0x00, 0), F3(~3, ~0x00, ~0),		"[1+2],d", F_ALIAS, v9 },
{ "lduw",	F3(3, 0x00, 0), F3(~3, ~0x00, ~0)|RS2_G0,	"[1],d", F_ALIAS, v9 }, /* ld [rs1+%g0],d */
{ "lduw",	F3(3, 0x00, 1), F3(~3, ~0x00, ~1),		"[1+i],d", F_ALIAS, v9 },
{ "lduw",	F3(3, 0x00, 1), F3(~3, ~0x00, ~1),		"[i+1],d", F_ALIAS, v9 },
{ "lduw",	F3(3, 0x00, 1), F3(~3, ~0x00, ~1)|RS1_G0,	"[i],d", F_ALIAS, v9 },
{ "lduw",	F3(3, 0x00, 1), F3(~3, ~0x00, ~1)|SIMM13(~0),	"[1],d", F_ALIAS, v9 }, /* ld [rs1+0],d */

{ "ldd",	F3(3, 0x03, 0), F3(~3, ~0x03, ~0)|ASI(~0),	"[1+2],d", 0, v6 },
{ "ldd",	F3(3, 0x03, 0), F3(~3, ~0x03, ~0)|ASI_RS2(~0),	"[1],d", 0, v6 }, /* ldd [rs1+%g0],d */
{ "ldd",	F3(3, 0x03, 1), F3(~3, ~0x03, ~1),		"[1+i],d", 0, v6 },
{ "ldd",	F3(3, 0x03, 1), F3(~3, ~0x03, ~1),		"[i+1],d", 0, v6 },
{ "ldd",	F3(3, 0x03, 1), F3(~3, ~0x03, ~1)|RS1_G0,	"[i],d", 0, v6 },
{ "ldd",	F3(3, 0x03, 1), F3(~3, ~0x03, ~1)|SIMM13(~0),	"[1],d", 0, v6 }, /* ldd [rs1+0],d */
{ "ldd",	F3(3, 0x23, 0), F3(~3, ~0x23, ~0)|ASI(~0),	"[1+2],H", 0, v6 },
{ "ldd",	F3(3, 0x23, 0), F3(~3, ~0x23, ~0)|ASI_RS2(~0),	"[1],H", 0, v6 }, /* ldd [rs1+%g0],d */
{ "ldd",	F3(3, 0x23, 1), F3(~3, ~0x23, ~1),		"[1+i],H", 0, v6 },
{ "ldd",	F3(3, 0x23, 1), F3(~3, ~0x23, ~1),		"[i+1],H", 0, v6 },
{ "ldd",	F3(3, 0x23, 1), F3(~3, ~0x23, ~1)|RS1_G0,	"[i],H", 0, v6 },
{ "ldd",	F3(3, 0x23, 1), F3(~3, ~0x23, ~1)|SIMM13(~0),	"[1],H", 0, v6 }, /* ldd [rs1+0],d */

{ "ldd",	F3(3, 0x33, 0), F3(~3, ~0x33, ~0)|ASI(~0),	"[1+2],D", F_NOTV9, v6 },
{ "ldd",	F3(3, 0x33, 0), F3(~3, ~0x33, ~0)|ASI_RS2(~0),	"[1],D", F_NOTV9, v6 }, /* ldd [rs1+%g0],d */
{ "ldd",	F3(3, 0x33, 1), F3(~3, ~0x33, ~1),		"[1+i],D", F_NOTV9, v6 },
{ "ldd",	F3(3, 0x33, 1), F3(~3, ~0x33, ~1),		"[i+1],D", F_NOTV9, v6 },
{ "ldd",	F3(3, 0x33, 1), F3(~3, ~0x33, ~1)|RS1_G0,	"[i],D", F_NOTV9, v6 },
{ "ldd",	F3(3, 0x33, 1), F3(~3, ~0x33, ~1)|SIMM13(~0),	"[1],D", F_NOTV9, v6 }, /* ldd [rs1+0],d */

{ "ldq",	F3(3, 0x22, 0), F3(~3, ~0x22, ~0)|ASI(~0),	"[1+2],J", 0, v9 },
{ "ldq",	F3(3, 0x22, 0), F3(~3, ~0x22, ~0)|ASI_RS2(~0),	"[1],J", 0, v9 }, /* ldd [rs1+%g0],d */
{ "ldq",	F3(3, 0x22, 1), F3(~3, ~0x22, ~1),		"[1+i],J", 0, v9 },
{ "ldq",	F3(3, 0x22, 1), F3(~3, ~0x22, ~1),		"[i+1],J", 0, v9 },
{ "ldq",	F3(3, 0x22, 1), F3(~3, ~0x22, ~1)|RS1_G0,	"[i],J", 0, v9 },
{ "ldq",	F3(3, 0x22, 1), F3(~3, ~0x22, ~1)|SIMM13(~0),	"[1],J", 0, v9 }, /* ldd [rs1+0],d */

{ "ldsb",	F3(3, 0x09, 0), F3(~3, ~0x09, ~0)|ASI(~0),	"[1+2],d", 0, v6 },
{ "ldsb",	F3(3, 0x09, 0), F3(~3, ~0x09, ~0)|ASI_RS2(~0),	"[1],d", 0, v6 }, /* ldsb [rs1+%g0],d */
{ "ldsb",	F3(3, 0x09, 1), F3(~3, ~0x09, ~1),		"[1+i],d", 0, v6 },
{ "ldsb",	F3(3, 0x09, 1), F3(~3, ~0x09, ~1),		"[i+1],d", 0, v6 },
{ "ldsb",	F3(3, 0x09, 1), F3(~3, ~0x09, ~1)|RS1_G0,	"[i],d", 0, v6 },
{ "ldsb",	F3(3, 0x09, 1), F3(~3, ~0x09, ~1)|SIMM13(~0),	"[1],d", 0, v6 }, /* ldsb [rs1+0],d */

{ "ldsh",	F3(3, 0x0a, 0), F3(~3, ~0x0a, ~0)|ASI_RS2(~0),	"[1],d", 0, v6 }, /* ldsh [rs1+%g0],d */
{ "ldsh",	F3(3, 0x0a, 0), F3(~3, ~0x0a, ~0)|ASI(~0),	"[1+2],d", 0, v6 },
{ "ldsh",	F3(3, 0x0a, 1), F3(~3, ~0x0a, ~1),		"[1+i],d", 0, v6 },
{ "ldsh",	F3(3, 0x0a, 1), F3(~3, ~0x0a, ~1),		"[i+1],d", 0, v6 },
{ "ldsh",	F3(3, 0x0a, 1), F3(~3, ~0x0a, ~1)|RS1_G0,	"[i],d", 0, v6 },
{ "ldsh",	F3(3, 0x0a, 1), F3(~3, ~0x0a, ~1)|SIMM13(~0),	"[1],d", 0, v6 }, /* ldsh [rs1+0],d */

{ "ldstub",	F3(3, 0x0d, 0), F3(~3, ~0x0d, ~0)|ASI(~0),	"[1+2],d", 0, v6 },
{ "ldstub",	F3(3, 0x0d, 0), F3(~3, ~0x0d, ~0)|ASI_RS2(~0),	"[1],d", 0, v6 }, /* ldstub [rs1+%g0],d */
{ "ldstub",	F3(3, 0x0d, 1), F3(~3, ~0x0d, ~1),		"[1+i],d", 0, v6 },
{ "ldstub",	F3(3, 0x0d, 1), F3(~3, ~0x0d, ~1),		"[i+1],d", 0, v6 },
{ "ldstub",	F3(3, 0x0d, 1), F3(~3, ~0x0d, ~1)|RS1_G0,	"[i],d", 0, v6 },
{ "ldstub",	F3(3, 0x0d, 1), F3(~3, ~0x0d, ~1)|SIMM13(~0),	"[1],d", 0, v6 }, /* ldstub [rs1+0],d */

{ "ldsw",	F3(3, 0x08, 0), F3(~3, ~0x08, ~0)|ASI(~0),	"[1+2],d", 0, v9 },
{ "ldsw",	F3(3, 0x08, 0), F3(~3, ~0x08, ~0)|ASI_RS2(~0),	"[1],d", 0, v9 }, /* ldsw [rs1+%g0],d */
{ "ldsw",	F3(3, 0x08, 1), F3(~3, ~0x08, ~1),		"[1+i],d", 0, v9 },
{ "ldsw",	F3(3, 0x08, 1), F3(~3, ~0x08, ~1),		"[i+1],d", 0, v9 },
{ "ldsw",	F3(3, 0x08, 1), F3(~3, ~0x08, ~1)|RS1_G0,	"[i],d", 0, v9 },
{ "ldsw",	F3(3, 0x08, 1), F3(~3, ~0x08, ~1)|SIMM13(~0),	"[1],d", 0, v9 }, /* ldsw [rs1+0],d */

{ "ldub",	F3(3, 0x01, 0), F3(~3, ~0x01, ~0)|ASI(~0),	"[1+2],d", 0, v6 },
{ "ldub",	F3(3, 0x01, 0), F3(~3, ~0x01, ~0)|ASI_RS2(~0),	"[1],d", 0, v6 }, /* ldub [rs1+%g0],d */
{ "ldub",	F3(3, 0x01, 1), F3(~3, ~0x01, ~1),		"[1+i],d", 0, v6 },
{ "ldub",	F3(3, 0x01, 1), F3(~3, ~0x01, ~1),		"[i+1],d", 0, v6 },
{ "ldub",	F3(3, 0x01, 1), F3(~3, ~0x01, ~1)|RS1_G0,	"[i],d", 0, v6 },
{ "ldub",	F3(3, 0x01, 1), F3(~3, ~0x01, ~1)|SIMM13(~0),	"[1],d", 0, v6 }, /* ldub [rs1+0],d */

{ "lduh",	F3(3, 0x02, 0), F3(~3, ~0x02, ~0)|ASI(~0),	"[1+2],d", 0, v6 },
{ "lduh",	F3(3, 0x02, 0), F3(~3, ~0x02, ~0)|ASI_RS2(~0),	"[1],d", 0, v6 }, /* lduh [rs1+%g0],d */
{ "lduh",	F3(3, 0x02, 1), F3(~3, ~0x02, ~1),		"[1+i],d", 0, v6 },
{ "lduh",	F3(3, 0x02, 1), F3(~3, ~0x02, ~1),		"[i+1],d", 0, v6 },
{ "lduh",	F3(3, 0x02, 1), F3(~3, ~0x02, ~1)|RS1_G0,	"[i],d", 0, v6 },
{ "lduh",	F3(3, 0x02, 1), F3(~3, ~0x02, ~1)|SIMM13(~0),	"[1],d", 0, v6 }, /* lduh [rs1+0],d */

{ "ldx",	F3(3, 0x0b, 0), F3(~3, ~0x0b, ~0)|ASI(~0),	"[1+2],d", 0, v9 },
{ "ldx",	F3(3, 0x0b, 0), F3(~3, ~0x0b, ~0)|ASI_RS2(~0),	"[1],d", 0, v9 }, /* ldx [rs1+%g0],d */
{ "ldx",	F3(3, 0x0b, 1), F3(~3, ~0x0b, ~1),		"[1+i],d", 0, v9 },
{ "ldx",	F3(3, 0x0b, 1), F3(~3, ~0x0b, ~1),		"[i+1],d", 0, v9 },
{ "ldx",	F3(3, 0x0b, 1), F3(~3, ~0x0b, ~1)|RS1_G0,	"[i],d", 0, v9 },
{ "ldx",	F3(3, 0x0b, 1), F3(~3, ~0x0b, ~1)|SIMM13(~0),	"[1],d", 0, v9 }, /* ldx [rs1+0],d */

{ "ldx",	F3(3, 0x21, 0)|RD(1), F3(~3, ~0x21, ~0)|RD(~1),	"[1+2],F", 0, v9 },
{ "ldx",	F3(3, 0x21, 0)|RD(1), F3(~3, ~0x21, ~0)|RS2_G0|RD(~1),	"[1],F", 0, v9 }, /* ld [rs1+%g0],d */
{ "ldx",	F3(3, 0x21, 1)|RD(1), F3(~3, ~0x21, ~1)|RD(~1),	"[1+i],F", 0, v9 },
{ "ldx",	F3(3, 0x21, 1)|RD(1), F3(~3, ~0x21, ~1)|RD(~1),	"[i+1],F", 0, v9 },
{ "ldx",	F3(3, 0x21, 1)|RD(1), F3(~3, ~0x21, ~1)|RS1_G0|RD(~1),	"[i],F", 0, v9 },
{ "ldx",	F3(3, 0x21, 1)|RD(1), F3(~3, ~0x21, ~1)|SIMM13(~0)|RD(~1),"[1],F", 0, v9 }, /* ld [rs1+0],d */

{ "lda",	F3(3, 0x10, 0), F3(~3, ~0x10, ~0),		"[1+2]A,d", 0, v6 },
{ "lda",	F3(3, 0x10, 0), F3(~3, ~0x10, ~0)|RS2_G0,	"[1]A,d", 0, v6 }, /* lda [rs1+%g0],d */
{ "lda",	F3(3, 0x10, 1), F3(~3, ~0x10, ~1),		"[1+i]o,d", 0, v9 },
{ "lda",	F3(3, 0x10, 1), F3(~3, ~0x10, ~1),		"[i+1]o,d", 0, v9 },
{ "lda",	F3(3, 0x10, 1), F3(~3, ~0x10, ~1)|RS1_G0,	"[i]o,d", 0, v9 },
{ "lda",	F3(3, 0x10, 1), F3(~3, ~0x10, ~1)|SIMM13(~0),	"[1]o,d", 0, v9 }, /* ld [rs1+0],d */
{ "lda",	F3(3, 0x30, 0), F3(~3, ~0x30, ~0),		"[1+2]A,g", 0, v9 },
{ "lda",	F3(3, 0x30, 0), F3(~3, ~0x30, ~0)|RS2_G0,	"[1]A,g", 0, v9 }, /* lda [rs1+%g0],d */
{ "lda",	F3(3, 0x30, 1), F3(~3, ~0x30, ~1),		"[1+i]o,g", 0, v9 },
{ "lda",	F3(3, 0x30, 1), F3(~3, ~0x30, ~1),		"[i+1]o,g", 0, v9 },
{ "lda",	F3(3, 0x30, 1), F3(~3, ~0x30, ~1)|RS1_G0,	"[i]o,g", 0, v9 },
{ "lda",	F3(3, 0x30, 1), F3(~3, ~0x30, ~1)|SIMM13(~0),	"[1]o,g", 0, v9 }, /* ld [rs1+0],d */

{ "ldda",	F3(3, 0x13, 0), F3(~3, ~0x13, ~0),		"[1+2]A,d", 0, v6 },
{ "ldda",	F3(3, 0x13, 0), F3(~3, ~0x13, ~0)|RS2_G0,	"[1]A,d", 0, v6 }, /* ldda [rs1+%g0],d */
{ "ldda",	F3(3, 0x13, 1), F3(~3, ~0x13, ~1),		"[1+i]o,d", 0, v9 },
{ "ldda",	F3(3, 0x13, 1), F3(~3, ~0x13, ~1),		"[i+1]o,d", 0, v9 },
{ "ldda",	F3(3, 0x13, 1), F3(~3, ~0x13, ~1)|RS1_G0,	"[i]o,d", 0, v9 },
{ "ldda",	F3(3, 0x13, 1), F3(~3, ~0x13, ~1)|SIMM13(~0),	"[1]o,d", 0, v9 }, /* ld [rs1+0],d */

{ "ldda",	F3(3, 0x33, 0), F3(~3, ~0x33, ~0),		"[1+2]A,H", 0, v9 },
{ "ldda",	F3(3, 0x33, 0), F3(~3, ~0x33, ~0)|RS2_G0,	"[1]A,H", 0, v9 }, /* ldda [rs1+%g0],d */
{ "ldda",	F3(3, 0x33, 1), F3(~3, ~0x33, ~1),		"[1+i]o,H", 0, v9 },
{ "ldda",	F3(3, 0x33, 1), F3(~3, ~0x33, ~1),		"[i+1]o,H", 0, v9 },
{ "ldda",	F3(3, 0x33, 1), F3(~3, ~0x33, ~1)|RS1_G0,	"[i]o,H", 0, v9 },
{ "ldda",	F3(3, 0x33, 1), F3(~3, ~0x33, ~1)|SIMM13(~0),	"[1]o,H", 0, v9 }, /* ld [rs1+0],d */

{ "ldqa",	F3(3, 0x32, 0), F3(~3, ~0x32, ~0),		"[1+2]A,J", 0, v9 },
{ "ldqa",	F3(3, 0x32, 0), F3(~3, ~0x32, ~0)|RS2_G0,	"[1]A,J", 0, v9 }, /* ldd [rs1+%g0],d */
{ "ldqa",	F3(3, 0x32, 1), F3(~3, ~0x32, ~1),		"[1+i]o,J", 0, v9 },
{ "ldqa",	F3(3, 0x32, 1), F3(~3, ~0x32, ~1),		"[i+1]o,J", 0, v9 },
{ "ldqa",	F3(3, 0x32, 1), F3(~3, ~0x32, ~1)|RS1_G0,	"[i]o,J", 0, v9 },
{ "ldqa",	F3(3, 0x32, 1), F3(~3, ~0x32, ~1)|SIMM13(~0),	"[1]o,J", 0, v9 }, /* ldd [rs1+0],d */

{ "ldsba",	F3(3, 0x19, 0), F3(~3, ~0x19, ~0),		"[1+2]A,d", 0, v6 },
{ "ldsba",	F3(3, 0x19, 0), F3(~3, ~0x19, ~0)|RS2_G0,	"[1]A,d", 0, v6 }, /* ldsba [rs1+%g0],d */
{ "ldsba",	F3(3, 0x19, 1), F3(~3, ~0x19, ~1),		"[1+i]o,d", 0, v9 },
{ "ldsba",	F3(3, 0x19, 1), F3(~3, ~0x19, ~1),		"[i+1]o,d", 0, v9 },
{ "ldsba",	F3(3, 0x19, 1), F3(~3, ~0x19, ~1)|RS1_G0,	"[i]o,d", 0, v9 },
{ "ldsba",	F3(3, 0x19, 1), F3(~3, ~0x19, ~1)|SIMM13(~0),	"[1]o,d", 0, v9 }, /* ld [rs1+0],d */

{ "ldsha",	F3(3, 0x1a, 0), F3(~3, ~0x1a, ~0),		"[1+2]A,d", 0, v6 },
{ "ldsha",	F3(3, 0x1a, 0), F3(~3, ~0x1a, ~0)|RS2_G0,	"[1]A,d", 0, v6 }, /* ldsha [rs1+%g0],d */
{ "ldsha",	F3(3, 0x1a, 1), F3(~3, ~0x1a, ~1),		"[1+i]o,d", 0, v9 },
{ "ldsha",	F3(3, 0x1a, 1), F3(~3, ~0x1a, ~1),		"[i+1]o,d", 0, v9 },
{ "ldsha",	F3(3, 0x1a, 1), F3(~3, ~0x1a, ~1)|RS1_G0,	"[i]o,d", 0, v9 },
{ "ldsha",	F3(3, 0x1a, 1), F3(~3, ~0x1a, ~1)|SIMM13(~0),	"[1]o,d", 0, v9 }, /* ld [rs1+0],d */

{ "ldstuba",	F3(3, 0x1d, 0), F3(~3, ~0x1d, ~0),		"[1+2]A,d", 0, v6 },
{ "ldstuba",	F3(3, 0x1d, 0), F3(~3, ~0x1d, ~0)|RS2_G0,	"[1]A,d", 0, v6 }, /* ldstuba [rs1+%g0],d */
{ "ldstuba",	F3(3, 0x1d, 1), F3(~3, ~0x1d, ~1),		"[1+i]o,d", 0, v9 },
{ "ldstuba",	F3(3, 0x1d, 1), F3(~3, ~0x1d, ~1),		"[i+1]o,d", 0, v9 },
{ "ldstuba",	F3(3, 0x1d, 1), F3(~3, ~0x1d, ~1)|RS1_G0,	"[i]o,d", 0, v9 },
{ "ldstuba",	F3(3, 0x1d, 1), F3(~3, ~0x1d, ~1)|SIMM13(~0),	"[1]o,d", 0, v9 }, /* ld [rs1+0],d */

{ "ldswa",	F3(3, 0x18, 0), F3(~3, ~0x18, ~0),		"[1+2]A,d", 0, v9 },
{ "ldswa",	F3(3, 0x18, 0), F3(~3, ~0x18, ~0)|RS2_G0,	"[1]A,d", 0, v9 }, /* lda [rs1+%g0],d */
{ "ldswa",	F3(3, 0x18, 1), F3(~3, ~0x18, ~1),		"[1+i]o,d", 0, v9 },
{ "ldswa",	F3(3, 0x18, 1), F3(~3, ~0x18, ~1),		"[i+1]o,d", 0, v9 },
{ "ldswa",	F3(3, 0x18, 1), F3(~3, ~0x18, ~1)|RS1_G0,	"[i]o,d", 0, v9 },
{ "ldswa",	F3(3, 0x18, 1), F3(~3, ~0x18, ~1)|SIMM13(~0),	"[1]o,d", 0, v9 }, /* ld [rs1+0],d */

{ "lduba",	F3(3, 0x11, 0), F3(~3, ~0x11, ~0),		"[1+2]A,d", 0, v6 },
{ "lduba",	F3(3, 0x11, 0), F3(~3, ~0x11, ~0)|RS2_G0,	"[1]A,d", 0, v6 }, /* lduba [rs1+%g0],d */
{ "lduba",	F3(3, 0x11, 1), F3(~3, ~0x11, ~1),		"[1+i]o,d", 0, v9 },
{ "lduba",	F3(3, 0x11, 1), F3(~3, ~0x11, ~1),		"[i+1]o,d", 0, v9 },
{ "lduba",	F3(3, 0x11, 1), F3(~3, ~0x11, ~1)|RS1_G0,	"[i]o,d", 0, v9 },
{ "lduba",	F3(3, 0x11, 1), F3(~3, ~0x11, ~1)|SIMM13(~0),	"[1]o,d", 0, v9 }, /* ld [rs1+0],d */

{ "lduha",	F3(3, 0x12, 0), F3(~3, ~0x12, ~0),		"[1+2]A,d", 0, v6 },
{ "lduha",	F3(3, 0x12, 0), F3(~3, ~0x12, ~0)|RS2_G0,	"[1]A,d", 0, v6 }, /* lduha [rs1+%g0],d */
{ "lduha",	F3(3, 0x12, 1), F3(~3, ~0x12, ~1),		"[1+i]o,d", 0, v9 },
{ "lduha",	F3(3, 0x12, 1), F3(~3, ~0x12, ~1),		"[i+1]o,d", 0, v9 },
{ "lduha",	F3(3, 0x12, 1), F3(~3, ~0x12, ~1)|RS1_G0,	"[i]o,d", 0, v9 },
{ "lduha",	F3(3, 0x12, 1), F3(~3, ~0x12, ~1)|SIMM13(~0),	"[1]o,d", 0, v9 }, /* ld [rs1+0],d */

{ "lduwa",	F3(3, 0x10, 0), F3(~3, ~0x10, ~0),		"[1+2]A,d", F_ALIAS, v9 }, /* lduwa === lda */
{ "lduwa",	F3(3, 0x10, 0), F3(~3, ~0x10, ~0)|RS2_G0,	"[1]A,d", F_ALIAS, v9 }, /* lda [rs1+%g0],d */
{ "lduwa",	F3(3, 0x10, 1), F3(~3, ~0x10, ~1),		"[1+i]o,d", F_ALIAS, v9 },
{ "lduwa",	F3(3, 0x10, 1), F3(~3, ~0x10, ~1),		"[i+1]o,d", F_ALIAS, v9 },
{ "lduwa",	F3(3, 0x10, 1), F3(~3, ~0x10, ~1)|RS1_G0,	"[i]o,d", F_ALIAS, v9 },
{ "lduwa",	F3(3, 0x10, 1), F3(~3, ~0x10, ~1)|SIMM13(~0),	"[1]o,d", F_ALIAS, v9 }, /* ld [rs1+0],d */

{ "ldxa",	F3(3, 0x1b, 0), F3(~3, ~0x1b, ~0),		"[1+2]A,d", 0, v9 }, /* lduwa === lda */
{ "ldxa",	F3(3, 0x1b, 0), F3(~3, ~0x1b, ~0)|RS2_G0,	"[1]A,d", 0, v9 }, /* lda [rs1+%g0],d */
{ "ldxa",	F3(3, 0x1b, 1), F3(~3, ~0x1b, ~1),		"[1+i]o,d", 0, v9 },
{ "ldxa",	F3(3, 0x1b, 1), F3(~3, ~0x1b, ~1),		"[i+1]o,d", 0, v9 },
{ "ldxa",	F3(3, 0x1b, 1), F3(~3, ~0x1b, ~1)|RS1_G0,	"[i]o,d", 0, v9 },
{ "ldxa",	F3(3, 0x1b, 1), F3(~3, ~0x1b, ~1)|SIMM13(~0),	"[1]o,d", 0, v9 }, /* ld [rs1+0],d */

{ "st",	F3(3, 0x04, 0), F3(~3, ~0x04, ~0)|ASI(~0),		"d,[1+2]", 0, v6 },
{ "st",	F3(3, 0x04, 0), F3(~3, ~0x04, ~0)|ASI_RS2(~0),		"d,[1]", 0, v6 }, /* st d,[rs1+%g0] */
{ "st",	F3(3, 0x04, 1), F3(~3, ~0x04, ~1),			"d,[1+i]", 0, v6 },
{ "st",	F3(3, 0x04, 1), F3(~3, ~0x04, ~1),			"d,[i+1]", 0, v6 },
{ "st",	F3(3, 0x04, 1), F3(~3, ~0x04, ~1)|RS1_G0,		"d,[i]", 0, v6 },
{ "st",	F3(3, 0x04, 1), F3(~3, ~0x04, ~1)|SIMM13(~0),		"d,[1]", 0, v6 }, /* st d,[rs1+0] */
{ "st",	F3(3, 0x24, 0), F3(~3, ~0x24, ~0)|ASI(~0),		"g,[1+2]", 0, v6 },
{ "st",	F3(3, 0x24, 0), F3(~3, ~0x24, ~0)|ASI_RS2(~0),		"g,[1]", 0, v6 }, /* st d[rs1+%g0] */
{ "st",	F3(3, 0x24, 1), F3(~3, ~0x24, ~1),			"g,[1+i]", 0, v6 },
{ "st",	F3(3, 0x24, 1), F3(~3, ~0x24, ~1),			"g,[i+1]", 0, v6 },
{ "st",	F3(3, 0x24, 1), F3(~3, ~0x24, ~1)|RS1_G0,		"g,[i]", 0, v6 },
{ "st",	F3(3, 0x24, 1), F3(~3, ~0x24, ~1)|SIMM13(~0),		"g,[1]", 0, v6 }, /* st d,[rs1+0] */

{ "st",	F3(3, 0x34, 0), F3(~3, ~0x34, ~0)|ASI(~0),		"D,[1+2]", F_NOTV9, v6 },
{ "st",	F3(3, 0x34, 0), F3(~3, ~0x34, ~0)|ASI_RS2(~0),		"D,[1]", F_NOTV9, v6 }, /* st d,[rs1+%g0] */
{ "st",	F3(3, 0x34, 1), F3(~3, ~0x34, ~1),			"D,[1+i]", F_NOTV9, v6 },
{ "st",	F3(3, 0x34, 1), F3(~3, ~0x34, ~1),			"D,[i+1]", F_NOTV9, v6 },
{ "st",	F3(3, 0x34, 1), F3(~3, ~0x34, ~1)|RS1_G0,		"D,[i]", F_NOTV9, v6 },
{ "st",	F3(3, 0x34, 1), F3(~3, ~0x34, ~1)|SIMM13(~0),		"D,[1]", F_NOTV9, v6 }, /* st d,[rs1+0] */
{ "st",	F3(3, 0x35, 0), F3(~3, ~0x35, ~0)|ASI(~0),		"C,[1+2]", F_NOTV9, v6 },
{ "st",	F3(3, 0x35, 0), F3(~3, ~0x35, ~0)|ASI_RS2(~0),		"C,[1]", F_NOTV9, v6 }, /* st d,[rs1+%g0] */
{ "st",	F3(3, 0x35, 1), F3(~3, ~0x35, ~1),			"C,[1+i]", F_NOTV9, v6 },
{ "st",	F3(3, 0x35, 1), F3(~3, ~0x35, ~1),			"C,[i+1]", F_NOTV9, v6 },
{ "st",	F3(3, 0x35, 1), F3(~3, ~0x35, ~1)|RS1_G0,		"C,[i]", F_NOTV9, v6 },
{ "st",	F3(3, 0x35, 1), F3(~3, ~0x35, ~1)|SIMM13(~0),		"C,[1]", F_NOTV9, v6 }, /* st d,[rs1+0] */

{ "st",	F3(3, 0x25, 0), F3(~3, ~0x25, ~0)|RD_G0|ASI(~0),	"F,[1+2]", 0, v6 },
{ "st",	F3(3, 0x25, 0), F3(~3, ~0x25, ~0)|RD_G0|ASI_RS2(~0),	"F,[1]", 0, v6 }, /* st d,[rs1+%g0] */
{ "st",	F3(3, 0x25, 1), F3(~3, ~0x25, ~1)|RD_G0,		"F,[1+i]", 0, v6 },
{ "st",	F3(3, 0x25, 1), F3(~3, ~0x25, ~1)|RD_G0,		"F,[i+1]", 0, v6 },
{ "st",	F3(3, 0x25, 1), F3(~3, ~0x25, ~1)|RD_G0|RS1_G0,		"F,[i]", 0, v6 },
{ "st",	F3(3, 0x25, 1), F3(~3, ~0x25, ~1)|RD_G0|SIMM13(~0),	"F,[1]", 0, v6 }, /* st d,[rs1+0] */

{ "stw",	F3(3, 0x04, 0), F3(~3, ~0x04, ~0)|ASI(~0),	"d,[1+2]", F_ALIAS, v9 },
{ "stw",	F3(3, 0x04, 0), F3(~3, ~0x04, ~0)|ASI_RS2(~0),	"d,[1]", F_ALIAS, v9 }, /* st d,[rs1+%g0] */
{ "stw",	F3(3, 0x04, 1), F3(~3, ~0x04, ~1),		"d,[1+i]", F_ALIAS, v9 },
{ "stw",	F3(3, 0x04, 1), F3(~3, ~0x04, ~1),		"d,[i+1]", F_ALIAS, v9 },
{ "stw",	F3(3, 0x04, 1), F3(~3, ~0x04, ~1)|RS1_G0,	"d,[i]", F_ALIAS, v9 },
{ "stw",	F3(3, 0x04, 1), F3(~3, ~0x04, ~1)|SIMM13(~0),	"d,[1]", F_ALIAS, v9 }, /* st d,[rs1+0] */

{ "sta",	F3(3, 0x14, 0), F3(~3, ~0x14, ~0),		"d,[1+2]A", 0, v6 },
{ "sta",	F3(3, 0x14, 0), F3(~3, ~0x14, ~0)|RS2(~0),	"d,[1]A", 0, v6 }, /* sta d,[rs1+%g0] */
{ "sta",	F3(3, 0x14, 1), F3(~3, ~0x14, ~1),		"d,[1+i]o", 0, v9 },
{ "sta",	F3(3, 0x14, 1), F3(~3, ~0x14, ~1),		"d,[i+1]o", 0, v9 },
{ "sta",	F3(3, 0x14, 1), F3(~3, ~0x14, ~1)|RS1_G0,	"d,[i]o", 0, v9 },
{ "sta",	F3(3, 0x14, 1), F3(~3, ~0x14, ~1)|SIMM13(~0),	"d,[1]o", 0, v9 }, /* st d,[rs1+0] */

{ "sta",	F3(3, 0x34, 0), F3(~3, ~0x34, ~0),		"g,[1+2]A", 0, v9 },
{ "sta",	F3(3, 0x34, 0), F3(~3, ~0x34, ~0)|RS2(~0),	"g,[1]A", 0, v9 }, /* sta d,[rs1+%g0] */
{ "sta",	F3(3, 0x34, 1), F3(~3, ~0x34, ~1),		"g,[1+i]o", 0, v9 },
{ "sta",	F3(3, 0x34, 1), F3(~3, ~0x34, ~1),		"g,[i+1]o", 0, v9 },
{ "sta",	F3(3, 0x34, 1), F3(~3, ~0x34, ~1)|RS1_G0,	"g,[i]o", 0, v9 },
{ "sta",	F3(3, 0x34, 1), F3(~3, ~0x34, ~1)|SIMM13(~0),	"g,[1]o", 0, v9 }, /* st d,[rs1+0] */

{ "stwa",	F3(3, 0x14, 0), F3(~3, ~0x14, ~0),		"d,[1+2]A", F_ALIAS, v9 },
{ "stwa",	F3(3, 0x14, 0), F3(~3, ~0x14, ~0)|RS2(~0),	"d,[1]A", F_ALIAS, v9 }, /* sta d,[rs1+%g0] */
{ "stwa",	F3(3, 0x14, 1), F3(~3, ~0x14, ~1),		"d,[1+i]o", F_ALIAS, v9 },
{ "stwa",	F3(3, 0x14, 1), F3(~3, ~0x14, ~1),		"d,[i+1]o", F_ALIAS, v9 },
{ "stwa",	F3(3, 0x14, 1), F3(~3, ~0x14, ~1)|RS1_G0,	"d,[i]o", F_ALIAS, v9 },
{ "stwa",	F3(3, 0x14, 1), F3(~3, ~0x14, ~1)|SIMM13(~0),	"d,[1]o", F_ALIAS, v9 }, /* st d,[rs1+0] */

{ "stb",	F3(3, 0x05, 0), F3(~3, ~0x05, ~0)|ASI(~0),	"d,[1+2]", 0, v6 },
{ "stb",	F3(3, 0x05, 0), F3(~3, ~0x05, ~0)|ASI_RS2(~0),	"d,[1]", 0, v6 }, /* stb d,[rs1+%g0] */
{ "stb",	F3(3, 0x05, 1), F3(~3, ~0x05, ~1),		"d,[1+i]", 0, v6 },
{ "stb",	F3(3, 0x05, 1), F3(~3, ~0x05, ~1),		"d,[i+1]", 0, v6 },
{ "stb",	F3(3, 0x05, 1), F3(~3, ~0x05, ~1)|RS1_G0,	"d,[i]", 0, v6 },
{ "stb",	F3(3, 0x05, 1), F3(~3, ~0x05, ~1)|SIMM13(~0),	"d,[1]", 0, v6 }, /* stb d,[rs1+0] */

{ "stba",	F3(3, 0x15, 0), F3(~3, ~0x15, ~0),		"d,[1+2]A", 0, v6 },
{ "stba",	F3(3, 0x15, 0), F3(~3, ~0x15, ~0)|RS2(~0),	"d,[1]A", 0, v6 }, /* stba d,[rs1+%g0] */
{ "stba",	F3(3, 0x15, 1), F3(~3, ~0x15, ~1),		"d,[1+i]o", 0, v9 },
{ "stba",	F3(3, 0x15, 1), F3(~3, ~0x15, ~1),		"d,[i+1]o", 0, v9 },
{ "stba",	F3(3, 0x15, 1), F3(~3, ~0x15, ~1)|RS1_G0,	"d,[i]o", 0, v9 },
{ "stba",	F3(3, 0x15, 1), F3(~3, ~0x15, ~1)|SIMM13(~0),	"d,[1]o", 0, v9 }, /* stb d,[rs1+0] */

{ "std",	F3(3, 0x07, 0), F3(~3, ~0x07, ~0)|ASI(~0),	"d,[1+2]", 0, v6 },
{ "std",	F3(3, 0x07, 0), F3(~3, ~0x07, ~0)|ASI_RS2(~0),	"d,[1]", 0, v6 }, /* std d,[rs1+%g0] */
{ "std",	F3(3, 0x07, 1), F3(~3, ~0x07, ~1),		"d,[1+i]", 0, v6 },
{ "std",	F3(3, 0x07, 1), F3(~3, ~0x07, ~1),		"d,[i+1]", 0, v6 },
{ "std",	F3(3, 0x07, 1), F3(~3, ~0x07, ~1)|RS1_G0,	"d,[i]", 0, v6 },
{ "std",	F3(3, 0x07, 1), F3(~3, ~0x07, ~1)|SIMM13(~0),	"d,[1]", 0, v6 }, /* std d,[rs1+0] */

{ "std",	F3(3, 0x26, 0), F3(~3, ~0x26, ~0)|ASI(~0),	"q,[1+2]", F_NOTV9, v6 },
{ "std",	F3(3, 0x26, 0), F3(~3, ~0x26, ~0)|ASI_RS2(~0),	"q,[1]", F_NOTV9, v6 }, /* std d,[rs1+%g0] */
{ "std",	F3(3, 0x26, 1), F3(~3, ~0x26, ~1),		"q,[1+i]", F_NOTV9, v6 },
{ "std",	F3(3, 0x26, 1), F3(~3, ~0x26, ~1),		"q,[i+1]", F_NOTV9, v6 },
{ "std",	F3(3, 0x26, 1), F3(~3, ~0x26, ~1)|RS1_G0,	"q,[i]", F_NOTV9, v6 },
{ "std",	F3(3, 0x26, 1), F3(~3, ~0x26, ~1)|SIMM13(~0),	"q,[1]", F_NOTV9, v6 }, /* std d,[rs1+0] */
{ "std",	F3(3, 0x27, 0), F3(~3, ~0x27, ~0)|ASI(~0),	"H,[1+2]", 0, v6 },
{ "std",	F3(3, 0x27, 0), F3(~3, ~0x27, ~0)|ASI_RS2(~0),	"H,[1]", 0, v6 }, /* std d,[rs1+%g0] */
{ "std",	F3(3, 0x27, 1), F3(~3, ~0x27, ~1),		"H,[1+i]", 0, v6 },
{ "std",	F3(3, 0x27, 1), F3(~3, ~0x27, ~1),		"H,[i+1]", 0, v6 },
{ "std",	F3(3, 0x27, 1), F3(~3, ~0x27, ~1)|RS1_G0,	"H,[i]", 0, v6 },
{ "std",	F3(3, 0x27, 1), F3(~3, ~0x27, ~1)|SIMM13(~0),	"H,[1]", 0, v6 }, /* std d,[rs1+0] */

{ "std",	F3(3, 0x36, 0), F3(~3, ~0x36, ~0)|ASI(~0),	"Q,[1+2]", F_NOTV9, v6 },
{ "std",	F3(3, 0x36, 0), F3(~3, ~0x36, ~0)|ASI_RS2(~0),	"Q,[1]", F_NOTV9, v6 }, /* std d,[rs1+%g0] */
{ "std",	F3(3, 0x36, 1), F3(~3, ~0x36, ~1),		"Q,[1+i]", F_NOTV9, v6 },
{ "std",	F3(3, 0x36, 1), F3(~3, ~0x36, ~1),		"Q,[i+1]", F_NOTV9, v6 },
{ "std",	F3(3, 0x36, 1), F3(~3, ~0x36, ~1)|RS1_G0,	"Q,[i]", F_NOTV9, v6 },
{ "std",	F3(3, 0x36, 1), F3(~3, ~0x36, ~1)|SIMM13(~0),	"Q,[1]", F_NOTV9, v6 }, /* std d,[rs1+0] */
{ "std",	F3(3, 0x37, 0), F3(~3, ~0x37, ~0)|ASI(~0),	"D,[1+2]", F_NOTV9, v6 },
{ "std",	F3(3, 0x37, 0), F3(~3, ~0x37, ~0)|ASI_RS2(~0),	"D,[1]", F_NOTV9, v6 }, /* std d,[rs1+%g0] */
{ "std",	F3(3, 0x37, 1), F3(~3, ~0x37, ~1),		"D,[1+i]", F_NOTV9, v6 },
{ "std",	F3(3, 0x37, 1), F3(~3, ~0x37, ~1),		"D,[i+1]", F_NOTV9, v6 },
{ "std",	F3(3, 0x37, 1), F3(~3, ~0x37, ~1)|RS1_G0,	"D,[i]", F_NOTV9, v6 },
{ "std",	F3(3, 0x37, 1), F3(~3, ~0x37, ~1)|SIMM13(~0),	"D,[1]", F_NOTV9, v6 }, /* std d,[rs1+0] */

{ "stda",	F3(3, 0x17, 0), F3(~3, ~0x17, ~0),		"d,[1+2]A", 0, v6 },
{ "stda",	F3(3, 0x17, 0), F3(~3, ~0x17, ~0)|RS2(~0),	"d,[1]A", 0, v6 }, /* stda d,[rs1+%g0] */
{ "stda",	F3(3, 0x17, 1), F3(~3, ~0x17, ~1),		"d,[1+i]o", 0, v9 },
{ "stda",	F3(3, 0x17, 1), F3(~3, ~0x17, ~1),		"d,[i+1]o", 0, v9 },
{ "stda",	F3(3, 0x17, 1), F3(~3, ~0x17, ~1)|RS1_G0,	"d,[i]o", 0, v9 },
{ "stda",	F3(3, 0x17, 1), F3(~3, ~0x17, ~1)|SIMM13(~0),	"d,[1]o", 0, v9 }, /* std d,[rs1+0] */
{ "stda",	F3(3, 0x37, 0), F3(~3, ~0x37, ~0),		"H,[1+2]A", 0, v9 },
{ "stda",	F3(3, 0x37, 0), F3(~3, ~0x37, ~0)|RS2(~0),	"H,[1]A", 0, v9 }, /* stda d,[rs1+%g0] */
{ "stda",	F3(3, 0x37, 1), F3(~3, ~0x37, ~1),		"H,[1+i]o", 0, v9 },
{ "stda",	F3(3, 0x37, 1), F3(~3, ~0x37, ~1),		"H,[i+1]o", 0, v9 },
{ "stda",	F3(3, 0x37, 1), F3(~3, ~0x37, ~1)|RS1_G0,	"H,[i]o", 0, v9 },
{ "stda",	F3(3, 0x37, 1), F3(~3, ~0x37, ~1)|SIMM13(~0),	"H,[1]o", 0, v9 }, /* std d,[rs1+0] */

{ "sth",	F3(3, 0x06, 0), F3(~3, ~0x06, ~0)|ASI(~0),	"d,[1+2]", 0, v6 },
{ "sth",	F3(3, 0x06, 0), F3(~3, ~0x06, ~0)|ASI_RS2(~0),	"d,[1]", 0, v6 }, /* sth d,[rs1+%g0] */
{ "sth",	F3(3, 0x06, 1), F3(~3, ~0x06, ~1),		"d,[1+i]", 0, v6 },
{ "sth",	F3(3, 0x06, 1), F3(~3, ~0x06, ~1),		"d,[i+1]", 0, v6 },
{ "sth",	F3(3, 0x06, 1), F3(~3, ~0x06, ~1)|RS1_G0,	"d,[i]", 0, v6 },
{ "sth",	F3(3, 0x06, 1), F3(~3, ~0x06, ~1)|SIMM13(~0),	"d,[1]", 0, v6 }, /* sth d,[rs1+0] */

{ "stha",	F3(3, 0x16, 0), F3(~3, ~0x16, ~0),		"d,[1+2]A", 0, v6 },
{ "stha",	F3(3, 0x16, 0), F3(~3, ~0x16, ~0)|RS2(~0),	"d,[1]A", 0, v6 }, /* stha ,[rs1+%g0] */
{ "stha",	F3(3, 0x16, 1), F3(~3, ~0x16, ~1),		"d,[1+i]o", 0, v9 },
{ "stha",	F3(3, 0x16, 1), F3(~3, ~0x16, ~1),		"d,[i+1]o", 0, v9 },
{ "stha",	F3(3, 0x16, 1), F3(~3, ~0x16, ~1)|RS1_G0,	"d,[i]o", 0, v9 },
{ "stha",	F3(3, 0x16, 1), F3(~3, ~0x16, ~1)|SIMM13(~0),	"d,[1]o", 0, v9 }, /* sth d,[rs1+0] */

{ "stx",	F3(3, 0x0e, 0), F3(~3, ~0x0e, ~0)|ASI(~0),	"d,[1+2]", 0, v9 },
{ "stx",	F3(3, 0x0e, 0), F3(~3, ~0x0e, ~0)|ASI_RS2(~0),	"d,[1]", 0, v9 }, /* stx d,[rs1+%g0] */
{ "stx",	F3(3, 0x0e, 1), F3(~3, ~0x0e, ~1),		"d,[1+i]", 0, v9 },
{ "stx",	F3(3, 0x0e, 1), F3(~3, ~0x0e, ~1),		"d,[i+1]", 0, v9 },
{ "stx",	F3(3, 0x0e, 1), F3(~3, ~0x0e, ~1)|RS1_G0,	"d,[i]", 0, v9 },
{ "stx",	F3(3, 0x0e, 1), F3(~3, ~0x0e, ~1)|SIMM13(~0),	"d,[1]", 0, v9 }, /* stx d,[rs1+0] */

{ "stx",	F3(3, 0x25, 0)|RD(1), F3(~3, ~0x25, ~0)|ASI(~0)|RD(~1),	"F,[1+2]", 0, v9 },
{ "stx",	F3(3, 0x25, 0)|RD(1), F3(~3, ~0x25, ~0)|ASI_RS2(~0)|RD(~1),"F,[1]", 0, v9 }, /* stx d,[rs1+%g0] */
{ "stx",	F3(3, 0x25, 1)|RD(1), F3(~3, ~0x25, ~1)|RD(~1),		"F,[1+i]", 0, v9 },
{ "stx",	F3(3, 0x25, 1)|RD(1), F3(~3, ~0x25, ~1)|RD(~1),		"F,[i+1]", 0, v9 },
{ "stx",	F3(3, 0x25, 1)|RD(1), F3(~3, ~0x25, ~1)|RS1_G0|RD(~1),	"F,[i]", 0, v9 },
{ "stx",	F3(3, 0x25, 1)|RD(1), F3(~3, ~0x25, ~1)|SIMM13(~0)|RD(~1),"F,[1]", 0, v9 }, /* stx d,[rs1+0] */

{ "stxa",	F3(3, 0x1e, 0), F3(~3, ~0x1e, ~0),		"d,[1+2]A", 0, v9 },
{ "stxa",	F3(3, 0x1e, 0), F3(~3, ~0x1e, ~0)|RS2(~0),	"d,[1]A", 0, v9 }, /* stxa d,[rs1+%g0] */
{ "stxa",	F3(3, 0x1e, 1), F3(~3, ~0x1e, ~1),		"d,[1+i]o", 0, v9 },
{ "stxa",	F3(3, 0x1e, 1), F3(~3, ~0x1e, ~1),		"d,[i+1]o", 0, v9 },
{ "stxa",	F3(3, 0x1e, 1), F3(~3, ~0x1e, ~1)|RS1_G0,	"d,[i]o", 0, v9 },
{ "stxa",	F3(3, 0x1e, 1), F3(~3, ~0x1e, ~1)|SIMM13(~0),	"d,[1]o", 0, v9 }, /* stx d,[rs1+0] */

{ "stq",	F3(3, 0x26, 0), F3(~3, ~0x26, ~0)|ASI(~0),	"J,[1+2]", 0, v9 },
{ "stq",	F3(3, 0x26, 0), F3(~3, ~0x26, ~0)|ASI_RS2(~0),	"J,[1]", 0, v9 }, /* stq [rs1+%g0] */
{ "stq",	F3(3, 0x26, 1), F3(~3, ~0x26, ~1),		"J,[1+i]", 0, v9 },
{ "stq",	F3(3, 0x26, 1), F3(~3, ~0x26, ~1),		"J,[i+1]", 0, v9 },
{ "stq",	F3(3, 0x26, 1), F3(~3, ~0x26, ~1)|RS1_G0,	"J,[i]", 0, v9 },
{ "stq",	F3(3, 0x26, 1), F3(~3, ~0x26, ~1)|SIMM13(~0),	"J,[1]", 0, v9 }, /* stq [rs1+0] */

{ "stqa",	F3(3, 0x36, 0), F3(~3, ~0x36, ~0)|ASI(~0),	"J,[1+2]A", 0, v9 },
{ "stqa",	F3(3, 0x36, 0), F3(~3, ~0x36, ~0)|ASI_RS2(~0),	"J,[1]A", 0, v9 }, /* stqa [rs1+%g0] */
{ "stqa",	F3(3, 0x36, 1), F3(~3, ~0x36, ~1),		"J,[1+i]o", 0, v9 },
{ "stqa",	F3(3, 0x36, 1), F3(~3, ~0x36, ~1),		"J,[i+1]o", 0, v9 },
{ "stqa",	F3(3, 0x36, 1), F3(~3, ~0x36, ~1)|RS1_G0,	"J,[i]o", 0, v9 },
{ "stqa",	F3(3, 0x36, 1), F3(~3, ~0x36, ~1)|SIMM13(~0),	"J,[1]o", 0, v9 }, /* stqa [rs1+0] */

{ "swap",	F3(3, 0x0f, 0), F3(~3, ~0x0f, ~0)|ASI(~0),	"[1+2],d", 0, v7 },
{ "swap",	F3(3, 0x0f, 0), F3(~3, ~0x0f, ~0)|ASI_RS2(~0),	"[1],d", 0, v7 }, /* swap [rs1+%g0],d */
{ "swap",	F3(3, 0x0f, 1), F3(~3, ~0x0f, ~1),		"[1+i],d", 0, v7 },
{ "swap",	F3(3, 0x0f, 1), F3(~3, ~0x0f, ~1),		"[i+1],d", 0, v7 },
{ "swap",	F3(3, 0x0f, 1), F3(~3, ~0x0f, ~1)|RS1_G0,	"[i],d", 0, v7 },
{ "swap",	F3(3, 0x0f, 1), F3(~3, ~0x0f, ~1)|SIMM13(~0),	"[1],d", 0, v7 }, /* swap [rs1+0],d */

{ "swapa",	F3(3, 0x1f, 0), F3(~3, ~0x1f, ~0),		"[1+2]A,d", 0, v7 },
{ "swapa",	F3(3, 0x1f, 0), F3(~3, ~0x1f, ~0)|RS2(~0),	"[1]A,d", 0, v7 }, /* swapa [rs1+%g0],d */
{ "swapa",	F3(3, 0x1f, 1), F3(~3, ~0x1f, ~1),		"[1+i]o,d", 0, v9 },
{ "swapa",	F3(3, 0x1f, 1), F3(~3, ~0x1f, ~1),		"[i+1]o,d", 0, v9 },
{ "swapa",	F3(3, 0x1f, 1), F3(~3, ~0x1f, ~1)|RS1_G0,	"[i]o,d", 0, v9 },
{ "swapa",	F3(3, 0x1f, 1), F3(~3, ~0x1f, ~1)|SIMM13(~0),	"[1]o,d", 0, v9 }, /* swap [rs1+0],d */

{ "restore",	F3(2, 0x3d, 0), F3(~2, ~0x3d, ~0)|ASI(~0),			"1,2,d", 0, v6 },
{ "restore",	F3(2, 0x3d, 0), F3(~2, ~0x3d, ~0)|RD_G0|RS1_G0|ASI_RS2(~0),	"", 0, v6 }, /* restore %g0,%g0,%g0 */
{ "restore",	F3(2, 0x3d, 1), F3(~2, ~0x3d, ~1),				"1,i,d", 0, v6 },
{ "restore",	F3(2, 0x3d, 1), F3(~2, ~0x3d, ~1)|RD_G0|RS1_G0|SIMM13(~0),	"", 0, v6 }, /* restore %g0,0,%g0 */

{ "rett",	F3(2, 0x39, 0), F3(~2, ~0x39, ~0)|RD_G0|ASI(~0),	"1+2", F_UNBR|F_DELAYED, v6 }, /* rett rs1+rs2 */
{ "rett",	F3(2, 0x39, 0), F3(~2, ~0x39, ~0)|RD_G0|ASI_RS2(~0),	"1", F_UNBR|F_DELAYED, v6 },	/* rett rs1,%g0 */
{ "rett",	F3(2, 0x39, 1), F3(~2, ~0x39, ~1)|RD_G0,		"1+i", F_UNBR|F_DELAYED, v6 }, /* rett rs1+X */
{ "rett",	F3(2, 0x39, 1), F3(~2, ~0x39, ~1)|RD_G0,		"i+1", F_UNBR|F_DELAYED, v6 }, /* rett X+rs1 */
{ "rett",	F3(2, 0x39, 1), F3(~2, ~0x39, ~1)|RD_G0|RS1_G0,		"i", F_UNBR|F_DELAYED, v6 }, /* rett X+rs1 */
{ "rett",	F3(2, 0x39, 1), F3(~2, ~0x39, ~1)|RD_G0|RS1_G0,		"i", F_UNBR|F_DELAYED, v6 },	/* rett X */
{ "rett",	F3(2, 0x39, 1), F3(~2, ~0x39, ~1)|RD_G0|SIMM13(~0),	"1", F_UNBR|F_DELAYED, v6 },	/* rett rs1+0 */

{ "save",	F3(2, 0x3c, 0), F3(~2, ~0x3c, ~0)|ASI(~0),	"1,2,d", 0, v6 },
{ "save",	F3(2, 0x3c, 1), F3(~2, ~0x3c, ~1),		"1,i,d", 0, v6 },
{ "save",	0x81e00000,	~0x81e00000,			"", F_ALIAS, v6 },

{ "ret",  F3(2, 0x38, 1)|RS1(0x1f)|SIMM13(8), F3(~2, ~0x38, ~1)|SIMM13(~8),	       "", F_UNBR|F_DELAYED, v6 }, /* jmpl %i7+8,%g0 */
{ "retl", F3(2, 0x38, 1)|RS1(0x0f)|SIMM13(8), F3(~2, ~0x38, ~1)|RS1(~0x0f)|SIMM13(~8), "", F_UNBR|F_DELAYED, v6 }, /* jmpl %o7+8,%g0 */

{ "jmpl",	F3(2, 0x38, 0), F3(~2, ~0x38, ~0)|ASI(~0),	"1+2,d", F_JSR|F_DELAYED, v6 },
{ "jmpl",	F3(2, 0x38, 0), F3(~2, ~0x38, ~0)|ASI_RS2(~0),	"1,d", F_JSR|F_DELAYED, v6 }, /* jmpl rs1+%g0,d */
{ "jmpl",	F3(2, 0x38, 1), F3(~2, ~0x38, ~1)|SIMM13(~0),	"1,d", F_JSR|F_DELAYED, v6 }, /* jmpl rs1+0,d */
{ "jmpl",	F3(2, 0x38, 1), F3(~2, ~0x38, ~1)|RS1_G0,	"i,d", F_JSR|F_DELAYED, v6 }, /* jmpl %g0+i,d */
{ "jmpl",	F3(2, 0x38, 1), F3(~2, ~0x38, ~1),		"1+i,d", F_JSR|F_DELAYED, v6 },
{ "jmpl",	F3(2, 0x38, 1), F3(~2, ~0x38, ~1),		"i+1,d", F_JSR|F_DELAYED, v6 },

{ "done",	F3(2, 0x3e, 0)|RD(0), F3(~2, ~0x3e, ~0)|RD(~0)|RS1_G0|SIMM13(~0),	"", 0, v9 },
{ "retry",	F3(2, 0x3e, 0)|RD(1), F3(~2, ~0x3e, ~0)|RD(~1)|RS1_G0|SIMM13(~0),	"", 0, v9 },
{ "saved",	F3(2, 0x31, 0)|RD(0), F3(~2, ~0x31, ~0)|RD(~0)|RS1_G0|SIMM13(~0),	"", 0, v9 },
{ "restored",	F3(2, 0x31, 0)|RD(1), F3(~2, ~0x31, ~0)|RD(~1)|RS1_G0|SIMM13(~0),	"", 0, v9 },
{ "sir",	F3(2, 0x30, 1)|RD(0xf), F3(~2, ~0x30, ~1)|RD(~0xf)|RS1_G0,		"i", 0, v9 },

{ "flush",	F3(2, 0x3b, 0), F3(~2, ~0x3b, ~0)|ASI(~0),	"1+2", 0, v8 },
{ "flush",	F3(2, 0x3b, 0), F3(~2, ~0x3b, ~0)|ASI_RS2(~0),	"1", 0, v8 }, /* flush rs1+%g0 */
{ "flush",	F3(2, 0x3b, 1), F3(~2, ~0x3b, ~1)|SIMM13(~0),	"1", 0, v8 }, /* flush rs1+0 */
{ "flush",	F3(2, 0x3b, 1), F3(~2, ~0x3b, ~1)|RS1_G0,	"i", 0, v8 }, /* flush %g0+i */
{ "flush",	F3(2, 0x3b, 1), F3(~2, ~0x3b, ~1),		"1+i", 0, v8 },
{ "flush",	F3(2, 0x3b, 1), F3(~2, ~0x3b, ~1),		"i+1", 0, v8 },

/* IFLUSH was renamed to FLUSH in v8.  */
{ "iflush",	F3(2, 0x3b, 0), F3(~2, ~0x3b, ~0)|ASI(~0),	"1+2", F_ALIAS, v6 },
{ "iflush",	F3(2, 0x3b, 0), F3(~2, ~0x3b, ~0)|ASI_RS2(~0),	"1", F_ALIAS, v6 }, /* flush rs1+%g0 */
{ "iflush",	F3(2, 0x3b, 1), F3(~2, ~0x3b, ~1)|SIMM13(~0),	"1", F_ALIAS, v6 }, /* flush rs1+0 */
{ "iflush",	F3(2, 0x3b, 1), F3(~2, ~0x3b, ~1)|RS1_G0,	"i", F_ALIAS, v6 },
{ "iflush",	F3(2, 0x3b, 1), F3(~2, ~0x3b, ~1),		"1+i", F_ALIAS, v6 },
{ "iflush",	F3(2, 0x3b, 1), F3(~2, ~0x3b, ~1),		"i+1", F_ALIAS, v6 },

{ "return",	F3(2, 0x39, 0), F3(~2, ~0x39, ~0)|ASI(~0),	"1+2", 0, v9 },
{ "return",	F3(2, 0x39, 0), F3(~2, ~0x39, ~0)|ASI_RS2(~0),	"1", 0, v9 }, /* return rs1+%g0 */
{ "return",	F3(2, 0x39, 1), F3(~2, ~0x39, ~1)|SIMM13(~0),	"1", 0, v9 }, /* return rs1+0 */
{ "return",	F3(2, 0x39, 1), F3(~2, ~0x39, ~1)|RS1_G0,	"i", 0, v9 }, /* return %g0+i */
{ "return",	F3(2, 0x39, 1), F3(~2, ~0x39, ~1),		"1+i", 0, v9 },
{ "return",	F3(2, 0x39, 1), F3(~2, ~0x39, ~1),		"i+1", 0, v9 },

{ "flushw",	F3(2, 0x2b, 0), F3(~2, ~0x2b, ~0)|RD_G0|RS1_G0|ASI_RS2(~0),	"", 0, v9 },

{ "membar",	F3(2, 0x28, 1)|RS1(0xf), F3(~2, ~0x28, ~1)|RD_G0|RS1(~0xf)|SIMM13(~127), "K", 0, v9 },
{ "stbar",	F3(2, 0x28, 0)|RS1(0xf), F3(~2, ~0x28, ~0)|RD_G0|RS1(~0xf)|SIMM13(~0), "", 0, v8 },

{ "prefetch",	F3(3, 0x2d, 0), F3(~3, ~0x2d, ~0),		"[1+2],*", 0, v9 },
{ "prefetch",	F3(3, 0x2d, 0), F3(~3, ~0x2d, ~0)|RS2_G0,	"[1],*", 0, v9 }, /* prefetch [rs1+%g0],prefetch_fcn */
{ "prefetch",	F3(3, 0x2d, 1), F3(~3, ~0x2d, ~1),		"[1+i],*", 0, v9 },
{ "prefetch",	F3(3, 0x2d, 1), F3(~3, ~0x2d, ~1),		"[i+1],*", 0, v9 },
{ "prefetch",	F3(3, 0x2d, 1), F3(~3, ~0x2d, ~1)|RS1_G0,	"[i],*", 0, v9 },
{ "prefetch",	F3(3, 0x2d, 1), F3(~3, ~0x2d, ~1)|SIMM13(~0),	"[1],*", 0, v9 }, /* prefetch [rs1+0],prefetch_fcn */
{ "prefetcha",	F3(3, 0x3d, 0), F3(~3, ~0x3d, ~0),		"[1+2]A,*", 0, v9 },
{ "prefetcha",	F3(3, 0x3d, 0), F3(~3, ~0x3d, ~0)|RS2_G0,	"[1]A,*", 0, v9 }, /* prefetcha [rs1+%g0],prefetch_fcn */
{ "prefetcha",	F3(3, 0x3d, 1), F3(~3, ~0x3d, ~1),		"[1+i]o,*", 0, v9 },
{ "prefetcha",	F3(3, 0x3d, 1), F3(~3, ~0x3d, ~1),		"[i+1]o,*", 0, v9 },
{ "prefetcha",	F3(3, 0x3d, 1), F3(~3, ~0x3d, ~1)|RS1_G0,	"[i]o,*", 0, v9 },
{ "prefetcha",	F3(3, 0x3d, 1), F3(~3, ~0x3d, ~1)|SIMM13(~0),	"[1]o,*", 0, v9 }, /* prefetcha [rs1+0],d */

 /* The 1<<12 is a long story.  It is necessary.  For more info, please contact rich@cygnus.com */
 /* FIXME: 'i' is wrong, need new letter for 5 bit unsigned constants.  */
{ "sll",	F3(2, 0x25, 0), F3(~2, ~0x25, ~0)|(1<<12)|ASI(~0),	"1,2,d", 0, v6 },
{ "sll",	F3(2, 0x25, 1), F3(~2, ~0x25, ~1)|(1<<12),		"1,i,d", 0, v6 },
{ "sra",	F3(2, 0x27, 0), F3(~2, ~0x27, ~0)|(1<<12)|ASI(~0),	"1,2,d", 0, v6 },
{ "sra",	F3(2, 0x27, 1), F3(~2, ~0x27, ~1)|(1<<12),		"1,i,d", 0, v6 },
{ "srl",	F3(2, 0x26, 0), F3(~2, ~0x26, ~0)|(1<<12)|ASI(~0),	"1,2,d", 0, v6 },
{ "srl",	F3(2, 0x26, 1), F3(~2, ~0x26, ~1)|(1<<12),		"1,i,d", 0, v6 },

 /* FIXME: 'j' is wrong, need new letter for 6 bit unsigned constants.  */
{ "sllx",	F3(2, 0x25, 0)|(1<<12), F3(~2, ~0x25, ~0)|(ASI(~0)^(1<<12)),	"1,2,d", 0, v9 },
{ "sllx",	F3(2, 0x25, 1)|(1<<12), F3(~2, ~0x25, ~1)|(0x3f<<6),		"1,j,d", 0, v9 },
{ "srax",	F3(2, 0x27, 0)|(1<<12), F3(~2, ~0x27, ~0)|(ASI(~0)^(1<<12)),	"1,2,d", 0, v9 },
{ "srax",	F3(2, 0x27, 1)|(1<<12), F3(~2, ~0x27, ~1)|(0x3f<<6),		"1,j,d", 0, v9 },
{ "srlx",	F3(2, 0x26, 0)|(1<<12), F3(~2, ~0x26, ~0)|(ASI(~0)^(1<<12)),	"1,2,d", 0, v9 },
{ "srlx",	F3(2, 0x26, 1)|(1<<12), F3(~2, ~0x26, ~1)|(0x3f<<6),		"1,j,d", 0, v9 },

{ "mulscc",	F3(2, 0x24, 0), F3(~2, ~0x24, ~0)|ASI(~0),	"1,2,d", 0, v6 },
{ "mulscc",	F3(2, 0x24, 1), F3(~2, ~0x24, ~1),		"1,i,d", 0, v6 },

{ "divscc",	F3(2, 0x1d, 0), F3(~2, ~0x1d, ~0)|ASI(~0),	"1,2,d", 0, sparclite },
{ "divscc",	F3(2, 0x1d, 1), F3(~2, ~0x1d, ~1),		"1,i,d", 0, sparclite },

{ "scan",	F3(2, 0x2c, 0), F3(~2, ~0x2c, ~0)|ASI(~0),	"1,2,d", 0, sparclite },
{ "scan",	F3(2, 0x2c, 1), F3(~2, ~0x2c, ~1),		"1,i,d", 0, sparclite },

{ "popc",	F3(2, 0x2e, 0), F3(~2, ~0x2e, ~0)|RS2_G0|ASI(~0),"2,d", 0, v9 },
{ "popc",	F3(2, 0x2e, 1), F3(~2, ~0x2e, ~1)|RS2_G0,	"i,d", 0, v9 },

{ "clr",	F3(2, 0x02, 0), F3(~2, ~0x02, ~0)|RD_G0|RS1_G0|ASI_RS2(~0),	"d", F_ALIAS, v6 }, /* or %g0,%g0,d */
{ "clr",	F3(2, 0x02, 1), F3(~2, ~0x02, ~1)|RS1_G0|SIMM13(~0),		"d", F_ALIAS, v6 }, /* or %g0,0,d	*/
{ "clr",	F3(3, 0x04, 0), F3(~3, ~0x04, ~0)|RD_G0|ASI(~0),		"[1+2]", F_ALIAS, v6 },
{ "clr",	F3(3, 0x04, 0), F3(~3, ~0x04, ~0)|RD_G0|ASI_RS2(~0),		"[1]", F_ALIAS, v6 }, /* st %g0,[rs1+%g0] */
{ "clr",	F3(3, 0x04, 1), F3(~3, ~0x04, ~1)|RD_G0,			"[1+i]", F_ALIAS, v6 },
{ "clr",	F3(3, 0x04, 1), F3(~3, ~0x04, ~1)|RD_G0,			"[i+1]", F_ALIAS, v6 },
{ "clr",	F3(3, 0x04, 1), F3(~3, ~0x04, ~1)|RD_G0|RS1_G0,			"[i]", F_ALIAS, v6 },
{ "clr",	F3(3, 0x04, 1), F3(~3, ~0x04, ~1)|RD_G0|SIMM13(~0),		"[1]", F_ALIAS, v6 }, /* st %g0,[rs1+0] */

{ "clrb",	F3(3, 0x05, 0), F3(~3, ~0x05, ~0)|RD_G0|ASI(~0),	"[1+2]", F_ALIAS, v6 },
{ "clrb",	F3(3, 0x05, 0), F3(~3, ~0x05, ~0)|RD_G0|ASI_RS2(~0),	"[1]", F_ALIAS, v6 }, /* stb %g0,[rs1+%g0] */
{ "clrb",	F3(3, 0x05, 1), F3(~3, ~0x05, ~1)|RD_G0,		"[1+i]", F_ALIAS, v6 },
{ "clrb",	F3(3, 0x05, 1), F3(~3, ~0x05, ~1)|RD_G0,		"[i+1]", F_ALIAS, v6 },
{ "clrb",	F3(3, 0x05, 1), F3(~3, ~0x05, ~1)|RD_G0|RS1_G0,		"[i]", F_ALIAS, v6 },
{ "clrb",	F3(3, 0x05, 1), F3(~3, ~0x05, ~1)|RD_G0|SIMM13(~0),	"[1]", F_ALIAS, v6 }, /* stb %g0,[rs1+0] */

{ "clrh",	F3(3, 0x06, 0), F3(~3, ~0x06, ~0)|RD_G0|ASI(~0),	"[1+2]", F_ALIAS, v6 },
{ "clrh",	F3(3, 0x06, 0), F3(~3, ~0x06, ~0)|RD_G0|ASI_RS2(~0),	"[1]", F_ALIAS, v6 }, /* sth %g0,[rs1+%g0] */
{ "clrh",	F3(3, 0x06, 1), F3(~3, ~0x06, ~1)|RD_G0,		"[1+i]", F_ALIAS, v6 },
{ "clrh",	F3(3, 0x06, 1), F3(~3, ~0x06, ~1)|RD_G0,		"[i+1]", F_ALIAS, v6 },
{ "clrh",	F3(3, 0x06, 1), F3(~3, ~0x06, ~1)|RD_G0|RS1_G0,		"[i]", F_ALIAS, v6 },
{ "clrh",	F3(3, 0x06, 1), F3(~3, ~0x06, ~1)|RD_G0|SIMM13(~0),	"[1]", F_ALIAS, v6 }, /* sth %g0,[rs1+0] */

{ "clrx",	F3(3, 0x0e, 0), F3(~3, ~0x0e, ~0)|RD_G0|ASI(~0),	"[1+2]", F_ALIAS, v9 },
{ "clrx",	F3(3, 0x0e, 0), F3(~3, ~0x0e, ~0)|RD_G0|ASI_RS2(~0),	"[1]", F_ALIAS, v9 }, /* stx %g0,[rs1+%g0] */
{ "clrx",	F3(3, 0x0e, 1), F3(~3, ~0x0e, ~1)|RD_G0,		"[1+i]", F_ALIAS, v9 },
{ "clrx",	F3(3, 0x0e, 1), F3(~3, ~0x0e, ~1)|RD_G0,		"[i+1]", F_ALIAS, v9 },
{ "clrx",	F3(3, 0x0e, 1), F3(~3, ~0x0e, ~1)|RD_G0|RS1_G0,		"[i]", F_ALIAS, v9 },
{ "clrx",	F3(3, 0x0e, 1), F3(~3, ~0x0e, ~1)|RD_G0|SIMM13(~0),	"[1]", F_ALIAS, v9 }, /* stx %g0,[rs1+0] */

{ "orcc",	F3(2, 0x12, 0), F3(~2, ~0x12, ~0)|ASI(~0),	"1,2,d", 0, v6 },
{ "orcc",	F3(2, 0x12, 1), F3(~2, ~0x12, ~1),		"1,i,d", 0, v6 },
{ "orcc",	F3(2, 0x12, 1), F3(~2, ~0x12, ~1),		"i,1,d", 0, v6 },

/* This is not a commutative instruction.  */
{ "orncc",	F3(2, 0x16, 0), F3(~2, ~0x16, ~0)|ASI(~0),	"1,2,d", 0, v6 },
{ "orncc",	F3(2, 0x16, 1), F3(~2, ~0x16, ~1),		"1,i,d", 0, v6 },

/* This is not a commutative instruction.  */
{ "orn",	F3(2, 0x06, 0), F3(~2, ~0x06, ~0)|ASI(~0),	"1,2,d", 0, v6 },
{ "orn",	F3(2, 0x06, 1), F3(~2, ~0x06, ~1),		"1,i,d", 0, v6 },

{ "tst",	F3(2, 0x12, 0), F3(~2, ~0x12, ~0)|RD_G0|ASI_RS2(~0),	"1", 0, v6 }, /* orcc rs1, %g0, %g0 */
{ "tst",	F3(2, 0x12, 0), F3(~2, ~0x12, ~0)|RD_G0|RS1_G0|ASI(~0),	"2", 0, v6 }, /* orcc %g0, rs2, %g0 */
{ "tst",	F3(2, 0x12, 1), F3(~2, ~0x12, ~1)|RD_G0|SIMM13(~0),	"1", 0, v6 }, /* orcc rs1, 0, %g0 */

{ "wr",	F3(2, 0x30, 0),		F3(~2, ~0x30, ~0)|ASI(~0),		"1,2,m", 0, v8 }, /* wr r,r,%asrX */
{ "wr",	F3(2, 0x30, 0),		F3(~2, ~0x30, ~0)|RD_G0|ASI(~0),	"1,2,y", 0, v6 }, /* wr r,r,%y */
{ "wr",	F3(2, 0x30, 1),		F3(~2, ~0x30, ~1),			"1,i,m", 0, v8 }, /* wr r,i,%asrX */
{ "wr",	F3(2, 0x30, 1),		F3(~2, ~0x30, ~1)|RD_G0,		"1,i,y", 0, v6 }, /* wr r,i,%y */
{ "wr",	F3(2, 0x31, 0),		F3(~2, ~0x31, ~0)|RD_G0|ASI(~0),	"1,2,p", F_NOTV9, v6 }, /* wr r,r,%psr */
{ "wr",	F3(2, 0x31, 1),		F3(~2, ~0x31, ~1)|RD_G0,		"1,i,p", F_NOTV9, v6 }, /* wr r,i,%psr */
{ "wr",	F3(2, 0x32, 0),		F3(~2, ~0x32, ~0)|RD_G0|ASI(~0),	"1,2,w", F_NOTV9, v6 }, /* wr r,r,%wim */
{ "wr",	F3(2, 0x32, 1),		F3(~2, ~0x32, ~1)|RD_G0,		"1,i,w", F_NOTV9, v6 }, /* wr r,i,%wim */
{ "wr",	F3(2, 0x33, 0),		F3(~2, ~0x33, ~0)|RD_G0|ASI(~0),	"1,2,t", F_NOTV9, v6 }, /* wr r,r,%tbr */
{ "wr",	F3(2, 0x33, 1),		F3(~2, ~0x33, ~1)|RD_G0,		"1,i,t", F_NOTV9, v6 }, /* wr r,i,%tbr */

{ "wr", F3(2, 0x30, 0)|RD(2),	F3(~2, ~0x30, ~0)|RD(~2)|ASI(~0),	"1,2,E", 0, v9 }, /* wr r,r,%ccr */
{ "wr", F3(2, 0x30, 1)|RD(2),	F3(~2, ~0x30, ~1)|RD(~2),		"1,i,E", 0, v9 }, /* wr r,i,%ccr */
{ "wr", F3(2, 0x30, 0)|RD(3),	F3(~2, ~0x30, ~0)|RD(~3)|ASI(~0),	"1,2,o", 0, v9 }, /* wr r,r,%asi */
{ "wr", F3(2, 0x30, 1)|RD(3),	F3(~2, ~0x30, ~1)|RD(~3),		"1,i,o", 0, v9 }, /* wr r,i,%asi */
{ "wr", F3(2, 0x30, 0)|RD(6),	F3(~2, ~0x30, ~0)|RD(~6)|ASI(~0),	"1,2,s", 0, v9 }, /* wr r,i,%fprs */
{ "wr", F3(2, 0x30, 1)|RD(6),	F3(~2, ~0x30, ~1)|RD(~6),		"1,i,s", 0, v9 }, /* wr r,i,%fprs */

{ "rd",	F3(2, 0x28, 0),			F3(~2, ~0x28, ~0)|SIMM13(~0),		"M,d", 0, v8 }, /* rd %asrX,r */
{ "rd",	F3(2, 0x28, 0),			F3(~2, ~0x28, ~0)|RS1_G0|SIMM13(~0),	"y,d", 0, v6 }, /* rd %y,r */
{ "rd",	F3(2, 0x29, 0),			F3(~2, ~0x29, ~0)|RS1_G0|SIMM13(~0),	"p,d", F_NOTV9, v6 }, /* rd %psr,r */
{ "rd",	F3(2, 0x2a, 0),			F3(~2, ~0x2a, ~0)|RS1_G0|SIMM13(~0),	"w,d", F_NOTV9, v6 }, /* rd %wim,r */
{ "rd",	F3(2, 0x2b, 0),			F3(~2, ~0x2b, ~0)|RS1_G0|SIMM13(~0),	"t,d", F_NOTV9, v6 }, /* rd %tbr,r */

{ "rd",	F3(2, 0x28, 0)|RS1(2),		F3(~2, ~0x28, ~0)|RS1(~2)|SIMM13(~0),	"E,d", 0, v9 }, /* rd %ccr,r */
{ "rd",	F3(2, 0x28, 0)|RS1(3),		F3(~2, ~0x28, ~0)|RS1(~3)|SIMM13(~0),	"o,d", 0, v9 }, /* rd %asi,r */
{ "rd",	F3(2, 0x28, 0)|RS1(4),		F3(~2, ~0x28, ~0)|RS1(~4)|SIMM13(~0),	"W,d", 0, v9 }, /* rd %tick,r */
{ "rd",	F3(2, 0x28, 0)|RS1(5),		F3(~2, ~0x28, ~0)|RS1(~5)|SIMM13(~0),	"P,d", 0, v9 }, /* rd %pc,r */
{ "rd",	F3(2, 0x28, 0)|RS1(6),		F3(~2, ~0x28, ~0)|RS1(~6)|SIMM13(~0),	"s,d", 0, v9 }, /* rd %fprs,r */

{ "rdpr",	F3(2, 0x2a, 0),		F3(~2, ~0x2a, ~0)|SIMM13(~0),	"?,d", 0, v9 },   /* rdpr %priv,r */
{ "wrpr",	F3(2, 0x32, 0),		F3(~2, ~0x32, ~0),		"1,2,!", 0, v9 }, /* wrpr r1,r2,%priv */
{ "wrpr",	F3(2, 0x32, 0),		F3(~2, ~0x32, ~0)|SIMM13(~0),	"1,!", 0, v9 },   /* wrpr r1,%priv */
{ "wrpr",	F3(2, 0x32, 1),		F3(~2, ~0x32, ~1),		"1,i,!", 0, v9 }, /* wrpr r1,i,%priv */
{ "wrpr",	F3(2, 0x32, 1),		F3(~2, ~0x32, ~1),		"i,1,!", F_ALIAS, v9 }, /* wrpr i,r1,%priv */
{ "wrpr",	F3(2, 0x32, 1),		F3(~2, ~0x32, ~1)|RS1(~0),	"i,!", 0, v9 },   /* wrpr i,%priv */

{ "mov",	F3(2, 0x30, 0),		F3(~2, ~0x30, ~0)|ASI(~0),		"1,2,m", F_ALIAS, v8 }, /* wr r,r,%asrX */
{ "mov",	F3(2, 0x30, 0),		F3(~2, ~0x30, ~0)|RD_G0|ASI(~0),	"1,2,y", F_ALIAS, v6 }, /* wr r,r,%y */
{ "mov",	F3(2, 0x30, 1),		F3(~2, ~0x30, ~1),			"1,i,m", F_ALIAS, v8 }, /* wr r,i,%asrX */
{ "mov",	F3(2, 0x30, 1),		F3(~2, ~0x30, ~1)|RD_G0,		"1,i,y", F_ALIAS, v6 }, /* wr r,i,%y */
{ "mov",	F3(2, 0x31, 0),		F3(~2, ~0x31, ~0)|RD_G0|ASI(~0),	"1,2,p", F_ALIAS|F_NOTV9, v6 }, /* wr r,r,%psr */
{ "mov",	F3(2, 0x31, 1),		F3(~2, ~0x31, ~1)|RD_G0,		"1,i,p", F_ALIAS|F_NOTV9, v6 }, /* wr r,i,%psr */
{ "mov",	F3(2, 0x32, 0),		F3(~2, ~0x32, ~0)|RD_G0|ASI(~0),	"1,2,w", F_ALIAS|F_NOTV9, v6 }, /* wr r,r,%wim */
{ "mov",	F3(2, 0x32, 1),		F3(~2, ~0x32, ~1)|RD_G0,		"1,i,w", F_ALIAS|F_NOTV9, v6 }, /* wr r,i,%wim */
{ "mov",	F3(2, 0x33, 0),		F3(~2, ~0x33, ~0)|RD_G0|ASI(~0),	"1,2,t", F_ALIAS|F_NOTV9, v6 }, /* wr r,r,%tbr */
{ "mov",	F3(2, 0x33, 1),		F3(~2, ~0x33, ~1)|RD_G0,		"1,i,t", F_ALIAS|F_NOTV9, v6 }, /* wr r,i,%tbr */

{ "mov",	F3(2, 0x28, 0),		 F3(~2, ~0x28, ~0)|SIMM13(~0),		"M,d", F_ALIAS, v8 }, /* rd %asr1,r */
{ "mov",	F3(2, 0x28, 0),		 F3(~2, ~0x28, ~0)|RS1_G0|SIMM13(~0),	"y,d", F_ALIAS, v6 }, /* rd %y,r */
{ "mov",	F3(2, 0x29, 0),		 F3(~2, ~0x29, ~0)|RS1_G0|SIMM13(~0),	"p,d", F_ALIAS|F_NOTV9, v6 }, /* rd %psr,r */
{ "mov",	F3(2, 0x2a, 0),		 F3(~2, ~0x2a, ~0)|RS1_G0|SIMM13(~0),	"w,d", F_ALIAS|F_NOTV9, v6 }, /* rd %wim,r */
{ "mov",	F3(2, 0x2b, 0),		 F3(~2, ~0x2b, ~0)|RS1_G0|SIMM13(~0),	"t,d", F_ALIAS|F_NOTV9, v6 }, /* rd %tbr,r */

{ "mov",	F3(2, 0x30, 0), F3(~2, ~0x30, ~0)|ASI_RS2(~0),	"1,y", F_ALIAS, v6 }, /* wr rs1,%g0,%y */
{ "mov",	F3(2, 0x30, 1), F3(~2, ~0x30, ~1),		"i,y", F_ALIAS, v6 },
{ "mov",	F3(2, 0x30, 1), F3(~2, ~0x30, ~1)|SIMM13(~0),	"1,y", F_ALIAS, v6 }, /* wr rs1,0,%y */
{ "mov",	F3(2, 0x31, 0), F3(~2, ~0x31, ~0)|ASI_RS2(~0),	"1,p", F_ALIAS|F_NOTV9, v6 }, /* wr rs1,%g0,%psr */
{ "mov",	F3(2, 0x31, 1), F3(~2, ~0x31, ~1),		"i,p", F_ALIAS|F_NOTV9, v6 },
{ "mov",	F3(2, 0x31, 1), F3(~2, ~0x31, ~1)|SIMM13(~0),	"1,p", F_ALIAS|F_NOTV9, v6 }, /* wr rs1,0,%psr */
{ "mov",	F3(2, 0x32, 0), F3(~2, ~0x32, ~0)|ASI_RS2(~0),	"1,w", F_ALIAS|F_NOTV9, v6 }, /* wr rs1,%g0,%wim */
{ "mov",	F3(2, 0x32, 1), F3(~2, ~0x32, ~1),		"i,w", F_ALIAS|F_NOTV9, v6 },
{ "mov",	F3(2, 0x32, 1), F3(~2, ~0x32, ~1)|SIMM13(~0),	"1,w", F_ALIAS|F_NOTV9, v6 }, /* wr rs1,0,%wim */
{ "mov",	F3(2, 0x33, 0), F3(~2, ~0x33, ~0)|ASI_RS2(~0),	"1,t", F_ALIAS|F_NOTV9, v6 }, /* wr rs1,%g0,%tbr */
{ "mov",	F3(2, 0x33, 1), F3(~2, ~0x33, ~1),		"i,t", F_ALIAS|F_NOTV9, v6 },
{ "mov",	F3(2, 0x33, 1), F3(~2, ~0x33, ~1)|SIMM13(~0),	"1,t", F_ALIAS|F_NOTV9, v6 }, /* wr rs1,0,%tbr */

{ "mov",	F3(2, 0x02, 0), F3(~2, ~0x02, ~0)|RS1_G0|ASI(~0),	"2,d", 0, v6 }, /* or %g0,rs2,d */
{ "mov",	F3(2, 0x02, 1), F3(~2, ~0x02, ~1)|RS1_G0,		"i,d", 0, v6 }, /* or %g0,i,d	*/
{ "mov",        F3(2, 0x02, 0), F3(~2, ~0x02, ~0)|ASI_RS2(~0),		"1,d", 0, v6 }, /* or rs1,%g0,d   */
{ "mov",        F3(2, 0x02, 1), F3(~2, ~0x02, ~1)|SIMM13(~0),		"1,d", 0, v6 }, /* or rs1,0,d */

{ "or",	F3(2, 0x02, 0), F3(~2, ~0x02, ~0)|ASI(~0),	"1,2,d", 0, v6 },
{ "or",	F3(2, 0x02, 1), F3(~2, ~0x02, ~1),		"1,i,d", 0, v6 },
{ "or",	F3(2, 0x02, 1), F3(~2, ~0x02, ~1),		"i,1,d", 0, v6 },

{ "bset",	F3(2, 0x02, 0), F3(~2, ~0x02, ~0)|ASI(~0),	"2,r", F_ALIAS, v6 },	/* or rd,rs2,rd */
{ "bset",	F3(2, 0x02, 1), F3(~2, ~0x02, ~1),		"i,r", F_ALIAS, v6 },	/* or rd,i,rd */

/* This is not a commutative instruction.  */
{ "andn",	F3(2, 0x05, 0), F3(~2, ~0x05, ~0)|ASI(~0),	"1,2,d", 0, v6 },
{ "andn",	F3(2, 0x05, 1), F3(~2, ~0x05, ~1),		"1,i,d", 0, v6 },

/* This is not a commutative instruction.  */
{ "andncc",	F3(2, 0x15, 0), F3(~2, ~0x15, ~0)|ASI(~0),	"1,2,d", 0, v6 },
{ "andncc",	F3(2, 0x15, 1), F3(~2, ~0x15, ~1),		"1,i,d", 0, v6 },

{ "bclr",	F3(2, 0x05, 0), F3(~2, ~0x05, ~0)|ASI(~0),	"2,r", F_ALIAS, v6 },	/* andn rd,rs2,rd */
{ "bclr",	F3(2, 0x05, 1), F3(~2, ~0x05, ~1),		"i,r", F_ALIAS, v6 },	/* andn rd,i,rd */

{ "cmp",	F3(2, 0x14, 0), F3(~2, ~0x14, ~0)|RD_G0|ASI(~0),	"1,2", 0, v6 },	/* subcc rs1,rs2,%g0 */
{ "cmp",	F3(2, 0x14, 1), F3(~2, ~0x14, ~1)|RD_G0,		"1,i", 0, v6 },	/* subcc rs1,i,%g0 */

{ "sub",	F3(2, 0x04, 0), F3(~2, ~0x04, ~0)|ASI(~0),	"1,2,d", 0, v6 },
{ "sub",	F3(2, 0x04, 1), F3(~2, ~0x04, ~1),		"1,i,d", 0, v6 },

{ "subcc",	F3(2, 0x14, 0), F3(~2, ~0x14, ~0)|ASI(~0),	"1,2,d", 0, v6 },
{ "subcc",	F3(2, 0x14, 1), F3(~2, ~0x14, ~1),		"1,i,d", 0, v6 },

{ "subx",	F3(2, 0x0c, 0), F3(~2, ~0x0c, ~0)|ASI(~0),	"1,2,d", F_NOTV9, v6 },
{ "subx",	F3(2, 0x0c, 1), F3(~2, ~0x0c, ~1),		"1,i,d", F_NOTV9, v6 },
{ "subc",	F3(2, 0x0c, 0), F3(~2, ~0x0c, ~0)|ASI(~0),	"1,2,d", 0, v9 },
{ "subc",	F3(2, 0x0c, 1), F3(~2, ~0x0c, ~1),		"1,i,d", 0, v9 },

{ "subxcc",	F3(2, 0x1c, 0), F3(~2, ~0x1c, ~0)|ASI(~0),	"1,2,d", F_NOTV9, v6 },
{ "subxcc",	F3(2, 0x1c, 1), F3(~2, ~0x1c, ~1),		"1,i,d", F_NOTV9, v6 },
{ "subccc",	F3(2, 0x1c, 0), F3(~2, ~0x1c, ~0)|ASI(~0),	"1,2,d", 0, v9 },
{ "subccc",	F3(2, 0x1c, 1), F3(~2, ~0x1c, ~1),		"1,i,d", 0, v9 },

{ "and",	F3(2, 0x01, 0), F3(~2, ~0x01, ~0)|ASI(~0),	"1,2,d", 0, v6 },
{ "and",	F3(2, 0x01, 1), F3(~2, ~0x01, ~1),		"1,i,d", 0, v6 },
{ "and",	F3(2, 0x01, 1), F3(~2, ~0x01, ~1),		"i,1,d", 0, v6 },

{ "andcc",	F3(2, 0x11, 0), F3(~2, ~0x11, ~0)|ASI(~0),	"1,2,d", 0, v6 },
{ "andcc",	F3(2, 0x11, 1), F3(~2, ~0x11, ~1),		"1,i,d", 0, v6 },
{ "andcc",	F3(2, 0x11, 1), F3(~2, ~0x11, ~1),		"i,1,d", 0, v6 },

{ "dec",	F3(2, 0x04, 1)|SIMM13(0x1), F3(~2, ~0x04, ~1)|SIMM13(~0x0001), "r", F_ALIAS, v6 },	/* sub rd,1,rd */
{ "dec",	F3(2, 0x04, 1),		    F3(~2, ~0x04, ~1),		       "i,r", F_ALIAS, v8 },	/* sub rd,imm,rd */
{ "deccc",	F3(2, 0x14, 1)|SIMM13(0x1), F3(~2, ~0x14, ~1)|SIMM13(~0x0001), "r", F_ALIAS, v6 },	/* subcc rd,1,rd */
{ "deccc",	F3(2, 0x14, 1),		    F3(~2, ~0x14, ~1),		       "i,r", F_ALIAS, v8 },	/* subcc rd,imm,rd */
{ "inc",	F3(2, 0x00, 1)|SIMM13(0x1), F3(~2, ~0x00, ~1)|SIMM13(~0x0001), "r", F_ALIAS, v6 },	/* add rd,1,rd */
{ "inc",	F3(2, 0x00, 1),		    F3(~2, ~0x00, ~1),		       "i,r", F_ALIAS, v8 },	/* add rd,imm,rd */
{ "inccc",	F3(2, 0x10, 1)|SIMM13(0x1), F3(~2, ~0x10, ~1)|SIMM13(~0x0001), "r", F_ALIAS, v6 },	/* addcc rd,1,rd */
{ "inccc",	F3(2, 0x10, 1),		    F3(~2, ~0x10, ~1),		       "i,r", F_ALIAS, v8 },	/* addcc rd,imm,rd */

{ "btst",	F3(2, 0x11, 0), F3(~2, ~0x11, ~0)|RD_G0|ASI(~0), "1,2", F_ALIAS, v6 },	/* andcc rs1,rs2,%g0 */
{ "btst",	F3(2, 0x11, 1), F3(~2, ~0x11, ~1)|RD_G0, "i,1", F_ALIAS, v6 },	/* andcc rs1,i,%g0 */

{ "neg",	F3(2, 0x04, 0), F3(~2, ~0x04, ~0)|RS1_G0|ASI(~0), "2,d", F_ALIAS, v6 }, /* sub %g0,rs2,rd */
{ "neg",	F3(2, 0x04, 0), F3(~2, ~0x04, ~0)|RS1_G0|ASI(~0), "r", F_ALIAS, v6 }, /* sub %g0,rd,rd */

{ "add",	F3(2, 0x00, 0), F3(~2, ~0x00, ~0)|ASI(~0),	"1,2,d", 0, v6 },
{ "add",	F3(2, 0x00, 1), F3(~2, ~0x00, ~1),		"1,i,d", 0, v6 },
{ "add",	F3(2, 0x00, 1), F3(~2, ~0x00, ~1),		"i,1,d", 0, v6 },
{ "addcc",	F3(2, 0x10, 0), F3(~2, ~0x10, ~0)|ASI(~0),	"1,2,d", 0, v6 },
{ "addcc",	F3(2, 0x10, 1), F3(~2, ~0x10, ~1),		"1,i,d", 0, v6 },
{ "addcc",	F3(2, 0x10, 1), F3(~2, ~0x10, ~1),		"i,1,d", 0, v6 },

{ "addx",	F3(2, 0x08, 0), F3(~2, ~0x08, ~0)|ASI(~0),	"1,2,d", F_NOTV9, v6 },
{ "addx",	F3(2, 0x08, 1), F3(~2, ~0x08, ~1),		"1,i,d", F_NOTV9, v6 },
{ "addx",	F3(2, 0x08, 1), F3(~2, ~0x08, ~1),		"i,1,d", F_NOTV9, v6 },
{ "addc",	F3(2, 0x08, 0), F3(~2, ~0x08, ~0)|ASI(~0),	"1,2,d", 0, v9 },
{ "addc",	F3(2, 0x08, 1), F3(~2, ~0x08, ~1),		"1,i,d", 0, v9 },
{ "addc",	F3(2, 0x08, 1), F3(~2, ~0x08, ~1),		"i,1,d", 0, v9 },

{ "addxcc",	F3(2, 0x18, 0), F3(~2, ~0x18, ~0)|ASI(~0),	"1,2,d", F_NOTV9, v6 },
{ "addxcc",	F3(2, 0x18, 1), F3(~2, ~0x18, ~1),		"1,i,d", F_NOTV9, v6 },
{ "addxcc",	F3(2, 0x18, 1), F3(~2, ~0x18, ~1),		"i,1,d", F_NOTV9, v6 },
{ "addccc",	F3(2, 0x18, 0), F3(~2, ~0x18, ~0)|ASI(~0),	"1,2,d", 0, v9 },
{ "addccc",	F3(2, 0x18, 1), F3(~2, ~0x18, ~1),		"1,i,d", 0, v9 },
{ "addccc",	F3(2, 0x18, 1), F3(~2, ~0x18, ~1),		"i,1,d", 0, v9 },

{ "smul",	F3(2, 0x0b, 0), F3(~2, ~0x0b, ~0)|ASI(~0),	"1,2,d", 0, v8 },
{ "smul",	F3(2, 0x0b, 1), F3(~2, ~0x0b, ~1),		"1,i,d", 0, v8 },
{ "smul",	F3(2, 0x0b, 1), F3(~2, ~0x0b, ~1),		"i,1,d", 0, v8 },
{ "smulcc",	F3(2, 0x1b, 0), F3(~2, ~0x1b, ~0)|ASI(~0),	"1,2,d", 0, v8 },
{ "smulcc",	F3(2, 0x1b, 1), F3(~2, ~0x1b, ~1),		"1,i,d", 0, v8 },
{ "smulcc",	F3(2, 0x1b, 1), F3(~2, ~0x1b, ~1),		"i,1,d", 0, v8 },
{ "umul",	F3(2, 0x0a, 0), F3(~2, ~0x0a, ~0)|ASI(~0),	"1,2,d", 0, v8 },
{ "umul",	F3(2, 0x0a, 1), F3(~2, ~0x0a, ~1),		"1,i,d", 0, v8 },
{ "umul",	F3(2, 0x0a, 1), F3(~2, ~0x0a, ~1),		"i,1,d", 0, v8 },
{ "umulcc",	F3(2, 0x1a, 0), F3(~2, ~0x1a, ~0)|ASI(~0),	"1,2,d", 0, v8 },
{ "umulcc",	F3(2, 0x1a, 1), F3(~2, ~0x1a, ~1),		"1,i,d", 0, v8 },
{ "umulcc",	F3(2, 0x1a, 1), F3(~2, ~0x1a, ~1),		"i,1,d", 0, v8 },
{ "sdiv",	F3(2, 0x0f, 0), F3(~2, ~0x0f, ~0)|ASI(~0),	"1,2,d", 0, v8 },
{ "sdiv",	F3(2, 0x0f, 1), F3(~2, ~0x0f, ~1),		"1,i,d", 0, v8 },
{ "sdiv",	F3(2, 0x0f, 1), F3(~2, ~0x0f, ~1),		"i,1,d", 0, v8 },
{ "sdivcc",	F3(2, 0x1f, 0), F3(~2, ~0x1f, ~0)|ASI(~0),	"1,2,d", 0, v8 },
{ "sdivcc",	F3(2, 0x1f, 1), F3(~2, ~0x1f, ~1),		"1,i,d", 0, v8 },
{ "sdivcc",	F3(2, 0x1f, 1), F3(~2, ~0x1f, ~1),		"i,1,d", 0, v8 },
{ "udiv",	F3(2, 0x0e, 0), F3(~2, ~0x0e, ~0)|ASI(~0),	"1,2,d", 0, v8 },
{ "udiv",	F3(2, 0x0e, 1), F3(~2, ~0x0e, ~1),		"1,i,d", 0, v8 },
{ "udiv",	F3(2, 0x0e, 1), F3(~2, ~0x0e, ~1),		"i,1,d", 0, v8 },
{ "udivcc",	F3(2, 0x1e, 0), F3(~2, ~0x1e, ~0)|ASI(~0),	"1,2,d", 0, v8 },
{ "udivcc",	F3(2, 0x1e, 1), F3(~2, ~0x1e, ~1),		"1,i,d", 0, v8 },
{ "udivcc",	F3(2, 0x1e, 1), F3(~2, ~0x1e, ~1),		"i,1,d", 0, v8 },

{ "mulx",	F3(2, 0x09, 0), F3(~2, ~0x09, ~0)|ASI(~0),	"1,2,d", 0, v9 },
{ "mulx",	F3(2, 0x09, 1), F3(~2, ~0x09, ~1),		"1,i,d", 0, v9 },
{ "sdivx",	F3(2, 0x2d, 0), F3(~2, ~0x2d, ~0)|ASI(~0),	"1,2,d", 0, v9 },
{ "sdivx",	F3(2, 0x2d, 1), F3(~2, ~0x2d, ~1),		"1,i,d", 0, v9 },
{ "udivx",	F3(2, 0x0d, 0), F3(~2, ~0x0d, ~0)|ASI(~0),	"1,2,d", 0, v9 },
{ "udivx",	F3(2, 0x0d, 1), F3(~2, ~0x0d, ~1),		"1,i,d", 0, v9 },

{ "call",	F1(0x1), F1(~0x1), "L", F_JSR|F_DELAYED, v6 },
{ "call",	F1(0x1), F1(~0x1), "L,#", F_JSR|F_DELAYED, v6 },

{ "call",	F3(2, 0x38, 0)|RD(0xf), F3(~2, ~0x38, ~0)|RD(~0xf)|ASI(~0),	"1+2", F_JSR|F_DELAYED, v6 }, /* jmpl rs1+rs2,%o7 */
{ "call",	F3(2, 0x38, 0)|RD(0xf), F3(~2, ~0x38, ~0)|RD(~0xf)|ASI(~0),	"1+2,#", F_JSR|F_DELAYED, v6 },
{ "call",	F3(2, 0x38, 0)|RD(0xf), F3(~2, ~0x38, ~0)|RD(~0xf)|ASI_RS2(~0),	"1", F_JSR|F_DELAYED, v6 }, /* jmpl rs1+%g0,%o7 */
{ "call",	F3(2, 0x38, 0)|RD(0xf), F3(~2, ~0x38, ~0)|RD(~0xf)|ASI_RS2(~0),	"1,#", F_JSR|F_DELAYED, v6 },
{ "call",	F3(2, 0x38, 1)|RD(0xf), F3(~2, ~0x38, ~1)|RD(~0xf),		"1+i", F_JSR|F_DELAYED, v6 }, /* jmpl rs1+i,%o7 */
{ "call",	F3(2, 0x38, 1)|RD(0xf), F3(~2, ~0x38, ~1)|RD(~0xf),		"1+i,#", F_JSR|F_DELAYED, v6 },
{ "call",	F3(2, 0x38, 1)|RD(0xf), F3(~2, ~0x38, ~1)|RD(~0xf),		"i+1", F_JSR|F_DELAYED, v6 }, /* jmpl i+rs1,%o7 */
{ "call",	F3(2, 0x38, 1)|RD(0xf), F3(~2, ~0x38, ~1)|RD(~0xf),		"i+1,#", F_JSR|F_DELAYED, v6 },
{ "call",	F3(2, 0x38, 1)|RD(0xf), F3(~2, ~0x38, ~1)|RD(~0xf)|RS1_G0,	"i", F_JSR|F_DELAYED, v6 }, /* jmpl %g0+i,%o7 */
{ "call",	F3(2, 0x38, 1)|RD(0xf), F3(~2, ~0x38, ~1)|RD(~0xf)|RS1_G0,	"i,#", F_JSR|F_DELAYED, v6 },
{ "call",	F3(2, 0x38, 1)|RD(0xf), F3(~2, ~0x38, ~1)|RD(~0xf)|SIMM13(~0),	"1", F_JSR|F_DELAYED, v6 }, /* jmpl rs1+0,%o7 */
{ "call",	F3(2, 0x38, 1)|RD(0xf), F3(~2, ~0x38, ~1)|RD(~0xf)|SIMM13(~0),	"1,#", F_JSR|F_DELAYED, v6 },


/* Conditional instructions.

   Because this part of the table was such a mess earlier, I have
   macrofied it so that all the branches and traps are generated from
   a single-line description of each condition value.  John Gilmore. */

/* Define branches -- one annulled, one without, etc. */
#define br(opcode, mask, lose, flags) \
 { opcode, (mask)|ANNUL, (lose),       ",a l",   (flags), v6 }, \
 { opcode, (mask)      , (lose)|ANNUL, "l",     (flags), v6 }

#define brx(opcode, mask, lose, flags) /* v9 */ \
 { opcode, (mask)|(2<<20)|BPRED, ANNUL|(lose), "Z,G",      (flags), v9 }, \
 { opcode, (mask)|(2<<20)|BPRED, ANNUL|(lose), ",T Z,G",   (flags), v9 }, \
 { opcode, (mask)|(2<<20)|BPRED|ANNUL, (lose), ",a Z,G",   (flags), v9 }, \
 { opcode, (mask)|(2<<20)|BPRED|ANNUL, (lose), ",a,T Z,G", (flags), v9 }, \
 { opcode, (mask)|(2<<20), ANNUL|BPRED|(lose), ",N Z,G",   (flags), v9 }, \
 { opcode, (mask)|(2<<20)|ANNUL, BPRED|(lose), ",a,N Z,G", (flags), v9 }, \
 { opcode, (mask)|BPRED, ANNUL|(lose)|(2<<20), "z,G",      (flags), v9 }, \
 { opcode, (mask)|BPRED, ANNUL|(lose)|(2<<20), ",T z,G",   (flags), v9 }, \
 { opcode, (mask)|BPRED|ANNUL, (lose)|(2<<20), ",a z,G",   (flags), v9 }, \
 { opcode, (mask)|BPRED|ANNUL, (lose)|(2<<20), ",a,T z,G", (flags), v9 }, \
 { opcode, (mask), ANNUL|BPRED|(lose)|(2<<20), ",N z,G",   (flags), v9 }, \
 { opcode, (mask)|ANNUL, BPRED|(lose)|(2<<20), ",a,N z,G", (flags), v9 }

/* Define four traps: reg+reg, reg + immediate, immediate alone, reg alone. */
#define tr(opcode, mask, lose, flags) \
 { opcode, (mask)|(2<<11)|IMMED, (lose)|RS1_G0,	"Z,i",   (flags), v9 }, /* %g0 + imm */ \
 { opcode, (mask)|(2<<11)|IMMED, (lose),	"Z,1+i", (flags), v9 }, /* rs1 + imm */ \
 { opcode, (mask)|(2<<11), IMMED|(lose),	"Z,1+2", (flags), v9 }, /* rs1 + rs2 */ \
 { opcode, (mask)|(2<<11), IMMED|(lose)|RS2_G0,	"Z,1",   (flags), v9 }, /* rs1 + %g0 */ \
 { opcode, (mask)|IMMED, (lose)|RS1_G0,	"z,i",   (flags)|F_ALIAS, v9 }, /* %g0 + imm */ \
 { opcode, (mask)|IMMED, (lose),	"z,1+i", (flags)|F_ALIAS, v9 }, /* rs1 + imm */ \
 { opcode, (mask), IMMED|(lose),	"z,1+2", (flags)|F_ALIAS, v9 }, /* rs1 + rs2 */ \
 { opcode, (mask), IMMED|(lose)|RS2_G0,	"z,1",   (flags)|F_ALIAS, v9 }, /* rs1 + %g0 */ \
 { opcode, (mask)|IMMED, (lose)|RS1_G0,		"i",     (flags), v6 }, /* %g0 + imm */ \
 { opcode, (mask)|IMMED, (lose),		"1+i",   (flags), v6 }, /* rs1 + imm */ \
 { opcode, (mask), IMMED|(lose),		"1+2",   (flags), v6 }, /* rs1 + rs2 */ \
 { opcode, (mask), IMMED|(lose)|RS2_G0,		"1",     (flags), v6 } /* rs1 + %g0 */

/* v9: We must put `brx' before `br', to ensure that we never match something
   v9: against an expression unless it is an expression.  Otherwise, we end
   v9: up with undefined symbol tables entries, because they get added, but
   v9: are not deleted if the pattern fails to match.  */

/* Define both branches and traps based on condition mask */
#define cond(bop, top, mask, flags) \
  brx(bop, F2(0, 1)|(mask), F2(~0, ~1)|((~mask)&COND(~0)), F_DELAYED|(flags)), /* v9 */ \
  br(bop,  F2(0, 2)|(mask), F2(~0, ~2)|((~mask)&COND(~0)), F_DELAYED|(flags)), \
  tr(top,  F3(2, 0x3a, 0)|(mask), F3(~2, ~0x3a, 0)|((~mask)&COND(~0)), ((flags) & ~(F_UNBR|F_CONDBR)))

/* Define all the conditions, all the branches, all the traps.  */

/* Standard branch, trap mnemonics */
cond ("b",	"ta",   CONDA, F_UNBR),
/* Alternative form (just for assembly, not for disassembly) */
cond ("ba",	"t",    CONDA, F_UNBR|F_ALIAS),

cond ("bcc",	"tcc",  CONDCC, F_CONDBR),
cond ("bcs",	"tcs",  CONDCS, F_CONDBR),
cond ("be",	"te",   CONDE, F_CONDBR),
cond ("bg",	"tg",   CONDG, F_CONDBR),
cond ("bgt",	"tgt",   CONDG, F_CONDBR|F_ALIAS),
cond ("bge",	"tge",  CONDGE, F_CONDBR),
cond ("bgeu",	"tgeu", CONDGEU, F_CONDBR|F_ALIAS), /* for cc */
cond ("bgu",	"tgu",  CONDGU, F_CONDBR),
cond ("bl",	"tl",   CONDL, F_CONDBR),
cond ("blt",	"tlt",   CONDL, F_CONDBR|F_ALIAS),
cond ("ble",	"tle",  CONDLE, F_CONDBR),
cond ("bleu",	"tleu", CONDLEU, F_CONDBR),
cond ("blu",	"tlu",  CONDLU, F_CONDBR|F_ALIAS), /* for cs */
cond ("bn",	"tn",   CONDN, F_CONDBR),
cond ("bne",	"tne",  CONDNE, F_CONDBR),
cond ("bneg",	"tneg", CONDNEG, F_CONDBR),
cond ("bnz",	"tnz",  CONDNZ, F_CONDBR|F_ALIAS), /* for ne */
cond ("bpos",	"tpos", CONDPOS, F_CONDBR),
cond ("bvc",	"tvc",  CONDVC, F_CONDBR),
cond ("bvs",	"tvs",  CONDVS, F_CONDBR),
cond ("bz",	"tz",   CONDZ, F_CONDBR|F_ALIAS), /* for e */

#undef cond
#undef br
#undef brr /* v9 */
#undef tr

#define brr(opcode, mask, lose, flags) /* v9 */ \
 { opcode, (mask)|BPRED, ANNUL|(lose), "1,k",      F_DELAYED|(flags), v9 }, \
 { opcode, (mask)|BPRED, ANNUL|(lose), ",T 1,k",   F_DELAYED|(flags), v9 }, \
 { opcode, (mask)|BPRED|ANNUL, (lose), ",a 1,k",   F_DELAYED|(flags), v9 }, \
 { opcode, (mask)|BPRED|ANNUL, (lose), ",a,T 1,k", F_DELAYED|(flags), v9 }, \
 { opcode, (mask), ANNUL|BPRED|(lose), ",N 1,k",   F_DELAYED|(flags), v9 }, \
 { opcode, (mask)|ANNUL, BPRED|(lose), ",a,N 1,k", F_DELAYED|(flags), v9 }

#define condr(bop, mask, flags) /* v9 */ \
  brr(bop, F2(0, 3)|COND(mask), F2(~0, ~3)|COND(~(mask)), (flags)) /* v9 */

/* v9 */ condr("brnz", 0x5, F_CONDBR),
/* v9 */ condr("brz", 0x1, F_CONDBR),
/* v9 */ condr("brgez", 0x7, F_CONDBR),
/* v9 */ condr("brlz", 0x3, F_CONDBR),
/* v9 */ condr("brlez", 0x2, F_CONDBR),
/* v9 */ condr("brgz", 0x6, F_CONDBR),

#undef condr /* v9 */
#undef brr /* v9 */

#define movr(opcode, mask, flags) /* v9 */ \
 { opcode, F3(2, 0x2f, 0)|RCOND(mask), F3(~2, ~0x2f, ~0)|RCOND(~(mask)), "1,2,d", (flags), v9 }, \
 { opcode, F3(2, 0x2f, 1)|RCOND(mask), F3(~2, ~0x2f, ~1)|RCOND(~(mask)), "1,j,d", (flags), v9 }

#define fmrrs(opcode, mask, lose, flags) /* v9 */ \
 { opcode, (mask), (lose), "1,f,g", (flags), v9 }
#define fmrrd(opcode, mask, lose, flags) /* v9 */ \
 { opcode, (mask), (lose), "1,B,H", (flags), v9 }
#define fmrrq(opcode, mask, lose, flags) /* v9 */ \
 { opcode, (mask), (lose), "1,R,J", (flags), v9 }

#define fmovrs(mop, mask, flags) /* v9 */ \
  fmrrs(mop, F3(2, 0x35, 0)|OPF_LOW5(5)|RCOND(mask), F3(~2, ~0x35, 0)|OPF_LOW5(~5)|RCOND(~(mask)), (flags)) /* v9 */
#define fmovrd(mop, mask, flags) /* v9 */ \
  fmrrd(mop, F3(2, 0x35, 0)|OPF_LOW5(6)|RCOND(mask), F3(~2, ~0x35, 0)|OPF_LOW5(~6)|RCOND(~(mask)), (flags)) /* v9 */
#define fmovrq(mop, mask, flags) /* v9 */ \
  fmrrq(mop, F3(2, 0x35, 0)|OPF_LOW5(7)|RCOND(mask), F3(~2, ~0x35, 0)|OPF_LOW5(~7)|RCOND(~(mask)), (flags)) /* v9 */

/* v9 */ movr("movrne", 0x5, 0),
/* v9 */ movr("movre", 0x1, 0),
/* v9 */ movr("movrgez", 0x7, 0),
/* v9 */ movr("movrlz", 0x3, 0),
/* v9 */ movr("movrlez", 0x2, 0),
/* v9 */ movr("movrgz", 0x6, 0),
/* v9 */ movr("movrnz", 0x5, F_ALIAS),
/* v9 */ movr("movrz", 0x1, F_ALIAS),

/* v9 */ fmovrs("fmovrsne", 0x5, 0),
/* v9 */ fmovrs("fmovrse", 0x1, 0),
/* v9 */ fmovrs("fmovrsgez", 0x7, 0),
/* v9 */ fmovrs("fmovrslz", 0x3, 0),
/* v9 */ fmovrs("fmovrslez", 0x2, 0),
/* v9 */ fmovrs("fmovrsgz", 0x6, 0),
/* v9 */ fmovrs("fmovrsnz", 0x5, F_ALIAS),
/* v9 */ fmovrs("fmovrsz", 0x1, F_ALIAS),

/* v9 */ fmovrd("fmovrdne", 0x5, 0),
/* v9 */ fmovrd("fmovrde", 0x1, 0),
/* v9 */ fmovrd("fmovrdgez", 0x7, 0),
/* v9 */ fmovrd("fmovrdlz", 0x3, 0),
/* v9 */ fmovrd("fmovrdlez", 0x2, 0),
/* v9 */ fmovrd("fmovrdgz", 0x6, 0),
/* v9 */ fmovrd("fmovrdnz", 0x5, F_ALIAS),
/* v9 */ fmovrd("fmovrdz", 0x1, F_ALIAS),

/* v9 */ fmovrq("fmovrqne", 0x5, 0),
/* v9 */ fmovrq("fmovrqe", 0x1, 0),
/* v9 */ fmovrq("fmovrqgez", 0x7, 0),
/* v9 */ fmovrq("fmovrqlz", 0x3, 0),
/* v9 */ fmovrq("fmovrqlez", 0x2, 0),
/* v9 */ fmovrq("fmovrqgz", 0x6, 0),
/* v9 */ fmovrq("fmovrqnz", 0x5, F_ALIAS),
/* v9 */ fmovrq("fmovrqz", 0x1, F_ALIAS),

#undef movr /* v9 */
#undef fmovr /* v9 */
#undef fmrr /* v9 */

#define movicc(opcode, cond, flags) /* v9 */ \
  { opcode, F3(2, 0x2c, 0)|MCOND(cond,1)|ICC, F3(~2, ~0x2c, ~0)|MCOND(~cond,~1)|XCC|(1<<11), "z,2,d", flags, v9 }, \
  { opcode, F3(2, 0x2c, 1)|MCOND(cond,1)|ICC, F3(~2, ~0x2c, ~1)|MCOND(~cond,~1)|XCC|(1<<11), "z,I,d", flags, v9 }, \
  { opcode, F3(2, 0x2c, 0)|MCOND(cond,1)|XCC, F3(~2, ~0x2c, ~0)|MCOND(~cond,~1)|(1<<11),     "Z,2,d", flags, v9 }, \
  { opcode, F3(2, 0x2c, 1)|MCOND(cond,1)|XCC, F3(~2, ~0x2c, ~1)|MCOND(~cond,~1)|(1<<11),     "Z,I,d", flags, v9 }

#define movfcc(opcode, fcond, flags) /* v9 */ \
  { opcode, F3(2, 0x2c, 0)|FCC(0)|MCOND(fcond,0), MCOND(~fcond,~0)|FCC(~0)|F3(~2, ~0x2c, ~0), "6,2,d", flags, v9 }, \
  { opcode, F3(2, 0x2c, 1)|FCC(0)|MCOND(fcond,0), MCOND(~fcond,~0)|FCC(~0)|F3(~2, ~0x2c, ~1), "6,I,d", flags, v9 }, \
  { opcode, F3(2, 0x2c, 0)|FCC(1)|MCOND(fcond,0), MCOND(~fcond,~0)|FCC(~1)|F3(~2, ~0x2c, ~0), "7,2,d", flags, v9 }, \
  { opcode, F3(2, 0x2c, 1)|FCC(1)|MCOND(fcond,0), MCOND(~fcond,~0)|FCC(~1)|F3(~2, ~0x2c, ~1), "7,I,d", flags, v9 }, \
  { opcode, F3(2, 0x2c, 0)|FCC(2)|MCOND(fcond,0), MCOND(~fcond,~0)|FCC(~2)|F3(~2, ~0x2c, ~0), "8,2,d", flags, v9 }, \
  { opcode, F3(2, 0x2c, 1)|FCC(2)|MCOND(fcond,0), MCOND(~fcond,~0)|FCC(~2)|F3(~2, ~0x2c, ~1), "8,I,d", flags, v9 }, \
  { opcode, F3(2, 0x2c, 0)|FCC(3)|MCOND(fcond,0), MCOND(~fcond,~0)|FCC(~3)|F3(~2, ~0x2c, ~0), "9,2,d", flags, v9 }, \
  { opcode, F3(2, 0x2c, 1)|FCC(3)|MCOND(fcond,0), MCOND(~fcond,~0)|FCC(~3)|F3(~2, ~0x2c, ~1), "9,I,d", flags, v9 }

#define movcc(opcode, cond, fcond, flags) /* v9 */ \
  movfcc (opcode, fcond, flags), /* v9 */ \
  movicc (opcode, cond, flags) /* v9 */

/* v9 */ movcc  ("mova",	CONDA, FCONDA, 0),
/* v9 */ movicc ("movcc",	CONDCC, 0),
/* v9 */ movicc ("movgeu",	CONDGEU, F_ALIAS),
/* v9 */ movicc ("movcs",	CONDCS, 0),
/* v9 */ movicc ("movlu",	CONDLU, F_ALIAS),
/* v9 */ movcc  ("move",	CONDE, FCONDE, 0),
/* v9 */ movcc  ("movg",	CONDG, FCONDG, 0),
/* v9 */ movcc  ("movge",	CONDGE, FCONDGE, 0),
/* v9 */ movicc ("movgu",	CONDGU, 0),
/* v9 */ movcc  ("movl",	CONDL, FCONDL, 0),
/* v9 */ movcc  ("movle",	CONDLE, FCONDLE, 0),
/* v9 */ movicc ("movleu",	CONDLEU, 0),
/* v9 */ movfcc ("movlg",	FCONDLG, 0),
/* v9 */ movcc  ("movn",	CONDN, FCONDN, 0),
/* v9 */ movcc  ("movne",	CONDNE, FCONDNE, 0),
/* v9 */ movicc ("movneg",	CONDNEG, 0),
/* v9 */ movcc  ("movnz",	CONDNZ, FCONDNZ, F_ALIAS),
/* v9 */ movfcc ("movo",	FCONDO, 0),
/* v9 */ movicc ("movpos",	CONDPOS, 0),
/* v9 */ movfcc ("movu",	FCONDU, 0),
/* v9 */ movfcc ("movue",	FCONDUE, 0),
/* v9 */ movfcc ("movug",	FCONDUG, 0),
/* v9 */ movfcc ("movuge",	FCONDUGE, 0),
/* v9 */ movfcc ("movul",	FCONDUL, 0),
/* v9 */ movfcc ("movule",	FCONDULE, 0),
/* v9 */ movicc ("movvc",	CONDVC, 0),
/* v9 */ movicc ("movvs",	CONDVS, 0),
/* v9 */ movcc  ("movz",	CONDZ, FCONDZ, F_ALIAS),

#undef movicc /* v9 */
#undef movfcc /* v9 */
#undef movcc /* v9 */

#define FM_SF 1		/* v9 - values for fpsize */
#define FM_DF 2		/* v9 */
#define FM_QF 3		/* v9 */

#define fmovicc(opcode, fpsize, cond, flags) /* v9 */ \
{ opcode, F3F(2, 0x35, 0x100+fpsize)|MCOND(cond,0),  F3F(~2, ~0x35, ~(0x100+fpsize))|MCOND(~cond,~0),  "z,f,g", flags, v9 }, \
{ opcode, F3F(2, 0x35, 0x180+fpsize)|MCOND(cond,0),  F3F(~2, ~0x35, ~(0x180+fpsize))|MCOND(~cond,~0),  "Z,f,g", flags, v9 }

#define fmovfcc(opcode, fpsize, fcond, flags) /* v9 */ \
{ opcode, F3F(2, 0x35, 0x000+fpsize)|MCOND(fcond,0), F3F(~2, ~0x35, ~(0x000+fpsize))|MCOND(~fcond,~0), "6,f,g", flags, v9 }, \
{ opcode, F3F(2, 0x35, 0x040+fpsize)|MCOND(fcond,0), F3F(~2, ~0x35, ~(0x040+fpsize))|MCOND(~fcond,~0), "7,f,g", flags, v9 }, \
{ opcode, F3F(2, 0x35, 0x080+fpsize)|MCOND(fcond,0), F3F(~2, ~0x35, ~(0x080+fpsize))|MCOND(~fcond,~0), "8,f,g", flags, v9 }, \
{ opcode, F3F(2, 0x35, 0x0c0+fpsize)|MCOND(fcond,0), F3F(~2, ~0x35, ~(0x0c0+fpsize))|MCOND(~fcond,~0), "9,f,g", flags, v9 }

/* FIXME: use fmovicc/fmovfcc? */ /* v9 */
#define fmovcc(opcode, fpsize, cond, fcond, flags) /* v9 */ \
{ opcode, F3F(2, 0x35, 0x100+fpsize)|MCOND(cond,0),  F3F(~2, ~0x35, ~(0x100+fpsize))|MCOND(~cond,~0),  "z,f,g", flags, v9 }, \
{ opcode, F3F(2, 0x35, 0x000+fpsize)|MCOND(fcond,0), F3F(~2, ~0x35, ~(0x000+fpsize))|MCOND(~fcond,~0), "6,f,g", flags, v9 }, \
{ opcode, F3F(2, 0x35, 0x180+fpsize)|MCOND(cond,0),  F3F(~2, ~0x35, ~(0x180+fpsize))|MCOND(~cond,~0),  "Z,f,g", flags, v9 }, \
{ opcode, F3F(2, 0x35, 0x040+fpsize)|MCOND(fcond,0), F3F(~2, ~0x35, ~(0x040+fpsize))|MCOND(~fcond,~0), "7,f,g", flags, v9 }, \
{ opcode, F3F(2, 0x35, 0x080+fpsize)|MCOND(fcond,0), F3F(~2, ~0x35, ~(0x080+fpsize))|MCOND(~fcond,~0), "8,f,g", flags, v9 }, \
{ opcode, F3F(2, 0x35, 0x0c0+fpsize)|MCOND(fcond,0), F3F(~2, ~0x35, ~(0x0c0+fpsize))|MCOND(~fcond,~0), "9,f,g", flags, v9 }

/* v9 */ fmovcc  ("fmovda",	FM_DF, CONDA, FCONDA, 0),
/* v9 */ fmovcc  ("fmovqa",	FM_QF, CONDA, FCONDA, 0),
/* v9 */ fmovcc  ("fmovsa",	FM_SF, CONDA, FCONDA, 0),
/* v9 */ fmovicc ("fmovdcc",	FM_DF, CONDCC, 0),
/* v9 */ fmovicc ("fmovqcc",	FM_QF, CONDCC, 0),
/* v9 */ fmovicc ("fmovscc",	FM_SF, CONDCC, 0),
/* v9 */ fmovicc ("fmovdcs",	FM_DF, CONDCS, 0),
/* v9 */ fmovicc ("fmovqcs",	FM_QF, CONDCS, 0),
/* v9 */ fmovicc ("fmovscs",	FM_SF, CONDCS, 0),
/* v9 */ fmovcc  ("fmovde",	FM_DF, CONDE, FCONDE, 0),
/* v9 */ fmovcc  ("fmovqe",	FM_QF, CONDE, FCONDE, 0),
/* v9 */ fmovcc  ("fmovse",	FM_SF, CONDE, FCONDE, 0),
/* v9 */ fmovcc  ("fmovdg",	FM_DF, CONDG, FCONDG, 0),
/* v9 */ fmovcc  ("fmovqg",	FM_QF, CONDG, FCONDG, 0),
/* v9 */ fmovcc  ("fmovsg",	FM_SF, CONDG, FCONDG, 0),
/* v9 */ fmovcc  ("fmovdge",	FM_DF, CONDGE, FCONDGE, 0),
/* v9 */ fmovcc  ("fmovqge",	FM_QF, CONDGE, FCONDGE, 0),
/* v9 */ fmovcc  ("fmovsge",	FM_SF, CONDGE, FCONDGE, 0),
/* v9 */ fmovicc ("fmovdgeu",	FM_DF, CONDGEU, F_ALIAS),
/* v9 */ fmovicc ("fmovqgeu",	FM_QF, CONDGEU, F_ALIAS),
/* v9 */ fmovicc ("fmovsgeu",	FM_SF, CONDGEU, F_ALIAS),
/* v9 */ fmovicc ("fmovdgu",	FM_DF, CONDGU, 0),
/* v9 */ fmovicc ("fmovqgu",	FM_QF, CONDGU, 0),
/* v9 */ fmovicc ("fmovsgu",	FM_SF, CONDGU, 0),
/* v9 */ fmovcc  ("fmovdl",	FM_DF, CONDL, FCONDL, 0),
/* v9 */ fmovcc  ("fmovql",	FM_QF, CONDL, FCONDL, 0),
/* v9 */ fmovcc  ("fmovsl",	FM_SF, CONDL, FCONDL, 0),
/* v9 */ fmovcc  ("fmovdle",	FM_DF, CONDLE, FCONDLE, 0),
/* v9 */ fmovcc  ("fmovqle",	FM_QF, CONDLE, FCONDLE, 0),
/* v9 */ fmovcc  ("fmovsle",	FM_SF, CONDLE, FCONDLE, 0),
/* v9 */ fmovicc ("fmovdleu",	FM_DF, CONDLEU, 0),
/* v9 */ fmovicc ("fmovqleu",	FM_QF, CONDLEU, 0),
/* v9 */ fmovicc ("fmovsleu",	FM_SF, CONDLEU, 0),
/* v9 */ fmovfcc ("fmovdlg",	FM_DF, FCONDLG, 0),
/* v9 */ fmovfcc ("fmovqlg",	FM_QF, FCONDLG, 0),
/* v9 */ fmovfcc ("fmovslg",	FM_SF, FCONDLG, 0),
/* v9 */ fmovicc ("fmovdlu",	FM_DF, CONDLU, F_ALIAS),
/* v9 */ fmovicc ("fmovqlu",	FM_QF, CONDLU, F_ALIAS),
/* v9 */ fmovicc ("fmovslu",	FM_SF, CONDLU, F_ALIAS),
/* v9 */ fmovcc  ("fmovdn",	FM_DF, CONDN, FCONDN, 0),
/* v9 */ fmovcc  ("fmovqn",	FM_QF, CONDN, FCONDN, 0),
/* v9 */ fmovcc  ("fmovsn",	FM_SF, CONDN, FCONDN, 0),
/* v9 */ fmovcc  ("fmovdne",	FM_DF, CONDNE, FCONDNE, 0),
/* v9 */ fmovcc  ("fmovqne",	FM_QF, CONDNE, FCONDNE, 0),
/* v9 */ fmovcc  ("fmovsne",	FM_SF, CONDNE, FCONDNE, 0),
/* v9 */ fmovicc ("fmovdneg",	FM_DF, CONDNEG, 0),
/* v9 */ fmovicc ("fmovqneg",	FM_QF, CONDNEG, 0),
/* v9 */ fmovicc ("fmovsneg",	FM_SF, CONDNEG, 0),
/* v9 */ fmovcc  ("fmovdnz",	FM_DF, CONDNZ, FCONDNZ, F_ALIAS),
/* v9 */ fmovcc  ("fmovqnz",	FM_QF, CONDNZ, FCONDNZ, F_ALIAS),
/* v9 */ fmovcc  ("fmovsnz",	FM_SF, CONDNZ, FCONDNZ, F_ALIAS),
/* v9 */ fmovfcc ("fmovdo",	FM_DF, FCONDO, 0),
/* v9 */ fmovfcc ("fmovqo",	FM_QF, FCONDO, 0),
/* v9 */ fmovfcc ("fmovso",	FM_SF, FCONDO, 0),
/* v9 */ fmovicc ("fmovdpos",	FM_DF, CONDPOS, 0),
/* v9 */ fmovicc ("fmovqpos",	FM_QF, CONDPOS, 0),
/* v9 */ fmovicc ("fmovspos",	FM_SF, CONDPOS, 0),
/* v9 */ fmovfcc ("fmovdu",	FM_DF, FCONDU, 0),
/* v9 */ fmovfcc ("fmovqu",	FM_QF, FCONDU, 0),
/* v9 */ fmovfcc ("fmovsu",	FM_SF, FCONDU, 0),
/* v9 */ fmovfcc ("fmovdue",	FM_DF, FCONDUE, 0),
/* v9 */ fmovfcc ("fmovque",	FM_QF, FCONDUE, 0),
/* v9 */ fmovfcc ("fmovsue",	FM_SF, FCONDUE, 0),
/* v9 */ fmovfcc ("fmovdug",	FM_DF, FCONDUG, 0),
/* v9 */ fmovfcc ("fmovqug",	FM_QF, FCONDUG, 0),
/* v9 */ fmovfcc ("fmovsug",	FM_SF, FCONDUG, 0),
/* v9 */ fmovfcc ("fmovduge",	FM_DF, FCONDUGE, 0),
/* v9 */ fmovfcc ("fmovquge",	FM_QF, FCONDUGE, 0),
/* v9 */ fmovfcc ("fmovsuge",	FM_SF, FCONDUGE, 0),
/* v9 */ fmovfcc ("fmovdul",	FM_DF, FCONDUL, 0),
/* v9 */ fmovfcc ("fmovqul",	FM_QF, FCONDUL, 0),
/* v9 */ fmovfcc ("fmovsul",	FM_SF, FCONDUL, 0),
/* v9 */ fmovfcc ("fmovdule",	FM_DF, FCONDULE, 0),
/* v9 */ fmovfcc ("fmovqule",	FM_QF, FCONDULE, 0),
/* v9 */ fmovfcc ("fmovsule",	FM_SF, FCONDULE, 0),
/* v9 */ fmovicc ("fmovdvc",	FM_DF, CONDVC, 0),
/* v9 */ fmovicc ("fmovqvc",	FM_QF, CONDVC, 0),
/* v9 */ fmovicc ("fmovsvc",	FM_SF, CONDVC, 0),
/* v9 */ fmovicc ("fmovdvs",	FM_DF, CONDVS, 0),
/* v9 */ fmovicc ("fmovqvs",	FM_QF, CONDVS, 0),
/* v9 */ fmovicc ("fmovsvs",	FM_SF, CONDVS, 0),
/* v9 */ fmovcc  ("fmovdz",	FM_DF, CONDZ, FCONDZ, F_ALIAS),
/* v9 */ fmovcc  ("fmovqz",	FM_QF, CONDZ, FCONDZ, F_ALIAS),
/* v9 */ fmovcc  ("fmovsz",	FM_SF, CONDZ, FCONDZ, F_ALIAS),

#undef fmovicc /* v9 */
#undef fmovfcc /* v9 */
#undef fmovcc /* v9 */
#undef FM_DF /* v9 */
#undef FM_QF /* v9 */
#undef FM_SF /* v9 */

#define brfc(opcode, mask, lose, flags) \
 { opcode, (mask), ANNUL|(lose), "l",    flags|F_DELAYED, v6 }, \
 { opcode, (mask)|ANNUL, (lose), ",a l", flags|F_DELAYED, v6 }

#define brfcx(opcode, mask, lose, flags) /* v9 */ \
 { opcode, FBFCC(0)|(mask)|BPRED, ANNUL|FBFCC(~0)|(lose), "6,G",      flags|F_DELAYED, v9 }, \
 { opcode, FBFCC(0)|(mask)|BPRED, ANNUL|FBFCC(~0)|(lose), ",T 6,G",   flags|F_DELAYED, v9 }, \
 { opcode, FBFCC(0)|(mask)|BPRED|ANNUL, FBFCC(~0)|(lose), ",a 6,G",   flags|F_DELAYED, v9 }, \
 { opcode, FBFCC(0)|(mask)|BPRED|ANNUL, FBFCC(~0)|(lose), ",a,T 6,G", flags|F_DELAYED, v9 }, \
 { opcode, FBFCC(0)|(mask), ANNUL|BPRED|FBFCC(~0)|(lose), ",N 6,G",   flags|F_DELAYED, v9 }, \
 { opcode, FBFCC(0)|(mask)|ANNUL, BPRED|FBFCC(~0)|(lose), ",a,N 6,G", flags|F_DELAYED, v9 }, \
 { opcode, FBFCC(1)|(mask)|BPRED, ANNUL|FBFCC(~1)|(lose), "7,G",      flags|F_DELAYED, v9 }, \
 { opcode, FBFCC(1)|(mask)|BPRED, ANNUL|FBFCC(~1)|(lose), ",T 7,G",   flags|F_DELAYED, v9 }, \
 { opcode, FBFCC(1)|(mask)|BPRED|ANNUL, FBFCC(~1)|(lose), ",a 7,G",   flags|F_DELAYED, v9 }, \
 { opcode, FBFCC(1)|(mask)|BPRED|ANNUL, FBFCC(~1)|(lose), ",a,T 7,G", flags|F_DELAYED, v9 }, \
 { opcode, FBFCC(1)|(mask), ANNUL|BPRED|FBFCC(~1)|(lose), ",N 7,G",   flags|F_DELAYED, v9 }, \
 { opcode, FBFCC(1)|(mask)|ANNUL, BPRED|FBFCC(~1)|(lose), ",a,N 7,G", flags|F_DELAYED, v9 }, \
 { opcode, FBFCC(2)|(mask)|BPRED, ANNUL|FBFCC(~2)|(lose), "8,G",      flags|F_DELAYED, v9 }, \
 { opcode, FBFCC(2)|(mask)|BPRED, ANNUL|FBFCC(~2)|(lose), ",T 8,G",   flags|F_DELAYED, v9 }, \
 { opcode, FBFCC(2)|(mask)|BPRED|ANNUL, FBFCC(~2)|(lose), ",a 8,G",   flags|F_DELAYED, v9 }, \
 { opcode, FBFCC(2)|(mask)|BPRED|ANNUL, FBFCC(~2)|(lose), ",a,T 8,G", flags|F_DELAYED, v9 }, \
 { opcode, FBFCC(2)|(mask), ANNUL|BPRED|FBFCC(~2)|(lose), ",N 8,G",   flags|F_DELAYED, v9 }, \
 { opcode, FBFCC(2)|(mask)|ANNUL, BPRED|FBFCC(~2)|(lose), ",a,N 8,G", flags|F_DELAYED, v9 }, \
 { opcode, FBFCC(3)|(mask)|BPRED, ANNUL|FBFCC(~3)|(lose), "9,G",      flags|F_DELAYED, v9 }, \
 { opcode, FBFCC(3)|(mask)|BPRED, ANNUL|FBFCC(~3)|(lose), ",T 9,G",   flags|F_DELAYED, v9 }, \
 { opcode, FBFCC(3)|(mask)|BPRED|ANNUL, FBFCC(~3)|(lose), ",a 9,G",   flags|F_DELAYED, v9 }, \
 { opcode, FBFCC(3)|(mask)|BPRED|ANNUL, FBFCC(~3)|(lose), ",a,T 9,G", flags|F_DELAYED, v9 }, \
 { opcode, FBFCC(3)|(mask), ANNUL|BPRED|FBFCC(~3)|(lose), ",N 9,G",   flags|F_DELAYED, v9 }, \
 { opcode, FBFCC(3)|(mask)|ANNUL, BPRED|FBFCC(~3)|(lose), ",a,N  9,G", flags|F_DELAYED, v9 }

/* v9: We must put `brfcx' before `brfc', to ensure that we never match
   v9: something against an expression unless it is an expression.  Otherwise,
   v9: we end up with undefined symbol tables entries, because they get added,
   v9: but are not deleted if the pattern fails to match.  */

#define condfc(fop, cop, mask, flags) \
  brfcx(fop, F2(0, 5)|COND(mask), F2(~0, ~5)|COND(~(mask)), flags), /* v9 */ \
  brfc(fop, F2(0, 6)|COND(mask), F2(~0, ~6)|COND(~(mask)), flags), \
  brfc(cop, F2(0, 7)|COND(mask), F2(~0, ~7)|COND(~(mask)), flags)

#define condf(fop, mask, flags) \
  brfcx(fop, F2(0, 5)|COND(mask), F2(~0, ~5)|COND(~(mask)), flags), /* v9 */ \
  brfc(fop, F2(0, 6)|COND(mask), F2(~0, ~6)|COND(~(mask)), flags)

condfc("fb",	"cb",	 0x8, 0),
condfc("fba",	"cba",	 0x8, F_ALIAS),
condfc("fbe",	"cb0",	 0x9, 0),
condf("fbz",		 0x9, F_ALIAS),
condfc("fbg",	"cb2",	 0x6, 0),
condfc("fbge",	"cb02",	 0xb, 0),
condfc("fbl",	"cb1",	 0x4, 0),
condfc("fble",	"cb01",	 0xd, 0),
condfc("fblg",	"cb12",	 0x2, 0),
condfc("fbn",	"cbn",	 0x0, 0),
condfc("fbne",	"cb123", 0x1, 0),
condf("fbnz",		 0x1, F_ALIAS),
condfc("fbo",	"cb012", 0xf, 0),
condfc("fbu",	"cb3",	 0x7, 0),
condfc("fbue",	"cb03",	 0xa, 0),
condfc("fbug",	"cb23",	 0x5, 0),
condfc("fbuge",	"cb023", 0xc, 0),
condfc("fbul",	"cb13",	 0x3, 0),
condfc("fbule",	"cb013", 0xe, 0),

#undef condfc
#undef brfc
#undef brfcx	/* v9 */

{ "jmp",	F3(2, 0x38, 0), F3(~2, ~0x38, ~0)|RD_G0|ASI(~0),	"1+2", F_UNBR|F_DELAYED, v6 }, /* jmpl rs1+rs2,%g0 */
{ "jmp",	F3(2, 0x38, 0), F3(~2, ~0x38, ~0)|RD_G0|ASI_RS2(~0),	"1", F_UNBR|F_DELAYED, v6 }, /* jmpl rs1+%g0,%g0 */
{ "jmp",	F3(2, 0x38, 1), F3(~2, ~0x38, ~1)|RD_G0,		"1+i", F_UNBR|F_DELAYED, v6 }, /* jmpl rs1+i,%g0 */
{ "jmp",	F3(2, 0x38, 1), F3(~2, ~0x38, ~1)|RD_G0,		"i+1", F_UNBR|F_DELAYED, v6 }, /* jmpl i+rs1,%g0 */
{ "jmp",	F3(2, 0x38, 1), F3(~2, ~0x38, ~1)|RD_G0|RS1_G0,		"i", F_UNBR|F_DELAYED, v6 }, /* jmpl %g0+i,%g0 */
{ "jmp",	F3(2, 0x38, 1), F3(~2, ~0x38, ~1)|RD_G0|SIMM13(~0),	"1", F_UNBR|F_DELAYED, v6 }, /* jmpl rs1+0,%g0 */

{ "nop",	F2(0, 4), 0xfeffffff, "", 0, v6 }, /* sethi 0, %g0 */

{ "set",	F2(0x0, 0x4), F2(~0x0, ~0x4), "Sh,d", F_ALIAS, v6 },

{ "sethi",	F2(0x0, 0x4), F2(~0x0, ~0x4), "h,d", 0, v6 },

{ "taddcc",	F3(2, 0x20, 0), F3(~2, ~0x20, ~0)|ASI(~0),	"1,2,d", 0, v6 },
{ "taddcc",	F3(2, 0x20, 1), F3(~2, ~0x20, ~1),		"1,i,d", 0, v6 },
{ "taddcc",	F3(2, 0x20, 1), F3(~2, ~0x20, ~1),		"i,1,d", 0, v6 },
{ "taddcctv",	F3(2, 0x22, 0), F3(~2, ~0x22, ~0)|ASI(~0),	"1,2,d", 0, v6 },
{ "taddcctv",	F3(2, 0x22, 1), F3(~2, ~0x22, ~1),		"1,i,d", 0, v6 },
{ "taddcctv",	F3(2, 0x22, 1), F3(~2, ~0x22, ~1),		"i,1,d", 0, v6 },

{ "tsubcc",	F3(2, 0x21, 0), F3(~2, ~0x21, ~0)|ASI(~0),	"1,2,d", 0, v6 },
{ "tsubcc",	F3(2, 0x21, 1), F3(~2, ~0x21, ~1),		"1,i,d", 0, v6 },
{ "tsubcctv",	F3(2, 0x23, 0), F3(~2, ~0x23, ~0)|ASI(~0),	"1,2,d", 0, v6 },
{ "tsubcctv",	F3(2, 0x23, 1), F3(~2, ~0x23, ~1),		"1,i,d", 0, v6 },

{ "unimp",	F2(0x0, 0x0), 0xffc00000, "n", F_NOTV9, v6 },
{ "illtrap",	F2(0, 0), F2(~0, ~0)|RD_G0, "n", 0, v9 },

/* This *is* a commutative instruction.  */
{ "xnor",	F3(2, 0x07, 0), F3(~2, ~0x07, ~0)|ASI(~0),	"1,2,d", 0, v6 },
{ "xnor",	F3(2, 0x07, 1), F3(~2, ~0x07, ~1),		"1,i,d", 0, v6 },
{ "xnor",	F3(2, 0x07, 1), F3(~2, ~0x07, ~1),		"i,1,d", 0, v6 },
/* This *is* a commutative instruction.  */
{ "xnorcc",	F3(2, 0x17, 0), F3(~2, ~0x17, ~0)|ASI(~0),	"1,2,d", 0, v6 },
{ "xnorcc",	F3(2, 0x17, 1), F3(~2, ~0x17, ~1),		"1,i,d", 0, v6 },
{ "xnorcc",	F3(2, 0x17, 1), F3(~2, ~0x17, ~1),		"i,1,d", 0, v6 },
{ "xor",	F3(2, 0x03, 0), F3(~2, ~0x03, ~0)|ASI(~0),	"1,2,d", 0, v6 },
{ "xor",	F3(2, 0x03, 1), F3(~2, ~0x03, ~1),		"1,i,d", 0, v6 },
{ "xor",	F3(2, 0x03, 1), F3(~2, ~0x03, ~1),		"i,1,d", 0, v6 },
{ "xorcc",	F3(2, 0x13, 0), F3(~2, ~0x13, ~0)|ASI(~0),	"1,2,d", 0, v6 },
{ "xorcc",	F3(2, 0x13, 1), F3(~2, ~0x13, ~1),		"1,i,d", 0, v6 },
{ "xorcc",	F3(2, 0x13, 1), F3(~2, ~0x13, ~1),		"i,1,d", 0, v6 },

{ "not",	F3(2, 0x07, 0), F3(~2, ~0x07, ~0)|ASI(~0), "1,d", F_ALIAS, v6 }, /* xnor rs1,%0,rd */
{ "not",	F3(2, 0x07, 0), F3(~2, ~0x07, ~0)|ASI(~0), "r", F_ALIAS, v6 }, /* xnor rd,%0,rd */

{ "btog",	F3(2, 0x03, 0), F3(~2, ~0x03, ~0)|ASI(~0),	"2,r", F_ALIAS, v6 }, /* xor rd,rs2,rd */
{ "btog",	F3(2, 0x03, 1), F3(~2, ~0x03, ~1),		"i,r", F_ALIAS, v6 }, /* xor rd,i,rd */

/* FPop1 and FPop2 are not instructions.  Don't accept them.  */

{ "fdtoi",	F3F(2, 0x34, 0x0d2), F3F(~2, ~0x34, ~0x0d2)|RS1_G0, "B,g", 0, v6 },
{ "fstoi",	F3F(2, 0x34, 0x0d1), F3F(~2, ~0x34, ~0x0d1)|RS1_G0, "f,g", 0, v6 },
{ "fqtoi",	F3F(2, 0x34, 0x0d3), F3F(~2, ~0x34, ~0x0d3)|RS1_G0, "R,g", 0, v8 },

{ "fdtox",	F3F(2, 0x34, 0x082), F3F(~2, ~0x34, ~0x082)|RS1_G0, "B,g", 0, v9 },
{ "fstox",	F3F(2, 0x34, 0x081), F3F(~2, ~0x34, ~0x081)|RS1_G0, "f,g", 0, v9 },
{ "fqtox",	F3F(2, 0x34, 0x083), F3F(~2, ~0x34, ~0x083)|RS1_G0, "R,g", 0, v9 },

{ "fitod",	F3F(2, 0x34, 0x0c8), F3F(~2, ~0x34, ~0x0c8)|RS1_G0, "f,H", 0, v6 },
{ "fitos",	F3F(2, 0x34, 0x0c4), F3F(~2, ~0x34, ~0x0c4)|RS1_G0, "f,g", 0, v6 },
{ "fitoq",	F3F(2, 0x34, 0x0cc), F3F(~2, ~0x34, ~0x0cc)|RS1_G0, "f,J", 0, v8 },

{ "fxtod",	F3F(2, 0x34, 0x088), F3F(~2, ~0x34, ~0x088)|RS1_G0, "f,H", 0, v9 },
{ "fxtos",	F3F(2, 0x34, 0x084), F3F(~2, ~0x34, ~0x084)|RS1_G0, "f,g", 0, v9 },
{ "fxtoq",	F3F(2, 0x34, 0x08c), F3F(~2, ~0x34, ~0x08c)|RS1_G0, "f,J", 0, v9 },

{ "fdtoq",	F3F(2, 0x34, 0x0ce), F3F(~2, ~0x34, ~0x0ce)|RS1_G0, "B,J", 0, v8 },
{ "fdtos",	F3F(2, 0x34, 0x0c6), F3F(~2, ~0x34, ~0x0c6)|RS1_G0, "B,g", 0, v6 },
{ "fqtod",	F3F(2, 0x34, 0x0cb), F3F(~2, ~0x34, ~0x0cb)|RS1_G0, "R,H", 0, v8 },
{ "fqtos",	F3F(2, 0x34, 0x0c7), F3F(~2, ~0x34, ~0x0c7)|RS1_G0, "R,g", 0, v8 },
{ "fstod",	F3F(2, 0x34, 0x0c9), F3F(~2, ~0x34, ~0x0c9)|RS1_G0, "f,H", 0, v6 },
{ "fstoq",	F3F(2, 0x34, 0x0cd), F3F(~2, ~0x34, ~0x0cd)|RS1_G0, "f,J", 0, v8 },

{ "fdivd",	F3F(2, 0x34, 0x04e), F3F(~2, ~0x34, ~0x04e), "v,B,H", 0, v6 },
{ "fdivq",	F3F(2, 0x34, 0x04f), F3F(~2, ~0x34, ~0x04f), "V,R,J", 0, v8 },
{ "fdivs",	F3F(2, 0x34, 0x04d), F3F(~2, ~0x34, ~0x04d), "e,f,g", 0, v6 },
{ "fmuld",	F3F(2, 0x34, 0x04a), F3F(~2, ~0x34, ~0x04a), "v,B,H", 0, v6 },
{ "fmulq",	F3F(2, 0x34, 0x04b), F3F(~2, ~0x34, ~0x04b), "V,R,J", 0, v8 },
{ "fmuls",	F3F(2, 0x34, 0x049), F3F(~2, ~0x34, ~0x049), "e,f,g", 0, v6 },

{ "fdmulq",	F3F(2, 0x34, 0x06e), F3F(~2, ~0x34, ~0x06e), "v,B,J", 0, v8 },
{ "fsmuld",	F3F(2, 0x34, 0x069), F3F(~2, ~0x34, ~0x069), "e,f,H", 0, v8 },

{ "fsqrtd",	F3F(2, 0x34, 0x02a), F3F(~2, ~0x34, ~0x02a)|RS1_G0, "B,H", 0, v7 },
{ "fsqrtq",	F3F(2, 0x34, 0x02b), F3F(~2, ~0x34, ~0x02b)|RS1_G0, "R,J", 0, v8 },
{ "fsqrts",	F3F(2, 0x34, 0x029), F3F(~2, ~0x34, ~0x029)|RS1_G0, "f,g", 0, v7 },

{ "fabsd",	F3F(2, 0x34, 0x00a), F3F(~2, ~0x34, ~0x00a)|RS1_G0, "B,H", 0, v9 },
{ "fabsq",	F3F(2, 0x34, 0x00b), F3F(~2, ~0x34, ~0x00b)|RS1_G0, "R,J", 0, v9 },
{ "fabss",	F3F(2, 0x34, 0x009), F3F(~2, ~0x34, ~0x009)|RS1_G0, "f,g", 0, v6 },
{ "fmovd",	F3F(2, 0x34, 0x002), F3F(~2, ~0x34, ~0x002)|RS1_G0, "B,H", 0, v9 },
{ "fmovq",	F3F(2, 0x34, 0x003), F3F(~2, ~0x34, ~0x003)|RS1_G0, "R,J", 0, v9 },
{ "fmovs",	F3F(2, 0x34, 0x001), F3F(~2, ~0x34, ~0x001)|RS1_G0, "f,g", 0, v6 },
{ "fnegd",	F3F(2, 0x34, 0x006), F3F(~2, ~0x34, ~0x006)|RS1_G0, "B,H", 0, v9 },
{ "fnegq",	F3F(2, 0x34, 0x007), F3F(~2, ~0x34, ~0x007)|RS1_G0, "R,J", 0, v9 },
{ "fnegs",	F3F(2, 0x34, 0x005), F3F(~2, ~0x34, ~0x005)|RS1_G0, "f,g", 0, v6 },

{ "faddd",	F3F(2, 0x34, 0x042), F3F(~2, ~0x34, ~0x042), "v,B,H", 0, v6 },
{ "faddq",	F3F(2, 0x34, 0x043), F3F(~2, ~0x34, ~0x043), "V,R,J", 0, v8 },
{ "fadds",	F3F(2, 0x34, 0x041), F3F(~2, ~0x34, ~0x041), "e,f,g", 0, v6 },
{ "fsubd",	F3F(2, 0x34, 0x046), F3F(~2, ~0x34, ~0x046), "v,B,H", 0, v6 },
{ "fsubq",	F3F(2, 0x34, 0x047), F3F(~2, ~0x34, ~0x047), "V,R,J", 0, v8 },
{ "fsubs",	F3F(2, 0x34, 0x045), F3F(~2, ~0x34, ~0x045), "e,f,g", 0, v6 },

#define CMPFCC(x)	(((x)&0x3)<<25)

{ "fcmpd",	          F3F(2, 0x35, 0x052),            F3F(~2, ~0x35, ~0x052)|RD_G0,  "v,B",   0, v6 },
{ "fcmpd",	CMPFCC(0)|F3F(2, 0x35, 0x052), CMPFCC(~0)|F3F(~2, ~0x35, ~0x052),	 "6,v,B", 0, v9 },
{ "fcmpd",	CMPFCC(1)|F3F(2, 0x35, 0x052), CMPFCC(~1)|F3F(~2, ~0x35, ~0x052),	 "7,v,B", 0, v9 },
{ "fcmpd",	CMPFCC(2)|F3F(2, 0x35, 0x052), CMPFCC(~2)|F3F(~2, ~0x35, ~0x052),	 "8,v,B", 0, v9 },
{ "fcmpd",	CMPFCC(3)|F3F(2, 0x35, 0x052), CMPFCC(~3)|F3F(~2, ~0x35, ~0x052),	 "9,v,B", 0, v9 },
{ "fcmped",	          F3F(2, 0x35, 0x056),            F3F(~2, ~0x35, ~0x056)|RD_G0,  "v,B",   0, v6 },
{ "fcmped",	CMPFCC(0)|F3F(2, 0x35, 0x056), CMPFCC(~0)|F3F(~2, ~0x35, ~0x056),	 "6,v,B", 0, v9 },
{ "fcmped",	CMPFCC(1)|F3F(2, 0x35, 0x056), CMPFCC(~1)|F3F(~2, ~0x35, ~0x056),	 "7,v,B", 0, v9 },
{ "fcmped",	CMPFCC(2)|F3F(2, 0x35, 0x056), CMPFCC(~2)|F3F(~2, ~0x35, ~0x056),	 "8,v,B", 0, v9 },
{ "fcmped",	CMPFCC(3)|F3F(2, 0x35, 0x056), CMPFCC(~3)|F3F(~2, ~0x35, ~0x056),	 "9,v,B", 0, v9 },
{ "fcmpq",	          F3F(2, 0x34, 0x053),            F3F(~2, ~0x34, ~0x053)|RD_G0,	 "V,R", 0, v8 },
{ "fcmpq",	CMPFCC(0)|F3F(2, 0x35, 0x053), CMPFCC(~0)|F3F(~2, ~0x35, ~0x053),	 "6,V,R", 0, v9 },
{ "fcmpq",	CMPFCC(1)|F3F(2, 0x35, 0x053), CMPFCC(~1)|F3F(~2, ~0x35, ~0x053),	 "7,V,R", 0, v9 },
{ "fcmpq",	CMPFCC(2)|F3F(2, 0x35, 0x053), CMPFCC(~2)|F3F(~2, ~0x35, ~0x053),	 "8,V,R", 0, v9 },
{ "fcmpq",	CMPFCC(3)|F3F(2, 0x35, 0x053), CMPFCC(~3)|F3F(~2, ~0x35, ~0x053),	 "9,V,R", 0, v9 },
{ "fcmpeq",	          F3F(2, 0x34, 0x057),            F3F(~2, ~0x34, ~0x057)|RD_G0,	 "V,R", 0, v8 },
{ "fcmpeq",	CMPFCC(0)|F3F(2, 0x35, 0x057), CMPFCC(~0)|F3F(~2, ~0x35, ~0x057),	 "6,V,R", 0, v9 },
{ "fcmpeq",	CMPFCC(1)|F3F(2, 0x35, 0x057), CMPFCC(~1)|F3F(~2, ~0x35, ~0x057),	 "7,V,R", 0, v9 },
{ "fcmpeq",	CMPFCC(2)|F3F(2, 0x35, 0x057), CMPFCC(~2)|F3F(~2, ~0x35, ~0x057),	 "8,V,R", 0, v9 },
{ "fcmpeq",	CMPFCC(3)|F3F(2, 0x35, 0x057), CMPFCC(~3)|F3F(~2, ~0x35, ~0x057),	 "9,V,R", 0, v9 },
{ "fcmps",	          F3F(2, 0x35, 0x051),            F3F(~2, ~0x35, ~0x051)|RD_G0, "e,f",   0, v6 },
{ "fcmps",	CMPFCC(0)|F3F(2, 0x35, 0x051), CMPFCC(~0)|F3F(~2, ~0x35, ~0x051),	 "6,e,f", 0, v9 },
{ "fcmps",	CMPFCC(1)|F3F(2, 0x35, 0x051), CMPFCC(~1)|F3F(~2, ~0x35, ~0x051),	 "7,e,f", 0, v9 },
{ "fcmps",	CMPFCC(2)|F3F(2, 0x35, 0x051), CMPFCC(~2)|F3F(~2, ~0x35, ~0x051),	 "8,e,f", 0, v9 },
{ "fcmps",	CMPFCC(3)|F3F(2, 0x35, 0x051), CMPFCC(~3)|F3F(~2, ~0x35, ~0x051),	 "9,e,f", 0, v9 },
{ "fcmpes",	          F3F(2, 0x35, 0x055),            F3F(~2, ~0x35, ~0x055)|RD_G0, "e,f",   0, v6 },
{ "fcmpes",	CMPFCC(0)|F3F(2, 0x35, 0x055), CMPFCC(~0)|F3F(~2, ~0x35, ~0x055),	 "6,e,f", 0, v9 },
{ "fcmpes",	CMPFCC(1)|F3F(2, 0x35, 0x055), CMPFCC(~1)|F3F(~2, ~0x35, ~0x055),	 "7,e,f", 0, v9 },
{ "fcmpes",	CMPFCC(2)|F3F(2, 0x35, 0x055), CMPFCC(~2)|F3F(~2, ~0x35, ~0x055),	 "8,e,f", 0, v9 },
{ "fcmpes",	CMPFCC(3)|F3F(2, 0x35, 0x055), CMPFCC(~3)|F3F(~2, ~0x35, ~0x055),	 "9,e,f", 0, v9 },

/* These Extended FPop (FIFO) instructions are new in the Fujitsu
   MB86934, replacing the CPop instructions from v6 and later
   processors.  */

#define EFPOP1_2(name, op, args) { name, F3F(2, 0x36, op), F3F(~2, ~0x36, ~op)|RS1_G0, args, 0, sparclite }
#define EFPOP1_3(name, op, args) { name, F3F(2, 0x36, op), F3F(~2, ~0x36, ~op),        args, 0, sparclite }
#define EFPOP2_2(name, op, args) { name, F3F(2, 0x37, op), F3F(~2, ~0x37, ~op)|RD_G0,  args, 0, sparclite }

EFPOP1_2 ("efitod",	0x0c8, "f,H"),
EFPOP1_2 ("efitos",	0x0c4, "f,g"),
EFPOP1_2 ("efdtoi",	0x0d2, "B,g"),
EFPOP1_2 ("efstoi",	0x0d1, "f,g"),
EFPOP1_2 ("efstod",	0x0c9, "f,H"),
EFPOP1_2 ("efdtos",	0x0c6, "B,g"),
EFPOP1_2 ("efmovs",	0x001, "f,g"),
EFPOP1_2 ("efnegs",	0x005, "f,g"),
EFPOP1_2 ("efabss",	0x009, "f,g"),
EFPOP1_2 ("efsqrtd",	0x02a, "B,H"),
EFPOP1_2 ("efsqrts",	0x029, "f,g"),
EFPOP1_3 ("efaddd",	0x042, "v,B,H"),
EFPOP1_3 ("efadds",	0x041, "e,f,g"),
EFPOP1_3 ("efsubd",	0x046, "v,B,H"),
EFPOP1_3 ("efsubs",	0x045, "e,f,g"),
EFPOP1_3 ("efdivd",	0x04e, "v,B,H"),
EFPOP1_3 ("efdivs",	0x04d, "e,f,g"),
EFPOP1_3 ("efmuld",	0x04a, "v,B,H"),
EFPOP1_3 ("efmuls",	0x049, "e,f,g"),
EFPOP1_3 ("efsmuld",	0x069, "e,f,H"),
EFPOP2_2 ("efcmpd",	0x052, "v,B"),
EFPOP2_2 ("efcmped",	0x056, "v,B"),
EFPOP2_2 ("efcmps",	0x051, "e,f"),
EFPOP2_2 ("efcmpes",	0x055, "e,f"),

#undef EFPOP1_2
#undef EFPOP1_3
#undef EFPOP2_2

/* These are marked F_ALIAS, so that they won't conflict with sparclite insns
   present.  Otherwise, the F_ALIAS flag is ignored.  */
{ "cpop1",	F3(2, 0x36, 0), F3(~2, ~0x36, ~1), "[1+2],d", F_ALIAS|F_NOTV9, v6 },
{ "cpop2",	F3(2, 0x37, 0), F3(~2, ~0x37, ~1), "[1+2],d", F_ALIAS|F_NOTV9, v6 },

#define IMPDEP(name, code) \
{ name,	F3(2, code, 0), F3(~2, ~code, ~0)|ASI(~0), "1,2,d", 0, v9 }, \
{ name,	F3(2, code, 1), F3(~2, ~code, ~1),	   "1,i,d", 0, v9 }, \
{ name, F3(2, code, 0), F3(~2, ~code, ~0),         "x,1,2,d", 0, v9 }, \
{ name, F3(2, code, 0), F3(~2, ~code, ~0),         "x,e,f,g", 0, v9 }

IMPDEP ("impdep1", 0x36),
IMPDEP ("impdep2", 0x37),

#undef IMPDEP

{ "casa",	F3(3, 0x3c, 0), F3(~3, ~0x3c, ~0), "[1]A,2,d", 0, v9 },
{ "casa",	F3(3, 0x3c, 1), F3(~3, ~0x3c, ~1), "[1]o,2,d", 0, v9 },
{ "casxa",	F3(3, 0x3e, 0), F3(~3, ~0x3e, ~0), "[1]A,2,d", 0, v9 },
{ "casxa",	F3(3, 0x3e, 1), F3(~3, ~0x3e, ~1), "[1]o,2,d", 0, v9 },

/* v9 synthetic insns */
/* FIXME: still missing "signx d", and "clruw d".  Can't be done here.  */
{ "iprefetch",	F2(0, 1)|(2<<20)|BPRED, F2(~0, ~1)|(1<<20)|ANNUL|COND(~0), "G", 0, v9 }, /* bn,a,pt %xcc,label */
{ "signx",	F3(2, 0x27, 0), F3(~2, ~0x27, ~0)|(1<<12)|ASI(~0)|RS2_G0, "1,d", F_ALIAS, v9 }, /* sra rs1,%g0,rd */
{ "clruw",	F3(2, 0x26, 0), F3(~2, ~0x26, ~0)|(1<<12)|ASI(~0)|RS2_G0, "1,d", F_ALIAS, v9 }, /* srl rs1,%g0,rd */
{ "cas",	F3(3, 0x3c, 0)|ASI(0x80), F3(~3, ~0x3c, ~0)|ASI(~0x80), "[1],2,d", F_ALIAS, v9 }, /* casa [rs1]ASI_P,rs2,rd */
{ "casl",	F3(3, 0x3c, 0)|ASI(0x88), F3(~3, ~0x3c, ~0)|ASI(~0x88), "[1],2,d", F_ALIAS, v9 }, /* casa [rs1]ASI_P_L,rs2,rd */
{ "casx",	F3(3, 0x3e, 0)|ASI(0x80), F3(~3, ~0x3e, ~0)|ASI(~0x80), "[1],2,d", F_ALIAS, v9 }, /* casxa [rs1]ASI_P,rs2,rd */
{ "casxl",	F3(3, 0x3e, 0)|ASI(0x88), F3(~3, ~0x3e, ~0)|ASI(~0x88), "[1],2,d", F_ALIAS, v9 }, /* casxa [rs1]ASI_P_L,rs2,rd */

};

const int bfd_sparc_num_opcodes = ((sizeof sparc_opcodes)/(sizeof sparc_opcodes[0]));

/* Utilities for argument parsing.  */

typedef struct
{
  int value;
  char *name;
} arg;

/* Look up NAME in TABLE.  */

static int
lookup_name (table, name)
     arg *table;
     char *name;
{
  arg *p;

  for (p = table; p->name; ++p)
    if (strcmp (name, p->name) == 0)
      return p->value;

  return -1;
}

/* Look up VALUE in TABLE.  */

static char *
lookup_value (table, value)
     arg *table;
     int value;
{
  arg *p;

  for (p = table; p->name; ++p)
    if (value == p->value)
      return p->name;

  return (char *) 0;
}

/* Handle ASI's.  */

static arg asi_table[] =
{
  { 0x10, "#ASI_AIUP" },
  { 0x11, "#ASI_AIUS" },
  { 0x18, "#ASI_AIUP_L" },
  { 0x19, "#ASI_AIUS_L" },
  { 0x80, "#ASI_P" },
  { 0x81, "#ASI_S" },
  { 0x82, "#ASI_PNF" },
  { 0x83, "#ASI_SNF" },
  { 0x88, "#ASI_P_L" },
  { 0x89, "#ASI_S_L" },
  { 0x8a, "#ASI_PNF_L" },
  { 0x8b, "#ASI_SNF_L" },
  { 0x10, "#ASI_AS_IF_USER_PRIMARY" },
  { 0x11, "#ASI_AS_IF_USER_SECONDARY" },
  { 0x18, "#ASI_AS_IF_USER_PRIMARY_L" },
  { 0x19, "#ASI_AS_IF_USER_SECONDARY_L" },
  { 0x80, "#ASI_PRIMARY" },
  { 0x81, "#ASI_SECONDARY" },
  { 0x82, "#ASI_PRIMARY_NOFAULT" },
  { 0x83, "#ASI_SECONDARY_NOFAULT" },
  { 0x88, "#ASI_PRIMARY_LITTLE" },
  { 0x89, "#ASI_SECONDARY_LITTLE" },
  { 0x8a, "#ASI_PRIMARY_NOFAULT_LITTLE" },
  { 0x8b, "#ASI_SECONDARY_NOFAULT_LITTLE" },
  { 0, 0 }
};

/* Return the value for ASI NAME, or -1 if not found.  */

int
sparc_encode_asi (name)
     char *name;
{
  return lookup_name (asi_table, name);
}

/* Return the name for ASI value VALUE or NULL if not found.  */

char *
sparc_decode_asi (value)
     int value;
{
  return lookup_value (asi_table, value);
}

/* Handle membar masks.  */

static arg membar_table[] =
{
  { 0x40, "#Sync" },
  { 0x20, "#MemIssue" },
  { 0x10, "#Lookaside" },
  { 0x08, "#StoreStore" },
  { 0x04, "#LoadStore" },
  { 0x02, "#StoreLoad" },
  { 0x01, "#LoadLoad" },
  { 0, 0 }
};

/* Return the value for membar arg NAME, or -1 if not found.  */

int
sparc_encode_membar (name)
     char *name;
{
  return lookup_name (membar_table, name);
}

/* Return the name for membar value VALUE or NULL if not found.  */

char *
sparc_decode_membar (value)
     int value;
{
  return lookup_value (membar_table, value);
}

/* Handle prefetch args.  */

static arg prefetch_table[] =
{
  { 0, "#n_reads" },
  { 1, "#one_read" },
  { 2, "#n_writes" },
  { 3, "#one_write" },
  { 4, "#page" },
  { 0, 0 }
};

/* Return the value for prefetch arg NAME, or -1 if not found.  */

int
sparc_encode_prefetch (name)
     char *name;
{
  return lookup_name (prefetch_table, name);
}

/* Return the name for prefetch value VALUE or NULL if not found.  */

char *
sparc_decode_prefetch (value)
     int value;
{
  return lookup_value (prefetch_table, value);
}
