/*
 * This file is part of SIS.
 * 
 * SIS, SPARC instruction simulator V1.8 Copyright (C) 1995 Jiri Gaisler,
 * European Space Agency
 * 
 * This program is free software; you can redistribute it and/or modify it under
 * the terms of the GNU General Public License as published by the Free
 * Software Foundation; either version 2 of the License, or (at your option)
 * any later version.
 * 
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * more details.
 * 
 * You should have received a copy of the GNU General Public License along with
 * this program; if not, write to the Free Software Foundation, Inc., 675
 * Mass Ave, Cambridge, MA 02139, USA.
 * 
 */

#include "sis.h"
#include "end.h"
#include <math.h>
#include <stdio.h>

extern int32    ext_irl, irqpend, iurev0, sis_verbose;

/* Load/store interlock delay */
#define FLSTHOLD 1

/* Load delay (delete if unwanted - speeds up simulation) */
#define LOAD_DEL 1

#define T_LD	2
#define T_LDD	3
#define T_ST	3
#define T_STD	4
#define T_LDST	4
#define T_JMPL	2
#define T_RETT	2

#define FSR_QNE 	0x2000
#define FP_EXE_MODE 0
#define	FP_EXC_PE   1
#define FP_EXC_MODE 2

#define	FBA	8
#define	FBN	0
#define	FBNE	1
#define	FBLG	2
#define	FBUL	3
#define	FBL 	4
#define	FBUG	5
#define	FBG 	6
#define	FBU 	7

#define	FCC_E 	0
#define	FCC_L 	1
#define	FCC_G 	2
#define	FCC_U 	3

#define PSR_ET 0x20
#define PSR_EF 0x1000
#define PSR_PS 0x40
#define PSR_S  0x80
#define PSR_N  0x0800000
#define PSR_Z  0x0400000
#define PSR_V  0x0200000
#define PSR_C  0x0100000
#define PSR_CC 0x0F00000
#define PSR_CWP 0x7
#define PSR_PIL 0x0f00

#define ICC_N	sregs->psr
#define ICC_Z	(sregs->psr << 1)
#define ICC_V	(sregs->psr << 2)
#define ICC_C	(sregs->psr << 3)

#define TRAP_IEXC 1
#define TRAP_UNIMP 2
#define TRAP_PRIVI 3
#define TRAP_FPDIS 4
#define TRAP_WOFL 5
#define TRAP_WUFL 6
#define TRAP_UNALI 7
#define TRAP_FPEXC 8
#define TRAP_DEXC 9
#define TRAP_TAG 10

#define FSR_TT		0x1C000
#define FP_IEEE		0x04000
#define FP_UNIMP	0x0C000
#define FP_SEQ_ERR	0x10000

#define	BICC_BN		0
#define	BICC_BE		1
#define	BICC_BLE	2
#define	BICC_BL		3
#define	BICC_BLEU	4
#define	BICC_BCS	5
#define	BICC_NEG	6
#define	BICC_BVS	7
#define	BICC_BA		8

#define INST_SIMM13 0x1fff
#define INST_RS2    0x1f
#define INST_I	    0x2000
#define ADD 	0x00
#define ADDCC 	0x10
#define ADDX 	0x08
#define ADDXCC 	0x18
#define TADDCC 	0x20
#define TADDCCTV 	0x22
#define IAND 	0x01
#define IANDCC 	0x11
#define IANDN 	0x05
#define IANDNCC	0x15
#define MULScc 	0x24
#define DIVScc 	0x1D
#define SMUL	0x0B
#define SMULCC	0x1B
#define IOR 	0x02
#define IORCC 	0x12
#define IORN 	0x06
#define IORNCC 	0x16
#define SLL 	0x25
#define SRA 	0x27
#define SRL 	0x26
#define SUB 	0x04
#define SUBCC 	0x14
#define SUBX 	0x0C
#define SUBXCC 	0x1C
#define IXNOR 	0x07
#define IXNORCC	0x17
#define IXOR 	0x03
#define IXORCC 	0x13
#define SETHI 	0x04
#define BICC 	0x02
#define FPBCC 	0x06
#define RDY 	0x28
#define RDPSR 	0x29
#define RDWIM 	0x2A
#define RDTBR 	0x2B
#define WRY	0x30
#define WRPSR	0x31
#define WRWIM	0x32
#define WRTBR	0x33
#define JMPL 	0x38
#define RETT 	0x39
#define TICC 	0x3A
#define SAVE 	0x3C
#define RESTORE 0x3D
#define LDD	0x03
#define LDDA	0x13
#define LD	0x00
#define LDA	0x10
#define LDF	0x20
#define LDDF	0x23
#define LDSTUB	0x0D
#define LDSTUBA	0x1D
#define LDUB	0x01
#define LDUBA	0x11
#define LDSB	0x09
#define LDSBA	0x19
#define LDUH	0x02
#define LDUHA	0x12
#define LDSH	0x0A
#define LDSHA	0x1A
#define LDFSR	0x21
#define ST	0x04
#define STA	0x14
#define STB	0x05
#define STBA	0x15
#define STD	0x07
#define STDA	0x17
#define STF	0x24
#define STDFQ	0x26
#define STDF	0x27
#define STFSR	0x25
#define STH	0x06
#define STHA	0x16
#define SWAP	0x0F
#define SWAPA	0x1F

/* # of cycles overhead when a trap is taken */
#define TRAP_C  3

int32           fpexec();
extern struct estate ebase;
extern int32    nfp;

sub_cc(operand1, operand2, result, sregs)
    int32           operand1;
    int32           operand2;
    int32           result;
    struct pstate  *sregs;
{
    sregs->psr = ((sregs->psr & ~PSR_N) | ((result >> 8) & PSR_N));
    if (result)
	sregs->psr &= ~PSR_Z;
    else
	sregs->psr |= PSR_Z;
    sregs->psr = (sregs->psr & ~PSR_V) | ((
					 ((operand1 & ~operand2 & ~result) |
			   (~operand1 & operand2 & result)) >> 10) & PSR_V);
    sregs->psr = (sregs->psr & ~PSR_C) | ((
					   ((~operand1 & operand2) |
			 ((~operand1 | operand2) & result)) >> 11) & PSR_C);
}

add_cc(operand1, operand2, result, psr)
    int32           operand1;
    int32           operand2;
    int32           result;
    uint32         *psr;
{
    *psr = ((*psr & ~PSR_N) | ((result >> 8) & PSR_N));
    if (result)
	*psr &= ~PSR_Z;
    else
	*psr |= PSR_Z;
    *psr = (*psr & ~PSR_V) | ((
			       ((operand1 & operand2 & ~result) |
			  (~operand1 & ~operand2 & result)) >> 10) & PSR_V);
    *psr = (*psr & ~PSR_C) | ((
			       ((operand1 & operand2) |
			 ((operand1 | operand2) & ~result)) >> 11) & PSR_C);
}

