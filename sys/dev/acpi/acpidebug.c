/* $OpenBSD: acpidebug.c,v 1.1 2006/03/05 14:46:46 marco Exp $ */
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

#include <machine/bus.h>

#include <dev/acpi/acpireg.h>
#include <dev/acpi/acpivar.h>
#include <dev/acpi/amltypes.h>
#include <dev/acpi/acpidebug.h>
#include <dev/acpi/dsdt.h>

void db_aml_walktree(struct aml_node *);
void db_aml_shownode(struct aml_node *);

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
	int i;

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
db_acpi_tree(db_expr_t addr, int haddr, db_expr_t count, char *modif)
{
	extern struct aml_node	aml_root;

	db_aml_walktree(aml_root.child);
}
