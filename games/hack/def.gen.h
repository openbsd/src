/*
 * Copyright (c) Stichting Mathematisch Centrum, Amsterdam, 1985.
 *
 *	$NetBSD: def.gen.h,v 1.3 1995/03/23 08:29:25 cgd Exp $
 */

struct gen {
	struct gen *ngen;
	xchar gx,gy;
	unsigned gflag;		/* 037: trap type; 040: SEEN flag */
				/* 0100: ONCE only */
#define	TRAPTYPE	037
#define	SEEN	040
#define	ONCE	0100
};
extern struct gen *fgold, *ftrap;
struct gen *g_at();
#define newgen()	(struct gen *) alloc(sizeof(struct gen))
