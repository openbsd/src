/*	$OpenBSD: chash.h,v 1.3 2025/10/30 14:54:55 claudio Exp $	*/
/*
 * Copyright (c) 2025 Claudio Jeker <claudio@openbsd.org>
 * Copyright (c) 2016 David Gwynne <dlg@openbsd.org>
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

#ifndef __CHASH_H__
#define __CHASH_H__

#include <stddef.h>
#include <stdint.h>

/* for -portable since __unused is differently used in some system headers */
#ifndef	__unused
#define	__unused	__attribute__((__unused__))
#endif

struct ch_type {
	int		(*t_equal)(const void *, const void *);
	uint64_t	(*t_hash)(const void *);
};

struct ch_group;
struct ch_meta;

struct ch_table {
	struct ch_group		**ch_tables;
	struct ch_meta		**ch_metas;
	uint32_t		  ch_level;
	uint32_t		  ch_num_elm;
};


#define CH_HEAD(_name, _type)						\
struct _name {								\
	struct ch_table		ch_table;				\
}

struct ch_iter {
	uint64_t		ci_ext_idx;
	uint32_t		ci_set_idx;
	uint32_t		ci_grp_idx;
};

int   _ch_init(const struct ch_type *, struct ch_table *);
void  _ch_destroy(const struct ch_type *, struct ch_table *);
void *_ch_insert(const struct ch_type *, struct ch_table *, uint64_t, void *);
void *_ch_remove(const struct ch_type *, struct ch_table *, uint64_t,
	    const void *);
void *_ch_find(const struct ch_type *, struct ch_table *, uint64_t,
	    const void *);
void *_ch_locate(const struct ch_type *, struct ch_table *, uint64_t,
	    int (*)(const void *, void *), void *);
void *_ch_first(const struct ch_type *, struct ch_table *, struct ch_iter *);
void *_ch_next(const struct ch_type *, struct ch_table *, struct ch_iter *);

#define CH_INS_FAILED	((void *)-1)

#define CH_INITIALIZER(_head)  { 0 }

#define CH_PROTOTYPE(_name, _type, _hash)				\
extern const struct ch_type *const _name##_CH_TYPE;			\
									\
__unused static inline int						\
_name##_CH_INIT(struct _name *head)					\
{									\
	return _ch_init(_name##_CH_TYPE, &head->ch_table);		\
}									\
									\
__unused static inline void						\
_name##_CH_DESTROY(struct _name *head)					\
{									\
	_ch_destroy(_name##_CH_TYPE, &head->ch_table);			\
}									\
									\
__unused static inline int						\
_name##_CH_INSERT(struct _name *head, struct _type *elm, struct _type **prev) \
{									\
	struct _type *p;						\
	uint64_t h;							\
	h = _hash(elm);							\
	p = _ch_insert(_name##_CH_TYPE, &head->ch_table, h, elm);	\
	if (p == CH_INS_FAILED)						\
		return -1;						\
	if (prev != NULL)						\
		*prev = p;						\
	return (p == NULL);						\
}									\
									\
__unused static inline struct _type *					\
_name##_CH_REMOVE(struct _name *head, struct _type *elm)		\
{									\
	uint64_t h;							\
	h = _hash(elm);							\
	return _ch_remove(_name##_CH_TYPE, &head->ch_table, h, elm);	\
}									\
									\
__unused static inline struct _type *					\
_name##_CH_FIND(struct _name *head, const struct _type *key)		\
{									\
	uint64_t h;							\
	h = _hash(key);							\
	return _ch_find(_name##_CH_TYPE, &head->ch_table, h, key);	\
}									\
									\
__unused static inline struct _type *					\
_name##_CH_LOCATE(struct _name *head, uint64_t hash, 			\
    int (*cmp)(const void *, void *), void *arg)			\
{									\
	return _ch_locate(_name##_CH_TYPE, &head->ch_table, hash,	\
	    cmp, arg);							\
}									\
									\
__unused static inline struct _type *					\
_name##_CH_FIRST(struct _name *head, struct ch_iter *iter)		\
{									\
	return _ch_first(_name##_CH_TYPE, &head->ch_table, iter);	\
}									\
									\
__unused static inline struct _type *					\
_name##_CH_NEXT(struct _name *head, struct ch_iter *iter)		\
{									\
	return _ch_next(_name##_CH_TYPE, &head->ch_table, iter);	\
}


#define CH_GENERATE(_name, _type, _eq, _hash)				\
static int                                                              \
_name##_CH_EQUAL(const void *lptr, const void *rptr)			\
{									\
	const struct _type *l = lptr, *r = rptr;			\
	return _eq(l, r);						\
}									\
									\
static uint64_t								\
_name##_CH_HASH(const void *ptr)					\
{									\
	const struct _type *obj = ptr;					\
	return _hash(obj);						\
}									\
									\
static const struct ch_type _name##_CH_INFO = {				\
	.t_equal = _name##_CH_EQUAL,					\
	.t_hash = _name##_CH_HASH,					\
};									\
const struct ch_type *const _name##_CH_TYPE = &_name##_CH_INFO

#define CH_INIT(_name, _head)		_name##_CH_INIT(_head)
#define CH_INSERT(_name, _head, _elm, _prev)				\
					_name##_CH_INSERT(_head, _elm, _prev)
#define CH_REMOVE(_name, _head, _elm)	_name##_CH_REMOVE(_head, _elm)
#define CH_FIND(_name, _head, _key)	_name##_CH_FIND(_head, _key)
#define CH_LOCATE(_name, _head, _h, _cmp, _a)			\
					_name##_CH_LOCATE(_head, _h, _cmp, _a)
#define CH_FIRST(_name, _head, _iter)	_name##_CH_FIRST(_head, _iter)
#define CH_NEXT(_name, _head, _iter)	_name##_CH_NEXT(_head, _iter)

#define CH_FOREACH(_e, _name, _head, _iter)				\
	for ((_e) = CH_FIRST(_name, (_head), (_iter));			\
	     (_e) != NULL;						\
	     (_e) = CH_NEXT(_name, (_head), (_iter)))

#endif
