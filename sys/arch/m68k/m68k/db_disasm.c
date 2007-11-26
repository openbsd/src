/*	$OpenBSD: db_disasm.c,v 1.10 2007/11/26 09:28:33 martynas Exp $	*/
/*	$NetBSD: db_disasm.c,v 1.19 1996/10/30 08:22:39 is Exp $	*/

/*
 * Copyright (c) 1994 Christian E. Hopps
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
 *      This product includes software developed by Christian E. Hopps.
 * 4. The name of the author may not be used to endorse or promote products
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
 * Notes:
 *
 * Much can be done with this format, with a lot of hacking even
 * a moto emul. could be built.  However things like symbol lookup
 * and reference are needed right away.
 *
 * the only functions that use the "get_xxx()" notation should be
 * ones that modify things in a dis_buffer_t besides the buffers.
 * (namely the used field)
 *
 * An attempt has been made to *always* increment dbuf->used++ immediately
 * after referencing a value beyond the current "short *" address.
 * this meant either only referencing the value once or placing it in
 * a local var.  If you play with this keep this style. Its very useful
 * in eliminating a very easy to make hard to find logic error.
 *
 * I broke style in 2 ways with one macro ``addchar()''
 * However it makes sense, consider that it is called *a lot* and
 * commonly with things like ","'s
 *
 * *dbuf->casm++ = ','; || ADDCHAR(dbuf,','); || addchar(dbuf,',');
 * I chose: 
 *	addchar(',');
 *
 * If this is not enough to convince you, please load up you emacs or
 * vi and do a fancy regex-replace, and compare for yourself.
 * (The 2 rules of style I broke if you failed to notice are:
 *  1: lower case macro name 2: implicit reference to local var name.)
 *
 * (chopps - March 1, 1994)
 */

#include <sys/param.h>

#include <machine/db_machdep.h>

#include <ddb/db_sym.h>
#include <ddb/db_output.h>
#include <m68k/m68k/db_disasm.h>

void get_modregstr(dis_buffer_t *, int, int, int, int);
void get_immed(dis_buffer_t *, int);
void get_fpustdGEN(dis_buffer_t *, u_short, const char *);
void addstr(dis_buffer_t *, const char *s);
void prints(dis_buffer_t *, int, int);
void printu(dis_buffer_t *, u_int, int);
void prints_wb(dis_buffer_t *, int, int, int);
void printu_wb(dis_buffer_t *, u_int, int, int);
void prints_bf(dis_buffer_t *, int, int, int);
void printu_bf(dis_buffer_t *, u_int, int, int);
void iaddstr(dis_buffer_t *, const char *s);
void iprints(dis_buffer_t *, int, int);
void iprintu(dis_buffer_t *, u_int, int);
void iprints_wb(dis_buffer_t *, int, int, int);
void iprintu_wb(dis_buffer_t *, u_int, int, int);
void make_cond(dis_buffer_t *, int , char *);
void print_fcond(dis_buffer_t *, char);
void print_mcond(dis_buffer_t *, char);
void print_disp(dis_buffer_t *, int, int, int);
void print_addr(dis_buffer_t *, u_long);
void print_reglist(dis_buffer_t *, int, u_short);
void print_freglist(dis_buffer_t *, int, u_short, int);
void print_fcode(dis_buffer_t *, u_short);

/* groups */
void opcode_bitmanip(dis_buffer_t *, u_short);
void opcode_move(dis_buffer_t *, u_short);
void opcode_misc(dis_buffer_t *, u_short);
void opcode_branch(dis_buffer_t *, u_short);
void opcode_coproc(dis_buffer_t *, u_short);
void opcode_0101(dis_buffer_t *, u_short);
void opcode_1000(dis_buffer_t *, u_short);
void opcode_addsub(dis_buffer_t *, u_short);
void opcode_1010(dis_buffer_t *, u_short);
void opcode_1011(dis_buffer_t *, u_short);
void opcode_1100(dis_buffer_t *, u_short);
void opcode_1110(dis_buffer_t *, u_short);
void opcode_fpu(dis_buffer_t *, u_short);
void opcode_mmu(dis_buffer_t *, u_short);
void opcode_mmu040(dis_buffer_t *, u_short);
void opcode_move16(dis_buffer_t *, u_short);

/* subs of groups */
void opcode_movec(dis_buffer_t *, u_short);
void opcode_divmul(dis_buffer_t *, u_short);
void opcode_movem(dis_buffer_t *, u_short);
void opcode_fmove_ext(dis_buffer_t *, u_short, u_short);
void opcode_pmove(dis_buffer_t *, u_short, u_short);
void opcode_pflush(dis_buffer_t *, u_short, u_short);

#define addchar(ch) (*dbuf->casm++ = ch)
#define iaddchar(ch) (*dbuf->cinfo++ = ch)

typedef void dis_func_t(dis_buffer_t *, u_short);

dis_func_t *const opcode_map[16] = {
	opcode_bitmanip, opcode_move, opcode_move, opcode_move,
	opcode_misc, opcode_0101, opcode_branch, opcode_move,
	opcode_1000, opcode_addsub, opcode_1010, opcode_1011,
	opcode_1100, opcode_addsub, opcode_1110, opcode_coproc
};

const char *const cc_table[16] = {
	"t", "f", "hi", "ls",
	"cc", "cs", "ne", "eq",
	"vc", "vs", "pl", "mi",
	"ge", "lt", "gt", "le"
};

const char *const fpcc_table[32] = {
	"f", "eq", "ogt", "oge", "olt", "ole", "ogl", "or",
	"un", "ueq", "ugt", "uge", "ult", "ule", "ne", "t",
	"sf", "seq", "gt", "ge", "lt", "le", "gl", "gle",
	"ngle", "ngl", "nle", "nlt", "nge", "ngt", "sne", "st" };

const char *const mmcc_table[16] = {
	"bs", "bc", "ls", "lc", "ss", "sc", "as", "sc",
	"ws", "wc", "is", "ic", "gs", "gc", "cs", "cc" };


const char *const aregs[8] = {"a0","a1","a2","a3","a4","a5","a6","sp"};
const char *const dregs[8] = {"d0","d1","d2","d3","d4","d5","d6","d7"};
const char *const fpregs[8] = {
	"fp0","fp1","fp2","fp3","fp4","fp5","fp6","fp7" };
const char *const fpcregs[3] = { "fpiar", "fpsr", "fpcr" };

/*
 * Disassemble intruction at location ``loc''.
 * Returns location of next instruction.
 */

static char asm_buffer[256];
static char info_buffer[256];

db_addr_t 
db_disasm(loc, moto_syntax)
	db_addr_t loc;
	boolean_t moto_syntax;
{
	u_short opc;
	dis_func_t *func;
	dis_buffer_t dbuf;

	dbuf.casm = dbuf.dasm = asm_buffer;
	dbuf.cinfo = dbuf.info = info_buffer;
	dbuf.used = 0;
	dbuf.val = (short *)loc;
	dbuf.mit = moto_syntax ? 0 : 1;
	
	dbuf.dasm[0] = 0;
	dbuf.info[0] = 0;

	opc = *dbuf.val;
	dbuf.used++;
	
	func = opcode_map[OPCODE_MAP(opc)];
	func(&dbuf, opc);

	db_printf("%s",asm_buffer);
	if (info_buffer[0]) 
		db_printf("\t[%s]\n",info_buffer);
	else
		db_printf("\n");
	return (loc + sizeof(short)*dbuf.used);
}
/*
 * Bit manipulation/MOVEP/Immediate.
 */
void
opcode_bitmanip(dbuf, opc)
	dis_buffer_t *dbuf;
	u_short opc;
{
	char *tmp;
	u_short ext;
	int sz;

	tmp = NULL;
	
	switch (opc) {
	case ANDITOCCR_INST:
		tmp = "andib\t";
		break;
	case ANDIROSR_INST:
		tmp = "andiw\t";
		break;
	case EORITOCCR_INST:
		tmp = "eorib\t";
		break;
	case EORITOSR_INST:
		tmp = "eoriw\t";
		break;
	case ORITOCCR_INST:
		tmp = "orib\t";
		break;
	case ORITOSR_INST:
		tmp = "oriw\t";
	 	break;
	}
	if (tmp) {
		addstr(dbuf, tmp);
		if (ISBITSET(opc,6)) {
			get_immed(dbuf, SIZE_WORD);
			addstr(dbuf, ",sr");
		} else {
			get_immed(dbuf, SIZE_BYTE);
			addstr(dbuf, ",ccr");
		}
		return;
	}

	if (IS_INST(RTM,opc)) {
		addstr(dbuf, "rtm\t");
		if (ISBITSET(opc,3))
			PRINT_AREG(dbuf, BITFIELD(opc,2,0));
		else
			PRINT_DREG(dbuf, BITFIELD(opc,2,0));
		return;
	}

	if (IS_INST(MOVEP,opc)) {
		addstr(dbuf, "movp");
		if (ISBITSET(opc,6)) 
			addchar('l');
		else
			addchar('w');
		addchar('\t');
		if (ISBITSET(opc,7)) {
			PRINT_DREG(dbuf, BITFIELD(opc, 11, 9));
			addchar(',');
		}
		PRINT_AREG(dbuf, BITFIELD(opc, 2, 0));
		addchar('@');
		addchar('(');
		print_disp(dbuf, *(dbuf->val + 1), SIZE_WORD,
		    BITFIELD(opc, 2, 0));
		dbuf->used++;
		addchar(')');
		if (!ISBITSET(opc,7)) {
			addchar(',');
			PRINT_DREG(dbuf, BITFIELD(opc, 11, 9));
		}
		return;
	}

	switch (opc & BCHGD_MASK) {
	case BCHGD_INST:
		tmp = "bchg\t";
		break;
	case BCLRD_INST:
		tmp = "bclr\t";
		break;
	case BSETD_INST:
		tmp = "bset\t";
		break;
	case BTSTD_INST:
		tmp = "btst\t";
		break;
	}
	if (tmp) {
		addstr(dbuf, tmp);
		PRINT_DREG(dbuf, BITFIELD(opc,11,9));
		addchar(',');
		get_modregstr(dbuf,5,GETMOD_BEFORE,0,0);
		return;
	}

	switch (opc & BCHGS_MASK) {
	case BCHGS_INST:
		tmp = "bchg\t";
		break;
	case BCLRS_INST:
		tmp = "bclr\t";
		break;
	case BSETS_INST:
		tmp = "bset\t";
		break;
	case BTSTS_INST:
		tmp = "btst\t";
		break;
	}
	if (tmp) {
		addstr(dbuf, tmp);
		get_immed(dbuf, SIZE_BYTE);
		addchar(',');
		get_modregstr(dbuf, 5, GETMOD_BEFORE, 0, 1);
		return;
	}

	if (IS_INST(CAS2,opc)) {
		u_short ext2;

		ext = *(dbuf->val + 1);
		ext2 = *(dbuf->val + 2);
		dbuf->used += 2;
		
		if (ISBITSET(opc,9))
			addstr(dbuf, "cas2l\t");
		else
			addstr(dbuf, "cas2w\t");

		PRINT_DREG(dbuf, BITFIELD(ext,2,0));
		addchar(':');
		PRINT_DREG(dbuf, BITFIELD(ext2,2,0));
		addchar(',');
		
		PRINT_DREG(dbuf, BITFIELD(ext,8,6));
		addchar(':');
		PRINT_DREG(dbuf, BITFIELD(ext2,8,6));
		addchar(',');
		
		if (ISBITSET(ext,15))
			PRINT_AREG(dbuf, BITFIELD(ext,14,12));
		else
			PRINT_DREG(dbuf, BITFIELD(ext,14,12));
		addchar('@');
		addchar(':');
		if (ISBITSET(ext2,15))
			PRINT_AREG(dbuf, BITFIELD(ext2,14,12));
		else
			PRINT_DREG(dbuf, BITFIELD(ext2,14,12));
		addchar('@');
		return;
	}

	switch (opc & CAS_MASK) {
	case CAS_INST:
		ext = *(dbuf->val + 1);
		dbuf->used++;

		addstr(dbuf,"cas");
		sz = BITFIELD(opc,10,9);
		if (sz == 0) {
			sz = SIZE_BYTE;
			addchar('b');
		} else if (sz == 1) {
			sz = SIZE_WORD;
			addchar('w');
		} else {
			sz = SIZE_LONG;
			addchar('l');
		}
		addchar('\t');
		PRINT_DREG(dbuf, BITFIELD(ext, 2, 0));
		addchar(',');
		PRINT_DREG(dbuf, BITFIELD(ext, 8, 6));
		addchar(',');
		get_modregstr(dbuf, 5, GETMOD_BEFORE, sz, 1);
		return;
	case CHK2_INST:
	/* case CMP2_INST: */
		ext = *(dbuf->val + 1);
		dbuf->used++;

		if (ISBITSET(ext,11)) 
			addstr(dbuf,"chk2");
		else 
			addstr(dbuf,"cmp2");
			
		sz = BITFIELD(opc,10,9);
		if (sz == 0) {
			sz = SIZE_BYTE;
			addchar('b');
		} else if (sz == 1) {
			sz = SIZE_WORD;
			addchar('w');
		} else {
			sz = SIZE_LONG;
			addchar('l');
		}
		addchar('\t');
		get_modregstr(dbuf, 5, GETMOD_BEFORE, sz, 1);

		addchar(',');
		if(ISBITSET(ext,15))
			PRINT_AREG(dbuf, BITFIELD(ext, 14, 12));
		else
			PRINT_DREG(dbuf, BITFIELD(ext, 14, 12));
		return;
	}
	
	switch (ADDI_MASK & opc) {
	case MOVES_INST:
		addstr(dbuf, "movs");
		sz = BITFIELD(opc,7,6);
		if (sz == 0) {
			addchar('b');
			sz = SIZE_BYTE;
		} else if (sz == 1) {
			addchar('w');
			sz = SIZE_WORD;
		} else {
			addchar ('l');
			sz = SIZE_LONG;
		}
		addchar('\t');
		
		ext = *(dbuf->val + 1);
		dbuf->used++;
		
		if (ISBITSET(ext,11)) {
			if (ISBITSET(ext,15)) 
				PRINT_AREG(dbuf,BITFIELD(ext,14,12));
			else
				PRINT_DREG(dbuf,BITFIELD(ext,14,12));
			addchar(',');
			get_modregstr(dbuf, 5, GETMOD_BEFORE, sz, 1);
		} else {
			get_modregstr(dbuf, 5, GETMOD_BEFORE, sz, 1);
			addchar(',');
			if (ISBITSET(ext,15)) 
				PRINT_AREG(dbuf,BITFIELD(ext,14,12));
			else
				PRINT_DREG(dbuf,BITFIELD(ext,14,12));
		}			
		return;
	case ADDI_INST:
		tmp = "addi";
		break;
	case ANDI_INST:
		tmp = "andi";
		break;
	case CMPI_INST:
		tmp = "cmpi";
		break;
	case EORI_INST:
		tmp = "eori";
		break;
	case ORI_INST:
		tmp = "ori";
		break;
	case SUBI_INST:
		tmp = "subi";
		break;
	}
	if (tmp) {
		addstr(dbuf, tmp);
		sz = BITFIELD(opc,7,6);
		switch (sz) {
		case 0:
			addchar('b');
			addchar('\t');
			sz = SIZE_BYTE;
			break;
		case 1:
			addchar('w');
			addchar('\t');
			sz = SIZE_WORD;
			break;
		case 2:
			addchar ('l');
			addchar('\t');
			get_immed(dbuf,SIZE_LONG);
			addchar(',');
			get_modregstr(dbuf,5,GETMOD_BEFORE,SIZE_LONG,2);
			return;
		}
		get_immed(dbuf,sz);
		addchar(',');
		get_modregstr(dbuf,5,GETMOD_BEFORE,sz,1);
		return;
	}
}

