/*	$OpenBSD: driver.c,v 1.3 1999/01/29 07:30:35 d Exp $	*/
/*	$NetBSD: driver.c,v 1.5 1997/10/20 00:37:16 lukem Exp $	*/
/*
 *  Hunt
 *  Copyright (c) 1985 Conrad C. Huang, Gregory S. Couch, Kenneth C.R.C. Arnold
 *  San Francisco, California
 */

#include <sys/ioctl.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <err.h>
#include <errno.h>
#include <signal.h>
#include <stdlib.h>
#include <unistd.h>
#include <stdio.h>
#include <tcpd.h>
#include <syslog.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include "hunt.h"
#include "conf.h"
#include "server.h"

int	Seed = 0;

char	*First_arg;		/* pointer to argv[0] */
char	*Last_arg;		/* pointer to end of argv/environ */
u_int16_t Server_port = HUNT_PORT;
int	Server_socket;		/* test socket to answer datagrams */
FLAG	inetd_spawned;		/* invoked via inetd */
FLAG	should_announce = TRUE;	/* true if listening on standard port */
u_short	sock_port;		/* port # of tcp listen socket */
u_short	stat_port;		/* port # of statistics tcp socket */
in_addr_t Server_addr = INADDR_ANY;	/* address to bind to */

static	void	clear_scores __P((void));
static	int	havechar __P((PLAYER *));
static	void	init __P((void));
	int	main __P((int, char *[], char *[]));
static	void	makeboots __P((void));
static	void	send_stats __P((void));
static	void	zap __P((PLAYER *, FLAG));
static  void	announce_game __P((void));

/*
 * main:
 *	The main program.
 */
