#	$OpenBSD: Makefile,v 1.7 2014/05/13 02:34:58 millert Exp $

PROG=	less
SRCS=	main.c screen.c brac.c ch.c charset.c cmdbuf.c command.c cvt.c \
	decode.c edit.c filename.c forwback.c ifile.c input.c jump.c \
	line.c linenum.c mark.c optfunc.c option.c opttbl.c os.c output.c \
	pattern.c position.c prompt.c search.c signal.c ttyin.c version.c
CFLAGS=	${PIPE} ${DEBUG} -I${.CURDIR}

DPADD=	${LIBOCURSES}
LDADD=	-locurses

.PATH:	${.CURDIR}/../../../usr.bin/less

.include <bsd.prog.mk>
