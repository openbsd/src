/*	$OpenBSD: tc-powerpc.c,v 1.1 1998/02/15 18:49:42 niklas Exp $	*/

/* tc-ppc.c -- Assemble for the PowerPC or POWER (RS/6000)
   Copyright (C) 1994 Free Software Foundation, Inc.
   Written by Ian Lance Taylor, Cygnus Support.

   This file is part of GAS, the GNU Assembler.

   GAS is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 2, or (at your option)
   any later version.

   GAS is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with GAS; see the file COPYING.  If not, write to
   the Free Software Foundation, 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA. */

#include <stdio.h>
#include <ctype.h>
#include "as.h"
#include "subsegs.h"
#include "obstack.h"
#include "tc-powerpc.h"

#include "opcode/ppc.h"

/* This is the assembler for the PowerPC or POWER (RS/6000) chips.  */

/* Tell the main code what the endianness is.  */
int target_big_endian;

/* Whether or not, we've set target_big_endian.  */
static int set_target_endian = 0;

static void ppc_set_cpu PARAMS((void));
static unsigned long ppc_insert_operand
	PARAMS((unsigned long insn, const struct powerpc_operand *operand, long val));
static void ppc_macro PARAMS((char *str, const struct powerpc_macro *macro));
static void ppc_byte PARAMS((int));
static int ppc_is_toc_sym PARAMS((symbolS *sym));
static void ppc_tc PARAMS((int));
static void ppc_cons PARAMS((int));
static enum reloc_type ppc_suffix PARAMS((char **));

/* Generic assembler global variables which must be defined by all
   targets.  */

/* Characters which always start a comment.  */
const char comment_chars[] = "#";

/* Characters which start a comment at the beginning of a line.  */
const char line_comment_chars[] = "#";

/* Characters which may be used to separate multiple commands on a
   single line.  */
const char line_separator_chars[] = ";";

/* Characters which are used to indicate an exponent in a floating
   point number.  */
const char EXP_CHARS[] = "eE";

/* Characters which mean that a number is a floating point constant,
   as in 0d1.0.  */
const char FLT_CHARS[] = "dD";

/* The target specific pseudo-ops which we support.  */

const pseudo_typeS md_pseudo_table[] =
{
	/* Pseudo-ops which must be overridden.  */
	{ "byte",	ppc_byte,	0 },
	
	{ "long",	ppc_cons,	4 },
	{ "word",	ppc_cons,	2 },
	{ "short",	ppc_cons,	2 },
	
	/* This pseudo-op is used even when not generating XCOFF output.  */
	{ "tc",		ppc_tc,		0 },
	
	{ NULL,		NULL,		0 }
};

const relax_typeS md_relax_table[] = {
	0,
};

int md_reloc_size = 12;		/* Size of relocation record */




/* Local variables.  */

/* The type of processor we are assembling for.  This is one or more
   of the PPC_OPCODE flags defined in opcode/ppc.h.  */
static int ppc_cpu = 0;

/* The size of the processor we are assembling for.  This is either
   PPC_OPCODE_32 or PPC_OPCODE_64.  */
static int ppc_size = PPC_OPCODE_32;

/* Opcode hash table.  */
static struct hash_control *ppc_hash;

/* Macro hash table.  */
static struct hash_control *ppc_macro_hash;

symbolS *GOT_symbol;		/* Pre-defined "_GLOBAL_OFFSET_TABLE" */


int
md_parse_option(argp, cntp, vecp)
	char **argp;
	int *cntp;
	char ***vecp;
{
	char *arg;
	
	switch (**argp) {
#ifdef	PIC
	case 'k':
		/* Predefine GOT symbol */
		GOT_symbol = symbol_find_or_make("__GLOBAL_OFFSET_TABLE_");
#endif
	case 'u':
		/* -u means that any undefined symbols should be treated as
		   external, which is the default for gas anyhow.  */
		break;
		
	case 'm':
		arg = *argp + 1;
		
		/* -mpwrx and -mpwr2 mean to assemble for the IBM POWER/2
		   (RIOS2).  */
		if (strcmp(arg, "pwrx") == 0 || strcmp(arg, "pwr2") == 0)
			ppc_cpu = PPC_OPCODE_POWER | PPC_OPCODE_POWER2;
		/* -mpwr means to assemble for the IBM POWER (RIOS1).  */
		else if (strcmp(arg, "pwr") == 0)
			ppc_cpu = PPC_OPCODE_POWER;
		/* -m601 means to assemble for the Motorola PowerPC 601, which includes
		   instructions that are holdovers from the Power. */
		else if (strcmp(arg, "601") == 0)
			ppc_cpu = PPC_OPCODE_PPC | PPC_OPCODE_601;
		/* -mppc, -mppc32, -m603, and -m604 mean to assemble for the
		   Motorola PowerPC 603/604.  */
		else if (strcmp(arg, "ppc") == 0
			 || strcmp(arg, "ppc32") == 0
			 || strcmp(arg, "403") == 0
			 || strcmp(arg, "603") == 0
			 || strcmp(arg, "604") == 0)
			ppc_cpu = PPC_OPCODE_PPC;
		/* -mppc64 and -m620 mean to assemble for the 64-bit PowerPC
		   620.  */
		else if (strcmp(arg, "ppc64") == 0 || strcmp(arg, "620") == 0) {
			ppc_cpu = PPC_OPCODE_PPC;
			ppc_size = PPC_OPCODE_64;
		}
		/* -mcom means assemble for the common intersection between Power
		   and PowerPC.  At present, we just allow the union, rather
		   than the intersection.  */
		else if (strcmp(arg, "com") == 0)
			ppc_cpu = PPC_OPCODE_COMMON;
		/* -many means to assemble for any architecture (PWR/PWRX/PPC).  */
		else if (strcmp(arg, "any") == 0)
			ppc_cpu = PPC_OPCODE_ANY;
		
		/* -mlittle/-mbig set the endianess */
		else if (strcmp(arg, "little") == 0 || strcmp(arg, "little-endian") == 0) {
			target_big_endian = 0;
			set_target_endian = 1;
		} else if (strcmp(arg, "big") == 0 || strcmp(arg, "big-endian") == 0) {
			target_big_endian = 1;
			set_target_endian = 1;
		} else {
			as_bad("invalid switch -m%s", arg);
			return 0;
		}
		**argp = 0;
		break;
		
	default:
		return 0;
	}
	
	return 1;
}

