/* $OpenBSD: acpidebug.c,v 1.18 2008/05/16 06:50:55 dlg Exp $ */
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
#include <sys/malloc.h>

#include <dev/acpi/acpireg.h>
#include <dev/acpi/acpivar.h>
#include <dev/acpi/amltypes.h>
#include <dev/acpi/acpidebug.h>
#include <dev/acpi/dsdt.h>

void db_aml_disline(uint8_t *, int, const char *, ...);
void db_aml_disint(struct aml_scope *, int, int);
uint8_t *db_aml_disasm(struct aml_node *, uint8_t *, uint8_t *, int, int);

extern int aml_pc(uint8_t *);
extern struct aml_scope *aml_pushscope(struct aml_scope *, uint8_t *, uint8_t *, struct aml_node *);
extern struct aml_scope *aml_popscope(struct aml_scope *);
extern uint8_t *aml_parsename(struct aml_scope *);
extern uint8_t *aml_parseend(struct aml_scope *);
extern int aml_parselength(struct aml_scope *);
extern int aml_parseopcode(struct aml_scope *);

extern const char *aml_mnem(int opcode, uint8_t *);
extern const char *aml_args(int opcode);
extern const char *aml_getname(uint8_t *);
extern const char *aml_nodename(struct aml_node *);

const char		*db_aml_objtype(struct aml_value *);
const char		*db_opregion(int);
int			db_parse_name(void);
void			db_aml_dump(int, u_int8_t *);
void			db_aml_showvalue(struct aml_value *);
void			db_aml_walktree(struct aml_node *);

const char		*db_aml_fieldacc(int);
const char		*db_aml_fieldlock(int);
const char		*db_aml_fieldupdate(int);

extern struct aml_node	aml_root;

/* name of scope for lexer */
char			scope[80];

