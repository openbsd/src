/*	$OpenBSD: answer.c,v 1.3 1999/01/29 07:30:34 d Exp $	*/
/*	$NetBSD: answer.c,v 1.3 1997/10/10 16:32:50 lukem Exp $	*/
/*
 *  Hunt
 *  Copyright (c) 1985 Conrad C. Huang, Gregory S. Couch, Kenneth C.R.C. Arnold
 *  San Francisco, California
 */

#include <ctype.h>
#include <errno.h>
#include <fcntl.h>
#include <stdlib.h>
#include <unistd.h>
#include <stdio.h>
#include <tcpd.h>
#include <syslog.h>
#include <string.h>
#include <sys/socket.h>
#include <netinet/in.h>

#include "hunt.h"
#include "server.h"
#include "conf.h"

/* Exported symbols for hosts_access(): */
int allow_severity	= LOG_INFO;
int deny_severity	= LOG_WARNING;

static void	stplayer __P((PLAYER *, int));
static void	stmonitor __P((PLAYER *));
static IDENT *	get_ident __P((u_long, u_long, char *, char, 
			struct request_info *));

int
answer()
{
	PLAYER			*pp;
	int			newsock;
	u_int32_t		mode;
	char			name[NAMELEN];
	u_int8_t		team;
	u_int32_t		enter_status;
	int			socklen;
	u_long			machine;
	u_int32_t		uid;
	struct sockaddr_in	sockstruct;
	char			*cp1, *cp2;
	int			flags;
	u_int32_t		version;
	struct request_info	ri;
	char			Ttyname[NAMELEN];	/* never used */

	socklen = sizeof sockstruct;
	errno = 0;
	newsock = accept(Socket, (struct sockaddr *) &sockstruct, &socklen);
	if (newsock < 0)
	{
		if (errno == EINTR)
			return FALSE;
		syslog(LOG_ERR, "accept: %m");
		cleanup(1);
	}

	/* Check for access permissions: */
	request_init(&ri, RQ_DAEMON, "huntd", RQ_FILE, newsock, 0);
	if (hosts_access(&ri) == 0) {
		close(newsock);
		return (FALSE);
	}

	machine = ntohl((u_int32_t)((struct sockaddr_in *) &sockstruct)->sin_addr.s_addr);
	version = htonl((u_int32_t) HUNT_VERSION);
	(void) write(newsock, &version, sizeof version);
	(void) read(newsock, &uid, sizeof uid);
	uid = ntohl((unsigned long) uid);
	(void) read(newsock, name, sizeof name);
	(void) read(newsock, &team, sizeof team);
	(void) read(newsock, &enter_status, sizeof enter_status);
	enter_status = ntohl((unsigned long) enter_status);
	(void) read(newsock, Ttyname, sizeof Ttyname);
	(void) read(newsock, &mode, sizeof mode);
	mode = ntohl(mode);

	/*
	 * Turn off blocking I/O, so a slow or dead terminal won't stop
	 * the game.  All subsequent reads check how many bytes they read.
	 */
	flags = fcntl(newsock, F_GETFL, 0);
	flags |= O_NDELAY;
	(void) fcntl(newsock, F_SETFL, flags);

	/*
	 * Make sure the name contains only printable characters
	 * since we use control characters for cursor control
	 * between driver and player processes
	 */
	for (cp1 = cp2 = name; *cp1 != '\0'; cp1++)
		if (isprint(*cp1) || *cp1 == ' ')
			*cp2++ = *cp1;
	*cp2 = '\0';

	/* The connection is solely for a message: */
	if (mode == C_MESSAGE) {
		fd_set r;
		struct timeval tmo = { 0, 1000000 / 2 };
		char	buf[BUFSIZ + 1];
		int	buflen;
		int	n;

		/* wait for 0.5 second for the message packet */
		FD_ZERO(&r);
		FD_SET(newsock, &r);
		n = select(newsock+1, &r, 0, 0, &tmo);
		if (n < 0)
			syslog(LOG_ERR, "select: %m");
		else if (n > 0)  {
			buflen = 0;
			while (buflen < (BUFSIZ - 1) && (n = read(newsock, 
			    buf + buflen, (BUFSIZ - 1) - buflen)) > 0) 
				buflen += n;
			buf[buflen] = '\0';

			if (team == ' ')
				outyx(ALL_PLAYERS, HEIGHT, 0, "%s: %s", 
					name, buf);
			else
				outyx(ALL_PLAYERS, HEIGHT, 0, "%s[%c]: %s", 
					name, team, buf);
			ce(ALL_PLAYERS);
			sendcom(ALL_PLAYERS, REFRESH);
			sendcom(ALL_PLAYERS, READY, 0);
			flush(ALL_PLAYERS);
		}

		(void) close(newsock);
		return FALSE;
	}

	/* The player is a monitor: */
	else if (mode == C_MONITOR) {
		if (conf_monitor && End_monitor < &Monitor[MAXMON]) {
			pp = End_monitor++;
			if (team == ' ')
				team = '*';
		} else {
			u_int32_t response;

			/* Too many monitors */
			response = htonl(0);
			(void) write(newsock, (char *) &response,
				sizeof response);
			(void) close(newsock);
			syslog(LOG_NOTICE, "too many monitors");
			return FALSE;
		}

	/* The player is a normal hunter: */
	} else {
		if (End_player < &Player[MAXPL])
			pp = End_player++;
		else {
			u_int32_t response;

			/* Too many players */
			response = htonl(0);
			(void) write(newsock, (char *) &response,
				sizeof response);
			(void) close(newsock);
			syslog(LOG_NOTICE, "too many players");
			return FALSE;
		}
	}

	pp->p_ident = get_ident(machine, uid, name, team, &ri);
	pp->p_output = fdopen(newsock, "w");
	pp->p_death[0] = '\0';
	pp->p_fd = newsock;
	FD_SET(pp->p_fd, &Fds_mask);
	if (pp->p_fd >= Num_fds)
		Num_fds = pp->p_fd + 1;

	pp->p_y = 0;
	pp->p_x = 0;

	if (mode == C_MONITOR)
		stmonitor(pp);
	else
		stplayer(pp, enter_status);
	return TRUE;
}