void
md_show_usage(stream)
	FILE *stream;
{
	fprintf(stream, "\
PowerPC options:\n\
-u			ignored\n\
-mpwrx, -mpwr2		generate code for IBM POWER/2 (RIOS2)\n\
-mpwr			generate code for IBM POWER (RIOS1)\n\
-m601			generate code for Motorola PowerPC 601\n\
-mppc, -mppc32, -m403, -m603, -m604\n\
			generate code for Motorola PowerPC 603/604\n\
-mppc64, -m620		generate code for Motorola PowerPC 620\n\
-mcom			generate code Power/PowerPC common instructions\n\
-many			generate code for any architecture (PWR/PWRX/PPC)\n");
  fprintf(stream, "\
-mlittle, -mlittle-endian\n\
			generate code for a little endian machine\n\
-mbig, -mbig-endian	generate code for a big endian machine\n");
}

/* Set ppc_cpu if it is not already set.  */
static void
ppc_set_cpu()
{
	const char *default_cpu = TARGET_CPU;
	
	if (ppc_cpu == 0) {
		if (strcmp(default_cpu, "rs6000") == 0)
			ppc_cpu = PPC_OPCODE_POWER;
		else if (strcmp(default_cpu, "powerpc") == 0
			 || strcmp(default_cpu, "powerpcle") == 0)
			ppc_cpu = PPC_OPCODE_PPC;
		else
			as_fatal("Unknown default cpu = %s", default_cpu);
	}
}

/* This gets called to early */
void
md_begin()
{
}

static int begin_called = 0;

static void
really_begin()
{
	register const struct powerpc_opcode *op;
	const struct powerpc_opcode *op_end;
	const struct powerpc_macro *macro;
	const struct powerpc_macro *macro_end;
	int dup_insn = 0;

	begin_called = 1;

	ppc_set_cpu();

	/* Insert the opcodes into a hash table.  */
	ppc_hash = hash_new();

	op_end = powerpc_opcodes + powerpc_num_opcodes;
	for (op = powerpc_opcodes; op < op_end; op++) {
		know((op->opcode & op->mask) == op->opcode);

		if ((op->flags & ppc_cpu) != 0
		    && ((op->flags & (PPC_OPCODE_32 | PPC_OPCODE_64)) == 0
			|| (op->flags & (PPC_OPCODE_32 | PPC_OPCODE_64)) == ppc_size)) {
			const char *retval;
			
			retval = hash_insert(ppc_hash, op->name, op);
			if (*retval) {
				/* Ignore Power duplicates for -m601 */
				if ((ppc_cpu & PPC_OPCODE_601) != 0
				    && (op->flags & PPC_OPCODE_POWER) != 0)
					continue;

				as_bad("Internal assembler error for instruction %s", op->name);
				dup_insn = 1;
			}
		}
	}

	/* Insert the macros into a hash table.  */
	ppc_macro_hash = hash_new();

	macro_end = powerpc_macros + powerpc_num_macros;
	for (macro = powerpc_macros; macro < macro_end; macro++) {
		if ((macro->flags & ppc_cpu) != 0) {
			const char *retval;

			retval = hash_insert(ppc_macro_hash, macro->name, macro);
			if (*retval) {
				as_bad("Internal assembler error for macro %s", macro->name);
				dup_insn = 1;
			}
		}
	}

	if (dup_insn)
		abort();

	/* Tell the main code what the endianness is if it is not overidden by the user.  */
	if (!set_target_endian) {
		set_target_endian = 1;
		target_big_endian = 1;
	}
}

