#	$OpenBSD: Makefile,v 1.11 1999/12/12 22:24:59 millert Exp $
#
# Take termcap/terminfo master file and generate termcap/terminfo .db files
# master: http://www.tuxedo.org/~esr/terminfo/termtypes.master.gz

MAN=	termcap.5
CLEANFILES+= terminfo.src termcap.src terminfo.db termcap.db

all: terminfo.db termcap.db

terminfo.src: termtypes.master
	tic -I ${.ALLSRC} | sed \
	    -e 's,/usr/share/lib/tabset,/usr/share/tabset,g' \
	    -e 's,/usr/lib/tabset,/usr/share/tabset,g' \
	    > ${.TARGET}
	@if [ ! -s ${.TARGET} ]; then \
	    echo ${.TARGET} is zero length!  You need to update /usr/bin/tic ;\
	    exit 1 ;\
	fi

terminfo.db: terminfo.src
	cap_mkdb -i -f terminfo terminfo.src

# Note the hack to remove lines betweem the 'xterm' extry and what it includes.
# This exists to prune out some unneeded entries so it is <= 1023 bytes.
termcap.src: termtypes.master
	tic -C ${.ALLSRC} | perl -e \
	    'undef $$/; $$_ = <STDIN>; \
	    s,/usr/share/lib/tabset,/usr/share/tabset,g; \
	    s,/usr/lib/tabset,/usr/share/tabset,g; \
	    s/(\nxterm\|[^\n]+\n)[^#]+(\s+:tc=)/$$1$$2/s; print' \
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
