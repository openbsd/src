/*	$OpenBSD: main.h,v 1.4 2018/08/13 06:36:29 anton Exp $	*/
/*
 *	Written by Alexaner Bluhm <bluhm@openbsd.org> 2016 Public Domain
 */

#define ASS(cond, mess)							\
	do {								\
		if (!(cond)) {						\
			mess;						\
			return (1);					\
		}							\
	} while (0)

#define ASSX(cond) ASS(cond,						\
	warnx("assertion " #cond " failed in %s on line %d",		\
	    __FILE__, __LINE__))

int check_inheritance(void);
int do_fdpass(void);
int do_flock(void);
int do_invalid_timer(void);
int do_pipe(void);
int do_process(void);
int do_pty(void);
int do_random(void);
int do_regress(int);
int do_signal(void);
int do_timer(void);
int do_tun(void);