/*
 * move byte/word/long and q
 * 00xx (01==.b 10==.l 11==.w) and 0111(Q)
 */
void
opcode_move(dbuf, opc)
	dis_buffer_t *dbuf;
	u_short opc;
{
	int sz, lused;

	sz = 0;
	switch (OPCODE_MAP(opc)) {
	case 0x1:		/* move.b */
		sz = SIZE_BYTE;
		break;
	case 0x3:		/* move.w */
		sz = SIZE_WORD;
		break;
	case 0x2:		/* move.l */
		sz = SIZE_LONG;
		break;
	case 0x7:		/* moveq */
		addstr(dbuf, "movq\t#");
		prints_bf(dbuf, opc, 7, 0);
		addchar(',');
		PRINT_DREG(dbuf,BITFIELD(opc,11,9));
		return;	
	}
	addstr(dbuf, "mov");

	if (BITFIELD(opc,8,6) == AR_DIR) 
		addchar('a');

	if (sz == SIZE_BYTE)
		addchar('b');
	else if (sz == SIZE_WORD)
		addchar('w');
	else
		addchar('l');

	addchar('\t');
	lused = dbuf->used;
	get_modregstr(dbuf, 5, GETMOD_BEFORE, sz, 0);
	addchar(',');
	get_modregstr(dbuf, 11, GETMOD_AFTER, sz, dbuf->used - lused);
}

/*
 * misc opcodes.
 */
void
opcode_misc(dbuf, opc)
	dis_buffer_t *dbuf;
	u_short opc;
{
	char *tmp;
	int sz;

	tmp = NULL;
	    
	/* Check against no option instructions */
	switch (opc) {
	case BGND_INST:
		tmp = "bgnd";
		break;
	case ILLEGAL_INST:
		tmp = "illegal";
		break;
	case MOVEFRC_INST:
	case MOVETOC_INST:
		opcode_movec(dbuf, opc);
		return;
	case NOP_INST:
		tmp = "nop";
		break;
	case RESET_INST:
		tmp = "reset";
		break;
	case RTD_INST:
		addstr(dbuf, "rtd\t");
		get_immed(dbuf, SIZE_WORD);
		return;
	case RTE_INST:
		tmp = "rte";
		break;
	case RTR_INST:
		tmp = "rtr";
		break;
	case RTS_INST:
		tmp = "rts";
		break;
	case STOP_INST:
		addstr(dbuf, "stop\t");
		get_immed(dbuf, SIZE_WORD);
		return;
	case TRAPV_INST:
		tmp = "trapv";
		break;
	default:
		break;
	}
	if (tmp) {
		addstr(dbuf, tmp);
		return;
	}

	switch (opc & BKPT_MASK) {
	case BKPT_INST:
		addstr(dbuf, "bkpt\t#");
		printu_bf(dbuf, opc, 2, 0);
		return;
	case EXTBW_INST:
		addstr(dbuf, "extw\t");
		get_modregstr(dbuf,2,DR_DIR,0,0);
		return;
	case EXTWL_INST:
		addstr(dbuf, "extl\t");
		get_modregstr(dbuf,2,DR_DIR,0,0);
		return;
	case EXTBL_INST:
		addstr(dbuf, "extbl\t");
		get_modregstr(dbuf,2,DR_DIR,0,0);
		return;
	case LINKW_INST:
	case LINKL_INST:
		if ((LINKW_MASK & opc) == LINKW_INST) {
			addstr(dbuf, "linkw\t");
			get_modregstr(dbuf, 2, AR_DIR, 0, 1);
		} else {
			addstr(dbuf, "linkl\t");
			get_modregstr(dbuf, 2, AR_DIR, 0, 2);
		}
		addchar(',');
		if ((LINKW_MASK & opc) == LINKW_INST)
			get_immed(dbuf, SIZE_WORD);
		else 
			get_immed(dbuf,SIZE_LONG);
		return;
	case MOVETOUSP_INST:
	case MOVEFRUSP_INST:
		addstr(dbuf, "movl\t");
		if (!ISBITSET(opc,3)) {
			get_modregstr(dbuf, 2, AR_DIR, 0, 0);
			addchar(',');
		}
		addstr(dbuf, "usp");
		if (ISBITSET(opc,3)) {
			addchar(',');
			get_modregstr(dbuf, 2, AR_DIR, 0, 0);
		}
		return;
	case SWAP_INST:
		addstr(dbuf, "swap\t");
		get_modregstr(dbuf, 2, DR_DIR, 0, 0);
		return;
	case UNLK_INST:
		addstr(dbuf, "unlk\t");
		get_modregstr(dbuf, 2, AR_DIR, 0, 0);
		return;
	}
	
	if ((opc & TRAP_MASK) == TRAP_INST) {
		addstr(dbuf, "trap\t#");
		printu_bf(dbuf, opc, 3, 0);
		return;
	}

	sz = 0;
	switch (DIVSL_MASK & opc) {
	case DIVSL_INST:
	case MULSL_INST:
		opcode_divmul(dbuf, opc);
		return;
	case JMP_INST:
		tmp = "jmp\t";
		break;
	case JSR_INST:
		tmp = "jsr\t";
		break;
	case MOVEFRCCR_INST:
		tmp = "mov\tccr,";
		break;
	case MOVEFRSR_INST:
		tmp = "mov\tsr,";
		break;
	case NBCD_INST:
		tmp = "nbcd\t";
		break;
	case PEA_INST:
		tmp = "pea\t";
		break;
	case TAS_INST:
		tmp = "tas\t";
		break;
	case MOVETOCCR_INST:
	case MOVETOSR_INST:
		tmp = "mov\t";
		sz = SIZE_WORD;
		break;
	}
	if (tmp) {
		addstr(dbuf, tmp);
		get_modregstr(dbuf,5, GETMOD_BEFORE, sz, 0);
		if(IS_INST(MOVETOSR,opc))
			addstr(dbuf, ",sr");
		else if(IS_INST(MOVETOCCR,opc))
			addstr(dbuf, ",ccr");
		return;
	}

	if ((opc & MOVEM_MASK) == MOVEM_INST) {
		opcode_movem(dbuf, opc);
		return;
	}

	switch (opc & CLR_MASK) {
	case CLR_INST:
		tmp = "clr";
		break;
	case NEG_INST:
		tmp = "neg";
		break;
	case NEGX_INST:
		tmp = "negx";
		break;
	case NOT_INST:
		tmp = "not";
		break;
	case TST_INST:
		tmp = "tst";
		break;
	}
	if (tmp) {
		int sz, msz;
		
		addstr(dbuf, tmp);

		msz = BITFIELD(opc,7,6);
		if (msz == 0) {
			tmp = "b\t";
			sz = SIZE_BYTE;
		} else if (msz == 1) {
			tmp = "w\t";
			sz = SIZE_WORD;
		} else {
			tmp = "l\t";
			sz = SIZE_LONG;
		}
		addstr(dbuf, tmp);
		get_modregstr(dbuf, 5, GETMOD_BEFORE, sz, 0);
		return;
	}
	
	if ((opc & LEA_MASK) == LEA_INST) {
		addstr(dbuf, "lea\t");
		get_modregstr(dbuf, 5, GETMOD_BEFORE, SIZE_LONG, 0);
		addchar(',');
		get_modregstr(dbuf, 11, AR_DIR, 0, 0);
		return;
	} else if ((opc & CHK_MASK) == CHK_INST) {
		if (BITFIELD(opc,8,7) == 0x3) {
			addstr(dbuf, "chkw\t");
			get_modregstr(dbuf, 5, GETMOD_BEFORE, SIZE_WORD, 0);
		} else {
			addstr(dbuf, "chkl\t");
			get_modregstr(dbuf, 5, GETMOD_BEFORE, SIZE_LONG, 0);
		}
		addchar(',');
		get_modregstr(dbuf, 11, DR_DIR, 0, 0);
		return;
	}	
}

