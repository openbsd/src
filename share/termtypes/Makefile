#	$OpenBSD: Makefile,v 1.24 2015/12/03 11:30:46 nicm Exp $

MAN=	termcap.5
TIC=	/usr/bin/tic

all: terminfo termcap

terminfo: termtypes.master
	${TIC} -x -o ${.TARGET} ${.ALLSRC}
	@[ -d ${.TARGET} ] || exit 1

termcap: termtypes.master
	${TIC} -C -x ${.ALLSRC} > ${.TARGET}
	@[ -s ${.TARGET} ] || exit 1

realinstall:
	find terminfo -type f -exec \
	     ${INSTALL} -D ${INSTALL_COPY} -o ${BINOWN} -g ${BINGRP} -m 444 \
	     {} ${DESTDIR}${BINDIR}/{} \;
	${INSTALL} ${INSTALL_COPY} -o ${BINOWN} -g ${BINGRP} -m 444 termcap \
	     ${DESTDIR}${BINDIR}/misc/termcap
	ln -fs ${BINDIR}/misc/termcap ${DESTDIR}/etc/termcap

clean:
	rm -f termcap
	rm -rf terminfo

.include <bsd.prog.mk>
