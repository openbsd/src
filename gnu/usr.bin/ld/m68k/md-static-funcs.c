/*	$OpenBSD: md-static-funcs.c,v 1.2 1998/03/26 19:46:58 niklas Exp $	*/


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
	if (r->r_relative) {
		*(long *)addr += relocation;
		_cachectl (addr, 4);		/* maintain cache coherency */
	}
}

