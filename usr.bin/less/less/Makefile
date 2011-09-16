#	$OpenBSD: Makefile,v 1.1 2011/09/16 18:12:09 shadchin Exp $

PROG=	less
SRCS=	main.c screen.c brac.c ch.c charset.c cmdbuf.c command.c cvt.c \
	decode.c edit.c filename.c forwback.c ifile.c input.c jump.c \
	line.c linenum.c lsystem.c mark.c optfunc.c option.c opttbl.c \
	os.c output.c pattern.c position.c prompt.c search.c signal.c \
	tags.c ttyin.c version.c

LDADD=	-lcurses
DPADD=	${LIBCURSES}

LINKS=	${BINDIR}/less ${BINDIR}/more ${BINDIR}/less ${BINDIR}/page

MAN=	less.1
MLINKS=	less.1 more.1 less.1 page.1

HELPFILE=${SHAREDIR}/misc/more.help

CFLAGS+=-DBINDIR=\"${BINDIR}\" -DSYSDIR=\"/etc\" -DHELPFILE=\"${HELPFILE}\"

CDIAGFLAGS+=-Wall -W

beforeinstall:
	${INSTALL} ${INSTALL_COPY} -o ${SHAREOWN} -g ${SHAREGRP} \
	    -m ${SHAREMODE} ${.CURDIR}/../less.hlp ${DESTDIR}${HELPFILE}

.include <bsd.prog.mk>
