/*	$OpenBSD: minidebug.c,v 1.2 1998/03/16 09:03:36 pefo Exp $	*/
/*-
 * Copyright (c) 1991, 1993
 *	The Regents of the University of California.  All rights reserved.
 *
 * This code is derived from software contributed to Berkeley by
 * Ralph Campbell.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *	This product includes software developed by the University of
 *	California, Berkeley and its contributors.
 * 4. Neither the name of the University nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE REGENTS AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 *	from: @(#)kadb.c	8.1 (Berkeley) 6/10/93
 *      $Id: minidebug.c,v 1.2 1998/03/16 09:03:36 pefo Exp $
 */

/*
 * Define machine dependent primitives for mdb.
 */

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/proc.h>
#include <sys/user.h>
#include <dev/cons.h>
#include <machine/pte.h>
#include <vm/vm_prot.h>
#undef SP
#include <machine/cpu.h>
#include <machine/reg.h>
#include <machine/intr.h>
#include <machine/trap.h>
#include <machine/mips_opcode.h>

#ifndef TRUE
#define TRUE 1
#define FALSE 0
#endif


static char *op_name[64] = {
/* 0 */	"spec",	"bcond","j",	"jal",	"beq",	"bne",	"blez",	"bgtz",
/* 8 */	"addi",	"addiu","slti",	"sltiu","andi",	"ori",	"xori",	"lui",
/*16 */	"cop0",	"cop1",	"cop2",	"cop3",	"beql",	"bnel",	"blezl","bgtzl",
/*24 */	"daddi","daddiu","ldl",	"ldr",	"op34",	"op35",	"op36",	"op37",
/*32 */	"lb",	"lh",	"lwl",	"lw",	"lbu",	"lhu",	"lwr",	"lwu",
/*40 */	"sb",	"sh",	"swl",	"sw",	"sdl",	"sdr",	"swr",	"cache",
/*48 */	"ll",	"lwc1",	"lwc2",	"lwc3",	"lld",	"ldc1",	"ldc2",	"ld",
/*56 */	"sc",	"swc1",	"swc2",	"swc3",	"scd",	"sdc1",	"sdc2",	"sd"
};

static char *spec_name[64] = {
/* 0 */	"sll",	"spec01","srl",	"sra",	"sllv",	"spec05","srlv","srav",
/* 8 */	"jr",	"jalr",	"spec12","spec13","syscall","break","spec16","sync",
/*16 */	"mfhi",	"mthi",	"mflo",	"mtlo",	"dsllv","spec25","dsrlv","dsrav",
/*24 */	"mult",	"multu","div",	"divu",	"dmult","dmultu","ddiv","ddivu",
/*32 */	"add",	"addu",	"sub",	"subu",	"and",	"or",	"xor",	"nor",
/*40 */	"spec50","spec51","slt","sltu",	"dadd","daddu","dsub","dsubu",
/*48 */	"tge","tgeu","tlt","tltu","teq","spec65","tne","spec67",
/*56 */	"dsll","spec71","dsrl","dsra","dsll32","spec75","dsrl32","dsra32"
};

static char *bcond_name[32] = {
/* 0 */	"bltz",	"bgez", "bltzl", "bgezl", "?", "?", "?", "?",
/* 8 */	"tgei", "tgeiu", "tlti", "tltiu", "teqi", "?", "tnei", "?",
/*16 */	"bltzal", "bgezal", "bltzall", "bgezall", "?", "?", "?", "?",
/*24 */	"?", "?", "?", "?", "?", "?", "?", "?",
};

static char *cop1_name[64] = {
/* 0 */	"fadd",	"fsub",	"fmpy",	"fdiv",	"fsqrt","fabs",	"fmov",	"fneg",
/* 8 */	"fop08","fop09","fop0a","fop0b","fop0c","fop0d","fop0e","fop0f",
/*16 */	"fop10","fop11","fop12","fop13","fop14","fop15","fop16","fop17",
/*24 */	"fop18","fop19","fop1a","fop1b","fop1c","fop1d","fop1e","fop1f",
/*32 */	"fcvts","fcvtd","fcvte","fop23","fcvtw","fop25","fop26","fop27",
/*40 */	"fop28","fop29","fop2a","fop2b","fop2c","fop2d","fop2e","fop2f",
/*48 */	"fcmp.f","fcmp.un","fcmp.eq","fcmp.ueq","fcmp.olt","fcmp.ult",
	"fcmp.ole","fcmp.ule",
/*56 */	"fcmp.sf","fcmp.ngle","fcmp.seq","fcmp.ngl","fcmp.lt","fcmp.nge",
	"fcmp.le","fcmp.ngt"
};

