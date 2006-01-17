/* $OpenBSD: dsdt.c,v 1.16 2006/01/17 23:42:14 jordan Exp $ */
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

struct aml_stream
{
  u_int8_t *start;
  u_int8_t *end;
  u_int8_t *pos;
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

  u_int8_t   *start;
  u_int8_t   *end;
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

int aml_isnamedop(u_int16_t);
void aml_parsefieldlist(struct acpi_context *, struct aml_node *parent);
int aml_parselength(struct acpi_context *);
const char *aml_parsename(struct acpi_context *, const char *);
u_int16_t aml_getopcode(struct acpi_context *);
int aml_parseargs(struct acpi_context *, struct aml_node *, const char *);

int aml_parse_objlist(struct acpi_context *, struct aml_node *);
struct aml_node *aml_parse_object(struct acpi_context *, struct aml_node *);
void aml_shownode(struct aml_node *);

u_int64_t  aml_bcd2dec(u_int64_t);
u_int64_t  aml_dec2bcd(u_int64_t);

int aml_lsb(u_int32_t val);
int aml_msb(u_int32_t val);

void aml_walkroot(void);
void aml_addchildnode(struct aml_node *, struct aml_node *);
struct aml_node *aml_create_node(struct acpi_context *, struct aml_node *, int opcode);
struct aml_node *aml_find_name(struct acpi_softc *, struct aml_node *, const char *);

int64_t aml_evalmath(u_int16_t, int64_t, int64_t);
int  aml_testlogical(u_int16_t, long, long);
int  aml_strcmp(u_int16_t opcode, const char *lhs, const char *rhs);

int  aml_evalint(struct acpi_softc *, struct aml_node *, struct aml_value *);
int  aml_eval_object(struct acpi_softc *, struct aml_node *, struct aml_value *, struct aml_value *);
#if 0
void aml_copyvalue(struct aml_value *, const struct aml_value *);
#endif
void aml_setinteger(struct aml_value *, int64_t);
void aml_setstring(struct aml_value *, const char *);
void aml_setbuffer(struct aml_value *, int, u_int8_t *);
void aml_setfield(struct aml_value *, int, int, struct aml_node *, struct aml_node *);
void aml_setopregion(struct aml_value *, int, int, u_int64_t);
void aml_setpackage(struct aml_value *, struct aml_node *);
void aml_setprocessor(struct aml_value *, u_int8_t, u_int32_t, u_int8_t);

struct aml_value *aml_getnodevalue(struct acpi_softc *, struct aml_node *, struct aml_value *);
void aml_setnodevalue(struct acpi_softc *, struct aml_node *, const struct aml_value *, struct aml_value *);
void aml_setnodeinteger(struct acpi_softc *, struct aml_node *, int64_t, struct aml_value *);

int aml_match(struct acpi_softc *, int, const struct aml_value *, const struct aml_value *);
int aml_cmpobj(struct acpi_softc *, const struct aml_value *, const struct aml_value *);

const char *aml_parsestr(struct acpi_context *);
u_int64_t   aml_parseint(struct acpi_context *, int);

struct aml_value *aml_allocvalue(int, int64_t, const void *, const char *);
struct aml_value *aml_copyvalue(const struct aml_value *);
void aml_freevalue(struct aml_value **);

struct aml_value *aml_allocint(uint64_t);
struct aml_value *aml_allocstr(const char *);

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

void *acpi_os_allocmem(size_t);
void  acpi_os_freemem(void *);

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

  //printf("alloc value: %.2x : %s (%llx)\n", type, lbl, ival);

  rv = (struct aml_value *)acpi_os_allocmem(sizeof(struct aml_value));
  memset(rv, 0, sizeof(struct aml_value));
  rv->type = type;
  rv->dynamic = 1;

  switch (type) {
#if 0 
    xxx
      case AML_OBJTYPE_REFOF:
      rv->v_index.index  = ival;
    rv->v_index.refobj = bval;
    break;
  case AML_OBJTYPE_ERROR:
    rv->v_error.error = ival;
    rv->v_error.errobj = bval;
    break;
#endif
  case AML_OBJTYPE_INTEGER:
    rv->v_integer = ival;
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
#if 0 
    xxx
      case AML_OBJTYPE_MUTEX:
  case AML_OBJTYPE_DEVICE:
  case AML_OBJTYPE_EVENT:
      rv->v_device = bval;
    break;
#endif
  }
  return rv;
}

