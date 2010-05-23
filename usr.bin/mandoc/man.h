/*	$Id: man.h,v 1.22 2010/05/23 22:45:00 schwarze Exp $ */
/*
 * Copyright (c) 2009 Kristaps Dzonsons <kristaps@kth.se>
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
#ifndef MAN_H
#define MAN_H

#include <time.h>

enum	mant {
	MAN_br = 0,
	MAN_TH,
	MAN_SH,
	MAN_SS,
	MAN_TP,
	MAN_LP,
	MAN_PP,
	MAN_P,
	MAN_IP,
	MAN_HP,
	MAN_SM,
	MAN_SB,
	MAN_BI,
	MAN_IB,
	MAN_BR,
	MAN_RB,
	MAN_R,
	MAN_B,
	MAN_I,
	MAN_IR,
	MAN_RI,
	MAN_na,
	MAN_i,
	MAN_sp,
	MAN_nf,
	MAN_fi,
	MAN_r,
	MAN_RE,
	MAN_RS,
	MAN_DT,
	MAN_UC,
	MAN_PD,
	MAN_Sp,
	MAN_Vb,
	MAN_Ve,
	MAN_AT,
	MAN_MAX
};

enum	man_type {
	MAN_TEXT,
	MAN_ELEM,
	MAN_ROOT,
	MAN_BLOCK,
	MAN_HEAD,
	MAN_BODY
};

struct	man_meta {
	char		*msec;
	time_t		 date;
	char		*vol;
	char		*title;
	char		*source;
};

struct	man_node {
	struct man_node	*parent;
	struct man_node	*child;
	struct man_node	*next;
	struct man_node	*prev;
	int		 nchild;
	int		 line;
	int		 pos;
	enum mant	 tok;
	int		 flags;
#define	MAN_VALID	(1 << 0)
#define	MAN_ACTED	(1 << 1)
#define	MAN_EOS		(1 << 2)
	enum man_type	 type;
	char		*string;
	struct man_node	*head;
	struct man_node	*body;
};

#define	MAN_IGN_MACRO	 (1 << 0)
#define	MAN_IGN_ESCAPE	 (1 << 2)

extern	const char *const *man_macronames;

__BEGIN_DECLS

struct	man;

void	 	  man_free(struct man *);
struct	man	 *man_alloc(void *, int, mandocmsg);
void		  man_reset(struct man *);
int	 	  man_parseln(struct man *, int, char *, int);
int		  man_endparse(struct man *);

const struct man_node *man_node(const struct man *);
const struct man_meta *man_meta(const struct man *);

__END_DECLS

#endif /*!MAN_H*/
