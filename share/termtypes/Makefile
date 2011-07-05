#	$OpenBSD: Makefile,v 1.19 2011/07/05 03:34:30 nicm Exp $
#
# Take termcap/terminfo master file and generate termcap/terminfo .db files

MAN=	termcap.5
CLEANFILES+= terminfo.src termcap.src terminfo.db termcap.db termcap
TIC=	/usr/bin/tic

all: terminfo.db termcap.db termcap

terminfo.src: termtypes.master
	${TIC} -I -x ${.ALLSRC} | sed \
	    -e 's,/usr/share/lib/tabset,/usr/share/tabset,g' \
	    -e 's,/usr/lib/tabset,/usr/share/tabset,g' \
	    -e '/^#/d' \
	    > ${.TARGET}
	@if [ ! -s ${.TARGET} ]; then \
	    echo ${.TARGET} is zero length!  You need to update ${TIC} ;\
	    rm -f ${.TARGET} ;\
	    exit 1 ;\
	fi

terminfo.db: terminfo.src
	cap_mkdb -i -f terminfo terminfo.src

termcap.src: termtypes.master
	${TIC} -C -x ${.ALLSRC} | sed \
	    -e 's,/usr/share/lib/tabset,/usr/share/tabset,g' \
	    -e 's,/usr/lib/tabset,/usr/share/tabset,g' \
	    -e '/^#/d' \
	    > ${.TARGET}
	@if [ ! -s ${.TARGET} ]; then \
	    echo ${.TARGET} is zero length!  You need to update ${TIC} ;\
	    rm -f ${.TARGET} ;\
	    exit 1 ;\
	fi

termcap.db: termcap.src
	cap_mkdb -f termcap termcap.src

termcap: termtypes.master
	${TIC} -C -x ${.ALLSRC} | sed \
	    -e 's,/usr/share/lib/tabset,/usr/share/tabset,g' \
	    -e 's,/usr/lib/tabset,/usr/share/tabset,g' \
	    > ${.TARGET}

realinstall:
	${INSTALL} ${INSTALL_COPY} -o ${BINOWN} -g ${BINGRP} -m 444 terminfo.db \
	     ${DESTDIR}${BINDIR}/misc/terminfo.db
	${INSTALL} ${INSTALL_COPY} -o ${BINOWN} -g ${BINGRP} -m 444 termcap.db \
	     ${DESTDIR}${BINDIR}/misc/termcap.db
	${INSTALL} ${INSTALL_COPY} -o ${BINOWN} -g ${BINGRP} -m 444 termcap \
	     ${DESTDIR}${BINDIR}/misc/termcap
	ln -fs ${BINDIR}/misc/termcap ${DESTDIR}/etc/termcap

.include <bsd.prog.mk>