/* Nothing to do at the moment */
void
md_end()
{
}

/* Parse an operand that is machine-specific */
/* ARGSUSED */
void
md_operand(exp)
	expressionS *exp;
{
}

#define	RELOC_PCRELATIVE(r)					\
	(((r) >= RELOC_REL24 && (r) <= RELOC_REL14_NTAKEN)	\
	 || (r) == RELOC_PLT24					\
	 || (r) == RELOC_REL32					\
	 || (r) == RELOC_PLTREL32)
static int reloc_target_size[] = {
	-1,  4,  4,  2,  2,  2,  2,  4,
	-1, -1,  4,  4, -1, -1,  2,  2,
	 2,  2,  4, -1, -1, -1, -1, -1,
	-1, -1,  4,  4,  4,  2,  2,  2
};

/* Insert an operand value into an instruction.  */
static unsigned long
ppc_insert_operand(insn, operand, val)
	unsigned long insn;
	const struct powerpc_operand *operand;
	long val;
{
	if (operand->bits != 32) {
		long min, max;
		long test;

#ifdef	__notdef__
		if ((operand->flags & PPC_OPERAND_SIGNED) != 0) {
			if ((operand->flags & PPC_OPERAND_SIGNOPT) != 0
			    && ppc_size == PPC_OPCODE_32)
				max = (1 << operand->bits) - 1;
			else
				max = (1 << (operand->bits - 1)) - 1;
			min = - (1 << (operand->bits - 1));
		} else {
			max = (1 << operand->bits) - 1;
			min = 0;
		}
#else
		/*
		 * For now, we allow both signed and unsigned operands
		 */
		min = - (1 << (operand->bits - 1));
		max = (1 << operand->bits) - 1;
#endif

		if ((operand->flags & PPC_OPERAND_NEGATIVE) != 0)
			test = - val;
		else
			test = val;

		if (test < min || test > max) {
			const char *err =
				"operand out of range (%s not between %ld and %ld)";
			char buf[100];

			sprint_value(buf, test);
			as_warn(err, buf, min, max);
		}
	}

	if (operand->insert) {
		const char *errmsg;

		errmsg = NULL;
		insn = (*operand->insert)(insn, (long)val, &errmsg);
		if (errmsg != (const char *)NULL)
			as_warn(errmsg);
	} else
		insn |= (((long) val & ((1 << operand->bits) - 1))
			 << operand->shift);

	return insn;
}

/* Parse @got, etc. and return the desired relocation.  */
static enum reloc_type
ppc_suffix(str_p)
	char **str_p;
{
	struct map_reloc {
		char *string;
		int length;
		enum reloc_type reloc;
	};

	char ident[20];
	char *str = *str_p;
	char *str2;
	int ch;
	int len;
	struct map_reloc *ptr;

#define MAP(str,reloc) { str, sizeof(str)-1, reloc }

	static struct map_reloc mapping[] = {
		MAP ("got",		RELOC_GOT16),
		MAP ("l",		RELOC_16_LO),
		MAP ("h",		RELOC_16_HI),
		MAP ("ha",		RELOC_16_HA),
		MAP ("brtaken",		RELOC_REL14_TAKEN),
		MAP ("brntaken",	RELOC_REL14_NTAKEN),
		MAP ("got@l",		RELOC_GOT16_LO),
		MAP ("got@h",		RELOC_GOT16_HI),
		MAP ("got@ha",		RELOC_GOT16_HA),
		MAP ("fixup",		RELOC_32),
		MAP ("pltrel24",	RELOC_PLT24),
		MAP ("copy",		RELOC_COPY),
		MAP ("globdat",		RELOC_GLOB_DAT),
		MAP ("local24pc",	RELOC_LOCAL24PC),
		MAP ("plt",		RELOC_PLT32),
		MAP ("pltrel",		RELOC_PLTREL32),
		MAP ("plt@l",		RELOC_PLT16_LO),
		MAP ("plt@h",		RELOC_PLT16_HI),
		MAP ("plt@ha",		RELOC_PLT16_HA),
		
		{ (char *)0,	0,	RELOC_NONE }
	};

	if (*str++ != '@')
		return RELOC_NONE;

	for (ch = *str, str2 = ident;
	     str2 < ident + sizeof(ident) - 1 && isalnum (ch) || ch == '@';
	     ch = *++str) {
		*str2++ = (islower (ch)) ? ch : tolower (ch);
	}

	*str2 = '\0';
	len = str2 - ident;

	ch = ident[0];
	for (ptr = &mapping[0]; ptr->length > 0; ptr++)
		if (ch == ptr->string[0]
		    && len == ptr->length
		    && memcmp(ident, ptr->string, ptr->length) == 0) {
			*str_p = str;
			return ptr->reloc;
		}

	return RELOC_NONE;
}

