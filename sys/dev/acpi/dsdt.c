/* $OpenBSD: dsdt.c,v 1.18 2006/01/20 20:20:28 jordan Exp $ */
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

struct aml_optable
{
	u_int16_t    opcode;
	const char  *mnem;
	const char  *args;
};

struct acpi_stack
{
	struct aml_value   *locals;
	struct aml_value   *args;
	struct aml_value    result;
	struct acpi_stack  *next;
};

struct acpi_context
{
	struct acpi_softc *sc;
	struct acpi_stack *stack;

	u_int8_t   *pos;
};

#ifdef ACPI_DEBUG
const char *opregion(int id);

const char *
opregion(int id)
{
	switch(id) {
	case 0: return "SystemMemory";
	case 1: return "SystemIO";
	case 2: return "PCIConfig";
	case 3: return "Embedded";
	case 4: return "SMBus";
	case 5: return "CMOS";
	case 6: return "PCIBAR";
	}
	return "";
}
#endif


int aml_parse_length(struct acpi_context *);
int aml_parse_fieldlist(struct acpi_context *, struct aml_node *);
int aml_parse_objlist(struct acpi_context *, struct aml_node *);
int aml_parse_args(struct acpi_context *, struct aml_node *, const char *);
u_int64_t   aml_parse_int(struct acpi_context *, int);
const char *aml_parse_string(struct acpi_context *);
const char *aml_parse_name(struct acpi_context *, const char *);
struct aml_node *aml_parse_object(struct acpi_context *, struct aml_node *);
struct aml_node *aml_preparse(struct acpi_context *, struct aml_node *, struct aml_optable *);

int aml_isnamedop(u_int16_t);
u_int16_t aml_getopcode(struct acpi_context *);

void aml_shownode(struct aml_node *);

u_int64_t  aml_bcd2dec(u_int64_t);
u_int64_t  aml_dec2bcd(u_int64_t);

int aml_lsb(u_int64_t val);
int aml_msb(u_int64_t val);

void aml_walkroot(void);
void aml_addchildnode(struct aml_node *, struct aml_node *);
struct aml_node *aml_create_node(struct acpi_context *, struct aml_node *, int opcode, const char *mnem);
struct aml_node *aml_find_name(struct acpi_softc *, struct aml_node *, const char *);

int64_t aml_str2int(const char *, int, int);
int64_t aml_evalmath(u_int16_t, int64_t, int64_t);
int  aml_testlogical(u_int16_t, long, long);
int  aml_strcmp(u_int16_t opcode, const char *lhs, const char *rhs);

int  aml_eval_object(struct acpi_softc *, struct aml_node *, struct aml_value *, struct aml_value *);

int64_t _aml_evalint(struct acpi_context *, struct aml_node *);
struct aml_value *aml_getnodevalue(struct acpi_context *, struct aml_node *);
struct aml_value *_aml_evalref(struct acpi_context *, struct aml_node *);
struct aml_value *aml_evalnode(struct acpi_context *, struct aml_node *);
struct aml_value *_aml_setnodevalue(struct acpi_context *, struct aml_node *, struct aml_value *, uint64_t);

int aml_match(int64_t, int, int64_t);

struct aml_node *childOf(struct aml_node *, int);

int  aml_tstbit(const u_int8_t *, int);
void aml_setbit(u_int8_t *, int, int);
void aml_bufcpy(u_int8_t *, int, const u_int8_t *, int, int);

void aml_create_bufferunit(struct acpi_context *, struct aml_node *, int, int);
void aml_create_fieldunit(struct acpi_context *, struct aml_node *, const char *, int, int, int);
struct aml_value *aml_bufferio(struct acpi_context *ctx, struct aml_value *pfield, struct aml_value *rhs);
struct aml_value *aml_fieldio(struct acpi_context *ctx, struct aml_value *pfield, struct aml_value *rhs);

void *acpi_os_allocmem(size_t);
void  acpi_os_freemem(void *);

void aml_addchildnode(struct aml_node *, struct aml_node *);

struct aml_node aml_root;

void *
acpi_os_allocmem(size_t size)
{
	return malloc(size, M_DEVBUF, M_WAITOK);
}

void
acpi_os_freemem(void *ptr)
{
	free(ptr, M_DEVBUF);
}

/* Bit mangling code */
int
aml_tstbit(const u_int8_t *pb, int bit)
{
	pb += aml_bytepos(bit);
	return (*pb & aml_bitmask(bit));
}

void
aml_setbit(u_int8_t *pb, int bit, int val)
{
	pb += aml_bytepos(bit);
	if (val) {
		*pb |= aml_bitmask(bit);
	}
	else {
		*pb &= ~aml_bitmask(bit);
	}
}

#define aml_ipaddr(n) ((n)-aml_root.start)

#define AML_NUM_LOCALS 8
#define AML_INTSTRLEN 16
#define AML_NAMESEG_LEN 4

struct aml_node *_aml_searchname(struct aml_node *list, const char *name);
struct aml_node *_aml_findname(struct aml_node *root, const char *name);

struct aml_node *_aml_searchname(struct aml_node *list, const char *name)
{
	if (list->opcode == AMLOP_FIELD ||
	    list->opcode == AMLOP_INDEXFIELD ||
	    list->opcode == AMLOP_BANKFIELD) {
		dnprintf(50, "search field objects\n");
		list = list->child;
	}
	while (list != NULL) {
		if (list->name && !strncmp(list->name, name, AML_NAMESEG_LEN)) {
			return list;
		}
	}
	return NULL;
}

struct aml_node *_aml_findname(struct aml_node *root, const char *name)
{
	struct aml_node *node;

	if (root == NULL) {
		root = &aml_root;
	}
	if (*name == AMLOP_ROOTCHAR) {
		name++;
		root = &aml_root;
	}
	while (*name == AMLOP_PARENTPREFIX) {
		name++;
		root = root->parent;
		if (root == NULL) {
			dnprintf(50, "_aml_find_node: Bad parent!!\n");
			return NULL;
		}
	}
	
	/* Ok... name is parsed.. now search all siblings of parent */
	for (node=NULL; *name && root && !node;) {
		if (*name == '.') name++;
		if ((node = _aml_searchname(root->child, name)) == NULL) {
			/* search parent */
			root = root->parent;
		}
		else if (name[AML_NAMESEG_LEN] == '\0') {
			/* found it */
			return node;
		}
		else {
			root = node;
			name += AML_NAMESEG_LEN;
		}
	}
	return node;
}

/* Allocate dynamic AML value
 *   type : Type of object to allocate (AML_OBJTYPE_XXXX)
 *   ival : Integer value (action depends on type)
 *   bval : Buffer value (action depends on type)
 *   lbl  : Debugging label
 */
struct aml_value *
aml_allocvalue(int type, int64_t ival, const void *bval, 
	       const char *lbl)
{
	struct aml_value *rv;

	rv = (struct aml_value *)acpi_os_allocmem(sizeof(struct aml_value));
	if (rv == NULL)
		return NULL;

	memset(rv, 0, sizeof(struct aml_value));
	rv->type = type;
	rv->dynamic = 1;
	rv->refcnt = 1;

	switch (type) {
	case AML_OBJTYPE_INTEGER:
		rv->v_integer = ival;
		break;
	case AML_OBJTYPE_NAMEREF:
		rv->v_nameref.name = bval;
		break;
	case AML_OBJTYPE_STRING:
		/* Allocate string: if pointer valid, copy data */
		rv->length = ival;
		rv->v_string = NULL;
		if (ival) {
			rv->v_string = acpi_os_allocmem(ival+1);
			memset(rv->v_string, 0, ival+1);
			if (bval) {
				strncpy(rv->v_string, bval, ival);
			}
		}
		break;
	case AML_OBJTYPE_BUFFER:
		/* Allocate buffer: if pointer valid, copy data */
		rv->length = ival;
		rv->v_buffer = NULL;
		if (ival) {
			rv->v_buffer = acpi_os_allocmem(ival);
			memset(rv->v_buffer, 0, ival);
			if (bval) {
				memcpy(rv->v_buffer, bval, ival);
			}
		}
		break;
	case AML_OBJTYPE_PACKAGE:
		/* Allocate package pointers */
		rv->length = ival;
		rv->v_package = (struct aml_value **)acpi_os_allocmem(rv->length * sizeof(struct aml_value *));
		memset(rv->v_package, 0, rv->length * sizeof(struct aml_value *));
		break;
	case AML_OBJTYPE_METHOD:
		/* Allocate method stack */
		rv->v_method.locals = (struct aml_value *)acpi_os_allocmem(8 * sizeof(struct aml_value));
		rv->v_method.args   = (struct aml_value *)acpi_os_allocmem(ival * sizeof(struct aml_value));
		memset(rv->v_method.locals, 0, 8 * sizeof(struct aml_value));
		memset(rv->v_method.args, 0, ival * sizeof(struct aml_value));
		break;
	case AML_OBJTYPE_OBJREF:
		rv->v_objref.index = ival;
		rv->v_objref.ref = (struct aml_node *)bval;
		break;
	case AML_OBJTYPE_MUTEX:
		rv->v_integer = ival;
		break;
	}
	return rv;
}

