#	$Id: Makefile,v 1.1.1.1 1995/10/18 08:41:03 deraadt Exp $

PROG=	rcs
CFLAGS+= -I${.CURDIR}/../lib
LDADD=	${LIBRCS} -lgnumalloc
DPADD=	${LIBRCS} /usr/lib/libgnumalloc.a

MAN=	rcs.1 rcsfile.5 rcsintro.1

.include "../../Makefile.inc"
.include <bsd.prog.mk>
