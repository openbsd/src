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

/* $Id: lex.c,v 1.8 2020/02/25 05:00:43 jsg Exp $ */

/*! \file */

#include <ctype.h>
#include <stdlib.h>

#include <isc/buffer.h>

#include <isc/lex.h>

#include <isc/parseint.h>

#include <errno.h>
#include <string.h>
#include <isc/util.h>

#include "unix/errno2result.h"

typedef struct inputsource {
	isc_result_t			result;
	isc_boolean_t			is_file;
	isc_boolean_t			need_close;
	isc_boolean_t			at_eof;
	isc_boolean_t			last_was_eol;
	isc_buffer_t *			pushback;
	unsigned int			ignored;
	void *				input;
	char *				name;
	unsigned long			line;
	unsigned long			saved_line;
	ISC_LINK(struct inputsource)	link;
} inputsource;

struct isc_lex {
	/* Unlocked. */
	size_t				max_token;
	char *				data;
	unsigned int			comments;
	isc_boolean_t			comment_ok;
	isc_boolean_t			last_was_eol;
	unsigned int			paren_count;
	unsigned int			saved_paren_count;
	isc_lexspecials_t		specials;
	LIST(struct inputsource)	sources;
};

static inline isc_result_t
grow_data(isc_lex_t *lex, size_t *remainingp, char **currp, char **prevp) {
	char *tmp;

	tmp = malloc(lex->max_token * 2 + 1);
	if (tmp == NULL)
		return (ISC_R_NOMEMORY);
	memmove(tmp, lex->data, lex->max_token + 1);
	*currp = tmp + (*currp - lex->data);
	if (*prevp != NULL)
		*prevp = tmp + (*prevp - lex->data);
	free(lex->data);
	lex->data = tmp;
	*remainingp += lex->max_token;
	lex->max_token *= 2;
	return (ISC_R_SUCCESS);
}

isc_result_t
isc_lex_create(size_t max_token, isc_lex_t **lexp) {
	isc_lex_t *lex;

	/*
	 * Create a lexer.
	 */
	REQUIRE(lexp != NULL && *lexp == NULL);

	if (max_token == 0U)
		max_token = 1;

	lex = malloc(sizeof(*lex));
	if (lex == NULL)
		return (ISC_R_NOMEMORY);
	lex->data = malloc(max_token + 1);
	if (lex->data == NULL) {
		free(lex);
		return (ISC_R_NOMEMORY);
	}
	lex->max_token = max_token;
	lex->comments = 0;
	lex->comment_ok = ISC_TRUE;
	lex->last_was_eol = ISC_TRUE;
	lex->paren_count = 0;
	lex->saved_paren_count = 0;
	memset(lex->specials, 0, 256);
	INIT_LIST(lex->sources);

	*lexp = lex;

	return (ISC_R_SUCCESS);
}

void
isc_lex_destroy(isc_lex_t **lexp) {
	isc_lex_t *lex;

	/*
	 * Destroy the lexer.
	 */

	REQUIRE(lexp != NULL);
	lex = *lexp;

	while (!EMPTY(lex->sources))
		RUNTIME_CHECK(isc_lex_close(lex) == ISC_R_SUCCESS);
	if (lex->data != NULL)
		free(lex->data);
	free(lex);

	*lexp = NULL;
}

void
isc_lex_setcomments(isc_lex_t *lex, unsigned int comments) {
	/*
	 * Set allowed lexer commenting styles.
	 */

	lex->comments = comments;
}

void
isc_lex_setspecials(isc_lex_t *lex, isc_lexspecials_t specials) {
	/*
	 * The characters in 'specials' are returned as tokens.  Along with
	 * whitespace, they delimit strings and numbers.
	 */

	memmove(lex->specials, specials, 256);
}

static inline isc_result_t
new_source(isc_lex_t *lex, isc_boolean_t is_file, isc_boolean_t need_close,
	   void *input, const char *name)
{
	inputsource *source;
	isc_result_t result;

	source = malloc(sizeof(*source));
	if (source == NULL)
		return (ISC_R_NOMEMORY);
	source->result = ISC_R_SUCCESS;
	source->is_file = is_file;
	source->need_close = need_close;
	source->at_eof = ISC_FALSE;
	source->last_was_eol = lex->last_was_eol;
	source->input = input;
	source->name = strdup(name);
	if (source->name == NULL) {
		free(source);
		return (ISC_R_NOMEMORY);
	}
	source->pushback = NULL;
	result = isc_buffer_allocate(&source->pushback,
				     (unsigned int)lex->max_token);
	if (result != ISC_R_SUCCESS) {
		free(source->name);
		free(source);
		return (result);
	}
	source->ignored = 0;
	source->line = 1;
	ISC_LIST_INITANDPREPEND(lex->sources, source, link);

	return (ISC_R_SUCCESS);
}

