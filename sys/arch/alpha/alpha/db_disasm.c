/*	$OpenBSD: db_disasm.c,v 1.3 1997/07/08 20:34:57 niklas Exp $	*/

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
} opcode[] = {
	{ OPC_PAL, "call_pal" },	/* 00 */
	{ OPC_RES, "opc01" },		/* 01 */
	{ OPC_RES, "opc02" },		/* 02 */
	{ OPC_RES, "opc03" },		/* 03 */
	{ OPC_RES, "opc04" },		/* 04 */
	{ OPC_RES, "opc05" },		/* 05 */
	{ OPC_RES, "opc06" },		/* 06 */
	{ OPC_RES, "opc07" },		/* 07 */
	{ OPC_MEM, "lda" },		/* 08 */
	{ OPC_MEM, "ldah" },		/* 09 */
	{ OPC_RES, "opc0a" },		/* 0A */
	{ OPC_MEM, "ldq_u" },		/* 0B */
	{ OPC_RES, "opc0c" },		/* 0C */
	{ OPC_RES, "opc0d" },		/* 0D */
	{ OPC_RES, "opc0e" },		/* 0E */
	{ OPC_MEM, "stq_u" },		/* 0F */
	{ OPC_OP, "inta" },		/* 10 */
	{ OPC_OP, "intl" },		/* 11 */
	{ OPC_OP, "ints" },		/* 12 */
	{ OPC_OP, "intm" },		/* 13 */
	{ OPC_RES, "opc14" },		/* 14 */
	{ OPC_OP, "fltv" },		/* 15 */
	{ OPC_OP, "flti" },		/* 16 */
	{ OPC_OP, "fltl" },		/* 17 */
	{ OPC_MEM, "misc" },		/* 18 */
	{ OPC_PAL, "pal19" },		/* 19 */
	{ OPC_MEM, "jsr" },		/* 1A */
	{ OPC_PAL, "pal1b" },		/* 1B */
	{ OPC_RES, "opc1c" },		/* 1C */
	{ OPC_PAL, "pal1d" },		/* 1D */
	{ OPC_PAL, "pal1e" },		/* 1E */
	{ OPC_PAL, "pal1f" },		/* 1F */
	{ OPC_MEM, "ldf" },		/* 20 */
	{ OPC_MEM, "ldg" },		/* 21 */
	{ OPC_MEM, "lds" },		/* 22 */
	{ OPC_MEM, "ldt" },		/* 23 */
	{ OPC_MEM, "stf" },		/* 24 */
	{ OPC_MEM, "stg" },		/* 25 */
	{ OPC_MEM, "sts" },		/* 26 */
	{ OPC_MEM, "stt" },		/* 27 */
	{ OPC_MEM, "ldl" },		/* 28 */
	{ OPC_MEM, "ldq" },		/* 29 */
	{ OPC_MEM, "ldl_l" },		/* 2A */
	{ OPC_MEM, "ldq_l" },		/* 2B */
	{ OPC_MEM, "stl" },		/* 2C */
	{ OPC_MEM, "stq" },		/* 2D */
	{ OPC_MEM, "stl_c" },		/* 2E */
	{ OPC_MEM, "stq_c" },		/* 2F */
	{ OPC_BR, "br" },		/* 30 */
	{ OPC_BR, "fbeq" },		/* 31 */
	{ OPC_BR, "fblt" },		/* 32 */
	{ OPC_BR, "fble" },		/* 33 */
	{ OPC_BR, "bsr" },		/* 34 */
	{ OPC_BR, "fbne" },		/* 35 */
	{ OPC_BR, "fbge" },		/* 36 */
	{ OPC_BR, "fbgt" },		/* 37 */
	{ OPC_BR, "blbc" },		/* 38 */
	{ OPC_BR, "beq" },		/* 39 */
	{ OPC_BR, "blt" },		/* 3A */
	{ OPC_BR, "ble" },		/* 3B */
	{ OPC_BR, "blbs" },		/* 3C */
	{ OPC_BR, "bne" },		/* 3D */
	{ OPC_BR, "bge" },		/* 3E */
	{ OPC_BR, "bgt" },		/* 3F */
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
			db_printf("\t0x%7x", arg);
			break;
		}
		break;
	case OPC_RES:
		db_printf("\t0x%8x", opc);
		break;
	case OPC_MEM:
		ra = arg >> 21;
		rb = (arg >> 16) & 0x1f;
		disp = arg & 0xffff;
		db_printf("\t$%d,$%d(%4x)", ra, rb, disp);
		break;
	case OPC_OP:
		ra = arg >> 21;
		rb = (arg >> 16) & 0x1f;
		func = (arg >> 5) & 0x7ff;
		rc = arg & 0x1f;
		db_printf("\t%3x,$%d,$%d,$%d", func, ra, rb, rc);
		break;
	case OPC_BR:
		ra = arg >> 21;
		disp = arg & 0x1fffff;
		db_printf("\t$%s,%6x", ra, disp);
		break;
	}
	db_printf("\n");
	return (0);
}
