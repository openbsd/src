/* $OpenBSD: acpidebug.c,v 1.3 2006/03/08 21:04:18 marco Exp $ */
/*
 * Copyright (c) 2006 Marco Peereboom <marco@openbsd.org>
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

#include <machine/db_machdep.h>
#include <ddb/db_command.h>
#include <ddb/db_output.h>
#include <ddb/db_extern.h>
#include <ddb/db_lex.h>

#include <machine/bus.h>

#include <dev/acpi/acpireg.h>
#include <dev/acpi/acpivar.h>
#include <dev/acpi/amltypes.h>
#include <dev/acpi/acpidebug.h>
#include <dev/acpi/dsdt.h>

void			db_aml_disasm(struct acpi_context *, int);
void			db_aml_disint(struct acpi_context *, int, int);
void			db_aml_disline(uint8_t *, int, const char *, ...);
void			db_aml_dump(int, u_int8_t *);
void			db_aml_walktree(struct aml_node *);
void			db_aml_shownode(struct aml_node *);
void			db_aml_showvalue(struct aml_value *);
void			db_spaceit(int);
const char		*db_aml_objtype(struct aml_value *);
const char		*db_aml_opname(int);
const char		*db_opregion(int);
struct aml_opcode	*db_findem(int);
int			db_aml_nodetype(struct aml_node *);

extern struct aml_node	aml_root;

/* Perfect hash values for AML opcodes */
#define HASH_VAL     11
#define HASH_SIZE    204
#define HASH_KEY(v)  (((v) * HASH_VAL) % HASH_SIZE)

struct aml_opcode	*htab[HASH_SIZE];

/* line buffer */
char			buf[128];

/* name of scope for lexer */
char			scope[80];

const char *
db_opregion(int id)
{
	switch(id) {
	case 0:
		return "SystemMemory";
	case 1:
		return "SystemIO";
	case 2:
		return "PCIConfig";
	case 3:
		return "Embedded";
	case 4:
		return "SMBus";
	case 5:
		return "CMOS";
	case 6:
		return "PCIBAR";
	}
	return "";
}
void
db_aml_dump(int len, u_int8_t *buf)
{
	int		idx;
	
	db_printf("{ ");
	for (idx = 0; idx < len; idx++) {
		db_printf("%s0x%.2x", idx ? ", " : "", buf[idx]);
	}
	db_printf(" }\n");
}

void
db_aml_showvalue(struct aml_value *value)
{
	int idx;

	if (value == NULL)
		return;

	if (value->node)
		db_printf("node:%.8x ", value->node);

	if (value->name)
		db_printf("name:%s ", value->name);

	switch (value->type) {
	case AML_OBJTYPE_OBJREF:
		db_printf("refof: %x {\n", value->v_objref.index);
		db_aml_showvalue(value->v_objref.ref);
		db_printf("}\n");
		break;
	case AML_OBJTYPE_NAMEREF:
		db_printf("nameref: %s %.8x\n", value->name,
			 value->v_objref.ref);
		break;
	case AML_OBJTYPE_STATICINT:
	case AML_OBJTYPE_INTEGER:
		db_printf("integer: %llx %s\n", value->v_integer,
		    (value->type == AML_OBJTYPE_STATICINT) ? "(static)" : "");
		break;
	case AML_OBJTYPE_STRING:
		db_printf("string: %s\n", value->v_string);
		break;
	case AML_OBJTYPE_PACKAGE:
		db_printf("package: %d {\n", value->length);
		for (idx = 0; idx < value->length; idx++)
			db_aml_showvalue(value->v_package[idx]);
		db_printf("}\n");
		break;
	case AML_OBJTYPE_BUFFER:
		db_printf("buffer: %d ", value->length);
		db_aml_dump(value->length, value->v_buffer);
		break;
	case AML_OBJTYPE_DEBUGOBJ:
		db_printf("debug");
		break;
	case AML_OBJTYPE_MUTEX:
		db_printf("mutex : %llx\n", value->v_integer);
		break;
	case AML_OBJTYPE_DEVICE:
		db_printf("device\n");
		break;
	case AML_OBJTYPE_EVENT:
		db_printf("event\n");
		break;
	case AML_OBJTYPE_PROCESSOR:
		db_printf("cpu: %x,%x,%x\n",
		    value->v_processor.proc_id,
		    value->v_processor.proc_addr,
		    value->v_processor.proc_len);
		break;
	case AML_OBJTYPE_METHOD:
		db_printf("method: args=%d, serialized=%d, synclevel=%d\n",
		    AML_METHOD_ARGCOUNT(value->v_method.flags),
		    AML_METHOD_SERIALIZED(value->v_method.flags),
		    AML_METHOD_SYNCLEVEL(value->v_method.flags));
		break;
	case AML_OBJTYPE_FIELDUNIT:
		db_printf("%s: access=%x,lock=%x,update=%x pos=%.4x "
		    "len=%.4x\n",
		    db_aml_opname(value->v_field.type),
		    AML_FIELD_ACCESS(value->v_field.flags),
		    AML_FIELD_LOCK(value->v_field.flags),
		    AML_FIELD_UPDATE(value->v_field.flags),
		    value->v_field.bitpos,
		    value->v_field.bitlen);

		db_aml_showvalue(value->v_field.ref1);
		db_aml_showvalue(value->v_field.ref2);
		break;
	case AML_OBJTYPE_BUFFERFIELD:
		db_printf("%s: pos=%.4x len=%.4x ", 
		    db_aml_opname(value->v_field.type),
		    value->v_field.bitpos,
		    value->v_field.bitlen);

		db_aml_dump(aml_bytelen(value->v_field.bitlen), 
		    value->v_field.ref1->v_buffer + 
		    aml_bytepos(value->v_field.bitpos));

		db_aml_showvalue(value->v_field.ref1);
		break;
	case AML_OBJTYPE_OPREGION:
		db_printf("opregion: %s,0x%llx,0x%x\n",
		    db_opregion(value->v_opregion.iospace),
		    value->v_opregion.iobase,
		    value->v_opregion.iolen);
		break;
	default:
		db_printf("unknown: %d\n", value->type);
		break;
	}
}

