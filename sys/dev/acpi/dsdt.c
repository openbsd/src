/* $OpenBSD: dsdt.c,v 1.19 2006/02/03 23:55:47 jordan Exp $ */
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

void *stacktop;

struct aml_opcode
{
	u_int16_t    opcode;
	const char  *mnem;
	const char  *args;
};

struct acpi_context
{
	int depth;
	uint8_t *pos;
	uint8_t *start;
	struct acpi_softc  *sc;
	struct aml_value  **locals;
	struct aml_value  **args;
	struct aml_node	   *scope;
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
u_int64_t   aml_parse_int(struct acpi_context *, int);
const char *aml_parse_string(struct acpi_context *);
const char *aml_parse_name(struct acpi_context *);

int aml_isnamedop(u_int16_t);
struct aml_opcode *aml_getopcode(struct acpi_context *ctx);

void aml_shownode(struct aml_node *);

u_int64_t  aml_bcd2dec(u_int64_t);
u_int64_t  aml_dec2bcd(u_int64_t);

int aml_lsb(u_int64_t val);
int aml_msb(u_int64_t val);

void acpi_freecontext(struct acpi_context *ctx);
struct acpi_context *acpi_alloccontext(struct acpi_softc *sc, 
				       struct aml_node *node,
				       int argc, 
				       struct aml_value *argv);

void aml_walkroot(void);
struct aml_node *aml_find_name(struct acpi_softc *, struct aml_node *, const char *);

int64_t aml_str2int(const char *, int, int);
int64_t aml_evalmath(u_int16_t, int64_t, int64_t);
int  aml_testlogical(u_int16_t, long, long);
int  aml_strcmp(u_int16_t opcode, const char *lhs, const char *rhs);

int64_t _aml_evalint(struct acpi_context *, struct aml_node *);
struct aml_value *aml_getnodevalue(struct acpi_context *, struct aml_node *);
struct aml_value *_aml_evalref(struct acpi_context *, struct aml_node *);
struct aml_value *aml_evalnode(struct acpi_context *, struct aml_node *);
struct aml_value *_aml_setnodevalue(struct acpi_context *, struct aml_node *, struct aml_value *, uint64_t);

struct aml_node *aml_create_node(struct aml_node *parent, 
				 int opc, const char *mnem,
				 uint8_t *start);

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

struct aml_node	  aml_root;
struct aml_value *aml_global_lock;

#define acpi_mutex_acquire(ctx,lock,iv)	 dnprintf(10,"ACQUIRE: %x" #lock "\n", (short)iv)
#define acpi_mutex_release(ctx,lock)	 dnprintf(10,"RELEASE: " #lock "\n")

void *
acpi_os_allocmem(size_t size)
{
	void *ptr;

	ptr =  malloc(size, M_DEVBUF, M_WAITOK);
	if (ptr) 
		memset(ptr, 0, size);
	return ptr;
}

void
acpi_os_freemem(void *ptr)
{
	//free(ptr, M_DEVBUF);
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

#define AML_REVISION   0x01
#define AML_NUM_LOCALS 8
#define AML_INTSTRLEN 16
#define AML_NAMESEG_LEN 4

void aml_addchildnode(struct aml_node *parent,
		      struct aml_node *child)
{
	struct aml_node **tmp;

	for (tmp = &parent->child; *tmp; tmp = &((*tmp)->sibling))
		;

	child->sibling = NULL;
	child->parent = parent;
	*tmp = child;
}

struct aml_node *aml_create_node(struct aml_node *parent, int opcode,
				 const char *mnem, uint8_t *start)
{
	struct aml_node *node;

	node = (struct aml_node *)acpi_os_allocmem(sizeof(struct aml_node));
	if (node == NULL) {
		return NULL;
	}
	node->opcode = opcode;
	node->mnem   = mnem;
	node->depth  = parent->depth+1;
	node->start  = start;
	node->end    = parent->end;
	node->child  = NULL;
	
	aml_addchildnode(parent, node);

	return node;
}

/* Allocate dynamic AML value
 *   type : Type of object to allocate (AML_OBJTYPE_XXXX)
 *   ival : Integer value (action depends on type)
 *   bval : Buffer value (action depends on type)
 *   lbl  : Debugging label
 */
struct aml_value *
aml_allocvalue(int type, int64_t ival, void *bval, 
	       const char *lbl)
{
	struct aml_value *rv;
	struct aml_value **pv;
	int64_t idx;

	rv = (struct aml_value *)acpi_os_allocmem(sizeof(struct aml_value));
	rv->type = type;
	rv->refcnt = 1;

	switch (type) {
	case AML_OBJTYPE_OBJREF:
		rv->v_objref.index  = ival;
		rv->v_objref.ref    = (struct aml_value *)bval;
		break;
	case AML_OBJTYPE_INTEGER:
		rv->v_integer = ival;
		break;
	case AML_OBJTYPE_STRING:
		/* Allocate string: if pointer valid, copy data */
		rv->length = ival;
		rv->v_string = NULL;
		if (ival) {
			rv->v_string = acpi_os_allocmem(ival+1);
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
			if (bval) {
				memcpy(rv->v_buffer, bval, ival);
			}
		}
		break;
	case AML_OBJTYPE_PACKAGE:
		/* Allocate package pointers */
		rv->length = ival;
		rv->v_package = (struct aml_value **)acpi_os_allocmem(rv->length * sizeof(struct aml_value *));
		if (bval != NULL) {
			pv = (struct aml_value **)bval;
			dnprintf(10, "alloc package..\n");
			for (idx=0; idx<ival; idx++) {
				rv->v_package[idx] = aml_copyvalue(pv[idx]);
			}
		}
		break;
	case AML_OBJTYPE_METHOD:
		rv->name = bval;
		rv->v_method.flags = ival;
		break;
	case AML_OBJTYPE_MUTEX:
		rv->name = bval;
		rv->v_integer = ival;
		break;
	case AML_OBJTYPE_NAMEREF:
	case AML_OBJTYPE_OPREGION:
	case AML_OBJTYPE_DEVICE:
	case AML_OBJTYPE_EVENT:
	case AML_OBJTYPE_POWERRSRC:
	case AML_OBJTYPE_PROCESSOR:
	case AML_OBJTYPE_THERMZONE:
		rv->name = bval;
		break;
	case AML_OBJTYPE_BUFFERFIELD:
	case AML_OBJTYPE_FIELDUNIT:
		break;
	default:
		dnprintf(10, "Unknown aml_allocvalue: %.2x\n", type);
	}
	return rv;
}

struct aml_value *
aml_allocint(uint64_t ival, int sc)
{
	return aml_allocvalue(AML_OBJTYPE_INTEGER, ival, &sc, "integer");
}

struct aml_value *
aml_allocstr(const char *str)
{
	return aml_allocvalue(AML_OBJTYPE_STRING, strlen(str), (void *)str, "string");
}

int 
aml_freevalue(struct aml_value **pv)
{
	struct aml_value *v = *pv;
	int idx;

	/* Don't free static values */
	if (v == NULL) 
		return (0);
	if (v->refcnt < 0)
		return (0);
	if (--v->refcnt > 0)
		return (0);

	//dnprintf(50, "freeing value : %x\n", v->type);
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
			aml_freevalue(&v->v_package[idx]);
		}
		acpi_os_freemem(v->v_package);
		v->v_package = NULL;
		break;
	case AML_OBJTYPE_METHOD:
		break;
	}
	v->type = 0;
	acpi_os_freemem(v);
	*pv = NULL;

	return (1);
}

void aml_dump(int len, u_int8_t *buf);

void
aml_dump(int len, u_int8_t *buf)
{
	int idx;
	
	dnprintf(50, "{ ");
	for (idx=0; idx<len; idx++) {
		dnprintf(50, "%s0x%.2x", idx ? ", " : "", buf[idx]);
	}
	dnprintf(50, " }\n");
}

