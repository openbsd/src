/*	$OpenBSD: parse.c,v 1.15 2022/12/26 18:43:49 jmc Exp $ */

/*
 * Copyright (c) 2016-2017 Martin Pieuchot
 * Copyright (c) 2016 Jasper Lievisse Adriaanse <jasper@openbsd.org>
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
 * DWARF to IT (internal type) representation parser.
 */

#include <sys/queue.h>
#include <sys/tree.h>
#include <sys/types.h>
#include <sys/ctf.h>

#include <assert.h>
#include <limits.h>
#include <err.h>
#include <stdlib.h>
#include <string.h>

#include "itype.h"
#include "xmalloc.h"
#include "dwarf.h"
#include "dw.h"
#include "pool.h"

#ifdef DEBUG
#include <stdio.h>
#endif

#ifndef NOPOOL
struct pool it_pool, im_pool, ir_pool;
#endif /* NOPOOL */

#ifndef nitems
#define nitems(_a)	(sizeof((_a)) / sizeof((_a)[0]))
#endif

#define DPRINTF(x...)	do { /*printf(x)*/ } while (0)

#define VOID_OFFSET	1	/* Fake offset for generating "void" type. */

/*
 * Tree used to resolve per-CU types based on their offset in
 * the abbrev section.
 */
RB_HEAD(ioff_tree, itype);

/*
 * Per-type trees used to merge existing types with the ones of
 * a newly parsed CU.
 */
RB_HEAD(itype_tree, itype)	 itypet[CTF_K_MAX];

/*
 * Tree of symbols used to build a list matching the order of
 * the ELF symbol table.
 */
struct isymb_tree	 isymbt;

struct itype		*void_it;		/* no type is emited for void */
uint16_t		 tidx, fidx, oidx;	/* type, func & object IDs */
uint16_t		 long_tidx;		/* index of "long", for array */


void		 cu_stat(void);
void		 cu_parse(struct dwcu *, struct itype_queue *,
		     struct ioff_tree *);
void		 cu_resolve(struct dwcu *, struct itype_queue *,
		     struct ioff_tree *);
void		 cu_reference(struct dwcu *, struct itype_queue *);
void		 cu_merge(struct dwcu *, struct itype_queue *);

struct itype	*parse_base(struct dwdie *, size_t);
struct itype	*parse_refers(struct dwdie *, size_t, int);
struct itype	*parse_array(struct dwdie *, size_t);
struct itype	*parse_enum(struct dwdie *, size_t);
struct itype	*parse_struct(struct dwdie *, size_t, int, size_t);
struct itype	*parse_function(struct dwdie *, size_t);
struct itype	*parse_funcptr(struct dwdie *, size_t);
struct itype	*parse_variable(struct dwdie *, size_t);

void		 subparse_subrange(struct dwdie *, size_t, struct itype *);
void		 subparse_enumerator(struct dwdie *, size_t, struct itype *);
void		 subparse_member(struct dwdie *, size_t, struct itype *, size_t);
void		 subparse_arguments(struct dwdie *, size_t, struct itype *);

size_t		 dav2val(struct dwaval *, size_t);
const char	*dav2str(struct dwaval *);
const char	*enc2name(unsigned short);

struct itype	*it_new(uint64_t, size_t, const char *, uint32_t, uint16_t,
		     uint64_t, uint16_t, unsigned int);
void		 it_merge(struct itype *, struct itype *);
void		 it_reference(struct itype *);
void		 it_free(struct itype *);
int		 it_cmp(struct itype *, struct itype *);
int		 it_name_cmp(struct itype *, struct itype *);
int		 it_off_cmp(struct itype *, struct itype *);
void		 ir_add(struct itype *, struct itype *);
void		 ir_purge(struct itype *);
struct imember	*im_new(const char *, size_t, size_t);

RB_GENERATE(itype_tree, itype, it_node, it_cmp);
RB_GENERATE(isymb_tree, itype, it_node, it_name_cmp);
RB_GENERATE(ioff_tree, itype, it_node, it_off_cmp);

/*
 * Construct a list of internal type and functions based on DWARF
 * INFO and ABBREV sections.
 *
 * Multiple CUs are supported.
 */
