/* $OpenBSD: magic-test.c,v 1.11 2015/08/12 07:43:27 nicm Exp $ */

/*
 * Copyright (c) 2015 Nicholas Marriott <nicm@openbsd.org>
 *
 * Permission to use, copy, modify, and distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF MIND, USE, DATA OR PROFITS, WHETHER
 * IN AN ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING
 * OUT OF OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */

#include <sys/types.h>

#include <ctype.h>
#include <errno.h>
#include <fcntl.h>
#include <limits.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <vis.h>

#include "magic.h"
#include "xmalloc.h"

static int
magic_one_eq(char a, char b, int cflag)
{
	if (a == b)
		return (1);
	if (cflag && islower((u_char)b) && tolower((u_char)a) == (u_char)b)
		return (1);
	return (0);
}

static int
magic_test_eq(const char *ap, size_t asize, const char *bp, size_t bsize,
    int cflag, int bflag, int Bflag)
{
	size_t	aoff, boff, aspaces, bspaces;

	aoff = boff = 0;
	while (aoff != asize && boff != bsize) {
		if (Bflag && isspace((u_char)ap[aoff])) {
			aspaces = 0;
			while (aoff != asize && isspace((u_char)ap[aoff])) {
				aspaces++;
				aoff++;
			}
			bspaces = 0;
			while (boff != bsize && isspace((u_char)bp[boff])) {
				bspaces++;
				boff++;
			}
			if (bspaces >= aspaces)
				continue;
			return (1);
		}
		if (magic_one_eq(ap[aoff], bp[boff], cflag)) {
			aoff++;
			boff++;
			continue;
		}
		if (bflag && isspace((u_char)bp[boff])) {
			boff++;
			continue;
		}
		if (ap[aoff] < bp[boff])
			return (-1);
		return (1);
	}
	return (0);
}

static int
magic_copy_from(struct magic_state *ms, ssize_t offset, void *dst, size_t size)
{
	if (offset < 0)
		offset = ms->offset;
	if (offset + size > ms->size)
		return (-1);
	memcpy(dst, ms->base + offset, size);
	return (0);
}

static void
magic_add_result(struct magic_state *ms, struct magic_line *ml,
    const char *fmt, ...)
{
	va_list	 ap;
	int	 separate;
	char	*s, *tmp, *add;

	va_start(ap, fmt);
	if (ml->stringify) {
		if (vasprintf(&s, fmt, ap) == -1) {
			va_end(ap);
			return;
		}
		va_end(ap);
		if (asprintf(&tmp, ml->result, s) == -1) {
			free(s);
			return;
		}
		free(s);
	} else {
		if (vasprintf(&tmp, ml->result, ap) == -1) {
			va_end(ap);
			return;
		}
		va_end(ap);
	}

	separate = 1;
	if (tmp[0] == '\\' && tmp[1] == 'b') {
		separate = 0;
		add = tmp + 2;
	} else
		add = tmp;

	if (separate && *ms->out != '\0')
		strlcat(ms->out, " ", sizeof ms->out);
	strlcat(ms->out, add, sizeof ms->out);

	free(tmp);
}

static void
magic_add_string(struct magic_state *ms, struct magic_line *ml,
    const char *s, size_t slen)
{
	char	*out;
	size_t	 outlen, offset;

	outlen = MAGIC_STRING_SIZE;
	if (outlen > slen)
		outlen = slen;
	for (offset = 0; offset < outlen; offset++) {
		if (s[offset] == '\0' || !isprint((u_char)s[offset])) {
			outlen = offset;
			break;
		}
	}
	out = xreallocarray(NULL, 4, outlen + 1);
	strvisx(out, s, outlen, VIS_TAB|VIS_NL|VIS_CSTYLE|VIS_OCTAL);
	magic_add_result(ms, ml, "%s", out);
	free(out);
}

static int
magic_test_signed(struct magic_line *ml, int64_t value, int64_t wanted)
{
	switch (ml->test_operator) {
	case 'x':
		return (1);
	case '<':
		return (value < wanted);
	case '[':
		return (value <= wanted);
	case '>':
		return (value > wanted);
	case ']':
		return (value >= wanted);
	case '=':
		return (value == wanted);
	case '&':
		return ((value & wanted) == wanted);
	case '^':
		return ((~value & wanted) == wanted);
	}
	return (-1);
}

