/*	$OpenBSD: monop.h,v 1.4 1998/09/20 23:36:54 pjanzen Exp $	*/
/*	$NetBSD: monop.h,v 1.4 1995/04/24 12:24:23 cgd Exp $	*/

/*
 * Copyright (c) 1980, 1993
 *	The Regents of the University of California.  All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *	This product includes software developed by the University of
 *	California, Berkeley and its contributors.
 * 4. Neither the name of the University nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE REGENTS AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 *	@(#)monop.h	8.1 (Berkeley) 5/31/93
 */

#include	<stdio.h>
#include	<stdlib.h>
#include	<string.h>

#ifdef __CHAR_UNSIGNED__
#define	shrt	short
#else
#define	shrt	char
#endif
#define	bool	int8_t

#define	TRUE	(1)
#define	FALSE	(0)

#define	N_MON	8	/* number of monopolies			*/
#define	N_PROP	22	/* number of normal property squares	*/
#define	N_RR	4	/* number of railroads			*/
#define	N_UTIL	2	/* number of utilities			*/
#define	N_SQRS	40	/* number of squares on board		*/
#define	MAX_PL	9	/* maximum number of players		*/
#define	MAX_PRP	(N_PROP+N_RR+N_UTIL) /* max # ownable property	*/

			/* square type numbers			*/
#define	PRPTY	0	/* normal property			*/
#define	RR	1	/* railroad				*/
#define	UTIL	2	/* water works - electric co		*/
#define	SAFE	3	/* safe spot				*/
#define	CC	4	/* community chest			*/
#define	CHANCE	5	/* chance (surprise!!!)			*/
#define	INC_TAX	6	/* Income tax */
#define	GOTO_J	7	/* Go To Jail! */
#define	LUX_TAX	8	/* Luxury tax */
#define	IN_JAIL	9	/* In jail */

#define	JAIL	40	/* JAIL square number			*/

#define	lucky(str)	printf("%s%s\n",str,lucky_mes[roll(1,num_luck)-1])
#define	printline()	printf("------------------------------\n")
#define	sqnum(sqp)	(sqp - board)

struct sqr_st {			/* structure for square			*/
	char	*name;			/* place name			*/
	shrt	owner;			/* owner number			*/
	shrt	type;			/* place type			*/
	struct prp_st	*desc;		/* description struct		*/
	int	cost;			/* cost				*/
};

typedef struct sqr_st	SQUARE;

struct mon_st {			/* monopoly description structure	*/
	char	*name;			/* monop. name (color)		*/
	shrt	owner;			/* owner of monopoly		*/
	shrt	num_in;			/* # in monopoly		*/
	shrt	num_own;		/* # owned (-1: not poss. monop)*/
	shrt	h_cost;			/* price of houses		*/
	char	*not_m;			/* name if not monopoly		*/
	char	*mon_n;			/* name if a monopoly		*/
	char	sqnums[3];		/* Square numbers (used to init)*/
	SQUARE	*sq[3];			/* list of squares in monop	*/
};

typedef struct mon_st	MON;

/*
 * This struct describes a property.  For railroads and utilities, only
 * the "morg" member is used.
 */
struct prp_st {			/* property description structure	*/
	bool	morg;			/* set if mortgaged		*/
	bool	monop;			/* set if monopoly		*/
	shrt	square;			/* square description		*/
	shrt	houses;			/* number of houses		*/
	MON	*mon_desc;		/* name of color		*/
	int	rent[6];		/* rents			*/
};

struct own_st {			/* element in list owned things		*/
	SQUARE	*sqr;			/* pointer to square		*/
	struct own_st	*next;		/* next in list			*/
};

typedef struct own_st	OWN;

struct plr_st {			/* player description structure		*/
	char	*name;			/* owner name			*/
	shrt	num_gojf;		/* # of get-out-of-jail-free's	*/
	shrt	num_rr;			/* # of railroads owned		*/
	shrt	num_util;		/* # of water works/elec. co.	*/
	shrt	loc;			/* location on board		*/
	shrt	in_jail;		/* count of turns in jail	*/
	int	money;			/* amount of money		*/
	OWN	*own_list;		/* start of property list	*/
};

typedef struct plr_st	PLAY;
typedef struct prp_st	PROP;
typedef struct prp_st	RR_S;
typedef struct prp_st	UTIL_S;

/* cards.c */
void	init_decks __P((void));
void	get_card __P((DECK *));

/* execute.c */
void	execute __P((int));
void	do_move __P((void));
void	move __P((int));
void	save __P((void));
void	restore __P((void));
int	rest_f __P((char *));

/* getinp.c */
int	getinp __P((char *, char *[]));

/* houses.c */
void	buy_houses __P((void));
void	sell_houses __P((void));

/* jail.c */
void	card __P((void));
void	ret_card __P((PLAY *));
void	pay __P((void));
int	move_jail __P((int, int ));
void	printturn __P((void));

/* misc.c */
int	getyn __P((char *));
void	notify __P((void));
void	next_play __P((void));
int	get_int __P((char *));
void	set_ownlist __P((int));
void	is_monop __P((MON *, int));
void	isnot_monop __P((MON *));
void	list __P((void));
void	list_all __P((void));
void	quit __P((void));

/* morg.c */
void	mortgage __P((void));
void	unmortgage __P((void));
void	force_morg __P((void));

/* print.c */
void	printboard __P((void));
void	where __P((void));
void	printsq __P((int, bool));
void	printhold __P((int));

/* prop.c */
void	buy __P((int, SQUARE *));
void	add_list __P((int, OWN **, int));
void	del_list __P((int, OWN **, shrt));
void	bid __P((void));
int	prop_worth __P((PLAY *));

/* rent.c */
void	rent __P((SQUARE *));

/* roll.c */
int	roll __P((int, int));

/* spec.c */
void	inc_tax __P((void));
void	goto_jail __P((void));
void	lux_tax __P((void));
void	cc __P((void));
void	chance __P((void));

/* trade.c */
void	trade __P((void));
void	resign __P((void));
