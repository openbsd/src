#	$OpenBSD: Makefile,v 1.2 2004/09/28 15:10:51 millert Exp $

.PATH:		${.CURDIR}/.. ${.CURDIR}/../auth

PROG=	sudo
BINOWN=	root
BINMODE=4555
LINKS=  ${BINDIR}/sudo ${BINDIR}/sudoedit
MLINKS= sudo.8 sudoedit.8


BINDIR=	/usr/bin
MAN=	sudo.8 sudoers.5

SRCS=	check.c env.c getspwuid.c interfaces.c logging.c parse.c set_perms.c \
	sudo.c tgetpass.c zero_bytes.c sudo_auth.c bsdauth.c sudo_edit.c

.include <bsd.prog.mk>