/* Output disassembled line */
void
db_spaceit(int len)
{
	while(len--) {
		db_printf("..");
	}
}

struct aml_opcode
*db_findem(int opcode)
{
	int			key, cnt;

	cnt = 0;
	key = HASH_KEY(opcode);

	while (htab[key] != NULL && htab[key]->opcode != opcode) {
		key = (key + 1) % HASH_SIZE;
		cnt++;
	}

	return (htab[key]);
}

int
db_aml_nodetype(struct aml_node *node)
{
	return (node && node->value) ? node->value->type : -1;
}

const char *
db_aml_opname(int opcode)
{
	struct aml_opcode	*opc;

	opc = db_findem(opcode);

	return (opc ? opc->mnem : "");
}

const char *
db_aml_objtype(struct aml_value *val)
{
	if (val == NULL)
		return "nil";

	switch (val->type) {
	case AML_OBJTYPE_STATICINT:
		return "staticint";
	case AML_OBJTYPE_INTEGER:
		return "integer";
	case AML_OBJTYPE_STRING:
		return "string";
	case AML_OBJTYPE_BUFFER:
		return "buffer";
	case AML_OBJTYPE_PACKAGE:
		return "package";
	case AML_OBJTYPE_DEVICE:
		return "device";
	case AML_OBJTYPE_EVENT:
		return "event";
	case AML_OBJTYPE_METHOD:
		return "method";
	case AML_OBJTYPE_MUTEX:
		return "mutex";
	case AML_OBJTYPE_OPREGION:
		return "opregion";
	case AML_OBJTYPE_POWERRSRC:
		return "powerrsrc";
	case AML_OBJTYPE_PROCESSOR:
		return "processor";
	case AML_OBJTYPE_THERMZONE:
		return "thermzone";
	case AML_OBJTYPE_DDBHANDLE:
		return "ddbhandle";
	case AML_OBJTYPE_DEBUGOBJ:
		return "debugobj";
	case AML_OBJTYPE_NAMEREF:
		return "nameref";
	case AML_OBJTYPE_OBJREF:
		return "refof";
	case AML_OBJTYPE_FIELDUNIT:
	case AML_OBJTYPE_BUFFERFIELD: 
		return db_aml_opname(val->v_field.type);
	};

	return ("");
}

void
db_aml_disline(uint8_t *pos, int level, const char *fmt, ...)
{
	va_list			ap;

	db_printf("%.4x %.2x ", pos - aml_root.start, level);
	db_spaceit(level);

	va_start(ap, fmt);
	vsnprintf(buf, sizeof buf, fmt, ap);
	db_printf(buf);
	va_end(ap);

	db_printf("\n");
}

