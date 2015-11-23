#	$OpenBSD: Makefile,v 1.6 2015/11/23 09:24:48 nicm Exp $

.PATH:	${.CURDIR}/..

PROG=	less
SRCS=	main.c screen.c brac.c ch.c charset.c cmdbuf.c command.c cvt.c \
	decode.c edit.c filename.c forwback.c ifile.c input.c jump.c \
	line.c linenum.c lsystem.c mark.c optfunc.c option.c opttbl.c \
	os.c output.c pattern.c position.c prompt.c search.c signal.c \
	tags.c ttyin.c version.c

LDADD=	-lcurses
DPADD=	${LIBCURSES}

LINKS=	${BINDIR}/less ${BINDIR}/more

MAN=	less.1 more.1
BINDIR=	/usr/bin

HELPDIR=${SHAREDIR}/misc

CFLAGS+=-I${.CURDIR}/.. -DSYSDIR=\"/etc\" -DHELPDIR=\"${HELPDIR}\"

beforeinstall:
	${INSTALL} ${INSTALL_COPY} -o ${SHAREOWN} -g ${SHAREGRP} \
	    -m ${SHAREMODE} ${.CURDIR}/../less.hlp \
	    ${DESTDIR}${HELPDIR}/less.help
	${INSTALL} ${INSTALL_COPY} -o ${SHAREOWN} -g ${SHAREGRP} \
	    -m ${SHAREMODE} ${.CURDIR}/../more.hlp \
	    ${DESTDIR}${HELPDIR}/more.help

.include <bsd.prog.mk>
