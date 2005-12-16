/* $OpenBSD: dsdt.c,v 1.11 2005/12/16 00:08:53 jordan Exp $ */
/*
 * Copyright (c) 2005 Jordan Hargrave <jordan@openbsd.org>
 *
 * Permission to use, copy, modify, and distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 * ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
 * OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/device.h>
#include <sys/malloc.h>

#include <machine/bus.h>

#include <dev/acpi/acpireg.h>
#include <dev/acpi/acpivar.h>
#include <dev/acpi/amltypes.h>
#include <dev/acpi/dsdt.h>

extern int acpi_debug;

struct aml_optable
{
	u_int16_t    opcode;
	const char  *mnem;
	const char  *args;
};

struct aml_stream
{
	u_int8_t *start;
	u_int8_t *end;
	u_int8_t *pos;
};

int aml_isnamedop(u_int16_t);
u_int8_t *aml_decodelength(u_int8_t *, int *);
u_int8_t *aml_decodename(u_int8_t *, const char **, const char *);
u_int8_t *aml_getopcode(u_int8_t *, u_int16_t *);
u_int8_t *aml_parseargs(struct acpi_softc *, struct aml_node *, u_int8_t *, const char *);
u_int8_t *aml_parse_object(struct acpi_softc *, struct aml_node *, u_int8_t *);

u_int64_t  aml_bcd2dec(u_int64_t);
u_int64_t  aml_dec2bcd(u_int64_t);

int aml_lsb(u_int32_t val);
int aml_msb(u_int32_t val);
void aml_addchildnode(struct aml_node *, struct aml_node *);
void aml_walktree(struct aml_node *, int);
void aml_walkroot(void);

int64_t aml_evalmath(u_int16_t, int64_t, int64_t);
int  aml_testlogical(u_int16_t, long, long);
int  aml_strcmp(u_int16_t opcode, const char *lhs, const char *rhs);

void aml_setinteger(struct aml_value *, int, int64_t);
void aml_setstring(struct aml_value *, const char *);
void aml_setbuffer(struct aml_value *, int, u_int8_t *);
void aml_setfield(struct aml_value *, int, int, struct aml_node *);
void aml_setopregion(struct aml_value *, int, int, u_int64_t);
void aml_setpackage(struct aml_value *, struct aml_node *);
void aml_setprocessor(struct aml_value *, u_int8_t, u_int32_t, u_int8_t);

void aml_setnodevalue(struct acpi_softc *, struct aml_node *, const struct aml_value *);
void aml_setnodeinteger(struct acpi_softc *, struct aml_node *, int64_t);

int aml_match(struct acpi_softc *, int, const struct aml_value *, const struct aml_value *);
int aml_cmpobj(struct acpi_softc *, const struct aml_value *, const struct aml_value *);

void
aml_setinteger(struct aml_value *val, int size, int64_t value)
{
	val->type = AML_OBJTYPE_INTEGER;
	val->v_integer = value;
	val->length = size;
}

void
aml_setstring(struct aml_value *val, const char *str)
{
	val->type = AML_OBJTYPE_STRING;
	val->length = strlen(str);
	val->v_string = str;
}

void
aml_setbuffer(struct aml_value *val, int size, u_int8_t *ptr)
{
	val->type = AML_OBJTYPE_STRING;
	val->length = size;
	val->v_buffer = ptr;
}

void
aml_setfield(struct aml_value *val, int bitpos, int bitlen, struct aml_node *ref)
{
	val->type = AML_OBJTYPE_FIELDUNIT;
	val->length = (bitlen + 7) / 8;
	val->v_field.bitpos = bitpos;
	val->v_field.bitlen = bitlen;
	val->v_field.ref = ref;
}

void
aml_setpackage(struct aml_value *val, struct aml_node *node)
{
}

void
aml_setprocessor(struct aml_value *val, u_int8_t id, u_int32_t addr, u_int8_t len)
{
	val->type = AML_OBJTYPE_PROCESSOR;
	val->v_processor.proc_id = id;
	val->v_processor.proc_addr = addr;
	val->v_processor.proc_len = len;
}

/* SetOpRegion addresses
 *  0 = SystemMem
 *  1 = SystemIO
 *  2 = PCIConfSpace
 *     dw offset,fn,dev,reserved
 */
void
aml_setopregion(struct aml_value *val, int addrtype, int size, u_int64_t addr)
{
	val->type = AML_OBJTYPE_OPREGION;
	val->v_opregion.address_space_id = addrtype;
	val->v_opregion.register_bit_width = 0;
	val->v_opregion.register_bit_offset = 0;
	val->v_opregion.access_size = size;
	val->v_opregion.address = addr;
}

/* Decode AML Package length
 * Upper two bits of first byte denote length
 *   0x00 = length is in lower 6 bits
 *   0x40 = length is lower 4 bits + 1 byte
 *   0x80 = length is lower 4 bits + 2 bytes
 *   0xC0 = length is lower 4 bits + 3 bytes
 */
u_int8_t *
aml_decodelength(u_int8_t *pos, int *length)
{
	u_int8_t lcode;

	lcode = *(pos++);

	*length = (lcode & 0xF);
	switch(lcode >> 6) {
	case 0x01:
		*length += (pos[0] << 4L);
		return pos+1;
	case 0x02:
		*length += (pos[0] << 4L) + (pos[1] << 12L);
		return pos+2;
	case 0x03:
		*length += (pos[0] << 4L) + (pos[1] << 12L) + (pos[2] << 20L);
		return pos+3;
	default:
		*length = (lcode & 0x3F);
		return pos;
	}
}