void aml_freevalue(struct aml_value **pv)
{
  int idx;
  struct aml_value *v = *pv;

  /* Don't free static values */
  if (v == NULL || !v->dynamic)
    return;

  printf("freeing value : %x\n", v->type);
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
    break;
  case AML_OBJTYPE_METHOD:
    for (idx=0; idx<8; idx++) {
#if 0 
      xxx
	aml_freevalue(v->v_method.locals[idx]);
      aml_freevalue(v->v_method.args[idx]);
#endif
    }
    acpi_os_freemem(v->v_method.locals);
    acpi_os_freemem(v->v_method.args);
  }
  v->type = 0;
  acpi_os_freemem(v);
  *pv = 0;
}

#if 0 
xxx
void
aml_showvalue(struct aml_value *val)
{
  int idx;

  if (val == NULL)
    return;
  switch(val->type) {
  case AML_OBJTYPE_INTEGER:
    dnprintf("integer: 0x%x", val->v_integer);
    break;
  case AML_OBJTYPE_STRING:
    dnprintf("string:  %s", val->v_string);
    break;
  case AML_OBJTYPE_PACKAGE:
    printf("package: len = %ld {\n", (unsigned long)val->length);
    for(idx=0; idx<val->length; idx++) {
      aml_showvalue(val->v_package[idx]);
    }
    printf("}\n");
    break;
  case AML_OBJTYPE_BUFFER:
    printf("buffer: len = %ld { ", (unsigned long)val->length);
    for(idx=0; idx<val->length; idx++) {
      printf("%s0x%.2x", (idx ? ", " : ""), val->v_buffer[idx]);
    }
    printf(" }\n");
    break;
  default:
    printf("xxx");
    break;
  }
  printf("\n");
}
#endif

const char *
aml_parsestr(struct acpi_context *ctx)
{
  const char *str = ctx->pos;

  ctx->pos += strlen(str)+1;
  return str;
}

/* Read value from AML bytestream */
uint64_t
aml_parseint(struct acpi_context *ctx, int size)
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

void
aml_setinteger(struct aml_value *val, int64_t value)
{
  val->type = AML_OBJTYPE_INTEGER;
  val->v_integer = value;
  val->length = 0;
}

void
aml_setstring(struct aml_value *val, const char *str)
{
  val->type = AML_OBJTYPE_STRING;
  val->length = strlen(str);
  val->v_string = (char *)str;
}

void
aml_setbuffer(struct aml_value *val, int size, u_int8_t *ptr)
{
  val->type = AML_OBJTYPE_STRING;
  val->length = size;
  val->v_buffer = ptr;
}

void
aml_setfield(struct aml_value *val, int bitpos, int bitlen, struct aml_node *ref, struct aml_node *node)
{
  dnprintf(50, "setfield: pos=%.8x len=%.8x ref=%s name=%s\n", bitpos, bitlen, ref->name, node->name);
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
  dnprintf(50, "setopregion: %.2x %.4x %.8x\n", addrtype, size, addr);
  val->type = AML_OBJTYPE_OPREGION;
  val->v_opregion.iospace = addrtype;
  val->v_opregion.iobase  = addr;
  val->v_opregion.iolen   = size;
}

/* Decode AML Package length
 * Upper two bits of first byte denote length
 *   0x00 = length is in lower 6 bits
 *   0x40 = length is lower 4 bits + 1 byte
 *   0x80 = length is lower 4 bits + 2 bytes
 *   0xC0 = length is lower 4 bits + 3 bytes
 */
int
aml_parselength(struct acpi_context *ctx)
{
  u_int8_t lcode;
  int ival;

  lcode = aml_parseint(ctx, 1);
  if (lcode <= 0x3F) {
    return lcode;
  }

  ival = lcode & 0xF;
  if (lcode >= 0x40)  ival |= aml_parseint(ctx, 1) << 4;
  if (lcode >= 0x80)  ival |= aml_parseint(ctx, 1) << 12;
  if (lcode >= 0xC0)  ival |= aml_parseint(ctx, 1) << 20;

  return ival;
}