void
dwarf_parse(const char *infobuf, size_t infolen, const char *abbuf,
    size_t ablen)
{
	struct dwbuf		 info = { .buf = infobuf, .len = infolen };
	struct dwbuf		 abbrev = { .buf = abbuf, .len = ablen };
	struct dwcu		*dcu = NULL;
	struct ioff_tree	 cu_iofft;
	struct itype_queue	 cu_itypeq;
	struct itype		*it;
	int			 i;

	for (i = 0; i < CTF_K_MAX; i++)
		RB_INIT(&itypet[i]);
	RB_INIT(&isymbt);

	void_it = it_new(++tidx, VOID_OFFSET, "void", 0,
	    CTF_INT_SIGNED, 0, CTF_K_INTEGER, 0);
	TAILQ_INSERT_TAIL(&itypeq, void_it, it_next);

	while (dw_cu_parse(&info, &abbrev, infolen, &dcu) == 0) {
		TAILQ_INIT(&cu_itypeq);

		/* We use a tree to speed-up type resolution. */
		RB_INIT(&cu_iofft);

		/* Parse this CU */
		cu_parse(dcu, &cu_itypeq, &cu_iofft);

		/* Resolve its types. */
		cu_resolve(dcu, &cu_itypeq, &cu_iofft);
		assert(RB_EMPTY(&cu_iofft));

		/* Mark used type as such. */
		cu_reference(dcu, &cu_itypeq);

#ifdef DEBUG
		/* Dump statistics for current CU. */
		cu_stat();
#endif

		/* Merge them with the common type list. */
		cu_merge(dcu, &cu_itypeq);

		dw_dcu_free(dcu);
	}

	/* We force array's index type to be 'long', for that we need its ID. */
	RB_FOREACH(it, itype_tree, &itypet[CTF_K_INTEGER]) {
		if (it_name(it) == NULL || it->it_size != (8 * sizeof(long)))
			continue;

		if (strcmp(it_name(it), "unsigned") == 0) {
			long_tidx = it->it_idx;
			break;
		}
	}
}

struct itype *
it_new(uint64_t index, size_t off, const char *name, uint32_t size,
    uint16_t enc, uint64_t ref, uint16_t type, unsigned int flags)
{
	struct itype *it;
#ifndef NOPOOL
	static int it_pool_inited = 0;

	if (!it_pool_inited) {
		pool_init(&it_pool, "it", 512, sizeof(struct itype));
		pool_init(&im_pool, "im", 1024, sizeof(struct imember));
		pool_init(&ir_pool, "ir", 1024, sizeof(struct itref));
		it_pool_inited = 1;
	}
#endif

	assert((name != NULL) || !(flags & (ITF_FUNC|ITF_OBJ)));

	it = pmalloc(&it_pool, sizeof(*it));
	SIMPLEQ_INIT(&it->it_refs);
	TAILQ_INIT(&it->it_members);
	it->it_off = off;
	it->it_ref = ref;
	it->it_refp = NULL;
	it->it_size = size;
	it->it_nelems = 0;
	it->it_enc = enc;
	it->it_idx = index;
	it->it_type = type;
	it->it_flags = flags;

	if (name == NULL) {
		it->it_flags |= ITF_ANON;
	} else {
		size_t n;

		if ((n = strlcpy(it->it_name, name, ITNAME_MAX)) > ITNAME_MAX)
			warnx("name %s too long %zd > %d", name, n, ITNAME_MAX);
	}

	return it;
}

struct itype *
it_dup(struct itype *it)
{
	struct imember *copim, *im;
	struct itype *copit;

	copit = it_new(it->it_idx, it->it_off, it_name(it), it->it_size,
	    it->it_enc, it->it_ref, it->it_type, it->it_flags);

	copit->it_refp = it->it_refp;
	copit->it_nelems = it->it_nelems;

	TAILQ_FOREACH(im, &it->it_members, im_next) {
		copim = im_new(im_name(im), im->im_ref, im->im_off);
		copim->im_refp = im->im_refp;
		TAILQ_INSERT_TAIL(&copit->it_members, copim, im_next);
	}

	return copit;
}

/*
 * Merge the content of ``it'', the full type declaration into the
 * forwarding representation ``fwd''.
 */
void
it_merge(struct itype *fwd, struct itype *it)
{
	assert(fwd->it_flags & ITF_FORWARD);
	assert(fwd->it_type == it->it_type);
	assert(TAILQ_EMPTY(&fwd->it_members));
	assert(SIMPLEQ_EMPTY(&it->it_refs));

	fwd->it_off = it->it_off;
	fwd->it_ref = it->it_ref;
	fwd->it_refp = it->it_refp;
	fwd->it_size = it->it_size;
	fwd->it_nelems = it->it_nelems;
	fwd->it_enc = it->it_enc;
	fwd->it_flags = it->it_flags;

	TAILQ_CONCAT(&fwd->it_members, &it->it_members, im_next);
	assert(TAILQ_EMPTY(&it->it_members));
}

const char *
it_name(struct itype *it)
{
	if (!(it->it_flags & ITF_ANON))
		return it->it_name;

	return NULL;
}

void
it_reference(struct itype *it)
{
	struct imember *im;

	if (it == NULL || it->it_flags & ITF_USED)
		return;

	it->it_flags |= ITF_USED;

	it_reference(it->it_refp);
	TAILQ_FOREACH(im, &it->it_members, im_next)
		it_reference(im->im_refp);
}

void
it_free(struct itype *it)
{
	struct imember *im;

	if (it == NULL)
		return;

	while ((im = TAILQ_FIRST(&it->it_members)) != NULL) {
		TAILQ_REMOVE(&it->it_members, im, im_next);
		pfree(&im_pool, im);
	}

	ir_purge(it);
	pfree(&it_pool, it);
}

/*
 * Return 0 if ``a'' matches ``b''.
 */