/* Decode AML Namestring from stream */
u_int8_t *
aml_decodename(u_int8_t *pos, const char **ref, const char *lbl)
{
	int count, pfxlen, idx;
	char *name;
	u_int8_t *base;

	base = pos;
	if (*pos == AMLOP_ROOTCHAR) {
		pos++;
	}
	while (*pos == AMLOP_PARENTPREFIX) {
		pos++;
	}
	pfxlen = pos - base;

	count = 1;
	if (*pos == AMLOP_MULTINAMEPREFIX) {
		count = *(++pos);
		pos++;
	}
	else if (*pos == AMLOP_DUALNAMEPREFIX) {
		count = 2;
		pos++;
	}
	else if (*pos == 0) {
		count = 0;
		pos++;
	}

	name = malloc(pfxlen + count * 5, M_DEVBUF, M_WAITOK);
	if (name == NULL) 
		return pos;

	if (pfxlen > 0) {
		memcpy(name, base, pfxlen);
	}
	/* Copy name segments in chunks of 4 bytes */
	base = name+pfxlen;
	for(idx=0; idx<count; idx++) {
		if (idx) *(base++) = '.';
		memcpy(base, pos, 4);
		pos += 4;
		base += 4;
	}
	*base = 0;

	dnprintf(50, " acpi_name (%s): %s\n", lbl, name);
	if (ref != NULL) {
		*ref = name;
	}
	else {
		free(name, M_DEVBUF);
	}

	return pos;
}

/* Is this opcode an encoded name? */
int
aml_isnamedop(u_int16_t opcode)
{
	switch (opcode) {
	case AMLOP_ROOTCHAR:
        case AMLOP_PARENTPREFIX:
        case AMLOP_MULTINAMEPREFIX:
        case AMLOP_DUALNAMEPREFIX:
        case AMLOP_NAMECHAR:
		return (1);
	}

	if (opcode >= 'A' && opcode <= 'Z')
		return (1);

	return (0);
}

u_int64_t
aml_bcd2dec(u_int64_t val)
{
	u_int64_t rval;
	int n,pos;

	pos=1;
	for (rval=0; val; val >>= 4) {
		n = (val & 0xF);
		if (n > 9)
			return (0);

		rval += (n * pos);
		pos *= 10;
	}
	return rval;
}

u_int64_t
aml_dec2bcd(u_int64_t val)
{
	u_int64_t rval;
	int n,pos;

	pos=0;
	for (rval=0; val; val /= 10) {
		n = (val % 10);

		rval += (n << pos);
		pos += 4;
	}
	return rval;
}

/* Calculate LSB */
int
aml_lsb(u_int32_t val)
{
	int n = 31;

	if (!val) return -1;
	if (val & 0x0000FFFF) { val <<= 16; n -= 16; };
	if (val & 0x00FF0000) { val <<= 8;  n -= 8; };
	if (val & 0x0F000000) { val <<= 4;  n -= 4; };
	if (val & 0x30000000) { val <<= 2;  n -= 2; };
	return (val & 0x40000000) ? n-1 : n;
}

/* Calculate MSB */
int
aml_msb(u_int32_t val)
{
	int n=0;

	if (!val) return -1;
	if (val & 0xFFFF0000) { val >>= 16; n += 16; };
	if (val & 0x0000FF00) { val >>= 8;  n += 8; };
	if (val & 0x000000F0) { val >>= 4;  n += 4; };
	if (val & 0x0000000C) { val >>= 2;  n += 2; };
	return (val & 0x00000002) ? n+1 : n;
}

/* Evaluate Math operands */
int64_t
aml_evalmath(u_int16_t opcode, int64_t lhs, int64_t rhs)
{
	switch (opcode) {
	case AMLOP_ADD:
		return (lhs + rhs);
	case AMLOP_SUBTRACT:
		return (lhs - rhs);
	case AMLOP_MULTIPLY:
		return (lhs * rhs);
	case AMLOP_DIVIDE:
		return (lhs / rhs);
	case AMLOP_MOD:
		return (lhs % rhs);
	case AMLOP_SHL:
		return (lhs << rhs);
	case AMLOP_SHR:
		return (lhs >> rhs);
	case AMLOP_AND:
		return (lhs & rhs);
	case AMLOP_NAND:
		return ~(lhs & rhs);
	case AMLOP_OR:
		return (lhs | rhs); 
	case AMLOP_NOR:
		return ~(lhs | rhs); 
	case AMLOP_XOR:
		return (lhs ^ rhs);
	case AMLOP_INCREMENT:
		return (lhs + 1);
	case AMLOP_DECREMENT:
		return (lhs - 1);
	case AMLOP_FINDSETLEFTBIT:
		return aml_msb(lhs);
	case AMLOP_FINDSETRIGHTBIT:
		return aml_lsb(lhs);
	case AMLOP_NOT:
		return ~(lhs);
	}

	return (0);
}

int
aml_strcmp(u_int16_t opcode, const char *lhs, const char *rhs)
{
	return (0);
}

/* Evaluate logical test operands */
int
aml_testlogical(u_int16_t opcode, long lhs, long rhs)
{
	switch(opcode) {
	case AMLOP_LAND:
		return (lhs && rhs);
	case AMLOP_LOR:
		return (lhs || rhs);
	case AMLOP_LNOT:
		return (!lhs);
	case AMLOP_LNOTEQUAL:
		return (lhs != rhs);
	case AMLOP_LLESSEQUAL:
		return (lhs <= rhs);
	case AMLOP_LGREATEREQUAL:
		return (lhs >= rhs);
	case AMLOP_LEQUAL:
		return (lhs == rhs);
	case AMLOP_LGREATER:
		return (lhs > rhs);
	case AMLOP_LLESS:
		return (lhs < rhs);
	}
	return 0;
}

/* Extract opcode from AML bytestream 
 *
 * Some opcodes are multibyte
 * Strings can also be embedded within the stream
 */
u_int8_t *
aml_getopcode(u_int8_t *pos, u_int16_t *opcode)
{
	u_int16_t twocode;

	/* Check for encoded name */
	if (aml_isnamedop(*pos)) {
		*opcode = AMLOP_NAMECHAR;
		return pos;
	}

	*opcode = *(pos++);
	twocode = (*opcode << 8L) + *pos;

	/* Check multi-byte opcodes */
	if (twocode == AMLOP_LNOTEQUAL ||
	    twocode == AMLOP_LLESSEQUAL ||
	    twocode == AMLOP_LGREATEREQUAL ||
	    *opcode == AMLOP_EXTPREFIX) {
		pos++;
		*opcode = twocode;
	}

	return pos;
}

