/*	$OpenBSD: hunt.c,v 1.3 1999/01/29 07:30:33 d Exp $	*/
/*	$NetBSD: hunt.c,v 1.8 1998/09/13 15:27:28 hubertf Exp $	*/
/*
 *  Hunt
 *  Copyright (c) 1985 Conrad C. Huang, Gregory S. Couch, Kenneth C.R.C. Arnold
 *  San Francisco, California
 */

#include <sys/stat.h>
#include <sys/time.h>
#include <ctype.h>
#include <err.h>
#include <errno.h>
#include <curses.h>
#include <signal.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <netdb.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>


#include "hunt.h"
#include "display.h"
#include "client.h"


/*
 * Some old versions of curses don't have these defined
 */

FLAG	Am_monitor = FALSE;
int	Socket;
char	map_key[256];			/* what to map keys to */
FLAG	no_beep = FALSE;
char	*Send_message = NULL;

static u_int16_t Server_port = HUNT_PORT;
static char	*Sock_host;
static char	*use_port;
static FLAG	Query_driver = FALSE;
static FLAG	Show_scores = FALSE;
static struct sockaddr_in	Daemon;


static char	name[NAMELEN];
static char	team = '-';

static int	in_visual;

static void	dump_scores __P((struct sockaddr_in));
static long	env_init __P((long));
static void	fill_in_blanks __P((void));
static void	leave __P((int, char *)) __attribute__((__noreturn__));
static struct sockaddr_in *list_drivers __P((void));
static void	sigterm __P((int));
static void	find_driver __P((FLAG));
static void	start_driver __P((void));

/*
 * main:
 *	Main program for local process
 */
int
main(ac, av)
	int	ac;
	char	**av;
{
	int		c;
	extern int	optind;
	extern char	*optarg;
	long		enter_status;

	/* Revoke privs: */
	setegid(getgid());
	setgid(getgid());

	enter_status = env_init((long) Q_CLOAK);
	while ((c = getopt(ac, av, "Sbcfh:l:mn:op:qst:w:")) != -1) {
		switch (c) {
		case 'l':	/* rsh compatibility */
		case 'n':
			(void) strlcpy(name, optarg, sizeof name);
			break;
		case 't':
			team = *optarg;
			if (!isdigit(team) && team != ' ') {
				warnx("Team names must be numeric");
				team = '-';
			}
			break;
		case 'o':
			Otto_mode = TRUE;
			break;
		case 'm':
			Am_monitor = TRUE;
			break;
		case 'S':
			Show_scores = TRUE;
			break;
		case 'q':	/* query whether hunt is running */
			Query_driver = TRUE;
			break;
		case 'w':
			Send_message = optarg;
			break;
		case 'h':
			Sock_host = optarg;
			break;
		case 'p':
			use_port = optarg;
			Server_port = atoi(use_port);
			break;
		case 'c':
			enter_status = Q_CLOAK;
			break;
		case 'f':
			enter_status = Q_FLY;
			break;
		case 's':
			enter_status = Q_SCAN;
			break;
		case 'b':
			no_beep = !no_beep;
			break;
		default:
		usage:
			fputs(
"usage:\thunt [-qmcsfS] [-n name] [-t team] [-p port] [-w message] [host]\n",
			stderr);
			exit(1);
		}
	}
	if (optind + 1 < ac)
		goto usage;
	else if (optind + 1 == ac)
		Sock_host = av[ac - 1];

	if (Show_scores) {
		struct sockaddr_in	*hosts;

		for (hosts = list_drivers(); hosts->sin_port != 0; hosts += 1)
			dump_scores(*hosts);
		exit(0);
	}
	if (Query_driver) {
		struct sockaddr_in	*hosts;

		for (hosts = list_drivers(); hosts->sin_port != 0; hosts += 1) {
			struct	hostent	*hp;
			int	num_players;

			hp = gethostbyaddr((char *) &hosts->sin_addr,
					sizeof hosts->sin_addr, AF_INET);
			num_players = ntohs(hosts->sin_port);
			printf("%d player%s hunting on %s!\n",
				num_players, (num_players == 1) ? "" : "s",
				hp != NULL ? hp->h_name :
				inet_ntoa(hosts->sin_addr));
		}
		exit(0);
	}
	if (Otto_mode)
		(void) strlcpy(name, "otto", sizeof name);
	else
		fill_in_blanks();

	(void) fflush(stdout);
	display_open();
	in_visual = TRUE;
	if (LINES < SCREEN_HEIGHT || COLS < SCREEN_WIDTH) {
		errno = 0;
		leave(1, "Need a larger window");
	}
	display_clear_the_screen();
	(void) signal(SIGINT, intr);
	(void) signal(SIGTERM, sigterm);
	(void) signal(SIGPIPE, SIG_IGN);

	for (;;) {
		find_driver(TRUE);

		if (Daemon.sin_port == 0) {
			errno = 0;
			leave(1, "Game not found, try again");
		}

	jump_in:
		do {
			int	option;

			Socket = socket(AF_INET, SOCK_STREAM, 0);
			if (Socket < 0)
				leave(1, "socket");
			option = 1;
			if (setsockopt(Socket, SOL_SOCKET, SO_USELOOPBACK,
			    &option, sizeof option) < 0)
				warn("setsockopt loopback");
			errno = 0;
			if (connect(Socket, (struct sockaddr *) &Daemon,
			    sizeof Daemon) < 0) {
				if (errno != ECONNREFUSED)
					leave(1, "connect");
			}
			else
				break;
			sleep(1);
		} while (close(Socket) == 0);

		do_connect(name, team, enter_status);
		if (Send_message != NULL) {
			do_message();
			if (enter_status == Q_MESSAGE)
				break;
			Send_message = NULL;
			/* don't continue as that will call find_driver */
			goto jump_in;
		}
		playit();
		if ((enter_status = quit(enter_status)) == Q_QUIT)
			break;
	}
	leave(0, (char *) NULL);
	/* NOTREACHED */
	return(0);
}

