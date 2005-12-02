/*	$OpenBSD: db_disasm.c,v 1.6 2005/12/02 20:01:33 miod Exp $	*/
/*
 * Mach Operating System
 * Copyright (c) 1993-1991 Carnegie Mellon University
 * Copyright (c) 1991 OMRON Corporation
 * All Rights Reserved.
 *
 * Permission to use, copy, modify and distribute this software and its
 * documentation is hereby granted, provided that both the copyright
 * notice and this permission notice appear in all copies of the
 * software, derivative works or modified versions, and any portions
 * thereof, and that both notices appear in supporting documentation.
 *
 * CARNEGIE MELLON AND OMRON ALLOW FREE USE OF THIS SOFTWARE IN ITS "AS IS"
 * CONDITION.  CARNEGIE MELLON AND OMRON DISCLAIM ANY LIABILITY OF ANY KIND
 * FOR ANY DAMAGES WHATSOEVER RESULTING FROM THE USE OF THIS SOFTWARE.
 *
 * Carnegie Mellon requests users of this software to return to
 *
 *  Software Distribution Coordinator  or  Software.Distribution@CS.CMU.EDU
 *  School of Computer Science
 *  Carnegie Mellon University
 *  Pittsburgh PA 15213-3890
 *
 * any improvements or extensions that they make and grant Carnegie the
 * rights to redistribute these changes.
 */

/*
 * m88k disassembler for use in ddb
 */

#include <sys/param.h>
#include <sys/types.h>

#include <machine/db_machdep.h>

#include <ddb/db_sym.h>		/* DB_STGY_PROC, db_printsym() */
#include <ddb/db_access.h>	/* db_get_value() */
#include <ddb/db_output.h>	/* db_printf() */
#include <ddb/db_interface.h>

static const char *instwidth[4] = {
	".d", "  ", ".h", ".b"
};

static const char *condname[6] = {
	"gt0 ", "eq0 ", "ge0 ", "lt0 ", "ne0 ", "le0 "
};

#ifdef M88100
static const char *m88100_ctrlreg[64] = {
	"cr0(PID)   ",
	"cr1(PSR)   ",
	"cr2(EPSR)  ",
	"cr3(SSBR)  ",
	"cr4(SXIP)  ",
	"cr5(SNIP)  ",
	"cr6(SFIP)  ",
	"cr7(VBR)   ",
	"cr8(DMT0)  ",
	"cr9(DMD0)  ",
	"cr10(DMA0) ",
	"cr11(DMT1) ",
	"cr12(DMD1) ",
	"cr13(DMA1) ",
	"cr14(DMT2) ",
	"cr15(DMD2) ",
	"cr16(DMA2) ",
	"cr17(SR0)  ",
	"cr18(SR1)  ",
	"cr19(SR2)  ",
	"cr20(SR3)  ",
	"fcr0(FPECR)",
	"fcr1(FPHS1)",
	"fcr2(FPLS1)",
	"fcr3(FPHS2)",
	"fcr4(FPLS2)",
	"fcr5(FPPT) ",
	"fcr6(FPRH) ",
	"fcr7(FPRL) ",
	"fcr8(FPIT) ",
	NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL,
	NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL,
	NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL,
	NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL,
	"fcr62(FPSR)",
	"fcr63(FPCR)"
};
#endif
#ifdef M88110
static const char *m88110_ctrlreg[64] = {
	"cr0(PID)   ",
	"cr1(PSR)   ",
	"cr2(EPSR)  ",
	NULL,
	"cr4(EXIP)  ",
	"cr5(ENIP)  ",
	NULL,
	"cr7(VBR)   ",
	NULL,
	NULL,
	NULL,
	NULL,
	NULL,
	NULL,
	"cr14(RES1) ",
	"cr15(RES2) ",
	"cr16(SRX)  ",
	"cr17(SR0)  ",
	"cr18(SR1)  ",
	"cr19(SR2)  ",
	"cr20(SR3)  ",
	"fcr0(FPECR)",
	NULL,
	NULL,
	NULL,
	"cr25(ICMD) ",
	"cr26(ICTL) ",
	"cr27(ISAR) ",
	"cr28(ISAP) ",
	"cr29(IUAP) ",
	"cr30(IIR)  ",
	"cr31(IBP)  ",
	"cr32(IPPU) ",
	"cr33(IPPL) ",
	"cr34(ISR)  ",
	"cr35(ILAR) ",
	"cr36(IPAR) ",
	NULL,
	NULL,
	NULL,
	"cr40(DCMD) ",
	"cr41(DCTL) ",
	"cr42(DSAR) ",
	"cr43(DSAP) ",
	"cr44(DUAP) ",
	"cr45(DIR)  ",
	"cr46(DBP)  ",
	"cr47(DPPU) ",
	"cr48(DPPL) ",
	"cr49(DSR)  ",
	"cr50(DLAR) ",
	"cr51(DPAR) ",
	NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL,
	NULL, NULL,
	"fcr62(FPSR)",
	"fcr63(FPCR)"
};
#endif
#if defined(M88100) && defined(M88110)
#define	ctrlreg	(CPU_IS88100 ? m88100_ctrlreg : m88110_ctrlreg)
#elif defined(M88100)
#define	ctrlreg	m88100_ctrlreg
#else
#define	ctrlreg	m88110_ctrlreg
#endif

