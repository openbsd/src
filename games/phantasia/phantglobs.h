/*	$OpenBSD: phantglobs.h,v 1.3 1998/11/29 19:57:01 pjanzen Exp $	*/
/*	$NetBSD: phantglobs.h,v 1.3 1995/04/24 12:24:39 cgd Exp $	*/

/*
 * phantglobs.h - global declarations for Phantasia
 */

extern	double	Circle;		/* which circle player is in */
extern	double	Shield;		/* force field thrown up in monster battle */

extern	bool	Beyond;		/* set if player is beyond point of no return */
extern	bool	Marsh;		/* set if player is in dead marshes */
extern	bool	Throne;		/* set if player is on throne */
extern	bool	Changed;	/* set if important player stats have changed */
extern	bool	Wizard;		/* set if player is the 'wizard' of the game */
extern	bool	Timeout;	/* set if short timeout waiting for input */
extern	bool	Windows;	/* set if we are set up for curses stuff */
extern	bool	Luckout;	/* set if we have tried to luck out in fight */
extern	bool	Foestrikes;	/* set if foe gets a chance to hit in battleplayer()*/
extern	bool	Echo;		/* set if echo input to terminal */

extern	int	Users;		/* number of users currently playing */
extern	int	Whichmonster;	/* which monster we are fighting */
extern	int	Lines;		/* line on screen counter for fight routines */

extern	char	Ch_Erase;	/* backspace key */
extern	char	Ch_Kill;	/* linekill key */

extern	jmp_buf Fightenv;	/* used to jump into fight routine */
extern	jmp_buf Timeoenv;	/* used for timing out waiting for input */

extern	long	Fileloc;	/* location in file of player statistics */

extern	const char *Login;	/* pointer to login of current player */
extern	char	*Enemyname;	/* pointer name of monster/player we are battling*/

extern	struct player	Player;	/* stats for player */
extern	struct player	Other;	/* stats for another player */

extern	struct monster	Curmonster;/* stats for current monster */

extern	struct energyvoid Enrgyvoid;/* energy void buffer */

extern	struct charstats Stattable[];/* used for rolling and changing player stats*/

extern	struct charstats *Statptr;/* pointer into Stattable[] */

extern	struct menuitem	Menu[];	/* menu of items for purchase */

extern	FILE	*Playersfp;	/* pointer to open player file */
extern	FILE	*Monstfp;	/* pointer to open monster file */
extern	FILE	*Messagefp;	/* pointer to open message file */
extern	FILE	*Energyvoidfp;	/* pointer to open energy void file */

extern	char	Databuf[];	/* a place to read data into */

/* some canned strings for messages */
extern	char	Illcmd[];
extern	char	Illmove[];
extern	char	Illspell[];
extern	char	Nomana[];
extern	char	Somebetter[];
extern	char	Nobetter[];

/* functions which we need to know about */

char	*descrlocation __P((struct player *, bool));
char	*descrstatus __P((struct player *));
char	*descrtype __P((struct player *, bool));
void	activelist __P((void));
void	adjuststats __P((void));
long	allocrecord __P((void));
long	allocvoid __P((void));
void	allstatslist __P((void));
void	altercoordinates __P((double, double, int));
void	awardtreasure __P((void));
void	battleplayer __P((long));
void	callmonster __P((int));
void	cancelmonster __P((void));
void	catchalarm __P((int));
void	changestats __P((bool));
void	checkbattle __P((void));
void	checktampered __P((void));
void	cleanup __P((int));
void	collecttaxes __P((double, double));
void	cursedtreasure __P((void));
void	death __P((char *));
void	displaystats __P((void));
double	distance __P((double, double, double, double));
void	dotampered __P((void));
double	drandom __P((void));
void	encounter __P((int));
void	enterscore __P((void));
void	error __P((char *));
double	explevel __P((double));
long	findname __P((char *, struct player *));
void	freerecord __P((struct player *, long));
void	genchar __P((int));
int	getanswer __P((char *, bool));
void	getstring __P((char *, int));
void	hitmonster __P((double));
void	ill_sig __P((int));
double	infloat __P((void));
void	initialstate __P((void));
void	initplayer __P((struct player *));
int	inputoption __P((void));
void	interrupt __P((void));
void	leavegame __P((void));
void	monsthits __P((void));
void	monstlist __P((void));
void	more __P((int));
void	movelevel __P((void));
void	myturn __P((void));
void	neatstuff __P((void));
int	pickmonster __P((void));
void	playerhits __P((void));
void	playinit __P((void));
void	procmain __P((void));
void	purgeoldplayers __P((void));
void	readmessage __P((void));
void	readrecord __P((struct player *, long));
long	recallplayer __P((void));
long	recallplayer __P((void));
long	rollnewplayer __P((void));
void	scorelist __P((void));
void	scramblestats __P((void));
void	tampered __P((int, double, double));
void	throneroom __P((void));
void	throwspell __P((void));
void	titlelist __P((void));
void	tradingpost __P((void));
void	truncstring __P((char *));
void	userlist __P((bool));
void	writerecord __P((struct player *, long));
void	writevoid __P((struct energyvoid *, long));
