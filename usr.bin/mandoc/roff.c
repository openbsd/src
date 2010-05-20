/*	$Id: roff.c,v 1.2 2010/05/20 00:58:02 schwarze Exp $ */
/*
 * Copyright (c) 2010 Kristaps Dzonsons <kristaps@bsd.lv>
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
#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <assert.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

#include "mandoc.h"
#include "roff.h"

#define	RSTACK_MAX	128

#define	ROFF_CTL(c) \
	('.' == (c) || '\'' == (c))

enum	rofft {
	ROFF_am,
	ROFF_ami,
	ROFF_am1,
	ROFF_de,
	ROFF_dei,
	ROFF_de1,
	ROFF_ds,
	ROFF_el,
	ROFF_ie,
	ROFF_if,
	ROFF_ig,
	ROFF_rm,
	ROFF_tr,
	ROFF_cblock,
	ROFF_ccond,
	ROFF_MAX
};

enum	roffrule {
	ROFFRULE_ALLOW,
	ROFFRULE_DENY
};

struct	roff {
	struct roffnode	*last; /* leaf of stack */
	mandocmsg	 msg; /* err/warn/fatal messages */
	void		*data; /* privdata for messages */
	enum roffrule	 rstack[RSTACK_MAX]; /* stack of !`ie' rules */
	int		 rstackpos; /* position in rstack */
};

struct	roffnode {
	enum rofft	 tok; /* type of node */
	struct roffnode	*parent; /* up one in stack */
	int		 line; /* parse line */
	int		 col; /* parse col */
	char		*end; /* end-rules: custom token */
	int		 endspan; /* end-rules: next-line or infty */
	enum roffrule	 rule; /* current evaluation rule */
};

#define	ROFF_ARGS	 struct roff *r, /* parse ctx */ \
			 enum rofft tok, /* tok of macro */ \
		 	 char **bufp, /* input buffer */ \
			 size_t *szp, /* size of input buffer */ \
			 int ln, /* parse line */ \
			 int ppos, /* original pos in buffer */ \
			 int pos, /* current pos in buffer */ \
			 int *offs /* reset offset of buffer data */

typedef	enum rofferr (*roffproc)(ROFF_ARGS);

struct	roffmac {
	const char	*name; /* macro name */
	roffproc	 proc; /* process new macro */
	roffproc	 text; /* process as child text of macro */
	roffproc	 sub; /* process as child of macro */
	int		 flags;
#define	ROFFMAC_STRUCT	(1 << 0) /* always interpret */
};

static	enum rofferr	 roff_block(ROFF_ARGS);
static	enum rofferr	 roff_block_text(ROFF_ARGS);
static	enum rofferr	 roff_block_sub(ROFF_ARGS);
static	enum rofferr	 roff_cblock(ROFF_ARGS);
static	enum rofferr	 roff_ccond(ROFF_ARGS);
static	enum rofferr	 roff_cond(ROFF_ARGS);
static	enum rofferr	 roff_cond_text(ROFF_ARGS);
static	enum rofferr	 roff_cond_sub(ROFF_ARGS);
static	enum rofferr	 roff_line(ROFF_ARGS);

const	struct roffmac	 roffs[ROFF_MAX] = {
	{ "am", roff_block, roff_block_text, roff_block_sub, 0 },
	{ "ami", roff_block, roff_block_text, roff_block_sub, 0 },
	{ "am1", roff_block, roff_block_text, roff_block_sub, 0 },
	{ "de", roff_block, roff_block_text, roff_block_sub, 0 },
	{ "dei", roff_block, roff_block_text, roff_block_sub, 0 },
	{ "de1", roff_block, roff_block_text, roff_block_sub, 0 },
	{ "ds", roff_line, NULL, NULL, 0 },
	{ "el", roff_cond, roff_cond_text, roff_cond_sub, ROFFMAC_STRUCT },
	{ "ie", roff_cond, roff_cond_text, roff_cond_sub, ROFFMAC_STRUCT },
	{ "if", roff_cond, roff_cond_text, roff_cond_sub, ROFFMAC_STRUCT },
	{ "ig", roff_block, roff_block_text, roff_block_sub, 0 },
	{ "rm", roff_line, NULL, NULL, 0 },
	{ "tr", roff_line, NULL, NULL, 0 },
	{ ".", roff_cblock, NULL, NULL, 0 },
	{ "\\}", roff_ccond, NULL, NULL, 0 },
};

