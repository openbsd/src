#	$OpenBSD: Makefile,v 1.2 2009/10/28 07:36:49 deraadt Exp $

PROG=	less
SRCS=	main.c screen.c brac.c ch.c charset.c cmdbuf.c command.c decode.c \
	edit.c filename.c forwback.c help.c ifile.c input.c jump.c line.c \
	linenum.c lsystem.c mark.c optfunc.c option.c opttbl.c os.c output.c \
	position.c prompt.c search.c signal.c tags.c ttyin.c version.c
CFLAGS=	${PIPE} ${DEBUG} -I${.CURDIR}
DPADD=	${LIBOCURSES}
LDADD=	-locurses

.PATH:	${.CURDIR}/../../../usr.bin/less

.include <bsd.prog.mk>