/*
 * ADDQ/SUBQ/Scc/DBcc/TRAPcc
 */
void
opcode_0101(dbuf, opc)
	dis_buffer_t *dbuf;
	u_short opc;
{
	int data;

	if (IS_INST(TRAPcc, opc) && BITFIELD(opc,2,0) > 1) {
		int opmode;

		opmode = BITFIELD(opc,2,0);
		make_cond(dbuf,11,"trap");

		if (opmode == 0x2) {
			addchar('w');
			addchar('\t');
			get_immed(dbuf, SIZE_WORD);
		} else if (opmode == 0x3) {
			addchar('l');
			addchar('\t');
			get_immed(dbuf, SIZE_LONG);
		}
		return;
	} else if (IS_INST(DBcc, opc)) {
		make_cond(dbuf,11,"db");
		addchar('\t');
		PRINT_DREG(dbuf, BITFIELD(opc,2,0));
		addchar(',');
		print_disp(dbuf, *(dbuf->val + 1), SIZE_WORD, -1);
		dbuf->used++;
		return;
	} else if (IS_INST(Scc,opc)) {
		make_cond(dbuf,11,"s");
		addchar('\t');
		get_modregstr(dbuf, 5, GETMOD_BEFORE, SIZE_BYTE, 0);
		return;
	} else if (IS_INST(ADDQ, opc) || IS_INST(SUBQ, opc)) {
		int size = BITFIELD(opc,7,6);

		if (IS_INST(SUBQ, opc)) 
			addstr(dbuf, "subq");
		else
			addstr(dbuf, "addq");
		
		if (size == 0x1) 
			addchar('w');
		else if (size == 0x2)
			addchar('l');
		else
			addchar('b');
		
		addchar('\t');
		addchar('#');
		data = BITFIELD(opc,11,9);
		if (data == 0)
			data = 8;
		printu(dbuf, data, SIZE_BYTE);
		addchar(',');
		get_modregstr(dbuf, 5, GETMOD_BEFORE, 0, 0);
		
		return;
	}
}

/*
 * Bcc/BSR/BRA
 */
void
opcode_branch(dbuf, opc)
	dis_buffer_t *dbuf;
	u_short opc;
{
	int disp, sz;

	if (IS_INST(BRA,opc))
	    addstr(dbuf, "bra");	    
	else if (IS_INST(BSR,opc))
	    addstr(dbuf, "bsr");	    
	else 
	    make_cond(dbuf,11,"b");

	disp = BITFIELD(opc,7,0);
	if (disp == 0) {
		/* 16-bit signed displacement */
		disp = *(dbuf->val + 1);
		dbuf->used++;
		sz = SIZE_WORD;
		addchar('w');
	} else if (disp == 0xff) {
		/* 32-bit signed displacement */
		disp = *(long *)(dbuf->val + 1);
		dbuf->used += 2;
		sz = SIZE_LONG;
		addchar('l');
	} else {
		/* 8-bit signed displacement in opcode. */
		/* Needs to be sign-extended... */
		if (ISBITSET(disp,7))
			disp -= 256;
		sz = SIZE_BYTE;
		addchar('b');
	}
	addchar('\t');
	print_addr(dbuf, disp + (u_long)dbuf->val + 2);
}

/*
 * ADD/ADDA/ADDX/SUB/SUBA/SUBX
 */
void
opcode_addsub(dbuf, opc)
	dis_buffer_t *dbuf;
	u_short opc;
{
	int sz, ch, amode;
	
	sz = BITFIELD(opc,7,6);
	amode = 0;
	
	if (sz == 0) {
		ch = 'b';
		sz = SIZE_BYTE;
	} else if (sz == 1) {
		ch = 'w';
		sz = SIZE_WORD;
	} else if (sz == 2) {
		ch = 'l';
		sz = SIZE_LONG;
	} else {
		amode = 1;
		if (!ISBITSET(opc,8))  {
			sz = SIZE_WORD;
			ch = 'w';
		} else {
			sz = SIZE_LONG;
			ch = 'l';
		}
	}
	
	if (!amode && (IS_INST(ADDX,opc) || IS_INST(SUBX,opc))) {
		if (IS_INST(ADDX,opc))
			addstr(dbuf,"addx");
		else
			addstr(dbuf,"subx");

		addchar(ch);
		addchar('\t');
		
		if (ISBITSET(opc,3)) {
			PRINT_AREG(dbuf,BITFIELD(opc,2,0));
			addchar('@');
			addchar('-');
			addchar(',');
			PRINT_AREG(dbuf,BITFIELD(opc,11,9));
			addchar('@');
			addchar('-');
		} else {
			PRINT_DREG(dbuf,BITFIELD(opc,2,0));
			addchar(',');
			PRINT_DREG(dbuf,BITFIELD(opc,11,9));
		}
	} else {
		if (IS_INST(ADD,opc))
			addstr(dbuf, "add");
		else
			addstr(dbuf, "sub");

		if (amode)
			addchar('a');
		addchar(ch);
		addchar('\t');

		if (ISBITSET(opc,8) && amode == 0) {
			PRINT_DREG(dbuf,BITFIELD(opc,11,9));
			addchar(',');
			get_modregstr(dbuf, 5, GETMOD_BEFORE, sz, 0);
		} else {
			get_modregstr(dbuf, 5, GETMOD_BEFORE, sz, 0);
			addchar(',');
			if (amode)
				PRINT_AREG(dbuf,BITFIELD(opc,11,9));
			else
				PRINT_DREG(dbuf,BITFIELD(opc,11,9));
		}
	}
	return;
}

/*
 * Shift/Rotate/Bit Field
 */
void
opcode_1110(dbuf, opc)
	dis_buffer_t *dbuf;
	u_short opc;
{
	char *tmp;
	u_short ext;
	int type, sz;

	tmp = NULL;
	
	switch (opc & BFCHG_MASK) {
	case BFCHG_INST:
		tmp = "bfchg";
		break;
	case BFCLR_INST:
		tmp = "bfclr";
		break;
	case BFEXTS_INST:
		tmp = "bfexts";
		break;
	case BFEXTU_INST:
		tmp = "bfextu";
		break;
	case BFFFO_INST:
		tmp = "bfffo";
		break;
	case BFINS_INST:
		tmp = "bfins";
		break;
	case BFSET_INST:
		tmp = "bfset";
		break;
	case BFTST_INST:
		tmp = "bftst";
		break;
	}
	if (tmp) {
		short bf;
		
		addstr(dbuf, tmp);
		addchar('\t');
		
		ext = *(dbuf->val + 1);
		dbuf->used++;
		
		if (IS_INST(BFINS,opc)) {
			PRINT_DREG(dbuf, BITFIELD(ext,14,12));
			addchar(',');
		}
		get_modregstr(dbuf, 5, GETMOD_BEFORE, 0, 1);
		addchar('{');

		bf = BITFIELD(ext,10,6);
		if (ISBITSET(ext, 11)) 
			PRINT_DREG(dbuf, bf);
		else 			
			printu_wb(dbuf, bf, SIZE_BYTE, 10);
		
		addchar(':');

		bf = BITFIELD(ext, 4, 0);
		if (ISBITSET(ext, 5))
			PRINT_DREG(dbuf, bf);
		else {
			if (bf == 0)
				bf = 32;
			printu_wb(dbuf, bf, SIZE_BYTE, 10);
		}
		addchar('}');
		if (ISBITSET(opc,8) && !IS_INST(BFINS,opc)) {
			addchar(',');
			PRINT_DREG(dbuf, BITFIELD(ext,14,12));
		} else
			*dbuf->casm = 0;
		return;
	}
	sz = BITFIELD(opc,7,6);
	if (sz == 0x3)
		type = BITFIELD(opc, 10, 9);
	else
		type = BITFIELD(opc, 4, 3);

	switch (type) {
	case AS_TYPE:
		addchar('a');
		addchar('s');
		break;
	case LS_TYPE:
		addchar('l');
		addchar('s');
		break;
	case RO_TYPE:
		addchar('r');
		addchar('o');
		break;
	case ROX_TYPE:
		addchar('r');
		addchar('o');
		addchar('x');
		break;
	}

	if (ISBITSET(opc,8))
		addchar('l');
	else
		addchar('r');
	
	switch (sz) {
	case 0:
		sz = SIZE_BYTE;
		addchar('b');
		break;
	case 3:
	case 1:
		sz = SIZE_WORD;
		addchar('w');
		break;
	case 2:
		sz = SIZE_LONG;
		addchar('l');
		break;
		
	}
	addchar('\t');
	if(BITFIELD(opc,7,6) == 0x3) {
		get_modregstr(dbuf, 5, GETMOD_BEFORE, sz, 0);
		return;
	} else if (ISBITSET(opc,5)) 
		PRINT_DREG(dbuf, BITFIELD(opc,11,9));
	else {
		addchar('#');
		sz = BITFIELD(opc,11,9);
		if (sz == 0)
			sz = 8;
		printu_wb(dbuf, sz, SIZE_BYTE, 10);
	}
	addchar(',');
	PRINT_DREG(dbuf, BITFIELD(opc,2,0));
	return;
}

/*
 * CMP/CMPA/EOR
 */
void
opcode_1011(dbuf, opc)
	dis_buffer_t *dbuf;
	u_short opc;
{
	int sz;
	
	if (IS_INST(CMPA,opc)) {
		addstr(dbuf, "cmpa");
		
		if (ISBITSET(opc, 8)) {
			addchar('l');
			sz = SIZE_LONG;
		} else {
			addchar('w');
			sz = SIZE_WORD;
		}
		addchar('\t');
	} else {
		if (IS_INST(CMP, opc))
			addstr(dbuf, "cmp");
		else
			addstr(dbuf, "eor");

		sz = BITFIELD(opc,7,6);
		switch (sz) {
		case 0:
			addchar('b');
			sz = SIZE_BYTE;
			break;
		case 1:
			addchar('w');
			sz = SIZE_WORD;
			break;
		case 2:
			addchar('l');
			sz = SIZE_LONG;
			break;
		}
		addchar('\t');
		if (IS_INST(EOR,opc)) {
			PRINT_DREG(dbuf, BITFIELD(opc,11,9));
			addchar(',');
		}
	}
	get_modregstr(dbuf, 5, GETMOD_BEFORE, sz, 0);

	if (IS_INST(CMPA,opc)) {
		addchar(',');
		PRINT_AREG(dbuf, BITFIELD(opc,11,9));
	} else if (IS_INST(CMP,opc)) {
		addchar(',');
		PRINT_DREG(dbuf, BITFIELD(opc,11,9));
	}
	return;
}

/*
 * OR/DIV/SBCD
 */