static	void		 roff_free1(struct roff *);
static	enum rofft	 roff_hash_find(const char *);
static	void		 roffnode_cleanscope(struct roff *);
static	int		 roffnode_push(struct roff *, 
				enum rofft, int, int);
static	void		 roffnode_pop(struct roff *);
static	enum rofft	 roff_parse(const char *, int *);


/*
 * Look up a roff token by its name.  Returns ROFF_MAX if no macro by
 * the nil-terminated string name could be found.
 */
static enum rofft
roff_hash_find(const char *p)
{
	int		 i;

	/* FIXME: make this be fast and efficient. */

	for (i = 0; i < (int)ROFF_MAX; i++)
		if (0 == strcmp(roffs[i].name, p))
			return((enum rofft)i);

	return(ROFF_MAX);
}


/*
 * Pop the current node off of the stack of roff instructions currently
 * pending.
 */
static void
roffnode_pop(struct roff *r)
{
	struct roffnode	*p;

	assert(r->last);
	p = r->last; 

	if (ROFF_el == p->tok)
		if (r->rstackpos > -1)
			r->rstackpos--;

	r->last = r->last->parent;
	if (p->end)
		free(p->end);
	free(p);
}


/*
 * Push a roff node onto the instruction stack.  This must later be
 * removed with roffnode_pop().
 */
static int
roffnode_push(struct roff *r, enum rofft tok, int line, int col)
{
	struct roffnode	*p;

	if (NULL == (p = calloc(1, sizeof(struct roffnode)))) {
		(*r->msg)(MANDOCERR_MEM, r->data, line, col, NULL);
		return(0);
	}

	p->tok = tok;
	p->parent = r->last;
	p->line = line;
	p->col = col;
	p->rule = p->parent ? p->parent->rule : ROFFRULE_DENY;

	r->last = p;
	return(1);
}


static void
roff_free1(struct roff *r)
{

	while (r->last)
		roffnode_pop(r);
}


void
roff_reset(struct roff *r)
{

	roff_free1(r);
}


void
roff_free(struct roff *r)
{

	roff_free1(r);
	free(r);
}


struct roff *
roff_alloc(const mandocmsg msg, void *data)
{
	struct roff	*r;

	if (NULL == (r = calloc(1, sizeof(struct roff)))) {
		(*msg)(MANDOCERR_MEM, data, 0, 0, NULL);
		return(0);
	}

	r->msg = msg;
	r->data = data;
	r->rstackpos = -1;
	return(r);
}


enum rofferr
roff_parseln(struct roff *r, int ln, 
		char **bufp, size_t *szp, int pos, int *offs)
{
	enum rofft	 t;
	int		 ppos;

	/*
	 * First, if a scope is open and we're not a macro, pass the
	 * text through the macro's filter.  If a scope isn't open and
	 * we're not a macro, just let it through.
	 */

	if (r->last && ! ROFF_CTL((*bufp)[pos])) {
		t = r->last->tok;
		assert(roffs[t].text);
		return((*roffs[t].text)
				(r, t, bufp, szp, ln, pos, pos, offs));
	} else if ( ! ROFF_CTL((*bufp)[pos]))
		return(ROFF_CONT);

	/*
	 * If a scope is open, go to the child handler for that macro,
	 * as it may want to preprocess before doing anything with it.
	 */

	if (r->last) {
		t = r->last->tok;
		assert(roffs[t].sub);
		return((*roffs[t].sub)
				(r, t, bufp, szp, ln, pos, pos, offs));
	}

	/*
	 * Lastly, as we've no scope open, try to look up and execute
	 * the new macro.  If no macro is found, simply return and let
	 * the compilers handle it.
	 */

	ppos = pos;
	if (ROFF_MAX == (t = roff_parse(*bufp, &pos)))
		return(ROFF_CONT);

	assert(roffs[t].proc);
	return((*roffs[t].proc)
			(r, t, bufp, szp, ln, ppos, pos, offs));
}