struct aml_optable aml_table[] = {
	/* Simple types */
	{ AMLOP_ZERO,             "Zero",            "!", },
	{ AMLOP_ONE,              "One",             "!",  },
	{ AMLOP_ONES,             "Ones",            "!",   },
	{ AMLOP_BYTEPREFIX,       "Byte",            "b",  },
	{ AMLOP_WORDPREFIX,       "Word",            "w",  },
	{ AMLOP_DWORDPREFIX,      "DWord",           "d",  },
	{ AMLOP_QWORDPREFIX,      "QWord",           "q",  },
	{ AMLOP_REVISION,         "Revision",        "",   },
	{ AMLOP_STRINGPREFIX,     "String",          "s",  },
	{ AMLOP_BUFFER,           "Buffer",          "piB", },

	/* Simple objects */
	{ AMLOP_DEBUG,            "DebugOp",         "",    },
	{ AMLOP_LOCAL0,           "Local0",          "",    },
	{ AMLOP_LOCAL1,           "Local1",          "",    },
	{ AMLOP_LOCAL2,           "Local2",          "",    },
	{ AMLOP_LOCAL3,           "Local3",          "",    },
	{ AMLOP_LOCAL4,           "Local4",          "",    },
	{ AMLOP_LOCAL5,           "Local5",          "",    },
	{ AMLOP_LOCAL6,           "Local6",          "",    },
	{ AMLOP_LOCAL7,           "Local7",          "",    },
	{ AMLOP_ARG0,             "Arg0",            "",    },
	{ AMLOP_ARG1,             "Arg1",            "",    },
	{ AMLOP_ARG2,             "Arg2",            "",    },
	{ AMLOP_ARG3,             "Arg3",            "",    },
	{ AMLOP_ARG4,             "Arg4",            "",    },
	{ AMLOP_ARG5,             "Arg5",            "",    },
	{ AMLOP_ARG6,             "Arg6",            "",    },

	/* Control flow */
	{ AMLOP_IF,               "If",              "piT",  },
	{ AMLOP_ELSE,             "Else",            "pT",   },
	{ AMLOP_WHILE,            "While",           "piT",  },
	{ AMLOP_BREAK,            "Break",           "",     },
	{ AMLOP_CONTINUE,         "Continue",        "",     },
	{ AMLOP_RETURN,           "Return",          "o",     },
	{ AMLOP_FATAL,            "Fatal",           "bdi", },
	{ AMLOP_NOP,              "Nop",             "",    },
	{ AMLOP_BREAKPOINT,       "BreakPoint",      "",    },

	/* Arithmetic operations */
	{ AMLOP_INCREMENT,        "Increment",       "S",     },
	{ AMLOP_DECREMENT,        "Decrement",       "S",     },
	{ AMLOP_ADD,              "Add",             "iir",   },
	{ AMLOP_SUBTRACT,         "Subtract",        "iir",   },
	{ AMLOP_MULTIPLY,         "Multiply",        "iir",   },
	{ AMLOP_DIVIDE,           "Divide",          "iirr",  },
	{ AMLOP_SHL,              "ShiftLeft",       "iir",   },
	{ AMLOP_SHR,              "ShiftRight",      "iir",   },
	{ AMLOP_AND,              "And",             "iir",   },
	{ AMLOP_NAND,             "Nand",            "iir",   },
	{ AMLOP_OR,               "Or",              "iir",   },
	{ AMLOP_NOR,              "Nor",             "iir",   },
	{ AMLOP_XOR,              "Xor",             "iir",   },
	{ AMLOP_NOT,              "Not",             "ir",    },
	{ AMLOP_MOD,              "Mod",             "iir",   },
	{ AMLOP_FINDSETLEFTBIT,   "FindSetLeftBit",  "ir",    },
	{ AMLOP_FINDSETRIGHTBIT,  "FindSetRightBit", "ir",    },

	/* Logical test operations */
	{ AMLOP_LAND,             "LAnd",            "ii",    },
	{ AMLOP_LOR,              "LOr",             "ii",    },
	{ AMLOP_LNOT,             "LNot",            "i",     },
	{ AMLOP_LNOTEQUAL,        "LNotEqual",       "tt",    },
	{ AMLOP_LLESSEQUAL,       "LLessEqual",      "tt",    },
	{ AMLOP_LGREATEREQUAL,    "LGreaterEqual",   "tt",    },
	{ AMLOP_LEQUAL,           "LEqual",          "tt",    },
	{ AMLOP_LGREATER,         "LGreater",        "tt",    },
	{ AMLOP_LLESS,            "LLess",           "tt",    },

	/* Named objects */
	{ AMLOP_EVENT,            "Event",           "N",   },
	{ AMLOP_NAME,             "Name",            "No",  },
	{ AMLOP_MUTEX,            "Mutex",           "Nb",  },
	{ AMLOP_ALIAS,            "Alias",           "Nn",  },
	{ AMLOP_DATAREGION,       "DataRegion",      "Nttt" },
	{ AMLOP_OPREGION,         "OpRegion",        "Nfii" },
	{ AMLOP_SCOPE,            "Scope",           "pNT" },
	{ AMLOP_DEVICE,           "Device",          "pNO" },
	{ AMLOP_POWERRSRC,        "Power Resource",  "pNbwO" },
	{ AMLOP_THERMALZONE,      "ThermalZone",     "pNT" },
	{ AMLOP_METHOD,           "Method",          "pNfT",  },
	{ AMLOP_PROCESSOR,        "Processor",       "pNbdbO", },
	{ AMLOP_FIELD,            "Field",           "pNfF" },
	{ AMLOP_INDEXFIELD,       "IndexField",      "pNnfF" },
	{ AMLOP_BANKFIELD,        "BankField",       "pNnifF" },

	/* Field operations */
	{ AMLOP_CREATEFIELD,      "CreateField",     "tiiN",   },
	{ AMLOP_CREATEQWORDFIELD, "CreateQWordField","tiN",    },
	{ AMLOP_CREATEDWORDFIELD, "CreateDWordField","tiN",    },
	{ AMLOP_CREATEWORDFIELD,  "CreateWordField", "tiN",    },
	{ AMLOP_CREATEBYTEFIELD,  "CreateByteField", "tiN",    },
	{ AMLOP_CREATEBITFIELD,   "CreateBitField",  "tiN",    },