/* Like normal .long/.short/.word, except support @got, etc. */
/* clobbers input_line_pointer, checks */
/* end-of-line. */
static void
ppc_cons(nbytes)
	register int nbytes;	/* 1=.byte, 2=.word, 4=.long */
{
	expressionS exp;
	enum reloc_type reloc;
	long get;
	segT segment;
	char *p;

	if (!begin_called)
		really_begin();
	
	if (is_it_end_of_statement()) {
		demand_empty_rest_of_line();
		return;
	}

	do {
		segment = expression(&exp);
		if (!need_pass_2) {
			know(segment != SEG_PASS1);
			if (segment == SEG_DIFFERENCE && exp.X_add_symbol == NULL) {
				as_bad("Subtracting symbol \"%s\"(segment\"%s\") is too hard. Absolute segment assumed.",
				       S_GET_NAME(exp.X_subtract_symbol),
				       segment_name(S_GET_SEGMENT(exp.X_subtract_symbol)));
				segment = SEG_ABSOLUTE;
			}
			p = frag_more(nbytes);
			switch (segment) {
			case SEG_BIG:
				as_bad("%s number invalid. Absolute 0 assumed.",
				       exp.X_add_number > 0 ? "Bignum" : "Floating-Point");
				md_number_to_chars(p, (long)0, nbytes);
				break;
			case SEG_ABSENT:
				as_warn("0 assumed for missing expression");
				exp.X_add_number = 0;
				know(exp.X_add_symbol == NULL);
				/* FALLTHROUGH */
			case SEG_ABSOLUTE:
				get = exp.X_add_number;
				md_number_to_chars(p, get, nbytes);
				break;
			default:
				if (*input_line_pointer == '@'
				    && (reloc = ppc_suffix(&input_line_pointer)) != RELOC_NONE) {
					int size = reloc_target_size[reloc];
					int offset = nbytes - size;
				
					fix_new(frag_now,
						p - frag_now->fr_literal + offset,
						size,
						exp.X_add_symbol, exp.X_subtract_symbol,
						exp.X_add_number, 0,
						reloc
#ifdef	PIC
						,exp.X_got_symbol
#endif
						);
				} else
					fix_new(frag_now, p - frag_now->fr_literal, nbytes,
						exp.X_add_symbol, exp.X_subtract_symbol,
						exp.X_add_number, 0,
						RELOC_32,
#ifdef	PIC
						exp.X_got_symbol
#endif
						);
				break;
			}
		}
	} while (*input_line_pointer++ == ',');

	input_line_pointer--;		/* Put terminator back into stream. */
	demand_empty_rest_of_line();
}


/* We need to keep a list of fixups.  We can't simply generate them as
   we go, because that would require us to first create the frag, and
   that would screw up references to ``.''.  */
struct ppc_fixup {
	expressionS exp;
	int opindex;
	enum reloc_type reloc;
};

#define MAX_INSN_FIXUPS	5

