/* $OpenBSD: byte.c,v 1.6 2004/10/09 20:26:57 mickey Exp $ */
/*
 * Copyright (c) 1999
 *	Marc Espie.  All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 */

/*
 * A set of routines to read object files and compensate for host
 * endianness
 */
static int
byte_sex(int mid)
{
	switch (mid) {
	case MID_I386:
	case MID_VAX:
	case MID_ALPHA:
	case MID_PMAX:
		return LITTLE_ENDIAN;
	case MID_M68K:
	case MID_M68K4K:
	case MID_M88K:
	case MID_SUN010:
	case MID_SUN020:
	case MID_HP200:
	case MID_HP300:
	case MID_HPPA:
	case MID_HPUX800:
	case MID_HPUX:
	case MID_SPARC:
	case MID_MIPS:
	case MID_SPARC64:
	case MID_POWERPC:
		return BIG_ENDIAN;
	default:
		/* we don't know what this is, so we don't want to process it */
		return 0;
	}
}

#define BAD_OBJECT(h)  (N_BADMAG(h) || !byte_sex(N_GETMID(h)))

/* handles endianess swaps */
static void
swap_u32s(u_int32_t *h, size_t n)
{
	size_t i;

	for (i = 0; i < n; i++)
		h[i] = swap32(h[i]);
}

static void
fix_header_order(struct exec *h)
{
	if (byte_sex(N_GETMID(*h)) != BYTE_ORDER)
		swap_u32s( ((u_int32_t *)(h))+1, sizeof *h/sizeof(u_int32_t) - 1);
}

static long
fix_32_order(u_int32_t l, int mid)
{
	if (byte_sex(mid) != BYTE_ORDER)
		return swap32(l);
	else
		return l;
}

static void
swap_nlist(struct nlist *p)
{
	p->n_un.n_strx = swap32(p->n_un.n_strx);
	p->n_desc = swap16(p->n_desc);
	p->n_value = swap32(p->n_value);
}

static void
fix_nlist_order(struct nlist *p, int mid)
{
	if (byte_sex(mid) != BYTE_ORDER)
		swap_nlist(p);
}

static void
fix_nlists_order(struct nlist *p, size_t n, int mid)
{
	size_t i;

	if (byte_sex(mid) != BYTE_ORDER)
		for (i = 0; i < n; i++)
			swap_nlist(p+i);
}

static void
fix_ranlib_order(struct ranlib *r, int mid)
{
	if (byte_sex(mid) != BYTE_ORDER) {
		r->ran_un.ran_strx = swap32(r->ran_un.ran_strx);
		r->ran_off = swap32(r->ran_off);
	}
}