static int
magic_test_unsigned(struct magic_line *ml, uint64_t value, uint64_t wanted)
{
	switch (ml->test_operator) {
	case 'x':
		return (1);
	case '<':
		return (value < wanted);
	case '[':
		return (value <= wanted);
	case '>':
		return (value > wanted);
	case ']':
		return (value >= wanted);
	case '=':
		return (value == wanted);
	case '&':
		return ((value & wanted) == wanted);
	case '^':
		return ((~value & wanted) == wanted);
	}
	return (-1);
}

static int
magic_test_double(struct magic_line *ml, double value, double wanted)
{
	switch (ml->test_operator) {
	case 'x':
		return (1);
	case '=':
		return (value == wanted);
	}
	return (-1);
}

static int
magic_test_type_none(__unused struct magic_line *ml,
    __unused struct magic_state *ms)
{
	return (0);
}

static int
magic_test_type_byte(struct magic_line *ml, struct magic_state *ms)
{
	int8_t	value;
	int	result;

	if (magic_copy_from(ms, -1, &value, sizeof value) != 0)
		return (0);

	if (ml->type_operator == '&')
		value &= (int8_t)ml->type_operand;
	else if (ml->type_operator == '-')
		value -= (int8_t)ml->type_operand;
	else if (ml->type_operator == '+')
		value += (int8_t)ml->type_operand;
	else if (ml->type_operator == '/')
		value /= (int8_t)ml->type_operand;
	else if (ml->type_operator == '%')
		value %= (int8_t)ml->type_operand;
	else if (ml->type_operator == '*')
		value *= (int8_t)ml->type_operand;
	else if (ml->type_operator != ' ')
		return (-1);

	result = magic_test_signed(ml, value, (int8_t)ml->test_signed);
	if (result == !ml->test_not && ml->result != NULL) {
		magic_add_result(ms, ml, "%c", (int)value);
		ms->offset += sizeof value;
	}
	return (result);
}

static int
magic_test_type_short(struct magic_line *ml, struct magic_state *ms)
{
	int16_t value;
	int	result;

	if (magic_copy_from(ms, -1, &value, sizeof value) != 0)
		return (0);
	if (ml->type == MAGIC_TYPE_BESHORT)
		value = be16toh(value);
	if (ml->type == MAGIC_TYPE_LESHORT)
		value = le16toh(value);

	if (ml->type_operator == '&')
		value &= (int16_t)ml->type_operand;
	else if (ml->type_operator == '-')
		value -= (int16_t)ml->type_operand;
	else if (ml->type_operator == '+')
		value += (int16_t)ml->type_operand;
	else if (ml->type_operator == '/')
		value /= (int16_t)ml->type_operand;
	else if (ml->type_operator == '%')
		value %= (int16_t)ml->type_operand;
	else if (ml->type_operator == '*')
		value *= (int16_t)ml->type_operand;
	else if (ml->type_operator != ' ')
		return (-1);

	result = magic_test_signed(ml, value, (int16_t)ml->test_signed);
	if (result == !ml->test_not && ml->result != NULL) {
		magic_add_result(ms, ml, "%hd", (int)value);
		ms->offset += sizeof value;
	}
	return (result);
}

static int
magic_test_type_long(struct magic_line *ml, struct magic_state *ms)
{
	int32_t value;
	int	result;

	if (magic_copy_from(ms, -1, &value, sizeof value) != 0)
		return (0);
	if (ml->type == MAGIC_TYPE_BELONG)
		value = be32toh(value);
	if (ml->type == MAGIC_TYPE_LELONG)
		value = le32toh(value);

	if (ml->type_operator == '&')
		value &= (int32_t)ml->type_operand;
	else if (ml->type_operator == '-')
		value -= (int32_t)ml->type_operand;
	else if (ml->type_operator == '+')
		value += (int32_t)ml->type_operand;
	else if (ml->type_operator == '/')
		value /= (int32_t)ml->type_operand;
	else if (ml->type_operator == '%')
		value %= (int32_t)ml->type_operand;
	else if (ml->type_operator == '*')
		value *= (int32_t)ml->type_operand;
	else if (ml->type_operator != ' ')
		return (-1);

	result = magic_test_signed(ml, value, (int32_t)ml->test_signed);
	if (result == !ml->test_not && ml->result != NULL) {
		magic_add_result(ms, ml, "%d", (int)value);
		ms->offset += sizeof value;
	}
	return (result);
}

