/*
 * (c) Thomas Pornin 1999 - 2002
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 4. The name of the authors may not be used to endorse or promote
 *    products derived from this software without specific prior written
 *    permission.
 *
 * THIS SOFTWARE IS PROVIDED ``AS IS'' AND WITHOUT ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL THE AUTHORS OR CONTRIBUTORS BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT
 * OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR
 * BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
 * WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE
 * OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE,
 * EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 */

#include "tune.h"
#include <stdio.h>
#include <string.h>
#include <stddef.h>
#include <limits.h>
#include <time.h>
#include "ucppi.h"
#include "mem.h"
#include "nhash.h"

/*
 * Assertion support. Each assertion is indexed by its predicate, and
 * the list of 'questions' which yield a true answer.
 */

static HTT assertions;
static int assertions_init_done = 0;

static struct assert *new_assertion(void)
{
	struct assert *a = getmem(sizeof(struct assert));

	a->nbval = 0;
	return a;
}

static void del_token_fifo(struct token_fifo *tf)
{
	size_t i;

	for (i = 0; i < tf->nt; i ++)
		if (S_TOKEN(tf->t[i].type)) freemem(tf->t[i].name);
	if (tf->nt) freemem(tf->t);
}

static void del_assertion(void *va)
{
	struct assert *a = va;
	size_t i;

	for (i = 0; i < a->nbval; i ++) del_token_fifo(a->val + i);
	if (a->nbval) freemem(a->val);
	freemem(a);
}

/*
 * print the contents of a token list
 */
static void print_token_fifo(struct token_fifo *tf)
{
	size_t i;

	for (i = 0; i < tf->nt; i ++)
		if (ttMWS(tf->t[i].type)) fputc(' ', emit_output);
		else fputs(token_name(tf->t + i), emit_output);
}

/*
 * print all assertions related to a given name
 */
static void print_assert(void *va)
{
	struct assert *a = va;
	size_t i;

	for (i = 0; i < a->nbval; i ++) {
		fprintf(emit_output, "#assert %s(", HASH_ITEM_NAME(a));
		print_token_fifo(a->val + i);
		fprintf(emit_output, ")\n");
	}
}

/*
 * compare two token_fifo, return 0 if they are identical, 1 otherwise.
 * All whitespace tokens are considered identical, but sequences of
 * whitespace are not shrinked.
 */
int cmp_token_list(struct token_fifo *f1, struct token_fifo *f2)
{
	size_t i;

	if (f1->nt != f2->nt) return 1;
	for (i = 0; i < f1->nt; i ++) {
		if (ttMWS(f1->t[i].type) && ttMWS(f2->t[i].type)) continue;
		if (f1->t[i].type != f2->t[i].type) return 1;
		if (f1->t[i].type == MACROARG
			&& f1->t[i].line != f2->t[i].line) return 1;
		if (S_TOKEN(f1->t[i].type)
			&& strcmp(f1->t[i].name, f2->t[i].name)) return 1;
	}
	return 0;
}

/*
 * for #assert
 * Assertions are not part of the ISO-C89 standard, but they are sometimes
 * encountered, for instance in Solaris standard include files.
 */
