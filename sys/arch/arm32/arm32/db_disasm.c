/* $NetBSD: db_disasm.c,v 1.2 1996/03/06 22:46:37 mark Exp $ */

/*
 * Copyright (c) 1996 Mark Brinicombe.
 * Copyright (c) 1996 Brini.
 *
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
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *	This product includes software developed by Brini.
 * 4. The name of the company nor the name of the author may be used to
 *    endorse or promote products derived from this software without specific
 *    prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY BRINI ``AS IS'' AND ANY EXPRESS OR IMPLIED
 * WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL BRINI OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT,
 * INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 * RiscBSD kernel project
 *
 * db_disasm.c
 *
 * Kernel disassembler
 *
 * Created      : 10/02/96
 *
 * Structured after the sparc/sparc/db_disasm.c by David S. Miller &
 * Paul Kranenburg
 *
 * This code is not complete. Not all instructions are disassembled.
 * Current LDF, STF, CDT and MSRF are not supported.
 */

#include <sys/param.h>
#include <machine/db_machdep.h>
#include <ddb/db_sym.h>

/*
 * General instruction format
 *
 *	insn[cc][mod]	[operands]
 *
 * Those fields with an uppercase format code indicate that the field follows
 * directly after the instruction before the separator i.e. they modify the instruction
 * rather than just being an operand to the instruction. The only exception is the
 * writeback flag which follows a operand.
 *
 *
 * 2 - print Operand 2 of a data processing instrcution
 * d - destination register (bits 12-15)
 * n - n register (bits 16-19)
 * s - s register (bits 8-11)
 * o - indirect register rn (bits 16-19) (used by swap)
 * m - m register (bits 0-4)
 * a - address operand of ldr/str instruction
 * l - register list for ldm/stm instruction
 * f - 1st fp operand (register) (bits 12-14)
 * g - 2nd fp operand (register) (bits 16-18)
 * h - 3rd fp operand (register/immediate) (bits 0-4)
 * b - branch address
 * X - block transfer type
 * c - comment field bits(0-23)
 * p - saved or current status register
 * B - byte transfer flag
 * S - set status flag
 * T - user mode transfer
 * P - fp precision
 * R - fp rounding
 * w - writeback flag
 * # - co-processor number
 * y - co-processor data processing registers
 * z - co-processor data transfer registers
 */

struct arm32_insn {
	u_int mask;
	u_int pattern;
	char* name;
	char* format;
};

