#	$OpenBSD: Makefile,v 1.1 2004/01/15 21:32:03 millert Exp $

.PATH:		${.CURDIR}/.. ${.CURDIR}/../auth

PROG=	sudo
BINOWN=	root
BINMODE=4555

BINDIR=	/usr/bin
MAN=	sudo.8 sudoers.5

SRCS=	check.c env.c getspwuid.c interfaces.c logging.c parse.c set_perms.c \
	sudo.c tgetpass.c zero_bytes.c sudo_auth.c bsdauth.c

.include <bsd.prog.mk>
