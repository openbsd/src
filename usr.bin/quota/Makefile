#	from: @(#)Makefile	8.1 (Berkeley) 6/6/93
#	$Id: Makefile,v 1.1.1.1 1995/10/18 08:45:58 deraadt Exp $

PROG=	quota
BINOWN=	root
BINMODE=4555

DPADD=	${LIBRPCSVC}
LDADD=	-lrpcsvc

.include <bsd.prog.mk>
