# $OpenBSD: Makefile,v 1.3 2000/10/12 01:41:20 brad Exp $

DEBUG =		-ggdb
CFLAGS +=	-Wall
LDFLAGS +=	-pthread
METRICS =	null once_overhead self_overhead mutex_nocont mutex_cont\
		cond_nowait cond_timed
MKDEP =		-p
SRCS =		${METRICS:=.c}
CLEANFILES +=	${METRICS}

all: ${METRICS}
	@${SUDO} /usr/bin/nice -n -19 sh -c \
	 'ulimit -d 65536; for m in ${METRICS}; do ${.OBJDIR}/$$m || exit; done'

.include <bsd.prog.mk>