int
roff_endparse(struct roff *r)
{

	if (NULL == r->last)
		return(1);
	return((*r->msg)(MANDOCERR_SCOPEEXIT, r->data, r->last->line, 
				r->last->col, NULL));
}


/*
 * Parse a roff node's type from the input buffer.  This must be in the
 * form of ".foo xxx" in the usual way.
 */
static enum rofft
roff_parse(const char *buf, int *pos)
{
	int		 j;
	char		 mac[5];
	enum rofft	 t;

	assert(ROFF_CTL(buf[*pos]));
	(*pos)++;

	while (buf[*pos] && (' ' == buf[*pos] || '\t' == buf[*pos]))
		(*pos)++;

	if ('\0' == buf[*pos])
		return(ROFF_MAX);

	for (j = 0; j < 4; j++, (*pos)++)
		if ('\0' == (mac[j] = buf[*pos]))
			break;
		else if (' ' == buf[*pos] || (j && '\\' == buf[*pos]))
			break;

	if (j == 4 || j < 1)
		return(ROFF_MAX);

	mac[j] = '\0';

	if (ROFF_MAX == (t = roff_hash_find(mac)))
		return(t);

	while (buf[*pos] && ' ' == buf[*pos])
		(*pos)++;

	return(t);
}


/* ARGSUSED */
static enum rofferr
roff_cblock(ROFF_ARGS)
{

	/*
	 * A block-close `..' should only be invoked as a child of an
	 * ignore macro, otherwise raise a warning and just ignore it.
	 */

	if (NULL == r->last) {
		if ( ! (*r->msg)(MANDOCERR_NOSCOPE, r->data, ln, ppos, NULL))
			return(ROFF_ERR);
		return(ROFF_IGN);
	}

	switch (r->last->tok) {
	case (ROFF_am):
		/* FALLTHROUGH */
	case (ROFF_ami):
		/* FALLTHROUGH */
	case (ROFF_am1):
		/* FALLTHROUGH */
	case (ROFF_de):
		/* FALLTHROUGH */
	case (ROFF_dei):
		/* FALLTHROUGH */
	case (ROFF_de1):
		/* FALLTHROUGH */
	case (ROFF_ig):
		break;
	default:
		if ( ! (*r->msg)(MANDOCERR_NOSCOPE, r->data, ln, ppos, NULL))
			return(ROFF_ERR);
		return(ROFF_IGN);
	}

	if ((*bufp)[pos])
		if ( ! (*r->msg)(MANDOCERR_ARGSLOST, r->data, ln, pos, NULL))
			return(ROFF_ERR);

	roffnode_pop(r);
	roffnode_cleanscope(r);
	return(ROFF_IGN);

}


static void
roffnode_cleanscope(struct roff *r)
{

	while (r->last) {
		if (--r->last->endspan < 0)
			break;
		roffnode_pop(r);
	}
}


/* ARGSUSED */
static enum rofferr
roff_ccond(ROFF_ARGS)
{

	if (NULL == r->last) {
		if ( ! (*r->msg)(MANDOCERR_NOSCOPE, r->data, ln, ppos, NULL))
			return(ROFF_ERR);
		return(ROFF_IGN);
	}

	switch (r->last->tok) {
	case (ROFF_el):
		/* FALLTHROUGH */
	case (ROFF_ie):
		/* FALLTHROUGH */
	case (ROFF_if):
		break;
	default:
		if ( ! (*r->msg)(MANDOCERR_NOSCOPE, r->data, ln, ppos, NULL))
			return(ROFF_ERR);
		return(ROFF_IGN);
	}

	if (r->last->endspan > -1) {
		if ( ! (*r->msg)(MANDOCERR_NOSCOPE, r->data, ln, ppos, NULL))
			return(ROFF_ERR);
		return(ROFF_IGN);
	}

	if ((*bufp)[pos])
		if ( ! (*r->msg)(MANDOCERR_ARGSLOST, r->data, ln, pos, NULL))
			return(ROFF_ERR);

	roffnode_pop(r);
	roffnode_cleanscope(r);
	return(ROFF_IGN);
}


