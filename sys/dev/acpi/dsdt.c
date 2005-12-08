/* $OpenBSD: dsdt.c,v 1.6 2005/12/08 00:01:13 jordan Exp $ */
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

struct dsdt_softc {
	struct device		sc_dev;

	bus_space_tag_t		sc_iot;
	bus_space_handle_t	sc_ioh;
};

int	dsdtmatch(struct device *, void *, void *);
void	dsdtattach(struct device *, struct device *, void *);
int	dsdt_parse_aml(struct dsdt_softc *, u_int8_t *, u_int32_t);

struct cfattach dsdt_ca = {
	sizeof(struct dsdt_softc), dsdtmatch, dsdtattach
};

struct cfdriver dsdt_cd = {
	NULL, "dsdt", DV_DULL
};

#ifdef AML_DEBUG
int amldebug=3;
#define dprintf(x...)     do { if (amldebug) printf(x); } while(0)
#define dnprintf(n,x...)  do { if (amldebug > (n)) printf(x); } while(0)
#else
#define dprintf(x...)
#define dnprintf(n,x...)
#endif

int
dsdtmatch(struct device *parent, void *match, void *aux)
{
	struct acpi_attach_args		*aaa = aux;
	struct acpi_table_header	*hdr;

	/* if we do not have a table, it is not us */
	if (aaa->aaa_table == NULL)
		return (0);

	/* if it is an DSDT table, we can attach */
	hdr = (struct acpi_table_header *)aaa->aaa_table;
	if (memcmp(hdr->signature, DSDT_SIG, sizeof(DSDT_SIG) - 1) == 0)
		return (1);

	/* Attach SSDT tables */
	if (memcmp(hdr->signature, SSDT_SIG, sizeof(SSDT_SIG) - 1) == 0)
		return (1);

	return (0);
}

void
dsdtattach(struct device *parent, struct device *self, void *aux)
{
	struct acpi_attach_args	*aa = aux;
	struct dsdt_softc	*sc = (struct dsdt_softc *) self;
	struct acpi_dsdt	*dsdt = (struct acpi_dsdt *)aa->aaa_table;

	dsdt_parse_aml(sc, dsdt->aml, dsdt->hdr_length - sizeof(dsdt->hdr));
}

struct aml_node
{
	struct aml_node *parent;
	struct aml_node *child;
	struct aml_node *sibling;

	u_int16_t   opcode;
	u_int8_t   *start;
	u_int8_t   *end;
	u_int8_t    flag;
	const char *name;
	const char *mnem;
};

struct aml_optable
{
	u_int16_t    opcode;
	const char  *mnem;
	const char  *args;
};

int aml_isnamedop(uint16_t);u_int8_t *aml_decodelength(u_int8_t *, int *);
u_int8_t *aml_decodename(u_int8_t *, const char **);
u_int8_t *aml_getopcode(u_int8_t *, u_int16_t *);
u_int8_t *aml_parseargs(struct dsdt_softc *, struct aml_node *, u_int8_t *, const char *);
u_int8_t *aml_parse_object(struct dsdt_softc *, struct aml_node *, u_int8_t *);
int aml_lsb(uint32_t val);
int aml_msb(uint32_t val);
void aml_addchildnode(struct aml_node *, struct aml_node *);
void aml_walktree(struct aml_node *, int);
void aml_walkroot(void);
int aml_find_node(struct aml_node *, const char *, 
		  void (*)(struct aml_node *, void *),
		  void *);

long aml_evalmath(u_int16_t, long, long);
int  aml_evallogical(u_int16_t, long, long);

#if 0
void aml_eval_opregion(struct dsdt_softc *sc, struct aml_value *result, 
		       struct aml_node *node)
{
	result->type = AML_TYPE_GAS;
	result->v_gas.address_space_id = node->flag;
	result->v_gas.register_bit_width = ..;
	result->v_gas.register_bit_offset = ..;
	result->v_gas.access_size = aml_eval_integer(result, node->child[0]);
	result->v_gas.address = aml_eval_integer(result, node->child[1]);
}
#endif

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

u_int8_t *
aml_decodename(u_int8_t *pos, const char **ref)
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

	dprintf("acpi_name: %s\n", name);
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
aml_isnamedop(uint16_t opcode)
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

/* Calculate LSB */
int aml_lsb(uint32_t val)
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
int aml_msb(uint32_t val)
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
long
aml_evalmath(u_int16_t opcode, long lhs, long rhs)
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