void
opcode_1000(dbuf, opc)
	dis_buffer_t *dbuf;
	u_short opc;
{
	int sz;
	
	if (IS_INST(UNPKA,opc)) {
		addstr(dbuf, "unpk\t");
		PRINT_AREG(dbuf,BITFIELD(opc,2,0));
		addstr(dbuf, "@-,");
		PRINT_AREG(dbuf,BITFIELD(opc,11,9));
		addstr(dbuf, "@-,");
		get_immed(dbuf,SIZE_WORD);
	} else if (IS_INST(UNPKD,opc)) {
		addstr(dbuf, "unpk\t");
		PRINT_DREG(dbuf,BITFIELD(opc,2,0));
		addchar(',');
		PRINT_DREG(dbuf,BITFIELD(opc,11,9));
		addchar(',');
		get_immed(dbuf,SIZE_WORD);
	} else if (IS_INST(SBCDA,opc)) {
		addstr(dbuf, "sbcd\t");
		PRINT_AREG(dbuf,BITFIELD(opc,2,0));
		addstr(dbuf, "@-,");
		PRINT_AREG(dbuf,BITFIELD(opc,11,9));
		addstr(dbuf, "@-");
	} else if (IS_INST(SBCDA,opc)) {
		addstr(dbuf, "sbcd\t");
		PRINT_DREG(dbuf,BITFIELD(opc,2,0));
		addchar(',');
		PRINT_DREG(dbuf,BITFIELD(opc,11,9));
	} else if (IS_INST(DIVSW,opc) || IS_INST(DIVUW,opc)) {
		if (IS_INST(DIVSW,opc))
			addstr(dbuf, "divsw\t");
		else
			addstr(dbuf, "divuw\t");
		get_modregstr(dbuf, 5, GETMOD_BEFORE, SIZE_WORD, 0);
		addchar(',');
		PRINT_DREG(dbuf, BITFIELD(opc,11,9));
	} else {
		addstr(dbuf, "or");

		sz = BITFIELD(opc,7,6);
		switch (sz) {
		case 0:
			addchar('b');
			sz = SIZE_BYTE;
			break;
		case 1:
			addchar('w');
			sz = SIZE_WORD;
			break;
		case 2:
			addchar('l');
			sz = SIZE_LONG;
			break;
		}
		addchar('\t');
		if (ISBITSET(opc,8)) {
			PRINT_DREG(dbuf, BITFIELD(opc,11,9));
			addchar(',');
		}
		get_modregstr(dbuf, 5, GETMOD_BEFORE, sz, 0);
		if (!ISBITSET(opc,8)) {
			addchar(',');
			PRINT_DREG(dbuf, BITFIELD(opc,11,9));
		}
	}
}

/*
 * AND/MUL/ABCD/EXG (1100)
 */
void
opcode_1100(dbuf, opc)
	dis_buffer_t *dbuf;
	u_short opc;
{
	int sz;
	
	if (IS_INST(ABCDA,opc)) {
		addstr(dbuf, "abcd\t");
		PRINT_AREG(dbuf,BITFIELD(opc,2,0));
		addstr(dbuf, "@-,");
		PRINT_AREG(dbuf,BITFIELD(opc,11,9));
		addstr(dbuf, "@-");
	} else if (IS_INST(ABCDA,opc)) {
		addstr(dbuf, "abcd\t");
		PRINT_DREG(dbuf,BITFIELD(opc,2,0));
		addchar(',');
		PRINT_DREG(dbuf,BITFIELD(opc,11,9));
	} else if (IS_INST(MULSW,opc) || IS_INST(MULUW,opc)) {
		if (IS_INST(MULSW,opc))
			addstr(dbuf, "mulsw\t");
		else
			addstr(dbuf, "muluw\t");
		get_modregstr(dbuf, 5, GETMOD_BEFORE, SIZE_WORD, 0);
		addchar(',');
		PRINT_DREG(dbuf, BITFIELD(opc,11,9));
	} else if (IS_INST(EXG,opc)) {
		addstr(dbuf, "exg\t");
		if (ISBITSET(opc,7)) {
			PRINT_DREG(dbuf,BITFIELD(opc,11,9));
			addchar(',');
			PRINT_AREG(dbuf,BITFIELD(opc,2,0));
		} else if (ISBITSET(opc,3)) {
			PRINT_AREG(dbuf,BITFIELD(opc,11,9));
			addchar(',');
			PRINT_AREG(dbuf,BITFIELD(opc,2,0));
		} else {
			PRINT_DREG(dbuf,BITFIELD(opc,11,9));
			addchar(',');
			PRINT_DREG(dbuf,BITFIELD(opc,2,0));
		}
	} else {
		addstr(dbuf, "and");

		sz = BITFIELD(opc,7,6);
		switch (sz) {
		case 0:
			addchar('b');
			sz = SIZE_BYTE;
			break;
		case 1:
			addchar('w');
			sz = SIZE_WORD;
			break;
		case 2:
			addchar('l');
			sz = SIZE_LONG;
			break;
		}
		addchar('\t');
		
		if (ISBITSET(opc,8)) {
			PRINT_DREG(dbuf, BITFIELD(opc,11,9));
			addchar(',');
		}
		get_modregstr(dbuf, 5, GETMOD_BEFORE, sz, 0);
		if (!ISBITSET(opc,8)) {
			addchar(',');
			PRINT_DREG(dbuf, BITFIELD(opc,11,9));
		}
	}
}

/*
 * Coprocessor instruction
 */
void
opcode_coproc(dbuf, opc)
	dis_buffer_t *dbuf;
	u_short opc;
{
	switch (BITFIELD(*dbuf->val,11,9)) {
	case 1:
		opcode_fpu(dbuf, opc);
		return;
	case 0:
		opcode_mmu(dbuf, opc);
		return;
	case 2:
		opcode_mmu040(dbuf, opc);
		return;
	case 3:
		opcode_move16(dbuf, opc);
		return;
	}
	switch (BITFIELD(opc,8,6)) {
	case 0:
		dbuf->used++;
		break;
	case 3:
		dbuf->used++;
		/*FALLTHROUGH*/
	case 2:
		dbuf->used++;
		break;
	case 1:
		dbuf->used++;
	case 4:
	case 5:
	default:
	}
	addstr(dbuf, "UNKNOWN COPROC OPCODE");
	return;
}

/*
 * Resvd
 */
void
opcode_1010(dbuf, opc)
	dis_buffer_t *dbuf;
	u_short opc;
{
	addstr(dbuf, "RSVD");
	dbuf->used++;
}

void
opcode_fpu(dbuf, opc)
	dis_buffer_t *dbuf;
	u_short opc;
{
	u_short ext;
	int type, opmode;

	type = BITFIELD(opc,8,6);
	switch (type) {
	/* cpGEN */
	case 0:
		ext = *(dbuf->val + 1);
		dbuf->used++;
		opmode = BITFIELD(ext,5,0);

		if (BITFIELD(opc,5,0) == 0 && BITFIELD(ext,15,10) == 0x17) {
			addstr(dbuf,"fmovcrx #");
			printu(dbuf,BITFIELD(ext,6,0),SIZE_BYTE);
			return;
		}
		if (ISBITSET(ext,15) || ISBITSET(ext,13)) {
			opcode_fmove_ext(dbuf, opc, ext);
			return;
		}

		switch(opmode) {
		case FMOVE:
			get_fpustdGEN(dbuf,ext,"fmov");
			return;
		case FABS:
			get_fpustdGEN(dbuf,ext,"fabs");
			return;
		case FACOS:
			get_fpustdGEN(dbuf,ext,"facos");
			return;
		case FADD:
			get_fpustdGEN(dbuf,ext,"fadd");
			return;
		case FASIN:
			get_fpustdGEN(dbuf,ext,"fasin");
			return;
		case FATAN:
			get_fpustdGEN(dbuf,ext,"fatan");
			return;
		case FATANH:
			get_fpustdGEN(dbuf,ext,"fatanh");
			return;
		case FCMP:
			get_fpustdGEN(dbuf,ext,"fcmp");
			return;
		case FCOS:
			get_fpustdGEN(dbuf,ext,"fcos");
			return;
		case FCOSH:
			get_fpustdGEN(dbuf,ext,"fcosh");
			return;
		case FDIV:
			get_fpustdGEN(dbuf,ext,"fdiv");
			return;
		case FETOX:
			get_fpustdGEN(dbuf,ext,"fetox");
			return;
		case FGETEXP:
			get_fpustdGEN(dbuf,ext,"fgetexp");
			return;
		case FGETMAN:
			get_fpustdGEN(dbuf,ext,"fgetman");
			return;
		case FINT:
			get_fpustdGEN(dbuf,ext,"fint");
			return;
		case FINTRZ:
			get_fpustdGEN(dbuf,ext,"fintrz");
			return;
		case FLOG10:
			get_fpustdGEN(dbuf,ext,"flog10");
			return;
		case FLOG2:
			get_fpustdGEN(dbuf,ext,"flog2");
			return;
		case FLOGN:
			get_fpustdGEN(dbuf,ext,"flogn");
			return;
		case FLOGNP1:
			get_fpustdGEN(dbuf,ext,"flognp1");
			return;
		case FMOD:
			get_fpustdGEN(dbuf,ext,"fmod");
			return;
		case FMUL:
			get_fpustdGEN(dbuf,ext,"fmul");
			return;
		case FNEG:
			get_fpustdGEN(dbuf,ext,"fneg");
			return;
		case FREM:
			get_fpustdGEN(dbuf,ext,"frem");
			return;
		case FSCALE:
			get_fpustdGEN(dbuf,ext,"fscale");
			return;
		case FSGLDIV:
			get_fpustdGEN(dbuf,ext,"fsgldiv");
			return;
		case FSGLMUL:
			get_fpustdGEN(dbuf,ext,"fsglmul");
			return;
		case FSIN:
			get_fpustdGEN(dbuf,ext,"fsin");
			return;
		case FSINH:
			get_fpustdGEN(dbuf,ext,"fsinh");
			return;
		case FSQRT:
			get_fpustdGEN(dbuf,ext,"fsqrt");
			return;
		case FSUB:
			get_fpustdGEN(dbuf,ext,"fsub");
			return;
		case FTAN:
			get_fpustdGEN(dbuf,ext,"ftan");
			return;
		case FTANH:
			get_fpustdGEN(dbuf,ext,"ftanh");
			return;
		case FTENTOX:
			get_fpustdGEN(dbuf,ext,"ftentox");
			return;
		case FTST:
			get_fpustdGEN(dbuf,ext,"ftst");
			return;
		case FTWOTOX:
			get_fpustdGEN(dbuf,ext,"ftwotox");
			return;
			
		}
	/* cpBcc */
	case 2:
		if (BITFIELD(opc,5,0) == 0 && *(dbuf->val + 1) == 0) {
			dbuf->used++;
			addstr (dbuf, "fnop");
			return;
		}			
	case 3:
		addstr(dbuf, "fb");
		print_fcond(dbuf, BITFIELD(opc,5,0));
		if (type == 2) {
			addchar('w');
			addchar('\t');
			print_disp(dbuf,*(dbuf->val + 1), SIZE_WORD, -1);
			dbuf->used++;
		} else {
			addchar('l');
			addchar('\t');
			print_disp(dbuf,*(long *)(dbuf->val + 1), SIZE_LONG,
				 -1);
			dbuf->used += 2;
		}
		return;
	/* cpDBcc/cpScc/cpTrap */
	case 1:
		ext = *(dbuf->val + 1);
		dbuf->used++;

		if (BITFIELD(opc,5,3) == 0x1) {
			/* fdbcc */
			addstr(dbuf,"fdb");
			print_fcond(dbuf,BITFIELD(ext,5,0));
			addchar('\t');
			PRINT_DREG(dbuf, BITFIELD(opc,2,0));
			addchar(',');
			print_disp(dbuf, *(dbuf->val + 2), SIZE_WORD, -1);
			dbuf->used++;
		} else if (BITFIELD(opc,5,3) == 0x7 &&
		    BITFIELD(opc,2,0) > 1) {
			addstr(dbuf,"ftrap");
			print_fcond(dbuf,BITFIELD(ext,5,0));

			if (BITFIELD(opc,2,0) == 0x2) {
				addchar('w');
				addchar('\t');
				dbuf->val++;
				get_immed(dbuf, SIZE_WORD);
				dbuf->val--;
			} else if (BITFIELD(opc,2,0) == 0x3) {
				addchar('l');
				addchar('\t');
				dbuf->val++;
				get_immed(dbuf, SIZE_LONG);
				dbuf->val--;
			}
		} else {
			addstr(dbuf,"fs");
			print_fcond(dbuf,BITFIELD(ext,5,0));
			addchar('\t');
			get_modregstr(dbuf, 5, GETMOD_BEFORE, SIZE_BYTE, 1);
		}
		return;
	case 4:
		addstr(dbuf,"fsave\t");
		get_modregstr(dbuf, 5, GETMOD_BEFORE, 0, 0);
		return;
	case 5:
		addstr(dbuf,"frestor\t");
		get_modregstr(dbuf, 5, GETMOD_BEFORE, 0, 0);
		return;
	}
}