isc_result_t
isc_lex_openfile(isc_lex_t *lex, const char *filename) {
	isc_result_t result = ISC_R_SUCCESS;
	FILE *stream = NULL;

	/*
	 * Open 'filename' and make it the current input source for 'lex'.
	 */

	if ((stream = fopen(filename, "r")) == NULL)
		return (isc__errno2result(errno));

	result = new_source(lex, ISC_TRUE, ISC_TRUE, stream, filename);
	if (result != ISC_R_SUCCESS)
		(void)fclose(stream);
	return (result);
}

isc_result_t
isc_lex_close(isc_lex_t *lex) {
	inputsource *source;

	/*
	 * Close the most recently opened object (i.e. file or buffer).
	 */

	source = HEAD(lex->sources);
	if (source == NULL)
		return (ISC_R_NOMORE);

	ISC_LIST_UNLINK(lex->sources, source, link);
	lex->last_was_eol = source->last_was_eol;
	if (source->is_file) {
		if (source->need_close)
			(void)fclose((FILE *)(source->input));
	}
	free(source->name);
	isc_buffer_free(&source->pushback);
	free(source);

	return (ISC_R_SUCCESS);
}

typedef enum {
	lexstate_start,
	lexstate_crlf,
	lexstate_string,
	lexstate_number,
	lexstate_maybecomment,
	lexstate_ccomment,
	lexstate_ccommentend,
	lexstate_eatline,
	lexstate_qstring
} lexstate;

#define IWSEOL (ISC_LEXOPT_INITIALWS | ISC_LEXOPT_EOL)

static void
pushback(inputsource *source, int c) {
	REQUIRE(source->pushback->current > 0);
	if (c == EOF) {
		source->at_eof = ISC_FALSE;
		return;
	}
	source->pushback->current--;
	if (c == '\n')
		source->line--;
}

static isc_result_t
pushandgrow(inputsource *source, int c) {
	if (isc_buffer_availablelength(source->pushback) == 0) {
		isc_buffer_t *tbuf = NULL;
		unsigned int oldlen;
		isc_region_t used;
		isc_result_t result;

		oldlen = isc_buffer_length(source->pushback);
		result = isc_buffer_allocate(&tbuf, oldlen * 2);
		if (result != ISC_R_SUCCESS)
			return (result);
		isc_buffer_usedregion(source->pushback, &used);
		result = isc_buffer_copyregion(tbuf, &used);
		INSIST(result == ISC_R_SUCCESS);
		tbuf->current = source->pushback->current;
		isc_buffer_free(&source->pushback);
		source->pushback = tbuf;
	}
	isc_buffer_putuint8(source->pushback, (uint8_t)c);
	return (ISC_R_SUCCESS);
}

