/*	$OpenBSD: db_struct.c,v 1.1 2009/08/09 23:04:49 miod Exp $	*/

/*
 * Copyright (c) 2009 Miodrag Vallat.
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

/*
 * ddb routines to describe struct information
 */

#include <sys/param.h>
#include <sys/systm.h>

#include <machine/db_machdep.h>

#include <ddb/db_lex.h>
#include <ddb/db_output.h>
#include <ddb/db_access.h>
#include <ddb/db_command.h>
#include <ddb/db_extern.h>
#include <ddb/db_interface.h>
#include <ddb/db_var.h>

#include "db_structinfo.h"

void	db_struct_print_field(uint, int, db_expr_t);

/*
 * Flags to pass db_struct_printf().
 */

#define	DBSP_STRUCT_NAME	0x01	/* prepend struct name */
#define	DBSP_VALUE		0x02	/* display field value */

void
db_struct_print_field(uint fidx, int flags, db_expr_t baseaddr)
{
	const struct ddb_field_info *field;
	const struct ddb_struct_info *struc;
	db_expr_t value;
	uint tmp;
	size_t namelen;
	int width, basecol, curcol;
	char tmpfmt[28];

	field = &ddb_field_info[fidx];
	basecol = 0;

	if (ISSET(flags, DBSP_STRUCT_NAME)) {
		struc = &ddb_struct_info[field->sidx];
		namelen = strlen(struc->name);
		db_printf("%-30s ", struc->name);
		if (namelen > 30)
			basecol += namelen + 1;
		else
			basecol += 30 + 1;
	}

	namelen = strlen(field->name);
	if (field->nitems == 1) {
		db_printf("%-30s ", field->name);
		if (namelen > 30)
			basecol += namelen + 1;
		else
			basecol += 30 + 1;
	} else {
		width = 30 - 2;
		tmp = field->nitems;
		while (tmp != 0) {
			width--;
			tmp /= 10;
		}
		if (namelen >= width) {
			db_printf("%s[%zu] ", field->name, field->nitems);
			basecol += namelen + (30 - width) + 1;
		} else {
			db_printf("%s[%zu]%*s ", field->name, field->nitems,
			    width - (int)namelen, "");
			/* namelen + (30-width) + (width-namelen) + 1 */
			basecol += 30 + 1;
		}
	}

	if (field->size == 0) {
		db_printf("bitfield");
		/* basecol irrelevant from there on */
	} else {
		snprintf(tmpfmt, sizeof tmpfmt, "%zu", field->size);
		basecol += strlen(tmpfmt) + 1;
		db_printf("%s ", tmpfmt);
	}

	if (ISSET(flags, DBSP_VALUE)) {
		/* only print the field value if it has a friendly size. */
		switch (field->size) {
		case 1:
			width = 4;
			break;
		case 2:
			width = 8;
			break;
		case 4:
			width = 12;
			break;
#ifdef __LP64__
		case 8:
			width = 20;
			break;
#endif
		default:
			width = 0;
		}
		if (width != 0) {
			baseaddr += field->offs;
			curcol = basecol;
			for (tmp = field->nitems; tmp != 0; tmp--) {
				value = db_get_value(baseaddr, field->size,
				    FALSE); /* assume unsigned */
				db_format(tmpfmt, sizeof tmpfmt, (long)value,
				    DB_FORMAT_N, 0, width);
				if (field->nitems > 1)
					db_printf("%s", tmpfmt);
				else
					db_printf("%20s", tmpfmt);
				baseaddr += field->size;

				/*
				 * Try to fit array elements on as few lines
				 * as possible.
				 */
				if (field->nitems > 1 && tmp > 1) {
					curcol += width + 1;
					if (basecol >= db_max_width ||
					    curcol + width >= db_max_width) {
						/* new line */
						db_printf("\n");
						if (basecol + width >=
						    db_max_width) {
							db_printf("\t");
							curcol = 8;
						} else {
							db_printf("%*s",
							    basecol, "");
							curcol = basecol;
						}
					} else
						db_printf(" ");
				}
			}
		}
	}

	db_printf("\n");
}


/*
 * show offset <value>: displays the list of struct fields which exist
 * at that particular offset from the beginning of the struct.
 */
void
db_struct_offset_cmd(db_expr_t addr, int have_addr, db_expr_t count,
    char *modifiers)
{
	db_expr_t offset = 0;
	const struct ddb_field_offsets *field;
	const uint *fidx;
	uint oidx;
	int width;
	char tmpfmt[28];

	/*
	 * Read the offset from the debuggger input.
	 * We don't want to get it from the standard parsing code, because
	 * this would set `dot' to this value, which doesn't make sense.
	 */

	if (!db_expression(&offset) || offset < 0) {
		db_printf("not a valid offset\n");
		db_flush_lex();
		return;
	}

	db_skip_to_eol();

	for (field = ddb_field_offsets, oidx = 0; oidx < NOFFS; field++, oidx++)
		if (field->offs == (size_t)offset)
			break;

	if (oidx == NOFFS) {
		db_format(tmpfmt, sizeof tmpfmt, (long)offset,
		    DB_FORMAT_N, 0, width);
		db_printf("no known structure element at offset %-*s\n",
		    width, tmpfmt);
		db_flush_lex();
		return;
	}

	db_printf("%-30s %-30s size\n", "struct", "member");
	for (fidx = field->list; *fidx != 0; fidx++)
		db_struct_print_field(*fidx, DBSP_STRUCT_NAME, 0);
}

/*
 * show struct <struct name> [addr]: displays the data starting at addr
 * (`dot' if unspecified) as a struct of the given type.
 */
void
db_struct_layout_cmd(db_expr_t addr, int have_addr, db_expr_t count,
    char *modifiers)
{
	const struct ddb_struct_info *struc;
	uint sidx, fidx;
	int t;

	/*
	 * Read the struct name from the debugger input.
	 */

	t = db_read_token();
	if (t != tIDENT) {
		db_printf("Bad struct name\n");
		db_flush_lex();
		return;
	}

	for (struc = ddb_struct_info, sidx = 0; sidx < NSTRUCT;
	    struc++, sidx++)
		if (strcmp(struc->name, db_tok_string) == 0)
			break;

	if (sidx == NSTRUCT) {
		db_printf("unknown struct %s\n", db_tok_string);
		db_flush_lex();
		return;
	}

	/*
	 * Read the address, if any, from the debugger input.
	 * In that case, update `dot' value.
	 */

	if (db_expression(&addr)) {
		db_dot = (db_addr_t)addr;
		db_last_addr = db_dot;
	} else
		addr = (db_expr_t)db_dot;

	db_skip_to_eol();

	/*
	 * Display the structure contents.
	 */

	db_printf("struct %s at %p (%zu bytes)\n", struc->name, (vaddr_t)addr,
	    struc->size);
	for (fidx = struc->fmin; fidx <= struc->fmax; fidx++)
		db_struct_print_field(fidx, DBSP_VALUE, addr);
}