int handle_assert(struct lexer_state *ls)
{
	int ina = 0, ltww;
	struct token t;
	struct token_fifo *atl = 0;
	struct assert *a;
	char *aname;
	int ret = -1;
	long l = ls->line;
	int nnp;
	size_t i;

	while (!next_token(ls)) {
		if (ls->ctok->type == NEWLINE) break;
		if (ttMWS(ls->ctok->type)) continue;
		if (ls->ctok->type == NAME) {
			if (!(a = HTT_get(&assertions, ls->ctok->name))) {
				a = new_assertion();
				aname = sdup(ls->ctok->name);
				ina = 1;
			}
			goto handle_assert_next;
		}
		error(l, "illegal assertion name for #assert");
		goto handle_assert_warp_ign;
	}
	goto handle_assert_trunc;

handle_assert_next:
	while (!next_token(ls)) {
		if (ls->ctok->type == NEWLINE) break;
		if (ttMWS(ls->ctok->type)) continue;
		if (ls->ctok->type != LPAR) {
			error(l, "syntax error in #assert");
			goto handle_assert_warp_ign;
		}
		goto handle_assert_next2;
	}
	goto handle_assert_trunc;

handle_assert_next2:
	atl = getmem(sizeof(struct token_fifo));
	atl->art = atl->nt = 0;
	for (nnp = 1, ltww = 1; nnp && !next_token(ls);) {
		if (ls->ctok->type == NEWLINE) break;
		if (ltww && ttMWS(ls->ctok->type)) continue;
		ltww = ttMWS(ls->ctok->type);
		if (ls->ctok->type == LPAR) nnp ++;
		else if (ls->ctok->type == RPAR) {
			if (!(-- nnp)) goto handle_assert_next3;
		}
		t.type = ls->ctok->type;
		if (S_TOKEN(t.type)) t.name = sdup(ls->ctok->name);
		aol(atl->t, atl->nt, t, TOKEN_LIST_MEMG);
	}
	goto handle_assert_trunc;

handle_assert_next3:
	while (!next_token(ls) && ls->ctok->type != NEWLINE) {
		if (!ttWHI(ls->ctok->type) && (ls->flags & WARN_STANDARD)) {
			warning(l, "trailing garbage in #assert");
		}
	}
	if (atl->nt && ttMWS(atl->t[atl->nt - 1].type) && (-- atl->nt) == 0)
		freemem(atl->t);
	if (atl->nt == 0) {
		error(l, "void assertion in #assert");
		goto handle_assert_error;
	}
	for (i = 0; i < a->nbval && cmp_token_list(atl, a->val + i); i ++);
	if (i != a->nbval) {
		/* we already have it */
		ret = 0;
		goto handle_assert_error;
	}

	/* This is a new assertion. Let's keep it. */
	aol(a->val, a->nbval, *atl, TOKEN_LIST_MEMG);
	if (ina) {
		HTT_put(&assertions, a, aname);
		freemem(aname);
	}
	if (emit_assertions) {
		fprintf(emit_output, "#assert %s(", HASH_ITEM_NAME(a));
		print_token_fifo(atl);
		fputs(")\n", emit_output);
	}
	freemem(atl);
	return 0;

handle_assert_trunc:
	error(l, "unfinished #assert");
handle_assert_error:
	if (atl) {
		del_token_fifo(atl);
		freemem(atl);
	}
	if (ina) {
		freemem(aname);
		freemem(a);
	}
	return ret;
handle_assert_warp_ign:
	while (!next_token(ls) && ls->ctok->type != NEWLINE);
	if (ina) {
		freemem(aname);
		freemem(a);
	}
	return ret;
}

/*
 * for #unassert
 */