log_cc(result, sregs)
    int32           result;
    struct pstate  *sregs;
{
    sregs->psr &= ~(PSR_CC);	/* Zero CC bits */
    sregs->psr = (sregs->psr | ((result >> 8) & PSR_N));
    if (result == 0)
	sregs->psr |= PSR_Z;
}

int
dispatch_instruction(sregs)
    struct pstate  *sregs;
{

    uint32          cwp, op, op2, op3, opf, opc, asi, a, rd, cond, rs1,
                    rs2;
    uint32          ldep;
    int32           operand1, operand2, *rdd, result, i, disp22, eicc,
                    new_cwp;
    int32           pc, npc, data, address, ws, mexc, fcc;

    sregs->ninst++;
    sregs->icnt = 1;
    cwp = ((sregs->psr & PSR_CWP) << 4);
    op = sregs->inst >> 30;
    pc = sregs->npc;
    npc = sregs->npc + 4;
    if (op > 1) {

	op3 = (sregs->inst >> 19) & 0x3f;
	rs1 = (sregs->inst >> 14) & 0x1f;
	rd = (sregs->inst >> 25) & 0x1f;

#ifdef LOAD_DEL

	/* Check if load dependecy is possible */
	ldep = ((ebase.simtime <= sregs->ildtime) && ((op3 & 0x38) != 0x28) &&
		((op3 & 0x3e) != 0x34) && (sregs->ildreg != 0));
	if (sregs->inst & INST_I) {
	    if (ldep && (sregs->ildreg == rs1))
		sregs->hold++;
	    operand2 = sregs->inst & INST_SIMM13;
	    if (operand2 > 0x0fff)
		operand2 |= 0xfffff000;
	} else {
	    rs2 = sregs->inst & INST_RS2;
	    if (rs2 > 7)
		operand2 = sregs->r[(cwp + rs2) & 0x7f];
	    else
		operand2 = sregs->g[rs2];
	    if (ldep && ((sregs->ildreg == rs1) || (sregs->ildreg == rs2)))
		sregs->hold++;
	}
#else
	if (sregs->inst & INST_I) {
	    operand2 = sregs->inst & INST_SIMM13;
	    if (operand2 > 0x0fff)
		operand2 |= 0xfffff000;
	} else {
	    rs2 = sregs->inst & INST_RS2;
	    if (rs2 > 7)
		operand2 = sregs->r[(cwp + rs2) & 0x7f];
	    else
		operand2 = sregs->g[rs2];
	}
#endif

	if (rd > 7)
	    rdd = &(sregs->r[(cwp + rd) & 0x7f]);
	else
	    rdd = &(sregs->g[rd]);
	if (rs1 > 7)
	    rs1 = sregs->r[(cwp + rs1) & 0x7f];
	else
	    rs1 = sregs->g[rs1];
    }
    switch (op) {
    case 0:
	op2 = (sregs->inst >> 22) & 0x7;
	switch (op2) {
	case SETHI:
	    rd = (sregs->inst >> 25) & 0x1f;
	    if (rd > 7)
		rdd = &(sregs->r[(cwp + rd) & 0x7f]);
	    else
		rdd = &(sregs->g[rd]);
	    *rdd = sregs->inst << 10;
	    break;
	case BICC:
#ifdef STAT
	    sregs->nbranch++;
#endif
	    cond = ((sregs->inst >> 25) & 0x0f);
	    switch (cond & 0x7) {
	    case BICC_BN:
		eicc = 0;
		break;
	    case BICC_BE:
		eicc = ICC_Z;
		break;
	    case BICC_BLE:
		eicc = ICC_Z | (ICC_N ^ ICC_V);
		break;
	    case BICC_BL:
		eicc = (ICC_N ^ ICC_V);
		break;
	    case BICC_BLEU:
		eicc = ICC_C | ICC_Z;
		break;
	    case BICC_BCS:
		eicc = ICC_C;
		break;
	    case BICC_NEG:
		eicc = ICC_N;
		break;
	    case BICC_BVS:
		eicc = ICC_V;
		break;
	    }
	    eicc &= PSR_N;
	    if (sregs->inst & 0x10000000)
		eicc = !eicc;
	    a = sregs->inst & 0x20000000;
	    if (eicc) {
		operand1 = sregs->inst & 0x3fffff;
		if (sregs->inst & 0x200000)
		    operand1 |= 0xffc00000;
		npc = sregs->pc + (operand1 << 2);
		if ((cond == BICC_BA) && (a))
		    sregs->annul = 1;
	    } else {
		if (a)
		    sregs->annul = 1;
	    }
	    break;
	case FPBCC:
#ifdef STAT
	    sregs->nbranch++;
#endif
	    if (!((sregs->psr & PSR_EF) && chk_fp(sregs))) {
		sregs->trap = TRAP_FPDIS;
		break;
	    }
	    if (ebase.simtime < sregs->ftime) {
		sregs->ftime = ebase.simtime + sregs->hold;
	    }
	    cond = ((sregs->inst >> 25) & 0x0f);
	    fcc = (sregs->fsr >> 10) & 0x3;
	    switch (cond & 0x7) {
	    case FBN:
		eicc = 0;
		break;
	    case FBNE:
		eicc = (fcc != FCC_E);
		break;
	    case FBLG:
		eicc = (fcc == FCC_L) || (fcc == FCC_G);
		break;
	    case FBUL:
		eicc = (fcc == FCC_L) || (fcc == FCC_U);
		break;
	    case FBL:
		eicc = (fcc == FCC_L);
		break;
	    case FBUG:
		eicc = (fcc == FCC_G) || (fcc == FCC_U);
		break;
	    case FBG:
		eicc = (fcc == FCC_G);
		break;
	    case FBU:
		eicc = (fcc == FCC_U);
		break;
	    }
	    if (sregs->inst & 0x10000000)
		eicc = !eicc;
	    a = sregs->inst & 0x20000000;
	    if (eicc) {
		operand1 = sregs->inst & 0x3fffff;
		if (sregs->inst & 0x200000)
		    operand1 |= 0xffc00000;
		npc = sregs->pc + (operand1 << 2);
		if ((cond == FBA) && (a))
		    sregs->annul = 1;
	    } else {
		if (a)
		    sregs->annul = 1;
	    }
	    break;

	default:
	    sregs->trap = TRAP_UNIMP;
	    break;
	}
	break;
    case 1:			/* CALL */
#ifdef STAT
	sregs->nbranch++;
#endif
	sregs->r[(cwp + 15) & 0x7f] = sregs->pc;
	npc = sregs->pc + (sregs->inst << 2);
	break;

    case 2:
	if ((op3 >> 1) == 0x1a) {
	    if (!((sregs->psr & PSR_EF) && chk_fp(sregs))) {
		sregs->trap = TRAP_FPDIS;
	    } else {
		rs1 = (sregs->inst >> 14) & 0x1f;
		rs2 = sregs->inst & 0x1f;
		sregs->trap = fpexec(op3, rd, rs1, rs2, sregs);
	    }
	} else {

	    switch (op3) {
	    case TICC:
		cond = ((sregs->inst >> 25) & 0x0f);
		switch (cond & 0x7) {
		case BICC_BN:
		    eicc = 0;
		    break;
		case BICC_BE:
		    eicc = ICC_Z;
		    break;
		case BICC_BLE:
		    eicc = ICC_Z | (ICC_N ^ ICC_V);
		    break;
		case BICC_BL:
		    eicc = (ICC_N ^ ICC_V);
		    break;
		case BICC_BLEU:
		    eicc = ICC_C | ICC_Z;
		    break;
		case BICC_BCS:
		    eicc = ICC_C;
		    break;
		case BICC_NEG:
		    eicc = ICC_N;
		    break;
		case BICC_BVS:
		    eicc = ICC_V;
		    break;
		}
		eicc &= PSR_N;
		if (sregs->inst & 0x10000000)
		    eicc = !eicc;
		if (eicc) {
		    sregs->trap = (0x80 | ((rs1 + operand2) & 0x7f));
		}
		break;

	    case MULScc:
		operand1 =
		    (((sregs->psr & PSR_V) ^ ((sregs->psr & PSR_N) >> 2))
		     << 10) | (rs1 >> 1);
		if ((sregs->y & 1) == 0)
		    operand2 = 0;
		*rdd = operand1 + operand2;
		sregs->y = (rs1 << 31) | (sregs->y >> 1);
		add_cc(operand1, operand2, *rdd, &sregs->psr);
		break;
	    case DIVScc:
		{
		  int sign;
		  uint32 result, remainder;
		  int c0, ov, y31;

		  sign = ((sregs->psr & PSR_V) != 0) ^ ((sregs->psr & PSR_N) != 0);

		  remainder = (sregs->y << 1) | (rs1 >> 31);

		  if (sign == 0)
		    {
		      result = remainder - operand2;
		      c0 = (sregs->y & ~operand2 & 0x80000000) != 0;
		    }
		  else
		    {
		      result = remainder + operand2;
		      c0 = (sregs->y & operand2 & 0x80000000) != 0;
		    }

		  if (result & 0x80000000)
		    sregs->psr |= PSR_N;
		  else
		    sregs->psr &= ~PSR_N;

		  y31 = (sregs->y & 0x80000000) == 0x80000000;

		  if (result == 0 && sign == y31)
		    sregs->psr |= PSR_Z;
		  else
		    sregs->psr &= ~PSR_Z;

		  sign = (sign && !y31) || (!c0 && (sign || !y31));

		  if (sign ^ (result >> 31))
		    sregs->psr != PSR_V;
		  else
		    sregs->psr &= ~PSR_V;

		  if (!sign)
		    sregs->psr != PSR_C;
		  else
		    sregs->psr &= ~PSR_C;

		  sregs->y = result;

		  if (rd != 0)
		    *rdd = (rs1 << 1) | !sign;
		}
		break;
	    case SMUL:
		{
		  unsigned short ul, vl;
		  short uh, vh;
		  unsigned int pp1, pp2, pp4;

		  ul = rs1;
		  uh = rs1 >> 16;
		  vl = operand2;
		  vh = operand2 >> 16;

		  pp1 = ul * vl;
		  pp2 = uh * vl + vh * ul;
		  pp4 = uh * vh;

		  *rdd = (pp2 << 16) + pp1;
		  sregs->y = pp4 + ((int)(pp2 + (pp1 >> 16)) >> 16);
		}
		break;
	    case IXNOR:
		*rdd = rs1 ^ ~operand2;
		break;
	    case IXNORCC:
		*rdd = rs1 ^ ~operand2;
		log_cc(*rdd, sregs);
		break;
	    case IXOR:
		*rdd = rs1 ^ operand2;
		break;
	    case IXORCC:
		*rdd = rs1 ^ operand2;
		log_cc(*rdd, sregs);
		break;
	    case IOR:
		*rdd = rs1 | operand2;
		break;
	    case IORCC:
		*rdd = rs1 | operand2;
		log_cc(*rdd, sregs);
		break;
	    case IORN:
		*rdd = rs1 | ~operand2;
		break;
	    case IORNCC:
		*rdd = rs1 | ~operand2;
		log_cc(*rdd, sregs);
		break;
	    case IANDNCC:
		*rdd = rs1 & ~operand2;
		log_cc(*rdd, sregs);
		break;
	    case IANDN:
		*rdd = rs1 & ~operand2;
		break;
	    case IAND:
		*rdd = rs1 & operand2;
		break;
	    case IANDCC:
		*rdd = rs1 & operand2;
		log_cc(*rdd, sregs);
		break;
	    case SUB:
		*rdd = rs1 - operand2;
		break;
	    case SUBCC:
		*rdd = rs1 - operand2;
		sub_cc(rs1, operand2, *rdd, sregs);
		break;
	    case SUBX:
		*rdd = rs1 - operand2 - ((sregs->psr >> 20) & 1);
		break;
	    case SUBXCC:
		*rdd = rs1 - operand2 - ((sregs->psr >> 20) & 1);
		sub_cc(rs1, operand2, *rdd, sregs);
		break;
	    case ADD:
		*rdd = rs1 + operand2;
		break;
	    case ADDCC:
		*rdd = rs1 + operand2;
		add_cc(rs1, operand2, *rdd, &sregs->psr);
		break;
	    case ADDX:
		*rdd = rs1 + operand2 + ((sregs->psr >> 20) & 1);
		break;
	    case ADDXCC:
		*rdd = rs1 + operand2 + ((sregs->psr >> 20) & 1);
		add_cc(rs1, operand2, *rdd, &sregs->psr);
		break;
	    case TADDCC:
		*rdd = rs1 + operand2;
		add_cc(rs1, operand2, *rdd, &sregs->psr);
		if ((rs1 | operand2) & 0x3)
		    sregs->psr |= PSR_V;
		break;
	    case TADDCCTV:
		*rdd = rs1 + operand2;
		result = 0;
		add_cc(rs1, operand2, *rdd, &result);
		if ((rs1 | operand2) & 0x3)
		    result |= PSR_V;
		if (result & PSR_V) {
		    sregs->trap = TRAP_TAG;
		} else {
		    sregs->psr = (sregs->psr & PSR_CC) | result;
		}
		break;
	    case SLL:
		*rdd = rs1 << (operand2 & 0x1f);
		break;
	    case SRL:
		*rdd = rs1 >> (operand2 & 0x1f);
		break;
	    case SRA:
		*rdd = ((int) rs1) >> (operand2 & 0x1f);
		break;
	    case SAVE:
		new_cwp = ((sregs->psr & PSR_CWP) - 1) & PSR_CWP;
		if (sregs->wim & (1 << new_cwp)) {
		    sregs->trap = TRAP_WOFL;
		    break;
		}
		if (rd > 7)
		    rdd = &(sregs->r[((new_cwp << 4) + rd) & 0x7f]);
		*rdd = rs1 + operand2;
		sregs->psr = (sregs->psr & ~PSR_CWP) | new_cwp;
		break;
	    case RESTORE:

#ifdef IUREV0
		if ((iurev0) && ((sregs->jmpltime + 1) == sregs->ninst)) {
		    if (!(sregs->rett_err)) {
			sregs->rett_err = 1;
			if (sis_verbose)
			    printf("IU rev.0 bug mode entered\n");
		    }
		}
#endif

		new_cwp = ((sregs->psr & PSR_CWP) + 1) & PSR_CWP;
		if (sregs->wim & (1 << new_cwp)) {
		    sregs->trap = TRAP_WUFL;
		    break;
		}
		if (rd > 7)
		    rdd = &(sregs->r[((new_cwp << 4) + rd) & 0x7f]);
		*rdd = rs1 + operand2;
		sregs->psr = (sregs->psr & ~PSR_CWP) | new_cwp;
		break;
	    case RDPSR:
		if (!(sregs->psr & PSR_S)) {
		    sregs->trap = TRAP_PRIVI;
		    break;
		}
		*rdd = sregs->psr;
#ifdef IUREV0

		if (iurev0 & sregs->rett_err) {
		    operand2 = sregs->psr;
		    *rdd |= PSR_ET;
		    *rdd &= ~(PSR_S);
		    *rdd |= ((*rdd & PSR_PS) << 1);
		    if (sis_verbose) {
			if (operand2 != *rdd)
			    printf("rdpsr failed: %08X -> %08X\n", operand2, *rdd);
		    }
		}
#endif
		break;
	    case RDY:
		if (!(sregs->psr & PSR_S)) {
		    sregs->trap = TRAP_PRIVI;
		    break;
		}
		*rdd = sregs->y;
		break;
	    case RDWIM:
		if (!(sregs->psr & PSR_S)) {
		    sregs->trap = TRAP_PRIVI;
		    break;
		}
		*rdd = sregs->wim;
		break;
	    case RDTBR:
		if (!(sregs->psr & PSR_S)) {
		    sregs->trap = TRAP_PRIVI;
		    break;
		}
		*rdd = sregs->tbr;
		break;
	    case WRPSR:
		if ((sregs->psr & 0x1f) > 7) {
		    sregs->trap = TRAP_UNIMP;
		    break;
		}
		if (!(sregs->psr & PSR_S)) {
		    sregs->trap = TRAP_PRIVI;
		    break;
		}
		sregs->psr = (rs1 ^ operand2) & 0x00f03fff;
		break;
	    case WRWIM:
		if (!(sregs->psr & PSR_S)) {
		    sregs->trap = TRAP_PRIVI;
		    break;
		}
		sregs->wim = (rs1 ^ operand2) & 0x0ff;
		break;
	    case WRTBR:
		if (!(sregs->psr & PSR_S)) {
		    sregs->trap = TRAP_PRIVI;
		    break;
		}
		sregs->tbr = (sregs->tbr & 0x00000ff0) |
		    ((rs1 ^ operand2) & 0xfffff000);
		break;
	    case WRY:
		sregs->y = (rs1 ^ operand2);
		break;
	    case JMPL:

#ifdef IUREV0
		if (iurev0)
		    sregs->jmpltime = sregs->ninst;
#endif
#ifdef STAT
		sregs->nbranch++;
#endif
		sregs->icnt = T_JMPL;	/* JMPL takes two cycles */
		if (rs1 & 0x3) {
		    sregs->trap = TRAP_UNALI;
		    break;
		}
		*rdd = sregs->pc;
		npc = rs1 + operand2;
		break;
	    case RETT:
#ifdef IUREV0
		if (iurev0 && sregs->rett_err) {
		    sregs->rett_err = 0;
		    if (sis_verbose)
			printf("IU rev.0 bug mode reset\n");
		}
#endif

		address = rs1 + operand2;
		new_cwp = ((sregs->psr & PSR_CWP) + 1) & PSR_CWP;
		sregs->icnt = T_RETT;	/* RETT takes two cycles */
		if (sregs->psr & PSR_ET) {
		    sregs->trap = TRAP_UNIMP;
		    break;
		}
		if (!(sregs->psr & PSR_S)) {
		    sregs->trap = TRAP_PRIVI;
		    break;
		}
		if (sregs->wim & (1 << new_cwp)) {
		    sregs->trap = TRAP_WUFL;
		    break;
		}
		if (address & 0x3) {
		    sregs->trap = TRAP_UNALI;
		    break;
		}
		sregs->psr = (sregs->psr & ~PSR_CWP) | new_cwp | PSR_ET;
		sregs->psr =
		    (sregs->psr & ~PSR_S) | ((sregs->psr & PSR_PS) << 1);
		npc = address;
		break;

	    default:
		sregs->trap = TRAP_UNIMP;
		break;
	    }
	}
	break;
    case 3:			/* Load/store instructions */

	address = rs1 + operand2;

	/* Check for load/store to alternate address space */

	if ((op3 >> 4) == 1) {
	    if (!(sregs->psr & PSR_S)) {
		sregs->trap = TRAP_PRIVI;
		break;
	    } else if (sregs->inst & INST_I) {
		sregs->trap = TRAP_UNIMP;
		break;
	    } else
		asi = (sregs->inst >> 5) & 0x0ff;
	} else {
	    if (sregs->psr & PSR_S)
		asi = 11;
	    else
		asi = 10;
#ifdef IUREV0
	    if (iurev0 && sregs->rett_err) {
		asi &= ~0x1;
		asi |= ((sregs->psr & PSR_PS) >> 6);
	    }
#endif
	}

	if (op3 & 4) {
	    sregs->icnt = T_ST;	/* Set store instruction count */
#ifdef STAT
	    sregs->nstore++;
#endif
	} else {
	    sregs->icnt = T_LD;	/* Set load instruction count */
#ifdef STAT
	    sregs->nload++;
#endif
	}

	/* Decode load/store instructions */

	switch (op3) {
	case LDDA:
	case LDD:
	    if (address & 0x7) {
		sregs->trap = TRAP_UNALI;
		break;
	    }
	    if (rd & 1) {
		rd &= 0x1e;
		if (rd > 7)
		    rdd = &(sregs->r[(cwp + rd) & 0x7f]);
		else
		    rdd = &(sregs->g[rd]);
	    }
	    mexc = memory_read(asi, address, &data, &ws);
	    sregs->hold += ws;
	    sregs->icnt = T_LDD;
	    if (mexc) {
		sregs->trap = TRAP_DEXC;
	    } else {
		rdd[0] = data;
		address += 4;
		mexc = memory_read(asi, address, &data, &ws);
		sregs->hold += ws;
#ifdef STAT
		sregs->nload++;	/* Double load counts twice */
#endif
		if (mexc) {
		    sregs->trap = TRAP_DEXC;
		} else {
		    rdd[1] = data;
		}
	    }
	    break;

	case LDA:
	case LD:
	    if (address & 0x3) {
		sregs->trap = TRAP_UNALI;
		break;
	    }
	    mexc = memory_read(asi, address, &data, &ws);
	    sregs->hold += ws;
	    if (mexc) {
		sregs->trap = TRAP_DEXC;
	    } else {
		*rdd = data;
	    }
	    break;
	case LDSTUB:
	case LDSTUBA:
	    mexc = memory_read(asi, address, &data, &ws);
	    sregs->hold += ws;
	    sregs->icnt = T_LDST;
	    if (mexc) {
		sregs->trap = TRAP_DEXC;
		break;
	    }
	    data = (data >> ((3 - (address & 0x3)) << 3)) & 0x0ff;
	    *rdd = data;
	    data = 0x0ff;
	    mexc = memory_write(asi, address, &data, 0, &ws);
	    sregs->hold += ws;
	    if (mexc) {
		sregs->trap = TRAP_DEXC;
	    }
#ifdef STAT
	    sregs->nload++;
#endif
	    break;
	case LDSBA:
	case LDUBA:
	case LDSB:
	case LDUB:
	    mexc = memory_read(asi, address, &data, &ws);
	    sregs->hold += ws;
	    if (mexc) {
		sregs->trap = TRAP_DEXC;
		break;
	    }
	    data = (data >> ((3 - (address & 0x3)) << 3)) & 0x0ff;
	    if ((op3 == LDSB) && (data >> 7))
		data |= 0xffffff00;
	    *rdd = data;
	    break;
	case LDSHA:
	case LDUHA:
	case LDSH:
	case LDUH:
	    if (address & 0x1) {
		sregs->trap = TRAP_UNALI;
		break;
	    }
	    mexc = memory_read(asi, address, &data, &ws);
	    sregs->hold += ws;
	    if (mexc) {
		sregs->trap = TRAP_DEXC;
		break;
	    }
	    if (!(address & 0x2))
		data >>= 16;
	    data &= 0x0ffff;
	    if ((op3 == LDSH) && (data >> 15))
		data |= 0xffff0000;
	    *rdd = data;
	    break;
	case LDF:
	    if (!((sregs->psr & PSR_EF) && chk_fp(sregs))) {
		sregs->trap = TRAP_FPDIS;
		break;
	    }
	    if (address & 0x3) {
		sregs->trap = TRAP_UNALI;
		break;
	    }
	    if (ebase.simtime < sregs->ftime) {
		if ((sregs->frd == rd) || (sregs->frs1 == rd) ||
		    (sregs->frs2 == rd))
		    sregs->fhold += (sregs->ftime - ebase.simtime);
	    }
	    mexc = memory_read(asi, address, &data, &ws);
	    sregs->hold += ws;
	    sregs->flrd = rd;
	    sregs->ltime = ebase.simtime + sregs->icnt + FLSTHOLD +
		sregs->hold + sregs->fhold;
	    if (mexc) {
		sregs->trap = TRAP_DEXC;
	    } else {
		sregs->fs[rd] = *((float32 *) & data);
	    }
	    break;
	case LDDF:
	    if (!((sregs->psr & PSR_EF) && chk_fp(sregs))) {
		sregs->trap = TRAP_FPDIS;
		break;
	    }
	    if (address & 0x7) {
		sregs->trap = TRAP_UNALI;
		break;
	    }
	    if (ebase.simtime < sregs->ftime) {
		if (((sregs->frd >> 1) == (rd >> 1)) ||
		    ((sregs->frs1 >> 1) == (rd >> 1)) ||
		    ((sregs->frs2 >> 1) == (rd >> 1)))
		    sregs->fhold += (sregs->ftime - ebase.simtime);
	    }
	    mexc = memory_read(asi, address, &data, &ws);
	    sregs->hold += ws;
	    sregs->icnt = T_LDD;
	    if (mexc) {
		sregs->trap = TRAP_DEXC;
	    } else {
		rd &= 0x1E;
		sregs->flrd = rd;
		sregs->fs[rd] = *((float32 *) & data);
		mexc = memory_read(asi, address + 4, &data, &ws);
		sregs->hold += ws;
#ifdef STAT
		sregs->nload++;	/* Double load counts twice */
#endif
		if (mexc) {
		    sregs->trap = TRAP_DEXC;
		} else {
		    sregs->fs[rd + 1] = *((float32 *) & data);
		    sregs->ltime = ebase.simtime + sregs->icnt + FLSTHOLD +
			sregs->hold + sregs->fhold;
		}
	    }
	    break;
	case LDFSR:
	    if (ebase.simtime < sregs->ftime) {
		sregs->fhold += (sregs->ftime - ebase.simtime);
	    }
	    if (!((sregs->psr & PSR_EF) && chk_fp(sregs))) {
		sregs->trap = TRAP_FPDIS;
		break;
	    }
	    if (address & 0x3) {
		sregs->trap = TRAP_UNALI;
		break;
	    }
	    mexc = memory_read(asi, address, &data, &ws);
	    sregs->hold += ws;
	    if (mexc) {
		sregs->trap = TRAP_DEXC;
	    } else {
		sregs->fsr =
		    (sregs->fsr & 0x7FF000) | (data & ~0x7FF000);
		set_fsr(sregs->fsr);
	    }
	    break;
	case STFSR:
	    if (!((sregs->psr & PSR_EF) && chk_fp(sregs))) {
		sregs->trap = TRAP_FPDIS;
		break;
	    }
	    if (address & 0x3) {
		sregs->trap = TRAP_UNALI;
		break;
	    }
	    if (ebase.simtime < sregs->ftime) {
		sregs->fhold += (sregs->ftime - ebase.simtime);
	    }
	    mexc = memory_write(asi, address, &sregs->fsr, 2, &ws);
	    sregs->hold += ws;
	    if (mexc) {
		sregs->trap = TRAP_DEXC;
	    }
	    break;

	case ST:
	case STA:
	    if (address & 0x3) {
		sregs->trap = TRAP_UNALI;
		break;
	    }
	    mexc = memory_write(asi, address, rdd, 2, &ws);
	    sregs->hold += ws;
	    if (mexc) {
		sregs->trap = TRAP_DEXC;
	    }
	    break;
	case STB:
	case STBA:
	    mexc = memory_write(asi, address, rdd, 0, &ws);
	    sregs->hold += ws;
	    if (mexc) {
		sregs->trap = TRAP_DEXC;
	    }
	    break;
	case STD:
	case STDA:
	    if (address & 0x7) {
		sregs->trap = TRAP_UNALI;
		break;
	    }
	    if (rd & 1) {
		rd &= 0x1e;
		if (rd > 7)
		    rdd = &(sregs->r[(cwp + rd) & 0x7f]);
		else
		    rdd = &(sregs->g[rd]);
	    }
	    mexc = memory_write(asi, address, rdd, 3, &ws);
	    sregs->hold += ws;
	    sregs->icnt = T_STD;
#ifdef STAT
	    sregs->nstore++;	/* Double store counts twice */
#endif
	    if (mexc) {
		sregs->trap = TRAP_DEXC;
		break;
	    }
	    break;
	case STDFQ:
	    if ((sregs->psr & 0x1f) > 7) {
		sregs->trap = TRAP_UNIMP;
		break;
	    }
	    if (!((sregs->psr & PSR_EF) && chk_fp(sregs))) {
		sregs->trap = TRAP_FPDIS;
		break;
	    }
	    if (address & 0x7) {
		sregs->trap = TRAP_UNALI;
		break;
	    }
	    if (!(sregs->fsr & FSR_QNE)) {
		sregs->fsr = (sregs->fsr & ~FSR_TT) | FP_SEQ_ERR;
		break;
	    }
	    rdd = &(sregs->fpq[0]);
	    mexc = memory_write(asi, address, rdd, 3, &ws);
	    sregs->hold += ws;
	    sregs->icnt = T_STD;
#ifdef STAT
	    sregs->nstore++;	/* Double store counts twice */
#endif
	    if (mexc) {
		sregs->trap = TRAP_DEXC;
		break;
	    } else {
		sregs->fsr &= ~FSR_QNE;
		sregs->fpstate = FP_EXE_MODE;
	    }
	    break;
	case STHA:
	case STH:
	    if (address & 0x1) {
		sregs->trap = TRAP_UNALI;
		break;
	    }
	    mexc = memory_write(asi, address, rdd, 1, &ws);
	    sregs->hold += ws;
	    if (mexc) {
		sregs->trap = TRAP_DEXC;
	    }
	    break;
	case STF:
	    if (!((sregs->psr & PSR_EF) && chk_fp(sregs))) {
		sregs->trap = TRAP_FPDIS;
		break;
	    }
	    if (address & 0x3) {
		sregs->trap = TRAP_UNALI;
		break;
	    }
	    if (ebase.simtime < sregs->ftime) {
		if (sregs->frd == rd)
		    sregs->fhold += (sregs->ftime - ebase.simtime);
	    }
	    mexc = memory_write(asi, address, &sregs->fsi[rd], 2, &ws);
	    sregs->hold += ws;
	    if (mexc) {
		sregs->trap = TRAP_DEXC;
	    }
	    break;
	case STDF:
	    if (!((sregs->psr & PSR_EF) && chk_fp(sregs))) {
		sregs->trap = TRAP_FPDIS;
		break;
	    }
	    if (address & 0x7) {
		sregs->trap = TRAP_UNALI;
		break;
	    }
	    rd &= 0x1E;
	    if (ebase.simtime < sregs->ftime) {
		if ((sregs->frd == rd) || (sregs->frd + 1 == rd))
		    sregs->fhold += (sregs->ftime - ebase.simtime);
	    }
	    mexc = memory_write(asi, address, &sregs->fsi[rd], 3, &ws);
	    sregs->hold += ws;
	    sregs->icnt = T_STD;
#ifdef STAT
	    sregs->nstore++;	/* Double store counts twice */
#endif
	    if (mexc) {
		sregs->trap = TRAP_DEXC;
	    }
	    break;
	case SWAP:
	case SWAPA:
	    if (address & 0x3) {
		sregs->trap = TRAP_UNALI;
		break;
	    }
	    mexc = memory_read(asi, address, &data, &ws);
	    sregs->hold += ws;
	    if (mexc) {
		sregs->trap = TRAP_DEXC;
		break;
	    }
	    mexc = memory_write(asi, address, rdd, 2, &ws);
	    sregs->hold += ws;
	    sregs->icnt = T_LDST;
	    if (mexc) {
		sregs->trap = TRAP_DEXC;
		break;
	    } else
		*rdd = data;
#ifdef STAT
	    sregs->nload++;
#endif
	    break;


	default:
	    sregs->trap = TRAP_UNIMP;
	    break;
	}

#ifdef LOAD_DEL

	if (!(op3 & 4)) {
	    sregs->ildtime = ebase.simtime + sregs->hold + sregs->icnt;
	    sregs->ildreg = rd;
	    if ((op3 | 0x10) == 0x13)
		sregs->ildreg |= 1;	/* Double load, odd register loaded
					 * last */
	}
#endif
	break;

    default:
	sregs->trap = TRAP_UNIMP;
	break;
    }
    sregs->g[0] = 0;
    if (!sregs->trap) {
	sregs->pc = pc;
	sregs->npc = npc;
    }
    return (0);
}

