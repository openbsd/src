/*	$OpenBSD: hunt.h,v 1.3 1999/01/29 07:30:36 d Exp $	*/
/*	$NetBSD: hunt.h,v 1.5 1998/09/13 15:27:28 hubertf Exp $	*/

/*
 *  Hunt
 *  Copyright (c) 1985 Conrad C. Huang, Gregory S. Couch, Kenneth C.R.C. Arnold
 *  San Francisco, California
 */

/*
 * Preprocessor define dependencies
 */

/* decrement version number for each change in startup protocol */
# define	HUNT_VERSION		(-1)
# define	HUNT_PORT		(('h' << 8) | 't')

# define	ADDCH		('a' | 0200)
# define	MOVE		('m' | 0200)
# define	REFRESH		('r' | 0200)
# define	CLRTOEOL	('c' | 0200)
# define	ENDWIN		('e' | 0200)
# define	CLEAR		('C' | 0200)
# define	REDRAW		('R' | 0200)
# define	LAST_PLAYER	('l' | 0200)
# define	BELL		('b' | 0200)
# define	READY		('g' | 0200)

# define	SCREEN_HEIGHT	24
# define	SCREEN_WIDTH	80
# define	HEIGHT	23
# define	WIDTH	51
# define	SCREEN_WIDTH2	128	/* Next power of 2 >= SCREEN_WIDTH */
# define	WIDTH2	64	/* Next power of 2 >= WIDTH (for fast access) */

# define	NAMELEN		20

# define	Q_QUIT		0
# define	Q_CLOAK		1
# define	Q_FLY		2
# define	Q_SCAN		3
# define	Q_MESSAGE	4

# define	C_PLAYER	0
# define	C_MONITOR	1
# define	C_MESSAGE	2
# define	C_SCORES	3
# define	C_TESTMSG()	(Query_driver ? C_MESSAGE :\
				(Show_scores ? C_SCORES :\
				(Am_monitor ? C_MONITOR :\
				C_PLAYER)))

typedef int			FLAG;

/* Objects within the maze: */

# define	DOOR	'#'
# define	WALL1	'-'
# define	WALL2	'|'
# define	WALL3	'+'
# define	WALL4	'/'
# define	WALL5	'\\'
# define	KNIFE	'K'
# define	SHOT	':'
# define	GRENADE	'o'
# define	SATCHEL	'O'
# define	BOMB	'@'
# define	MINE	';'
# define	GMINE	'g'
# define	SLIME	'$'
# define	LAVA	'~'
# define	DSHOT	'?'
# define	FALL	'F'
# define	BOOT		'b'
# define	BOOT_PAIR	'B'

# define	SPACE	' '

# define	ABOVE	'i'
# define	BELOW	'!'
# define	RIGHT	'}'
# define	LEFTS	'{'
# define	FLYER	'&'
# define	isplayer(c)	(c == LEFTS || c == RIGHT ||\
				c == ABOVE || c == BELOW || c == FLYER)

# ifndef TRUE
# define	TRUE	1
# define	FALSE	0
# endif