void
aml_parsefieldlist(struct acpi_context *ctx, struct aml_node *node)
{
  u_int8_t type, attr;
  int len, start;
  struct aml_node *pf;

  start = 0;
  dnprintf(50, "-- parsefield\n");
  while (ctx->pos < node->end) {
    switch (aml_parseint(ctx, 1)) {
    case 0x00: /* reserved */
      len = aml_parselength(ctx);
      start += len;
      break;
    case 0x01: /* access field */
      type = aml_parseint(ctx, 1);
      attr = aml_parseint(ctx, 1);
      dnprintf(50, "  type=%.2x  attr=%.2x\n", type, attr);
      break;
    default: /* named field */
      --ctx->pos;
      pf = aml_create_node(ctx, node, AMLOP_CREATEFIELD);
      pf->name = aml_parsename(ctx, "field");
      len = aml_parselength(ctx);

      //aml_setfield(pf->value, start, len, node, pf);
      start += len;
    }
  }
}

/* Decode AML Namestring from stream */
const char *
aml_parsename(struct acpi_context *ctx, const char *lbl)
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
u_int16_t
aml_getopcode(struct acpi_context *ctx)
{
  u_int16_t twocode;
  u_int16_t opcode;

  /* Check for encoded name */
  if (aml_isnamedop(*ctx->pos)) {
    return AMLOP_NAMECHAR;
  }

  opcode  = aml_parseint(ctx, 1);
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

#if 0 
xxx
/* Copy an AML value object */
void aml_copyvalue(struct aml_value *dst, const struct aml_value *src)
{
  dst->type   = src->type;
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
  case AML_OBJTYPE_PACKAGE:
    dst->v_package = src->v_package;
    break;
  }
}
#endif

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

struct aml_value aml_debugobj;

struct aml_value *
aml_getnodevalue(struct acpi_softc *sc, struct aml_node *node,
		 struct aml_value *env)
{
  int id;

  if (node == NULL) {
    printf("aml_getnodevalue: null\n");
    return NULL;
  }
  switch (node->opcode) {
  case AMLOP_DEBUG:
    return &aml_debugobj;

  case AMLOP_LOCAL0:
  case AMLOP_LOCAL1:
  case AMLOP_LOCAL2:
  case AMLOP_LOCAL3:
  case AMLOP_LOCAL4:
  case AMLOP_LOCAL5:
  case AMLOP_LOCAL6:
  case AMLOP_LOCAL7:
    id = node->opcode - AMLOP_LOCAL0;
    return &env->v_method.locals[id];

  case AMLOP_ARG0:
  case AMLOP_ARG1:
  case AMLOP_ARG2:
  case AMLOP_ARG3:
  case AMLOP_ARG4:
  case AMLOP_ARG5:
  case AMLOP_ARG6:
    id = node->opcode - AMLOP_ARG0;
    return &env->v_method.args[id];

  case AMLOP_ZERO:
  case AMLOP_ONE:
  case AMLOP_ONES:
  case AMLOP_BYTEPREFIX:
  case AMLOP_WORDPREFIX:
  case AMLOP_DWORDPREFIX:
  case AMLOP_QWORDPREFIX:
  case AMLOP_STRINGPREFIX:
    return node->value;

  default:
    printf("aml_getnodevalue: no type: %.4x\n", node->opcode);
    break;
  }
  return NULL;
}

void
aml_setnodevalue(struct acpi_softc *sc, struct aml_node *node, const struct aml_value *val,
		 struct aml_value *env)
{
  struct aml_value *dest = NULL;
  struct aml_value  lhs;
  int id;

  if (node == NULL) {
    printf("aml_setnodevalue: null\n");
    return;
  }
  dnprintf(50, "--- setnodevalue:\n");
  aml_shownode(node);
  aml_showvalue((struct aml_value *)val);
  switch (node->opcode) {
  case AMLOP_DEBUG:
    dest = &aml_debugobj;
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
    dest = &env->v_method.locals[id];
    break;

  case AMLOP_ARG0:
  case AMLOP_ARG1:
  case AMLOP_ARG2:
  case AMLOP_ARG3:
  case AMLOP_ARG4:
  case AMLOP_ARG5:
  case AMLOP_ARG6:
    id = node->opcode - AMLOP_ARG0;
    dest = &env->v_method.args[id];
    break;

  case AMLOP_NAMECHAR:
    return aml_setnodevalue(sc, aml_find_name(sc, NULL, node->name), val, env);

  case AMLOP_CREATEFIELD:
  case AMLOP_CREATEBITFIELD:
  case AMLOP_CREATEBYTEFIELD:
  case AMLOP_CREATEWORDFIELD:
  case AMLOP_CREATEDWORDFIELD:
  case AMLOP_CREATEQWORDFIELD:
    aml_eval_object(sc, node, &lhs, env);
    aml_showvalue(&lhs);
    for(;;);
    break;

  case AMLOP_ZERO:
  case AMLOP_ONE:
  case AMLOP_ONES:
  case AMLOP_REVISION:
  case AMLOP_BYTEPREFIX:
  case AMLOP_WORDPREFIX:
  case AMLOP_DWORDPREFIX:
  case AMLOP_QWORDPREFIX:
  default:
    printf("aml_setnodeval: read-only %.4x\n", node->opcode);
    break;
  }
  if (dest) {
    dnprintf(50, "aml_setnodeval: %.4x\n", node->opcode);
#if 0 
    xxx
      aml_copyvalue(dest, val);
#endif
  }
}

