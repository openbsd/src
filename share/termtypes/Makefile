#	$OpenBSD: Makefile,v 1.2 1998/12/07 19:53:41 millert Exp $
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
	    > ${.OBJDIR}/terminfo.src

terminfo.db: terminfo.src
	info_mkdb -f terminfo ${.OBJDIR}/terminfo.src

# Note the disgusting ed hack to remove three lines from xterm
termcap.src: termtypes.master
	tic -C ${.ALLSRC} | sed \
	    -e 's,/usr/share/lib/tabset,/usr/share/tabset,g' \
	    -e 's,/usr/lib/tabset,/usr/share/tabset,g' \
	    > ${.OBJDIR}/termcap.src
	echo "1\n/^xterm|\n\nd\nd\nd\n.\nw\nq\n" | ed - ${.OBJDIR}/termcap.src >/dev/null 2>&1

termcap.db: termcap.src
	cap_mkdb -f termcap ${.OBJDIR}/termcap.src

realinstall:
	${INSTALL} ${INSTALL_COPY} -o ${BINOWN} -g ${BINGRP} -m 444 terminfo.db \
	     ${DESTDIR}${BINDIR}/misc/terminfo.db
	${INSTALL} ${INSTALL_COPY} -o ${BINOWN} -g ${BINGRP} -m 444 termcap.db \
	     ${DESTDIR}${BINDIR}/misc/termcap.db
	${INSTALL} -c -o ${BINOWN} -g ${BINGRP} -m 444 ${.CURDIR}/termcap.src \
	     ${DESTDIR}${BINDIR}/misc/termcap
	${INSTALL} -c -o ${BINOWN} -g ${BINGRP} -m 444 ${.CURDIR}/map3270 \
	     ${DESTDIR}${BINDIR}/misc/map3270
	ln -fs ${BINDIR}/misc/termcap ${DESTDIR}/etc/termcap

.include <bsd.prog.mk>
