/*	$OpenBSD: db_disasm.c,v 1.12 1997/11/06 23:48:53 deraadt Exp $	*/

/*
 * Copyright (c) 1997 Niklas Hallqvist.  All rights reserverd.
 * Copyright (c) 1997 Theo de Raadt. All rights reserved.
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
 *	This product includes software developed by Niklas Hallqvist and
 *	Theo de Raadt.
 * 4. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS;
 * OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
 * WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR
 * OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF
 * ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include <sys/param.h>
#include <sys/systm.h>

#include <vm/vm.h>

#include <machine/db_machdep.h>
#include <machine/frame.h>

#include <ddb/db_interface.h>
#include <ddb/db_variables.h>
#include <ddb/db_output.h>
#include <ddb/db_sym.h>

struct opcode opcode[] = {
	{ OPC_PAL, "call_pal", 0 },	/* 00 */
	{ OPC_RES, "opc01", 0 },	/* 01 */
	{ OPC_RES, "opc02", 0 },	/* 02 */
	{ OPC_RES, "opc03", 0 },	/* 03 */
	{ OPC_RES, "opc04", 0 },	/* 04 */
	{ OPC_RES, "opc05", 0 },	/* 05 */
	{ OPC_RES, "opc06", 0 },	/* 06 */
	{ OPC_RES, "opc07", 0 },	/* 07 */
	{ OPC_MEM, "lda", 1 },		/* 08 */
	{ OPC_MEM, "ldah", 1 },		/* 09 */
	{ OPC_RES, "opc0a", 0 },	/* 0A */
	{ OPC_MEM, "ldq_u", 1 },	/* 0B */
	{ OPC_RES, "opc0c", 0 },	/* 0C */
	{ OPC_RES, "opc0d", 0 },	/* 0D */
	{ OPC_RES, "opc0e", 0 },	/* 0E */
	{ OPC_MEM, "stq_u", 1 },	/* 0F */
	{ OPC_OP, "inta", 0 },		/* 10 */
	{ OPC_OP, "intl", 0 },		/* 11 */
	{ OPC_OP, "ints", 0 },		/* 12 */
	{ OPC_OP, "intm", 0 },		/* 13 */
	{ OPC_RES, "opc14", 0 },	/* 14 */
	{ OPC_OP, "fltv", 1 },		/* 15 */
	{ OPC_OP, "flti", 1 },		/* 16 */
	{ OPC_OP, "fltl", 1 },		/* 17 */
	{ OPC_MEM, "misc", 0 },		/* 18 */
	{ OPC_PAL, "pal19", 0 },	/* 19 */
	{ OPC_MEM, "jsr", 0 },		/* 1A */
	{ OPC_PAL, "pal1b", 0 },	/* 1B */
	{ OPC_RES, "opc1c", 0 },	/* 1C */
	{ OPC_PAL, "pal1d", 0 },	/* 1D */
	{ OPC_PAL, "pal1e", 0 },	/* 1E */
	{ OPC_PAL, "pal1f", 0 },	/* 1F */
	{ OPC_MEM, "ldf", 1 },		/* 20 */
	{ OPC_MEM, "ldg", 1 },		/* 21 */
	{ OPC_MEM, "lds", 1 },		/* 22 */
	{ OPC_MEM, "ldt", 1 },		/* 23 */
	{ OPC_MEM, "stf", 1 },		/* 24 */
	{ OPC_MEM, "stg", 1 },		/* 25 */
	{ OPC_MEM, "sts", 1 },		/* 26 */
	{ OPC_MEM, "stt", 1 },		/* 27 */
	{ OPC_MEM, "ldl", 1 },		/* 28 */
	{ OPC_MEM, "ldq", 1 },		/* 29 */
	{ OPC_MEM, "ldl_l", 1 },	/* 2A */
	{ OPC_MEM, "ldq_l", 1 },	/* 2B */
	{ OPC_MEM, "stl", 1 },		/* 2C */
	{ OPC_MEM, "stq", 1 },		/* 2D */
	{ OPC_MEM, "stl_c", 1 },	/* 2E */
	{ OPC_MEM, "stq_c", 1 },	/* 2F */
	{ OPC_BR, "br", 1 },		/* 30 */
	{ OPC_BR, "fbeq", 1 },		/* 31 */
	{ OPC_BR, "fblt", 1 },		/* 32 */
	{ OPC_BR, "fble", 1 },		/* 33 */
	{ OPC_BR, "bsr", 1 },		/* 34 */
	{ OPC_BR, "fbne", 1 },		/* 35 */
	{ OPC_BR, "fbge", 1 },		/* 36 */
	{ OPC_BR, "fbgt", 1 },		/* 37 */
	{ OPC_BR, "blbc", 1 },		/* 38 */
	{ OPC_BR, "beq", 1 },		/* 39 */
	{ OPC_BR, "blt", 1 },		/* 3A */
	{ OPC_BR, "ble", 1 },		/* 3B */
	{ OPC_BR, "blbs", 1 },		/* 3C */
	{ OPC_BR, "bne", 1 },		/* 3D */
	{ OPC_BR, "bge", 1 },		/* 3E */
	{ OPC_BR, "bgt", 1 },		/* 3F */
};