int
main(ac, av, ep)
	int	ac;
	char	**av, **ep;
{
	PLAYER	*pp;
	int	had_char;
	short	port_num, reply;
	static fd_set	read_fds;
	static FLAG	first = TRUE;
	static FLAG	server = FALSE;
	extern int	optind;
	extern char	*optarg;
	int		c;
	FILE *		cffile;
	static struct timeval	linger = { 0, 0 };

	First_arg = av[0];
	if (ep == NULL || *ep == NULL)
		ep = av + ac;
	while (*ep)
		ep++;
	Last_arg = ep[-1] + strlen(ep[-1]);

	config();

	while ((c = getopt(ac, av, "sp:a:")) != -1) {
		switch (c) {
		  case 's':
			server = TRUE;
			break;
		  case 'p':
			should_announce = FALSE;
			Server_port = atoi(optarg);
			break;
		  case 'a':
			Server_addr = inet_addr(optarg);
			if (Server_addr == INADDR_NONE)
				err(1, "bad interface address: %s", optarg);
			break;
		  default:
erred:
			fprintf(stderr, "Usage: %s [-s] [-p port] [-a addr]\n",
			    av[0]);
			exit(2);
		}
	}
	if (optind < ac)
		goto erred;

	/* Open syslog: */
	openlog("huntd", LOG_PID | (conf_logerr && !server? LOG_PERROR : 0),
		LOG_DAEMON);

	/* Initialise game parameters: */
	init();

again:
	do {
		/* Wait for something to happen: */
		read_fds = Fds_mask;
		errno = 0;
		while (select(Num_fds, &read_fds, NULL, NULL, NULL) < 0)
		{
			if (errno != EINTR) {
				syslog(LOG_ERR, "select: %m");
				cleanup(1);
			}
			errno = 0;
		}

		/* Remember which descriptors are active: */
		Have_inp = read_fds;

		/* Handle a datagram sent to the server socket: */
		if (FD_ISSET(Server_socket, &read_fds)) {
			struct sockaddr_in	test;
			int 			namelen;
			u_int16_t		msg;

			namelen = sizeof test;
			(void) recvfrom(Server_socket, 
				&msg, sizeof msg,
				0,
				(struct sockaddr *) &test, &namelen);

			port_num = htons(sock_port);
			switch (ntohs(msg)) {
			  case C_MESSAGE:
				if (Nplayer <= 0)
					break;
				reply = htons((u_short) Nplayer);
				(void) sendto(Server_socket, 
					&reply, sizeof reply, 
					0,
					(struct sockaddr *) &test, sizeof test);
				break;
			  case C_SCORES:
				reply = htons(stat_port);
				(void) sendto(Server_socket, 
					&reply, sizeof reply, 
					0,
					(struct sockaddr *) &test, sizeof test);
				break;
			  case C_PLAYER:
			  case C_MONITOR:
				if (msg == C_MONITOR && Nplayer <= 0)
					break;
				reply = htons(sock_port);
				(void) sendto(Server_socket, 
					&reply, sizeof reply, 
					0,
					(struct sockaddr *) &test, sizeof test);
				break;
			}
		}

		/* Process input and move bullets until we've exhausted input */
		for (;;) {
			had_char = FALSE;
			for (pp = Player; pp < End_player; pp++)
				if (havechar(pp)) {
					execute(pp);
					pp->p_nexec++;
					had_char++;
				}
			for (pp = Monitor; pp < End_monitor; pp++)
				if (havechar(pp)) {
					mon_execute(pp);
					pp->p_nexec++;
					had_char++;
				}
			if (!had_char)
				break;
			moveshots();
			for (pp = Player; pp < End_player; )
				if (pp->p_death[0] != '\0')
					zap(pp, TRUE);
				else
					pp++;
			for (pp = Monitor; pp < End_monitor; )
				if (pp->p_death[0] != '\0')
					zap(pp, FALSE);
				else
					pp++;
		}

		/* Answer new player connections: */
		if (FD_ISSET(Socket, &read_fds))
			if (answer()) {
				if (first && should_announce)
					announce_game();
				first = FALSE;
			}

		/* Answer statistics connections: */
		if (FD_ISSET(Status, &read_fds))
			send_stats();

		/* Flush/synchronize all the displays: */
		for (pp = Player; pp < End_player; pp++) {
			if (FD_ISSET(pp->p_fd, &read_fds))
				sendcom(pp, READY, pp->p_nexec);
			pp->p_nexec = 0;
			flush(pp);
		}
		for (pp = Monitor; pp < End_monitor; pp++) {
			if (FD_ISSET(pp->p_fd, &read_fds))
				sendcom(pp, READY, pp->p_nexec);
			pp->p_nexec = 0;
			flush(pp);
		}
	} while (Nplayer > 0);

	/* No more players. Wait for a short while for one to come back: */
	read_fds = Fds_mask;
	linger.tv_sec = conf_linger;
	if (select(Num_fds, &read_fds, NULL, NULL, &linger) > 0) {
		/* Someone returned! Resume the game: */
		goto again;
	}

	/* If we are an inetd server, we should restart: */
	if (server) {
		clear_scores();
		makemaze();
		clearwalls();
		makeboots();
		first = TRUE;
		goto again;
	}

	/* Destroy all the monitors: */
	for (pp = Monitor; pp < End_monitor; )
		zap(pp, FALSE);

	/* The end: */
	cleanup(0);
	exit(0);
}

/*
 * init:
 *	Initialize the global parameters.
 */
