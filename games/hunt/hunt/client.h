/*	$OpenBSD: client.h,v 1.2 1999/02/01 06:53:55 d Exp $	*/

/* hunt.c */
extern FLAG	Am_monitor;		/* -m flag */
extern FLAG	no_beep;		/* -b flag */
extern char *	Send_message;		/* -w message */
extern int	Socket;			/* connection to server */
extern char     map_key[256];		/* HUNT envvar */

void	bad_con __P((void));
void	bad_ver __P((void));
void	intr __P((int));

/* connect.c */
void	do_connect __P((char *, u_int8_t, u_int32_t));

/* playit.c */
void	playit __P((void));
void	do_message __P((void));
int	quit __P((int));

/* otto.c */
extern int	Otto_mode;
int	otto __P((int, int, char, char *, size_t));
int	otto_quit __P((int));
