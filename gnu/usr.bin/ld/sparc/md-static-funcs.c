/* *	$OpenBSD: md-static-funcs.c,v 1.4 2002/07/19 19:28:12 marc Exp $*/

/*
 *
 * Simple SPARC relocations for the benefit of self-relocation of ld.so
 * avoiding the use of global variables (ie. reloc_bitshift[] et. al.).
 * Only types supported are RELOC_32 and RELOC_RELATIVE.
 *
 * This *must* be a static function, so it is not called through a jmpslot.
 */
static void
md_relocate_simple(struct relocation_info *r, long relocation, char *addr)
{
	unsigned long	mask;
	unsigned long	shift;

	switch (r->r_type) {
	case RELOC_32:
		mask = 0xffffffff;
		shift = 0;
		break;
	case RELOC_RELATIVE:
		mask = 0x003fffff;
		shift = 10;
		break;
	}
	relocation += (*(long *)addr & mask) << shift;
	relocation >>= shift;
	relocation &= mask;

	*(long *) (addr) &= ~mask;
	*(long *) (addr) |= relocation;
}