void
aml_setnodeinteger(struct acpi_softc *sc, struct aml_node *node, int64_t value,
		   struct aml_value *env)
{
  struct aml_value ival;

  aml_setinteger(&ival, value);
  aml_setnodevalue(sc, node, &ival, env);
}

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

struct aml_node *
aml_create_node(struct acpi_context *ctx, struct aml_node *parent, int opcode)
{
  struct aml_node *node;

  node = acpi_os_allocmem(sizeof(struct aml_node));
  memset(node, 0, sizeof(struct aml_node));

  node->start  = ctx->pos;
  node->opcode = (opcode == -1) ? aml_getopcode(ctx) : opcode;
  aml_addchildnode(parent, node);

  return node;
}

int
aml_evalint(struct acpi_softc *sc, struct aml_node *node, struct aml_value *env)
{
  struct  aml_value ival;

  aml_eval_object(sc, node, &ival, env);
  if (ival.type == AML_OBJTYPE_INTEGER)
    return ival.v_integer;

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
    return aml_eval_object(sc, root, result, env);
  }
  return (1);
}

int
aml_eval_object(struct acpi_softc *sc, struct aml_node *node, struct aml_value *result, 
		struct aml_value *env)
{
  memset(result, 0, sizeof(struct aml_value));
#if 0 
  xxx
    struct  aml_value lhs, rhs, tmp, pkg;
  struct aml_value *px;
  int64_t iresult, id, idx;
  struct  aml_node *cflow = NULL;
  int     i1, i2, i3;
  char   *tmpstr;

  if (node == NULL) 
    return (-1);

  dnprintf(50, "--- Evaluating object:\n"); 
  aml_shownode(node);

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
#if 0
    aml_copyvalue(result, &node->value);
#endif
    break;

  case AMLOP_BUFFER:
    i1 = aml_evalint(sc, childOf(node, 0), env);
    dnprintf(50, "@@@@@@@@@@@@@@ buffer: %.4x %.4x\n", i1, node->value->length);
    break;

  case AMLOP_STORE:
    aml_eval_object(sc, childOf(node, 0), &lhs, env);
    aml_setnodevalue(sc, childOf(node, 1), &lhs, env);
    break;

  case AMLOP_DEBUG:
#if 0
    aml_copyvalue(result, &aml_debugobj);
#endif
    break;

  case AMLOP_NAME:
  case AMLOP_ALIAS:
    return aml_eval_object(sc, childOf(node, 0), result, env);

  case AMLOP_PROCESSOR:
    i1 = aml_evalint(sc, childOf(node, 0), env);
    i2 = aml_evalint(sc, childOf(node, 1), env);
    i3 = aml_evalint(sc, childOf(node, 2), env);
    aml_setprocessor(result, i1, i2, i3);
    break;

  case AMLOP_OPREGION:
    i1 = aml_evalint(sc, childOf(node, 0), env);
    i2 = aml_evalint(sc, childOf(node, 1), env);
    aml_setopregion(result, node->flag, i1, i2);
    break;

  case AMLOP_IF:
    i1 = aml_evalint(sc, childOf(node,0), env);
    if (i1 != 0) {
      /* Test true, select 'If' block */
      cflow = childOf(node, 1);
    }
    else if (node->sibling->opcode == AMLOP_ELSE) {
      /* Test false, select 'Else' block */
      cflow = node->sibling->child;
    }
    while (cflow) {
      /* Execute all instructions in scope block */
      aml_eval_object(sc, cflow, result, env);
      cflow = cflow->sibling;
    }
    break;

