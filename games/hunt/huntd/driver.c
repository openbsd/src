/*	$OpenBSD: driver.c,v 1.6 1999/03/22 00:29:15 pjanzen Exp $	*/
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
#include <paths.h>
#include <fcntl.h>
#include "hunt.h"
#include "conf.h"
#include "server.h"

char	*First_arg;		/* pointer to argv[0] */
char	*Last_arg;		/* pointer to end of argv/environ */
u_int16_t Server_port = HUNT_PORT;
int	Server_socket;		/* test socket to answer datagrams */
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
static	void	siginfo __P((int));
static	void	print_stats __P((FILE *));
static	void	handle_wkport __P((int));

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
	static fd_set	read_fds;
	static FLAG	first = TRUE;
	static FLAG	server = FALSE;
	extern int	optind;
	extern char	*optarg;
	int		c;
	static struct timeval	linger = { 0, 0 };
	static struct timeval	timeout = { 0, 0 }, *to;
	struct spawn	*sp;
	int		ret;
	int		nready;

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
		/* First, poll to see if we can get input */
		timerclear(&timeout);
		do {
			read_fds = Fds_mask;
			errno = 0;
			nready = select(Num_fds, &read_fds, NULL, NULL, 
			    &timeout);
			if (nready < 0 && errno != EINTR) {
				log(LOG_ERR, "select");
				cleanup(1);
			}
		} while (nready < 0);

		if (nready == 0) {
			/*
			 * Nothing was ready. We do some work now
			 * to see if the simulation has any pending work
			 * to do, and decide if we need to to block 
			 * indefinitely or just timeout.
			 */
			if (conf_simstep && can_moveshots()) {
				/*
				 * block for a short time before continuing
				 * with explosions, bullets and whatnot
				 */
				to = &timeout;
				to->tv_sec =  conf_simstep / 1000000;
				to->tv_usec = conf_simstep % 1000000;
			} else
				/*
				 * since there's nothing going on,
				 * just block waiting for external activity
				 */
				to = NULL;
			
			do {
				read_fds = Fds_mask;
				errno = 0;
				nready = select(Num_fds, &read_fds, NULL, NULL, 
				    to);
				if (nready < 0 && errno != EINTR) {
					log(LOG_ERR, "select");
					cleanup(1);
				}
			} while (nready < 0);
		}

		/* Remember which descriptors are active: */
		Have_inp = read_fds;

		/* Answer new player connections: */
		if (FD_ISSET(Socket, &read_fds))
			answer_first();

		/* Continue answering new player connections: */
		for (sp = Spawn; sp; sp = sp->next)
			if (FD_ISSET(sp->fd, &read_fds) && answer_next(sp)) {
				if (first && should_announce)
					announce_game();
				first = FALSE;
			}

		/* Process input and move bullets until we've exhausted input */
		had_char = TRUE;
		while (had_char) {

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

			had_char = FALSE;
			for (pp = Player; pp < End_player; pp++)
				if (havechar(pp)) {
					execute(pp);
					pp->p_nexec++;
					had_char = TRUE;
				}
			for (pp = Monitor; pp < End_monitor; pp++)
				if (havechar(pp)) {
					mon_execute(pp);
					pp->p_nexec++;
					had_char = TRUE;
				}
		}

		/* Handle a datagram sent to the server socket: */
		if (FD_ISSET(Server_socket, &read_fds))
			handle_wkport(Server_socket);

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

	/* No more players! */

	/* Continuous game? */
	if (conf_linger < 0)
		goto again;

	/* Wait a short while for one to come back: */
	read_fds = Fds_mask;
	linger.tv_sec = conf_linger;
	while ((ret = select(Num_fds, &read_fds, NULL, NULL, &linger)) < 0) {
		if (errno != EINTR) {
			log(LOG_WARNING, "select");
			break;
		}
	}
	if (ret > 0)
		/* Someone returned! Resume the game: */
		goto again;
	/* else, it timed out, and the game is really over. */

	/* If we are an inetd server, we should re-init the map and restart: */
	if (server) {
		clear_scores();
		makemaze();
		clearwalls();
		makeboots();
		first = TRUE;
		goto again;
	}

	/* Get rid of any attached monitors: */
	for (pp = Monitor; pp < End_monitor; )
		zap(pp, FALSE);

	/* Fin: */
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
	struct sigaction	sact;

	(void) setsid();
	if (setpgid(getpid(), getpid()) == -1)
		err(1, "setpgid");

	sact.sa_flags = SA_RESTART;
	sigemptyset(&sact.sa_mask);

	/* Ignore HUP, QUIT and PIPE: */
	sact.sa_handler = SIG_IGN;
	if (sigaction(SIGHUP, &sact, NULL) == -1)
		err(1, "sigaction SIGHUP");
	if (sigaction(SIGQUIT, &sact, NULL) == -1)
		err(1, "sigaction SIGQUIT");
	if (sigaction(SIGPIPE, &sact, NULL) == -1)
		err(1, "sigaction SIGPIPE");

	/* Clean up gracefully on INT and TERM: */
	sact.sa_handler = cleanup;
	if (sigaction(SIGINT, &sact, NULL) == -1)
		err(1, "sigaction SIGINT");
	if (sigaction(SIGTERM, &sact, NULL) == -1)
		err(1, "sigaction SIGTERM");

	/* Handle INFO: */
	sact.sa_handler = siginfo;
	if (sigaction(SIGINFO, &sact, NULL) == -1)
		err(1, "sigaction SIGINFO");

	if (chdir("/") == -1)
		warn("chdir");
	(void) umask(0777);

	/* Initialize statistics socket: */
	addr.sin_family = AF_INET;
	addr.sin_addr.s_addr = Server_addr;
	addr.sin_port = 0;

	Status = socket(AF_INET, SOCK_STREAM, 0);
	if (bind(Status, (struct sockaddr *) &addr, sizeof addr) < 0) {
		log(LOG_ERR, "bind");
		cleanup(1);
	}
	if (listen(Status, 5) == -1) {
		log(LOG_ERR, "listen");
		cleanup(1);
	}

	len = sizeof (struct sockaddr_in);
	if (getsockname(Status, (struct sockaddr *) &addr, &len) < 0)  {
		log(LOG_ERR, "getsockname");
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
		log(LOG_ERR, "setsockopt loopback");
	if (bind(Socket, (struct sockaddr *) &addr, sizeof addr) < 0) {
		log(LOG_ERR, "bind");
		cleanup(1);
	}
	if (listen(Socket, 5) == -1) {
		log(LOG_ERR, "listen");
		cleanup(1);
	}

	len = sizeof (struct sockaddr_in);
	if (getsockname(Socket, (struct sockaddr *) &addr, &len) < 0)  {
		log(LOG_ERR, "getsockname");
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
		/* We are probably running from inetd:  don't log to stderr */
		Server_socket = STDIN_FILENO;
		conf_logerr = 0;
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
			log(LOG_ERR, "bind port %d", Server_port);
			cleanup(1);
		}

		/* Datagram sockets do not need a listen() call. */
	}

	/* We'll handle the broadcast listener in the main loop: */
	FD_SET(Server_socket, &Fds_mask);
	if (Server_socket + 1 > Num_fds)
		Num_fds = Server_socket + 1;

	/* Initialise the random seed: */
	srandom(getpid() + time((time_t *) NULL));

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

	logx(LOG_INFO, "game started");
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

		/* Tell the attacker his new strength: */
		outyx(attacker, STAT_DAM_ROW, STAT_VALUE_COL, "%2d/%2d", 
			attacker->p_damage, attacker->p_damcap);

		/* Tell the attacker his new 'kill count': */
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
	return (random() % range);
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

	/* Do we already have characters? */
	if (pp->p_ncount < pp->p_nchar)
		return TRUE;
	/* Is the player being quiet? */
	if (!FD_ISSET(pp->p_fd, &Have_inp))
		return FALSE;
	/* Remove the player from the read set until we have drained them: */
	FD_CLR(pp->p_fd, &Have_inp);

	/* Suck their keypresses into a buffer: */
