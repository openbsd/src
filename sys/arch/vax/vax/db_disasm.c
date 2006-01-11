/*	$OpenBSD: db_disasm.c,v 1.14 2006/01/11 22:49:52 miod Exp $ */
/*	$NetBSD: db_disasm.c,v 1.10 1998/04/13 12:10:27 ragge Exp $ */
/*
 * Copyright (c) 2002, Miodrag Vallat.
 * Copyright (c) 1996 Ludd, University of Lule}, Sweden.
 * All rights reserved.
 *
 * This code is derived from software contributed to Ludd by
 * Bertram Barth.
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
 *	This product includes software developed at Ludd, University of 
 *	Lule}, Sweden and its contributors.
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


#include <sys/param.h>
#include <sys/proc.h>
#include <sys/reboot.h>
#include <sys/systm.h>

#include <machine/db_machdep.h>
#include <ddb/db_sym.h>
#include <ddb/db_variables.h>
#include <ddb/db_interface.h>
#include <ddb/db_output.h>

#include <vax/vax/db_disasm.h>

#ifdef VMS_MODE
#define DEFERRED   "@"
#define LITERAL	   "#"
#else
#define DEFERRED   "*"
#define LITERAL	   "$"
#endif
/*
 * disassembling vax instructions works as follows:
 *
 * 1.	get first byte as opcode (check for two-byte opcodes!)
 * 2.	lookup in op-table for mnemonic and operand-list
 * 2.a	store the mnemonic
 * 3.	for each operand in list: get the size/type
 * 3.a	evaluate addressing mode for this operand
 * 3.b	store each operand(s)
 * 4.	db_printf the opcode and the (value of the) operands
 * 5.	return the start of the next instruction
 *
 * - if jump/branch calculate (and display) the target-address
 */

/* 
#define BROKEN_DB_REGS
*/
#ifdef	BROKEN_DB_REGS
struct {		/* Due to order and contents of db_regs[], we can't */
	char *name;	/* use this array to extract register-names. */
	void *valuep;	/* eg. "psl" vs "pc", "pc" vs "sp" */
} my_db_regs[16] = {
	{ "r0",		NULL },
	{ "r1",		NULL },
	{ "r2",		NULL },
	{ "r3",		NULL },
	{ "r4",		NULL },
	{ "r5",		NULL },
	{ "r6",		NULL },
	{ "r7",		NULL },
	{ "r8",		NULL },
	{ "r9",		NULL },
	{ "r10",	NULL },
	{ "r11",	NULL },
	{ "ap",		NULL },		/* aka "r12" */
	{ "fp",		NULL },		/* aka "r13" */
	{ "sp",		NULL },		/* aka "r14" */
	{ "pc",		NULL },		/* aka "r15" */
};
#else
#define my_db_regs db_regs
#endif

typedef struct {
	char		dasm[256];	/* disassembled instruction as text */
	char	       *ppc;	/* pseudo PC */
	u_int		opc;	/* op-code */
	char	       *argp;	/* pointer into argument-list */
	int		off;	/* offset specified by last argument */
	int		addr;	/* address specified by last argument */
} inst_buffer;

int get_byte(inst_buffer * ib);
int get_word(inst_buffer * ib);
int get_long(inst_buffer * ib);

int get_opcode(inst_buffer * ib);
int get_operands(inst_buffer * ib);
int get_operand(inst_buffer * ib, int size);

void add_str(inst_buffer * ib, char *s);
void add_int(inst_buffer * ib, int i);
void add_xint(inst_buffer * ib, int i);
void add_sym(inst_buffer * ib, int i);
void add_off(inst_buffer * ib, int i);

#define err_print  printf

/*
 * Disassemble instruction at 'loc'.  'altfmt' specifies an
 * (optional) alternate format (altfmt for vax: don't assume
 * that each external label is a procedure entry mask).
 * Return address of start of next instruction.
 * Since this function is used by 'examine' and by 'step'
 * "next instruction" does NOT mean the next instruction to
 * be executed but the 'linear' next instruction.
 */
db_addr_t
db_disasm(loc, altfmt)
	db_addr_t	loc;
	boolean_t	altfmt;
{
	db_expr_t	diff;
	db_sym_t	sym;
	char	       *symname;

	inst_buffer	ib;

	bzero(&ib, sizeof(ib));
	ib.ppc = (void *) loc;

	if (!altfmt) {		/* ignore potential entry masks in altfmt */
		diff = INT_MAX;
		symname = NULL;
		sym = db_search_symbol(loc, DB_STGY_PROC, &diff);
		db_symbol_values(sym, &symname, 0);

		if (symname && !diff) { /* symbol at loc */
			db_printf("function \"%s()\", entry-mask 0x%x\n\t\t",
				  symname, (unsigned short) get_word(&ib));
			ib.ppc += 2;
		}
	}
	get_opcode(&ib);
	get_operands(&ib);
	db_printf("%s\n", ib.dasm);

	return ((u_int) ib.ppc);
}

