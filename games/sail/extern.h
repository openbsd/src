/*	$OpenBSD: extern.h,v 1.1 1999/01/18 21:14:15 pjanzen Exp $	*/
/*	$NetBSD: extern.h,v 1.8 1998/09/13 15:27:30 hubertf Exp $ */

/*
 * Copyright (c) 1983, 1993
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
 *	@(#)externs.h	8.1 (Berkeley) 5/31/93
 */

#include <stdio.h>
#include <signal.h>
#include <string.h>
#include <ctype.h>
#include <setjmp.h>
#include <sys/types.h>
#include "machdep.h"

	/* program mode */
int mode;
jmp_buf restart;
#define MODE_PLAYER	1
#define MODE_DRIVER	2
#define MODE_LOGGER	3

	/* command line flags */
char debug;				/* -D */
char randomize;				/* -x, give first available ship */
char longfmt;				/* -l, print score in long format */
char nobells;				/* -b, don't ring bell before Signal */

	/* other initial modes */
gid_t gid;
gid_t egid;

#define die()		((random() >> 3) % 6 + 1)
#define sqr(a)		((a) * (a))
#define abs(a)		((a) > 0 ? (a) : -(a))
#define min(a,b)	((a) < (b) ? (a) : (b))

#define grappled(a)	((a)->file->ngrap)
#define fouled(a)	((a)->file->nfoul)
#define snagged(a)	(grappled(a) + fouled(a))

#define grappled2(a, b)	((a)->file->grap[(b)->file->index].sn_count)
#define fouled2(a, b)	((a)->file->foul[(b)->file->index].sn_count)
#define snagged2(a, b)	(grappled2(a, b) + fouled2(a, b))

#define Xgrappled2(a, b) ((a)->file->grap[(b)->file->index].sn_turn < turn-1 ? grappled2(a, b) : 0)
#define Xfouled2(a, b)	((a)->file->foul[(b)->file->index].sn_turn < turn-1 ? fouled2(a, b) : 0)
#define Xsnagged2(a, b)	(Xgrappled2(a, b) + Xfouled2(a, b))

#define cleangrapple(a, b, c)	Cleansnag(a, b, c, 1)
#define cleanfoul(a, b, c)	Cleansnag(a, b, c, 2)
#define cleansnag(a, b, c)	Cleansnag(a, b, c, 3)

#define sterncolour(sp)	((sp)->file->stern+'0'-((sp)->file->captured?10:0))
#define sternrow(sp)	((sp)->file->row + dr[(sp)->file->dir])
#define sterncol(sp)	((sp)->file->col + dc[(sp)->file->dir])

#define capship(sp)	((sp)->file->captured?(sp)->file->captured:(sp))

#define readyname(r)	((r) & R_LOADING ? '*' : ((r) & R_INITIAL ? '!' : ' '))

/* loadL and loadR, should match loadname[] */
#define L_EMPTY		0		/* should be 0, don't change */
#define L_GRAPE		1
#define L_CHAIN		2
#define L_ROUND		3
#define L_DOUBLE	4
#define L_EXPLODE	5

/*
 * readyL and readyR, these are bits, except R_EMPTY
 */
#define R_EMPTY		0		/* not loaded and not loading */
#define R_LOADING	1		/* loading */
#define R_DOUBLE	2		/* loading double */
#define R_LOADED	4		/* loaded */
#define R_INITIAL	8		/* loaded initial */

#define HULL		0
#define RIGGING		1

#define W_CAPTAIN	1
#define W_CAPTURED	2
#define W_CLASS		3
#define W_CREW		4
#define W_DBP		5
#define W_DRIFT		6
#define W_EXPLODE	7
#define W_FILE		8
#define W_FOUL		9
#define W_GUNL		10
#define W_GUNR		11
#define W_HULL		12
#define W_MOVE		13
#define W_OBP		14
#define W_PCREW		15
#define W_UNFOUL	16
#define W_POINTS	17
#define W_QUAL		18
#define W_UNGRAP	19
#define W_RIGG		20
#define W_COL		21
#define W_DIR		22
#define W_ROW		23
#define W_SIGNAL	24
#define W_SINK		25
#define W_STRUCK	26
#define W_TA		27
#define W_ALIVE		28
#define W_TURN		29
#define W_WIND		30
#define W_FS		31
#define W_GRAP		32
#define W_RIG1		33
#define W_RIG2		34
#define W_RIG3		35
#define W_RIG4		36
#define W_BEGIN		37
#define W_END		38
#define W_DDEAD		39

