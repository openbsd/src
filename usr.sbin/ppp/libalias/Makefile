LIB=	alias
CFLAGS+=-Wall -I${.CURDIR} -DNO_FW_PUNCH
SRCS=	alias.c alias_db.c alias_ftp.c alias_irc.c alias_util.c alias_old.c
MAN=libalias.3 
BINDIR=/usr/lib

beforeinstall:
	${INSTALL} -C -o ${BINOWN} -g ${BINGRP} -m 444 ${.CURDIR}/alias.h \
		${DESTDIR}/usr/include

.include <bsd.lib.mk>