#define printval(x) \
	do { \
		if ((x) < 0) \
			db_printf("-0x%X", -(x)); \
		else \
			db_printf("0x%X", (x));	\
	} while (0)

/* prototypes */
void oimmed(int, const char *, long);
void ctrlregs(int, const char *, long);
void printsod(int);
void sindou(int, const char *, long);
void jump(int, const char *, long);
void instset(int, const char *, long);
void symofset(int, int, int);
void obranch(int, const char *, long);
void brcond(int, const char *, long);
void otrap(int, const char *, long);
void obit(int, const char *, long);
void bitman(int, const char *, long);
void immem(int, const char *, long);
void nimmem(int, const char *, long);
void lognim(int, const char *, long);
void onimmed(int, const char *, long);

/* Handlers immediate integer arithmetic instructions */
void
oimmed(int inst, const char *opcode, long iadr)
{
	int Linst = inst & 0177777;
	int Hinst = inst >> 16;
	int H6inst = Hinst >> 10;
	int rs1 = Hinst & 037;
	int rd = (Hinst >> 5) & 037;

	if (H6inst > 017 && H6inst < 030 && (H6inst & 01) == 1)
		db_printf("\t%s.u", opcode);
	else
		db_printf("\t%s  ", opcode);
	db_printf("\t\tr%-3d,r%-3d,", rd, rs1);
	printval(Linst);
}

/* Handles instructions dealing with control registers */
void
ctrlregs(int inst, const char *opcode, long iadr)
{
	int L6inst = (inst >> 11) & 037;
	int creg = (inst >> 5) & 077;
	int rd = (inst >> 21) & 037;
	int rs1 = (inst >> 16) & 037;

	db_printf("\t%s", opcode);

	if (L6inst == 010 || L6inst == 011)
		db_printf("\t\tr%-3d,%s", rd, ctrlreg[creg]);
	else if (L6inst == 020 || L6inst == 021)
		db_printf("\t\tr%-3d,%s", rs1, ctrlreg[creg]);
	else
		db_printf("\t\tr%-3d,r%-3d,%s", rd, rs1, ctrlreg[creg]);
}

void
printsod(int t)
{
	if (t == 0)
		db_printf("s");
	else
		db_printf("d");
}