int
it_cmp(struct itype *a, struct itype *b)
{
	int diff;

	if ((diff = (a->it_type - b->it_type)) != 0)
		return diff;

	/* Basic types need to have the same size. */
	if ((a->it_type == CTF_K_INTEGER || a->it_type == CTF_K_FLOAT) &&
	    (diff = (a->it_size - b->it_size) != 0))
		return diff;

	/* Match by name */
	if (!(a->it_flags & ITF_ANON) && !(b->it_flags & ITF_ANON))
		return strcmp(it_name(a), it_name(b));

	/* Only one of them is anonym */
	if ((a->it_flags & ITF_ANON) != (b->it_flags & ITF_ANON))
		return (a->it_flags & ITF_ANON) ? -1 : 1;

	/* Match by reference */
	if ((a->it_refp != NULL) && (b->it_refp != NULL))
		return it_cmp(a->it_refp, b->it_refp);

	return 1;
}

int
it_name_cmp(struct itype *a, struct itype *b)
{
	int diff;

	if ((diff = strcmp(it_name(a), it_name(b))) != 0)
		return diff;

	return ((a->it_flags|ITF_MASK) - (b->it_flags|ITF_MASK));
}

int
it_off_cmp(struct itype *a, struct itype *b)
{
	return a->it_off - b->it_off;
}

void
ir_add(struct itype *it, struct itype *tmp)
{
	struct itref *ir;

	SIMPLEQ_FOREACH(ir, &tmp->it_refs, ir_next) {
		if (ir->ir_itp == it)
			return;
	}

	ir = pmalloc(&ir_pool, sizeof(*ir));
	ir->ir_itp = it;
	SIMPLEQ_INSERT_TAIL(&tmp->it_refs, ir, ir_next);
}

void
ir_purge(struct itype *it)
{
	struct itref *ir;

	while ((ir = SIMPLEQ_FIRST(&it->it_refs)) != NULL) {
		SIMPLEQ_REMOVE_HEAD(&it->it_refs, ir_next);
		pfree(&ir_pool, ir);
	}
}

struct imember *
im_new(const char *name, size_t ref, size_t off)
{
	struct imember *im;

	im = pmalloc(&im_pool, sizeof(*im));
	im->im_ref = ref;
	im->im_off = off;
	im->im_refp = NULL;
	if (name == NULL) {
		im->im_flags = IMF_ANON;
	} else {
		size_t n;

		n = strlcpy(im->im_name, name, ITNAME_MAX);
		if (n > ITNAME_MAX)
			warnx("name %s too long %zd > %d", name, n,
			    ITNAME_MAX);
		im->im_flags = 0;
	}

	return im;
}

const char *
im_name(struct imember *im)
{
	if (!(im->im_flags & IMF_ANON))
		return im->im_name;

	return NULL;
}

void
cu_stat(void)
{
#ifndef NOPOOL
	pool_dump();
#endif
}

/*
 * Iterate over all types found in a given CU.  For all non-resolved types
 * use their DWARF relative offset to find the relative type they are pointing
 * to.  The CU offset tree, `cuot', is used to speedup relative type lookup.
 */
void
cu_resolve(struct dwcu *dcu, struct itype_queue *cutq, struct ioff_tree *cuot)
{
	struct itype	*it, *ref, tmp;
	struct imember	*im;
	unsigned int	 toresolve;
	size_t		 off = dcu->dcu_offset;

	TAILQ_FOREACH(it, cutq, it_next) {
		if (!(it->it_flags & (ITF_UNRES|ITF_UNRES_MEMB)))
			continue;

		/* If this type references another one, try to find it. */
		if (it->it_flags & ITF_UNRES) {
			tmp.it_off = it->it_ref + off;
			ref = RB_FIND(ioff_tree, cuot, &tmp);
			if (ref != NULL) {
				it->it_refp = ref;
				ir_add(it, ref);
				it->it_flags &= ~ITF_UNRES;
			}
		}

		/* If this type has members, resolve all of them. */
		toresolve = it->it_nelems;
		if ((it->it_flags & ITF_UNRES_MEMB) && toresolve > 0) {
			TAILQ_FOREACH(im, &it->it_members, im_next) {
				tmp.it_off = im->im_ref + off;
				ref = RB_FIND(ioff_tree, cuot, &tmp);
				if (ref != NULL) {
					im->im_refp = ref;
					ir_add(it, ref);
					toresolve--;
				}
			}
			if (toresolve == 0)
				it->it_flags &= ~ITF_UNRES_MEMB;
		}
#if defined(DEBUG)
		if (it->it_flags & (ITF_UNRES|ITF_UNRES_MEMB)) {
			printf("0x%zx: %s type=%d unresolved 0x%llx",
			    it->it_off, it_name(it), it->it_type, it->it_ref);
			if (toresolve)
				printf(": %d members", toresolve);
			TAILQ_FOREACH(im, &it->it_members, im_next) {
				if (im->im_refp != NULL)
					continue;
				printf("\n%zu: %s", im->im_ref, im_name(im));
			}
			printf("\n");
		}
#endif /* defined(DEBUG) */
	}

	/* We'll reuse the tree for the next CU, so empty it. */
	RB_FOREACH_SAFE(it, ioff_tree, cuot, ref)
		RB_REMOVE(ioff_tree, cuot, it);
}

