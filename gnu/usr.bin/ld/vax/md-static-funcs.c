/*	$OpenBSD: md-static-funcs.c,v 1.2 2002/07/19 19:28:12 marc Exp $	*/
/*	$NetBSD: md-static-funcs.c,v 1.1 1995/10/19 13:10:17 ragge Exp $	*/
/*
 * Called by ld.so when onanating.
 * This *must* be a static function, so it is not called through a jmpslot.
 */

static void
md_relocate_simple(struct relocation_info *r, long relocation, char *addr)
{
	if (r->r_relative)
		*(long *)addr += relocation;
}