int
get_opcode(ib)
	inst_buffer    *ib;
{
	ib->opc = get_byte(ib);
	if (ib->opc >= 0xfd) {
		/* two byte op-code */
		ib->opc = ib->opc << 8;
		ib->opc += get_byte(ib);
	}

	if (ib->opc > 0xffff) {
		add_str(ib, "invalid opcode ");
		add_xint(ib, ib->opc);
	} else {
		if (ib->opc > 0xff)
			add_str(ib, vax_inst2[INDEX_OPCODE(ib->opc)].mnemonic);
		else
			add_str(ib, vax_inst[ib->opc].mnemonic);
		add_str(ib, "\t");
	}
	return (ib->opc);
}

int
get_operands(ib)
	inst_buffer    *ib;
{
	int		aa = 0; /* absolute address mode ? */
	int		size;

	if (ib->opc > 0xffff) {
		/* invalid opcode */
		ib->argp = NULL;
		return (-1);
	} else if (ib->opc > 0xff) {
		/* two-byte opcode */
		ib->argp = vax_inst2[INDEX_OPCODE(ib->opc)].argdesc;
	} else
		ib->argp = vax_inst[ib->opc].argdesc;

	if (ib->argp == NULL)
		return (0);

	while (*ib->argp) {
		switch (*ib->argp) {

		case 'b':	/* branch displacement */
			switch (*(++ib->argp)) {
			case 'b':
				ib->off = (signed char) get_byte(ib);
				break;
			case 'w':
				ib->off = (short) get_word(ib);
				break;
			case 'l':
				ib->off = get_long(ib);
				break;
			default:
				err_print("invalid branch-type %X (%c) found.\n",
					  *ib->argp, *ib->argp);
			}
			/* add_int(ib, ib->off); */
			ib->addr = (u_int) ib->ppc + ib->off;
			add_off(ib, ib->addr);
			break;

		case 'a':	/* absolute addressing mode */
			aa = 1;
			/* FALLTHROUGH */

		default:
			switch (*(++ib->argp)) {
			case 'b':	/* Byte */
				size = SIZE_BYTE;
				break;
			case 'w':	/* Word */
				size = SIZE_WORD;
				break;
			case 'l':	/* Long-Word */
			case 'f':	/* F_Floating */
				size = SIZE_LONG;
				break;
			case 'q':	/* Quad-Word */
			case 'd':	/* D_Floating */
			case 'g':	/* G_Floating */
				size = SIZE_QWORD;
				break;
			case 'o':	/* Octa-Word */
			case 'h':	/* H_Floating */
				size = SIZE_OWORD;
				break;
			default:
				err_print("invalid op-type %X (%c) found.\n",
					  *ib->argp, *ib->argp);
				size = 0;
			}
			if (aa) {
				/* get the address */
				ib->addr = get_operand(ib, size);
				add_sym(ib, ib->addr);
			} else {
				/* get the operand */
				ib->addr = get_operand(ib, size);
				add_off(ib, ib->addr);
			}
		}

		if (!*ib->argp || !*++ib->argp)
			break;
		if (*ib->argp++ == ',') {
			add_str(ib, ", ");
		} else {
			err_print("error in opcodes.c\n");
			return (-1);
		}
	}

	return (0);
}