void
cu_reference(struct dwcu *dcu, struct itype_queue *cutq)
{
	struct itype *it;

	TAILQ_FOREACH(it, cutq, it_next) {
		if (it->it_flags & (ITF_OBJ|ITF_FUNC))
			it_reference(it);
	}
}

/*
 * Merge type representation from a CU with already known types.
 */
void
cu_merge(struct dwcu *dcu, struct itype_queue *cutq)
{
	struct itype *it, *nit, *prev, *first;
	int diff;

	/* First ``it'' that needs a duplicate check. */
	first = TAILQ_FIRST(cutq);
	if (first == NULL)
		return;

	TAILQ_CONCAT(&itypeq, cutq, it_next);

	/*
	 * First pass: merge types
	 */
	for (it = first; it != NULL; it = nit) {
		nit = TAILQ_NEXT(it, it_next);

		/* Move functions & variable to their own list. */
		if (it->it_flags & (ITF_FUNC|ITF_OBJ)) {
			/*
			 * FIXME: allow static variables with the same name
			 * to be of different type.
			 */
			if (RB_FIND(isymb_tree, &isymbt, it) == NULL)
				RB_INSERT(isymb_tree, &isymbt, it);
			continue;
		}

		/* Look if we already have this type. */
		if (it->it_flags & ITF_USED)
			prev = RB_FIND(itype_tree, &itypet[it->it_type], it);
		else
			prev = NULL;

		if (prev != NULL) {
			struct itype *old = it;
			struct itref *ir;
			struct imember *im;

			/* Substitute references */
			while ((ir = SIMPLEQ_FIRST(&old->it_refs)) != NULL) {
				it = ir->ir_itp;

				SIMPLEQ_REMOVE_HEAD(&old->it_refs, ir_next);
				pfree(&ir_pool, ir);

				if (it->it_refp == old)
					it->it_refp = prev;

				TAILQ_FOREACH(im, &it->it_members, im_next) {
					if (im->im_refp == old)
						im->im_refp = prev;
				}
			}

			/* If we first got a forward reference, complete it. */
			if ((prev->it_flags & ITF_FORWARD) &&
			    (old->it_flags & ITF_FORWARD) == 0)
			    	it_merge(prev, old);

			old->it_flags &= ~ITF_USED;
		} else if (it->it_flags & ITF_USED) {
			RB_INSERT(itype_tree, &itypet[it->it_type], it);
		}
	}

	/*
	 * Second pass: update indexes
	 */
	diff = 0;
	for (it = first; it != NULL; it = nit) {
		nit = TAILQ_NEXT(it, it_next);

		if (it->it_flags & (ITF_FUNC|ITF_OBJ))
			continue;

		/* Adjust indexes */
		if (it->it_flags & ITF_USED) {
			it->it_idx -= diff;
			continue;
		}

		/* Remove unused */
		TAILQ_REMOVE(&itypeq, it, it_next);
		it_free(it);
		diff++;
	}

	/* Update global index to match removed entries. */
	it = TAILQ_LAST(&itypeq, itype_queue);
	while (it->it_flags & (ITF_FUNC|ITF_OBJ))
		it = TAILQ_PREV(it, itype_queue, it_next);

	tidx = it->it_idx;
}

/*
 * Parse a CU.
 */
void
cu_parse(struct dwcu *dcu, struct itype_queue *cutq, struct ioff_tree *cuot)
{
	struct itype *it = NULL;
	struct dwdie *die;
	size_t psz = dcu->dcu_psize;
	size_t off = dcu->dcu_offset;

	assert(RB_EMPTY(cuot));

	SIMPLEQ_FOREACH(die, &dcu->dcu_dies, die_next) {
		uint64_t tag = die->die_dab->dab_tag;

		switch (tag) {
		case DW_TAG_array_type:
			it = parse_array(die, dcu->dcu_psize);
			break;
		case DW_TAG_enumeration_type:
			it = parse_enum(die, dcu->dcu_psize);
			break;
		case DW_TAG_pointer_type:
			it = parse_refers(die, psz, CTF_K_POINTER);
			break;
		case DW_TAG_structure_type:
			it = parse_struct(die, psz, CTF_K_STRUCT, off);
			if (it == NULL)
				continue;
			break;
		case DW_TAG_typedef:
			it = parse_refers(die, psz, CTF_K_TYPEDEF);
			break;
		case DW_TAG_union_type:
			it = parse_struct(die, psz, CTF_K_UNION, off);
			if (it == NULL)
				continue;
			break;
		case DW_TAG_base_type:
			it = parse_base(die, psz);
			if (it == NULL)
				continue;
			break;
		case DW_TAG_const_type:
			it = parse_refers(die, psz, CTF_K_CONST);
			break;
		case DW_TAG_volatile_type:
			it = parse_refers(die, psz, CTF_K_VOLATILE);
			break;
		case DW_TAG_restrict_type:
			it = parse_refers(die, psz, CTF_K_RESTRICT);
			break;
		case DW_TAG_subprogram:
			it = parse_function(die, psz);
			if (it == NULL)
				continue;
			break;
		case DW_TAG_subroutine_type:
			it = parse_funcptr(die, psz);
			break;
		/*
		 * Children are assumed to be right after their parent in
		 * the list.  The parent parsing function takes care of
		 * parsing them.
		 */
		 case DW_TAG_member:
			 assert(it->it_type == CTF_K_STRUCT ||
			    it->it_type == CTF_K_UNION ||
			    it->it_type == CTF_K_ENUM);
			continue;
		 case DW_TAG_subrange_type:
			assert(it->it_type == CTF_K_ARRAY);
			continue;
		case DW_TAG_formal_parameter:
			/*
			 * If we skipped the second inline definition,
			 * skip its arguments.
			 */
			if (it == NULL)
				continue;

			/* See comment in subparse_arguments(). */
			if (it->it_type == CTF_K_STRUCT ||
			    it->it_type == CTF_K_UNION ||
			    it->it_type == CTF_K_ENUM ||
			    it->it_type == CTF_K_TYPEDEF)
				continue;

			if (it->it_flags & ITF_OBJ)
				continue;

			assert(it->it_type == CTF_K_FUNCTION);
			continue;
		case DW_TAG_variable:
			it = parse_variable(die, psz);
			/* Unnamed variables are discarded. */
			if (it == NULL)
				continue;
			break;
#if 1
		case DW_TAG_lexical_block:
		case DW_TAG_inlined_subroutine:
			continue;
#endif
		case DW_TAG_compile_unit:
		default:
			DPRINTF("%s\n", dw_tag2name(tag));
			continue;
		}

		TAILQ_INSERT_TAIL(cutq, it, it_next);
		RB_INSERT(ioff_tree, cuot, it);
	}
}