/* This routine is called for each instruction to be assembled.  */
void
md_assemble(str)
	char *str;
{
	char *s;
	const struct powerpc_opcode *opcode;
	unsigned long insn;
	const unsigned char *opindex_ptr;
	int skip_optional;
	int need_paren;
	int next_opindex;
	struct ppc_fixup fixups[MAX_INSN_FIXUPS];
	int fc;
	char *f;
	int i;
	enum reloc_type reloc;

	if (!begin_called)
		really_begin();
	
	/* Get the opcode.  */
	for (s = str; *s != '\0' && ! isspace (*s); s++)
		;
	if (*s != '\0')
		*s++ = '\0';

	/* Look up the opcode in the hash table.  */
	opcode = (const struct powerpc_opcode *) hash_find(ppc_hash, str);
	if (opcode == (const struct powerpc_opcode *)NULL) {
		const struct powerpc_macro *macro;

		macro = (const struct powerpc_macro *)hash_find(ppc_macro_hash, str);
		if (macro == (const struct powerpc_macro *)NULL)
			as_bad("Unrecognized opcode: `%s'", str);
		else
			ppc_macro(s, macro);

		return;
	}

	insn = opcode->opcode;

	str = s;
	while (isspace(*str))
		++str;

	/* PowerPC operands are just expressions.  The only real issue is
	   that a few operand types are optional.  All cases which might use
	   an optional operand separate the operands only with commas (in
	   some cases parentheses are used, as in ``lwz 1,0(1)'' but such
	   cases never have optional operands).  There is never more than
	   one optional operand for an instruction.  So, before we start
	   seriously parsing the operands, we check to see if we have an
	   optional operand, and, if we do, we count the number of commas to
	   see whether the operand should be omitted.  */
	skip_optional = 0;
	for (opindex_ptr = opcode->operands; *opindex_ptr != 0; opindex_ptr++) {
		const struct powerpc_operand *operand;

		operand = &powerpc_operands[*opindex_ptr];
		if ((operand->flags & PPC_OPERAND_OPTIONAL) != 0) {
			unsigned int opcount;

			/* There is an optional operand.  Count the number of
			   commas in the input line.  */
			if (*str == '\0')
				opcount = 0;
			else {
				opcount = 1;
				s = str;
				while ((s = strchr(s, ',')) != (char *)NULL) {
					++opcount;
					++s;
				}
			}

			/* If there are fewer operands in the line then are called
			   for by the instruction, we want to skip the optional
			   operand.  */
			if (opcount < strlen(opcode->operands))
				skip_optional = 1;
			
			break;
		}
	}

	/* Gather the operands.  */
	need_paren = 0;
	next_opindex = 0;
	fc = 0;
	for (opindex_ptr = opcode->operands; *opindex_ptr != 0; opindex_ptr++) {
		const struct powerpc_operand *operand;
		const char *errmsg;
		char *hold;
		expressionS ex;
		char endc;

		if (next_opindex == 0)
			operand = &powerpc_operands[*opindex_ptr];
		else {
			operand = &powerpc_operands[next_opindex];
			next_opindex = 0;
		}

		errmsg = NULL;

		/* If this is a fake operand, then we do not expect anything
		   from the input.  */
		if ((operand->flags & PPC_OPERAND_FAKE) != 0) {
			insn = (*operand->insert)(insn, 0L, &errmsg);
			if (errmsg != (const char *)NULL)
				as_warn(errmsg);
			continue;
		}

		/* If this is an optional operand, and we are skipping it, just
		   insert a zero.  */
		if ((operand->flags & PPC_OPERAND_OPTIONAL) != 0
		    && skip_optional) {
			if (operand->insert) {
				insn = (*operand->insert)(insn, 0L, &errmsg);
				if (errmsg != (const char *)NULL)
					as_warn(errmsg);
			}
			if ((operand->flags & PPC_OPERAND_NEXT) != 0)
				next_opindex = *opindex_ptr + 1;
			continue;
		}

		/* Gather the operand.  */
		hold = input_line_pointer;
		input_line_pointer = str;

		expression(&ex);
		str = input_line_pointer;
		input_line_pointer = hold;

		if (ex.X_add_symbol == NULL
		    && ex.X_subtract_symbol == NULL) {
			/* Allow @HA, @L, @H on constants. */
			char *orig_str = str;

			if ((reloc = ppc_suffix(&str)) != RELOC_NONE)
				switch (reloc) {
				default:
					str = orig_str;
					break;

				case RELOC_16_LO:
					ex.X_add_number &= 0xffff;
					break;

				case RELOC_16_HI:
					ex.X_add_number = (ex.X_add_number >> 16) & 0xffff;
					break;

				case RELOC_16_HA:
					ex.X_add_number = ((ex.X_add_number >> 16) & 0xffff)
						+ ((ex.X_add_number >> 15) & 1);
					break;
				}
			insn = ppc_insert_operand(insn, operand, ex.X_add_number);
		} else if ((reloc = ppc_suffix(&str)) != RELOC_NONE) {
			/* For the absoulte forms of branchs, convert the PC relative form back into
			   the absolute.  */
			if ((operand->flags & PPC_OPERAND_ABSOLUTE) != 0)
				switch (reloc) {
				case RELOC_REL24:
					reloc = RELOC_24;
					break;
				case RELOC_REL14:
					reloc = RELOC_14;
					break;
				case RELOC_REL14_TAKEN:
					reloc = RELOC_14_TAKEN;
					break;
				case RELOC_REL14_NTAKEN:
					reloc = RELOC_14_NTAKEN;
					break;
				}

			/* We need to generate a fixup for this expression.  */
			if (fc >= MAX_INSN_FIXUPS)
				as_fatal ("too many fixups");
			fixups[fc].exp = ex;
			fixups[fc].opindex = 0;
			fixups[fc].reloc = reloc;
			++fc;
		} else {
			/* We need to generate a fixup for this expression.  */
			if (fc >= MAX_INSN_FIXUPS)
				as_fatal ("too many fixups");
			fixups[fc].exp = ex;
			fixups[fc].opindex = *opindex_ptr;
			fixups[fc].reloc = RELOC_NONE;
			++fc;
		}

		if (need_paren) {
			endc = ')';
			need_paren = 0;
		} else if ((operand->flags & PPC_OPERAND_PARENS) != 0) {
			endc = '(';
			need_paren = 1;
		}
		else
			endc = ',';

		/* The call to expression should have advanced str past any
		   whitespace.  */
		if (*str != endc
		    && (endc != ',' || *str != '\0')) {
			as_bad("syntax error; found `%c' but expected `%c'", *str, endc);
			break;
		}

		if (*str != '\0')
			++str;
	}

	while (isspace (*str))
		++str;

	if (*str != '\0')
		as_bad("junk at end of line: `%s'", str);

	/* Write out the instruction.  */
	f = frag_more(4);
	md_number_to_chars(f, insn, 4);

	/* Create any fixups.  At this point we do not use a
	   enum reloc_type, but instead just use the
	   RELOC_NONE plus the operand index.  This lets us easily
	   handle fixups for any operand type, although that is admittedly
	   not a very exciting feature.  We pick a reloc type in
	   md_apply_fix.  */
	for (i = 0; i < fc; i++) {
		const struct powerpc_operand *operand;

		operand = &powerpc_operands[fixups[i].opindex];
		if (fixups[i].reloc != RELOC_NONE) {
			int size = reloc_target_size[fixups[i].reloc];
			int pcrel = RELOC_PCRELATIVE(fixups[i].reloc);
			int offset;

			offset = target_big_endian ? (4 - size) : 0;

			if (size < 1 || size > 4)
				abort();

			fix_new(frag_now, f - frag_now->fr_literal + offset, size,
				fixups[i].exp.X_add_symbol,
				fixups[i].exp.X_subtract_symbol,
				fixups[i].exp.X_add_number,
				pcrel, fixups[i].reloc
#ifdef	PIC
				,fixups[i].exp.X_got_symbol
#endif
				);
		} else
			fix_new(frag_now, f - frag_now->fr_literal, 4,
				fixups[i].exp.X_add_symbol,
				fixups[i].exp.X_subtract_symbol,
				fixups[i].exp.X_add_number,
				(operand->flags & PPC_OPERAND_RELATIVE) != 0,
				(enum reloc_type)(fixups[i].opindex + (int)RELOC_MAX)
#ifdef	PIC
				,fixups[i].exp.X_got_symbol
#endif
				);
	}
}