/*
 * XXX - This screws up on:  fmovem  a0@(312),fpcr/fpsr/fpi
 */
void
opcode_fmove_ext(dbuf, opc, ext)
	dis_buffer_t *dbuf;
	u_short opc, ext;
{
	int sz;

	sz = 0;
	if (BITFIELD(ext,15,13) == 3) {
		/* fmove r ==> m */
		addstr(dbuf, "fmov");
		switch (BITFIELD(ext,12,10)) {
		case 0:
			addchar('l');
			sz = SIZE_LONG;
			break;
		case 1:
			addchar('s');
			sz = SIZE_SINGLE;
			break;
		case 2:
			addchar('x');
			sz = SIZE_EXTENDED;
			break;
		case 7:
		case 3:
			addchar('p');
			sz = SIZE_PACKED;
			break;
		case 4:
			addchar('w');
			sz = SIZE_WORD;
			break;
		case 5:
			addchar('d');
			sz = SIZE_DOUBLE;
			break;
		case 6:
			addchar('b');
			sz = SIZE_BYTE;
			break;
		}
		addchar('\t');
		PRINT_FPREG(dbuf, BITFIELD(ext,9,7));
		addchar(',');
		get_modregstr(dbuf, 5, GETMOD_BEFORE, sz, 1);
		if (sz == SIZE_PACKED) {
			addchar('{');
			if (ISBITSET(ext,12)) {
				PRINT_DREG(dbuf,BITFIELD(ext,6,4));
			} else {
				addchar('#');
				prints_bf(dbuf, ext, 6, 4);
			}
			addchar('}');
		}
		return;
	}
	addstr(dbuf,"fmovm");

	if (!ISBITSET(ext,14)) {
		/* fmove[m] control reg */
		addchar('l');
		addchar('\t');

		if (ISBITSET(ext,13)) {
			print_freglist(dbuf, AR_DEC, BITFIELD(ext,12,10), 1);
			addchar(',');
		}
		get_modregstr(dbuf, 5, GETMOD_BEFORE, SIZE_LONG, 1);
		if (!ISBITSET(ext,13)) {
			addchar(',');
			print_freglist(dbuf, AR_DEC, BITFIELD(ext,12,10), 1);
		}
		return;
	}
	addchar('x');
	addchar('\t');

	if (ISBITSET(ext,11)) {
		if (ISBITSET(ext,13)) {
			PRINT_DREG(dbuf,BITFIELD(ext,6,4));
			addchar(',');
		}
		get_modregstr(dbuf, 5, GETMOD_BEFORE, SIZE_EXTENDED, 1);
		if (!ISBITSET(ext,13)) {
			addchar(',');
			PRINT_DREG(dbuf,BITFIELD(ext,6,4));
		}		
	} else {
		if (ISBITSET(ext,13)) {
			print_freglist(dbuf, BITFIELD(opc,5,3),
				       BITFIELD(ext,7,0), 0);
			addchar(',');
		}
		get_modregstr(dbuf, 5, GETMOD_BEFORE, SIZE_EXTENDED, 1);
		if (!ISBITSET(ext,13)) {
			addchar(',');
			print_freglist(dbuf, BITFIELD(opc,5,3),
				       BITFIELD(ext,7,0), 0);
		}		
	}
}

void
opcode_mmu(dbuf, opc)
	dis_buffer_t *dbuf;
	u_short opc;
{
	u_short ext;
	int type;

	type = BITFIELD(opc,8,6);
	switch (type) {
	/* cpGEN? */
	case 0:
		ext = *(dbuf->val + 1);
		dbuf->used++;
		
		switch(BITFIELD(ext,15,13)) {
		case 5:
		case 1:
			opcode_pflush(dbuf, opc, ext);
			return;
		case 0:
		case 3:
		case 2:
			opcode_pmove(dbuf, opc, ext);
			return;
		case 4:
			addstr(dbuf, "ptest");
			if (ISBITSET(ext,9))
				addchar('r');
			else
				addchar('w');
			addchar('\t');
			print_fcode(dbuf, BITFIELD(ext, 5, 0));
			addchar(',');
			get_modregstr(dbuf, 5, GETMOD_BEFORE, 0, 1);
			addchar(',');
			addchar('#');
			printu_bf(dbuf, ext, 12, 10);
			if (ISBITSET(ext, 8)) {
				addchar(',');
				PRINT_AREG(dbuf, BITFIELD(ext, 7, 5));
			}
		}
		return;
	case 2:
	case 3:
		addstr(dbuf, "pb");
		print_mcond(dbuf, BITFIELD(opc,5,0));
		if (type == 2) {
			addchar('w');
			addchar('\t');
			print_disp(dbuf,*(dbuf->val + 1), SIZE_WORD, -1);
			dbuf->used++;
		} else {
			addchar('l');
			addchar('\t');
			print_disp(dbuf,*(long *)(dbuf->val + 1), SIZE_LONG,
				 -1);
			dbuf->used += 2;
		}
		return;
	case 1:
		ext = *(dbuf->val + 1);
		dbuf->used++;

		if (BITFIELD(opc,5,3) == 0x1) {
			/* fdbcc */
			addstr(dbuf,"pdb");
			print_fcond(dbuf,BITFIELD(ext,5,0));
			addchar('\t');
			PRINT_DREG(dbuf, BITFIELD(opc,2,0));
			addchar(',');
			print_disp(dbuf, *(dbuf->val + 2), SIZE_WORD, -1);
			dbuf->used++;
		} else if (BITFIELD(opc,5,3) == 0x7 &&
		    BITFIELD(opc,2,0) > 1) {
			addstr(dbuf,"ptrap");
			print_fcond(dbuf,BITFIELD(ext,5,0));

			if (BITFIELD(opc,2,0) == 0x2) {
				addchar('w');
				addchar('\t');
				dbuf->val++;
				get_immed(dbuf, SIZE_WORD);
				dbuf->val--;
			} else if (BITFIELD(opc,2,0) == 0x3) {
				addchar('l');
				addchar('\t');
				dbuf->val++;
				get_immed(dbuf, SIZE_LONG);
				dbuf->val--;
			}
		} else {
			addstr(dbuf,"ps");
			print_fcond(dbuf,BITFIELD(ext,5,0));
			addchar('\t');
			get_modregstr(dbuf, 5, GETMOD_BEFORE, SIZE_BYTE, 1);
		}
		return;
	case 4:
		addstr(dbuf,"psave\t");
		get_modregstr(dbuf, 5, GETMOD_BEFORE, 0, 0);
		return;
	case 5:
		addstr(dbuf,"prestore\t");
		get_modregstr(dbuf, 5, GETMOD_BEFORE, 0, 0);
		return;
	}
}

void
opcode_pflush(dbuf, opc, ext)
	dis_buffer_t *dbuf;
	u_short opc, ext;
{
	u_short mode, mask, fc;
	
	mode = BITFIELD(ext,12,10);
	mask = BITFIELD(ext,8,5);
	fc = BITFIELD(ext, 5, 0);
	
	if (ext == 0xa000) {
		addstr(dbuf,"pflushr\t");
		get_modregstr(dbuf, 5, GETMOD_BEFORE, SIZE_LONG, 1);
		return;
	}
	
	if (mode == 0) {
		addstr(dbuf,"pload");
		if (ISBITSET(ext,9))
			addchar('r');
		else
			addchar('w');
		addchar(' ');
		print_fcode(dbuf, fc);
	}
	
	addstr(dbuf,"pflush");
	switch (mode) {
	case 1:
		addchar('a');
		*dbuf->casm = 0;
		break;
	case 7:
	case 5:
		addchar('s');
		/*FALLTHROUGH*/
	case 6:
	case 4:
		addchar('\t');
		print_fcode(dbuf, fc);
		addchar(',');
		addchar('#');
		printu(dbuf, mask, SIZE_BYTE);
		if (!ISBITSET(mode,1)) 
			break;
		addchar(',');
		get_modregstr(dbuf, 5, GETMOD_BEFORE, SIZE_LONG, 1);
	}
}

void
opcode_pmove(dbuf, opc, ext)
	dis_buffer_t *dbuf;
	u_short opc, ext;
{
	const char *reg;
	int rtom, sz, preg;

	reg  = "???";
	sz   = 0;
	rtom = ISBITSET(ext, 9);
	preg = BITFIELD(ext, 12, 10);
	
	addstr(dbuf,"pmov");
	if (ISBITSET(ext,8)) {
		addchar('f');
		addchar('d');
	}
	switch (BITFIELD(ext, 15, 13)) {
	case 0: /* tt regs 030o */
		switch (preg) {
		case 2:
			reg = "tt0";
			break;
		case 3:
			reg = "tt1";
			break;
		}
		sz = SIZE_LONG;
		break;
	case 2:
		switch (preg) {
		case 0:
			reg = "tc";
			sz = SIZE_LONG;
			break;
		case 1:
			reg = "drp";
			sz = SIZE_QUAD;
			break;
		case 2:
			reg = "srp";
			sz = SIZE_QUAD;
			break;
		case 3:
			reg = "crp";
			sz = SIZE_QUAD;
			break;
		case 4:
			reg = "cal";
			sz = SIZE_BYTE;
			break;
		case 5:
			reg = "val";
			sz = SIZE_BYTE;
			break;
		case 6:
			reg = "scc";
			sz = SIZE_BYTE;
			break;
		case 7:
			reg = "ac";
			sz = SIZE_WORD;
		}
		break;
	case 3:
		switch (preg) {
		case 0:
			reg = "mmusr";
			break;
		case 1:
			reg = "pcsr";
			break;
		case 4:
			reg = "bad";
			break;
		case 5:
			reg = "bac";
			break;
		}
		sz = SIZE_WORD;
		break;
	}
	switch (sz) {
	case SIZE_BYTE:
		addchar ('b');
		break;
	case SIZE_WORD:
		addchar ('w');
		break;
	case SIZE_LONG:
		addchar ('l');
		break;
	case SIZE_QUAD:
		addchar ('d');
		break;
	}		
	addchar('\t');
	
	if (!rtom) {
		get_modregstr(dbuf, 5, GETMOD_BEFORE, sz, 1);
		addchar(',');
	}
	addstr(dbuf, reg);
	if (BITFIELD(ext, 15, 13) == 3 && preg > 1) 
		printu_bf(dbuf, ext, 4, 2);
	if (rtom) {
		addchar(',');
		get_modregstr(dbuf, 5, GETMOD_BEFORE, sz, 1);
	}
	return;
}