int
get_operand(ib, size)
	inst_buffer    *ib;
	int		size;
{
	int		c = get_byte(ib);
	int		mode = c >> 4;
	int		reg = c & 0x0F;
	int		lit = c & 0x3F;
	int		tmp = 0;
	char		buf[16];

	switch (mode) {
	case 0:		/* literal */
	case 1:		/* literal */
	case 2:		/* literal */
	case 3:		/* literal */
		add_str(ib, LITERAL);
		add_int(ib, lit);
		tmp = lit;
		break;

	case 4:		/* indexed */
		snprintf(buf, sizeof buf, "[%s]", my_db_regs[reg].name);
		get_operand(ib, 0);
		add_str(ib, buf);
		break;

	case 5:		/* register */
		add_str(ib, my_db_regs[reg].name);
		break;

	case 6:		/* register deferred */
		add_str(ib, "(");
		add_str(ib, my_db_regs[reg].name);
		add_str(ib, ")");
		break;

	case 7:		/* autodecrement */
		add_str(ib, "-(");
		add_str(ib, my_db_regs[reg].name);
		add_str(ib, ")");
		if (reg == 0x0F) {	/* pc is not allowed in this mode */
			err_print("autodecrement not allowed for PC.\n");
		}
		break;

	case 9:		/* autoincrement deferred */
		add_str(ib, DEFERRED);
		if (reg == 0x0F) {	/* pc: immediate deferred */
			/*
			 * addresses are always longwords!
			 */
			tmp = get_long(ib);
			add_off(ib, tmp);
			break;
		}
		/* fall through */
	case 8:		/* autoincrement */
		if (reg == 0x0F) {	/* pc: immediate ==> special syntax */
			switch (size) {
			case SIZE_BYTE:
				tmp = (signed char) get_byte(ib);
				break;
			case SIZE_WORD:
				tmp = (signed short) get_word(ib);
				break;
			case SIZE_LONG:
				tmp = get_long(ib);
				break;
			default:
				err_print("illegal op-type %d\n", size);
				tmp = -1;
			}
			if (mode == 8)
				add_str(ib, LITERAL);
			add_int(ib, tmp);
			break;
		}
		add_str(ib, "(");
		add_str(ib, my_db_regs[reg].name);
		add_str(ib, ")+");
		break;

	case 11:	/* byte displacement deferred/ relative deferred  */
		add_str(ib, DEFERRED);
	case 10:	/* byte displacement / relative mode */
		tmp = (signed char) get_byte(ib);
		if (reg == 0x0F) {
			add_off(ib, (u_int) ib->ppc + tmp);
			break;
		}
		/* add_str (ib, "b^"); */
		add_int(ib, tmp);
		add_str(ib, "(");
		add_str(ib, my_db_regs[reg].name);
		add_str(ib, ")");
		break;

	case 13:		/* word displacement deferred */
		add_str(ib, DEFERRED);
	case 12:		/* word displacement */
		tmp = (signed short) get_word(ib);
		if (reg == 0x0F) {
			add_off(ib, (u_int) ib->ppc + tmp);
			break;
		}
		/* add_str (ib, "w^"); */
		add_int(ib, tmp);
		add_str(ib, "(");
		add_str(ib, my_db_regs[reg].name);
		add_str(ib, ")");
		break;

	case 15:		/* long displacement deferred */
		add_str(ib, DEFERRED);
	case 14:		/* long displacement */
		tmp = get_long(ib);
		if (reg == 0x0F) {
			add_off(ib, (u_int) ib->ppc + tmp);
			break;
		}
		/* add_str (ib, "l^"); */
		add_int(ib, tmp);
		add_str(ib, "(");
		add_str(ib, my_db_regs[reg].name);
		add_str(ib, ")");
		break;

	default:
		err_print("can\'t evaluate operand (%02X).\n", lit);
		break;
	}

	return (0);
}

int
get_byte(ib)
	inst_buffer    *ib;
{
	return ((unsigned char) *(ib->ppc++));
}

int
get_word(ib)
	inst_buffer    *ib;
{
	int		tmp;
	char	       *p = (void *) &tmp;

	*p++ = get_byte(ib);
	*p++ = get_byte(ib);
	return (tmp);
}

int
get_long(ib)
	inst_buffer    *ib;
{
	int		tmp;
	char	       *p = (void *) &tmp;

	*p++ = get_byte(ib);
	*p++ = get_byte(ib);
	*p++ = get_byte(ib);
	*p++ = get_byte(ib);
	return (tmp);
}

void
add_str(ib, s)
	inst_buffer    *ib;
	char	       *s;
{

	if (s == NULL)
		s = "-reserved-";

	strlcat(ib->dasm, s, sizeof(ib->dasm));
}

void
add_int(ib, i)
	inst_buffer    *ib;
	int		i;
{
	char		buf[32];

	if (i < 100 && i > -100)
		snprintf(buf, sizeof buf, "%d", i);
	else
		snprintf(buf, sizeof buf, "0x%x", i);
	add_str(ib, buf);
}

void
add_xint(ib, val)
	inst_buffer    *ib;
	int		val;
{
	char		buf[32];

	snprintf(buf, sizeof buf, "0x%x", val);
	add_str(ib, buf);
}

void
add_sym(ib, loc)
	inst_buffer    *ib;
	int		loc;
{
	db_expr_t	diff;
	db_sym_t	sym;
	char	       *symname;

	if (!loc)
		return;

	diff = INT_MAX;
	symname = NULL;
	sym = db_search_symbol(loc, DB_STGY_ANY, &diff);
	db_symbol_values(sym, &symname, 0);

	if (symname && !diff) {
		/* add_str(ib, "<"); */
		add_str(ib, symname);
		/* add_str(ib, ">"); */
	}
	else
		add_xint(ib, loc);
}

void
add_off(ib, loc)
	inst_buffer    *ib;
	int		loc;
{
	db_expr_t	diff;
	db_sym_t	sym;
	char	       *symname;

	if (!loc)
		return;
	  
	diff = INT_MAX;
	symname = NULL;
	sym = db_search_symbol(loc, DB_STGY_ANY, &diff);
	db_symbol_values(sym, &symname, 0);

	if (symname) {
		/* add_str(ib, "<"); */
		add_str(ib, symname);
		if (diff) {
			add_str(ib, "+");
			add_xint(ib, diff);
		}
		/* add_str(ib, ">"); */
	}
	else
		add_xint(ib, loc);
}