  case AMLOP_WHILE:
    for (;;) {
      if (cflow == NULL) {
	/* Perform While test */
	cflow = childOf(node, 1);
	i1 = aml_evalint(sc, childOf(node, 0), env);
	if (i1 == 0) 
	  break;
      }
      else if (cflow->opcode == AMLOP_BREAK) 
	break;
      else if (cflow->opcode == AMLOP_CONTINUE)
	/* Reset cflow to NULL; restart block */
	cflow = NULL;
      else {
	/* Execute all instructions in scope block */
	aml_eval_object(sc, cflow, result, env);
	cflow = cflow->sibling;
      }
    }
    break;
		
  case AMLOP_RETURN:
    aml_eval_object(sc, childOf(node, 0), result, env);
    break;

  case AMLOP_ARG0:
  case AMLOP_ARG1:
  case AMLOP_ARG2:
  case AMLOP_ARG3:
  case AMLOP_ARG4:
  case AMLOP_ARG5:
  case AMLOP_ARG6:
    id = node->opcode - AMLOP_ARG0;
#if 0
    if (id < env->length)
      aml_copyvalue(result, &node->value->v_method.locals[id]);
#endif
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
#if 0
    aml_copyvalue(result, &env->v_method.locals[id]);
#endif
    break;

  case AMLOP_PACKAGE:
  case AMLOP_VARPACKAGE:
    i1 = aml_evalint(sc, childOf(node, 0), env);
    dnprintf(50, "package = %d\n", i1);
    result->type = AML_OBJTYPE_PACKAGE;
    result->length = i1;

    result->v_package = acpi_os_allocmem(i1 * sizeof(struct aml_value));
    for (i2=0; i2<i1; i2++) {
      aml_eval_object(sc, childOf(node, i2+1), result->v_package[i2], env);
    }
    break;
	
  case AMLOP_INCREMENT:
  case AMLOP_DECREMENT:
    i1 = aml_evalint(sc, childOf(node, 0), env);
    iresult = aml_evalmath(node->opcode, i1, 0);
    aml_setnodeinteger(sc, childOf(node, 0), iresult, env);
    break;

  case AMLOP_NOT:
  case AMLOP_FINDSETLEFTBIT:
  case AMLOP_FINDSETRIGHTBIT:
    i1 = aml_evalint(sc, childOf(node, 0), env);
    iresult = aml_evalmath(node->opcode, i1, 0);
    aml_setnodeinteger(sc, childOf(node, 1), iresult, env);
    break;

  case AMLOP_DIVIDE:
    i1 = aml_evalint(sc, childOf(node, 0), env);
    i2 = aml_evalint(sc, childOf(node, 1), env);

    /* Set remainder */
    iresult = aml_evalmath(AMLOP_MOD,    i1, i2);
    aml_setnodeinteger(sc, childOf(node, 2), iresult, env);

    /* Set quotient */
    iresult = aml_evalmath(node->opcode, i1, i2);
    aml_setnodeinteger(sc, childOf(node, 3), iresult, env);
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
    i1 = aml_evalint(sc, childOf(node, 0), env);
    i2 = aml_evalint(sc, childOf(node, 1), env);

    iresult = aml_evalmath(node->opcode, i1, i2);
    aml_setnodeinteger(sc, childOf(node, 2), iresult, env);
    break;

  case AMLOP_LNOT:
    i1 = aml_evalint(sc, childOf(node, 0), env);
    iresult = aml_testlogical(node->opcode, i1, 0);
    aml_setinteger(result, iresult);
    break;

  case AMLOP_LAND:
  case AMLOP_LOR:
    i1 = aml_evalint(sc, childOf(node, 0), env);
    i2 = aml_evalint(sc, childOf(node, 1), env);
    iresult = aml_testlogical(node->opcode, i1, i2);
    aml_setinteger(result, iresult);
    break;

  case AMLOP_LEQUAL:
  case AMLOP_LNOTEQUAL:
  case AMLOP_LLESSEQUAL:
  case AMLOP_LGREATEREQUAL:
  case AMLOP_LGREATER:
  case AMLOP_LLESS:
    aml_eval_object(sc, childOf(node, 0), &lhs, env);
    aml_eval_object(sc, childOf(node, 1), &rhs, env);
    if (lhs.type == AML_OBJTYPE_INTEGER && rhs.type == AML_OBJTYPE_INTEGER) {
      iresult = aml_testlogical(node->opcode, lhs.v_integer, rhs.v_integer);
    }
    else if (lhs.type == AML_OBJTYPE_STRING && rhs.type == AML_OBJTYPE_STRING) {
      iresult = aml_strcmp(node->opcode, lhs.v_string, rhs.v_string);
    }
    aml_setinteger(result, iresult);
    break;