static char *fmt_name[16] = {
	"s",	"d",	"e",	"fmt3",
	"w",	"fmt5",	"fmt6",	"fmt7",
	"fmt8",	"fmt9",	"fmta",	"fmtb",
	"fmtc",	"fmtd",	"fmte",	"fmtf"
};

static char *reg_name[32] = {
	"zero",	"at",	"v0",	"v1",	"a0",	"a1",	"a2",	"a3",
	"t0",	"t1",	"t2",	"t3",	"t4",	"t5",	"t6",	"t7",
	"s0",	"s1",	"s2",	"s3",	"s4",	"s5",	"s6",	"s7",
	"t8",	"t9",	"k0",	"k1",	"gp",	"sp",	"s8",	"ra"
};

static char *c0_opname[64] = {
	"c0op00","tlbr",  "tlbwi", "c0op03","c0op04","c0op05","tlbwr", "c0op07",
	"tlbp",  "c0op11","c0op12","c0op13","c0op14","c0op15","c0op16","c0op17",
	"rfe",   "c0op21","c0op22","c0op23","c0op24","c0op25","c0op26","c0op27",
	"eret","c0op31","c0op32","c0op33","c0op34","c0op35","c0op36","c0op37",
	"c0op40","c0op41","c0op42","c0op43","c0op44","c0op45","c0op46","c0op47",
	"c0op50","c0op51","c0op52","c0op53","c0op54","c0op55","c0op56","c0op57",
	"c0op60","c0op61","c0op62","c0op63","c0op64","c0op65","c0op66","c0op67",
	"c0op70","c0op71","c0op72","c0op73","c0op74","c0op75","c0op77","c0op77",
};

static char *c0_reg[32] = {
	"index","random","tlblo0","tlblo1","context","tlbmask","wired","c0r7",
	"badvaddr","count","tlbhi","c0r11","sr","cause","epc",	"prid",
	"config","lladr","watchlo","watchhi","xcontext","c0r21","c0r22","c0r23",
	"c0r24","c0r25","ecc","cacheerr","taglo","taghi","errepc","c0r31"
};

extern u_int mdbpeek __P((int));
extern void mdbpoke __P((int, int));
extern void cpu_setwatch __P((int, int));
extern void trapDump __P((char *));
extern void stacktrace __P((void));
extern u_int MipsEmulateBranch __P((int *, int, int, u_int));
extern char *trap_type[];
extern int num_tlbentries;
static void arc_dump_tlb __P((int,int));
static void prt_break __P((void));
static int mdbprintins __P((int, int));
static void mdbsetsstep __P((void));
static int mdbclrsstep __P((int));
static void print_regs __P((void));
static void break_insert __P((void));
static void break_restore __P((void));
static int break_find __P((int));

struct pcb mdbpcb;
int mdbmkfault;

#define MAXBRK 10
struct brk {
	int	inst;
	int	addr;
} brk_tab[MAXBRK];

/*
 * Mini debugger for kernel.
 */
static int
gethex(u_int *val, u_int dotval)
{
	u_int c;

	*val = 0;
	while((c = cngetc()) != '\e' && c != '\n' && c != '\r') {
		if(c >= '0' && c <= '9') {
			*val = (*val << 4) + c - '0';
			cnputc(c);
		}
		else if(c >= 'a' && c <= 'f') {
			*val = (*val << 4) + c - 'a' + 10;
			cnputc(c);
		}
		else if(c == '\b') {
			*val = *val >> 4;
			printf("\b \b");
		}
		else if(c == ',') {
			cnputc(c);
			return(c);
		}
		else if(c == '.') {
			*val = dotval;;
			cnputc(c);
		}
	}
	if(c == '\r')
		c = '\n';
	return(c);
}