#define NLOG 10
struct logs {
	char l_name[20];
	uid_t l_uid;
	int l_shipnum;
	int l_gamenum;
	int l_netpoints;
};

struct BP {
	short turnsent;
	struct ship *toship;
	short mensent;
};

struct snag {
	short sn_count;
	short sn_turn;
};

#define NSCENE	nscene
#define NSHIP	10
#define NBP	3

#define NNATION	8
#define N_A	0
#define N_B	1
#define N_S	2
#define N_F	3
#define N_J	4
#define N_D	5
#define N_K	6
#define N_O	7

struct File {
	int index;
	char captain[20];		/* 0 */
	short points;			/* 20 */
	unsigned char loadL;		/* 22 */
	unsigned char loadR;		/* 24 */
	unsigned char readyL;		/* 26 */
	unsigned char readyR;		/* 28 */
	struct BP OBP[NBP];		/* 30 */
	struct BP DBP[NBP];		/* 48 */
	char struck;			/* 66 */
	struct ship *captured;		/* 68 */
	short pcrew;			/* 70 */
	char movebuf[10];		/* 72 */
	char drift;			/* 82 */
	short nfoul;
	short ngrap;
	struct snag foul[NSHIP];	/* 84 */
	struct snag grap[NSHIP];	/* 124 */
	char RH;			/* 224 */
	char RG;			/* 226 */
	char RR;			/* 228 */
	char FS;			/* 230 */
	char explode;			/* 232 */
	char sink;			/* 234 */
	unsigned char dir;
	short col;
	short row;
	char loadwith;
	char stern;
};

struct ship {
	const char *shipname;		/* 0 */
	struct shipspecs *specs;	/* 2 */
	unsigned char nationality;	/* 4 */
	short shiprow;			/* 6 */
	short shipcol;			/* 8 */
	char shipdir;			/* 10 */
	struct File *file;		/* 12 */
};

struct scenario {
	char winddir;			/* 0 */
	char windspeed;			/* 2 */
	char windchange;		/* 4 */
	unsigned char vessels;		/* 12 */
	const char *name;		/* 14 */
	struct ship ship[NSHIP];	/* 16 */
};
extern struct scenario scene[];
extern int nscene;

struct shipspecs {
	char bs;
	char fs;
	char ta;
	short guns;
	unsigned char class;
	char hull;
	unsigned char qual;
	char crew1;
	char crew2;
	char crew3;
	char gunL;
	char gunR;
	char carL;
	char carR;
	int rig1;
	int rig2;
	int rig3;
	int rig4;
	short pts;
};
extern struct shipspecs specs[];

struct scenario *cc;		/* the current scenario */
struct ship *ls;		/* &cc->ship[cc->vessels] */

#define SHIP(s)		(&cc->ship[s])
#define foreachship(sp)	for ((sp) = cc->ship; (sp) < ls; (sp)++)

struct windeffects {
	char A, B, C, D;
};
extern const struct windeffects WET[7][6];

struct Tables {
	char H, G, C, R;
};
extern const struct Tables RigTable[11][6];
extern const struct Tables HullTable[11][6];

extern const char AMMO[9][4];
extern const char HDT[9][10];
extern const char HDTrake[9][10];
extern const char QUAL[9][5];
extern const char MT[9][3];

extern const char *const countryname[];
extern const char *const classname[];
extern const char *const directionname[];
extern const char *const qualname[];
extern const char loadname[];

extern const char rangeofshot[];

extern const char dr[], dc[];

int winddir;
int windspeed;
int turn;
int game;
int alive;
int people;
char hasdriver;

/* assorted.c */
void table __P((int, int, int, struct ship *, struct ship *, int));
void Cleansnag __P((struct ship *, struct ship *, int, int));

/* dr_1.c */
void unfoul __P((void));
void boardcomp __P((void));
int fightitout __P((struct ship *, struct ship *, int));
void resolve __P((void));
void compcombat __P((void));
int next __P((void));