/* Output disassembled integer */
void
db_aml_disint(struct acpi_context *ctx, int level, int type)
{
	u_int8_t		*pos;
	int64_t			i1;

	pos = ctx->pos;
	i1 = aml_eparseint(ctx, type);
	db_aml_disline(pos, level, "%c:0x%.8llx (%lld)", type, i1, i1);
}

/* Disassemble AML Opcode */
void
db_aml_disasm(struct acpi_context *ctx, int level)
{
	struct aml_opcode	*opc;
	uint8_t			*end, *np;
	const char		*arg, *fname;
	struct aml_node		*node;
	int			idx, len;

#if 0
	/* if we want separators */
	if (level == 0) 
		db_printf("<<<<<<<<<<<<<<\n");
#endif
	np = ctx->pos;
	opc = aml_getopcode(ctx);
	db_aml_disline(np, level, opc->mnem);
	for (arg = opc->args; *arg; arg++) {
		np = ctx->pos;
		switch (*arg) {
		case AML_ARG_BYTE: 
		case AML_ARG_WORD:
		case AML_ARG_DWORD:
		case AML_ARG_QWORD: 
			db_aml_disint(ctx, level + 1, *arg);
			break;
		case AML_ARG_STRING: 
			db_aml_disline(np, level + 1, ctx->pos);
			ctx->pos += strlen(ctx->pos) + 1;
			break;
		case AML_ARG_NAMESTRING:
			fname = aml_parse_name(ctx);
			db_aml_disline(np, level + 1, fname);
			break;
		case AML_ARG_NAMEREF:
			fname = aml_parse_name(ctx);
			node = aml_searchname(ctx->scope, fname);
			db_aml_disline(np, level + 1, "%s:%s", fname, 
			    node ? db_aml_objtype(node->value) : "none");
			if (db_aml_nodetype(node) == AML_OBJTYPE_METHOD)
				/* Parse method arguments */
				for (idx = 0; idx < AML_METHOD_ARGCOUNT(node->value->v_method.flags); idx++)
					db_aml_disasm(ctx, level + 1);
			break;
		case AML_ARG_INTEGER:
		case AML_ARG_TERMOBJ:
		case AML_ARG_DATAOBJ:
		case AML_ARG_SIMPLENAME:
		case AML_ARG_SUPERNAME:
			db_aml_disasm(ctx, level + 1);
			break;
		case AML_ARG_DATAOBJLIST:
		case AML_ARG_TERMOBJLIST:
		case AML_ARG_METHOD:
			while (ctx->pos < end)
				db_aml_disasm(ctx, level + 1);
			break;
		case AML_ARG_BYTELIST:
			for (idx = 0; idx < end - ctx->pos - 7; idx += 8) {
				db_aml_disline(np, level + 1, "buf %.4x: %.2x "
				    "%.2x %.2x %.2x %.2x %.2x %.2x %.2x", 
				    idx, ctx->pos[idx],
				    ctx->pos[idx + 1], 
				    ctx->pos[idx + 2], ctx->pos[idx + 3],
				    ctx->pos[idx + 4], ctx->pos[idx + 5], 
				    ctx->pos[idx + 6], ctx->pos[idx + 7]);
			}
			ctx->pos = end;
			break;
		case AML_ARG_FLAG:
			/* Flags */
			idx = aml_eparseint(ctx, AML_ARG_BYTE);
			if (opc->opcode == AMLOP_METHOD)
				db_aml_disline(np, level + 1,
				    "args:%d serialized:%d synclevel:%d",
				    AML_METHOD_ARGCOUNT(idx),
				    AML_METHOD_SERIALIZED(idx),
				    AML_METHOD_SYNCLEVEL(idx));
			else
				db_aml_disline(np, level + 1,
				    "acc:%d lock:%d update:%d",
				    AML_FIELD_ACCESS(idx),
				    AML_FIELD_LOCK(idx),
				    AML_FIELD_UPDATE(idx));
			break;
		case AML_ARG_FIELDLIST:
			for (idx = 0; ctx->pos < end; idx += len) {
				np = ctx->pos;
				switch (*ctx->pos) {
				case AML_FIELD_RESERVED:
					ctx->pos++;
					len = aml_parse_length(ctx);
					break;
				case AML_FIELD_ATTR__:
					db_aml_disline(np, level + 1,
					    "-- attr %.2x %.2x", 
					    ctx->pos[1], ctx->pos[2]);
					ctx->pos += 3;
					len = 0;
					break;
				default:
					fname = aml_parse_name(ctx);
					len = aml_parse_length(ctx);
					db_aml_disline(np, level + 1,
					    "pos:%.4x len:%.4x name:%s", 
					    idx, len, fname);
					break;
				}
			}
			break;
		case AML_ARG_OBJLEN:
			end = aml_eparselen(ctx);
			break;
		}
	}
#if 0
	/* if we want separators */
	if (level == 0) 
		db_printf(">>>>>>>>>>>>>>\n");
#endif
}