# ifdef BROADCAST
static int
broadcast_vec(s, vector)
	int			s;		/* socket */
	struct	sockaddr	**vector;
{
	char			if_buf[BUFSIZ];
	struct	ifconf		ifc;
	struct	ifreq		*ifr;
	unsigned int		n;
	int			vec_cnt;

	*vector = NULL;
	ifc.ifc_len = sizeof if_buf;
	ifc.ifc_buf = if_buf;
	if (ioctl(s, SIOCGIFCONF, (char *) &ifc) < 0)
		return 0;
	vec_cnt = 0;
	n = ifc.ifc_len / sizeof (struct ifreq);
	*vector = (struct sockaddr *) malloc(n * sizeof (struct sockaddr));
	if (*vector == NULL)
		leave(1, "malloc");
	for (ifr = ifc.ifc_req; n != 0; n--, ifr++)
		if (ioctl(s, SIOCGIFBRDADDR, ifr) >= 0)
			memcpy(&(*vector)[vec_cnt++], &ifr->ifr_addr,
				sizeof (*vector)[0]));
	return vec_cnt;
}
# endif

static struct sockaddr_in	*
list_drivers()
{
	u_short			msg;
	u_short			port_num;
	static struct sockaddr_in		test;
	int			test_socket;
	int			namelen;
	char			local_name[MAXHOSTNAMELEN + 1];
	static int		initial = TRUE;
	static struct in_addr	local_address;
	struct hostent		*hp;
# ifdef BROADCAST
	static	int		brdc;
	static	struct sockaddr_in		*brdv;
# else
	u_long			local_net;
# endif
	int			i;
	static	struct sockaddr_in		*listv;
	static	unsigned int	listmax;
	unsigned int		listc;
	fd_set			mask;
	struct timeval		wait;

	if (initial) {			/* do one time initialization */
# ifndef BROADCAST
		sethostent(1);		/* don't bother to close host file */
# endif
		if (gethostname(local_name, sizeof local_name) < 0)
			leave(1, "gethostname");
		local_name[sizeof(local_name) - 1] = '\0';
		if ((hp = gethostbyname(local_name)) == NULL)
			leave(1, "gethostbyname");
		local_address = * ((struct in_addr *) hp->h_addr);

		listmax = 20;
		listv = (struct sockaddr_in *) malloc(listmax * sizeof (struct sockaddr_in));
		if (listv == NULL)
			leave(1, "malloc");
	} else if (Sock_host != NULL)
		return listv;		/* address already valid */

	test_socket = socket(AF_INET, SOCK_DGRAM, 0);
	if (test_socket < 0)
		leave(1, "socket");
	test.sin_family = AF_INET;
	test.sin_port = htons(Server_port);
	listc = 0;

	if (Sock_host != NULL) {	/* explicit host given */
		if ((hp = gethostbyname(Sock_host)) == NULL) 
			leave(1, "gethostbyname");
		test.sin_addr = *((struct in_addr *) hp->h_addr);
		goto test_one_host;
	}

	if (!initial) {
		/* favor host of previous session by broadcasting to it first */
		test.sin_addr = Daemon.sin_addr;
		msg = htons(C_PLAYER);		/* Must be playing! */
		(void) sendto(test_socket, (char *) &msg, sizeof msg, 0,
		    (struct sockaddr *) &test, sizeof test);
	}

# ifdef BROADCAST
	if (initial)
		brdc = broadcast_vec(test_socket, (struct sockaddr **) &brdv);

	if (brdc <= 0) {
		initial = FALSE;
		test.sin_addr = local_address;
		goto test_one_host;
	}

# ifdef SO_BROADCAST
	/* Sun's will broadcast even though this option can't be set */
	option = 1;
	if (setsockopt(test_socket, SOL_SOCKET, SO_BROADCAST,
	    &option, sizeof option) < 0)
		leave(1, "setsockopt broadcast");
# endif

	/* send broadcast packets on all interfaces */
	msg = htons(C_TESTMSG());
	for (i = 0; i < brdc; i++) {
		test.sin_addr = brdv[i].sin_addr;
		if (sendto(test_socket, (char *) &msg, sizeof msg, 0,
		    (struct sockaddr *) &test, test) < 0)
			leave(1, "sendto");
	}
# else /* !BROADCAST */
	/* loop thru all hosts on local net and send msg to them. */
	msg = htons(C_TESTMSG());
	local_net = inet_netof(local_address);
	sethostent(0);		/* rewind host file */
	while ((hp = gethostent()) != NULL) {
		if (local_net == inet_netof(* ((struct in_addr *) hp->h_addr))){
			test.sin_addr = * ((struct in_addr *) hp->h_addr);
			(void) sendto(test_socket, (char *) &msg, sizeof msg, 0,
			    (struct sockaddr *) &test, sizeof test);
		}
	}
#endif

get_response:
	namelen = sizeof listv[0];
	errno = 0;
	wait.tv_sec = 1;
	wait.tv_usec = 0;
	for (;;) {
		if (listc + 1 >= listmax) {
			listmax += 20;
			listv = (struct sockaddr_in *) realloc((char *) listv,
						listmax * sizeof listv[0]);
			if (listv == NULL)
				leave(1, "realloc");
		}

		FD_ZERO(&mask);
		FD_SET(test_socket, &mask);
		if (select(test_socket + 1, &mask, NULL, NULL, &wait) == 1 &&
		    recvfrom(test_socket, (char *) &port_num, sizeof(port_num),
		    0, (struct sockaddr *) &listv[listc], &namelen) > 0) {
			/*
			 * Note that we do *not* convert from network to host
			 * order since the port number *should* be in network
			 * order:
			 */
			for (i = 0; i < listc; i += 1)
				if (listv[listc].sin_addr.s_addr
				== listv[i].sin_addr.s_addr)
					break;
			if (i == listc)
				listv[listc++].sin_port = port_num;
			continue;
		}

		if (errno != 0 && errno != EINTR)
			leave(1, "select/recvfrom");

		/* terminate list with local address */
		listv[listc].sin_family = AF_INET;
		listv[listc].sin_addr = local_address;
		listv[listc].sin_port = htons(0);

		(void) close(test_socket);
		initial = FALSE;
		return listv;
	}

test_one_host:
	msg = htons(C_TESTMSG());
	(void) sendto(test_socket, (char *) &msg, sizeof msg, 0,
	    (struct sockaddr *) &test, sizeof test);
	goto get_response;
}