static void
init()
{
	int	i;
	struct sockaddr_in	test_port;
	int	msg;
	int	len;
	struct sockaddr_in	addr;

	/* XXX should we call deamon() instead ??? */
	(void) setsid();
	(void) setpgid(getpid(), getpid());

	/* Handle some signals: */
	(void) signal(SIGHUP, SIG_IGN);
	(void) signal(SIGINT, cleanup);
	(void) signal(SIGQUIT, SIG_IGN);
	(void) signal(SIGTERM, cleanup);
	(void) signal(SIGPIPE, SIG_IGN);

	(void) chdir("/");		/* just in case it core dumps */
	(void) umask(0777);		/* No privacy at all! */

	/* Initialize statistics socket: */
	addr.sin_family = AF_INET;
	addr.sin_addr.s_addr = Server_addr;
	addr.sin_port = 0;

	Status = socket(AF_INET, SOCK_STREAM, 0);
	if (bind(Status, (struct sockaddr *) &addr, sizeof addr) < 0) {
		syslog(LOG_ERR, "bind: %m");
		cleanup(1);
	}
	(void) listen(Status, 5);

	len = sizeof (struct sockaddr_in);
	if (getsockname(Status, (struct sockaddr *) &addr, &len) < 0)  {
		syslog(LOG_ERR, "getsockname: %m");
		cleanup(1);
	}
	stat_port = ntohs(addr.sin_port);

	/* Initialize main socket: */
	addr.sin_family = AF_INET;
	addr.sin_addr.s_addr = Server_addr;
	addr.sin_port = 0;

	Socket = socket(AF_INET, SOCK_STREAM, 0);
	msg = 1;
	if (setsockopt(Socket, SOL_SOCKET, SO_USELOOPBACK, &msg, sizeof msg)<0)
		syslog(LOG_ERR, "setsockopt loopback %m");
	if (bind(Socket, (struct sockaddr *) &addr, sizeof addr) < 0) {
		syslog(LOG_ERR, "bind: %m");
		cleanup(1);
	}
	(void) listen(Socket, 5);

	len = sizeof (struct sockaddr_in);
	if (getsockname(Socket, (struct sockaddr *) &addr, &len) < 0)  {
		syslog(LOG_ERR, "getsockname: %m");
		cleanup(1);
	}
	sock_port = ntohs(addr.sin_port);

	/* Initialize minimal select mask */
	FD_ZERO(&Fds_mask);
	FD_SET(Socket, &Fds_mask);
	FD_SET(Status, &Fds_mask);
	Num_fds = ((Socket > Status) ? Socket : Status) + 1;

	/* Check if stdin is a socket: */
	len = sizeof (struct sockaddr_in);
	if (getsockname(STDIN_FILENO, (struct sockaddr *) &test_port, &len) >= 0
	    && test_port.sin_family == AF_INET) {
		/* We are probably running from inetd: */
		inetd_spawned = TRUE;
		Server_socket = STDIN_FILENO;
		if (test_port.sin_port != htons((u_short) Server_port)) {
			should_announce = FALSE;
			Server_port = ntohs(test_port.sin_port);
		}
	} else {
		/* We need to listen on a socket: */
		test_port = addr;
		test_port.sin_port = htons((u_short) Server_port);

		Server_socket = socket(AF_INET, SOCK_DGRAM, 0);
		if (bind(Server_socket, (struct sockaddr *) &test_port,
		    sizeof test_port) < 0) {
			syslog(LOG_ERR, "bind port %d: %m", Server_port);
			cleanup(1);
		}
		(void) listen(Server_socket, 5);
	}

	/* We'll handle the broadcast listener in the main loop: */
	FD_SET(Server_socket, &Fds_mask);
	if (Server_socket + 1 > Num_fds)
		Num_fds = Server_socket + 1;

	/* Initialise the random seed: */
	Seed = getpid() + time((time_t *) NULL);

	/* Dig the maze: */
	makemaze();

	/* Create some boots, if needed: */
	makeboots();

	/* Construct a table of what objects a player can see over: */
	for (i = 0; i < NASCII; i++)
		See_over[i] = TRUE;
	See_over[DOOR] = FALSE;
	See_over[WALL1] = FALSE;
	See_over[WALL2] = FALSE;
	See_over[WALL3] = FALSE;
	See_over[WALL4] = FALSE;
	See_over[WALL5] = FALSE;

	syslog(LOG_INFO, "game started");
}

/*
 * makeboots:
 *	Put the boots in the maze
 */
static void
makeboots()
{
	int	x, y;
	PLAYER	*pp;

	if (conf_boots) {
		do {
			x = rand_num(WIDTH - 1) + 1;
			y = rand_num(HEIGHT - 1) + 1;
		} while (Maze[y][x] != SPACE);
		Maze[y][x] = BOOT_PAIR;
	}

	for (pp = Boot; pp < &Boot[NBOOTS]; pp++)
		pp->p_flying = -1;
}


/*
 * checkdam:
 *	Apply damage to the victim from an attacker.
 *	If the victim dies as a result, give points to 'credit',
 */