/* Handle a macro.  Gather all the operands, transform them as
   described by the macro, and call md_assemble recursively.  All the
   operands are separated by commas; we don't accept parentheses
   around operands here.  */
static void
ppc_macro(str, macro)
	char *str;
	const struct powerpc_macro *macro;
{
	char *operands[10];
	unsigned int count;
	char *s;
	unsigned int len;
	const char *format;
	int arg;
	char *send;
	char *complete;

	/* Gather the users operands into the operands array.  */
	count = 0;
	s = str;
	while (1) {
		if (count >= sizeof operands / sizeof operands[0])
			break;
		operands[count++] = s;
		s = strchr (s, ',');
		if (s == (char *) NULL)
			break;
		*s++ = '\0';
	}

	if (count != macro->operands) {
		as_bad("wrong number of operands");
		return;
	}

	/* Work out how large the string must be (the size is unbounded
	   because it includes user input).  */
	len = 0;
	format = macro->format;
	while (*format != '\0') {
		if (*format != '%') {
			++len;
			++format;
		} else {
			arg = strtol(format + 1, &send, 10);
			know(send != format && arg >= 0 && arg < count);
			len += strlen(operands[arg]);
			format = send;
		}
	}

	/* Put the string together.  */
	complete = s = (char *)alloca(len + 1);
	format = macro->format;
	while (*format != '\0') {
		if (*format != '%')
			*s++ = *format++;
		else {
			arg = strtol(format + 1, &send, 10);
			strcpy(s, operands[arg]);
			s += strlen(s);
			format = send;
		}
	}
	*s = '\0';

	/* Assemble the constructed instruction.  */
	md_assemble(complete);
}  

/* Pseudo-op handling.  */

/* The .byte pseudo-op.  This is similar to the normal .byte
   pseudo-op, but it can also take a single ASCII string.  */
static void
ppc_byte(ignore)
	int ignore;
{
	if (*input_line_pointer != '\"') {
		cons(1);
		return;
	}

	/* Gather characters.  A real double quote is doubled.  Unusual
	   characters are not permitted.  */
	++input_line_pointer;
	while (1) {
		char c;

		c = *input_line_pointer++;

		if (c == '\"') {
			if (*input_line_pointer != '\"')
				break;
			++input_line_pointer;
		}

		FRAG_APPEND_1_CHAR(c);
	}

	demand_empty_rest_of_line();
}

/* The .tc pseudo-op.  This is used when generating either XCOFF or
   ELF.  This takes two or more arguments.

   When generating XCOFF output, the first argument is the name to
   give to this location in the toc; this will be a symbol with class
   TC.  The rest of the arguments are 4 byte values to actually put at
   this location in the TOC; often there is just one more argument, a
   relocateable symbol reference.

   When not generating XCOFF output, the arguments are the same, but
   the first argument is simply ignored.  */
static void
ppc_tc(ignore)
	int ignore;
{

	/* Skip the TOC symbol name.  */
	while (is_part_of_name(*input_line_pointer)
	       || *input_line_pointer == '['
	       || *input_line_pointer == ']'
	       || *input_line_pointer == '{'
	       || *input_line_pointer == '}')
		++input_line_pointer;

	/* Align to a four byte boundary.  */
	frag_align(2, 0);
	record_alignment(now_seg, 2);

	if (*input_line_pointer != ',')
		demand_empty_rest_of_line();
	else {
		++input_line_pointer;
		cons(4);
	}
}