/* Handles floating point instructions */
void
sindou(int inst, const char *opcode, long iadr)
{
	int rs2 = inst & 037;
	int td = (inst >> 5) & 03;
	int t2 = (inst >> 7) & 03;
	int t1 = (inst >> 9) & 03;
	int rs1 = (inst >> 16) & 037;
	int rd = (inst >> 21) & 037;
	int checkbits = (inst >> 11) & 037;

	db_printf("\t%s.", opcode);
	printsod(td);
	if ((checkbits > 010 && checkbits < 014) || checkbits == 04) {
		printsod(t2);
		db_printf(" ");
		if (checkbits == 012 || checkbits == 013)
			db_printf("\t\tr%-3d,r%-3d", rd, rs2);
		else
			db_printf("\t\tr%-3d,r%-3d", rd, rs2);
	} else {
		printsod(t1);
		printsod(t2);
		db_printf("\t\tr%-3d,r%-3d,r%-3d", rd, rs1, rs2);
	}
}

void
jump(int inst, const char *opcode, long iadr)
{
	int rs2 = inst & 037;
	int Nbit = (inst >> 10) & 01;

	db_printf("\t%s", opcode);
	if (Nbit == 1)
		db_printf(".n");
	else
		db_printf("  ");
	db_printf("\t\tr%-3d", rs2);
}

/* Handles ff1, ff0, tbnd and rte instructions */
void
instset(int inst, const char *opcode, long iadr)
{
	int rs2 = inst & 037;
	int rs1 = (inst >> 16) & 037;
	int rd = (inst >> 21) & 037;
	int checkbits = (inst >> 10) & 077;
	int H6inst = (inst >> 26) & 077;

	db_printf("\t%s", opcode);
	if (H6inst == 076) {
		db_printf("\t\tr%-3d,", rs1);
		printval(inst & 0177777);
	} else if (checkbits == 072 || checkbits == 073)
		db_printf("\t\tr%-3d,r%-3d", rd, rs2);
	else if (checkbits == 076)
		db_printf("\t\tr%-3d,r%-3d", rs1, rs2);
}

void
symofset(int disp, int bit, int iadr)
{
	long addr;

	if (disp & (1 << (bit - 1))) {
		/* negative value */
		addr = iadr + ((disp << 2) | (~0 << bit));
	} else {
		addr = iadr + (disp << 2);
	}
	db_printsym(addr, DB_STGY_PROC, db_printf);
}

void
obranch(int inst, const char *opcode, long iadr)
{
	int cond = (inst >> 26) & 01;
	int disp = inst & 0377777777;

	if (cond == 0)
		db_printf("\t%s\t\t", opcode);
	else
		db_printf("\t%s.n\t\t", opcode);
	symofset(disp, 26, iadr);
}

/* Handles branch on conditions instructions */
void
brcond(int inst, const char *opcode, long iadr)
{
	int cond = (inst >> 26) & 1;
	int match = (inst >> 21) & 037;
	int rs = (inst >> 16) & 037;
	int disp = inst & 0177777;

	if (cond == 0)
		db_printf("\t%s\t\t", opcode);
	else
		db_printf("\t%s.n\t\t", opcode);
	if (((inst >> 27) & 03) == 1) {
		switch (match) {
		case 1:
			db_printf("%s,", condname[0]);
			break;
		case 2:
			db_printf("%s,", condname[1]);
			break;
		case 3:
			db_printf("%s,", condname[2]);
			break;
		case 12:
			db_printf("%s,", condname[3]);
			break;
		case 13:
			db_printf("%s,", condname[4]);
			break;
		case 14:
			db_printf("%s,", condname[5]);
			break;
		default:
			printval(match);
			db_printf(",");
			break;
		}
	} else {
		printval(match);
		db_printf(",");
	}

	db_printf("r%-3d,", rs);
	symofset(disp, 16, iadr);
}

void
otrap(int inst, const char *opcode, long iadr)
{
	int vecno = inst & 0777;
	int match = (inst >> 21) & 037;
	int rs = (inst >> 16) & 037;

	db_printf("\t%s\t", opcode);
	if (((inst >> 12) & 017) == 0xe) {
		switch (match) {
		case 1:
			db_printf("%s,", condname[0]);
			break;
		case 2:
			db_printf("%s,", condname[1]);
			break;
		case 3:
			db_printf("%s,", condname[2]);
			break;
		case 12:
			db_printf("%s,", condname[3]);
			break;
		case 13:
			db_printf("%s,", condname[4]);
			break;
		case 14:
			db_printf("%s,", condname[5]);
			break;
		default:
			printval(match);
			db_printf(",");
			break;
		}
	} else {
		printval(match);
		db_printf(",");
	}
	db_printf("\tr%-3d,", rs);
	printval(vecno);
}