static int
magic_test_type_quad(struct magic_line *ml, struct magic_state *ms)
{
	int64_t value;
	int	result;

	if (magic_copy_from(ms, -1, &value, sizeof value) != 0)
		return (0);
	if (ml->type == MAGIC_TYPE_BEQUAD)
		value = be64toh(value);
	if (ml->type == MAGIC_TYPE_LEQUAD)
		value = le64toh(value);

	if (ml->type_operator == '&')
		value &= (int64_t)ml->type_operand;
	else if (ml->type_operator == '-')
		value -= (int64_t)ml->type_operand;
	else if (ml->type_operator == '+')
		value += (int64_t)ml->type_operand;
	else if (ml->type_operator == '/')
		value /= (int64_t)ml->type_operand;
	else if (ml->type_operator == '%')
		value %= (int64_t)ml->type_operand;
	else if (ml->type_operator == '*')
		value *= (int64_t)ml->type_operand;
	else if (ml->type_operator != ' ')
		return (-1);

	result = magic_test_signed(ml, value, (int64_t)ml->test_signed);
	if (result == !ml->test_not && ml->result != NULL) {
		magic_add_result(ms, ml, "%lld", (long long)value);
		ms->offset += sizeof value;
	}
	return (result);
}

static int
magic_test_type_ubyte(struct magic_line *ml, struct magic_state *ms)
{
	uint8_t value;
	int	result;

	if (magic_copy_from(ms, -1, &value, sizeof value) != 0)
		return (0);

	if (ml->type_operator == '&')
		value &= (uint8_t)ml->type_operand;
	else if (ml->type_operator == '-')
		value -= (uint8_t)ml->type_operand;
	else if (ml->type_operator == '+')
		value += (uint8_t)ml->type_operand;
	else if (ml->type_operator == '/')
		value /= (uint8_t)ml->type_operand;
	else if (ml->type_operator == '%')
		value %= (uint8_t)ml->type_operand;
	else if (ml->type_operator == '*')
		value *= (uint8_t)ml->type_operand;
	else if (ml->type_operator != ' ')
		return (-1);

	result = magic_test_unsigned(ml, value, (uint8_t)ml->test_unsigned);
	if (result == !ml->test_not && ml->result != NULL) {
		magic_add_result(ms, ml, "%c", (unsigned int)value);
		ms->offset += sizeof value;
	}
	return (result);
}

static int
magic_test_type_ushort(struct magic_line *ml, struct magic_state *ms)
{
	uint16_t	value;
	int		result;

	if (magic_copy_from(ms, -1, &value, sizeof value) != 0)
		return (0);
	if (ml->type == MAGIC_TYPE_UBESHORT)
		value = be16toh(value);
	if (ml->type == MAGIC_TYPE_ULESHORT)
		value = le16toh(value);

	if (ml->type_operator == '&')
		value &= (uint16_t)ml->type_operand;
	else if (ml->type_operator == '-')
		value -= (uint16_t)ml->type_operand;
	else if (ml->type_operator == '+')
		value += (uint16_t)ml->type_operand;
	else if (ml->type_operator == '/')
		value /= (uint16_t)ml->type_operand;
	else if (ml->type_operator == '%')
		value %= (uint16_t)ml->type_operand;
	else if (ml->type_operator == '*')
		value *= (uint16_t)ml->type_operand;
	else if (ml->type_operator != ' ')
		return (-1);

	result = magic_test_unsigned(ml, value, (uint16_t)ml->test_unsigned);
	if (result == !ml->test_not && ml->result != NULL) {
		magic_add_result(ms, ml, "%hu", (unsigned int)value);
		ms->offset += sizeof value;
	}
	return (result);
}

static int
magic_test_type_ulong(struct magic_line *ml, struct magic_state *ms)
{
	uint32_t	value;
	int		result;

	if (magic_copy_from(ms, -1, &value, sizeof value) != 0)
		return (0);
	if (ml->type == MAGIC_TYPE_UBELONG)
		value = be32toh(value);
	if (ml->type == MAGIC_TYPE_ULELONG)
		value = le32toh(value);

	if (ml->type_operator == '&')
		value &= (uint32_t)ml->type_operand;
	else if (ml->type_operator == '-')
		value -= (uint32_t)ml->type_operand;
	else if (ml->type_operator == '+')
		value += (uint32_t)ml->type_operand;
	else if (ml->type_operator == '/')
		value /= (uint32_t)ml->type_operand;
	else if (ml->type_operator == '%')
		value %= (uint32_t)ml->type_operand;
	else if (ml->type_operator == '*')
		value *= (uint32_t)ml->type_operand;
	else if (ml->type_operator != ' ')
		return (-1);

	result = magic_test_unsigned(ml, value, (uint32_t)ml->test_unsigned);
	if (result == !ml->test_not && ml->result != NULL) {
		magic_add_result(ms, ml, "%u", (unsigned int)value);
		ms->offset += sizeof value;
	}
	return (result);
}

