/*	$OpenBSD: db_disasm.c,v 1.4 1997/07/08 21:55:39 niklas Exp $	*/

/*
 * Copyright (c) 1997 Niklas Hallqvist.  All rights reserverd.
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
 *	This product includes software developed by Niklas Hallqvist.
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

#include <sys/types.h>
#include <sys/systm.h>

#include <machine/db_machdep.h>

#include <ddb/db_interface.h>
#include <ddb/db_output.h>

static struct opcode {
	enum opc_fmt { OPC_PAL, OPC_RES, OPC_MEM, OPC_OP, OPC_BR } opc_fmt;
	char *opc_name;
	int opc_print;
} opcode[] = {
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
	{ OPC_OP, "inta", 1 },		/* 10 */
	{ OPC_OP, "intl", 1 },		/* 11 */
	{ OPC_OP, "ints", 1 },		/* 12 */
	{ OPC_OP, "intm", 1 },		/* 13 */
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

vm_offset_t 
db_disasm(loc, flag)
	vm_offset_t loc;
	boolean_t flag;
{
	u_int32_t ins = *(u_int32_t *)loc;
	int opc = ins >> 26;
	int arg = ins & 0x3ffffff;
	int ra, rb, rc, disp, func;

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
			db_printf("%08x", ins);
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
				db_printf("fetch\t0($%d)", rb);
				break;
			case 0xa000:
				db_printf("fetch_m\t0($%d)", rb);
				break;
			case 0xc000:
				db_printf("rpcc\t$%d", ra);
				break;
			case 0xe000:
				db_printf("rc\t$%d", ra);
				break;
			case 0xf000:
				db_printf("rs\t$%d", ra);
				break;
			default:
				db_printf("%08x", ins);
			        break;
			}
			break;
		case 0x1a:
			switch (disp >> 14) {
			case 0:
				db_printf("jmp\t$%d,($%d),0x%x", ra, rb,
				    disp & 0x3fff);
				break;
			case 1:
				db_printf("jsr\t$%d,($%d),0x%x", ra, rb,
				    disp & 0x3fff);
				break;
			case 2:
				db_printf("ret\t$%d,($%d),0x%x", ra, rb,
				    disp & 0x3fff);
				break;
			case 3:
				db_printf("jsr_coroutine\t$%d,($%d),0x%x", ra,
				    rb, disp & 0x3fff);
				break;
			}
			break;
		default:
			db_printf("\t$%d,0x%x($%d)", ra, disp, rb);
			break;
		}
		break;
	case OPC_OP:
		ra = arg >> 21;
		rb = (arg >> 16) & 0x1f;
		func = (arg >> 5) & 0x7ff;
		rc = arg & 0x1f;
		db_printf("\t%03x,$%d,$%d,$%d", func, ra, rb, rc);
		break;
	case OPC_BR:
		ra = arg >> 21;
		disp = arg & 0x1fffff;
		db_printf("\t$%d,0x%x", ra, disp);
		break;
	}
	db_printf("\n");
	return (loc + 4);
}