/* dr_2.c */
void thinkofgrapples __P((void));
void checkup __P((void));
void prizecheck __P((void));
int str_end __P((const char *));
void closeon __P((struct ship *, struct ship *, char[], int, int, int));
int score __P((char[], struct ship *, struct ship *, int));
void move_ship __P((const char *, struct ship *, unsigned char *, short *, short *, char *));
void try __P((char[], char [], int, int, int, int, int, struct ship *,
    struct ship *, int *, int));
void rmend __P((char *));

/* dr_3.c */
void moveall __P((void));
int stillmoving __P((int));
int is_isolated __P((struct ship *));
int push __P((struct ship *, struct ship *));
void step __P((int, struct ship *, char *));
void sendbp __P((struct ship *, struct ship *, int, int));
int is_toughmelee __P((struct ship *, struct ship *, int, int));
void reload __P((void));
void checksails __P((void));

/* dr_4.c */
void ungrap __P((struct ship *, struct ship *));
void grap __P((struct ship *, struct ship *));

/* dr_5.c */
void subtract __P((struct ship *, int, int [3], struct ship *, int));
int mensent __P((struct ship *, struct ship *, int[3], struct ship **, int *,
    int));

/* dr_main.c */
int dr_main __P((void));

/* game.c */
int maxturns __P((struct ship *, char *));
int maxmove __P((struct ship *, int, int));

/* lo_main.c */
int lo_main __P((void));

/* misc.c */
int range __P((struct ship *, struct ship *));
struct ship *closestenemy __P((struct ship *, int, int));
int angle __P((int, int));
int gunsbear __P((struct ship *, struct ship *));
int portside __P((struct ship *, struct ship *, int));
int colours __P((struct ship *));
void logger __P((struct ship *));

/* parties.c */
int meleeing __P((struct ship *, struct ship *));
int boarding __P((struct ship *, int));
void unboard __P((struct ship *, struct ship *, int));

/* pl_1.c */
void leave __P((int)) __attribute__((__noreturn__));
void choke __P((int)) __attribute__((__noreturn__));
void child __P((int));

/* pl_2.c */
void play __P((void));

/* pl_3.c */
void acceptcombat __P((void));
void grapungrap __P((void));
void unfoulplayer __P((void));

/* pl_4.c */
void changesail __P((void));
void acceptsignal __P((void));
void lookout __P((void));
const char *saywhat __P((struct ship *, int));
void eyeball __P((struct ship *));

/* pl_5.c */
void acceptmove __P((void));
void acceptboard __P((void));
void parties __P((int[3], struct ship *, int, int));

/* pl_6.c */
void repair __P((void));
int turned __P((void));
void loadplayer __P((void));

/* pl_7.c */
void initscreen __P((void));
void cleanupscreen __P((void));
void newturn __P((int));
void Signal __P((char *, struct ship *, ...))
	 __attribute__((__format__(__printf__,1,3)));
void Msg __P((char *, ...))
	 __attribute__((__format__(__printf__,1,2)));
void Scroll __P((void));
void prompt __P((const char *, struct ship *));
void endprompt __P((int));
int sgetch __P((const char *, struct ship *, int));
void sgetstr __P((const char *, char *, int));
void draw_screen __P((void));
void draw_view __P((void));
void draw_turn __P((void));
void draw_stat __P((void));
void draw_slot __P((void));
void draw_board __P((void));
void centerview __P((void));
void upview __P((void));
void downview __P((void));
void leftview __P((void));
void rightview __P((void));
void adjustview __P((void));

/* pl_main.c */
int pl_main __P((void));
void initialize __P((void));

/* sync.c */
void fmtship __P((char *, size_t, const char *, struct ship *));
void makesignal __P((struct ship *, const char *, struct ship *, ...))
	 __attribute__((__format__(__printf__,2,4)));
void makemsg __P((struct ship *, const char *, ...))
	 __attribute__((__format__(__printf__,2,3)));
int sync_exists __P((int));
int sync_open __P((void));
void sync_close __P((int));
void Write __P((int, struct ship *, long, long, long, long));
void Writestr __P((int, struct ship *, const char *));
int Sync __P((void));
int sync_update __P((int, struct ship *, const char *, long, long, long, long));