const char *
db_opregion(int id)
{
	switch (id) {
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
	for (idx = 0; idx < len; idx++)
		db_printf("%s0x%.2x", idx ? ", " : "", buf[idx]);
	db_printf(" }\n");
}

void
db_aml_showvalue(struct aml_value *value)
{
	int		idx;

	if (value == NULL)
		return;

	if (value->node)
		db_printf("[%s] ", aml_nodename(value->node));

	switch (value->type & ~AML_STATIC) {
	case AML_OBJTYPE_OBJREF:
		db_printf("refof: %x {\n", value->v_objref.index);
		db_aml_showvalue(value->v_objref.ref);
		db_printf("}\n");
		break;
	case AML_OBJTYPE_NAMEREF:
		db_printf("nameref: %s\n", value->v_nameref);
		break;
	case AML_OBJTYPE_INTEGER:
		db_printf("integer: %llx %s\n", value->v_integer,
		    (value->type & AML_STATIC) ? "(static)" : "");
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
		    aml_mnem(value->v_field.type, NULL),
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
		    aml_mnem(value->v_field.type, NULL),
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

const char *
db_aml_objtype(struct aml_value *val)
{
	if (val == NULL)
		return "nil";

	switch (val->type) {
	case AML_OBJTYPE_INTEGER+AML_STATIC:
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
		return aml_mnem(val->v_field.type, NULL);
	}

	return ("");
}

void
db_aml_walktree(struct aml_node *node)
{
	while (node) {
		db_aml_showvalue(node->value);
		db_aml_walktree(node->child);

		node = node->sibling;
	}
}

int
db_parse_name(void)
{
	int		t, rv = 1;

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
	} while (t != tEOL);

	if (!strlen(scope)) {
		db_printf("Invalid input\n");
		goto error;
	}

	rv = 0;
error:
	/* get rid of the rest of input */
	db_flush_lex();
	return (rv);
}

/* ddb interface */
void
db_acpi_showval(db_expr_t addr, int haddr, db_expr_t count, char *modif)
{
	struct aml_node *node;

	if (db_parse_name())
		return;

	node = aml_searchname(&aml_root, scope);
	if (node)
		db_aml_showvalue(node->value);
	else
		db_printf("Not a valid value\n");
}

void
db_acpi_disasm(db_expr_t addr, int haddr, db_expr_t count, char *modif)
{
	struct aml_node *node;

	if (db_parse_name())
		return;

	node = aml_searchname(&aml_root, scope);
	if (node && node->value && node->value->type == AML_OBJTYPE_METHOD) {
		db_aml_disasm(node, node->value->v_method.start,
		    node->value->v_method.end, -1, 0);
	} else
		db_printf("Not a valid method\n");
}

void
db_acpi_tree(db_expr_t addr, int haddr, db_expr_t count, char *modif)
{
	db_aml_walktree(aml_root.child);
}

void
db_acpi_trace(db_expr_t addr, int haddr, db_expr_t count, char *modif)
{
	struct aml_scope *root;
	int idx;
	extern struct aml_scope *aml_lastscope;

	for (root=aml_lastscope; root && root->pos; root=root->parent) {
		db_printf("%.4x Called: %s\n", aml_pc(root->pos),
		    aml_nodename(root->node));
		for (idx = 0; idx<root->nargs; idx++) {
			db_printf("  arg%d: ", idx);
			db_aml_showvalue(&root->args[idx]);
		}
		for (idx = 0; root->locals && idx < AML_MAX_LOCAL; idx++) {
			if (root->locals[idx].type) {
				db_printf("  local%d: ", idx);
				db_aml_showvalue(&root->locals[idx]);
			}
		}
	}
}

void
db_aml_disline(uint8_t *pos, int depth, const char *fmt, ...)
{
	va_list ap;
	char line[128];

	db_printf("%.6x: ", aml_pc(pos));
	while (depth--)
		db_printf("  ");

	va_start(ap, fmt);
	vsnprintf(line, sizeof(line), fmt, ap);
	db_printf(line);
	va_end(ap);
}

void
db_aml_disint(struct aml_scope *scope, int opcode, int depth)
{
	switch (opcode) {
	case AML_ANYINT:
		db_aml_disasm(scope->node, scope->pos, scope->end, -1, depth);
		break;
	case AMLOP_BYTEPREFIX:
		db_aml_disline(scope->pos, depth, "0x%.2x\n",
		    *(uint8_t *)(scope->pos));
		scope->pos += 1;
		break;
	case AMLOP_WORDPREFIX:
		db_aml_disline(scope->pos, depth, "0x%.4x\n",
		    *(uint16_t *)(scope->pos));
		scope->pos += 2;
		break;
	case AMLOP_DWORDPREFIX:
		db_aml_disline(scope->pos, depth, "0x%.8x\n",
		    *(uint32_t *)(scope->pos));
		scope->pos += 4;
		break;
	case AMLOP_QWORDPREFIX:
		db_aml_disline(scope->pos, depth, "0x%.4llx\n",
		    *(uint64_t *)(scope->pos));
		scope->pos += 8;
		break;
	}
}

uint8_t *
db_aml_disasm(struct aml_node *root, uint8_t *start, uint8_t *end,
    int count, int depth)
{
	int idx, opcode, len, off=0;
	struct aml_scope *scope;
	uint8_t *name, *pos;
	const char *mnem, *args;
	struct aml_node *node;
	char *tmpstr;

	if (start == end)
		return end;

	scope = aml_pushscope(NULL, start, end, root);
	while (scope->pos < scope->end && count--) {
		pos = scope->pos;
		start = scope->pos;
		opcode = aml_parseopcode(scope);

		mnem = aml_mnem(opcode, scope->pos);
		args = aml_args(opcode);

		if (*args == 'p') {
			end = aml_parseend(scope);
			args++;
		}
		node = scope->node;
		if (*args == 'N') {
			name = aml_parsename(scope);
			node = aml_searchname(scope->node, name);
			db_aml_disline(pos, depth, "%s %s (%s)\n",
			    mnem, aml_getname(name), aml_nodename(node));
			args++;
		} else if (mnem[0] != '.') {
			db_aml_disline(pos, depth, "%s\n", mnem);
		}
		while (*args) {
			pos = scope->pos;
			switch (*args) {
			case 'k':
			case 'c':
			case 'D':
			case 'L':
			case 'A':
				break;
			case 'i':
			case 't':
			case 'S':
			case 'r':
				scope->pos = db_aml_disasm(node, scope->pos,
				    scope->end, 1, depth+1);
				break;
			case 'T':
			case 'M':
				scope->pos = db_aml_disasm(node, scope->pos,
				    end, -1, depth+1);
				break;
			case 'I':
				/* Special case: if */
				scope->pos = db_aml_disasm(node, scope->pos,
				    end, -1, depth+1);
				if (scope->pos >= scope->end)
					break;
				if (*scope->pos == AMLOP_ELSE) {
					++scope->pos;
					end = aml_parseend(scope);
					db_aml_disline(scope->pos, depth, "Else\n");
					scope->pos = db_aml_disasm(node, scope->pos,
					    end, -1, depth+1);
				}
				break;
			case 'N':
				name = aml_parsename(scope);
				db_aml_disline(pos, depth+1, "%s\n", aml_getname(name));
				break;
			case 'n':
				off = (opcode != AMLOP_NAMECHAR);
				name = aml_parsename(scope);
				node = aml_searchname(scope->node, name);
				db_aml_disline(pos, depth+off, "%s <%s>\n",
				    aml_getname(name), aml_nodename(node));

				if (!node || !node->value ||
				    node->value->type != AML_OBJTYPE_METHOD)
					break;

				/* Method calls */
				for (idx = 0;
				    idx < AML_METHOD_ARGCOUNT(node->value->v_method.flags);
				    idx++) {
					scope->pos = db_aml_disasm(node, scope->pos,
					    scope->end, 1, depth+1);
				}
				break;
			case 'b':
				off = (opcode != AMLOP_BYTEPREFIX);
				db_aml_disint(scope, AMLOP_BYTEPREFIX, depth+off);
				break;
			case 'w':
				off = (opcode != AMLOP_WORDPREFIX);
				db_aml_disint(scope, AMLOP_WORDPREFIX, depth+off);
				break;
			case 'd':
				off = (opcode != AMLOP_DWORDPREFIX);
				db_aml_disint(scope, AMLOP_DWORDPREFIX, depth+off);
				break;
			case 's':
				db_aml_disline(pos, depth, "\"%s\"\n", scope->pos);
				scope->pos += strlen(scope->pos)+1;
				break;
			case 'B':
				tmpstr = malloc(16 * 6 + 1, M_DEVBUF, M_WAITOK);
				for (idx = 0; idx < min(end-scope->pos, 8); idx++)
					snprintf(tmpstr+idx*6, 7, "0x%.2x, ",
					    scope->pos[idx]);
				db_aml_disline(pos, depth+1, "ByteList <%s>\n", tmpstr);
				free(tmpstr, M_DEVBUF);
				scope->pos = end;
				break;
			case 'F':
				off = 0;
				while (scope->pos < end) {
					len = 0;
					pos = scope->pos;
					switch (*scope->pos) {
					case 0x00: // reserved
						scope->pos++;
						len = aml_parselength(scope);
						db_aml_disline(pos, depth+1,
						    "Reserved\t%.4x,%.4x\n",
						    off, len);
						break;
					case 0x01: // attr
						db_aml_disline(pos, depth+1,
						    "Attr:%.2x,%.2x\n",
						    scope->pos[1], scope->pos[2]);
						scope->pos += 3;
						break;
					default:
						name = aml_parsename(scope);
						len = aml_parselength(scope);
						db_aml_disline(pos, depth+1,
						    "NamedField\t%.4x,%.4x %s\n",
						    off, len, aml_getname(name));
					}
					off += len;
				}
				scope->pos = end;
				break;
			default:
				db_printf("remaining args: '%s'\n", args);
			}
			args++;
		}
	}
	pos = scope->pos;
	aml_popscope(scope);
	return pos;
}
