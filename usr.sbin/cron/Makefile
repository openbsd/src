#	$OpenBSD: Makefile,v 1.5 2015/10/31 12:19:41 millert Exp $

PROG=	cron
SRCS=	cron.c database.c user.c entry.c job.c do_command.c \
	misc.c env.c popen.c atrun.c common.c
CFLAGS+=-I${.CURDIR}
MAN=	cron.8

.include <bsd.prog.mk>