struct opinstr {
	char	*nam;
	u_char	opc;
	u_char	func;
} opinstr[] = {
	{ "addl",0x10,0x00 },	{ "subl",0x10,0x09 },	{ "cmpeq",0x10,0x2d },
	{ "addl/v",0x10,0x40 },	{ "subl/v",0x10,0x49 },	{ "cmplt",0x10,0x4d },
	{ "addq",0x10,0x20 },	{ "subq",0x10,0x29 },	{ "cmple",0x10,0x6d },
	{ "addq/v",0x10,0x60 },	{ "subq/v",0x10,0x69 },	{ "cmpult",0x10,0x1d },
	{ "cmpule",0x10,0x3d },
	{ "cmpbge",0x10,0x0f },

	{ "s4addl",0x10,0x02 },	{ "s4subl",0x10,0x0b },	{ "s8addl",0x10,0x12 },
	    { "s8subl",0x10,0x1b },
	{ "s4addq",0x10,0x22 },	{ "s4subq",0x10,0x2b },	{ "s8addq",0x10,0x32 },
	    { "s8subq",0x10,0x3b },

	{ "and",0x11,0x00 },	{ "bis",0x11,0x20 },	{ "xor",0x11,0x40 },
	{ "bic",0x11,0x08 },	{ "ornot",0x11,0x28 },	{ "eqv",0x11,0x48 },	
	{ "cmovq",0x11,0x24 },	{ "cmovlt",0x11,0x44 },	{ "cmovle",0x11,0x64 },	
	{ "cmovne",0x11,0x26 },	{ "cmovge",0x11,0x46 },	{ "cmovgt",0x11,0x66 },	
	{ "cmovbs",0x11,0x14 },	{ "cmovbc",0x11,0x16 },

	{ "sll",0x12,0x39 },	{ "sra",0x12,0x3c },	{ "srl",0x12,0x34 },
	{ "extbl",0x12,0x06 },	{ "insbl",0x12,0x0b },	{ "mskbl",0x12,0x02 },
	{ "extwl",0x12,0x16 },	{ "inswl",0x12,0x1b },	{ "mskwl",0x12,0x12 },
	{ "extll",0x12,0x26 },	{ "insll",0x12,0x2b },	{ "mskll",0x12,0x22 },
	{ "extql",0x12,0x36 },	{ "insql",0x12,0x3b },	{ "mskql",0x12,0x32 },
	{ "extwh",0x12,0x5a },	{ "inswh",0x12,0x57 },	{ "mskwh",0x12,0x52 },
	{ "extlh",0x12,0x6a },	{ "inslh",0x12,0x67 },	{ "msklh",0x12,0x62 },
	{ "extqh",0x12,0x7a },	{ "insqh",0x12,0x77 },	{ "mskqh",0x12,0x72 },
							{ "zap",0x12,0x30 },
							{ "zapnot",0x12,0x31 },

	{ "mull",0x13,0x00 },	{ "mull/v",0x13,0x40 },	{ "mulq",0x13,0x20 },
	{ "mulq/v",0x13,0x60 },	{ "umulh",0x13,0x30 },
};

char *jsrnam[] = {
	"jmp",
	"jsr",
	"ret",
	"jsr_coroutine"
};