static void
find_driver(do_startup)
	FLAG	do_startup;
{
	struct sockaddr_in	*hosts;

	hosts = list_drivers();
	if (hosts[0].sin_port != htons(0)) {
		int	i, c;

		if (hosts[1].sin_port == htons(0)) {
			Daemon = hosts[0];
			return;
		}
		/* go thru list and return host that matches daemon */
		display_clear_the_screen();
		display_move(1, 0);
		display_put_str("Pick one:");
		for (i = 0; i < HEIGHT - 4 && hosts[i].sin_port != htons(0);
								i += 1) {
			struct	hostent	*hp;
			char	buf[80];

			display_move(3 + i, 0);
			hp = gethostbyaddr((char *) &hosts[i].sin_addr,
					sizeof hosts[i].sin_addr, AF_INET);
			(void) snprintf(buf, sizeof buf,
				"%8c    %.64s", 'a' + i,
				hp != NULL ? hp->h_name
				: inet_ntoa(hosts->sin_addr));
			display_put_str(buf);
		}
		display_move(4 + i, 0);
		display_put_str("Enter letter: ");
		display_refresh();
		while (!islower(c = getchar()) || (c -= 'a') >= i) {
			display_beep();
			display_refresh();
		}
		Daemon = hosts[c];
		display_clear_the_screen();
		return;
	}
	if (!do_startup)
		return;

	start_driver();
	sleep(2);
	find_driver(FALSE);
}

