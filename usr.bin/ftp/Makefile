#	$NetBSD: Makefile,v 1.8 1997/01/19 14:19:02 lukem Exp $
#	from: @(#)Makefile	8.2 (Berkeley) 4/3/94

# define SMALLFTP if editing is to be disabled
#SMALLFTP=yes

PROG=	ftp
SRCS=	cmds.c cmdtab.c domacro.c fetch.c ftp.c main.c ruserpass.c util.c

.if defined(SMALLFTP)
CFLAGS+=-DSMALLFTP
.else
SRCS+=	complete.c
LDADD+=	-ledit -ltermcap
.endif

.include <bsd.prog.mk>
