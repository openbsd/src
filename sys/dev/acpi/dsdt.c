/* $OpenBSD: dsdt.c,v 1.3 2005/12/07 07:46:51 jordan Exp $ */
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
int amldebug=0;
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

#if 0
	/* Attach SSDT tables */
	if (memcmp(hdr->signature, SSDT_SIG, sizeof(SSDT_SIG) - 1) == 0)
		return (1);
#endif

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

struct aml_optable
{
	u_int16_t    opcode;
	const char  *mnem;
	const char  *args;
};

int aml_isnamedop(uint16_t);
u_int8_t *aml_decodelength(u_int8_t *, int *);
u_int8_t *aml_decodename(u_int8_t *);
u_int8_t *aml_getopcode(u_int8_t *, u_int16_t *);
u_int8_t *aml_parseargs(struct dsdt_softc *, u_int8_t *, const char *);
u_int8_t *aml_parse_object(struct dsdt_softc *, u_int8_t *);

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
aml_decodename(u_int8_t *pos)
{
	int count;
	char *name;

	if (*pos == AMLOP_ROOTCHAR) {
		dprintf(" root ");
		pos++;
	}
	while (*pos == AMLOP_PARENTPREFIX) {
		dprintf(" parent ");
		pos++;
	}

	count = 1;
	if (*pos == AMLOP_MULTINAMEPREFIX) {
		dprintf(" multi ");
		count = *(++pos);
		pos++;
	}
	if (*pos == AMLOP_DUALNAMEPREFIX) {
		dprintf(" dual ");
		count = 2;
		pos++;
	}

	name = malloc(count * 4 + 1, M_DEVBUF, M_WAITOK);
	if (name != NULL) {
		memset(name, 0, count * 4 + 1);
		memcpy(name, pos, count * 4);
		dprintf("acpi_name: %s\n", name);
		free(name, M_DEVBUF);
	}
	pos += count*4;

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
	{ AMLOP_WHILE,            "While",           "ptT",  },
	{ AMLOP_BREAK,            "Break",           "",     },
	{ AMLOP_CONTINUE,         "Continue",        "",     },
	{ AMLOP_RETURN,           "Return",          "t",     },

	{ AMLOP_FATAL,            "Fatal",           "bdi", },
	{ AMLOP_NOP,              "Nop",             "",    },
	{ AMLOP_BREAKPOINT,       "BreakPoint",      "",    },

	/* Arithmetic operations */
	{ AMLOP_INCREMENT,        "Increment",       "v",     },
	{ AMLOP_DECREMENT,        "Decrement",       "v",     },
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
	{ AMLOP_METHOD,           "Method",          "pNmT",  },
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
	{ AMLOP_ACQUIRE,          "Acquire",         "vw",     },
	{ AMLOP_RELEASE,          "Release",         "v",      },
	{ AMLOP_SIGNAL,           "Signal",          "v",      },
	{ AMLOP_WAIT,             "Wait",            "vi",     },
	{ AMLOP_RESET,            "Reset",           "v",      },
 
	{ AMLOP_INDEX,            "Index",           "ttr",    },
	{ AMLOP_PACKAGE,          "Package",         "pfT",    },
	{ AMLOP_VARPACKAGE,       "VarPackage",      "piT",    },
	{ AMLOP_DEREFOF,          "DerefOf",         "t",      },
	{ AMLOP_REFOF,            "RefOf",           "v",      },
	{ AMLOP_CONDREFOF,        "CondRef",         "vv",     },

	{ AMLOP_LOADTABLE,        "LoadTable",       "tttttt" },
	{ AMLOP_STALL,            "Stall",           "i",      },
	{ AMLOP_SLEEP,            "Sleep",           "i",      },
	{ AMLOP_LOAD,             "Load",            "Nv" },
	{ AMLOP_UNLOAD,           "Unload",          "v" }, 
	{ AMLOP_STORE,            "Store",           "tv",     },
	{ AMLOP_CONCAT,           "Concat",          "ttr" },
	{ AMLOP_CONCATRES,        "ConcatRes",       "ttr" },
	{ AMLOP_NOTIFY,           "Notify",          "vi" },
	{ AMLOP_SIZEOF,           "Sizeof",          "v",      },
	{ AMLOP_MATCH,            "Match",           "tbibii", },
	{ AMLOP_OBJECTTYPE,       "ObjectType",      "v", },
	{ AMLOP_COPYOBJECT,       "CopyObject",      "tn" },
	{ 0xFFFF }
};

#if 0
u_int8_t *aml_createinteger(uint8_t *pos, int len)
{
	return pos+len;
}
#endif

u_int8_t *
aml_parseargs(struct dsdt_softc *sc, u_int8_t *pos, const char *arg)
{
	int len;
	u_int8_t *nxtpos, *endpos;

	endpos = NULL;
	nxtpos = pos;
	while (*arg) {
		switch (*arg) {
		case AML_ARG_BYTE:
		case AML_ARG_FIELDFLAG:
		case AML_ARG_METHODFLAG:
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
		case AML_ARG_BYTELIST:
			dprintf("bytelist\n");
			nxtpos = endpos;
			break;
		case AML_ARG_STRING:
			dprintf("string: %s\n", pos);
			len = strlen((const char *)pos);
			nxtpos = pos + len + 1;
			break;
		case AML_ARG_NAMESTRING:
			dprintf("getting name..\n");
			nxtpos = aml_decodename(pos);
			break;
		case AML_ARG_OBJLEN:
			nxtpos = aml_decodelength(pos, &len);
			endpos = pos + len;
			break;
		case AML_ARG_DATAOBJ:
		case AML_ARG_INTEGER:
		case AML_ARG_TERMOBJ:
		case AML_ARG_RESULT:
			nxtpos = aml_parse_object(sc, pos);
			break;
		case AML_ARG_TERMOBJLIST:
		case AML_ARG_DATAOBJLIST:
			while (nxtpos && nxtpos < endpos) {
				nxtpos = aml_parse_object(sc, nxtpos);
			}
			break;
		default:
			dprintf("Unknown arg: %c\n", *arg);
			break;
		}
		pos = nxtpos;

		arg++;
	}
	return pos;
}

u_int8_t *
aml_parse_object(struct dsdt_softc *sc, u_int8_t *pos)
{
	struct aml_optable *optab = aml_table;
	u_int8_t  *nxtpos;
	u_int16_t  opcode;

	/* Get AML Opcode; if it is an embedded name, extract name */
	nxtpos = aml_getopcode(pos, &opcode);
	if (opcode == AMLOP_NAMECHAR) {
		return aml_decodename(pos);
	}
	while (optab->opcode != 0xFFFF) {
		if  (optab->opcode == opcode) {
			dprintf("opcode: %.4x = %s\n", opcode, optab->mnem);
			return aml_parseargs(sc, nxtpos, optab->args);
		}
		optab++;
	}
	printf("Invalid AML Opcode : %.4x\n", opcode);
	return NULL;
}

int
dsdt_parse_aml(struct dsdt_softc *sc, u_int8_t *start, u_int32_t length)
{
	u_int8_t  *pos, *nxtpos;

	for (pos = start; pos && pos < start+length; pos=nxtpos) {
		nxtpos = aml_parse_object(sc, pos);
	}
	printf(" : parsed %d AML bytes\n", length);
	return (0);
}