check_again:
	if ((pp->p_nchar = read(pp->p_fd, pp->p_cbuf, sizeof pp->p_cbuf)) <= 0)
	{
		if (errno == EINTR)
			goto check_again;
		if (errno != EAGAIN) {
			log(LOG_INFO, "read");
			/* Assume their connection was lost/closed: */
			pp->p_cbuf[0] = 'q';
			pp->p_nchar = 1;
		}
	}
	/* Reset pointer into read buffer */
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
	logx(LOG_INFO, "game over");
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
		logx(LOG_ERR, "accept");
		return;
	}

	/* Check for access permissions: */
	request_init(&ri, RQ_DAEMON, "huntd", RQ_FILE, s, 0);
	fromhost(&ri);
	if (hosts_access(&ri) == 0) {
		logx(LOG_INFO, "rejected connection from %s", eval_client(&ri));
		close(s);
		return;
	}

	fp = fdopen(s, "w");
	if (fp == NULL) {
		log(LOG_ERR, "fdopen");
		(void) close(s);
		return;
	}

	print_stats(fp);

	(void) fclose(fp);
}

/*
 * print_stats:
 * 	emit the game statistics
 */
void
print_stats(fp)
	FILE *fp;
{
	IDENT	*ip;
	PLAYER  *pp;

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
	fputs("\n\nName\t\tEnemy\tFriend\tDeaths\tStill\tSaved\tConnect\n", fp);
	for (ip = Scores; ip != NULL; ip = ip->i_next) {
		fprintf(fp, "%s%c%c%c\t", ip->i_name,
			ip->i_team == ' ' ? ' ' : '[',
			ip->i_team,
			ip->i_team == ' ' ? ' ' : ']'
		);
		if (strlen(ip->i_name) + 3 < 8)
			putc('\t', fp);
		fprintf(fp, "%d\t%d\t%d\t%d\t%d\t",
			ip->i_gkills, ip->i_bkills, ip->i_deaths,
			ip->i_stillb, ip->i_saved);
		for (pp = Player; pp < End_player; pp++)
			if (pp->p_ident == ip)
				putc('p', fp);
		for (pp = Monitor; pp < End_monitor; pp++)
			if (pp->p_ident == ip)
				putc('m', fp);
		putc('\n', fp);
	}
}