static int
magic_test_type_uquad(struct magic_line *ml, struct magic_state *ms)
{
	uint64_t	value;
	int		result;

	if (magic_copy_from(ms, -1, &value, sizeof value) != 0)
		return (0);
	if (ml->type == MAGIC_TYPE_UBEQUAD)
		value = be64toh(value);
	if (ml->type == MAGIC_TYPE_ULEQUAD)
		value = le64toh(value);

	if (ml->type_operator == '&')
		value &= (uint64_t)ml->type_operand;
	else if (ml->type_operator == '-')
		value -= (uint64_t)ml->type_operand;
	else if (ml->type_operator == '+')
		value += (uint64_t)ml->type_operand;
	else if (ml->type_operator == '/')
		value /= (uint64_t)ml->type_operand;
	else if (ml->type_operator == '%')
		value %= (uint64_t)ml->type_operand;
	else if (ml->type_operator == '*')
		value *= (uint64_t)ml->type_operand;
	else if (ml->type_operator != ' ')
		return (-1);

	result = magic_test_unsigned(ml, value, (uint64_t)ml->test_unsigned);
	if (result == !ml->test_not && ml->result != NULL) {
		magic_add_result(ms, ml, "%llu", (unsigned long long)value);
		ms->offset += sizeof value;
	}
	return (result);
}

static int
magic_test_type_float(struct magic_line *ml, struct magic_state *ms)
{
	uint32_t	value0;
	double		value;
	int		result;

	if (magic_copy_from(ms, -1, &value0, sizeof value0) != 0)
		return (0);
	if (ml->type == MAGIC_TYPE_BEFLOAT)
		value0 = be32toh(value0);
	if (ml->type == MAGIC_TYPE_LEFLOAT)
		value0 = le32toh(value0);
	memcpy(&value, &value0, sizeof value);

	if (ml->type_operator != ' ')
		return (-1);

	result = magic_test_double(ml, value, (float)ml->test_double);
	if (result == !ml->test_not && ml->result != NULL) {
		magic_add_result(ms, ml, "%g", value);
		ms->offset += sizeof value0;
	}
	return (1);
}

static int
magic_test_type_double(struct magic_line *ml, struct magic_state *ms)
{
	uint64_t	value0;
	double		value;
	int		result;

	if (magic_copy_from(ms, -1, &value0, sizeof value0) != 0)
		return (0);
	if (ml->type == MAGIC_TYPE_BEDOUBLE)
		value0 = be64toh(value0);
	if (ml->type == MAGIC_TYPE_LEDOUBLE)
		value0 = le64toh(value0);
	memcpy(&value, &value0, sizeof value);

	if (ml->type_operator != ' ')
		return (-1);

	result = magic_test_double(ml, value, (double)ml->test_double);
	if (result == !ml->test_not && ml->result != NULL) {
		magic_add_result(ms, ml, "%g", value);
		ms->offset += sizeof value0;
	}
	return (1);
}

static int
magic_test_type_string(struct magic_line *ml, struct magic_state *ms)
{
	const char	*s, *cp;
	size_t		 slen;
	int		 result, cflag = 0, bflag = 0, Bflag = 0;

	cp = &ml->type_string[(sizeof "string") - 1];
	if (*cp != '\0') {
		if (*cp != '/')
			return (-1);
		cp++;
		for (; *cp != '\0'; cp++) {
			switch (*cp) {
			case 'B':
			case 'W':
				Bflag = 1;
				break;
			case 'b':
			case 'w':
				bflag = 1;
				break;
			case 'c':
				cflag = 1;
				break;
			case 't':
				break;
			default:
				return (-1);
			}
		}
	}

	s = ms->base + ms->offset;
	slen = ms->size - ms->offset;
	if (slen < ml->test_string_size)
		return (0);

	result = magic_test_eq(s, slen, ml->test_string, ml->test_string_size,
	    cflag, bflag, Bflag);
	switch (ml->test_operator) {
	case 'x':
		result = 1;
		break;
	case '<':
		result = result < 0;
		break;
	case '>':
		result = result > 0;
		break;
	case '=':
		slen = ml->test_string_size; /* only print what was found */
		result = result == 0;
		break;
	default:
		result = -1;
		break;
	}
	if (result == !ml->test_not) {
		if (ml->result != NULL)
			magic_add_string(ms, ml, s, slen);
		if (result && ml->test_operator == '=')
			ms->offset = s - ms->base + ml->test_string_size;
	}
	return (result);
}