#define T_FABSs		2
#define T_FADDs		4
#define T_FADDd		4
#define T_FCMPs		4
#define T_FCMPd		4
#define T_FDIVs		20
#define T_FDIVd		35
#define T_FMOVs		2
#define T_FMULs		5
#define T_FMULd		9
#define T_FNEGs		2
#define T_FSQRTs	37
#define T_FSQRTd	65
#define T_FSUBs		4
#define T_FSUBd		4
#define T_FdTOi		7
#define T_FdTOs		3
#define T_FiTOs		6
#define T_FiTOd		6
#define T_FsTOi		6
#define T_FsTOd		2

#define FABSs	0x09
#define FADDs	0x41
#define FADDd	0x42
#define FCMPs	0x51
#define FCMPd	0x52
#define FCMPEs	0x55
#define FCMPEd	0x56
#define FDIVs	0x4D
#define FDIVd	0x4E
#define FMOVs	0x01
#define FMULs	0x49
#define FMULd	0x4A
#define FNEGs	0x05
#define FSQRTs	0x29
#define FSQRTd	0x2A
#define FSUBs	0x45
#define FSUBd	0x46
#define FdTOi	0xD2
#define FdTOs	0xC6
#define FiTOs	0xC4
#define FiTOd	0xC8
#define FsTOi	0xD1
#define FsTOd	0xC9