void
checkdam(victim, attacker, credit, damage, shot_type)
	PLAYER	*victim, *attacker;
	IDENT	*credit;
	int	damage;
	char	shot_type;
{
	char	*cp;
	int	y;

	/* Don't do anything if the victim is already in the throes of death */
	if (victim->p_death[0] != '\0')
		return;

	/* Weaken slime attacks by 0.5 * number of boots the victim has on: */
	if (shot_type == SLIME)
		switch (victim->p_nboots) {
		  default:
			break;
		  case 1:
			damage = (damage + 1) / 2;
			break;
		  case 2:
			if (attacker != NULL)
				message(attacker, "He has boots on!");
			return;
		}

	/* The victim sustains some damage: */
	victim->p_damage += damage;

	/* Check if the victim survives the hit: */
	if (victim->p_damage <= victim->p_damcap) {
		/* They survive. */
		outyx(victim, STAT_DAM_ROW, STAT_VALUE_COL, "%2d",
			victim->p_damage);
		return;
	}

	/* Describe how the victim died: */
	switch (shot_type) {
	  default:
		cp = "Killed";
		break;
	  case FALL:
		cp = "Killed on impact";
		break;
	  case KNIFE:
		cp = "Stabbed to death";
		victim->p_ammo = 0;		/* No exploding */
		break;
	  case SHOT:
		cp = "Shot to death";
		break;
	  case GRENADE:
	  case SATCHEL:
	  case BOMB:
		cp = "Bombed";
		break;
	  case MINE:
	  case GMINE:
		cp = "Blown apart";
		break;
	  case SLIME:
		cp = "Slimed";
		if (credit != NULL)
			credit->i_slime++;
		break;
	  case LAVA:
		cp = "Baked";
		break;
	  case DSHOT:
		cp = "Eliminated";
		break;
	}

	if (credit == NULL) {
		char *blame;

		/*
		 * Nobody is taking the credit for the kill.
		 * Attribute it to either a mine or 'act of God'.
		 */
		switch (shot_type) {
		case MINE:
		case GMINE:
			blame = "a mine";
			break;
		default:
			blame = "act of God";
			break;
		}

		/* Set the death message: */
		(void) snprintf(victim->p_death, sizeof victim->p_death, 
			"| %s by %s |", cp, blame);

		/* No further score crediting needed. */
		return;
	}

	/* Set the death message: */
	(void) snprintf(victim->p_death, sizeof victim->p_death, 
		"| %s by %s |", cp, credit->i_name);

	if (victim == attacker) {
		/* No use killing yourself. */
		credit->i_kills--;
		credit->i_bkills++;
	} 
	else if (victim->p_ident->i_team == ' '
	    || victim->p_ident->i_team != credit->i_team) {
		/* A cross-team kill: */
		credit->i_kills++;
		credit->i_gkills++;
	}
	else {
		/* They killed someone on the same team: */
		credit->i_kills--;
		credit->i_bkills++;
	}

	/* Compute the new credited score: */
	credit->i_score = credit->i_kills / (double) credit->i_entries;

	/* The victim accrues one death: */
	victim->p_ident->i_deaths++;

	/* Account for 'Stillborn' deaths */
	if (victim->p_nchar == 0)
		victim->p_ident->i_stillb++;

	if (attacker) {
		/* Give the attacker player a bit more strength */
		attacker->p_damcap += conf_killgain;
		attacker->p_damage -= conf_killgain;
		if (attacker->p_damage < 0)
			attacker->p_damage = 0;

		/* Tell the attacker's his new strength: */
		outyx(attacker, STAT_DAM_ROW, STAT_VALUE_COL, "%2d/%2d", 
			attacker->p_damage, attacker->p_damcap);

		/* Tell the attacker's his new 'kill count': */
		outyx(attacker, STAT_KILL_ROW, STAT_VALUE_COL, "%3d",
			(attacker->p_damcap - conf_maxdam) / 2);

		/* Update the attacker's score for everyone else */
		y = STAT_PLAY_ROW + 1 + (attacker - Player);
		outyx(ALL_PLAYERS, y, STAT_NAME_COL,
			"%5.2f", attacker->p_ident->i_score);
	}
}

/*
 * zap:
 *	Kill off a player and take them out of the game.
 *	The 'was_player' flag indicates that the player was not
 *	a monitor and needs extra cleaning up.
 */
