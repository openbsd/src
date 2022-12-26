/*
 * Copyright (C) Internet Systems Consortium, Inc. ("ISC")
 *
 * Permission to use, copy, modify, and/or distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND ISC DISCLAIMS ALL WARRANTIES WITH
 * REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF MERCHANTABILITY
 * AND FITNESS.  IN NO EVENT SHALL ISC BE LIABLE FOR ANY SPECIAL, DIRECT,
 * INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING FROM
 * LOSS OF USE, DATA OR PROFITS, WHETHER IN AN ACTION OF CONTRACT, NEGLIGENCE
 * OR OTHER TORTIOUS ACTION, ARISING OUT OF OR IN CONNECTION WITH THE USE OR
 * PERFORMANCE OF THIS SOFTWARE.
 */

#include <isc/regex.h>
#include <isc/types.h>
#include <string.h>

/*
 * Validate the regular expression 'C' locale.
 */
int
isc_regex_validate(const char *c) {
	enum {
		none, parse_bracket, parse_bound,
		parse_ce, parse_ec, parse_cc
	} state = none;
	/* Well known character classes. */
	const char *cc[] = {
		":alnum:", ":digit:", ":punct:", ":alpha:", ":graph:",
		":space:", ":blank:", ":lower:", ":upper:", ":cntrl:",
		":print:", ":xdigit:"
	};
	int seen_comma = 0;
	int seen_high = 0;
	int seen_char = 0;
	int seen_ec = 0;
	int seen_ce = 0;
	int have_atom = 0;
	int group = 0;
	int range = 0;
	int sub = 0;
	int empty_ok = 0;
	int neg = 0;
	int was_multiple = 0;
	unsigned int low = 0;
	unsigned int high = 0;
	const char *ccname = NULL;
	int range_start = 0;

	if (c == NULL || *c == 0)
		return(-1);

	while (c != NULL && *c != 0) {
		switch (state) {
		case none:
			switch (*c) {
			case '\\':	/* make literal */
				++c;
				switch (*c) {
				case '1': case '2': case '3':
				case '4': case '5': case '6':
				case '7': case '8': case '9':
					if ((*c - '0') > sub)
						return(-1);
					have_atom = 1;
					was_multiple = 0;
					break;
				case 0:
					return(-1);
				default:
					goto literal;
				}
				++c;
				break;
			case '[':	/* bracket start */
				++c;
				neg = 0;
				was_multiple = 0;
				seen_char = 0;
				state = parse_bracket;
				break;
			case '{': 	/* bound start */
				switch (c[1]) {
				case '0': case '1': case '2': case '3':
				case '4': case '5': case '6': case '7':
				case '8': case '9':
					if (!have_atom)
						return(-1);
					if (was_multiple)
						return(-1);
					seen_comma = 0;
					seen_high = 0;
					low = high = 0;
					state = parse_bound;
					break;
				default:
					goto literal;
				}
				++c;
				have_atom = 1;
				was_multiple = 1;
				break;
			case '}':
				goto literal;
			case '(':	/* group start */
				have_atom = 0;
				was_multiple = 0;
				empty_ok = 1;
				++group;
				++sub;
				++c;
				break;
			case ')':	/* group end */
				if (group && !have_atom && !empty_ok)
					return(-1);
				have_atom = 1;
				was_multiple = 0;
				if (group != 0)
					--group;
				++c;
				break;
			case '|':	/* alternative separator */
				if (!have_atom)
					return(-1);
				have_atom = 0;
				empty_ok = 0;
				was_multiple = 0;
				++c;
				break;
			case '^':
			case '$':
				have_atom = 1;
				was_multiple = 1;
				++c;
				break;
			case '+':
			case '*':
			case '?':
				if (was_multiple)
					return(-1);
				if (!have_atom)
					return(-1);
				have_atom = 1;
				was_multiple = 1;
				++c;
				break;
			case '.':
			default:
			literal:
				have_atom = 1;
				was_multiple = 0;
				++c;
				break;
			}
			break;
		case parse_bound:
			switch (*c) {
			case '0': case '1': case '2': case '3': case '4':
			case '5': case '6': case '7': case '8': case '9':
				if (!seen_comma) {
					low = low * 10 + *c - '0';
					if (low > 255)
						return(-1);
				} else {
					seen_high = 1;
					high = high * 10 + *c - '0';
					if (high > 255)
						return(-1);
				}
				++c;
				break;
			case ',':
				if (seen_comma)
					return(-1);
				seen_comma = 1;
				++c;
				break;
			default:
			case '{':
				return(-1);
			case '}':
				if (seen_high && low > high)
					return(-1);
				seen_comma = 0;
				state = none;
				++c;
				break;
			}
			break;
		case parse_bracket:
			switch (*c) {
			case '^':
				if (seen_char || neg) goto inside;
				neg = 1;
				++c;
				break;
			case '-':
				if (range == 2) goto inside;
				if (!seen_char) goto inside;
				if (range == 1)
					return(-1);
				range = 2;
				++c;
				break;
			case '[':
				++c;
				switch (*c) {
				case '.':	/* collating element */
					if (range != 0) --range;
					++c;
					state = parse_ce;
					seen_ce = 0;
					break;
				case '=':	/* equivalence class */
					if (range == 2)
						return(-1);
					++c;
					state = parse_ec;
					seen_ec = 0;
					break;
				case ':':	/* character class */
					if (range == 2)
						return(-1);
					ccname = c;
					++c;
					state = parse_cc;
					break;
				}
				seen_char = 1;
				break;
			case ']':
				if (!c[1] && !seen_char)
					return(-1);
				if (!seen_char)
					goto inside;
				++c;
				range = 0;
				have_atom = 1;
				state = none;
				break;
			default:
			inside:
				seen_char = 1;
				if (range == 2 && (*c & 0xff) < range_start)
					return(-1);
				if (range != 0)
					--range;
				range_start = *c & 0xff;
				++c;
				break;
			};
			break;
		case parse_ce:
			switch (*c) {
			case '.':
				++c;
				switch (*c) {
				case ']':
					if (!seen_ce)
						return(-1);
					++c;
					state = parse_bracket;
					break;
				default:
					if (seen_ce)
						range_start = 256;
					else
						range_start = '.';
					seen_ce = 1;
					break;
				}
				break;
			default:
				if (seen_ce)
					range_start = 256;
				else
					range_start = *c;
				seen_ce = 1;
				++c;
				break;
			}
			break;
		case parse_ec:
			switch (*c) {
			case '=':
				++c;
				switch (*c) {
				case ']':
					if (!seen_ec)
						return(-1);
					++c;
					state = parse_bracket;
					break;
				default:
					seen_ec = 1;
					break;
				}
				break;
			default:
				seen_ec = 1;
				++c;
				break;
			}
			break;
		case parse_cc:
			switch (*c) {
			case ':':
				++c;
				switch (*c) {
				case ']': {
					unsigned int i;
					int found = 0;
					for (i = 0;
					     i < sizeof(cc)/sizeof(*cc);
					     i++)
					{
						unsigned int len;
						len = strlen(cc[i]);
						if (len !=
						    (unsigned int)(c - ccname))
							continue;
						if (strncmp(cc[i], ccname, len))
							continue;
						found = 1;
					}
					if (!found)
						return(-1);
					++c;
					state = parse_bracket;
					break;
					}
				default:
					break;
				}
				break;
			default:
				++c;
				break;
			}
			break;
		}
	}
	if (group != 0)
		return(-1);
	if (state != none)
		return(-1);
	if (!have_atom)
		return(-1);
	return (sub);
}
