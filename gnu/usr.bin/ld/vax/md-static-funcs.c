/*	$OpenBSD: md-static-funcs.c,v 1.1 1996/07/27 10:39:58 maja Exp $	*/
/*	$NetBSD: md-static-funcs.c,v 1.1 1995/10/19 13:10:17 ragge Exp $	*/
/*
 * Called by ld.so when onanating.
 * This *must* be a static function, so it is not called through a jmpslot.
 */

static void
md_relocate_simple(r, relocation, addr)
struct relocation_info	*r;
long			relocation;
char			*addr;
{
if (r->r_relative)
	*(long *)addr += relocation;
}