static
void dump(u_int *addr, u_int size)
{
	int	cnt;

	cnt = 0;

	size = (size + 3) / 4;
	while(size--) {
		if((cnt++ & 3) == 0)
			printf("\n%08x: ",(int)addr);
		printf("%08x ",*addr++);
	}
}

static void
print_regs()
{
	printf("\n");
	printf("T0-7 %08x %08x %08x %08x %08x %08x %08x %08x\n",
		mdbpcb.pcb_regs[T0],mdbpcb.pcb_regs[T1],
		mdbpcb.pcb_regs[T2],mdbpcb.pcb_regs[T3],
		mdbpcb.pcb_regs[T4],mdbpcb.pcb_regs[T5],
		mdbpcb.pcb_regs[T6],mdbpcb.pcb_regs[T7]);
	printf("T8-9 %08x %08x     A0-4 %08x %08x %08x %08x\n",
		mdbpcb.pcb_regs[T8],mdbpcb.pcb_regs[T9],
		mdbpcb.pcb_regs[A0],mdbpcb.pcb_regs[A1],
		mdbpcb.pcb_regs[A2],mdbpcb.pcb_regs[A3]);
	printf("S0-7 %08x %08x %08x %08x %08x %08x %08x %08x\n",
		mdbpcb.pcb_regs[S0],mdbpcb.pcb_regs[S1],
		mdbpcb.pcb_regs[S2],mdbpcb.pcb_regs[S3],
		mdbpcb.pcb_regs[S4],mdbpcb.pcb_regs[S5],
		mdbpcb.pcb_regs[S6],mdbpcb.pcb_regs[S7]);
	printf("  S8 %08x     V0-1 %08x %08x       GP %08x       SP %08x\n",
		mdbpcb.pcb_regs[S8],mdbpcb.pcb_regs[V0],
		mdbpcb.pcb_regs[V1],mdbpcb.pcb_regs[GP],
		mdbpcb.pcb_regs[SP]);
	printf("  AT %08x       PC %08x       RA %08x       SR %08x",
		mdbpcb.pcb_regs[AST],mdbpcb.pcb_regs[PC],
		mdbpcb.pcb_regs[RA],mdbpcb.pcb_regs[SR]);
}

void
set_break(int va)
{
	int i;

	va = va & ~3;
	for(i = 0; i < MAXBRK; i++) {
		if(brk_tab[i].addr == 0) {
			brk_tab[i].addr = va;
			brk_tab[i].inst = *(u_int *)va;
			return;
		}
	}
	printf(" Break table full!!");
}

void
del_break(int va)
{
	int i;

	va = va & ~3;
	for(i = 0; i < MAXBRK; i++) {
		if(brk_tab[i].addr == va) {
			brk_tab[i].addr = 0;
			return;
		}
	}
	printf(" Break to remove not found!!");
}

static void
break_insert()
{
	int i;

	for(i = 0; i < MAXBRK; i++) {
		if(brk_tab[i].addr != 0) {
			brk_tab[i].inst = *(u_int *)brk_tab[i].addr;
			*(u_int *)brk_tab[i].addr = BREAK_BRKPT;
			R4K_FlushDCache(brk_tab[i].addr,4);
			R4K_FlushICache(brk_tab[i].addr,4);
		}
	}
}

static void
break_restore()
{
	int i;

	for(i = 0; i < MAXBRK; i++) {
		if(brk_tab[i].addr != 0) {
			*(u_int *)brk_tab[i].addr = brk_tab[i].inst;
			R4K_FlushDCache(brk_tab[i].addr,4);
			R4K_FlushICache(brk_tab[i].addr,4);
		}
	}
}

static int
break_find(va)
	int va;
{
	int i;

	for(i = 0; i < MAXBRK; i++) {
		if(brk_tab[i].addr == va) {
			return(i);
		}
	}
	return(-1);
}

void
prt_break()
{
	int i;

	for(i = 0; i < MAXBRK; i++) {
		if(brk_tab[i].addr != 0) {
			printf("\n    %08x\t", brk_tab[i].addr);
			mdbprintins(brk_tab[i].inst, brk_tab[i].addr);
		}
	}
}