	/* Conversion operations */
	{ AMLOP_TOINTEGER,        "ToInteger",       "tr",     },
	{ AMLOP_TOBUFFER,         "ToBuffer",        "tr",     },
	{ AMLOP_TODECSTRING,      "ToDecString",     "ir",     },
	{ AMLOP_TOHEXSTRING,      "ToHexString",     "ir",     }, 
	{ AMLOP_TOSTRING,         "ToString",        "t",      },
	{ AMLOP_FROMBCD,          "FromBCD",         "ir",     },
	{ AMLOP_TOBCD,            "ToBCD",           "ir",     },
	{ AMLOP_MID,              "Mid",             "tiir",   },

	/* Mutex/Signal operations */
	{ AMLOP_ACQUIRE,          "Acquire",         "Sw",     },
	{ AMLOP_RELEASE,          "Release",         "S",      },
	{ AMLOP_SIGNAL,           "Signal",          "S",      },
	{ AMLOP_WAIT,             "Wait",            "Si",     },
	{ AMLOP_RESET,            "Reset",           "S",      },
 
	{ AMLOP_INDEX,            "Index",           "ttr",    },
	{ AMLOP_PACKAGE,          "Package",         "pbT",    },
	{ AMLOP_VARPACKAGE,       "VarPackage",      "piT",    },
	{ AMLOP_DEREFOF,          "DerefOf",         "t",      },
	{ AMLOP_REFOF,            "RefOf",           "S",      },
	{ AMLOP_CONDREFOF,        "CondRef",         "SS",     },

	{ AMLOP_LOADTABLE,        "LoadTable",       "tttttt" },
	{ AMLOP_STALL,            "Stall",           "i",      },
	{ AMLOP_SLEEP,            "Sleep",           "i",      },
	{ AMLOP_LOAD,             "Load",            "NS" },
	{ AMLOP_UNLOAD,           "Unload",          "S" }, 
	{ AMLOP_STORE,            "Store",           "oS",     },
	{ AMLOP_CONCAT,           "Concat",          "ttr" },
	{ AMLOP_CONCATRES,        "ConcatRes",       "ttr" },
	{ AMLOP_NOTIFY,           "Notify",          "Si" },
	{ AMLOP_SIZEOF,           "Sizeof",          "S",      },
	{ AMLOP_MATCH,            "Match",           "tbibii", },
	{ AMLOP_OBJECTTYPE,       "ObjectType",      "S", },
	{ AMLOP_COPYOBJECT,       "CopyObject",      "tS" },
	{ 0xFFFF }
};

int aml_evalnode(struct acpi_softc *, struct aml_node *, struct aml_value *, 
		 struct aml_value *);
void aml_copyvalue(struct aml_value *, const struct aml_value *);

/* Copy an AML value object */
void aml_copyvalue(struct aml_value *dst, const struct aml_value *src)
{
	dst->type = src->type;
	dst->length = src->length;

	switch (dst->type) {
	case AML_OBJTYPE_INTEGER:
		dst->v_integer = src->v_integer;
		break;
	case AML_OBJTYPE_STRING:
		dst->v_string = src->v_string;
		break;
	case AML_OBJTYPE_BUFFER:
		dst->v_buffer = src->v_buffer;
		break;
	}
}

struct aml_node *childOf(struct aml_node *, int);

struct aml_node *
childOf(struct aml_node *parent, int child)
{
	struct aml_node *node = parent->child;

	while(node && child--) {
		node = node->sibling;
	}
	return node;
}

#define AML_NUM_LOCALS 8
#define AML_INTSTRLEN 16

struct aml_value aml_debug;

void
aml_setnodevalue(struct acpi_softc *sc, struct aml_node *node, const struct aml_value *val)
{
	switch (node->opcode) {
	case AMLOP_DEBUG:
		aml_copyvalue(&aml_debug, val);
		break;

	case AMLOP_ZERO:
	case AMLOP_ONE:
	case AMLOP_ONES:
	case AMLOP_REVISION:
	case AMLOP_BYTEPREFIX:
	case AMLOP_WORDPREFIX:
	case AMLOP_DWORDPREFIX:
	case AMLOP_QWORDPREFIX:
		printf("read-only\n");
		break;

	default:
		aml_copyvalue(&node->value, val);
		break;
	}
}

void
aml_setnodeinteger(struct acpi_softc *sc, struct aml_node *node, int64_t value)
{
	struct aml_value ival;

	aml_setinteger(&ival, 0, value);
	aml_setnodevalue(sc, node, &ival);
}

#if 0
int
aml_callmethod(struct acpi_softc *sc, struct aml_node *parent, const char *method, 
	       int nargs, struct aml_value *args, struct aml_value *result)
{
	struct aml_node *pnode;
	struct aml_value env;

	pnode = aml_findnode(sc, parent, method);
	if (pnode == NULL || pnode->type != AML_METHOD) 
		return (-1);

	if (nargs != AML_METHOD_ARGCOUNT(pnode->flag)) 
		return (-1);

	env.type = AML_OBJTYPE_METHOD;
	env.v_method.argcount = nargs;
	env.v_method.args     = args;
	env.v_method.locals   = malloc(sizeof(struct aml_value) * AML_NUM_LOCALS,
				       M_DEVBUF, M_WAITOK);
	aml_evalnode(sc, pnode, result, &env);
}

int
aml_readfield(struct acpi_softc *sc, const struct aml_value *field, struct aml_value *dest)
{
}

int
aml_writefield(struct acpi_softc *sc, const struct aml_value *field, const struct aml_value *src)
{
}
#endif

int aml_cmpobj(struct acpi_softc *sc, const struct aml_value *lhs,
	       const struct aml_value *rhs)
{
	/* ASSERT: lhs and rhs are of same type */
	switch (lhs->type) {
	case AML_OBJTYPE_INTEGER:
		return (lhs->v_integer - rhs->v_integer);
	case AML_OBJTYPE_STRING:
		return strcmp(lhs->v_string, rhs->v_string);
	default:
		printf("Unknown compare type for cmpobj\n");
		break;
	}

	return (0);
}