/* Turn a string in input_line_pointer into a floating point constant
   of type type, and store the appropriate bytes in *litp.  The number
   of LITTLENUMS emitted is stored in *sizep .  An error message is
   returned.  */
char *
md_atof(type, litp, sizep)
	int type;
	char *litp;
	int *sizep;
{
	int prec;
	LITTLENUM_TYPE words[4];
	char *t;
	int i;

	if (!begin_called)
		really_begin();
	
	switch (type) {
	case 'f':
		prec = 2;
		break;

	case 'd':
		prec = 4;
		break;

	default:
		*sizep = 0;
		return "bad call to md_atof";
	}

	t = atof_ieee(input_line_pointer, type, words);
	if (t)
		input_line_pointer = t;

	*sizep = prec * 2;

	if (target_big_endian) {
		for (i = 0; i < prec; i++) {
			md_number_to_chars(litp, (valueT) words[i], 2);
			litp += 2;
		}
	} else {
		for (i = prec - 1; i >= 0; i--) {
			md_number_to_chars(litp, (valueT) words[i], 2);
			litp += 2;
		}
	}
	
	return "";
}

/* Write a value out to the object file, using the appropriate
   endianness.  */
void
md_number_to_chars(buf, val, n)
	char *buf;
	long val;
	int n;
{
	if (!begin_called)
		really_begin();
	
	if (target_big_endian)
		buf += n;
	while (--n >= 0) {
		if (target_big_endian)
			*--buf = val;
		else
			*buf++ = val;
		val >>= 8;
	}
}

/* Align a section (I don't know why this is machine dependent).  */
long
md_section_align(seg, addr)
	segT seg;
	long addr;
{
	/* Align all sections to 128-bit boundaries */
	return (addr + 15) & ~15;
}

/* We don't have any form of relaxing.  */
int
md_estimate_size_before_relax(fragp, seg)
	fragS *fragp;
	segT seg;
{
	abort();
	return 0;
}

/* Convert a machine dependent frag.  We never generate these.  */
void
md_convert_frag(headers, fragp)
	object_headers *headers;
	fragS *fragp;
{
	abort();
}

/* We have no need to default values of symbols.  */
/*ARGSUSED*/
symbolS *
md_undefined_symbol(name)
	char *name;
{
	return 0;
}

/* Functions concerning relocs.  */

/* The location from which a PC relative jump should be calculated,
   given a PC relative reloc.  */
long
md_pcrel_from(fixp)
	fixS *fixp;
{
	return fixp->fx_frag->fr_address + fixp->fx_where;
}
 
/* See whether a symbol is in the TOC section.  FIXME (ws) */
static int
ppc_is_toc_sym(sym)
	symbolS *sym;
{
	return strcmp(segment_name(S_GET_SEGMENT(sym)), ".got") == 0;
}

static long
md_getl(p)
	unsigned char *p;
{
	if (target_big_endian)
		return (p[0] << 24)
			| (p[1] << 16)
			| (p[2] << 8)
			| p[3];
	else
		return (p[3] << 24)
			| (p[2] << 16)
			| (p[1] << 8)
			| p[0];
}

static void
md_putl(l, p)
	unsigned long l;
	unsigned char *p;
{
	if (target_big_endian) {
		*p++ = l >> 24;
		*p++ = l >> 16;
		*p++ = l >> 8;
		*p++ = l;
	} else {
		*p++ = l;
		*p++ = l >> 8;
		*p++ = l >> 16;
		*p++ = l >> 24;
	}
}

/* Apply a fixup to the object code.  This is called for all the
   fixups we generated by the call to fix_new_exp, above.  In the call
   above we used a reloc code which was the largest legal reloc code
   plus the operand index.  Here we undo that to recover the operand
   index.  At this point all symbol values should be fully resolved,
   and we attempt to completely resolve the reloc.  If we can not do
   that, we determine the correct reloc code and put it back in the
   fixup.  */