int
mdb(int causeReg, int vadr, int p, int kernelmode)
{
	int c;
	int newaddr;
	int size;
	int cause;
static int ssandrun;	/* Single step and run flag (when cont at brk) */

	splhigh();
	cause = (causeReg & CR_EXC_CODE) >> CR_EXC_CODE_SHIFT;
	newaddr = (int)(mdbpcb.pcb_regs[PC]);
	switch(cause) {
	case T_BREAK:
		if(*(int *)newaddr == BREAK_SOVER) {
			break_restore();
			mdbpcb.pcb_regs[PC] += 4;
			printf("\nStop break (panic)\n# ");
			printf("    %08x\t",newaddr);
			mdbprintins(*(int *)newaddr, newaddr);
			printf("\n# ");
			break;
		}
		if(*(int *)newaddr == BREAK_BRKPT) {
			break_restore();
			printf("\rBRK %08x\t",newaddr);
			if(mdbprintins(*(int *)newaddr, newaddr)) {
				newaddr += 4;
				printf("\n    %08x\t",newaddr);
				mdbprintins(*(int *)newaddr, newaddr);
			}
			printf("\n# ");
			break;
		}
		if(mdbclrsstep(causeReg)) {
			if(ssandrun) { /* Step over bp before free run */
				ssandrun = 0;
				break_insert();
				return(TRUE);
			}
			printf("\r    %08x\t",newaddr);
			if(mdbprintins(*(int *)newaddr, newaddr)) {
				newaddr += 4;
				printf("\n    %08x\t",newaddr);
				mdbprintins(*(int *)newaddr, newaddr);
			}
			printf("\n# ");
		}
		break;

	default:
		printf("\n-- %s --\n# ",trap_type[cause]);
	}
	ssandrun = 0;
	break_restore();

	while(1) {
		c = cngetc();
		switch(c) {
		case 'T':
			trapDump("Debugger");
			break;
		case 'b':
			printf("break-");
			c = cngetc();
			switch(c) {
			case 's':
				printf("set at ");
				c = gethex(&newaddr, newaddr);
				if(c != '\e') {
					set_break(newaddr);
				}
				break;

			case 'd':
				printf("delete at ");
				c = gethex(&newaddr, newaddr);
				if(c != '\e') {
					del_break(newaddr);
				}
				break;

			case 'p':
				printf("print");
				prt_break();
				break;
			}
			break;

		case 'r':
			print_regs();
			break;

		case 'I':
			printf("Instruction at ");
			c = gethex(&newaddr, newaddr);
			while(c != '\e') {
				printf("\n    %08x\t",newaddr);
				mdbprintins(*(int *)newaddr, newaddr);
				newaddr += 4;
				c = cngetc();
			}
			break;

		case 'c':
			printf("continue");
			if(break_find((int)(mdbpcb.pcb_regs[PC])) >= 0) {
				ssandrun = 1;
				mdbsetsstep();
			}
			else {
				break_insert();
			}
			return(TRUE);
		case 'S':
			printf("Stack traceback:\n");
			stacktrace();
			return(TRUE);
		case 's':
			set_break(mdbpcb.pcb_regs[PC] + 8);
			return(TRUE);
		case ' ':
			mdbsetsstep();
			return(TRUE);

		case 'd':
			printf("dump ");
			c = gethex(&newaddr, newaddr);
			if(c == ',') {
				c = gethex(&size,256);
			}
			else {
				size = 16;
			}
			if(c == '\n' && newaddr != 0) {
				dump((u_int *)newaddr, size);
				newaddr += size;
			}
			break;

		case 'm':
			printf("mod ");
			c = gethex(&newaddr, newaddr);
			while(c == ',') {
				c = gethex(&size, 0);
				if(c != '\e')
					*((u_int *)newaddr)++ = size;
			}
			break;

		case 'i':
			printf("in-");
			c = cngetc();
			switch(c) {
			case 'b':
				printf("byte ");
				c = gethex(&newaddr, newaddr);
				if(c == '\n') {
					printf("= %02x",
						*(u_char *)newaddr);	
				}
				break;
			case 'h':
				printf("halfword ");
				c = gethex(&newaddr, newaddr);
				if(c == '\n') {
					printf("= %04x",
						*(u_short *)newaddr);	
				}
				break;
			case 'w':
				printf("word ");
				c = gethex(&newaddr, newaddr);
				if(c == '\n') {
					printf("= %08x",
						*(u_int *)newaddr);	
				}
				break;
			}
			break;

		case 'o':
			printf("out-");
			c = cngetc();
			switch(c) {
			case 'b':
				printf("byte ");
				c = gethex(&newaddr, newaddr);
				if(c == ',') {
					c = gethex(&size, 0);
					if(c == '\n') {
						*(u_char *)newaddr = size;	
					}
				}
				break;
			case 'h':
				printf("halfword ");
				c = gethex(&newaddr, newaddr);
				if(c == ',') {
					c = gethex(&size, 0);
					if(c == '\n') {
						*(u_short *)newaddr = size;	
					}
				}
				break;
			case 'w':
				printf("word ");
				c = gethex(&newaddr, newaddr);
				if(c == ',') {
					c = gethex(&size, 0);
					if(c == '\n') {
						*(u_int *)newaddr = size;	
					}
				}
				break;
			}
			break;

		case 't':
			printf("tlb-dump\n");
			arc_dump_tlb(0,23);
			(void)cngetc();
			arc_dump_tlb(24,47);
			break;

		case 'f':
			printf("flush-");
			c = cngetc();
			switch(c) {
			case 't':
				printf("tlb");
				R4K_TLBFlush(num_tlbentries);
				break;

			case 'c':
				printf("cache");
				R4K_FlushCache();
				break;
			}
			break;
			
		case 'w':
			printf("watch ");
			c = gethex(&newaddr, newaddr);
			size = 3;
			if(c == ',') {
				c = cngetc();
				cnputc(c);
				if(c == 'r')
					size = 2;
				else if(c == 'w')
					size = 1;
				else
					size = 0;
			}
			cpu_setwatch(0, (newaddr & ~7) | size);
			break;

		default:
			cnputc('\a');
			break;
		}
		printf("\n# ");
	}
}