static int
magic_test_type_pstring(struct magic_line *ml, struct magic_state *ms)
{
	const char	*s, *cp;
	size_t		 slen;
	int		 result;

	cp = &ml->type_string[(sizeof "pstring") - 1];
	if (*cp != '\0') {
		if (*cp != '/')
			return (-1);
		cp++;
		for (; *cp != '\0'; cp++) {
			switch (*cp) {
			default:
				return (-1);
			}
		}
	}

	s = ms->base + ms->offset;
	if (ms->size - ms->offset < 1)
		return (-1);
	slen = *(u_char *)s;
	if (slen > ms->size - ms->offset)
		return (-1);
	s++;

	if (slen < ml->test_string_size)
		result = -1;
	else if (slen > ml->test_string_size)
		result = 1;
	else
		result = memcmp(s, ml->test_string, ml->test_string_size);
	switch (ml->test_operator) {
	case 'x':
		result = 1;
		break;
	case '<':
		result = result < 0;
		break;
	case '>':
		result = result > 0;
		break;
	case '=':
		result = result == 0;
		break;
	default:
		result = -1;
		break;
	}
	if (result == !ml->test_not) {
		if (ml->result != NULL)
			magic_add_string(ms, ml, s, slen);
		if (result)
			ms->offset += slen + 1;
	}
	return (result);
}

static int
magic_test_type_date(struct magic_line *ml, struct magic_state *ms)
{
	int32_t	value;
	int	result;
	time_t	t;
	char	s[64];

	if (magic_copy_from(ms, -1, &value, sizeof value) != 0)
		return (0);
	if (ml->type == MAGIC_TYPE_BEDATE ||
	    ml->type == MAGIC_TYPE_BELDATE)
		value = be32toh(value);
	if (ml->type == MAGIC_TYPE_LEDATE ||
	    ml->type == MAGIC_TYPE_LELDATE)
		value = le32toh(value);

	if (ml->type_operator == '&')
		value &= (int32_t)ml->type_operand;
	else if (ml->type_operator != ' ')
		return (-1);

	result = magic_test_signed(ml, value, (int32_t)ml->test_signed);
	if (result == !ml->test_not && ml->result != NULL) {
		t = value;
		switch (ml->type) {
		case MAGIC_TYPE_LDATE:
		case MAGIC_TYPE_LELDATE:
		case MAGIC_TYPE_BELDATE:
			ctime_r(&t, s);
			break;
		default:
			asctime_r(gmtime(&t), s);
			break;
		}
		s[strcspn(s, "\n")] = '\0';
		magic_add_result(ms, ml, "%s", s);
		ms->offset += sizeof value;
	}
	return (result);
}

static int
magic_test_type_qdate(struct magic_line *ml, struct magic_state *ms)
{
	int64_t value;
	int	result;
	time_t	t;
	char	s[64];

	if (magic_copy_from(ms, -1, &value, sizeof value) != 0)
		return (0);
	if (ml->type == MAGIC_TYPE_BEQDATE ||
	    ml->type == MAGIC_TYPE_BEQLDATE)
		value = be64toh(value);
	if (ml->type == MAGIC_TYPE_LEQDATE ||
	    ml->type == MAGIC_TYPE_LEQLDATE)
		value = le64toh(value);

	if (ml->type_operator == '&')
		value &= (int64_t)ml->type_operand;
	else if (ml->type_operator != ' ')
		return (-1);

	result = magic_test_signed(ml, value, (int64_t)ml->test_signed);
	if (result == !ml->test_not && ml->result != NULL) {
		t = value;
		switch (ml->type) {
		case MAGIC_TYPE_QLDATE:
		case MAGIC_TYPE_LEQLDATE:
		case MAGIC_TYPE_BEQLDATE:
			ctime_r(&t, s);
			break;
		default:
			asctime_r(gmtime(&t), s);
			break;
		}
		s[strcspn(s, "\n")] = '\0';
		magic_add_result(ms, ml, "%s", s);
		ms->offset += sizeof value;
	}
	return (result);
}

