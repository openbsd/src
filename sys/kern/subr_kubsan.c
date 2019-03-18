/*	$OpenBSD: subr_kubsan.c,v 1.1 2019/03/18 17:30:08 anton Exp $	*/

/*
 * Copyright (c) 2019 Anton Lindqvist <anton@openbsd.org>
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

#include <sys/param.h>
#include <sys/atomic.h>
#include <sys/syslimits.h>
#include <sys/systm.h>

#define NUMBER_BUFSIZ		32
#define LOCATION_BUFSIZ		(PATH_MAX + 32)	/* filename:line:column */
#define LOCATION_REPORTED	(1U << 30)

#define NBITS(typ)	(1 << ((typ)->t_info >> 1))
#define SIGNED(typ)	((typ)->t_info & 1)

struct type_descriptor {
	uint16_t t_kind;
	uint16_t t_info;
	char t_name[1];	/* type name as variable length array */
};

struct source_location {
	const char *sl_filename;
	uint32_t sl_line;
	uint32_t sl_column;
};

struct invalid_value_data {
	struct source_location d_src;
	struct type_descriptor *d_type;
};

struct out_of_bounds_data {
	struct source_location d_src;
	struct type_descriptor *d_atype;	/* array type */
	struct type_descriptor *d_itype;	/* index type */
};

struct overflow_data {
	struct source_location d_src;
	struct type_descriptor *d_type;
};

struct pointer_overflow_data {
	struct source_location d_src;
};

struct shift_out_of_bounds_data {
	struct source_location d_src;
	struct type_descriptor *d_ltype;
	struct type_descriptor *d_rtype;
};

struct unreachable_data {
	struct source_location d_src;
};

struct type_mismatch {
	struct source_location d_src;
	struct type_descriptor *d_type;
	uint8_t d_align;	/* log2 alignment */
	uint8_t d_kind;
};

void	kubsan_handle_load_invalid_value(struct invalid_value_data *,
	    unsigned long);
void	kubsan_handle_negate_overflow(struct overflow_data *, unsigned long);
void	kubsan_handle_out_of_bounds(struct out_of_bounds_data *, unsigned long);
void	kubsan_handle_overflow(struct overflow_data *, unsigned long,
	    unsigned long, char);
void	kubsan_handle_pointer_overflow(struct pointer_overflow_data *,
	    unsigned long, unsigned long);
void	kubsan_handle_type_mismatch(struct type_mismatch *, unsigned long);
void    kubsan_handle_shift_out_of_bounds(struct shift_out_of_bounds_data *,
	    unsigned long, unsigned long);
void	kubsan_handle_ureachable(struct unreachable_data *);

int64_t		 kubsan_deserialize_int(struct type_descriptor *,
		    unsigned long);
uint64_t	 kubsan_deserialize_uint(struct type_descriptor *,
		    unsigned long);
void		 kubsan_format_int(struct type_descriptor *, unsigned long,
		    char *, size_t);
void		 kubsan_format_location(struct source_location *, char *,
		    size_t);
int		 kubsan_is_reported(struct source_location *);
const char	*kubsan_kind(uint8_t);
void		 kubsan_report(const char *, ...)
		    __attribute__((__format__(__kprintf__, 1, 2)));

static int	is_negative(struct type_descriptor *, unsigned long);
static int	is_shift_exponent_too_large(struct type_descriptor *,
		    unsigned long);

#ifdef KUBSAN_WATCH
int kubsan_watch = 2;
#else
int kubsan_watch = 1;
#endif

/*
 * Compiling the kernel with `-fsanitize=undefined' will cause the following
 * functions to be called when a sanitizer detects undefined behavior.
 * Some sanitizers are omitted since they are only applicable to C++.
 *
 * Every __ubsan_*() sanitizer function also has a corresponding
 * __ubsan_*_abort() function as part of the ABI provided by Clang.
 * But, since the kernel never is compiled with `fno-sanitize-recover' for
 * obvious reasons, they are also omitted.
 */

void
__ubsan_handle_add_overflow(struct overflow_data *data,
    unsigned long lhs, unsigned long rhs)
{
	kubsan_handle_overflow(data, lhs, rhs, '+');
}

void
__ubsan_handle_builtin_unreachable(struct unreachable_data *data)
{
	kubsan_handle_ureachable(data);
}

void
__ubsan_handle_divrem_overflow(struct overflow_data *data,
    unsigned long lhs, unsigned long rhs)
{
	kubsan_handle_overflow(data, lhs, rhs, '/');
}

void
__ubsan_handle_load_invalid_value(struct invalid_value_data *data,
    unsigned long val)
{
	kubsan_handle_load_invalid_value(data, val);
}

void
__ubsan_handle_mul_overflow(struct overflow_data *data,
    unsigned long lhs, unsigned long rhs)
{
	kubsan_handle_overflow(data, lhs, rhs, '*');
}

void
__ubsan_handle_negate_overflow(struct overflow_data *data, unsigned long val)
{
	kubsan_handle_negate_overflow(data, val);
}