isc_result_t
isc_lex_gettoken(isc_lex_t *lex, unsigned int options, isc_token_t *tokenp) {
	inputsource *source;
	int c;
	isc_boolean_t done = ISC_FALSE;
	isc_boolean_t no_comments = ISC_FALSE;
	isc_boolean_t escaped = ISC_FALSE;
	lexstate state = lexstate_start;
	lexstate saved_state = lexstate_start;
	isc_buffer_t *buffer;
	FILE *stream;
	char *curr, *prev;
	size_t remaining;
	uint32_t as_ulong;
	unsigned int saved_options;
	isc_result_t result;

	/*
	 * Get the next token.
	 */

	source = HEAD(lex->sources);
	REQUIRE(tokenp != NULL);

	if (source == NULL) {
		if ((options & ISC_LEXOPT_NOMORE) != 0) {
			tokenp->type = isc_tokentype_nomore;
			return (ISC_R_SUCCESS);
		}
		return (ISC_R_NOMORE);
	}

	if (source->result != ISC_R_SUCCESS)
		return (source->result);

	lex->saved_paren_count = lex->paren_count;
	source->saved_line = source->line;

	if (isc_buffer_remaininglength(source->pushback) == 0 &&
	    source->at_eof)
	{
		if ((options & ISC_LEXOPT_DNSMULTILINE) != 0 &&
		    lex->paren_count != 0) {
			lex->paren_count = 0;
			return (ISC_R_UNBALANCED);
		}
		if ((options & ISC_LEXOPT_EOF) != 0) {
			tokenp->type = isc_tokentype_eof;
			return (ISC_R_SUCCESS);
		}
		return (ISC_R_EOF);
	}

	isc_buffer_compact(source->pushback);

	saved_options = options;
	if ((options & ISC_LEXOPT_DNSMULTILINE) != 0 && lex->paren_count > 0)
		options &= ~IWSEOL;

	curr = lex->data;
	*curr = '\0';

	prev = NULL;
	remaining = lex->max_token;

	if (source->is_file)
		flockfile(source->input);

	do {
		if (isc_buffer_remaininglength(source->pushback) == 0) {
			if (source->is_file) {
				stream = source->input;

				c = getc_unlocked(stream);
				if (c == EOF) {
					if (ferror(stream)) {
						source->result = ISC_R_IOERROR;
						result = source->result;
						goto done;
					}
					source->at_eof = ISC_TRUE;
				}
			} else {
				buffer = source->input;

				if (buffer->current == buffer->used) {
					c = EOF;
					source->at_eof = ISC_TRUE;
				} else {
					c = *((unsigned char *)buffer->base +
					      buffer->current);
					buffer->current++;
				}
			}
			if (c != EOF) {
				source->result = pushandgrow(source, c);
				if (source->result != ISC_R_SUCCESS) {
					result = source->result;
					goto done;
				}
			}
		}

		if (!source->at_eof) {
			if (state == lexstate_start)
				/* Token has not started yet. */
				source->ignored =
				   isc_buffer_consumedlength(source->pushback);
			c = isc_buffer_getuint8(source->pushback);
		} else {
			c = EOF;
		}

		if (c == '\n')
			source->line++;

		if (lex->comment_ok && !no_comments) {
			if (!escaped && c == ';' &&
			    ((lex->comments & ISC_LEXCOMMENT_DNSMASTERFILE)
			     != 0)) {
				saved_state = state;
				state = lexstate_eatline;
				no_comments = ISC_TRUE;
				continue;
			} else if (c == '/' &&
				   (lex->comments &
				    (ISC_LEXCOMMENT_C|
				     ISC_LEXCOMMENT_CPLUSPLUS)) != 0) {
				saved_state = state;
				state = lexstate_maybecomment;
				no_comments = ISC_TRUE;
				continue;
			} else if (c == '#' &&
				   ((lex->comments & ISC_LEXCOMMENT_SHELL)
				    != 0)) {
				saved_state = state;
				state = lexstate_eatline;
				no_comments = ISC_TRUE;
				continue;
			}
		}

	no_read:
		/* INSIST(c == EOF || (c >= 0 && c <= 255)); */
		switch (state) {
		case lexstate_start:
			if (c == EOF) {
				lex->last_was_eol = ISC_FALSE;
				if ((options & ISC_LEXOPT_DNSMULTILINE) != 0 &&
				    lex->paren_count != 0) {
					lex->paren_count = 0;
					result = ISC_R_UNBALANCED;
					goto done;
				}
				if ((options & ISC_LEXOPT_EOF) == 0) {
					result = ISC_R_EOF;
					goto done;
				}
				tokenp->type = isc_tokentype_eof;
				done = ISC_TRUE;
			} else if (c == ' ' || c == '\t') {
				if (lex->last_was_eol &&
				    (options & ISC_LEXOPT_INITIALWS)
				    != 0) {
					lex->last_was_eol = ISC_FALSE;
					tokenp->type = isc_tokentype_initialws;
					tokenp->value.as_char = c;
					done = ISC_TRUE;
				}
			} else if (c == '\n') {
				if ((options & ISC_LEXOPT_EOL) != 0) {
					tokenp->type = isc_tokentype_eol;
					done = ISC_TRUE;
				}
				lex->last_was_eol = ISC_TRUE;
			} else if (c == '\r') {
				if ((options & ISC_LEXOPT_EOL) != 0)
					state = lexstate_crlf;
			} else if (c == '"' &&
				   (options & ISC_LEXOPT_QSTRING) != 0) {
				lex->last_was_eol = ISC_FALSE;
				no_comments = ISC_TRUE;
				state = lexstate_qstring;
			} else if (lex->specials[c]) {
				lex->last_was_eol = ISC_FALSE;
				if ((c == '(' || c == ')') &&
				    (options & ISC_LEXOPT_DNSMULTILINE) != 0) {
					if (c == '(') {
						if (lex->paren_count == 0)
							options &= ~IWSEOL;
						lex->paren_count++;
					} else {
						if (lex->paren_count == 0) {
						    result = ISC_R_UNBALANCED;
						    goto done;
						}
						lex->paren_count--;
						if (lex->paren_count == 0)
							options =
								saved_options;
					}
					continue;
				}
				tokenp->type = isc_tokentype_special;
				tokenp->value.as_char = c;
				done = ISC_TRUE;
			} else if (isdigit((unsigned char)c) &&
				   (options & ISC_LEXOPT_NUMBER) != 0) {
				lex->last_was_eol = ISC_FALSE;
				if ((options & ISC_LEXOPT_OCTAL) != 0 &&
				    (c == '8' || c == '9'))
					state = lexstate_string;
				else
					state = lexstate_number;
				goto no_read;
			} else {
				lex->last_was_eol = ISC_FALSE;
				state = lexstate_string;
				goto no_read;
			}
			break;
		case lexstate_crlf:
			if (c != '\n')
				pushback(source, c);
			tokenp->type = isc_tokentype_eol;
			done = ISC_TRUE;
			lex->last_was_eol = ISC_TRUE;
			break;
		case lexstate_number:
			if (c == EOF || !isdigit((unsigned char)c)) {
				if (c == ' ' || c == '\t' || c == '\r' ||
				    c == '\n' || c == EOF ||
				    lex->specials[c]) {
					int base;
					if ((options & ISC_LEXOPT_OCTAL) != 0)
						base = 8;
					else if ((options & ISC_LEXOPT_CNUMBER) != 0)
						base = 0;
					else
						base = 10;
					pushback(source, c);

					result = isc_parse_uint32(&as_ulong,
								  lex->data,
								  base);
					if (result == ISC_R_SUCCESS) {
						tokenp->type =
							isc_tokentype_number;
						tokenp->value.as_ulong =
							as_ulong;
					} else if (result == ISC_R_BADNUMBER) {
						isc_tokenvalue_t *v;

						tokenp->type =
							isc_tokentype_string;
						v = &(tokenp->value);
						v->as_textregion.base =
							lex->data;
						v->as_textregion.length =
							(unsigned int)
							(lex->max_token -
							 remaining);
					} else
						goto done;
					done = ISC_TRUE;
					continue;
				} else if (!(options & ISC_LEXOPT_CNUMBER) ||
					   ((c != 'x' && c != 'X') ||
					   (curr != &lex->data[1]) ||
					   (lex->data[0] != '0'))) {
					/* Above test supports hex numbers */
					state = lexstate_string;
				}
			} else if ((options & ISC_LEXOPT_OCTAL) != 0 &&
				   (c == '8' || c == '9')) {
				state = lexstate_string;
			}
			if (remaining == 0U) {
				result = grow_data(lex, &remaining,
						   &curr, &prev);
				if (result != ISC_R_SUCCESS)
					goto done;
			}
			INSIST(remaining > 0U);
			*curr++ = c;
			*curr = '\0';
			remaining--;
			break;
		case lexstate_string:
			/*
			 * EOF needs to be checked before lex->specials[c]
			 * as lex->specials[EOF] is not a good idea.
			 */
			if (c == '\r' || c == '\n' || c == EOF ||
			    (!escaped &&
			     (c == ' ' || c == '\t' || lex->specials[c]))) {
				pushback(source, c);
				if (source->result != ISC_R_SUCCESS) {
					result = source->result;
					goto done;
				}
				tokenp->type = isc_tokentype_string;
				tokenp->value.as_textregion.base = lex->data;
				tokenp->value.as_textregion.length =
					(unsigned int)
					(lex->max_token - remaining);
				done = ISC_TRUE;
				continue;
			}
			if ((options & ISC_LEXOPT_ESCAPE) != 0)
				escaped = (!escaped && c == '\\') ?
						ISC_TRUE : ISC_FALSE;
			if (remaining == 0U) {
				result = grow_data(lex, &remaining,
						   &curr, &prev);
				if (result != ISC_R_SUCCESS)
					goto done;
			}
			INSIST(remaining > 0U);
			*curr++ = c;
			*curr = '\0';
			remaining--;
			break;
		case lexstate_maybecomment:
			if (c == '*' &&
			    (lex->comments & ISC_LEXCOMMENT_C) != 0) {
				state = lexstate_ccomment;
				continue;
			} else if (c == '/' &&
			    (lex->comments & ISC_LEXCOMMENT_CPLUSPLUS) != 0) {
				state = lexstate_eatline;
				continue;
			}
			pushback(source, c);
			c = '/';
			no_comments = ISC_FALSE;
			state = saved_state;
			goto no_read;
		case lexstate_ccomment:
			if (c == EOF) {
				result = ISC_R_UNEXPECTEDEND;
				goto done;
			}
			if (c == '*')
				state = lexstate_ccommentend;
			break;
		case lexstate_ccommentend:
			if (c == EOF) {
				result = ISC_R_UNEXPECTEDEND;
				goto done;
			}
			if (c == '/') {
				/*
				 * C-style comments become a single space.
				 * We do this to ensure that a comment will
				 * act as a delimiter for strings and
				 * numbers.
				 */
				c = ' ';
				no_comments = ISC_FALSE;
				state = saved_state;
				goto no_read;
			} else if (c != '*')
				state = lexstate_ccomment;
			break;
		case lexstate_eatline:
			if ((c == '\n') || (c == EOF)) {
				no_comments = ISC_FALSE;
				state = saved_state;
				goto no_read;
			}
			break;
		case lexstate_qstring:
			if (c == EOF) {
				result = ISC_R_UNEXPECTEDEND;
				goto done;
			}
			if (c == '"') {
				if (escaped) {
					escaped = ISC_FALSE;
					/*
					 * Overwrite the preceding backslash.
					 */
					INSIST(prev != NULL);
					*prev = '"';
				} else {
					tokenp->type = isc_tokentype_qstring;
					tokenp->value.as_textregion.base =
						lex->data;
					tokenp->value.as_textregion.length =
						(unsigned int)
						(lex->max_token - remaining);
					no_comments = ISC_FALSE;
					done = ISC_TRUE;
				}
			} else {
				if (c == '\n' && !escaped &&
			    (options & ISC_LEXOPT_QSTRINGMULTILINE) == 0) {
					pushback(source, c);
					result = ISC_R_UNBALANCEDQUOTES;
					goto done;
				}
				if (c == '\\' && !escaped)
					escaped = ISC_TRUE;
				else
					escaped = ISC_FALSE;
				if (remaining == 0U) {
					result = grow_data(lex, &remaining,
							   &curr, &prev);
					if (result != ISC_R_SUCCESS)
						goto done;
				}
				INSIST(remaining > 0U);
				prev = curr;
				*curr++ = c;
				*curr = '\0';
				remaining--;
			}
			break;
		default:
			FATAL_ERROR(__FILE__, __LINE__, "Unexpected state %d",
				    state);
			/* Does not return. */
		}

	} while (!done);

	result = ISC_R_SUCCESS;
 done:
	if (source->is_file)
		funlockfile(source->input);
	return (result);
}