int
fpexec(op3, rd, rs1, rs2, sregs)
    uint32          op3, rd, rs1, rs2;
    struct pstate  *sregs;
{
    uint32          opf, tem, accex;
    float32         ftmps;
    float64         ftmpd;
    int32           fcc;
    char           *res;
    uint32          ldadj;

    if (sregs->fpstate == FP_EXC_MODE) {
	sregs->fsr = (sregs->fsr & ~FSR_TT) | FP_SEQ_ERR;
	sregs->fpstate == FP_EXC_PE;
	return (0);
    }
    if (sregs->fpstate == FP_EXC_PE) {
	sregs->fpstate = FP_EXC_MODE;
	return (TRAP_FPEXC);
    }
    opf = (sregs->inst >> 5) & 0x1ff;

    /*
     * Check if we already have an FPop in the pipe. If so, halt until it is
     * finished by incrementing fhold with the remaining execution time
     */

    if (ebase.simtime < sregs->ftime) {
	sregs->fhold = (sregs->ftime - ebase.simtime);
    } else {
	sregs->fhold = 0;

	/* Check load dependencies. */

	if (ebase.simtime < sregs->ltime) {

	    /* Don't check rs1 if single operand instructions */

	    if (((opf >> 6) == 0) || ((opf >> 6) == 3))
		rs1 = 32;

	    /* Adjust for double floats */

	    ldadj = opf & 1;
	    if (!(((sregs->flrd - rs1) >> ldadj) && ((sregs->flrd - rs2) >> ldadj)))
		sregs->fhold++;
	}
    }