/* ARGSUSED */
static enum rofferr
roff_block(ROFF_ARGS)
{
	int		sv;
	size_t		sz;

	if (ROFF_ig != tok && '\0' == (*bufp)[pos]) {
		if ( ! (*r->msg)(MANDOCERR_NOARGS, r->data, ln, ppos, NULL))
			return(ROFF_ERR);
		return(ROFF_IGN);
	} else if (ROFF_ig != tok) {
		while ((*bufp)[pos] && ' ' != (*bufp)[pos])
			pos++;
		while (' ' == (*bufp)[pos])
			pos++;
	}

	if ( ! roffnode_push(r, tok, ln, ppos))
		return(ROFF_ERR);

	if ('\0' == (*bufp)[pos])
		return(ROFF_IGN);

	sv = pos;
	while ((*bufp)[pos] && ' ' != (*bufp)[pos] && 
			'\t' != (*bufp)[pos])
		pos++;

	/*
	 * Note: groff does NOT like escape characters in the input.
	 * Instead of detecting this, we're just going to let it fly and
	 * to hell with it.
	 */

	assert(pos > sv);
	sz = (size_t)(pos - sv);

	if (1 == sz && '.' == (*bufp)[sv])
		return(ROFF_IGN);

	r->last->end = malloc(sz + 1);

	if (NULL == r->last->end) {
		(*r->msg)(MANDOCERR_MEM, r->data, ln, pos, NULL);
		return(ROFF_ERR);
	}

	memcpy(r->last->end, *bufp + sv, sz);
	r->last->end[(int)sz] = '\0';

	if ((*bufp)[pos])
		if ( ! (*r->msg)(MANDOCERR_ARGSLOST, r->data, ln, pos, NULL))
			return(ROFF_ERR);

	return(ROFF_IGN);
}


/* ARGSUSED */
static enum rofferr
roff_block_sub(ROFF_ARGS)
{
	enum rofft	t;
	int		i, j;

	/*
	 * First check whether a custom macro exists at this level.  If
	 * it does, then check against it.  This is some of groff's
	 * stranger behaviours.  If we encountered a custom end-scope
	 * tag and that tag also happens to be a "real" macro, then we
	 * need to try interpreting it again as a real macro.  If it's
	 * not, then return ignore.  Else continue.
	 */

	if (r->last->end) {
		i = pos + 1;
		while (' ' == (*bufp)[i] || '\t' == (*bufp)[i])
			i++;

		for (j = 0; r->last->end[j]; j++, i++)
			if ((*bufp)[i] != r->last->end[j])
				break;

		if ('\0' == r->last->end[j] && 
				('\0' == (*bufp)[i] ||
				 ' ' == (*bufp)[i] ||
				 '\t' == (*bufp)[i])) {
			roffnode_pop(r);
			roffnode_cleanscope(r);

			if (ROFF_MAX != roff_parse(*bufp, &pos))
				return(ROFF_RERUN);
			return(ROFF_IGN);
		}
	}

	/*
	 * If we have no custom end-query or lookup failed, then try
	 * pulling it out of the hashtable.
	 */

	ppos = pos;
	t = roff_parse(*bufp, &pos);

	/* If we're not a comment-end, then throw it away. */
	if (ROFF_cblock != t)
		return(ROFF_IGN);

	assert(roffs[t].proc);
	return((*roffs[t].proc)(r, t, bufp, 
			szp, ln, ppos, pos, offs));
}


/* ARGSUSED */
static enum rofferr
roff_block_text(ROFF_ARGS)
{

	return(ROFF_IGN);
}


/* ARGSUSED */
static enum rofferr
roff_cond_sub(ROFF_ARGS)
{
	enum rofft	 t;
	enum roffrule	 rr;

	ppos = pos;
	rr = r->last->rule;

	roff_cond_text(r, tok, bufp, szp, ln, ppos, pos, offs);

	if (ROFF_MAX == (t = roff_parse(*bufp, &pos)))
		return(ROFFRULE_DENY == rr ? ROFF_IGN : ROFF_CONT);

	/*
	 * A denied conditional must evaluate its children if and only
	 * if they're either structurally required (such as loops and
	 * conditionals) or a closing macro.
	 */
	if (ROFFRULE_DENY == rr)
		if ( ! (ROFFMAC_STRUCT & roffs[t].flags))
			if (ROFF_ccond != t)
				return(ROFF_IGN);

	assert(roffs[t].proc);
	return((*roffs[t].proc)
			(r, t, bufp, szp, ln, ppos, pos, offs));
}