struct itype *
parse_base(struct dwdie *die, size_t psz)
{
	struct itype *it;
	struct dwaval *dav;
	uint16_t encoding, enc = 0, bits = 0;
	int type;

	SIMPLEQ_FOREACH(dav, &die->die_avals, dav_next) {
		switch (dav->dav_dat->dat_attr) {
		case DW_AT_encoding:
			enc = dav2val(dav, psz);
			break;
		case DW_AT_byte_size:
			bits = 8 * dav2val(dav, psz);
			break;
		default:
			DPRINTF("%s\n", dw_at2name(dav->dav_dat->dat_attr));
			break;
		}
	}

	switch (enc) {
	case DW_ATE_unsigned:
	case DW_ATE_address:
		encoding = 0;
		type = CTF_K_INTEGER;
		break;
	case DW_ATE_unsigned_char:
		encoding = CTF_INT_CHAR;
		type = CTF_K_INTEGER;
		break;
	case DW_ATE_signed:
		encoding = CTF_INT_SIGNED;
		type = CTF_K_INTEGER;
		break;
	case DW_ATE_signed_char:
		encoding = CTF_INT_SIGNED | CTF_INT_CHAR;
		type = CTF_K_INTEGER;
		break;
	case DW_ATE_boolean:
		encoding = CTF_INT_SIGNED | CTF_INT_BOOL;
		type = CTF_K_INTEGER;
		break;
	case DW_ATE_float:
		if (bits < psz)
			encoding = CTF_FP_SINGLE;
		else if (bits == psz)
			encoding = CTF_FP_DOUBLE;
		else
			encoding = CTF_FP_LDOUBLE;
		type = CTF_K_FLOAT;
		break;
	case DW_ATE_complex_float:
		if (bits < psz)
			encoding = CTF_FP_CPLX;
		else if (bits == psz)
			encoding = CTF_FP_DCPLX;
		else
			encoding = CTF_FP_LDCPLX;
		type = CTF_K_FLOAT;
		break;
	case DW_ATE_imaginary_float:
		if (bits < psz)
			encoding = CTF_FP_IMAGRY;
		else if (bits == psz)
			encoding = CTF_FP_DIMAGRY;
		else
			encoding = CTF_FP_LDIMAGRY;
		type = CTF_K_FLOAT;
		break;
	default:
		DPRINTF("unknown encoding: %d\n", enc);
		return (NULL);
	}

	it = it_new(++tidx, die->die_offset, enc2name(enc), bits,
	    encoding, 0, type, 0);

	return it;
}

struct itype *
parse_refers(struct dwdie *die, size_t psz, int type)
{
	struct itype *it;
	struct dwaval *dav;
	const char *name = NULL;
	size_t ref = 0, size = 0;

	SIMPLEQ_FOREACH(dav, &die->die_avals, dav_next) {
		switch (dav->dav_dat->dat_attr) {
		case DW_AT_name:
			name = dav2str(dav);
			break;
		case DW_AT_type:
			ref = dav2val(dav, psz);
			break;
		case DW_AT_byte_size:
			size = dav2val(dav, psz);
			assert(size < UINT_MAX);
			break;
		default:
			DPRINTF("%s\n", dw_at2name(dav->dav_dat->dat_attr));
			break;
		}
	}

	it = it_new(++tidx, die->die_offset, name, size, 0, ref, type,
	    ITF_UNRES);

	if (it->it_ref == 0 && (it->it_size == sizeof(void *) ||
	    type == CTF_K_CONST || type == CTF_K_VOLATILE ||
	    type == CTF_K_POINTER)) {
		/* Work around GCC/clang not emiting a type for void */
		it->it_flags &= ~ITF_UNRES;
		it->it_ref = VOID_OFFSET;
		it->it_refp = void_it;
	}

	return it;
}

