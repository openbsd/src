#	$OpenBSD: Makefile,v 1.4 2010/07/02 22:43:37 guenther Exp $

PROG=	less
SRCS=	main.c screen.c brac.c ch.c charset.c cmdbuf.c command.c decode.c \
	edit.c filename.c forwback.c help.c ifile.c input.c jump.c line.c \
	linenum.c mark.c optfunc.c option.c opttbl.c os.c output.c \
	position.c prompt.c search.c signal.c ttyin.c version.c
CFLAGS=	${PIPE} ${DEBUG} -I${.CURDIR} -DSMALL_PROGRAM
DPADD=	${LIBOCURSES}
LDADD=	-locurses

.PATH:	${.CURDIR}/../../../usr.bin/less

.include <bsd.prog.mk>