void
isc_lex_ungettoken(isc_lex_t *lex, isc_token_t *tokenp) {
	inputsource *source;
	/*
	 * Unget the current token.
	 */

	source = HEAD(lex->sources);
	REQUIRE(source != NULL);
	REQUIRE(tokenp != NULL);
	REQUIRE(isc_buffer_consumedlength(source->pushback) != 0 ||
		tokenp->type == isc_tokentype_eof);

	UNUSED(tokenp);

	isc_buffer_first(source->pushback);
	lex->paren_count = lex->saved_paren_count;
	source->line = source->saved_line;
	source->at_eof = ISC_FALSE;
}

void
isc_lex_getlasttokentext(isc_lex_t *lex, isc_token_t *tokenp, isc_region_t *r)
{
	inputsource *source;

	source = HEAD(lex->sources);
	REQUIRE(source != NULL);
	REQUIRE(tokenp != NULL);
	REQUIRE(isc_buffer_consumedlength(source->pushback) != 0 ||
		tokenp->type == isc_tokentype_eof);

	UNUSED(tokenp);

	INSIST(source->ignored <= isc_buffer_consumedlength(source->pushback));
	r->base = (unsigned char *)isc_buffer_base(source->pushback) +
		  source->ignored;
	r->length = isc_buffer_consumedlength(source->pushback) -
		    source->ignored;
}

char *
isc_lex_getsourcename(isc_lex_t *lex) {
	inputsource *source;

	source = HEAD(lex->sources);

	if (source == NULL)
		return (NULL);

	return (source->name);
}

unsigned long
isc_lex_getsourceline(isc_lex_t *lex) {
	inputsource *source;

	source = HEAD(lex->sources);

	if (source == NULL)
		return (0);

	return (source->line);
}