void
__ubsan_handle_out_of_bounds(struct out_of_bounds_data *data,
    unsigned long idx)
{
	kubsan_handle_out_of_bounds(data, idx);
}

void
__ubsan_handle_pointer_overflow(struct pointer_overflow_data *data,
    unsigned long base, unsigned long res)
{
	kubsan_handle_pointer_overflow(data, base, res);
}

void
__ubsan_handle_shift_out_of_bounds(struct shift_out_of_bounds_data *data,
    unsigned long lhs, unsigned long rhs)
{
	kubsan_handle_shift_out_of_bounds(data, lhs, rhs);
}

void
__ubsan_handle_sub_overflow(struct overflow_data *data,
    unsigned long lhs, unsigned long rhs)
{
	kubsan_handle_overflow(data, lhs, rhs, '-');
}

void
__ubsan_handle_type_mismatch_v1(struct type_mismatch *data,
    unsigned long ptr)
{
	kubsan_handle_type_mismatch(data, ptr);
}

void
kubsan_handle_load_invalid_value(struct invalid_value_data *data,
    unsigned long val)
{
	char bloc[LOCATION_BUFSIZ];
	char bval[NUMBER_BUFSIZ];

	if (kubsan_is_reported(&data->d_src))
		return;

	kubsan_format_location(&data->d_src, bloc, sizeof(bloc));
	kubsan_format_int(data->d_type, val, bval, sizeof(bval));
	kubsan_report("kubsan: %s: load invalid value: load of value %s is "
	    "not a valid value for type %s\n",
	    bloc, bval, data->d_type->t_name);
}

void
kubsan_handle_negate_overflow(struct overflow_data *data, unsigned long val)
{
	char bloc[LOCATION_BUFSIZ];
	char bval[NUMBER_BUFSIZ];

	if (kubsan_is_reported(&data->d_src))
		return;

	kubsan_format_location(&data->d_src, bloc, sizeof(bloc));
	kubsan_format_int(data->d_type, val, bval, sizeof(bval));
	kubsan_report("kubsan: %s: negate overflow: negation of %s cannot be "
	    "represented in type %s\n",
	    bloc, bval, data->d_type->t_name);
}

void
kubsan_handle_out_of_bounds(struct out_of_bounds_data *data,
    unsigned long idx)
{
	char bloc[LOCATION_BUFSIZ];
	char bidx[NUMBER_BUFSIZ];

	if (kubsan_is_reported(&data->d_src))
		return;

	kubsan_format_location(&data->d_src, bloc, sizeof(bloc));
	kubsan_format_int(data->d_itype, idx, bidx, sizeof(bidx));
	kubsan_report("kubsan: %s: out of bounds: index %s is out of range "
	    "for type %s\n",
	    bloc, bidx, data->d_atype->t_name);
}

void
kubsan_handle_overflow(struct overflow_data *data, unsigned long rhs,
    unsigned long lhs, char op)
{
	char bloc[LOCATION_BUFSIZ];
	char blhs[NUMBER_BUFSIZ];
	char brhs[NUMBER_BUFSIZ];

	if (kubsan_is_reported(&data->d_src))
		return;

	kubsan_format_location(&data->d_src, bloc, sizeof(bloc));
	kubsan_format_int(data->d_type, lhs, blhs, sizeof(blhs));
	kubsan_format_int(data->d_type, rhs, brhs, sizeof(brhs));
	kubsan_report("kubsan: %s: %s integer overflow: %s %c %s cannot "
	    "be represented in type %s\n",
	    bloc, SIGNED(data->d_type) ? "signed" : "unsigned",
	    blhs, op, brhs, data->d_type->t_name);
}

void
kubsan_handle_pointer_overflow(struct pointer_overflow_data *data,
    unsigned long base, unsigned long res)
{
	char bloc[LOCATION_BUFSIZ];

	if (kubsan_is_reported(&data->d_src))
		return;

	kubsan_format_location(&data->d_src, bloc, sizeof(bloc));
	kubsan_report("kubsan: %s: pointer overflow: pointer expression with"
	    " base %#lx overflowed to %#lx\n",
	    bloc, base, res);
}

void
kubsan_handle_type_mismatch(struct type_mismatch *data, unsigned long ptr)
{
	char bloc[LOCATION_BUFSIZ];
	unsigned long align = 1UL << data->d_align;

	if (kubsan_is_reported(&data->d_src))
		return;

	kubsan_format_location(&data->d_src, bloc, sizeof(bloc));
	if (ptr == 0UL)
		kubsan_report("kubsan: %s: type mismatch: %s null pointer of "
		    "type %s\n",
		    bloc, kubsan_kind(data->d_kind), data->d_type->t_name);
	else if (ptr & (align - 1))
		kubsan_report("kubsan: %s: type mismatch: %s misaligned address "
		    "%p for type %s which requires %lu byte alignment\n",
		    bloc, kubsan_kind(data->d_kind), (void *)ptr,
		    data->d_type->t_name, align);
	else
		kubsan_report("kubsan: %s: type mismatch: %s address %p with "
		    "insufficient space for an object of type %s\n",
		    bloc, kubsan_kind(data->d_kind), (void *)ptr,
		    data->d_type->t_name);
}