static void
zap(pp, was_player)
	PLAYER	*pp;
	FLAG	was_player;
{
	int	len;
	BULLET	*bp;
	PLAYER	*np;
	int	x, y;
	int	savefd;

	if (was_player) {
		/* If they died from a shot, clean up shrapnel */
		if (pp->p_undershot)
			fixshots(pp->p_y, pp->p_x, pp->p_over);
		/* Let the player see their last position: */
		drawplayer(pp, FALSE);
		/* Remove from game: */
		Nplayer--;
	}

	/* Display the cause of death in the centre of the screen: */
	len = strlen(pp->p_death);
	x = (WIDTH - len) / 2;
	outyx(pp, HEIGHT / 2, x, "%s", pp->p_death);

	/* Put some horizontal lines around and below the death message: */
	memset(pp->p_death + 1, '-', len - 2);
	pp->p_death[0] = '+';
	pp->p_death[len - 1] = '+';
	outyx(pp, HEIGHT / 2 - 1, x, "%s", pp->p_death);
	outyx(pp, HEIGHT / 2 + 1, x, "%s", pp->p_death);

	/* Move to bottom left */
	cgoto(pp, HEIGHT, 0);

	savefd = pp->p_fd;

	if (was_player) {
		int	expl_charge;
		int	expl_type;
		int	ammo_exploding;

		/* Check all the bullets: */
		for (bp = Bullets; bp != NULL; bp = bp->b_next) {
			if (bp->b_owner == pp)
				/* Zapped players can't own bullets: */
				bp->b_owner = NULL;
			if (bp->b_x == pp->p_x && bp->b_y == pp->p_y)
				/* Bullets over the player are now over air: */
				bp->b_over = SPACE;
		}

		/* Explode a random fraction of the player's ammo: */
		ammo_exploding = rand_num(pp->p_ammo);

		/* Determine the type and amount of detonation: */
		expl_charge = rand_num(ammo_exploding + 1);
		if (pp->p_ammo == 0)
			/* Ignore the no-ammo case: */
			expl_charge = 0;
		else if (ammo_exploding >= pp->p_ammo - 1) {
			/* Maximal explosions always appear as slime: */
			expl_charge = pp->p_ammo;
			expl_type = SLIME;
		} else {
			/*
			 * Figure out the best effective explosion
			 * type to use, given the amount of charge
			 */
			int btype, stype;
			for (btype = MAXBOMB - 1; btype > 0; btype--)
				if (expl_charge >= shot_req[btype])
					break;
			for (stype = MAXSLIME - 1; stype > 0; stype--)
				if (expl_charge >= slime_req[stype])
					break;
			/* Pick the larger of the bomb or slime: */
			if (btype >= 0 && stype >= 0) {
				if (shot_req[btype] > slime_req[btype])
					btype = -1;
			}
			if (btype >= 0)  {
				expl_type = shot_type[btype];
				expl_charge = shot_req[btype];
			} else
				expl_type = SLIME;
		}

		if (expl_charge > 0) {
			char buf[BUFSIZ];

			/* Detonate: */
			(void) add_shot(expl_type, pp->p_y, pp->p_x, 
			    pp->p_face, expl_charge, (PLAYER *) NULL, 
			    TRUE, SPACE);

			/* Explain what the explosion is about. */
			snprintf(buf, sizeof buf, "%s detonated.", 
				pp->p_ident->i_name);
			message(ALL_PLAYERS, buf);

			while (pp->p_nboots-- > 0) {
				/* Throw one of the boots away: */
				for (np = Boot; np < &Boot[NBOOTS]; np++)
					if (np->p_flying < 0)
						break;
#ifdef DIAGNOSTIC
				if (np >= &Boot[NBOOTS])
					err(1, "Too many boots");
#endif
				/* Start the boots from where the player is */
				np->p_undershot = FALSE;
				np->p_x = pp->p_x;
				np->p_y = pp->p_y;
				/* Throw for up to 20 steps */
				np->p_flying = rand_num(20);
				np->p_flyx = 2 * rand_num(6) - 5;
				np->p_flyy = 2 * rand_num(6) - 5;
				np->p_over = SPACE;
				np->p_face = BOOT;
				showexpl(np->p_y, np->p_x, BOOT);
			}
		}
		/* No explosion. Leave the player's boots behind. */
		else if (pp->p_nboots > 0) {
			if (pp->p_nboots == 2)
				Maze[pp->p_y][pp->p_x] = BOOT_PAIR;
			else
				Maze[pp->p_y][pp->p_x] = BOOT;
			if (pp->p_undershot)
				fixshots(pp->p_y, pp->p_x,
					Maze[pp->p_y][pp->p_x]);
		}

		/* Any unexploded ammo builds up in the volcano: */
		volcano += pp->p_ammo - expl_charge;

		/* Volcano eruption: */
		if (conf_volcano && rand_num(100) < volcano / 
		    conf_volcano_max) {
			/* Erupt near the middle of the map */
			do {
				x = rand_num(WIDTH / 2) + WIDTH / 4;
				y = rand_num(HEIGHT / 2) + HEIGHT / 4;
			} while (Maze[y][x] != SPACE);

			/* Convert volcano charge into lava: */
			(void) add_shot(LAVA, y, x, LEFTS, volcano,
				(PLAYER *) NULL, TRUE, SPACE);
			volcano = 0;

			/* Tell eveyone what's happening */
			message(ALL_PLAYERS, "Volcano eruption.");
		}

		/* Drone: */
		if (conf_drone && rand_num(100) < 2) {
			/* Find a starting place near the middle of the map: */
			do {
				x = rand_num(WIDTH / 2) + WIDTH / 4;
				y = rand_num(HEIGHT / 2) + HEIGHT / 4;
			} while (Maze[y][x] != SPACE);

			/* Start the drone going: */
			add_shot(DSHOT, y, x, rand_dir(),
				shot_req[conf_mindshot +
				rand_num(MAXBOMB - conf_mindshot)],
				(PLAYER *) NULL, FALSE, SPACE);
		}

		/* Tell the zapped player's client to shut down. */
		sendcom(pp, ENDWIN, ' ');
		(void) fclose(pp->p_output);

		/* Close up the gap in the Player array: */
		End_player--;
		if (pp != End_player) {
			/* Move the last player into the gap: */
			memcpy(pp, End_player, sizeof *pp);
			outyx(ALL_PLAYERS, 
				STAT_PLAY_ROW + 1 + (pp - Player), 
				STAT_NAME_COL,
				"%5.2f%c%-10.10s %c",
				pp->p_ident->i_score, stat_char(pp),
				pp->p_ident->i_name, pp->p_ident->i_team);
		}

		/* Erase the last player from the display: */
		cgoto(ALL_PLAYERS, STAT_PLAY_ROW + 1 + Nplayer, STAT_NAME_COL);
		ce(ALL_PLAYERS);
	}
	else {
		/* Zap a monitor */

		/* Close the session: */
		sendcom(pp, ENDWIN, LAST_PLAYER);
		(void) fclose(pp->p_output);

		/* shuffle the monitor table */
		End_monitor--;
		if (pp != End_monitor) {
			memcpy(pp, End_monitor, sizeof *pp);
			outyx(ALL_PLAYERS, 
				STAT_MON_ROW + 1 + (pp - Player), STAT_NAME_COL,
				"%5.5s %-10.10s %c", " ",
				pp->p_ident->i_name, pp->p_ident->i_team);
		}

		/* Erase the last monitor in the list */
		cgoto(ALL_PLAYERS,
			STAT_MON_ROW + 1 + (End_monitor - Monitor),
			STAT_NAME_COL);
		ce(ALL_PLAYERS);
	}

	/* Update the file descriptor sets used by select: */
	FD_CLR(savefd, &Fds_mask);
	if (Num_fds == savefd + 1) {
		Num_fds = Socket;
		if (Server_socket > Socket)
			Num_fds = Server_socket;
		for (np = Player; np < End_player; np++)
			if (np->p_fd > Num_fds)
				Num_fds = np->p_fd;
		for (np = Monitor; np < End_monitor; np++)
			if (np->p_fd > Num_fds)
				Num_fds = np->p_fd;
		Num_fds++;
	}
}