static int
magic_test_type_udate(struct magic_line *ml, struct magic_state *ms)
{
	uint32_t	value;
	int		result;
	time_t		t;
	char		s[64];

	if (magic_copy_from(ms, -1, &value, sizeof value) != 0)
		return (0);
	if (ml->type == MAGIC_TYPE_BEDATE ||
	    ml->type == MAGIC_TYPE_BELDATE)
		value = be32toh(value);
	if (ml->type == MAGIC_TYPE_LEDATE ||
	    ml->type == MAGIC_TYPE_LELDATE)
		value = le32toh(value);

	if (ml->type_operator == '&')
		value &= (uint32_t)ml->type_operand;
	else if (ml->type_operator != ' ')
		return (-1);

	result = magic_test_unsigned(ml, value, (uint32_t)ml->test_unsigned);
	if (result == !ml->test_not && ml->result != NULL) {
		t = value;
		switch (ml->type) {
		case MAGIC_TYPE_LDATE:
		case MAGIC_TYPE_LELDATE:
		case MAGIC_TYPE_BELDATE:
			ctime_r(&t, s);
			break;
		default:
			asctime_r(gmtime(&t), s);
			break;
		}
		s[strcspn(s, "\n")] = '\0';
		magic_add_result(ms, ml, "%s", s);
		ms->offset += sizeof value;
	}
	return (result);
}

static int
magic_test_type_uqdate(struct magic_line *ml, struct magic_state *ms)
{
	uint64_t	value;
	int		result;
	time_t		t;
	char		s[64];

	if (magic_copy_from(ms, -1, &value, sizeof value) != 0)
		return (0);
	if (ml->type == MAGIC_TYPE_UBEQDATE ||
	    ml->type == MAGIC_TYPE_UBEQLDATE)
		value = be64toh(value);
	if (ml->type == MAGIC_TYPE_ULEQDATE ||
	    ml->type == MAGIC_TYPE_ULEQLDATE)
		value = le64toh(value);

	if (ml->type_operator == '&')
		value &= (uint64_t)ml->type_operand;
	else if (ml->type_operator != ' ')
		return (-1);

	result = magic_test_unsigned(ml, value, (uint64_t)ml->test_unsigned);
	if (result == !ml->test_not && ml->result != NULL) {
		t = value;
		switch (ml->type) {
		case MAGIC_TYPE_UQLDATE:
		case MAGIC_TYPE_ULEQLDATE:
		case MAGIC_TYPE_UBEQLDATE:
			ctime_r(&t, s);
			break;
		default:
			asctime_r(gmtime(&t), s);
			break;
		}
		s[strcspn(s, "\n")] = '\0';
		magic_add_result(ms, ml, "%s", s);
		ms->offset += sizeof value;
	}
	return (result);
}

static int
magic_test_type_bestring16(__unused struct magic_line *ml,
    __unused struct magic_state *ms)
{
	return (-2);
}

static int
magic_test_type_lestring16(__unused struct magic_line *ml,
    __unused struct magic_state *ms)
{
	return (-2);
}

static int
magic_test_type_melong(__unused struct magic_line *ml,
    __unused struct magic_state *ms)
{
	return (-2);
}

static int
magic_test_type_medate(__unused struct magic_line *ml,
    __unused struct magic_state *ms)
{
	return (-2);
}

static int
magic_test_type_meldate(__unused struct magic_line *ml,
    __unused struct magic_state *ms)
{
	return (-2);
}

static int
magic_test_type_regex(struct magic_line *ml, struct magic_state *ms)
{
	const char	*cp;
	regex_t		 re;
	regmatch_t	 m;
	int		 result, flags = 0, sflag = 0;

	cp = &ml->type_string[(sizeof "regex") - 1];
	if (*cp != '\0') {
		if (*cp != '/')
			return (-1);
		cp++;
		for (; *cp != '\0'; cp++) {
			switch (*cp) {
			case 's':
				sflag = 1;
				break;
			case 'c':
				flags |= REG_ICASE;
				break;
			default:
				return (-1);
			}
		}
	}

	if (regcomp(&re, ml->test_string, REG_EXTENDED) != 0)
		return (-1);
	m.rm_so = ms->offset;
	m.rm_eo = ms->size;

	result = (regexec(&re, ms->base, 1, &m, REG_STARTEND) == 0);
	if (result == !ml->test_not && ml->result != NULL) {
		magic_add_result(ms, ml, "%s", "");
		if (result) {
			if (sflag)
				ms->offset = m.rm_so;
			else
				ms->offset = m.rm_eo;
		}
	}
	regfree(&re);
	return (result);
}