    sregs->finst++;

    sregs->frs1 = rs1;		/* Store src and dst for dependecy check */
    sregs->frs2 = rs2;
    sregs->frd = rd;

    sregs->ftime = ebase.simtime + sregs->hold + sregs->fhold;

    /* SPARC is big-endian - swap double floats if host is little-endian */
    /* This is ugly - I know ... */
#ifdef HOST_LITTLE_ENDIAN_FLOAT
    rs1 &= 0x1f;
    switch (opf) {
	case FADDd:
	case FDIVd:
	case FMULd:
	case FSQRTd:
	case FSUBd:
        case FCMPd:
        case FCMPEd:
	case FdTOi:
	case FdTOs:
    	    sregs->fdp[rs1 | 1] = sregs->fs[rs1 & ~1];
    	    sregs->fdp[rs1 & ~1] = sregs->fs[rs1 | 1];
    	    sregs->fdp[rs2 | 1] = sregs->fs[rs2 & ~1];
    	    sregs->fdp[rs2 & ~1] = sregs->fs[rs2 | 1];
    default:
    }
#endif

    clear_accex();

    switch (opf) {
    case FABSs:
	sregs->fs[rd] = fabs(sregs->fs[rs2]);
	sregs->ftime += T_FABSs;
	sregs->frs1 = 32;	/* rs1 ignored */
	break;
    case FADDs:
	sregs->fs[rd] = sregs->fs[rs1] + sregs->fs[rs2];
	sregs->ftime += T_FADDs;
	break;
    case FADDd:
	sregs->fd[rd >> 1] = sregs->fd[rs1 >> 1] + sregs->fd[rs2 >> 1];
	sregs->ftime += T_FADDd;
	break;
    case FCMPs:
    case FCMPEs:
	if (sregs->fs[rs1] == sregs->fs[rs2])
	    fcc = 3;
	else if (sregs->fs[rs1] < sregs->fs[rs2])
	    fcc = 2;
	else if (sregs->fs[rs1] > sregs->fs[rs2])
	    fcc = 1;
	else
	    fcc = 0;
	sregs->fsr |= 0x0C00;
	sregs->fsr &= ~(fcc << 10);
	sregs->ftime += T_FCMPs;
	sregs->frd = 32;	/* rd ignored */
	if ((fcc == 0) && (opf == FCMPEs)) {
	    sregs->fpstate == FP_EXC_PE;
	    sregs->fsr = (sregs->fsr & ~0x1C000) | (1 << 14);
	}
	break;
    case FCMPd:
    case FCMPEd:
	if (sregs->fd[rs1 >> 1] == sregs->fd[rs2 >> 1])
	    fcc = 3;
	else if (sregs->fd[rs1 >> 1] < sregs->fd[rs2 >> 1])
	    fcc = 2;
	else if (sregs->fd[rs1 >> 1] > sregs->fd[rs2 >> 1])
	    fcc = 1;
	else
	    fcc = 0;
	sregs->fsr |= 0x0C00;
	sregs->fsr &= ~(fcc << 10);
	sregs->ftime += T_FCMPd;
	sregs->frd = 32;	/* rd ignored */
	if ((fcc == 0) && (opf == FCMPEd)) {
	    sregs->fpstate == FP_EXC_PE;
	    sregs->fsr = (sregs->fsr & ~FSR_TT) | FP_IEEE;
	}
	break;
    case FDIVs:
	sregs->fs[rd] = sregs->fs[rs1] / sregs->fs[rs2];
	sregs->ftime += T_FDIVs;
	break;
    case FDIVd:
	sregs->fd[rd >> 1] = sregs->fd[rs1 >> 1] / sregs->fd[rs2 >> 1];
	sregs->ftime += T_FDIVd;
	break;
    case FMOVs:
	sregs->fs[rd] = sregs->fs[rs2];
	sregs->ftime += T_FMOVs;
	sregs->frs1 = 32;	/* rs1 ignored */
	break;
    case FMULs:
	sregs->fs[rd] = sregs->fs[rs1] * sregs->fs[rs2];
	sregs->ftime += T_FMULs;
	break;
    case FMULd:
	sregs->fd[rd >> 1] = sregs->fd[rs1 >> 1] * sregs->fd[rs2 >> 1];
	sregs->ftime += T_FMULd;
	break;
    case FNEGs:
	sregs->fs[rd] = -sregs->fs[rs2];
	sregs->ftime += T_FNEGs;
	sregs->frs1 = 32;	/* rs1 ignored */
	break;
    case FSQRTs:
	if (sregs->fs[rs2] < 0.0) {
	    sregs->fpstate == FP_EXC_PE;
	    sregs->fsr = (sregs->fsr & ~FSR_TT) | FP_IEEE;
	    sregs->fsr = (sregs->fsr & 0x1f) | 0x10;
	    break;
	}
	sregs->fs[rd] = sqrt(sregs->fs[rs2]);
	sregs->ftime += T_FSQRTs;
	sregs->frs1 = 32;	/* rs1 ignored */
	break;
    case FSQRTd:
	if (sregs->fd[rs2 >> 1] < 0.0) {
	    sregs->fpstate == FP_EXC_PE;
	    sregs->fsr = (sregs->fsr & ~FSR_TT) | FP_IEEE;
	    sregs->fsr = (sregs->fsr & 0x1f) | 0x10;
	    break;
	}
	sregs->fd[rd >> 1] = sqrt(sregs->fd[rs2 >> 1]);
	sregs->ftime += T_FSQRTd;
	sregs->frs1 = 32;	/* rs1 ignored */
	break;
    case FSUBs:
	sregs->fs[rd] = sregs->fs[rs1] - sregs->fs[rs2];
	sregs->ftime += T_FSUBs;
	break;
    case FSUBd:
	sregs->fd[rd >> 1] = sregs->fd[rs1 >> 1] - sregs->fd[rs2 >> 1];
	sregs->ftime += T_FSUBd;
	break;
    case FdTOi:
	sregs->fsi[rd] = (int) sregs->fd[rs2 >> 1];
	sregs->ftime += T_FdTOi;
	sregs->frs1 = 32;	/* rs1 ignored */
	break;
    case FdTOs:
	sregs->fs[rd] = (float32) sregs->fd[rs2 >> 1];
	sregs->ftime += T_FdTOs;
	sregs->frs1 = 32;	/* rs1 ignored */
	break;
    case FiTOs:
	sregs->fs[rd] = (float32) sregs->fsi[rs2];
	sregs->ftime += T_FiTOs;
	sregs->frs1 = 32;	/* rs1 ignored */
	break;
    case FiTOd:
	sregs->fd[rd >> 1] = (float64) sregs->fsi[rs2];
	sregs->ftime += T_FiTOd;
	sregs->frs1 = 32;	/* rs1 ignored */
	break;
    case FsTOi:
	sregs->fsi[rd] = (int) sregs->fs[rs2];
	sregs->ftime += T_FsTOi;
	sregs->frs1 = 32;	/* rs1 ignored */
	break;
    case FsTOd:
	sregs->fd[rd >> 1] = sregs->fs[rs2];
	sregs->ftime += T_FsTOd;
	sregs->frs1 = 32;	/* rs1 ignored */
	break;

    default:
	sregs->fsr = (sregs->fsr & ~FSR_TT) | FP_UNIMP;
	sregs->fpstate == FP_EXC_PE;
    }