int handle_unassert(struct lexer_state *ls)
{
	int ltww;
	struct token t;
	struct token_fifo atl;
	struct assert *a;
	int ret = -1;
	long l = ls->line;
	int nnp;
	size_t i;

	atl.art = atl.nt = 0;
	while (!next_token(ls)) {
		if (ls->ctok->type == NEWLINE) break;
		if (ttMWS(ls->ctok->type)) continue;
		if (ls->ctok->type == NAME) {
			if (!(a = HTT_get(&assertions, ls->ctok->name))) {
				ret = 0;
				goto handle_unassert_warp;
			}
			goto handle_unassert_next;
		}
		error(l, "illegal assertion name for #unassert");
		goto handle_unassert_warp;
	}
	goto handle_unassert_trunc;

handle_unassert_next:
	while (!next_token(ls)) {
		if (ls->ctok->type == NEWLINE) break;
		if (ttMWS(ls->ctok->type)) continue;
		if (ls->ctok->type != LPAR) {
			error(l, "syntax error in #unassert");
			goto handle_unassert_warp;
		}
		goto handle_unassert_next2;
	}
	if (emit_assertions)
		fprintf(emit_output, "#unassert %s\n", HASH_ITEM_NAME(a));
	HTT_del(&assertions, HASH_ITEM_NAME(a));
	return 0;

handle_unassert_next2:
	for (nnp = 1, ltww = 1; nnp && !next_token(ls);) {
		if (ls->ctok->type == NEWLINE) break;
		if (ltww && ttMWS(ls->ctok->type)) continue;
		ltww = ttMWS(ls->ctok->type);
		if (ls->ctok->type == LPAR) nnp ++;
		else if (ls->ctok->type == RPAR) {
			if (!(-- nnp)) goto handle_unassert_next3;
		}
		t.type = ls->ctok->type;
		if (S_TOKEN(t.type)) t.name = sdup(ls->ctok->name);
		aol(atl.t, atl.nt, t, TOKEN_LIST_MEMG);
	}
	goto handle_unassert_trunc;

handle_unassert_next3:
	while (!next_token(ls) && ls->ctok->type != NEWLINE) {
		if (!ttWHI(ls->ctok->type) && (ls->flags & WARN_STANDARD)) {
			warning(l, "trailing garbage in #unassert");
		}
	}
	if (atl.nt && ttMWS(atl.t[atl.nt - 1].type) && (-- atl.nt) == 0)
		freemem(atl.t);
	if (atl.nt == 0) {
		error(l, "void assertion in #unassert");
		return ret;
	}
	for (i = 0; i < a->nbval && cmp_token_list(&atl, a->val + i); i ++);
	if (i != a->nbval) {
		/* we have it, undefine it */
		del_token_fifo(a->val + i);
		if (i < (a->nbval - 1))
			mmvwo(a->val + i, a->val + i + 1, (a->nbval - i - 1)
				* sizeof(struct token_fifo));
		if ((-- a->nbval) == 0) freemem(a->val);
		if (emit_assertions) {
			fprintf(emit_output, "#unassert %s(",
				HASH_ITEM_NAME(a));
			print_token_fifo(&atl);
			fputs(")\n", emit_output);
		}
	}
	ret = 0;
	goto handle_unassert_finish;

handle_unassert_trunc:
	error(l, "unfinished #unassert");
handle_unassert_finish:
	if (atl.nt) del_token_fifo(&atl);
	return ret;
handle_unassert_warp:
	while (!next_token(ls) && ls->ctok->type != NEWLINE);
	return ret;
}

/*
 * Add the given assertion (as string).
 */
int make_assertion(char *aval)
{
	struct lexer_state lls;
	size_t n = strlen(aval) + 1;
	char *c = sdup(aval);
	int ret;

	*(c + n - 1) = '\n';
	init_buf_lexer_state(&lls, 0);
	lls.flags = DEFAULT_LEXER_FLAGS;
	lls.input = 0;
	lls.input_string = (unsigned char *)c;
	lls.pbuf = 0;
	lls.ebuf = n;
	lls.line = -1;
	ret = handle_assert(&lls);
	freemem(c);
	free_lexer_state(&lls);
	return ret;
}

/*
 * Remove the given assertion (as string).
 */
int destroy_assertion(char *aval)
{
	struct lexer_state lls;
	size_t n = strlen(aval) + 1;
	char *c = sdup(aval);
	int ret;

	*(c + n - 1) = '\n';
	init_buf_lexer_state(&lls, 0);
	lls.flags = DEFAULT_LEXER_FLAGS;
	lls.input = 0;
	lls.input_string = (unsigned char *)c;
	lls.pbuf = 0;
	lls.ebuf = n;
	lls.line = -1;
	ret = handle_unassert(&lls);
	freemem(c);
	free_lexer_state(&lls);
	return ret;
}

/*
 * erase the assertion table
 */
void wipe_assertions(void)
{
	if (assertions_init_done) HTT_kill(&assertions);
	assertions_init_done = 0;
}

/*
 * initialize the assertion table
 */
void init_assertions(void)
{
	wipe_assertions();
	HTT_init(&assertions, del_assertion);
	assertions_init_done = 1;
}

/*
 * retrieve an assertion from the hash table
 */
struct assert *get_assertion(char *name)
{
	return HTT_get(&assertions, name);
}

/*
 * print already defined assertions
 */
void print_assertions(void)
{
	HTT_scan(&assertions, print_assert);
}