void
aml_showvalue(struct aml_value *value)
{
	int idx;

	if (value == NULL)
		return;
	if (value->name)
		dnprintf(50, "name:%s ", value->name);
	switch (value->type) {
	case AML_OBJTYPE_INTEGER:
		dnprintf(50, "integer: %x\n", value->v_integer);
		break;
	case AML_OBJTYPE_STRING:
		dnprintf(50, "string: %s\n", value->v_string);
		break;
	case AML_OBJTYPE_BUFFER:
		dnprintf(50, "buffer: %d ", value->length);
		aml_dump(value->length, value->v_buffer);
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
	case AML_OBJTYPE_MUTEX:
		dnprintf(50, "mutex : %x\n", value->v_integer);
		break;
	case AML_OBJTYPE_DEVICE:
		dnprintf(50, "device\n");
		break;
	case AML_OBJTYPE_EVENT:
		dnprintf(50, "event\n");
		break;
	case AML_OBJTYPE_PROCESSOR:
		dnprintf(50, "cpu: %x,%x,%x\n",
			 value->v_processor.proc_id,
			 value->v_processor.proc_addr,
			 value->v_processor.proc_len);
		break;
	case AML_OBJTYPE_METHOD:
		dnprintf(50, "method: args=%d, serialized=%d, synclevel=%d\n",
			 AML_METHOD_ARGCOUNT(value->v_method.flags),
			 AML_METHOD_SERIALIZED(value->v_method.flags),
			 AML_METHOD_SYNCLEVEL(value->v_method.flags));
		break;
	case AML_OBJTYPE_FIELDUNIT:
		dnprintf(50, "field: access=%x,lock=%x,update=%x type=%6s pos=%.4x len=%.4x\n",
			 AML_FIELD_ACCESS(value->v_field.flags),
			 AML_FIELD_LOCK(value->v_field.flags),
			 AML_FIELD_UPDATE(value->v_field.flags),
			 value->v_field.ftyp,
			 value->v_field.bitpos,
			 value->v_field.bitlen);
		aml_showvalue(value->v_field.ref1);
		aml_showvalue(value->v_field.ref2);
		break;
	case AML_OBJTYPE_BUFFERFIELD:
		dnprintf(50, "bufferfield: type=%.4x pos=%.4x len=%.4x ", 
			 value->v_field.type,
			 value->v_field.bitpos,
			 value->v_field.bitlen);
		aml_dump(aml_bytelen(value->v_field.bitlen), 
			 value->v_field.ref1->v_buffer+aml_bytepos(value->v_field.bitpos));
		aml_showvalue(value->v_field.ref1);
		break;
	case AML_OBJTYPE_OPREGION:
		dnprintf(50, "opregion: %s,0x%llx,0x%x\n",
			 opregion(value->v_opregion.iospace),
			 value->v_opregion.iobase,
			 value->v_opregion.iolen);
		break;
	default:
		dnprintf(50, "unknown: %d\n", value->type);
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

	switch (rhs->type) {
	case AML_OBJTYPE_INTEGER:
		return aml_allocint(rhs->v_integer, rhs->length);
	case AML_OBJTYPE_STRING:
		return aml_allocvalue(AML_OBJTYPE_STRING,
				      rhs->length,
				      rhs->v_string,
				      "copystr");
	case AML_OBJTYPE_BUFFER:
		return aml_allocvalue(AML_OBJTYPE_BUFFER,
				      rhs->length,
				      rhs->v_buffer,
				      "copybuf");
	case AML_OBJTYPE_PACKAGE:
		return aml_allocvalue(AML_OBJTYPE_PACKAGE,
				      rhs->length,
				      rhs->v_package,
				      "copypkg");

	case AML_OBJTYPE_BUFFERFIELD:
	case AML_OBJTYPE_FIELDUNIT:
		rv = aml_allocvalue(rhs->type, 0, NULL, "field");
		if (rv != NULL) {
			rv->name = rhs->name;
			rv->length = rhs->length;
			rv->v_field = rhs->v_field;
		}
		return rv;

	default:
		dnprintf(10,"copy unknown : %x\n", rhs->type);
		rv = aml_allocvalue(rhs->type, 0, NULL, "any");
		*rv = *rhs;
		break;
	}
	return rv;
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

/* Decode AML Namestring from stream */
const char *
aml_parse_name(struct acpi_context *ctx)
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

struct aml_opcode aml_table[] = {
	/* Simple types */
	{ AMLOP_ZERO,		  "Zero",	     "!"  },
	{ AMLOP_ONE,		  "One",	     "!"  },
	{ AMLOP_ONES,		  "Ones",	     "!"  },
	{ AMLOP_BYTEPREFIX,	  "Byte",	     "b"  },
	{ AMLOP_WORDPREFIX,	  "Word",	     "w"  },
	{ AMLOP_DWORDPREFIX,	  "DWord",	     "d"  },
	{ AMLOP_QWORDPREFIX,	  "QWord",	     "q"  },
	{ AMLOP_REVISION,	  "Revision",	     ""	  },
	{ AMLOP_STRINGPREFIX,	  "String",	     "s"  },
	{ AMLOP_DEBUG,		  "DebugOp",	     "",  },
	{ AMLOP_BUFFER,		  "Buffer",	     "piB" },
	{ AMLOP_PACKAGE,	  "Package",	     "pbT" },
	{ AMLOP_VARPACKAGE,	  "VarPackage",	     "piT" },

	/* Simple objects */
	{ AMLOP_LOCAL0,		  "Local0",	     "",    },
	{ AMLOP_LOCAL1,		  "Local1",	     "",    },
	{ AMLOP_LOCAL2,		  "Local2",	     "",    },
	{ AMLOP_LOCAL3,		  "Local3",	     "",    },
	{ AMLOP_LOCAL4,		  "Local4",	     "",    },
	{ AMLOP_LOCAL5,		  "Local5",	     "",    },
	{ AMLOP_LOCAL6,		  "Local6",	     "",    },
	{ AMLOP_LOCAL7,		  "Local7",	     "",    },
	{ AMLOP_ARG0,		  "Arg0",	     "",    },
	{ AMLOP_ARG1,		  "Arg1",	     "",    },
	{ AMLOP_ARG2,		  "Arg2",	     "",    },
	{ AMLOP_ARG3,		  "Arg3",	     "",    },
	{ AMLOP_ARG4,		  "Arg4",	     "",    },
	{ AMLOP_ARG5,		  "Arg5",	     "",    },
	{ AMLOP_ARG6,		  "Arg6",	     "",    },

	/* Control flow */
	{ AMLOP_IF,		  "If",		     "piT",  },
	{ AMLOP_ELSE,		  "Else",	     "pT",   },
	{ AMLOP_WHILE,		  "While",	     "piT",  },
	{ AMLOP_BREAK,		  "Break",	     "",     },
	{ AMLOP_CONTINUE,	  "Continue",	     "",     },
	{ AMLOP_RETURN,		  "Return",	     "t",     },
	{ AMLOP_FATAL,		  "Fatal",	     "bdi", },
	{ AMLOP_NOP,		  "Nop",	     "",    },
	{ AMLOP_BREAKPOINT,	  "BreakPoint",	     "",    },

	/* Arithmetic operations */
	{ AMLOP_INCREMENT,	  "Increment",	     "t",     },
	{ AMLOP_DECREMENT,	  "Decrement",	     "t",     },
	{ AMLOP_ADD,		  "Add",	     "iit",   },
	{ AMLOP_SUBTRACT,	  "Subtract",	     "iit",   },
	{ AMLOP_MULTIPLY,	  "Multiply",	     "iit",   },
	{ AMLOP_DIVIDE,		  "Divide",	     "iitt",  },
	{ AMLOP_SHL,		  "ShiftLeft",	     "iit",   },
	{ AMLOP_SHR,		  "ShiftRight",	     "iit",   },
	{ AMLOP_AND,		  "And",	     "iit",   },
	{ AMLOP_NAND,		  "Nand",	     "iit",   },
	{ AMLOP_OR,		  "Or",		     "iit",   },
	{ AMLOP_NOR,		  "Nor",	     "iit",   },
	{ AMLOP_XOR,		  "Xor",	     "iit",   },
	{ AMLOP_NOT,		  "Not",	     "it",    },
	{ AMLOP_MOD,		  "Mod",	     "iit",   },
	{ AMLOP_FINDSETLEFTBIT,	  "FindSetLeftBit",  "it",    },
	{ AMLOP_FINDSETRIGHTBIT,  "FindSetRightBit", "it",    },

	/* Logical test operations */
	{ AMLOP_LAND,		  "LAnd",	     "ii",    },
	{ AMLOP_LOR,		  "LOr",	     "ii",    },
	{ AMLOP_LNOT,		  "LNot",	     "i",     },
	{ AMLOP_LNOTEQUAL,	  "LNotEqual",	     "tt",    },
	{ AMLOP_LLESSEQUAL,	  "LLessEqual",	     "tt",    },
	{ AMLOP_LGREATEREQUAL,	  "LGreaterEqual",   "tt",    },
	{ AMLOP_LEQUAL,		  "LEqual",	     "tt",    },
	{ AMLOP_LGREATER,	  "LGreater",	     "tt",    },
	{ AMLOP_LLESS,		  "LLess",	     "tt",    },

	/* Named objects */
	{ AMLOP_NAMECHAR,	  "NameRef",	     "n" },
	{ AMLOP_ALIAS,		  "Alias",	     "nN",  },
	{ AMLOP_NAME,		  "Name",	     "Nt",  },
	{ AMLOP_EVENT,		  "Event",	     "N",   },
	{ AMLOP_MUTEX,		  "Mutex",	     "Nb",  },
	{ AMLOP_DATAREGION,	  "DataRegion",	     "Nttt" },
	{ AMLOP_OPREGION,	  "OpRegion",	     "Nbii" },
	{ AMLOP_SCOPE,		  "Scope",	     "pNT" },
	{ AMLOP_DEVICE,		  "Device",	     "pNT" },
	{ AMLOP_POWERRSRC,	  "Power Resource",  "pNbwT" },
	{ AMLOP_THERMALZONE,	  "ThermalZone",     "pNT" },
	{ AMLOP_PROCESSOR,	  "Processor",	     "pNbdbT", },
	{ AMLOP_METHOD,		  "Method",	     "pNfM",  },

	/* Field operations */
	{ AMLOP_FIELD,		  "Field",	     "pnfF" },
	{ AMLOP_INDEXFIELD,	  "IndexField",	     "pntfF" },
	{ AMLOP_BANKFIELD,	  "BankField",	     "pnnifF" },
	{ AMLOP_CREATEFIELD,	  "CreateField",     "tiiN",   },
	{ AMLOP_CREATEQWORDFIELD, "CreateQWordField","tiN",    },
	{ AMLOP_CREATEDWORDFIELD, "CreateDWordField","tiN",    },
	{ AMLOP_CREATEWORDFIELD,  "CreateWordField", "tiN",    },
	{ AMLOP_CREATEBYTEFIELD,  "CreateByteField", "tiN",    },
	{ AMLOP_CREATEBITFIELD,	  "CreateBitField",  "tiN",    },

	/* Conversion operations */
	{ AMLOP_TOINTEGER,	  "ToInteger",	     "tt",     },
	{ AMLOP_TOBUFFER,	  "ToBuffer",	     "tt",     },
	{ AMLOP_TODECSTRING,	  "ToDecString",     "it",     },
	{ AMLOP_TOHEXSTRING,	  "ToHexString",     "it",     }, 
	{ AMLOP_TOSTRING,	  "ToString",	     "t",      },
	{ AMLOP_FROMBCD,	  "FromBCD",	     "it",     },
	{ AMLOP_TOBCD,		  "ToBCD",	     "it",     },
	{ AMLOP_MID,		  "Mid",	     "tiit",   },

	/* Mutex/Signal operations */
	{ AMLOP_ACQUIRE,	  "Acquire",	     "tw",     },
	{ AMLOP_RELEASE,	  "Release",	     "t",      },
	{ AMLOP_SIGNAL,		  "Signal",	     "t",      },
	{ AMLOP_WAIT,		  "Wait",	     "ti",     },
	{ AMLOP_RESET,		  "Reset",	     "t",      },
 
	{ AMLOP_INDEX,		  "Index",	     "tit",    },
	{ AMLOP_DEREFOF,	  "DerefOf",	     "t",      },
	{ AMLOP_REFOF,		  "RefOf",	     "t",      },
	{ AMLOP_CONDREFOF,	  "CondRef",	     "tt",     },

	{ AMLOP_LOADTABLE,	  "LoadTable",	     "tttttt" },
	{ AMLOP_STALL,		  "Stall",	     "i",      },
	{ AMLOP_SLEEP,		  "Sleep",	     "i",      },
	{ AMLOP_LOAD,		  "Load",	     "nt" },
	{ AMLOP_UNLOAD,		  "Unload",	     "t" }, 
	{ AMLOP_STORE,		  "Store",	     "tt",     },
	{ AMLOP_CONCAT,		  "Concat",	     "ttt" },
	{ AMLOP_CONCATRES,	  "ConcatRes",	     "ttt" },
	{ AMLOP_NOTIFY,		  "Notify",	     "ti" },
	{ AMLOP_SIZEOF,		  "Sizeof",	     "t",      },
	{ AMLOP_MATCH,		  "Match",	     "tbibii", },
	{ AMLOP_OBJECTTYPE,	  "ObjectType",	     "t", },
	{ AMLOP_COPYOBJECT,	  "CopyObject",	     "tt" },
	{ 0xFFFF }
};

/* Extract opcode from AML bytestream 
 *
 * Some opcodes are multibyte
 * Name strings can also be embedded within the stream
 */
struct aml_opcode *aml_getopcode(struct acpi_context *ctx)
{
	uint8_t *pc;
	struct aml_opcode *ptab;
	uint16_t twocode, opcode;

	/* Check if this is a name object */
	pc = ctx->pos;
	if (aml_isnamedop(*pc)) {
		opcode = AMLOP_NAMECHAR;
	}
	else {
		opcode = aml_parse_int(ctx, 1);
		twocode = (opcode << 8L) + *ctx->pos;

		/* Check multi-byte opcodes */
		if (twocode == AMLOP_LNOTEQUAL ||
		    twocode == AMLOP_LLESSEQUAL ||
		    twocode == AMLOP_LGREATEREQUAL ||
		    opcode == AMLOP_EXTPREFIX) {
			opcode = twocode;
			ctx->pos++;
		}
	}
	for (ptab = aml_table; ptab->opcode != 0xFFFF; ptab++) {
		if (ptab->opcode == opcode)
			return ptab;
	}
	return NULL;
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
		return aml_eval_object(sc, root, result, 0, env);
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

struct aml_value *aml_econcat(struct acpi_context *ctx);
struct aml_value *aml_ederef(struct acpi_context *ctx, struct aml_value *val);
void aml_resizeval(struct aml_value *pv, int newlen);
int aml_parse_length(struct acpi_context *ctx);
struct aml_value *aml_edebugobj;
struct aml_value *aml_eparseval(struct acpi_context *ctx);
struct aml_opcode *aml_getopcode(struct acpi_context *ctx);
struct aml_value *aml_domethod(struct acpi_context *ctx, struct aml_value *val);
struct aml_value *aml_esetnodevalue(struct acpi_context *ctx,  struct aml_value *lhs, 
				    struct aml_value *rhs, int64_t rval);
struct aml_value *aml_eparselist(struct acpi_context *ctx, uint8_t *end);

struct aml_node *_aml_searchname(struct aml_node *, const char *);
struct aml_node *aml_doname(struct aml_node *, const char *, int);
struct aml_node *aml_searchname(struct aml_node *, const char *);
struct aml_node *aml_createname(struct aml_node *, const char *);
struct aml_value *aml_doif(struct acpi_context *ctx);
struct aml_value *aml_eparsescope(struct acpi_context *ctx, const char *name, uint8_t *end,
				  struct aml_opcode *opc);
struct aml_value *aml_dowhile(struct acpi_context *ctx);
int64_t aml_eparseint(struct acpi_context *ctx);
struct aml_value *aml_efieldunit(struct acpi_context *ctx, int opcode);
struct aml_value *aml_ebufferfield(struct acpi_context *ctx, int bitlen, int size, int opcode);
uint8_t *aml_eparselen(struct acpi_context *ctx);
struct aml_value *aml_eparseref(struct acpi_context *ctx);
int64_t aml_val2int(struct acpi_context *ctx, struct aml_value *val);
struct aml_value *aml_efield(struct acpi_context *ctx, struct aml_value *e_fld,
			     struct aml_value *rhs);
struct aml_value *aml_val2buf(struct acpi_context *ctx, struct aml_value *val);
void aml_addvname(struct acpi_context *ctx, const char *name, int opcode,
		  struct aml_value *val);
struct aml_value *aml_eparsenode(struct acpi_context *ctx, struct aml_node *node);
void aml_delchildren(struct acpi_context *ctx, struct aml_node *node);
struct aml_value *aml_doloadtable(struct acpi_context *ctx);
struct aml_value *aml_domatch(struct acpi_context *ctx);

/* Search list of objects for a name match 
 *  Special case for fields: search children only
 */
struct aml_node *_aml_searchname(struct aml_node *list, const char *name)
{
	struct aml_node *child;

	if (list == NULL) {
		return NULL;
	}
	while (list) {
		if (list->opcode == AMLOP_FIELD ||
		    list->opcode == AMLOP_BANKFIELD ||
		    list->opcode == AMLOP_INDEXFIELD) {
			if ((child = _aml_searchname(list->child, name)) != NULL) {
				return child;
			}
		}
		if (list->name && !strncmp(list->name, name, AML_NAMESEG_LEN)) {
			return list;
		}
		list = list->sibling;
	}
	return NULL;
}

/* Create name references in tree, even if not initialized */
struct aml_node *aml_doname(struct aml_node *root, const char *name, int create)
{
	struct aml_node *tmp;

	if (*name == AMLOP_ROOTCHAR) {
		name++;
		root = &aml_root;
	}
	while (*name == AMLOP_PARENTPREFIX) {
		name++;
		if ((root = root->parent) == NULL) {
			return NULL;
		}
	}
	if (root == NULL) {
		root = &aml_root;
	}
	if (*name && name[AML_NAMESEG_LEN] == '\0' && !create) {
		do {
			tmp = _aml_searchname(root->child, name);
			root = root->parent;
		} while (tmp == NULL && root != NULL);
		return tmp;
	}

	for (tmp = root; tmp && *name; name += AML_NAMESEG_LEN) {
		if (*name == '.') name++;

		tmp = _aml_searchname(root->child, name);

		/* Create name if queried */
		if (tmp == NULL && create) {
			tmp = aml_create_node(root, -1, "DUMMY", root->start);
			if (tmp != NULL) {
				tmp->name = acpi_os_allocmem(AML_NAMESEG_LEN+1);
				if (tmp->name) {
					memcpy((char *)tmp->name, name, AML_NAMESEG_LEN);
				}
			}
		}
		root = tmp;
	}
	return tmp;
}

struct aml_node *aml_createname(struct aml_node *root, const char *name)
{
	return aml_doname(root, name, 1);
}

struct aml_node *aml_searchname(struct aml_node *root, const char *name)
{
	return aml_doname(root, name, 0);
}

struct aml_value *aml_ederef(struct acpi_context *ctx, struct aml_value *val)
{
	struct aml_node	 *pn;
	struct aml_value *ref;
	int64_t i1;

	if (val == NULL) {
		return NULL;
	}
	switch (val->type) {
	case AML_OBJTYPE_NAMEREF:
		if (val->v_objref.ref == NULL) {
			if ((pn = aml_searchname(ctx->scope, val->name)) != NULL) {
				val->v_objref.ref = pn->value;
			}
		}
		if (val->v_objref.ref != NULL) {
			return aml_ederef(ctx, val->v_objref.ref);
		}
		return NULL;
	case AML_OBJTYPE_OBJREF:
		i1 = val->v_objref.index;
		ref = aml_ederef(ctx, val->v_objref.ref);

		if (i1 == -1) {
			return aml_ederef(ctx, ref);
		}
		if (i1 > ref->length) {
			aml_resizeval(ref, i1);
		}
		switch (ref->type) {
		case AML_OBJTYPE_PACKAGE:
#if 1
			if (ref->v_package[i1] == NULL) {
				/* Lazy allocate package */
				dnprintf(10, "LazyPkg: %lld/%d\n", i1, ref->length);
				ref->v_package[i1] = aml_allocint(0, 0);
			}
#endif
			return ref->v_package[i1];
		case AML_OBJTYPE_BUFFER:
			return aml_allocvalue(AML_OBJTYPE_BUFFERFIELD, 8, ref->v_buffer+i1, "test");
		default:
			dnprintf(50,"Unknown refof\n");
		}
		break;
	}
	return val;
}

struct aml_value *aml_eparseref(struct acpi_context *ctx)
{
	struct aml_value *lhs, *rhs;

	lhs = aml_eparseval(ctx);
	rhs = aml_ederef(ctx, lhs);
	if (lhs != rhs) {
		aml_freevalue(&lhs);
	}
	return rhs;
}

struct aml_value *
aml_efield(struct acpi_context *ctx, struct aml_value *e_fld,
	   struct aml_value *rhs)
{
	struct aml_value *e_rgn;
	struct aml_value *rv;
	struct aml_value  tmp;

	dnprintf(10, "efield %s: ", rhs ? "set" : "get");
	aml_showvalue(e_fld);
	aml_showvalue(rhs);

	tmp.type = AML_OBJTYPE_INTEGER;
	switch (e_fld->v_field.type) {
	case AMLOP_INDEXFIELD:
		/* Set INDEX value to FIELD position byte, then write RHS to DATA */
		if (e_fld->v_field.bitpos & 7) {
			dnprintf(10, "aml_efield: INDEXFIELD not byte-aligned..\n");
		}
		tmp.v_integer = e_fld->v_field.bitpos >> 3;
		aml_efield(ctx, e_fld->v_field.ref1, &tmp);
		return aml_efield(ctx, e_fld->v_field.ref2, rhs);

	case AMLOP_BANKFIELD:
		/* Set bank value */
		tmp.v_integer = e_fld->v_field.ref3;
		aml_efield(ctx, e_fld->v_field.ref2, &tmp);
		return aml_efield(ctx, e_fld->v_field.ref2, rhs);

	case AMLOP_FIELD:
		/* e_rgn should be OPREGION */
		e_rgn = aml_ederef(ctx, e_fld->v_field.ref1);
		if (e_rgn->type != AML_OBJTYPE_OPREGION) {
			dnprintf(10, "aml_efield: Wrong FIELD type!\n");
			return NULL;
		}
		if (AML_FIELD_LOCK(e_fld->v_field.flags)) {
			acpi_mutex_acquire(ctx, aml_global_lock, -1);
		}
		rv = aml_allocvalue(AML_OBJTYPE_BUFFER, aml_bytelen(e_fld->v_field.bitlen), NULL, 0);
		if (rhs == NULL) {
			/* Read field */
			if (rv != NULL) {
				acpi_gasio(ctx->sc, ACPI_IOREAD,
					   e_rgn->v_opregion.iospace,
					   e_rgn->v_opregion.iobase + aml_bytepos(e_fld->v_field.bitpos),
					   AML_FIELD_ACCESS(e_fld->v_field.flags),
					   aml_bytelen(e_fld->v_field.bitlen), rv->v_buffer);
			}
			if (AML_FIELD_LOCK(e_fld->v_field.flags)) {
				acpi_mutex_release(ctx, aml_global_lock);
			}
			return rv;
		}
    
		/* Write field */
		rhs = aml_val2buf(ctx, rhs);
		aml_showvalue(rhs);

		switch (AML_FIELD_UPDATE(e_fld->v_field.flags)) {
		case AML_FIELD_PRESERVE:
			/* XXX: fix length, don't read if whole length */
			dnprintf(10, "old iobase = %llx,%lx\n", e_rgn->v_opregion.iobase, aml_bytepos(e_fld->v_field.bitpos));
			acpi_gasio(ctx->sc, ACPI_IOREAD,
				   e_rgn->v_opregion.iospace,
				   e_rgn->v_opregion.iobase + aml_bytepos(e_fld->v_field.bitpos),
				   AML_FIELD_ACCESS(e_fld->v_field.flags),
				   aml_bytelen(e_fld->v_field.bitlen), rv->v_buffer);
			aml_showvalue(rv);
			break;
		case AML_FIELD_WRITEASONES:
			memset(rv->v_buffer, 0xFF, rv->length);
			break;
		case AML_FIELD_WRITEASZEROES:
			memset(rv->v_buffer, 0x00, rv->length);
			break;
		}
		aml_bufcpy(rv->v_buffer, e_fld->v_field.bitpos, 
			   rhs->v_buffer, 0, 
			   e_fld->v_field.bitlen);
		acpi_gasio(ctx->sc, ACPI_IOWRITE,
			   e_rgn->v_opregion.iospace,
			   e_rgn->v_opregion.iobase + aml_bytepos(e_fld->v_field.bitpos),
			   AML_FIELD_ACCESS(e_fld->v_field.flags),
			   aml_bytelen(e_fld->v_field.bitlen), rv->v_buffer);
		if (AML_FIELD_LOCK(e_fld->v_field.flags)) {
			acpi_mutex_release(ctx, aml_global_lock);
		}
		break;
	default:
		/* This is a buffer field */
		e_rgn = aml_ederef(ctx, e_fld->v_field.ref1);
		if (e_rgn->type != AML_OBJTYPE_BUFFER) {
			dnprintf(10, "aml_efield: Wrong type!\n");
			return NULL;
		}
		if (rhs == NULL) {
			/* Read buffer */
			rv = aml_allocvalue(AML_OBJTYPE_BUFFER, aml_bytelen(e_fld->v_field.bitlen), 
					    NULL, 0);
			if (rv != NULL) {
				aml_bufcpy(rv->v_buffer, 0, 
					   e_rgn->v_buffer, e_fld->v_field.bitpos, 
					   e_fld->v_field.bitlen);
			}
			return rv;
		}

		/* Write buffer */
		rhs = aml_val2buf(ctx, rhs);
		aml_bufcpy(e_rgn->v_buffer, e_fld->v_field.bitpos, 
			   rhs->v_buffer, 0, 
			   e_fld->v_field.bitlen);
		break;
	}
	return NULL;
}

struct aml_value *
aml_val2buf(struct acpi_context *ctx, struct aml_value *val)
{
	if (val == NULL)
		return NULL;
	switch (val->type) {
	case AML_OBJTYPE_BUFFER:
		return val;
	case AML_OBJTYPE_BUFFERFIELD:
	case AML_OBJTYPE_FIELDUNIT:
		return aml_efield(ctx, val, NULL);
	case AML_OBJTYPE_NAMEREF:
		return aml_val2buf(ctx, aml_ederef(ctx, val));
	case AML_OBJTYPE_INTEGER:
		return aml_allocvalue(AML_OBJTYPE_BUFFER, 8, &val->v_integer, "");
	case AML_OBJTYPE_STRING:
		return aml_allocvalue(AML_OBJTYPE_BUFFER, val->length, val->v_string, "");
	default:
		dnprintf(10, "Unknown val2buf : %d\n", val->type);
		return NULL;
	}    
}

int64_t
aml_val2int(struct acpi_context *ctx, struct aml_value *val)
{
	int64_t rval;

	if (val == NULL) {
		dnprintf(10, "null val2int\n");
		return 0;
	}
	switch (val->type) {
	case AML_OBJTYPE_BUFFER:
		rval = 0;
		if (val->length < 8) {
			memcpy(&rval, val->v_buffer, val->length);
		}
		return rval;
	case AML_OBJTYPE_INTEGER:
		return val->v_integer;
	case AML_OBJTYPE_STRING:
/* XXX: fix me

if (val->v_string == NULL) {
rval = 0;
}
else if (!strncmp(val->v_string, "0x", 2)) {
sscanf(val->v_string+2, "%Lx", &rval);
}
else {
rval = strtol(val->v_string);
}
*/
		return rval;
	case AML_OBJTYPE_NAMEREF:
	case AML_OBJTYPE_OBJREF:
		val = aml_ederef(ctx, val);
		break;
	case AML_OBJTYPE_BUFFERFIELD:
	case AML_OBJTYPE_FIELDUNIT:
		val = aml_efield(ctx, val, NULL);
		break;
	case AML_OBJTYPE_METHOD:
		val = aml_domethod(ctx, val);
		break;
	default:
		dnprintf(10, "Unknown val2int: %x\n", val->type);
		return 0x0;
	}
	return aml_val2int(ctx, val);
}

void
aml_addvname(struct acpi_context *ctx, const char *name, int opcode,
	     struct aml_value *val)
{
	struct aml_node *pn;

	val->name   = name;
	val->refcnt = -1;
	pn = aml_createname(ctx->scope, name);
	if (pn != NULL) {
		pn->opcode = opcode;
		if (!pn->value) {
			pn->value  = val;
		}
		aml_showvalue(val);
	}
	dnprintf(10, "\n");
}

/* Parse package length & return pointer to end of package */
uint8_t *
aml_eparselen(struct acpi_context *ctx)
{
	uint8_t *pos = ctx->pos;

	return pos + aml_parse_length(ctx);
}

/* Return integer value */
int64_t
aml_eparseint(struct acpi_context *ctx)
{
	struct aml_value *rv;
	int64_t rval;

	rv = aml_eparseval(ctx);
	rval = aml_val2int(ctx, rv);
	aml_freevalue(&rv);

	return rval;
}

/* Create field unit 
 *   AMLOP_FIELD
 *   AMLOP_INDEXFIELD
 *   AMLOP_BANKFIELD
 */
#define AML_FIELD_RESERVED  0x00
#define AML_FIELD_ATTRIB    0x01

struct aml_value *
aml_efieldunit(struct acpi_context *ctx, int opcode)
{
	uint8_t *end;
	int attr, access;
	struct aml_value *rv, tmp;

	/* Create field template */
	memset(&tmp, 0, sizeof(tmp));
	tmp.type = AML_OBJTYPE_FIELDUNIT;
	tmp.v_field.type = opcode;

	end = aml_eparselen(ctx);
	switch (opcode) {
	case AMLOP_FIELD:
		tmp.v_field.ftyp = "field";
		tmp.v_field.ref1 = aml_eparseval(ctx);
		break;
	case AMLOP_INDEXFIELD:
		tmp.v_field.ftyp = "index";
		tmp.v_field.ref1 = aml_eparseval(ctx);
		tmp.v_field.ref2 = aml_eparseval(ctx);
		break;
	case AMLOP_BANKFIELD:
		tmp.v_field.ftyp = "bank";
		tmp.v_field.ref1 = aml_eparseval(ctx);
		tmp.v_field.ref2 = aml_eparseval(ctx);
		tmp.v_field.ref3 = aml_eparseint(ctx);
		break;
	}
	tmp.v_field.flags = aml_parse_int(ctx, 1);

	while (ctx->pos < end) {
		switch (*ctx->pos) {
		case AML_FIELD_RESERVED:
			ctx->pos++;
			tmp.v_field.bitlen = aml_parse_length(ctx);
			break;
		case AML_FIELD_ATTRIB:
			ctx->pos++;
			access = aml_parse_int(ctx, 1);
			attr   = aml_parse_int(ctx, 1);

			tmp.v_field.flags &= ~AML_FIELD_ACCESSMASK;
			tmp.v_field.flags |= (access & AML_FIELD_ACCESSMASK);
			break;
		default:
			tmp.name = aml_parse_name(ctx);
			tmp.v_field.bitlen = aml_parse_length(ctx);
			rv = aml_copyvalue(&tmp);
			if (rv != NULL) {
				aml_addvname(ctx, tmp.name, opcode, rv);
			}
			break;
		}
		tmp.v_field.bitpos += tmp.v_field.bitlen;
	}
	return NULL;
}

/*
 * Create buffer field object
 *
 *  AMLOP_CREATEFIELD
 *  AMLOP_CREATEBITFIELD
 *  AMLOP_CREATEBYTEFIELD
 *  AMLOP_CREATEWORDFIELD
 *  AMLOP_CREATEDWORDFIELD
 *  AMLOP_CREATEQWORDFIELD
 */
struct aml_value *
aml_ebufferfield(struct acpi_context *ctx, int bitlen, int size, int opcode)
{
	struct aml_value *rv;

	rv = aml_allocvalue(AML_OBJTYPE_BUFFERFIELD, 0, NULL, "createbf");
	if (rv != NULL) {
		rv->v_field.type = opcode;
		rv->v_field.ref1 = aml_eparseval(ctx);
		rv->v_field.bitpos = aml_eparseint(ctx) * size;
		if (opcode == AMLOP_CREATEFIELD) {
			bitlen = aml_eparseint(ctx);
		}
		rv->name = aml_parse_name(ctx);
		rv->v_field.bitlen = bitlen;

		aml_addvname(ctx, rv->name, opcode, rv);
	}
	return rv;
}

/* Set node value */
struct aml_value *
aml_esetnodevalue(struct acpi_context *ctx,  struct aml_value *lhs, 
		  struct aml_value *rhs, int64_t rval)
{
	struct aml_value *tmp;

	if (rhs == NULL) {
		rhs = aml_allocint(rval, 0);
	}

	/* Eval RHS */
	switch (rhs->type) {
	case AML_OBJTYPE_FIELDUNIT:
	case AML_OBJTYPE_BUFFERFIELD:
		rhs = aml_efield(ctx, rhs, NULL);
		break;
	case AML_OBJTYPE_NAMEREF:
	case AML_OBJTYPE_OBJREF:
		tmp = aml_ederef(ctx, rhs);
		if (tmp == rhs) {
			dnprintf(10, "No deref!\n");
		}
		rhs = tmp;
		break;
	}

	/* Eval LHS */
	switch (lhs->type) {
	case AML_OBJTYPE_INTEGER:
		if (lhs->length == 0) {
			lhs->v_integer = aml_val2int(ctx, rhs);
		}
		break;
	case AML_OBJTYPE_STRING:
		if (rhs->type != AML_OBJTYPE_STRING) {
			dnprintf(10, "no string...\n");
		}
		else {
			lhs->length   = rhs->length;
			lhs->v_string = rhs->v_string;
		}
		break;
	case AML_OBJTYPE_BUFFER:
		rhs = aml_val2buf(ctx, rhs);
		lhs->length = rhs->length;
		lhs->v_buffer = rhs->v_buffer;
		break;
	case AML_OBJTYPE_BUFFERFIELD:
	case AML_OBJTYPE_FIELDUNIT:
		aml_efield(ctx, lhs, rhs);
		break;
	case AML_OBJTYPE_UNINITIALIZED:
		*lhs = *rhs;
		break;
	case AML_OBJTYPE_OBJREF:
		rhs = aml_esetnodevalue(ctx, aml_ederef(ctx, lhs), rhs, 0);
		break;
	case AML_OBJTYPE_DEBUGOBJ:
		//aml_freevalue(&lhs->v_debug);
		//lhs->v_debug = aml_copyvalue(rhs);
		break;
	default:
		dnprintf(10, "Unknown set value: %x\n", lhs->type);
	}

	return rhs;
}

/* Parse list of objects */
struct aml_value *aml_eparselist(struct acpi_context *ctx, uint8_t *end)
{
	struct aml_value *rv;
  
	rv = NULL;
	while (ctx->pos && ctx->pos < end) {
		aml_freevalue(&rv);
		rv = aml_eparseval(ctx);
	}
	return rv;
}

/* Concatenate results */
struct aml_value *aml_econcat(struct acpi_context *ctx)
{
	struct aml_value  *lhs, *rhs, *set, *tmp;

	lhs = aml_eparseval(ctx);
	rhs = aml_eparseval(ctx);
	set = aml_eparseval(ctx);
	if (lhs == NULL || rhs == NULL || lhs->type != rhs->type) {
		return NULL;
	}
	switch (lhs->type) {
	case AML_OBJTYPE_STRING:
		tmp = aml_allocvalue(AML_OBJTYPE_STRING,
				     lhs->length+rhs->length, NULL, "concat");
		if (tmp != NULL) {
			strlcpy(tmp->v_string, lhs->v_string, lhs->length);
			strlcat(tmp->v_string, rhs->v_string, rhs->length);
			return aml_esetnodevalue(ctx, set, tmp, 0);
		}
		break;
	case AML_OBJTYPE_BUFFER:
		tmp = aml_allocvalue(AML_OBJTYPE_BUFFER,
				     lhs->length+rhs->length, NULL, "concat");
		if (tmp != NULL) {
			memcpy(tmp->v_buffer, lhs->v_buffer, lhs->length);
			memcpy(tmp->v_buffer+lhs->length, rhs->v_buffer, rhs->length);
			return aml_esetnodevalue(ctx, set, tmp, 0);
		}
		break;
	}
	return NULL;
}

struct aml_value *aml_eparsescope(struct acpi_context *ctx, const char *name, uint8_t *end,
				  struct aml_opcode *opc)
{
	struct aml_node *oldscope;
	struct aml_value *rv;

	oldscope = ctx->scope;
	ctx->scope = aml_createname(ctx->scope, name);
	ctx->scope->mnem = opc->mnem;
	ctx->scope->opcode = opc->opcode;
	rv = aml_eparselist(ctx, end);
	ctx->scope = oldscope;
	return rv;
}

/* Parse AMLOP_IF/AMLOP_ELSE block */
struct aml_value *aml_doif(struct acpi_context *ctx)
{
	struct aml_value *rv;
	uint8_t *end;
	int64_t	 i1;

	rv  = NULL;
	end = aml_eparselen(ctx);
	i1  = aml_eparseint(ctx);
	dnprintf(10,"evalif: %lld\n", i1);
	if (i1 != 0) {
		/* Parse IF block */
		rv = aml_eparselist(ctx, end);
	}
	if (*end == AMLOP_ELSE) {
		/* Parse ELSE block */
		ctx->pos = ++end;
		end = aml_eparselen(ctx);
		if (i1 == 0) {
			rv = aml_eparselist(ctx, end);
		}
	}
	ctx->pos = end;
	return rv;
}

/* Parse AMLOP_WHILE/AMLOP_BREAK/AMLOP_CONTINUE block */
struct aml_value *aml_dowhile(struct acpi_context *ctx)
{
	uint8_t *start, *end;
	struct aml_value *rv;
	int64_t i1, iw=0;

	end   = aml_eparselen(ctx);
	start = ctx->pos;

	rv = NULL;
	for(;;) {
		if (ctx->pos == end) {
			ctx->pos = start;
		}
		/* Perform test condition */
		if (ctx->pos == start) {
			i1 = aml_eparseint(ctx);
			dnprintf(10, "whiletest: %lld\n", i1);
			if (i1 == 0) {
				break;
			}
		}
		if (*ctx->pos == AMLOP_BREAK) {
			dnprintf(10,"break\n");
			break;
		}
		else if (*ctx->pos == AMLOP_CONTINUE) {
			dnprintf(10,"continue\n");
			ctx->pos = start;
		}
		else {
			aml_freevalue(&rv);
			rv = aml_eparseval(ctx);
		}
		if (iw++ > 10) break;
	}
	return rv;
}

struct aml_value *aml_domethod(struct acpi_context *ctx, struct aml_value *val)
{
	int64_t i1, i2;
	struct aml_value **newarg, **oldarg;
	struct aml_value **newloc, **oldloc;
	struct aml_value *rv;
	uint8_t *oldpos;

	oldarg = ctx->args;
	oldloc = ctx->locals;

	if (val->type != AML_OBJTYPE_METHOD) {
		dnprintf(10, "aml_domethod: Invalid type\n");
	}

	i2 = AML_METHOD_ARGCOUNT(val->v_method.flags);
	newarg = (struct aml_value **)acpi_os_allocmem(sizeof(struct aml_value *)*8);
	newloc = (struct aml_value **)acpi_os_allocmem(sizeof(struct aml_value *)*8);

	/* Parse arguments */
	for (i1 = 0; i1<i2; i1++) {
		newarg[i1] = aml_eparseval(ctx);
	}
	dnprintf(10,"\nCall %s: (%lld args)\n", val->name, i2);
	for (i1 = 0; i1<i2; i1++) {
		dnprintf(10,"  arg%lld: ", i1);
		aml_showvalue(newarg[i1]);
	}

	/* Save old parse position, call method */
	oldpos = ctx->pos;
	ctx->pos = val->v_method.start;
	ctx->args = newarg;
	ctx->locals = newloc;
	rv = aml_eparselist(ctx, val->v_method.end);
	ctx->pos = oldpos;
	ctx->args = oldarg;
	ctx->locals = oldloc;

	for (i1=0; i1<8; i1++) {
		aml_freevalue(&newloc[i1]);
	}
	for (i1=0; i1<i2; i1++) {
		aml_freevalue(&newarg[i1]);
	}
	return rv;
  
}

/* Resize buffer/string/package if out-of-bounds access */
void aml_resizeval(struct aml_value *pv, int newlen)
{
	struct aml_value **oldpkg;
	int i1;

	++newlen;
	dnprintf(10, "supersizeme\n");
	switch (pv->type) {
	case AML_OBJTYPE_PACKAGE:
		oldpkg = pv->v_package;
		pv->v_package = (struct aml_value **)acpi_os_allocmem(newlen * sizeof(struct aml_value *));

		/* Assign old package values */
		for (i1 = 0; i1 < pv->length; i1++) {
			pv->v_package[i1] = oldpkg[i1];
		}

		/* Free old package */
		acpi_os_freemem(oldpkg);

		/* Set new length */
		pv->length = newlen-1;

		break;
	}
}

struct aml_value *aml_doloadtable(struct acpi_context *ctx)
{
	aml_eparseval(ctx);
	aml_eparseval(ctx);
	aml_eparseval(ctx);
	aml_eparseval(ctx);
	aml_eparseval(ctx);
	aml_eparseval(ctx);
	aml_eparseval(ctx);
	return NULL;
}

struct aml_value *aml_domatch(struct acpi_context *ctx)
{
	struct aml_value *lhs, *rv;
	int64_t op1, op2, mv1, mv2, idx, mval;

	lhs = aml_eparseval(ctx);
	op1 = aml_parse_int(ctx,1);
	mv1 = aml_eparseint(ctx);
	op2 = aml_parse_int(ctx,1);
	mv2 = aml_eparseint(ctx);
	idx = aml_eparseint(ctx);

	/* ASSERT: lhs is package */
	rv = aml_allocint(-1, 0);
	if (lhs->type == AML_OBJTYPE_PACKAGE) {
		for (; idx < lhs->length; idx++) {
			mval = aml_val2int(ctx,lhs->v_package[idx]);
			if (aml_match(mval, op1, mv1) && aml_match(mval, op2, mv2)) {
				rv->v_integer = idx;
				break;
			}
		}
	}
	aml_freevalue(&lhs);
	return rv;
}

struct aml_value *
aml_eparseval(struct acpi_context *ctx)
{
	struct aml_opcode *opc;
	struct aml_value  *lhs, *rhs, *tmp, *rv;
	uint8_t *end, *start;
	const char *name;
	int64_t i1, i2;

	rhs = NULL;
	lhs = NULL;
	tmp = NULL;
	rv  = NULL;

	/* Allocate a new instruction, get opcode, etc */
	start = ctx->pos;
	opc = aml_getopcode(ctx);

	dnprintf(10, "### %2d %.4x %s\n",
		 ctx->depth, opc->opcode, opc->mnem);

	ctx->depth++;
	end = NULL;
	switch (opc->opcode) {
	case AMLOP_NAMECHAR:
		name = aml_parse_name(ctx);
		rv = aml_allocvalue(AML_OBJTYPE_NAMEREF, 0, (char *)name, "nameref");
		if ((rhs = aml_ederef(ctx, rv)) != NULL) {
			if (rhs->type == AML_OBJTYPE_METHOD) {
				rv = aml_domethod(ctx, rhs);
			}
			else {
				rv = rhs;
				rhs = NULL;
			}
		}
		break;
	case AMLOP_NOP:
		break;
	case AMLOP_ZERO:
		rv = aml_allocint(0,1);
		aml_showvalue(rv);
		break;
	case AMLOP_ONE:
		rv = aml_allocint(1,1);
		aml_showvalue(rv);
		break;
	case AMLOP_ONES:
		rv = aml_allocint(-1LL, 1);
		aml_showvalue(rv);
		break;
	case AMLOP_REVISION:
		rv = aml_allocint(AML_REVISION, 1);
		aml_showvalue(rv);
		break;
	case AMLOP_BYTEPREFIX:
		rv = aml_allocint(aml_parse_int(ctx, 1), 0);
		aml_showvalue(rv);
		break;
	case AMLOP_WORDPREFIX:
		rv = aml_allocint(aml_parse_int(ctx, 2), 0);
		aml_showvalue(rv);
		break;
	case AMLOP_DWORDPREFIX:
		rv = aml_allocint(aml_parse_int(ctx, 4), 0);
		aml_showvalue(rv);
		break;
	case AMLOP_QWORDPREFIX:
		rv = aml_allocint(aml_parse_int(ctx, 8), 0);
		aml_showvalue(rv);
		break;
	case AMLOP_STRINGPREFIX:
		rv = aml_allocstr(ctx->pos);
		if (rv != NULL){
			ctx->pos += rv->length+1;
		}
		aml_showvalue(rv);
		break;
	case AMLOP_FIELD:
	case AMLOP_INDEXFIELD:
	case AMLOP_BANKFIELD:
		rv = aml_efieldunit(ctx, opc->opcode);
		break;
	case AMLOP_CREATEFIELD:
		rv = aml_ebufferfield(ctx, -1, 1, opc->opcode);
		break;
	case AMLOP_CREATEBITFIELD:
		rv = aml_ebufferfield(ctx, 1,  1, opc->opcode);
		break;
	case AMLOP_CREATEBYTEFIELD:
		rv = aml_ebufferfield(ctx, 8,  8, opc->opcode);
		break;
	case AMLOP_CREATEWORDFIELD:
		rv = aml_ebufferfield(ctx, 8, 16, opc->opcode);
		break;
	case AMLOP_CREATEDWORDFIELD:
		rv = aml_ebufferfield(ctx, 8, 32, opc->opcode);
		break;
	case AMLOP_CREATEQWORDFIELD:
		rv = aml_ebufferfield(ctx, 8, 64, opc->opcode);
		break;
	case AMLOP_DEBUG:
		if (aml_edebugobj == NULL) {
			aml_edebugobj = aml_allocvalue(AML_OBJTYPE_DEBUGOBJ, 0, NULL, "debug");
		}
		rv = aml_edebugobj;
		break;
	case AMLOP_BUFFER:
		end = aml_eparselen(ctx);
		i2  = aml_eparseint(ctx);     // requested length
		i1  = end - ctx->pos;	      // supplied length

		rv = aml_allocvalue(AML_OBJTYPE_BUFFER, i2, NULL, "buffer");
		if (i1 > 0) {
			memcpy(rv->v_buffer, ctx->pos, i1);
		}
		break;
	case AMLOP_PACKAGE:
	case AMLOP_VARPACKAGE:
		end = aml_eparselen(ctx);

		/* Packages have fixed length, varpkg is variable */
		i2 = (opc->opcode == AMLOP_PACKAGE) ?
			aml_parse_int(ctx, 1) :
			aml_eparseint(ctx);

		rv = aml_allocvalue(AML_OBJTYPE_PACKAGE, i2, NULL, "package");
		for (i1=0; i1 < i2 && ctx->pos < end; i1++) {
			rv->v_package[i1] = aml_eparseval(ctx);
		}
#if 0
		for (; i1 < rv->length; i1++) {
			rv->v_package[i1] = aml_allocint(0,0);
		}
#endif
		dnprintf(10, "package: %lld of %lld parsed\n", i1, i2);
		break;
	case AMLOP_LOCAL0:
	case AMLOP_LOCAL1:
	case AMLOP_LOCAL2:
	case AMLOP_LOCAL3:
	case AMLOP_LOCAL4:
	case AMLOP_LOCAL5:
	case AMLOP_LOCAL6:
	case AMLOP_LOCAL7:
		i1 = opc->opcode - AMLOP_LOCAL0;
		if (ctx->locals[i1] == NULL) {
			/* Lazy allocate LocalX */
			dnprintf(10, "LazyLocal%lld\n", i1);
			ctx->locals[i1] = aml_allocint(0, 0);
		}
		else {
			ctx->locals[i1]->refcnt++;
		}
		rv = ctx->locals[i1];
		break;
	case AMLOP_ARG0:
	case AMLOP_ARG1:
	case AMLOP_ARG2:
	case AMLOP_ARG3:
	case AMLOP_ARG4:
	case AMLOP_ARG5:
	case AMLOP_ARG6:
		i1 = opc->opcode - AMLOP_ARG0;
		if (ctx->args[i1] == NULL) {
			/* Lazy allocate ArgX - shouldn't happen? */
			dnprintf(10, "LazyArg%lld\n", i1);
			ctx->args[i1] = aml_allocint(0, 0);
		}
		rv = ctx->args[i1];
		break;

	case AMLOP_INCREMENT:
	case AMLOP_DECREMENT:
		lhs = aml_eparseval(ctx);
		i1  = aml_val2int(ctx, lhs);
		rv  = aml_esetnodevalue(ctx, lhs, NULL, aml_evalmath(opc->opcode, i1, 1));
		break;
	case AMLOP_FINDSETLEFTBIT:
	case AMLOP_FINDSETRIGHTBIT:
	case AMLOP_NOT:
		i1  = aml_eparseint(ctx);
		lhs = aml_eparseval(ctx);
		rv  = aml_esetnodevalue(ctx, lhs, NULL, aml_evalmath(opc->opcode, i1, 0));
		break;
	case AMLOP_DIVIDE:
		i1  = aml_eparseint(ctx);
		i2  = aml_eparseint(ctx);

		lhs = aml_eparseval(ctx);
		rhs = aml_esetnodevalue(ctx, lhs, NULL, aml_evalmath(AMLOP_MOD, i1, i2));
		aml_freevalue(&lhs);

		lhs = aml_eparseval(ctx);
		rv  = aml_esetnodevalue(ctx, lhs, NULL, aml_evalmath(AMLOP_DIVIDE, i1, i2));
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
		i1  = aml_eparseint(ctx);
		i2  = aml_eparseint(ctx);
		lhs = aml_eparseval(ctx);
		rv  = aml_esetnodevalue(ctx, lhs, NULL, aml_evalmath(opc->opcode, i1, i2));
		break;
	case AMLOP_LAND:
	case AMLOP_LOR:
		i1 = aml_eparseint(ctx);
		i2 = aml_eparseint(ctx);
		rv = aml_allocint(aml_testlogical(opc->opcode, i1, i2),0);
		break;
	case AMLOP_LNOT:
		i1 = aml_eparseint(ctx);
		rv = aml_allocint(!i1, 1);
		break;
	case AMLOP_LLESS:
	case AMLOP_LLESSEQUAL:
	case AMLOP_LEQUAL:
	case AMLOP_LNOTEQUAL:
	case AMLOP_LGREATEREQUAL:
	case AMLOP_LGREATER:
		lhs = aml_eparseval(ctx);
		rhs = aml_eparseval(ctx);
		i1  = aml_comparevalue(opc->opcode, lhs, rhs);
		rv  = aml_allocint(i1, 0);
		break;
	case AMLOP_TOINTEGER:
		i1  = aml_eparseint(ctx);
		lhs = aml_eparseval(ctx);
		rv  = aml_esetnodevalue(ctx, lhs, NULL, i1);
		break;
	case AMLOP_FROMBCD:
		i1  = aml_eparseint(ctx);
		lhs = aml_eparseval(ctx);
		rv  = aml_esetnodevalue(ctx, lhs, NULL, aml_bcd2dec(i1));
		break;
	case AMLOP_TOBCD:
		i1  = aml_eparseint(ctx);
		lhs = aml_eparseval(ctx);
		rv  = aml_esetnodevalue(ctx, lhs, NULL, aml_dec2bcd(i1));
		break;
	case AMLOP_MID:
		rhs = aml_eparseval(ctx);
		i1  = aml_eparseint(ctx);  // index
		i2  = aml_eparseint(ctx);  // length
		if (rhs != NULL) {
			switch (rhs->type) {
			case AML_OBJTYPE_STRING:
				/* Validate index is within range */
				if (i1 >= lhs->length) 
					i1 = i2 = 0;
				if (i1+i2 >= lhs->length) 
					i2 = lhs->length - i1;
				rv = aml_allocvalue(AML_OBJTYPE_STRING, i2, lhs->v_string + i1, "mid");
				aml_esetnodevalue(ctx, lhs, rv, 0);
				break;
			case AML_OBJTYPE_BUFFER:
				if (i1 >= lhs->length) 
					i1 = i2 = 0;
				if (i1+i2 >= lhs->length) 
					i2 = lhs->length - i1;
				rv = aml_allocvalue(AML_OBJTYPE_BUFFER, i2, lhs->v_buffer + i1, "mid");
				aml_esetnodevalue(ctx, lhs, rv, 0);
				break;
			}
		}
		break;
	case AMLOP_TOSTRING:
		rhs = aml_eparseval(ctx);
		i1  = aml_eparseint(ctx);
		lhs = aml_eparseval(ctx);

		tmp = aml_val2buf(ctx, rhs);
		if (i1 > tmp->length) {
			i1 = tmp->length;
		}
		for(i2=0; i2<i1; i1++) {
			if (tmp->v_buffer[i2] == 0) {
				break;
			}
		}
		rv  = aml_allocvalue(AML_OBJTYPE_STRING, i2, tmp->v_buffer, "");
		aml_esetnodevalue(ctx, lhs, rv, 0);
		break;
	case AMLOP_TOBUFFER:
		rhs = aml_eparseval(ctx);
		lhs = aml_eparseval(ctx);
		rv  = aml_val2buf(ctx, lhs);
		aml_esetnodevalue(ctx, lhs, rv, 0);
		break;
	case AMLOP_TODECSTRING:
		i1  = aml_eparseint(ctx);
		lhs = aml_eparseval(ctx);
		rv = aml_allocvalue(AML_OBJTYPE_STRING, AML_INTSTRLEN, NULL, "todec");
		if (rhs != NULL) {
			snprintf(rhs->v_string, AML_INTSTRLEN, "%lld", i1);
			aml_esetnodevalue(ctx, lhs, rv, 0);
		}
		break;
	case AMLOP_TOHEXSTRING:
		i1  = aml_eparseint(ctx);
		lhs = aml_eparseval(ctx);
		rv  = aml_allocvalue(AML_OBJTYPE_STRING, AML_INTSTRLEN, NULL, "todec");
		if (rhs != NULL) {
			snprintf(rhs->v_string, AML_INTSTRLEN, "0x%Lx", i1);
			aml_esetnodevalue(ctx, lhs, rv, 0);
		}
		break;
	case AMLOP_STALL:
		i1 = aml_eparseint(ctx);
		dnprintf(10, "stall %lld usecs\n", i1);
		break;
	case AMLOP_SLEEP:
		i1 = aml_eparseint(ctx);
		dnprintf(10, "sleep %lld msecs\n", i1);
		break;
	case AMLOP_MUTEX:
		name = aml_parse_name(ctx);
		i1 = aml_parse_int(ctx, 1);
		rv = aml_allocvalue(AML_OBJTYPE_MUTEX, i1, (char *)name, "mutex");
		if (rv != NULL) {
			aml_addvname(ctx, name, opc->opcode, rv);
		}
		break;
	case AMLOP_ACQUIRE:
		lhs = aml_eparseref(ctx);
		i1  = aml_parse_int(ctx, 2);
		acpi_mutex_acquire(ctx, &lhs->v_mutex, i1);
		rv = aml_allocint(0,0);
		break;
	case AMLOP_RELEASE:
		lhs = aml_eparseref(ctx);
		acpi_mutex_release(ctx, &lhs->v_mutex);
		break;
	case AMLOP_EVENT:
		name = aml_parse_name(ctx);
		rv = aml_allocvalue(AML_OBJTYPE_EVENT, 0, (char *)name, "event");
		if (rv != NULL) {
			aml_addvname(ctx, name, opc->opcode, rv);
		}
		break;
	case AMLOP_SIGNAL:
		lhs = aml_eparseref(ctx);
		dnprintf(10,"signal: %s\n", lhs->v_string);
		break;
	case AMLOP_WAIT:
		lhs = aml_eparseref(ctx);
		i1  = aml_eparseint(ctx);
		dnprintf(10, "wait: %s %Lx\n", lhs->v_string, i1);
		break;
	case AMLOP_RESET:
		lhs = aml_eparseref(ctx);
		dnprintf(10, "reset: %s\n", lhs->v_string);
		break;
	case AMLOP_NOTIFY:
		lhs = aml_eparseval(ctx);
		i1  = aml_eparseint(ctx);
		dnprintf(10,"NOTIFY: %Lx %s\n", i1, lhs->name);
		break;
	case AMLOP_LOAD:
	case AMLOP_STORE:
		rhs = aml_eparseval(ctx);
		lhs = aml_eparseval(ctx);
		rv  = aml_esetnodevalue(ctx, lhs, rhs, 0);
		break;
	case AMLOP_COPYOBJECT:
		rhs = aml_eparseval(ctx);
		lhs = aml_eparseval(ctx);
		rv = aml_copyvalue(rhs);
		aml_esetnodevalue(ctx, lhs, rv, 0);
		break;
	case AMLOP_OPREGION:
		name = aml_parse_name(ctx);
		rv = aml_allocvalue(AML_OBJTYPE_OPREGION, 0, (char *)name, "opregion");
		if (rv != NULL) {
			rv->v_opregion.iospace = aml_parse_int(ctx, 1);
			rv->v_opregion.iobase  = aml_eparseint(ctx);
			rv->v_opregion.iolen   = aml_eparseint(ctx);
#if 0
			rv->v_opregion.buf = acpi_os_allocmem(rv->v_opregion.iolen);
			memset(rv->v_opregion.buf, 0xFF, rv->v_opregion.iolen);
			acpi_gasio(ctx->sc, ACPI_IOREAD,
				   rv->v_opregion.iospace,
				   rv->v_opregion.iobase,
				   1,
				   rv->v_opregion.iolen,
				   rv->v_opregion.buf);
#endif
			aml_addvname(ctx, name, opc->opcode, rv);
		}
		break;
	case AMLOP_ALIAS:
		name = aml_parse_name(ctx);
		rv = aml_allocvalue(AML_OBJTYPE_NAMEREF, 0, (void *)name, "alias");
		if (rv != NULL) {
			name = aml_parse_name(ctx);
			aml_addvname(ctx, name, opc->opcode, rv);
		}
		break;
	case AMLOP_NAME:
		name = aml_parse_name(ctx);
		rv = aml_eparseval(ctx);
		if (rv != NULL) {
			aml_addvname(ctx, name, opc->opcode, rv);
		}
		break;
	case AMLOP_RETURN:
		rv = aml_eparseref(ctx);
		ctx->pos = NULL;
		break;
	case AMLOP_CONCAT:
		rv = aml_econcat(ctx);
		break;

	case AMLOP_SCOPE:
		end = aml_eparselen(ctx);
		name = aml_parse_name(ctx);

		/* Save old scope, create new scope */
		rv = aml_eparsescope(ctx, name, end, opc);
		break;
	case AMLOP_DEVICE:
		end  = aml_eparselen(ctx);
		name = aml_parse_name(ctx);

		rv   = aml_allocvalue(AML_OBJTYPE_DEVICE, 0, (char *)name, "device");

		/* Save old scope, create new scope */
		lhs  = aml_eparsescope(ctx, name, end, opc);
		break;
	case AMLOP_POWERRSRC:
		end  = aml_eparselen(ctx);
		name = aml_parse_name(ctx);

		rv = aml_allocvalue(AML_OBJTYPE_POWERRSRC, 0, (char *)name, "power");
		if (rv != NULL) {
			rv->v_powerrsrc.pwr_level = aml_parse_int(ctx, 1);
			rv->v_powerrsrc.pwr_order = aml_parse_int(ctx, 2);

			lhs = aml_eparsescope(ctx, name, end, opc);
		}
		break;
	case AMLOP_PROCESSOR:
		end  = aml_eparselen(ctx);
		name = aml_parse_name(ctx);

		rv = aml_allocvalue(AML_OBJTYPE_PROCESSOR, 0, (char *)name, "processor");
		if (rv != NULL) {
			rv->v_processor.proc_id = aml_parse_int(ctx, 1);
			rv->v_processor.proc_addr = aml_parse_int(ctx, 4);
			rv->v_processor.proc_len = aml_parse_int(ctx, 1);

			lhs = aml_eparsescope(ctx, name, end, opc);
		}
		break;
	case AMLOP_THERMALZONE:
		end  = aml_eparselen(ctx);
		name = aml_parse_name(ctx);

		rv = aml_allocvalue(AML_OBJTYPE_THERMZONE, 0, (char *)name, "tzone");
		if (rv != NULL) {
			lhs = aml_eparsescope(ctx, name, end, opc);
		}
		break;
	case AMLOP_OBJECTTYPE:
		lhs = aml_eparseval(ctx);
		rv  = aml_allocint(lhs->type,1);
		break;
	case AMLOP_SIZEOF:
		lhs = aml_eparseval(ctx);
		if (lhs != NULL) {
			rv = aml_allocint(lhs->length, 1);
		}
		break;
	case AMLOP_METHOD:
		end  = aml_eparselen(ctx);
		name = aml_parse_name(ctx);
		i1   = aml_parse_int(ctx, 1);
		rv = aml_allocvalue(AML_OBJTYPE_METHOD, i1, (char *)name, "method");
		if (rv != NULL) {
			/* Allocate method block */
			rv->length = end - ctx->pos;
			rv->v_method.start = acpi_os_allocmem(rv->length);
			rv->v_method.end   = rv->v_method.start + rv->length;

			memcpy(rv->v_method.start, ctx->pos, rv->length);
			aml_addvname(ctx, name, opc->opcode, rv);
		}
		break;
	case AMLOP_CONDREFOF:
		rhs = aml_eparseval(ctx);
		lhs = aml_eparseval(ctx);
		rv  = aml_allocint(0, 1);
		if (aml_ederef(ctx, rhs) != NULL) {
			tmp = aml_allocvalue(AML_OBJTYPE_OBJREF, -1, rhs, "condref");
			aml_esetnodevalue(ctx, lhs, tmp, 0);
			rv->v_integer = 1;
		}
		dnprintf(10,"condrefof: %lld\n", rv->v_integer);
		break;
	case AMLOP_REFOF:
		rhs = aml_eparseval(ctx);
		lhs = aml_eparseval(ctx);
		rv  = aml_allocvalue(AML_OBJTYPE_OBJREF, -1, rhs, "refof");
		aml_esetnodevalue(ctx, lhs, rv, 0);
		break;
	case AMLOP_INDEX:
		rhs = aml_eparseval(ctx);
		i1  = aml_eparseint(ctx);
		lhs = aml_eparseval(ctx);
		rv  = aml_allocvalue(AML_OBJTYPE_OBJREF, i1, rhs, "index");
		aml_esetnodevalue(ctx, lhs, rv, 0);
		break;
	case AMLOP_DEREFOF:
		rv = aml_eparseref(ctx);
		break;
	case AMLOP_LOADTABLE:
		rv = aml_doloadtable(ctx);
		break;
	case AMLOP_MATCH:
		rv = aml_domatch(ctx);
		break;
	case AMLOP_WHILE:
		rv  = aml_dowhile(ctx);
		break;
	case AMLOP_IF:
		rv  = aml_doif(ctx);
		break;

	default:
		dnprintf(10,"Unknown opcode: %.4x %s\n", opc->opcode, opc->mnem);
		break;
	}
	if (end > ctx->pos) {
		ctx->pos = end;
	}

	--ctx->depth;
	return rv;
}

/* Remove all children nodes */
void aml_delchildren(struct acpi_context *ctx, struct aml_node *node)
{
	struct aml_node *pn;

	if (node == NULL) {
		return;
	}
	while ((pn = node->child) != NULL) {
		node->child = node->child->sibling;
		aml_delchildren(ctx, pn);
		acpi_os_freemem(pn);
	}
}

/* Ok.. we have a node and hopefully its value.. call value */
struct aml_value *aml_eparsenode(struct acpi_context *ctx, struct aml_node *node)
{
	struct aml_node *oldscope;
	struct aml_value *rv;

	if (node->value == NULL) {
		return NULL;
	}
	switch (node->value->type) {
	case AML_OBJTYPE_NAMEREF:
	case AML_OBJTYPE_INTEGER:
	case AML_OBJTYPE_STRING:
	case AML_OBJTYPE_BUFFER:
	case AML_OBJTYPE_PACKAGE:
		return node->value;
	case AML_OBJTYPE_BUFFERFIELD:
	case AML_OBJTYPE_FIELDUNIT:
		return aml_efield(ctx, node->value, NULL);
	case AML_OBJTYPE_METHOD:
		/* Setup method call */
		oldscope = ctx->scope;
		ctx->scope = node->parent;

		dnprintf(10, "Call function: %s\n", node->name);

		ctx->pos = node->value->v_method.start;
		rv = aml_eparselist(ctx, node->value->v_method.end);

		/* Delete dynamic names */
		aml_delchildren(ctx, node);

		ctx->scope = oldscope;
		return rv;
	default:
		dnprintf(10, "Unknown node value: %d\n", node->value->type);
	}
	return NULL;
}

int aml_eval_object(struct acpi_softc *sc, struct aml_node *node,
		    struct aml_value *ret, int argc, struct aml_value *argv)
{
	struct acpi_context *ctx;
	struct aml_value *rv;

	ctx = acpi_alloccontext(sc, node, argc, argv);
	rv = aml_eparsenode(ctx, node);
	aml_showvalue(rv);
	*ret = *rv;

	/* XXX: must free rv */
	acpi_freecontext(ctx);

	return -1;
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
		dnprintf(50, "%s", node->value->name);
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

struct acpi_context *acpi_alloccontext(struct acpi_softc *sc, 
				       struct aml_node *node,
				       int argc, 
				       struct aml_value *argv)
{
	struct acpi_context *ctx;
	int idx;

	if ((ctx = acpi_os_allocmem(sizeof(struct acpi_context))) != NULL) {
		ctx->sc = sc;
		ctx->depth = 0;
		ctx->start = node->start;
		ctx->pos   = node->start;
		ctx->scope = node;
		ctx->locals = (struct aml_value **)acpi_os_allocmem(sizeof(struct aml_value *) * 8);
		ctx->args   = (struct aml_value **)acpi_os_allocmem(sizeof(struct aml_value *) * 8);
		for (idx=0; idx<argc; idx++) {
			ctx->args[idx] = aml_copyvalue(&argv[idx]);
		}
	}
	return ctx;
}

void acpi_freecontext(struct acpi_context *ctx)
{
	int idx;

	if (ctx) {
		for (idx=0; idx<8; idx++) {
			aml_freevalue(&ctx->args[idx]);
			aml_freevalue(&ctx->locals[idx]);
		}
		acpi_os_freemem(ctx->args);
		acpi_os_freemem(ctx->locals);
		acpi_os_freemem(ctx);
	}
}

int
acpi_parse_aml(struct acpi_softc *sc, u_int8_t *start, u_int32_t length)
{
	struct acpi_context *ctx;
	struct aml_value *rv;

	stacktop = &sc;
	
	aml_root.depth = -1;
	aml_root.mnem  = "ROOT";
	aml_root.start = start;
	aml_root.end   = start + length;

	ctx = acpi_alloccontext(sc, &aml_root, 0, NULL);
	rv  = aml_eparselist(ctx, aml_root.end);
	aml_freevalue(&rv);
	acpi_freecontext(ctx);

	dnprintf(50, " : parsed %d AML bytes\n", length);

	//aml_walktree(&aml_root);

	return (0);
}