void
print_fcode(dbuf, fc)
	dis_buffer_t *dbuf;
	u_short fc;
{
	if (ISBITSET(fc, 4))
		printu_bf(dbuf, fc, 3, 0);
	else if (ISBITSET(fc, 3))
		PRINT_DREG(dbuf, BITFIELD(fc, 2, 0));
	else if (fc == 1)
		addstr(dbuf, "sfc");
	else
		addstr(dbuf, "dfc");
}
void
opcode_mmu040(dbuf, opc)
	dis_buffer_t *dbuf;
	u_short opc;
{
	if (ISBITSET(opc, 6)) {
		addstr(dbuf, "ptest");
		if (ISBITSET(opc, 5))
			addchar('r');
		else
			addchar('w');
		addchar('\t');
		PRINT_AREG(dbuf, BITFIELD(opc,2,0));
		addchar('@');
	} else {
		addstr(dbuf, "pflush");
		switch (BITFIELD(opc, 4, 3)) {
		case 3:
			addchar('a');
			break;
		case 2:
			addchar('a');
			addchar('n');
			break;
		case 0:
			addchar('n');
			/*FALLTHROUGH*/
		case 1:
			addchar('\t');
			PRINT_AREG(dbuf, BITFIELD(opc,2,0));
			addchar('@');
			break;
		}
	}
	*dbuf->casm = 0;
}


/*
 * disassemble long format (64b) divs/muls divu/mulu opcode.
 * Note: opcode's dbuf->used already accounted for.
 */
void
opcode_divmul(dbuf, opc)
	dis_buffer_t *dbuf;
	u_short opc;
{
	u_short ext;
	int iq, hr;
	
	ext = *(dbuf->val + 1);
	dbuf->used++;

	iq = BITFIELD(ext,14,12);
	hr = BITFIELD(ext,2,0);
	
	if (IS_INST(DIVSL,opc)) 
		addstr(dbuf, "div");
	else
		addstr(dbuf, "mul");
	if (ISBITSET(ext,11))
		addchar('s');
	else
		addchar('u');
	addchar('l');
	if (IS_INST(DIVSL,opc) && !ISBITSET(ext,10) && iq != hr)
		addchar('l');
	addchar('\t');

	get_modregstr(dbuf,5,GETMOD_BEFORE,SIZE_LONG,1);
	addchar(',');

	if (ISBITSET(ext,10) ||
	    (iq != hr && IS_INST(DIVSL,opc))) {
		/* 64 bit version */
		PRINT_DREG(dbuf, hr);
		if (dbuf->mit) 
			addchar(',');
		else
			addchar(':');
	}
	PRINT_DREG(dbuf, iq);
}

void
print_reglist(dbuf, mod, rl)
	dis_buffer_t *dbuf;
	int mod;
	u_short rl;
{
	const char *const regs[16] = {
		"d0","d1","d2","d3","d4","d5","d6","d7",
		"a0","a1","a2","a3","a4","a5","a6","a7" };
	int bit, list;

	if (mod == AR_DEC) {
		list = rl;
		rl = 0;
		/* I am sure there is some trick... */
		for (bit = 0; bit < 16; bit++)
			if (list & (1 << bit)) 
				rl |= (0x8000 >> bit);
	} 
	for (bit = 0, list = 0; bit < 16; bit++) {
		if (ISBITSET(rl,bit) && bit != 8) {
			if (list == 0) {
				list = 1;
				addstr(dbuf, regs[bit]);
			} else if (list == 1) {
				list++;
				addchar('-');
			}
		} else {
			if (list) {
				if (list > 1)
					addstr(dbuf, regs[bit-1]);
				addchar('/');
				list = 0;
			}
			if (ISBITSET(rl,bit)) {
				addstr(dbuf, regs[bit]);
				list = 1;
			}
		}
	}
	if (list > 1)
		addstr(dbuf, regs[15]);

	if (dbuf->casm[-1] == '/' || dbuf->casm[-1] == '-')
		dbuf->casm--;
	*dbuf->casm = 0;
}

void
print_freglist(dbuf, mod, rl, cntl)
	dis_buffer_t *dbuf;
	int mod, cntl;
	u_short rl;
{
	const char *const * regs;
	int bit, list, upper;

	regs = cntl ? fpcregs : fpregs;
	upper = cntl ? 3 : 8;

	if (!cntl && mod != AR_DEC) {
		list = rl;
		rl = 0;
		/* I am sure there is some trick... */
		for (bit = 0; bit < upper; bit++)
			if (list & (1 << bit)) 
				rl |= (0x80 >> bit);
	} 
	for (bit = 0, list = 0; bit < upper; bit++) {
		if (ISBITSET(rl,bit)) {
			if (list == 0) {
				addstr(dbuf, regs[bit]);
				if (cntl)
					addchar('/');
				else
					list = 1;
			} else if (list == 1) {
				list++;
				addchar('-');
			}
		} else {
			if (list) {
				if (list > 1)
					addstr(dbuf, regs[bit-1]);
				addchar('/');
				list = 0;
			}
		}
	}
	if (list > 1)
		addstr(dbuf, regs[upper-1]);

	if (dbuf->casm[-1] == '/' || dbuf->casm[-1] == '-')
		dbuf->casm--;
	*dbuf->casm = 0;
}

/*
 * disassemble movem opcode.
 */
void
opcode_movem(dbuf, opc)
	dis_buffer_t *dbuf;
	u_short opc;
{
	u_short rl;
	
	rl = *(dbuf->val + 1);
	dbuf->used++;
	
	if (ISBITSET(opc,6))
		addstr(dbuf, "movml\t");
	else
		addstr(dbuf, "movmw\t");
	if (ISBITSET(opc,10)) {
		get_modregstr(dbuf, 5, GETMOD_BEFORE, 0, 1);
		addchar(',');
		print_reglist(dbuf, BITFIELD(opc,5,3), rl);
	} else {
		print_reglist(dbuf, BITFIELD(opc,5,3), rl);
		addchar(',');
		get_modregstr(dbuf, 5, GETMOD_BEFORE, 0, 1);
	}
}

/*
 * disassemble movec opcode.
 */
void
opcode_movec(dbuf, opc)
	dis_buffer_t *dbuf;
	u_short opc;
{
	char *tmp;
	u_short ext;

	ext = *(dbuf->val + 1);
	dbuf->used++;

	addstr(dbuf, "movc\t");
	if (ISBITSET(opc,0)) {
		dbuf->val++;
		if (ISBITSET(ext,15)) 
			get_modregstr(dbuf,14,AR_DIR,0,0);
		else
			get_modregstr(dbuf,14,DR_DIR,0,0);
		dbuf->val--;
		addchar(',');
	}
	switch (BITFIELD(ext,11,0)) {
		/* 010/020/030/040/CPU32/060 */
	case 0x000:
		tmp = "sfc";
		break;
	case 0x001:
		tmp = "dfc";
		break;
	case 0x800:
		tmp = "usp";
		break;
	case 0x801:
		tmp = "vbr";
		break;
		/* 020/030 */
	case 0x802:
		tmp = "caar";
		break;
		/* 020/030/040/060 */
	case 0x002:
		tmp = "cacr";
		break;
		/* 020/030/040 */
	case 0x803:
		tmp = "msp";
		break;
	case 0x804:
		tmp = "isp";
		break;
		/* 040/060 */
	case 0x003:
		tmp = "tc";
		break;
	case 0x004:
		tmp = "itt0";
		break;
	case 0x005:
		tmp = "itt1";
		break;
	case 0x006:
		tmp = "dtt0";
		break;
	case 0x007:
		tmp = "dtt1";
		break;
		/* 040 */
	case 0x805:
		tmp = "mmusr";
		break;
		/* 040/060 */
	case 0x806:
		tmp = "urp";
		break;
	case 0x807:
		tmp = "srp";
		break;
		/* 060 */
	case 0x008:
		tmp = "buscr";
		break;
	case 0x808:
		tmp = "pcr";
		break;
	default:
		tmp = "INVALID";
		break;
	}
	addstr(dbuf, tmp);
	if (!ISBITSET(opc,0)) {
		dbuf->val++;
		addchar(',');
		if (ISBITSET(ext,15)) 
			get_modregstr(dbuf,14,AR_DIR,0,0);
		else
			get_modregstr(dbuf,14,DR_DIR,0,0);
		dbuf->val--;
	}
}

void
opcode_move16(dbuf, opc)
	dis_buffer_t *dbuf;
	u_short opc;
{
	u_short ext;

	addstr(dbuf, "move16\t");

	if (ISBITSET(opc, 5)) {
		PRINT_AREG(dbuf, BITFIELD(opc,2,0));
		addstr(dbuf, "@+,");
		ext = *(dbuf->val + 1);
		PRINT_AREG(dbuf, BITFIELD(ext,14,12));
		addstr(dbuf, "@+");
		dbuf->used++;
	} else {
		switch (BITFIELD(opc,4,3)) {
		case 0:
			PRINT_AREG(dbuf, BITFIELD(opc,2,0));
			addstr(dbuf, "@+,");
			get_immed(dbuf, SIZE_LONG);
			break;
		case 1:
			get_immed(dbuf, SIZE_LONG);
			addchar(',');
			PRINT_AREG(dbuf, BITFIELD(opc,2,0));
			addstr(dbuf, "@+");
			break;
		case 2:
			PRINT_AREG(dbuf, BITFIELD(opc,2,0));
			addstr(dbuf, "@,");
			get_immed(dbuf, SIZE_LONG);
			break;
		case 3:
			get_immed(dbuf, SIZE_LONG);
			addchar(',');
			PRINT_AREG(dbuf, BITFIELD(opc,2,0));
			addchar('@');
			break;
		}
	}
}

/*
 * copy const string 's' into ``dbuf''->casm
 */
void
addstr(dbuf, s)
	dis_buffer_t *dbuf;
	const char *s;
{
	while ((*dbuf->casm++ = *s++))
		;
	dbuf->casm--;
}

/*
 * copy const string 's' into ``dbuf''->cinfo
 */
void
iaddstr(dbuf, s)
	dis_buffer_t *dbuf;
	const char *s;
{
	while ((*dbuf->cinfo++ = *s++))
		;
	dbuf->cinfo--;
}