struct aml_value *
aml_allocint(uint64_t ival)
{
	return aml_allocvalue(AML_OBJTYPE_INTEGER, ival, NULL, "integer");
}

struct aml_value *
aml_allocstr(const char *str)
{
	return aml_allocvalue(AML_OBJTYPE_STRING, strlen(str), str, "string");
}

int 
aml_freevalue(struct aml_value *v)
{
	int idx;

	/* XXX: fix this ref count */
	return (0);

	/* Don't free static values */
	if (v == NULL || !v->dynamic)
		return (0);

	dnprintf(50, "freeing value : %x\n", v->type);
	switch (v->type) {
	case AML_OBJTYPE_STRING:
		if (v->v_string) {
			acpi_os_freemem((void *)v->v_string);
			v->v_string = NULL;
		}
		break;
	case AML_OBJTYPE_BUFFER:
		if (v->v_buffer) {
			acpi_os_freemem(v->v_buffer);
			v->v_buffer = NULL;
		}
		break;
	case AML_OBJTYPE_PACKAGE:
		for (idx=0; idx<v->length; idx++) {
			aml_freevalue(v->v_package[idx]);
		}
		acpi_os_freemem(v->v_package);
		v->v_package = NULL;
		break;
	case AML_OBJTYPE_METHOD:
		break;
	}
	v->type = 0;
	acpi_os_freemem(v);

	return (1);
}


void
aml_showvalue(struct aml_value *value)
{
	int idx;

	if (value == NULL)
		return;
	switch (value->type) {
	case AML_OBJTYPE_INTEGER:
		dnprintf(50, "integer: %x\n", value->v_integer);
		break;
	case AML_OBJTYPE_STRING:
		dnprintf(50, "string: %s\n", value->v_string);
		break;
	case AML_OBJTYPE_BUFFER:
		dnprintf(50, "buffer: %d {\n", value->length);
		for (idx=0; idx<value->length; idx++) {
			dnprintf(50, "%s0x%.2x", (idx ? "," : ""), value->v_buffer[idx]);
		}
		dnprintf(50, "}\n");
		break;
	case AML_OBJTYPE_PACKAGE:
		dnprintf(50, "package: %d {\n", value->length);
		for (idx=0; idx<value->length; idx++)
			aml_showvalue(value->v_package[idx]);
		dnprintf(50, "}\n");
		break;
	case AML_OBJTYPE_DEBUGOBJ:
		dnprintf(50, "debug");
		break;
	case AML_OBJTYPE_DEVICE:
#if 0 
		xxx
			dnprintf(50, "device: %s", val->v_device->name);
#endif
		break;
	case AML_OBJTYPE_PROCESSOR:
		dnprintf(50, "cpu: %x,%x,%x\n",
			 value->v_processor.proc_id,
			 value->v_processor.proc_addr,
			 value->v_processor.proc_len);
		break;
	case AML_OBJTYPE_FIELDUNIT:
		dnprintf(50, "field: %.4x %x,%x\n",
			 value->v_field.ftype,
			 value->v_field.bitpos,
			 value->v_field.bitlen);
		break;
	case AML_OBJTYPE_BUFFERFIELD:
		dnprintf(50, "bufferfield: %.4x %x,%x\n", 
			 value->v_field.ftype,
			 value->v_field.bitpos,
			 value->v_field.bitlen);
		break;
	case AML_OBJTYPE_OPREGION:
		dnprintf(50, "opregion: %s,0x%x,0x%x\n",
			 opregion(value->v_opregion.iospace),
			 value->v_opregion.iobase,
			 value->v_opregion.iolen);
		break;
	default:
		printf("unknown: %d\n", value->type);
		break;
	}
}

int
aml_comparevalue(int opcode, const struct aml_value *lhs, const struct aml_value *rhs)
{
	if (lhs->type != rhs->type) {
		dnprintf(50, "aml_compare: type mismatch: %x,%x\n",
			 lhs->type, rhs->type);
		return (0);
	}
	switch (lhs->type) {
	case AML_OBJTYPE_INTEGER:
		return aml_testlogical(opcode, lhs->v_integer, rhs->v_integer);
	case AML_OBJTYPE_STRING:
	case AML_OBJTYPE_BUFFER:
		/* XXX: implement buffer/string compare */
		break;
	}
	return (0);
}

struct aml_value *
aml_copyvalue(const struct aml_value *rhs)
{
	struct aml_value *rv;
	int idx;

	switch (rhs->type) {
	case AML_OBJTYPE_INTEGER:
		return aml_allocint(rhs->v_integer);
	case AML_OBJTYPE_STRING:
		return aml_allocstr(rhs->v_string);
	case AML_OBJTYPE_BUFFER:
		return aml_allocvalue(rhs->type, rhs->length, rhs->v_buffer, "");
	case AML_OBJTYPE_PACKAGE:
		rv = aml_allocvalue(rhs->type, rhs->length, NULL, "");
		for (idx=0; idx<rv->length; idx++) {
			rv->v_package[idx] = aml_copyvalue(rhs->v_package[idx]);
		}
		break;
	}
	return NULL;
}

/* 
 * AML Parsing routines
 */
const char *
aml_parse_string(struct acpi_context *ctx)
{
	const char *str = ctx->pos;

	ctx->pos += strlen(str)+1;
	return str;
}

/* Read value from AML bytestream */
uint64_t
aml_parse_int(struct acpi_context *ctx, int size)
{
	u_int8_t *pc = ctx->pos;

	ctx->pos += size;
	switch (size) {
	case 1:
		return *(u_int8_t *)pc;
	case 2:
		return *(u_int16_t *)pc;
	case 4:
		return *(u_int32_t *)pc;
	case 8:
		return *(u_int64_t *)pc;
	}

	return (0);
}

/* Decode AML Package length
 * Upper two bits of first byte denote length
 *   0x00 = length is in lower 6 bits
 *   0x40 = length is lower 4 bits + 1 byte
 *   0x80 = length is lower 4 bits + 2 bytes
 *   0xC0 = length is lower 4 bits + 3 bytes
 */
int
aml_parse_length(struct acpi_context *ctx)
{
	u_int8_t lcode;
	int ival;

	lcode = aml_parse_int(ctx, 1);
	if (lcode <= 0x3F) {
		return lcode;
	}

	ival = lcode & 0xF;
	if (lcode >= 0x40)  ival |= aml_parse_int(ctx, 1) << 4;
	if (lcode >= 0x80)  ival |= aml_parse_int(ctx, 1) << 12;
	if (lcode >= 0xC0)  ival |= aml_parse_int(ctx, 1) << 20;

	return ival;
}

int
aml_parse_fieldlist(struct acpi_context *ctx, struct aml_node *parent)
{
	u_int16_t flag, type, attr;
	int bitpos, bitlen;
	const char *name;

	bitpos = 0;
	flag = parent->flag;

	/* Format of flag:
	 *   bits 00 - 03 : access mode
	 *   bits 04 - 04 : lock mode
	 *   bits 05 - 08 : update mode
	 *   bits 08 - 15 : attribute
	 */
	dnprintf(50, "-- parse_fieldlist\n");
	while (ctx->pos < parent->end) {
		bitlen = 0;
		switch (aml_parse_int(ctx, 1)) {
		case 0x00: /* reserved */
			bitlen = aml_parse_length(ctx);
			break;
		case 0x01: /* access field */
			type = aml_parse_int(ctx, 1);
			attr = aml_parse_int(ctx, 1);
			dnprintf(50, "  type=%.2x  attr=%.2x\n", type, attr);
			flag = AML_FIELD_SETATTR(flag, type, attr);
			break;
		default: /* named field */
			--ctx->pos;
			name = aml_parse_name(ctx, "field");
			bitlen = aml_parse_length(ctx);
			aml_create_fieldunit(ctx, parent, name, bitpos, bitlen, flag);
		}
		bitpos += bitlen;
	}
	if (ctx->pos != parent->end) {
		dnprintf(50, "parse_fieldlist: invalid end!\n");
		ctx->pos = parent->end;
		return (1);
	}
	return (0);
}