struct arm32_insn arm32_i[] = {
    { 0x0f000000, 0x0f000000, "swi",	"c" },
    { 0x0f000000, 0x0a000000, "b",	"b" },
    { 0x0f000000, 0x0b000000, "bl",	"b" },
    { 0x0fe000f0, 0x00000090, "mul",	"Sdms" },
    { 0x0fe000f0, 0x00200090, "mla",	"Sdmsn" },
    { 0x0e100000, 0x04000000, "str",	"BTdaW" },
    { 0x0e100000, 0x04100000, "ldr",	"BTdaW" },
    { 0x0c100010, 0x04000000, "str",	"BTdaW" },
    { 0x0c100010, 0x04100000, "ldr",	"BTdaW" },
    { 0x0e100000, 0x08000000, "stm",	"XnWl" },
    { 0x0e100000, 0x08100000, "ldm",	"XnWl" },
    { 0x0fb00ff0, 0x01000090, "swap",	"Bdmo" },
    { 0x0fbf0fff, 0x010f0000, "mrs",	"dp" },
    { 0x0dbffff0, 0x0129f000, "msr",	"pm" },
    { 0x0dbffff0, 0x0128f000, "msrf",	"pm" },
    { 0x0de00000, 0x00000000, "and",	"Sdn2" },
    { 0x0de00000, 0x00200000, "eor",	"Sdn2" },
    { 0x0de00000, 0x00400000, "sub",	"Sdn2" },
    { 0x0de00000, 0x00600000, "rsb",	"Sdn2" },
    { 0x0de00000, 0x00800000, "add",	"Sdn2" },
    { 0x0de00000, 0x00a00000, "adc",	"Sdn2" },
    { 0x0de00000, 0x00c00000, "sbc",	"Sdn2" },
    { 0x0de00000, 0x00e00000, "rsc",	"Sdn2" },
    { 0x0de00000, 0x01000000, "tst",	"Sn2" },
    { 0x0de00000, 0x01200000, "teq",	"Sn2" },
    { 0x0de00000, 0x01400000, "cmp",	"Sn2" },
    { 0x0de00000, 0x01600000, "cmn",	"Sn2" },
    { 0x0de00000, 0x01800000, "orr",	"Sdn2" },
    { 0x0de00000, 0x01a00000, "mov",	"Sd2" },
    { 0x0de00000, 0x01c00000, "bic",	"Sdn2" },
    { 0x0de00000, 0x01e00000, "mvn",	"Sd2" },
    { 0x0ff08f10, 0x0e000100, "adf",	"PRfgh" },
    { 0x0ff08f10, 0x0e100100, "muf",	"PRfgh" },
    { 0x0ff08f10, 0x0e200100, "suf",	"PRfgh" },
    { 0x0ff08f10, 0x0e300100, "rsf",	"PRfgh" },
    { 0x0ff08f10, 0x0e400100, "dvf",	"PRfgh" },
    { 0x0ff08f10, 0x0e500100, "rdf",	"PRfgh" },
    { 0x0ff08f10, 0x0e600100, "pow",	"PRfgh" },
    { 0x0ff08f10, 0x0e700100, "rpw",	"PRfgh" },
    { 0x0ff08f10, 0x0e800100, "rmf",	"PRfgh" },
    { 0x0ff08f10, 0x0e900100, "fml",	"PRfgh" },
    { 0x0ff08f10, 0x0ea00100, "fdv",	"PRfgh" },
    { 0x0ff08f10, 0x0eb00100, "frd",	"PRfgh" },
    { 0x0ff08f10, 0x0ec00100, "pol",	"PRfgh" },
    { 0x0f008f10, 0x0e000100, "fpbop",	"PRfgh" },
    { 0x0ff08f10, 0x0e008100, "mvf",	"PRfh" },
    { 0x0ff08f10, 0x0e108100, "mnf",	"PRfh" },
    { 0x0ff08f10, 0x0e208100, "abs",	"PRfh" },
    { 0x0ff08f10, 0x0e308100, "rnd",	"PRfh" },
    { 0x0ff08f10, 0x0e408100, "sqt",	"PRfh" },
    { 0x0ff08f10, 0x0e508100, "log",	"PRfh" },
    { 0x0ff08f10, 0x0e608100, "lgn",	"PRfh" },
    { 0x0ff08f10, 0x0e708100, "exp",	"PRfh" },
    { 0x0ff08f10, 0x0e808100, "sin",	"PRfh" },
    { 0x0ff08f10, 0x0e908100, "cos",	"PRfh" },
    { 0x0ff08f10, 0x0ea08100, "tan",	"PRfh" },
    { 0x0ff08f10, 0x0eb08100, "asn",	"PRfh" },
    { 0x0ff08f10, 0x0ec08100, "acs",	"PRfh" },
    { 0x0ff08f10, 0x0ed08100, "atn",	"PRfh" },
    { 0x0f008f10, 0x0e008100, "fpuop",	"PRfh" },
    { 0x0e100f00, 0x0c000100, "stf",	"P" },
    { 0x0e100f00, 0x0c100100, "ldf",	"P" },
    { 0x0f100010, 0x0e000010, "mcr",	"#z" },
    { 0x0f100010, 0x0e100010, "mrc",	"#z" },
    { 0x0f000010, 0x0e000000, "cdp",	"#y" },
    { 0x0e000000, 0x0c000000, "cdt",	"#" },
    { 0x00000000, 0x00000000, NULL,	NULL }
};

char *arm32_insn_conditions[] = {
    "eq",
    "ne",
    "cs",
    "cc",
    "mi",
    "pl",
    "vs",
    "vc",
    "hi",
    "ls",
    "ge",
    "lt",
    "gt",
    "le",
    "",
    "nv"
};

char *insn_block_transfers[] = {
    "da",
    "ia",
    "db",
    "ib"
};

char *insn_stack_block_transfers[] = {
    "fa",
    "ea",
    "fd",
    "fa"
};

char *op_shifts[] = {
    "lsl",
    "lsr",
    "asr",
    "ror"
};

char *insn_fpa_rounding[] = {
    "",
    "p",
    "m",
    "z"
};

char *insn_fpa_precision[] = {
    "s",
    "d",
    "e",
    "p"
};

char *insn_fpaconstants[] = {
    "0.0",
    "1.0",
    "2.0",
    "3.0",
    "4.0",
    "5.0",
    "0.5",
    "10.0"
};