/* Handles 10 bit immediate bit field operations */
void
obit(int inst, const char *opcode, long iadr)
{
	int rs = (inst >> 16) & 037;
	int rd = (inst >> 21) & 037;
	int width = (inst >> 5) & 037;
	int offset = inst & 037;

	db_printf("\t%s\t\tr%-3d,r%-3d,", opcode, rd, rs);
	if (((inst >> 10) & 077) != 052)
		printval(width);
	db_printf("<");
	printval(offset);
	db_printf(">");
}

/* Handles triadic mode bit field instructions */
void
bitman(int inst, const char *opcode, long iadr)
{
	int rs1 = (inst >> 16) & 037;
	int rd = (inst >> 21) & 037;
	int rs2 = inst & 037;

	db_printf("\t%s\t\tr%-3d,r%-3d,r%-3d", opcode, rd, rs1, rs2);
}

/* Handles immediate load/store/exchange instructions */
void
immem(int inst, const char *opcode, long iadr)
{
	int immed = inst & 0xFFFF;
	int rd = (inst >> 21) & 037;
	int rs = (inst >> 16) & 037;
	int st_lda = (inst >> 28) & 03;
	int aryno = (inst >> 26) & 03;
	char c = ' ';

	switch (st_lda) {
	case 0:
		if (aryno == 0 || aryno == 01)
			opcode = "xmem";
		else
			opcode = "ld";
		if (aryno == 0)
			aryno = 03;
		if (aryno != 01)
			c = 'u';
		break;
	case 1:
		opcode = "ld";
		break;
	}

	db_printf("\t%s%s%c\t\tr%-3d,r%-3d,",
	    opcode, instwidth[aryno], c, rd, rs);
	printval(immed);
}

/* Handles triadic mode load/store/exchange instructions */
void
nimmem(int inst, const char *opcode, long iadr)
{
	int scaled = (inst >> 9) & 01;
	int rd = (inst >> 21) & 037;
	int rs1 = (inst >> 16) & 037;
	int rs2 = inst & 037;
	int st_lda = (inst >> 12) & 03;
	int aryno = (inst >> 10) & 03;
	char c = ' ';
	const char *user;

	switch (st_lda) {
	case 0:
		if (aryno == 0 || aryno == 01)
			opcode = "xmem";
		else
			opcode = "ld";
		if (aryno == 0)
			aryno = 03;
		if (aryno != 01)
			c = 'u';
		break;
	case 1:
		opcode = "ld";
		break;
	}


	if (st_lda != 03 && ((inst >> 8) & 01) != 0)
		user = ".usr";
	else
		user = "    ";

	db_printf("\t%s%s%c%s\tr%-3d,r%-3d",
	    opcode, instwidth[aryno], c, user, rd, rs1);

	if (scaled)
		db_printf("[r%-3d]", rs2);
	else
		db_printf(",r%-3d", rs2);
}

/* Handles triadic mode logical instructions */
void
lognim(int inst, const char *opcode, long iadr)
{
	int rd = (inst >> 21) & 037;
	int rs1 = (inst >> 16) & 037;
	int rs2 = inst & 037;
	int complemt = (inst >> 10) & 01;
	char *c = "  ";

	if (complemt)
		c = ".c";

	db_printf("\t%s%s\t\tr%-3d,r%-3d,r%-3d", opcode, c, rd, rs1, rs2);
}