void
md_apply_fix(fixp, val)
	fixS *fixp;
	long val;
{
	/* Remember value for reloc entry */
	fixp->fx_addnumber = val;

	if (!fixp->fx_pcrel && fixp->fx_addsy) {
		val = fixp->fx_offset;
		if (fixp->fx_subsy != (symbolS *)NULL) {
			/* We can't actually support subtracting a symbol.  */
			as_bad("expression too complex");
		}
	}

	if ((int)fixp->fx_r_type >= (int)RELOC_MAX) {
		int opindex;
		const struct powerpc_operand *operand;
		char *where;
		unsigned long insn;

		opindex = (int)fixp->fx_r_type - (int)RELOC_MAX;

		operand = &powerpc_operands[opindex];

		/* Fetch the instruction, insert the fully resolved operand
		   value, and stuff the instruction back again.  */
		where = fixp->fx_frag->fr_literal + fixp->fx_where;
		insn = md_getl((unsigned char *)where);
		insn = ppc_insert_operand(insn, operand, val);
		md_putl(insn, (unsigned char *)where);

		/* Determine a reloc value based on the operand information.
		   We are only prepared to turn a few of the operands into
		   relocs.
		   FIXME: We need to handle the DS field at the very least.
		   FIXME: Selecting the reloc type is a bit haphazard; perhaps
		   there should be a new field in the operand table.  */
		if ((operand->flags & PPC_OPERAND_RELATIVE) != 0
		    && operand->bits == 26
		    && operand->shift == 0)
			fixp->fx_r_type = RELOC_REL24;
		else if ((operand->flags & PPC_OPERAND_RELATIVE) != 0
			 && operand->bits == 16
			 && operand->shift == 0)
			fixp->fx_r_type = RELOC_REL14;
		else if ((operand->flags & PPC_OPERAND_ABSOLUTE) != 0
			 && operand->bits == 26
			 && operand->shift == 0)
			fixp->fx_r_type = RELOC_24;
		else if ((operand->flags & PPC_OPERAND_ABSOLUTE) != 0
			 && operand->bits == 16
			 && operand->shift == 0)
			fixp->fx_r_type = RELOC_14;
		else if ((operand->flags & PPC_OPERAND_PARENS) != 0
			 && operand->bits == 16
			 && operand->shift == 0
			 && operand->insert == NULL
			 && fixp->fx_addsy != NULL
			 && ppc_is_toc_sym(fixp->fx_addsy)) {
			fixp->fx_size = 2;
			if (target_big_endian)
				fixp->fx_where += 2;
			fixp->fx_r_type = RELOC_GOT16;
		} else if (fixp->fx_addsy == NULL
			   && fixp->fx_subsy == NULL)
			fixp->fx_r_type = RELOC_NONE;
		else {
			as_bad("unresolved expression that must be resolved");
			if (fixp->fx_addsy)
				ps(fixp->fx_addsy);
			if (fixp->fx_subsy) {
				fprintf(stderr, "-");
				ps(fixp->fx_subsy);
			}
			fprintf(stderr,"\n");
			return;
		}
	} else {
		switch (fixp->fx_r_type) {
		case RELOC_32:
			if (fixp->fx_pcrel) {
				fixp->fx_r_type = RELOC_REL32;
				val += fixp->fx_frag->fr_address + fixp->fx_where;
			}			/* fall through */
		case RELOC_REL32:
			md_number_to_chars(fixp->fx_frag->fr_literal + fixp->fx_where,
					   val, 4);
			break;
		case RELOC_16_HA:
			val += (val << 1) & 0x8000;
			/* Fall Through */
		case RELOC_16_HI:
			val >>= 16;
			/* Fall Through */
		case RELOC_16_LO:
		case RELOC_GOT16:
		case RELOC_16:
			if (fixp->fx_pcrel)
				abort();

			md_number_to_chars(fixp->fx_frag->fr_literal + fixp->fx_where,
					   val, 2);
			break;
		default:
			fprintf(stderr,
				"Gas failure, reloc value %d\n", fixp->fx_r_type);
			fflush(stderr);
			abort();
		}
	}
}

void
tc_aout_fix_to_chars(where, fixp, segment_address)
	char *where;
	fixS *fixp;
	relax_addressT segment_address;
{
	long r_index;
	long r_extern;
	long r_addend = 0;
	long r_address;
#ifdef	PIC
	int kflag = 0;
#endif

	know(fixp->fx_addsy);
	
	if (!S_IS_DEFINED(fixp->fx_addsy)) {
		r_extern = 1;
		r_index = fixp->fx_addsy->sy_number;
	} else {
		r_extern = 0;
		r_index = S_GET_TYPE(fixp->fx_addsy);
#ifdef	PIC
		if (flagseen['k'])
			as_bad("tc_aout_fix_to_chars for -k to be done");
#endif
	}
	
	md_number_to_chars(where,
			   r_address = fixp->fx_frag->fr_address
		       		       + fixp->fx_where
				       - segment_address,
			   4);

	/* This is probably wrong for little-endian, but... */
	where[4] = r_index >> 16;
	where[5] = r_index >> 8;
	where[6] = r_index;
	where[7] = ((r_extern << 7) & 0x80) | (fixp->fx_r_type & 0x1f);
	
	if (fixp->fx_addsy->sy_frag)
		r_addend = fixp->fx_addsy->sy_frag->fr_address;
	
	if (fixp->fx_pcrel) {
#ifdef	PIC
		if (fixp->fx_gotsy) {
			r_addend = r_address;
			r_addend += fixp->fx_addnumber;
		} else
#endif
			r_addend -= r_address;
	} else {
#ifdef	PIC
		if (kflag)
			r_addend = 0;
		else
#endif
			r_addend = fixp->fx_addnumber;
	}
	
	md_number_to_chars(&where[8], r_addend, 4);
}
