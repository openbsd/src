/*	$OpenBSD: itype.h,v 1.5 2019/11/11 19:10:35 mpi Exp $ */

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

#ifndef _ITTYPE_H_
#define _ITTYPE_H_

#define ITNAME_MAX	128

struct imember;
struct itref;

/*
 * Internal type representation.
 *
 * Some bits of DWARF that we want to keep around to resolve types and
 * variables to their intrinsics.
 */
struct itype {
	TAILQ_ENTRY(itype)	 it_next;   /* itype: global queue of types */
	TAILQ_ENTRY(itype)	 it_symb;   /* itype: global queue of symbol */
	RB_ENTRY(itype)		 it_node;   /* itype: per-type tree of types */

	SIMPLEQ_HEAD(, itref)	 it_refs;   /* itype: backpointing refs */

	TAILQ_HEAD(, imember)	 it_members;/* itype: members of struct/union */

	size_t			 it_off;    /* DWARF: matching .abbrev offset */
	uint64_t		 it_ref;    /* DWARF: CU offset of ref. type */

	struct itype		*it_refp;   /* itype: resolved type */

	char			 it_name[ITNAME_MAX];/* CTF: type name */
	uint32_t		 it_size;   /* CTF: size in byte or bits */
	uint32_t		 it_nelems; /* CTF: # of members or arguments */
	uint16_t		 it_enc;    /* CTF: base type encoding */
	uint16_t		 it_idx;    /* CTF: generated type ID */
	uint16_t		 it_type;   /* CTF: type */
	uint8_t			 __pad[2];

	unsigned int		 it_flags;  /* itype: parser flags */
#define	ITF_UNRES		 0x01	    /* needs to be resolved */
#define	ITF_UNRES_MEMB		 0x02	    /* members need to be resolved */
#define	ITF_FUNC		 0x04	    /* is a function */
#define	ITF_OBJ			 0x08	    /* is an object */
#define	ITF_FORWARD		 0x10	    /* is a forward declaration */
#define	ITF_INSERTED		 0x20	    /* already found/inserted */
#define	ITF_USED		 0x40	    /* referenced in the current CU */
#define	ITF_ANON		 0x80	    /* type without name */
#define	ITF_MASK		(ITF_INSERTED|ITF_USED)
};

/*
 * Member for types with a variable length (struct, array, etc).
 */
struct imember {
	TAILQ_ENTRY(imember)	 im_next;
	char			 im_name[ITNAME_MAX]; /* struct field name */
	size_t			 im_ref;    /* CU offset of the field type */
	size_t			 im_off;    /* field offset in struct/union */
	struct itype		*im_refp;   /* resolved CTF type */
	unsigned int		 im_flags;  /* parser flags */
#define	IMF_ANON		 0x01	    /* member without name */
};

/*
 * Used to build a list of backpointing references to speed up
 * merging duplicated types.
 */
struct itref {
	SIMPLEQ_ENTRY(itref)	 ir_next;
	struct itype		*ir_itp;
};

TAILQ_HEAD(itype_queue, itype);
RB_HEAD(isymb_tree, itype);

/* lists of types, functions & data objects */
extern struct itype_queue itypeq, ifuncq, iobjq;
extern struct isymb_tree isymbt;	    /* tree of symbols */
extern uint16_t tidx;		    	    /* type index */
extern uint16_t long_tidx;		    /* type ID for "long" */

RB_PROTOTYPE(isymb_tree, itype, it_node, it_name_cmp);

struct itype *it_dup(struct itype *);
const char *it_name(struct itype *);
const char *im_name(struct imember *);

#endif /*_ITTYPE_H_ */
