#	$OpenBSD: Makefile,v 1.1 2003/04/14 14:35:04 millert Exp $

PROG=	less
SRCS=	main.c screen.c brac.c ch.c charset.c cmdbuf.c command.c decode.c \
	edit.c filename.c forwback.c help.c ifile.c input.c jump.c line.c \
	linenum.c lsystem.c mark.c optfunc.c option.c opttbl.c os.c output.c \
	position.c prompt.c search.c signal.c tags.c ttyin.c version.c
CFLAGS=	-Os ${PIPE} ${DEBUG} -I${.CURDIR}
MAN=
DPADD=	${LIBOCURSES}
LDADD=	-locurses

.PATH:	${.CURDIR}/../../../usr.bin/less

.include <bsd.prog.mk>
