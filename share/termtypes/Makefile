#	$OpenBSD: Makefile,v 1.22 2011/07/19 10:20:20 millert Exp $
#
# Take termcap/terminfo master file and generate termcap/terminfo .db files

MAN=	termcap.5
CLEANFILES+= terminfo.src termcap.src terminfo.db termcap.db termcap
TIC=	/usr/bin/tic

all: terminfo.db termcap.db termcap

terminfo.src: termtypes.master
	${TIC} -I -x ${.ALLSRC} | sed -e '/^#/d' -e '/^$$/d' > ${.TARGET}
	@[ -s ${.TARGET} ] || exit 1

terminfo.db: terminfo.src
	cap_mkdb -i -f terminfo ${.ALLSRC}

termcap.src: termcap
	sed -e '/^#/d' -e '/^$$/d' ${.ALLSRC} > ${.TARGET}
	@[ -s ${.TARGET} ] || exit 1

termcap.db: termcap.src
	cap_mkdb -f termcap ${.ALLSRC}

termcap: termtypes.master
	${TIC} -C -x ${.ALLSRC} > ${.TARGET}
	@[ -s ${.TARGET} ] || exit 1

realinstall:
	${INSTALL} ${INSTALL_COPY} -o ${BINOWN} -g ${BINGRP} -m 444 terminfo.db \
	     ${DESTDIR}${BINDIR}/misc/terminfo.db
	${INSTALL} ${INSTALL_COPY} -o ${BINOWN} -g ${BINGRP} -m 444 termcap.db \
	     ${DESTDIR}${BINDIR}/misc/termcap.db
	${INSTALL} ${INSTALL_COPY} -o ${BINOWN} -g ${BINGRP} -m 444 termcap \
	     ${DESTDIR}${BINDIR}/misc/termcap
	ln -fs ${BINDIR}/misc/termcap ${DESTDIR}/etc/termcap

.include <bsd.prog.mk>