/* Start a monitor: */
static void
stmonitor(pp)
	PLAYER	*pp;
{

	/* Monitors get to see the entire maze: */
	memcpy(pp->p_maze, Maze, sizeof pp->p_maze);
	drawmaze(pp);

	/* Put the monitor's name near the bottom right on all screens: */
	outyx(ALL_PLAYERS, 
		STAT_MON_ROW + 1 + (pp - Monitor), STAT_NAME_COL,
		"%5.5s%c%-10.10s %c", " ", 
		stat_char(pp), pp->p_ident->i_name, pp->p_ident->i_team);

	/* Ready the monitor: */
	sendcom(pp, REFRESH);
	sendcom(pp, READY, 0);
	flush(pp);
}

/* Start a player: */
static void
stplayer(newpp, enter_status)
	PLAYER	*newpp;
	int	enter_status;
{
	int	x, y;
	PLAYER	*pp;
	int len;

	Nplayer++;

	for (y = 0; y < UBOUND; y++)
		for (x = 0; x < WIDTH; x++)
			newpp->p_maze[y][x] = Maze[y][x];
	for (     ; y < DBOUND; y++) {
		for (x = 0; x < LBOUND; x++)
			newpp->p_maze[y][x] = Maze[y][x];
		for (     ; x < RBOUND; x++)
			newpp->p_maze[y][x] = SPACE;
		for (     ; x < WIDTH;  x++)
			newpp->p_maze[y][x] = Maze[y][x];
	}
	for (     ; y < HEIGHT; y++)
		for (x = 0; x < WIDTH; x++)
			newpp->p_maze[y][x] = Maze[y][x];

	/* Drop the new player somewhere in the maze: */
	do {
		x = rand_num(WIDTH - 1) + 1;
		y = rand_num(HEIGHT - 1) + 1;
	} while (Maze[y][x] != SPACE);
	newpp->p_over = SPACE;
	newpp->p_x = x;
	newpp->p_y = y;
	newpp->p_undershot = FALSE;

	/* Send them flying if needed */
	if (enter_status == Q_FLY && conf_fly) {
		newpp->p_flying = rand_num(conf_flytime);
		newpp->p_flyx = 2 * rand_num(conf_flystep + 1) - conf_flystep;
		newpp->p_flyy = 2 * rand_num(conf_flystep + 1) - conf_flystep;
		newpp->p_face = FLYER;
	} else {
		newpp->p_flying = -1;
		newpp->p_face = rand_dir();
	}

	/* Initialize the new player's attributes: */
	newpp->p_damage = 0;
	newpp->p_damcap = conf_maxdam;
	newpp->p_nchar = 0;
	newpp->p_ncount = 0;
	newpp->p_nexec = 0;
	newpp->p_ammo = conf_ishots;
	newpp->p_nboots = 0;

	/* Decide on what cloak/scan status to enter with */
	if (enter_status == Q_SCAN && conf_scan) {
		newpp->p_scan = conf_scanlen * Nplayer;
		newpp->p_cloak = 0;
	} else if (conf_cloak) {
		newpp->p_scan = 0;
		newpp->p_cloak = conf_cloaklen;
	} else {
		newpp->p_scan = 0;
		newpp->p_cloak = 0;
	}
	newpp->p_ncshot = 0;

	/*
	 * For each new player, place a large mine and
	 * a small mine somewhere in the maze:
	 */
	do {
		x = rand_num(WIDTH - 1) + 1;
		y = rand_num(HEIGHT - 1) + 1;
	} while (Maze[y][x] != SPACE);
	Maze[y][x] = GMINE;
	for (pp = Monitor; pp < End_monitor; pp++)
		check(pp, y, x);

	do {
		x = rand_num(WIDTH - 1) + 1;
		y = rand_num(HEIGHT - 1) + 1;
	} while (Maze[y][x] != SPACE);
	Maze[y][x] = MINE;
	for (pp = Monitor; pp < End_monitor; pp++)
		check(pp, y, x);

	/* Create a score line for the new player: */
	(void) snprintf(Buf, sizeof Buf, "%5.2f%c%-10.10s %c", 
		newpp->p_ident->i_score, stat_char(newpp), 
		newpp->p_ident->i_name, newpp->p_ident->i_team);
	len = strlen(Buf);
	y = STAT_PLAY_ROW + 1 + (newpp - Player);
	for (pp = Player; pp < End_player; pp++) {
		if (pp != newpp) {
			/* Give everyone a few more shots: */
			pp->p_ammo += conf_nshots;
			newpp->p_ammo += conf_nshots;
			outyx(pp, y, STAT_NAME_COL, Buf, len);
			ammo_update(pp);
		}
	}
	for (pp = Monitor; pp < End_monitor; pp++)
		outyx(pp, y, STAT_NAME_COL, Buf, len);

	/* Show the new player what they can see and where they are: */
	drawmaze(newpp);
	drawplayer(newpp, TRUE);
	look(newpp);

	/* Make sure that the position they enter in will be erased: */
	if (enter_status == Q_FLY && conf_fly)
		showexpl(newpp->p_y, newpp->p_x, FLYER);

	/* Ready the new player: */
	sendcom(newpp, REFRESH);
	sendcom(newpp, READY, 0);
	flush(newpp);
}