u_int mdb_ss_addr;
u_int mdb_ss_instr;

static void
mdbsetsstep()
{
	register u_int va;
	register int *locr0 = mdbpcb.pcb_regs;

	/* compute next address after current location */
	if(mdbpeek(locr0[PC]) != 0) {
		va = MipsEmulateBranch(locr0, locr0[PC], 0, mdbpeek(locr0[PC]));
	}
	else {
		va = locr0[PC] + 4;
	}
	if (mdb_ss_addr) {
		printf("mdbsetsstep: breakpoint already set at %x (va %x)\n",
			mdb_ss_addr, va);
		return;
	}
	mdb_ss_addr = va;

	if ((int)va < 0) {
		/* kernel address */
		mdb_ss_instr = mdbpeek(va);
		mdbpoke(va, BREAK_SSTEP);
		R4K_FlushDCache(va,4);
		R4K_FlushICache(va,4);
		return;
	}
}

static int
mdbclrsstep(int cr)
{
	register u_int pc, va;
	u_int instr;

	/* fix pc if break instruction is in the delay slot */
	pc = mdbpcb.pcb_regs[PC];
	if (cr < 0)
		pc += 4;

	/* check to be sure its the one we are expecting */
	va = mdb_ss_addr;
	if (!va || va != pc)
		return(FALSE);

	/* read break instruction */
	instr = mdbpeek(va);
	if (instr != BREAK_SSTEP)
		return(FALSE);

	if ((int)va < 0) {
		/* kernel address */
		mdbpoke(va, mdb_ss_instr);
		R4K_FlushDCache(va,4);
		R4K_FlushICache(va,4);
		mdb_ss_addr = 0;
		return(TRUE);
	}

	printf("can't clear break at %x\n", va);
	mdb_ss_addr = 0;
	return(FALSE);
}