struct itype *
parse_array(struct dwdie *die, size_t psz)
{
	struct itype *it;
	struct dwaval *dav;
	const char *name = NULL;
	size_t ref = 0;

	SIMPLEQ_FOREACH(dav, &die->die_avals, dav_next) {
		switch (dav->dav_dat->dat_attr) {
		case DW_AT_name:
			name = dav2str(dav);
			break;
		case DW_AT_type:
			ref = dav2val(dav, psz);
			break;
		default:
			DPRINTF("%s\n", dw_at2name(dav->dav_dat->dat_attr));
			break;
		}
	}

	it = it_new(++tidx, die->die_offset, name, 0, 0, ref, CTF_K_ARRAY,
	    ITF_UNRES);

	subparse_subrange(die, psz, it);

	return it;
}

struct itype *
parse_enum(struct dwdie *die, size_t psz)
{
	struct itype *it;
	struct dwaval *dav;
	const char *name = NULL;
	size_t size = 0;

	SIMPLEQ_FOREACH(dav, &die->die_avals, dav_next) {
		switch (dav->dav_dat->dat_attr) {
		case DW_AT_byte_size:
			size = dav2val(dav, psz);
			assert(size < UINT_MAX);
			break;
		case DW_AT_name:
			name = dav2str(dav);
			break;
		default:
			DPRINTF("%s\n", dw_at2name(dav->dav_dat->dat_attr));
			break;
		}
	}

	it = it_new(++tidx, die->die_offset, name, size, 0, 0, CTF_K_ENUM, 0);

	subparse_enumerator(die, psz, it);

	return it;
}

void
subparse_subrange(struct dwdie *die, size_t psz, struct itype *it)
{
	struct dwaval *dav;

	assert(it->it_type == CTF_K_ARRAY);

	if (die->die_dab->dab_children == DW_CHILDREN_no)
		return;

	/*
	 * This loop assumes that the children of a DIE are just
	 * after it on the list.
	 */
	while ((die = SIMPLEQ_NEXT(die, die_next)) != NULL) {
		uint64_t tag = die->die_dab->dab_tag;
		size_t nelems = 0;

		if (tag != DW_TAG_subrange_type)
			break;

		SIMPLEQ_FOREACH(dav, &die->die_avals, dav_next) {
			switch (dav->dav_dat->dat_attr) {
			case DW_AT_count:
				nelems = dav2val(dav, psz);
				break;
			case DW_AT_upper_bound:
				nelems = dav2val(dav, psz) + 1;
				break;
			default:
				DPRINTF("%s\n",
				    dw_at2name(dav->dav_dat->dat_attr));
				break;
			}
		}

		assert(nelems < UINT_MAX);
		it->it_nelems = nelems;
	}
}

void
subparse_enumerator(struct dwdie *die, size_t psz, struct itype *it)
{
	struct imember *im;
	struct dwaval *dav;

	assert(it->it_type == CTF_K_ENUM);

	if (die->die_dab->dab_children == DW_CHILDREN_no)
		return;

	/*
	 * This loop assumes that the children of a DIE are just
	 * after it on the list.
	 */
	while ((die = SIMPLEQ_NEXT(die, die_next)) != NULL) {
		uint64_t tag = die->die_dab->dab_tag;
		size_t val = 0;
		const char *name = NULL;

		if (tag != DW_TAG_enumerator)
			break;

		SIMPLEQ_FOREACH(dav, &die->die_avals, dav_next) {
			switch (dav->dav_dat->dat_attr) {
			case DW_AT_name:
				name = dav2str(dav);
				break;
			case DW_AT_const_value:
				val = dav2val(dav, psz);
				break;
			default:
				DPRINTF("%s\n",
				    dw_at2name(dav->dav_dat->dat_attr));
				break;
			}
		}

		if (name == NULL) {
			warnx("%s with anon member", it_name(it));
			continue;
		}

		im = im_new(name, val, 0);
		assert(it->it_nelems < UINT_MAX);
		it->it_nelems++;
		TAILQ_INSERT_TAIL(&it->it_members, im, im_next);
	}
}

struct itype *
parse_struct(struct dwdie *die, size_t psz, int type, size_t off)
{
	struct itype *it = NULL;
	struct dwaval *dav;
	const char *name = NULL;
	unsigned int flags = 0;
	size_t size = 0;
	int forward = 0;

	SIMPLEQ_FOREACH(dav, &die->die_avals, dav_next) {
		switch (dav->dav_dat->dat_attr) {
		case DW_AT_declaration:
			forward = dav2val(dav, psz);
			break;
		case DW_AT_byte_size:
			size = dav2val(dav, psz);
			assert(size < UINT_MAX);
			break;
		case DW_AT_name:
			name = dav2str(dav);
			break;
		default:
			DPRINTF("%s\n", dw_at2name(dav->dav_dat->dat_attr));
			break;
		}
	}


	if (forward)
		flags = ITF_FORWARD;
	it = it_new(++tidx, die->die_offset, name, size, 0, 0, type, flags);
	subparse_member(die, psz, it, off);

	return it;
}