/* Decode AML Namestring from stream */
const char *
aml_parse_name(struct acpi_context *ctx, const char *lbl)
{
	int count, pfxlen;
	char *name, *pn;
	u_int8_t *base;

	pfxlen = 0;
	if (ctx->pos[pfxlen] == AMLOP_ROOTCHAR) {
		pfxlen++;
	}
	while (ctx->pos[pfxlen] == AMLOP_PARENTPREFIX) {
		pfxlen++;
	}

	switch (ctx->pos[pfxlen]) {
	case 0x00:
		count = 0;
		base  = ctx->pos + pfxlen + 1;
		break;
	case AMLOP_MULTINAMEPREFIX:
		count = ctx->pos[pfxlen+1];
		base = ctx->pos + pfxlen + 2;
		break;
	case AMLOP_DUALNAMEPREFIX:
		count = 2;
		base  = ctx->pos + pfxlen + 1;
		break;
	default:
		count = 1;
		base  = ctx->pos + pfxlen;
		break;
	}

	name = acpi_os_allocmem(pfxlen + count * 5);
	pn = name;

	while (pfxlen--) 
		*(pn++) = *(ctx->pos++);
			    
	/* Copy name segments in chunks of 4 bytes */
	while (count--) {
		memcpy(pn, base, 4);
		if (count) {
			*(pn + 4) = '.';
			pn++;
		}
		pn += 4;
		base += 4;
	}
	*pn = 0;

	dnprintf(50, " acpi_name (%s): %s\n", lbl, name);

	ctx->pos = base;

	return name;
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

/*
 * Math eval routines
 */
u_int64_t
aml_bcd2dec(u_int64_t val)
{
	u_int64_t rval;
	int n, pos;

	pos = 1;
	for (rval = 0; val; val >>= 4) {
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
	int n, pos;

	pos = 0;
	for (rval = 0; val; val /= 10) {
		n = (val % 10);

		rval += (n << pos);
		pos += 4;
	}
	return rval;
}

/* Calculate LSB */
int
aml_lsb(u_int64_t val)
{
	int lsb;

	if (val == 0) 
		return (0);

	for (lsb = 1; !(val & 0x1); lsb++)
		val >>= 1;
	return lsb;
}

/* Calculate MSB */
int
aml_msb(u_int64_t val)
{
	int msb;

	if (val == 0) 
		return (0);

	for (msb = 1; val != 0x1; msb++)
		val >>= 1;
	return msb;
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

struct aml_optable aml_table[] = {
	/* Simple types */
	{ AMLOP_ZERO,             "Zero",            "!"  },
	{ AMLOP_ONE,              "One",             "!"  },
	{ AMLOP_ONES,             "Ones",            "!"  },
	{ AMLOP_BYTEPREFIX,       "Byte",            "b"  },
	{ AMLOP_WORDPREFIX,       "Word",            "w"  },
	{ AMLOP_DWORDPREFIX,      "DWord",           "d"  },
	{ AMLOP_QWORDPREFIX,      "QWord",           "q"  },
	{ AMLOP_REVISION,         "Revision",        ""   },
	{ AMLOP_STRINGPREFIX,     "String",          "s"  },
	{ AMLOP_DEBUG,            "DebugOp",         "",  },
	{ AMLOP_BUFFER,           "Buffer",          "piB" },
	{ AMLOP_PACKAGE,          "Package",         "pbT" },
	{ AMLOP_VARPACKAGE,       "VarPackage",      "piT" },

	/* Simple objects */
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
	{ AMLOP_INCREMENT,        "Increment",       "t",     },
	{ AMLOP_DECREMENT,        "Decrement",       "t",     },
	{ AMLOP_ADD,              "Add",             "iit",   },
	{ AMLOP_SUBTRACT,         "Subtract",        "iit",   },
	{ AMLOP_MULTIPLY,         "Multiply",        "iit",   },
	{ AMLOP_DIVIDE,           "Divide",          "iitt",  },
	{ AMLOP_SHL,              "ShiftLeft",       "iit",   },
	{ AMLOP_SHR,              "ShiftRight",      "iit",   },
	{ AMLOP_AND,              "And",             "iit",   },
	{ AMLOP_NAND,             "Nand",            "iit",   },
	{ AMLOP_OR,               "Or",              "iit",   },
	{ AMLOP_NOR,              "Nor",             "iit",   },
	{ AMLOP_XOR,              "Xor",             "iit",   },
	{ AMLOP_NOT,              "Not",             "it",    },
	{ AMLOP_MOD,              "Mod",             "iit",   },
	{ AMLOP_FINDSETLEFTBIT,   "FindSetLeftBit",  "it",    },
	{ AMLOP_FINDSETRIGHTBIT,  "FindSetRightBit", "it",    },

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
	{ AMLOP_NAMECHAR,         "NameRef",         "n" },
	{ AMLOP_ALIAS,            "Alias",           "nN",  },
	{ AMLOP_NAME,             "Name",            "Nt",  },
	{ AMLOP_EVENT,            "Event",           "N",   },
	{ AMLOP_MUTEX,            "Mutex",           "Nb",  },
	{ AMLOP_DATAREGION,       "DataRegion",      "Nttt" },
	{ AMLOP_OPREGION,         "OpRegion",        "Nbii" },
	{ AMLOP_SCOPE,            "Scope",           "pNT" },
	{ AMLOP_DEVICE,           "Device",          "pNT" },
	{ AMLOP_POWERRSRC,        "Power Resource",  "pNbwT" },
	{ AMLOP_THERMALZONE,      "ThermalZone",     "pNT" },
	{ AMLOP_PROCESSOR,        "Processor",       "pNbdbT", },
	{ AMLOP_METHOD,           "Method",          "pNfM",  },

	/* Field operations */
	{ AMLOP_FIELD,            "Field",           "pnfF" },
	{ AMLOP_INDEXFIELD,       "IndexField",      "pntfF" },
	{ AMLOP_BANKFIELD,        "BankField",       "pnnifF" },
	{ AMLOP_CREATEFIELD,      "CreateField",     "tiiN",   },
	{ AMLOP_CREATEQWORDFIELD, "CreateQWordField","tiN",    },
	{ AMLOP_CREATEDWORDFIELD, "CreateDWordField","tiN",    },
	{ AMLOP_CREATEWORDFIELD,  "CreateWordField", "tiN",    },
	{ AMLOP_CREATEBYTEFIELD,  "CreateByteField", "tiN",    },
	{ AMLOP_CREATEBITFIELD,   "CreateBitField",  "tiN",    },

	/* Conversion operations */
	{ AMLOP_TOINTEGER,        "ToInteger",       "tt",     },
	{ AMLOP_TOBUFFER,         "ToBuffer",        "tt",     },
	{ AMLOP_TODECSTRING,      "ToDecString",     "it",     },
	{ AMLOP_TOHEXSTRING,      "ToHexString",     "it",     }, 
	{ AMLOP_TOSTRING,         "ToString",        "t",      },
	{ AMLOP_FROMBCD,          "FromBCD",         "it",     },
	{ AMLOP_TOBCD,            "ToBCD",           "it",     },
	{ AMLOP_MID,              "Mid",             "tiit",   },

	/* Mutex/Signal operations */
	{ AMLOP_ACQUIRE,          "Acquire",         "tw",     },
	{ AMLOP_RELEASE,          "Release",         "t",      },
	{ AMLOP_SIGNAL,           "Signal",          "t",      },
	{ AMLOP_WAIT,             "Wait",            "ti",     },
	{ AMLOP_RESET,            "Reset",           "t",      },
 
	{ AMLOP_INDEX,            "Index",           "tit",    },
	{ AMLOP_DEREFOF,          "DerefOf",         "t",      },
	{ AMLOP_REFOF,            "RefOf",           "t",      },
	{ AMLOP_CONDREFOF,        "CondRef",         "tt",     },

	{ AMLOP_LOADTABLE,        "LoadTable",       "tttttt" },
	{ AMLOP_STALL,            "Stall",           "i",      },
	{ AMLOP_SLEEP,            "Sleep",           "i",      },
	{ AMLOP_LOAD,             "Load",            "nt" },
	{ AMLOP_UNLOAD,           "Unload",          "t" }, 
	{ AMLOP_STORE,            "Store",           "tt",     },
	{ AMLOP_CONCAT,           "Concat",          "ttt" },
	{ AMLOP_CONCATRES,        "ConcatRes",       "ttt" },
	{ AMLOP_NOTIFY,           "Notify",          "ti" },
	{ AMLOP_SIZEOF,           "Sizeof",          "t",      },
	{ AMLOP_MATCH,            "Match",           "tbibii", },
	{ AMLOP_OBJECTTYPE,       "ObjectType",      "t", },
	{ AMLOP_COPYOBJECT,       "CopyObject",      "tt" },
	{ 0xFFFF }
};

/* Extract opcode from AML bytestream 
 *
 * Some opcodes are multibyte
 * Name strings can also be embedded within the stream
 */
u_int16_t
aml_getopcode(struct acpi_context *ctx)
{
	u_int16_t twocode;
	u_int16_t opcode;

	/* Check for encoded name */
	if (aml_isnamedop(*ctx->pos)) {
		return AMLOP_NAMECHAR;
	}

	opcode  = aml_parse_int(ctx, 1);
	twocode = (opcode << 8L) + *ctx->pos;

	/* Check multi-byte opcodes */
	if (twocode == AMLOP_LNOTEQUAL ||
	    twocode == AMLOP_LLESSEQUAL ||
	    twocode == AMLOP_LGREATEREQUAL ||
	    opcode == AMLOP_EXTPREFIX) {
		ctx->pos++;
		return twocode;
	}

	return opcode;
}

struct aml_node *
childOf(struct aml_node *parent, int child)
{
	struct aml_node *node = parent->child;

	while(node && child--) {
		node = node->sibling;
	}
	return node;
}

struct aml_value aml_debugobj;

/* Test AML_MATCH operation */
int
aml_match(int64_t lhs, int mtype, int64_t rhs)
{
	switch (mtype) {
	case AML_MATCH_TR: return (1);
	case AML_MATCH_EQ: return (lhs == rhs);
	case AML_MATCH_LT: return (lhs < rhs);
	case AML_MATCH_LE: return (lhs <= rhs);
	case AML_MATCH_GE: return (lhs >= rhs);
	case AML_MATCH_GT: return (lhs > rhs);
	}
	return (0);
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

struct aml_node *
aml_create_node(struct acpi_context *ctx, struct aml_node *parent, int opcode, const char *mnem)
{
	struct aml_node *node;

	node = acpi_os_allocmem(sizeof(struct aml_node));
	memset(node, 0, sizeof(struct aml_node));

	node->depth  = parent->depth+1;
	node->start  = ctx->pos;
	node->opcode = opcode;
	node->mnem   = mnem;
	aml_addchildnode(parent, node);

	return node;
}

struct aml_node *
aml_find_name(struct acpi_softc *sc, struct aml_node *root, const char *name)
{
	struct aml_node *ret;
	const char *sname;

	if (*name == AMLOP_ROOTCHAR) {
		root = &aml_root;
		name++;
	}
	while (*name == AMLOP_PARENTPREFIX) {
		if (root) root = root->parent;
		name++;
	}
	if (root == NULL)
		root = &aml_root;

	for (ret=NULL; root && !ret; root = root->sibling) {
		if ((sname = root->name) != NULL) {
			if (*sname == AMLOP_ROOTCHAR) 
				sname++;
			while (*sname == AMLOP_PARENTPREFIX) 
				sname++;
			if (!strcmp(name, sname)) {
				return root;
			}
		}
		if (root->child)
			ret = aml_find_name(sc, root->child, name);
	}
	return ret;
}

int
aml_eval_name(struct acpi_softc *sc, struct aml_node *root, const char *name, 
	      struct aml_value *result, struct aml_value *env)
{
	root = aml_find_name(sc, root, name);

	if (root != NULL) {
		dnprintf(50, "found eval object : %s, %.4x\n", root->name, root->opcode);
		return aml_eval_object(sc, root, result, env);
	}
	return (1);
}

int64_t
aml_str2int(const char *str, int len, int radix)
{
	int64_t rval, cb;

	rval = 0;
	while (*str && len--) {
		cb = *(str++);
		rval *= radix;
		if (cb >= 'A' && cb <= 'F' && radix == 16) 
			rval += (cb - 'A');
		else if (cb >= 'a' && cb <= 'f' && radix == 16)
			rval += (cb - 'a');
		else if (cb >= '0' && cb <= '9')
			rval += (cb - '0');
		else {
			break;
		}
	}
	return rval;
}


/* aml_bufcpy copies/shifts buffer data, special case for aligned transfers
 *   dstPos/srcPos are bit positions within destination/source buffers
 */
void
aml_bufcpy(u_int8_t *pDst, int dstPos, const u_int8_t *pSrc, int srcPos, 
	   int len)
{
	int idx;

	if (aml_bytealigned(dstPos|srcPos|len)) {
		/* Aligned transfer: use memcpy */
		memcpy(pDst+aml_bytepos(dstPos), pSrc+aml_bytepos(srcPos), aml_bytelen(len));
	}
	else {
		/* Misaligned transfer: perform bitwise copy */
		for (idx=0; idx<len; idx++) {
			aml_setbit(pDst, idx+dstPos, aml_tstbit(pSrc, idx+srcPos));
		}
	}
}

void
aml_create_fieldunit(struct acpi_context *ctx, struct aml_node *parent, const char *name, int bitpos, int bitlen, int flags)
{
	struct aml_node *pfield;

	dnprintf(50, "create_fieldunit: name=%s bitpos=%.8x bitlen=%.8x\n", name, bitpos, bitlen);

	pfield = aml_create_node(ctx, parent, AMLOP_FIELDUNIT, "fieldunit");
	if (pfield == NULL)
		return;

	pfield->name = name;

	/* Setup Field Unit object: point to OpRegion NameRef in parent field */
	pfield->value = aml_allocvalue(AML_OBJTYPE_FIELDUNIT, 0, NULL, "FieldUnit");
	if (pfield->value != NULL) {
		pfield->value->length = aml_bytelen(bitlen);
		pfield->value->v_field.flags  = flags;
		pfield->value->v_field.ftype  = parent->opcode;
		pfield->value->v_field.bitpos = bitpos;
		pfield->value->v_field.bitlen = bitlen;
		pfield->value->v_field.ref    = childOf(parent, 0);

		switch (AML_FIELD_ACCESS(flags)) {
		case AML_FIELD_ANYACC:
		case AML_FIELD_BYTEACC:
			pfield->value->v_field.acc_size = 1;
			break;
		case AML_FIELD_WORDACC:
			pfield->value->v_field.acc_size = 2;
			break;
		case AML_FIELD_DWORDACC:
			pfield->value->v_field.acc_size = 4;
			break;
		default:
			dnprintf(50, "aml_createfieldunit: invalid field access size: %x\n", 
				 AML_FIELD_ACCESS(flags));
			break;
		}
	}
}

void
aml_create_bufferunit(struct acpi_context *ctx, struct aml_node *node, int bitlen, int size)
{
	/* ASSERT: node->value is NULL */
	node->value = aml_allocvalue(AML_OBJTYPE_BUFFERFIELD, 0, NULL, "bufferunit");
	if (node->value != NULL) {
		if (bitlen == -1) {
			/* AMLOP_CREATEFIELD: decode bitlength */
			bitlen = _aml_evalint(ctx, childOf(node, 2));
		}
		node->value->length = aml_bytelen(bitlen);
		node->value->v_field.flags = 0;
		node->value->v_field.ftype  = node->opcode;
		node->value->v_field.bitpos = _aml_evalint(ctx, childOf(node, 1)) * size;
		node->value->v_field.bitlen = bitlen;
		node->value->v_field.ref = childOf(node, 0);  
		node->value->v_field.acc_size = 1;

		dnprintf(50, "create_bufferunit: name=%s pos=%.8x len=%.8x\n", 
			 node->name, node->value->v_field.bitpos,
			 node->value->v_field.bitlen);
	}
}

/* Copy to/from a buffer object */
struct aml_value *
aml_bufferio(struct acpi_context *ctx, struct aml_value *pfield, struct aml_value *rhs)
{
	struct aml_value *pbuf, *rv;

	/* ASSERT: pfield is AML_OBJTYPE_BUFFERFIELD */
	pbuf = aml_evalnode(ctx, pfield->v_field.ref);
	if (pbuf->type != AML_OBJTYPE_BUFFERFIELD) {
		dnprintf(50, "Invalid bufferio!\n");
		return NULL;
	}
	if (rhs == NULL) {
		/* Return buffer object */
		rv = aml_allocvalue(AML_OBJTYPE_BUFFER, pfield->length, NULL, "bufferio");
		if (rv != NULL) {
			aml_bufcpy(rv->v_buffer, 0, pbuf->v_buffer, 
				   pfield->v_field.bitpos, 
				   pfield->v_field.bitlen);
		}
		aml_freevalue(pbuf);
		return rv;
	}

	switch (rhs->type) {
	case AML_OBJTYPE_INTEGER:
		aml_bufcpy(pbuf->v_buffer, pfield->v_field.bitpos, (u_int8_t *)&rhs->v_integer, 0, 
			   pfield->v_field.bitlen);
		break;
	case AML_OBJTYPE_BUFFER:
		aml_bufcpy(pbuf->v_buffer, pfield->v_field.bitpos, rhs->v_buffer, 0, 
			   pfield->v_field.bitlen);
		break;
	default:
		dnprintf(50, "invalid type to bufferio\n");
	}
	return NULL;
}

/* Copy to/from a field object */
struct aml_value *
aml_fieldio(struct acpi_context *ctx, struct aml_value *pfield, struct aml_value *rhs)
{
	struct aml_value *pbuf, *rv;

	switch (pfield->v_field.ftype) {
	case AMLOP_INDEXFIELD:
		/* Write bit offset to INDEX register 
		   INDEX is childOf(pfield->v_field.ref, 0)
		   DATA  is childOf(pfield->v_field.ref, 1)
		*/
		_aml_setnodevalue(ctx, childOf(pfield->v_field.ref, 0), 0, pfield->v_field.bitpos);
		if (rhs == NULL) {
			return aml_evalnode(ctx, childOf(pfield->v_field.ref, 1));
		}
		return _aml_setnodevalue(ctx, childOf(pfield->v_field.ref, 1), rhs, 0);

	case AMLOP_BANKFIELD:
		return NULL;

	case AMLOP_FIELD:
		break;

	}
	/* ASSERT: pfield is AML_OBJTYPE_FIELDUNIT */
	pbuf = aml_evalnode(ctx, pfield->v_field.ref);
	if (pbuf->type != AML_OBJTYPE_OPREGION) {
		dnprintf(50, "Invalid fieldio!\n");
		return NULL;
	}
	
	/* Allocate large enough buffer to hold size bytes */
	rv = aml_allocvalue(AML_OBJTYPE_BUFFER, pfield->length, NULL, "bufferio");
	if (rhs == NULL) {
		/* Return buffer object */
		if (rv != NULL) {
			/* Copy data from field object */
			acpi_gasio(ctx->sc, ACPI_IOREAD, 
				   pbuf->v_opregion.iospace,
				   pbuf->v_opregion.iobase + aml_bytepos(pfield->v_field.bitpos),
				   pfield->v_field.acc_size, rv->length, 
				   rv->v_buffer);
		}
		aml_freevalue(pbuf);
		return rv;
	}
	
	/* Check how we should write nonaligned bits */
	switch (AML_FIELD_UPDATE(pfield->v_field.flags)) {
	case AML_FIELD_PRESERVE:
		/* XXX: don't need to read old bits if new bits == length */
		acpi_gasio(ctx->sc, ACPI_IOREAD,
			   pbuf->v_opregion.iospace,
			   pbuf->v_opregion.iobase + aml_bytepos(pfield->v_field.bitpos),
			   pfield->v_field.acc_size, rv->length,
			   rv->v_buffer);
		break;
	case AML_FIELD_WRITEASONES:
		memset(rv->v_buffer, 0xFF, rv->length);
		break;
	case AML_FIELD_WRITEASZEROES:
		memset(rv->v_buffer, 0x00, rv->length);
		break;
	}
	return pbuf;
}

struct aml_value *
_aml_setnodevalue(struct acpi_context *ctx, struct aml_node *node, struct aml_value *rhs, uint64_t ival)
{
	if (node == NULL) {
		dnprintf(50, "aml_setnodevalue: invalid node!\n");
		return NULL;
	}

	/* Integer value passed in */
	if (rhs == NULL)
		rhs = aml_allocint(ival);

	dnprintf(50, "aml_setnodevalue: ");
	aml_shownode(node);
	aml_showvalue(rhs);

	return rhs;
}

struct aml_value *
aml_getnodevalue(struct acpi_context *ctx, struct aml_node *node)
{
	u_int64_t i1, i2;

	if (node->value != NULL) {
		node->value->refcnt++;
		return node->value;
	}

	switch (node->opcode) {
	case AMLOP_LOCAL0:
	case AMLOP_LOCAL1:
	case AMLOP_LOCAL2:
	case AMLOP_LOCAL3:
	case AMLOP_LOCAL4:
	case AMLOP_LOCAL5:
	case AMLOP_LOCAL6:
	case AMLOP_LOCAL7:
		i1 = node->opcode - AMLOP_LOCAL0;
		if (ctx->stack) {
			node->value = &ctx->stack->locals[i1];
		}
		break;

	case AMLOP_ARG0:
	case AMLOP_ARG1:
	case AMLOP_ARG2:
	case AMLOP_ARG3:
	case AMLOP_ARG4:
	case AMLOP_ARG5:
	case AMLOP_ARG6:
		i1 = node->opcode - AMLOP_ARG0;
		if (ctx->stack) {
			node->value = &ctx->stack->args[i1];
		}
		break;

	case AMLOP_CREATEFIELD:
		aml_create_bufferunit(ctx, node, -1, 1);
		break;
	case AMLOP_CREATEBITFIELD:
		aml_create_bufferunit(ctx, node, 1, 1);
		break;
	case AMLOP_CREATEBYTEFIELD:
		aml_create_bufferunit(ctx, node, 8, 8);
		break;
	case AMLOP_CREATEWORDFIELD:
		aml_create_bufferunit(ctx, node, 16, 8);
		break;
	case AMLOP_CREATEDWORDFIELD:
		aml_create_bufferunit(ctx, node, 32, 8);
		break;
	case AMLOP_CREATEQWORDFIELD:
		aml_create_bufferunit(ctx, node, 64, 8);
		break;
		
	case AMLOP_DEBUG:
		node->value = aml_allocvalue(AML_OBJTYPE_DEBUGOBJ, 0, NULL, "debugobj");
		break;

	case AMLOP_BUFFER:
		/* Requested length of buffer can be greater than supplied bytes */
		i1 = _aml_evalint(ctx, childOf(node, 0));
		i2 = node->end - node->start;
		
		dnprintf(50, "allocate buffer: %d/%d\n", i2, i1);
		node->value = aml_allocvalue(AML_OBJTYPE_BUFFER, i1, NULL, "buffer");
		if (node->value && i2 > 0) {
			memcpy(node->value->v_buffer, node->start, i2);
		}
		break;

	case AMLOP_PACKAGE:
	case AMLOP_VARPACKAGE:
		i1 = _aml_evalint(ctx, childOf(node, 0));
		dnprintf(50, "allocate package: %d\n", i1);
		node->value = aml_allocvalue(AML_OBJTYPE_PACKAGE, i1, NULL, "package");
		for (i2=0; node->value && i2 < i1; i2++) {
			node->value->v_package[i2] = aml_evalnode(ctx, childOf(node, 1+i2));
		}
		break;

	case AMLOP_OPREGION:
		node->value = aml_allocvalue(AML_OBJTYPE_OPREGION, 0, NULL, "opregion");
		if (node->value != NULL) {
			node->value->v_opregion.iospace = _aml_evalint(ctx, childOf(node, 0));
			node->value->v_opregion.iobase = _aml_evalint(ctx, childOf(node, 1));
			node->value->v_opregion.iolen = _aml_evalint(ctx, childOf(node, 2));
			dnprintf(50, "allocate opregion; %x %llx %x\n",
				 node->value->v_opregion.iospace,
				 node->value->v_opregion.iobase,
				 node->value->v_opregion.iolen);
		}
		break;

	case AMLOP_PROCESSOR:
		node->value = aml_allocvalue(AML_OBJTYPE_PROCESSOR, 0, NULL, "processor");
		if (node->value != NULL) {
			node->value->v_processor.proc_id = _aml_evalint(ctx, childOf(node, 0));
			node->value->v_processor.proc_addr = _aml_evalint(ctx, childOf(node, 1));
			node->value->v_processor.proc_len = _aml_evalint(ctx, childOf(node, 2));
			dnprintf(50, "allocate processor; %x %x %x\n",
				 node->value->v_processor.proc_id,
				 node->value->v_processor.proc_addr,
				 node->value->v_processor.proc_len);
		}
		break;

	case AMLOP_THERMALZONE:
		node->value = aml_allocvalue(AML_OBJTYPE_THERMZONE, 0, NULL, "thermzone");
		break;

	case AMLOP_EVENT:
		node->value = aml_allocvalue(AML_OBJTYPE_EVENT, 0, NULL, "event");
		break;

	case AMLOP_DEVICE:
		node->value = aml_allocvalue(AML_OBJTYPE_DEVICE, 0, NULL, "device");
		break;

	case AMLOP_MUTEX:
		i1 = _aml_evalint(ctx, childOf(node, 0));
		node->value = aml_allocvalue(AML_OBJTYPE_MUTEX, i1, NULL, "mutex");
		break;

	case AMLOP_POWERRSRC:
		node->value = aml_allocvalue(AML_OBJTYPE_POWERRSRC, 0, NULL, "powerrsrc");
		if (node->value != NULL) {
			node->value->v_powerrsrc.pwr_level = _aml_evalint(ctx, childOf(node, 0));
			node->value->v_powerrsrc.pwr_order = _aml_evalint(ctx, childOf(node, 1));
		}
		break;

	default:
		return NULL;
	}
	if (node->value)
		node->value->refcnt++;
	return node->value;
}

/*
 * Evaluate integer object
 */
int64_t
_aml_evalint(struct acpi_context *ctx, struct aml_node *node)
{
	struct aml_value *rhs;
	int64_t rval = 0;

	rhs = aml_evalnode(ctx, node);
	if (rhs != NULL) {
		switch (rhs->type) {
		case AML_OBJTYPE_INTEGER:
			rval = rhs->v_integer;
			break;
		case AML_OBJTYPE_STRING:
			/* Convert hex/decimal string to integer */
			if (!strncmp(rhs->v_string, "0x", 2)) 
				rval = aml_str2int(rhs->v_string+2, rhs->length, 16);
			else
				rval = aml_str2int(rhs->v_string, rhs->length, 10);
			break;
		case AML_OBJTYPE_BUFFER:
			rval = *rhs->v_buffer;
			break;
		default:
			dnprintf(50, "aml_evalint: invalid type: %x\n", rhs->type);
			break;
		}
		aml_freevalue(rhs);
	}
	return rval;
}

struct aml_value *
_aml_evalref(struct acpi_context *ctx, struct aml_node *node)
{
	if (node->value->v_nameref.ref == NULL) {
		dnprintf(50,"deref: %s\n", node->value->v_nameref.name);
		node->value->v_nameref.ref = aml_find_name(ctx->sc, NULL, node->value->v_nameref.name);
	} 
	return aml_evalnode(ctx, node->value->v_nameref.ref);
}

struct aml_value *aml_deref(struct acpi_context *ctx, struct aml_value *rv);

struct aml_value *
aml_deref(struct acpi_context *ctx, struct aml_value *rv)
{
	struct aml_value *lhs = NULL;

	if (rv != NULL) {
		switch (rv->type) {
		case AML_OBJTYPE_BUFFERFIELD:
			lhs = rv;
			rv = aml_bufferio(ctx, rv, NULL);
			break;
		case AML_OBJTYPE_FIELDUNIT:
			lhs = rv;
			rv = aml_fieldio(ctx, rv, NULL);
			break;
		case AML_OBJTYPE_OBJREF:
			break;
		}
	}
	aml_freevalue(lhs);
	return rv;
}


struct aml_value *
aml_evalnode(struct acpi_context *ctx, struct aml_node *node)
{
	struct aml_value *rv, *lhs, *rhs;
	struct aml_node  *cflow;
	int64_t i1, i2, i3, i4, i5, i6;

	if (node == NULL) {
		dnprintf(50, "aml_evalnode: invalid node\n");
		return NULL;
	}

#ifdef ACPI_DEBUG
	dnprintf(50, " %d ", node->depth);
	for(i1=0; i1<node->depth; i1++) {
		dnprintf(50, "..");
	}
	printf("evalnode: addr:%.8x %.4x %s\n", 
	       aml_ipaddr(node->start),
	       node->opcode, 
	       node->mnem);
#endif

	/* Initialize helper variables */
	lhs = NULL;
	rhs = NULL;
	cflow = NULL;

	/* Check if node already contains value */
	if ((rv = aml_getnodevalue(ctx, node)) != NULL) {
		/* Dereference object if necessary */
		return aml_deref(ctx, rv);
	}

	switch (node->opcode) {
	case AMLOP_NOP:
	case AMLOP_ELSE:
	case AMLOP_BREAK:
	case AMLOP_BREAKPOINT:
	case AMLOP_CONTINUE:
		break;

	case AMLOP_WHILE:
		for (;;) {
			if (cflow == NULL) {
				/* Test while condition */
				cflow = childOf(node, 1);
				i1 = _aml_evalint(ctx, childOf(node, 0));
				dnprintf(50, "aml_while: %d\n", i1);
				if (i1 == 0)
					break;
			}
			else if (cflow->opcode == AMLOP_BREAK)
				break;
			else if (cflow->opcode == AMLOP_CONTINUE)
				/* Reset cflow to NULL; restart block */
				cflow = NULL;
			else {
				/* Execute next node in block */
				aml_freevalue(lhs);
				lhs = aml_evalnode(ctx, cflow);
				cflow = cflow->sibling;
			}
		}
		break;

	case AMLOP_IF:
		i1 = _aml_evalint(ctx, childOf(node, 0));
		dnprintf(50, "aml_if: %d\n", i1);
		if (i1 != 0) {
			/* Test true, select 'If' block */
			cflow = childOf(node, 1);
		}
		else if (node->sibling && node->sibling->opcode == AMLOP_ELSE) {
			/* Test false, select 'Else' block */
			cflow = childOf(node->sibling, 0);
		}
		while (cflow) {
			/* Execute all instructions in scope block */
			rv = aml_evalnode(ctx, cflow);
			cflow = cflow->sibling;
		}
		break;

	case AMLOP_METHOD:
		if (node->child == NULL) {
			/* Parse method object */
		}
		dnprintf(50, "Evaluating method: %s\n", node->name);
		for (cflow = node->child; cflow; cflow = cflow->sibling) {
			aml_freevalue(rv);
			rv = aml_evalnode(ctx, cflow);
		}
		break;

	case AMLOP_NAMECHAR:
		/* Return dereferenced object */
		rv = _aml_evalref(ctx, node);
		break;

	case AMLOP_NAME:
	case AMLOP_ALIAS:
	case AMLOP_RETURN:
		/* Return child value */
		rv = aml_evalnode(ctx, childOf(node, 0));
		break;

	case AMLOP_INCREMENT:
	case AMLOP_DECREMENT:
		i1 = _aml_evalint(ctx, childOf(node, 0));
		rv = _aml_setnodevalue(ctx, childOf(node, 0), NULL, 
				       aml_evalmath(node->opcode, i1, 1));
		break;
		
	case AMLOP_NOT:
	case AMLOP_FINDSETLEFTBIT:
	case AMLOP_FINDSETRIGHTBIT:
		i1 = _aml_evalint(ctx, childOf(node, 0));
		rv = _aml_setnodevalue(ctx, childOf(node, 1), NULL, 
				       aml_evalmath(node->opcode, i1, 0));
		break;

	case AMLOP_DIVIDE:
		/* Divide: Return quotient and remainder */
		i1 = _aml_evalint(ctx, childOf(node, 0));
		i2 = _aml_evalint(ctx, childOf(node, 1));
		_aml_setnodevalue(ctx, childOf(node, 2), NULL, 
				  aml_evalmath(AMLOP_MOD, i1, i2));
		rv = _aml_setnodevalue(ctx, childOf(node, 3), NULL,
				       aml_evalmath(AMLOP_DIVIDE, i1, i2));
		break;

	case AMLOP_ADD:
	case AMLOP_SUBTRACT:
	case AMLOP_MULTIPLY:
	case AMLOP_SHL:
	case AMLOP_SHR:
	case AMLOP_AND:
	case AMLOP_NAND:
	case AMLOP_OR:
	case AMLOP_XOR:
	case AMLOP_NOR:
	case AMLOP_MOD:
		i1 = _aml_evalint(ctx, childOf(node, 0));
		i2 = _aml_evalint(ctx, childOf(node, 1));
		rv = _aml_setnodevalue(ctx, childOf(node, 2), NULL, 
				       aml_evalmath(node->opcode, i1, i2));
		break;

	case AMLOP_LNOT:
		i1 = _aml_evalint(ctx, childOf(node, 0));
		rv = aml_allocint(!i1);
		break;

	case AMLOP_LAND:
	case AMLOP_LOR:
	case AMLOP_LEQUAL:
	case AMLOP_LNOTEQUAL:
	case AMLOP_LLESSEQUAL:
	case AMLOP_LGREATEREQUAL:
	case AMLOP_LGREATER:
	case AMLOP_LLESS:
		lhs = aml_evalnode(ctx, childOf(node, 0));
		rhs = aml_evalnode(ctx, childOf(node, 1));

		i1 = aml_comparevalue(node->opcode, lhs, rhs);
		rv = aml_allocint(i1);
		break;

	case AMLOP_TODECSTRING:
		i1 = _aml_evalint(ctx, childOf(node, 0));
		rhs = aml_allocvalue(AML_OBJTYPE_STRING, AML_INTSTRLEN, NULL, "");
		if (rhs != NULL) {
			snprintf(rhs->v_string, AML_INTSTRLEN, "%d", i1);
			rv = _aml_setnodevalue(ctx, childOf(node, 1), rhs, 0);
		}
		break;

	case AMLOP_TOHEXSTRING:
		i1 = _aml_evalint(ctx, childOf(node, 0));
		rhs = aml_allocvalue(AML_OBJTYPE_STRING, AML_INTSTRLEN, NULL, "");
		if (rhs != NULL) {
			snprintf(rhs->v_string, AML_INTSTRLEN, "%x", i1);
			rv = _aml_setnodevalue(ctx, childOf(node, 1), rhs, 0);
		}
		break;

	case AMLOP_TOINTEGER:
		i1 = _aml_evalint(ctx, childOf(node, 0));
		rv = _aml_setnodevalue(ctx, childOf(node, 1), NULL, i1);
		break;

	case AMLOP_FROMBCD:
		i1 = _aml_evalint(ctx, childOf(node, 0));
		rv = _aml_setnodevalue(ctx, childOf(node, 1), NULL, aml_bcd2dec(i1));
		break;

	case AMLOP_TOBCD:
		i1 = _aml_evalint(ctx, childOf(node, 0));
		rv = _aml_setnodevalue(ctx, childOf(node, 1), NULL, aml_dec2bcd(i1));
		break;

	case AMLOP_MATCH:
		/* Perform match operation  */
		lhs = aml_evalnode(ctx, childOf(node, 0));  /* package */

		/* Allocate default return value */
		rv = aml_allocint(-1);
		if (lhs->type == AML_OBJTYPE_PACKAGE && rv != NULL) {
			i1 = _aml_evalint(ctx, childOf(node, 1));    /* op1 */
			i2 = _aml_evalint(ctx, childOf(node, 2));    /* match1 */
			i3 = _aml_evalint(ctx, childOf(node, 3));    /* op2 */
			i4 = _aml_evalint(ctx, childOf(node, 4));    /* match2 */
			i5 = _aml_evalint(ctx, childOf(node, 5));    /* index */

			while (i5 < lhs->length) {
				if (lhs->v_package[i5]->type == AML_OBJTYPE_INTEGER) {
					i6 = lhs->v_package[i5]->v_integer;
				}
				else {
					dnprintf(50, "aml_match: invalid index: %d:%x\n",
						 i5, lhs->v_package[i5]->type);
					break;
				}
				if (aml_match(i6, i1, i2) && aml_match(i6, i3, i4)) {
					rv->v_integer = i5;
					break;
				}
				i5++;
			}
		}
		break;

	case AMLOP_COPYOBJECT:
		/* Return copy of an object */
		lhs = aml_evalnode(ctx, childOf(node, 0));
		rhs = aml_copyvalue(lhs);
		rv = _aml_setnodevalue(ctx, childOf(node, 1), rhs, 0);
		break;

	case AMLOP_STORE:
		/* Store value into another */
		rhs = aml_evalnode(ctx, childOf(node, 0));
		rv = _aml_setnodevalue(ctx, childOf(node, 1), rhs, 0);
		break;

	case AMLOP_OBJECTTYPE:
		/* Return object type */
		lhs = aml_evalnode(ctx, childOf(node, 0));
		rv = aml_allocint(lhs->type);
		break;

	case AMLOP_SIZEOF:
		/* Return sizeof object (string/buffer/package) */
		lhs = aml_evalnode(ctx, childOf(node, 0));

		switch (lhs->type) {
		case AML_OBJTYPE_STRING:
		case AML_OBJTYPE_BUFFER:
		case AML_OBJTYPE_PACKAGE:
			i1 = lhs->length;
			break;
		default:
			i1 = 0;
			dnprintf(50,"aml_sizeof: incorrect type: %x\n", lhs->type);
			break;
		}
		rv = aml_allocint(i1);
		break;

	case AMLOP_REFOF:
		rv = aml_allocvalue(AML_OBJTYPE_OBJREF, -1, childOf(node, 0), "refof");
		break;

	case AMLOP_CONDREFOF:
		rv  = aml_allocint(AML_FALSE);
		lhs = aml_evalnode(ctx, childOf(node, 0));
		if (lhs != NULL) {
			/* Conditional reference */
			rv->v_integer = AML_TRUE;
			rhs = aml_allocvalue(AML_OBJTYPE_OBJREF, -1, childOf(node, 0), "condref");
			_aml_setnodevalue(ctx, childOf(node, 1), rhs, 0);
		}
		break;

	case AMLOP_DEREFOF:
		lhs = aml_evalnode(ctx, childOf(node, 0));
		if (lhs->type == AML_OBJTYPE_OBJREF) {
			rv = aml_evalnode(ctx, lhs->v_objref.ref);
		}
		else if (lhs->type == AML_OBJTYPE_STRING) {
			/* XXX: fix me
			   rv = aml_evalname(ctx, node, lhs->v_string);
			*/
		}
		break;

	case AMLOP_INDEX:
		i1 = _aml_evalint(ctx, childOf(node, 1));
		rv = aml_allocvalue(AML_OBJTYPE_OBJREF, i1, childOf(node, 0), "index");

		break;

	case AMLOP_MID:
		/* Return substring (actual contents) */
		lhs = aml_evalnode(ctx, childOf(node, 0));
		i1 = _aml_evalint(ctx, childOf(node, 1));  /* length */
		i2 = _aml_evalint(ctx, childOf(node, 2));  /* index */

		/* check bounds against source object */
		if (i1 >= lhs->length)
			i1 = i2 = 0;
		if (i1+i2 >= lhs->length)
			i2 = lhs->length - i1;
		switch (lhs->type) {
		case AML_OBJTYPE_STRING:
			rhs = aml_allocvalue(AML_OBJTYPE_STRING, 0, NULL, "");
			if (rhs != NULL) {
				rhs->length = i1;
				rhs->v_string = lhs->v_string + i2;
				rv = _aml_setnodevalue(ctx, childOf(node, 3), rhs, 0);
			}
			break;
		case AML_OBJTYPE_BUFFER:
			rhs = aml_allocvalue(AML_OBJTYPE_BUFFER, 0, NULL, "");
			if (rhs != NULL) {
				rhs->length = i1;
				rhs->v_buffer = lhs->v_buffer + i2;
				rv = _aml_setnodevalue(ctx, childOf(node, 3), rhs, 0);
			}
			break;
		default:
			dnprintf(50, "aml_mid: invalid type: %x\n", lhs->type);
			break;
		}
		break;

	case AMLOP_CONCAT:
		/* Concatenate result values */
		lhs = aml_evalnode(ctx, childOf(node, 0));
		rhs = aml_evalnode(ctx, childOf(node, 1));
		if (lhs->type == rhs->type) {
			switch (lhs->type) {
			case AML_OBJTYPE_STRING:
				rv = aml_allocvalue(AML_OBJTYPE_STRING, lhs->length+rhs->length, NULL, "");
				if (rv != NULL) {
					strncpy(rv->v_string, lhs->v_string, lhs->length);
					strncpy(rv->v_string+lhs->length, rhs->v_string, rhs->length);
				}
				break;
			case AML_OBJTYPE_BUFFER:
				rv = aml_allocvalue(AML_OBJTYPE_BUFFER, lhs->length+rhs->length, NULL, "");
				if (rv != NULL) {
					memcpy(rv->v_buffer, lhs->v_buffer, lhs->length);
					memcpy(rv->v_buffer+lhs->length, rhs->v_buffer, rhs->length);
				}
				break;
			}
		}
		break;

	case AMLOP_NOTIFY:
		i1 = _aml_evalint(ctx, childOf(node, 1));
		dnprintf(50,"NOTIFY: %s %x\n", "", i1);
		break;

	case AMLOP_WAIT:
		lhs = aml_evalnode(ctx, childOf(node, 0));
		i1 = _aml_evalint(ctx, childOf(node, 1));
		break;

	case AMLOP_SIGNAL:
		lhs = aml_evalnode(ctx, childOf(node, 0));
		break;

	case AMLOP_RESET:
		lhs = aml_evalnode(ctx, childOf(node, 0));
		break;

	case AMLOP_ACQUIRE:
		lhs = aml_evalnode(ctx, childOf(node, 0));
		i1 = _aml_evalint(ctx, childOf(node, 1));
		break;

	case AMLOP_RELEASE:
		lhs = aml_evalnode(ctx, childOf(node, 0));
		break;

	case AMLOP_STALL:
		i1 = _aml_evalint(ctx, childOf(node, 0));
		dnprintf(50, "stall %d usecs\n", i1);
		break;

	case AMLOP_SLEEP:
		i1 = _aml_evalint(ctx, childOf(node, 0));
		dnprintf(50, "sleep %d msecs\n", i1);
		break;
	}

	/* Free temporary objects */
	aml_freevalue(lhs);
	aml_freevalue(rhs);

	aml_showvalue(rv);

	return aml_deref(ctx, rv);
}

int
aml_eval_object(struct acpi_softc *sc, struct aml_node *node, struct aml_value *result, 
		struct aml_value *env)
{
	struct acpi_context ctx;
	struct aml_value *rv;

	memset(&ctx, 0, sizeof(ctx));
	ctx.pos = node->start;

	memset(result, 0, sizeof(struct aml_value));
	rv = aml_evalnode(&ctx, node);
	*result = *rv;

	return (0);
}

int
aml_parse_args(struct acpi_context *ctx, struct aml_node *node, const char *arg)
{
	struct aml_node *pnode;

	while (*arg) {
		pnode = node;
		switch (*arg) {
		case AML_ARG_FLAG:
			node->flag = aml_parse_int(ctx, 1);
			if (node->opcode == AMLOP_METHOD) {
				dnprintf(50, " method %s %.2x argcount:%d serialized:%d synclevel:%d\n",
					 node->name, node->flag,
					 AML_METHOD_ARGCOUNT(node->flag),
					 AML_METHOD_SERIALIZED(node->flag),
					 AML_METHOD_SYNCLEVEL(node->flag));
			}
			else {
				dnprintf(50, " field %.2x access:%d lock:%d update:%d\n",
					 node->flag,
					 AML_FIELD_ACCESS(node->flag),
					 AML_FIELD_LOCK(node->flag),
					 AML_FIELD_UPDATE(node->flag));
			}
			break;
		case AML_ARG_IMPBYTE:
			/* Implied byte: same as opcode */
			node->value = aml_allocint((char)node->opcode);
			dnprintf(50, " ibyte: %x\n", (int8_t)node->opcode);
			break;
		case AML_ARG_BYTE:
			if (node->opcode != AMLOP_BYTEPREFIX) {
				pnode = aml_create_node(ctx, node, AMLOP_BYTEPREFIX, "byte");
			}
			pnode->value = aml_allocint(aml_parse_int(ctx, 1));
			dnprintf(50, " byte: %x\n", pnode->value->v_integer);
			break;
		case AML_ARG_WORD:
			if (node->opcode != AMLOP_WORDPREFIX) {
				pnode = aml_create_node(ctx, node, AMLOP_WORDPREFIX, "word");
			}
			pnode->value = aml_allocint(aml_parse_int(ctx, 2));
			dnprintf(50, " word: %x\n", pnode->value->v_integer);
			break;
		case AML_ARG_DWORD:
			if (node->opcode != AMLOP_DWORDPREFIX) {
				pnode = aml_create_node(ctx, node, AMLOP_DWORDPREFIX, "dword");
			}
			pnode->value = aml_allocint(aml_parse_int(ctx, 4));
			dnprintf(50, " dword: %x\n", pnode->value->v_integer);
			break;
		case AML_ARG_QWORD:
			if (node->opcode == AMLOP_QWORDPREFIX) {
				pnode = aml_create_node(ctx, node, AMLOP_QWORDPREFIX, "qword");
			}
			pnode->value = aml_allocint(aml_parse_int(ctx, 8));
			dnprintf(50, " qword: %x\n", pnode->value->v_integer);	
			break;
		case AML_ARG_FIELDLIST:
			aml_parse_fieldlist(ctx, node);
			break;
		case AML_ARG_BYTELIST:
			dnprintf(50, " bytelist\n");
			node->start = ctx->pos;
			ctx->pos    = node->end;
			break;
		case AML_ARG_STRING:
			node->value = aml_allocstr(aml_parse_string(ctx));
			dnprintf(50, " string: %s\n", node->value->v_string);
			break;
		case AML_ARG_NAMESTRING:
			node->name = aml_parse_name(ctx, "name");
			break;
		case AML_ARG_NAMEREF:
			if (node->opcode != AMLOP_NAMECHAR) {
				pnode = aml_create_node(ctx, node, AMLOP_NAMECHAR, "nameref");
			}
			pnode->value = aml_allocvalue(AML_OBJTYPE_NAMEREF, 0, aml_parse_name(ctx, "nameref"), "");
			break;
		case AML_ARG_OBJLEN:
			dnprintf(50, " pkglen\n");
			node->end = ctx->pos;
			node->end += aml_parse_length(ctx);
			break;
		case AML_ARG_METHOD:
			dnprintf(50, " method\n");
			node->start = ctx->pos;
			ctx->pos = node->end;
			break;
		case AML_ARG_INTEGER:
		case AML_ARG_TERMOBJ:
			/* Recursively parse children */
			aml_parse_object(ctx, node);
			break;
		case AML_ARG_TERMOBJLIST:
			/* Recursively parse children */
			aml_parse_objlist(ctx, node);
			break;

		default:
			printf("Unknown arg: %c\n", *arg);
			break;
		}

		arg++;
	}

	return (0);
}

struct aml_node *
aml_preparse(struct acpi_context *ctx, struct aml_node *parent, struct aml_optable *optab)
{
	const char *arg;

	/* Check for named object */
	for (arg=optab->args; arg && *arg; arg++) {
		if (*arg == AML_ARG_NAMESTRING) {
			/* XXX: check for duplicate name */
		}
	}
	return aml_create_node(ctx, parent, optab->opcode, optab->mnem);
}

/* Parse a single object */
struct aml_node *
aml_parse_object(struct acpi_context *ctx, struct aml_node *parent)
{
	struct aml_optable *optab;
	struct aml_node *node;
	u_int16_t opcode;

	/* Get AML Opcode; if it is an embedded name, extract name */
	opcode = aml_getopcode(ctx);
	for (optab = aml_table; optab->opcode != 0xFFFF; optab++) {
		if  (optab->opcode == opcode) {
			/* Found an opcode in the table */
			node = aml_preparse(ctx, parent, optab);
			aml_parse_args(ctx, node, optab->args);
			return node;
		}
	}
	printf("Invalid AML Opcode : @ %.4x %.4x\n", aml_ipaddr(ctx->pos), opcode);

	return NULL;
}

/* Parse list of objects; stop at end of parent */
int
aml_parse_objlist(struct acpi_context *ctx, struct aml_node *parent)
{
	while (ctx->pos < parent->end) {
		aml_parse_object(ctx, parent);
	}
	if (ctx->pos != parent->end) {
		dnprintf(50, "aml_parse_objlist: invalid end! %x\n", parent->end - ctx->pos);
		ctx->pos = parent->end;
		return (1);
	}
	return (0);
}


void
aml_shownode(struct aml_node *node)
{
	dnprintf(50, " opcode:%.4x  flag:%.2x  mnem:%s %s ",
		 node->opcode, node->flag, node->mnem, node->name ? node->name : "");
	switch(node->opcode) {
	case AMLOP_METHOD:
		dnprintf(50, "argcount:%d serialized:%d synclevel:%d",
			 AML_METHOD_ARGCOUNT(node->flag),
			 AML_METHOD_SERIALIZED(node->flag),
			 AML_METHOD_SYNCLEVEL(node->flag));
		break;
		
	case AMLOP_NAMECHAR:
		dnprintf(50, "%s", node->value->v_nameref.name);
		break;

	case AMLOP_FIELD:
	case AMLOP_BANKFIELD:
	case AMLOP_INDEXFIELD:
		dnprintf(50, "access:%d lock:%d update:%d",
			 AML_FIELD_ACCESS(node->flag),
			 AML_FIELD_LOCK(node->flag),
			 AML_FIELD_UPDATE(node->flag));
		break;
		
	case AMLOP_BYTEPREFIX:
		dnprintf(50, "byte: %.2x", node->value->v_integer);
		break;
	case AMLOP_WORDPREFIX:
		dnprintf(50, "word: %.4x", node->value->v_integer);
		break;
	case AMLOP_DWORDPREFIX:
		dnprintf(50, "dword: %.8x", node->value->v_integer);
		break;
	case AMLOP_STRINGPREFIX:
		dnprintf(50, "string: %s", node->value->v_string);
		break;
	}
	dnprintf(50, "\n");
}

void
aml_walktree(struct aml_node *node)
{
	int idx;

	while(node) {
		dnprintf(50, " %d ", node->depth);
		for(idx=0; idx<node->depth; idx++) {
			dnprintf(50, "..");
		}
		aml_shownode(node);
		aml_walktree(node->child);
		node = node->sibling;
	}
}

void
aml_walkroot()
{
	aml_walktree(aml_root.child);
}

int
aml_find_node(struct aml_node *node, const char *name, 
	      void (*cbproc)(struct aml_node *, void *arg),
	      void *arg)
{
	const char *nn;

	while (node) {
		if ((nn = node->name) != NULL) {
			if (*nn == AMLOP_ROOTCHAR) nn++;
			while (*nn == AMLOP_PARENTPREFIX) nn++;
			if (!strcmp(name, nn))
				cbproc(node, arg);
		}
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
	struct acpi_context ctx;

	memset(&ctx, 0, sizeof(ctx));
	ctx.pos = start;

	aml_root.start = start;
	aml_root.end   = start + length;

	aml_parse_objlist(&ctx, &aml_root);
	if (length != aml_ipaddr(ctx.pos)) {
		dnprintf(50, " : error parsing AML @ 0x%x\n", aml_ipaddr(ctx.pos));
		return (-1);
	}

	dnprintf(50, " : parsed %d AML bytes\n", length);

	aml_walktree(&aml_root);

	return (0);
}