  case AMLOP_CREATEFIELD:
    i1 = aml_evalint(sc, childOf(node, 1), env);
    i2 = aml_evalint(sc, childOf(node, 2), env);
    aml_setfield(&lhs, i1, i2, childOf(node, 0), node);
    aml_setnodevalue(sc, childOf(node, 3), &lhs, env);
    break;
  case AMLOP_CREATEBITFIELD:
    i1 = aml_evalint(sc, childOf(node, 1), env);
    aml_setfield(&lhs, i1, 1, childOf(node, 0), node);
    aml_setnodevalue(sc, childOf(node, 2), &lhs, env);
    break;
  case AMLOP_CREATEBYTEFIELD:
    i1 = aml_evalint(sc, childOf(node, 1), env);
    aml_setfield(&lhs, i1 * 8, 8, childOf(node, 0), node);
    aml_setnodevalue(sc, childOf(node, 2), &lhs, env);
    break;
  case AMLOP_CREATEWORDFIELD:
    i1 = aml_evalint(sc, childOf(node, 1), env);
    aml_setfield(&lhs, i1 * 8, 16, childOf(node, 0), node);
    aml_setnodevalue(sc, childOf(node, 2), &lhs, env);
    break;
  case AMLOP_CREATEDWORDFIELD:
    i1 = aml_evalint(sc, childOf(node, 1), env);
    aml_setfield(&lhs, i1 * 8, 32, childOf(node, 0), node);
    aml_setnodevalue(sc, childOf(node, 2), &lhs, env);
    break;
  case AMLOP_CREATEQWORDFIELD:
    i1 = aml_evalint(sc, childOf(node, 1), env);
    aml_setfield(&lhs, i1 * 8, 64, childOf(node, 0), node);
    aml_setnodevalue(sc, childOf(node, 2), &lhs, env);
    break;
		
  case AMLOP_TOBCD:
    i1 = aml_evalint(sc, childOf(node, 0), env);
    iresult = aml_dec2bcd(i1);
    aml_setnodeinteger(sc, childOf(node, 1), iresult, env);
    break;
  case AMLOP_FROMBCD:
    i1 = aml_evalint(sc, childOf(node, 0), env);
    iresult = aml_bcd2dec(i1);
    aml_setnodeinteger(sc, childOf(node, 1), iresult, env);
    break;
  case AMLOP_TODECSTRING:
    tmpstr = acpi_os_allocmem(AML_INTSTRLEN+1);
    if (tmpstr != NULL) {
      aml_eval_object(sc, childOf(node, 0), &lhs, env);
      if (lhs.type == AML_OBJTYPE_INTEGER) 
	snprintf(tmpstr, AML_INTSTRLEN, "%d", lhs.v_integer);
    }
    break;
  case AMLOP_TOHEXSTRING:
    tmpstr = acpi_os_allocmem(AML_INTSTRLEN+1);
    if (tmpstr != NULL) {
      aml_eval_object(sc, childOf(node, 0), &lhs, env);
      if (lhs.type == AML_OBJTYPE_INTEGER) 
	snprintf(tmpstr, AML_INTSTRLEN, "%x", lhs.v_integer);
    }
    break;

  case AMLOP_MID:
    aml_eval_object(sc, childOf(node, 0), &tmp, env);
    aml_eval_object(sc, childOf(node, 1), &lhs, env);
    aml_eval_object(sc, childOf(node, 2), &rhs, env);
    if (tmp.type != AML_OBJTYPE_STRING) 
      return (-1);

    tmpstr = acpi_os_allocmem(rhs.v_integer+1);
    if (tmpstr != NULL) {
      strncpy(tmpstr, tmp.v_string + lhs.v_integer, rhs.v_integer);
    }
    break;

  case AMLOP_STALL:
    i1 = aml_evalint(sc, childOf(node, 0), env);
    dnprintf(50, "aml_stall: %d\n", i1);
    break;
  case AMLOP_SLEEP:
    i1 = aml_evalint(sc, childOf(node, 0), env);
    dnprintf(50, "aml_sleep: %d\n", i1);
    break;
  case AMLOP_OBJECTTYPE:
    aml_eval_object(sc, childOf(node, 0), &lhs, env);
    aml_setinteger(result, lhs.type);
    break;