void
subparse_member(struct dwdie *die, size_t psz, struct itype *it, size_t offset)
{
	struct imember *im;
	struct dwaval *dav;
	const char *name;
	size_t off = 0, ref = 0, bits = 0;
	uint8_t lvl = die->die_lvl;

	assert(it->it_type == CTF_K_STRUCT || it->it_type == CTF_K_UNION);

	if (die->die_dab->dab_children == DW_CHILDREN_no)
		return;

	/*
	 * This loop assumes that the children of a DIE are just
	 * after it on the list.
	 */
	while ((die = SIMPLEQ_NEXT(die, die_next)) != NULL) {
		int64_t tag = die->die_dab->dab_tag;

		name = NULL;
		if (die->die_lvl <= lvl)
			break;

		/* Skip members of members */
		if (die->die_lvl > lvl + 1)
			continue;
		/*
		 * Nested declaration.
		 *
		 * This matches the case where a ``struct'', ``union'',
		 * ``enum'' or ``typedef'' is first declared "inside" a
		 * union or struct declaration.
		 */
		if (tag == DW_TAG_structure_type || tag == DW_TAG_union_type ||
		    tag == DW_TAG_enumeration_type || tag == DW_TAG_typedef)
			continue;

		it->it_flags |= ITF_UNRES_MEMB;

		SIMPLEQ_FOREACH(dav, &die->die_avals, dav_next) {
			switch (dav->dav_dat->dat_attr) {
			case DW_AT_name:
				name = dav2str(dav);
				break;
			case DW_AT_type:
				ref = dav2val(dav, psz);
				break;
			case DW_AT_data_member_location:
				off = 8 * dav2val(dav, psz);
				break;
			case DW_AT_bit_size:
				bits = dav2val(dav, psz);
				assert(bits < USHRT_MAX);
				break;
			default:
				DPRINTF("%s\n",
				    dw_at2name(dav->dav_dat->dat_attr));
				break;
			}
		}

		/*
		 * When a structure is declared inside an union, we
		 * have to generate a reference to make the resolver
		 * happy.
		 */
		if ((ref == 0) && (tag == DW_TAG_structure_type))
			ref = die->die_offset - offset;

		im = im_new(name, ref, off);
		assert(it->it_nelems < UINT_MAX);
		it->it_nelems++;
		TAILQ_INSERT_TAIL(&it->it_members, im, im_next);
	}
}


void
subparse_arguments(struct dwdie *die, size_t psz, struct itype *it)
{
	struct imember *im;
	struct dwaval *dav;
	size_t ref = 0;

	assert(it->it_type == CTF_K_FUNCTION);

	if (die->die_dab->dab_children == DW_CHILDREN_no)
		return;

	/*
	 * This loop assumes that the children of a DIE are after it
	 * on the list.
	 */
	while ((die = SIMPLEQ_NEXT(die, die_next)) != NULL) {
		uint64_t tag = die->die_dab->dab_tag;

		if (tag == DW_TAG_unspecified_parameters) {
			/* TODO */
			continue;
		}

		/*
		 * Nested declaration.
		 *
		 * This matches the case where a ``struct'', ``union'',
		 * ``enum'', ``typedef'' or ``static'' variable is first
		 * declared inside a function declaration.
		 */
		switch (tag) {
		case DW_TAG_structure_type:
		case DW_TAG_union_type:
		case DW_TAG_enumeration_type:
		case DW_TAG_typedef:
		case DW_TAG_variable:
			continue;
		default:
			break;
		}

		if (tag != DW_TAG_formal_parameter)
			break;

		it->it_flags |= ITF_UNRES_MEMB;

		SIMPLEQ_FOREACH(dav, &die->die_avals, dav_next) {
			switch (dav->dav_dat->dat_attr) {
			case DW_AT_type:
				ref = dav2val(dav, psz);
				break;
			default:
				DPRINTF("%s\n",
				    dw_at2name(dav->dav_dat->dat_attr));
				break;
			}
		}

		im = im_new(NULL, ref, 0);
		assert(it->it_nelems < UINT_MAX);
		it->it_nelems++;
		TAILQ_INSERT_TAIL(&it->it_members, im, im_next);
	}
}