static int
magic_test_type_search(struct magic_line *ml, struct magic_state *ms)
{
	const char	*cp, *endptr, *start, *found;
	size_t		 size, end, i;
	uint64_t	 range;
	int		 result, n, cflag = 0, bflag = 0, Bflag = 0;

	cp = &ml->type_string[(sizeof "search") - 1];
	if (*cp != '\0') {
		if (*cp != '/')
			return (-1);
		cp++;

		endptr = magic_strtoull(cp, &range);
		if (endptr == NULL || (*endptr != '/' && *endptr != '\0'))
			return (-1);

		if (*endptr == '/') {
			for (cp = endptr + 1; *cp != '\0'; cp++) {
				switch (*cp) {
				case 'B':
				case 'W':
					Bflag = 1;
					break;
				case 'b':
				case 'w':
					bflag = 1;
					break;
				case 'c':
					cflag = 1;
					break;
				case 't':
					break;
				default:
					return (-1);
				}
			}
		}
	} else
		range = UINT64_MAX;
	if (range > (uint64_t)ms->size - ms->offset)
		range = ms->size - ms->offset;
	size = ml->test_string_size;

	/* Want to search every starting position from up to range + size. */
	end = range + size;
	if (end > ms->size - ms->offset) {
		if (size > ms->size - ms->offset)
			end = 0;
		else
			end = ms->size - ms->offset - size;
	}

	/*
	 * < and > and the flags are only in /etc/magic with search/1 so don't
	 * support them with anything else.
	 */
	start = ms->base + ms->offset;
	if (end == 0)
		found = NULL;
	else if (ml->test_operator == 'x')
		found = start;
	else if (range == 1) {
		n = magic_test_eq(start, ms->size - ms->offset, ml->test_string,
		    size, cflag, bflag, Bflag);
		if (n == -1 && ml->test_operator == '<')
			found = start;
		else if (n == 1 && ml->test_operator == '>')
			found = start;
		else if (n == 0 && ml->test_operator == '=')
			found = start;
		else
			found = NULL;
	} else {
		if (ml->test_operator != '=')
			return (-2);
		for (i = 0; i < end; i++) {
			n = magic_test_eq(start + i, ms->size - ms->offset - i,
			    ml->test_string, size, cflag, bflag, Bflag);
			if (n == 0) {
				found = start + i;
				break;
			}
		}
		if (i == end)
			found = NULL;
	}
	result = (found != NULL);

	if (result == !ml->test_not && ml->result != NULL && found != NULL) {
		magic_add_string(ms, ml, found, ms->size - ms->offset);
		ms->offset = found - start + size;
	}
	return (result);
}

static int
magic_test_type_default(__unused struct magic_line *ml,
    __unused struct magic_state *ms)
{
	return (1);
}

static int (*magic_test_functions[])(struct magic_line *,
    struct magic_state *) = {
	magic_test_type_none,
	magic_test_type_byte,
	magic_test_type_short,
	magic_test_type_long,
	magic_test_type_quad,
	magic_test_type_ubyte,
	magic_test_type_ushort,
	magic_test_type_ulong,
	magic_test_type_uquad,
	magic_test_type_float,
	magic_test_type_double,
	magic_test_type_string,
	magic_test_type_pstring,
	magic_test_type_date,
	magic_test_type_qdate,
	magic_test_type_date,
	magic_test_type_qdate,
	magic_test_type_udate,
	magic_test_type_uqdate,
	magic_test_type_udate,
	magic_test_type_qdate,
	magic_test_type_short,
	magic_test_type_long,
	magic_test_type_quad,
	magic_test_type_ushort,
	magic_test_type_ulong,
	magic_test_type_uquad,
	magic_test_type_float,
	magic_test_type_double,
	magic_test_type_date,
	magic_test_type_qdate,
	magic_test_type_date,
	magic_test_type_qdate,
	magic_test_type_udate,
	magic_test_type_uqdate,
	magic_test_type_udate,
	magic_test_type_uqdate,
	magic_test_type_bestring16,
	magic_test_type_short,
	magic_test_type_long,
	magic_test_type_quad,
	magic_test_type_ushort,
	magic_test_type_ulong,
	magic_test_type_uquad,
	magic_test_type_float,
	magic_test_type_double,
	magic_test_type_date,
	magic_test_type_qdate,
	magic_test_type_date,
	magic_test_type_qdate,
	magic_test_type_udate,
	magic_test_type_uqdate,
	magic_test_type_udate,
	magic_test_type_uqdate,
	magic_test_type_lestring16,
	magic_test_type_melong,
	magic_test_type_medate,
	magic_test_type_meldate,
	magic_test_type_regex,
	magic_test_type_search,
	magic_test_type_default,
};

