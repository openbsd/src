
CPPFLAGS +=	-pthread
LDFLAGS +=	-pthread
CFLAGS +=	-O -ggdb
METRICS =	null once_overhead self_overhead mutex_nocont mutex_cont\
		cond_nowait cond_timed
MKDEP =		-p
SRCS =		${METRICS:=.c}
CLEANFILES +=	${METRICS}

all: ${METRICS}
	@${SUDO} nice -n -19 ${SHELL} -c \
	 'ulimit -d 65536; for m in ${METRICS}; do $$m || exit; done'


.include <bsd.prog.mk>