/* ARGSUSED */
static int
mdbprintins(int ins, int mdbdot)
{
	InstFmt i;
	int delay = 0;

	i.word = ins;

	switch (i.JType.op) {
	case OP_SPECIAL:
		if (i.word == 0) {
			printf("nop");
			break;
		}
		if (i.RType.func == OP_ADDU && i.RType.rt == 0) {
			printf("move\t%s,%s",
				reg_name[i.RType.rd],
				reg_name[i.RType.rs]);
			break;
		}
		printf("%s", spec_name[i.RType.func]);
		switch (i.RType.func) {
		case OP_SLL:
		case OP_SRL:
		case OP_SRA:
		case OP_DSLL:
		case OP_DSRL:
		case OP_DSRA:
		case OP_DSLL32:
		case OP_DSRL32:
		case OP_DSRA32:
			printf("\t%s,%s,%d",
				reg_name[i.RType.rd],
				reg_name[i.RType.rt],
				i.RType.shamt);
			break;

		case OP_SLLV:
		case OP_SRLV:
		case OP_SRAV:
		case OP_DSLLV:
		case OP_DSRLV:
		case OP_DSRAV:
			printf("\t%s,%s,%s",
				reg_name[i.RType.rd],
				reg_name[i.RType.rt],
				reg_name[i.RType.rs]);
			break;

		case OP_MFHI:
		case OP_MFLO:
			printf("\t%s", reg_name[i.RType.rd]);
			break;

		case OP_JR:
		case OP_JALR:
			delay = 1;
			/* FALLTHROUGH */
		case OP_MTLO:
		case OP_MTHI:
			printf("\t%s", reg_name[i.RType.rs]);
			break;

		case OP_MULT:
		case OP_MULTU:
		case OP_DMULT:
		case OP_DMULTU:
		case OP_DIV:
		case OP_DIVU:
		case OP_DDIV:
		case OP_DDIVU:
			printf("\t%s,%s",
				reg_name[i.RType.rs],
				reg_name[i.RType.rt]);
			break;

		case OP_SYSCALL:
		case OP_SYNC:
			break;

		case OP_BREAK:
			printf("\t%d", (i.RType.rs << 5) | i.RType.rt);
			break;

		default:
			printf("\t%s,%s,%s",
				reg_name[i.RType.rd],
				reg_name[i.RType.rs],
				reg_name[i.RType.rt]);
		};
		break;

	case OP_BCOND:
		printf("%s\t%s,", bcond_name[i.IType.rt],
			reg_name[i.IType.rs]);
		goto pr_displ;

	case OP_BLEZ:
	case OP_BLEZL:
	case OP_BGTZ:
	case OP_BGTZL:
		printf("%s\t%s,", op_name[i.IType.op],
			reg_name[i.IType.rs]);
		goto pr_displ;

	case OP_BEQ:
	case OP_BEQL:
		if (i.IType.rs == 0 && i.IType.rt == 0) {
			printf("b\t");
			goto pr_displ;
		}
		/* FALLTHROUGH */
	case OP_BNE:
	case OP_BNEL:
		printf("%s\t%s,%s,", op_name[i.IType.op],
			reg_name[i.IType.rs],
			reg_name[i.IType.rt]);
	pr_displ:
		delay = 1;
		printf("0x%08x", mdbdot + 4 + ((short)i.IType.imm << 2));
		break;

	case OP_COP0:
		switch (i.RType.rs) {
		case OP_BCx:
		case OP_BCy:
			printf("bc0%c\t",
				"ft"[i.RType.rt & COPz_BC_TF_MASK]);
			goto pr_displ;

		case OP_MT:
			printf("mtc0\t%s,%s",
				reg_name[i.RType.rt],
				c0_reg[i.RType.rd]);
			break;

		case OP_DMT:
			printf("dmtc0\t%s,%s",
				reg_name[i.RType.rt],
				c0_reg[i.RType.rd]);
			break;

		case OP_MF:
			printf("mfc0\t%s,%s",
				reg_name[i.RType.rt],
				c0_reg[i.RType.rd]);
			break;

		case OP_DMF:
			printf("dmfc0\t%s,%s",
				reg_name[i.RType.rt],
				c0_reg[i.RType.rd]);
			break;

		default:
			printf("%s", c0_opname[i.FRType.func]);
		};
		break;

	case OP_COP1:
		switch (i.RType.rs) {
		case OP_BCx:
		case OP_BCy:
			printf("bc1%c\t",
				"ft"[i.RType.rt & COPz_BC_TF_MASK]);
			goto pr_displ;

		case OP_MT:
			printf("mtc1\t%s,f%d",
				reg_name[i.RType.rt],
				i.RType.rd);
			break;

		case OP_MF:
			printf("mfc1\t%s,f%d",
				reg_name[i.RType.rt],
				i.RType.rd);
			break;

		case OP_CT:
			printf("ctc1\t%s,f%d",
				reg_name[i.RType.rt],
				i.RType.rd);
			break;

		case OP_CF:
			printf("cfc1\t%s,f%d",
				reg_name[i.RType.rt],
				i.RType.rd);
			break;

		default:
			printf("%s.%s\tf%d,f%d,f%d",
				cop1_name[i.FRType.func],
				fmt_name[i.FRType.fmt],
				i.FRType.fd, i.FRType.fs, i.FRType.ft);
		};
		break;

	case OP_J:
	case OP_JAL:
		printf("%s\t", op_name[i.JType.op]);
		printf("0x%8x",(mdbdot & 0xF0000000) | (i.JType.target << 2));
		delay = 1;
		break;

	case OP_LWC1:
	case OP_SWC1:
		printf("%s\tf%d,", op_name[i.IType.op],
			i.IType.rt);
		goto loadstore;

	case OP_LB:
	case OP_LH:
	case OP_LW:
	case OP_LD:
	case OP_LBU:
	case OP_LHU:
	case OP_LWU:
	case OP_SB:
	case OP_SH:
	case OP_SW:
	case OP_SD:
		printf("%s\t%s,", op_name[i.IType.op],
			reg_name[i.IType.rt]);
	loadstore:
		printf("%d(%s)", (short)i.IType.imm,
			reg_name[i.IType.rs]);
		break;

	case OP_ORI:
	case OP_XORI:
		if (i.IType.rs == 0) {
			printf("li\t%s,0x%x",
				reg_name[i.IType.rt],
				i.IType.imm);
			break;
		}
		/* FALLTHROUGH */
	case OP_ANDI:
		printf("%s\t%s,%s,0x%x", op_name[i.IType.op],
			reg_name[i.IType.rt],
			reg_name[i.IType.rs],
			i.IType.imm);
		break;

	case OP_LUI:
		printf("%s\t%s,0x%x", op_name[i.IType.op],
			reg_name[i.IType.rt],
			i.IType.imm);
		break;

	case OP_ADDI:
	case OP_DADDI:
	case OP_ADDIU:
	case OP_DADDIU:
		if (i.IType.rs == 0) {
 			printf("li\t%s,%d",
				reg_name[i.IType.rt],
				(short)i.IType.imm);
			break;
		}
		/* FALLTHROUGH */
	default:
		printf("%s\t%s,%s,%d", op_name[i.IType.op],
			reg_name[i.IType.rt],
			reg_name[i.IType.rs],
			(short)i.IType.imm);
	}
	return(delay);
}