/* Evaluate logical test operands */
int
aml_evallogical(u_int16_t opcode, long lhs, long rhs)
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
aml_getopcode(u_int8_t *pos, uint16_t *opcode)
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
	{ AMLOP_ONES,             "Ones",            "",   },
	{ AMLOP_ZERO,             "Zero",            "", },
	{ AMLOP_ONE,              "One",             "",  },
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
	{ AMLOP_RETURN,           "Return",          "t",     },

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
	{ AMLOP_NAME,             "Name",            "No",  },
	{ AMLOP_SCOPE,            "Scope",           "pNT" },
	{ AMLOP_DATAREGION,       "DataRegion",      "Nttt" },
	{ AMLOP_OPREGION,         "OpRegion",        "Nfii" },
	{ AMLOP_DEVICE,           "Device",          "pNO" },
	{ AMLOP_POWERRSRC,        "Power Resource",  "pNbwO" },
	{ AMLOP_THERMALZONE,      "ThermalZone",     "pNT" },
	{ AMLOP_METHOD,           "Method",          "pNfT",  },
	{ AMLOP_PROCESSOR,        "Processor",       "pNbdbO", },
	{ AMLOP_FIELD,            "Field",           "pNfF" },
	{ AMLOP_INDEXFIELD,       "IndexField",      "pNnbF" },
	{ AMLOP_BANKFIELD,        "BankField",       "pNnibF" },
	{ AMLOP_MUTEX,            "Mutex",           "Nf",  },
	{ AMLOP_EVENT,            "Event",           "N",   },
	{ AMLOP_ALIAS,            "Alias",           "Nn",  },

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
	{ AMLOP_STORE,            "Store",           "tS",     },
	{ AMLOP_CONCAT,           "Concat",          "ttr" },
	{ AMLOP_CONCATRES,        "ConcatRes",       "ttr" },
	{ AMLOP_NOTIFY,           "Notify",          "Si" },
	{ AMLOP_SIZEOF,           "Sizeof",          "S",      },
	{ AMLOP_MATCH,            "Match",           "tbibii", },
	{ AMLOP_OBJECTTYPE,       "ObjectType",      "S", },
	{ AMLOP_COPYOBJECT,       "CopyObject",      "tn" },
	{ 0xFFFF }
};

u_int8_t *
aml_parseargs(struct dsdt_softc *sc, struct aml_node *node, u_int8_t *pos, const char *arg)
{
	int len;
	u_int8_t *nxtpos;

	nxtpos = pos;
	while (*arg) {
		switch (*arg) {
		case AML_ARG_FLAG:
			dprintf("flag: %x\n", *(u_int8_t *)pos);
			node->flag = *(u_int8_t *)pos;
			nxtpos = pos+1;
			break;
		case AML_ARG_BYTE:
			dprintf("byte: %x\n", *(u_int8_t *)pos);
			nxtpos = pos+1;
			break;
		case AML_ARG_WORD:
			dprintf("word: %x\n", *(u_int16_t *)pos);
			nxtpos = pos+2;
			break;
                case AML_ARG_DWORD:
			dprintf("dword: %x\n", *(u_int32_t *)pos);
			nxtpos = pos+4;
			break;
		case AML_ARG_QWORD:
			dprintf("qword: %x\n", *(u_int32_t *)pos);
			nxtpos = pos+8;
			break;
		case AML_ARG_FIELDLIST:
			dprintf("fieldlist\n");
			nxtpos = node->end;
			break;
		case AML_ARG_BYTELIST:
			dprintf("bytelist\n");
			nxtpos = node->end;
			break;
		case AML_ARG_STRING:
			dprintf("string: %s\n", pos);
			len = strlen((const char *)pos);
			nxtpos = pos + len + 1;
			break;
		case AML_ARG_NAMESTRING:
			nxtpos = aml_decodename(pos, &node->name);
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
aml_parse_object(struct dsdt_softc *sc, struct aml_node *parent, u_int8_t *pos)
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
		return aml_decodename(pos, &node->mnem);
	}
	while (optab->opcode != 0xFFFF) {
		if  (optab->opcode == node->opcode) {
			dprintf("opcode: %.4x = %s\n", node->opcode, optab->mnem);
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
		case AMLOP_BYTEPREFIX:
			printf("byte: %.2x", *(u_int8_t *)(node->start+1));
			break;
		case AMLOP_WORDPREFIX:
			printf("word: %.4x", *(u_int16_t *)(node->start+1));
			break;
		case AMLOP_DWORDPREFIX:
			printf("dword: %.4x", *(u_int32_t *)(node->start+1));
			break;
		case AMLOP_STRINGPREFIX:
			printf("string: %s", (const char *)(node->start+1));
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

void foundhid(struct aml_node *, void *);
const char *aml_pnpid(uint32_t pid);

const char hext[] = "0123456789ABCDEF";

const char *
aml_pnpid(uint32_t pid)
{
	static char pnpid[8];

	pnpid[0] = '@' + ((pid >> 2) & 0x1F);
	pnpid[1] = '@' + ((pid << 3) & 0x18) + ((pid >> 13) & 0x7);
	pnpid[2] = '@' + ((pid >> 8) & 0x1F);
	pnpid[3] = hext[(pid >> 20) & 0xF];
	pnpid[4] = hext[(pid >> 16) & 0xF];
	pnpid[5] = hext[(pid >> 28) & 0xF];
	pnpid[6] = hext[(pid >> 24) & 0xF];
	pnpid[7] = 0;

	return pnpid;
}

void
foundhid(struct aml_node *node, void *arg)
{
	const char *dev;

	printf("found hid device: %s ", node->parent->name);
	switch(node->child->opcode) {
	case AMLOP_STRINGPREFIX:
		dev = (const char *)node->child->start+1;
		break;
	case AMLOP_BYTEPREFIX:
		dev = aml_pnpid(*(u_int8_t *)(node->child->start+1));
		break;
	case AMLOP_WORDPREFIX:
		dev = aml_pnpid(*(u_int16_t *)(node->child->start+1));
		break;
	case AMLOP_DWORDPREFIX:
		dev = aml_pnpid(*(u_int32_t *)(node->child->start+1));
		break;
	}
	printf("  device: %s\n", dev);
}

int
dsdt_parse_aml(struct dsdt_softc *sc, u_int8_t *start, u_int32_t length)
{
	u_int8_t  *pos, *nxtpos;

	for (pos = start; pos && pos < start+length; pos=nxtpos) {
		nxtpos = aml_parse_object(sc, &aml_root, pos);
	}
	printf(" : parsed %d AML bytes\n", length);

	return (0);
}