void
get_modregstr_moto(dbuf, bit, mod, sz, dd)
	dis_buffer_t *dbuf;
	int bit, mod, sz, dd;
{
	u_char scale, idx;
	const short *nval;
	u_short ext;
	int disp, odisp, bd, od, reg;
	
	odisp = 0;

	/* check to see if we have been given the mod */
	if (mod != GETMOD_BEFORE && mod != GETMOD_AFTER)
		reg = BITFIELD(*dbuf->val, bit, bit-2);
	else if (mod == GETMOD_BEFORE) {
		mod = BITFIELD(*dbuf->val, bit, bit-2);
		reg = BITFIELD(*dbuf->val, bit-3, bit-5);
	} else {
		reg = BITFIELD(*dbuf->val, bit, bit-2);
		mod = BITFIELD(*dbuf->val, bit-3, bit-5);
	}
	switch (mod) {
	case DR_DIR:
	case AR_DIR:
		if (mod == DR_DIR)
			PRINT_DREG(dbuf, reg);
		else
			PRINT_AREG(dbuf, reg);
		break;
	case AR_DIS:
		print_disp(dbuf, *(dbuf->val + 1 + dd), SIZE_WORD,reg);
		dbuf->used++;
		/*FALLTHROUGH*/
	case AR_IND:
	case AR_INC:
	case AR_DEC:
		if (mod == AR_DEC)
			addchar('-');
		addchar('(');
		PRINT_AREG(dbuf, reg);
		addchar(')');
		if (mod == AR_INC)
			addchar('+');
		break;
	/* mod 6 & 7 are the biggies. */
	case MOD_SPECIAL:
		if (reg == 0) {
			/* abs short addr */
			print_addr(dbuf, *(dbuf->val + 1 + dd));
			dbuf->used++;
			addchar('.');
			addchar('w');
			break;
		} else if (reg == 1) {
			/* abs long addr */
			print_addr(dbuf, *(u_long *)(dbuf->val + 1 + dd));
			dbuf->used += 2;
			addchar('.');
			addchar('l');
			break;
		} else if (reg == 2) {
			/* pc ind displ. xxx(PC) */
			dbuf->used++;
			print_disp(dbuf, *(dbuf->val + 1 + dd), SIZE_WORD,
				   -1);
			addstr(dbuf,"(pc)");
			break;
		} else if (reg == 4) {
			/* uses ``sz'' to figure immediate data. */
			if (sz == SIZE_BYTE) {
				addchar('#');
				prints(dbuf,
				    *((char *)dbuf->val + 3+ (dd * 2)), sz);
				dbuf->used++;
			} else if (sz == SIZE_WORD) {
				addchar('#');
				prints(dbuf, *(dbuf->val + 1 + dd), sz);
				dbuf->used++;
			} else if (sz == SIZE_LONG) {
				addchar('#');
				prints(dbuf, *(long *)(dbuf->val + 1 + dd),
				    sz);
				dbuf->used += 2;
			} else if (sz == SIZE_QUAD) {
				dbuf->used += 4;
				addstr(dbuf,"#<quad>");
			} else if (sz == SIZE_SINGLE) {
				dbuf->used += 2;
				addstr(dbuf,"#<single>");
			} else if (sz == SIZE_DOUBLE) {
				dbuf->used += 4;
				addstr(dbuf,"#<double>");
			} else if (sz == SIZE_PACKED) {
				dbuf->used += 6;
				addstr(dbuf,"#<packed>");
			} else if (sz == SIZE_EXTENDED) {
				dbuf->used += 6;
				addstr(dbuf,"#<extended>");
			}
			break;
		}
		/* standrd PC stuff. */
		/*FALLTHROUGH*/
	case AR_IDX: 
		ext = *(dbuf->val + 1 + dd);
		dbuf->used++;
		nval = dbuf->val + 2 + dd; /* set to possible displacements */
		scale = BITFIELD(ext,10,9);
		idx = BITFIELD(ext,14,12);
		
		if (ISBITSET(ext,8)) {
			/* either base disp, or memory indirect */
			bd = BITFIELD(ext,5,4);
			od = BITFIELD(ext,1,0);
			if (bd == 1)
				disp = 0;
			else if (bd == 2) {
				dbuf->used++;
				disp = *nval++;
			} else {
				dbuf->used += 2;
				disp = *(long *)nval;
				nval += 2;
			}

			if (od == 1) 
				odisp = 0;
			else if (od == 2) {
				dbuf->used++;
				odisp = *nval++;
			} else if (od == 3) {
				dbuf->used += 2;
				odisp = *(long *)nval;
				nval += 2;
			}
		} else {
			/*
			 * We set od and bd to zero, these values are
			 * not allowed in opcodes that use base and
			 * outer displacement, e.g. we can tell if we
			 * are using on of those modes by checking
			 * `bd' and `od'.
			 */
			od = 0;	
			bd = 0;
			disp = (char)BITFIELD(ext,7,0);
		}
		/*
		 * write everything into buf
		 */
		addchar('(');
		if (od)
			addchar('['); /* begin memory indirect xxx-indexed */
		prints(dbuf, disp,
		    bd == 2 ? SIZE_WORD :
		    bd == 3 ? SIZE_LONG :
		    SIZE_BYTE);
		addchar(',');
		if (bd && ISBITSET(ext,7)) {
			addchar('z');
			if (mod != MOD_SPECIAL) 
				PRINT_AREG(dbuf,reg);
			else {
				addchar('p');
				addchar('c');
			} 
		} else if (mod == AR_IDX) 
			PRINT_AREG(dbuf, reg);
		else {
			addchar('p');
			addchar('c');
		}
		
		if (od && ISBITSET(ext,2)) 
			addchar(']'); /* post-indexed. */
		addchar(',');
		if (bd && ISBITSET(ext,6)) 
			addchar('0');
		else {
			if (0x8000 & ext)
				PRINT_AREG(dbuf, idx);
			else
				PRINT_DREG(dbuf, idx);
			addchar('.');
			addchar(0x800 & ext ? 'l' : 'w');
			if (scale) {
				addchar('*');
				addchar('0' + (1 << scale));
			}
		}
		if (od) {
			if (!ISBITSET(ext,2)) 
				addchar(']'); /* pre-indexed */
			addchar(',');
			prints(dbuf, odisp,
			    od == 2 ? SIZE_WORD :
			    od == 3 ? SIZE_LONG :
			    SIZE_BYTE);
		}
		addchar(')');
		break;
	}
	*dbuf->casm = 0;
}			
	
/* mit syntax makes for spaghetti parses */
void
get_modregstr_mit(dbuf, bit, mod, sz, dd)
	dis_buffer_t *dbuf;
	int bit, mod, sz, dd;
{
	u_char scale, idx;
	const short *nval;
	u_short ext;
	int disp, odisp, bd, od, reg;
	
	disp = odisp = 0;
	/* check to see if we have been given the mod */
	if (mod != GETMOD_BEFORE && mod != GETMOD_AFTER)
		reg = BITFIELD(*dbuf->val, bit, bit-2);
	else if (mod == GETMOD_BEFORE) {
		mod = BITFIELD(*dbuf->val, bit, bit-2);
		reg = BITFIELD(*dbuf->val, bit-3, bit-5);
	} else {
		reg = BITFIELD(*dbuf->val, bit, bit-2);
		mod = BITFIELD(*dbuf->val, bit-3, bit-5);
	}
	switch (mod) {
	case DR_DIR:
	case AR_DIR:
		if (mod == DR_DIR)
			PRINT_DREG(dbuf, reg);
		else
			PRINT_AREG(dbuf, reg);
		break;
	case AR_DIS:
		dbuf->used++;	/* tell caller we used an ext word. */
		disp = *(dbuf->val + 1 + dd);
		/*FALLTHROUGH*/
	case AR_IND:
	case AR_INC:
	case AR_DEC:
		PRINT_AREG(dbuf, reg);
		addchar('@' );
		if (mod == AR_DEC)
			addchar('-');
		else if (mod == AR_INC)
			addchar('+');
		else if (mod == AR_DIS) {
			addchar('(');
			print_disp(dbuf, disp, SIZE_WORD, reg);
			addchar(')');
		}
		break;
	/* mod 6 & 7 are the biggies. */
	case MOD_SPECIAL:
		if (reg == 0) {
			/* abs short addr */
			print_addr(dbuf, *(dbuf->val + 1 + dd));
			dbuf->used++;
			break;
		} else if (reg == 1) {
			/* abs long addr */
			print_addr(dbuf, *(u_long *)(dbuf->val + 1 + dd));
			dbuf->used += 2;
			break;
		} else if (reg == 2) {
			/* pc ind displ. pc@(xxx) */
			addstr(dbuf,"pc@(");
			print_disp(dbuf, *(dbuf->val + 1 + dd), SIZE_WORD, -1);
			dbuf->used++;
			addchar(')');
			break;
		} else if (reg == 4) {
			/* uses ``sz'' to figure immediate data. */
			if (sz == SIZE_BYTE) {
				addchar('#');
				prints(dbuf,
				    *((char *)dbuf->val + 3 + (dd * 2)), sz);
				dbuf->used++;
			} else if (sz == SIZE_WORD) {
				addchar('#');
				prints(dbuf, *(dbuf->val + 1 + dd), sz);
				dbuf->used++;
			} else if (sz == SIZE_LONG) {
				addchar('#');
				prints(dbuf, *(long *)(dbuf->val + 1 + dd),
				    sz);
				dbuf->used += 2;
			} else if (sz == SIZE_QUAD) {
				dbuf->used += 4;
				addstr(dbuf,"#<quad>");
			} else if (sz == SIZE_SINGLE) {
				dbuf->used += 2;
				addstr(dbuf,"#<single>");
			} else if (sz == SIZE_DOUBLE) {
				dbuf->used += 4;
				addstr(dbuf,"#<double>");
			} else if (sz == SIZE_PACKED) {
				dbuf->used += 6;
				addstr(dbuf,"#<packed>");
			} else if (sz == SIZE_EXTENDED) {
				dbuf->used += 6;
				addstr(dbuf,"#<extended>");
			}
			break;
		}
		/* standrd PC stuff. */
		/*FALLTHROUGH*/
	case AR_IDX: 
		dbuf->used++;	/* indicate use of ext word. */
		ext = *(dbuf->val + 1 + dd);
		nval = dbuf->val + 2 + dd; /* set to possible displacements */
		scale = BITFIELD(ext,10,9);
		idx = BITFIELD(ext,14,12);
		
		if (ISBITSET(ext,8)) {
			/* either base disp, or memory indirect */
			bd = BITFIELD(ext,5,4);
			od = BITFIELD(ext,1,0);
			if (bd == 1)
				disp = 0;
			else if (bd == 2) {
				dbuf->used++;
				disp = *nval++;
			} else {
				dbuf->used += 2;
				disp = *(long *)nval;
				nval += 2;
			}

			if (od == 1) 
				odisp = 0;
			else if (od == 2) {
				dbuf->used++;
				odisp = *nval++;
			} else if (od == 3) {
				dbuf->used += 2;
				odisp = *(long *)nval;
				nval += 2;
			}
		} else {
			/*
			 * We set od and bd to zero, these values are
			 * not allowed in opcodes that use base and
			 * outer displacement, e.g. we can tell if we
			 * are using on of those modes by checking
			 * `bd' and `od'.
			 */
			od = 0;	
			bd = 0;
			disp = (char)BITFIELD(ext,7,0);
		}
		/*
		 * write everything into buf
		 */
		/* if base register not suppresed */
		if (mod == AR_IDX && (!bd || !ISBITSET(ext,7)))
			PRINT_AREG(dbuf, reg);
		else if (mod == MOD_SPECIAL && ISBITSET(ext,7)) {
			addchar('z');
			addchar('p');
			addchar('c');
		} else if (mod == MOD_SPECIAL) {
			addchar('p');
			addchar('c');
		}
		addchar('@');
		addchar('(');
		
		if (bd && bd != 1) {
			prints(dbuf, disp,
			    bd == 2 ? SIZE_WORD :
			    bd == 3 ? SIZE_LONG :
			    SIZE_BYTE);
			if (od && !ISBITSET(ext,6) && !ISBITSET(ext,2)) 
				/* Pre-indexed and not suppressing index */
				addchar(',');
			else if (od && ISBITSET(ext,2)) {
				/* Post-indexed */
				addchar(')');
				addchar('@');
				addchar('(');
			} else if (!od)
				addchar(',');
		} else if (!bd) {
		       	/* don't forget simple 8 bit displacement. */
			prints(dbuf, disp,
			    bd == 2 ? SIZE_WORD :
			    bd == 3 ? SIZE_LONG :
			    SIZE_BYTE);
			addchar(',');
		}
		
		/* Post-indexed? */
		if (od && ISBITSET(ext,2)) {
			/* have displacement? */
			if (od != 1) {
				prints(dbuf, odisp,
				    od == 2 ? SIZE_WORD :
				    od == 3 ? SIZE_LONG :
				    SIZE_BYTE);
				addchar(',');
			}
		} 
			
		if (!bd || !ISBITSET(ext,6)) {
			if (ISBITSET(ext,15))
				PRINT_AREG(dbuf,idx);
			else
				PRINT_DREG(dbuf,idx);
			addchar(':');
			addchar(ISBITSET(ext,11) ? 'l' : 'w');
			if (scale) {
				addchar(':');
				addchar('0' + (1 << scale));
			}
		}
		/* pre-indexed? */
		if (od && !ISBITSET(ext,2)) {
			if (od != 1) {
				addchar(')');
				addchar('@');
				addchar('(');
				prints(dbuf, odisp,
				    od == 2 ? SIZE_WORD :
				    od == 3 ? SIZE_LONG :
				    SIZE_BYTE);
			}
		}
		addchar(')');
		break;
	}
	*dbuf->casm = 0;
}		

