#	$OpenBSD: Makefile,v 1.2 1997/09/21 11:43:33 deraadt Exp $

PROG=	cron
SRCS=	cron.c database.c user.c entry.c job.c do_command.c \
	misc.c env.c popen.c 
CFLAGS+=-I${.CURDIR}
MAN=	cron.8

.include <bsd.prog.mk>