int
aml_match(struct acpi_softc *sc, int mtype, const struct aml_value *lhs, 
	  const struct aml_value *rhs)
{
	int rc;

	if (mtype == AML_MATCH_TR)
		return (1);

	if (lhs->type != rhs->type)
		return (0);

	rc = aml_cmpobj(sc, lhs, rhs);
	switch (mtype) {
	case AML_MATCH_EQ:
		return (rc == 0);
	case AML_MATCH_LT:
		return (rc < 0);
	case AML_MATCH_LE:
		return (rc <= 0);
	case AML_MATCH_GE:
		return (rc >= 0);
	case AML_MATCH_GT:
		return (rc > 0);
	}

	return (0);
}

int
aml_evalnode(struct acpi_softc *sc, struct aml_node *node, struct aml_value *result, 
	     struct aml_value *env)
{
	struct  aml_value lhs, rhs, tmp, pkg, op1, op2;
	int64_t iresult, id, idx;
	struct  aml_node *cflow;
	char   *tmpstr;

	if (node == NULL) 
		return (-1);

	cflow = NULL;
	switch (node->opcode) {
	case AMLOP_ZERO:
	case AMLOP_ONE:
	case AMLOP_ONES:
	case AMLOP_BYTEPREFIX:
	case AMLOP_WORDPREFIX:
	case AMLOP_DWORDPREFIX:
	case AMLOP_QWORDPREFIX:
	case AMLOP_STRINGPREFIX:
	case AMLOP_REVISION:
	case AMLOP_BUFFER:
		aml_copyvalue(result, &node->value);
		break;

	case AMLOP_DEBUG:
		aml_copyvalue(result, &aml_debug);
		break;

	case AMLOP_PROCESSOR:
		aml_evalnode(sc, childOf(node, 0), &lhs, env);
		aml_evalnode(sc, childOf(node, 1), &rhs, env);
		aml_evalnode(sc, childOf(node, 2), &tmp, env);
		aml_setprocessor(result, lhs.v_integer, rhs.v_integer, tmp.v_integer);
		break;

	case AMLOP_OPREGION:
		aml_evalnode(sc, childOf(node, 0), &lhs, env);
		aml_evalnode(sc, childOf(node, 1), &rhs, env);
		aml_setopregion(result, node->flag, rhs.v_integer, lhs.v_integer);
		break;

	case AMLOP_IF:
		aml_evalnode(sc, childOf(node, 0), &lhs, env);		
		if (lhs.v_integer) {
			/* Test true, select 'If' block */
			cflow = childOf(node, 1);
		}
		else if (node->sibling->opcode == AMLOP_ELSE) {
			/* Test false, select 'Else' block */
			cflow = node->sibling->child;
		}
		while (cflow) {
			/* Execute all instructions in scope block */
			aml_evalnode(sc, cflow, result, env);
			cflow = cflow->sibling;
		}
		break;

	case AMLOP_WHILE:
		for (;;) {
			if (cflow == NULL) {
				/* Perform While test */
				cflow = childOf(node, 1);
				aml_evalnode(sc, childOf(node, 0), &lhs, env);
				if (lhs.v_integer == 0) 
					break;
			}
			else if (cflow->opcode == AMLOP_BREAK) 
				break;
			else if (cflow->opcode == AMLOP_CONTINUE)
				/* Reset cflow to NULL; restart block */
				cflow = NULL;
			else {
				/* Execute all instructions in scope block */
				aml_evalnode(sc, cflow, result, env);
				cflow = cflow->sibling;
			}
		}
		break;
		
	case AMLOP_RETURN:
		aml_evalnode(sc, childOf(node, 0), result, env);
		break;

	case AMLOP_ARG0:
	case AMLOP_ARG1:
	case AMLOP_ARG2:
	case AMLOP_ARG3:
	case AMLOP_ARG4:
	case AMLOP_ARG5:
	case AMLOP_ARG6:
		id = node->opcode - AMLOP_ARG0;
		if (id < env->v_method.argcount) {
			aml_copyvalue(result, &env->v_method.args[id]);
		}
		break;

	case AMLOP_LOCAL0:
	case AMLOP_LOCAL1:
	case AMLOP_LOCAL2:
	case AMLOP_LOCAL3:
	case AMLOP_LOCAL4:
	case AMLOP_LOCAL5:
	case AMLOP_LOCAL6:
	case AMLOP_LOCAL7:
		id = node->opcode - AMLOP_LOCAL0;
		aml_copyvalue(result, &env->v_method.locals[id]);
		break;

	case AMLOP_PACKAGE:
	case AMLOP_VARPACKAGE:
		break;
		
	case AMLOP_INCREMENT:
	case AMLOP_DECREMENT:
		aml_evalnode(sc, childOf(node, 0), &lhs, env);
		/* ASSERT: lhs.type == AML_OBJTYPE_INTEGER */
		if (lhs.type == AML_OBJTYPE_INTEGER) {
			iresult = aml_evalmath(node->opcode, lhs.v_integer, 0);
			aml_setnodeinteger(sc, childOf(node, 0), iresult);
		}
		break;

	case AMLOP_NOT:
	case AMLOP_FINDSETLEFTBIT:
	case AMLOP_FINDSETRIGHTBIT:
		aml_evalnode(sc, childOf(node, 0), &lhs, env);
		/* ASSERT: lhs.type == AML_OBJTYPE_INTEGER */
		if (lhs.type == AML_OBJTYPE_INTEGER) {
			iresult = aml_evalmath(node->opcode, lhs.v_integer, 0);
			aml_setnodeinteger(sc, childOf(node, 1), iresult);
		}
		break;

	case AMLOP_DIVIDE:
		aml_evalnode(sc, childOf(node, 0), &lhs, env);
		aml_evalnode(sc, childOf(node, 1), &rhs, env);
		if (lhs.type == AML_OBJTYPE_INTEGER && rhs.type == AML_OBJTYPE_INTEGER) {
			/* Set remainder */
			iresult = aml_evalmath(AMLOP_MOD,    lhs.v_integer, rhs.v_integer);
			aml_setnodeinteger(sc, childOf(node, 2), iresult);

			/* Set quotient */
			iresult = aml_evalmath(node->opcode, lhs.v_integer, rhs.v_integer);
			aml_setnodeinteger(sc, childOf(node, 3), iresult);
		}
		break;

	case AMLOP_ADD:
	case AMLOP_SUBTRACT:
	case AMLOP_MULTIPLY:
	case AMLOP_SHL:
	case AMLOP_SHR:
	case AMLOP_AND:
	case AMLOP_NAND:
	case AMLOP_OR:
	case AMLOP_NOR:
	case AMLOP_XOR:
	case AMLOP_MOD:
		aml_evalnode(sc, childOf(node, 0), &lhs, env);
		aml_evalnode(sc, childOf(node, 1), &rhs, env);
		if (lhs.type == AML_OBJTYPE_INTEGER && rhs.type == AML_OBJTYPE_INTEGER) {
			iresult = aml_evalmath(node->opcode, lhs.v_integer, rhs.v_integer);
			aml_setnodeinteger(sc, childOf(node, 2), iresult);
		}
		break;

	case AMLOP_LNOT:
		aml_evalnode(sc, childOf(node, 0), &lhs, env);
		if (lhs.type == AML_OBJTYPE_INTEGER) {
			iresult = aml_testlogical(node->opcode, lhs.v_integer, 0);
			aml_setinteger(result, 0, iresult);
		}
		break;

	case AMLOP_LAND:
	case AMLOP_LOR:
		aml_evalnode(sc, childOf(node, 0), &lhs, env);
		aml_evalnode(sc, childOf(node, 1), &rhs, env);
		if (lhs.type == AML_OBJTYPE_INTEGER && rhs.type == AML_OBJTYPE_INTEGER) {
			iresult = aml_testlogical(node->opcode, lhs.v_integer, rhs.v_integer);
			aml_setinteger(result, 0, iresult);
		}
		break;

	case AMLOP_LEQUAL:
	case AMLOP_LNOTEQUAL:
	case AMLOP_LLESSEQUAL:
	case AMLOP_LGREATEREQUAL:
	case AMLOP_LGREATER:
	case AMLOP_LLESS:
		aml_evalnode(sc, childOf(node, 0), &lhs, env);
		aml_evalnode(sc, childOf(node, 1), &rhs, env);
		if (lhs.type == AML_OBJTYPE_INTEGER && rhs.type == AML_OBJTYPE_INTEGER) {
			iresult = aml_testlogical(node->opcode, lhs.v_integer, rhs.v_integer);
		}
		else if (lhs.type == AML_OBJTYPE_STRING && rhs.type == AML_OBJTYPE_STRING) {
			iresult = aml_strcmp(node->opcode, lhs.v_string, rhs.v_string);
		}
		aml_setinteger(result, 0, iresult);
		break;

	case AMLOP_CREATEFIELD:
		aml_evalnode(sc, childOf(node, 1), &lhs, env);
		aml_evalnode(sc, childOf(node, 2), &rhs, env);
		if (lhs.type == AML_OBJTYPE_INTEGER) {
			aml_setfield(result, lhs.v_integer, rhs.v_integer, childOf(node, 0));
		}
		break;
	case AMLOP_CREATEBITFIELD:
		aml_evalnode(sc, childOf(node, 1), &lhs, env);
		if (lhs.type == AML_OBJTYPE_INTEGER) {
			aml_setfield(result, lhs.v_integer, 1, childOf(node, 0));
		}
		break;
	case AMLOP_CREATEBYTEFIELD:
		aml_evalnode(sc, childOf(node, 1), &lhs, env);
		if (lhs.type == AML_OBJTYPE_INTEGER) {
			aml_setfield(result, lhs.v_integer * 8, 8, childOf(node, 0));
		}
		break;
	case AMLOP_CREATEWORDFIELD:
		aml_evalnode(sc, childOf(node, 1), &lhs, env);
		if (lhs.type == AML_OBJTYPE_INTEGER) {
			aml_setfield(result, lhs.v_integer * 8, 16, childOf(node, 0));
		}
		break;
	case AMLOP_CREATEDWORDFIELD:
		aml_evalnode(sc, childOf(node, 1), &lhs, env);
		if (lhs.type == AML_OBJTYPE_INTEGER) {
			aml_setfield(result, lhs.v_integer * 8, 32, childOf(node, 0));
		}
		break;
	case AMLOP_CREATEQWORDFIELD:
		aml_evalnode(sc, childOf(node, 1), &lhs, env);
		if (lhs.type == AML_OBJTYPE_INTEGER) {
			aml_setfield(result, lhs.v_integer * 8, 64, childOf(node, 0));
		}
		break;

	case AMLOP_TOBCD:
		aml_evalnode(sc, childOf(node, 0), &lhs, env);
		if (lhs.type == AML_OBJTYPE_INTEGER) {
			iresult = aml_dec2bcd(lhs.v_integer);
			aml_setnodeinteger(sc, childOf(node, 1), iresult);
		}
		break;
	case AMLOP_FROMBCD:
		aml_evalnode(sc, childOf(node, 0), &lhs, env);
		if (lhs.type == AML_OBJTYPE_INTEGER) {
			iresult = aml_bcd2dec(lhs.v_integer);
			aml_setnodeinteger(sc, childOf(node, 1), iresult);
		}
		break;
	case AMLOP_TODECSTRING:
		tmpstr = malloc(AML_INTSTRLEN+1, M_DEVBUF, M_WAITOK);
		if (tmpstr != NULL) {
			aml_evalnode(sc, childOf(node, 0), &lhs, env);
			if (lhs.type == AML_OBJTYPE_INTEGER) 
				snprintf(tmpstr, AML_INTSTRLEN, "%d", lhs.v_integer);
		}
		break;
	case AMLOP_TOHEXSTRING:
		tmpstr = malloc(AML_INTSTRLEN+1, M_DEVBUF, M_WAITOK);
		if (tmpstr != NULL) {
			aml_evalnode(sc, childOf(node, 0), &lhs, env);
			if (lhs.type == AML_OBJTYPE_INTEGER) 
				snprintf(tmpstr, AML_INTSTRLEN, "%x", lhs.v_integer);
		}
		break;

	case AMLOP_MID:
		aml_evalnode(sc, childOf(node, 0), &tmp, env);
		aml_evalnode(sc, childOf(node, 1), &lhs, env);
		aml_evalnode(sc, childOf(node, 2), &rhs, env);
		if (tmp.type != AML_OBJTYPE_STRING) 
			return (-1);

		tmpstr = malloc(rhs.v_integer+1, M_DEVBUF, M_WAITOK);
		if (tmpstr != NULL) {
			strncpy(tmpstr, tmp.v_string + lhs.v_integer, rhs.v_integer);
		}
		break;

	case AMLOP_STALL:
		aml_evalnode(sc, childOf(node, 0), &lhs, env);
		dnprintf(50, "aml_stall: %d\n", lhs.v_integer);
		break;
	case AMLOP_SLEEP:
		aml_evalnode(sc, childOf(node, 0), &lhs, env);
		dnprintf(50, "aml_sleep: %d\n", lhs.v_integer);
		break;
	case AMLOP_OBJECTTYPE:
		aml_evalnode(sc, childOf(node, 0), &lhs, env);
		aml_setinteger(result, 1, lhs.type);
		break;

	case AMLOP_NAMECHAR: /* Inline method call */
		cflow = node->sibling;

		/* node = aml_findnode(sc, node->mnem); */
		if (node->opcode == AMLOP_METHOD) {
			/* Arguments are following in the stream */
			lhs.v_method.argcount = AML_METHOD_ARGCOUNT(node->flag);
			lhs.v_method.args = malloc(sizeof(struct aml_value) * lhs.v_method.argcount, 
						   M_DEVBUF, M_WAITOK);
			lhs.v_method.locals = malloc(sizeof(struct aml_value) * AML_NUM_LOCALS,
						     M_DEVBUF, M_WAITOK);
			for (id=0; id<lhs.v_method.argcount; id++) {
				aml_evalnode(sc, cflow, &lhs.v_method.args[id], env);
				cflow = cflow->sibling;
			}

			/* Evaluate method itself */
			aml_evalnode(sc, node, result, &lhs);
			free(lhs.v_method.args, M_DEVBUF);
			free(lhs.v_method.locals, M_DEVBUF);
		}
		break;

	case AMLOP_METHOD:
		for (cflow=childOf(node, 0); cflow; cflow=cflow->sibling) {
			aml_evalnode(sc, cflow, result, env);
		}
		break;

	case AMLOP_CONCAT:
		aml_evalnode(sc, childOf(node, 0), &lhs, env);
		aml_evalnode(sc, childOf(node, 1), &rhs, env);
		break;

	case AMLOP_NOP:
		break;

	case AMLOP_MATCH:
		aml_evalnode(sc, childOf(node, 0), &pkg, env);
		aml_evalnode(sc, childOf(node, 1), &op1, env);
		aml_evalnode(sc, childOf(node, 2), &lhs, env);
		aml_evalnode(sc, childOf(node, 3), &op2, env);
		aml_evalnode(sc, childOf(node, 4), &lhs, env);
		aml_evalnode(sc, childOf(node, 5), &tmp, env);
		if (pkg.type == AML_OBJTYPE_PACKAGE) {
			iresult = -1;
			for (idx=tmp.v_integer; idx < pkg.length; idx++) {
				if (aml_match(sc, op1.v_integer, &pkg.v_package[idx], &lhs) ||
				    aml_match(sc, op2.v_integer, &pkg.v_package[idx], &rhs)) {
					iresult = idx;
					break;
				}
			}
			aml_setinteger(result, 0, iresult);
		}
		break;

	default:
		printf("Unknown eval: %.4x %s\n", node->opcode, node->mnem);
		break;
	}
	return (0);
}