    accex = get_accex();

#ifdef HOST_LITTLE_ENDIAN_FLOAT
    switch (opf) {
    case FADDd:
    case FDIVd:
    case FMULd:
    case FSQRTd:
    case FSUBd:
    case FiTOd:
    case FsTOd:
	sregs->fs[rd & ~1] = sregs->fdp[rd | 1];
	sregs->fs[rd | 1] = sregs->fdp[rd & ~1];
    default:
    }
#endif
    if (sregs->fpstate == FP_EXC_PE) {
	sregs->fpq[0] = sregs->pc;
	sregs->fpq[1] = sregs->inst;
	sregs->fsr |= FSR_QNE;
    } else {
	tem = (sregs->fsr >> 23) & 0x1f;
	if (tem & accex) {
	    sregs->fpstate = FP_EXC_PE;
	    sregs->fsr = (sregs->fsr & ~FSR_TT) | FP_IEEE;
	    sregs->fsr = ((sregs->fsr & ~0x1f) | accex);
	} else {
	    sregs->fsr = ((((sregs->fsr >> 5) | accex) << 5) | accex);
	}
	if (sregs->fpstate == FP_EXC_PE) {
	    sregs->fpq[0] = sregs->pc;
	    sregs->fpq[1] = sregs->inst;
	    sregs->fsr |= FSR_QNE;
	}
    }
    clear_accex();

