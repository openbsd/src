/*	$OpenBSD: server.h,v 1.2 1999/02/01 06:53:56 d Exp $	*/
/*	$NetBSD: hunt.h,v 1.5 1998/09/13 15:27:28 hubertf Exp $	*/

/*
 *  Hunt
 *  Copyright (c) 1985 Conrad C. Huang, Gregory S. Couch, Kenneth C.R.C. Arnold
 *  San Francisco, California
 */

#include <stdio.h>
#include <sys/socket.h>

/*
 * Choose MAXPL and MAXMON carefully.  The screen is assumed to be
 * 23 lines high and will only tolerate (MAXPL == 17 && MAXMON == 0)
 * or (MAXPL + MAXMON <= 16).
 */
#define MAXPL		14
#define MAXMON		2
#if (MAXPL + MAXMON > 16)
#warning "MAXPL + MAXMON is excessive"
#endif

#define MSGLEN		SCREEN_WIDTH

#define UBOUND		1
#define DBOUND		(HEIGHT - 1)
#define LBOUND		1
#define RBOUND		(WIDTH - 1)

#define NASCII		128

/* Layout of the scoreboard: */
#define STAT_LABEL_COL	60
#define STAT_VALUE_COL	74
#define STAT_NAME_COL	61
#define STAT_SCAN_COL	(STAT_NAME_COL + 5)
#define STAT_AMMO_ROW	0
#define STAT_GUN_ROW	1
#define STAT_DAM_ROW	2
#define STAT_KILL_ROW	3
#define STAT_PLAY_ROW	5
#define STAT_MON_ROW	(STAT_PLAY_ROW + MAXPL + 1)
#define STAT_NAME_LEN	18

/* Number of boots: */
#define NBOOTS		2

/* Bitmask of directions */
#define NORTH		01
#define SOUTH		02
#define EAST		010
#define WEST		020

# undef CTRL
#define CTRL(x)	((x) & 037)

#define BULREQ		1		/* 0 */
#define GRENREQ		9		/* 1 */
#define SATREQ		25		/* 2 */
#define BOMB7REQ	49		/* 3 */
#define BOMB9REQ	81		/* 4 */
#define BOMB11REQ	121		/* 5 */
#define BOMB13REQ	169		/* 6 */
#define BOMB15REQ	225		/* 7 */
#define BOMB17REQ	289		/* 8 */
#define BOMB19REQ	361		/* 9 */
#define BOMB21REQ	441		/* 10 */
#define MAXBOMB				   11

#define SLIMEREQ	5		/* 0 */
#define SSLIMEREQ	10		/* 1 */
#define SLIME2REQ	15		/* 2 */
#define SLIME3REQ	20		/* 3 */
#define MAXSLIME			   4

#define EXPLEN		16

#define _scan_char(pp)	(((pp)->p_scan < 0) ? ' ' : '*')
#define _cloak_char(pp)	(((pp)->p_cloak < 0) ? _scan_char(pp) : '+')
#define stat_char(pp)	(((pp)->p_flying < 0) ? _cloak_char(pp) : FLYER)

typedef struct bullet_def	BULLET;
typedef struct expl_def		EXPL;
typedef struct player_def	PLAYER;
typedef struct ident_def	IDENT;
typedef struct regen_def	REGEN;

#define	ALL_PLAYERS		((PLAYER *)1)

struct ident_def {
	char	i_name[NAMELEN];
	char	i_team;
	long	i_machine;
	long	i_uid;
	float	i_kills;
	int	i_entries;
	float	i_score;
	int	i_absorbed;
	int	i_faced;
	int	i_shot;
	int	i_robbed;
	int	i_slime;
	int	i_missed;
	int	i_ducked;
	int	i_gkills, i_bkills, i_deaths, i_stillb, i_saved;
	IDENT	*i_next;
};

struct player_def {
	IDENT	*p_ident;
	char	p_over;
	int	p_face;
	int	p_undershot;
	int	p_flying;
	int	p_flyx, p_flyy;
	int	p_nboots;
	FILE	*p_output;
	int	p_fd;
	int	p_mask;
	int	p_damage;
	int	p_damcap;
	int	p_ammo;
	int	p_ncshot;
	int	p_scan;
	int	p_cloak;
	int	p_x, p_y;
	int	p_ncount;
	int	p_nexec;
	long	p_nchar;
	char	p_death[MSGLEN];
	char	p_maze[HEIGHT][WIDTH2];
	int	p_curx, p_cury;
	int	p_lastx, p_lasty;
	char	p_cbuf[BUFSIZ];
};