u_int8_t *
aml_parseargs(struct acpi_softc *sc, struct aml_node *node, u_int8_t *pos, 
	      const char *arg)
{
	int len;
	u_int8_t *nxtpos;

	nxtpos = pos;
	while (*arg) {
		switch (*arg) {
		case AML_ARG_FLAG:
			node->flag = *(u_int8_t *)pos;
			if (node->opcode == AMLOP_METHOD) {
				dnprintf(50, " method %s %.2x argcount:%d serialized:%d synclevel:%d\n",
					node->name, node->flag,
					AML_METHOD_ARGCOUNT(node->flag),
					AML_METHOD_SERIALIZED(node->flag),
					AML_METHOD_SYNCLEVEL(node->flag));
			}
			else {
				dnprintf(50, " field %s %.2x access:%d lock:%d update:%d\n",
					node->name, node->flag,
					AML_FIELD_ACCESS(node->flag),
					AML_FIELD_LOCK(node->flag),
					AML_FIELD_UPDATE(node->flag));
			}
			nxtpos = pos+1;
			break;
		case AML_ARG_IMPBYTE:
			/* Implied byte: same as opcode */
			dnprintf(50, " ibyte: %x\n", (int8_t)node->opcode);
			aml_setinteger(&node->value, 1, (int8_t)node->opcode);
			break;
		case AML_ARG_BYTE:
			dnprintf(50, " byte: %x\n", *(u_int8_t *)pos);
			if (node->opcode == AMLOP_BYTEPREFIX) {
				aml_setinteger(&node->value, 1, *(int8_t *)pos);
			}
			nxtpos = pos+1;
			break;
		case AML_ARG_WORD:
			dnprintf(50, " word: %x\n", *(u_int16_t *)pos);
			if (node->opcode == AMLOP_WORDPREFIX) {
				aml_setinteger(&node->value, 2, (int16_t)letoh16(*(u_int16_t *)pos));
			}
			nxtpos = pos+2;
			break;
                case AML_ARG_DWORD:
			dnprintf(50, " dword: %x\n", *(u_int32_t *)pos);
			if (node->opcode == AMLOP_DWORDPREFIX) {
				aml_setinteger(&node->value, 4, (int32_t)letoh32(*(u_int32_t *)pos));
			}
			nxtpos = pos+4;
			break;
		case AML_ARG_QWORD:
			dnprintf(50, " qword: %x\n", *(u_int32_t *)pos);
			if (node->opcode == AMLOP_QWORDPREFIX) {
				aml_setinteger(&node->value, 8, (int64_t)letoh64(*(u_int64_t *)pos));
			}
			nxtpos = pos+8;
			break;
		case AML_ARG_FIELDLIST:
			dnprintf(50, " fieldlist\n");
			nxtpos = node->end;
			break;
		case AML_ARG_BYTELIST:
			dnprintf(50, " bytelist\n");
			aml_setbuffer(&node->value, node->end - pos, pos);
			nxtpos = node->end;
			break;
		case AML_ARG_STRING:
			dnprintf(50, " string: %s\n", pos);
			len = strlen((const char *)pos);
			aml_setstring(&node->value, (const char *)pos);
			nxtpos = pos + len + 1;
			break;
		case AML_ARG_NAMESTRING:
			nxtpos = aml_decodename(pos, &node->name, "name");
			break;
		case AML_ARG_NAMEREF:
			nxtpos = aml_decodename(pos, NULL, "ref");
			break;
		case AML_ARG_OBJLEN:
			nxtpos = aml_decodelength(pos, &len);
			node->end = pos + len;
			break;
		case AML_ARG_INTEGER:
		case AML_ARG_DATAOBJ:
		case AML_ARG_TERMOBJ:
		case AML_ARG_RESULT:
		case AML_ARG_SUPERNAME:
			nxtpos = aml_parse_object(sc, node, pos);
			break;
		case AML_ARG_TERMOBJLIST:
		case AML_ARG_DATAOBJLIST:
			while (nxtpos && nxtpos < node->end) {
				nxtpos = aml_parse_object(sc, node, nxtpos);
			}
			break;
		default:
			printf("Unknown arg: %c\n", *arg);
			break;
		}
		pos = nxtpos;

		arg++;
	}

	return pos;
}