/*
 * Given a disassembly buffer ``dbuf'' and a starting bit of the
 * mod|reg field ``bit'' (or just a reg field if ``mod'' is not
 * GETMOD_BEFORE or GETMOD_AFTER), disassemble and write into ``dbuf''
 * the mod|reg pair.
 */
void get_modregstr(dbuf, bit, mod, sz, dispdisp)
	dis_buffer_t *dbuf;
	int bit, mod, sz, dispdisp;
{
	if (dbuf->mit) 
		get_modregstr_mit(dbuf,bit,mod,sz,dispdisp);
	else 
		get_modregstr_moto(dbuf,bit,mod,sz,dispdisp);
}

/*
 * given a bit position ``bit'' in the current ``dbuf''->val
 * and the ``base'' string of the opcode, append the full
 * opcode name including condition found at ``bit''.
 */
void
make_cond(dbuf, bit, base)
	dis_buffer_t *dbuf;
	int bit;
	char *base;
{
	int cc;
	const char *ccs;

	cc = BITFIELD(*dbuf->val,bit,bit-3);
	ccs = cc_table[cc&15];

	addstr(dbuf, base);
	addstr(dbuf, ccs);
}

void
print_fcond(dbuf, cp)
	dis_buffer_t *dbuf;
	char cp;
{
	addstr(dbuf,fpcc_table[cp&31]); 	/* XXX - not 63 ?*/
}

void
print_mcond(dbuf, cp)
	dis_buffer_t *dbuf;
	char cp;
{
	addstr(dbuf,mmcc_table[cp&15]);
}

/*
 * given dis_buffer_t ``dbuf'' get the immediate value from the
 * extension words following current instruction, output a
 * hash (``#'') sign and the value.  Increment the ``dbuf''->used
 * field accordingly.
 */
void
get_immed(dbuf,sz)
	dis_buffer_t *dbuf;
	int sz;
{
	addchar('#');
	switch (sz) {
	case SIZE_BYTE:
		prints(dbuf, BITFIELD(*(dbuf->val + 1),7,0), SIZE_BYTE);
		dbuf->used++;
		break;
	case SIZE_WORD:
		prints(dbuf, *(dbuf->val + 1), SIZE_WORD);
		dbuf->used++;
		break;
	case SIZE_LONG:
		prints(dbuf, *(long *)(dbuf->val + 1), SIZE_LONG);
		dbuf->used += 2;
		break;
	}
	return;
}

void
get_fpustdGEN(dbuf,ext,name)
	dis_buffer_t *dbuf;
	u_short ext;
	const char *name;
{
	int sz;
	
	/*
	 * If bit seven is set, its a 040 s/d opcode, then if bit 2 is
	 * set its "d".  This is not documented, however thats the way
	 * it is.
	 */

	sz = 0;
	addchar(*name++);
	if (ISBITSET(ext,7)) {
		if(ISBITSET(ext,2))
			addchar('d');
		else
			addchar('s');
	}
	addstr(dbuf,name);

	if (ISBITSET(ext,14)) {
		switch (BITFIELD(ext,12,10)) {
		case 0:
			addchar('l');
			sz = SIZE_LONG;
			break;
		case 1:
			addchar('s');
			sz = SIZE_SINGLE;
			break;
		case 2:
			addchar('x');
			sz = SIZE_EXTENDED;
			break;
		case 3:
			addchar('p');
			sz = SIZE_PACKED;
			break;
		case 4:
			addchar('w');
			sz = SIZE_WORD;
			break;
		case 5:
			addchar('d');
			sz = SIZE_DOUBLE;
			break;
		case 6:
			addchar('b');
			sz = SIZE_BYTE;
			break;
		}
		addchar('\t');
		get_modregstr(dbuf, 5, GETMOD_BEFORE, sz, 1);
		if (BITFIELD(ext,6,3) == 6) {
			addchar(',');
			PRINT_FPREG(dbuf, BITFIELD(ext,2,0));
			addchar(':');
			PRINT_FPREG(dbuf, BITFIELD(ext,9,7));
		} else if (BITFIELD(ext,5,0) != FTST) {
			addchar(',');
			PRINT_FPREG(dbuf, BITFIELD(ext,9,7));
		}
	} else {
		addchar('x');
		addchar('\t');
		PRINT_FPREG(dbuf, BITFIELD(ext,12,10));
		if (BITFIELD(ext,6,3) == 6) {
			addchar(',');
			PRINT_FPREG(dbuf, BITFIELD(ext,2,0));
			addchar(':');
			PRINT_FPREG(dbuf, BITFIELD(ext,9,7));
		} else if (BITFIELD(ext,5,0) != FTST) {
			addchar(',');
			PRINT_FPREG(dbuf, BITFIELD(ext,9,7));
		}
	}
}

u_long
get_areg_val(reg)
	int reg;
{
	return (0);
}

/*
 * given value ``disp'' print it to ``dbuf''->buf. ``rel'' is a
 * register number 0-7 (a0-a7), or -1 (pc). Thus possible extra info
 * could be output to the ``dbuf''->info buffer.
 */
void
print_disp(dbuf, disp, sz, rel)
	dis_buffer_t *dbuf;
	int disp, sz, rel;
{
	db_expr_t diff;
	db_sym_t sym;
	char *symname;
	u_long nv;
		
	prints(dbuf, disp, sz);

	if (rel == -1) 
		/* XXX This may be wrong for a couple inst. */
		nv = disp + (u_int)dbuf->val + 2;
	else
		return; /* nv = get_areg_val(rel); */
		
	diff = INT_MAX;
	symname = NULL;	
	sym = db_search_symbol(nv, DB_STGY_PROC, &diff);
	db_symbol_values(sym, &symname, 0);

	if (symname) {
		iaddstr(dbuf, "disp:");
		iaddstr(dbuf, symname);
		iaddchar('+');
		iprintu(dbuf, diff, SIZE_LONG);
		iaddchar(' ');
		*dbuf->cinfo = 0;
	}
}

void
print_addr(dbuf, addr)
	dis_buffer_t *dbuf;
	u_long addr;
{
	db_expr_t diff;
	db_sym_t sym;
	char *symname;
				
	diff = INT_MAX;
	symname = NULL;
	sym = db_search_symbol(addr, DB_STGY_ANY, &diff);
	db_symbol_values(sym, &symname, 0);

	if (symname) {
		if (diff == 0)
			addstr(dbuf,symname);
		else {
			addchar('<');
			addstr(dbuf,symname);
			addchar('+');
			printu(dbuf, diff, SIZE_LONG);
			addchar('>');
			*dbuf->casm = 0;
		}
		iaddstr(dbuf,"addr:");
		iprintu(dbuf, addr, SIZE_LONG);
		iaddchar(' ');
		*dbuf->cinfo = 0;
	} else {
		printu(dbuf, addr, SIZE_LONG);
	}
}

void
prints(dbuf, val, sz)
	dis_buffer_t *dbuf;
	int val;
	int sz;
{
	extern int db_radix;

	if (val == 0) {
		dbuf->casm[0] = '0';
		dbuf->casm[1] = 0;
	} else if (sz == SIZE_BYTE) 
		prints_wb(dbuf, (char)val, sz, db_radix);
	else if (sz == SIZE_WORD) 
		prints_wb(dbuf, (short)val, sz, db_radix);
	else 
		prints_wb(dbuf, (long)val, sz, db_radix);
	
	dbuf->casm = &dbuf->casm[strlen(dbuf->casm)];
}

void
iprints(dbuf, val, sz)
	dis_buffer_t *dbuf;
	int val;
	int sz;
{
	extern int db_radix;

	if (val == 0) {
		dbuf->cinfo[0] = '0';
		dbuf->cinfo[1] = 0;
	} else if (sz == SIZE_BYTE) 
		iprints_wb(dbuf, (char)val, sz, db_radix);
	else if (sz == SIZE_WORD) 
		iprints_wb(dbuf, (short)val, sz, db_radix);
	else 
		iprints_wb(dbuf, (long)val, sz, db_radix);
	
	dbuf->cinfo = &dbuf->cinfo[strlen(dbuf->cinfo)];
}

void
printu(dbuf, val, sz)
	dis_buffer_t *dbuf;
	u_int val;
	int sz;
{
	extern int db_radix;

	if (val == 0) {
		dbuf->casm[0] = '0';
		dbuf->casm[1] = 0;
	} else if (sz == SIZE_BYTE) 
		printu_wb(dbuf, (u_char)val, sz, db_radix);
	else if (sz == SIZE_WORD) 
		printu_wb(dbuf, (u_short)val, sz, db_radix);
	else 
		printu_wb(dbuf, (u_long)val, sz, db_radix);
	dbuf->casm = &dbuf->casm[strlen(dbuf->casm)];
}

void
iprintu(dbuf, val, sz)
	dis_buffer_t *dbuf;
	u_int val;
	int sz;
{
	extern int db_radix;

	if (val == 0) {
		dbuf->cinfo[0] = '0';
		dbuf->cinfo[1] = 0;
	} else if (sz == SIZE_BYTE) 
		iprintu_wb(dbuf, (u_char)val, sz, db_radix);
	else if (sz == SIZE_WORD) 
		iprintu_wb(dbuf, (u_short)val, sz, db_radix);
	else 
		iprintu_wb(dbuf, (u_long)val, sz, db_radix);
	dbuf->cinfo = &dbuf->cinfo[strlen(dbuf->cinfo)];
}

void
printu_wb(dbuf, val, sz, base)
	dis_buffer_t *dbuf;
	u_int val;
	int sz, base;
{
	static char buf[sizeof(long) * NBBY / 3 + 2];
	char *p, ch;

	if (base != 10) {
		addchar('0');
		if (base != 8) {
			base = 16;
			addchar('x');
		}
	}

	p = buf;
	do {
		*++p = "0123456789abcdef"[val % base];
	} while (val /= base);

	while ((ch = *p--))
		addchar(ch);
	
	*dbuf->casm = 0;
}

void
prints_wb(dbuf, val, sz, base)
	dis_buffer_t *dbuf;
	int val;
	int sz, base;
{
	if (val < 0) {
		addchar('-');
		val = -val;
	}
	printu_wb(dbuf, val, sz, base);
}

void
iprintu_wb(dbuf, val, sz, base)
	dis_buffer_t *dbuf;
	u_int val;
	int sz, base;
{
	static char buf[sizeof(long) * NBBY / 3 + 2];
	char *p, ch;

	if (base != 10) {
		iaddchar('0');
		if (base != 8) {
			base = 16;
			iaddchar('x');
		}
	}

	p = buf;
	do {
		*++p = "0123456789abcdef"[val % base];
	} while (val /= base);

	while ((ch = *p--))
		iaddchar(ch);
	
	*dbuf->cinfo = 0;
}

void
iprints_wb(dbuf, val, sz, base)
	dis_buffer_t *dbuf;
	int val;
	int sz, base;
{
	if (val < 0) {
		iaddchar('-');
		val = -val;
	}
	iprintu_wb(dbuf, val, sz, base);
}


void
prints_bf(dbuf, val, sb, eb)
	dis_buffer_t *dbuf;
	int val, sb, eb;
{
	if (ISBITSET(val,sb)) 
		val = (~0 & ~BITFIELD(~0, sb, eb)) | BITFIELD(val, sb, eb);
	else
		val = BITFIELD(val,sb,eb);
	
	prints(dbuf, val, SIZE_LONG);
}

void
printu_bf(dbuf, val, sb, eb)
	dis_buffer_t *dbuf;
	u_int val;
	int sb, eb;
{
	printu(dbuf,BITFIELD(val,sb,eb),SIZE_LONG);
}	