static int
magic_test_line(struct magic_line *ml, struct magic_state *ms)
{
	struct magic_line	*child;
	int64_t			 offset, wanted, next;
	int			 result;
	uint8_t			 b;
	uint16_t		 s;
	uint32_t		 l;

	if (ml->indirect_type == ' ')
		wanted = ml->offset;
	else {
		wanted = ml->indirect_offset;
		if (ml->indirect_relative) {
			if (wanted < 0 && -wanted > ms->offset)
				return (0);
			if (wanted > 0 && ms->offset + wanted > ms->size)
				return (0);
			next = ms->offset + ml->indirect_offset;
		} else
			next = wanted;

		switch (ml->indirect_type) {
		case 'b':
		case 'B':
			if (magic_copy_from(ms, next, &b, sizeof b) != 0)
				return (0);
			wanted = b;
			break;
		case 's':
			if (magic_copy_from(ms, next, &s, sizeof s) != 0)
				return (0);
			wanted = le16toh(s);
			break;
		case 'S':
			if (magic_copy_from(ms, next, &s, sizeof s) != 0)
				return (0);
			wanted = be16toh(s);
			break;
		case 'l':
			if (magic_copy_from(ms, next, &l, sizeof l) != 0)
				return (0);
			wanted = le16toh(l);
			break;
		case 'L':
			if (magic_copy_from(ms, next, &l, sizeof l) != 0)
				return (0);
			wanted = be16toh(l);
			break;
		}

		switch (ml->indirect_operator) {
		case '+':
			wanted += ml->indirect_operand;
			break;
		case '-':
			wanted -= ml->indirect_operand;
			break;
		case '*':
			wanted *= ml->indirect_operand;
			break;
		}
	}

	if (ml->offset_relative) {
		if (wanted < 0 && -wanted > ms->offset)
			return (0);
		if (wanted > 0 && ms->offset + wanted > ms->size)
			return (0);
		offset = ms->offset + wanted;
	} else
		offset = wanted;
	if (offset < 0 || offset > ms->size)
		return (0);
	ms->offset = offset;

	result = magic_test_functions[ml->type](ml, ms);
	if (result == -1) {
		magic_warn(ml, "test %s/%c failed", ml->type_string,
		    ml->test_operator);
		return (0);
	}
	if (result == -2) {
		magic_warn(ml, "test %s/%c not implemented", ml->type_string,
		    ml->test_operator);
		return (0);
	}
	if (result == ml->test_not)
		return (0);
	if (ml->mimetype != NULL)
		ms->mimetype = ml->mimetype;

	magic_warn(ml, "test %s/%c matched at offset %llu: '%s'",
	    ml->type_string, ml->test_operator, ms->offset,
	    ml->result == NULL ? "" : ml->result);

	offset = ms->offset;
	TAILQ_FOREACH(child, &ml->children, entry) {
		ms->offset = offset;
		magic_test_line(child, ms);
	}
	return (ml->result != NULL);
}

const char *
magic_test(struct magic *m, const void *base, size_t size, int flags)
{
	struct magic_line		*ml;
	static struct magic_state	 ms;

	memset(&ms, 0, sizeof ms);

	ms.base = base;
	ms.size = size;

	ms.text = !!(flags & MAGIC_TEST_TEXT);

	RB_FOREACH(ml, magic_tree, &m->tree) {
		ms.offset = 0;
		if (ml->text == ms.text && magic_test_line(ml, &ms))
			break;
	}

	if (*ms.out != '\0') {
		if (flags & MAGIC_TEST_MIME) {
			if (ms.mimetype)
				return (xstrdup(ms.mimetype));
			return (NULL);
		}
		return (xstrdup(ms.out));
	}
	return (NULL);
}