/*
 * Send the game statistics to the controlling tty
 */
static void
siginfo(sig)
	int sig;
{
	int tty;
	FILE *fp;

	if ((tty = open(_PATH_TTY, O_WRONLY)) >= 0) {
		fp = fdopen(tty, "w");
		print_stats(fp);
		answer_info(fp);
		fclose(fp);
	}
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
		free((char *) ip);
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

/*
 * Handle a UDP packet sent to the well known port.
 */
static void
handle_wkport(fd)
	int fd;
{
	struct sockaddr		fromaddr;
	int 			fromlen;
	u_int16_t		query;
	u_int16_t		response;
	struct request_info	ri;

	request_init(&ri, RQ_DAEMON, "huntd", RQ_FILE, fd, 0);
	fromhost(&ri);
	fromlen = sizeof fromaddr;
	if (recvfrom(fd, &query, sizeof query, 0, &fromaddr, &fromlen) == -1)
	{
		log(LOG_WARNING, "recvfrom");
		return;
	}

	/* Do we allow access? */
	if (hosts_access(&ri) == 0) {
		logx(LOG_INFO, "rejected connection from %s", eval_client(&ri));
		return;
	}

	query = ntohs(query);

	switch (query) {
	  case C_MESSAGE:
		if (Nplayer <= 0)
			/* Don't bother replying if nobody to talk to: */
			return;
		/* Return the number of people playing: */
		response = Nplayer;
		break;
	  case C_SCORES:
		/* Someone wants the statistics port: */
		response = stat_port;
		break;
	  case C_PLAYER:
	  case C_MONITOR:
		/* Someone wants to play or watch: */
		if (query == C_MONITOR && Nplayer <= 0)
			/* Don't bother replying if there's nothing to watch: */
			return;
		/* Otherwise, tell them how to get to the game: */
		response = sock_port;
		break;
	  default:
		log(LOG_INFO, "unknown udp query %d", query);
		return;
	}

	response = ntohs(response);
	if (sendto(fd, &response, sizeof response, 0,
	    &fromaddr, sizeof fromaddr) == -1)
		log(LOG_WARNING, "sendto");
}