    return (0);


}

int
execute_trap(sregs)
    struct pstate  *sregs;
{
    int32           cwp;

    if (sregs->trap == 256) {
	sregs->pc = 0;
	sregs->npc = 4;
	sregs->trap = 0;
    } else {

	if ((sregs->psr & PSR_ET) == 0)
	    return (ERROR);

	sregs->tbr = (sregs->tbr & 0xfffff000) | (sregs->trap << 4);
	sregs->trap = 0;
	sregs->psr &= ~PSR_ET;
	sregs->psr |= ((sregs->psr & PSR_S) >> 1);
	sregs->annul = 0;
	sregs->psr = (((sregs->psr & PSR_CWP) - 1) & 0x7) | (sregs->psr & ~PSR_CWP);
	cwp = ((sregs->psr & PSR_CWP) << 4);
	sregs->r[(cwp + 17) & 0x7f] = sregs->pc;
	sregs->r[(cwp + 18) & 0x7f] = sregs->npc;
	sregs->psr |= PSR_S;
	sregs->pc = sregs->tbr;
	sregs->npc = sregs->tbr + 4;

	/* Increase simulator time */
	sregs->icnt = TRAP_C;

    }


    return (0);

}

extern struct irqcell irqarr[16];

void
check_interrupts(sregs)
    struct pstate  *sregs;
{
    if ((ext_irl) && (sregs->psr & PSR_ET) &&
	((ext_irl == 15) || (ext_irl > ((sregs->psr & PSR_PIL) >> 8)))) {
	if (sregs->trap == 0) {
	    sregs->trap = 16 + ext_irl;
	    irqarr[ext_irl & 0x0f].callback(irqarr[ext_irl & 0x0f].arg);
	    clear_int(ext_irl);
	}
    }
}

