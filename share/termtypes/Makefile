#	$OpenBSD: Makefile,v 1.12 2000/01/09 06:02:42 millert Exp $
#
# Take termcap/terminfo master file and generate termcap/terminfo .db files
# master: http://www.tuxedo.org/~esr/terminfo/termtypes.master.gz

MAN=	termcap.5
CLEANFILES+= terminfo.src termcap.src terminfo.db termcap.db

all: terminfo.db termcap.db

terminfo.src: termtypes.master
	tic -I -x ${.ALLSRC} | sed \
	    -e 's,/usr/share/lib/tabset,/usr/share/tabset,g' \
	    -e 's,/usr/lib/tabset,/usr/share/tabset,g' \
	    > ${.TARGET}
	@if [ ! -s ${.TARGET} ]; then \
	    echo ${.TARGET} is zero length!  You need to update /usr/bin/tic ;\
	    exit 1 ;\
	fi

terminfo.db: terminfo.src
	cap_mkdb -i -f terminfo terminfo.src

termcap.src: termtypes.master
	tic -C -x ${.ALLSRC} | sed \
	    -e 's,/usr/share/lib/tabset,/usr/share/tabset,g' \
	    -e 's,/usr/lib/tabset,/usr/share/tabset,g' \
	    > ${.TARGET}
	@if [ ! -s ${.TARGET} ]; then \
	    echo ${.TARGET} is zero length!  You need to update /usr/bin/tic ;\
	    exit 1 ;\
	fi

termcap.db: termcap.src
	cap_mkdb -f termcap termcap.src

realinstall:
	${INSTALL} ${INSTALL_COPY} -o ${BINOWN} -g ${BINGRP} -m 444 terminfo.db \
	     ${DESTDIR}${BINDIR}/misc/terminfo.db
	${INSTALL} ${INSTALL_COPY} -o ${BINOWN} -g ${BINGRP} -m 444 termcap.db \
	     ${DESTDIR}${BINDIR}/misc/termcap.db
	${INSTALL} ${INSTALL_COPY} -o ${BINOWN} -g ${BINGRP} -m 444 termcap.src \
	     ${DESTDIR}${BINDIR}/misc/termcap
	${INSTALL} ${INSTALL_COPY} -o ${BINOWN} -g ${BINGRP} -m 444 ${.CURDIR}/map3270 \
	     ${DESTDIR}${BINDIR}/misc/map3270
	ln -fs ${BINDIR}/misc/termcap ${DESTDIR}/etc/termcap

.include <bsd.prog.mk>