/*
 *	Dump TLB contents.
 */
static void
arc_dump_tlb(int first,int last)
{
	int tlbno;
	struct tlb tlb;

	tlbno = first;

	while(tlbno <= last) {
		R4K_TLBRead(tlbno, &tlb);
		if(tlb.tlb_lo0 & PG_V || tlb.tlb_lo1 & PG_V) {
			printf("TLB %2d vad 0x%08x ", tlbno, tlb.tlb_hi);
		}
		else {
			printf("TLB*%2d vad 0x%08x ", tlbno, tlb.tlb_hi);
		}
		printf("0=0x%08x ", pfn_to_vad(tlb.tlb_lo0));
		printf("%c", tlb.tlb_lo0 & PG_M ? 'M' : ' ');
		printf("%c", tlb.tlb_lo0 & PG_G ? 'G' : ' ');
		printf(" atr %x ", (tlb.tlb_lo0 >> 3) & 7);
		printf("1=0x%08x ", pfn_to_vad(tlb.tlb_lo1));
		printf("%c", tlb.tlb_lo1 & PG_M ? 'M' : ' ');
		printf("%c", tlb.tlb_lo1 & PG_G ? 'G' : ' ');
		printf(" atr %x ", (tlb.tlb_lo1 >> 3) & 7);
		printf(" sz=%x\n", tlb.tlb_mask);

		tlbno++;
	}
}