  case AMLOP_NAMECHAR: /* Inline method call */
    aml_eval_name(sc, NULL, node->name, result, env);
    break;

  case AMLOP_METHOD:
    dnprintf(50, "eval-method : %s  argcount:%d\n", 
	     node->name, AML_METHOD_ARGCOUNT(node->flag));

    lhs.type = AML_OBJTYPE_METHOD;
    lhs.length = AML_METHOD_ARGCOUNT(node->flag);
    if (lhs.length > 0) {
      lhs.v_method.args = acpi_os_allocmem(lhs.length * sizeof(struct aml_value));
      memset(lhs.v_method.args, 0, lhs.length * sizeof(struct aml_value));
    }
    lhs.v_method.locals = acpi_os_allocmem(8 * sizeof(struct aml_value));

    for (i1=0; i1<lhs.length; i1++) {
      dnprintf(50, " evalmeth: %s:%d\n", node->name, i1);
      aml_eval_object(sc, childOf(node, i1), &lhs.v_method.args[i1], env);
      aml_showvalue(&lhs.v_method.args[i1]);
    }
    while (childOf(node, i1)) {
      aml_eval_object(sc, childOf(node, i1++), result, &lhs);
    }
    break;

  case AMLOP_CONCAT:
    aml_eval_object(sc, childOf(node, 0), &lhs, env);
    aml_eval_object(sc, childOf(node, 1), &rhs, env);
    break;

  case AMLOP_NOP:
    break;

  case AMLOP_SIZEOF:
    aml_eval_object(sc, childOf(node, 0), &lhs, env);
    px = aml_getnodevalue(sc, childOf(node, 0), env);
    aml_showvalue(px);
    for(;;);
    break;

  case AMLOP_MATCH:
    aml_eval_object(sc, childOf(node, 0), &pkg, env);
    i1 = aml_evalint(sc, childOf(node, 1), env);
    aml_eval_object(sc, childOf(node, 2), &lhs, env);
    i2 = aml_evalint(sc, childOf(node, 3), env);
    aml_eval_object(sc, childOf(node, 4), &lhs, env);
    idx = aml_evalint(sc, childOf(node, 5), env);
    if (pkg.type == AML_OBJTYPE_PACKAGE) {
      iresult = -1;
      while (idx < pkg.length) {
	if (aml_match(sc, i1, &pkg.v_package[idx], &lhs) ||
	    aml_match(sc, i2, &pkg.v_package[idx], &rhs)) {
	  iresult = idx;
	  break;
	}
	idx++;
      }
      aml_setinteger(result, iresult);
    }
    break;

  default:
    printf("Unknown eval: %.4x %s\n", node->opcode, node->mnem);
    break;
  }
#endif
  return (0);
}