/* ARGSUSED */
static enum rofferr
roff_cond_text(ROFF_ARGS)
{
	char		*ep, *st;
	enum roffrule	 rr;

	rr = r->last->rule;

	/*
	 * We display the value of the text if out current evaluation
	 * scope permits us to do so.
	 */

	st = &(*bufp)[pos];
	if (NULL == (ep = strstr(st, "\\}"))) {
		roffnode_cleanscope(r);
		return(ROFFRULE_DENY == rr ? ROFF_IGN : ROFF_CONT);
	}

	if (ep > st && '\\' != *(ep - 1)) {
		ep = '\0';
		roffnode_pop(r);
	}

	roffnode_cleanscope(r);
	return(ROFFRULE_DENY == rr ? ROFF_IGN : ROFF_CONT);
}


/* ARGSUSED */
static enum rofferr
roff_cond(ROFF_ARGS)
{
	int		 cpos;  /* position of the condition */
	int		 sv;

	/* Stack overflow! */

	if (ROFF_ie == tok && r->rstackpos == RSTACK_MAX - 1) {
		(*r->msg)(MANDOCERR_MEM, r->data, ln, ppos, NULL);
		return(ROFF_ERR);
	}

	cpos = pos;

	if (ROFF_if == tok || ROFF_ie == tok) {
		/*
		 * Read ahead past the conditional.  FIXME: this does
		 * not work, as conditionals don't end on whitespace,
		 * but are parsed according to a formal grammar.  It's
		 * good enough for now, however.
		 */
		while ((*bufp)[pos] && ' ' != (*bufp)[pos])
			pos++;
	}

	sv = pos;
	while (' ' == (*bufp)[pos])
		pos++;

	/*
	 * Roff is weird.  If we have just white-space after the
	 * conditional, it's considered the BODY and we exit without
	 * really doing anything.  Warn about this.  It's probably
	 * wrong.
	 */
	if ('\0' == (*bufp)[pos] && sv != pos) {
		if ( ! (*r->msg)(MANDOCERR_NOARGS, r->data, ln, ppos, NULL))
			return(ROFF_ERR);
		return(ROFF_IGN);
	}

	if ( ! roffnode_push(r, tok, ln, ppos))
		return(ROFF_ERR);

	/* XXX: Implement more conditionals. */

	if (ROFF_if == tok || ROFF_ie == tok)
		r->last->rule = 'n' == (*bufp)[cpos] ?
		    ROFFRULE_ALLOW : ROFFRULE_DENY;
	else if (ROFF_el == tok) {
		/* 
		 * An `.el' will get the value of the current rstack
		 * entry set in prior `ie' calls or defaults to DENY.
	 	 */
		if (r->rstackpos < 0)
			r->last->rule = ROFFRULE_DENY;
		else
			r->last->rule = r->rstack[r->rstackpos];
	}
	if (ROFF_ie == tok) {
		/*
		 * An if-else will put the NEGATION of the current
		 * evaluated conditional into the stack.
		 */
		r->rstackpos++;
		if (ROFFRULE_DENY == r->last->rule)
			r->rstack[r->rstackpos] = ROFFRULE_ALLOW;
		else
			r->rstack[r->rstackpos] = ROFFRULE_DENY;
	}
	if (r->last->parent && ROFFRULE_DENY == r->last->parent->rule)
		r->last->rule = ROFFRULE_DENY;

	r->last->endspan = 1;

	if ('\\' == (*bufp)[pos] && '{' == (*bufp)[pos + 1]) {
		r->last->endspan = -1;
		pos += 2;
	} 

	/*
	 * If there are no arguments on the line, the next-line scope is
	 * assumed.
	 */

	if ('\0' == (*bufp)[pos])
		return(ROFF_IGN);

	/* Otherwise re-run the roff parser after recalculating. */

	*offs = pos;
	return(ROFF_RERUN);
}


/* ARGSUSED */
static enum rofferr
roff_line(ROFF_ARGS)
{

	return(ROFF_IGN);
}