/*
 * rand_dir:
 *	Return a random direction
 */
int
rand_dir()
{
	switch (rand_num(4)) {
	  case 0:
		return LEFTS;
	  case 1:
		return RIGHT;
	  case 2:
		return BELOW;
	  case 3:
		return ABOVE;
	}
	/* NOTREACHED */
	return(-1);
}

/*
 * get_ident:
 *	Get the score structure of a player
 */
static IDENT *
get_ident(machine, uid, name, team, ri)
	u_long	machine;
	u_long	uid;
	char	*name;
	char	team;
	struct request_info *ri;
{
	IDENT		*ip;
	static IDENT	punt;

	for (ip = Scores; ip != NULL; ip = ip->i_next)
		if (ip->i_machine == machine
		&&  ip->i_uid == uid
		/* &&  ip->i_team == team */
		&&  strncmp(ip->i_name, name, NAMELEN) == 0)
			break;

	if (ip != NULL) {
		if (ip->i_team != team) {
			syslog(LOG_INFO, "player %s %s team %c",
				name,
				team == ' ' ? "left" : ip->i_team == ' ' ? 
					"joined" : "changed to",
				team == ' ' ? ip->i_team : team);
			ip->i_team = team;
		}
		if (ip->i_entries < conf_scoredecay)
			ip->i_entries++;
		else
			ip->i_kills = (ip->i_kills * (conf_scoredecay - 1))
				/ conf_scoredecay;
		ip->i_score = ip->i_kills / (double) ip->i_entries;
	}
	else {
		/* Alloc new entry -- it is released in clear_scores() */
		ip = (IDENT *) malloc(sizeof (IDENT));
		if (ip == NULL) {
			syslog(LOG_ERR, "malloc: %m");
			/* Fourth down, time to punt */
			ip = &punt;
		}
		ip->i_machine = machine;
		ip->i_team = team;
		ip->i_uid = uid;
		strlcpy(ip->i_name, name, sizeof ip->i_name);
		ip->i_kills = 0;
		ip->i_entries = 1;
		ip->i_score = 0;
		ip->i_absorbed = 0;
		ip->i_faced = 0;
		ip->i_shot = 0;
		ip->i_robbed = 0;
		ip->i_slime = 0;
		ip->i_missed = 0;
		ip->i_ducked = 0;
		ip->i_gkills = ip->i_bkills = ip->i_deaths = 0;
		ip->i_stillb = ip->i_saved = 0;
		ip->i_next = Scores;
		Scores = ip;

		syslog(LOG_INFO, "new player: %s%s%c%s",
			name, 
			team == ' ' ? "" : " (team ",
			team,
			team == ' ' ? "" : ")");
	}

	return ip;
}