void
kubsan_handle_shift_out_of_bounds(struct shift_out_of_bounds_data *data,
    unsigned long lhs, unsigned long rhs)
{
	char bloc[LOCATION_BUFSIZ];
	char blhs[NUMBER_BUFSIZ];
	char brhs[NUMBER_BUFSIZ];

	if (kubsan_is_reported(&data->d_src))
		return;

	kubsan_format_location(&data->d_src, bloc, sizeof(bloc));
	kubsan_format_int(data->d_ltype, lhs, blhs, sizeof(blhs));
	kubsan_format_int(data->d_rtype, rhs, brhs, sizeof(brhs));
	if (is_negative(data->d_rtype, rhs))
		kubsan_report("kubsan: %s: shift: shift exponent %s is "
		    "negative\n",
		    bloc, brhs);
	else if (is_shift_exponent_too_large(data->d_rtype, rhs))
		kubsan_report("kubsan: %s: shift: shift exponent %s is too "
		    "large for %u-bit type\n",
		    bloc, brhs, NBITS(data->d_rtype));
	else if (is_negative(data->d_ltype, lhs))
		kubsan_report("kubsan: %s: shift: left shift of negative "
		    "value %s\n",
		    bloc, blhs);
	else
		kubsan_report("kubsan: %s: shift: left shift of %s by %s "
		    "places cannot be represented in type %s\n",
		    bloc, blhs, brhs, data->d_ltype->t_name);
}

void
kubsan_handle_ureachable(struct unreachable_data *data)
{
	char bloc[LOCATION_BUFSIZ];

	if (kubsan_is_reported(&data->d_src))
		return;

	kubsan_format_location(&data->d_src, bloc, sizeof(bloc));
	kubsan_report("kubsan: %s: unreachable: calling "
	    "__builtin_unreachable()\n",
	    bloc);
}

int64_t
kubsan_deserialize_int(struct type_descriptor *typ, unsigned long val)
{
	switch (NBITS(typ)) {
	case 8:
		return ((int8_t)val);
	case 16:
		return ((int16_t)val);
	case 32:
		return ((int32_t)val);
	case 64:
	default:
		return ((int64_t)val);
	}
}

uint64_t
kubsan_deserialize_uint(struct type_descriptor *typ, unsigned long val)
{
	switch (NBITS(typ)) {
	case 8:
		return ((uint8_t)val);
	case 16:
		return ((uint16_t)val);
	case 32:
		return ((uint32_t)val);
	case 64:
	default:
		return ((uint64_t)val);
	}
}

void
kubsan_format_int(struct type_descriptor *typ, unsigned long val,
    char *buf, size_t bufsiz)
{
	switch (typ->t_kind) {
	case 0:	/* integer */
		if (SIGNED(typ)) {
			int64_t i = kubsan_deserialize_int(typ, val);
			snprintf(buf, bufsiz, "%lld", i);
		} else {
			uint64_t u = kubsan_deserialize_uint(typ, val);
			snprintf(buf, bufsiz, "%llu", u);
		}
		break;
	default:
		snprintf(buf, bufsiz, "%#x<NaN>", typ->t_kind);
	}
}

void
kubsan_format_location(struct source_location *src, char *buf,
    size_t bufsiz)
{
	snprintf(buf, bufsiz, "%s:%u:%u",
	    src->sl_filename, src->sl_line & ~LOCATION_REPORTED,
	    src->sl_column);
}

int
kubsan_is_reported(struct source_location *src)
{
	uint32_t *line = &src->sl_line;
	uint32_t prev;

	/*
	 * Treat everything as reported when disabled.
	 * Otherwise, new violations would go by unnoticed.
	 */
	if (__predict_false(kubsan_watch == 0))
		return (1);

	do {
		prev = *line;
		/* If already reported, avoid redundant atomic operation. */
		if (prev & LOCATION_REPORTED)
			break;
	} while (atomic_cas_uint(line, prev, prev | LOCATION_REPORTED) != prev);

	return (prev & LOCATION_REPORTED);
}

const char *
kubsan_kind(uint8_t kind)
{
	static const char *kinds[] = {
		"load of",
		"store to",
		"reference binding to",
		"member access within",
		"member call on",
		"constructor call on",
		"downcast of",
		"downcast of",
		"upcast of",
		"cast to virtual base of",
		"_Nonnull binding to",
		"dynamic operation on"
	};

	if (kind >= nitems(kinds))
		return ("?");

	return (kinds[kind]);
}

void
kubsan_report(const char *fmt, ...)
{
	va_list ap;

	va_start(ap, fmt);
	vprintf(fmt, ap);
	va_end(ap);

#ifdef DDB
	if (kubsan_watch == 2)
		db_enter();
#endif
}

static int
is_negative(struct type_descriptor *typ, unsigned long val)
{
	return (SIGNED(typ) && kubsan_deserialize_int(typ, val) < 0);
}

static int
is_shift_exponent_too_large(struct type_descriptor *typ, unsigned long val)
{
	return (kubsan_deserialize_int(typ, val) >= NBITS(typ));
}