init_regs(sregs)
    struct pstate  *sregs;
{
    int32           i;

    sregs->pc = 0;
    sregs->npc = 4;
    sregs->trap = 0;
    sregs->psr &= 0x00f03fdf;
    sregs->psr |= 0x080;	/* Set supervisor bit */
    sregs->breakpoint = 0;
    sregs->annul = 0;
    sregs->fpstate = FP_EXE_MODE;
    sregs->fpqn = 0;
    sregs->ftime = 0;
    sregs->ltime = 0;
    sregs->err_mode = 0;
    ext_irl = 0;
    irqpend = 0;
    sregs->g[0] = 0;
#ifdef HOST_LITTLE_ENDIAN_FLOAT
    sregs->fdp = (float32 *) sregs->fd;
    sregs->fsi = (int32 *) sregs->fs;
#else
    sregs->fs = (float32 *) sregs->fd;
    sregs->fsi = (int32 *) sregs->fd;
#endif
    sregs->fsr = 0;
    sregs->fpu_pres = !nfp;
    set_fsr(sregs->fsr);
    sregs->bphit = 0;
    sregs->ildreg = 0;
    sregs->ildtime = 0;

    sregs->rett_err = 0;
    sregs->jmpltime = 0;
}

chk_fp(sregs)
    struct pstate  *sregs;
{
    return (sregs->fpu_pres);
}
