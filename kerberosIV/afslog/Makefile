#	from @(#)Makefile	8.1 (Berkeley) 6/1/93
#	$Id: Makefile,v 1.2 1998/09/19 02:42:18 rahnds Exp $

PROG=	afslog
DPADD=	${LIBKRB} ${LIBDES} ${LIBKAFS}
LDADD=	-lkafs -lkrb -ldes
BINDIR=	/usr/bin

.include <bsd.prog.mk>