char *regnam __P((int));

char *
regnam(r)
	int r;
{
	extern struct db_variable db_regs[];

	if (r == 31)
		return ("zero");
	return (db_regs[r].name);
}

vm_offset_t 
db_disasm(loc, flag)
	vm_offset_t loc;
	boolean_t flag;
{
	char rnam[8];
	u_int32_t ins = *(u_int32_t *)loc;
	int opc = ins >> 26;
	int arg = ins & 0x3ffffff;
	int ra, rb, rc, disp, func, imm;
	int i;

	if (opcode[opc].opc_print)
		db_printf("%s\t", opcode[opc].opc_name);
	switch (opcode[opc].opc_fmt) {
	case OPC_PAL:
		switch (arg) {
		case 0x0000000:
			db_printf("halt");
			break;
		case 0x0000080:
			db_printf("bpt");
			break;
		case 0x0000086:
			db_printf("imb");
			break;
		default:
			db_printf("0x%08x", ins);
			break;
		}
		break;
	case OPC_RES:
		db_printf("0x%08x", ins);
		break;
	case OPC_MEM:
		ra = arg >> 21;
		rb = (arg >> 16) & 0x1f;
		disp = arg & 0xffff;
		switch (opc) {
		case 0x18:
			/* Memory fmt with a function code */
			switch (disp) {
			case 0x0000:
				db_printf("trapb");
				break;
			case 0x4000:
				db_printf("mb");
				break;
			case 0x8000:
				db_printf("fetch\t0(%s)", regnam(rb));
				break;
			case 0xa000:
				db_printf("fetch_m\t0($s)", regnam(rb));
				break;
			case 0xc000:
				db_printf("rpcc\t%s", regnam(ra));
				break;
			case 0xe000:
				db_printf("rc\t%s", regnam(ra));
				break;
			case 0xf000:
				db_printf("rs\t%s", regnam(ra));
				break;
			default:
				db_printf("0x%08x", ins);
			        break;
			}
			break;
		case 0x1a:
			db_printf("%s\t\t%s,(%s),0x%x", jsrnam[disp >> 14],
			    regnam(ra), regnam(rb), disp & 0x3fff);
			break;
		default:
			db_printf("\t%s,0x%x(%s)", regnam(ra), disp,
			    regnam(rb));
			break;
		}
		break;
	case OPC_OP:
		ra = arg >> 21;
		rb = (arg >> 16) & 0x1f;
		func = (arg >> 5) & 0x7f;
		imm = (arg >> 5) & 0x80;
		rc = arg & 0x1f;

		switch (opc) {
		case 0x11:
			if (func == 0x20 && imm == 0 && ra == 31 &&
			    rb == 31 && rc == 31) {
				db_printf("nop");
				break;
			}
			/*FALLTHROUGH*/
		case 0x10:
		case 0x12:
		case 0x13:
			if (imm)	/* literal */
				sprintf(rnam, "0x%x", (arg >> 13) & 0xff);
			else
				sprintf(rnam, "%s", regnam(rb));

			for (i = 0; i < sizeof opinstr/sizeof(opinstr[0]); i++)
				if (opinstr[i].opc == opc &&
				    opinstr[i].func == func)
					break;
			if (i != sizeof opinstr/sizeof(opinstr[0]))
				db_printf("%s\t\t%s,%s,%s",
				    opinstr[i].nam, regnam(ra), rnam,
				    regnam(rc));
			else 
				db_printf("%s\t\t0x%03x,%s,%s,%s",
				    opcode[opc].opc_name, func,
				    regnam(ra), rnam, regnam(rc));
			break;
		default:
			db_printf("0x%03x,%s,%s,%s", func, regnam(ra),
			    regnam(rb), regnam(rc));
			break;
		}
		break;
	case OPC_BR:
		ra = arg >> 21;
		disp = arg & 0x1fffff;
		db_printf("\t%s,0x%x [", regnam(ra), disp);
		disp = (disp & 0x100000) ? -((-disp) & 0xfffff) << 2 :
		    (disp & 0xfffff) << 2;
		db_printsym(loc + sizeof (int) + disp, DB_STGY_PROC);
		db_printf("]");
		break;
	}
	db_printf("\n");
	return (loc + sizeof (int));
}