/*
 * rand_num:
 *	Return a random number in a given range.
 */
int
rand_num(range)
	int	range;
{
	if (range == 0)
		return 0;
	Seed = Seed * 11109 + 13849;
	return (((Seed >> 16) & 0xffff) % range);
}

/*
 * havechar:
 *	Check to see if we have any characters in the input queue; if
 *	we do, read them, stash them away, and return TRUE; else return
 *	FALSE.
 */
static int
havechar(pp)
	PLAYER	*pp;
{

	if (pp->p_ncount < pp->p_nchar)
		return TRUE;
	if (!FD_ISSET(pp->p_fd, &Have_inp))
		return FALSE;
	FD_CLR(pp->p_fd, &Have_inp);
check_again:
	errno = 0;
	if ((pp->p_nchar = read(pp->p_fd, pp->p_cbuf, sizeof pp->p_cbuf)) <= 0)
	{
		if (errno == EINTR)
			goto check_again;
		pp->p_cbuf[0] = 'q';
	}
	pp->p_ncount = 0;
	return TRUE;
}

/*
 * cleanup:
 *	Exit with the given value, cleaning up any droppings lying around
 */
void
cleanup(eval)
	int	eval;
{
	PLAYER	*pp;

	/* Place their cursor in a friendly position: */
	cgoto(ALL_PLAYERS, HEIGHT, 0);

	/* Send them all the ENDWIN command: */
	sendcom(ALL_PLAYERS, ENDWIN, LAST_PLAYER);

	/* And close their connections: */
	for (pp = Player; pp < End_player; pp++)
		(void) fclose(pp->p_output);
	for (pp = Monitor; pp < End_monitor; pp++)
		(void) fclose(pp->p_output);

	/* Close the server socket: */
	(void) close(Socket);

	/* The end: */
	syslog(LOG_INFO, "game over");
	exit(eval);
}

