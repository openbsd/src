.PATH:		${.CURDIR}/..

PROG=	tcfsuse
BINOWN=	root
#BINMODE?=4555
BINMODE?=555

BINDIR=	/usr/bin
NOMAN=
#MAN=	tcfsuse.1

SRCS=	tcfsuse.c tcfsputkey.c tcfsrun.c tcfsrmkey.c tcfsgenkey.c tcfstat.c \
	tcfsflag.c

.include <bsd.prog.mk>

LDADD+=	-lutil -ldes
DPADD+= ${LIBUTIL} ${LIBDES}