int
aml_parseargs(struct acpi_context *ctx, struct aml_node *node, const char *arg)
{
  struct aml_node *pnode;

  while (*arg) {
    pnode = node;
    switch (*arg) {
    case AML_ARG_FLAG:
      node->flag = aml_parseint(ctx, 1);
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
      break;
    case AML_ARG_IMPBYTE:
      /* Implied byte: same as opcode */
      node->value = aml_allocint((char)node->opcode);
      dnprintf(50, " ibyte: %x\n", (int8_t)node->opcode);
      break;
    case AML_ARG_BYTE:
      if (node->opcode != AMLOP_BYTEPREFIX) {
	pnode = aml_create_node(ctx, node, AMLOP_BYTEPREFIX);
      }
      pnode->value = aml_allocint(aml_parseint(ctx, 1));
      dnprintf(50, " byte: %x\n", pnode->value->v_integer);
      break;
    case AML_ARG_WORD:
      if (node->opcode != AMLOP_WORDPREFIX) {
	pnode = aml_create_node(ctx, node, AMLOP_WORDPREFIX);
      }
      pnode->value = aml_allocint(aml_parseint(ctx, 2));
      dnprintf(50, " word: %x\n", pnode->value->v_integer);
      break;
    case AML_ARG_DWORD:
      if (node->opcode != AMLOP_DWORDPREFIX) {
	pnode = aml_create_node(ctx, node, AMLOP_DWORDPREFIX);
      }
      pnode->value = aml_allocint(aml_parseint(ctx, 4));
      dnprintf(50, " dword: %x\n", pnode->value->v_integer);
      break;
    case AML_ARG_QWORD:
      if (node->opcode == AMLOP_QWORDPREFIX) {
	pnode = aml_create_node(ctx, node, AMLOP_QWORDPREFIX);
      }
      pnode->value = aml_allocint(aml_parseint(ctx, 8));
      dnprintf(50, " qword: %x\n", pnode->value->v_integer);	
      break;
    case AML_ARG_FIELDLIST:
      dnprintf(50, " fieldlist\n");
      aml_parsefieldlist(ctx, node);
      break;
    case AML_ARG_BYTELIST:
      dnprintf(50, " bytelist\n");
      node->start = ctx->pos;
      ctx->pos    = node->end;
      break;
    case AML_ARG_STRING:
      node->value = aml_allocstr(aml_parsestr(ctx));
      dnprintf(50, " string: %s\n", node->value->v_string);
      break;
    case AML_ARG_NAMESTRING:
      node->name = aml_parsename(ctx, "name");
      break;
    case AML_ARG_NAMEREF:
      pnode = aml_create_node(ctx, node, AMLOP_NAMECHAR);
      pnode->name = aml_parsename(ctx, "nameref");
      break;
    case AML_ARG_OBJLEN:
      dnprintf(50, " pkglen\n");
      node->end = ctx->pos;
      node->end += aml_parselength(ctx);
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

void
aml_shownode(struct aml_node *node)
{
  dnprintf(50, " opcode:%.4x  mnem:%s %s %.2x ",
	   node->opcode, node->mnem ? node->mnem : "", 
	   node->name ? node->name : "",
	   node->flag);
  switch(node->opcode) {
  case AMLOP_METHOD:
    dnprintf(50, "argcount:%d serialized:%d synclevel:%d",
	     AML_METHOD_ARGCOUNT(node->flag),
	     AML_METHOD_SERIALIZED(node->flag),
	     AML_METHOD_SYNCLEVEL(node->flag));
    break;
  case AMLOP_FIELD:
  case AMLOP_BANKFIELD:
  case AMLOP_INDEXFIELD:
    dnprintf(50, "access:%d lock:%d update:%d\n",
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

struct aml_node *
aml_parse_object(struct acpi_context *ctx, struct aml_node *parent)
{
  struct aml_optable *optab = aml_table;
  struct aml_node *node;

  /* Get AML Opcode; if it is an embedded name, extract name */
  node = aml_create_node(ctx, parent, -1);
  while (optab->opcode != 0xFFFF) {
    if  (optab->opcode == node->opcode) {
      node->mnem = optab->mnem;
      aml_parseargs(ctx, node, optab->args);
      return node;
    }
    optab++;
  }
  printf("Invalid AML Opcode : @ %.4x %.4x\n", ctx->pos - ctx->start, node->opcode);
  acpi_os_freemem(node);

  return NULL;
}

void
aml_walktree(struct aml_node *node, int depth)
{
  int idx;

  while(node) {
    dnprintf(50, " %d ", depth);
    for(idx=0; idx<depth; idx++) {
      dnprintf(50, "..");
    }
    aml_shownode(node);
    aml_walktree(node->child, depth+1);
    node = node->sibling;
  }
}

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

void ex5(struct aml_node *, void *);

void
ex5(struct aml_node *node, void *arg)
{
  struct acpi_softc *sc = arg;
  struct aml_value res, env;

  memset(&res, 0, sizeof(res));
  memset(&env, 0, sizeof(env));
	
  dnprintf(50, "Value is: %s\n", node->name);
  aml_eval_object(sc, node->child, &res, &env);
  aml_showvalue(&res);
}

int
aml_parse_objlist(struct acpi_context *ctx, struct aml_node *parent)
{
  while (ctx->pos < parent->end) {
    aml_parse_object(ctx, parent);
  }
  if (ctx->pos != parent->end) {
    dnprintf(50, "parseobjlist: invalid end!\n");
    ctx->pos = parent->end;
    return (1);
  }
  return (0);
}

int
acpi_parse_aml(struct acpi_softc *sc, u_int8_t *start, u_int32_t length)
{
  struct acpi_context ctx;

  memset(&ctx, 0, sizeof(ctx));
  ctx.pos   = start;
  ctx.start = start;
  ctx.end   = ctx.start + length;

  aml_root.start = start;
  aml_root.end   = start + length;

  aml_parse_objlist(&ctx, &aml_root);
  dnprintf(50, " : parsed %d AML bytes\n", length);

  return (0);
}