struct itype *
parse_function(struct dwdie *die, size_t psz)
{
	struct itype *it;
	struct dwaval *dav;
	const char *name = NULL;
	size_t ref = 0;

	SIMPLEQ_FOREACH(dav, &die->die_avals, dav_next) {
		switch (dav->dav_dat->dat_attr) {
		case DW_AT_name:
			name = dav2str(dav);
			break;
		case DW_AT_type:
			ref = dav2val(dav, psz);
			break;
		case DW_AT_abstract_origin:
			/*
			 * Skip second empty definition for inline
			 * functions.
			 */
			return NULL;
		default:
			DPRINTF("%s\n", dw_at2name(dav->dav_dat->dat_attr));
			break;
		}
	}

	/*
	 * Work around for clang 4.0 generating DW_TAG_subprogram without
	 * any attribute.
	 */
	if (name == NULL)
		return NULL;

	it = it_new(++fidx, die->die_offset, name, 0, 0, ref, CTF_K_FUNCTION,
	    ITF_UNRES|ITF_FUNC);

	subparse_arguments(die, psz, it);

	if (it->it_ref == 0) {
		/* Work around GCC not emiting a type for void */
		it->it_flags &= ~ITF_UNRES;
		it->it_ref = VOID_OFFSET;
		it->it_refp = void_it;
	}

	return it;
}

struct itype *
parse_funcptr(struct dwdie *die, size_t psz)
{
	struct itype *it;
	struct dwaval *dav;
	const char *name = NULL;
	size_t ref = 0;

	SIMPLEQ_FOREACH(dav, &die->die_avals, dav_next) {
		switch (dav->dav_dat->dat_attr) {
		case DW_AT_name:
			name = dav2str(dav);
			break;
		case DW_AT_type:
			ref = dav2val(dav, psz);
			break;
		default:
			DPRINTF("%s\n", dw_at2name(dav->dav_dat->dat_attr));
			break;
		}
	}

	it = it_new(++tidx, die->die_offset, name, 0, 0, ref, CTF_K_FUNCTION,
	    ITF_UNRES);

	subparse_arguments(die, psz, it);

	if (it->it_ref == 0) {
		/* Work around GCC not emiting a type for void */
		it->it_flags &= ~ITF_UNRES;
		it->it_ref = VOID_OFFSET;
		it->it_refp = void_it;
	}

	return it;
}

struct itype *
parse_variable(struct dwdie *die, size_t psz)
{
	struct itype *it = NULL;
	struct dwaval *dav;
	const char *name = NULL;
	size_t ref = 0;
	int forward = 0, global = 0;

	SIMPLEQ_FOREACH(dav, &die->die_avals, dav_next) {
		switch (dav->dav_dat->dat_attr) {
		case DW_AT_declaration:
			forward = dav2val(dav, psz);
			break;
		case DW_AT_name:
			name = dav2str(dav);
			break;
		case DW_AT_type:
			ref = dav2val(dav, psz);
			break;
		case DW_AT_location:
			switch (dav->dav_dat->dat_form) {
			case DW_FORM_block:
			case DW_FORM_block1:
			case DW_FORM_block2:
			case DW_FORM_block4:
				global = 1;
				break;
			default:
				break;
			}
			break;
		default:
			DPRINTF("%s\n", dw_at2name(dav->dav_dat->dat_attr));
			break;
		}
	}


	if (global && !forward && name != NULL) {
		it = it_new(++oidx, die->die_offset, name, 0, 0, ref, 0,
		    ITF_UNRES|ITF_OBJ);
	}

	return it;
}

size_t
dav2val(struct dwaval *dav, size_t psz)
{
	uint64_t val = (uint64_t)-1;

	switch (dav->dav_dat->dat_form) {
	case DW_FORM_addr:
	case DW_FORM_ref_addr:
		if (psz == sizeof(uint32_t))
			val = dav->dav_u32;
		else
			val = dav->dav_u64;
		break;
	case DW_FORM_block1:
	case DW_FORM_block2:
	case DW_FORM_block4:
	case DW_FORM_block:
		dw_loc_parse(&dav->dav_buf, NULL, &val, NULL);
		break;
	case DW_FORM_flag:
	case DW_FORM_data1:
	case DW_FORM_ref1:
		val = dav->dav_u8;
		break;
	case DW_FORM_data2:
	case DW_FORM_ref2:
		val = dav->dav_u16;
		break;
	case DW_FORM_data4:
	case DW_FORM_ref4:
		val = dav->dav_u32;
		break;
	case DW_FORM_sdata:
	case DW_FORM_data8:
	case DW_FORM_ref8:
		val = dav->dav_u64;
		break;
	case DW_FORM_strp:
		val = dav->dav_u32;
		break;
	case DW_FORM_flag_present:
		val = 1;
		break;
	default:
		break;
	}

	return val;
}

const char *
dav2str(struct dwaval *dav)
{
	const char *str = NULL;
	extern const char *dstrbuf;
	extern size_t dstrlen;

	switch (dav->dav_dat->dat_form) {
	case DW_FORM_string:
		str = dav->dav_str;
		break;
	case DW_FORM_strp:
		if (dav->dav_u32 >= dstrlen)
			str = NULL;
		else
			str = dstrbuf + dav->dav_u32;
		break;
	default:
		break;
	}

	return str;
}

const char *
enc2name(unsigned short enc)
{
	static const char *enc_name[] = { "address", "boolean", "complex float",
	    "float", "signed", "char", "unsigned", "unsigned char",
	    "imaginary float", "packed decimal", "numeric string", "edited",
	    "signed fixed", "unsigned fixed", "decimal float" };

	if (enc > 0 && enc <= nitems(enc_name))
		return enc_name[enc - 1];

	return "invalid";
}