/* Handles triadic mode arithmetic instructions */
void
onimmed(int inst, const char *opcode, long iadr)
{
	int rd = (inst >> 21) & 037;
	int rs1 = (inst >> 16) & 037;
	int rs2 = inst & 037;
	int carry = (inst >> 8) & 03;
	int nochar = (inst >> 10) & 07;
	int nodecode = (inst >> 11) & 01;
	const char *tab, *c;

	if (nochar > 02)
		tab = "\t\t";
	else
		tab = "\t";

	if (!nodecode) {
		switch (carry) {
		case 01:
			c = ".co ";
			break;
		case 02:
			c = ".ci ";
			break;
		case 03:
			c = ".cio";
			break;
		default:
			c = "    ";
			break;
		}
	} else
		c = "    ";

	db_printf("\t%s%s%sr%-3d,r%-3d,r%-3d", opcode, c, tab, rd, rs1, rs2);
}

static const struct opdesc {
	u_int mask, match;
	void (*opfun)(int, const char *, long);
	const char *farg;
} opdecode[] = {
	/* ORDER IS IMPORTANT BELOW */
	{ 0xf0000000,	0x00000000,	immem,		NULL },
	{ 0xf0000000,	0x10000000,	immem,		NULL },
	{ 0xf0000000,	0x20000000,	immem,		"st" },
	{ 0xf0000000,	0x30000000,	immem,		"lda" },

	{ 0xf8000000,	0x40000000,	oimmed,		"and" },
	{ 0xf8000000,	0x48000000,	oimmed,		"mask" },
	{ 0xf8000000,	0x50000000,	oimmed,		"xor" },
	{ 0xf8000000,	0x58000000,	oimmed,		"or" },
	{ 0xfc000000,	0x60000000,	oimmed,		"addu" },
	{ 0xfc000000,	0x64000000,	oimmed,		"subu" },
	{ 0xfc000000,	0x68000000,	oimmed,		"divu" },
	{ 0xfc000000,	0x6c000000,	oimmed,		"mul" },
	{ 0xfc000000,	0x70000000,	oimmed,		"add" },
	{ 0xfc000000,	0x74000000,	oimmed,		"sub" },
	{ 0xfc000000,	0x78000000,	oimmed,		"div" },
	{ 0xfc000000,	0x7c000000,	oimmed,		"cmp" },

	{ 0xfc00f800,	0x80004000,	ctrlregs,	"ldcr" },
	{ 0xfc00f800,	0x80004800,	ctrlregs,	"fldcr" },
	{ 0xfc00f800,	0x80008000,	ctrlregs,	"stcr" },
	{ 0xfc00f800,	0x80008800,	ctrlregs,	"fstcr" },
	{ 0xfc00f800,	0x8000c000,	ctrlregs,	"xcr" },
	{ 0xfc00f800,	0x8000c800,	ctrlregs,	"fxcr" },

	{ 0xfc00f800,	0x84000000,	sindou,		"fmul" },
	{ 0xfc1fff80,	0x84002000,	sindou,		"flt" },
	{ 0xfc00f800,	0x84002800,	sindou,		"fadd" },
	{ 0xfc00f800,	0x84003000,	sindou,		"fsub" },
	{ 0xfc00f860,	0x84003800,	sindou,		"fcmp" },
	{ 0xfc1ffe60,	0x84004800,	sindou,		"int" },
	{ 0xfc1ffe60,	0x84005000,	sindou,		"nint" },
	{ 0xfc1ffe60,	0x84005800,	sindou,		"trnc" },
	{ 0xfc00f800,	0x84007000,	sindou,		"fdiv" },

	{ 0xf8000000,	0xc0000000,	obranch,	"br" },
	{ 0xf8000000,	0xc8000000,	obranch,	"bsr" },

	{ 0xf8000000,	0xd0000000,	brcond,		"bb0" },
	{ 0xf8000000,	0xd8000000,	brcond,		"bb1" },
	{ 0xf8000000,	0xe8000000,	brcond,		"bcnd" },

	{ 0xfc00fc00,	0xf0008000,	obit,		"clr" },
	{ 0xfc00fc00,	0xf0008800,	obit,		"set" },
	{ 0xfc00fc00,	0xf0009000,	obit,		"ext" },
	{ 0xfc00fc00,	0xf0009800,	obit,		"extu" },
	{ 0xfc00fc00,	0xf000a000,	obit,		"mak" },
	{ 0xfc00fc00,	0xf000a800,	obit,		"rot" },

	{ 0xfc00fe00,	0xf000d000,	otrap,		"tb0" },
	{ 0xfc00fe00,	0xf000d800,	otrap,		"tb1" },
	{ 0xfc00fe00,	0xf000e800,	otrap,		"tcnd" },

	{ 0xfc00f2e0,	0xf4000000,	nimmem,		NULL },
	{ 0xfc00f2e0,	0xf4000200,	nimmem,		NULL },
	{ 0xfc00f2e0,	0xf4001000,	nimmem,		NULL },
	{ 0xfc00f2e0,	0xf4001200,	nimmem,		NULL },
	{ 0xfc00f2e0,	0xf4002000,	nimmem,		"st" },
	{ 0xfc00f2e0,	0xf4002200,	nimmem,		"st" },
	{ 0xfc00f2e0,	0xf4003000,	nimmem,		"lda" },
	{ 0xfc00f2e0,	0xf4003200,	nimmem,		"lda" },

	{ 0xfc00fbe0,	0xf4004000,	lognim,		"and" },
	{ 0xfc00fbe0,	0xf4005000,	lognim,		"xor" },
	{ 0xfc00fbe0,	0xf4005800,	lognim,		"or" },

	{ 0xfc00fce0,	0xf4006000,	onimmed,	"addu" },
	{ 0xfc00fce0,	0xf4006400,	onimmed,	"subu" },
	{ 0xfc00fce0,	0xf4006800,	onimmed,	"divu" },
	{ 0xfc00fce0,	0xf4006c00,	onimmed,	"mul" },
	{ 0xfc00fce0,	0xf4007000,	onimmed,	"add" },
	{ 0xfc00fce0,	0xf4007400,	onimmed,	"sub" },
	{ 0xfc00fce0,	0xf4007800,	onimmed,	"div" },
	{ 0xfc00fce0,	0xf4007c00,	onimmed,	"cmp" },

	{ 0xfc00ffe0,	0xf4008000,	bitman,		"clr" },
	{ 0xfc00ffe0,	0xf4008800,	bitman,		"set" },
	{ 0xfc00ffe0,	0xf4009000,	bitman,		"ext" },
	{ 0xfc00ffe0,	0xf4009800,	bitman,		"extu" },
	{ 0xfc00ffe0,	0xf400a000,	bitman,		"mak" },
	{ 0xfc00ffe0,	0xf400a800,	bitman,		"rot" },

	{ 0xfc00fbe0,	0xf400c000,	jump,		"jmp" },
	{ 0xfc00fbe0,	0xf400c800,	jump,		"jsr" },

	{ 0xfc00ffe0,	0xf400e800,	instset,	"ff1" },
	{ 0xfc00ffe0,	0xf400ec00,	instset,	"ff0" },
	{ 0xfc00ffe0,	0xf400f800,	instset,	"tbnd" },
	{ 0xfc00ffe0,	0xf400fc00,	instset,	"rte" },
	{ 0xfc000000,	0xf8000000,	instset,	"tbnd" },
	{ 0,		0,		NULL,		NULL }
};

int
m88k_print_instruction(u_int iadr, long inst)
{
	const struct opdesc *p;

	/*
	 * This messes up "orb" instructions ever so slightly,
	 * but keeps us in sync between routines...
	 */
	if (inst == 0) {
		db_printf("\t.word 0");
	} else {
		for (p = opdecode; p->mask; p++)
			if ((inst & p->mask) == p->match) {
				(*p->opfun)(inst, p->farg, iadr);
				break;
			}
		if (!p->mask)
			db_printf("\t.word 0x%x", inst);
	}

	return (iadr + 4);
}

db_addr_t
db_disasm(db_addr_t loc, boolean_t altfmt)
{
	m88k_print_instruction(loc, db_get_value(loc, 4, FALSE));
	db_printf("\n");
	return (loc + 4);
}