void
db_aml_shownode(struct aml_node *node)
{
	db_printf(" opcode:%.4x  mnem:%s %s ",
	    node->opcode, node->mnem, node->name ? node->name : "");

	switch(node->opcode) {
	case AMLOP_METHOD:
		break;
		
	case AMLOP_NAMECHAR:
		db_printf("%s", node->value->name);
		break;

	case AMLOP_FIELD:
	case AMLOP_BANKFIELD:
	case AMLOP_INDEXFIELD:
		break;
		
	case AMLOP_BYTEPREFIX:
		db_printf("byte: %.2x", node->value->v_integer);
		break;
	case AMLOP_WORDPREFIX:
		db_printf("word: %.4x", node->value->v_integer);
		break;
	case AMLOP_DWORDPREFIX:
		db_printf("dword: %.8x", node->value->v_integer);
		break;
	case AMLOP_STRINGPREFIX:
		db_printf("string: %s", node->value->v_string);
		break;
	}
	db_printf("\n");
}

void
db_aml_walktree(struct aml_node *node)
{
	int			i;

	while(node) {
		db_printf(" %d ", node->depth);
		for(i = 0; i < node->depth; i++)
			db_printf("..");

		db_aml_shownode(node);
		db_aml_walktree(node->child);
		node = node->sibling;
	}
}

/* ddb interface */
void
db_acpi_showval(db_expr_t addr, int haddr, db_expr_t count, char *modif)
{
	struct aml_node 		*node;
	int				t;

	memset(scope, 0, sizeof scope);
	do {
		t = db_read_token();
		if (t == tIDENT) {
			if (strlcat(scope, db_tok_string, sizeof scope) >=
			    sizeof scope) {
				printf("Input too long\n");
				goto error;
			}
			t = db_read_token();
			if (t == tDOT)
				if (strlcat(scope, ".", sizeof scope) >=
				    sizeof scope) {
					printf("Input too long 2\n");
					goto error;
				}
		}
	}
	while (t != tEOL);

	if (!strlen(scope)) {
		db_printf("Invalid input\n");
		goto error;
	}

	/* get rid of the rest of input */
	db_flush_lex();

	node = aml_searchname(&aml_root, scope);
	if (node)
		db_aml_showvalue(node->value);
	else
		db_printf("Not a valid value\n");

error:
	db_flush_lex();
}

void
db_acpi_disasm(db_expr_t addr, int haddr, db_expr_t count, char *modif)
{
	extern struct acpi_softc	*acpi_softc;
	struct acpi_softc 		*sc = acpi_softc;
	struct acpi_context 		*ctx;
	struct aml_node 		*node;
	int				t;

	memset(scope, 0, sizeof scope);
	do {
		t = db_read_token();
		if (t == tIDENT) {
			if (strlcat(scope, db_tok_string, sizeof scope) >=
			    sizeof scope) {
				printf("Input too long\n");
				goto error;
			}
			t = db_read_token();
			if (t == tDOT)
				if (strlcat(scope, ".", sizeof scope) >=
				    sizeof scope) {
					printf("Input too long 2\n");
					goto error;
				}
		}
	}
	while (t != tEOL);

	if (!strlen(scope)) {
		db_printf("Invalid input\n");
		goto error;
	}

	/* get rid of the rest of input */
	db_flush_lex();

	ctx = acpi_alloccontext(sc, &aml_root, 0, NULL);
	node = aml_searchname(&aml_root, scope);
	if (node && node->value && node->value->type == AML_OBJTYPE_METHOD) {
		ctx->pos = node->value->v_method.start;
		while (ctx->pos < node->value->v_method.end)
			db_aml_disasm(ctx, 0);
	} else
		db_printf("Not a valid method\n");

	acpi_freecontext(ctx);

error:
	db_flush_lex();
}

void
db_acpi_tree(db_expr_t addr, int haddr, db_expr_t count, char *modif)
{
	db_aml_walktree(aml_root.child);

}
