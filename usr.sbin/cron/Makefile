#	$OpenBSD: Makefile,v 1.3 2002/07/15 19:13:29 millert Exp $

PROG=	cron
SRCS=	cron.c database.c user.c entry.c job.c do_command.c \
	misc.c env.c popen.c atrun.c
CFLAGS+=-I${.CURDIR} -Wall
MAN=	cron.8

.include <bsd.prog.mk>