struct bullet_def {
	int	b_x, b_y;
	int	b_face;
	int	b_charge;
	char	b_type;
	char	b_size;
	char	b_over;
	PLAYER	*b_owner;
	IDENT	*b_score;
	FLAG	b_expl;
	BULLET	*b_next;
};

struct expl_def {
	int	e_x, e_y;
	char	e_char;
	EXPL	*e_next;
};

struct regen_def {
	int	r_x, r_y;
	REGEN	*r_next;
};

struct spawn {
	int		fd;
	int		state;
	struct sockaddr source;
	int 		sourcelen;
	u_int32_t	uid;
	char		name[NAMELEN+1];
	u_int8_t	team;
	u_int32_t	enter_status;
	char		ttyname[NAMELEN];
	u_int32_t	mode;
	char		msg[BUFSIZ];
	int		msglen;
	struct spawn *	next;
	struct spawn **	prevnext;
};

extern struct spawn *	Spawn;

extern int	Socket;

/* answer.c */
void	answer_first __P((void));
int	answer_next __P((struct spawn *));
int	rand_dir __P((void));
void	answer_info __P((FILE *));

/* draw.c */
void	drawmaze __P((PLAYER *));
void	look __P((PLAYER *));
void	check __P((PLAYER *, int, int));
void	showstat __P((PLAYER *));
void	drawplayer __P((PLAYER *, FLAG));
void	message __P((PLAYER *, char *));

/* driver.c */
int	rand_num __P((int));
void	checkdam __P((PLAYER *, PLAYER *, IDENT *, int, char));
void	cleanup __P((int));

/* execute.c */
void	mon_execute __P((PLAYER *));
void	execute __P((PLAYER *));
void	add_shot __P((int, int, int, char, int, PLAYER *, int, char));
BULLET *create_shot __P((int, int, int, char, int, int, PLAYER *, IDENT *,
	int, char));
void	ammo_update __P((PLAYER *));

/* expl.c */
void	showexpl __P((int, int, char));
void	rollexpl __P((void));
void	makemaze __P((void));
void	clearwalls __P((void));
int	can_rollexpl __P((void));

/* makemaze.c */
void	makemaze __P((void));

/* shots.c */
int	can_moveshots __P((void));
void	moveshots __P((void));
PLAYER *play_at __P((int, int));
int	opposite __P((int, char));
BULLET *is_bullet __P((int, int));
void	fixshots __P((int, int, char));

/* terminal.c */
void	cgoto __P((PLAYER *, int, int));
void	outch __P((PLAYER *, char));
void	outstr __P((PLAYER *, char *, int));
void	outyx __P((PLAYER *, int, int, const char *, ...))
			__attribute__((format (printf, 4, 5)));
void	clrscr __P((PLAYER *));
void	ce __P((PLAYER *));
void	sendcom __P((PLAYER *, int, ...));
void	flush __P((PLAYER *));
void	log __P((int, const char *, ...))
			__attribute__((format (printf, 2, 3)));
void	logx __P((int, const char *, ...))
			__attribute__((format (printf, 2, 3)));

/* extern.c */
extern FLAG	Am_monitor;
extern char	Buf[BUFSIZ];
extern char	Maze[HEIGHT][WIDTH2];
extern char	Orig_maze[HEIGHT][WIDTH2];
extern fd_set	Fds_mask;
extern fd_set	Have_inp;
extern int	Nplayer;
extern int	Num_fds;
extern int	Socket;
extern int	Status;
extern int	See_over[NASCII];
extern BULLET *	Bullets;
extern EXPL *	Expl[EXPLEN];
extern EXPL *	Last_expl;
extern PLAYER	Player[MAXPL];
extern PLAYER *	End_player;
extern PLAYER	Boot[NBOOTS];
extern IDENT *	Scores;
extern PLAYER	Monitor[MAXMON];
extern PLAYER *	End_monitor;
extern int	volcano;
extern int	shot_req[MAXBOMB];
extern int	shot_type[MAXBOMB];
extern int	slime_req[MAXSLIME];