void
aml_addchildnode(struct aml_node *parent, struct aml_node *child)
{
	struct aml_node *psib;

	child->parent = parent;
	child->sibling = NULL;
	for (psib = parent->child; psib; psib = psib->sibling) {
		if (psib->sibling == NULL) {
			psib->sibling = child;
			return;
		}
	}
	parent->child = child;
}

u_int8_t *
aml_parse_object(struct acpi_softc *sc, struct aml_node *parent, u_int8_t *pos)
{
	struct aml_optable *optab = aml_table;
	u_int8_t  *nxtpos;
	struct aml_node *node;

	node = malloc(sizeof(struct aml_node), M_DEVBUF, M_WAITOK);
	if (node == NULL) 
		return pos;
	memset(node, 0, sizeof(struct aml_node));

	/* Get AML Opcode; if it is an embedded name, extract name */
	node->start = pos;
	nxtpos = aml_getopcode(pos, &node->opcode);
	if (node->opcode == AMLOP_NAMECHAR) {
		aml_addchildnode(parent, node);
		dnprintf(50, "opcode: xxxx <name>\n");
		return aml_decodename(pos, &node->mnem, "embed");
	}
	while (optab->opcode != 0xFFFF) {
		if  (optab->opcode == node->opcode) {
			dnprintf(50, "opcode: %.4x = %s\n", node->opcode, optab->mnem);
			aml_addchildnode(parent, node);
			node->mnem = optab->mnem;
			return aml_parseargs(sc, node, nxtpos, optab->args);
		}
		optab++;
	}
	printf("Invalid AML Opcode : %.4x\n", node->opcode);
	free(node, M_DEVBUF);

	return NULL;
}