/*
 * send_stats:
 *	Accept a connection to the statistics port, and emit
 *	the stats.
 */
static void
send_stats()
{
	IDENT	*ip;
	FILE	*fp;
	int	s;
	struct sockaddr_in	sockstruct;
	int	socklen;
	struct request_info ri;

	/* Accept a connection to the statistics socket: */
	socklen = sizeof sockstruct;
	s = accept(Status, (struct sockaddr *) &sockstruct, &socklen);
	if (s < 0) {
		if (errno == EINTR)
			return;
		syslog(LOG_ERR, "accept: %m");
		return;
	}

        /* Check for access permissions: */
        request_init(&ri, RQ_DAEMON, "huntd", RQ_FILE, s, 0);
        if (hosts_access(&ri) == 0) {
                close(s);
                return;
        }

	fp = fdopen(s, "w");
	if (fp == NULL) {
		syslog(LOG_ERR, "fdopen: %m");
		(void) close(s);
		return;
	}

	/* Send the statistics as raw text down the socket: */
	fputs("Name\t\tScore\tDucked\tAbsorb\tFaced\tShot\tRobbed\tMissed\tSlimeK\n", fp);
	for (ip = Scores; ip != NULL; ip = ip->i_next) {
		fprintf(fp, "%s%c%c%c\t", ip->i_name,
			ip->i_team == ' ' ? ' ' : '[',
			ip->i_team,
			ip->i_team == ' ' ? ' ' : ']'
		);
		if (strlen(ip->i_name) + 3 < 8)
			putc('\t', fp);
		fprintf(fp, "%.2f\t%d\t%d\t%d\t%d\t%d\t%d\t%d\n",
			ip->i_score, ip->i_ducked, ip->i_absorbed,
			ip->i_faced, ip->i_shot, ip->i_robbed,
			ip->i_missed, ip->i_slime);
	}
	fputs("\n\nName\t\tEnemy\tFriend\tDeaths\tStill\tSaved\n", fp);
	for (ip = Scores; ip != NULL; ip = ip->i_next) {
		fprintf(fp, "%s%c%c%c\t", ip->i_name,
			ip->i_team == ' ' ? ' ' : '[',
			ip->i_team,
			ip->i_team == ' ' ? ' ' : ']'
		);
		if (strlen(ip->i_name) + 3 < 8)
			putc('\t', fp);
		fprintf(fp, "%d\t%d\t%d\t%d\t%d\n",
			ip->i_gkills, ip->i_bkills, ip->i_deaths,
			ip->i_stillb, ip->i_saved);
	}

	(void) fclose(fp);
}

/*
 * clear_scores:
 *	Clear the Scores list.
 */
static void
clear_scores()
{
	IDENT	*ip, *nextip;

	/* Release the list of scores: */
	for (ip = Scores; ip != NULL; ip = nextip) {
		nextip = ip->i_next;
		(void) free((char *) ip);
	}
	Scores = NULL;
}

/*
 * announce_game:
 *	Publically announce the game
 */
static void
announce_game()
{

	/* Stub */
}
