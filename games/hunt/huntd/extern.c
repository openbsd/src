/*	$OpenBSD: extern.c,v 1.3 1999/01/29 07:30:35 d Exp $	*/
/*	$NetBSD: extern.c,v 1.2 1997/10/10 16:33:24 lukem Exp $	*/
/*
 *  Hunt
 *  Copyright (c) 1985 Conrad C. Huang, Gregory S. Couch, Kenneth C.R.C. Arnold
 *  San Francisco, California
 */

# include	"hunt.h"
# include	"server.h"

FLAG	Am_monitor = FALSE;		/* current process is a monitor */

char	Buf[BUFSIZ];			/* general scribbling buffer */
char	Maze[HEIGHT][WIDTH2];		/* the maze */
char	Orig_maze[HEIGHT][WIDTH2];	/* the original maze */

fd_set	Fds_mask;			/* mask for the file descriptors */
fd_set	Have_inp;			/* which file descriptors have input */
int	Nplayer = 0;			/* number of players */
int	Num_fds;			/* number of maximum file descriptor */
int	Socket;				/* main socket */
int	Status;				/* stat socket */
int	See_over[NASCII];		/* lookup table for determining whether
					 * character represents "transparent"
					 * item */

BULLET	*Bullets = NULL;		/* linked list of bullets */

EXPL	*Expl[EXPLEN];			/* explosion lists */
EXPL	*Last_expl;			/* last explosion on Expl[0] */

PLAYER	Player[MAXPL];			/* all the players */
PLAYER	*End_player = Player;		/* last active player slot */
PLAYER	Boot[NBOOTS];			/* all the boots */
IDENT	*Scores;			/* score cache */
PLAYER	Monitor[MAXMON];		/* all the monitors */
PLAYER	*End_monitor = Monitor;		/* last active monitor slot */

int	volcano = 0;			/* Explosion size */

int	shot_req[MAXBOMB]	= {
				BULREQ, GRENREQ, SATREQ,
				BOMB7REQ, BOMB9REQ, BOMB11REQ,
				BOMB13REQ, BOMB15REQ, BOMB17REQ,
				BOMB19REQ, BOMB21REQ,
			};
int	shot_type[MAXBOMB]	= {
				SHOT, GRENADE, SATCHEL,
				BOMB, BOMB, BOMB,
				BOMB, BOMB, BOMB,
				BOMB, BOMB,
			};

int	slime_req[MAXSLIME]	= {
				SLIMEREQ, SSLIMEREQ, SLIME2REQ, SLIME3REQ,
			};