void
aml_walktree(struct aml_node *node, int depth)
{
	int idx;

	while(node) {
		printf(" %d ", depth);
		for(idx=0; idx<depth; idx++) {
			printf("..");
		}
		printf(" opcode:%.4x  mnem:%s %s ",
		       node->opcode, node->mnem, node->name ? node->name : "");
		switch(node->opcode) {
		case AMLOP_METHOD:
			printf(" argcount:%d serialized:%d synclevel:%d",
			       AML_METHOD_ARGCOUNT(node->flag),
			       AML_METHOD_SERIALIZED(node->flag),
			       AML_METHOD_SYNCLEVEL(node->flag));
			break;
		case AMLOP_FIELD:
		case AMLOP_BANKFIELD:
		case AMLOP_INDEXFIELD:
			dnprintf(50, " access:%d lock:%d update:%d\n",
				AML_FIELD_ACCESS(node->flag),
				AML_FIELD_LOCK(node->flag),
				AML_FIELD_UPDATE(node->flag));
			break;

		case AMLOP_BYTEPREFIX:
			printf("byte: %.2x", node->value.v_integer);
			break;
		case AMLOP_WORDPREFIX:
			printf("word: %.4x", node->value.v_integer);
			break;
		case AMLOP_DWORDPREFIX:
			printf("dword: %.8x", node->value.v_integer);
			break;
		case AMLOP_STRINGPREFIX:
			printf("string: %s", node->value.v_string);
			break;
		}
		printf("\n");
		aml_walktree(node->child, depth+1);

		node = node->sibling;
	}
}

struct aml_node aml_root;

void
aml_walkroot()
{
	aml_walktree(aml_root.child, 0);
}

int
aml_find_node(struct aml_node *node, const char *name, 
	      void (*cbproc)(struct aml_node *, void *arg),
	      void *arg)
{
	while (node) {
		if (node->name && !strcmp(name, node->name)) 
			cbproc(node, arg);
		aml_find_node(node->child, name, cbproc, arg);
		node = node->sibling;
	}
	return (0);
}

const char hext[] = "0123456789ABCDEF";

const char *
aml_eisaid(u_int32_t pid)
{
	static char id[8];

	id[0] = '@' + ((pid >> 2) & 0x1F);
	id[1] = '@' + ((pid << 3) & 0x18) + ((pid >> 13) & 0x7);
	id[2] = '@' + ((pid >> 8) & 0x1F);
	id[3] = hext[(pid >> 20) & 0xF];
	id[4] = hext[(pid >> 16) & 0xF];
	id[5] = hext[(pid >> 28) & 0xF];
	id[6] = hext[(pid >> 24) & 0xF];
	id[7] = 0;

	return id;
}

int
acpi_parse_aml(struct acpi_softc *sc, u_int8_t *start, u_int32_t length)
{
	u_int8_t  *pos, *nxtpos;

	for (pos = start; pos && pos < start+length; pos=nxtpos) {
		nxtpos = aml_parse_object(sc, &aml_root, pos);
	}
	printf(" : parsed %d AML bytes\n", length);

	return (0);
}
