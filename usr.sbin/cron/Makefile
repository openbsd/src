#	$Id: Makefile,v 1.1.1.1 1995/10/18 08:47:30 deraadt Exp $

PROG=	cron
SRCS=	cron.c database.c user.c entry.c job.c do_command.c \
	misc.c env.c popen.c 
CFLAGS+=-I${.CURDIR}
MAN=	cron.8

.include <bsd.prog.mk>