#define insn_condition(x)	arm32_insn_conditions[(x >> 28) & 0x0f]
#define insn_blktrans(x)	insn_block_transfers[(x >> 23) & 3]
#define insn_stkblktrans(x)	insn_stack_block_transfers[(x >> 23) & 3]
#define op2_shift(x)		op_shifts[(x >> 5) & 3]
#define insn_fparnd(x)		insn_fpa_rounding[(x >> 5) & 0x03]
#define insn_fpaprec(x)		insn_fpa_precision[(((x >> 18) & 2)|(x >> 7)) & 3]
#define insn_fpaimm(x)		insn_fpaconstants[x & 0x07]

void db_register_shift	__P((u_int insn));
void db_print_reglist	__P((u_int insn));
void db_insn_ldrstr	__P((u_int insn, u_int loc));


vm_offset_t
db_disasm(loc, altfmt)
	vm_offset_t loc;
	boolean_t altfmt;
{
	struct arm32_insn*	i_ptr = (struct arm32_insn *)&arm32_i;

	u_int insn;
	int matchp;
	int branch;
	char* f_ptr, *cp;
	int fmt;

	fmt = 0;
	matchp = 0;
	insn = db_get_value(loc, 4, 0);

/*	db_printf("loc=%08x insn=%08x : ", loc, insn);*/
/*	db_printf("loc=%08x : ", loc);*/

	while (i_ptr->name) {
		if ((insn & i_ptr->mask) ==  i_ptr->pattern) {
			matchp = 1;
			break;
		}
		i_ptr++;
	}

	if (!matchp) {
		db_printf("undefined\n");
		return(loc + 4);
	}

	db_printf("%s%s", i_ptr->name, insn_condition(insn));

	f_ptr = i_ptr->format;

	while (*f_ptr) {
		switch (*f_ptr) {
		case '2':
			if (insn & 0x02000000) {
				db_printf("#0x%08x", (insn & 0xff) << (((insn >> 7) & 0x1e)));
			} else {
				db_register_shift(insn);
			}
			break;
		case 'd':
			db_printf("r%d", ((insn >> 12) & 0x0f));
			break;
		case 'n':
			db_printf("r%d", ((insn >> 16) & 0x0f));
			break;
		case 's':
			db_printf("r%d", ((insn >> 8) & 0x0f));
			break;
		case 'o':
			db_printf("[r%d]", ((insn >> 16) & 0x0f));
			break;
		case 'm':
			db_printf("r%d", ((insn >> 0) & 0x0f));
			break;
		case 'a':
			db_insn_ldrstr(insn, loc);
			break;
		case 'l':
			db_print_reglist(insn);
			break;
		case 'f':
			db_printf("f%d", (insn >> 12) & 7);
			break;
		case 'g':
			db_printf("f%d", (insn >> 16) & 7);
			break;
		case 'h':
			if (insn & (1 << 3))
				db_printf("#%s", insn_fpaimm(insn));
			else
				db_printf("f%d", insn & 7);
			break;
		case 'b':
			branch = ((insn << 2) & 0x03ffffff);
			if (branch & 0x02000000)
				branch |= 0xfc000000;
			db_printsym((db_addr_t)(loc + 8 + branch),
			    DB_STGY_ANY);
			break;
		case 'X':
			db_printf("%s", insn_blktrans(insn));
			break;
		case 'c':
			db_printf("0x%08x", (insn & 0x00ffffff));
			break;
		case 'p':
			if (insn & 0x00400000)
				db_printf("spsr");
			else
				db_printf("cpsr");
		case 'B':
			if (insn & 0x00400000)
				db_printf("b");
			break;
		case 'S':
			if (insn & 0x00100000)
				db_printf("s");
			break;
		case 'T':
			if ((insn & 0x01200000) == 0x00200000)
				db_printf("t");
			break;
		case 'P':
			db_printf("%s", insn_fpaprec(insn));
			break;
		case 'R':
			db_printf("%s", insn_fparnd(insn));
			break;
		case 'W':
			if (insn & (1 << 21))
				db_printf("!");
			break;
		case '#':
			db_printf("CP #%d", (insn >> 8) & 0x0f);
			break;
		case 'y':
			db_printf("%d, ", (insn >> 20) & 0x0f);

			db_printf("cr%d, cr%d, cr%d", (insn >> 12) & 0x0f, (insn >> 16) & 0x0f,
			    insn & 0x0f);

			db_printf(", %d", (insn >> 5) & 0x07);
			break;
		case 'z':
			db_printf("%d, ", (insn >> 21) & 0x07);
			db_printf("r%d, cr%d, cr%d", (insn >> 12) & 0x0f, (insn >> 16) & 0x0f,
			    insn & 0x0f);

			if (((insn >> 5) & 0x07) != 0)
				db_printf(", %d", (insn >> 5) & 0x07);
			break;
		default:
			db_printf("[%02x:c](unknown)", *f_ptr, *f_ptr);
			break;
		}
		++fmt;
		if (*(f_ptr+1) > 'A' && *(f_ptr+1) < 'Z')
			++f_ptr;
		else if (*(++f_ptr)) {
			if (fmt == 1)
				db_printf("\t");
			else
				db_printf(", ");
		}
	};

	db_printf("\n");

	return(loc + 4);
}