static void
dump_scores(host)
	struct sockaddr_in	host;
{
	struct	hostent	*hp;
	int	s;
	char	buf[BUFSIZ];
	int	cnt;

	hp = gethostbyaddr((char *) &host.sin_addr, sizeof host.sin_addr,
								AF_INET);
	printf("\n%s:\n", hp != NULL ? hp->h_name : inet_ntoa(host.sin_addr));
	fflush(stdout);

	s = socket(AF_INET, SOCK_STREAM, 0);
	if (s < 0)
		leave(1, "socket");
	if (connect(s, (struct sockaddr *) &host, sizeof host) < 0)
		leave(1, "connect");
	while ((cnt = read(s, buf, sizeof buf)) > 0)
		write(fileno(stdout), buf, cnt);
	(void) close(s);
}

static void
start_driver()
{
	if (Am_monitor) {
		errno = 0;
		leave(1, "No one playing");
	}

	if (Sock_host != NULL) {
		sleep(3);
		return;
	}

	errno = 0;
	leave(1, "huntd not running");
}

/*
 * bad_con:
 *	We had a bad connection.  For the moment we assume that this
 *	means the game is full.
 */
void
bad_con()
{
	leave(1, "lost connection to huntd");
}

/*
 * bad_ver:
 *	version number mismatch.
 */
void
bad_ver()
{
	errno = 0;
	leave(1, "Version number mismatch. No go.");
}

/*
 * sigterm:
 *	Handle a terminate signal
 */
static void
sigterm(dummy)
	int dummy;
{
	leave(0, (char *) NULL);
}

/*
 * rmnl:
 *	Remove a '\n' at the end of a string if there is one
 */
static void
rmnl(s)
	char	*s;
{
	char	*cp;

	cp = strrchr(s, '\n');
	if (cp != NULL)
		*cp = '\0';
}

/*
 * intr:
 *	Handle a interrupt signal
 */
void
intr(dummy)
	int dummy;
{
	int	ch;
	int	explained;
	int	y, x;

	(void) signal(SIGINT, SIG_IGN);
	display_getyx(&y, &x);
	display_move(HEIGHT, 0);
	display_put_str("Really quit? ");
	display_clear_eol();
	display_refresh();
	explained = FALSE;
	for (;;) {
		ch = getchar();
		if (isupper(ch))
			ch = tolower(ch);
		if (ch == 'y') {
			if (Socket != 0) {
				(void) write(Socket, "q", 1);
				(void) close(Socket);
			}
			leave(0, (char *) NULL);
		}
		else if (ch == 'n') {
			(void) signal(SIGINT, intr);
			display_move(y, x);
			display_refresh();
			return;
		}
		if (!explained) {
			display_put_str("(Yes or No) ");
			display_refresh();
			explained = TRUE;
		}
		display_beep();
		display_refresh();
	}
}

/*
 * leave:
 *	Leave the game somewhat gracefully, restoring all current
 *	tty stats.
 */
static void
leave(eval, mesg)
	int	eval;
	char	*mesg;
{
	int saved_errno;

	saved_errno = errno;
	if (in_visual) {
		display_move(HEIGHT, 0);
		display_refresh();
		display_end();
	}
	errno = saved_errno;

	if (errno == 0 && mesg != NULL)
		errx(eval, mesg);
	else if (mesg != NULL)
		err(eval, mesg);
	exit(eval);
}

/*
 * env_init:
 *	initialise game parameters from the HUNT envvar
 */
static long
env_init(enter_status)
	long	enter_status;
{
	int	i;
	char	*envp, *envname, *s;

	/* Map all keys to themselves: */
	for (i = 0; i < 256; i++)
		map_key[i] = (char) i;

	envname = NULL;
	if ((envp = getenv("HUNT")) != NULL) {
		while ((s = strpbrk(envp, "=,")) != NULL) {
			if (strncmp(envp, "cloak,", s - envp + 1) == 0) {
				enter_status = Q_CLOAK;
				envp = s + 1;
			}
			else if (strncmp(envp, "scan,", s - envp + 1) == 0) {
				enter_status = Q_SCAN;
				envp = s + 1;
			}
			else if (strncmp(envp, "fly,", s - envp + 1) == 0) {
				enter_status = Q_FLY;
				envp = s + 1;
			}
			else if (strncmp(envp, "nobeep,", s - envp + 1) == 0) {
				no_beep = TRUE;
				envp = s + 1;
			}
			else if (strncmp(envp, "name=", s - envp + 1) == 0) {
				envname = s + 1;
				if ((s = strchr(envp, ',')) == NULL) {
					*envp = '\0';
					strlcpy(name, envname, sizeof name);
					break;
				}
				*s = '\0';
				strlcpy(name, envname, sizeof name);
				envp = s + 1;
			}
			else if (strncmp(envp, "port=", s - envp + 1) == 0) {
				use_port = s + 1;
				Server_port = atoi(use_port);
				if ((s = strchr(envp, ',')) == NULL) {
					*envp = '\0';
					break;
				}
				*s = '\0';
				envp = s + 1;
			}
			else if (strncmp(envp, "host=", s - envp + 1) == 0) {
				Sock_host = s + 1;
				if ((s = strchr(envp, ',')) == NULL) {
					*envp = '\0';
					break;
				}
				*s = '\0';
				envp = s + 1;
			}
			else if (strncmp(envp, "message=", s - envp + 1) == 0) {
				Send_message = s + 1;
				if ((s = strchr(envp, ',')) == NULL) {
					*envp = '\0';
					break;
				}
				*s = '\0';
				envp = s + 1;
			}
			else if (strncmp(envp, "team=", s - envp + 1) == 0) {
				team = *(s + 1);
				if (!isdigit(team))
					team = ' ';
				if ((s = strchr(envp, ',')) == NULL) {
					*envp = '\0';
					break;
				}
				*s = '\0';
				envp = s + 1;
			}			/* must be last option */
			else if (strncmp(envp, "mapkey=", s - envp + 1) == 0) {
				for (s = s + 1; *s != '\0'; s += 2) {
					map_key[(unsigned int) *s] = *(s + 1);
					if (*(s + 1) == '\0') {
						break;
					}
				}
				*envp = '\0';
				break;
			} else {
				*s = '\0';
				printf("unknown option %s\n", envp);
				if ((s = strchr(envp, ',')) == NULL) {
					*envp = '\0';
					break;
				}
				envp = s + 1;
			}
		}
		if (*envp != '\0') {
			if (envname == NULL)
				strlcpy(name, envp, sizeof name);
			else
				printf("unknown option %s\n", envp);
		}
	}
	return enter_status;
}

/*
 * fill_in_blanks:
 *	quiz the user for the information they didn't provide earlier
 */
static void
fill_in_blanks()
{
	int	i;
	char	*cp;

again:
	if (name[0] != '\0') {
		printf("Entering as '%s'", name);
		if (team != ' ' && team != '-')
			printf(" on team %c.\n", team);
		else
			putchar('\n');
	} else {
		printf("Enter your code name: ");
		if (fgets(name, sizeof name, stdin) == NULL)
			exit(1);
	}
	rmnl(name);
	if (name[0] == '\0') {
		printf("You have to have a code name!\n");
		goto again;
	}
	for (cp = name; *cp != '\0'; cp++)
		if (!isprint(*cp)) {
			name[0] = '\0';
			printf("Illegal character in your code name.\n");
			goto again;
		}
	if (team == '-') {
		printf("Enter your team (0-9 or nothing): ");
		i = getchar();
		if (isdigit(i))
			team = i;
		else if (i == '\n' || i == EOF)
			team = ' ';
		/* ignore trailing chars */
		while (i != '\n' && i != EOF)
			i = getchar();
		if (team == '-') {
			printf("Teams must be numeric.\n");
			goto again;
		}
	}
}