void
db_register_shift(insn)
	u_int insn;
{
	db_printf("r%d", (insn & 0x0f));
	if ((insn & 0x00000ff0) == 0)
		;
	else if ((insn & 0x00000ff0) == 0x00000060)
		db_printf(", RRX");
	else {
		if (insn & 0x10)
			db_printf(", %s r%d", op2_shift(insn),
			    (insn >> 8) & 0x0f);
		else
			db_printf(", %s #%d", op2_shift(insn),
			    (insn >> 7) & 0x1f);
	}
}


void
db_print_reglist(insn)
	u_int insn;
{
	int loop;
	int start;
	int comma;

	db_printf("{");
	start = -1;
	comma = 0;

	for (loop = 0; loop < 17; ++loop) {
		if (start != -1) {
			if (!(insn & (1 << loop)) || loop == 16) {
				if (comma)
					db_printf(", ");
				else
					comma = 1;
        			if (start == loop - 1)
        				db_printf("r%d", start);
        			else
        				db_printf("r%d-r%d", start, loop - 1);
        			start = -1;
        		}
        	} else {
        		if (insn & (1 << loop))
        			start = loop;
        	}
        }
	db_printf("}");

	if (insn & (1 << 22))
		db_printf("^");
}

void
db_insn_ldrstr(insn, loc)
	u_int insn;
	u_int loc;
{
	if (((insn >> 16) & 0x0f) == 15 && (insn & (1 << 21)) == 0
	    && (insn & (1 << 24)) != 0 && (insn & (1 << 25) == 0)) {
		if (insn & 0x00800000)
			loc += (insn & 0xffff);
		else
			loc -= (insn & 0xffff);
		db_printsym((db_addr_t)(loc - 8), DB_STGY_ANY);
 	} else {
		printf("[r%d", (insn >> 16) & 0x0f);
		printf("%s, ", (insn & (1 << 24)) ? "" : "]");

		if (!(insn & 0x00800000))
			printf("-");
		if (insn & (1 << 25))
			db_register_shift(insn);
		else
			printf("#0x%04x", insn & 0xfff);
		if (insn & (1 << 24))
			printf("]");
	}
}



#if 0

u_int instruction_msrf(u_int addr, u_int word)
  {
    printf("MSR%s\t", opcode_condition(word));

    printf("%s_flg, ", (word & 0x00400000) ? "SPSR" : "CPSR");

    if (word & 0x02000000)
      printf("#0x%08x", (word & 0xff) << (32 - ((word >> 7) & 0x1e)));
    else
      printf("r%d", word &0x0f);
    return(addr);
  }


u_int instruction_cdt(u_int addr, u_int word)
  {
    printf("%s%s%s\t", (word & (1 << 20)) ? "LDC" : "STC",
      opcode_condition(word), (word & (1 << 22)) ? "L" : "");

    printf("CP #%d, cr%d, ", (word >> 8) & 0x0f, (word >> 12) & 0x0f);

    printf("[r%d", (word >> 16) & 0x0f);

    printf("%s, ", (word & (1 << 24)) ? "" : "]");

    if (!(word & (1 << 23)))
      printf("-");

    printf("#0x%02x", word & 0xff);

    if (word & (1 << 24))
      printf("]");

    if (word & (1 << 21))
      printf("!");

    return(addr);
  }


u_int instruction_ldfstf(u_int addr, u_int word)
  {
    printf("%s%s%s\t", (word & (1 << 20)) ? "LDF" : "STF",
      opcode_condition(word), (word & (1 << 22)) ? "L" : "");

    printf("f%d, [r%d", (word >> 12) & 0x07, (word >> 16) & 0x0f);

    printf("%s, ", (word & (1 << 24)) ? "" : "]");

    if (!(word & (1 << 23)))
      printf("-");

    printf("#0x%03x", (word & 0xff) << 2);

    if (word & (1 << 24))
      printf("]");

    if (word & (1 << 21))
      printf("!");

    return(addr);
  }

#endif

/* End of db_disasm.c */
